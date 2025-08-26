#include "mirinae/vulkan_pch.h"

#include "mirinae/render/renderee.hpp"

#include <numeric>
#include <set>

#include <daltools/dmd/parser.h>
#include <sung/basic/stringtool.hpp>

#include "mirinae/render/cmdbuf.hpp"


namespace {

    void calc_tangents(
        mirinae::VertexStatic& p0,
        mirinae::VertexStatic& p1,
        mirinae::VertexStatic& p2
    ) {
        glm::vec3 edge1 = p1.pos_ - p0.pos_;
        glm::vec3 edge2 = p2.pos_ - p0.pos_;
        glm::vec2 delta_uv1 = p1.texcoord_ - p0.texcoord_;
        glm::vec2 delta_uv2 = p2.texcoord_ - p0.texcoord_;
        const auto deno = delta_uv1.x * delta_uv2.y - delta_uv2.x * delta_uv1.y;
        if (0 == deno)
            return;

        const auto f = 1 / deno;
        glm::vec3 tangent;
        tangent.x = f * (delta_uv2.y * edge1.x - delta_uv1.y * edge2.x);
        tangent.y = f * (delta_uv2.y * edge1.y - delta_uv1.y * edge2.y);
        tangent.z = f * (delta_uv2.y * edge1.z - delta_uv1.y * edge2.z);
        tangent = glm::normalize(tangent);

        p0.tangent_ = tangent;
        p1.tangent_ = tangent;
        p2.tangent_ = tangent;
    }

    void calc_tangents(
        mirinae::VertexSkinned& p0,
        mirinae::VertexSkinned& p1,
        mirinae::VertexSkinned& p2
    ) {
        glm::vec3 edge1 = p1.pos_ - p0.pos_;
        glm::vec3 edge2 = p2.pos_ - p0.pos_;
        glm::vec2 delta_uv1 = p1.uv_ - p0.uv_;
        glm::vec2 delta_uv2 = p2.uv_ - p0.uv_;
        const auto deno = delta_uv1.x * delta_uv2.y - delta_uv2.x * delta_uv1.y;
        if (0 == deno)
            return;

        const auto f = 1 / deno;
        glm::vec3 tangent;
        tangent.x = f * (delta_uv2.y * edge1.x - delta_uv1.y * edge2.x);
        tangent.y = f * (delta_uv2.y * edge1.y - delta_uv1.y * edge2.y);
        tangent.z = f * (delta_uv2.y * edge1.z - delta_uv1.y * edge2.z);
        tangent = glm::normalize(tangent);

        p0.tangent_ = tangent;
        p1.tangent_ = tangent;
        p2.tangent_ = tangent;
    }


    class MaterialResources {

    public:
        static void forward_request(
            const dal::path& res_id,
            const dal::parser::Material& src_material,
            mirinae::ITextureManager& tex_man
        ) {
            request_texture(res_id, src_material.albedo_map_, true, tex_man);

            request_texture(res_id, src_material.normal_map_, false, tex_man);

            request_texture(
                res_id, src_material.roughness_map_, false, tex_man
            );
        }

        void fetch(
            const dal::path& res_id,
            const dal::parser::Material& src_material,
            mirinae::ITextureManager& tex_man
        ) {
            albedo_map_ = this->block_for_tex(
                res_id,
                src_material.albedo_map_,
                ":asset/textures/missing_texture.ktx",
                true,
                tex_man
            );

            normal_map_ = this->block_for_tex(
                res_id,
                src_material.normal_map_,
                ":asset/textures/null_normal_map.ktx",
                false,
                tex_man
            );

            orm_map_ = this->block_for_tex(
                res_id,
                src_material.roughness_map_,
                ":asset/textures/white.ktx",
                false,
                tex_man
            );

            model_ubuf_.roughness = src_material.roughness_;
            model_ubuf_.metallic = src_material.metallic_;
        }

        mirinae::U_GbufModel model_ubuf_;
        std::shared_ptr<mirinae::ITexture> albedo_map_;
        std::shared_ptr<mirinae::ITexture> normal_map_;
        std::shared_ptr<mirinae::ITexture> orm_map_;

