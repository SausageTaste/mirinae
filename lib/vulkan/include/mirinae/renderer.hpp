#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <daltools/filesys/res_mgr.hpp>
#include "mirinae/cosmos.hpp"
#include "mirinae/lightweight/create_info.hpp"
#include "mirinae/lightweight/input_proc.hpp"


namespace mirinae {

    class IRenderer : public IInputProcessor {

    public:
        virtual ~IRenderer() = default;

        virtual void do_frame() = 0;
        virtual void notify_window_resize(uint32_t width, uint32_t height) = 0;
    };


    std::unique_ptr<IRenderer> create_vk_renderer(
        EngineCreateInfo&& cinfo,
        std::shared_ptr<dal::IResourceManager>& res_mgr,
        std::shared_ptr<ScriptEngine>& script,
        std::shared_ptr<CosmosSimulator>& cosmos
    );

}  // namespace mirinae
