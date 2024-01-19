#include "mirinae/platform/filesys.hpp"

#include <filesystem>
#include <fstream>
#include <optional>


// FilesysStd
namespace {

    std::filesystem::path get_first_dir_of_path(const std::filesystem::path& path) {
        auto it = path.begin();
        return *it;
    }

    std::optional<std::filesystem::path> find_asset_folder() {
        std::filesystem::path cur_path = ".";

        for (int i = 0; i < 10; ++i) {
            const auto folder_path = cur_path / "asset";
            if (std::filesystem::is_directory(folder_path)) {
                return folder_path;
            }
            else {
                cur_path /= "..";
            }
        }

        return std::nullopt;
    }


    class FilesysStd : public mirinae::IFilesys {

    public:
        FilesysStd(const mirinae::respath_t& resources_dir)
            : resources_dir_{ resources_dir }
        {

        }

        bool read_file_to_vector(const mirinae::respath_t& res_path, std::vector<uint8_t>& output) override {
            using namespace std::string_literals;

            const auto full_path = this->resolve_res_path(res_path);
            if (!full_path.has_value())
                return false;

            std::ifstream file{ full_path.value(), std::ios::ate | std::ios::binary | std::ios::in };
            if (!file.is_open())
                return false;

            const auto file_size = static_cast<size_t>(file.tellg());
            output.resize(file_size);

            file.seekg(0);
            file.read(reinterpret_cast<char*>(output.data()), output.size());
            file.close();
            return true;
        }

        std::optional<mirinae::respath_t> resolve_res_path(const mirinae::respath_t& res_path) const {
            if (::get_first_dir_of_path(res_path).u8string() == "asset") {
                std::filesystem::path path = res_path;
                auto it = path.begin(); ++it;
                std::filesystem::path output = ::find_asset_folder().value();
                while (it != path.end()) {
                    output /= *it;
                    ++it;
                }
                return output;
            }
            else {
                return (resources_dir_ / res_path);
            }
        }

    private:
        std::filesystem::path resources_dir_;

    };

}


namespace mirinae {

    respath_t replace_file_name_ext(const respath_t& res_path, const respath_t& new_file_name_ext) {
        auto output = res_path;
        output.replace_filename(new_file_name_ext);
        return output;
    }

}


namespace mirinae {

    std::unique_ptr<IFilesys> create_filesys_std(const respath_t& resources_dir) {
        return std::make_unique<FilesysStd>(resources_dir);
    }

}
