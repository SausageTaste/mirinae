#include "mirinae/vulkan_pch.h"

#include "mirinae/render/texture.hpp"

#include <ktxvulkan.h>
#include <daltools/common/task_sys.hpp>
#include <daltools/img/backend/ktx.hpp>
#include <daltools/img/backend/stb.hpp>
#include <sung/basic/stringtool.hpp>
#include <sung/basic/time.hpp>

#include "mirinae/render/cmdbuf.hpp"
#include "mirinae/render/enum_str.hpp"
#include "mirinae/render/mem_cinfo.hpp"
#include "mirinae/render/vkmajorplayers.hpp"

#define SWITCH_STR(x) \
    case x:           \
        return #x;


namespace {

    const char* to_str(ktx_error_code_e code) {
        switch (code) {
            SWITCH_STR(KTX_SUCCESS)
            SWITCH_STR(KTX_FILE_DATA_ERROR)
            SWITCH_STR(KTX_FILE_ISPIPE)
            SWITCH_STR(KTX_FILE_OPEN_FAILED)
            SWITCH_STR(KTX_FILE_OVERFLOW)
            SWITCH_STR(KTX_FILE_READ_ERROR)
            SWITCH_STR(KTX_FILE_SEEK_ERROR)
            SWITCH_STR(KTX_FILE_UNEXPECTED_EOF)
            SWITCH_STR(KTX_FILE_WRITE_ERROR)
            SWITCH_STR(KTX_GL_ERROR)
            SWITCH_STR(KTX_INVALID_OPERATION)
            SWITCH_STR(KTX_INVALID_VALUE)
            SWITCH_STR(KTX_NOT_FOUND)
            SWITCH_STR(KTX_OUT_OF_MEMORY)
            SWITCH_STR(KTX_TRANSCODE_FAILED)
            SWITCH_STR(KTX_UNKNOWN_FILE_FORMAT)
            SWITCH_STR(KTX_UNSUPPORTED_TEXTURE_TYPE)
            SWITCH_STR(KTX_UNSUPPORTED_FEATURE)
            SWITCH_STR(KTX_LIBRARY_NOT_LINKED)
            SWITCH_STR(KTX_DECOMPRESS_LENGTH_ERROR)
            SWITCH_STR(KTX_DECOMPRESS_CHECKSUM_ERROR)
            default:
                return "unknown";
        }
    }


    auto interpret_fbuf_usage(const mirinae::FbufUsage usage) {
        VkImageAspectFlags aspect_mask = 0;
        VkImageLayout image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageUsageFlags flag = 0;

        switch (usage) {
            case mirinae::FbufUsage::color_attachment:
                aspect_mask = VK_IMAGE_ASPECT_COLOR_BIT;
                image_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
                flag = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT |
                       VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
                break;

            case mirinae::FbufUsage::depth_map:
                aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
                image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                flag = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT |
                       VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
                break;

            case mirinae::FbufUsage::depth_stencil_attachment:
                aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT |
                              VK_IMAGE_ASPECT_STENCIL_BIT;
                image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                flag = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT |
                       VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
                break;

            case mirinae::FbufUsage::depth_attachment:
                aspect_mask = VK_IMAGE_ASPECT_DEPTH_BIT;
                image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
                flag = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                       VK_IMAGE_USAGE_SAMPLED_BIT |
                       VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
                break;

            default:
                MIRINAE_ABORT("unsupported framebuffer usage: {}", (int)usage);
        }

        return std::make_tuple(flag, aspect_mask, image_layout);
    }

    void copy_buffer_to_image(
        const VkCommandBuffer cmdbuf,
        const VkImage dst_image,
        const VkBuffer src_buffer,
        const uint32_t width,
        const uint32_t height,
        const uint32_t mip_level
    ) {
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = mip_level;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { width, height, 1 };

        vkCmdCopyBufferToImage(
            cmdbuf,
            src_buffer,
            dst_image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );
    }

