#include "mirinae/render/pipeline.hpp"

#include <optional>

#include <spdlog/spdlog.h>

#include "mirinae/util/filesys.hpp"


namespace {
    
    class ShaderModule {

    public:
        ShaderModule() = default;

        ShaderModule(const char* const spv_path, VkDevice logi_device) {
            if (!this->init(spv_path, logi_device)) {
                throw std::runtime_error{ "Failed to initialize a ShaderModule" };
            }
        }

        ShaderModule(const std::filesystem::path& spv_path, VkDevice logi_device) 
            : ShaderModule(spv_path.u8string().c_str(), logi_device)
        {

        }

        ~ShaderModule() {
            if (nullptr != handle_) {
                spdlog::warn("A ShaderModule was not destroyed correctly");
            }
        }

        bool init(const char* const spv_path, VkDevice logi_device) {
            if (const auto data = mirinae::load_file<std::vector<uint32_t>>(spv_path)) {
                if (this->init(data.value(), logi_device)) {
                    return true;
                }
                else {
                    spdlog::error("Failed to create shader with file: {}", spv_path);
                    return false;
                }
            }
            else {
                spdlog::error("Failed to load SPV shader file: {}", spv_path);
                return false;
            }
        }

        bool init(const std::filesystem::path& spv_path, VkDevice logi_device) {
            return this->init(spv_path.u8string().c_str(), logi_device);
        }

        bool init(const std::vector<uint32_t>& spv, VkDevice logi_device) {
            if (auto shader = this->create_shader_module(spv, logi_device)) {
                handle_ = shader.value();
                return true;
            }
            else {
                spdlog::error("Failed to create shader module with given data");
                return false;
            }
        }

        void destroy(VkDevice logi_device) {
            if (nullptr != handle_) {
                vkDestroyShaderModule(logi_device, handle_, nullptr);
                handle_ = nullptr;
            }
        }

        VkShaderModule get() {
            return handle_;
        }

    private:
        static std::optional<VkShaderModule> create_shader_module(const std::vector<uint32_t>& spv, VkDevice logi_device) {
            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = spv.size();
            createInfo.pCode = spv.data();

            VkShaderModule shaderModule;
            if (VK_SUCCESS != vkCreateShaderModule(logi_device, &createInfo, nullptr, &shaderModule))
                return std::nullopt;

            return shaderModule;
        }

        VkShaderModule handle_ = nullptr;

    };

}


namespace mirinae {

    Pipeline create_unorthodox_pipeline(const VkExtent2D& swapchain_extent, RenderPass& renderpass, LogiDevice& logi_device) {
        const auto root_dir = find_resources_folder();
        ::ShaderModule vert_shader{ *root_dir / "shaders" / "unorthodox_vert.spv", logi_device.get() };
        ::ShaderModule frag_shader{ *root_dir / "shaders" / "unorthodox_frag.spv", logi_device.get() };

        std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
        {
            auto& shader_info = shaderStages.emplace_back();
            shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shader_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
            shader_info.module = vert_shader.get();
            shader_info.pName = "main";
        }
        {
            auto& shader_info = shaderStages.emplace_back();
            shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shader_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            shader_info.module = frag_shader.get();
            shader_info.pName = "main";
        }

        std::vector<VkDynamicState> dynamicStates{
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR,
        };

        VkPipelineDynamicStateCreateInfo dynamicState{};
        {
            dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicState.dynamicStateCount = dynamicStates.size();
            dynamicState.pDynamicStates = dynamicStates.data();
        }

        auto binding_description = VertexStatic::make_binding_description();
        auto attribute_descriptions = VertexStatic::make_attribute_descriptions();
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        {
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputInfo.vertexBindingDescriptionCount = 0;
            vertexInputInfo.pVertexBindingDescriptions = &binding_description;
            vertexInputInfo.vertexAttributeDescriptionCount = attribute_descriptions.size();
            vertexInputInfo.pVertexAttributeDescriptions = attribute_descriptions.data();
        }

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        {
            inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssembly.primitiveRestartEnable = VK_FALSE;
        }

        VkPipelineViewportStateCreateInfo viewportState{};
        {
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.scissorCount = 1;
        }

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        {
            rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizer.depthClampEnable = VK_FALSE;
            rasterizer.rasterizerDiscardEnable = VK_FALSE;
            rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
            rasterizer.lineWidth = 1;
            rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
            rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rasterizer.depthBiasEnable = VK_FALSE;
            rasterizer.depthBiasConstantFactor = 0;
            rasterizer.depthBiasClamp = 0;
            rasterizer.depthBiasSlopeFactor = 0;
        }

        VkPipelineMultisampleStateCreateInfo multisampling{};
        {
            multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            multisampling.sampleShadingEnable = VK_FALSE;
            multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
            multisampling.minSampleShading = 1;
            multisampling.pSampleMask = nullptr;
            multisampling.alphaToCoverageEnable = VK_FALSE;
            multisampling.alphaToOneEnable = VK_FALSE;
        }

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        {
            colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachment.blendEnable = VK_FALSE;
            colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        }

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        {
            colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.logicOpEnable = VK_FALSE;
            colorBlending.logicOp = VK_LOGIC_OP_COPY;
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &colorBlendAttachment;
            colorBlending.blendConstants[0] = 0;
            colorBlending.blendConstants[1] = 0;
            colorBlending.blendConstants[2] = 0;
            colorBlending.blendConstants[3] = 0;
        }

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        {
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 0;
            pipelineLayoutInfo.pSetLayouts = nullptr; 
            pipelineLayoutInfo.pushConstantRangeCount = 0; 
            pipelineLayoutInfo.pPushConstantRanges = nullptr; 
        }

        VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(logi_device.get(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = shaderStages.size();;
        pipelineInfo.pStages = shaderStages.data();
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = nullptr;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        pipelineInfo.layout = pipelineLayout;
        pipelineInfo.renderPass = renderpass.get();
        pipelineInfo.subpass = 0;
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; 
        pipelineInfo.basePipelineIndex = -1; 

        VkPipeline graphicsPipeline;
        if (vkCreateGraphicsPipelines(logi_device.get(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        vert_shader.destroy(logi_device.get());
        frag_shader.destroy(logi_device.get());

        return Pipeline{ graphicsPipeline, pipelineLayout };
    }

}