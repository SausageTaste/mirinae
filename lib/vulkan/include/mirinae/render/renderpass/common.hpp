#pragma once

#include <map>

#include "mirinae/cosmos.hpp"
#include "mirinae/render/renderee.hpp"
#include "mirinae/render/vkdevice.hpp"


namespace mirinae {

    using FrameIndex = mirinae::StrongType<int, struct FrameIndexStrongTypeTag>;


    struct DrawSheet {
        struct StaticRenderPairs {
            struct Actor {
                mirinae::RenderActor* actor_;
                glm::dmat4 model_mat_;
            };

            mirinae::RenderModel* model_;
            std::vector<Actor> actors_;
        };

        struct SkinnedRenderPairs {
            struct Actor {
                mirinae::RenderActorSkinned* actor_;
                glm::dmat4 model_mat_;
            };

            mirinae::RenderModelSkinned* model_;
            std::vector<Actor> actors_;
        };

        StaticRenderPairs& get_static_pair(mirinae::RenderModel& model) {
            for (auto& x : static_pairs_) {
                if (x.model_ == &model)
                    return x;
            }

            auto& output = static_pairs_.emplace_back();
            output.model_ = &model;
            return output;
        }

        SkinnedRenderPairs& get_skinn_pair(mirinae::RenderModelSkinned& model) {
            for (auto& x : skinned_pairs_) {
                if (x.model_ == &model)
                    return x;
            }

            auto& output = skinned_pairs_.emplace_back();
            output.model_ = &model;
            return output;
        }

        std::vector<StaticRenderPairs> static_pairs_;
        std::vector<SkinnedRenderPairs> skinned_pairs_;
        cpnt::Ocean* ocean_ = nullptr;
        cpnt::AtmosphereSimple* atmosphere_ = nullptr;
    };


    class FbufImageBundle {

    public:
        void init(
            uint32_t width,
            uint32_t height,
            mirinae::ITextureManager& tex_man,
            mirinae::VulkanDevice& device
        ) {
            depth_ = create_tex_depth(width, height, device);
            albedo_ = create_tex_attach(
                width,
                height,
                VK_FORMAT_R8G8B8A8_UNORM,
                mirinae::FbufUsage::color_attachment,
                "albedo",
                device
            );
            normal_ = create_tex_attach(
                width,
                height,
                VK_FORMAT_R8G8B8A8_UNORM,
                mirinae::FbufUsage::color_attachment,
                "normal",
                device
            );
            material_ = create_tex_attach(
                width,
                height,
                VK_FORMAT_R8G8B8A8_UNORM,
                mirinae::FbufUsage::color_attachment,
                "material",
                device
            );
            compo_ = create_tex_attach(
                width,
                height,
                device.img_formats().rgb_hdr(),
                mirinae::FbufUsage::color_attachment,
                "compo",
                device
            );
        }

        uint32_t width() const { return depth_->width(); }
        uint32_t height() const { return depth_->height(); }
        VkExtent2D extent() const { return { this->width(), this->height() }; }

        mirinae::ITexture& depth() { return *depth_; }
        mirinae::ITexture& albedo() { return *albedo_; }
        mirinae::ITexture& normal() { return *normal_; }
        mirinae::ITexture& material() { return *material_; }
        mirinae::ITexture& compo() { return *compo_; }

    private:
        std::unique_ptr<mirinae::ITexture> depth_;
        std::unique_ptr<mirinae::ITexture> albedo_;
        std::unique_ptr<mirinae::ITexture> normal_;
        std::unique_ptr<mirinae::ITexture> material_;
        std::unique_ptr<mirinae::ITexture> compo_;
    };


    class IRenderPassBundle {

    public:
        IRenderPassBundle(mirinae::VulkanDevice& device) : device_(device) {}

        virtual ~IRenderPassBundle() = default;
        virtual void destroy() = 0;

