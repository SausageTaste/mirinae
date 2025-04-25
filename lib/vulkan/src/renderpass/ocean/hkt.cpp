#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/ocean/ocean.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/renderpass/builder.hpp"
#include "mirinae/renderpass/ocean/common.hpp"


namespace {

    class U_OceanTildeHktPushConst {

    public:
        template <typename T>
        U_OceanTildeHktPushConst& time(T t) {
            time_ = static_cast<float>(t);
            return *this;
        }

        template <typename T>
        U_OceanTildeHktPushConst& repeat_time(T t) {
            repeat_time_ = static_cast<float>(t);
            return *this;
        }

        template <typename T>
        U_OceanTildeHktPushConst& depth(T d) {
            depth_ = static_cast<float>(d);
            return *this;
        }

        U_OceanTildeHktPushConst& L(size_t idx, int32_t value) {
            L_.at(idx) = value;
            return *this;
        }

        U_OceanTildeHktPushConst& N(int32_t n) {
            N_ = n;
            return *this;
        }

    private:
        float time_;
        float repeat_time_;
        float depth_;
        std::array<int32_t, 3> L_;
        int32_t N_;
    };


    struct FrameData {
        std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> hk_;
        std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> hkt_1_;
        std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> hkt_2_;
        VkDescriptorSet desc_set_ = VK_NULL_HANDLE;
    };

    using FrameDataArr = std::array<FrameData, mirinae::MAX_FRAMES_IN_FLIGHT>;

}  // namespace


// Tasks
namespace { namespace task {

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

            mirinae::begin_cmdbuf(cmdbuf_);
            this->record(cmdbuf_, *rp_, *ctxt_, *reg_, *frame_data_);
            mirinae::end_cmdbuf(cmdbuf_);
        }

        static bool record(
            const VkCommandBuffer cmdbuf,
            const mirinae::IPipelinePair& rp,
            const mirinae::RpCtxt& ctxt,
            const entt::registry& reg,
            const ::FrameDataArr& frame_data
        ) {
            auto ocean = mirinae::find_ocean_cpnt(reg);
            if (!ocean)
                return false;
            auto& ocean_entt = *ocean;
            auto& fd = frame_data.at(ctxt.f_index_.get());

            mirinae::ImageMemoryBarrier barrier;
            barrier.set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .set_src_access(VK_ACCESS_SHADER_WRITE_BIT)
                .set_dst_access(VK_ACCESS_SHADER_READ_BIT)
                .old_layout(VK_IMAGE_LAYOUT_GENERAL)
                .new_layout(VK_IMAGE_LAYOUT_GENERAL)
                .set_signle_mip_layer();
            for (size_t i = 0; i < mirinae::CASCADE_COUNT; ++i) {
                barrier.image(fd.hk_[i]->img_.image());
                barrier.record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                );
            }

            barrier.old_lay(VK_IMAGE_LAYOUT_UNDEFINED)
                .set_src_acc(0)
                .set_dst_acc(VK_ACCESS_SHADER_WRITE_BIT);
            for (size_t i = 0; i < mirinae::CASCADE_COUNT; ++i) {
                barrier.image(fd.hkt_1_[i]->img_.image());
                barrier.record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                );
                barrier.image(fd.hkt_2_[i]->img_.image());
                barrier.record_single(
                    cmdbuf,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT
                );
            }

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, rp.pipeline()
            );

            mirinae::DescSetBindInfo{}
                .bind_point(VK_PIPELINE_BIND_POINT_COMPUTE)
                .layout(rp.pipe_layout())
                .add(fd.desc_set_)
                .record(cmdbuf);

            ::U_OceanTildeHktPushConst pc;
            pc.time(ocean_entt.time_)
                .repeat_time(ocean_entt.repeat_time_)
                .depth(ocean_entt.depth_)
                .L(0, ocean_entt.cascades_[0].L_)
                .L(1, ocean_entt.cascades_[1].L_)
                .L(2, ocean_entt.cascades_[2].L_)
                .N(mirinae::OCEAN_TEX_DIM);

            mirinae::PushConstInfo pc_info;
            pc_info.layout(rp.pipe_layout())
                .add_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                .record(cmdbuf, pc);

            vkCmdDispatch(
                cmdbuf,
                mirinae::OCEAN_TEX_DIM / 16,
                mirinae::OCEAN_TEX_DIM / 16,
                mirinae::CASCADE_COUNT
            );

            return true;
        }

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

        std::string_view name() const override { return "ocean hkt"; }

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


// Ocean Tilde Hkt
namespace {