    private:
        static void request_texture(
            dal::path res_id,
            const std::string& file_name,
            const bool srgb,
            mirinae::ITextureManager& tex_man
        ) {
            if (file_name.empty())
                return;

            res_id.replace_filename(file_name);
            tex_man.request(res_id, srgb);
        }

        static std::shared_ptr<mirinae::ITexture> block_for_tex(
            dal::path res_id,
            const std::string& file_name,
            const std::string& fallback_path,
            const bool srgb,
            mirinae::ITextureManager& tex_man
        ) {
            if (file_name.empty())
                return tex_man.block_for_tex(fallback_path, srgb);

            res_id.replace_filename(file_name);
            auto output = tex_man.block_for_tex(res_id, srgb);
            if (!output)
                return tex_man.block_for_tex(fallback_path, srgb);

            return output;
        }
    };


}  // namespace


// RenderUnit
namespace mirinae {

    void RenderUnit::init(
        const std::string& name,
        uint32_t max_flight_count,
        const VerticesStaticPair& vertices,
        const U_GbufModel& ubuf_data,
        VkImageView albedo_map,
        VkImageView normal_map,
        VkImageView orm_map,
        CommandPool& cmd_pool,
        DesclayoutManager& desclayouts,
        VulkanDevice& device
    ) {
        name_ = name;
        raw_data_ = vertices;

        auto& desclayout = desclayouts.get("gbuf:model");
        desc_pool_.init(
            max_flight_count, desclayout.size_info(), device.logi_device()
        );
        desc_sets_ = desc_pool_.alloc(
            max_flight_count, desclayout.layout(), device.logi_device()
        );

        uniform_buf_.init_ubuf(sizeof(U_GbufModel), device.mem_alloc());
        uniform_buf_.set_data(
            &ubuf_data, sizeof(U_GbufModel), device.mem_alloc()
        );

        DescWriteInfoBuilder builder;
        for (size_t i = 0; i < max_flight_count; i++) {
            builder.set_descset(desc_sets_.at(i))
                .add_ubuf(uniform_buf_)
                .add_img_sampler(albedo_map, device.samplers().get_linear())
                .add_img_sampler(normal_map, device.samplers().get_linear())
                .add_img_sampler(orm_map, device.samplers().get_linear());
        }
        builder.apply_all(device.logi_device());

        vert_index_pair_.init(
            raw_data_,
            cmd_pool,
            device.mem_alloc(),
            device.graphics_queue(),
            device.logi_device()
        );
    }

    void RenderUnit::destroy(
        VulkanMemoryAllocator mem_alloc, VkDevice logi_device
    ) {
        vert_index_pair_.destroy(mem_alloc);
        uniform_buf_.destroy(mem_alloc);
        desc_pool_.destroy(logi_device);
    }

    VkDescriptorSet RenderUnit::get_desc_set(size_t index) const {
        return desc_sets_.at(index);
    }

    void RenderUnit::record_bind_vert_buf(VkCommandBuffer cmdbuf) const {
        vert_index_pair_.record_bind(cmdbuf);
    }

    uint32_t RenderUnit::vertex_count() const {
        return vert_index_pair_.vertex_count();
    }

}  // namespace mirinae


// RenderUnitSkinned
namespace mirinae {

    void RenderUnitSkinned::init(
        const std::string& name,
        uint32_t max_flight_count,
        const VerticesSkinnedPair& vertices,
        const U_GbufModel& ubuf_data,
        VkImageView albedo_map,
        VkImageView normal_map,
        VkImageView orm_map,
        CommandPool& cmd_pool,
        DesclayoutManager& desclayouts,
        VulkanDevice& device
    ) {
        name_ = name;

        auto& desclayout = desclayouts.get("gbuf:model");
        desc_pool_.init(
            max_flight_count, desclayout.size_info(), device.logi_device()
        );
        desc_sets_ = desc_pool_.alloc(
            max_flight_count, desclayout.layout(), device.logi_device()
        );

        uniform_buf_.init_ubuf(sizeof(U_GbufModel), device.mem_alloc());
        uniform_buf_.set_data(
            &ubuf_data, sizeof(U_GbufModel), device.mem_alloc()
        );

        DescWriteInfoBuilder builder;
        for (size_t i = 0; i < max_flight_count; i++) {
            builder.set_descset(desc_sets_.at(i))
                .add_ubuf(uniform_buf_)
                .add_img_sampler(albedo_map, device.samplers().get_linear())
                .add_img_sampler(normal_map, device.samplers().get_linear())
                .add_img_sampler(orm_map, device.samplers().get_linear());
        }
        builder.apply_all(device.logi_device());

        vert_index_pair_.init(
            vertices,
            cmd_pool,
            device.mem_alloc(),
            device.graphics_queue(),
            device.logi_device()
        );
    }

