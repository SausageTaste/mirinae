#include "mirinae/renderpass/atmos/sky.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/vkmajorplayers.hpp"
#include "mirinae/renderpass/builder.hpp"
#include "mirinae/renderpass/common.hpp"


namespace {

    constexpr int32_t LUT_WIDTH = 256;
    constexpr int32_t LUT_HEIGHT = 64;


    class U_AtmosTransLutPushConst {

    public:
        U_AtmosTransLutPushConst& pv_inv(const glm::mat4& x) {
            pv_inv_ = x;
            return *this;
        }

    private:
        glm::mat4 pv_inv_;
        int32_t output_width_ = LUT_WIDTH;
        int32_t output_height_ = LUT_HEIGHT;
    };


    struct FrameData {
        mirinae::HRpImage trans_lut_;
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
            const mirinae::IPipelinePair& rp,
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

            mirinae::begin_cmdbuf(cmdbuf_, DEBUG_LABEL);
            this->record(cmdbuf_, *frame_data_, *rp_, *ctxt_);
            mirinae::end_cmdbuf(cmdbuf_, DEBUG_LABEL);
        }

        static bool record(
            const VkCommandBuffer cmdbuf,
            const ::FrameDataArr& frame_data,
            const mirinae::IPipelinePair& rp,
            const mirinae::RpCtxt& ctxt
        ) {
            auto& fd = frame_data.at(ctxt.f_index_.get());

            mirinae::ImageMemoryBarrier{}
                .image(fd.trans_lut_->img_.image())
                .set_src_access(VK_ACCESS_SHADER_READ_BIT)
                .set_dst_access(VK_ACCESS_SHADER_WRITE_BIT)
                .old_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_GENERAL)
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .set_signle_mip_layer()
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                );

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, rp.pipeline()
            );

            mirinae::DescSetBindInfo{}
                .bind_point(VK_PIPELINE_BIND_POINT_COMPUTE)
                .layout(rp.pipe_layout())
                .add(fd.desc_set_)
                .record(cmdbuf);

            ::U_AtmosTransLutPushConst pc;
            pc.pv_inv(glm::inverse(ctxt.main_cam_.pv()));

            mirinae::PushConstInfo{}
                .layout(rp.pipe_layout())
                .add_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                .record(cmdbuf, pc);

            vkCmdDispatch(cmdbuf, ::LUT_WIDTH / 16, ::LUT_HEIGHT / 16, 1);

            mirinae::ImageMemoryBarrier{}
                .image(fd.trans_lut_->img_.image())
                .set_src_access(VK_ACCESS_SHADER_WRITE_BIT)
                .set_dst_access(VK_ACCESS_SHADER_READ_BIT)
                .old_layout(VK_IMAGE_LAYOUT_GENERAL)
                .new_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .set_signle_mip_layer()
                .record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
                );

            return true;
        }

        const mirinae::DebugLabel DEBUG_LABEL{
            "Atmos Trans LUT", 0.9, 0.45, 0.45
        };

        mirinae::FenceTask fence_;
        VkCommandBuffer cmdbuf_ = VK_NULL_HANDLE;

        const ::FrameDataArr* frame_data_ = nullptr;
        const entt::registry* reg_ = nullptr;
        const mirinae::IPipelinePair* rp_ = nullptr;
        const mirinae::RpCtxt* ctxt_ = nullptr;
        mirinae::RpCommandPool* cmd_pool_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
    };


    class RpTask : public mirinae::IRpTask {

    public:
        RpTask() {}

        void init(
            const entt::registry& reg,
            const mirinae::IPipelinePair& rp,
            const ::FrameDataArr& frame_data,
            mirinae::RpCommandPool& cmd_pool,
            mirinae::VulkanDevice& device
        ) {
            record_tasks_.init(reg, rp, frame_data, cmd_pool, device);
        }

        std::string_view name() const override { return "atmos trans LUT"; }

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
        , public mirinae::IPipelinePair {

    public:
        RpStates(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device
        )
            : device_(device), cosmos_(cosmos), rp_res_(rp_res) {
            mirinae::CommandPool cmd_pool;
            cmd_pool.init(device);

            // Storage images
            {
                mirinae::ImageCreateInfo cinfo;
                cinfo.set_dimensions(256, 64)
                    .set_format(VK_FORMAT_R16G16B16A16_SFLOAT)
                    .add_usage(VK_IMAGE_USAGE_SAMPLED_BIT)
                    .add_usage(VK_IMAGE_USAGE_STORAGE_BIT);

                mirinae::ImageViewBuilder builder;
                builder.format(cinfo.format())
                    .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT);

                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    const auto img_name = fmt::format("trans_lut_f#{}", i);
                    auto img = rp_res.ren_img_.new_img(img_name, name_s());
                    img->img_.init(cinfo.get(), device.mem_alloc());
                    frame_data_.at(i).trans_lut_ = img;

                    builder.image(img->img_.image());
                    img->view_.reset(builder, device);
                }
            }

            // Image transitions
            {
                mirinae::ImageMemoryBarrier barrier;
                barrier.set_src_access(0)
                    .set_dst_access(VK_ACCESS_TRANSFER_WRITE_BIT)
                    .old_layout(VK_IMAGE_LAYOUT_UNDEFINED)
                    .new_layout(VK_IMAGE_LAYOUT_GENERAL)
                    .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                    .layer_count(1)
                    .mip_count(1);

                mirinae::CommandPool cmd_pool;
                cmd_pool.init(device);
                auto cmdbuf = cmd_pool.begin_single_time(device);
                for (auto& fd : frame_data_) {
                    barrier.image(fd.trans_lut_->img_.image());
                    barrier.record_single(
                        cmdbuf,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT
                    );
                }
                cmd_pool.end_single_time(cmdbuf, device);
                cmd_pool.destroy(device.logi_device());
            }

            // Desc layouts
            {
                mirinae::DescLayoutBuilder builder{ name_s() + ":main" };
                builder.new_binding()
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(1)
                    .finish_binding();
                rp_res.desclays_.add(builder, device.logi_device());
            }

            auto& layout = rp_res.desclays_.get(name_s() + ":main");

            // Desciptor Sets
            {
                desc_pool_.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    layout.size_info(),
                    device.logi_device()
                );

                auto desc_sets = desc_pool_.alloc(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    layout.layout(),
                    device.logi_device()
                );

                mirinae::DescWriter writer;
                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = frame_data_[i];
                    fd.desc_set_ = desc_sets[i];

                    writer.add_storage_img_info(fd.trans_lut_->view_.get())
                        .add_storage_img_write(fd.desc_set_, 0);
                }
                writer.apply_all(device.logi_device());
            }

            // Pipeline
            {
                mirinae::PipelineLayoutBuilder{}
                    .add_stage_flags(VK_SHADER_STAGE_COMPUTE_BIT)
                    .pc<U_AtmosTransLutPushConst>()
                    .desc(layout.layout())
                    .build(pipe_layout_, device);

                pipeline_ = mirinae::create_compute_pipeline(
                    ":asset/spv/atmos_trans_lut_comp.spv", pipe_layout_, device
                );
            }

            cmd_pool.destroy(device.logi_device());
            return;
        }

        ~RpStates() override {
            for (auto& fd : frame_data_) {
                rp_res_.ren_img_.free_img(fd.trans_lut_->id(), name_s());
                fd.desc_set_ = VK_NULL_HANDLE;
            }

            desc_pool_.destroy(device_.logi_device());
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        std::string_view name() const override { return "atmos trans LUT"; }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            auto out = std::make_unique<::RpTask>();
            out->init(
                cosmos_.reg(), *this, frame_data_, rp_res_.cmd_pool_, device_
            );
            return out;
        }

        VkPipeline pipeline() const override { return pipeline_; }
        VkPipelineLayout pipe_layout() const override { return pipe_layout_; }

    private:
        mirinae::VulkanDevice& device_;
        mirinae::CosmosSimulator& cosmos_;
        mirinae::RpResources& rp_res_;

        ::FrameDataArr frame_data_;
        mirinae::DescPool desc_pool_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;

        sung::MonotonicRealtimeTimer timer_;
    };

}  // namespace


namespace mirinae::rp {

    std::unique_ptr<IRpBase> create_rp_atmos_trans_lut(RpCreateBundle& bundle) {
        return std::make_unique<RpStates>(
            bundle.cosmos_, bundle.rp_res_, bundle.device_
        );
    }

}  // namespace mirinae::rp
