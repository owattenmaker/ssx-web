#pragma once
// SSX 3 (PS2) audio decoders natively (web/audio-decode.js, 1:1; docs/audio-formats.md): the PT tag header, EA SCxl streams,
// BNKl v5 banks, EA-XA, PS-ADPCM (VAG), signed 8-bit PCM and EA MicroTalk (UTK, the EE FPU arithmetic). Everything decodes to
// float PCM in [-1, 1). The JS works in doubles: the integer codecs here wrap through int32 where the JS's `>>` does (ToInt32),
// the MicroTalk FPU helpers are the JS's own (eeMul / accAdd / guardAddSub on float32 values held in doubles).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace ssx_native {

namespace audio {

enum Codec : int { kMicroTalk = 0x04, kVag = 0x05, kPcm16be = 0x07, kPcm16le = 0x08, kPcm8 = 0x09, kEaxa = 0x0a };

// a decoded sound: planar channels
struct Decoded {
    int sampleRate = 22050;
    int channels = 1;
    size_t length = 0;
    std::vector<std::vector<float>> data;
    int64_t loopStart = -1;
    int64_t loopEnd = -1;
};

namespace detail {

inline uint32_t bitsOf(float x) {
    uint32_t b;
    std::memcpy(&b, &x, 4);
    return b;
}
inline float fromBits(uint32_t b) {
    float x;
    std::memcpy(&x, &b, 4);
    return x;
}
inline double fround(double x) {
    return double(float(x));
}
constexpr double kTwoM24 = 5.9604644775390625e-8;
constexpr double kMinNormal = 1.1754943508222875e-38;
// the float32 next to r toward zero (r a nonzero float32)
inline double towardZero(double r) {
    if (r > kMinNormal || r < -kMinNormal) {
        if (r == INFINITY) {
            return 3.4028234663852886e38;
        }
        if (r == -INFINITY) {
            return -3.4028234663852886e38;
        }
        return fround(r - r * kTwoM24);
    }
    return double(fromBits(bitsOf(float(r)) - 1));
}
inline double eeMul(double a, double b) {
    const double p = a * b;
    const double r = fround(p);
    return r == p || r == 0 ? r : (r > 0 ? r > p : r < p) ? towardZero(r) : r;
}
inline double accAdd(double a, double b) {
    const double s = a + b;
    const double r = fround(s);
    if (r != s) {
        return r != 0 && (r > 0 ? r > s : r < s) ? towardZero(r) : r;
    }
    const double bb = s - a;
    const double err = (a - (s - bb)) + (b - bb);
    return err != 0 && r != 0 && (err < 0) == (r > 0) ? towardZero(r) : r;
}
inline double guardAddSub(double a, double b, bool subtract) {
    uint32_t x = bitsOf(float(a));
    uint32_t y = bitsOf(float(b));
    const int d = int((x >> 23) & 255) - int((y >> 23) & 255);
    if (d >= 25) {
        y &= 0x80000000u;
    } else if (d <= -25) {
        x &= 0x80000000u;
    } else if (d > 0) {
        y &= (0xffffffffu << (d - 1));
    } else if (d < 0) {
        x &= (0xffffffffu << (-d - 1));
    }
    const double fa = double(fromBits(x));
    const double fb = double(fromBits(y));
    const double e = subtract ? fa - fb : fa + fb;
    const double r = fround(e);
    return r == e || r == 0 ? r : (r > 0 ? r > e : r < e) ? towardZero(r) : r;
}
inline double eeAdd(double a, double b) {
    return guardAddSub(a, b, false);
}
inline double eeSub(double a, double b) {
    return guardAddSub(a, b, true);
}
inline double f32bits(uint32_t b) {
    return double(fromBits(b));
}

inline uint32_t u16le(const uint8_t* u8, size_t p) {
    return uint32_t(u8[p]) | (uint32_t(u8[p + 1]) << 8);
}
inline uint32_t u32le(const uint8_t* u8, size_t p) {
    return uint32_t(u8[p]) | (uint32_t(u8[p + 1]) << 8) | (uint32_t(u8[p + 2]) << 16) | (uint32_t(u8[p + 3]) << 24);
}
inline int32_t s16be(const uint8_t* u8, size_t p) {
    const int32_t v = (int32_t(u8[p]) << 8) | int32_t(u8[p + 1]);
    return (v & 0x8000) ? v - 0x10000 : v;
}
inline std::string fourcc(const uint8_t* u8, size_t p) {
    return std::string(reinterpret_cast<const char*>(u8 + p), 4);
}
// JS ToInt32 of an integer-valued double
inline int32_t wrap32(int64_t v) {
    return int32_t(uint32_t(uint64_t(v)));
}

}  // namespace detail

// one PT tag value: an integer (len <= 4) or bytes
struct Tag {
    double value = 0;
    std::vector<uint8_t> bytes;
    bool isBytes = false;
};
struct PtPatch {
    std::map<int, Tag> params;
    std::map<int, Tag> sample;
};
struct Pt {
    int platform = 0;
    std::vector<PtPatch> patches;
    size_t end = 0;
};

// EE 0x3C7388
inline Pt parsePT(const uint8_t* u8, size_t size, size_t pos) {
    if (pos + 4 > size || u8[pos] != 0x50 || u8[pos + 1] != 0x54) {
        throw std::runtime_error("PT header expected at " + std::to_string(pos));
    }
    Pt pt;
    pt.platform = u8[pos + 2];
    size_t p = pos + 4;
    PtPatch cur;
    bool inSample = false;
    for (;;) {
        if (p >= size) {
            throw std::runtime_error("PT header runs past the buffer");
        }
        const int tag = u8[p++];
        if (tag == 0xfc) {
            continue;
        }
        if (tag == 0xff) {
            break;
        }
        if (tag == 0xfd) {
            inSample = true;
            continue;
        }
        if (tag == 0xfe) {
            pt.patches.push_back(cur);
            cur = PtPatch{};
            inSample = false;
            continue;
        }
        uint32_t len = u8[p++];
        if (len == 0xff) {
            len = (uint32_t(u8[p]) << 24) | (uint32_t(u8[p + 1]) << 16) | (uint32_t(u8[p + 2]) << 8) | uint32_t(u8[p + 3]);
            p += 4;
        }
        Tag t;
        if (len <= 4) {
            double v = 0;
            for (uint32_t i = 0; i < len; ++i) {
                v = v * 256 + u8[p + i];
            }
            t.value = v;
        } else {
            t.isBytes = true;
            t.bytes.assign(u8 + p, u8 + std::min<size_t>(size, p + len));
        }
        p += len;
        (inSample || tag >= 0x80 ? cur.sample : cur.params)[tag] = t;
    }
    pt.patches.push_back(cur);
    pt.end = p;
    return pt;
}

struct SampleInfo {
    int version = 0;
    int channels = 1;
    int sampleRate = 22050;
    int64_t sampleCount = 0;
    int64_t loopStart = -1;
    int64_t loopEnd = -1;
    int codec = kVag;
    std::vector<int64_t> dataOffsets;
    std::map<int, Tag> params;
};
inline SampleInfo sampleInfo(const std::map<int, Tag>& sample) {
    auto get = [&](int t, double d) {
        auto it = sample.find(t);
        return it == sample.end() ? d : it->second.value;
    };
    SampleInfo s;
    s.version = int(get(0x80, 0));
    s.channels = int(get(0x82, 1));
    s.sampleRate = int(get(0x84, 22050));
    s.sampleCount = int64_t(get(0x85, 0));
    s.loopStart = sample.count(0x86) ? int64_t(sample.at(0x86).value) : -1;
    s.loopEnd = sample.count(0x87) ? int64_t(sample.at(0x87).value) : -1;
    s.codec = int(get(0xa0, kVag));
    for (int t = 0x88; t <= 0x8b; ++t) {
        if (t != 0x8a && sample.count(t)) {
            s.dataOffsets.push_back(int64_t(sample.at(t).value));
        }
    }
    return s;
}

struct History {
    int32_t h1 = 0;
    int32_t h2 = 0;
};

// EA-XA (SNDDRV.IRX decxa16c 0x9BA0): 15-byte frames of 28 samples, exact integer math, no history clamp
inline size_t decodeEAXA(const uint8_t* u8, size_t pos, size_t count, History& state, float* out, size_t stride = 1) {
    static const int32_t c1s[4] = {0, 240, 460, 392};
    static const int32_t c2s[4] = {0, 0, -208, -220};
    int32_t h1 = state.h1;
    int32_t h2 = state.h2;
    size_t o = 0;
    size_t left = count;
    while (left > 0) {
        const uint8_t hdr = u8[pos];
        const int32_t c1 = c1s[(hdr >> 4) & 3];
        const int32_t c2 = c2s[(hdr >> 4) & 3];
        const int shift = (hdr & 0x0f) + 8;
        const size_t n = left < 28 ? left : 28;
        for (size_t i = 0; i < n; ++i) {
            const uint8_t byte = u8[pos + 1 + (i >> 1)];
            const int32_t nib = (i & 1) ? byte & 0x0f : byte >> 4;
            // ((nib << 28) >> shift): int32; the sum in double, ToInt32, >> 8
            const int32_t a = detail::wrap32(int64_t(nib) << 28) >> shift;
            const int32_t s = detail::wrap32(int64_t(a) + int64_t(h1) * c1 + int64_t(h2) * c2) >> 8;
            h2 = h1;
            h1 = s;
            out[o] = float(double(s > 32767 ? 32767 : s < -32768 ? -32768 : s) / 32768);
            o += stride;
        }
        left -= n;
        pos += 15;
    }
    state.h1 = h1;
    state.h2 = h2;
    return pos;
}

// PS-ADPCM (SPU VAG): 16-byte frames of 28 samples
inline size_t decodePSADPCM(const uint8_t* u8, size_t pos, size_t count, History& state, float* out, size_t stride = 1) {
    static const int32_t f0s[5] = {0, 60, 115, 98, 122};
    static const int32_t f1s[5] = {0, 0, -52, -55, -60};
    int32_t h1 = state.h1;
    int32_t h2 = state.h2;
    size_t o = 0;
    size_t left = count;
    while (left > 0) {
        const uint8_t hdr = u8[pos];
        int filter = hdr >> 4;
        if (filter > 4) {
            filter = 0;
        }
        const int shift = hdr & 0x0f;
        const int32_t f0 = f0s[filter];
        const int32_t f1 = f1s[filter];
        const size_t n = left < 28 ? left : 28;
        for (size_t i = 0; i < n; ++i) {
            const uint8_t byte = u8[pos + 2 + (i >> 1)];
            const int32_t nib = (i & 1) ? byte >> 4 : byte & 0x0f;
            int32_t s = (detail::wrap32(int64_t(nib) << 28) >> 16) >> shift;
            s += detail::wrap32(int64_t(h1) * f0 + int64_t(h2) * f1 + 32) >> 6;
            if (s > 32767) {
                s = 32767;
            } else if (s < -32768) {
                s = -32768;
            }
            h2 = h1;
            h1 = s;
            out[o] = float(double(s) / 32768);
            o += stride;
        }
        left -= n;
        pos += 16;
    }
    state.h1 = h1;
    state.h2 = h2;
    return pos;
}

inline size_t decodePCM8(const uint8_t* u8, size_t pos, size_t count, float* out, size_t stride = 1) {
    for (size_t i = 0; i < count; ++i) {
        out[i * stride] = float(double(int8_t(u8[pos + i])) / 128);
    }
    return pos + count;
}

// EA MicroTalk (UTK; EE 0x3CD6F0 / 0x3CD878 / 0x3CD2B0 / 0x3CD590 / 0x3CE410 / 0x3CDE68)
class UtkDecoder {
public:
    UtkDecoder(const uint8_t* u8, size_t size, size_t pos, bool forceReducedBw = false, bool plain = false)
        : u8_(u8), size_(size), forceReducedBw_(forceReducedBw), plain_(plain) {
        open(pos);
        parseHeader();
    }
    bool lost = false;
    size_t ptr = 0;

