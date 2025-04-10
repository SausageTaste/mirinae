#include "mirinae/scene/jolt_job_sys.hpp"

#include <Jolt/Core/JobSystemWithBarrier.h>
#include <daltools/common/task_sys.hpp>
#include <sung/basic/static_pool.hpp>

#include "mirinae/lightweight/include_spdlog.hpp"


namespace {

    class JoltEnkiTaskSystem final : public JPH::JobSystemWithBarrier {
        virtual void nortti() {}

    private:
        class MyJob
            : public JPH::JobSystem::Job
            , public enki::ITaskSet {

        public:
            MyJob(
                const char* inJobName,
                const JPH::ColorArg inColor,
                JobSystem* inJobSystem,
                const JobFunction& inJobFunction,
                const JPH::uint32 inNumDependencies
            )
                : Job(inJobName,
                      inColor,
                      inJobSystem,
                      inJobFunction,
                      inNumDependencies) {}

            void ExecuteRange(enki::TaskSetPartition r, uint32_t tid) override {
                this->Execute();
                this->Release();
            }
        };

        class JobPool {

        public:
            [[nodiscard]]
            MyJob* alloc(
                const char* inJobName,
                const JPH::ColorArg inColor,
                JobSystem* inJobSystem,
                const JobFunction& inJobFunction,
                const JPH::uint32 inNumDependencies
            ) {
                auto job = data_.alloc(
                    inJobName,
                    inColor,
                    inJobSystem,
                    inJobFunction,
                    inNumDependencies
                );

                const auto active_count = data_.active_count();
                if (active_count > max_alloc_.load(std::memory_order_acquire)) {
                    SPDLOG_INFO("Max alloc: {} ", active_count);
                    max_alloc_.store(active_count, std::memory_order_release);
                }

                if (job)
                    return job;

                SPDLOG_CRITICAL("No more jobs available in the pool!");
                return nullptr;
            }

            void free(MyJob* inJob) {
                if (!data_.is_valid(inJob)) {
                    SPDLOG_CRITICAL("Invalid job pointer passed to free!");
                    return;
                }

                if (!inJob->IsDone())
                    dal::tasker().WaitforTask(inJob);
                data_.free(inJob);
            }

        private:
            sung::StaticPool<MyJob, 1024> data_;
            std::atomic_size_t max_alloc_;
        };

    public:
        JoltEnkiTaskSystem() : JPH::JobSystemWithBarrier(8) {}

        int GetMaxConcurrency() const override {
            return dal::tasker().GetNumTaskThreads() - 1;
        }

        JobHandle CreateJob(
            const char* inName,
            const JPH::ColorArg inColor,
            const JobFunction& inJobFunction,
            const JPH::uint32 inNumDependencies = 0
        ) override {
            return JobHandle(jobs_.alloc(
                inName, inColor, this, inJobFunction, inNumDependencies
            ));
        }

        void FreeJob(Job* inJob) override {
            auto task = static_cast<MyJob*>(inJob);
            jobs_.free(task);
        }

        void QueueJob(Job* inJob) override {
            auto task = static_cast<MyJob*>(inJob);
            task->AddRef();
            dal::tasker().AddTaskSetToPipe(task);
        }

        void QueueJobs(Job** inJobs, JPH::uint inNumJobs) override {
            for (JPH::uint i = 0; i < inNumJobs; ++i) {
                this->QueueJob(inJobs[i]);
            }
        }

    private:
        JobPool jobs_;
    };

}  // namespace

namespace mirinae {

    std::unique_ptr<JPH::JobSystem> create_jolt_job_sys() {
        return std::make_unique<JoltEnkiTaskSystem>();
    }

}  // namespace mirinae
