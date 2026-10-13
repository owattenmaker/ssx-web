#pragma once
// A portable run replay (web/server/replay-file.mjs, docs/online-records.md "The replay file") and its pad stream
// (web/replay.js createRecording), read natively: the same bytes, the same per-tick pad values bit for bit.
//   bytes: 'SSXR', u8 version (1), u32 meta length, meta (UTF-8 JSON), u32 pad length, pad stream
//   pad record: varint tick delta, 3-byte mask of the changed channels, 3-byte mask of those kept as float32, then a byte
//   per changed channel (0 / 1 for channels 0..3, else BYTE_VALUE[b]) or 4 bytes for a float one
#include "json.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace ssx_native {

struct ReplayFile {
    nlohmann::ordered_json meta;
    std::vector<uint8_t> pad;
};

inline uint32_t readU32(const uint8_t* p) {
    return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
}

// decodeReplayFile: a raw SSXR file (readReplayFile inflates the deflate-raw upload / storage form first)
inline ReplayFile decodeReplayFile(const std::vector<uint8_t>& b) {
    if (b.size() < 13 || b[0] != 0x53 || b[1] != 0x53 || b[2] != 0x58 || b[3] != 0x52 || b[4] != 1) {
        throw std::runtime_error("not a replay file");
    }
    const uint32_t ml = readU32(b.data() + 5);
    if (uint64_t(9) + ml + 4 > b.size()) {
        throw std::runtime_error("replay file truncated");
    }
    ReplayFile f;
    // as chars: nlohmann's parser over unsigned char needs std::char_traits<unsigned char>, which newer libc++ lacks
    f.meta = nlohmann::ordered_json::parse(std::string(reinterpret_cast<const char*>(b.data()) + 9, ml));
    const uint32_t pl = readU32(b.data() + 9 + ml);
    if (uint64_t(13) + ml + pl != b.size()) {
        throw std::runtime_error("replay file length");
    }
    if (!f.meta.is_object()) {
        throw std::runtime_error("replay meta");
    }
    f.pad.assign(b.begin() + 13 + ml, b.end());
    return f;
}

// A replay file as stored or uploaded: raw SSXR, or its deflate-raw form (stb_image's inflater, native/third_party/stb_impl.cpp)
std::vector<uint8_t> inflateRaw(const std::vector<uint8_t>& in);
inline ReplayFile readReplayFile(const std::vector<uint8_t>& bytes) {
    const bool raw = bytes.size() >= 4 && bytes[0] == 0x53 && bytes[1] == 0x53 && bytes[2] == 0x58 && bytes[3] == 0x52;
    return decodeReplayFile(raw ? bytes : inflateRaw(bytes));
}

// web/replay.js BYTE_VALUE: towardZero(b * R255) with R255 = 0x3b808081 and the product in double (a JS number)
inline const std::array<float, 256>& byteValues() {
    static const std::array<float, 256> table = [] {
        std::array<float, 256> t{};
        uint32_t r255Bits = 0x3b808081u;
        float r255;
        std::memcpy(&r255, &r255Bits, 4);
        for (int b = 0; b < 256; ++b) {
            const double x = double(b) * double(r255);
            float f = float(x);
            if (std::abs(double(f)) > std::abs(x)) {
                uint32_t bits;
                std::memcpy(&bits, &f, 4);
                bits -= 1;
                std::memcpy(&f, &bits, 4);
            }
            t[size_t(b)] = f;
        }
        return t;
    }();
    return table;
}

// web/replay.js recording.pad(t, out, cursor) for sequential reads (t = 0, 1, 2 ...); starts over when t goes back
class PadReader {
public:
    explicit PadReader(const std::vector<uint8_t>& stream) : buf_(stream) {
        reset();
    }
    void reset() {
        pos_ = 0;
        at_ = 0;
        base_ = 0;
        next_ = -1;
        started_ = false;
        pad_.fill(0.f);
    }
    // the pad of tick t into out (24 floats)
    void read(int64_t t, float* out) {
        if (!started_ || t < at_) {
            reset();
            started_ = true;
        }
        const auto& values = byteValues();
        for (;;) {
            if (next_ < 0) {
                if (pos_ >= buf_.size()) {
                    next_ = INT64_MAX;
                    break;
                }
                // JS: dt |= (b & 0x7F) << sh (32-bit shifts, as the page reads it)
                int32_t dt = 0;
                int sh = 0;
                uint8_t b;
                do {
                    b = buf_.at(pos_++);
                    dt |= int32_t(uint32_t(b & 0x7F) << (sh & 31));
                    sh += 7;
                } while (b & 0x80);
                next_ = base_ + dt;
                base_ = next_;
            }
            if (next_ > t) {
                break;
            }
            const size_t q = pos_;
            const uint32_t mask = uint32_t(buf_.at(q)) | (uint32_t(buf_.at(q + 1)) << 8) | (uint32_t(buf_.at(q + 2)) << 16);
            const uint32_t floats = uint32_t(buf_.at(q + 3)) | (uint32_t(buf_.at(q + 4)) << 8) | (uint32_t(buf_.at(q + 5)) << 16);
            pos_ += 6;
            for (int i = 0; i < 24; ++i) {
                if (!(mask & (1u << i))) {
                    continue;
                }
                if (floats & (1u << i)) {
                    uint8_t bytes[4];
                    for (int k = 0; k < 4; ++k) {
                        bytes[k] = buf_.at(pos_++);
                    }
                    std::memcpy(&pad_[size_t(i)], bytes, 4);
                } else {
                    const uint8_t v = buf_.at(pos_++);
                    pad_[size_t(i)] = i < 4 ? float(v) : values[v];
                }
            }
            next_ = -1;
        }
        at_ = t;
        std::memcpy(out, pad_.data(), sizeof(float) * 24);
    }

private:
    const std::vector<uint8_t>& buf_;
    size_t pos_ = 0;
    int64_t at_ = 0;
    int64_t base_ = 0;
    int64_t next_ = -1;
    bool started_ = false;
    std::array<float, 24> pad_{};
};

}  // namespace ssx_native