    void RenderUnitSkinned::destroy(
        VulkanMemoryAllocator mem_alloc, VkDevice logi_device
    ) {
        vert_index_pair_.destroy(mem_alloc);
        uniform_buf_.destroy(mem_alloc);
        desc_pool_.destroy(logi_device);
    }

    VkDescriptorSet RenderUnitSkinned::get_desc_set(size_t index) const {
        return desc_sets_.at(index);
    }

    void RenderUnitSkinned::record_bind_vert_buf(VkCommandBuffer cmdbuf) const {
        vert_index_pair_.record_bind(cmdbuf);
    }

    uint32_t RenderUnitSkinned::vertex_count() const {
        return vert_index_pair_.vertex_count();
    }

}  // namespace mirinae


// OverlayRenderUnit
namespace mirinae {

    OverlayRenderUnit::OverlayRenderUnit(VulkanDevice& device)
        : device_(device) {}

    OverlayRenderUnit::~OverlayRenderUnit() { this->destroy(); }

    OverlayRenderUnit::OverlayRenderUnit(OverlayRenderUnit&& rhs) noexcept
        : device_(rhs.device_) {
        std::swap(desc_pool_, rhs.desc_pool_);
        std::swap(uniform_buf_, rhs.uniform_buf_);
        std::swap(desc_sets_, rhs.desc_sets_);
    }

    OverlayRenderUnit& OverlayRenderUnit::operator=(
        OverlayRenderUnit&& rhs
    ) noexcept {
        std::swap(desc_pool_, rhs.desc_pool_);
        std::swap(uniform_buf_, rhs.uniform_buf_);
        std::swap(desc_sets_, rhs.desc_sets_);
        return *this;
    }

    void OverlayRenderUnit::init(
        uint32_t max_flight_count,
        VkImageView color_view,
        VkImageView mask_view,
        VkSampler sampler,
        DesclayoutManager& desclayouts,
        ITextureManager& tex_man
    ) {
        auto& desclayout = desclayouts.get("overlay:main");
        desc_pool_.init(
            max_flight_count, desclayout.size_info(), device_.logi_device()
        );
        desc_sets_ = desc_pool_.alloc(
            max_flight_count, desclayout.layout(), device_.logi_device()
        );

        for (uint32_t i = 0; i < max_flight_count; ++i) {
            auto& ubuf = uniform_buf_.emplace_back();
            ubuf.init_ubuf(sizeof(U_OverlayMain), device_.mem_alloc());
        }

        DescWriteInfoBuilder builder;
        for (size_t i = 0; i < max_flight_count; i++) {
            builder.set_descset(desc_sets_.at(i))
                .add_ubuf(uniform_buf_.at(i))
                .add_img_sampler_general(color_view, sampler)
                .add_img_sampler(mask_view, sampler);
        }
        builder.apply_all(device_.logi_device());
    }

    void OverlayRenderUnit::destroy() {
        for (auto& ubuf : uniform_buf_) ubuf.destroy(device_.mem_alloc());
        uniform_buf_.clear();

        desc_pool_.destroy(device_.logi_device());
    }

    void OverlayRenderUnit::udpate_ubuf(uint32_t index) {
        auto& ubuf = uniform_buf_.at(index);
        ubuf.set_data(&ubuf_data_, sizeof(U_OverlayMain), device_.mem_alloc());
    }

    VkDescriptorSet OverlayRenderUnit::get_desc_set(size_t index) {
        return desc_sets_.at(index);
    }

}  // namespace mirinae


// RenderModel
namespace mirinae {

    RenderModel::RenderModel(VulkanDevice& vulkan_device)
        : device_(vulkan_device) {}

