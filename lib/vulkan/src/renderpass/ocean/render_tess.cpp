#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/ocean/ocean.hpp"

#include <entt/entity/registry.hpp>
#include <sung/basic/aabb.hpp>
#include <sung/basic/cvar.hpp>
#include <sung/basic/time.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/mem_cinfo.hpp"
#include "mirinae/renderpass/builder.hpp"
#include "mirinae/renderpass/ocean/common.hpp"


namespace {

    sung::AutoCVarFlt cv_foam_sss_base{ "ocean:foam_sss_base", "", 0 };
    sung::AutoCVarFlt cv_foam_sss_scale{ "ocean:foam_sss_scale", "", 4 };
    sung::AutoCVarFlt cv_foam_threshold{ "ocean:foam_threshold", "", 8.5 };


    template <typename T>
    const T* find_first_cpnt(const entt::registry& reg) {
        for (auto e : reg.view<const T>()) {
            return &reg.get<const T>(e);
        }
        return nullptr;
    }


    class U_OceanTessPushConst {

    public:
        U_OceanTessPushConst& pvm(
            const glm::dmat4& proj,
            const glm::dmat4& view,
            const glm::dmat4& model
        ) {
            pvm_ = proj * view * model;
            view_ = view;
            model_ = model;
            return *this;
        }

        template <typename T>
        U_OceanTessPushConst& patch_offset(T x, T y) {
            patch_offset_scale_.x = static_cast<float>(x);
            patch_offset_scale_.y = static_cast<float>(y);
            return *this;
        }

        template <typename T>
        U_OceanTessPushConst& patch_scale(T x, T y) {
            patch_offset_scale_.z = static_cast<float>(x);
            patch_offset_scale_.w = static_cast<float>(y);
            return *this;
        }

        template <typename T>
        U_OceanTessPushConst& tile_dimensions(T x, T y) {
            tile_dims_n_fbuf_size_.x = static_cast<float>(x);
            tile_dims_n_fbuf_size_.y = static_cast<float>(y);
            return *this;
        }

        template <typename T>
        U_OceanTessPushConst& tile_dimensions(T x) {
            tile_dims_n_fbuf_size_.x = static_cast<float>(x);
            tile_dims_n_fbuf_size_.y = static_cast<float>(x);
            return *this;
        }

        U_OceanTessPushConst& fbuf_size(const VkExtent2D& x) {
            tile_dims_n_fbuf_size_.z = static_cast<float>(x.width);
            tile_dims_n_fbuf_size_.w = static_cast<float>(x.height);
            return *this;
        }

        template <typename T>
        U_OceanTessPushConst& tile_index(T x, T y) {
            tile_index_count_.x = static_cast<float>(x);
            tile_index_count_.y = static_cast<float>(y);
            return *this;
        }

        template <typename T>
        U_OceanTessPushConst& tile_count(T x, T y) {
            tile_index_count_.z = static_cast<float>(x);
            tile_index_count_.w = static_cast<float>(y);
            return *this;
        }

        template <typename T>
        U_OceanTessPushConst& patch_height(T x) {
            patch_height_ = static_cast<float>(x);
            return *this;
        }

    private:
        glm::mat4 pvm_;
        glm::mat4 view_;
        glm::mat4 model_;
        glm::vec4 patch_offset_scale_;
        glm::vec4 tile_dims_n_fbuf_size_;
        glm::vec4 tile_index_count_;
        float patch_height_;
    };

    static_assert(sizeof(U_OceanTessPushConst) < 256, "");


    struct U_OceanTessParams {

    public:
        U_OceanTessParams& texco_offset(size_t idx, float x, float y) {
            texco_offset_rot_[idx].x = x;
            texco_offset_rot_[idx].y = y;
            return *this;
        }

        U_OceanTessParams& texco_offset(size_t idx, const glm::vec2& v) {
            return this->texco_offset(idx, v.x, v.y);
        }

        U_OceanTessParams& texco_scale(size_t idx, float x, float y) {
            texco_offset_rot_[idx].z = x;
            texco_offset_rot_[idx].w = y;
            return *this;
        }

        U_OceanTessParams& texco_scale(size_t idx, const glm::vec2& v) {
            return this->texco_scale(idx, v.x, v.y);
        }

        U_OceanTessParams& dlight_color(const glm::vec3& dir) {
            dlight_color_.x = dir.x;
            dlight_color_.y = dir.y;
            dlight_color_.z = dir.z;
            return *this;
        }

