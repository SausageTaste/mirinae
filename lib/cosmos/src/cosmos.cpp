#include "mirinae/cosmos.hpp"

#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/lightweight/task.hpp"


namespace {

    class TaskGlobalInit : public mirinae::StageTask {

    public:
        TaskGlobalInit(mirinae::CosmosSimulator& cosmos) : cosmos_(cosmos) {
            fence_.succeed(this);
        }

        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            SPDLOG_INFO("TaskGlobalInit start");
            cosmos_.tick_clock();
            cosmos_.scene().do_frame();
            SPDLOG_INFO("TaskGlobalInit end");
        }

        enki::ITaskSet* get_fence() override { return &fence_; }

    private:
        mirinae::CosmosSimulator& cosmos_;
        mirinae::FenceTask fence_;
    };

}  // namespace


namespace mirinae {

    CosmosSimulator::CosmosSimulator(ScriptEngine& script)
        : scene_(clock_, script) {}

    void CosmosSimulator::register_tasks(TaskGraph& tasks) {
        auto& stage = tasks.stages_.emplace_back();
        stage.task_ = std::make_unique<TaskGlobalInit>(*this);

        scene_.register_tasks(tasks);
    }

    void CosmosSimulator::tick_clock() { clock_.tick(); }

    void CosmosSimulator::do_frame() {
        phys_world_.pre_sync(scene_.clock().dt(), *scene_.reg_);
        phys_world_.do_frame(scene_.clock().dt());
        phys_world_.post_sync(scene_.clock().dt(), *scene_.reg_);
    }

}  // namespace mirinae