    RenderModel::~RenderModel() {
        for (auto& unit : render_units_)
            unit.destroy(device_.mem_alloc(), device_.logi_device());
        render_units_.clear();

        for (auto& unit : render_units_alpha_)
            unit.destroy(device_.mem_alloc(), device_.logi_device());
        render_units_alpha_.clear();
    }

    bool RenderModel::is_ready() const {
        if (render_units_.empty() && render_units_alpha_.empty())
            return false;

        return true;
    }

    void RenderModel::access_positions(IModelAccessor& acc) const {
        for (auto& unit : render_units_) {
            for (auto idx : unit.raw_data().indices_) {
                auto& v = unit.raw_data().vertices_.at(idx);
                if (!acc.position(v.pos_))
                    return;
            }
        }

        for (auto& unit : render_units_alpha_) {
            for (auto idx : unit.raw_data().indices_) {
                auto& v = unit.raw_data().vertices_.at(idx);
                if (!acc.position(v.pos_))
                    return;
            }
        }
    }

    size_t RenderModel::ren_unit_count() const {
        return render_units_.size() + render_units_alpha_.size();
    }

    std::string_view RenderModel::ren_unit_name(size_t index) const {
        if (index < render_units_.size())
            return render_units_.at(index).name();
        else if (index < render_units_.size() + render_units_alpha_.size())
            return render_units_alpha_.at(index - render_units_.size()).name();
        else
            return "";
    }

}  // namespace mirinae


// RenderModelSkinned
namespace mirinae {

    RenderModelSkinned::RenderModelSkinned(VulkanDevice& vulkan_device)
        : skel_anim_(std::make_shared<SkelAnimPair>())
        , device_(vulkan_device) {}

    RenderModelSkinned::~RenderModelSkinned() {
        for (auto& unit : runits_)
            unit.destroy(device_.mem_alloc(), device_.logi_device());
        runits_.clear();

        for (auto& unit : runits_alpha_)
            unit.destroy(device_.mem_alloc(), device_.logi_device());
        runits_alpha_.clear();
    }

    bool RenderModelSkinned::is_ready() const { return false; }

    void RenderModelSkinned::access_positions(IModelAccessor& acc) const {}

    size_t RenderModelSkinned::ren_unit_count() const {
        return runits_.size() + runits_alpha_.size();
    }

    std::string_view RenderModelSkinned::ren_unit_name(size_t index) const {
        if (index < runits_.size())
            return runits_.at(index).name();
        else if (index < runits_.size() + runits_alpha_.size())
            return runits_alpha_.at(index - runits_.size()).name();
        else
            return "";
    }

}  // namespace mirinae


// ModelManager
namespace {

    class ModelLoadTask : public sung::IStandardLoadTask {

    public:
        ModelLoadTask(const dal::path& path, dal::Filesystem& filesys)
            : filesys_(filesys), path_(path) {}

