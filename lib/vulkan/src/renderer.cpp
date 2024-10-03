#include "mirinae/renderer.hpp"

#include <spdlog/spdlog.h>

#include <daltools/common/util.h>
#include <sung/general/time.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/lightweight/script.hpp"
#include "mirinae/math/glm_fmt.hpp"
#include "mirinae/math/mamath.hpp"
#include "mirinae/overlay/overlay.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/renderpass.hpp"
#include "mirinae/render/renderpass/compo.hpp"
#include "mirinae/render/renderpass/envmap.hpp"
#include "mirinae/render/renderpass/gbuf.hpp"
#include "mirinae/render/renderpass/shadow.hpp"


namespace {

    using FrameIndex = mirinae::StrongType<int, struct FrameIndexStrongTypeTag>;


    bool is_fbuf_too_small(uint32_t width, uint32_t height) {
        if (width < 5)
            return true;
        if (height < 5)
            return true;
        else
            return false;
    }

    template <typename T>
    std::pair<T, T> calc_scaled_dimensions(T w, T h, double factor) {
        return std::make_pair(
            static_cast<T>(static_cast<double>(w) * factor),
            static_cast<T>(static_cast<double>(h) * factor)
        );
    }


    class DominantCommandProc : public mirinae::IInputProcessor {

    public:
        DominantCommandProc(mirinae::VulkanDevice& device) : device_(device) {}

        bool on_key_event(const mirinae::key::Event& e) override {
            keys_.notify(e);

            if (e.key == mirinae::key::KeyCode::enter) {
                if (keys_.is_pressed(mirinae::key::KeyCode::lalt)) {
                    if (e.action_type == mirinae::key::ActionType::up) {
                        device_.osio().toggle_fullscreen();
                    }
                    return true;
                }
            }

            return false;
        }

    private:
        mirinae::key::EventAnalyzer keys_;
        mirinae::VulkanDevice& device_;
    };


    class FrameSync {

    public:
        void init(VkDevice logi_device) {
            this->destroy(logi_device);

            for (auto& x : img_available_semaphores_) x.init(logi_device);
            for (auto& x : render_finished_semaphores_) x.init(logi_device);
            for (auto& x : in_flight_fences_) x.init(true, logi_device);
        }

        void destroy(VkDevice logi_device) {
            for (auto& x : img_available_semaphores_) x.destroy(logi_device);
            for (auto& x : render_finished_semaphores_) x.destroy(logi_device);
            for (auto& x : in_flight_fences_) x.destroy(logi_device);
        }

        mirinae::Semaphore& get_cur_img_ava_semaph() {
            return img_available_semaphores_.at(cur_frame_.get());
        }
        mirinae::Semaphore& get_cur_render_fin_semaph() {
            return render_finished_semaphores_.at(cur_frame_.get());
        }
        mirinae::Fence& get_cur_in_flight_fence() {
            return in_flight_fences_.at(cur_frame_.get());
        }

        FrameIndex get_frame_index() const { return cur_frame_; }
        void increase_frame_index() {
            cur_frame_ = (cur_frame_ + 1) % mirinae::MAX_FRAMES_IN_FLIGHT;
        }

    private:
        std::array<mirinae::Semaphore, mirinae::MAX_FRAMES_IN_FLIGHT>
            img_available_semaphores_;
        std::array<mirinae::Semaphore, mirinae::MAX_FRAMES_IN_FLIGHT>
            render_finished_semaphores_;
        std::array<mirinae::Fence, mirinae::MAX_FRAMES_IN_FLIGHT>
            in_flight_fences_;
        FrameIndex cur_frame_{ 0 };
    };


    class CascadeInfo {

    public:
        struct Cascade {
            std::array<glm::dvec3, 8> frustum_verts_;
            glm::dmat4 light_mat_;
            double near_;
            double far_;
        };

        void update(
            const double ratio,
            const glm::dmat4& view_inv,
            const mirinae::PerspectiveCamera<double>& pers,
            const mirinae::cpnt::DLight& dlight
        ) {
            const auto dist = this->make_plane_distances(pers.near_, pers.far_);

            for (size_t i = 0; i < dist.size() - 1; ++i) {
                auto& c = cascades_.at(i);

                c.near_ = dist[i];
                c.far_ = dist[i + 1];

                this->make_frustum_vertices(
                    ratio, c.near_, pers.fov_, view_inv, c.frustum_verts_.data()
                );

                this->make_frustum_vertices(
                    ratio,
                    c.far_,
                    pers.fov_,
                    view_inv,
                    c.frustum_verts_.data() + 4
                );

                c.light_mat_ = dlight.make_light_mat(c.frustum_verts_);

                far_depths_[i] = this->calc_clip_depth(
                    -c.far_, pers.near_, pers.far_
                );
            }

            return;
        }

        std::array<Cascade, 4> cascades_;
        std::array<double, 4> far_depths_;

    private:
        static void make_frustum_vertices(
            const double screen_ratio,
            const double plane_dist,
            const mirinae::Angle fov,
            const glm::dmat4& view_inv,
            glm::dvec3* const out
        ) {
            const auto tan_half_angle_vertical = std::tan(fov.rad() * 0.5);
            const auto tan_half_angle_horizontal = tan_half_angle_vertical *
                                                   screen_ratio;

            const auto half_width = plane_dist * tan_half_angle_horizontal;
            const auto half_height = plane_dist * tan_half_angle_vertical;

            out[0] = glm::dvec3{ -half_width, -half_height, -plane_dist };
            out[1] = glm::dvec3{ half_width, -half_height, -plane_dist };
            out[2] = glm::dvec3{ -half_width, half_height, -plane_dist };
            out[3] = glm::dvec3{ half_width, half_height, -plane_dist };

            for (size_t i = 0; i < 4; ++i)
                out[i] = view_inv * glm::dvec4{ out[i], 1 };
        }

        static std::array<double, 5> make_plane_distances(
            const double p_near, const double p_far
        ) {
            std::array<double, 5> out;
            const auto dist = p_far - p_near;

            out[0] = p_near;
            out[1] = p_near + dist * 0.05;
            out[2] = p_near + dist * 0.2;
            out[3] = p_near + dist * 0.5;
            out[4] = p_far;

            return out;
        }

        double calc_clip_depth(double z, double n, double f) {
            return (f * (z + n)) / (z * (f - n));
        }
    };


    class ShadowMapPool {

    public:
        struct Item {
            auto width() const { return tex_->width(); }
            auto height() const { return tex_->height(); }
            VkFramebuffer fbuf() { return fbuf_.get(); }

            std::unique_ptr<mirinae::ITexture> tex_;
            mirinae::Fbuf fbuf_;
            glm::dmat4 mat_;
        };

        size_t size() const { return shadow_maps_.size(); }

        auto begin() { return shadow_maps_.begin(); }
        auto end() { return shadow_maps_.end(); }

        Item& at(size_t index) { return shadow_maps_.at(index); }
        VkImageView get_img_view_at(size_t index) const {
            return shadow_maps_.at(index).tex_->image_view();
        }

        void add(
            uint32_t width, uint32_t height, mirinae::TextureManager& tex_man
        ) {
            auto& added = shadow_maps_.emplace_back();
            added.tex_ = tex_man.create_depth(width, height);
        }

        void recreate_fbufs(
            const mirinae::IRenderPassBundle& rp, mirinae::VulkanDevice& device
        ) {
            for (auto& x : shadow_maps_) {
                mirinae::FbufCinfo fbuf_info;
                fbuf_info.set_rp(rp.renderpass())
                    .add_attach(x.tex_->image_view())
                    .set_dim(x.width(), x.height());
                x.fbuf_.init(fbuf_info.get(), device.logi_device());
            }
        }

        void destroy_fbufs(mirinae::VulkanDevice& device) {
            for (auto& x : shadow_maps_) {
                x.fbuf_.destroy(device.logi_device());
            }
        }

    private:
        std::vector<Item> shadow_maps_;
    };

}  // namespace


namespace {

    struct DrawSheet {
        struct StaticRenderPairs {
            struct Actor {
                mirinae::RenderActor* actor_;
                glm::dmat4 model_mat_;
            };

            mirinae::RenderModel* model_;
            std::vector<Actor> actors_;
        };

        struct SkinnedRenderPairs {
            struct Actor {
                mirinae::RenderActorSkinned* actor_;
                glm::dmat4 model_mat_;
            };

            mirinae::RenderModelSkinned* model_;
            std::vector<Actor> actors_;
        };

        StaticRenderPairs& get_static_pair(mirinae::RenderModel& model) {
            for (auto& x : static_pairs_) {
                if (x.model_ == &model)
                    return x;
            }

            auto& output = static_pairs_.emplace_back();
            output.model_ = &model;
            return output;
        }

        SkinnedRenderPairs& get_skinn_pair(mirinae::RenderModelSkinned& model) {
            for (auto& x : skinned_pairs_) {
                if (x.model_ == &model)
                    return x;
            }

            auto& output = skinned_pairs_.emplace_back();
            output.model_ = &model;
            return output;
        }

        std::vector<StaticRenderPairs> static_pairs_;
        std::vector<SkinnedRenderPairs> skinned_pairs_;
    };

    DrawSheet make_draw_sheet(mirinae::Scene& scene) {
        using CTrans = mirinae::cpnt::Transform;
        using CStaticModelActor = mirinae::cpnt::StaticActorVk;
        using CSkinnedModelActor = mirinae::cpnt::SkinnedActorVk;

        DrawSheet sheet;

        for (auto enttid : scene.reg_.view<CTrans, CStaticModelActor>()) {
            auto& pair = scene.reg_.get<CStaticModelActor>(enttid);
            auto& trans = scene.reg_.get<CTrans>(enttid);

            auto& dst = sheet.get_static_pair(*pair.model_);
            auto& actor = dst.actors_.emplace_back();
            actor.actor_ = pair.actor_.get();
            actor.model_mat_ = trans.make_model_mat();
        }

        for (auto& enttid : scene.reg_.view<CTrans, CSkinnedModelActor>()) {
            auto& pair = scene.reg_.get<CSkinnedModelActor>(enttid);
            auto& trans = scene.reg_.get<CTrans>(enttid);

            auto& dst = sheet.get_skinn_pair(*pair.model_);
            auto& actor = dst.actors_.emplace_back();
            actor.actor_ = pair.actor_.get();
            actor.model_mat_ = trans.make_model_mat();
        }

        return sheet;
    }

}  // namespace


// Render pass states
namespace {

    const glm::dvec3 DVEC_ZERO{ 0, 0, 0 };
    const glm::dvec3 DVEC_DOWN{ 0, -1, 0 };

