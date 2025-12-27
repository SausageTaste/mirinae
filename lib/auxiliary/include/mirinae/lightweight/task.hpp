#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <dal/common/task_sys.hpp>


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

        void set_size(uint32_t size);
        void set_size(uint64_t size);

    private:
        std::vector<enki::Dependency> deps_;
    };


    struct FenceTask : public DependingTask {
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override;
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
        template <typename T>
        T* push_back(std::unique_ptr<T>&& stage_task) {
            auto& item = stages_.emplace_back();
            auto ptr = stage_task.get();
            item.task_ = std::move(stage_task);
            return ptr;
        }

        template <typename T, typename... TArgs>
        T* emplace_back(TArgs&&... args) {
            static_assert(
                std::is_constructible_v<T, TArgs...>,
                "T is not constructible with the provided arguments."
            );

            auto& item = stages_.emplace_back();
            auto task = std::make_unique<T>(std::forward<TArgs>(args)...);
            auto ptr = task.get();
            item.task_ = std::move(task);
            return ptr;
        }

        void start();

    private:
        struct Stage {
            std::unique_ptr<StageTask> task_;
        };

        std::vector<Stage> stages_;
    };

}  // namespace mirinae
