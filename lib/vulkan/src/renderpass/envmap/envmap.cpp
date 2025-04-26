#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/envmap/envmap.hpp"

#include <set>

#include <entt/entity/registry.hpp>

#include "mirinae/cpnt/envmap.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/draw_set.hpp"
#include "mirinae/renderpass/builder.hpp"
#include "mirinae/renderpass/envmap/cubemap.hpp"
#include "mirinae/renderpass/envmap/rp.hpp"


namespace {

    constexpr double ENVMAP_UPDATE_INVERVAL = 10;

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


    glm::dmat4 make_proj(double znear, double zfar) {
        constexpr static auto ANGLE_90 = mirinae::Angle::from_deg(90.0);
        return glm::perspectiveRH_ZO(ANGLE_90.rad(), 1.0, zfar, znear);
    }

}  // namespace


namespace { namespace task {

    class DrawBase : public mirinae::DependingTask {

    public:
        void init(
            const entt::registry& reg,
            const mirinae::IRenPass& rp,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            reg_ = &reg;
            rp_ = &rp;
            cmd_pool_ = &cmd_pool;
            device_ = &device;
        }

        void prepare(
            const mirinae::RpCtxt& ctxt,
            const mirinae::EnvmapBundle::Item& env_item
        ) {
            ctxt_ = &ctxt;
            env_item_ = &env_item;
        }

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            cmdbuf_ = cmd_pool_->get(ctxt_->f_index_, tid, *device_);
            if (cmdbuf_ == VK_NULL_HANDLE)
                return;

            draw_set_.clear();
            draw_set_.fetch(*reg_);

            mirinae::begin_cmdbuf(cmdbuf_);
            this->record(cmdbuf_, *env_item_, *reg_, draw_set_, *rp_, *ctxt_);
            mirinae::end_cmdbuf(cmdbuf_);
        }

        static void record(
            const VkCommandBuffer cmdbuf,
            const mirinae::EnvmapBundle::Item& env_item,
            const entt::registry& reg,
            const mirinae::DrawSetStatic& draw_set,
            const mirinae::IRenPass& rp,
            const mirinae::RpCtxt& ctxt
        ) {
            namespace cpnt = mirinae::cpnt;

            mirinae::RenderPassBeginInfo rp_info{};
            rp_info.rp(rp.render_pass())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values());

            const auto proj_mat = ::make_proj(0.1, 1000);

            mirinae::DescSetBindInfo descset_info{ rp.pipe_layout() };

            auto& cube_map = env_item.cube_map_;
            auto& base_cube = cube_map.base();

            mirinae::Viewport{}
                .set_wh(base_cube.extent2d())
                .record_single(cmdbuf);
            mirinae::Rect2D{}
                .set_wh(base_cube.extent2d())
                .record_scissor(cmdbuf);
            rp_info.wh(base_cube.width(), base_cube.height());