    const std::array<glm::dmat4, 6> CUBE_VIEW_MATS{
        glm::lookAt(DVEC_ZERO, glm::dvec3(1, 0, 0), DVEC_DOWN),
        glm::lookAt(DVEC_ZERO, glm::dvec3(-1, 0, 0), DVEC_DOWN),
        glm::lookAt(DVEC_ZERO, glm::dvec3(0, 1, 0), glm::dvec3(0, 0, 1)),
        glm::lookAt(DVEC_ZERO, DVEC_DOWN, glm::dvec3(0, 0, -1)),
        glm::lookAt(DVEC_ZERO, glm::dvec3(0, 0, 1), DVEC_DOWN),
        glm::lookAt(DVEC_ZERO, glm::dvec3(0, 0, -1), DVEC_DOWN)
    };


    class RpStatesEnvmap {

    public:
        void init(
            mirinae::RenderPassPackage& rp_pkg,
            mirinae::TextureManager& tex_man,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        ) {
            desc_pool_.init(
                5,
                desclayouts.get("envdiffuse:main").size_info() +
                    desclayouts.get("env_sky:main").size_info(),
                device.logi_device()
            );

            auto& added = cube_map_.emplace_back();
            added.init(rp_pkg, tex_man, desc_pool_, desclayouts, device);
            added.world_pos_ = { 0.14983922321477,
                                 0.66663010560478,
                                 -1.1615585516897 };

            brdf_lut_.init(512, 512, rp_pkg, device);

            sky_tex_ = tex_man.request(
                ":asset/textures/kloofendal_48d_partly_cloudy_puresky_1k.hdr",
                false
            );
            assert(sky_tex_);

            desc_set_ = desc_pool_.alloc(
                desclayouts.get("env_sky:main").layout(), device.logi_device()
            );

            mirinae::DescWriteInfoBuilder desc_info;
            desc_info.set_descset(desc_set_)
                .add_img_sampler(
                    sky_tex_->image_view(), device.samplers().get_linear()
                )
                .apply_all(device.logi_device());

            timer_.set_min();
        }

        void destroy(mirinae::VulkanDevice& device) {
            for (auto& x : cube_map_) x.destroy(device);
            cube_map_.clear();
            desc_pool_.destroy(device.logi_device());
            brdf_lut_.destroy(device);
        }

        void record(
            const VkCommandBuffer cur_cmd_buf,
            const ::DrawSheet& draw_sheet,
            const ::FrameIndex frame_index,
            const mirinae::CosmosSimulator& cosmos,
            const mirinae::ShainImageIndex image_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            if (timer_.check_if_elapsed(100)) {
                record_sky(cur_cmd_buf, desc_set_, rp_pkg);

                record_base(
                    cur_cmd_buf,
                    draw_sheet,
                    frame_index,
                    cosmos,
                    image_index,
                    rp_pkg
                );

                record_diffuse(
                    cur_cmd_buf, draw_sheet, frame_index, image_index, rp_pkg
                );
                record_specular(
                    cur_cmd_buf, draw_sheet, frame_index, image_index, rp_pkg
                );
            }
        }

        VkImageView diffuse_view(size_t index) const {
            return cube_map_.at(index).diffuse().cube_view();
        }
        VkImageView specular_view(size_t index) const {
            return cube_map_.at(index).specular().cube_view();
        }
        VkImageView brdf_lut_view() const { return brdf_lut_.view(); }
        VkImageView sky_tex_view() const { return sky_tex_->image_view(); }

        glm::dvec3& envmap_pos(size_t index) {
            return cube_map_.at(index).world_pos_;
        }

    private:
        class ColorCubeMap {

