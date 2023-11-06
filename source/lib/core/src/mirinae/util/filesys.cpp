#include "mirinae/util/filesys.hpp"

#include <spdlog/spdlog.h>


namespace mirinae {

    std::optional<std::filesystem::path> find_resources_folder() {
        std::filesystem::path cur_path = ".";

        for (int i = 0; i < 10; ++i) {
            const auto folder_path = cur_path / "resources";
            if (std::filesystem::is_directory(folder_path)) {
                return folder_path;
            }
            else {
                cur_path /= "..";
            }
        }

        spdlog::error("Cannot find resources folder");
        return std::nullopt;
    }

}
