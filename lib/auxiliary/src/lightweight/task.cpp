#include "mirinae/lightweight/task.hpp"

#include "mirinae/lightweight/include_spdlog.hpp"


namespace mirinae {

    void DependingTask::set_size(uint32_t size) { m_SetSize = size; }

    void DependingTask::set_size(uint64_t size) {
        if (size > std::numeric_limits<uint32_t>::max()) {
            SPDLOG_WARN("Task size exceeds uint32_t limit: {}", size);
            m_SetSize = std::numeric_limits<uint32_t>::max();
        } else {
            m_SetSize = static_cast<uint32_t>(size);
        }
    }


    void FenceTask::ExecuteRange(enki::TaskSetPartition range, uint32_t tid) {}


    void TaskGraph::start() {
        for (auto& stage : stages_) {
            dal::tasker().AddTaskSetToPipe(stage.task_.get());
            dal::tasker().WaitforTask(stage.task_->get_fence());
        }
    }

}  // namespace mirinae
