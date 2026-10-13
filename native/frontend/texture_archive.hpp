#pragma once
// The texture archives (docs/asset-formats.md "Texture archives (.tex)", web/texture-archive.js): 'SSXTEX01', u32 index
// length, the JSON index, zero padding to 16, then the entries' PNG files back to back. Each entry decodes to exact RGBA8.
#include "image_io.hpp"
#include "json.hpp"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ssx_native {

class TextureArchive {
public:
    explicit TextureArchive(const std::string& path);
    bool has(int id) const {
        return entries_.count(id) > 0;
    }
    Image decode(int id) const;

private:
    struct Entry {
        size_t offset = 0;
        size_t size = 0;
    };
    std::vector<uint8_t> bytes_;
    size_t payload_ = 0;
    std::map<int, Entry> entries_;
};

}  // namespace ssx_native
