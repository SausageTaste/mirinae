#include "filesys.hpp"


namespace {

    std::filesystem::path get_first_dir_of_path(const std::filesystem::path& path) {
        auto it = path.begin();
        return *it;
    }


    class AssetFile {

    private:
        AAsset* m_asset = nullptr;
        size_t m_file_size = 0;

    public:
        AssetFile() = default;

        ~AssetFile() {
            this->close();
        }

        bool open(const char* const path, AAssetManager* const asset_mgr) noexcept {
            this->close();

            this->m_asset = AAssetManager_open(static_cast<AAssetManager*>(asset_mgr), path, AASSET_MODE_UNKNOWN);
            if (!this->is_ready())
                return false;

            this->m_file_size = static_cast<size_t>(AAsset_getLength64(this->m_asset));
            return true;
        }

        void close() {
            if (nullptr != this->m_asset)
                AAsset_close(this->m_asset);

            this->m_asset = nullptr;
            this->m_file_size = 0;
        }

        [[nodiscard]]
        bool is_ready() const noexcept {
            return nullptr != this->m_asset;
        }

        [[nodiscard]]
        size_t tell() const {
            const auto cur_pos = AAsset_getRemainingLength(this->m_asset);
            return this->m_file_size - static_cast<size_t>(cur_pos);
        }

        [[nodiscard]]
        size_t size() const {
            return this->m_file_size;
        }

        bool read(void* const dst, const size_t dst_size) {
            // Android asset manager implicitly read beyond file range WTF!!!
            const auto remaining = this->m_file_size - this->tell();
            const auto size_to_read = dst_size < remaining ? dst_size : remaining;
            if (size_to_read <= 0)
                return false;

            const auto read_bytes = AAsset_read(this->m_asset, dst, size_to_read);
            return read_bytes > 0;
        }

    };


    class FilesysAndroidAsset : public mirinae::IFilesys {

    public:
        FilesysAndroidAsset(AAssetManager* mgr)
            : mgr_(mgr)
        {

        }

        bool read_file_to_vector(const mirinae::respath_t& file_path, std::vector<uint8_t>& output) override {
            mirinae::respath_t new_path;
            if (::get_first_dir_of_path(file_path) == "asset") {
                bool skipped_once = false;
                for (auto element : file_path) {
                    if (skipped_once)
                        new_path /= element;
                    else
                        skipped_once = true;
                }
            }
            else {
                new_path = file_path;
            }

            AssetFile file;
            if (!file.open(new_path.c_str(), mgr_))
                return false;

            output.resize(file.size());
            file.read(output.data(), output.size());
            return true;
        }

    private:
        AAssetManager* mgr_ = nullptr;

    };

}


namespace mirinapp {

    std::unique_ptr<mirinae::IFilesys> create_filesys_android_asset(AAssetManager* mgr) {
        return std::make_unique<::FilesysAndroidAsset>(mgr);
    }

}