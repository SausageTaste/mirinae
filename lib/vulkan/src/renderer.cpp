#include "mirinae/renderer.hpp"

#include <spdlog/spdlog.h>

#include <daltools/common/util.h>
#include <sung/general/time.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/lightweight/script.hpp"
#include "mirinae/math/glm_fmt.hpp"
#include "mirinae/math/mamath.hpp"
#include "mirinae/overlay/overlay.hpp"
#include "mirinae/render/renderpass.hpp"


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


    class ShadowMapPool {

    public:
        struct Item {
            float width() const { return tex_->width(); }
            float height() const { return tex_->height(); }
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
            uint32_t width,
            uint32_t height,
            mirinae::IRenderPassBundle& rp,
            mirinae::TextureManager& tex_man,
            mirinae::VulkanDevice& device
        ) {
            auto& added = shadow_maps_.emplace_back();
            added.tex_ = tex_man.create_depth(width, height);
        }

        void recreate_fbufs(
            mirinae::IRenderPassBundle& rp, mirinae::VulkanDevice& device
        ) {
            for (auto& x : shadow_maps_) {
                const auto img_view = x.tex_->image_view();

                VkFramebufferCreateInfo framebufferInfo{};
                framebufferInfo.sType =
                    VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                framebufferInfo.renderPass = rp.renderpass();
                framebufferInfo.attachmentCount = 1;
                framebufferInfo.pAttachments = &img_view;
                framebufferInfo.width = x.tex_->width();
                framebufferInfo.height = x.tex_->height();
                framebufferInfo.layers = 1;

                x.fbuf_.init(framebufferInfo, device.logi_device());
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
            mirinae::VulkanDevice& device
        ) {
            auto& added = cube_map_.emplace_back();
            added.init(rp_pkg, tex_man, device);
            added.world_pos_ = { 0.14983922321477,
                                 0.66663010560478,
                                 -1.1615585516897 };
        }

        void destroy(mirinae::VulkanDevice& device) {
            for (auto& x : cube_map_) x.destroy(device);
            cube_map_.clear();
        }

        void record(
            const VkCommandBuffer cur_cmd_buf,
            const ::DrawSheet& draw_sheet,
            const ::FrameIndex frame_index,
            const mirinae::ShainImageIndex image_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = *rp_pkg.cubemap_;

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = rp.renderpass();
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = { 512, 512 };
            renderPassInfo.clearValueCount = rp.clear_value_count();
            renderPassInfo.pClearValues = rp.clear_values();

            const auto proj_mat = glm::perspective<double>(
                glm::radians(90.0), 1.0, 0.1, 1000.0
            );

            for (auto& cube_map : cube_map_) {
                const auto world_mat = glm::translate<double>(
                    glm::dmat4(1), -cube_map.world_pos_
                );

                for (int i = 0; i < 6; ++i) {
                    renderPassInfo.framebuffer = cube_map.fbufs_[i].get();

                    vkCmdBeginRenderPass(
                        cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE
                    );
                    vkCmdBindPipeline(
                        cur_cmd_buf,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        rp.pipeline()
                    );

                    VkViewport viewport{};
                    viewport.x = 0.0f;
                    viewport.y = 0.0f;
                    viewport.width = 512;
                    viewport.height = 512;
                    viewport.minDepth = 0.0f;
                    viewport.maxDepth = 1.0f;
                    vkCmdSetViewport(cur_cmd_buf, 0, 1, &viewport);

                    VkRect2D scissor{};
                    scissor.offset = { 0, 0 };
                    scissor.extent = { 512, 512 };
                    vkCmdSetScissor(cur_cmd_buf, 0, 1, &scissor);

                    for (auto& pair : draw_sheet.static_pairs_) {
                        for (auto& unit : pair.model_->render_units_) {
                            auto unit_desc = unit.get_desc_set(frame_index.get()
                            );
                            vkCmdBindDescriptorSets(
                                cur_cmd_buf,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                rp.pipeline_layout(),
                                0,
                                1,
                                &unit_desc,
                                0,
                                nullptr
                            );
                            unit.record_bind_vert_buf(cur_cmd_buf);

                            for (auto& actor : pair.actors_) {
                                auto actor_desc = actor.actor_->get_desc_set(
                                    frame_index.get()
                                );
                                vkCmdBindDescriptorSets(
                                    cur_cmd_buf,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    rp.pipeline_layout(),
                                    1,
                                    1,
                                    &actor_desc,
                                    0,
                                    nullptr
                                );

                                mirinae::U_EnvmapPushConst push_const;
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
            }
        }

        VkImageView get_view(size_t index) const {
            return cube_map_.at(index).cubemap_view_;
        }

    private:
        class CubeMap {

        public:
            bool init(
                mirinae::RenderPassPackage& rp_pkg,
                mirinae::TextureManager& tex_man,
                mirinae::VulkanDevice& device
            ) {
                mirinae::ImageCreateInfo cinfo;
                cinfo.set_format(VK_FORMAT_B10G11R11_UFLOAT_PACK32)
                    .set_dimensions(512, 512)
                    .set_mip_levels(1)
                    .set_arr_layers(6)
                    .add_usage_sampled()
                    .add_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                    .add_flag(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT);
                img_.init(cinfo.get(), device.mem_alloc());

                VkImageViewCreateInfo v_cinfo{};
                v_cinfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                v_cinfo.image = VK_NULL_HANDLE;
                v_cinfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
                v_cinfo.format = img_.format();
                v_cinfo.components = { VK_COMPONENT_SWIZZLE_R };
                v_cinfo.subresourceRange = {
                    VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1
                };
                v_cinfo.subresourceRange.layerCount = 6;
                v_cinfo.image = img_.image();
                VK_CHECK(vkCreateImageView(
                    device.logi_device(), &v_cinfo, nullptr, &cubemap_view_
                ));

                v_cinfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                v_cinfo.subresourceRange.layerCount = 1;
                for (uint32_t i = 0; i < 6; i++) {
                    v_cinfo.subresourceRange.baseArrayLayer = i;
                    VK_CHECK(vkCreateImageView(
                        device.logi_device(), &v_cinfo, nullptr, &face_views_[i]
                    ));
                }

                depth_map_ = tex_man.create_depth(CUBE_IMG_SIZE, CUBE_IMG_SIZE);

                for (uint32_t i = 0; i < 6; i++) {
                    const std::array<VkImageView, 2> attachments = {
                        depth_map_->image_view(), face_views_[i]
                    };

                    VkFramebufferCreateInfo fbuf_cinfo = {};
                    fbuf_cinfo.sType =
                        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                    fbuf_cinfo.renderPass = rp_pkg.cubemap_->renderpass();
                    fbuf_cinfo.attachmentCount = static_cast<uint32_t>(
                        attachments.size()
                    );
                    fbuf_cinfo.pAttachments = attachments.data();
                    fbuf_cinfo.width = CUBE_IMG_SIZE;
                    fbuf_cinfo.height = CUBE_IMG_SIZE;
                    fbuf_cinfo.layers = 1;

                    fbufs_[i].init(fbuf_cinfo, device.logi_device());
                }

                return true;
            }

            void destroy(mirinae::VulkanDevice& device) {
                for (auto& x : fbufs_) x.destroy(device.logi_device());

                vkDestroyImageView(
                    device.logi_device(), cubemap_view_, nullptr
                );

                for (auto& x : face_views_)
                    vkDestroyImageView(device.logi_device(), x, nullptr);

                depth_map_.reset();
                img_.destroy(device.mem_alloc());
            }

            mirinae::Image img_;
            std::unique_ptr<mirinae::ITexture> depth_map_;
            VkImageView cubemap_view_ = VK_NULL_HANDLE;
            std::array<VkImageView, 6> face_views_;
            std::array<mirinae::Fbuf, 6> fbufs_;
            glm::dvec3 world_pos_;
        };

        constexpr static uint32_t CUBE_IMG_SIZE = 512;
        std::vector<CubeMap> cube_map_;
    };


    class RpStatesShadow {

    public:
        void record_static(
            const VkCommandBuffer cur_cmd_buf,
            const ::DrawSheet& draw_sheet,
            const ::FrameIndex frame_index,
            const mirinae::RenderPassPackage& rp_pkg
        ) {
            auto& rp = *rp_pkg.shadowmap_;

            for (auto& shadow : shadow_maps_) {
                VkRenderPassBeginInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = rp.renderpass();
                renderPassInfo.framebuffer = shadow.fbuf();
                renderPassInfo.renderArea.offset = { 0, 0 };
                renderPassInfo.renderArea.extent = shadow.tex_->extent();
                renderPassInfo.clearValueCount = rp.clear_value_count();
                renderPassInfo.pClearValues = rp.clear_values();

                vkCmdBeginRenderPass(
                    cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE
                );
                vkCmdBindPipeline(
                    cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                VkViewport viewport{};
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = shadow.width();
                viewport.height = shadow.height();
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                vkCmdSetViewport(cur_cmd_buf, 0, 1, &viewport);

                VkRect2D scissor{};
                scissor.offset = { 0, 0 };
                scissor.extent = shadow.tex_->extent();
                vkCmdSetScissor(cur_cmd_buf, 0, 1, &scissor);

                for (auto& pair : draw_sheet.static_pairs_) {
                    for (auto& unit : pair.model_->render_units_) {
                        auto unit_desc = unit.get_desc_set(frame_index.get());
                        unit.record_bind_vert_buf(cur_cmd_buf);

                        for (auto& actor : pair.actors_) {
                            auto actor_desc = actor.actor_->get_desc_set(
                                frame_index.get()
                            );
                            vkCmdBindDescriptorSets(
                                cur_cmd_buf,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                rp.pipeline_layout(),
                                0,
                                1,
                                &actor_desc,
                                0,
                                nullptr
                            );

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
            auto& rp = *rp_pkg.shadowmap_skin_;

            for (auto& shadow : shadow_maps_) {
                VkRenderPassBeginInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = rp.renderpass();
                renderPassInfo.framebuffer = shadow.fbuf();
                renderPassInfo.renderArea.offset = { 0, 0 };
                renderPassInfo.renderArea.extent = shadow.tex_->extent();
                renderPassInfo.clearValueCount = rp.clear_value_count();
                renderPassInfo.pClearValues = rp.clear_values();

                vkCmdBeginRenderPass(
                    cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE
                );
                vkCmdBindPipeline(
                    cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                VkViewport viewport{};
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = shadow.width();
                viewport.height = shadow.height();
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                vkCmdSetViewport(cur_cmd_buf, 0, 1, &viewport);

                VkRect2D scissor{};
                scissor.offset = { 0, 0 };
                scissor.extent = shadow.tex_->extent();
                vkCmdSetScissor(cur_cmd_buf, 0, 1, &scissor);

                for (auto& pair : draw_sheet.skinned_pairs_) {
                    for (auto& unit : pair.model_->runits_) {
                        auto unit_desc = unit.get_desc_set(frame_index.get());
                        unit.record_bind_vert_buf(cur_cmd_buf);

                        for (auto& actor : pair.actors_) {
                            auto actor_desc = actor.actor_->get_desc_set(
                                frame_index.get()
                            );
                            vkCmdBindDescriptorSets(
                                cur_cmd_buf,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                rp.pipeline_layout(),
                                0,
                                1,
                                &actor_desc,
                                0,
                                nullptr
                            );

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

    private:
        ::ShadowMapPool shadow_maps_;
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
            auto& rp = *rp_pkg.gbuf_;

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = rp.renderpass();
            renderPassInfo.framebuffer = rp.fbuf_at(image_index.get());
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = fbuf_exd;
            renderPassInfo.clearValueCount = rp.clear_value_count();
            renderPassInfo.pClearValues = rp.clear_values();

            vkCmdBeginRenderPass(
                cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE
            );
            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(fbuf_exd.width);
            viewport.height = static_cast<float>(fbuf_exd.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cur_cmd_buf, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = { 0, 0 };
            scissor.extent = fbuf_exd;
            vkCmdSetScissor(cur_cmd_buf, 0, 1, &scissor);

            for (auto& pair : draw_sheet.static_pairs_) {
                for (auto& unit : pair.model_->render_units_) {
                    auto unit_desc = unit.get_desc_set(frame_index.get());
                    vkCmdBindDescriptorSets(
                        cur_cmd_buf,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        rp.pipeline_layout(),
                        0,
                        1,
                        &unit_desc,
                        0,
                        nullptr
                    );
                    unit.record_bind_vert_buf(cur_cmd_buf);

                    for (auto& actor : pair.actors_) {
                        auto actor_desc = actor.actor_->get_desc_set(
                            frame_index.get()
                        );
                        vkCmdBindDescriptorSets(
                            cur_cmd_buf,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            rp.pipeline_layout(),
                            1,
                            1,
                            &actor_desc,
                            0,
                            nullptr
                        );

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
            auto& rp = *rp_pkg.gbuf_skin_;

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = rp.renderpass();
            renderPassInfo.framebuffer = rp.fbuf_at(image_index.get());
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = fbuf_exd;
            renderPassInfo.clearValueCount = rp.clear_value_count();
            renderPassInfo.pClearValues = rp.clear_values();

            vkCmdBeginRenderPass(
                cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE
            );
            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(fbuf_exd.width);
            viewport.height = static_cast<float>(fbuf_exd.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cur_cmd_buf, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = { 0, 0 };
            scissor.extent = fbuf_exd;
            vkCmdSetScissor(cur_cmd_buf, 0, 1, &scissor);

            for (auto& pair : draw_sheet.skinned_pairs_) {
                for (auto& unit : pair.model_->runits_) {
                    auto unit_desc = unit.get_desc_set(frame_index.get());
                    vkCmdBindDescriptorSets(
                        cur_cmd_buf,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        rp.pipeline_layout(),
                        0,
                        1,
                        &unit_desc,
                        0,
                        nullptr
                    );
                    unit.record_bind_vert_buf(cur_cmd_buf);

                    for (auto& actor : pair.actors_) {
                        auto actor_desc = actor.actor_->get_desc_set(
                            frame_index.get()
                        );
                        vkCmdBindDescriptorSets(
                            cur_cmd_buf,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            rp.pipeline_layout(),
                            1,
                            1,
                            &actor_desc,
                            0,
                            nullptr
                        );

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
            VkImageView envmap,
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
                    .add_img_sampler(envmap, device.samplers().get_cubemap());
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
            auto& rp = *rp_pkg.compo_;
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = rp.renderpass();
            renderPassInfo.framebuffer = rp.fbuf_at(image_index.get());
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = fbuf_ext;
            renderPassInfo.clearValueCount = rp.clear_value_count();
            renderPassInfo.pClearValues = rp.clear_values();

            vkCmdBeginRenderPass(
                cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE
            );
            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(fbuf_ext.width);
            viewport.height = static_cast<float>(fbuf_ext.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cur_cmd_buf, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = { 0, 0 };
            scissor.extent = fbuf_ext;
            vkCmdSetScissor(cur_cmd_buf, 0, 1, &scissor);

            auto desc_main = desc_sets_.at(frame_index.get());
            vkCmdBindDescriptorSets(
                cur_cmd_buf,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                rp.pipeline_layout(),
                0,
                1,
                &desc_main,
                0,
                nullptr
            );

            vkCmdDraw(cur_cmd_buf, 3, 1, 0, 0);

            vkCmdEndRenderPass(cur_cmd_buf);
        }

        mirinae::DescPool desc_pool_;
        std::vector<VkDescriptorSet> desc_sets_;
        std::vector<mirinae::Buffer> ubufs_;
    };


    class RpStatesTransp {

    public:
        void init(
            mirinae::DesclayoutManager& desclayouts,
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

            mirinae::DescWriteInfoBuilder builder;
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& ubuf = ubufs_.emplace_back();
                ubuf.init_ubuf(
                    sizeof(mirinae::U_TranspFrame), device.mem_alloc()
                );

                builder.set_descset(desc_sets_.at(i)).add_ubuf(ubuf);
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
            auto& rp = *rp_pkg.transp_;

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = rp.renderpass();
            renderPassInfo.framebuffer = rp.fbuf_at(image_index.get());
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = fbuf_ext;
            renderPassInfo.clearValueCount = rp.clear_value_count();
            renderPassInfo.pClearValues = rp.clear_values();

            vkCmdBeginRenderPass(
                cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE
            );
            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(fbuf_ext.width);
            viewport.height = static_cast<float>(fbuf_ext.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cur_cmd_buf, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = { 0, 0 };
            scissor.extent = fbuf_ext;
            vkCmdSetScissor(cur_cmd_buf, 0, 1, &scissor);

            auto desc_frame = desc_sets_.at(frame_index.get());
            vkCmdBindDescriptorSets(
                cur_cmd_buf,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                rp.pipeline_layout(),
                0,
                1,
                &desc_frame,
                0,
                nullptr
            );

            for (auto& pair : draw_sheet.static_pairs_) {
                for (auto& unit : pair.model_->render_units_alpha_) {
                    auto unit_desc = unit.get_desc_set(frame_index.get());
                    vkCmdBindDescriptorSets(
                        cur_cmd_buf,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        rp.pipeline_layout(),
                        1,
                        1,
                        &unit_desc,
                        0,
                        nullptr
                    );
                    unit.record_bind_vert_buf(cur_cmd_buf);

                    for (auto& actor : pair.actors_) {
                        auto actor_desc = actor.actor_->get_desc_set(
                            frame_index.get()
                        );
                        vkCmdBindDescriptorSets(
                            cur_cmd_buf,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            rp.pipeline_layout(),
                            2,
                            1,
                            &actor_desc,
                            0,
                            nullptr
                        );

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
            auto& rp = *rp_pkg.transp_skin_;

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = rp.renderpass();
            renderPassInfo.framebuffer = rp.fbuf_at(image_index.get());
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = fbuf_ext;
            renderPassInfo.clearValueCount = rp.clear_value_count();
            renderPassInfo.pClearValues = rp.clear_values();

            vkCmdBeginRenderPass(
                cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE
            );
            vkCmdBindPipeline(
                cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(fbuf_ext.width);
            viewport.height = static_cast<float>(fbuf_ext.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cur_cmd_buf, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = { 0, 0 };
            scissor.extent = fbuf_ext;
            vkCmdSetScissor(cur_cmd_buf, 0, 1, &scissor);

            auto desc_frame = desc_sets_.at(frame_index.get());
            vkCmdBindDescriptorSets(
                cur_cmd_buf,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                rp.pipeline_layout(),
                0,
                1,
                &desc_frame,
                0,
                nullptr
            );

            for (auto& pair : draw_sheet.skinned_pairs_) {
                for (auto& unit : pair.model_->runits_alpha_) {
                    auto unit_desc = unit.get_desc_set(frame_index.get());
                    vkCmdBindDescriptorSets(
                        cur_cmd_buf,
                        VK_PIPELINE_BIND_POINT_GRAPHICS,
                        rp.pipeline_layout(),
                        1,
                        1,
                        &unit_desc,
                        0,
                        nullptr
                    );
                    unit.record_bind_vert_buf(cur_cmd_buf);

                    for (auto& actor : pair.actors_) {
                        auto actor_desc = actor.actor_->get_desc_set(
                            frame_index.get()
                        );
                        vkCmdBindDescriptorSets(
                            cur_cmd_buf,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            rp.pipeline_layout(),
                            2,
                            1,
                            &actor_desc,
                            0,
                            nullptr
                        );

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
            auto& rp = *rp_pkg.fillscreen_;
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = rp.renderpass();
            renderPassInfo.framebuffer = rp.fbuf_at(image_index.get());
            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = shain_exd;
            renderPassInfo.clearValueCount = rp.clear_value_count();
            renderPassInfo.pClearValues = rp.clear_values();

            vkCmdBeginRenderPass(
                cmdbuf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE
            );
            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = static_cast<float>(shain_exd.width);
            viewport.height = static_cast<float>(shain_exd.height);
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;
            vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

            VkRect2D scissor{};
            scissor.offset = { 0, 0 };
            scissor.extent = shain_exd;
            vkCmdSetScissor(cmdbuf, 0, 1, &scissor);

            auto desc_main = desc_sets_.at(frame_index.get());
            vkCmdBindDescriptorSets(
                cmdbuf,
                VK_PIPELINE_BIND_POINT_GRAPHICS,
                rp.pipeline_layout(),
                0,
                1,
                &desc_main,
                0,
                nullptr
            );

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

            rp_states_shadow_.pool().add(
                2048, 2048, *rp_.shadowmap_, tex_man_, device_
            );
            rp_states_shadow_.pool().add(
                256, 256, *rp_.shadowmap_, tex_man_, device_
            );

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

            const auto proj_mat = cam.proj_.make_proj_mat(
                swapchain_.width(), swapchain_.height()
            );
            const auto view_mat = cam.view_.make_view_mat();

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
                const auto view_dir = cam.view_.make_forward_dir();

                dlight.transform_.pos_ = cam.view_.pos_;
                dlight.transform_.reset_rotation();
                dlight.transform_.rotate(
                    sung::TAngle<double>::from_deg(-60), { 1, 0, 0 }
                );
                dlight.transform_.rotate(
                    sung::TAngle<double>::from_deg(-85), { 0, 1, 0 }
                );
                /*
                dlight.transform_.rotate(
                    sung::TAngle<double>::from_rad(
                        SUNG_PI * -0.5 - std::atan2(view_dir.z, view_dir.x)
                    ),
                    { 0, 1, 0 }
                );
                */

                rp_states_shadow_.pool().at(0).mat_ = dlight.make_light_mat();
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

            rp_states_shadow_.record_static(
                cur_cmd_buf, draw_sheet, framesync_.get_frame_index(), rp_
            );

            rp_states_shadow_.record_skinned(
                cur_cmd_buf, draw_sheet, framesync_.get_frame_index(), rp_
            );

            rp_states_envmap_.record(
                cur_cmd_buf,
                draw_sheet,
                framesync_.get_frame_index(),
                image_index,
                rp_
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

            rp_states_fillscreen_.record(
                cur_cmd_buf,
                swapchain_.extent(),
                framesync_.get_frame_index(),
                image_index,
                rp_
            );

            // Shader: Overlay
            {
                auto& rp = *rp_.overlay_;
                VkRenderPassBeginInfo renderPassInfo{};
                renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
                renderPassInfo.renderPass = rp.renderpass();
                renderPassInfo.framebuffer = rp.fbuf_at(image_index.get());
                renderPassInfo.renderArea.offset = { 0, 0 };
                renderPassInfo.renderArea.extent = swapchain_.extent();
                renderPassInfo.clearValueCount = rp.clear_value_count();
                renderPassInfo.pClearValues = rp.clear_values();

                vkCmdBeginRenderPass(
                    cur_cmd_buf, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE
                );
                vkCmdBindPipeline(
                    cur_cmd_buf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                VkViewport viewport{};
                viewport.x = 0.0f;
                viewport.y = 0.0f;
                viewport.width = static_cast<float>(swapchain_.width());
                viewport.height = static_cast<float>(swapchain_.height());
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;
                vkCmdSetViewport(cur_cmd_buf, 0, 1, &viewport);

                VkRect2D scissor{};
                scissor.offset = { 0, 0 };
                scissor.extent = swapchain_.extent();
                vkCmdSetScissor(cur_cmd_buf, 0, 1, &scissor);

                widget_ren_data.cmd_buf_ = cur_cmd_buf;
                widget_ren_data.pipe_layout_ = rp.pipeline_layout();
                overlay_man_.record_render(widget_ren_data);

                vkCmdEndRenderPass(cur_cmd_buf);
            }

            VK_CHECK(vkEndCommandBuffer(cur_cmd_buf));

            // Submit and present
            {
                VkSubmitInfo submitInfo{};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

                VkSemaphore waitSemaphores[] = {
                    framesync_.get_cur_img_ava_semaph().get(),
                };
                VkPipelineStageFlags waitStages[] = {
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                };
                submitInfo.waitSemaphoreCount = 1;
                submitInfo.pWaitSemaphores = waitSemaphores;
                submitInfo.pWaitDstStageMask = waitStages;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &cur_cmd_buf;

                VkSemaphore signalSemaphores[] = {
                    framesync_.get_cur_render_fin_semaph().get(),
                };
                submitInfo.signalSemaphoreCount = 1;
                submitInfo.pSignalSemaphores = signalSemaphores;

                VK_CHECK(vkQueueSubmit(
                    device_.graphics_queue(),
                    1,
                    &submitInfo,
                    framesync_.get_cur_in_flight_fence().get()
                ));

                std::array<uint32_t, 1> swapchain_indices{ image_index.get() };
                std::array<VkSwapchainKHR, 1> swapchains{ swapchain_.get() };

                VkPresentInfoKHR presentInfo{};
                presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                presentInfo.waitSemaphoreCount = 1;
                presentInfo.pWaitSemaphores = signalSemaphores;
                presentInfo.swapchainCount = static_cast<uint32_t>(
                    swapchains.size()
                );
                presentInfo.pSwapchains = swapchains.data();
                presentInfo.pImageIndices = swapchain_indices.data();
                presentInfo.pResults = nullptr;

                VK_CHECK(
                    vkQueuePresentKHR(device_.present_queue(), &presentInfo)
                );
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

            rp_.init(
                fbuf_images_.width(),
                fbuf_images_.height(),
                fbuf_images_,
                desclayout_,
                swapchain_,
                device_
            );

            rp_states_shadow_.pool().recreate_fbufs(*rp_.shadowmap_, device_);

            rp_states_envmap_.init(rp_, tex_man_, device_);
            rp_states_compo_.init(
                desclayout_,
                fbuf_images_,
                rp_states_shadow_.pool().get_img_view_at(0),
                rp_states_shadow_.pool().get_img_view_at(1),
                rp_states_envmap_.get_view(0),
                device_
            );
            rp_states_transp_.init(desclayout_, device_);
            rp_states_fillscreen_.init(desclayout_, fbuf_images_, device_);
        }

        void destroy_swapchain_and_relatives() {
            device_.wait_idle();

            rp_states_shadow_.pool().destroy_fbufs(device_);

            rp_states_fillscreen_.destroy(device_);
            rp_states_transp_.destroy(device_);
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
                    ubuf_data.set_dlight_mat(light.make_light_mat());
                    ubuf_data.set_dlight_dir(light.calc_to_light_dir(view_mat));
                    ubuf_data.set_dlight_color(light.color_);
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
        ::RpStatesTransp rp_states_transp_;
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