    class RpStatesOceanTildeHkt
        : public mirinae::IRpBase
        , public mirinae::IPipelinePair {

    public:
        RpStatesOceanTildeHkt(
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
                cinfo.set_dimensions(mirinae::OCEAN_TEX_DIM)
                    .set_format(VK_FORMAT_R32G32B32A32_SFLOAT)
                    .add_usage(VK_IMAGE_USAGE_STORAGE_BIT);

                mirinae::ImageViewBuilder builder;
                builder.format(cinfo.format())
                    .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT);

                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = frame_data_[i];

                    for (size_t j = 0; j < mirinae::CASCADE_COUNT; ++j) {
                        const auto img_name = fmt::format(
                            "hkt_1_c{}_f{}", j, i
                        );
                        auto img = rp_res.ren_img_.new_img(img_name, names());
                        img->img_.init(cinfo.get(), device.mem_alloc());
                        builder.image(img->img_.image());
                        img->view_.reset(builder, device);
                        fd.hkt_1_[j] = img;
                    }

                    for (size_t j = 0; j < mirinae::CASCADE_COUNT; ++j) {
                        const auto img_name = fmt::format(
                            "hkt_2_c{}_f{}", j, i
                        );
                        auto img = rp_res.ren_img_.new_img(img_name, names());
                        img->img_.init(cinfo.get(), device.mem_alloc());
                        builder.image(img->img_.image());
                        img->view_.reset(builder, device);
                        fd.hkt_2_[j] = img;
                    }
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
                    for (size_t i = 0; i < mirinae::CASCADE_COUNT; ++i) {
                        barrier.image(fd.hkt_1_[i]->img_.image());
                        barrier.record_single(
                            cmdbuf,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT
                        );

                        barrier.image(fd.hkt_2_[i]->img_.image());
                        barrier.record_single(
                            cmdbuf,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT
                        );
                    }
                }
                cmd_pool.end_single_time(cmdbuf, device);
                cmd_pool.destroy(device.logi_device());
            }

            // Reference images
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                for (size_t j = 0; j < mirinae::CASCADE_COUNT; ++j) {
                    const auto img_name = fmt::format(
                        "ocean_tilde_h:height_map_c{}_f#{}", j, i
                    );
                    auto img = rp_res.ren_img_.get_img_reader(
                        img_name, this->names()
                    );
                    MIRINAE_ASSERT(nullptr != img);
                    frame_data_[i].hk_[j] = img;
                }
            }

            // Desc layouts
            {
                mirinae::DescLayoutBuilder builder{ this->names() + ":main" };
                builder
                    .new_binding()  // hkt_1
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(3)
                    .finish_binding()
                    .new_binding()  // hkt_2
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(3)
                    .finish_binding()
                    .new_binding()  // hk
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(3)
                    .finish_binding();
                rp_res.desclays_.add(builder, device.logi_device());
            }

            // Desciptor Sets
            {
                auto& layout = rp_res.desclays_.get(this->names() + ":main");

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

                    writer.add_storage_img_info(fd.hkt_1_[0]->view_.get())
                        .add_storage_img_info(fd.hkt_1_[1]->view_.get())
                        .add_storage_img_info(fd.hkt_1_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 0);
                    writer.add_storage_img_info(fd.hkt_2_[0]->view_.get())
                        .add_storage_img_info(fd.hkt_2_[1]->view_.get())
                        .add_storage_img_info(fd.hkt_2_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 1);
                    writer.add_storage_img_info(fd.hk_[0]->view_.get())
                        .add_storage_img_info(fd.hk_[1]->view_.get())
                        .add_storage_img_info(fd.hk_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 2);
                }
                writer.apply_all(device.logi_device());
            }

            // Pipeline
            {
                mirinae::PipelineLayoutBuilder{}
                    .add_stage_flags(VK_SHADER_STAGE_COMPUTE_BIT)
                    .pc<U_OceanTildeHktPushConst>()
                    .desc(rp_res.desclays_.get(names() + ":main").layout())
                    .build(pipe_layout_, device);

                pipeline_ = mirinae::create_compute_pipeline(
                    ":asset/spv/ocean_tilde_hkt_comp.spv", pipe_layout_, device
                );
            }

            cmd_pool.destroy(device.logi_device());
            return;
        }

        ~RpStatesOceanTildeHkt() override {
            for (auto& fd : frame_data_) {
                for (size_t i = 0; i < mirinae::CASCADE_COUNT; ++i) {
                    rp_res_.ren_img_.free_img(fd.hk_[i]->id(), names());
                    rp_res_.ren_img_.free_img(fd.hkt_1_[i]->id(), names());
                    rp_res_.ren_img_.free_img(fd.hkt_2_[i]->id(), names());
                }
            }

            desc_pool_.destroy(device_.logi_device());
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        std::string_view name() const override { return "ocean_tilde_hkt"; }
        std::string names() const { return std::string(this->name()); }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            auto out = std::make_unique<task::RpTask>();
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

    std::unique_ptr<mirinae::IRpBase> create_rp_ocean_hkt(
        RpCreateBundle& bundle
    ) {
        return std::make_unique<RpStatesOceanTildeHkt>(
            bundle.cosmos_, bundle.rp_res_, bundle.device_
        );
    }

}  // namespace mirinae::rp