        public:
            bool init(
                uint32_t width,
                uint32_t height,
                mirinae::RenderPassPackage& rp_pkg,
                mirinae::TextureManager& tex_man,
                mirinae::VulkanDevice& device
            ) {
                mirinae::ImageCreateInfo cinfo;
                cinfo.set_format(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
                    .set_dimensions(width, height)
                    .set_mip_levels(1)
                    .set_arr_layers(6)
                    .add_usage_sampled()
                    .add_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                    .add_flag(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
                img_.init(cinfo.get(), device.mem_alloc());

                mirinae::ImageViewBuilder iv_builder;
                iv_builder.view_type(VK_IMAGE_VIEW_TYPE_CUBE)
                    .format(img_.format())
                    .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .arr_layers(6)
                    .image(img_.image());
                cubemap_view_.reset(iv_builder, device);

                iv_builder.view_type(VK_IMAGE_VIEW_TYPE_2D).arr_layers(1);
                for (uint32_t i = 0; i < 6; i++) {
                    iv_builder.base_arr_layer(i);
                    face_views_[i].reset(iv_builder, device);
                }

                for (uint32_t i = 0; i < 6; i++) {
                    mirinae::FbufCinfo fbuf_cinfo;
                    fbuf_cinfo.set_rp(rp_pkg.get("env_diffuse").renderpass())
                        .add_attach(face_views_[i].get())
                        .set_dim(width, height);
                    fbufs_[i].init(fbuf_cinfo.get(), device.logi_device());
                }

                return true;
            }

            void destroy(mirinae::VulkanDevice& device) {
                for (auto& x : fbufs_) x.destroy(device.logi_device());

                cubemap_view_.destroy(device);
                for (auto& x : face_views_) x.destroy(device);

                img_.destroy(device.mem_alloc());
            }

            uint32_t width() const { return img_.width(); }
            uint32_t height() const { return img_.height(); }
            VkExtent2D extent2d() const { return img_.extent2d(); }

            VkFramebuffer face_fbuf(size_t index) const {
                return fbufs_.at(index).get();
            }
            VkImageView face_view(size_t index) const {
                return face_views_.at(index).get();
            }
            VkImageView cube_view() const { return cubemap_view_.get(); }

        private:
            mirinae::Image img_;
            mirinae::ImageView cubemap_view_;
            std::array<mirinae::ImageView, 6> face_views_;
            std::array<mirinae::Fbuf, 6> fbufs_;
        };

        class ColorCubeMapWithMips {

        public:
            bool init(
                uint32_t base_width,
                uint32_t base_height,
                mirinae::RenderPassPackage& rp_pkg,
                mirinae::TextureManager& tex_man,
                mirinae::VulkanDevice& device
            ) {
                constexpr uint32_t MAX_MIP_LEVELS = 4;

                mirinae::ImageCreateInfo cinfo;
                cinfo.set_format(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
                    .set_dimensions(base_width, base_height)
                    .deduce_mip_levels()
                    .set_arr_layers(6)
                    .add_usage_sampled()
                    .add_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                    .add_flag(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
                if (cinfo.mip_levels() > MAX_MIP_LEVELS)
                    cinfo.set_mip_levels(MAX_MIP_LEVELS);
                img_.init(cinfo.get(), device.mem_alloc());

                mirinae::ImageViewBuilder iv_builder;
                iv_builder.view_type(VK_IMAGE_VIEW_TYPE_CUBE)
                    .format(img_.format())
                    .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .arr_layers(6)
                    .mip_levels(img_.mip_levels())
                    .image(img_.image());
                cubemap_view_.reset(iv_builder, device);

                iv_builder.view_type(VK_IMAGE_VIEW_TYPE_2D)
                    .arr_layers(1)
                    .mip_levels(1);

                mips_.resize(img_.mip_levels());
                for (uint32_t lvl = 0; lvl < img_.mip_levels(); ++lvl) {
                    auto& mip = mips_[lvl];

                    iv_builder.base_mip_level(lvl);
                    mip.roughness_ = static_cast<float>(lvl) /
                                     (img_.mip_levels() - 1);
                    mip.width_ = img_.width() >> lvl;
                    mip.height_ = img_.height() >> lvl;

                    for (uint32_t face_i = 0; face_i < 6; ++face_i) {
                        auto& face = mip.faces_[face_i];

                        iv_builder.base_arr_layer(face_i);
                        face.view_.reset(iv_builder, device);

                        mirinae::FbufCinfo fbuf_cinfo;
                        fbuf_cinfo
                            .set_rp(rp_pkg.get("env_diffuse").renderpass())
                            .add_attach(face.view_.get())
                            .set_dim(mip.width_, mip.height_);
                        face.fbuf_.init(fbuf_cinfo.get(), device.logi_device());
                    }
                }

                return true;
            }

            void destroy(mirinae::VulkanDevice& device) {
                for (auto& mip : mips_) mip.destroy(device);

                cubemap_view_.destroy(device);
                img_.destroy(device.mem_alloc());
            }

            VkImageView cube_view() const { return cubemap_view_.get(); }

            auto base_width() const { return img_.width(); }
            auto base_height() const { return img_.height(); }

            auto& mips() const { return mips_; }

        private:
            struct FaceData {
                void destroy(mirinae::VulkanDevice& device) {
                    view_.destroy(device);
                    fbuf_.destroy(device.logi_device());
                }

                mirinae::ImageView view_;
                mirinae::Fbuf fbuf_;
            };

            struct MipData {
                void destroy(mirinae::VulkanDevice& device) {
                    for (auto& x : faces_) x.destroy(device);
                }

                VkExtent2D extent2d() const {
                    VkExtent2D out;
                    out.width = width_;
                    out.height = height_;
                    return out;
                }

                std::array<FaceData, 6> faces_;
                float roughness_ = 0.0;
                uint32_t width_ = 0;
                uint32_t height_ = 0;
            };

            mirinae::Image img_;
            mirinae::ImageView cubemap_view_;
            std::vector<MipData> mips_;
        };

        class ColorDepthCubeMap {

        public:
            bool init(
                uint32_t width,
                uint32_t height,
                mirinae::RenderPassPackage& rp_pkg,
                mirinae::TextureManager& tex_man,
                mirinae::VulkanDevice& device
            ) {
                mirinae::ImageCreateInfo cinfo;
                cinfo.set_format(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
                    .set_dimensions(width, height)
                    .deduce_mip_levels()
                    .set_arr_layers(6)
                    .add_usage_sampled()
                    .add_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                    .add_usage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                    .add_usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                    .add_flag(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
                img_.init(cinfo.get(), device.mem_alloc());

                mirinae::ImageViewBuilder iv_builder;
                iv_builder.view_type(VK_IMAGE_VIEW_TYPE_CUBE)
                    .format(img_.format())
                    .mip_levels(img_.mip_levels())
                    .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .arr_layers(6)
                    .image(img_.image());
                cubemap_view_.reset(iv_builder, device);

                iv_builder.view_type(VK_IMAGE_VIEW_TYPE_2D).arr_layers(1);
                for (uint32_t i = 0; i < 6; i++) {
                    iv_builder.base_arr_layer(i);
                    face_views_[i].reset(iv_builder, device);
                }
                iv_builder.mip_levels(1);
                for (uint32_t i = 0; i < 6; i++) {
                    iv_builder.base_arr_layer(i);
                    fbuf_face_views_[i].reset(iv_builder, device);
                }

                depth_map_ = tex_man.create_depth(img_.width(), img_.height());

                for (uint32_t i = 0; i < 6; i++) {
                    mirinae::FbufCinfo fbuf_cinfo;
                    fbuf_cinfo.set_rp(rp_pkg.get("env_base").renderpass())
                        .add_attach(depth_map_->image_view())
                        .add_attach(fbuf_face_views_[i].get())
                        .set_dim(img_.width(), img_.height());
                    fbufs_[i].init(fbuf_cinfo.get(), device.logi_device());
                }

                return true;
            }

            void destroy(mirinae::VulkanDevice& device) {
                for (auto& x : fbufs_) x.destroy(device.logi_device());

                cubemap_view_.destroy(device);
                for (auto& x : face_views_) x.destroy(device);
                for (auto& x : fbuf_face_views_) x.destroy(device);

                depth_map_.reset();
                img_.destroy(device.mem_alloc());
            }

            uint32_t width() const { return img_.width(); }
            uint32_t height() const { return img_.height(); }
            VkExtent2D extent2d() const { return img_.extent2d(); }

            VkFramebuffer face_fbuf(size_t index) const {
                return fbufs_.at(index).get();
            }
            VkImageView face_view(size_t index) const {
                return face_views_.at(index).get();
            }
            VkImageView cube_view() const { return cubemap_view_.get(); }

        public:
            mirinae::Image img_;
            mirinae::Semaphore semaphores_;

        private:
            std::unique_ptr<mirinae::ITexture> depth_map_;
            mirinae::ImageView cubemap_view_;
            std::array<mirinae::ImageView, 6> face_views_;
            std::array<mirinae::ImageView, 6> fbuf_face_views_;
            std::array<mirinae::Fbuf, 6> fbufs_;
        };

        class CubeMap {

        public:
            bool init(
                mirinae::RenderPassPackage& rp_pkg,
                mirinae::TextureManager& tex_man,
                mirinae::DescPool& desc_pool,
                mirinae::DesclayoutManager& desclayouts,
                mirinae::VulkanDevice& device
            ) {
                if (!base_.init(256, 256, rp_pkg, tex_man, device))
                    return false;
                if (!diffuse_.init(256, 256, rp_pkg, tex_man, device))
                    return false;
                if (!specular_.init(128, 128, rp_pkg, tex_man, device))
                    return false;

                desc_set_ = desc_pool.alloc(
                    desclayouts.get("envdiffuse:main").layout(),
                    device.logi_device()
                );
                auto sampler = device.samplers().get_linear();
                mirinae::DescWriteInfoBuilder write;
                write.set_descset(desc_set_)
                    .add_img_sampler(base_.cube_view(), sampler)
                    .apply_all(device.logi_device());

                return true;
            }

            void destroy(mirinae::VulkanDevice& device) {
                base_.destroy(device);
                diffuse_.destroy(device);
                specular_.destroy(device);
            }

            const ColorDepthCubeMap& base() const { return base_; }
            const ColorCubeMap& diffuse() const { return diffuse_; }
            const ColorCubeMapWithMips& specular() const { return specular_; }
            VkDescriptorSet desc_set() const { return desc_set_; }

            glm::dvec3 world_pos_;

        private:
            ColorDepthCubeMap base_;
            ColorCubeMap diffuse_;
            ColorCubeMapWithMips specular_;
            VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
        };

        class BrdfLut {

        public:
            bool init(
                uint32_t width,
                uint32_t height,
                mirinae::RenderPassPackage& rp_pkg,
                mirinae::VulkanDevice& device
            ) {
                mirinae::ImageCreateInfo cinfo;
                cinfo.set_format(VK_FORMAT_R16G16_SFLOAT)
                    .set_dimensions(width, height)
                    .set_mip_levels(1)
                    .add_usage_sampled()
                    .add_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
                img_.init(cinfo.get(), device.mem_alloc());

                mirinae::ImageViewBuilder iv_builder;
                iv_builder.view_type(VK_IMAGE_VIEW_TYPE_2D)
                    .format(img_.format())
                    .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .image(img_.image());
                view_.reset(iv_builder, device);

                mirinae::FbufCinfo fbuf_cinfo;
                fbuf_cinfo.set_rp(rp_pkg.get("env_lut").renderpass())
                    .add_attach(view_.get())
                    .set_dim(width, height);
                fbuf_.init(fbuf_cinfo.get(), device.logi_device());

                mirinae::CommandPool pool;
                pool.init(device);
                const auto cmdbuf = pool.begin_single_time(device);
                this->record_drawing(cmdbuf, rp_pkg);
                pool.end_single_time(cmdbuf, device);
                device.wait_idle();
                pool.destroy(device.logi_device());

                return true;
            }

            void destroy(mirinae::VulkanDevice& device) {
                fbuf_.destroy(device.logi_device());
                view_.destroy(device);
                img_.destroy(device.mem_alloc());
            }

            VkImageView view() const { return view_.get(); }

        private:
            void record_drawing(
                const VkCommandBuffer cmdbuf,
                const mirinae::RenderPassPackage& rp_pkg
            ) {
                auto& rp = rp_pkg.get("env_lut");

                mirinae::RenderPassBeginInfo{}
                    .rp(rp.renderpass())
                    .fbuf(fbuf_.get())
                    .wh(img_.width(), img_.height())
                    .clear_value_count(rp.clear_value_count())
                    .clear_values(rp.clear_values())
                    .record_begin(cmdbuf);

                vkCmdBindPipeline(
                    cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                mirinae::Viewport{}
                    .set_wh(img_.width(), img_.height())
                    .record_single(cmdbuf);
                mirinae::Rect2D{}
                    .set_wh(img_.width(), img_.height())
                    .record_scissor(cmdbuf);

                vkCmdDraw(cmdbuf, 6, 1, 0, 0);
                vkCmdEndRenderPass(cmdbuf);
            }

            mirinae::Image img_;
            mirinae::ImageView view_;
            mirinae::Fbuf fbuf_;
        };

        void record_base(
            const VkCommandBuffer cur_cmd_buf,
            const ::DrawSheet& draw_sheet,
            const ::FrameIndex frame_index,
            const mirinae::CosmosSimulator& cosmos,
            const mirinae::ShainImageIndex image_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = rp_pkg.get("env_base");

            mirinae::RenderPassBeginInfo rp_info{};
            rp_info.rp(rp.renderpass())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values());

            const auto proj_mat = glm::perspectiveRH_ZO(
                mirinae::Angle::from_deg(90.0).rad(), 1.0, 0.1, 1000.0
            );

            mirinae::DescSetBindInfo descset_info{ rp.pipeline_layout() };

            for (auto& cube_map : cube_map_) {
                auto& base_cube = cube_map.base();
                const auto world_mat = glm::translate<double>(
                    glm::dmat4(1), -cube_map.world_pos_
                );

                mirinae::Viewport{}
                    .set_wh(base_cube.extent2d())
                    .record_single(cur_cmd_buf);
                mirinae::Rect2D{}
                    .set_wh(base_cube.extent2d())
                    .record_scissor(cur_cmd_buf);
                rp_info.wh(base_cube.width(), base_cube.height());

                for (int i = 0; i < 6; ++i) {
                    rp_info.fbuf(cube_map.base().face_fbuf(i))
                        .record_begin(cur_cmd_buf);

                    vkCmdBindPipeline(
                        cur_cmd_buf,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        rp.pipeline()
                    );

                    mirinae::U_EnvmapPushConst push_const;
                    for (auto e : cosmos.reg().view<mirinae::cpnt::DLight>()) {
                        const auto& light =
                            cosmos.reg().get<mirinae::cpnt::DLight>(e);
                        push_const.dlight_dir_ = glm::vec4{
                            light.calc_to_light_dir(glm::dmat4(1)), 0
                        };
                        push_const.dlight_color_ = glm::vec4{ light.color_, 0 };
                        break;
                    }

                    for (auto& pair : draw_sheet.static_pairs_) {
                        for (auto& unit : pair.model_->render_units_) {
                            descset_info.first_set(0)
                                .set(unit.get_desc_set(frame_index.get()))
                                .record(cur_cmd_buf);

                            unit.record_bind_vert_buf(cur_cmd_buf);

                            for (auto& actor : pair.actors_) {
                                descset_info.first_set(1)
                                    .set(actor.actor_->get_desc_set(
                                        frame_index.get()
                                    ))
                                    .record(cur_cmd_buf);

                                push_const.proj_view_ = proj_mat *
                                                        CUBE_VIEW_MATS[i] *
                                                        world_mat;
                                vkCmdPushConstants(
                                    cur_cmd_buf,
                                    rp.pipeline_layout(),
                                    VK_SHADER_STAGE_VERTEX_BIT,
                                    0,
                                    sizeof(mirinae::U_EnvmapPushConst),
                                    &push_const
                                );

                                vkCmdDrawIndexed(
                                    cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                                );
                            }
                        }
                    }

                    vkCmdEndRenderPass(cur_cmd_buf);
                }

                auto& img = cube_map.base().img_;
                for (uint32_t i = 1; i < img.mip_levels(); ++i) {
                    mirinae::ImageMemoryBarrier barrier;
                    barrier.image(img.image())
                        .set_src_access(VK_ACCESS_NONE)
                        .set_dst_access(VK_ACCESS_TRANSFER_WRITE_BIT)
                        .old_layout(VK_IMAGE_LAYOUT_UNDEFINED)
                        .new_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
                        .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                        .mip_base(i)
                        .mip_count(1)
                        .layer_base(0)
                        .layer_count(6);
                    barrier.record_single(
                        cur_cmd_buf,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT
                    );

                    mirinae::ImageBlit blit;
                    blit.set_src_offsets_full(
                        img.width() >> (i - 1), img.height() >> (i - 1)
                    );
                    blit.set_dst_offsets_full(
                        img.width() >> i, img.height() >> i
                    );
                    blit.src_subres()
                        .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                        .mip_level(i - 1)
                        .layer_base(0)
                        .layer_count(6);
                    blit.dst_subres()
                        .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                        .mip_level(i)
                        .layer_base(0)
                        .layer_count(6);

                    vkCmdBlitImage(
                        cur_cmd_buf,
                        img.image(),
                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        img.image(),
                        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        1,
                        &blit.get(),
                        VK_FILTER_LINEAR
                    );

                    barrier.image(img.image())
                        .set_src_access(VK_ACCESS_TRANSFER_WRITE_BIT)
                        .set_dst_access(VK_ACCESS_TRANSFER_READ_BIT)
                        .old_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
                        .new_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                    barrier.record_single(
                        cur_cmd_buf,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT
                    );
                }

                mirinae::ImageMemoryBarrier barrier;
                barrier.image(img.image())
                    .set_src_access(VK_ACCESS_TRANSFER_READ_BIT)
                    .set_dst_access(VK_ACCESS_SHADER_READ_BIT)
                    .old_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                    .new_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                    .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .mip_base(0)
                    .mip_count(img.mip_levels())
                    .layer_base(0)
                    .layer_count(6);
                barrier.record_single(
                    cur_cmd_buf,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );
            }
        }

        void record_sky(
            const VkCommandBuffer cur_cmd_buf,
            const VkDescriptorSet desc_set,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = rp_pkg.get("env_sky");

            mirinae::RenderPassBeginInfo rp_info{};
            rp_info.rp(rp.renderpass())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values());

            const auto proj_mat = glm::perspectiveRH_ZO(
                mirinae::Angle::from_deg(90.0).rad(), 1.0, 0.1, 1000.0
            );

            for (auto& cube_map : cube_map_) {
                auto& base_cube = cube_map.base();
                const auto world_mat = glm::translate<double>(
                    glm::dmat4(1), -cube_map.world_pos_
                );

                mirinae::Viewport{}
                    .set_wh(base_cube.width(), base_cube.height())
                    .record_single(cur_cmd_buf);
                mirinae::Rect2D{}
                    .set_wh(base_cube.width(), base_cube.height())
                    .record_scissor(cur_cmd_buf);
                rp_info.wh(base_cube.width(), base_cube.height());

                for (int i = 0; i < 6; ++i) {
                    rp_info.fbuf(cube_map.base().face_fbuf(i))
                        .record_begin(cur_cmd_buf);

                    vkCmdBindPipeline(
                        cur_cmd_buf,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        rp.pipeline()
                    );

                    mirinae::DescSetBindInfo{}
                        .layout(rp.pipeline_layout())
                        .set(desc_set)
                        .record(cur_cmd_buf);

                    mirinae::U_EnvSkyPushConst pc;
                    pc.proj_view_ = proj_mat * CUBE_VIEW_MATS[i];
                    vkCmdPushConstants(
                        cur_cmd_buf,
                        rp.pipeline_layout(),
                        VK_SHADER_STAGE_VERTEX_BIT,
                        0,
                        sizeof(mirinae::U_EnvSkyPushConst),
                        &pc
                    );

                    vkCmdDraw(cur_cmd_buf, 36, 1, 0, 0);

                    vkCmdEndRenderPass(cur_cmd_buf);
                }
            }
        }

        void record_diffuse(
            const VkCommandBuffer cur_cmd_buf,
            const ::DrawSheet& draw_sheet,
            const ::FrameIndex frame_index,
            const mirinae::ShainImageIndex image_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = rp_pkg.get("env_diffuse");

            mirinae::RenderPassBeginInfo rp_info{};
            rp_info.rp(rp.renderpass())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values());

            const auto proj_mat = glm::perspectiveRH_ZO(
                mirinae::Angle::from_deg(90.0).rad(), 1.0, 0.01, 10.0
            );

            mirinae::DescSetBindInfo descset_info{ rp.pipeline_layout() };

            for (auto& cube_map : cube_map_) {
                const auto& diffuse = cube_map.diffuse();
                const auto world_mat = glm::translate<double>(
                    glm::dmat4(1), -cube_map.world_pos_
                );

                const mirinae::Viewport viewport{ diffuse.extent2d() };
                const mirinae::Rect2D scissor{ diffuse.extent2d() };
                rp_info.wh(diffuse.extent2d());

                for (int i = 0; i < 6; ++i) {
                    rp_info.fbuf(diffuse.face_fbuf(i))
                        .record_begin(cur_cmd_buf);

                    vkCmdBindPipeline(
                        cur_cmd_buf,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        rp.pipeline()
                    );

                    viewport.record_single(cur_cmd_buf);
                    scissor.record_scissor(cur_cmd_buf);

                    descset_info.set(cube_map.desc_set()).record(cur_cmd_buf);

                    mirinae::U_EnvdiffusePushConst push_const;
                    push_const.proj_view_ = proj_mat * CUBE_VIEW_MATS[i];
                    vkCmdPushConstants(
                        cur_cmd_buf,
                        rp.pipeline_layout(),
                        VK_SHADER_STAGE_VERTEX_BIT,
                        0,
                        sizeof(mirinae::U_EnvdiffusePushConst),
                        &push_const
                    );

                    vkCmdDraw(cur_cmd_buf, 36, 1, 0, 0);
                    vkCmdEndRenderPass(cur_cmd_buf);
                }
            }
        }

        void record_specular(
            const VkCommandBuffer cur_cmd_buf,
            const ::DrawSheet& draw_sheet,
            const ::FrameIndex frame_index,
            const mirinae::ShainImageIndex image_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = rp_pkg.get("env_specular");

            mirinae::RenderPassBeginInfo rp_info{};
            rp_info.rp(rp.renderpass())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values());

            const auto proj_mat = glm::perspectiveRH_ZO(
                mirinae::Angle::from_deg(90.0).rad(), 1.0, 0.01, 10.0
            );

            mirinae::DescSetBindInfo descset_info{ rp.pipeline_layout() };

            for (auto& cube_map : cube_map_) {
                auto& specular = cube_map.specular();
                const auto world_mat = glm::translate<double>(
                    glm::dmat4(1), -cube_map.world_pos_
                );

                for (auto& mip : specular.mips()) {
                    const mirinae::Rect2D scissor{ mip.extent2d() };
                    const mirinae::Viewport viewport{ scissor.extent2d() };
                    rp_info.wh(scissor.extent2d());

                    for (int i = 0; i < 6; ++i) {
                        auto& face = mip.faces_[i];

                        rp_info.fbuf(face.fbuf_.get())
                            .record_begin(cur_cmd_buf);

                        vkCmdBindPipeline(
                            cur_cmd_buf,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            rp.pipeline()
                        );

                        viewport.record_single(cur_cmd_buf);
                        scissor.record_scissor(cur_cmd_buf);

                        descset_info.set(cube_map.desc_set())
                            .record(cur_cmd_buf);

                        mirinae::U_EnvSpecularPushConst push_const;
                        push_const.proj_view_ = proj_mat * CUBE_VIEW_MATS[i];
                        push_const.roughness_ = mip.roughness_;
                        vkCmdPushConstants(
                            cur_cmd_buf,
                            rp.pipeline_layout(),
                            VK_SHADER_STAGE_VERTEX_BIT |
                                VK_SHADER_STAGE_FRAGMENT_BIT,
                            0,
                            sizeof(mirinae::U_EnvSpecularPushConst),
                            &push_const
                        );

                        vkCmdDraw(cur_cmd_buf, 36, 1, 0, 0);
                        vkCmdEndRenderPass(cur_cmd_buf);
                    }
                }
            }
        }

        std::vector<CubeMap> cube_map_;
        mirinae::DescPool desc_pool_;
        sung::MonotonicRealtimeTimer timer_;
        BrdfLut brdf_lut_;
        std::shared_ptr<mirinae::ITexture> sky_tex_;
        VkDescriptorSet desc_set_ = VK_NULL_HANDLE;  // For env sky
    };


    class RpStatesShadow {

    public:
        void record_static(
            const VkCommandBuffer cur_cmd_buf,
            const ::DrawSheet& draw_sheet,
            const ::FrameIndex frame_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = rp_pkg.get("shadowmap");

            assert(shadow_maps_.size() == 2);

            {
                auto& shadow = shadow_maps_.at(0);

                mirinae::RenderPassBeginInfo{}
                    .rp(rp.renderpass())
                    .fbuf(shadow.fbuf())
                    .wh(shadow.tex_->extent())
                    .clear_value_count(rp.clear_value_count())
                    .clear_values(rp.clear_values())
                    .record_begin(cur_cmd_buf);

                vkCmdBindPipeline(
                    cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                const auto half_width = shadow.width() / 2.0;
                const auto half_height = shadow.height() / 2.0;
                const std::array<glm::dvec2, 4> offsets{
                    glm::dvec2{ 0, 0 },
                    glm::dvec2{ half_width, 0 },
                    glm::dvec2{ 0, half_height },
                    glm::dvec2{ half_width, half_height },
                };

                mirinae::DescSetBindInfo descset_info{ rp.pipeline_layout() };

                for (size_t cascade_i = 0; cascade_i < 4; ++cascade_i) {
                    const auto& cascade = cascade_info_.cascades_.at(cascade_i);
                    auto& offset = offsets.at(cascade_i);

                    mirinae::Viewport{}
                        .set_xy(offset)
                        .set_wh(half_width, half_height)
                        .record_single(cur_cmd_buf);
                    mirinae::Rect2D{}
                        .set_xy(offset)
                        .set_wh(half_width, half_height)
                        .record_scissor(cur_cmd_buf);

                    for (auto& pair : draw_sheet.static_pairs_) {
                        for (auto& unit : pair.model_->render_units_) {
                            unit.record_bind_vert_buf(cur_cmd_buf);

                            for (auto& actor : pair.actors_) {
                                descset_info
                                    .set(actor.actor_->get_desc_set(
                                        frame_index.get()
                                    ))
                                    .record(cur_cmd_buf);

                                mirinae::U_ShadowPushConst push_const;
                                push_const.pvm_ = cascade.light_mat_ *
                                                  actor.model_mat_;

                                vkCmdPushConstants(
                                    cur_cmd_buf,
                                    rp.pipeline_layout(),
                                    VK_SHADER_STAGE_VERTEX_BIT,
                                    0,
                                    sizeof(push_const),
                                    &push_const
                                );

                                vkCmdDrawIndexed(
                                    cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                                );
                            }
                        }
                    }
                }

                vkCmdEndRenderPass(cur_cmd_buf);
            }

            {
                auto& shadow = shadow_maps_.at(1);

                mirinae::RenderPassBeginInfo{}
                    .rp(rp.renderpass())
                    .fbuf(shadow.fbuf())
                    .wh(shadow.tex_->extent())
                    .clear_value_count(rp.clear_value_count())
                    .clear_values(rp.clear_values())
                    .record_begin(cur_cmd_buf);

                vkCmdBindPipeline(
                    cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                mirinae::Viewport{}
                    .set_wh(shadow.width(), shadow.height())
                    .record_single(cur_cmd_buf);
                mirinae::Rect2D{}
                    .set_wh(shadow.tex_->extent())
                    .record_scissor(cur_cmd_buf);

                mirinae::DescSetBindInfo descset_info{ rp.pipeline_layout() };

                for (auto& pair : draw_sheet.static_pairs_) {
                    for (auto& unit : pair.model_->render_units_) {
                        auto unit_desc = unit.get_desc_set(frame_index.get());
                        unit.record_bind_vert_buf(cur_cmd_buf);

                        for (auto& actor : pair.actors_) {
                            descset_info
                                .set(actor.actor_->get_desc_set(frame_index.get(
                                )))
                                .record(cur_cmd_buf);

                            mirinae::U_ShadowPushConst push_const;
                            push_const.pvm_ = shadow.mat_ * actor.model_mat_;

                            vkCmdPushConstants(
                                cur_cmd_buf,
                                rp.pipeline_layout(),
                                VK_SHADER_STAGE_VERTEX_BIT,
                                0,
                                sizeof(push_const),
                                &push_const
                            );

                            vkCmdDrawIndexed(
                                cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                            );
                        }
                    }
                }
                vkCmdEndRenderPass(cur_cmd_buf);
            }
        }

        void record_skinned(
            const VkCommandBuffer cur_cmd_buf,
            const ::DrawSheet& draw_sheet,
            const ::FrameIndex frame_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = rp_pkg.get("shadowmap_skin");

            {
                auto& shadow = shadow_maps_.at(0);

                mirinae::RenderPassBeginInfo{}
                    .rp(rp.renderpass())
                    .fbuf(shadow.fbuf())
                    .wh(shadow.tex_->extent())
                    .clear_value_count(rp.clear_value_count())
                    .clear_values(rp.clear_values())
                    .record_begin(cur_cmd_buf);

                vkCmdBindPipeline(
                    cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                const auto half_width = shadow.width() / 2.0;
                const auto half_height = shadow.height() / 2.0;
                const std::array<glm::dvec2, 4> offsets{
                    glm::dvec2{ 0, 0 },
                    glm::dvec2{ half_width, 0 },
                    glm::dvec2{ 0, half_height },
                    glm::dvec2{ half_width, half_height },
                };

                mirinae::DescSetBindInfo descset_info{ rp.pipeline_layout() };

                for (size_t cascade_i = 0; cascade_i < 4; ++cascade_i) {
                    const auto& cascade = cascade_info_.cascades_.at(cascade_i);
                    auto& offset = offsets.at(cascade_i);

                    mirinae::Viewport{}
                        .set_xy(offset)
                        .set_wh(half_width, half_height)
                        .record_single(cur_cmd_buf);
                    mirinae::Rect2D{}
                        .set_xy(offset)
                        .set_wh(half_width, half_height)
                        .record_scissor(cur_cmd_buf);

                    for (auto& pair : draw_sheet.skinned_pairs_) {
                        for (auto& unit : pair.model_->runits_) {
                            auto unit_desc = unit.get_desc_set(frame_index.get()
                            );
                            unit.record_bind_vert_buf(cur_cmd_buf);

                            for (auto& actor : pair.actors_) {
                                descset_info
                                    .set(actor.actor_->get_desc_set(
                                        frame_index.get()
                                    ))
                                    .record(cur_cmd_buf);

                                mirinae::U_ShadowPushConst push_const;
                                push_const.pvm_ = cascade.light_mat_ *
                                                  actor.model_mat_;

                                vkCmdPushConstants(
                                    cur_cmd_buf,
                                    rp.pipeline_layout(),
                                    VK_SHADER_STAGE_VERTEX_BIT,
                                    0,
                                    sizeof(push_const),
                                    &push_const
                                );

                                vkCmdDrawIndexed(
                                    cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                                );
                            }
                        }
                    }
                }

                vkCmdEndRenderPass(cur_cmd_buf);
            }

            {
                auto& shadow = shadow_maps_.at(1);

                mirinae::RenderPassBeginInfo{}
                    .rp(rp.renderpass())
                    .fbuf(shadow.fbuf())
                    .wh(shadow.tex_->extent())
                    .clear_value_count(rp.clear_value_count())
                    .clear_values(rp.clear_values())
                    .record_begin(cur_cmd_buf);

                vkCmdBindPipeline(
                    cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                mirinae::Viewport{}
                    .set_wh(shadow.width(), shadow.height())
                    .record_single(cur_cmd_buf);
                mirinae::Rect2D{}
                    .set_wh(shadow.width(), shadow.height())
                    .record_scissor(cur_cmd_buf);

                mirinae::DescSetBindInfo descset_info{ rp.pipeline_layout() };

                for (auto& pair : draw_sheet.skinned_pairs_) {
                    for (auto& unit : pair.model_->runits_) {
                        auto unit_desc = unit.get_desc_set(frame_index.get());
                        unit.record_bind_vert_buf(cur_cmd_buf);

                        for (auto& actor : pair.actors_) {
                            descset_info
                                .set(actor.actor_->get_desc_set(frame_index.get(
                                )))
                                .record(cur_cmd_buf);

                            mirinae::U_ShadowPushConst push_const;
                            push_const.pvm_ = shadow.mat_ * actor.model_mat_;

                            vkCmdPushConstants(
                                cur_cmd_buf,
                                rp.pipeline_layout(),
                                VK_SHADER_STAGE_VERTEX_BIT,
                                0,
                                sizeof(push_const),
                                &push_const
                            );

                            vkCmdDrawIndexed(
                                cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                            );
                        }
                    }
                }
                vkCmdEndRenderPass(cur_cmd_buf);
            }
        }

        ::ShadowMapPool& pool() { return shadow_maps_; }
        ::CascadeInfo& cascade() { return cascade_info_; }

    private:
        ::ShadowMapPool shadow_maps_;
        ::CascadeInfo cascade_info_;
    };


    class RpStatesGbuf {

    public:
        void record_static(
            const VkCommandBuffer cur_cmd_buf,
            const VkExtent2D& fbuf_exd,
            const ::DrawSheet& draw_sheet,
            const ::FrameIndex frame_index,
            const mirinae::ShainImageIndex image_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = rp_pkg.get("gbuf");

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(rp.fbuf_at(image_index.get()))
                .wh(fbuf_exd)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cur_cmd_buf);

            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ fbuf_exd }.record_single(cur_cmd_buf);
            mirinae::Rect2D{ fbuf_exd }.record_scissor(cur_cmd_buf);

            mirinae::DescSetBindInfo descset_info{ rp.pipeline_layout() };

            for (auto& pair : draw_sheet.static_pairs_) {
                for (auto& unit : pair.model_->render_units_) {
                    descset_info.first_set(0)
                        .set(unit.get_desc_set(frame_index.get()))
                        .record(cur_cmd_buf);

                    unit.record_bind_vert_buf(cur_cmd_buf);

                    for (auto& actor : pair.actors_) {
                        descset_info.first_set(1)
                            .set(actor.actor_->get_desc_set(frame_index.get()))
                            .record(cur_cmd_buf);

                        vkCmdDrawIndexed(
                            cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                        );
                    }
                }
            }

            vkCmdEndRenderPass(cur_cmd_buf);
        }

        void record_skinned(
            const VkCommandBuffer cur_cmd_buf,
            const VkExtent2D& fbuf_exd,
            const ::DrawSheet& draw_sheet,
            const ::FrameIndex frame_index,
            const mirinae::ShainImageIndex image_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = rp_pkg.get("gbuf_skin");

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(rp.fbuf_at(image_index.get()))
                .wh(fbuf_exd)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cur_cmd_buf);

            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ fbuf_exd }.record_single(cur_cmd_buf);
            mirinae::Rect2D{ fbuf_exd }.record_scissor(cur_cmd_buf);

            mirinae::DescSetBindInfo descset_info{ rp.pipeline_layout() };

            for (auto& pair : draw_sheet.skinned_pairs_) {
                for (auto& unit : pair.model_->runits_) {
                    descset_info.first_set(0)
                        .set(unit.get_desc_set(frame_index.get()))
                        .record(cur_cmd_buf);

                    unit.record_bind_vert_buf(cur_cmd_buf);

                    for (auto& actor : pair.actors_) {
                        descset_info.first_set(1)
                            .set(actor.actor_->get_desc_set(frame_index.get()))
                            .record(cur_cmd_buf);

                        vkCmdDrawIndexed(
                            cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                        );
                    }
                }
            }

            vkCmdEndRenderPass(cur_cmd_buf);
        }
    };


    class RpStatesCompo {

    public:
        void init(
            mirinae::DesclayoutManager& desclayouts,
            mirinae::FbufImageBundle& fbufs,
            VkImageView dlight_shadowmap,
            VkImageView slight_shadowmap,
            VkImageView env_diffuse,
            VkImageView env_specular,
            VkImageView env_lut,
            mirinae::VulkanDevice& device
        ) {
            auto& desclayout = desclayouts.get("compo:main");
            desc_pool_.init(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                desclayout.size_info(),
                device.logi_device()
            );
            desc_sets_ = desc_pool_.alloc(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                desclayout.layout(),
                device.logi_device()
            );

            const auto sam_lin = device.samplers().get_linear();
            const auto sam_nea = device.samplers().get_nearest();
            const auto sam_cube = device.samplers().get_cubemap();
            mirinae::DescWriteInfoBuilder builder;
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& ubuf = ubufs_.emplace_back();
                ubuf.init_ubuf(
                    sizeof(mirinae::U_CompoMain), device.mem_alloc()
                );

                builder.set_descset(desc_sets_.at(i))
                    .add_img_sampler(fbufs.depth().image_view(), sam_lin)
                    .add_img_sampler(fbufs.albedo().image_view(), sam_lin)
                    .add_img_sampler(fbufs.normal().image_view(), sam_lin)
                    .add_img_sampler(fbufs.material().image_view(), sam_lin)
                    .add_ubuf(ubuf)
                    .add_img_sampler(dlight_shadowmap, sam_nea)
                    .add_img_sampler(slight_shadowmap, sam_nea)
                    .add_img_sampler(env_diffuse, sam_cube)
                    .add_img_sampler(env_specular, sam_cube)
                    .add_img_sampler(env_lut, sam_lin);
            }
            builder.apply_all(device.logi_device());
        }

        void destroy(mirinae::VulkanDevice& device) {
            desc_pool_.destroy(device.logi_device());

            for (auto& ubuf : ubufs_) ubuf.destroy(device.mem_alloc());
            ubufs_.clear();
        }

        void record(
            const VkCommandBuffer cur_cmd_buf,
            const VkExtent2D& fbuf_ext,
            const ::FrameIndex frame_index,
            const mirinae::ShainImageIndex image_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = rp_pkg.get("compo");

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(rp.fbuf_at(image_index.get()))
                .wh(fbuf_ext)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cur_cmd_buf);

            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ fbuf_ext }.record_single(cur_cmd_buf);
            mirinae::Rect2D{ fbuf_ext }.record_scissor(cur_cmd_buf);

