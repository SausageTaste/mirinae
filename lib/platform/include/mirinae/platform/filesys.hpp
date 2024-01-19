#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <optional>


namespace mirinae {

    using respath_t = std::filesystem::path;

    respath_t replace_file_name_ext(const respath_t& res_path, const respath_t& new_file_name_ext);


    class IFilesys {

    public:
        virtual ~IFilesys() = default;

        virtual bool read_file_to_vector(const respath_t& res_path, std::vector<uint8_t>& output) = 0;

        std::optional<std::vector<uint8_t>> read_file_to_vector(const respath_t& file_path) {
            std::vector<uint8_t> output;
            if (this->read_file_to_vector(file_path, output)) {
                return output;
            }
            else {
                return std::nullopt;
            }
        }

    };


    std::unique_ptr<IFilesys> create_filesys_std(const respath_t& resources_dir);

}
