#include "mirinae/renderee/atmos.hpp"

#include <entt/entity/registry.hpp>

#include "mirinae/render/mem_cinfo.hpp"


// RenUnitAtmosEpic
namespace mirinae {

    RenUnitAtmosEpic::RenUnitAtmosEpic(
        uint32_t max_flight_count, VulkanDevice& device
    )
        : device_(device) {
        mirinae::BufferCreateInfo cinfo;
        cinfo.preset_ubuf<AtmosphereParameters>().prefer_host();

        for (uint32_t i = 0; i < max_flight_count; ++i) {
            auto& buffer = ubuf_.emplace_back();
            buffer.init(cinfo, device_.mem_alloc());

            auto& span = ubuf_span_.emplace_back();
            span.buf_ = buffer.get();
            span.offset_ = 0;
            span.size_ = buffer.size();
        }
    }

    RenUnitAtmosEpic::~RenUnitAtmosEpic() {}

    void RenUnitAtmosEpic::update_ubuf(
        const FrameIndex f_index, const AtmosphereParameters& params
    ) {
        auto& buf = ubuf_.at(f_index.get());
        buf.set_data(params);
    }

}  // namespace mirinae


// TaskAtmosEpic
namespace mirinae {

    void TaskAtmosEpic::init(
        uint32_t max_flight_count,
        entt::registry& reg,
        RpCtxtBase& rp_ctxt,
        VulkanDevice& device
    ) {
        reg_ = &reg;
        rp_ctxt_ = &rp_ctxt;
        device_ = &device;
        max_flight_count_ = max_flight_count;
    }

    void TaskAtmosEpic::prepare() {
        this->set_size(reg_->view<cpnt::AtmosphereEpic>().size());
    }

    void TaskAtmosEpic::ExecuteRange(
        enki::TaskSetPartition range, uint32_t tid
    ) {
        namespace cpnt = mirinae::cpnt;

        auto& reg = *reg_;
        auto view = reg.view<cpnt::AtmosphereEpic>();
        auto begin = view.begin() + range.start;
        auto end = view.begin() + range.end;

        for (auto it = begin; it != end; ++it) {
            const auto e = *it;
            auto& atmos = reg.get<cpnt::AtmosphereEpic>(e);

            auto ren_unit = atmos.ren_unit<RenUnitAtmosEpic>();
            if (!ren_unit) {
                atmos.ren_unit_ = std::make_unique<RenUnitAtmosEpic>(
                    max_flight_count_, *device_
                );
                ren_unit = atmos.ren_unit<RenUnitAtmosEpic>();
            }

            ren_unit->update_ubuf(rp_ctxt_->f_index_, atmos.atmos_params_);
        }
    }

}  // namespace mirinae
