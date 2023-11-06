#pragma once

#include <memory>
#include <vector>


namespace mirinae {

    class IFilesys {

    public:
        virtual ~IFilesys() = default;

        virtual bool read_file_to_vector(const char* file_path, std::vector<uint8_t>& output) = 0;

    };


    std::unique_ptr<IFilesys> create_filesys_std();

}