            mirinae::DescSetBindInfo{}
                .layout(rp.pipeline_layout())
                .set(desc_sets_.at(frame_index.get()))
                .record(cur_cmd_buf);

            vkCmdDraw(cur_cmd_buf, 3, 1, 0, 0);

            vkCmdEndRenderPass(cur_cmd_buf);
        }

        mirinae::DescPool desc_pool_;
        std::vector<VkDescriptorSet> desc_sets_;
        std::vector<mirinae::Buffer> ubufs_;
    };


    class RpStatesCompoSky {

    public:
        void init(
            VkImageView sky_texture,
            mirinae::RenderPassPackage& rp_pkg,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::FbufImageBundle& fbufs,
            mirinae::Swapchain& shain,
            mirinae::VulkanDevice& device
        ) {
            auto& desclayout = desclayouts.get("compo_sky:main");
            desc_pool_.init(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                desclayout.size_info(),
                device.logi_device()
            );
            desc_sets_ = desc_pool_.alloc(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                desclayout.layout(),
                device.logi_device()
            );

            const auto sam_lin = device.samplers().get_linear();
            mirinae::DescWriteInfoBuilder builder;
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                builder.set_descset(desc_sets_.at(i))
                    .add_img_sampler(sky_texture, sam_lin);
            }
            builder.apply_all(device.logi_device());

