#include "renderpass/bloom/bloom.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/cpnt/light.hpp"
#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/lightweight/task.hpp"

#include "render/cmdbuf.hpp"
#include "render/mem_cinfo.hpp"
#include "render/vkmajorplayers.hpp"
#include "renderpass/builder.hpp"
#include "renderpass/common.hpp"


namespace {

    struct U_BloomUpPushConst {
        template <typename T>
        U_BloomUpPushConst& aspect_ratio(T width, T height) {
            const auto v = static_cast<double>(height) /
                           static_cast<double>(width);
            aspect_ratio_rcp_ = static_cast<float>(v);
            return *this;
        }

        float filter_radius_ = 0.005;
        float aspect_ratio_rcp_ = 1.0;
    };


    struct FrameData {
        struct DownsampleLevel {
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
            for (auto& lvl : levels_) {
                lvl.destroy(device);
            }
            levels_.clear();
        }

        std::vector<DownsampleLevel> levels_;
        mirinae::HRpImage downsamples_;
        mirinae::HRpImage upsamples_;
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
            this->record_copy(cmdbuf_, fd);
            this->record(cmdbuf_, fd, *reg_, *rp_, *ctxt_);
            mirinae::end_cmdbuf(cmdbuf_, DEBUG_LABEL);
        }

        static void record_copy(
            const VkCommandBuffer cmdbuf, const ::FrameData& fd
        ) {
            std::vector<VkImageCopy> regions;

            for (int i = 0; i < fd.levels_.size(); ++i) {
                auto& lvl = fd.levels_.at(i);
                auto& r = regions.emplace_back();

                r.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                r.srcSubresource.mipLevel = i;
                r.srcSubresource.baseArrayLayer = 0;
                r.srcSubresource.layerCount = 1;

                r.dstSubresource = r.srcSubresource;

                r.srcOffset = { 0, 0, 0 };
                r.dstOffset = { 0, 0, 0 };
                r.extent.width = lvl.extent_.width;
                r.extent.height = lvl.extent_.height;
                r.extent.depth = 1;
            }

            mirinae::ImageMemoryBarrier{}
                .image(fd.downsamples_->img_.image())
                .set_src_access(VK_ACCESS_SHADER_READ_BIT)
                .set_dst_access(VK_ACCESS_TRANSFER_READ_BIT)
                .old_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .set_signle_mip_layer()
                .mip_count(fd.downsamples_->img_.mip_levels())
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT
                );