            for (int i = 0; i < 6; ++i) {
                rp_info.fbuf(cube_map.base().face_fbuf(i)).record_begin(cmdbuf);

                vkCmdBindPipeline(
                    cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                mirinae::U_EnvmapPushConst push_const;
                for (auto e : reg.view<cpnt::DLight, cpnt::Transform>()) {
                    const auto& light = reg.get<cpnt::DLight>(e);
                    const auto& tform = reg.get<cpnt::Transform>(e);
                    push_const.dlight_dir_ = glm::vec4{
                        light.calc_to_light_dir(glm::dmat4(1), tform), 0
                    };
                    push_const.dlight_color_ = glm::vec4{
                        light.color_.scaled_color(), 0
                    };
                    break;
                }

                for (auto& pair : draw_set.opa()) {
                    auto& unit = *pair.unit_;
                    auto& actor = *pair.actor_;

                    descset_info.first_set(0)
                        .set(unit.get_desc_set(ctxt.f_index_.get()))
                        .record(cmdbuf);

                    unit.record_bind_vert_buf(cmdbuf);

                    descset_info.first_set(1)
                        .set(actor.get_desc_set(ctxt.f_index_.get()))
                        .record(cmdbuf);

                    push_const.proj_view_ = proj_mat * CUBE_VIEW_MATS[i] *
                                            env_item.world_mat_;

                    mirinae::PushConstInfo{}
                        .layout(rp.pipe_layout())
                        .add_stage_vert()
                        .record(cmdbuf, push_const);

                    vkCmdDrawIndexed(cmdbuf, unit.vertex_count(), 1, 0, 0, 0);
                }

                vkCmdEndRenderPass(cmdbuf);
            }

            auto& img = cube_map.base().img_;
            mirinae::ImageMemoryBarrier{}
                .image(img.image())
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .old_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                .set_src_access(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                .set_dst_access(VK_ACCESS_TRANSFER_READ_BIT)
                .mip_base(0)
                .mip_count(1)
                .layer_base(0)
                .layer_count(6)
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT
                );

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
                    cmdbuf,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT
                );