            auto& rp = rp_pkg.get("compo_sky");

            mirinae::FbufCinfo fbuf_cinfo;
            fbuf_cinfo.set_rp(rp.renderpass())
                .set_dim(fbufs.width(), fbufs.height())
                .add_attach(fbufs.depth().image_view())
                .add_attach(fbufs.compo().image_view());
            for (int i = 0; i < shain.views_count(); ++i)
                fbufs_.push_back(fbuf_cinfo.build(device));
        }

        void destroy(mirinae::VulkanDevice& device) {
            desc_pool_.destroy(device.logi_device());

            for (auto& x : fbufs_)
                vkDestroyFramebuffer(device.logi_device(), x, nullptr);
            fbufs_.clear();
        }

        void record(
            const VkCommandBuffer cur_cmd_buf,
            const glm::mat4 proj_inv,
            const glm::mat4 view_inv,
            const VkExtent2D& fbuf_ext,
            const ::FrameIndex frame_index,
            const mirinae::ShainImageIndex image_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = rp_pkg.get("compo_sky");

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(fbufs_.at(image_index.get()))
                .wh(fbuf_ext)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cur_cmd_buf);

            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ fbuf_ext }.record_single(cur_cmd_buf);
            mirinae::Rect2D{ fbuf_ext }.record_scissor(cur_cmd_buf);

            mirinae::DescSetBindInfo{}
                .layout(rp.pipeline_layout())
                .set(desc_sets_.at(frame_index.get()))
                .record(cur_cmd_buf);

            mirinae::U_CompoSkyMain pc;
            pc.proj_inv_ = proj_inv;
            pc.view_inv_ = view_inv;

            vkCmdPushConstants(
                cur_cmd_buf,
                rp.pipeline_layout(),
                VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(mirinae::U_CompoSkyMain),
                &pc
            );

            vkCmdDraw(cur_cmd_buf, 3, 1, 0, 0);

            vkCmdEndRenderPass(cur_cmd_buf);
        }

    private:
        mirinae::DescPool desc_pool_;
        std::vector<VkDescriptorSet> desc_sets_;
        std::vector<VkFramebuffer> fbufs_;
    };


    class RpStatesTransp {

    public:
        void init(
            mirinae::DesclayoutManager& desclayouts,
            VkImageView dlight_shadowmap,
            VkImageView slight_shadowmap,
            VkImageView env_diffuse,
            VkImageView env_specular,
            VkImageView env_lut,
            mirinae::VulkanDevice& device
        ) {
            auto& desclayout = desclayouts.get("transp:frame");
            desc_pool_.init(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                desclayout.size_info(),
                device.logi_device()
            );
            desc_sets_ = desc_pool_.alloc(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                desclayout.layout(),
                device.logi_device()
            );

            const auto sam_nea = device.samplers().get_nearest();
            const auto sam_lin = device.samplers().get_linear();
            const auto sam_cube = device.samplers().get_cubemap();

            mirinae::DescWriteInfoBuilder builder;
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& ubuf = ubufs_.emplace_back();
                ubuf.init_ubuf(
                    sizeof(mirinae::U_TranspFrame), device.mem_alloc()
                );

                builder.set_descset(desc_sets_.at(i))
                    .add_ubuf(ubuf)
                    .add_img_sampler(dlight_shadowmap, sam_nea)
                    .add_img_sampler(slight_shadowmap, sam_nea)
                    .add_img_sampler(env_diffuse, sam_cube)
                    .add_img_sampler(env_specular, sam_cube)
                    .add_img_sampler(env_lut, sam_lin);
            }
            builder.apply_all(device.logi_device());
        }

        void destroy(mirinae::VulkanDevice& device) {
            desc_pool_.destroy(device.logi_device());

            for (auto& ubuf : ubufs_) ubuf.destroy(device.mem_alloc());
            ubufs_.clear();
        }

        void record_static(
            const VkCommandBuffer cur_cmd_buf,
            const VkExtent2D& fbuf_ext,
            const ::DrawSheet& draw_sheet,
            const ::FrameIndex frame_index,
            const mirinae::ShainImageIndex image_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = rp_pkg.get("transp");

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(rp.fbuf_at(image_index.get()))
                .wh(fbuf_ext)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cur_cmd_buf);

            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ fbuf_ext }.record_single(cur_cmd_buf);
            mirinae::Rect2D{ fbuf_ext }.record_scissor(cur_cmd_buf);

            mirinae::DescSetBindInfo descset_info{ rp.pipeline_layout() };
            descset_info.first_set(0)
                .set(desc_sets_.at(frame_index.get()))
                .record(cur_cmd_buf);

            for (auto& pair : draw_sheet.static_pairs_) {
                for (auto& unit : pair.model_->render_units_alpha_) {
                    descset_info.first_set(1)
                        .set(unit.get_desc_set(frame_index.get()))
                        .record(cur_cmd_buf);

                    unit.record_bind_vert_buf(cur_cmd_buf);

                    for (auto& actor : pair.actors_) {
                        descset_info.first_set(2)
                            .set(actor.actor_->get_desc_set(frame_index.get()))
                            .record(cur_cmd_buf);

                        vkCmdDrawIndexed(
                            cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                        );
                    }
                }
            }

            vkCmdEndRenderPass(cur_cmd_buf);
        }

        void record_skinned(
            const VkCommandBuffer cur_cmd_buf,
            const VkExtent2D& fbuf_ext,
            const ::DrawSheet& draw_sheet,
            const ::FrameIndex frame_index,
            const mirinae::ShainImageIndex image_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = rp_pkg.get("transp_skin");

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(rp.fbuf_at(image_index.get()))
                .wh(fbuf_ext)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cur_cmd_buf);

            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ fbuf_ext }.record_single(cur_cmd_buf);
            mirinae::Rect2D{ fbuf_ext }.record_scissor(cur_cmd_buf);

            mirinae::DescSetBindInfo descset_info{ rp.pipeline_layout() };
            descset_info.first_set(0)
                .set(desc_sets_.at(frame_index.get()))
                .record(cur_cmd_buf);

            for (auto& pair : draw_sheet.skinned_pairs_) {
                for (auto& unit : pair.model_->runits_alpha_) {
                    descset_info.first_set(1)
                        .set(unit.get_desc_set(frame_index.get()))
                        .record(cur_cmd_buf);

                    unit.record_bind_vert_buf(cur_cmd_buf);

                    for (auto& actor : pair.actors_) {
                        descset_info.first_set(2)
                            .set(actor.actor_->get_desc_set(frame_index.get()))
                            .record(cur_cmd_buf);

                        vkCmdDrawIndexed(
                            cur_cmd_buf, unit.vertex_count(), 1, 0, 0, 0
                        );
                    }
                }
            }

            vkCmdEndRenderPass(cur_cmd_buf);
        }

        mirinae::DescPool desc_pool_;
        std::vector<VkDescriptorSet> desc_sets_;
        std::vector<mirinae::Buffer> ubufs_;
    };


    class RpStatesDebugMesh {

    public:
        void init(mirinae::VulkanDevice& device) {}

        void destroy(mirinae::VulkanDevice& device) {}

        void begin_record(
            const VkCommandBuffer cmdbuf,
            const VkExtent2D& fbuf_ext,
            const mirinae::ShainImageIndex image_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = rp_pkg.get("debug_mesh");

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(rp.fbuf_at(image_index.get()))
                .wh(fbuf_ext)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cmdbuf);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ fbuf_ext }.record_single(cmdbuf);
            mirinae::Rect2D{ fbuf_ext }.record_scissor(cmdbuf);
        }

        void draw(
            const VkCommandBuffer cmdbuf,
            const glm::vec3& p0,
            const glm::vec3& p1,
            const glm::vec3& p2,
            const glm::vec4& color,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            mirinae::U_DebugMeshPushConst pc;
            pc.vertices_[0] = glm::vec4(p0, 1);
            pc.vertices_[1] = glm::vec4(p1, 1);
            pc.vertices_[2] = glm::vec4(p2, 1);
            pc.color_ = color;

            vkCmdPushConstants(
                cmdbuf,
                rp_pkg.get("debug_mesh").pipeline_layout(),
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0,
                sizeof(mirinae::U_DebugMeshPushConst),
                &pc
            );

            vkCmdDraw(cmdbuf, 3, 1, 0, 0);
        }

        void end_record(
            const VkCommandBuffer cmdbuf,
            const VkExtent2D& fbuf_ext,
            const mirinae::ShainImageIndex image_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            vkCmdEndRenderPass(cmdbuf);
        }
    };


    class RpStatesFillscreen {

    public:
        void init(
            mirinae::DesclayoutManager& desclayouts,
            mirinae::FbufImageBundle& fbufs,
            mirinae::VulkanDevice& device
        ) {
            auto& desclayout = desclayouts.get("fillscreen:main");
            desc_pool_.init(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                desclayout.size_info(),
                device.logi_device()
            );
            desc_sets_ = desc_pool_.alloc(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                desclayout.layout(),
                device.logi_device()
            );

            mirinae::DescWriteInfoBuilder builder;
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                builder.set_descset(desc_sets_.at(i))
                    .add_img_sampler(
                        fbufs.compo().image_view(),
                        device.samplers().get_linear()
                    );
            }
            builder.apply_all(device.logi_device());
        }

        void destroy(mirinae::VulkanDevice& device) {
            desc_pool_.destroy(device.logi_device());
        }

        void record(
            VkCommandBuffer cmdbuf,
            VkExtent2D shain_exd,
            ::FrameIndex frame_index,
            mirinae::ShainImageIndex image_index,
            mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = rp_pkg.get("fillscreen");

            mirinae::RenderPassBeginInfo{}
                .rp(rp.renderpass())
                .fbuf(rp.fbuf_at(image_index.get()))
                .wh(shain_exd)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cmdbuf);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ shain_exd }.record_single(cmdbuf);
            mirinae::Rect2D{ shain_exd }.record_scissor(cmdbuf);

            mirinae::DescSetBindInfo{}
                .layout(rp.pipeline_layout())
                .set(desc_sets_.at(frame_index.get()))
                .record(cmdbuf);

            vkCmdDraw(cmdbuf, 3, 1, 0, 0);

            vkCmdEndRenderPass(cmdbuf);
        }

        mirinae::DescPool desc_pool_;
        std::vector<VkDescriptorSet> desc_sets_;
    };

}  // namespace


