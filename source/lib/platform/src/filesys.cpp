#include "mirinae/platform/filesys.hpp"

#include <filesystem>
#include <fstream>
#include <optional>


// FilesysStd
namespace {

    std::optional<std::filesystem::path> find_resources_folder() {
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
        bool read_file_to_vector(const char* file_path, std::vector<uint8_t>& output) override {
            using namespace std::string_literals;

            const auto full_path = find_resources_folder().value_or("."s) / file_path;
            std::ifstream file{ full_path, std::ios::ate | std::ios::binary | std::ios::in };

            if (!file.is_open())
                return false;

            const auto file_size = static_cast<size_t>(file.tellg());
            output.resize(file_size);

            file.seekg(0);
            file.read(reinterpret_cast<char*>(output.data()), output.size());
            file.close();
            return true;
        }

    };

}


namespace mirinae {

    std::unique_ptr<IFilesys> create_filesys_std() {
        return std::make_unique<FilesysStd>();
    }

}
