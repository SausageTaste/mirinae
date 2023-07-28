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

    void create_unorthodox_pipeline(VkDevice logi_device) {
        const auto root_dir = find_resources_folder();
        ::ShaderModule vert_shader{ *root_dir / "shaders" / "unorthodox_vert.spv", logi_device };
        ::ShaderModule frag_shader{ *root_dir / "shaders" / "unorthodox_frag.spv", logi_device };

        std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
        {
            auto& shader_info = shader_stages.emplace_back();
            shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shader_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
            shader_info.module = vert_shader.get();
            shader_info.pName = "main";
        }
        {
            auto& shader_info = shader_stages.emplace_back();
            shader_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shader_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            shader_info.module = frag_shader.get();
            shader_info.pName = "main";
        }

        vert_shader.destroy(logi_device);
        frag_shader.destroy(logi_device);
        return;
    }

}
