#pragma once

#include <memory>
#include <vector>

#include <daltools/common/task_sys.hpp>


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

        void set_size(uint32_t size) { m_SetSize = size; }
        void set_size(uint64_t size) { m_SetSize = size; }

    private:
        std::vector<enki::Dependency> deps_;
    };


    struct FenceTask : public DependingTask {
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
        }
    };


    struct StageTask : public enki::ITaskSet {

    public:
        StageTask(std::string_view name) : name_(name) {}

        virtual enki::ITaskSet* get_fence() = 0;

        const std::string& name() const { return name_; }

    private:
        std::string name_;
    };


    class TaskGraph {

    public:
        template <typename T, typename... TArgs>
        T* emplace_back(TArgs&&... args) {
            auto& item = stages_.emplace_back();
            auto task = std::make_unique<T>(std::forward<TArgs>(args)...);
            auto ptr = task.get();
            item.task_ = std::move(task);
            return ptr;
        }

        void start() {
            for (auto& stage : stages_) {
                dal::tasker().AddTaskSetToPipe(stage.task_.get());
                dal::tasker().WaitforTask(stage.task_->get_fence());
            }
        }

        struct Stage {
            std::unique_ptr<StageTask> task_;
        };

        std::vector<Stage> stages_;
    };

}  // namespace mirinae