        virtual VkFramebuffer fbuf_at(uint32_t index) const = 0;
        virtual const VkClearValue* clear_values() const = 0;
        virtual uint32_t clear_value_count() const = 0;

        VkRenderPass renderpass() const { return renderpass_; }
        VkPipeline pipeline() const { return pipeline_; }
        VkPipelineLayout pipeline_layout() const { return layout_; }

    protected:
        mirinae::VulkanDevice& device_;
        VkRenderPass renderpass_ = VK_NULL_HANDLE;
        VkPipeline pipeline_ = VK_NULL_HANDLE;
        VkPipelineLayout layout_ = VK_NULL_HANDLE;
    };


    class IRenderPassRegistry {

    public:
        virtual ~IRenderPassRegistry() = default;

        virtual void add(
            const std::string& name, std::unique_ptr<IRenderPassBundle>&& rp
        ) = 0;

        virtual const IRenderPassBundle& get(const std::string& name) const = 0;

        template <typename T, typename... Args>
        void add(const std::string& name, Args&&... args) {
            this->add(name, std::make_unique<T>(std::forward<Args>(args)...));
        }
    };


    class RenderPassRaii {

    public:
        RenderPassRaii(VulkanDevice& device) : device_(device) {}

        ~RenderPassRaii() { this->clear(); }

        RenderPassRaii& operator=(VkRenderPass rp) {
            this->reset(rp);
            return *this;
        }

        operator VkRenderPass() const { return rp_; }

        VkRenderPass operator*() const { return rp_; }

        VkRenderPass get() const { return rp_; }

        void reset(VkRenderPass rp) {
            this->clear();
            rp_ = rp;
        }

        void clear() {
            if (rp_ != VK_NULL_HANDLE)
                vkDestroyRenderPass(device_.logi_device(), rp_, nullptr);
            rp_ = VK_NULL_HANDLE;
        }

    public:
        VulkanDevice& device_;
        VkRenderPass rp_ = VK_NULL_HANDLE;
    };


    class RpResources {

    public:
        using str = std::string;

        class Image {

        public:
            Image(const str& id) : id_(id) {}
            const std::string& id() const { return id_; }

            mirinae::Image img_;
            mirinae::ImageView view_;

        private:
            std::string id_;
        };
        using HImage = std::shared_ptr<Image>;

    public:
        RpResources(VulkanDevice& device);
        ~RpResources();

        // Remove the user from user list of the image.
        // If the user list becomes empty, the image will be freed.
        void free_img(const str& id, const str& user_id);

        // The user will be a writer.
        // The ID will be made like this: `<user_id>:<name>`.
        HImage new_img(const str& name, const str& user_id);

        // The user will be a reader.
        // Returns nullptr if the image does not exist.
        HImage get_img_reader(const str& id, const str& user_id);

    private:
        class ImageRecord;

        VulkanDevice& device_;
        std::map<str, ImageRecord> imgs_;
    };

    using RpImage = RpResources::Image;
    using HRpImage = std::shared_ptr<RpImage>;


    struct ViewFrustum {
        void update(const glm::dmat4& proj, const glm::dmat4& view);

        std::vector<glm::vec3> vertices_;
        std::vector<glm::vec3> axes_;
        glm::dmat4 view_inv_;
    };


    struct RpContext {
        std::shared_ptr<CosmosSimulator> cosmos_;
        std::shared_ptr<DrawSheet> draw_sheet_;
        ViewFrustum view_frustum_;
        FrameIndex f_index_;
        ShainImageIndex i_index_;
        glm::dmat4 proj_mat_;
        glm::dmat4 view_mat_;
        glm::dvec3 view_pos_;
        VkCommandBuffer cmdbuf_;
    };


    struct IRpStates {
        virtual ~IRpStates() = default;
        virtual const std::string& name() const = 0;
        virtual void record(const RpContext& context) = 0;
    };

    using URpStates = std::unique_ptr<IRpStates>;

}  // namespace mirinae
