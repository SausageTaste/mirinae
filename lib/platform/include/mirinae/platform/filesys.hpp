#pragma once

#include <memory>
#include <vector>
#include <optional>


namespace mirinae {

    class IFilesys {

    public:
        virtual ~IFilesys() = default;

        virtual bool read_file_to_vector(const char* file_path, std::vector<uint8_t>& output) = 0;

        std::optional<std::vector<uint8_t>> read_file_to_vector(const char* file_path) {
            std::vector<uint8_t> output;
            if (this->read_file_to_vector(file_path, output)) {
                return output;
            }
            else {
                return std::nullopt;
            }
        }

    };


    std::unique_ptr<IFilesys> create_filesys_std();

}
