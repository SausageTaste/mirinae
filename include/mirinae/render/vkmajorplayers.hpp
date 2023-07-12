#pragma once

#include <vector>

#include <vulkan/vulkan.h>

#include <mirinae/util/konsts.hpp>


namespace mirinae {

    class InstanceFactory {

    public:
        InstanceFactory() {
            {
                appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
                appInfo.pApplicationName = "mirinapp";
                appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
                appInfo.pEngineName = mirinae::ENGINE_NAME;
                appInfo.engineVersion = VK_MAKE_VERSION(mirinae::ENGINE_VERSION_MAJOR, mirinae::ENGINE_VERSION_MINOR, mirinae::ENGINE_VERSION_PATCH);
                appInfo.apiVersion = VK_API_VERSION_1_0;
            }

            {
                uint32_t glfwExtensionCount = 0;
                const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

                createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
                createInfo.pApplicationInfo = &appInfo;
                createInfo.enabledExtensionCount = glfwExtensionCount;
                createInfo.ppEnabledExtensionNames = glfwExtensions;
                createInfo.enabledLayerCount = 0;
            }
        }

        VkInstance create() {
            VkInstance instance;
            VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
            if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
                throw std::runtime_error("failed to create instance!");
            }
            return instance;
        }

        static std::vector<VkExtensionProperties> get_extensions() {
            uint32_t count = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

            std::vector<VkExtensionProperties> extensions(count);
            vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data());
            return extensions;
        }

    private:
        VkApplicationInfo appInfo{};
        VkInstanceCreateInfo createInfo{};

    };


    class VulkanInstance {

    public:
        VulkanInstance() {
            InstanceFactory factory;
            instance_ = factory.create();
        }

        ~VulkanInstance() {
            vkDestroyInstance(instance_, nullptr);
            instance_ = 0;
        }

    private:
        VkInstance instance_ = 0;

    };

}