        U_OceanTessParams& dlight_dir(const glm::vec3& dir) {
            dlight_dir_.x = dir.x;
            dlight_dir_.y = dir.y;
            dlight_dir_.z = dir.z;
            return *this;
        }

        U_OceanTessParams& fog_color(const glm::vec3& color) {
            fog_color_density_.x = color.r;
            fog_color_density_.y = color.g;
            fog_color_density_.z = color.b;
            return *this;
        }

        U_OceanTessParams& fog_density(float density) {
            fog_color_density_.w = density;
            return *this;
        }

        template <typename T>
        U_OceanTessParams& jacobian_scale(size_t idx, T x) {
            jacobian_scale_[idx] = static_cast<float>(x);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& len_scale(size_t idx, T x) {
            len_lod_scales_[idx] = static_cast<float>(x);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& len_scales(T x, T y, T z) {
            len_lod_scales_.x = static_cast<float>(x);
            len_lod_scales_.y = static_cast<float>(y);
            len_lod_scales_.z = static_cast<float>(z);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& lod_scale(T x) {
            len_lod_scales_.w = static_cast<float>(x);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& ocean_color(const glm::tvec3<T>& color) {
            ocean_color_.x = static_cast<float>(color.x);
            ocean_color_.y = static_cast<float>(color.y);
            ocean_color_.z = static_cast<float>(color.z);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& foam_bias(T x) {
            foam_bias_ = static_cast<float>(x);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& foam_scale(T x) {
            foam_scale_ = static_cast<float>(x);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& foam_threshold(T x) {
            foam_threshold_ = static_cast<float>(x);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& roughness(T x) {
            roughness_ = static_cast<float>(x);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& sss_base(T x) {
            sss_base_ = static_cast<float>(x);
            return *this;
        }

        template <typename T>
        U_OceanTessParams& sss_scale(T x) {
            sss_scale_ = static_cast<float>(x);
            return *this;
        }

    private:
        glm::vec4 texco_offset_rot_[mirinae::CASCADE_COUNT];
        glm::vec4 dlight_color_;
        glm::vec4 dlight_dir_;
        glm::vec4 fog_color_density_;
        glm::vec4 jacobian_scale_;
        glm::vec4 len_lod_scales_;
        glm::vec4 ocean_color_;
        float foam_bias_;
        float foam_scale_;
        float foam_threshold_;
        float roughness_;
        float sss_base_;
        float sss_scale_;
    };


    struct FrameData {
        std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> disp_map_;
        std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> deri_map_;
        std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> turb_map_;
        mirinae::HRpImage trans_lut_;
        mirinae::HRpImage sky_view_lut_;
        mirinae::HRpImage cam_scat_vol_;
        mirinae::Buffer ubuf_;
        mirinae::Fbuf fbuf_;
        VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
    };

    using FrameDataArr = std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT>;

}  // namespace


namespace {

    template <typename T>
    bool has_separating_axis(
        const mirinae::ViewFrustum& view_frustum,
        const std::array<glm::tvec3<T>, 4>& points
    ) {
        const auto to_patch = glm::tmat4x4<T>(view_frustum.view_inv_);
        const auto to_patch3 = glm::tmat3x3<T>(to_patch);

        MIRINAE_ASSERT(view_frustum.vtx_.size() == 8);
        std::array<glm::tvec3<T>, 8> frustum_points;
        for (size_t i = 0; i < 8; ++i) {
            const auto& p = glm::tvec4<T>(view_frustum.vtx_[i], 1);
            frustum_points[i] = glm::tvec3<T>(to_patch * p);
        }

        std::vector<glm::tvec3<T>> axes;
        axes.reserve(view_frustum.axes_.size() + 3);
        for (auto& v : view_frustum.axes_) {
            axes.push_back(to_patch3 * v);
        }
        axes.push_back(glm::tvec3<T>(1, 0, 0));
        axes.push_back(glm::tvec3<T>(0, 1, 0));
        axes.push_back(glm::tvec3<T>(0, 0, 1));

        for (auto& axis : axes) {
            sung::Aabb1DLazyInit<T> frustum_aabb;
            for (auto& p : frustum_points)
                frustum_aabb.set_or_expand(glm::dot(p, axis));

            sung::Aabb1DLazyInit<T> points_aabb;
            for (auto& p : points) points_aabb.set_or_expand(glm::dot(p, axis));

            if (!frustum_aabb.is_intersecting_cl(points_aabb))
                return true;
        }

        return false;
    }


    template <typename T>
    void traverse_quad_tree(
        const VkCommandBuffer cmdbuf,
        const int depth,
        const T x_min,
        const T x_max,
        const T y_min,
        const T y_max,
        const T height,
        const mirinae::RpCtxt& ctxt,
        U_OceanTessPushConst& pc,
        const mirinae::PushConstInfo& pc_info,
        const glm::tmat4x4<T>& pv,
        const glm::tvec2<T>& fbuf_size
    ) {
        using Vec2 = glm::tvec2<T>;
        using Vec3 = glm::tvec3<T>;
        using Vec4 = glm::tvec4<T>;

        constexpr T HALF = 0.5;
        constexpr T MARGIN = 1;

        const auto x_margin = MARGIN;
        const auto y_margin = MARGIN;
        const std::array<Vec3, 4> points{
            Vec3(x_min - x_margin, height, y_min - y_margin),
            Vec3(x_min - x_margin, height, y_max + y_margin),
            Vec3(x_max + x_margin, height, y_max + y_margin),
            Vec3(x_max + x_margin, height, y_min - y_margin),
        };

        // Check frustum
        if (::has_separating_axis<T>(ctxt.main_cam_.view_frustum(), points)) {
            /*
            auto& dbg = ctxt.debug_ren_;
            dbg.add_tri(
                pv * Vec4(points[0], 1),
                pv * Vec4(points[1], 1),
                pv * Vec4(points[2], 1),
                glm::vec4(1, 0, 0, 0.1)
            );
            dbg.add_tri(
                pv * Vec4(points[0], 1),
                pv * Vec4(points[2], 1),
                pv * Vec4(points[3], 1),
                glm::vec4(1, 0, 0, 0.1)
            );
            */
            return;
        }

        std::array<Vec3, 4> ndc_points;
        for (size_t i = 0; i < 4; ++i) {
            auto ndc4 = pv * Vec4(points[i], 1);
            ndc4 /= ndc4.w;
            ndc_points[i] = Vec3(ndc4);
        }

        if (depth > 8) {
            pc.patch_offset(x_min - x_margin, y_min - y_margin)
                .patch_scale(
                    x_max - x_min + x_margin + x_margin,
                    y_max - y_min + y_margin + y_margin
                );
            pc_info.record(cmdbuf, pc);
            vkCmdDraw(cmdbuf, 4, 1, 0, 0);
            return;
        }

        T longest_edge = 0;
        for (size_t i = 0; i < 4; ++i) {
            const auto next_idx = (i + 1) % ndc_points.size();
            const auto& p0 = Vec2(ndc_points[i]) * HALF + HALF;
            const auto& p1 = Vec2(ndc_points[next_idx]) * HALF + HALF;
            const auto edge = (p1 - p0) * fbuf_size;
            const auto len = glm::length(edge);
            longest_edge = (std::max<T>)(longest_edge, len);
        }
        for (size_t i = 0; i < 2; ++i) {
            const auto next_idx = (i + 2) % ndc_points.size();
            const auto& p0 = Vec2(ndc_points[i]) * HALF + HALF;
            const auto& p1 = Vec2(ndc_points[next_idx]) * HALF + HALF;
            const auto edge = (p1 - p0) * fbuf_size;
            const auto len = glm::length(edge);
            longest_edge = (std::max<T>)(longest_edge, len);
        }

        if (glm::length(longest_edge) > 1000) {
            const auto x_mid = (x_min + x_max) * 0.5;
            const auto y_mid = (y_min + y_max) * 0.5;
            ::traverse_quad_tree<T>(
                cmdbuf,
                depth + 1,
                x_min,
                x_mid,
                y_min,
                y_mid,
                height,
                ctxt,
                pc,
                pc_info,
                pv,
                fbuf_size
            );
            ::traverse_quad_tree<T>(
                cmdbuf,
                depth + 1,
                x_min,
                x_mid,
                y_mid,
                y_max,
                height,
                ctxt,
                pc,
                pc_info,
                pv,
                fbuf_size
            );
            ::traverse_quad_tree<T>(
                cmdbuf,
                depth + 1,
                x_mid,
                x_max,
                y_mid,
                y_max,
                height,
                ctxt,
                pc,
                pc_info,
                pv,
                fbuf_size
            );
            ::traverse_quad_tree<T>(
                cmdbuf,
                depth + 1,
                x_mid,
                x_max,
                y_min,
                y_mid,
                height,
                ctxt,
                pc,
                pc_info,
                pv,
                fbuf_size
            );
        } else {
            pc.patch_offset(x_min - x_margin, y_min - y_margin)
                .patch_scale(
                    x_max - x_min + x_margin + x_margin,
                    y_max - y_min + y_margin + y_margin
                );
            pc_info.record(cmdbuf, pc);
            vkCmdDraw(cmdbuf, 4, 1, 0, 0);
            return;
        }
    }

}  // namespace


// Tasks
namespace { namespace task {

    class DrawTasks : public mirinae::DependingTask {

    public:
        DrawTasks() { fence_.succeed(this); }

        void init(
            const entt::registry& reg,
            const mirinae::FbufImageBundle& gbufs,
            const mirinae::IRenPass& rp,
            ::FrameDataArr& fdata,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            cmd_pool_ = &cmd_pool;
            device_ = &device;
            fdata_ = &fdata;
            gbufs_ = &gbufs;
            reg_ = &reg;
            rp_ = &rp;
        }

        void prepare(const mirinae::RpCtxt& ctxt) { ctxt_ = &ctxt; }

        enki::ITaskSet& fence() { return fence_; }

        void collect_cmdbuf(std::vector<VkCommandBuffer>& out) {
            if (VK_NULL_HANDLE != cmdbuf_) {
                out.push_back(cmdbuf_);
            }
        }

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            cmdbuf_ = VK_NULL_HANDLE;
            auto ocean = ::find_first_cpnt<mirinae::cpnt::Ocean>(*reg_);
            if (!ocean)
                return;

            cmdbuf_ = cmd_pool_->get(ctxt_->f_index_, tid, *device_);
            if (cmdbuf_ == VK_NULL_HANDLE)
                return;

            auto& fd = fdata_->at(ctxt_->f_index_.get());
            const auto gbuf_ext = gbufs_->extent();
            const auto atmos =
                ::find_first_cpnt<mirinae::cpnt::AtmosphereSimple>(*reg_);

            mirinae::begin_cmdbuf(cmdbuf_, DEBUG_LABEL);
            this->update_ubuf(*reg_, atmos, *ocean, *ctxt_, fd, *device_);
            this->record_barriers(cmdbuf_, fd, *gbufs_, *ctxt_);
            this->record(cmdbuf_, fd, *ocean, *rp_, *ctxt_, gbuf_ext);
            mirinae::end_cmdbuf(cmdbuf_, DEBUG_LABEL);
        }

        static void update_ubuf(
            const entt::registry& reg,
            const mirinae::cpnt::AtmosphereSimple* atmos,
            const mirinae::cpnt::Ocean& ocean_entt,
            const mirinae::RpCtxt& ctxt,
            ::FrameData& fd,
            mirinae::VulkanDevice& device
        ) {
            namespace cpnt = mirinae::cpnt;

            const auto view_mat = ctxt.main_cam_.view();

            U_OceanTessParams ubuf;
            ubuf.foam_bias(ocean_entt.foam_bias_)
                .foam_scale(ocean_entt.foam_scale_)
                .foam_threshold(cv_foam_threshold.get())
                .jacobian_scale(0, ocean_entt.cascades_[0].jacobian_scale_)
                .jacobian_scale(1, ocean_entt.cascades_[1].jacobian_scale_)
                .jacobian_scale(2, ocean_entt.cascades_[2].jacobian_scale_)
                .len_scale(0, ocean_entt.cascades_[0].lod_scale_)
                .len_scale(1, ocean_entt.cascades_[1].lod_scale_)
                .len_scale(2, ocean_entt.cascades_[2].lod_scale_)
                .lod_scale(ocean_entt.lod_scale_)
                .ocean_color(ocean_entt.ocean_color_)
                .roughness(ocean_entt.roughness_)
                .sss_base(cv_foam_sss_base.get())
                .sss_scale(cv_foam_sss_scale.get());
            for (size_t i = 0; i < mirinae::CASCADE_COUNT; i++)
                ubuf.texco_offset(i, ocean_entt.cascades_[i].texco_offset_)
                    .texco_scale(i, ocean_entt.cascades_[i].texco_scale_);
            if (atmos)
                ubuf.fog_color(atmos->fog_color_)
                    .fog_density(atmos->fog_density_);

            for (auto e : reg.view<cpnt::DLight, cpnt::Transform>()) {
                auto& light = reg.get<cpnt::DLight>(e);
                auto& tform = reg.get<cpnt::Transform>(e);
                ubuf.dlight_color(light.color_.scaled_color())
                    .dlight_dir(light.calc_to_light_dir(view_mat, tform));
                break;
            }

            fd.ubuf_.set_data(ubuf);
        }

        static void record_barriers(
            const VkCommandBuffer cmdbuf,
            const ::FrameData& fd,
            const mirinae::FbufImageBundle& gbufs,
            const mirinae::RpCtxt& ctxt
        ) {
            mirinae::ImageMemoryBarrier tex_barr;
            tex_barr.set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .old_layout(VK_IMAGE_LAYOUT_GENERAL)
                .new_layout(VK_IMAGE_LAYOUT_GENERAL)
                .set_src_acc(VK_ACCESS_SHADER_WRITE_BIT)
                .set_dst_acc(VK_ACCESS_SHADER_READ_BIT)
                .set_signle_mip_layer();

            for (auto& img : fd.disp_map_) {
                tex_barr.image(img->img_.image())
                    .record_single(
                        cmdbuf,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT
                    );
            }
            for (auto& img : fd.deri_map_) {
                tex_barr.image(img->img_.image())
                    .record_single(
                        cmdbuf,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                    );
            }
            for (auto& img : fd.turb_map_) {
                tex_barr.image(img->img_.image())
                    .record_single(
                        cmdbuf,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                    );
            }
        }

        static void record(
            const VkCommandBuffer cmdbuf,
            const ::FrameData& fd,
            const mirinae::cpnt::Ocean& ocean_entt,
            const mirinae::IRenPass& rp,
            const mirinae::RpCtxt& ctxt,
            const VkExtent2D& fbuf_ext
        ) {
            const auto& proj_mat = ctxt.main_cam_.proj();
            const auto& view_mat = ctxt.main_cam_.view();
            const auto& view_inv = ctxt.main_cam_.view_inv();
            const auto& view_pos = ctxt.main_cam_.view_pos();

            mirinae::RenderPassBeginInfo{}
                .rp(rp.render_pass())
                .fbuf(fd.fbuf_.get())
                .wh(fbuf_ext)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cmdbuf);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ fbuf_ext }.record_single(cmdbuf);
            mirinae::Rect2D{ fbuf_ext }.record_scissor(cmdbuf);

            mirinae::DescSetBindInfo{}
                .layout(rp.pipe_layout())
                .add(fd.desc_set_)
                .record(cmdbuf);

            mirinae::PushConstInfo pc_info;
            pc_info.layout(rp.pipe_layout())
                .add_stage_vert()
                .add_stage_tesc()
                .add_stage_tese()
                .add_stage_frag();

            U_OceanTessPushConst pc;
            pc.fbuf_size(fbuf_ext)
                .tile_count(ocean_entt.tile_count_x_, ocean_entt.tile_count_y_)
                .tile_dimensions(ocean_entt.tile_size_)
                .pvm(proj_mat, view_mat, glm::dmat4(1))
                .patch_height(ocean_entt.height_);

            const auto z_far = proj_mat[3][2] / proj_mat[2][2];
            const auto scale = z_far * 1.25;
            const auto pv = proj_mat * view_mat;
            const auto cam_x = std::round(view_pos.x * 0.1) * 10;
            const auto cam_z = std::round(view_pos.z * 0.1) * 10;

            ::traverse_quad_tree<double>(
                cmdbuf,
                0,
                cam_x - scale,
                cam_x + scale,
                cam_z - scale,
                cam_z + scale,
                ocean_entt.height_,
                ctxt,
                pc,
                pc_info,
                pv,
                glm::dvec2(fbuf_ext.width, fbuf_ext.height)
            );

            vkCmdEndRenderPass(cmdbuf);
        }

        const mirinae::DebugLabel DEBUG_LABEL{ "Ocean Tess", 0.31, 0.76, 0.97 };

        mirinae::FenceTask fence_;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;

        const entt::registry* reg_ = nullptr;
        const mirinae::FbufImageBundle* gbufs_ = nullptr;
        const mirinae::IRenPass* rp_ = nullptr;
        const mirinae::RpCtxt* ctxt_ = nullptr;
        ::FrameDataArr* fdata_ = nullptr;
        mirinae::RpCommandPool* cmd_pool_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
    };


    class RpTask : public mirinae::IRpTask {

    public:
        void init(
            const entt::registry& reg,
            const mirinae::FbufImageBundle& gbufs,
            const mirinae::IRenPass& rp,
            ::FrameDataArr& fdata,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            record_tasks_.init(reg, gbufs, rp, fdata, cmd_pool, device);
        }

        std::string_view name() const override { return "composition dlights"; }

        void prepare(const mirinae::RpCtxt& ctxt) override {
            record_tasks_.prepare(ctxt);
        }

        void collect_cmdbuf(std::vector<VkCommandBuffer>& out) override {
            record_tasks_.collect_cmdbuf(out);
        }

        enki::ITaskSet* record_task() override { return &record_tasks_; }

        enki::ITaskSet* record_fence() override {
            return &record_tasks_.fence();
        }

    private:
        DrawTasks record_tasks_;
    };

}}  // namespace ::task


// Ocean tessellation
namespace {

    class RpStatesOceanTess
        : public mirinae::IRpBase
        , public mirinae::RenPassBundle<2> {

    public:
        RpStatesOceanTess(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device
        )
            : device_(device), cosmos_(cosmos), rp_res_(rp_res) {
            // Sky texture
            {
                auto& reg = cosmos.reg();
                auto& tex = *rp_res.tex_man_;
                for (auto e : reg.view<mirinae::cpnt::AtmosphereSimple>()) {
                    auto& atmos = reg.get<mirinae::cpnt::AtmosphereSimple>(e);
                    if (tex.block_for_tex(atmos.sky_tex_path_, false)) {
                        sky_tex_ = tex.get(atmos.sky_tex_path_);
                        break;
                    }
                }
                if (!sky_tex_)
                    sky_tex_ = tex.missing_tex();
            }

            // Images
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& fd = frame_data_[i];

                fd.trans_lut_ = rp_res_.ren_img_.get_img_reader(
                    fmt::format("atmos trans LUT:trans_lut_f#{}", i), name_s()
                );
                fd.sky_view_lut_ = rp_res_.ren_img_.get_img_reader(
                    fmt::format("sky view LUT:sky_view_lut_f#{}", i), name_s()
                );
                fd.cam_scat_vol_ = rp_res_.ren_img_.get_img_reader(
                    fmt::format("atmos cam volume:cam_vol_f#{}", i), name_s()
                );

                MIRINAE_ASSERT(nullptr != fd.trans_lut_);
                MIRINAE_ASSERT(nullptr != fd.sky_view_lut_);
                MIRINAE_ASSERT(nullptr != fd.cam_scat_vol_);

                for (size_t j = 0; j < mirinae::CASCADE_COUNT; j++) {
                    const auto img_name = fmt::format(
                        "ocean_finalize:displacement_c{}_f{}", j, i
                    );
                    fd.disp_map_[j] = rp_res.ren_img_.get_img_reader(
                        img_name, name_s()
                    );
                    MIRINAE_ASSERT(nullptr != fd.disp_map_[j]);
                }

                for (size_t j = 0; j < mirinae::CASCADE_COUNT; j++) {
                    const auto img_name = fmt::format(
                        "ocean_finalize:derivatives_c{}_f{}", j, i
                    );
                    fd.deri_map_[j] = rp_res.ren_img_.get_img_reader(
                        img_name, name_s()
                    );
                    MIRINAE_ASSERT(nullptr != fd.deri_map_[j]);
                }

                for (size_t j = 0; j < mirinae::CASCADE_COUNT; j++) {
                    const auto img_name = fmt::format(
                        "ocean_finalize:turbulence_c{}", j
                    );
                    auto img = rp_res.ren_img_.get_img_reader(
                        img_name, name_s()
                    );
                    for (auto& fd : frame_data_) {
                        fd.turb_map_[j] = img;
                    }
                    MIRINAE_ASSERT(nullptr != img);
                }
            }

            // Ubuf
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                mirinae::BufferCreateInfo buf_cinfo;
                buf_cinfo.preset_ubuf(sizeof(U_OceanTessParams));

                auto& fd = frame_data_[i];
                fd.ubuf_.init(buf_cinfo, device.mem_alloc());
            }

            // Descriptor layout
            {
                mirinae::DescLayoutBuilder builder{ name_s() + ":main" };
                builder
                    .new_binding()  // U_OceanTessParams
                    .set_type(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                    .set_count(1)
                    .add_stage(VK_SHADER_STAGE_VERTEX_BIT)
                    .add_stage(VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT)
                    .add_stage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
                    .add_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .finish_binding();
                builder
                    .new_binding()  // Displacement map
                    .set_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    .set_count(3)
                    .set_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .add_stage(VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
                    .finish_binding();
                builder
                    .new_binding()  // Derivative map
                    .set_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    .set_count(3)
                    .set_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .finish_binding();
                builder
                    .new_binding()  // Turbulance map
                    .set_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    .set_count(3)
                    .set_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .finish_binding();
                builder
                    .new_binding()  // Transmission LUT
                    .set_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    .set_count(1)
                    .set_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .finish_binding();
                builder
                    .new_binding()  // Sky view LUT
                    .set_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    .set_count(1)
                    .set_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .finish_binding();
                builder
                    .new_binding()  // Camera scattering volume
                    .set_type(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                    .set_count(1)
                    .set_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .finish_binding();
                builder  // Sky texture
                    .add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);
                rp_res.desclays_.add(builder, device.logi_device());
            }

            // Desciptor Sets
            {
                desc_pool_.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    rp_res.desclays_.get(name_s() + ":main").size_info(),
                    device.logi_device()
                );

                auto desc_sets = desc_pool_.alloc(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    rp_res.desclays_.get(name_s() + ":main").layout(),
                    device.logi_device()
                );

                mirinae::DescWriter writer;
                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = frame_data_[i];
                    fd.desc_set_ = desc_sets[i];

                    writer  // U_OceanTessParams
                        .add_buf_info(fd.ubuf_)
                        .add_buf_write(fd.desc_set_, 0);
                    writer  // Height maps
                        .add_img_info()
                        .set_img_view(fd.disp_map_[0]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_img_info()
                        .set_img_view(fd.disp_map_[1]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_img_info()
                        .set_img_view(fd.disp_map_[2]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_sampled_img_write(fd.desc_set_, 1);
                    writer  // Normal maps
                        .add_img_info()
                        .set_img_view(fd.deri_map_[0]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_img_info()
                        .set_img_view(fd.deri_map_[1]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_img_info()
                        .set_img_view(fd.deri_map_[2]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_sampled_img_write(fd.desc_set_, 2);
                    writer  // Turbulance maps
                        .add_img_info()
                        .set_img_view(fd.turb_map_[0]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_img_info()
                        .set_img_view(fd.turb_map_[1]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_img_info()
                        .set_img_view(fd.turb_map_[2]->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_GENERAL);
                    writer.add_sampled_img_write(fd.desc_set_, 3);

                    // Transmission LUT
                    writer.add_img_info()
                        .set_img_view(fd.trans_lut_->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 4);
                    // Sky View LUT
                    writer.add_img_info()
                        .set_img_view(fd.sky_view_lut_->view_.get())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 5);
                    // Cam Scat Vol
                    writer.add_img_info()
                        .set_img_view(fd.cam_scat_vol_->view_.get())
                        .set_sampler(device.samplers().get_cubemap())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 6);
                    // Sky Texture
                    writer.add_img_info()
                        .set_img_view(sky_tex_->image_view())
                        .set_sampler(device.samplers().get_linear())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(fd.desc_set_, 7);
                }
                writer.apply_all(device.logi_device());
            }

            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}
                    .desc(rp_res.desclays_.get(name_s() + ":main").layout())
                    .add_vertex_flag()
                    .add_tesc_flag()
                    .add_tese_flag()
                    .add_frag_flag()
                    .pc<U_OceanTessPushConst>(0)
                    .build(pipe_layout_, device);
            }

            // Render pass
            this->recreate_render_pass(render_pass_, device);

            // Pipeline
            this->recreate_pipeline(pipeline_, device);

            // Framebuffers
            this->recreate_fbufs(frame_data_, device);

            // Misc
            {
                clear_values_.at(0).color = { 0.0f, 0.0f, 0.0f, 1.0f };
                clear_values_.at(1).depthStencil = { 0, 0 };
            }

            return;
        }

        ~RpStatesOceanTess() override {
            auto& ren_img = rp_res_.ren_img_;

            for (auto& fd : frame_data_) {
                for (size_t i = 0; i < mirinae::CASCADE_COUNT; i++) {
                    ren_img.free_img(fd.disp_map_[i]->id(), name_s());
                    ren_img.free_img(fd.deri_map_[i]->id(), name_s());
                    ren_img.free_img(fd.turb_map_[i]->id(), name_s());
                }

                ren_img.free_img(fd.trans_lut_->id(), name_s());
                ren_img.free_img(fd.sky_view_lut_->id(), name_s());
                ren_img.free_img(fd.cam_scat_vol_->id(), name_s());

                fd.ubuf_.destroy();
                fd.fbuf_.destroy(device_.logi_device());
                fd.desc_set_ = VK_NULL_HANDLE;
            }

            desc_pool_.destroy(device_.logi_device());
            this->destroy_render_pass_elements(device_);
        }

        std::string_view name() const override { return "ocean_tess"; }

        void on_resize(uint32_t width, uint32_t height) override {
            this->recreate_render_pass(render_pass_, device_);
            this->recreate_pipeline(pipeline_, device_);
            this->recreate_fbufs(frame_data_, device_);
        }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            auto out = std::make_unique<task::RpTask>();
            out->init(
                cosmos_.reg(),
                rp_res_.gbuf_,
                *this,
                frame_data_,
                rp_res_.cmd_pool_,
                device_
            );
            return out;
        }

    private:
        void recreate_render_pass(
            mirinae::RenderPass& render_pass, mirinae::VulkanDevice& device
        ) const {
            mirinae::RenderPassBuilder builder;

            builder.attach_desc()
                .add(rp_res_.gbuf_.compo_format())
                .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .op_pair_load_store();
            builder.attach_desc()
                .add(rp_res_.gbuf_.depth_format())
                .ini_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .fin_lay(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
                .op_pair_load_store();

            builder.color_attach_ref().add_color_attach(0);
            builder.depth_attach_ref().set(1);

            builder.subpass_dep().add().preset_single();

            render_pass.reset(builder.build(device.logi_device()), device);
        }

        void recreate_pipeline(
            mirinae::RpPipeline& pipeline, mirinae::VulkanDevice& device
        ) const {
            mirinae::PipelineBuilder builder{ device };

            builder.shader_stages()
                .add_vert(":asset/spv/ocean_tess_vert.spv")
                .add_tesc(":asset/spv/ocean_tess_tesc.spv")
                .add_tese(":asset/spv/ocean_tess_tese.spv")
                .add_frag(":asset/spv/ocean_tess_frag.spv");

            builder.input_assembly_state().topology_patch_list();

            builder.tes_state().patch_ctrl_points(4);

            builder.rasterization_state().cull_mode_back();
            // builder.rasterization_state().polygon_mode_line();

            builder.depth_stencil_state()
                .depth_test_enable(true)
                .depth_write_enable(true);

            builder.color_blend_state().add(false, 1);

            builder.dynamic_state()
                .add(VK_DYNAMIC_STATE_LINE_WIDTH)
                .add_viewport()
                .add_scissor();

            pipeline.reset(builder.build(render_pass_, pipe_layout_), device);
        }

        void recreate_fbufs(
            ::FrameDataArr& fdata, mirinae::VulkanDevice& device
        ) const {
            for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                mirinae::FbufCinfo fbuf_cinfo;
                fbuf_cinfo.set_rp(render_pass_)
                    .set_dim(rp_res_.gbuf_.width(), rp_res_.gbuf_.height())
                    .add_attach(rp_res_.gbuf_.compo(i).image_view())
                    .add_attach(rp_res_.gbuf_.depth(i).image_view());
                fdata.at(i).fbuf_.reset(
                    fbuf_cinfo.build(device), device.logi_device()
                );
            }
        }

        mirinae::VulkanDevice& device_;
        mirinae::CosmosSimulator& cosmos_;
        mirinae::RpResources& rp_res_;

        ::FrameDataArr frame_data_;
        std::shared_ptr<mirinae::ITexture> sky_tex_;
        mirinae::DescPool desc_pool_;
    };

}  // namespace


namespace mirinae::rp {

    std::unique_ptr<mirinae::IRpBase> create_rp_ocean_tess(
        RpCreateBundle& bundle
    ) {
        return std::make_unique<RpStatesOceanTess>(
            bundle.cosmos_, bundle.rp_res_, bundle.device_
        );
    }

}  // namespace mirinae::rp
