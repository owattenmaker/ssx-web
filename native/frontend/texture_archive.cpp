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
        entries_[e["id"].get<int>()] = {e["offset"].get<size_t>(), e["size"].get<size_t>()};
    }
}

Image TextureArchive::decode(int id) const {
    auto it = entries_.find(id);
    if (it == entries_.end()) {
        throw std::runtime_error("texture " + std::to_string(id) + " not in the archive");
    }
    return decodePng(bytes_.data() + payload_ + it->second.offset, it->second.size);
}

}  // namespace ssx_native
