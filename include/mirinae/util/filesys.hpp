#pragma once

#include <filesystem>
#include <fstream>
#include <optional>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "mirinae/util/image.hpp"


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

        return std::nullopt;
    }


    template <typename T>
    std::optional<T> load_file(const char* const path) {
        using namespace std::string_literals;

        std::ifstream file{ path, std::ios::ate | std::ios::binary | std::ios::in };

        if (!file.is_open())
            return std::nullopt;

        const auto file_size = static_cast<size_t>(file.tellg());
        T buffer;
        buffer.resize(file_size);

        file.seekg(0);
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        file.close();

        return buffer;
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
