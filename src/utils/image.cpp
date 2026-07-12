#include "image.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "files.hpp"

ImageData LoadImage(const std::string& path)
{
    std::string content = fs::ReadFileAsString(path);

    ImageData image{};

    int width, height, channels;
    unsigned char* data = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(content.data()), content.size(),
                                                &width, &height, &channels, 4);

    if (!data)
    {
        throw std::runtime_error("Failed to load texture from file: " + path);
    }

    image.width = static_cast<uint32_t>(width);
    image.height = static_cast<uint32_t>(height);

    std::span<uint8_t> image_data(data, width * height * 4);
    image.data.assign(image_data.begin(), image_data.end());
    stbi_image_free(data);

    return image;
}
