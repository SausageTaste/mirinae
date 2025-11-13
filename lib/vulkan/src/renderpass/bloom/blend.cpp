#include "renderpass/bloom/bloom.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/camera.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/task.hpp"

#include "render/cmdbuf.hpp"
#include "render/mem_cinfo.hpp"
#include "render/vkmajorplayers.hpp"
#include "renderpass/builder.hpp"
#include "renderpass/common.hpp"


namespace {

    struct U_BloomBlendPushConst {
        float strength_ = 0.03;
    };


    struct FrameData {
        mirinae::HRpImage upsamples_;
        mirinae::Fbuf fbuf_;
        VkExtent2D extent_ = {};
        VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
    };

    using FrameDataArr = std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT>;

}  // namespace


// Tasks
namespace {

    class DrawTasks : public mirinae::DependingTask {

    public:
        DrawTasks() { fence_.succeed(this); }

        void init(
            const entt::registry& reg,
            const mirinae::IRenPass& rp,
            const ::FrameDataArr& frame_data,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            reg_ = &reg;
            rp_ = &rp;
            frame_data_ = &frame_data;
            cmd_pool_ = &cmd_pool;
            device_ = &device;
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
            cmdbuf_ = cmd_pool_->get(ctxt_->f_index_, tid, *device_);
            if (cmdbuf_ == VK_NULL_HANDLE)
                return;

            auto& fd = frame_data_->at(ctxt_->f_index_.get());

            mirinae::begin_cmdbuf(cmdbuf_, DEBUG_LABEL);
            this->record(cmdbuf_, fd, *reg_, *rp_);
            mirinae::end_cmdbuf(cmdbuf_, DEBUG_LABEL);
        }

        static void record(
            const VkCommandBuffer cmdbuf,
            const ::FrameData& fd,
            const entt::registry& reg,
            const mirinae::IRenPass& rp
        ) {
            mirinae::ImageMemoryBarrier{}
                .image(fd.upsamples_->img_.image())
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .set_src_acc(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
                .add_src_acc(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                .set_dst_acc(VK_ACCESS_SHADER_READ_BIT)
                .old_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                .set_signle_mip_layer()
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );

            mirinae::RenderPassBeginInfo{}
                .rp(rp.render_pass())
                .fbuf(fd.fbuf_.get())
                .wh(fd.extent_)
                .clear_value_count(rp.clear_value_count())
                .clear_values(rp.clear_values())
                .record_begin(cmdbuf);

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
            );

            mirinae::Viewport{ fd.extent_ }.record_single(cmdbuf);
            mirinae::Rect2D{ fd.extent_ }.record_scissor(cmdbuf);

            mirinae::DescSetBindInfo{}
                .bind_point(VK_PIPELINE_BIND_POINT_GRAPHICS)
                .layout(rp.pipe_layout())
                .add(fd.desc_set_)
                .record(cmdbuf);

            ::U_BloomBlendPushConst pc;
            for (auto e : reg.view<mirinae::cpnt::StandardCamera>()) {
                const auto& cam = reg.get<mirinae::cpnt::StandardCamera>(e);
                pc.strength_ = cam.bloom_strength_;
                break;
            }

            mirinae::PushConstInfo{}
                .layout(rp.pipe_layout())
                .add_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                .record(cmdbuf, pc);

            vkCmdDraw(cmdbuf, 3, 1, 0, 0);
            vkCmdEndRenderPass(cmdbuf);
        }

        const mirinae::DebugLabel DEBUG_LABEL{ "Bloom Blend", 1, 1, 1 };

        mirinae::FenceTask fence_;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;

        const ::FrameDataArr* frame_data_ = nullptr;
        const entt::registry* reg_ = nullptr;
        const mirinae::IRenPass* rp_ = nullptr;
        const mirinae::RpCtxt* ctxt_ = nullptr;
        mirinae::RpCommandPool* cmd_pool_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
    };


    class RpTask : public mirinae::IRpTask {

    public:
        RpTask() {}

        void init(
            const entt::registry& reg,
            const mirinae::IRenPass& rp,
            const ::FrameDataArr& frame_data,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            record_tasks_.init(reg, rp, frame_data, cmd_pool, device);
        }

        std::string_view name() const override { return "bloom blend"; }

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

}  // namespace


namespace {