        sung::TaskStatus tick() override {
            if (path_.empty())
                return this->fail("Path is empty");

            filesys_.read_file(path_, raw_data_);
            if (raw_data_.empty())
                return this->fail("Failed to read file");

            const auto result = dal::parser::parse_dmd(
                dmd_,
                reinterpret_cast<const uint8_t*>(raw_data_.data()),
                raw_data_.size()
            );
            switch (result) {
                case dal::parser::ModelParseResult::success:
                    break;
                case dal::parser::ModelParseResult::magic_numbers_dont_match:
                    return this->fail("Cannot read file");
                case dal::parser::ModelParseResult::decompression_failed:
                    return this->fail("Not supported file");
                case dal::parser::ModelParseResult::corrupted_content:
                    return this->fail("Corrupted content");
                default:
                    return this->fail("Unknown error");
            }

            for (const auto& unit : dmd_.units_straight_) {
                tex_ids_srgb.insert(unit.material_.albedo_map_);
                tex_ids.insert(unit.material_.normal_map_);
                tex_ids.insert(unit.material_.roughness_map_);
            }
            for (const auto& unit : dmd_.units_indexed_) {
                tex_ids_srgb.insert(unit.material_.albedo_map_);
                tex_ids.insert(unit.material_.normal_map_);
                tex_ids.insert(unit.material_.roughness_map_);
            }
            for (const auto& unit : dmd_.units_straight_joint_) {
                tex_ids_srgb.insert(unit.material_.albedo_map_);
                tex_ids.insert(unit.material_.normal_map_);
                tex_ids.insert(unit.material_.roughness_map_);
            }
            for (const auto& unit : dmd_.units_indexed_joint_) {
                tex_ids_srgb.insert(unit.material_.albedo_map_);
                tex_ids.insert(unit.material_.normal_map_);
                tex_ids.insert(unit.material_.roughness_map_);
            }
            tex_ids.erase("");
            tex_ids_srgb.erase("");

            for (auto& src_unit : dmd_.units_indexed_) {
                auto& dst_vertices = units_indexed_.emplace_back();

                dst_vertices.indices_.assign(
                    src_unit.mesh_.indices_.begin(),
                    src_unit.mesh_.indices_.end()
                );

                for (auto& src_vertex : src_unit.mesh_.vertices_) {
                    auto& dst_vertex = dst_vertices.vertices_.emplace_back();
                    dst_vertex.pos_ = src_vertex.pos_;
                    dst_vertex.normal_ = src_vertex.normal_;
                    dst_vertex.texcoord_ = src_vertex.uv_;
                }

                {
                    const auto tri_count = dst_vertices.indices_.size() / 3;
                    for (size_t i = 0; i < tri_count; ++i) {
                        const auto i0 = dst_vertices.indices_.at(i * 3 + 0);
                        const auto i1 = dst_vertices.indices_.at(i * 3 + 1);
                        const auto i2 = dst_vertices.indices_.at(i * 3 + 2);

                        auto& v0 = dst_vertices.vertices_.at(i0);
                        auto& v1 = dst_vertices.vertices_.at(i1);
                        auto& v2 = dst_vertices.vertices_.at(i2);

                        ::calc_tangents(v0, v1, v2);
                    }
                }
            }

            for (const auto& src_unit : dmd_.units_indexed_joint_) {
                auto& dst_vertices = units_indexed_joint_.emplace_back();

                dst_vertices.indices_.assign(
                    src_unit.mesh_.indices_.begin(),
                    src_unit.mesh_.indices_.end()
                );

                for (auto& src_vertex : src_unit.mesh_.vertices_) {
                    auto& dst_vertex = dst_vertices.vertices_.emplace_back();
                    dst_vertex.pos_ = src_vertex.pos_;
                    dst_vertex.normal_ = src_vertex.normal_;
                    dst_vertex.uv_ = src_vertex.uv_;
                    dst_vertex.joint_indices_ = src_vertex.joint_indices_;
                    dst_vertex.joint_weights_ = src_vertex.joint_weights_;

                    this->refine_joint_data(
                        dst_vertex.joint_indices_, dst_vertex.joint_weights_
                    );
                }

                {
                    const auto tri_count = dst_vertices.indices_.size() / 3;
                    for (size_t i = 0; i < tri_count; ++i) {
                        const auto i0 = dst_vertices.indices_.at(i * 3 + 0);
                        const auto i1 = dst_vertices.indices_.at(i * 3 + 1);
                        const auto i2 = dst_vertices.indices_.at(i * 3 + 2);

                        auto& v0 = dst_vertices.vertices_.at(i0);
                        auto& v1 = dst_vertices.vertices_.at(i1);
                        auto& v2 = dst_vertices.vertices_.at(i2);

                        ::calc_tangents(v0, v1, v2);
                    }
                }
            }

            return this->success();
        }

        const dal::parser::Model* try_get_dmd() const {
            if (!this->has_succeeded())
                return nullptr;
            return &dmd_;
        }

        const std::set<std::string>& get_tex_ids() const { return tex_ids; }
        const std::set<std::string>& get_tex_ids_srgb() const {
            return tex_ids_srgb;
        }

        const std::vector<mirinae::VerticesStaticPair>& units_indexed() const {
            return units_indexed_;
        }

        const std::vector<mirinae::VerticesSkinnedPair>&
        units_indexed_joint() const {
            return units_indexed_joint_;
        }

