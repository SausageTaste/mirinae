#include "render/vkdebug.hpp"

#define LOAD_VK_FUNC(func_name, device) \
    reinterpret_cast<PFN_##func_name>(vkGetDeviceProcAddr(device, #func_name))


namespace {

    PFN_vkCmdBeginDebugUtilsLabelEXT cmd_begin_debug_label = nullptr;
    PFN_vkCmdEndDebugUtilsLabelEXT cmd_end_debug_label = nullptr;
    PFN_vkSetDebugUtilsObjectNameEXT set_debug_object_name_ = nullptr;

}  // namespace


// DebugLabel
namespace mirinae {

    void DebugLabel::load_funcs(VkDevice device) {
        if (nullptr == ::cmd_begin_debug_label) {
            ::cmd_begin_debug_label = LOAD_VK_FUNC(
                vkCmdBeginDebugUtilsLabelEXT, device
            );
        }

        if (nullptr == ::cmd_end_debug_label) {
            ::cmd_end_debug_label = LOAD_VK_FUNC(
                vkCmdEndDebugUtilsLabelEXT, device
            );
        }
    }

    DebugLabel::DebugLabel() {
        info_ = {};
        info_.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    }

    DebugLabel::DebugLabel(const char* label) {
        info_ = {};
        info_.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        info_.pLabelName = label;
    }

    DebugLabel::DebugLabel(
        const char* label, double r, double g, double b, double a
    ) {
        info_ = {};
        info_.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
        info_.pLabelName = label;
        info_.color[0] = static_cast<float>(r);
        info_.color[1] = static_cast<float>(g);
        info_.color[2] = static_cast<float>(b);
        info_.color[3] = static_cast<float>(a);
    }

    DebugLabel& DebugLabel::set_label(const char* label) {
        info_.pLabelName = label;
        return *this;
    }

    DebugLabel& DebugLabel::set_color(double r, double g, double b, double a) {
        info_.color[0] = static_cast<float>(r);
        info_.color[1] = static_cast<float>(g);
        info_.color[2] = static_cast<float>(b);
        info_.color[3] = static_cast<float>(a);
        return *this;
    }

    void DebugLabel::record_begin(VkCommandBuffer cmdbuf) const {
        if (::cmd_begin_debug_label) {
            ::cmd_begin_debug_label(cmdbuf, &info_);
        }
    }

    void DebugLabel::record_end(VkCommandBuffer cmdbuf) {
        if (::cmd_end_debug_label) {
            ::cmd_end_debug_label(cmdbuf);
        }
    }

}  // namespace mirinae


// DebugAnnoName
namespace mirinae {

    DebugAnnoName::DebugAnnoName() {
        info_ = {};
        info_.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
        info_.pNext = nullptr;
        info_.objectType = VK_OBJECT_TYPE_UNKNOWN;
        info_.objectHandle = 0;
        info_.pObjectName = nullptr;
    }

    void DebugAnnoName::load_funcs(VkDevice device) {
        if (nullptr == ::set_debug_object_name_) {
            ::set_debug_object_name_ = LOAD_VK_FUNC(
                vkSetDebugUtilsObjectNameEXT, device
            );
        }
    }

    DebugAnnoName& DebugAnnoName::set_type(VkObjectType type) {
        info_.objectType = type;
        return *this;
    }

    DebugAnnoName& DebugAnnoName::set_handle(uint64_t handle) {
        info_.objectHandle = handle;
        return *this;
    }

    DebugAnnoName& DebugAnnoName::set_name(const char* name) {
        info_.pObjectName = name;
        return *this;
    }

    void DebugAnnoName::apply(VkDevice device) const {
        if (::set_debug_object_name_)
            ::set_debug_object_name_(device, &info_);
    }

}  // namespace mirinae
