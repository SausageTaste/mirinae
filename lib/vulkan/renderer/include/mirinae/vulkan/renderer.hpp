#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <sung/basic/threading.hpp>

#include "mirinae/cosmos.hpp"
#include "mirinae/lightweight/create_info.hpp"
#include "mirinae/lightweight/debug_ren.hpp"
#include "mirinae/lightweight/input_proc.hpp"


namespace mirinae {

    class TaskGraph;


    class IRenderer : public IInputProcessor {

    public:
        virtual ~IRenderer() = default;

        virtual void register_tasks(TaskGraph& tasks) = 0;
        virtual void do_frame() = 0;
        virtual void notify_window_resize(uint32_t width, uint32_t height) = 0;

        virtual IDebugRen& debug_ren() = 0;
    };


    std::unique_ptr<IRenderer> create_vk_renderer(
        mirinae::EngineCreateInfo& cinfo,
        sung::HTaskSche task_sche,
        std::shared_ptr<mirinae::CosmosSimulator> cosmos
    );

}  // namespace mirinae
