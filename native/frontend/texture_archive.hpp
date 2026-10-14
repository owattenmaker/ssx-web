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
    // ids are numbers (the world library) or names (the wardrobe archives): keyed by their text
    static std::string key(const nlohmann::json& id) {
        return id.is_string() ? id.get<std::string>() : id.dump();
    }
    bool has(const nlohmann::json& id) const {
        return entries_.count(key(id)) > 0;
    }
    Image decode(const nlohmann::json& id) const;
    // an entry's stored bytes (a PNG file, or a BC entry of the Xbox HD rider set: frontend/bc_texels.hpp)
    struct Bytes {
        const uint8_t* data = nullptr;
        size_t size = 0;
    };
    Bytes raw(const nlohmann::json& id) const;

private:
    struct Entry {
        size_t offset = 0;
        size_t size = 0;
    };
    std::vector<uint8_t> bytes_;
    size_t payload_ = 0;
    std::map<std::string, Entry> entries_;
};

}  // namespace ssx_native