    void generate_mipmaps(
        const VkCommandBuffer cmdbuf,
        const VkImage image,
        const uint32_t width,
        const uint32_t height,
        const uint32_t mip_levels
    ) {
        mirinae::ImageMemoryBarrier barrier;
        barrier.image(image)
            .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
            .mip_count(1)
            .layer_base(0)
            .layer_count(1);

        int32_t mip_width = width;
        int32_t mip_height = height;
        for (uint32_t i = 1; i < mip_levels; i++) {
            barrier.set_src_access(VK_ACCESS_TRANSFER_WRITE_BIT)
                .set_dst_access(VK_ACCESS_TRANSFER_READ_BIT)
                .old_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                .mip_base(i - 1);
            barrier.record_single(
                cmdbuf,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT
            );

            mirinae::ImageBlit blit;
            blit.set_src_offsets_full(mip_width, mip_height)
                .set_dst_offsets_full(
                    mirinae::make_half_dim(mip_width),
                    mirinae::make_half_dim(mip_height)
                );
            blit.src_subres()
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .mip_level(i - 1)
                .layer_base(0)
                .layer_count(1);
            blit.dst_subres()
                .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
                .mip_level(i)
                .layer_base(0)
                .layer_count(1);

            vkCmdBlitImage(
                cmdbuf,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1,
                &blit.get(),
                VK_FILTER_LINEAR
            );

            barrier.set_src_access(VK_ACCESS_TRANSFER_READ_BIT)
                .set_dst_access(VK_ACCESS_SHADER_READ_BIT)
                .old_layout(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
                .new_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            barrier.record_single(
                cmdbuf,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
            );

            mip_width = mirinae::make_half_dim(mip_width);
            mip_height = mirinae::make_half_dim(mip_height);
        }

        barrier.set_src_access(VK_ACCESS_TRANSFER_WRITE_BIT)
            .set_dst_access(VK_ACCESS_SHADER_READ_BIT)
            .old_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            .new_layout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            .mip_base(mip_levels - 1);
        barrier.record_single(
            cmdbuf,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
        );
    }


    class KtxDeviceInfo {

    public:
        KtxDeviceInfo() = default;
        ~KtxDeviceInfo() { this->destroy(); }

        bool init(VkCommandPool cmd_pool, mirinae::VulkanDevice& device) {
            const auto res = ktxVulkanDeviceInfo_Construct(
                &ktx_device_,
                device.phys_device(),
                device.logi_device(),
                device.graphics_queue(),
                cmd_pool,
                nullptr
            );
            if (res == KTX_SUCCESS) {
                initialized_ = true;
                return true;
            }
            return false;
        }

        void destroy() {
            if (initialized_) {
                ktxVulkanDeviceInfo_Destruct(&ktx_device_);
                initialized_ = false;
            }
        }

        std::optional<ktx_error_code_e> upload_tex(
            ktxTexture& This,
            ktxVulkanTexture& vkTexture,
            VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
            VkImageUsageFlags usageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
            VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        ) {
            const auto result = ktxTexture_VkUploadEx(
                &This, &ktx_device_, &vkTexture, tiling, usageFlags, finalLayout
            );

            if (KTX_SUCCESS == result) {
                return std::nullopt;
            } else {
                return result;
            }
        }

    private:
        ktxVulkanDeviceInfo ktx_device_;
        bool initialized_ = false;
    };

}  // namespace


// Texture data
namespace {

    struct ITextureData : public mirinae::ITexture {
        virtual ~ITextureData() = default;

        virtual void destroy() = 0;
        virtual const std::string& id() const = 0;
    };


    class TextureData : public ITextureData {

    public:
        TextureData(mirinae::VulkanDevice& device) : device_(device) {}

        ~TextureData() { this->destroy(); }

