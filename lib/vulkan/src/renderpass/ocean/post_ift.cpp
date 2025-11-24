#include "renderpass/ocean/ocean.hpp"

#include <entt/entity/registry.hpp>
#include <sung/basic/cvar.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/lightweight/task.hpp"

#include "render/cmdbuf.hpp"
#include "render/mem_cinfo.hpp"
#include "renderpass/builder.hpp"
#include "renderpass/ocean/common.hpp"


namespace {

    sung::AutoCVarFlt cv_displace_scale_x{ "ocean:displace_scale_x", "", 1 };
    sung::AutoCVarFlt cv_displace_scale_y{ "ocean:displace_scale_y", "", 1 };


    struct U_OceanFinalizePushConst {
        glm::vec2 hor_displace_scale_;
        float dt_;
        float turb_time_factor_;
        int32_t N_;
    };


    struct FrameData {
        std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> hkt_1_;
        std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> hkt_2_;
        std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> disp_;
        std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> deri_;
        VkDescriptorSet desc_set_;
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
            cmdbuf_ = VK_NULL_HANDLE;
            auto ocean = mirinae::find_ocean_cpnt(*reg_);
            if (!ocean)
                return;

            cmdbuf_ = cmd_pool_->get(ctxt_->f_index_, tid, *device_);
            if (cmdbuf_ == VK_NULL_HANDLE)
                return;

            mirinae::begin_cmdbuf(cmdbuf_, DEBUG_LABEL);
            this->record(cmdbuf_, *frame_data_, *ocean, *rp_, *ctxt_);
            mirinae::end_cmdbuf(cmdbuf_, DEBUG_LABEL);
        }

        static bool record(
            const VkCommandBuffer cmdbuf,
            const ::FrameDataArr& frame_data,
            const mirinae::cpnt::Ocean& ocean_entt,
            const mirinae::IPipelinePair& rp,
            const mirinae::RpCtxt& ctxt
        ) {
            auto& fd = frame_data.at(ctxt.f_index_.get());

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, rp.pipeline()
            );

            mirinae::DescSetBindInfo{}
                .bind_point(VK_PIPELINE_BIND_POINT_COMPUTE)
                .layout(rp.pipe_layout())
                .add(fd.desc_set_)
                .record(cmdbuf);

            VkMemoryBarrier mem_bar = {};
            mem_bar.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            mem_bar.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
            mem_bar.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;

            vkCmdPipelineBarrier(
                cmdbuf,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                0,
                1,
                &mem_bar,
                0,
                nullptr,
                0,
                nullptr
            );

            U_OceanFinalizePushConst pc;
            pc.hor_displace_scale_.x = cv_displace_scale_x.get();
            pc.hor_displace_scale_.y = cv_displace_scale_y.get();
            pc.dt_ = ctxt.dt_;
            pc.turb_time_factor_ = ocean_entt.trub_time_factor_;
            pc.N_ = ::mirinae::OCEAN_TEX_DIM;

            mirinae::PushConstInfo{}
                .layout(rp.pipe_layout())
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

