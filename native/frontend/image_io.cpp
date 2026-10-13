#include "image_io.hpp"
#include <fstream>
#include <stdexcept>
#include "stb/stb_image.h"
#include "stb/stb_image_write.h"

namespace ssx_native {

Image decodePng(const uint8_t* data, size_t size) {
    Image img;
    int channels = 0;
    unsigned char* p = stbi_load_from_memory(data, int(size), &img.width, &img.height, &channels, 4);
    if (!p) {
        throw std::runtime_error(std::string("PNG decode: ") + stbi_failure_reason());
    }
    img.rgba.assign(p, p + size_t(img.width) * img.height * 4);
    stbi_image_free(p);
    return img;
}

Image readPng(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw std::runtime_error("cannot read " + path);
    }
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return decodePng(bytes.data(), bytes.size());
}

void writePng(const std::string& path, int width, int height, const uint8_t* rgba) {
    if (!stbi_write_png(path.c_str(), width, height, 4, rgba, width * 4)) {
        throw std::runtime_error("cannot write " + path);
    }
}

}  // namespace ssx_native