        void init_iimage2d(
            const std::string& id,
            const dal::TDataImage2D<uint8_t>& image,
            std::shared_ptr<dal::IImage> img_data,
            bool srgb,
            mirinae::CommandPool& cmd_pool
        ) {
            id_ = id;
            img_data_ = img_data;

            mirinae::BufferCreateInfo staging_buf_cinfo;
            staging_buf_cinfo.preset_staging(image.data_size());

            mirinae::Buffer staging_buffer;
            staging_buffer.init(staging_buf_cinfo, device_.mem_alloc());
            staging_buffer.set_data(image.data(), image.data_size());

            mirinae::ImageCreateInfo img_info;
            img_info.fetch_from_image(image, srgb)
                .deduce_mip_levels()
                .add_usage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                .add_usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                .add_usage_sampled();
            texture_.init(img_info.get(), device_.mem_alloc());

            auto cmdbuf = cmd_pool.begin_single_time(device_.logi_device());
            mirinae::record_img_buf_copy_mip(
                cmdbuf,
                texture_.width(),
                texture_.height(),
                texture_.mip_levels(),
                texture_.image(),
                staging_buffer.buffer()
            );
            cmd_pool.end_single_time(cmdbuf, device_);
            staging_buffer.destroy();

            mirinae::ImageViewBuilder iv_builder;
            iv_builder.format(texture_.format())
                .mip_levels(texture_.mip_levels())
                .image(texture_.image());
            texture_view_.reset(iv_builder, device_);

            VkMemoryRequirements mem_req = {};
            vkGetImageMemoryRequirements(
                device_.logi_device(), texture_.image(), &mem_req
            );

            SPDLOG_DEBUG(
                "Raw texture loaded: {}*{}, {}, {}, {} levels, '{}'",
                texture_.width(),
                texture_.height(),
                sung::lstrip(mirinae::to_str(texture_.format()), "VK_FORMAT_"),
                sung::format_bytes(mem_req.size),
                img_info.mip_levels(),
                id
            );
        }

