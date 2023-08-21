#pragma once

#include <filesystem>
#include <fstream>
#include <optional>

#include "mirinae/util/image.hpp"


namespace mirinae {

    std::optional<std::filesystem::path> find_resources_folder();


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

    template <typename T>
    std::optional<T> load_file(const std::filesystem::path& path) {
        return load_file<T>(path.u8string().c_str());
    }

    std::unique_ptr<IImage2D> load_image(const char* const path);

}
