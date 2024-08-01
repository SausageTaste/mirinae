#include "filesys.hpp"

#include <optional>


namespace {

    namespace fs = std::filesystem;


    fs::path get_first_dir_of_path(const fs::path &path) {
        auto it = path.begin();
        return *it;
    }


    class AssetFile {

    private:
        AAsset *m_asset = nullptr;
        size_t m_file_size = 0;

    public:
        AssetFile() = default;

        ~AssetFile() {
            this->close();
        }

        bool open(const char *const path, AAssetManager *const asset_mgr) noexcept {
            this->close();

            this->m_asset = AAssetManager_open(static_cast<AAssetManager *>(asset_mgr), path,
                                               AASSET_MODE_UNKNOWN);
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

        int read(void *const dst, const size_t dst_size) {
            // Android asset manager implicitly read beyond file range WTF!!!
            const auto remaining = this->m_file_size - this->tell();
            const auto size_to_read = dst_size < remaining ? dst_size : remaining;
            if (size_to_read <= 0)
                return false;

            return AAsset_read(this->m_asset, dst, size_to_read);
        }

        std::optional<std::vector<uint8_t>> read() {
            std::vector<uint8_t> out(m_file_size);
            if (m_file_size == this->read(out.data(), out.size()))
                return out;
            else
                return std::nullopt;
        }

    };


    class FilesubsysAndroidAsset : public dal::IFileSubsys {

    public:
        FilesubsysAndroidAsset(AAssetManager *mgr, dal::BundleRepository &bundles)
                : mgr_(mgr), bundles_(bundles) {

        }

        bool is_file(const fs::path &path) final {
            return false;
        }

        std::vector<fs::path> list_files(const fs::path &path) final {
            return {};
        }

        std::vector<fs::path> list_folders(const fs::path &path) final {
            return {};
        }

        size_t read_file(const fs::path &path, uint8_t *buf, size_t buf_size) final {
            const auto raw_path = this->make_raw_path(path);
            if (!raw_path.has_value())
                return false;

            AssetFile file;
            if (!file.open(raw_path->c_str(), mgr_))
                return false;

            return file.read(reinterpret_cast<char *>(buf), buf_size);
        }

        bool read_file(const fs::path &i_path, bindata_t &out) final {
            const auto raw_path = this->make_raw_path(i_path);
            if (!raw_path.has_value())
                return false;

            AssetFile file;
            if (file.open(raw_path->c_str(), mgr_)) {
                const auto file_size = file.size();
                out.resize(file_size);
                file.read(out.data(), out.size());
                return true;
            }

            const auto parent_path = raw_path->parent_path();

            {
                const auto bundle_file_data = bundles_.get_file_data(
                        parent_path.u8string(), raw_path->filename().u8string()
                );
                if (nullptr != bundle_file_data.first) {
                    out.assign(
                            bundle_file_data.first,
                            bundle_file_data.first + bundle_file_data.second
                    );
                    return true;
                }
            }

            std::vector<uint8_t> file_content;
            {
                file.open(parent_path.c_str(), mgr_);
                if (!file.is_ready())
                    return false;

                file_content.resize(file.size());
                file.read(file_content.data(), file_content.size());
            }

            if (!bundles_.notify(parent_path.u8string(), file_content))
                return false;

            {
                const auto bundle_file_data = bundles_.get_file_data(
                        parent_path.u8string(), raw_path->filename().u8string()
                );
                if (nullptr != bundle_file_data.first) {
                    out.assign(
                            bundle_file_data.first,
                            bundle_file_data.first + bundle_file_data.second
                    );
                    return true;
                }
            }

            return false;
        }

    private:
        std::optional<fs::path> make_raw_path(const fs::path &i_path) const {
            const auto prefix_str = prefix_.u8string();
            const auto interf_path_str = i_path.u8string();
            const auto prefix_loc = interf_path_str.find(prefix_str);
            if (prefix_loc != 0) {
                return std::nullopt;
            } else {
                auto suffix = interf_path_str.substr(prefix_str.size());
                if (suffix[0] == '/')
                    suffix = suffix.substr(1);
                return suffix;
            }
        }

        [[nodiscard]]
        fs::path make_i_path(const fs::path &raw_path) const {
            return prefix_ / raw_path;
        }

        fs::path prefix_ = ":asset";
        AAssetManager *mgr_ = nullptr;
        dal::BundleRepository &bundles_;

    };

}


namespace mirinapp {

    std::unique_ptr<dal::IFileSubsys> create_filesubsys_android_asset(
            AAssetManager *mgr, dal::Filesystem &filesys
    ) {
        return std::make_unique<::FilesubsysAndroidAsset>(mgr, filesys.bundle_repo());
    }

}
