#pragma once
// PNG in and out for the front end (stb_image / stb_image_write, native/third_party/stb): decoded to exact RGBA8, no colour
// management (as web/png-texels.js decodes the exported textures).
#include <cstdint>
#include <string>
#include <vector>

namespace ssx_native {

struct Image {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgba;
};
Image decodePng(const uint8_t* data, size_t size);
Image readPng(const std::string& path);
void writePng(const std::string& path, int width, int height, const uint8_t* rgba);

}  // namespace ssx_native