    private:
        static void refine_joint_data(glm::ivec4& indices, glm::vec4& weights) {
            for (int i = 0; i < 4; ++i) {
                if (indices[i] < 0) {
                    indices[i] = 0;
                    weights[i] = 0;
                }
            }

            const auto weight_sum = std::reduce(&weights[0], &weights[0] + 4);
            if (0 == weight_sum) {
                indices = glm::ivec4(0);
                weights = glm::vec4(0);
                return;
            }

            for (int i = 0; i < 4; ++i) {
                weights[i] /= weight_sum;
            }
        }

        dal::Filesystem& filesys_;
        dal::path path_;
        std::vector<std::byte> raw_data_;
        dal::parser::Model dmd_;
        std::set<std::string> tex_ids;
        std::set<std::string> tex_ids_srgb;
        std::vector<mirinae::VerticesStaticPair> units_indexed_;
        std::vector<mirinae::VerticesSkinnedPair> units_indexed_joint_;
    };


    class LoadTaskManager {

    public:
        LoadTaskManager(
            sung::HTaskSche task_sche, mirinae::VulkanDevice& device
        )
            : task_sche_(task_sche), filesys_(&device.filesys()) {}

        bool add_task(const dal::path& path) {
            if (this->has_task(path))
                return false;

            auto task = std::make_shared<ModelLoadTask>(path, *filesys_);
            task_sche_->add_task(task);
            tasks_.emplace(path.u8string(), task);
            return true;
        }

        bool has_task(const dal::path& path) {
            return tasks_.find(path.u8string()) != tasks_.end();
        }

        void remove_task(const dal::path& path) {
            tasks_.erase(path.u8string());
        }

        std::shared_ptr<ModelLoadTask> try_get_task(const dal::path& path) {
            const auto it = tasks_.find(path.u8string());
            if (it == tasks_.end())
                return nullptr;
            return it->second;
        }

    private:
        std::unordered_map<std::string, std::shared_ptr<ModelLoadTask>> tasks_;
        sung::HTaskSche task_sche_;
        dal::Filesystem* filesys_;
    };


    class ModelManager : public mirinae::IModelManager {

    public:
        using HRenMdlStatic = mirinae::HRenMdlStatic;
        using HRenMdlSkinned = mirinae::HRenMdlSkinned;

        ModelManager(
            sung::HTaskSche task_sche,
            mirinae::HTexMgr tex_man,
            mirinae::DesclayoutManager& desclayouts,
            mirinae::VulkanDevice& device
        )
            : device_(device)
            , desclayouts_(desclayouts)
            , load_tasks_(task_sche, device)
            , task_sche_(task_sche)
            , tex_man_(tex_man) {
            cmd_pool_.init(
                device_.graphics_queue_family_index().value(),
                device_.logi_device()
            );
        }

        ~ModelManager() override { cmd_pool_.destroy(device_.logi_device()); }

        dal::ReqResult request_static(const dal::path& res_id) override {
            auto found = models_.find(res_id);
            if (models_.end() != found)
                return dal::ReqResult::ready;

            auto task = load_tasks_.try_get_task(res_id);
            if (!task) {
                if (!load_tasks_.add_task(res_id))
                    return dal::ReqResult::unknown_error;
                return dal::ReqResult::loading;
            }
            if (!task->is_done())
                return dal::ReqResult::loading;
            if (task->has_failed()) {
                SPDLOG_ERROR(
                    "Failed to load model '{}': {}",
                    res_id.u8string(),
                    task->err_msg()
                );
                return dal::ReqResult::cannot_read_file;
            }

            const auto dmd = task->try_get_dmd();
            if (!dmd)
                return dal::ReqResult::cannot_read_file;

            bool loading = false;
            for (const auto& tex_id : task->get_tex_ids()) {
                const auto tex_path = res_id.parent_path() / tex_id;
                const auto res_result = tex_man_->request(tex_path, false);
                loading |= (dal::ReqResult::loading == res_result);
            }
            for (const auto& tex_id : task->get_tex_ids_srgb()) {
                const auto tex_path = res_id.parent_path() / tex_id;
                const auto res_result = tex_man_->request(tex_path, true);
                loading |= (dal::ReqResult::loading == res_result);
            }
            if (loading)
                return dal::ReqResult::loading;

            auto output = std::make_shared<mirinae::RenderModel>(device_);

            for (size_t i = 0; i < dmd->units_indexed_.size(); ++i) {
                const auto& src_unit = dmd->units_indexed_.at(i);
                const auto& dst_vertices = task->units_indexed().at(i);

                ::MaterialResources mat_res;
                mat_res.fetch(res_id, src_unit.material_, *tex_man_);

                auto& dst_unit =
                    ((src_unit.material_.transparency_)
                         ? output->render_units_alpha_.emplace_back()
                         : output->render_units_.emplace_back());
                dst_unit.init(
                    src_unit.name_,
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    dst_vertices,
                    mat_res.model_ubuf_,
                    mat_res.albedo_map_->image_view(),
                    mat_res.normal_map_->image_view(),
                    mat_res.orm_map_->image_view(),
                    cmd_pool_,
                    desclayouts_,
                    device_
                );
            }

            models_[res_id] = output;
            return dal::ReqResult::ready;
        }