        void init_iimage2d(
            const std::string& id,
            const dal::TDataImage2D<float>& image,
            std::shared_ptr<dal::IImage> img_data,
            bool srgb,
            mirinae::CommandPool& cmd_pool
        ) {
            id_ = id;
            img_data_ = img_data;

            mirinae::BufferCreateInfo staging_buf_cinfo;
            staging_buf_cinfo.preset_staging(image.data_size());

            mirinae::Buffer staging_buffer;
            staging_buffer.init(staging_buf_cinfo, device_.mem_alloc());
            staging_buffer.set_data(image.data(), image.data_size());

            mirinae::ImageCreateInfo img_info;
            img_info.fetch_from_image(image, srgb)
                .deduce_mip_levels()
                .add_usage(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                .add_usage(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                .add_usage_sampled();
            texture_.init(img_info.get(), device_.mem_alloc());

            auto cmdbuf = cmd_pool.begin_single_time(device_.logi_device());
            mirinae::record_img_buf_copy_mip(
                cmdbuf,
                texture_.width(),
                texture_.height(),
                texture_.mip_levels(),
                texture_.image(),
                staging_buffer.buffer()
            );
            cmd_pool.end_single_time(cmdbuf, device_);
            staging_buffer.destroy();

            mirinae::ImageViewBuilder iv_builder;
            iv_builder.format(texture_.format())
                .mip_levels(texture_.mip_levels())
                .image(texture_.image());
            texture_view_.reset(iv_builder, device_);

            VkMemoryRequirements mem_req = {};
            vkGetImageMemoryRequirements(
                device_.logi_device(), texture_.image(), &mem_req
            );

            SPDLOG_DEBUG(
                "Raw texture loaded: {}*{}, {}, {}, {} levels, '{}'",
                texture_.width(),
                texture_.height(),
                sung::lstrip(mirinae::to_str(texture_.format()), "VK_FORMAT_"),
                sung::format_bytes(mem_req.size),
                img_info.mip_levels(),
                id
            );
        }

        void init_depth(uint32_t width, uint32_t height) {
            id_ = "<depth>";

            mirinae::ImageCreateInfo img_info;
            img_info.set_dimensions(width, height)
                .set_format(device_.img_formats().depth_map())
                .add_usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
                .add_usage_sampled();
            texture_.init(img_info.get(), device_.mem_alloc());

            mirinae::ImageViewBuilder iv_builder;
            iv_builder.format(device_.img_formats().depth_map())
                .aspect_mask(VK_IMAGE_ASPECT_DEPTH_BIT)
                .image(texture_.image());
            texture_view_.reset(iv_builder, device_);
        }

        void init_attachment(
            uint32_t width,
            uint32_t height,
            VkFormat format,
            mirinae::FbufUsage usage,
            const char* name
        ) {
            id_ = name;

            const auto [usage_flag, aspect_mask, image_layout] =
                ::interpret_fbuf_usage(usage);
            mirinae::ImageCreateInfo img_info;
            img_info.set_dimensions(width, height)
                .set_format(format)
                .add_usage(usage_flag);
            texture_.init(img_info.get(), device_.mem_alloc());

            mirinae::ImageViewBuilder iv_builder;
            iv_builder.format(format)
                .aspect_mask(aspect_mask)
                .image(texture_.image());
            texture_view_.reset(iv_builder, device_);
        }

        void destroy() override {
            texture_view_.destroy(device_);
            texture_.destroy(device_.mem_alloc());
        }

        VkFormat format() const override { return texture_.format(); }

        VkImage image() const override { return texture_.image(); }
        VkImageView image_view() const override { return texture_view_.get(); }

        uint32_t width() const override { return texture_.width(); }
        uint32_t height() const override { return texture_.height(); }

        void free_img_data() override { img_data_.reset(); }
        const dal::IImage* img_data() const override { return img_data_.get(); }

        const std::string& id() const override { return id_; }

    private:
        mirinae::VulkanDevice& device_;
        mirinae::Image texture_;
        mirinae::ImageView texture_view_;
        std::shared_ptr<dal::IImage> img_data_;
        std::string id_;
    };


    class KtxTextureData : public ITextureData {

    public:
        KtxTextureData(mirinae::VulkanDevice& device) : device_(device) {}

        bool init(
            const std::string& id, dal::KtxImage& src, KtxDeviceInfo& vdi
        ) {
            this->destroy();

            id_ = id;

            if (src.need_transcoding()) {
                SPDLOG_ERROR("KTX image needs transcoding: {}", id);
                return false;
            }

            data_ = ktxVulkanTexture{};
            if (const auto res = vdi.upload_tex(src.ktx(), data_.value())) {
                SPDLOG_ERROR("Failed to upload KTX ({}): {}", to_str(*res), id);
                return false;
            }

            mirinae::ImageViewBuilder iv_builder;
            iv_builder.format(data_->imageFormat)
                .mip_levels(data_->levelCount)
                .image(data_->image);
            texture_view_.reset(iv_builder, device_);

            VkMemoryRequirements mem_req = {};
            vkGetImageMemoryRequirements(
                device_.logi_device(), data_->image, &mem_req
            );

            SPDLOG_DEBUG(
                "KTX texture loaded: {}*{}, {}, {}, {} levels, '{}'",
                src.base_width(),
                src.base_height(),
                sung::lstrip(mirinae::to_str(data_->imageFormat), "VK_FORMAT_"),
                sung::format_bytes(mem_req.size),
                data_->levelCount,
                id
            );
            return true;
        }

        ~KtxTextureData() { this->destroy(); }

        void destroy() override {
            id_.clear();
            texture_view_.destroy(device_);

            if (data_) {
                ktxVulkanTexture_Destruct(
                    &data_.value(), device_.logi_device(), nullptr
                );
                data_.reset();
            }
        }

        VkFormat format() const override { return data_->imageFormat; }
        VkImage image() const override { return data_->image; }
        VkImageView image_view() const override { return texture_view_.get(); }
        uint32_t width() const override { return data_->width; }
        uint32_t height() const override { return data_->height; }

        const std::string& id() const override { return id_; }

    private:
        mirinae::VulkanDevice& device_;
        std::string id_;
        std::optional<ktxVulkanTexture> data_;
        mirinae::ImageView texture_view_;
    };

}  // namespace


// Loader
namespace {

    namespace fs = std::filesystem;