// Engine
namespace {

    class RendererVulkan : public mirinae::IRenderer {

    public:
        RendererVulkan(
            mirinae::EngineCreateInfo&& cinfo,
            std::shared_ptr<mirinae::ScriptEngine>& script,
            std::shared_ptr<mirinae::CosmosSimulator>& cosmos,
            int init_width,
            int init_height
        )
            : device_(std::move(cinfo))
            , script_(script)
            , cosmos_(cosmos)
            , tex_man_(device_)
            , model_man_(device_)
            , desclayout_(device_)
            , overlay_man_(
                  init_width, init_height, desclayout_, tex_man_, device_
              )
            , fbuf_width_(init_width)
            , fbuf_height_(init_height) {
            // This must be the first member variable right after vtable pointer
            static_assert(offsetof(RendererVulkan, device_) == sizeof(void*));

            framesync_.init(device_.logi_device());

            rp_states_shadow_.pool().add(4096, 4096, tex_man_);
            rp_states_shadow_.pool().add(256, 256, tex_man_);

            this->create_swapchain_and_relatives(fbuf_width_, fbuf_height_);

            cmd_pool_.init(
                device_.graphics_queue_family_index().value(),
                device_.logi_device()
            );
            for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i)
                cmd_buf_.push_back(cmd_pool_.alloc(device_.logi_device()));

