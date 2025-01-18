#include "mirinae/render/renderpass/ocean.hpp"

#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/renderpass/builder.hpp"


namespace {

    class RpStatesOceanTest : public mirinae::rp::ocean::IRpStates {

    public:
        bool init(mirinae::rp::ocean::RpCreateParams& params) override {
            auto& device = *params.device_;
            auto& rp_res = *params.rp_res_;
            auto& desclayouts = *params.desclayouts_;

            // Images
            {
                mirinae::ImageCreateInfo cinfo;
                cinfo.set_dimensions(128, 128)
                    .set_format(VK_FORMAT_R32G32B32A32_SFLOAT)
                    .add_usage(VK_IMAGE_USAGE_SAMPLED_BIT)
                    .add_usage(VK_IMAGE_USAGE_STORAGE_BIT);

                mirinae::ImageViewBuilder builder;
                builder.format(cinfo.format())
                    .aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT);

                for (size_t i = 0; i < mirinae::MAX_FRAMES_IN_FLIGHT; i++) {
                    const auto img_name = fmt::format("height_map_f#{}", i);
                    auto img = rp_res.new_img(img_name, this->name());
                    img->img_.init(cinfo.get(), device.mem_alloc());
                    builder.image(img->img_.image());
                    img->view_.reset(builder, device);
                    imgs_.push_back(std::move(img));
                }
            }

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
                for (auto p_img : imgs_) {
                    barrier.image(p_img->img_.image());
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
                mirinae::DescLayoutBuilder builder{ "ocean_test:main" };
                builder.new_binding()
                    .set_type(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                    .set_stage(VK_SHADER_STAGE_COMPUTE_BIT)
                    .add_stage(VK_SHADER_STAGE_FRAGMENT_BIT)
                    .set_count(1)
                    .finish_binding();
                desclayouts.add(builder, device.logi_device());
            }

            // Desciptor Sets
            {
                desc_pool_.init(
                    static_cast<uint32_t>(imgs_.size()),
                    desclayouts.get("ocean_test:main").size_info(),
                    device.logi_device()
                );

                desc_sets_ = desc_pool_.alloc(
                    static_cast<uint32_t>(imgs_.size()),
                    desclayouts.get("ocean_test:main").layout(),
                    device.logi_device()
                );

                mirinae::DescWriteInfoBuilder builder;
                for (size_t i = 0; i < imgs_.size(); i++) {
                    builder.set_descset(desc_sets_[i])
                        .add_storage_img(imgs_[i]->view_.get());
                }
                builder.apply_all(device.logi_device());
            }

            // Pipeline Layout
            {
                pipeline_layout_ =
                    mirinae::PipelineLayoutBuilder{}
                        .desc(desclayouts.get("ocean_test:main").layout())
                        .build(device);
                MIRINAE_ASSERT(VK_NULL_HANDLE != pipeline_layout_);
            }

            // Pipeline
            {
                mirinae::PipelineBuilder::ShaderStagesBuilder shader_builder{
                    device
                };
                shader_builder.add_comp(":asset/spv/ocean_test_comp.spv");

                VkComputePipelineCreateInfo cinfo{};
                cinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
                cinfo.layout = pipeline_layout_;
                cinfo.stage = *shader_builder.data();

                const auto res = vkCreateComputePipelines(
                    device.logi_device(),
                    VK_NULL_HANDLE,
                    1,
                    &cinfo,
                    nullptr,
                    &pipeline_
                );
                MIRINAE_ASSERT(res == VK_SUCCESS);
            }

            return true;
        }

        void destroy(
            mirinae::RpResources& rp_res, mirinae::VulkanDevice& device
        ) override {
            for (auto& img : imgs_) rp_res.free_img(img->id(), this->name());
            imgs_.clear();

            desc_pool_.destroy(device.logi_device());

            if (VK_NULL_HANDLE != pipeline_) {
                vkDestroyPipeline(device.logi_device(), pipeline_, nullptr);
                pipeline_ = VK_NULL_HANDLE;
            }

            if (VK_NULL_HANDLE != pipeline_layout_) {
                vkDestroyPipelineLayout(
                    device.logi_device(), pipeline_layout_, nullptr
                );
                pipeline_layout_ = VK_NULL_HANDLE;
            }
        }

        const std::string& name() const override {
            static const std::string name = "ocean_test";
            return name;
        }

        void record(const mirinae::rp::ocean::RpContext& ctxt) override {
            auto cmdbuf = ctxt.cmdbuf;

            vkCmdBindPipeline(
                cmdbuf, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_
            );
            vkCmdBindDescriptorSets(
                cmdbuf,
                VK_PIPELINE_BIND_POINT_COMPUTE,
                pipeline_layout_,
                0,
                1,
                &desc_sets_[ctxt.f_index.get()],
                0,
                0
            );
            vkCmdDispatch(cmdbuf, 128, 128, 1);
        }

    private:
        std::vector<mirinae::HRpImage> imgs_;
        std::vector<VkDescriptorSet> desc_sets_;
        mirinae::DescPool desc_pool_;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
    };

}  // namespace
namespace mirinae::rp::ocean {

    std::unique_ptr<IRpStates> create_rp_states_ocean_test() {
        return std::make_unique<RpStatesOceanTest>();
    }

}  // namespace mirinae::rp::ocean
