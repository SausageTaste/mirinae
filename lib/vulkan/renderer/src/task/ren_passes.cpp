#include "task/ren_passes.hpp"


namespace {

    void try_run(enki::ITaskSet* task) {
        if (task) {
            dal::tasker().AddTaskSetToPipe(task);
        }
    }

    void try_wait(enki::ITaskSet* task) {
        if (task) {
            dal::tasker().WaitforTask(task);
        }
    }

}  // namespace


// RenderPassesTask
namespace mirinae {

    void RenderPassesTask::init(
        CmdBufList& cmdbuf_list,
        FlagShip& flag_ship,
        RpCtxt& rp_ctxt,
        RpResources& rp_res,
        std::function<bool()> resize_func,
        std::vector<std::unique_ptr<IRpBase>>& passes,
        VulkanDevice& device
    ) {
        cmdbuf_list_ = &cmdbuf_list;
        device_ = &device;
        flag_ship_ = &flag_ship;
        resize_func_ = resize_func;
        rp_ctxt_ = &rp_ctxt;
        rp_res_ = &rp_res;

        for (auto& rp : passes) {
            if (auto task = rp->create_task()) {
                passes_.push_back(std::move(task));
            }
        }
    }

    void RenderPassesTask::prepare() {}

    void RenderPassesTask::ExecuteRange(
        enki::TaskSetPartition range, uint32_t tid
    ) {
        if (flag_ship_->need_resize()) {
            if (resize_func_) {
                resize_func_();
                flag_ship_->set_need_resize(false);
            }
        }

        if (flag_ship_->dont_render()) {
            return;
        }

        rp_res_->cmd_pool_.reset_pool(rp_ctxt_->f_index_, *device_);
        cmdbuf_list_->clear(rp_ctxt_->f_index_);

        for (auto& rp : passes_) rp->prepare(*rp_ctxt_);
        for (auto& rp : passes_) ::try_run(rp->update_task());
        for (auto& rp : passes_) ::try_wait(rp->update_fence());
        for (auto& rp : passes_) ::try_run(rp->record_task());
        for (auto& rp : passes_) ::try_wait(rp->record_fence());
        for (auto& rp : passes_)
            rp->collect_cmdbuf(cmdbuf_list_->vector(rp_ctxt_->f_index_));
    }

}  // namespace mirinae
