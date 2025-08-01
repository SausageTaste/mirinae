#pragma once

#include <vulkan/vulkan.h>


namespace mirinae {

    class DebugLabel {

    public:
        static void load_funcs(VkDevice device);

        DebugLabel();
        DebugLabel(const char* label);
        DebugLabel(const char*, double r, double g, double b, double a = 0.5);

        DebugLabel& set_label(const char* label);
        DebugLabel& set_color(double r, double g, double b, double a = 0.5);

        void record_begin(VkCommandBuffer cmdbuf) const;
        static void record_end(VkCommandBuffer cmdbuf);

    private:
        static PFN_vkCmdBeginDebugUtilsLabelEXT vkCmdBeginDebugUtilsLabelEXT;
        static PFN_vkCmdEndDebugUtilsLabelEXT vkCmdEndDebugUtilsLabelEXT;

        VkDebugUtilsLabelEXT info_;
    };


    class DebugAnnoName {

    public:
        DebugAnnoName();

        static void load_funcs(VkDevice device);

        DebugAnnoName& set_type(VkObjectType type);
        DebugAnnoName& set_handle(uint64_t handle);
        DebugAnnoName& set_name(const char* name);

        template <typename T>
        DebugAnnoName& set_handle(T handle) {
            info_.objectHandle = (uint64_t)handle;
            return *this;
        }

        void apply(VkDevice device) const;

    private:
        static PFN_vkSetDebugUtilsObjectNameEXT set_debug_object_name_;

        VkDebugUtilsObjectNameInfoEXT info_;
    };

}
