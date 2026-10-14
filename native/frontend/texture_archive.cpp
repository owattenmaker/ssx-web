#include "texture_archive.hpp"
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace ssx_native {

TextureArchive::TextureArchive(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw std::runtime_error("cannot read " + path);
    }
    bytes_.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (bytes_.size() < 12 || std::memcmp(bytes_.data(), "SSXTEX01", 8) != 0) {
        throw std::runtime_error("not a texture archive: " + path);
    }
    uint32_t n;
    std::memcpy(&n, bytes_.data() + 8, 4);
    const auto index = nlohmann::json::parse(std::string(reinterpret_cast<const char*>(bytes_.data()) + 12, n));
    payload_ = (12 + size_t(n) + 15) / 16 * 16;
    for (const auto& e : index["entries"]) {
        entries_[key(e["id"])] = {e["offset"].get<size_t>(), e["size"].get<size_t>()};
    }
}

TextureArchive::Bytes TextureArchive::raw(const nlohmann::json& id) const {
    auto it = entries_.find(key(id));
    if (it == entries_.end()) {
        throw std::runtime_error("texture " + key(id) + " not in the archive");
    }
    if (payload_ + it->second.offset + it->second.size > bytes_.size()) {
        throw std::runtime_error("texture " + key(id) + " past the end of the archive");
    }
    return {bytes_.data() + payload_ + it->second.offset, it->second.size};
}

Image TextureArchive::decode(const nlohmann::json& id) const {
    const Bytes b = raw(id);
    return decodePng(b.data, b.size);
}

}  // namespace ssx_native
