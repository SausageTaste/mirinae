#include "mirinae/render/renderee.hpp"

#include <daltools/dmd/parser.h>
#include <spdlog/spdlog.h>
#include <sung/general/stringtool.hpp>

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
        void fetch(
            const mirinae::respath_t& res_id,
            const dal::parser::Material& src_material,
            mirinae::ITextureManager& tex_man
        ) {
            albedo_map_ = this->request_texture(
                res_id,
                src_material.albedo_map_,
                ":asset/textures/missing_texture.ktx",
                true,
                tex_man
            );

            normal_map_ = this->request_texture(
                res_id,
                src_material.normal_map_,
                ":asset/textures/null_normal_map.png",
                false,
                tex_man
            );

            orm_map_ = this->request_texture(
                res_id,
                src_material.roughness_map_,
                ":asset/textures/white.png",
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
        static std::shared_ptr<mirinae::ITexture> request_texture(
            const mirinae::respath_t& res_id,
            const std::string& file_name,
            const std::string& fallback_path,
            const bool srgb,
            mirinae::ITextureManager& tex_man
        ) {
            if (file_name.empty())
                return tex_man.request(fallback_path, srgb);

            const auto full_path = mirinae::replace_file_name_ext(
                res_id, file_name
            );

            auto output = tex_man.request(full_path, srgb);
            if (!output)
                return tex_man.request(fallback_path, srgb);

            return output;
        }
    };


}  // namespace


// RenderUnit
namespace mirinae {

    void RenderUnit::init(
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

    void RenderUnit::destroy(
        VulkanMemoryAllocator mem_alloc, VkDevice logi_device
    ) {
        vert_index_pair_.destroy(mem_alloc);
        uniform_buf_.destroy(mem_alloc);
        desc_pool_.destroy(logi_device);
    }

    VkDescriptorSet RenderUnit::get_desc_set(size_t index) {
        return desc_sets_.at(index);
    }

    void RenderUnit::record_bind_vert_buf(VkCommandBuffer cmdbuf) {
        vert_index_pair_.record_bind(cmdbuf);
    }

    uint32_t RenderUnit::vertex_count() const {
        return vert_index_pair_.vertex_count();
    }

}  // namespace mirinae


// RenderUnitSkinned
namespace mirinae {

