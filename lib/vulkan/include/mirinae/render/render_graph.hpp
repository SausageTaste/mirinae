#pragma once

#include <deque>
#include <string>
#include <vector>

#include "mirinae/render/vkdevice.hpp"


namespace mirinae {

    class RenderGraphImage {

    public:
        RenderGraphImage(const std::string_view name) : name_(name) {}

        RenderGraphImage(const RenderGraphImage&) = delete;
        RenderGraphImage& operator=(const RenderGraphImage&) = delete;

        const std::string& name() const { return name_; }

        RenderGraphImage& set_format(VkFormat x) {
            format_ = x;
            return *this;
        }

    private:
        std::string name_;
        VkFormat format_ = VK_FORMAT_UNDEFINED;
    };


    class RenderGraphPass {

    public:
        RenderGraphPass(const std::string_view name) : name_(name) {}

        RenderGraphPass(const RenderGraphPass&) = delete;
        RenderGraphPass& operator=(const RenderGraphPass&) = delete;

        RenderGraphPass& add_color_output(RenderGraphImage& img) {
            color_outputs_.push_back(&img);
            return *this;
        }

        RenderGraphPass& add_depth_output(RenderGraphImage& img) {
            depth_outputs_.push_back(&img);
            return *this;
        }

    private:
        std::string name_;
        std::vector<RenderGraphImage*> color_outputs_;
        std::vector<RenderGraphImage*> depth_outputs_;
    };


    class RenderGraphDef {

    public:
        RenderGraphImage& new_img(const std::string_view name) {
            images_.emplace_back(name);
            return images_.back();
        }

        RenderGraphPass& new_pass(const std::string_view name) {
            passes_.emplace_back(name);
            return passes_.back();
        }

    private:
        std::deque<RenderGraphImage> images_;
        std::deque<RenderGraphPass> passes_;
    };

}  // namespace mirinae
