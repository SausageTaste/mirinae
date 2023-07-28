#include "mirinae/util/filesys.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
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

    Image2D load_image(const char* const path) {
        Image2D image;
        int width, height, channels;
        const auto data = stbi_load(path, &width, &height, &channels, 0);
        image.init(data, width, height, channels, 1);
        stbi_image_free(data);
        return image;
    }

}