    std::optional<ktx_texture_transcode_fmt_e> determine_transcode_format(
        dal::KtxImage& src, const VkPhysicalDeviceFeatures& df
    ) {
        auto ktx2 = src.ktx2();
        if (!ktx2)
            return std::nullopt;

        const auto cm = ktxTexture2_GetColorModel_e(ktx2);
        const auto num_cpnt = src.num_cpnts();

        if (cm == KHR_DF_MODEL_UASTC && df.textureCompressionASTC_LDR)
            return KTX_TTF_ASTC_4x4_RGBA;
        else if (cm == KHR_DF_MODEL_ETC1S && df.textureCompressionETC2)
            return KTX_TTF_ETC;
        else if (df.textureCompressionASTC_LDR)
            return KTX_TTF_ASTC_4x4_RGBA;
        else if (df.textureCompressionETC2)
            return KTX_TTF_ETC2_RGBA;
        else if (df.textureCompressionBC) {
            switch (num_cpnt) {
                case 1:
                    return KTX_TTF_BC4_R;
                case 2:
                    return KTX_TTF_BC5_RG;
                case 3:
                    return KTX_TTF_BC1_RGB;
                case 4:
                    return KTX_TTF_BC3_RGBA;
                default:
                    return std::nullopt;
            }
        } else
            return std::nullopt;
    }


    class ImageLoadTask : public sung::IStandardLoadTask {

    public:
        ImageLoadTask(
            const fs::path& path,
            dal::Filesystem& filesys,
            const VkPhysicalDeviceFeatures& device_features
        )
            : filesys_(&filesys), path_(path), df_(device_features) {}

        sung::TaskStatus tick() override {
            if (path_.empty())
                return this->fail("Path is empty");
            if (!filesys_)
                return this->fail("Filesystem is not set");

            filesys_->read_file(path_, raw_data_);
            if (raw_data_.empty())
                return this->fail("Failed to read file");

            dal::ImageParseInfo pinfo;
            pinfo.file_path_ = path_.u8string();
            pinfo.data_ = reinterpret_cast<uint8_t*>(raw_data_.data());
            pinfo.size_ = raw_data_.size();
            pinfo.force_rgba_ = true;

            img_ = dal::parse_img(pinfo);
            if (!img_)
                return this->fail("Failed to parse image");

            if (auto ktx = img_->as<dal::KtxImage>()) {
                if (ktx->need_transcoding()) {
                    auto tf = ::determine_transcode_format(*ktx, df_);
                    if (!tf)
                        return this->fail("Failed to find transcode format");
                    if (!ktx->transcode(tf.value()))
                        return this->fail("Failed to transcode KTX");
                }
            }

            return this->success();
        }

        std::shared_ptr<dal::IImage> try_get_img() {
            if (!this->has_succeeded())
                return nullptr;
            if (!img_)
                return nullptr;
            return img_;
        }

        const fs::path& file_path() const { return path_; }

    private:
        fs::path path_;
        dal::Filesystem* filesys_;
        VkPhysicalDeviceFeatures df_;
        std::vector<std::byte> raw_data_;
        std::shared_ptr<dal::IImage> img_;
    };


    class LoadTaskManager {

    public:
        LoadTaskManager(
            sung::HTaskSche task_sche, mirinae::VulkanDevice& device
        )
            : task_sche_(task_sche)
            , filesys_(&device.filesys())
            , device_features_(device.features()) {}

        bool add_task(const fs::path& path) {
            if (this->has_task(path))
                return false;

            auto task = std::make_shared<ImageLoadTask>(
                path, *filesys_, device_features_
            );
            task_sche_->add_task(task);
            tasks_.emplace(path.u8string(), task);
            return true;
        }

        bool has_task(const fs::path& path) {
            return tasks_.find(path.u8string()) != tasks_.end();
        }

        void remove_task(const fs::path& path) {
            tasks_.erase(path.u8string());
        }

        std::shared_ptr<ImageLoadTask> try_get_task(const fs::path& path) {
            const auto it = tasks_.find(path.u8string());
            if (it == tasks_.end())
                return nullptr;
            return it->second;
        }

