#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <string>
#include <span>

struct ImageData
{
    uint32_t width;
    uint32_t height;
    std::vector<uint8_t> data;
};

ImageData LoadImage(const std::string& path);
