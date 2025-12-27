#include "task/update_ren_ctxt.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"


namespace {

    std::optional<mirinae::ShainImageIndex> try_acquire_image(
        mirinae::FrameSync& framesync,
        mirinae::Swapchain& swapchain,
        mirinae::VulkanDevice& device
    ) {
        framesync.get_cur_in_flight_fence().wait(device.logi_device());

        const auto i_idx = swapchain.acquire_next_image(
            framesync.get_cur_img_ava_semaph().get(), device.logi_device()
        );
        if (!i_idx)
            return std::nullopt;

        framesync.get_cur_in_flight_fence().reset(device.logi_device());
        return i_idx.value();
    }

    void update(
        const mirinae::Scene& scene,
        mirinae::FlagShip& flag_ship,
        mirinae::FrameSync& framesync,
        mirinae::RpContext& ren_ctxt,
        mirinae::Swapchain& swapchain,
        mirinae::VulkanDevice& device
    ) {
        namespace cpnt = mirinae::cpnt;

        if (flag_ship.need_resize()) {
            flag_ship.set_dont_render(true);
            return;
        }
        if (mirinae::is_fbuf_too_small(swapchain.width(), swapchain.height())) {
            flag_ship.set_need_resize(true);
            flag_ship.set_dont_render(true);
            return;
        }
        const auto i_idx = ::try_acquire_image(framesync, swapchain, device);
        if (!i_idx) {
            flag_ship.set_need_resize(true);
            flag_ship.set_dont_render(true);
            return;
        }

        flag_ship.set_need_resize(false);
        flag_ship.set_dont_render(false);

        const auto e_cam = scene.main_camera_;
        const auto cam = scene.reg_->try_get<cpnt::StandardCamera>(e_cam);
        if (!cam) {
            SPDLOG_WARN("No camera found in scene.");
            flag_ship.set_dont_render(true);
            return;
        }

        ren_ctxt.main_cam_.update(
            *cam,
            scene.reg_->try_get<cpnt::Transform>(e_cam),
            swapchain.width(),
            swapchain.height()
        );

        ren_ctxt.f_index_ = framesync.get_frame_index();
        ren_ctxt.i_index_ = i_idx.value();
        ren_ctxt.dt_ = scene.clock().dt();
    }

}  // namespace


// UpdateRenContext
namespace mirinae {

    void UpdateRenContext::init(
        const Scene& scene,
        FlagShip& flag_ship,
        FrameSync& framesync,
        RpContext& rp_ctxt,
        Swapchain& swapchain,
        VulkanDevice& device
    ) {
        device_ = &device;
        flag_ship_ = &flag_ship;
        framesync_ = &framesync;
        ren_ctxt_ = &rp_ctxt;
        scene_ = &scene;
        swapchain_ = &swapchain;
    }

    void UpdateRenContext::prepare() {}

    void UpdateRenContext::ExecuteRange(
        enki::TaskSetPartition range, uint32_t tid
    ) {
        ::update(
            *scene_, *flag_ship_, *framesync_, *ren_ctxt_, *swapchain_, *device_
        );
    }

}  // namespace mirinae