        const mirinae::DebugLabel DEBUG_LABEL{
            "Ocean Post IFT", 0.31, 0.76, 0.97
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

        std::string_view name() const override { return "ocean post"; }

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


// Ocean Finalize
namespace {

    class RpStatesOceanFinalize
        : public mirinae::IRpBase
        , public mirinae::IPipelinePair {

    public:
        RpStatesOceanFinalize(
            mirinae::CosmosSimulator& cosmos,
            mirinae::RpResources& rp_res,
            mirinae::VulkanDevice& device
        )
            : device_(device), cosmos_(cosmos), rp_res_(rp_res) {
            mirinae::CommandPool cmd_pool;
            cmd_pool.init(device);

            // Reference images
            for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                auto& fd = fdata_[i];

                for (size_t j = 0; j < mirinae::CASCADE_COUNT; j++) {
                    fd.hkt_1_[j] = rp_res.ren_img_.get_img_reader(
                        fmt::format("ocean_tilde_hkt:hkt_1_c{}_f{}", j, i),
                        name_s()
                    );
                    MIRINAE_ASSERT(nullptr != fd.hkt_1_[j]);
                    fd.hkt_2_[j] = rp_res.ren_img_.get_img_reader(
                        fmt::format("ocean_tilde_hkt:hkt_2_c{}_f{}", j, i),
                        name_s()
                    );
                    MIRINAE_ASSERT(nullptr != fd.hkt_2_[j]);
                }
            }

            // Storage images
            {
                mirinae::ImageCreateInfo cinfo;
                cinfo.set_dimensions(mirinae::OCEAN_TEX_DIM)
                    .add_usage(VK_IMAGE_USAGE_SAMPLED_BIT)
                    .add_usage(VK_IMAGE_USAGE_STORAGE_BIT);

                mirinae::ImageViewBuilder builder;
                builder.aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT);

                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = fdata_[i];

                    for (size_t j = 0; j < mirinae::CASCADE_COUNT; j++) {
                        const auto i_name = fmt::format(
                            "displacement_c{}_f{}", j, i
                        );
                        auto img = rp_res.ren_img_.new_img(i_name, name_s());
                        MIRINAE_ASSERT(nullptr != img);

                        cinfo.set_format(VK_FORMAT_R32G32B32A32_SFLOAT);
                        img->img_.init(cinfo.get(), device.mem_alloc());
                        builder.image(img->img_.image());

                        builder.format(cinfo.format());
                        img->view_.reset(builder, device);
                        img->set_dbg_names(device_);

                        fd.disp_[j] = std::move(img);
                    }

                    for (size_t j = 0; j < mirinae::CASCADE_COUNT; j++) {
                        const auto i_name = fmt::format(
                            "derivatives_c{}_f{}", j, i
                        );
                        auto img = rp_res.ren_img_.new_img(i_name, name_s());
                        MIRINAE_ASSERT(nullptr != img);

                        cinfo.set_format(VK_FORMAT_R32G32B32A32_SFLOAT);
                        img->img_.init(cinfo.get(), device.mem_alloc());
                        builder.image(img->img_.image());

                        builder.format(cinfo.format());
                        img->view_.reset(builder, device);
                        img->set_dbg_names(device_);

                        fd.deri_[j] = std::move(img);
                    }
                }

                for (size_t j = 0; j < mirinae::CASCADE_COUNT; j++) {
                    const auto i_name = fmt::format("turbulence_c{}", j);
                    auto img = rp_res.ren_img_.new_img(i_name, name_s());
                    MIRINAE_ASSERT(nullptr != img);

                    cinfo.set_format(VK_FORMAT_R32G32B32A32_SFLOAT);
                    img->img_.init(cinfo.get(), device.mem_alloc());
                    builder.image(img->img_.image());

                    builder.format(cinfo.format());
                    img->view_.reset(builder, device);
                    img->set_dbg_names(device_);

                    turb_[j] = std::move(img);
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
                for (auto fd : fdata_) {
                    for (size_t i = 0; i < mirinae::CASCADE_COUNT; i++) {
                        barrier.image(fd.disp_[i]->img_.image());
                        barrier.record_single(
                            cmdbuf,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT
                        );

                        barrier.image(fd.deri_[i]->img_.image());
                        barrier.record_single(
                            cmdbuf,
                            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                            VK_PIPELINE_STAGE_TRANSFER_BIT
                        );
                    }
                }
                for (size_t i = 0; i < mirinae::CASCADE_COUNT; i++) {
                    barrier.image(turb_[i]->img_.image());
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
                builder
                    .new_binding()  // displacement
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(3)
                    .finish_binding()
                    .new_binding()  // derivatives
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(3)
                    .finish_binding()
                    .new_binding()  // turbulence
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(3)
                    .finish_binding()
                    .new_binding()  // hkt dxdy
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(3)
                    .finish_binding()
                    .new_binding()  // hkt dz
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(3)
                    .finish_binding()
                    .new_binding()  // hkt ddxddz
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .set_count(3)
                    .finish_binding();
                rp_res.desclays_.add(builder, device.logi_device());
            }

            // Desciptor Sets
            {
                auto& layout = rp_res.desclays_.get(name_s() + ":main");

                desc_pool_.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    layout.size_info(),
                    device.logi_device()
                );

                auto sets = desc_pool_.alloc(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    layout.layout(),
                    device.logi_device()
                );

                mirinae::DescWriter writer;
                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    auto& fd = fdata_[i];
                    fd.desc_set_ = sets[i];

                    writer.add_storage_img_info(fd.disp_[0]->view_.get())
                        .add_storage_img_info(fd.disp_[1]->view_.get())
                        .add_storage_img_info(fd.disp_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 0);
                    writer.add_storage_img_info(fd.deri_[0]->view_.get())
                        .add_storage_img_info(fd.deri_[1]->view_.get())
                        .add_storage_img_info(fd.deri_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 1);
                    writer.add_storage_img_info(turb_[0]->view_.get())
                        .add_storage_img_info(turb_[1]->view_.get())
                        .add_storage_img_info(turb_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 2);
                    writer.add_storage_img_info(fd.hkt_1_[0]->view_.get())
                        .add_storage_img_info(fd.hkt_1_[1]->view_.get())
                        .add_storage_img_info(fd.hkt_1_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 3);
                    writer.add_storage_img_info(fd.hkt_2_[0]->view_.get())
                        .add_storage_img_info(fd.hkt_2_[1]->view_.get())
                        .add_storage_img_info(fd.hkt_2_[2]->view_.get())
                        .add_storage_img_write(fd.desc_set_, 4);
                }
                writer.apply_all(device.logi_device());
            }

            // Pipeline
            {
                mirinae::PipelineLayoutBuilder{}
                    .add_stage_flags(VK_SHADER_STAGE_COMPUTE_BIT)
                    .pc<U_OceanFinalizePushConst>()
                    .desc(rp_res.desclays_.get(name_s() + ":main").layout())
                    .build(pipe_layout_, device);

                pipeline_ = mirinae::create_compute_pipeline(
                    ":asset/spv/ocean_post_ift_comp.spv", pipe_layout_, device
                );
            }

            cmd_pool.destroy(device.logi_device());
            return;
        }