    private:
        std::unordered_map<std::string, std::shared_ptr<ImageLoadTask>> tasks_;
        sung::HTaskSche task_sche_;
        dal::Filesystem* filesys_;
        VkPhysicalDeviceFeatures device_features_;
    };

}  // namespace


namespace {

    class FinishResultMut {

    public:
        FinishResultMut() = default;

        FinishResultMut(const FinishResultMut&) = delete;
        FinishResultMut& operator=(const FinishResultMut&) = delete;

        FinishResultMut(FinishResultMut&& rhs) {
            std::lock_guard lock(rhs.mut_);
            done_ = rhs.done_;
            err_msg_ = std::move(rhs.err_msg_);
        }

        FinishResultMut& operator=(FinishResultMut&& rhs) {
            if (this != &rhs) {
                std::lock_guard lock1(mut_, std::adopt_lock);
                std::lock_guard lock2(rhs.mut_, std::adopt_lock);
                done_ = rhs.done_;
                err_msg_ = std::move(rhs.err_msg_);
            }
            return *this;
        }

        bool is_done() {
            std::lock_guard lock(mut_);
            return done_;
        }

        bool has_succeeded() {
            std::lock_guard lock(mut_);
            return done_ && err_msg_.empty();
        }

        bool has_failed() {
            std::lock_guard lock(mut_);
            return done_ && !err_msg_.empty();
        }

        std::string get_err_msg() {
            std::lock_guard lock(mut_);
            return err_msg_;
        }

        void succeed() {
            std::lock_guard lock(mut_);
            done_ = true;
            err_msg_.clear();
        }

        void fail(const std::string& err_msg) {
            std::lock_guard lock(mut_);
            done_ = true;
            err_msg_ = err_msg;
        }

        void reset() {
            std::lock_guard lock(mut_);
            done_ = false;
            err_msg_.clear();
        }

    private:
        std::mutex mut_;
        std::string err_msg_;
        bool done_ = false;
    };


    class ImageFactory {

    public:
        ImageFactory(
            const VkPhysicalDeviceFeatures& df, dal::Filesystem& filesys
        )
            : device_features_(df), filesys_(filesys) {}

        ImageFactory(const ImageFactory&) = delete;
        ImageFactory(ImageFactory&&) = delete;
        ImageFactory& operator=(const ImageFactory&) = delete;
        ImageFactory& operator=(ImageFactory&&) = delete;

        void tick() {
            for (auto& item : items_) {
                item.update();
            }
        }

        void request_loading(const fs::path& path) {
            auto& item = items_.emplace_back();
            item.init(path, device_features_, filesys_);
        }

    private:
        class LoadTask
            : public enki::ITaskSet
            , public FinishResultMut {

        public:
            void init(
                const fs::path& path,
                const VkPhysicalDeviceFeatures& df,
                dal::Filesystem& filesys
            ) {
                image_path_ = path;
                df_ = &df;
                filesys_ = &filesys;
            }

            void start() {
                if (image_path_.empty())
                    return this->fail("Path is empty");
                if (!filesys_)
                    return this->fail("Filesystem is not set");

                filesys_->read_file(image_path_, raw_data_);
                if (raw_data_.empty())
                    return this->fail("Failed to read file");

                dal::ImageParseInfo pinfo;
                pinfo.file_path_ = image_path_.u8string();
                pinfo.data_ = reinterpret_cast<uint8_t*>(raw_data_.data());
                pinfo.size_ = raw_data_.size();
                pinfo.force_rgba_ = true;

                img_ = dal::parse_img(pinfo);
                if (!img_)
                    return this->fail("Failed to parse image");

                if (auto ktx = img_->as<dal::KtxImage>()) {
                    if (ktx->need_transcoding()) {
                        auto tf = ::determine_transcode_format(*ktx, *df_);
                        if (!tf)
                            return this->fail("Failed to find transcode");
                        if (!ktx->transcode(tf.value()))
                            return this->fail("Failed to transcode KTX");
                    }
                }

                return this->succeed();
            }

            const fs::path& img_path() const { return image_path_; }

        private:
            void ExecuteRange(enki::TaskSetPartition, uint32_t) override {
                this->start();
            }

            // Input
            fs::path image_path_;
            const VkPhysicalDeviceFeatures* df_ = nullptr;
            dal::Filesystem* filesys_ = nullptr;
            // Output
            std::vector<std::byte> raw_data_;
            std::shared_ptr<dal::IImage> img_;
        };

