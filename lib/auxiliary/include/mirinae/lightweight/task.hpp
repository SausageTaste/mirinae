#pragma once

#include <memory>
#include <vector>

#include <enkiTS/TaskScheduler.h>

#include "mirinae/lightweight/include_spdlog.hpp"


namespace mirinae {

    struct DependingTask : public enki::ITaskSet {

    public:
        template <typename... TArgs>
        void succeed(const TArgs&... prev) {
            assert((((const void*)prev != (const void*)this) && ...));

            deps_.resize(sizeof...(prev));
            size_t i = 0;
            (deps_[i++].SetDependency(prev, this), ...);
        }

    private:
        std::vector<enki::Dependency> deps_;
    };


    struct FenceTask : public DependingTask {
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            SPDLOG_INFO("Fence");
        }
    };


    struct StageTask : public enki::ITaskSet {
        virtual enki::ITaskSet* get_fence() = 0;
    };


    class TaskGraph {

    public:
        void start() {
            for (auto& stage : stages_) {
                SPDLOG_INFO("Stage start");
                dal::tasker().AddTaskSetToPipe(stage.task_.get());
                dal::tasker().WaitforTask(stage.task_->get_fence());
                SPDLOG_INFO("Stage end");
            }
        }

        struct Stage {
            std::unique_ptr<StageTask> task_;
        };

        std::vector<Stage> stages_;
    };

}  // namespace mirinae
