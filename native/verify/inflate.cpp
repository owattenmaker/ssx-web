#include "replay_file.hpp"
#include "stb/stb_image.h"
#include <cstdlib>

namespace ssx_native {

std::vector<uint8_t> inflateRaw(const std::vector<uint8_t>& in) {
    int n = 0;
    char* out = stbi_zlib_decode_noheader_malloc(reinterpret_cast<const char*>(in.data()), int(in.size()), &n);
    if (!out) {
        throw std::runtime_error("not a replay file (neither SSXR nor deflate-raw)");
    }
    std::vector<uint8_t> bytes(out, out + n);
    std::free(out);
    return bytes;
}

}  // namespace ssx_native