        class Item : public FinishResultMut {

        public:
            Item() = default;
            Item(const Item&) = delete;
            Item& operator=(const Item&) = delete;

            void init(
                const fs::path& path,
                const VkPhysicalDeviceFeatures& df,
                dal::Filesystem& filesys
            ) {
                load_task_.init(path, df, filesys);

                dal::tasker().AddTaskSetToPipe(&load_task_);
            }

            void update() {
                if (!load_task_.is_done()) {
                    return;
                }
                if (load_task_.has_failed()) {
                    return this->fail(load_task_.get_err_msg());
                }

                SPDLOG_INFO(
                    "Image loaded: {}", load_task_.img_path().u8string()
                );
            }

        private:
            LoadTask load_task_;
        };

        dal::Filesystem& filesys_;
        std::list<Item> items_;
        VkPhysicalDeviceFeatures device_features_;
    };

}  // namespace


// TextureManager
namespace {

    class TextureManager : public mirinae::ITextureManager {

    public:
        TextureManager(sung::HTaskSche task_sche, mirinae::VulkanDevice& device)
            : device_(device)
            , img_factory_(device.features(), device.filesys())
            , loader_mgr_(task_sche, device) {
            cmd_pool_.init(
                device_.graphics_queue_family_index().value(),
                device_.logi_device()
            );

            if (!ktx_device_.init(cmd_pool_.get(), device)) {
                MIRINAE_ABORT("Failed to construct KTX device info");
            }

            // Missing tex
            {
                ::ImageLoadTask task(
                    ":asset/textures/missing_texture.ktx",
                    device.filesys(),
                    device.features()
                );
                while (task.tick() == sung::TaskStatus::running) {
                }

                if (task.has_succeeded()) {
                    auto img = task.try_get_img();
                    if (auto kts_img = img->as<dal::KtxImage>()) {
                        auto out = std::make_shared<KtxTextureData>(device_);
                        if (out->init(
                                "missing_texture", *kts_img, ktx_device_
                            )) {
                            missing_tex_ = out;
                        }
                        textures_.push_back(out);
                    }
                } else {
                    MIRINAE_ABORT("Failed to load missing texture");
                }
            }
        }

        ~TextureManager() {
            this->destroy_all();
            ktx_device_.destroy();
            cmd_pool_.destroy(device_.logi_device());
        }

        dal::ReqResult request(const dal::path& res_id, bool srgb) override {
            img_factory_.tick();

            if (auto index = this->find_index(res_id))
                return dal::ReqResult::ready;

            auto task = loader_mgr_.try_get_task(res_id);
            if (!task) {
                img_factory_.request_loading(res_id);
                loader_mgr_.add_task(res_id);
                return dal::ReqResult::loading;
            }
            if (!task->is_done())
                return dal::ReqResult::loading;

            const auto id = res_id.u8string();
            auto img = task->try_get_img();
            if (!img) {
                SPDLOG_ERROR(
                    "Failed to load image ({}): {}", id, task->err_msg()
                );
                return dal::ReqResult::cannot_read_file;
            }

            if (auto kts_img = img->as<dal::KtxImage>()) {
                auto out = std::make_shared<KtxTextureData>(device_);
                if (out->init(id, *kts_img, ktx_device_)) {
                    textures_.push_back(out);
                    return dal::ReqResult::ready;
                } else {
                    return dal::ReqResult::not_supported_file;
                }
            } else if (auto raw_img = img->as<dal::TDataImage2D<uint8_t>>()) {
                auto out = std::make_shared<TextureData>(device_);
                out->init_iimage2d(id, *raw_img, img, srgb, cmd_pool_);
                textures_.push_back(out);
                return dal::ReqResult::ready;
            } else if (auto raw_img = img->as<dal::TDataImage2D<float>>()) {
                auto out = std::make_shared<TextureData>(device_);
                out->init_iimage2d(id, *raw_img, img, srgb, cmd_pool_);
                textures_.push_back(out);
                return dal::ReqResult::ready;
            } else {
                SPDLOG_ERROR("Unsupported image type: {}", id);
                return dal::ReqResult::not_supported_file;
            }

            return dal::ReqResult::unknown_error;
        }