        dal::ReqResult request_skinned(const dal::path& res_id) override {
            auto found = skin_models_.find(res_id);
            if (skin_models_.end() != found)
                return dal::ReqResult::ready;

            auto task = load_tasks_.try_get_task(res_id);
            if (!task) {
                if (!load_tasks_.add_task(res_id))
                    return dal::ReqResult::unknown_error;
                return dal::ReqResult::loading;
            }
            if (!task->is_done())
                return dal::ReqResult::loading;

            const auto dmd = task->try_get_dmd();
            if (!dmd)
                return dal::ReqResult::cannot_read_file;

            bool loading = false;
            for (const auto& tex_id : task->get_tex_ids()) {
                const auto tex_path = res_id.parent_path() / tex_id;
                const auto res_result = tex_man_->request(tex_path, false);
                loading |= (dal::ReqResult::loading == res_result);
            }
            for (const auto& tex_id : task->get_tex_ids_srgb()) {
                const auto tex_path = res_id.parent_path() / tex_id;
                const auto res_result = tex_man_->request(tex_path, true);
                loading |= (dal::ReqResult::loading == res_result);
            }
            if (loading)
                return dal::ReqResult::loading;

            auto output = std::make_shared<mirinae::RenderModelSkinned>(
                device_
            );

            for (size_t i = 0; i < dmd->units_indexed_joint_.size(); ++i) {
                const auto& src_unit = dmd->units_indexed_joint_.at(i);
                const auto& dst_vertices = task->units_indexed_joint().at(i);

                ::MaterialResources mat_res;
                mat_res.fetch(res_id, src_unit.material_, *tex_man_);

                auto& dst_unit = src_unit.material_.transparency_
                                     ? output->runits_alpha_.emplace_back()
                                     : output->runits_.emplace_back();
                dst_unit.init(
                    src_unit.name_,
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    dst_vertices,
                    mat_res.model_ubuf_,
                    mat_res.albedo_map_->image_view(),
                    mat_res.normal_map_->image_view(),
                    mat_res.orm_map_->image_view(),
                    cmd_pool_,
                    desclayouts_,
                    device_
                );
            }

            output->skel_anim_->skel_ = dmd->skeleton_;
            output->skel_anim_->anims_ = dmd->animations_;

            skin_models_[res_id] = output;
            return dal::ReqResult::ready;
        }

        HRenMdlStatic get_static(const dal::path& res_id) override {
            auto found = models_.find(res_id);
            if (models_.end() != found)
                return found->second;

            return nullptr;
        }

        HRenMdlSkinned get_skinned(const dal::path& res_id) override {
            auto found = skin_models_.find(res_id);
            if (skin_models_.end() != found)
                return found->second;

            return nullptr;
        }

    private:
        mirinae::VulkanDevice& device_;
        mirinae::DesclayoutManager& desclayouts_;
        ::LoadTaskManager load_tasks_;
        sung::HTaskSche task_sche_;
        mirinae::HTexMgr tex_man_;
        mirinae::CommandPool cmd_pool_;

        std::map<dal::path, mirinae::HRenMdlStatic> models_;
        std::map<dal::path, mirinae::HRenMdlSkinned> skin_models_;
    };

}  // namespace


