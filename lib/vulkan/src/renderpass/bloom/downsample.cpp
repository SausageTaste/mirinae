#include "mirinae/renderpass/bloom.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/mem_cinfo.hpp"
#include "mirinae/render/vkmajorplayers.hpp"
#include "mirinae/renderpass/builder.hpp"
#include "mirinae/renderpass/common.hpp"


namespace {

    constexpr int32_t TEX_RES = 32;


    struct U_BloomDownPushConst {
        float dummy_;
    };


    struct FrameData {
        struct DownsampleStage {
            void destroy(mirinae::VulkanDevice& device) {
                view_.destroy(device);
                fbuf_.destroy(device.logi_device());
            }

            void create_fbuf(
                VkRenderPass render_pass_, mirinae::VulkanDevice& device
            ) {
                mirinae::FbufCinfo fbuf_cinfo;
                fbuf_cinfo.set_rp(render_pass_)
                    .set_dim(extent_)
                    .add_attach(view_.get());
                fbuf_.reset(fbuf_cinfo.build(device), device.logi_device());
            }

            mirinae::ImageView view_;
            mirinae::Fbuf fbuf_;
            VkExtent2D extent_ = {};
            VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
        };

        void clear_levels(mirinae::VulkanDevice& device) {
            for (auto& lvl : stages_down_) {
                lvl.destroy(device);
            }
            stages_down_.clear();
        }

        std::vector<DownsampleStage> stages_down_;
        mirinae::HRpImage downsamples_;
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
            this->record(cmdbuf_, fd, *reg_, *rp_, *ctxt_);
            mirinae::end_cmdbuf(cmdbuf_, DEBUG_LABEL);
        }

        static void record(
            const VkCommandBuffer cmdbuf,
            const ::FrameData& fd,
            const entt::registry& reg,
            const mirinae::IRenPass& rp,
            const mirinae::RpCtxt& ctxt
        ) {
            for (size_t i = 0; i < fd.stages_down_.size(); ++i) {
                auto& stage = fd.stages_down_.at(i);

                mirinae::ImageMemoryBarrier{}
                    .image(fd.downsamples_->img_.image())
                    .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .set_src_acc(VK_ACCESS_TRANSFER_READ_BIT)
                    .set_dst_acc(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                    .old_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                    .new_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .set_signle_mip_layer()
                    .mip_base(i)
                    .mip_count(1)
                    .record_single(
                        cmdbuf,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                    );

                mirinae::RenderPassBeginInfo{}
                    .rp(rp.render_pass())
                    .fbuf(stage.fbuf_.get())
                    .wh(stage.extent_)
                    .clear_value_count(rp.clear_value_count())
                    .clear_values(rp.clear_values())
                    .record_begin(cmdbuf);

                vkCmdBindPipeline(
                    cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, rp.pipeline()
                );

                mirinae::Viewport{ stage.extent_ }.record_single(cmdbuf);
                mirinae::Rect2D{ stage.extent_ }.record_scissor(cmdbuf);

                mirinae::DescSetBindInfo{}
                    .bind_point(VK_PIPELINE_BIND_POINT_GRAPHICS)
                    .layout(rp.pipe_layout())
                    .add(stage.desc_set_)
                    .record(cmdbuf);

                ::U_BloomDownPushConst pc;

                mirinae::PushConstInfo{}
                    .layout(rp.pipe_layout())
                    .add_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .record(cmdbuf, pc);

                vkCmdDraw(cmdbuf, 3, 1, 0, 0);
                vkCmdEndRenderPass(cmdbuf);

                mirinae::ImageMemoryBarrier{}
                    .image(fd.downsamples_->img_.image())
                    .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .set_src_acc(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                    .set_dst_acc(VK_ACCESS_SHADER_READ_BIT)
                    .old_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .new_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                    .set_signle_mip_layer()
                    .mip_base(i)
                    .mip_count(1)
                    .record_single(
                        cmdbuf,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                    );
            }
        }

        const mirinae::DebugLabel DEBUG_LABEL{ "Bloom Downsample", 1, 1, 1 };

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

        std::string_view name() const override { return "bloom downsample"; }

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

            // Allocate images
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& fd = frame_data_.at(i);

                const auto img_name = fmt::format("downsamples_f#{}", i);
                fd.downsamples_ = rp_res.ren_img_.new_img(img_name, name_s());
            }

            // Create images
            this->recreate_images(cmd_pool);

            // Desc layouts
            {
                mirinae::DescLayoutBuilder builder{ name_s() + ":main" };
                builder.add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);
                rp_res.desclays_.add(builder, device.logi_device());
            }

            auto& layout = rp_res.desclays_.get("bloom downsample:main");