    void open(size_t pos) {
        if (plain_) {
            pcmPatch_ = false;
            initBits(pos);
            return;
        }
        pcmPatch_ = at(pos) == 0xee;
        initBits(pos + 1);
    }
    // one 432-sample frame (into mem_[324..756))
    const double* decodeFrame() {
        using namespace detail;
        double delta[12] = {};
        int idx = int(readBits(6));
        delta[0] = eeMul(eeSub(rcTable()[size_t(idx)], rc_[0]), 0.25);
        const bool multipulse = idx < threshold_;
        for (int i = 1; i < 4; ++i) {
            idx = int(readBits(6));
            delta[i] = eeMul(eeSub(rcTable()[size_t(idx)], rc_[size_t(i)]), 0.25);
        }
        for (int i = 4; i < 12; ++i) {
            idx = int(readBits(5));
            delta[i] = eeMul(eeSub(rcTable()[size_t(idx + 16)], rc_[size_t(i)]), 0.25);
        }
        double* m = mem_;
        double* e = exc_;
        static const double kGain115 = f32bits(0x3d888889);
        static const double kInterp[3] = {f32bits(0x3f18ee49), f32bits(0xbdeaaefb), f32bits(0x3c93b945)};
        for (int sf = 0; sf < 4; ++sf) {
            const int lag = int(readBits(8));
            const double pitchGain = eeMul(double(readBits(4)), kGain115);
            double fixedGain = fixedGains_[readBits(6)];
            if (!reducedBw_) {
                excitation(multipulse, 5, 1);
            } else {
                const int align = int(readBits(1));
                const int zero = int(readBits(1));
                excitation(multipulse, 5 + align, 2);
                if (zero) {
                    for (int k = 1 - align; k < 108; k += 2) {
                        e[5 + k] = 0;
                    }
                } else {
                    for (int k = 0; k < 5; ++k) {
                        e[k] = 0;
                        e[5 + 108 + k] = 0;
                    }
                    for (int k = 1 - align; k < 108; k += 2) {
                        const int q = 5 + k;
                        e[q] = eeAdd(eeAdd(eeMul(eeAdd(e[q - 1], e[q + 1]), kInterp[0]), eeMul(eeAdd(e[q - 3], e[q + 3]), kInterp[1])),
                                     eeMul(eeAdd(e[q - 5], e[q + 5]), kInterp[2]));
                    }
                    fixedGain = eeMul(fixedGain, 0.5);
                }
            }
            const int src = sf * 108 + 216 - lag;
            const int dst = 324 + sf * 108;
            for (int k = 0; k < 108; ++k) {
                const int i = src + k;
                const double past = i >= 0 ? m[i] : i >= -12 ? synth_[12 + i] : i >= -24 ? rc_[24 + i] : fixedGains_[88 + i];
                m[dst + k] = eeAdd(eeMul(pitchGain, past), eeMul(fixedGain, e[5 + k]));
            }
        }
        std::memmove(m, m + 324 + 108, sizeof(double) * 324);
        for (int s = 0; s < 4; ++s) {
            for (int i = 0; i < 12; ++i) {
                rc_[i] = eeAdd(rc_[i], delta[i]);
            }
            synthesize(s * 12, s == 3 ? 33 : 1);
        }
        if (plain_) {
            return m + 324;
        }
        size_t pos = ptr - 1;
        if (pcmPatch_) {
            const int32_t off = s16be(pos);
            const int32_t cnt = s16be(pos + 2);
            if (cnt > 0 && (off < 0 || off + cnt > 432)) {
                lost = true;
            } else {
                for (int32_t i = 0; i < cnt; ++i) {
                    m[324 + off + i] = s16be(pos + 4 + 2 * size_t(i));
                }
            }
            // pos += 4 + 2 x cnt (a negative count moves it back, as in the JS)
            pos = size_t(int64_t(pos) + 4 + 2 * int64_t(cnt));
        }
        pcmPatch_ = at(pos) == 0xee;
        initBits(pos + 1);
        return m + 324;
    }

private:
    static const double* rcTable() {
        static const double t[64] = {
            0, -0.99677598476409912109375, -0.99032700061798095703125, -0.983879029750823974609375,
            -0.977430999279022216796875, -0.970982015132904052734375, -0.964533984661102294921875,
            -0.958085000514984130859375, -0.9516370296478271484375, -0.930754005908966064453125,
            -0.904959976673126220703125, -0.879167020320892333984375, -0.853372991085052490234375,
            -0.827579021453857421875, -0.801786005496978759765625, -0.775991976261138916015625,
            -0.75019800662994384765625, -0.724404990673065185546875, -0.6986110210418701171875,
            -0.6706349849700927734375, -0.61904799938201904296875, -0.567460000514984130859375,
            -0.515873014926910400390625, -0.4642859995365142822265625, -0.4126980006694793701171875,
            -0.361110985279083251953125, -0.309523999691009521484375, -0.257937014102935791015625,
            -0.20634900033473968505859375, -0.1547619998455047607421875, -0.10317499935626983642578125,
            -0.05158700048923492431640625,
            0, 0.05158700048923492431640625, 0.10317499935626983642578125, 0.1547619998455047607421875,
            0.20634900033473968505859375, 0.257937014102935791015625, 0.309523999691009521484375,
            0.361110985279083251953125, 0.4126980006694793701171875, 0.4642859995365142822265625,
            0.515873014926910400390625, 0.567460000514984130859375, 0.61904799938201904296875,
            0.6706349849700927734375, 0.6986110210418701171875, 0.724404990673065185546875,
            0.75019800662994384765625, 0.775991976261138916015625, 0.801786005496978759765625,
            0.827579021453857421875, 0.853372991085052490234375, 0.879167020320892333984375,
            0.904959976673126220703125, 0.930754005908966064453125, 0.9516370296478271484375,
            0.958085000514984130859375, 0.964533984661102294921875, 0.970982015132904052734375,
            0.977430999279022216796875, 0.983879029750823974609375, 0.99032700061798095703125,
            0.99677598476409912109375,
        };
        return t;
    }
    static const uint8_t* codebook(int model) {
        static const uint8_t a[256] = {
            4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 17, 4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 21,
            4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 18, 4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 25,
            4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 17, 4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 22,
            4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 18, 4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 0,
            4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 17, 4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 21,
            4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 18, 4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 26,
            4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 17, 4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 22,
            4, 6, 5, 9, 4, 6, 5, 13, 4, 6, 5, 10, 4, 6, 5, 18, 4, 6, 5, 9, 4, 6, 5, 14, 4, 6, 5, 10, 4, 6, 5, 2,
        };
        static const uint8_t b[256] = {
            4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 23, 4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 27,
            4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 24, 4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 1,
            4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 23, 4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 28,
            4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 24, 4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 3,
            4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 23, 4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 27,
            4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 24, 4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 1,
            4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 23, 4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 28,
            4, 11, 7, 15, 4, 12, 8, 19, 4, 11, 7, 16, 4, 12, 8, 24, 4, 11, 7, 15, 4, 12, 8, 20, 4, 11, 7, 16, 4, 12, 8, 3,
        };
        return model == 0 ? a : b;
    }
    uint8_t at(size_t p) const {
        return p < size_ ? u8_[p] : 0;
    }
    int32_t s16be(size_t p) const {
        const int32_t v = (int32_t(at(p)) << 8) | int32_t(at(p + 1));
        return (v & 0x8000) ? v - 0x10000 : v;
    }
    void initBits(size_t p) {
        ptr = p + 1;
        value_ = at(p);
        count_ = 8;
    }
    uint32_t readBits(int n) {
        const uint32_t x = value_ & ((1u << n) - 1);
        value_ >>= n;
        count_ -= n;
        if (count_ < 8) {
            value_ |= uint32_t(at(ptr++)) << count_;
            count_ += 8;
        }
        return x;
    }
    uint32_t peek8() const {
        return value_ & 0xff;
    }
    void parseHeader() {
        using namespace detail;
        reducedBw_ = int(readBits(1)) | (forceReducedBw_ ? 1 : 0);
        threshold_ = 32 - int(readBits(4));
        fixedGains_[0] = eeMul(double(1 + readBits(4)), 8);
        static const double k0001 = f32bits(0x3a83126f);
        static const double k104 = f32bits(0x3f851eb8);
        const double base = eeAdd(eeMul(double(readBits(6)), k0001), k104);
        for (int i = 1; i < 64; ++i) {
            fixedGains_[i] = eeMul(fixedGains_[i - 1], base);
        }
    }
    void excitation(bool multipulse, int off, int stride) {
        static const uint8_t next[29] = {1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
        static const uint8_t sizes[29] = {8, 7, 8, 7, 2, 2, 2, 3, 3, 4, 4, 3, 3, 5, 5, 4, 4, 6, 6, 5, 5, 7, 7, 6, 6, 8, 8, 7, 7};
        static const int8_t pulse[29] = {0, 0, 0, 0, 0, -1, 1, -1, 1, -2, 2, -2, 2, -3, 3, -3, 3, -4, 4, -4, 4, -5, 5, -5, 5, -6, 6, -6, 6};
        double* out = exc_;
        int i = 0;
        if (multipulse) {
            int model = 0;
            while (i < 108) {
                const int cmd = codebook(model)[peek8()];
                model = next[cmd];
                readBits(sizes[cmd]);
                if (cmd > 3) {
                    out[off + i] = pulse[cmd];
                    i += stride;
                } else if (cmd > 1) {
                    int n = 7 + int(readBits(6));
                    if (i + n * stride > 108) {
                        n = (108 - i) / stride;
                    }
                    while (n-- > 0) {
                        out[off + i] = 0;
                        i += stride;
                    }
                } else {
                    int x = 7;
                    while (readBits(1)) {
                        x++;
                    }
                    if (!readBits(1)) {
                        x = -x;
                    }
                    out[off + i] = x;
                    i += stride;
                }
            }
        } else {
            while (i < 108) {
                const uint32_t b = value_ & 3;
                if (b == 1) {
                    out[off + i] = -2;
                    readBits(2);
                } else if (b == 3) {
                    out[off + i] = 2;
                    readBits(2);
                } else {
                    out[off + i] = 0;
                    readBits(1);
                }
                i += stride;
            }
        }
    }
    // 0x3CD590 (Float32Array temporaries: each store rounds to float32, as the JS's typed arrays do)
    void rcToLpc(double* lpc) {
        using namespace detail;
        double t2[12] = {};
        double t1[12] = {};
        for (int i = 10; i >= 0; --i) {
            t2[i + 1] = rc_[i];
        }
        t2[0] = 1;
        for (int i = 0; i < 12; ++i) {
            double x = eeMul(-rc_[11], t2[11]);
            for (int j = 10; j >= 0; --j) {
                x = eeSub(x, eeMul(rc_[j], t2[j]));
                t2[j + 1] = fround(eeAdd(eeMul(rc_[j], x), t2[j]));
            }
            t2[0] = fround(x);
            t1[i] = fround(x);
            for (int j = 0; j < i; ++j) {
                x = eeSub(x, eeMul(lpc[j], t1[i - 1 - j]));
            }
            lpc[i] = fround(x);
        }
    }
    void synthesize(int offset, int blocks) {
        using namespace detail;
        double lpc[12] = {};
        rcToLpc(lpc);
        double* h = synth_;
        double* m = mem_;
        int p = 324 + offset;
        for (int b = 0; b < blocks; ++b) {
            for (int j = 0; j < 12; ++j) {
                double x = m[p];
                for (int k = 1; k < j; ++k) {
                    x = accAdd(x, eeMul(lpc[k], h[k - j + 12]));
                }
                for (int k = j; k < 12; ++k) {
                    x = accAdd(x, eeMul(lpc[k], h[k - j]));
                }
                if (j > 0) {
                    x = accAdd(x, eeMul(lpc[0], h[12 - j]));
                }
                h[11 - j] = fround(x);
                m[p++] = fround(x);
            }
        }
    }

    const uint8_t* u8_;
    size_t size_;
    bool forceReducedBw_;
    bool plain_;
    bool pcmPatch_ = false;
    uint32_t value_ = 0;
    int count_ = 0;
    int reducedBw_ = 0;
    int threshold_ = 0;
    // the JS's Float32Array state, held as float32 values in doubles
    double fixedGains_[64] = {};
    double rc_[12] = {};
    double synth_[12] = {};
    double mem_[324 + 432] = {};
    double exc_[5 + 108 + 5] = {};
};

// QA: samples the MicroTalk decoder had to replace (web/audio-decode.js decodeStats)
inline size_t& nonFiniteCount() {
    static size_t n = 0;
    return n;
}

inline void utkRun(UtkDecoder& dec, size_t count, float* out, size_t stride) {
    size_t done = 0;
    while (done < count) {
        const double* frame = dec.decodeFrame();
        const size_t n = std::min<size_t>(432, count - done);
        if (dec.lost) {
            for (size_t i = done; i < count; ++i) {
                out[i * stride] = 0;
            }
            break;
        }
        for (size_t i = 0; i < n; ++i) {
            const double s = frame[i];
            if (!std::isfinite(s)) {
                nonFiniteCount()++;
            }
            out[(done + i) * stride] = std::isfinite(s) ? float((s > 32767 ? 32767 : s < -32768 ? -32768 : s) / 32768) : 0.f;
        }
        done += n;
    }
}

inline size_t decodeMicroTalk(const uint8_t* u8, size_t size, size_t pos, size_t count, float* out, size_t stride = 1, bool forceReducedBw = false) {
    UtkDecoder dec(u8, size, pos, forceReducedBw);
    utkRun(dec, count, out, stride);
    return dec.ptr - 2;
}

// an SCxl stream (SCHl SCCl SCDl* [SCLl] SCEl)
struct StreamInfo : SampleInfo {
    struct Block {
        size_t offset = 0;
        size_t size = 0;
        size_t samples = 0;
    };
    int platform = 0;
    std::vector<Block> blocks;
    size_t blockCount = 0;
    int64_t loopBlock = -1;
    size_t end = 0;
    size_t decodedSamples = 0;
};
inline StreamInfo parseStream(const uint8_t* u8, size_t size, size_t offset = 0) {
    using namespace detail;
    if (offset + 8 > size || fourcc(u8, offset) != "SCHl") {
        throw std::runtime_error("SCHl expected at " + std::to_string(offset));
    }
    size_t p = offset;
    const size_t hsize = u32le(u8, p + 4);
    const Pt pt = parsePT(u8, size, p + 8);
    StreamInfo info;
    static_cast<SampleInfo&>(info) = sampleInfo(pt.patches[0].sample);
    info.params = pt.patches[0].params;
    info.platform = pt.platform;
    p += hsize;
    for (;;) {
        if (p + 8 > size) {
            throw std::runtime_error("stream runs past the buffer");
        }
        const std::string id = fourcc(u8, p);
        const size_t bsize = u32le(u8, p + 4);
        if (id == "SCCl") {
            info.blockCount = u32le(u8, p + 8);
        } else if (id == "SCDl") {
            info.blocks.push_back({p, bsize, u32le(u8, p + 8)});
        } else if (id == "SCLl") {
            info.loopBlock = u32le(u8, p + 8);
        } else if (id == "SCEl") {
            p += bsize;
            break;
        } else {
            throw std::runtime_error("unexpected block " + id + " at " + std::to_string(p));
        }
        if (bsize < 8) {
            throw std::runtime_error("bad block size at " + std::to_string(p));
        }
        p += bsize;
    }
    info.end = p;
    for (const auto& b : info.blocks) {
        info.decodedSamples += b.samples;
    }
    return info;
}

inline Decoded decodeStream(const uint8_t* u8, size_t size, size_t offset = 0) {
    using namespace detail;
    const StreamInfo info = parseStream(u8, size, offset);
    const int ch = info.channels;
    const size_t total = info.sampleCount ? size_t(info.sampleCount) : info.decodedSamples;
    Decoded d;
    d.sampleRate = info.sampleRate;
    d.channels = ch;
    d.length = total;
    d.data.assign(size_t(ch), std::vector<float>(total, 0.f));
    std::vector<History> states(static_cast<size_t>(ch));
    std::vector<std::unique_ptr<UtkDecoder>> utk(static_cast<size_t>(ch));
    size_t at = 0;
    for (const auto& b : info.blocks) {
        const size_t tab = b.offset + 12;
        const size_t base = tab + 4 * size_t(ch);
        const int64_t left = int64_t(total) - int64_t(at);
        const size_t n = size_t(std::min<int64_t>(int64_t(b.samples), left));
        if (left <= 0 || n == 0) {
            break;
        }
        for (int c = 0; c < ch; ++c) {
            const size_t start = base + u32le(u8, tab + 4 * size_t(c));
            float* out = d.data[size_t(c)].data() + at;
            if (info.codec == kEaxa) {
                decodeEAXA(u8, start + 4, n, states[size_t(c)], out);
            } else if (info.codec == kVag) {
                decodePSADPCM(u8, start, n, states[size_t(c)], out);
            } else if (info.codec == kPcm8) {
                decodePCM8(u8, start, n, out);
            } else if (info.codec == kMicroTalk) {
                if (!utk[size_t(c)]) {
                    utk[size_t(c)] = std::make_unique<UtkDecoder>(u8, size, start + 1, false, info.version < 3);
                } else {
                    utk[size_t(c)]->open(start + 1);
                }
                utkRun(*utk[size_t(c)], n, out, 1);
            } else {
                throw std::runtime_error("unsupported stream codec " + std::to_string(info.codec));
            }
        }
        at += n;
    }
    return d;
}

// a BNKl v5 bank
struct Bank {
    struct Entry {
        int index = 0;
        std::vector<SampleInfo> patches;
    };
    int version = 0;
    int count = 0;
    size_t headerSize = 0;
    size_t spuDataSize = 0;
    size_t ramDataSize = 0;
    // a null entry: patches empty and index -1
    std::vector<Entry> entries;
};
inline Bank parseBank(const uint8_t* u8, size_t size) {
    using namespace detail;
    if (size < 0x14 || fourcc(u8, 0) != "BNKl") {
        throw std::runtime_error("BNKl expected");
    }
    Bank bank;
    bank.version = u8[4];
    bank.count = int(u16le(u8, 6));
    bank.headerSize = u32le(u8, 8);
    bank.spuDataSize = u32le(u8, 12);
    bank.ramDataSize = u32le(u8, 16);
    for (int i = 0; i < bank.count; ++i) {
        const size_t slot = 0x14 + 4 * size_t(i);
        const uint32_t rel = u32le(u8, slot);
        Bank::Entry e;
        if (!rel) {
            e.index = -1;
            bank.entries.push_back(e);
            continue;
        }
        const Pt pt = parsePT(u8, size, slot + rel);
        e.index = i;
        for (const auto& p : pt.patches) {
            SampleInfo s = sampleInfo(p.sample);
            s.params = p.params;
            e.patches.push_back(s);
        }
        bank.entries.push_back(e);
    }
    return bank;
}

inline Decoded decodeBankPatch(const uint8_t* u8, size_t size, const SampleInfo& patch) {
    using namespace detail;
    const size_t n = size_t(std::max<int64_t>(0, patch.sampleCount));
    Decoded d;
    d.sampleRate = patch.sampleRate;
    d.channels = patch.channels;
    d.length = n;
    d.loopStart = patch.loopStart;
    d.loopEnd = patch.loopEnd;
    for (int c = 0; c < patch.channels; ++c) {
        std::vector<float> out(n, 0.f);
        size_t pos = size_t(patch.dataOffsets.at(size_t(c)));
        History h;
        switch (patch.codec) {
        case kEaxa:
            decodeEAXA(u8, pos, n, h, out.data());
            break;
        case kVag:
            decodePSADPCM(u8, pos, n, h, out.data());
            break;
        case kPcm8:
            if (u8[pos] == 0 && u8[pos + 1] == kPcm8 && u32le(u8, pos + 12) == n) {
                pos += 16;
            }
            decodePCM8(u8, pos, n, out.data());
            break;
        case kMicroTalk: {
            auto param = [&](int t) -> const Tag* {
                auto it = patch.params.find(t);
                return it == patch.params.end() ? nullptr : &it->second;
            };
            const Tag* loopTag = c == 0 ? param(0x1a) : param(0x26);
            if (patch.loopStart >= 0 && loopTag) {
                const size_t intro = size_t(std::min<int64_t>(patch.loopStart, int64_t(n)));
                const size_t body = size_t(patch.dataOffsets[0] + int64_t(loopTag->value));
                if (intro > 0) {
                    decodeMicroTalk(u8, size, pos, intro, out.data());
                }
                const size_t last = patch.loopEnd >= int64_t(intro) ? size_t(std::min<int64_t>(int64_t(n), patch.loopEnd + 1)) : n;
                if (last > intro) {
                    decodeMicroTalk(u8, size, body, last - intro, out.data() + intro);
                }
            } else {
                decodeMicroTalk(u8, size, pos, n, out.data());
            }
            break;
        }
        default:
            throw std::runtime_error("unsupported bank codec " + std::to_string(patch.codec));
        }
        d.data.push_back(std::move(out));
    }
    return d;
}

}  // namespace audio

}  // namespace ssx_native