    void RenderUnitSkinned::init(
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

    VkDescriptorSet RenderUnitSkinned::get_desc_set(size_t index) {
        return desc_sets_.at(index);
    }

    void RenderUnitSkinned::record_bind_vert_buf(VkCommandBuffer cmdbuf) {
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
                .add_img_sampler(color_view, sampler)
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

    RenderModel::~RenderModel() {
        for (auto& unit : render_units_)
            unit.destroy(device_.mem_alloc(), device_.logi_device());
        render_units_.clear();

        for (auto& unit : render_units_alpha_)
            unit.destroy(device_.mem_alloc(), device_.logi_device());
        render_units_alpha_.clear();
    }

}  // namespace mirinae


// RenderModelSkinned
namespace mirinae {

    RenderModelSkinned::~RenderModelSkinned() {
        for (auto& unit : runits_)
            unit.destroy(device_.mem_alloc(), device_.logi_device());
        runits_.clear();

        for (auto& unit : runits_alpha_)
            unit.destroy(device_.mem_alloc(), device_.logi_device());
        runits_alpha_.clear();
    }

}  // namespace mirinae


// ModelManager
namespace mirinae {

    class ModelManager::Pimpl {

    public:
        Pimpl(VulkanDevice& device) : device_(device) {
            cmd_pool_.init(
                device_.graphics_queue_family_index().value(),
                device_.logi_device()
            );
        }

        ~Pimpl() { cmd_pool_.destroy(device_.logi_device()); }

        std::shared_ptr<RenderModel> request_static(
            const mirinae::respath_t& res_id,
            DesclayoutManager& desclayouts,
            ITextureManager& tex_man
        ) {
            auto found = models_.find(res_id);
            if (models_.end() != found)
                return found->second;

            const auto content = device_.filesys().read_file(res_id);
            if (!content.has_value()) {
                spdlog::warn("Failed to read dmd file: {}", res_id.u8string());
                return nullptr;
            }

            dal::parser::Model parsed_model;
            const auto parse_result = dal::parser::parse_dmd(
                parsed_model, content->data(), content->size()
            );
            if (dal::parser::ModelParseResult::success != parse_result) {
                spdlog::warn(
                    "Failed to parse dmd file: {}",
                    static_cast<int>(parse_result)
                );
                return nullptr;
            }

            auto output = std::make_shared<RenderModel>(device_);

            for (const auto& src_unit : parsed_model.units_indexed_) {
                VerticesStaticPair dst_vertices;
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

                MaterialResources mat_res;
                mat_res.fetch(res_id, src_unit.material_, tex_man);

                auto& dst_unit =
                    ((src_unit.material_.transparency_)
                         ? output->render_units_alpha_.emplace_back()
                         : output->render_units_.emplace_back());
                dst_unit.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    dst_vertices,
                    mat_res.model_ubuf_,
                    mat_res.albedo_map_->image_view(),
                    mat_res.normal_map_->image_view(),
                    mat_res.orm_map_->image_view(),
                    cmd_pool_,
                    desclayouts,
                    device_
                );
            }

            for (const auto& src_unit : parsed_model.units_indexed_joint_) {
                VerticesStaticPair dst_vertices;
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

                MaterialResources mat_res;
                mat_res.fetch(res_id, src_unit.material_, tex_man);

                auto& dst_unit =
                    ((src_unit.material_.transparency_)
                         ? output->render_units_alpha_.emplace_back()
                         : output->render_units_.emplace_back());
                dst_unit.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    dst_vertices,
                    mat_res.model_ubuf_,
                    mat_res.albedo_map_->image_view(),
                    mat_res.normal_map_->image_view(),
                    mat_res.orm_map_->image_view(),
                    cmd_pool_,
                    desclayouts,
                    device_
                );
            }

            models_[res_id] = output;
            return output;
        }

        std::shared_ptr<RenderModelSkinned> request_skinned(
            const mirinae::respath_t& res_id,
            DesclayoutManager& desclayouts,
            ITextureManager& tex_man
        ) {
            auto found = skin_models_.find(res_id);
            if (skin_models_.end() != found)
                return found->second;

            const auto content = device_.filesys().read_file(res_id);
            if (!content.has_value()) {
                spdlog::warn("Failed to read dmd file: {}", res_id.u8string());
                return nullptr;
            }

            dal::parser::Model parsed_model;
            const auto parse_result = dal::parser::parse_dmd(
                parsed_model, content->data(), content->size()
            );
            if (dal::parser::ModelParseResult::success != parse_result) {
                spdlog::warn(
                    "Failed to parse dmd file: {}",
                    static_cast<int>(parse_result)
                );
                return nullptr;
            }

            auto output = std::make_shared<RenderModelSkinned>(device_);

            if (!parsed_model.units_indexed_.empty()) {
                spdlog::warn(
                    "Skinned model '{}' has static units, which are ignored",
                    res_id.u8string()
                );
            }

            for (const auto& src_unit : parsed_model.units_indexed_joint_) {
                if (src_unit.mesh_.indices_.empty()) {
                    spdlog::warn(
                        "Skinned model '{}' has a render unit with no indices: "
                        "'{}'",
                        res_id.u8string(),
                        src_unit.name_
                    );
                    continue;
                }

                VerticesSkinnedPair dst_vertices;

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

                MaterialResources mat_res;
                mat_res.fetch(res_id, src_unit.material_, tex_man);

                auto& dst_unit = src_unit.material_.transparency_
                                     ? output->runits_alpha_.emplace_back()
                                     : output->runits_.emplace_back();
                dst_unit.init(
                    mirinae::MAX_FRAMES_IN_FLIGHT,
                    dst_vertices,
                    mat_res.model_ubuf_,
                    mat_res.albedo_map_->image_view(),
                    mat_res.normal_map_->image_view(),
                    mat_res.orm_map_->image_view(),
                    cmd_pool_,
                    desclayouts,
                    device_
                );
            }

            output->skel_anim_->skel_ = parsed_model.skeleton_;
            output->skel_anim_->anims_ = parsed_model.animations_;

            skin_models_[res_id] = output;
            return output;
        }

    private:
        VulkanDevice& device_;
        CommandPool cmd_pool_;

        std::map<respath_t, std::shared_ptr<RenderModel>> models_;
        std::map<respath_t, std::shared_ptr<RenderModelSkinned>> skin_models_;
    };


    ModelManager::ModelManager(VulkanDevice& device)
        : pimpl_(std::make_unique<Pimpl>(device)) {}

    ModelManager::~ModelManager() {}

    std::shared_ptr<RenderModel> ModelManager::request_static(
        const mirinae::respath_t& res_id,
        DesclayoutManager& desclayouts,
        ITextureManager& tex_man
    ) {
        return pimpl_->request_static(res_id, desclayouts, tex_man);
    }

    std::shared_ptr<RenderModelSkinned> ModelManager::request_skinned(
        const mirinae::respath_t& res_id,
        DesclayoutManager& desclayouts,
        ITextureManager& tex_man
    ) {
        return pimpl_->request_skinned(res_id, desclayouts, tex_man);
    }

}  // namespace mirinae


// RenderActor
namespace mirinae {

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

    VkDescriptorSet RenderActor::get_desc_set(size_t index) {
        return desc_sets_.at(index);
    }

}  // namespace mirinae


// RenderActorSkinned
namespace mirinae {

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

    VkDescriptorSet RenderActorSkinned::get_desc_set(size_t index) {
        return desc_sets_.at(index);
    }

}  // namespace mirinae
