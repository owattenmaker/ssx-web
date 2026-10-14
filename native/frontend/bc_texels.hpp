#pragma once
// BC1 / BC2 (DXT1 / DXT3) rider textures of the Xbox HD set (web/bc-texels.js natively; docs/xbox-textures.md section 8): the
// archive entry ('SXBC' header, level 0 blocks), the exact decode, the 2 x 2 box filter, the encoder of the lower mip levels (the
// page's bcMipChain, uploaded as a compressed texture when the device has texture-compression-bc).
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace ssx_native {

namespace bc {

constexpr size_t kHeader = 16;

struct Entry {
    int codec = 0;
    // "ps2" or "xbox"
    bool xbox = false;
    uint32_t width = 0, height = 0;
    const uint8_t* blocks = nullptr;
    size_t size = 0;
};
struct Level {
    uint32_t width = 0, height = 0;
    std::vector<uint8_t> data;
};

inline bool isEntry(const uint8_t* b, size_t n) {
    return n >= kHeader && b[0] == 0x53 && b[1] == 0x58 && b[2] == 0x42 && b[3] == 0x43;
}
inline size_t blockBytes(int codec) {
    return codec == 1 ? 8 : 16;
}
inline size_t levelBytes(int codec, uint32_t w, uint32_t h) {
    return size_t(std::max(1u, (w + 3) >> 2)) * std::max(1u, (h + 3) >> 2) * blockBytes(codec);
}
inline Entry parse(const uint8_t* b, size_t n) {
    if (!isEntry(b, n)) {
        throw std::runtime_error("Not a BC texture entry");
    }
    Entry e;
    e.codec = b[4];
    if ((e.codec != 1 && e.codec != 2) || b[5] > 1) {
        throw std::runtime_error("Bad BC texture header");
    }
    e.xbox = b[5] == 1;
    e.width = uint32_t(b[6]) | (uint32_t(b[7]) << 8);
    e.height = uint32_t(b[8]) | (uint32_t(b[9]) << 8);
    if (!e.width || !e.height) {
        throw std::runtime_error("Bad BC texture header");
    }
    e.size = levelBytes(e.codec, e.width, e.height);
    if (n < kHeader + e.size) {
        throw std::runtime_error("Truncated BC texture entry");
    }
    e.blocks = b + kHeader;
    return e;
}

inline void expand(uint32_t c, uint8_t* pal, int o) {
    const uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
    pal[o] = uint8_t((r << 3) | (r >> 2));
    pal[o + 1] = uint8_t((g << 2) | (g >> 4));
    pal[o + 2] = uint8_t((b << 3) | (b >> 2));
    pal[o + 3] = 255;
}
// the four colours of a colour block (four: 4-colour mode, BC2 always, BC1 when c0 > c1)
inline void palette(uint32_t c0, uint32_t c1, bool four, uint8_t* pal) {
    expand(c0, pal, 0);
    expand(c1, pal, 4);
    for (int k = 0; k < 3; ++k) {
        const int a = pal[k], b = pal[4 + k];
        if (four) {
            pal[8 + k] = uint8_t((2 * a + b) / 3);
            pal[12 + k] = uint8_t((a + 2 * b) / 3);
        } else {
            pal[8 + k] = uint8_t((a + b) >> 1);
            pal[12 + k] = 0;
        }
    }
    pal[11] = 255;
    pal[15] = four ? 255 : 0;
}

inline std::vector<uint8_t> decode(int codec, const uint8_t* blocks, uint32_t width, uint32_t height) {
    std::vector<uint8_t> out(size_t(width) * height * 4);
    const size_t step = blockBytes(codec);
    const uint32_t bw = std::max(1u, (width + 3) >> 2), bh = std::max(1u, (height + 3) >> 2);
    uint8_t pal[16];
    for (uint32_t by = 0; by < bh; ++by) {
        for (uint32_t bx = 0; bx < bw; ++bx) {
            const size_t p = (size_t(by) * bw + bx) * step;
            const size_t c = codec == 1 ? p : p + 8;
            const uint32_t c0 = uint32_t(blocks[c]) | (uint32_t(blocks[c + 1]) << 8);
            const uint32_t c1 = uint32_t(blocks[c + 2]) | (uint32_t(blocks[c + 3]) << 8);
            const uint32_t bits = uint32_t(blocks[c + 4]) | (uint32_t(blocks[c + 5]) << 8) | (uint32_t(blocks[c + 6]) << 16) | (uint32_t(blocks[c + 7]) << 24);
            palette(c0, c1, codec != 1 || c0 > c1, pal);
            for (int i = 0; i < 16; ++i) {
                const uint32_t x = bx * 4 + uint32_t(i & 3), y = by * 4 + uint32_t(i >> 2);
                if (x >= width || y >= height) {
                    continue;
                }
                const int k = int((bits >> (2 * i)) & 3) * 4;
                const size_t o = (size_t(y) * width + x) * 4;
                out[o] = pal[k];
                out[o + 1] = pal[k + 1];
                out[o + 2] = pal[k + 2];
                out[o + 3] = codec == 1 ? pal[k + 3] : uint8_t(((blocks[p + size_t(i >> 1)] >> ((i & 1) * 4)) & 15) * 17);
            }
        }
    }
    return out;
}

// 2 x 2 box filter (the rounded mean of the texels that exist)
inline Level boxDown(const std::vector<uint8_t>& data, uint32_t width, uint32_t height) {
    Level l;
    l.width = std::max(1u, width >> 1);
    l.height = std::max(1u, height >> 1);
    l.data.resize(size_t(l.width) * l.height * 4);
    const uint32_t sx = width > 1 ? 2 : 1, sy = height > 1 ? 2 : 1, n = sx * sy;
    for (uint32_t y = 0; y < l.height; ++y) {
        for (uint32_t x = 0; x < l.width; ++x) {
            for (int c = 0; c < 4; ++c) {
                uint32_t s = 0;
                for (uint32_t dy = 0; dy < sy; ++dy) {
                    for (uint32_t dx = 0; dx < sx; ++dx) {
                        s += data[((size_t(y) * sy + dy) * width + x * sx + dx) * 4 + size_t(c)];
                    }
                }
                l.data[(size_t(y) * l.width + x) * 4 + size_t(c)] = uint8_t((s + (n >> 1)) / n);
            }
        }
    }
    return l;
}

// JS Math.round (the spec rule: the nearest integer, halves up; floor(x + 0.5) is off for 0.49999999999999994)
inline double jsRound(double x) {
    const double r = std::floor(x);
    return x - r >= 0.5 ? r + 1 : r;
}
// JS Math.hypot as V8 and JavaScriptCore compute it: the arguments scaled by the largest, the squares Kahan-summed
inline double jsHypot(double x, double y, double z) {
    const double a[3] = {std::abs(x), std::abs(y), std::abs(z)};
    const double max = std::max(a[0], std::max(a[1], a[2]));
    if (std::isinf(max)) {
        return INFINITY;
    }
    if (std::isnan(a[0]) || std::isnan(a[1]) || std::isnan(a[2])) {
        return NAN;
    }
    if (max == 0) {
        return 0;
    }
    double sum = 0;
    double compensation = 0;
    for (double v : a) {
        const double n = v / max;
        const double summand = n * n - compensation;
        const double preliminary = sum + summand;
        compensation = (preliminary - sum) - summand;
        sum = preliminary;
    }
    return std::sqrt(sum) * max;
}
inline uint32_t q565(double r, double g, double b) {
    auto cl = [](double v, double m) { return uint32_t(std::max(0.0, std::min(m, jsRound(v * m / 255)))); };
    return (cl(r, 31) << 11) | (cl(g, 63) << 5) | cl(b, 31);
}

// one colour block of 16 RGBA texels t
inline void encodeColour(const uint8_t* t, int codec, uint8_t* out) {
    bool punch = false;
    if (codec == 1) {
        for (int i = 0; i < 16; ++i) {
            punch = punch || t[i * 4 + 3] < 128;
        }
    }
    std::vector<int> use;
    for (int i = 0; i < 16; ++i) {
        if (!punch || t[i * 4 + 3] >= 128) {
            use.push_back(i);
        }
    }
    if (use.empty()) {
        out[0] = out[1] = out[2] = out[3] = 0;
        out[4] = out[5] = out[6] = out[7] = 255;
        return;
    }
    // the colours' principal axis (power iteration on the covariance), the endpoints at the extreme projections
    double mr = 0, mg = 0, mb = 0;
    for (int i : use) {
        mr += t[i * 4];
        mg += t[i * 4 + 1];
        mb += t[i * 4 + 2];
    }
    mr /= double(use.size());
    mg /= double(use.size());
    mb /= double(use.size());
    double xx = 0, xy = 0, xz = 0, yy = 0, yz = 0, zz = 0;
    for (int i : use) {
        const double r = t[i * 4] - mr, g = t[i * 4 + 1] - mg, b = t[i * 4 + 2] - mb;
        xx += r * r;
        xy += r * g;
        xz += r * b;
        yy += g * g;
        yz += g * b;
        zz += b * b;
    }
    double ax = 1, ay = 1, az = 1;
    for (int k = 0; k < 6; ++k) {
        const double nx = xx * ax + xy * ay + xz * az;
        const double ny = xy * ax + yy * ay + yz * az;
        const double nz = xz * ax + yz * ay + zz * az;
        const double l = jsHypot(nx, ny, nz);
        if (l < 1e-9) {
            break;
        }
        ax = nx / l;
        ay = ny / l;
        az = nz / l;
    }
    double lo = INFINITY, hi = -INFINITY;
    for (int i : use) {
        const double d = (t[i * 4] - mr) * ax + (t[i * 4 + 1] - mg) * ay + (t[i * 4 + 2] - mb) * az;
        lo = std::min(lo, d);
        hi = std::max(hi, d);
    }
    const uint32_t e0 = q565(mr + ax * hi, mg + ay * hi, mb + az * hi);
    const uint32_t e1 = q565(mr + ax * lo, mg + ay * lo, mb + az * lo);
    struct Fit {
        uint32_t c0, c1, bits;
        double err;
        bool four;
    };
    uint8_t pal[16];
    auto fit = [&](uint32_t c0, uint32_t c1) {
        // 4-colour mode needs c0 > c1 (BC1; BC2 reads 4-colour anyway), the punch-through mode c0 <= c1
        if (punch ? c0 > c1 : c0 < c1) {
            std::swap(c0, c1);
        }
        const bool four = codec != 1 || c0 > c1;
        palette(c0, c1, four, pal);
        uint32_t bits = 0;
        double err = 0;
        for (int i = 0; i < 16; ++i) {
            int best = 0;
            double bd = INFINITY;
            if (punch && t[i * 4 + 3] < 128) {
                bits |= 3u << (2 * i);
                continue;
            }
            for (int k = 0; k < (four ? 4 : 3); ++k) {
                const double dr = double(t[i * 4]) - pal[k * 4];
                const double dg = double(t[i * 4 + 1]) - pal[k * 4 + 1];
                const double db = double(t[i * 4 + 2]) - pal[k * 4 + 2];
                const double d = dr * dr + dg * dg + db * db;
                if (d < bd) {
                    bd = d;
                    best = k;
                }
            }
            bits |= uint32_t(best) << (2 * i);
            err += bd;
        }
        return Fit{c0, c1, bits, err, four};
    };
    Fit best = fit(e0, e1);
    // one least-squares refinement of the endpoints for the chosen indices (4-colour mode)
    if (best.four && best.c0 != best.c1) {
        const double w0[4] = {1, 0, 2.0 / 3, 1.0 / 3};
        double aa = 0, ab = 0, bb = 0;
        double ra[3] = {0, 0, 0}, rb[3] = {0, 0, 0};
        for (int i = 0; i < 16; ++i) {
            const double a = w0[(best.bits >> (2 * i)) & 3];
            const double b = 1 - a;
            aa += a * a;
            ab += a * b;
            bb += b * b;
            for (int c = 0; c < 3; ++c) {
                ra[c] += a * t[i * 4 + c];
                rb[c] += b * t[i * 4 + c];
            }
        }
        const double det = aa * bb - ab * ab;
        if (std::abs(det) > 1e-6) {
            double p[3], q[3];
            for (int c = 0; c < 3; ++c) {
                p[c] = (ra[c] * bb - rb[c] * ab) / det;
                q[c] = (rb[c] * aa - ra[c] * ab) / det;
            }
            const Fit alt = fit(q565(p[0], p[1], p[2]), q565(q[0], q[1], q[2]));
            if (alt.err < best.err) {
                best = alt;
            }
        }
    }
    out[0] = uint8_t(best.c0 & 255);
    out[1] = uint8_t(best.c0 >> 8);
    out[2] = uint8_t(best.c1 & 255);
    out[3] = uint8_t(best.c1 >> 8);
    out[4] = uint8_t(best.bits & 255);
    out[5] = uint8_t((best.bits >> 8) & 255);
    out[6] = uint8_t((best.bits >> 16) & 255);
    out[7] = uint8_t(best.bits >> 24);
}

// RGBA texels -> blocks (texels outside the image clamp to the edge)
inline std::vector<uint8_t> encode(int codec, const std::vector<uint8_t>& data, uint32_t width, uint32_t height) {
    const size_t step = blockBytes(codec);
    const uint32_t bw = std::max(1u, (width + 3) >> 2), bh = std::max(1u, (height + 3) >> 2);
    std::vector<uint8_t> out(size_t(bw) * bh * step);
    uint8_t t[64];
    for (uint32_t by = 0; by < bh; ++by) {
        for (uint32_t bx = 0; bx < bw; ++bx) {
            for (int i = 0; i < 16; ++i) {
                const uint32_t x = std::min(width - 1, bx * 4 + uint32_t(i & 3));
                const uint32_t y = std::min(height - 1, by * 4 + uint32_t(i >> 2));
                std::memcpy(t + i * 4, data.data() + (size_t(y) * width + x) * 4, 4);
            }
            uint8_t* o = out.data() + (size_t(by) * bw + bx) * step;
            if (codec == 2) {
                for (int i = 0; i < 16; i += 2) {
                    o[i >> 1] = uint8_t(uint32_t(jsRound(t[i * 4 + 3] / 17.0)) | (uint32_t(jsRound(t[(i + 1) * 4 + 3] / 17.0)) << 4));
                }
                encodeColour(t, 2, o + 8);
            } else {
                encodeColour(t, 1, o);
            }
        }
    }
    return out;
}

// bcMipChain: level 0 as stored, the levels below from the decoded level above, box-filtered and encoded again
inline std::vector<Level> mipChain(const Entry& e) {
    std::vector<Level> levels;
    levels.push_back({e.width, e.height, std::vector<uint8_t>(e.blocks, e.blocks + e.size)});
    Level level{e.width, e.height, decode(e.codec, e.blocks, e.width, e.height)};
    while (level.width > 1 || level.height > 1) {
        level = boxDown(level.data, level.width, level.height);
        levels.push_back({level.width, level.height, encode(e.codec, level.data, level.width, level.height)});
    }
    return levels;
}

}  // namespace bc

}  // namespace ssx_native