            mirinae::ImageMemoryBarrier{}
                .image(fd.upsamples_->img_.image())
                .set_src_access(0)
                .set_dst_access(VK_ACCESS_TRANSFER_WRITE_BIT)
                .old_layout(VK_IMAGE_LAYOUT_UNDEFINED)
                .new_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .set_signle_mip_layer()
                .mip_count(fd.upsamples_->img_.mip_levels())
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT
                );

            vkCmdCopyImage(
                cmdbuf,
                fd.downsamples_->img_.image(),
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                fd.upsamples_->img_.image(),
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                (uint32_t)regions.size(),
                regions.data()
            );

            mirinae::ImageMemoryBarrier{}
                .image(fd.upsamples_->img_.image())
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .set_src_acc(VK_ACCESS_TRANSFER_WRITE_BIT)
                .set_dst_acc(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
                .add_dst_acc(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                .old_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                .set_signle_mip_layer()
                .mip_count(fd.upsamples_->img_.mip_levels())
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
                );
        }

        static void record(
            const VkCommandBuffer cmdbuf,
            const ::FrameData& fd,
            const entt::registry& reg,
            const mirinae::IRenPass& rp,
            const mirinae::RpCtxt& ctxt
        ) {
            ::U_BloomUpPushConst pc;
            for (auto e : reg.view<mirinae::cpnt::StandardCamera>()) {
                const auto& cam = reg.get<mirinae::cpnt::StandardCamera>(e);
                pc.filter_radius_ = cam.bloom_radius_;
                break;
            }

            for (int i = 0; i < fd.levels_.size() - 1; ++i) {
                const auto dst_mip = fd.levels_.size() - 2 - i;
                const auto data_mip = fd.levels_.size() - 1 - i;
                auto& stage = fd.levels_.at(dst_mip);

                mirinae::ImageMemoryBarrier{}
                    .image(fd.upsamples_->img_.image())
                    .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .set_src_acc(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT)
                    .add_src_acc(VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
                    .set_dst_acc(VK_ACCESS_SHADER_READ_BIT)
                    .old_layout(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
                    .new_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                    .set_signle_mip_layer()
                    .mip_base(data_mip)
                    .mip_count(1)
                    .record_single(
                        cmdbuf,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
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

                pc.aspect_ratio(stage.extent_.width, stage.extent_.height);

                mirinae::PushConstInfo{}
                    .layout(rp.pipe_layout())
                    .add_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .record(cmdbuf, pc);

                vkCmdDraw(cmdbuf, 3, 1, 0, 0);
                vkCmdEndRenderPass(cmdbuf);
            }
        }

        const mirinae::DebugLabel DEBUG_LABEL{ "Bloom Upsample", 1, 1, 1 };

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

        std::string_view name() const override { return "bloom upsample"; }

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
                auto& fd = frame_data_[i];
                fd.downsamples_ = rp_res_.ren_img_.get_img_reader(
                    fmt::format("bloom downsample:downsamples_f#{}", i),
                    name_s()
                );
            }

            // Allocate images
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& fd = frame_data_.at(i);
                fd.upsamples_ = rp_res_.ren_img_.new_img(
                    fmt::format("upsamples_f#{}", i), name_s()
                );
            }

            // Create images
            this->recreate_images(cmd_pool);

            // Desc layouts
            {
                mirinae::DescLayoutBuilder builder{ name_s() + ":main" };
                builder.add_img(VK_SHADER_STAGE_FRAGMENT_BIT, 1);
                rp_res.desclays_.add(builder, device.logi_device());
            }

            auto& layout = rp_res.desclays_.get("bloom upsample:main");

            // Descriptor Sets
            this->recreate_desc_sets(layout);

            // Pipeline layout
            {
                mirinae::PipelineLayoutBuilder{}
                    .desc(layout.layout())
                    .add_frag_flag()
                    .pc<U_BloomUpPushConst>()
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
                    .add_vert(":asset/spv/bloom_upsample_vert.spv")
                    .add_frag(":asset/spv/bloom_upsample_frag.spv");

                builder.color_blend_state().add().set_additive_blend();

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
                for (auto& lvl : fd.levels_) {
                    lvl.destroy(device_);
                }

                rp_res_.ren_img_.free_img(fd.upsamples_->id(), name_s());
            }

            desc_pool_.destroy(device_.logi_device());
            this->destroy_render_pass_elements(device_);
        }

        std::string_view name() const override { return "bloom upsample"; }

        void on_resize(uint32_t width, uint32_t height) override {
            auto& layout = rp_res_.desclays_.get("bloom upsample:main");

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
                    .add_usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                    .deduce_mip_levels();

                for (auto& fd : frame_data_) {
                    fd.upsamples_->img_.init(cinfo.get(), device_.mem_alloc());
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
                    auto img = fd.upsamples_;

                    builder.image(img->img_.image())
                        .mip_levels(img->img_.mip_levels())
                        .base_mip_level(0);
                    img->view_.reset(builder, device_);
                    img->set_dbg_names(device_);

                    fd.clear_levels(device_);
                    for (size_t j = 0; j < img->img_.mip_levels(); j++) {
                        auto& stage = fd.levels_.emplace_back();

                        builder.mip_levels(1).base_mip_level(j);
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
                    .set_dst_access(VK_ACCESS_TRANSFER_WRITE_BIT)
                    .old_layout(VK_IMAGE_LAYOUT_UNDEFINED)
                    .new_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
                    .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .set_signle_mip_layer();

                auto cmdbuf = cmd_pool.begin_single_time(device_);
                for (auto& fd : frame_data_) {
                    barrier.image(fd.upsamples_->img_.image())
                        .mip_count(fd.upsamples_->img_.mip_levels());
                    barrier.record_single(
                        cmdbuf,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT
                    );
                }
                cmd_pool.end_single_time(cmdbuf, device_);
            }
        }

        void recreate_desc_sets(const mirinae::DescLayout& layout) {
            const auto mip_levels =
                frame_data_.front().upsamples_->img_.mip_levels();
            const auto alloc_count = mirinae::MAX_FRAMES_IN_FLIGHT * mip_levels;

            desc_pool_.init(
                alloc_count, layout.size_info(), device_.logi_device()
            );

            auto desc_sets = desc_pool_.alloc(
                alloc_count, layout.layout(), device_.logi_device()
            );

            mirinae::DescWriter writer;
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& fd = frame_data_[i];

                for (size_t j = 0; j < fd.levels_.size() - 1; j++) {
                    auto& lvl = fd.levels_.at(j);
                    auto& lvl_next = fd.levels_.at(j + 1);

                    lvl.desc_set_ = desc_sets.back();
                    desc_sets.pop_back();

                    writer.add_img_info()
                        .set_img_view(lvl_next.view_.get())
                        .set_sampler(device_.samplers().get_linear_clamp())
                        .set_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                    writer.add_sampled_img_write(lvl.desc_set_, 0);
                }
            }
            writer.apply_all(device_.logi_device());
        }

        void recreate_framebuffers() {
            for (auto& fd : frame_data_) {
                for (auto& stage : fd.levels_) {
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

    std::unique_ptr<IRpBase> create_bloom_upsample(RpCreateBundle& bundle) {
        return std::make_unique<RpStates>(
            bundle.cosmos_, bundle.rp_res_, bundle.device_
        );
    }

}  // namespace mirinae::rp
