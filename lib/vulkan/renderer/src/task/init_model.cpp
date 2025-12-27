#include "task/init_model.hpp"

#include <mutex>

#include <entt/entity/registry.hpp>

#include "mirinae/cpnt/transform.hpp"
#include "mirinae/lightweight/include_spdlog.hpp"
#include "mirinae/vulkan/base/renderee/ren_actor_skinned.hpp"


namespace {

    std::mutex g_model_mtx;
    std::mutex g_actor_mtx;

}  // namespace


namespace {

    class InitStaticModel : public mirinae::IInitModelTask {

    public:
        void init(
            mirinae::Scene& scene,
            mirinae::VulkanDevice& device,
            mirinae::IModelManager& model_mgr,
            mirinae::RpCtxt& rp_ctxt,
            mirinae::RpResources& rp_res
        ) override {
            reg_ = scene.reg_.get();
            device_ = &device;
            model_mgr_ = &model_mgr;
            rp_ctxt_ = &rp_ctxt;
            rp_res_ = &rp_res;
        }

        void prepare() override {
            this->set_size(reg_->view<mirinae::cpnt::MdlActorStatic>().size());
        }

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            namespace cpnt = mirinae::cpnt;

            auto& reg = *reg_;
            auto view = reg.view<cpnt::MdlActorStatic>();
            auto begin = view.begin() + range.start;
            auto end = view.begin() + range.end;

            for (auto it = begin; it != end; ++it) {
                const auto e = *it;
                auto& mactor = reg.get<cpnt::MdlActorStatic>(e);

                if (!this->load_model(mactor, *model_mgr_))
                    continue;

                if (!this->create_actor(mactor, rp_res_->desclays_, *device_))
                    continue;

                this->update_ubuf(e, mactor, reg, *rp_ctxt_, *device_);
            }

            return;
        }

        static bool load_model(
            mirinae::cpnt::MdlActorStatic& mactor,
            mirinae::IModelManager& model_mgr
        ) {
            if (mactor.model_)
                return true;

            std::lock_guard<std::mutex> lock(g_model_mtx);

            const auto& mdl_path = mactor.model_path_;
            const auto path_str = dal::tostr(mdl_path);

            const auto res = model_mgr.request_static(mdl_path);
            if (dal::ReqResult::loading == res) {
                return false;
            } else if (dal::ReqResult::ready != res) {
                SPDLOG_WARN("Failed to get model: {}", path_str);
                mactor.model_path_ =
                    "Sung/missing_static_mdl.dun/missing_static_mdl.dmd";
                return false;
            }

            mactor.model_ = model_mgr.get_static(mdl_path);
            if (!mactor.model_) {
                SPDLOG_WARN("Failed to get model: {}", path_str);
                return false;
            }

            return true;
        }

        static bool create_actor(
            mirinae::cpnt::MdlActorStatic& mactor,
            mirinae::DesclayoutManager& desclayout,
            mirinae::VulkanDevice& device
        ) {
            if (mactor.actor_)
                return true;

            std::lock_guard<std::mutex> lock(g_actor_mtx);

            auto a = std::make_shared<mirinae::RenderActor>(device);
            a->init(mirinae::MAX_FRAMES_IN_FLIGHT, desclayout);
            mactor.actor_ = a;
            return true;
        }

        static bool update_ubuf(
            const entt::entity e,
            mirinae::cpnt::MdlActorStatic& mactor,
            const entt::registry& reg,
            const mirinae::RpCtxt& rp_ctxt,
            mirinae::VulkanDevice& device
        ) {
            auto actor = mactor.get_actor<mirinae::RenderActor>();

            glm::dmat4 model_mat(1);
            if (auto tform = reg.try_get<mirinae::cpnt::Transform>(e))
                model_mat = tform->make_model_mat();
            const auto vm = rp_ctxt.main_cam_.view() * model_mat;
            const auto pvm = rp_ctxt.main_cam_.proj() * vm;

            mirinae::U_GbufActor udata;
            udata.model = model_mat;
            udata.view_model = vm;
            udata.pvm = pvm;

            actor->udpate_ubuf(rp_ctxt.f_index_.get(), udata);

            return true;
        }