// RenderActor
namespace mirinae {

    RenderActor::RenderActor(VulkanDevice& vulkan_device)
        : device_(vulkan_device) {}

    RenderActor::~RenderActor() { this->destroy(); }

    void RenderActor::init(
        uint32_t max_flight_count, DesclayoutManager& desclayouts
    ) {
        auto& desclayout = desclayouts.get("gbuf:actor");
        desc_pool_.init(
            max_flight_count, desclayout.size_info(), device_.logi_device()
        );
        desc_sets_ = desc_pool_.alloc(
            max_flight_count, desclayout.layout(), device_.logi_device()
        );

        for (uint32_t i = 0; i < max_flight_count; ++i) {
            auto& ubuf = uniform_buf_.emplace_back();
            ubuf.init_ubuf(sizeof(U_GbufActor), device_.mem_alloc());
        }

        DescWriteInfoBuilder builder;
        for (size_t i = 0; i < max_flight_count; i++) {
            builder.set_descset(desc_sets_.at(i)).add_ubuf(uniform_buf_.at(i));
        }
        builder.apply_all(device_.logi_device());
    }

    void RenderActor::destroy() {
        for (auto& ubuf : uniform_buf_) ubuf.destroy(device_.mem_alloc());
        uniform_buf_.clear();

        desc_pool_.destroy(device_.logi_device());
    }

    void RenderActor::udpate_ubuf(
        uint32_t index, const U_GbufActor& data, VulkanMemoryAllocator mem_alloc
    ) {
        auto& ubuf = uniform_buf_.at(index);
        ubuf.set_data(&data, sizeof(U_GbufActor), mem_alloc);
    }

    VkDescriptorSet RenderActor::get_desc_set(size_t index) const {
        return desc_sets_.at(index);
    }

}  // namespace mirinae


// RenderActorSkinned
namespace mirinae {

    RenderActorSkinned::RenderActorSkinned(VulkanDevice& vulkan_device)
        : device_(vulkan_device) {}

    RenderActorSkinned::~RenderActorSkinned() { this->destroy(); }

    void RenderActorSkinned::init(
        uint32_t max_flight_count, DesclayoutManager& desclayouts
    ) {
        auto& desclayout = desclayouts.get("gbuf:actor_skinned");
        desc_pool_.init(
            max_flight_count, desclayout.size_info(), device_.logi_device()
        );
        desc_sets_ = desc_pool_.alloc(
            max_flight_count, desclayout.layout(), device_.logi_device()
        );

        for (uint32_t i = 0; i < max_flight_count; ++i) {
            auto& ubuf = uniform_buf_.emplace_back();
            ubuf.init_ubuf(sizeof(U_GbufActorSkinned), device_.mem_alloc());
        }

        DescWriteInfoBuilder builder;
        for (size_t i = 0; i < max_flight_count; i++) {
            builder.set_descset(desc_sets_.at(i)).add_ubuf(uniform_buf_.at(i));
        }
        builder.apply_all(device_.logi_device());
    }

    void RenderActorSkinned::destroy() {
        for (auto& ubuf : uniform_buf_) ubuf.destroy(device_.mem_alloc());
        uniform_buf_.clear();

        desc_pool_.destroy(device_.logi_device());
    }

    void RenderActorSkinned::udpate_ubuf(
        uint32_t index,
        const U_GbufActorSkinned& data,
        VulkanMemoryAllocator mem_alloc
    ) {
        auto& ubuf = uniform_buf_.at(index);
        ubuf.set_data(&data, sizeof(U_GbufActorSkinned), mem_alloc);
    }

    VkDescriptorSet RenderActorSkinned::get_desc_set(size_t index) const {
        return desc_sets_.at(index);
    }

}  // namespace mirinae


// Free functions
namespace mirinae {

    HMdlMgr create_model_mgr(
        sung::HTaskSche task_sche,
        HTexMgr tex_man,
        DesclayoutManager& desclayouts,
        VulkanDevice& device
    ) {
        return std::make_shared<ModelManager>(
            task_sche, tex_man, desclayouts, device
        );
    }

}  // namespace mirinae