            {
                input_mgrs_.add(std::make_unique<DominantCommandProc>(device_));
                input_mgrs_.add(&overlay_man_);
            }

            // Widget: Dev console
            {
                dev_console_output_ = mirinae::create_text_blocks();
                script->replace_output_buf(dev_console_output_);

                auto w = mirinae::create_dev_console(
                    overlay_man_.text_render_data(),
                    desclayout_,
                    tex_man_,
                    *script,
                    device_
                );
                w->replace_output_buf(dev_console_output_);
                w->hide(true);
                overlay_man_.widgets().add_widget(std::move(w));
            }

            fps_timer_.set_fps_cap(120);
        }

        ~RendererVulkan() {
            device_.wait_idle();

            auto& reg = cosmos_->reg();
            for (auto enttid : reg.view<mirinae::cpnt::StaticActorVk>())
                reg.remove<mirinae::cpnt::StaticActorVk>(enttid);
            for (auto& enttid : reg.view<mirinae::cpnt::SkinnedActorVk>())
                reg.remove<mirinae::cpnt::SkinnedActorVk>(enttid);

            cmd_pool_.destroy(device_.logi_device());
            this->destroy_swapchain_and_relatives();
            framesync_.destroy(device_.logi_device());
        }

        void do_frame() override {
            const auto t = cosmos_->ftime().tp_;
            const auto delta_time = cosmos_->ftime().dt_;

            auto& cam = cosmos_->reg().get<mirinae::cpnt::StandardCamera>(
                cosmos_->scene().main_camera_
            );

            this->update_unloaded_models();

            const auto image_index_opt = this->try_acquire_image();
            if (!image_index_opt) {
                return;
            }
            const auto image_index = image_index_opt.value();

            const auto aspect_ratio = (double)swapchain_.width() /
                                      (double)swapchain_.height();
            const auto proj_mat = cam.proj_.make_proj_mat(
                swapchain_.width(), swapchain_.height()
            );
            const auto proj_inv = glm::inverse(proj_mat);
            const auto view_mat = cam.view_.make_view_mat();
            const auto view_inv = glm::inverse(view_mat);

            // Update widgets
            mirinae::WidgetRenderUniData widget_ren_data;
            widget_ren_data.win_dim_ = overlay_man_.win_dim();
            widget_ren_data.frame_index_ = framesync_.get_frame_index().get();
            widget_ren_data.cmd_buf_ = VK_NULL_HANDLE;
            widget_ren_data.pipe_layout_ = VK_NULL_HANDLE;
            overlay_man_.widgets().tick(widget_ren_data);

            const auto draw_sheet = ::make_draw_sheet(cosmos_->scene());
            auto cur_cmd_buf = cmd_buf_.at(framesync_.get_frame_index().get());

            for (auto& l : cosmos_->reg().view<mirinae::cpnt::DLight>()) {
                auto& dlight = cosmos_->reg().get<mirinae::cpnt::DLight>(l);
                dlight.transform_.pos_ = cam.view_.pos_;

                rp_states_shadow_.cascade().update(
                    swapchain_.calc_ratio(), view_inv, cam.proj_, dlight
                );
                rp_states_shadow_.pool().at(0).mat_ =
                    rp_states_shadow_.cascade().cascades_.front().light_mat_;

                break;
            }

            for (auto& l : cosmos_->reg().view<mirinae::cpnt::SLight>()) {
                auto& slight = cosmos_->reg().get<mirinae::cpnt::SLight>(l);
                slight.transform_.pos_ = cam.view_.pos_ +
                                         glm::dvec3{ 0, -0.1, 0 };
                slight.transform_.rot_ = cam.view_.rot_;
                slight.transform_.rotate(
                    sung::TAngle<double>::from_deg(std::atan(0.1 / 5.0)),
                    cam.view_.make_right_dir()
                );

                rp_states_shadow_.pool().at(1).mat_ = slight.make_light_mat();
                break;
            }

            this->update_ubufs(proj_mat, view_mat);

            // Begin recording
            {
                VK_CHECK(vkResetCommandBuffer(cur_cmd_buf, 0));

                VkCommandBufferBeginInfo beginInfo{};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.flags = 0;
                beginInfo.pInheritanceInfo = nullptr;
                VK_CHECK(vkBeginCommandBuffer(cur_cmd_buf, &beginInfo));

                std::array<VkClearValue, 3> clear_values;
                clear_values[0].depthStencil = { 1.f, 0 };
                clear_values[1].color = { 0.f, 0.f, 0.f, 1.f };
                clear_values[2].color = { 0.f, 0.f, 0.f, 1.f };
            }

            rp_states_envmap_.record(
                cur_cmd_buf,
                draw_sheet,
                framesync_.get_frame_index(),
                *cosmos_,
                image_index,
                rp_
            );

            rp_states_shadow_.record_static(
                cur_cmd_buf, draw_sheet, framesync_.get_frame_index(), rp_
            );

            rp_states_shadow_.record_skinned(
                cur_cmd_buf, draw_sheet, framesync_.get_frame_index(), rp_
            );

            rp_states_gbuf_.record_static(
                cur_cmd_buf,
                fbuf_images_.extent(),
                draw_sheet,
                framesync_.get_frame_index(),
                image_index,
                rp_
            );

            rp_states_gbuf_.record_skinned(
                cur_cmd_buf,
                fbuf_images_.extent(),
                draw_sheet,
                framesync_.get_frame_index(),
                image_index,
                rp_
            );

            rp_states_compo_.record(
                cur_cmd_buf,
                fbuf_images_.extent(),
                framesync_.get_frame_index(),
                image_index,
                rp_
            );

            rp_states_compo_sky_.record(
                cur_cmd_buf,
                proj_inv,
                view_inv,
                fbuf_images_.extent(),
                framesync_.get_frame_index(),
                image_index,
                rp_
            );

            rp_states_transp_.record_static(
                cur_cmd_buf,
                fbuf_images_.extent(),
                draw_sheet,
                framesync_.get_frame_index(),
                image_index,
                rp_
            );

            rp_states_transp_.record_skinned(
                cur_cmd_buf,
                fbuf_images_.extent(),
                draw_sheet,
                framesync_.get_frame_index(),
                image_index,
                rp_
            );

            rp_states_debug_mesh_.begin_record(
                cur_cmd_buf, fbuf_images_.extent(), image_index, rp_
            );
            rp_states_debug_mesh_.end_record(
                cur_cmd_buf, fbuf_images_.extent(), image_index, rp_
            );

            rp_states_fillscreen_.record(
                cur_cmd_buf,
                swapchain_.extent(),
                framesync_.get_frame_index(),
                image_index,
                rp_
            );

            // Shader: Overlay
            {
                auto& rp = rp_.get("overlay");

                mirinae::RenderPassBeginInfo{}
                    .rp(rp.renderpass())
                    .fbuf(rp.fbuf_at(image_index.get()))
                    .wh(swapchain_.extent())
                    .clear_value_count(rp.clear_value_count())
                    .clear_values(rp.clear_values())
                    .record_begin(cur_cmd_buf);

                vkCmdBindPipeline(
                    cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                mirinae::Viewport{}
                    .set_wh(swapchain_.width(), swapchain_.height())
                    .record_single(cur_cmd_buf);
                mirinae::Rect2D{}
                    .set_wh(swapchain_.width(), swapchain_.height())
                    .record_scissor(cur_cmd_buf);

                widget_ren_data.cmd_buf_ = cur_cmd_buf;
                widget_ren_data.pipe_layout_ = rp.pipeline_layout();
                overlay_man_.record_render(widget_ren_data);

                vkCmdEndRenderPass(cur_cmd_buf);
            }

            VK_CHECK(vkEndCommandBuffer(cur_cmd_buf));

            // Submit and present
            {
                const VkSemaphore signal_semaph =
                    framesync_.get_cur_render_fin_semaph().get();

                mirinae::SubmitInfo{}
                    .add_wait_semaph_color_attach_out(
                        framesync_.get_cur_img_ava_semaph().get()
                    )
                    .add_signal_semaph(signal_semaph)
                    .add_cmdbuf(cur_cmd_buf)
                    .queue_submit_single(
                        device_.graphics_queue(),
                        framesync_.get_cur_in_flight_fence().get()
                    );

                mirinae::PresentInfo{}
                    .add_wait_semaph(signal_semaph)
                    .add_swapchain(swapchain_.get())
                    .add_image_index(image_index.get())
                    .queue_present(device_.present_queue());
            }

            framesync_.increase_frame_index();
        }

        void notify_window_resize(uint32_t width, uint32_t height) override {
            fbuf_width_ = width;
            fbuf_height_ = height;
            fbuf_resized_ = true;
        }

        bool on_key_event(const mirinae::key::Event& e) override {
            if (input_mgrs_.on_key_event(e))
                return true;

            return false;
        }

        bool on_text_event(char32_t c) override {
            if (input_mgrs_.on_text_event(c))
                return true;

            return false;
        }

        bool on_mouse_event(const mirinae::mouse::Event& e) override {
            if (input_mgrs_.on_mouse_event(e))
                return true;

            return false;
        }

    private:
        void create_swapchain_and_relatives(
            uint32_t fbuf_width, uint32_t fbuf_height
        ) {
            device_.wait_idle();
            swapchain_.init(fbuf_width, fbuf_height, device_);

            const auto [gbuf_width, gbuf_height] = ::calc_scaled_dimensions(
                swapchain_.width(), swapchain_.height(), 0.9
            );
            fbuf_images_.init(gbuf_width, gbuf_height, tex_man_);

            mirinae::rp::gbuf::create_rp(
                rp_,
                fbuf_images_.width(),
                fbuf_images_.height(),
                fbuf_images_,
                desclayout_,
                swapchain_,
                device_
            );
            mirinae::rp::envmap::create_rp(rp_, desclayout_, device_);
            mirinae::rp::shadow::create_rp(
                rp_, fbuf_images_.depth().format(), desclayout_, device_
            );
            mirinae::rp::compo::create_rp(
                rp_,
                fbuf_images_.width(),
                fbuf_images_.height(),
                fbuf_images_,
                desclayout_,
                swapchain_,
                device_
            );
            rp_.init_render_passes(
                fbuf_images_.width(),
                fbuf_images_.height(),
                fbuf_images_,
                desclayout_,
                swapchain_,
                device_
            );

            rp_states_shadow_.pool().recreate_fbufs(
                rp_.get("shadowmap"), device_
            );

            rp_states_envmap_.init(rp_, tex_man_, desclayout_, device_);
            rp_states_compo_.init(
                desclayout_,
                fbuf_images_,
                rp_states_shadow_.pool().get_img_view_at(0),
                rp_states_shadow_.pool().get_img_view_at(1),
                rp_states_envmap_.diffuse_view(0),
                rp_states_envmap_.specular_view(0),
                rp_states_envmap_.brdf_lut_view(),
                device_
            );
            rp_states_compo_sky_.init(
                rp_states_envmap_.sky_tex_view(),
                rp_,
                desclayout_,
                fbuf_images_,
                swapchain_,
                device_
            );
            rp_states_transp_.init(
                desclayout_,
                rp_states_shadow_.pool().get_img_view_at(0),
                rp_states_shadow_.pool().get_img_view_at(1),
                rp_states_envmap_.diffuse_view(0),
                rp_states_envmap_.specular_view(0),
                rp_states_envmap_.brdf_lut_view(),
                device_
            );
            rp_states_debug_mesh_.init(device_);
            rp_states_fillscreen_.init(desclayout_, fbuf_images_, device_);
        }

        void destroy_swapchain_and_relatives() {
            device_.wait_idle();

            rp_states_shadow_.pool().destroy_fbufs(device_);

            rp_states_fillscreen_.destroy(device_);
            rp_states_debug_mesh_.destroy(device_);
            rp_states_transp_.destroy(device_);
            rp_states_compo_sky_.destroy(device_);
            rp_states_compo_.destroy(device_);
            rp_states_envmap_.destroy(device_);

            rp_.destroy();
            swapchain_.destroy(device_.logi_device());
        }

        std::optional<mirinae::ShainImageIndex> try_acquire_image() {
            framesync_.get_cur_in_flight_fence().wait(device_.logi_device());

            if (fbuf_resized_) {
                if (::is_fbuf_too_small(fbuf_width_, fbuf_height_)) {
                    fbuf_resized_ = true;
                } else {
                    fbuf_resized_ = false;
                    this->destroy_swapchain_and_relatives();
                    this->create_swapchain_and_relatives(
                        fbuf_width_, fbuf_height_
                    );
                    overlay_man_.on_fbuf_resize(fbuf_width_, fbuf_height_);
                }
                return std::nullopt;
            }

            const auto image_index_opt = swapchain_.acquire_next_image(
                framesync_.get_cur_img_ava_semaph().get(), device_.logi_device()
            );
            if (!image_index_opt) {
                if (::is_fbuf_too_small(fbuf_width_, fbuf_height_)) {
                    fbuf_resized_ = true;
                } else {
                    fbuf_resized_ = false;
                    this->destroy_swapchain_and_relatives();
                    this->create_swapchain_and_relatives(
                        fbuf_width_, fbuf_height_
                    );
                }
                return std::nullopt;
            }

            framesync_.get_cur_in_flight_fence().reset(device_.logi_device());
            return image_index_opt.value();
        }

        void update_unloaded_models() {
            namespace cpnt = mirinae::cpnt;
            using SrcSkinn = cpnt::SkinnedModelActor;
            using mirinae::RenderActorSkinned;

            auto& scene = cosmos_->scene();
            auto& reg = cosmos_->reg();

            for (auto eid : scene.entt_without_model_) {
                if (const auto src = reg.try_get<cpnt::StaticModelActor>(eid)) {
                    auto model = model_man_.request_static(
                        src->model_path_, desclayout_, tex_man_
                    );
                    if (!model) {
                        spdlog::warn(
                            "Failed to load model: {}",
                            src->model_path_.u8string()
                        );
                        continue;
                    }

                    auto& d = reg.emplace<cpnt::StaticActorVk>(eid);
                    d.model_ = model;
                    d.actor_ = std::make_shared<mirinae::RenderActor>(device_);
                    d.actor_->init(mirinae::MAX_FRAMES_IN_FLIGHT, desclayout_);
                } else if (const auto src = reg.try_get<SrcSkinn>(eid)) {
                    auto model = model_man_.request_skinned(
                        src->model_path_, desclayout_, tex_man_
                    );
                    if (!model) {
                        spdlog::warn(
                            "Failed to load model: {}",
                            src->model_path_.u8string()
                        );
                        continue;
                    }

                    auto& d = reg.emplace<cpnt::SkinnedActorVk>(eid);
                    d.model_ = model;
                    d.actor_ = std::make_shared<RenderActorSkinned>(device_);
                    d.actor_->init(mirinae::MAX_FRAMES_IN_FLIGHT, desclayout_);
                    src->anim_state_.set_skel_anim(d.model_->skel_anim_);
                }
            }

            scene.entt_without_model_.clear();
        }

        void update_ubufs(
            const glm::dmat4& proj_mat, const glm::dmat4& view_mat
        ) {
            namespace cpnt = mirinae::cpnt;
            const auto t = cosmos_->ftime().tp_;

            auto& scene = cosmos_->scene();
            auto& reg = cosmos_->reg();

            // Update ubuf: U_GbufActor
            reg.view<cpnt::Transform, cpnt::StaticActorVk>().each(
                [&](auto enttid, auto& transform, auto& ren_pair) {
                    const auto model_mat = transform.make_model_mat();

                    mirinae::U_GbufActor ubuf_data;
                    ubuf_data.model = model_mat;
                    ubuf_data.view_model = view_mat * model_mat;
                    ubuf_data.pvm = proj_mat * view_mat * model_mat;

                    ren_pair.actor_->udpate_ubuf(
                        framesync_.get_frame_index().get(),
                        ubuf_data,
                        device_.mem_alloc()
                    );
                }
            );

            // Update ubuf: U_GbufActorSkinned
            reg.view<
                   cpnt::Transform,
                   cpnt::SkinnedActorVk,
                   cpnt::SkinnedModelActor>()
                .each([&](auto enttid,
                          auto& transform,
                          auto& ren_pair,
                          auto& mactor) {
                    const auto model_m = transform.make_model_mat();
                    mactor.anim_state_.update_tick(cosmos_->ftime());

                    mirinae::U_GbufActorSkinned ubuf_data;
                    mactor.anim_state_.sample_anim(
                        ubuf_data.joint_transforms_,
                        mirinae::MAX_JOINTS,
                        cosmos_->ftime()
                    );
                    ubuf_data.view_model = view_mat * model_m;
                    ubuf_data.pvm = proj_mat * view_mat * model_m;

                    ren_pair.actor_->udpate_ubuf(
                        framesync_.get_frame_index().get(),
                        ubuf_data,
                        device_.mem_alloc()
                    );
                });

            // Update ubuf: U_CompoMain
            {
                mirinae::U_CompoMain ubuf_data;
                ubuf_data.set_proj(proj_mat);
                ubuf_data.set_view(view_mat);

                for (auto e : cosmos_->reg().view<cpnt::DLight>()) {
                    const auto& light = cosmos_->reg().get<cpnt::DLight>(e);
                    const auto& cascade = rp_states_shadow_.cascade();
                    const auto& cascades = cascade.cascades_;

                    for (size_t i = 0; i < cascades.size(); ++i)
                        ubuf_data.set_dlight_mat(i, cascades.at(i).light_mat_);
                    ubuf_data.set_dlight_dir(light.calc_to_light_dir(view_mat));
                    ubuf_data.set_dlight_color(light.color_);
                    ubuf_data.set_dlight_cascade_depths(cascade.far_depths_);
                    break;
                }

                for (auto e : cosmos_->reg().view<cpnt::SLight>()) {
                    const auto& l = cosmos_->reg().get<cpnt::SLight>(e);
                    ubuf_data.set_slight_mat(l.make_light_mat());
                    ubuf_data.set_slight_pos(l.calc_view_space_pos(view_mat));
                    ubuf_data.set_slight_dir(l.calc_to_light_dir(view_mat));
                    ubuf_data.set_slight_color(l.color_);
                    ubuf_data.set_slight_inner_angle(l.inner_angle_);
                    ubuf_data.set_slight_outer_angle(l.outer_angle_);
                    ubuf_data.set_slight_max_dist(l.max_distance_);
                    break;
                }

                rp_states_compo_.ubufs_.at(framesync_.get_frame_index().get())
                    .set_data(
                        &ubuf_data, sizeof(ubuf_data), device_.mem_alloc()
                    );

                rp_states_transp_.ubufs_.at(framesync_.get_frame_index().get())
                    .set_data(
                        &ubuf_data, sizeof(ubuf_data), device_.mem_alloc()
                    );
            }
        }

        // This must be the first member variable
        mirinae::VulkanDevice device_;
        std::shared_ptr<mirinae::ScriptEngine> script_;
        std::shared_ptr<mirinae::CosmosSimulator> cosmos_;

        mirinae::TextureManager tex_man_;
        mirinae::ModelManager model_man_;
        mirinae::DesclayoutManager desclayout_;
        mirinae::FbufImageBundle fbuf_images_;
        mirinae::OverlayManager overlay_man_;
        mirinae::RenderPassPackage rp_;
        ::RpStatesShadow rp_states_shadow_;
        ::RpStatesEnvmap rp_states_envmap_;
        ::RpStatesGbuf rp_states_gbuf_;
        ::RpStatesCompo rp_states_compo_;
        ::RpStatesCompoSky rp_states_compo_sky_;
        ::RpStatesTransp rp_states_transp_;
        ::RpStatesDebugMesh rp_states_debug_mesh_;
        ::RpStatesFillscreen rp_states_fillscreen_;
        mirinae::Swapchain swapchain_;
        ::FrameSync framesync_;
        mirinae::CommandPool cmd_pool_;
        std::vector<VkCommandBuffer> cmd_buf_;
        mirinae::InputProcesserMgr input_mgrs_;
        dal::TimerThatCaps fps_timer_;
        std::shared_ptr<mirinae::ITextData> dev_console_output_;

        uint32_t fbuf_width_ = 0;
        uint32_t fbuf_height_ = 0;
        bool fbuf_resized_ = false;
        bool flashlight_on_ = false;
        bool quit_ = false;
    };

}  // namespace


namespace mirinae {

    std::unique_ptr<IRenderer> create_vk_renderer(
        EngineCreateInfo&& cinfo,
        std::shared_ptr<ScriptEngine>& script,
        std::shared_ptr<CosmosSimulator>& cosmos
    ) {
        const auto w = cinfo.init_width_;
        const auto h = cinfo.init_height_;
        return std::make_unique<::RendererVulkan>(
            std::move(cinfo), script, cosmos, w, h
        );
    }

}  // namespace mirinae