        std::shared_ptr<mirinae::ITexture> get(
            const dal::path& res_id
        ) override {
            if (auto index = this->find_index(res_id))
                return textures_.at(index.value());

            return nullptr;
        }

        std::shared_ptr<mirinae::ITexture> missing_tex() override {
            return missing_tex_;
        }

        std::unique_ptr<mirinae::ITexture> create_image(
            const std::string& id, const dal::IImage2D& image, bool srgb
        ) override {
            if (auto img = image.as<dal::TDataImage2D<uint8_t>>()) {
                auto output = std::make_unique<TextureData>(device_);
                output->init_iimage2d(id, *img, nullptr, srgb, cmd_pool_);
                return output;
            } else {
                SPDLOG_ERROR("Unsupported image type: {}", id);
                return nullptr;
            }
        }

    private:
        std::optional<size_t> find_index(const dal::path& id) {
            for (size_t i = 0; i < textures_.size(); ++i) {
                if (textures_.at(i)->id() == id.u8string())
                    return i;
            }
            return std::nullopt;
        }

        void destroy_all() {
            missing_tex_.reset();

            for (auto& tex : textures_) {
                if (tex.use_count() > 1)
                    SPDLOG_WARN(
                        "Want to destroy texture '{}' is still in use",
                        tex->id()
                    );
                tex->destroy();
            }
            textures_.clear();
        }

        // std::shared_ptr<dal::IResourceManager> res_mgr_;
        mirinae::VulkanDevice& device_;
        mirinae::CommandPool cmd_pool_;
        ImageFactory img_factory_;
        LoadTaskManager loader_mgr_;
        KtxDeviceInfo ktx_device_;
        std::vector<std::shared_ptr<ITextureData>> textures_;
        std::shared_ptr<mirinae::ITexture> missing_tex_;
    };


}  // namespace


namespace mirinae {

    void record_img_buf_copy_mip(
        const VkCommandBuffer cmdbuf,
        const uint32_t width,
        const uint32_t height,
        const uint32_t mip_levels,
        const VkImage dst_image,
        const VkBuffer src_buffer
    ) {
        mirinae::ImageMemoryBarrier barrier;
        barrier.image(dst_image)
            .set_src_access(0)
            .set_dst_access(VK_ACCESS_TRANSFER_WRITE_BIT)
            .old_layout(VK_IMAGE_LAYOUT_UNDEFINED)
            .new_layout(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            .set_aspect_mask(VK_IMAGE_ASPECT_COLOR_BIT)
            .mip_base(0)
            .mip_count(mip_levels)
            .layer_base(0)
            .layer_count(1);
        barrier.record_single(
            cmdbuf,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT
        );

        ::copy_buffer_to_image(cmdbuf, dst_image, src_buffer, width, height, 0);
        ::generate_mipmaps(cmdbuf, dst_image, width, height, mip_levels);
    }


    std::unique_ptr<ITexture> create_tex_depth(
        uint32_t width, uint32_t height, VulkanDevice& device
    ) {
        auto output = std::make_unique<TextureData>(device);
        output->init_depth(width, height);
        return output;
    }


    std::shared_ptr<ITextureManager> create_tex_mgr(
        sung::HTaskSche task_sche, VulkanDevice& device
    ) {
        return std::make_shared<::TextureManager>(task_sche, device);
    }

}  // namespace mirinae