        entt::registry* reg_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
        mirinae::IModelManager* model_mgr_ = nullptr;
        mirinae::RpCtxt* rp_ctxt_ = nullptr;
        mirinae::RpResources* rp_res_ = nullptr;
    };


    class InitSkinnedModel : public mirinae::IInitModelTask {

    public:
        void init(
            mirinae::Scene& scene,
            mirinae::VulkanDevice& device,
            mirinae::IModelManager& model_mgr,
            mirinae::RpCtxt& rp_ctxt,
            mirinae::RpResources& rp_res
        ) override {
            scene_ = &scene;
            device_ = &device;
            model_mgr_ = &model_mgr;
            rp_ctxt_ = &rp_ctxt;
            rp_res_ = &rp_res;
        }

        void prepare() override {
            this->set_size(
                scene_->reg_->view<mirinae::cpnt::MdlActorSkinned>().size()
            );
        }

    private:
        void ExecuteRange(enki::TaskSetPartition range, uint32_t tid) override {
            namespace cpnt = mirinae::cpnt;

            auto& reg = *scene_->reg_;
            auto& desclay = rp_res_->desclays_;
            auto view = reg.view<cpnt::MdlActorSkinned>();
            auto begin = view.begin() + range.start;
            auto end = view.begin() + range.end;

            for (auto it = begin; it != end; ++it) {
                const auto e = *it;
                auto& mactor = reg.get<cpnt::MdlActorSkinned>(e);

                auto ren_model = this->prep_model(mactor, *model_mgr_);
                if (!ren_model)
                    continue;

                auto ren_actor = this->prep_actor(
                    desclay, *ren_model, mactor, *device_
                );
                if (!ren_actor)
                    continue;

                this->update_ubuf(
                    e, *rp_ctxt_, *scene_, mactor.anim_state_, *ren_actor
                );
            }
        }

        static mirinae::RenderModelSkinned* prep_model(
            mirinae::cpnt::MdlActorSkinned& mactor,
            mirinae::IModelManager& model_mgr
        ) {
            if (mactor.model_)
                return mactor.get_model<mirinae::RenderModelSkinned>();

            std::lock_guard<std::mutex> lock(g_model_mtx);

            const auto& mdl_path = mactor.model_path_;
            const auto path_str = dal::tostr(mdl_path);

            const auto res = model_mgr.request_skinned(mdl_path);
            if (dal::ReqResult::loading == res) {
                return nullptr;
            } else if (dal::ReqResult::ready != res) {
                SPDLOG_WARN("Failed to get model: {}", path_str);
                mactor.model_path_ =
                    "Sung/missing_static_mdl.dun/missing_static_mdl.dmd";
                return nullptr;
            }

            auto model = model_mgr.get_skinned(mdl_path);
            if (!model) {
                SPDLOG_WARN("Failed to get model: {}", path_str);
                return nullptr;
            }

            mactor.model_ = model;
            mactor.anim_state_.set_skel_anim(model->skel_anim_);

            return model.get();
        }

        static mirinae::RenderActorSkinned* prep_actor(
            const mirinae::DesclayoutManager& desclayout,
            const mirinae::RenderModelSkinned& ren_model,
            mirinae::cpnt::MdlActorSkinned& mactor,
            mirinae::VulkanDevice& device
        ) {
            if (mactor.actor_)
                return mactor.get_actor<mirinae::RenderActorSkinned>();

            std::vector<mirinae::RenderActorSkinned::RenUnitInfo> runit_info;
            runit_info.reserve(
                ren_model.runits_.size() + ren_model.runits_alpha_.size()
            );
            for (auto& src_unit : ren_model.runits_) {
                auto& dst_unit = runit_info.emplace_back();
                dst_unit.src_vtx_buf_ = &src_unit.vk_buffers().vtx();
                dst_unit.transparent_ = false;
            }
            for (auto& src_unit : ren_model.runits_alpha_) {
                auto& dst_unit = runit_info.emplace_back();
                dst_unit.src_vtx_buf_ = &src_unit.vk_buffers().vtx();
                dst_unit.transparent_ = true;
            }

            std::lock_guard<std::mutex> lock(g_actor_mtx);

            auto a = std::make_shared<mirinae::RenderActorSkinned>(device);
            a->init(
                mirinae::MAX_FRAMES_IN_FLIGHT,
                ren_model.skel_anim_->joint_count(),
                runit_info,
                desclayout
            );
            mactor.actor_ = a;
            return a.get();
        }

        static bool update_ubuf(
            const entt::entity e,
            const mirinae::RpCtxt& rp_ctxt,
            const mirinae::Scene& scene,
            const mirinae::SkinAnimState& anim_state,
            mirinae::RenderActorSkinned& ren_actor
        ) {
            auto& reg = *scene.reg_;

            glm::dmat4 model_mat(1);
            if (auto tform = reg.try_get<mirinae::cpnt::Transform>(e))
                model_mat = tform->make_model_mat();
            const auto vm = rp_ctxt.main_cam_.view() * model_mat;
            const auto pvm = rp_ctxt.main_cam_.proj() * vm;

            mirinae::U_GbufActor udata_static;
            udata_static.model = model_mat;
            udata_static.view_model = vm;
            udata_static.pvm = pvm;

            std::array<glm::mat4, 256> joint_palette;
            anim_state.sample_anim(
                joint_palette.data(), joint_palette.size(), scene.clock()
            );

            ren_actor.update_ubuf(rp_ctxt.f_index_, udata_static);
            ren_actor.update_joint_palette(
                rp_ctxt.f_index_, joint_palette.data(), joint_palette.size()
            );
            return true;
        }

        mirinae::Scene* scene_ = nullptr;
        mirinae::VulkanDevice* device_ = nullptr;
        mirinae::IModelManager* model_mgr_ = nullptr;
        mirinae::RpCtxt* rp_ctxt_ = nullptr;
        mirinae::RpResources* rp_res_ = nullptr;
    };

}  // namespace


namespace mirinae {

    std::unique_ptr<IInitModelTask> create_init_static_model_task() {
        return std::make_unique<InitStaticModel>();
    }

    std::unique_ptr<IInitModelTask> create_init_skinned_model_task() {
        return std::make_unique<InitSkinnedModel>();
    }

}  // namespace mirinae
