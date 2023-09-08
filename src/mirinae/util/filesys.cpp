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

    std::unique_ptr<IImage2D> load_image(const char* const path) {
        int width, height, channels;

        if (stbi_is_hdr(path)) {
            const auto data = stbi_loadf(path, &width, &height, &channels, STBI_rgb_alpha);
            auto image = std::make_unique<TImage2D<float>>();
            image->init(data, width, height, 4);
            stbi_image_free(data);
            return image;
        }
        else {
            const auto data = stbi_load(path, &width, &height, &channels, STBI_rgb_alpha);
            auto image = std::make_unique<TImage2D<uint8_t>>();
            image->init(data, width, height, 4);
            stbi_image_free(data);
            return image;
        }
    }

}