    class RpStates
        : public mirinae::IRpBase
        , public mirinae::RenPassBundle<1> {

    public:
        RpStates(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device
        )
            : device_(device), cosmos_(cosmos), rp_res_(rp_res) {
            auto& gbuf = rp_res_.gbuf_;
            mirinae::CommandPool cmd_pool;
            cmd_pool.init(device);

            // Image reference
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& fd = frame_data_.at(i);
                fd.upsamples_ = rp_res_.ren_img_.get_img_reader(
                    fmt::format("bloom upsample:upsamples_f#{}", i), name_s()
                );
                MIRINAE_ASSERT(nullptr != fd.upsamples_);
            }

            // Desc layouts
            {
                mirinae::DescLayoutBuilder builder{ name_s() + ":main" };
                builder.add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);
                rp_res.desclays_.add(builder, device.logi_device());
            }

            auto& layout = rp_res.desclays_.get("bloom blend:main");

            // Descriptor Sets
            this->recreate_desc_sets(layout);

            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}
                    .desc(layout.layout())
                    .add_frag_flag()
                    .pc<U_BloomBlendPushConst>()
                    .build(pipe_layout_, device);
            }

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(device.img_formats().rgb_hdr())
                    .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .load_op(VK_ATTACHMENT_LOAD_OP_LOAD)
                    .stor_op(VK_ATTACHMENT_STORE_OP_STORE);

                builder.color_attach_ref().add_color_attach(0);

                builder.depth_attach_ref().clear();

                builder.subpass_dep().add().preset_single();

                render_pass_.reset(builder.build(device.logi_device()), device);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder builder{ device };

                builder.shader_stages()
                    .add_vert(":asset/spv/bloom_blend_vert.spv")
                    .add_frag(":asset/spv/bloom_blend_frag.spv");

                builder.color_blend_state().add(true).set_alpha_blend();

                builder.dynamic_state().add_viewport().add_scissor();

                pipeline_.reset(
                    builder.build(render_pass_, pipe_layout_), device
                );
            }

            // Framebuffers
            this->recreate_framebuffers();

            // Misc
            {
                clear_values_[0].color = { 0, 0, 0, 1 };
            }

            cmd_pool.destroy(device.logi_device());
        }

        ~RpStates() override {
            for (auto& fd : frame_data_) {
                fd.fbuf_.destroy(device_.logi_device());
                rp_res_.ren_img_.free_img(fd.upsamples_->id(), name_s());
            }

            desc_pool_.destroy(device_.logi_device());
            this->destroy_render_pass_elements(device_);
        }

        std::string_view name() const override { return "bloom blend"; }

        void on_resize(uint32_t width, uint32_t height) override {
            auto& layout = rp_res_.desclays_.get("bloom upsample:main");

            this->recreate_desc_sets(layout);
            this->recreate_framebuffers();
        }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            auto out = std::make_unique<::RpTask>();
            out->init(
                cosmos_.reg(), *this, frame_data_, rp_res_.cmd_pool_, device_
            );
            return out;
        }

    private:
        void recreate_desc_sets(const mirinae::DescLayout& layout) {
            desc_pool_.init(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                layout.size_info(),
                device_.logi_device()
            );

            auto desc_sets = desc_pool_.alloc(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                layout.layout(),
                device_.logi_device()
            );

            mirinae::DescWriter writer;
            for (auto& fd : frame_data_) {
                fd.desc_set_ = desc_sets.back();
                desc_sets.pop_back();

                writer.add_img_info()
                    .set_img_view(fd.upsamples_->view_.get())
                    .set_sampler(device_.samplers().get_linear_clamp())
                    .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                writer.add_sampled_img_write(fd.desc_set_, 0);
            }
            writer.apply_all(device_.logi_device());
        }

        void recreate_framebuffers() {
            auto& gbuf = rp_res_.gbuf_;

            for (int i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; ++i) {
                const mirinae::FrameIndex f_idx(i);
                auto& fd = frame_data_.at(i);
                fd.extent_ = gbuf.extent();

                mirinae::FbufCinfo fbuf_cinfo;
                fbuf_cinfo.set_rp(render_pass_)
                    .set_dim(fd.extent_)
                    .add_attach(gbuf.compo(f_idx).image_view());
                fd.fbuf_.reset(
                    fbuf_cinfo.build(device_), device_.logi_device()
                );
            }
        }

        mirinae::VulkanDevice& device_;
        mirinae::CosmosSimulator& cosmos_;
        mirinae::RpResources& rp_res_;

        ::FrameDataArr frame_data_;
        mirinae::DescPool desc_pool_;
    };

}  // namespace


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_bloom_blend(RpCreateBundle& bundle) {
        return std::make_unique<RpStates>(
            bundle.cosmos_, bundle.rp_res_, bundle.device_
        );
    }

}  // namespace mirinae::rp