                mirinae::ImageBlit blit;
                blit.set_src_offsets_full(
                    img.width() >> (i - 1), img.height() >> (i - 1)
                );
                blit.set_dst_offsets_full(img.width() >> i, img.height() >> i);
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
                    cmdbuf,
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
                    cmdbuf,
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
                cmdbuf,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
            );
        }

        mirinae::DrawSetStatic draw_set_;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;

        const mirinae::EnvmapBundle::Item* env_item_ = nullptr;
        const entt::registry* reg_ = nullptr;
        const mirinae::IRenPass* rp_ = nullptr;
        const mirinae::RpCtxt* ctxt_ = nullptr;
        mirinae::RpCommandPool* cmd_pool_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
    };


    class DrawTasks : public enki::ITaskSet {

    public:
        void prepare(const mirinae::RpCtxt& ctxt) {}

        enki::ITaskSet& fence() { return fence_; }

        void collect_cmdbuf(std::vector<VkCommandBuffer>& out) {}

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {

        }

        mirinae::FenceTask fence_;
    };


    class RpTask : public mirinae::IRpTask {

    public:
        RpTask() {}

        void init(
            const entt::registry& reg,
            const mirinae::IRenPass& rp,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {}

        std::string_view name() const override { return "envmap brdf"; }

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


namespace {

    class EnvmapSelector {

    public:
        void notify(entt::entity e, double distance) {
            entities_.insert({ distance, e });
        }

        bool empty() const { return entities_.empty(); }

        bool has_entt(entt::entity e) const {
            for (const auto& x : entities_) {
                if (x.second == e)
                    return true;
            }

            return false;
        }

        auto begin() const { return entities_.begin(); }
        auto end() const { return entities_.end(); }

    private:
        std::set<std::pair<double, entt::entity>> entities_;
    };


    struct LocalRpReg : public mirinae::IEnvmapRpBundle {

    public:
        void init(
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        ) {
            base_ = mirinae::create_rp_base(desclayouts, device);
            diffuse_ = mirinae::create_rp_diffuse(desclayouts, device);
            specular_ = mirinae::create_rp_specular(desclayouts, device);
            sky_ = mirinae::create_rp_sky(desclayouts, device);
            brdf_lut_ = mirinae::create_rp_brdf_lut(desclayouts, device);
        }

        const mirinae::IRenPass& rp_base() const override { return *base_; }

        const mirinae::IRenPass& rp_diffuse() const override {
            return *diffuse_;
        }

        const mirinae::IRenPass& rp_specular() const { return *specular_; }

        const mirinae::IRenPass& rp_sky() const { return *sky_; }

        const mirinae::IRenPass& rp_brdf_lut() const override {
            return *brdf_lut_;
        }

    private:
        std::unique_ptr<mirinae::IRenPass> base_;
        std::unique_ptr<mirinae::IRenPass> diffuse_;
        std::unique_ptr<mirinae::IRenPass> specular_;
        std::unique_ptr<mirinae::IRenPass> sky_;
        std::unique_ptr<mirinae::IRenPass> brdf_lut_;
    };


    class RpMaster : public mirinae::IRpStates {

    public:
        RpMaster(mirinae::VulkanDevice& device) : device_(device) {}

        ~RpMaster() override { this->destroy(); }

        const std::string& name() const override {
            static const std::string out = "envmap";
            return out;
        }

        void init(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::DesclayoutManager& desclayouts
        ) {
            auto& tex_man = *rp_res.tex_man_;
            rp_pkg_.init(desclayouts, device_);
            desc_pool_.init(
                5,
                desclayouts.get("envdiffuse:main").size_info() +
                    desclayouts.get("env_sky:main").size_info(),
                device_.logi_device()
            );

            // Sky texture
            {
                auto e = this->select_atmos_simple(cosmos.reg());
                auto& atmos = cosmos.reg().get<mirinae::cpnt::AtmosphereSimple>(
                    e
                );
                if (tex_man.request_blck(atmos.sky_tex_path_, false)) {
                    sky_tex_ = tex_man.get(atmos.sky_tex_path_);
                } else {
                    sky_tex_ = tex_man.missing_tex();
                }
            }

            desc_set_ = desc_pool_.alloc(
                desclayouts.get("env_sky:main").layout(), device_.logi_device()
            );

            mirinae::DescWriteInfoBuilder desc_info;
            desc_info.set_descset(desc_set_)
                .add_img_sampler(
                    sky_tex_->image_view(), device_.samplers().get_linear()
                )
                .apply_all(device_.logi_device());

            envmaps_ = std::make_unique<mirinae::EnvmapBundle>(
                rp_pkg_, device_
            );
            envmaps_->add(rp_pkg_, desc_pool_, desclayouts);
            rp_res.envmaps_ = envmaps_;

            timer_.set_min();
        }

        void destroy() {
            envmaps_->destroy();
            desc_pool_.destroy(device_.logi_device());
        }

        void record(const mirinae::RpContext& ctxt) override {
            if (!timer_.check_if_elapsed(ENVMAP_UPDATE_INVERVAL))
                return;

            auto& reg = ctxt.cosmos_->reg();
            ::EnvmapSelector entities;
            for (auto e : reg.view<mirinae::cpnt::Envmap>()) {
                if (auto tform = reg.try_get<mirinae::cpnt::Transform>(e)) {
                    const auto d = glm::length(tform->pos_ - ctxt.view_pos_);
                    entities.notify(e, d);
                } else {
                    entities.notify(e, glm::length(ctxt.view_pos_));
                }
            }
            if (entities.empty())
                return;

            for (auto& x : *envmaps_) {
                if (x.entity_ != entt::null && entities.has_entt(x.entity_))
                    continue;

                x.entity_ = entt::null;
                for (auto& [_, candidate] : entities) {
                    if (!envmaps_->has_entt(candidate)) {
                        x.entity_ = candidate;
                        x.timer_.set_min();
                        break;
                    }
                }
            }

            auto chosen = envmaps_->choose_to_update();
            if (!chosen)
                return;
            const auto entity = chosen->entity_;
            auto& env_cpnt = reg.get<mirinae::cpnt::Envmap>(entity);
            env_cpnt.last_updated_.check();
            chosen->timer_.check();

            if (auto tform = reg.try_get<mirinae::cpnt::Transform>(entity)) {
                constexpr static auto IDENTITY = glm::dmat4(1);
                chosen->world_mat_ = glm::translate(IDENTITY, -tform->pos_);
            } else {
                chosen->world_mat_ = glm::dmat4(1);
            }

            SPDLOG_DEBUG("Updating envmap: entt={}", (int)chosen->entity_);
            this->record_sky(entity, *chosen, ctxt, desc_set_);
            this->record_base(entity, *chosen, ctxt);
            this->record_diffuse(entity, *chosen, ctxt);
            this->record_specular(entity, *chosen, ctxt);
        }

    private:
        static entt::entity select_atmos_simple(entt::registry& reg) {
            for (auto entity : reg.view<mirinae::cpnt::AtmosphereSimple>())
                return entity;

            return entt::null;
        }

        static glm::dmat4 make_proj(double znear, double zfar) {
            constexpr static auto ANGLE_90 = mirinae::Angle::from_deg(90.0);
            return glm::perspectiveRH_ZO(ANGLE_90.rad(), 1.0, zfar, znear);
        }

        void record_base(
            const entt::entity e_env,
            const mirinae::EnvmapBundle::Item& env_item,
            const mirinae::RpContext& ctxt
        ) {
            namespace cpnt = mirinae::cpnt;
            auto& rp = rp_pkg_.rp_base();
            auto& reg = ctxt.cosmos_->reg();
            const auto cmdbuf = ctxt.cmdbuf_;

            mirinae::RenderPassBeginInfo rp_info{};
            rp_info.rp(rp.render_pass())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values());

            const auto proj_mat = this->make_proj(0.1, 1000);

            mirinae::DescSetBindInfo descset_info{ rp.pipe_layout() };

            auto& cube_map = env_item.cube_map_;
            auto& base_cube = cube_map.base();

            mirinae::Viewport{}
                .set_wh(base_cube.extent2d())
                .record_single(cmdbuf);
            mirinae::Rect2D{}
                .set_wh(base_cube.extent2d())
                .record_scissor(cmdbuf);
            rp_info.wh(base_cube.width(), base_cube.height());

            for (int i = 0; i < 6; ++i) {
                rp_info.fbuf(cube_map.base().face_fbuf(i)).record_begin(cmdbuf);

                vkCmdBindPipeline(
                    cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                mirinae::U_EnvmapPushConst push_const;
                for (auto e : reg.view<cpnt::DLight, cpnt::Transform>()) {
                    const auto& light = reg.get<cpnt::DLight>(e);
                    const auto& tform = reg.get<cpnt::Transform>(e);
                    push_const.dlight_dir_ = glm::vec4{
                        light.calc_to_light_dir(glm::dmat4(1), tform), 0
                    };
                    push_const.dlight_color_ = glm::vec4{
                        light.color_.scaled_color(), 0
                    };
                    break;
                }

                for (auto& pair : ctxt.draw_sheet_->static_) {
                    auto& unit = *pair.unit_;
                    descset_info.first_set(0)
                        .set(unit.get_desc_set(ctxt.f_index_.get()))
                        .record(cmdbuf);

                    unit.record_bind_vert_buf(cmdbuf);

                    for (auto& actor : pair.actors_) {
                        descset_info.first_set(1)
                            .set(actor.actor_->get_desc_set(ctxt.f_index_.get())
                            )
                            .record(cmdbuf);

                        push_const.proj_view_ = proj_mat * CUBE_VIEW_MATS[i] *
                                                env_item.world_mat_;

                        mirinae::PushConstInfo{}
                            .layout(rp.pipe_layout())
                            .add_stage_vert()
                            .record(cmdbuf, push_const);

                        vkCmdDrawIndexed(
                            cmdbuf, unit.vertex_count(), 1, 0, 0, 0
                        );
                    }
                }

                vkCmdEndRenderPass(cmdbuf);
            }

            auto& img = cube_map.base().img_;
            mirinae::ImageMemoryBarrier{}
                .image(img.image())
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .old_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                .set_src_access(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                .set_dst_access(VK_ACCESS_TRANSFER_READ_BIT)
                .mip_base(0)
                .mip_count(1)
                .layer_base(0)
                .layer_count(6)
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT
                );

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
                    cmdbuf,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT
                );

                mirinae::ImageBlit blit;
                blit.set_src_offsets_full(
                    img.width() >> (i - 1), img.height() >> (i - 1)
                );
                blit.set_dst_offsets_full(img.width() >> i, img.height() >> i);
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
                    cmdbuf,
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
                    cmdbuf,
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
                cmdbuf,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
            );
        }

        void record_sky(
            const entt::entity e_env,
            const mirinae::EnvmapBundle::Item& env_item,
            const mirinae::RpContext& ctxt,
            const VkDescriptorSet desc_set
        ) {
            namespace cpnt = mirinae::cpnt;
            auto& rp = rp_pkg_.rp_sky();
            auto& reg = ctxt.cosmos_->reg();
            const auto cmdbuf = ctxt.cmdbuf_;

            mirinae::RenderPassBeginInfo rp_info{};
            rp_info.rp(rp.render_pass())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values());

            const auto proj_mat = this->make_proj(0.1, 1000.0);

            auto& cube_map = env_item.cube_map_;
            auto& base_cube = cube_map.base();

            glm::dmat4 world_mat(1);
            if (auto tform = reg.try_get<cpnt::Transform>(e_env)) {
                world_mat = glm::translate<double>(world_mat, -tform->pos_);
            }

            mirinae::Viewport{}
                .set_wh(base_cube.width(), base_cube.height())
                .record_single(cmdbuf);
            mirinae::Rect2D{}
                .set_wh(base_cube.width(), base_cube.height())
                .record_scissor(cmdbuf);
            rp_info.wh(base_cube.width(), base_cube.height());

            for (int i = 0; i < 6; ++i) {
                rp_info.fbuf(base_cube.face_fbuf(i)).record_begin(cmdbuf);

                vkCmdBindPipeline(
                    cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                mirinae::DescSetBindInfo{}
                    .layout(rp.pipe_layout())
                    .set(desc_set)
                    .record(cmdbuf);

                mirinae::U_EnvSkyPushConst pc;
                pc.proj_view_ = proj_mat * CUBE_VIEW_MATS[i];

                mirinae::PushConstInfo{}
                    .layout(rp.pipe_layout())
                    .add_stage_vert()
                    .record(cmdbuf, pc);

                vkCmdDraw(cmdbuf, 36, 1, 0, 0);

                vkCmdEndRenderPass(cmdbuf);
            }
        }

        void record_diffuse(
            const entt::entity e_env,
            const mirinae::EnvmapBundle::Item& env_item,
            const mirinae::RpContext& ctxt
        ) {
            namespace cpnt = mirinae::cpnt;
            auto& rp = rp_pkg_.rp_diffuse();
            auto& reg = ctxt.cosmos_->reg();
            const auto cmdbuf = ctxt.cmdbuf_;

            mirinae::RenderPassBeginInfo rp_info{};
            rp_info.rp(rp.render_pass())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values());

            const auto proj_mat = this->make_proj(0.01, 10.0);

            mirinae::DescSetBindInfo descset_info{ rp.pipe_layout() };

            auto& cube_map = env_item.cube_map_;
            auto& diffuse = cube_map.diffuse();

            const mirinae::Viewport viewport{ diffuse.extent2d() };
            const mirinae::Rect2D scissor{ diffuse.extent2d() };
            rp_info.wh(diffuse.extent2d());

            for (int i = 0; i < 6; ++i) {
                rp_info.fbuf(diffuse.face_fbuf(i)).record_begin(cmdbuf);

                vkCmdBindPipeline(
                    cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                viewport.record_single(cmdbuf);
                scissor.record_scissor(cmdbuf);

                descset_info.set(cube_map.desc_set()).record(cmdbuf);

                mirinae::U_EnvdiffusePushConst push_const;
                push_const.proj_view_ = proj_mat * CUBE_VIEW_MATS[i];

                mirinae::PushConstInfo{}
                    .layout(rp.pipe_layout())
                    .add_stage_vert()
                    .record(cmdbuf, push_const);

                vkCmdDraw(cmdbuf, 36, 1, 0, 0);
                vkCmdEndRenderPass(cmdbuf);
            }

            mirinae::ImageMemoryBarrier barrier;
            barrier.image(diffuse.cube_img())
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .old_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                .set_src_access(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                .set_dst_access(VK_ACCESS_SHADER_READ_BIT)
                .mip_base(0)
                .mip_count(1)
                .layer_base(0)
                .layer_count(6);
            barrier.record_single(
                cmdbuf,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
            );
        }

        void record_specular(
            const entt::entity e_env,
            const mirinae::EnvmapBundle::Item& env_item,
            const mirinae::RpContext& ctxt
        ) {
            namespace cpnt = mirinae::cpnt;
            auto& rp = rp_pkg_.rp_specular();
            auto& reg = ctxt.cosmos_->reg();
            const auto cmdbuf = ctxt.cmdbuf_;

            mirinae::RenderPassBeginInfo rp_info{};
            rp_info.rp(rp.render_pass())
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values());

            const auto proj_mat = this->make_proj(0.01, 10.0);

            mirinae::DescSetBindInfo descset_info{ rp.pipe_layout() };

            auto& cube_map = env_item.cube_map_;
            auto& specular = cube_map.specular();

            for (auto& mip : specular.mips()) {
                const mirinae::Rect2D scissor{ mip.extent2d() };
                const mirinae::Viewport viewport{ scissor.extent2d() };
                rp_info.wh(scissor.extent2d());

                for (int i = 0; i < 6; ++i) {
                    auto& face = mip.faces_[i];

                    rp_info.fbuf(face.fbuf_.get()).record_begin(cmdbuf);

                    vkCmdBindPipeline(
                        cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                    );

                    viewport.record_single(cmdbuf);
                    scissor.record_scissor(cmdbuf);

                    descset_info.set(cube_map.desc_set()).record(cmdbuf);

                    mirinae::U_EnvSpecularPushConst push_const;
                    push_const.proj_view_ = proj_mat * CUBE_VIEW_MATS[i];
                    push_const.roughness_ = mip.roughness_;

                    mirinae::PushConstInfo{}
                        .layout(rp.pipe_layout())
                        .add_stage_vert()
                        .add_stage_frag()
                        .record(cmdbuf, push_const);

                    vkCmdDraw(cmdbuf, 36, 1, 0, 0);
                    vkCmdEndRenderPass(cmdbuf);
                }
            }

            mirinae::ImageMemoryBarrier barrier;
            barrier.image(specular.cube_img())
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .old_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                .set_src_access(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                .set_dst_access(VK_ACCESS_SHADER_READ_BIT)
                .mip_base(0)
                .mip_count(specular.mip_levels())
                .layer_base(0)
                .layer_count(6);
            barrier.record_single(
                cmdbuf,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
            );
        }

        mirinae::VulkanDevice& device_;
        ::LocalRpReg rp_pkg_;
        mirinae::DescPool desc_pool_;
        sung::MonotonicRealtimeTimer timer_;
        std::shared_ptr<mirinae::EnvmapBundle> envmaps_;
        std::shared_ptr<mirinae::ITexture> sky_tex_;
        VkDescriptorSet desc_set_ = VK_NULL_HANDLE;  // For env sky
    };

}  // namespace


namespace mirinae::rp::envmap {

    URpStates create_rp_states_envmap(
        CosmosSimulator& cosmos,
        RpResources& rp_res,
        DesclayoutManager& desclayouts,
        VulkanDevice& device
    ) {
        auto out = std::make_unique<RpMaster>(device);
        out->init(cosmos, rp_res, desclayouts);
        return out;
    }

}  // namespace mirinae::rp::envmap
