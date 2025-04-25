#include "mirinae/vulkan_pch.h"

#include "mirinae/renderpass/ocean/common.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/renderpass/builder.hpp"


namespace mirinae {

    const cpnt::Ocean* find_ocean_cpnt(const entt::registry& reg) {
        for (auto e : reg.view<cpnt::Ocean>()) {
            // Only one ocean is allowed
            return &reg.get<cpnt::Ocean>(e);
        }
        return nullptr;
    }

    VkPipeline create_compute_pipeline(
        const std::filesystem::path& spv_path,
        const VkPipelineLayout pipeline_layout,
        mirinae::VulkanDevice& device
    ) {
        mirinae::PipelineBuilder::ShaderStagesBuilder shader_builder{ device };
        shader_builder.add_comp(spv_path);

        VkComputePipelineCreateInfo cinfo{};
        cinfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        cinfo.layout = pipeline_layout;
        cinfo.stage = *shader_builder.data();

        VkPipeline pipeline = VK_NULL_HANDLE;
        const auto res = vkCreateComputePipelines(
            device.logi_device(), VK_NULL_HANDLE, 1, &cinfo, nullptr, &pipeline
        );
        MIRINAE_ASSERT(res == VK_SUCCESS);

        return pipeline;
    }

}  // namespace mirinae
