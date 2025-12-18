#include "mirinae/scene/jolt_job_sys.hpp"

#include <Jolt/Core/JobSystemWithBarrier.h>
#include <dal/parser/common/task_sys.hpp>
#include <sung/basic/static_pool.hpp>
#include <sung/basic/time.hpp>

#include "mirinae/lightweight/include_spdlog.hpp"


namespace {

    class Counter {

    public:
        void increment() {
            count_.fetch_add(1);
            max_.store(std::max(count_.load(), max_.load()));
        }

        void decrement() { count_.fetch_sub(1); }

        void periodic_report() {
            if (timer_.check_if_elapsed(1.0)) {
                SPDLOG_INFO("Max jobs: {}", max_.load());
                max_.store(0);
            }
        }

    private:
        std::atomic<uint64_t> count_ = 0;
        std::atomic<uint64_t> max_ = 0;
        sung::MonotonicRealtimeTimer timer_;
    };


    class JoltEnkiTaskSystem final : public JPH::JobSystemWithBarrier {

    private:
        class JobTask : private enki::ITaskSet {

        public:
            bool try_exec(Job* inJob) {
                {
                    std::unique_lock<std::mutex> lock(mut_, std::try_to_lock);
                    if (!lock.owns_lock())
                        return false;
                    if (job_)
                        return false;

                    this->try_join();
                    this->set_job(inJob);
                }

                dal::tasker().AddTaskSetToPipe(this);
                return true;
            }

        private:
            void ExecuteRange(enki::TaskSetPartition r, uint32_t tid) override {
                std::lock_guard<std::mutex> lock(mut_);
                joinable_ = true;

                if (job_) {
                    job_->Execute();
                    this->clear_job();
                }
            }

            void clear_job() {
                if (job_) {
                    job_->Release();
                    job_ = nullptr;
                }
            }

            void set_job(Job* const inJob) {
                this->clear_job();
                inJob->AddRef();
                job_ = inJob;
            }

            void try_join() {
                if (joinable_) {
                    joinable_ = false;
                    dal::tasker().WaitforTask(this);
                }
            }

            std::mutex mut_;
            Job* job_ = nullptr;
            bool joinable_ = false;
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
            JobHandle job(job_pool_.alloc(
                inName, inColor, this, inJobFunction, inNumDependencies
            ));

            counter_.increment();
            // counter_.periodic_report();
            return job;
        }

        void FreeJob(Job* inJob) override {
            job_pool_.free(inJob);
            counter_.decrement();
        }

        void QueueJob(Job* inJob) override {
            for (auto& task : task_pool_) {
                if (task.try_exec(inJob)) {
                    return;
                }
            }

            SPDLOG_ERROR("No available task to queue job");
        }

        void QueueJobs(Job** inJobs, JPH::uint inNumJobs) override {
            for (JPH::uint i = 0; i < inNumJobs; ++i) {
                this->QueueJob(inJobs[i]);
            }
        }

    private:
        sung::StaticPool<Job, 1024> job_pool_;
        std::array<JobTask, 1024> task_pool_;
        ::Counter counter_;
    };


}  // namespace

namespace mirinae {

    std::unique_ptr<JPH::JobSystem> create_jolt_job_sys() {
        return std::make_unique<JoltEnkiTaskSystem>();
    }

}  // namespace mirinae