        ~RpStatesOceanFinalize() override {
            for (auto& fdata : fdata_) {
                for (size_t i = 0; i < mirinae::CASCADE_COUNT; i++) {
                    rp_res_.ren_img_.free_img(fdata.hkt_1_[i]->id(), name_s());
                    rp_res_.ren_img_.free_img(fdata.hkt_2_[i]->id(), name_s());
                    rp_res_.ren_img_.free_img(fdata.disp_[i]->id(), name_s());
                    rp_res_.ren_img_.free_img(fdata.deri_[i]->id(), name_s());
                }

                fdata.desc_set_ = VK_NULL_HANDLE;
            }

            for (size_t i = 0; i < mirinae::CASCADE_COUNT; i++)
                rp_res_.ren_img_.free_img(turb_[i]->id(), name_s());

            desc_pool_.destroy(device_.logi_device());
            pipeline_.destroy(device_);
            pipe_layout_.destroy(device_);
        }

        std::string_view name() const override { return "ocean_finalize"; }

        std::unique_ptr<mirinae::IRpTask> create_task() override {
            auto out = std::make_unique<task::RpTask>();
            out->init(cosmos_.reg(), *this, fdata_, rp_res_.cmd_pool_, device_);
            return out;
        }

        VkPipeline pipeline() const override { return pipeline_; }
        VkPipelineLayout pipe_layout() const override { return pipe_layout_; }

    private:
        mirinae::VulkanDevice& device_;
        mirinae::CosmosSimulator& cosmos_;
        mirinae::RpResources& rp_res_;

        ::FrameDataArr fdata_;
        std::array<mirinae::HRpImage, mirinae::CASCADE_COUNT> turb_;
        mirinae::DescPool desc_pool_;
        mirinae::RpPipeline pipeline_;
        mirinae::RpPipeLayout pipe_layout_;
    };

}  // namespace


namespace mirinae::rp {

    std::unique_ptr<mirinae::IRpBase> create_rp_ocean_post_ift(
        RpCreateBundle& bundle
    ) {
        return std::make_unique<RpStatesOceanFinalize>(
            bundle.cosmos_, bundle.rp_res_, bundle.device_
        );
    }

}  // namespace mirinae::rp
