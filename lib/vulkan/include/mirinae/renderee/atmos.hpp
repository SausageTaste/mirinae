#pragma once

#include <entt/fwd.hpp>

#include "mirinae/context/base.hpp"
#include "mirinae/cpnt/atmos.hpp"
#include "mirinae/lightweight/task.hpp"
#include "mirinae/render/vkdevice.hpp"


namespace mirinae {

    class RenUnitAtmosEpic : public IAtmosEpicRenUnit {

    public:
        RenUnitAtmosEpic(uint32_t max_flight_count, VulkanDevice& device);
        ~RenUnitAtmosEpic() override;

        const BufferSpan& ubuf_at(const FrameIndex f_idx) const;
        void update_ubuf(
            const FrameIndex f_index, const AtmosphereParameters& params
        );

    private:
        VulkanDevice& device_;
        std::vector<Buffer> ubuf_;
        std::vector<BufferSpan> ubuf_span_;
    };


    class TaskAtmosEpic : public mirinae::DependingTask {

    public:
        void init(
            uint32_t max_flight_count,
            entt::registry& reg,
            RpCtxtBase& rp_ctxt,
            VulkanDevice& device
        );

        void prepare();

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override;

        entt::registry* reg_ = nullptr;
        VulkanDevice* device_ = nullptr;
        RpCtxtBase* rp_ctxt_ = nullptr;
        uint32_t max_flight_count_ = 0;
    };

}  // namespace mirinae