            // Descriptor Sets
            this->recreate_desc_sets(layout);

            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}
                    .desc(layout.layout())
                    .add_frag_flag()
                    .pc<U_BloomDownPushConst>()
                    .build(pipe_layout_, device);
            }

            // Render pass
            {
                mirinae::RenderPassBuilder builder;

                builder.attach_desc()
                    .add(device.img_formats().rgb_hdr())
                    .ini_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .fin_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .load_op(VK_ATTACHMENT_LOAD_OP_CLEAR)
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
                    .add_vert(":asset/spv/bloom_downsample_vert.spv")
                    .add_frag(":asset/spv/bloom_downsample_frag.spv");

                builder.color_blend_state().add(false, 1);

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
                for (auto& stage : fd.stages_down_) {
                    stage.destroy(device_);
                }

                rp_res_.ren_img_.free_img(fd.downsamples_->id(), name_s());
            }

            desc_pool_.destroy(device_.logi_device());
            this->destroy_render_pass_elements(device_);
        }

        std::string_view name() const override { return "bloom downsample"; }

        void on_resize(uint32_t width, uint32_t height) override {
            auto& layout = rp_res_.desclays_.get("bloom downsample:main");

            mirinae::CommandPool cmd_pool;
            cmd_pool.init(device_);

            this->recreate_images(cmd_pool);
            this->recreate_desc_sets(layout);
            this->recreate_framebuffers();

            cmd_pool.destroy(device_.logi_device());
        }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            auto out = std::make_unique<::RpTask>();
            out->init(
                cosmos_.reg(), *this, frame_data_, rp_res_.cmd_pool_, device_
            );
            return out;
        }

    private:
        void recreate_images(mirinae::CommandPool& cmd_pool) {
            auto& gbuf = rp_res_.gbuf_;

            const auto img_format = device_.img_formats().rgb_hdr();
            const auto img_width = gbuf.width() / 2;
            const auto img_height = gbuf.height() / 2;

            // Create images
            {
                mirinae::ImageCreateInfo cinfo;
                cinfo.set_dimensions(img_width, img_height)
                    .set_type(VK_IMAGE_TYPE_2D)
                    .set_format(img_format)
                    .add_usage(VK_IMAGE_USAGE_SAMPLED_BIT)
                    .add_usage(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
                    .add_usage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                    .deduce_mip_levels();

                for (auto& fd : frame_data_) {
                    fd.downsamples_->img_.init(
                        cinfo.get(), device_.mem_alloc()
                    );
                }
            }

            // Create views
            {
                mirinae::ImageViewBuilder builder;
                builder.format(img_format)
                    .view_type(VK_IMAGE_VIEW_TYPE_2D)
                    .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .base_arr_layer(0)
                    .arr_layers(1);

                for (auto& fd : frame_data_) {
                    auto& rp_img = *fd.downsamples_;
                    auto& img = rp_img.img_;
                    auto& view = rp_img.view_;

                    builder.image(img.image())
                        .base_mip_level(0)
                        .mip_levels(img.mip_levels());
                    view.reset(builder, device_);
                    rp_img.set_dbg_names(device_);

                    fd.clear_levels(device_);
                    for (size_t j = 0; j < img.mip_levels(); j++) {
                        auto& stage = fd.stages_down_.emplace_back();

                        builder.base_mip_level(j).mip_levels(1);
                        stage.view_.reset(builder, device_);

                        stage.extent_.width = std::max(1u, img_width >> j);
                        stage.extent_.height = std::max(1u, img_height >> j);
                    }
                }
            }

            // Image transitions
            {
                mirinae::ImageMemoryBarrier barrier;
                barrier.set_src_access(0)
                    .set_dst_access(VK_ACCESS_TRANSFER_READ_BIT)
                    .old_layout(VK_IMAGE_LAYOUT_UNDEFINED)
                    .new_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                    .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .set_signle_mip_layer();

                auto cmdbuf = cmd_pool.begin_single_time(device_);
                for (auto& fd : frame_data_) {
                    auto& img = fd.downsamples_->img_;

                    barrier.image(img.image())
                        .mip_count(img.mip_levels())
                        .record_single(
                            cmdbuf,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                        );
                }
                cmd_pool.end_single_time(cmdbuf, device_);
            }
        }

        void recreate_desc_sets(const mirinae::DescLayout& layout) {
            const auto mip_levels =
                frame_data_.front().downsamples_->img_.mip_levels();
            const auto alloc_count = mirinae::MAX_FRAMES_IN_FLIGHT * mip_levels;

            desc_pool_.destroy(device_.logi_device());
            desc_pool_.init(
                alloc_count, layout.size_info(), device_.logi_device()
            );

            auto desc_sets = desc_pool_.alloc(
                alloc_count, layout.layout(), device_.logi_device()
            );

            mirinae::DescWriter writer;
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& fd = frame_data_[i];

                for (size_t j = 0; j < fd.stages_down_.size(); j++) {
                    auto& stage = fd.stages_down_[j];
                    stage.desc_set_ = desc_sets.back();
                    desc_sets.pop_back();

                    const auto view = (j >= 1)
                                          ? fd.stages_down_[j - 1].view_.get()
                                          : rp_res_.gbuf_.compo(i).image_view();

                    writer.add_img_info()
                        .set_img_view(view)
                        .set_sampler(device_.samplers().get_linear_clamp())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(stage.desc_set_, 0);
                }
            }
            writer.apply_all(device_.logi_device());
        }

        void recreate_framebuffers() {
            for (auto& fd : frame_data_) {
                for (auto& stage : fd.stages_down_) {
                    stage.create_fbuf(render_pass_, device_);
                }
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

    std::unique_ptr<IRpBase> create_bloom_downsample(RpCreateBundle& bundle) {
        return std::make_unique<RpStates>(
            bundle.cosmos_, bundle.rp_res_, bundle.device_
        );
    }

}  // namespace mirinae::rp
