#pragma once
// The race HUD's canvas widgets (1:1 with the page's): the tinted image copies the page makes with offscreen canvases (web/ui.js
// text, trick-hud.js TrickHudRenderer.tinted, race-place-hud.js tintedAtlas / tintedGlow, boost-gauge.js), ui.text, the trick HUD
// draw-list renderer (TrickHudRenderer.render), the key caps (input-glyphs.js drawKeyCap), the in-race place (race-place-hud.js),
// the boost gauge, orb and letters (boost-gauge.js, boost-orb.js, boost-letters.js, ui.js drawBoostLetters) and the progress
// meter (progress-meter-hud.js), all drawn through HudCanvas.
#include "ee_float.hpp"
#include "hud_canvas.hpp"
#include "trick_hud.hpp"
#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace ssx_native {

namespace hud {

// JS Math.round
inline double jsRound(double x) {
    const double r = std::floor(x);
    return x - r >= 0.5 ? r + 1 : r;
}
// a Uint8ClampedArray store (ToUint8Clamp: round half to even)
inline uint8_t clampByte(double v) {
    if (!(v > 0)) {
        return 0;
    }
    if (v >= 255) {
        return 255;
    }
    const double fl = std::floor(v);
    const double rest = v - fl;
    if (rest > 0.5) {
        return uint8_t(fl + 1);
    }
    if (rest < 0.5) {
        return uint8_t(fl);
    }
    return uint8_t(int(fl) % 2 == 0 ? fl : fl + 1);
}
// a float written to an 8-bit render target
inline uint8_t unorm(double x) {
    return uint8_t(std::clamp(std::floor(x * 255 + 0.5), 0.0, 255.0));
}
inline std::string hex2(int v) {
    char b[8];
    std::snprintf(b, sizeof b, "%02x", v);
    return b;
}
inline std::array<uint8_t, 3> parseHex(const std::string& colour) {
    std::array<uint8_t, 3> c{};
    for (int k = 0; k < 3; ++k) {
        c[size_t(k)] = uint8_t(std::stoi(colour.substr(size_t(1 + 2 * k), 2), nullptr, 16));
    }
    return c;
}

// The HUD's images: the PNGs of /assets/UI and the canvas copies made from them, each made once (the page's caches)
class HudImages {
public:
    HudCanvas* canvas = nullptr;
    std::string dir;

    int png(const std::string& name) {
        auto it = pngs_.find(name);
        if (it != pngs_.end()) {
            return it->second;
        }
        const int k = canvas->loadPng(dir + "/" + name + ".png", name);
        pngs_[name] = k;
        return k;
    }
    // drawImage(im); globalCompositeOperation 'multiply', fillRect(rgb); 'destination-in', drawImage(im)
    int multiply(const std::string& name, std::array<uint8_t, 3> rgb) {
        const std::string key = "m:" + name + ":" + std::to_string(rgb[0]) + "," + std::to_string(rgb[1]) + "," + std::to_string(rgb[2]);
        return cached(key, [&] {
            const HudImage& im = canvas->image(png(name));
            std::vector<uint8_t> out(im.premul.size());
            for (size_t i = 0; i < out.size(); i += 4) {
                const double da = im.premul[i + 3] / 255.0;
                for (int k = 0; k < 3; ++k) {
                    const double s = rgb[size_t(k)] / 255.0;
                    const double d = im.premul[i + size_t(k)] / 255.0;
                    const uint8_t first = unorm(s * (1 - da) + s * d);
                    out[i + size_t(k)] = unorm(first / 255.0 * da);
                }
                out[i + 3] = im.premul[i + 3];
            }
            return canvas->addCanvas(im.width, im.height, std::move(out));
        });
    }
    // a canvas of ceil(sw) x ceil(sh) with drawImage(im, sx, sy, sw, sh, 0, 0, sw, sh): bilinear, kept half a texel inside the source
    // rect (web/lui-player.js cut, web/loading-screen.js tinted's first draw)
    int cutRegion(const std::string& name, double sx, double sy, double sw, double sh) {
        char key[160];
        std::snprintf(key, sizeof key, "c:%s:%.17g,%.17g,%.17g,%.17g", name.c_str(), sx, sy, sw, sh);
        return cached(key, [&] {
            const HudImage& im = canvas->image(png(name));
            const int w = std::max(1, int(std::ceil(sw)));
            const int h = std::max(1, int(std::ceil(sh)));
            return canvas->addCanvas(w, h, cutPixels(im, sx, sy, sw, sh, w, h));
        });
    }
    // the cut's tinted copy: drawImage(cut), 'multiply' fillRect(rgb), 'destination-in' drawImage(cut) (web/lui-player.js tinted)
    int multiplyCut(const std::string& name, double sx, double sy, double sw, double sh, std::array<uint8_t, 3> rgb) {
        char key[180];
        std::snprintf(key, sizeof key, "mc:%s:%.17g,%.17g,%.17g,%.17g:%d,%d,%d", name.c_str(), sx, sy, sw, sh, rgb[0], rgb[1], rgb[2]);
        return cached(key, [&] {
            const HudImage& cut = canvas->image(cutRegion(name, sx, sy, sw, sh));
            return canvas->addCanvas(cut.width, cut.height, multiplied(cut.premul, rgb));
        });
    }
    // web/loading-screen.js tinted: the page region drawn into its own canvas, then multiply and destination-in
    int multiplyRegion(const std::string& name, double sx, double sy, double sw, double sh, std::array<uint8_t, 3> rgb) {
        char key[180];
        std::snprintf(key, sizeof key, "r:%s:%.17g,%.17g,%.17g,%.17g:%d,%d,%d", name.c_str(), sx, sy, sw, sh, rgb[0], rgb[1], rgb[2]);
        return cached(key, [&] {
            const HudImage& im = canvas->image(png(name));
            const int w = int(std::ceil(sw));
            const int h = int(std::ceil(sh));
            return canvas->addCanvas(w, h, multiplied(cutPixels(im, sx, sy, sw, sh, w, h), rgb));
        });
    }
    static std::vector<uint8_t> cutPixels(const HudImage& im, double sx, double sy, double sw, double sh, int w, int h) {
        std::vector<uint8_t> copy(size_t(w) * h * 4, 0);
        auto texel = [&](int x, int y, int k) {
            x = std::clamp(x, 0, im.width - 1);
            y = std::clamp(y, 0, im.height - 1);
            return double(im.premul[(size_t(y) * im.width + x) * 4 + size_t(k)]);
        };
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (x >= sw || y >= sh) {
                    continue;
                }
                const double u = std::clamp(sx + x + 0.5, sx + 0.5, sx + sw - 0.5) - 0.5;
                const double v = std::clamp(sy + y + 0.5, sy + 0.5, sy + sh - 0.5) - 0.5;
                const int x0 = int(std::floor(u));
                const int y0 = int(std::floor(v));
                // bilinear with 4-bit sub-texel weights, rounded (Chrome's half-texel cuts differ from it by 1 on a few texels: its filter
                // runs in float32 from the inverse matrix, so the half-way weights are not exact)
                const int fx = int(std::floor((u - x0) * 16));
                const int fy = int(std::floor((v - y0) * 16));
                for (int k = 0; k < 4; ++k) {
                    const int value = int(texel(x0, y0, k)) * (16 - fx) * (16 - fy) + int(texel(x0 + 1, y0, k)) * fx * (16 - fy) +
                                      int(texel(x0, y0 + 1, k)) * (16 - fx) * fy + int(texel(x0 + 1, y0 + 1, k)) * fx * fy;
                    copy[(size_t(y) * w + x) * 4 + size_t(k)] = uint8_t(std::clamp((value + 128) >> 8, 0, 255));
                }
            }
        }
        return copy;
    }
    static std::vector<uint8_t> multiplied(const std::vector<uint8_t>& premul, std::array<uint8_t, 3> rgb) {
        std::vector<uint8_t> out(premul.size());
        for (size_t i = 0; i < out.size(); i += 4) {
            const double da = premul[i + 3] / 255.0;
            for (int k = 0; k < 3; ++k) {
                const double s = rgb[size_t(k)] / 255.0;
                const double d = premul[i + size_t(k)] / 255.0;
                const uint8_t first = unorm(s * (1 - da) + s * d);
                out[i + size_t(k)] = unorm(first / 255.0 * da);
            }
            out[i + 3] = premul[i + 3];
        }
        return out;
    }
    // drawImage(im); 'source-in', fillRect(colour) (web/ui.js text tints)
    int sourceIn(const std::string& name, std::array<uint8_t, 3> rgb) {
        const std::string key = "s:" + name + ":" + std::to_string(rgb[0]) + "," + std::to_string(rgb[1]) + "," + std::to_string(rgb[2]);
        return cached(key, [&] {
            const HudImage& im = canvas->image(png(name));
            std::vector<uint8_t> out(im.premul.size());
            for (size_t i = 0; i < out.size(); i += 4) {
                const double da = im.premul[i + 3] / 255.0;
                for (int k = 0; k < 3; ++k) {
                    out[i + size_t(k)] = unorm(rgb[size_t(k)] / 255.0 * da);
                }
                out[i + 3] = im.premul[i + 3];
            }
            return canvas->addCanvas(im.width, im.height, std::move(out));
        });
    }
    // getImageData of `source`, each texel byte through fn(byte, channel), putImageData (an ImageData tint)
    template <class Fn>
    int imageData(const std::string& key, int source, Fn fn) {
        return cached(key, [&] {
            const HudImage& im = canvas->image(source);
            std::vector<uint8_t> data = unpremultiply(im.premul);
            for (size_t i = 0; i < data.size(); ++i) {
                data[i] = fn(data[i], int(i % 4));
            }
            return canvas->addCanvas(im.width, im.height, premultiply(data));
        });
    }
    int cachedCanvas(const std::string& key, int w, int h, const std::vector<uint8_t>& unpremul) {
        return cached(key, [&] { return canvas->addCanvas(w, h, premultiply(unpremul)); });
    }

private:
    template <class Make>
    int cached(const std::string& key, Make make) {
        auto it = made_.find(key);
        if (it != made_.end()) {
            return it->second;
        }
        const int k = make();
        made_[key] = k;
        return k;
    }
    std::map<std::string, int> pngs_;
    std::map<std::string, int> made_;
};

// web/ui.js text(): the raw glyph tables (fonts[font]), the image tinted with source-in
struct UiText {
    HudImages* images = nullptr;
    std::map<std::string, GlyphTable> fonts;

    void text(HudCanvas& c, const std::string& value, double x, double y, double size, const std::string& colour, const std::string& font,
              const std::string& alignment = "left") {
        auto ft = fonts.find(font);
        if (ft == fonts.end()) {
            return;
        }
        const GlyphTable& glyphs = ft->second;
        const double scale = size / (font == "FEFONT" ? 22 : 17);
        const auto cps = codePoints(value);
        double total = 0;
        for (char32_t ch : cps) {
            auto it = glyphs.find(ch);
            const int a = it == glyphs.end() ? 0 : it->second.advance;
            total += (a != 0 ? a : 10) * scale;
        }
        if (alignment == "right") {
            x -= total;
        }
        if (alignment == "center") {
            x -= total / 2;
        }
        const int im = images->sourceIn(font + "-0", parseHex(colour));
        for (char32_t ch : cps) {
            auto it = glyphs.find(ch);
            if (it == glyphs.end()) {
                x += 10 * scale;
                continue;
            }
            const Glyph& g = it->second;
            c.drawImage(im, g.x, g.y, g.w, g.h, x + g.dx * scale, y + g.dy * scale, g.w * scale, g.h * scale);
            x += g.advance * scale;
        }
    }
};

// web/trick-hud.js TrickHudRenderer
class TrickHudRenderer {
public:
    HudImages* images = nullptr;
    const TrickHud* hud = nullptr;

    // a font draw's tint: vertex RGB trunc(colour x 204); a sprite's: the 128 unity
    int tinted(const std::string& name, const Argb& argb, bool text) {
        auto q = [&](double v) {
            if (text) {
                return uint8_t(hudTextByte(v));
            }
            return uint8_t(std::max(0.0, std::min(255.0, jsRound(std::trunc(v * 128) * 255 / 128))));
        };
        return images->multiply(name, {q(argb[1]), q(argb[2]), q(argb[3])});
    }
    static double alphaOf(double a) {
        return std::max(0.0, std::min(1.0, std::trunc(a * 128) / 128));
    }
    void text(HudCanvas& c, const Draw& d, double dx, double dy, const Argb& argb) {
        const int im = tinted(d.font + "-0", argb, true);
        const GlyphTable& g = hud->glyphs(d.font);
        c.setGlobalAlpha(alphaOf(argb[0]));
        double x = d.x + dx;
        for (char32_t ch : codePoints(d.text)) {
            auto it = g.find(ch);
            if (it == g.end()) {
                continue;
            }
            const Glyph& q = it->second;
            const int gy = q.dy > 127 ? q.dy - 256 : q.dy;
            c.drawImage(im, q.x, q.y, q.w, q.h, x + q.dx * d.scale[0], d.y + dy + gy * d.scale[1], q.w * d.scale[0], q.h * d.scale[1]);
            x += q.advance * d.scale[0];
        }
    }
    // keyText(): FEFONT on this renderer's atlas, the ui.text shape
    void keyText(HudCanvas& c, const std::string& value, double x, double y, double size, const std::string& colour, const std::string& font,
                 const std::string& alignment) {
        const GlyphTable& g = hud->glyphs(font);
        const double sc = size / 22;
        const auto rgb = parseHex(colour);
        double w = 0;
        for (char32_t ch : codePoints(value)) {
            auto it = g.find(ch);
            const int a = it == g.end() ? 0 : it->second.advance;
            w += (a != 0 ? a : 10) * sc;
        }
        const double a = c.globalAlpha();
        Draw d;
        d.font = font;
        d.text = value;
        d.x = x - (alignment == "center" ? w / 2 : alignment == "right" ? w : 0);
        d.y = y;
        d.scale = {sc, sc};
        text(c, d, 0, 0, {a, rgb[0] / 255.0, rgb[1] / 255.0, rgb[2] / 255.0});
        c.setGlobalAlpha(a);
    }
    // web/input-glyphs.js drawKeyCap
    template <class TextFn>
    static void drawKeyCap(HudCanvas& c, TextFn textFn, const std::string& label, double x, double y, double w, double h, double stretch) {
        const double k = h / 16;
        c.save();
        c.keyCapBox(x, y, w, h, 3 * k, 1.2 * k);
        double arrow = -1;
        if (label == "←") {
            arrow = 180;
        } else if (label == "↑") {
            arrow = 270;
        } else if (label == "↓") {
            arrow = 90;
        } else if (label == "→") {
            arrow = 0;
        }
        if (arrow >= 0) {
            c.translate(x + w / 2, y + h / 2);
            c.rotate(arrow * M_PI / 180);
            c.scale(k, k);
            c.keyCapArrow();
            c.restore();
            return;
        }
        c.translate(x + w / 2, y + 3 * k);
        c.scale(stretch, 1);
        textFn(label, kTextK * h);
        c.restore();
    }
    void render(HudCanvas& c, const std::vector<Draw>& list) {
        c.save();
        c.scale(1, 448.0 / 480);
        // within a layer the font glyphs land above the sprites (a stable sort by order x 2 + text)
        std::vector<const Draw*> sorted;
        for (const auto& d : list) {
            sorted.push_back(&d);
        }
        auto rank = [](const Draw* d) { return d->order * 2 + (d->kind == Draw::Text ? 1 : 0); };
        std::stable_sort(sorted.begin(), sorted.end(), [&](const Draw* a, const Draw* b) { return rank(a) < rank(b); });
        for (const Draw* dp : sorted) {
            const Draw& d = *dp;
            if (d.kind == Draw::Text) {
                if (d.shadowArgb && (d.shadow[0] != 0 || d.shadow[1] != 0) && (*d.shadowArgb)[0] > 0) {
                    text(c, d, d.shadow[0], d.shadow[1], {(*d.shadowArgb)[0], 0, 0, 0});
                }
                text(c, d, 0, 0, d.argb);
            } else if (d.kind == Draw::Sprite && !d.key.empty()) {
                c.save();
                c.setGlobalAlpha(alphaOf(d.argb[0]));
                const Vec2 size = d.size ? *d.size : Vec2{d.w, d.h};
                drawKeyCap(
                    c,
                    [&](const std::string& label, double size) { keyText(c, label, 0, 0, size, "#f0f5f8", "FEFONT", "center"); },
                    d.key, d.x, d.y, size[0] * d.scale[0], size[1] * d.scale[1], kLuiStretch);
                c.restore();
            } else if (d.kind == Draw::Sprite && d.sprite && !d.sprite->page.empty()) {
                const int im = tinted(d.sprite->page, d.argb, false);
                const HudImage& image = c.image(im);
                const double v0 = d.sprite->uv[0];
                const double u0 = d.sprite->uv[1];
                const double u1 = d.sprite->uv[2];
                const double v1 = d.sprite->uv[3];
                const Vec2 size = d.size ? *d.size : Vec2{d.w, d.h};
                const double w = size[0] * d.scale[0];
                const double h = size[1] * d.scale[1];
                c.setGlobalAlpha(alphaOf(d.argb[0]));
                c.save();
                c.translate(d.x + (u1 < u0 ? w : 0), d.y + (v1 < v0 ? h : 0));
                c.scale(u1 < u0 ? -1 : 1, v1 < v0 ? -1 : 1);
                c.drawImage(im, std::min(u0, u1) * image.width, std::min(v0, v1) * image.height, std::abs(u1 - u0) * image.width,
                            std::abs(v1 - v0) * image.height, 0, 0, w, h);
                c.restore();
            } else if (d.kind == Draw::Rect) {
                auto q = [](double v) { return uint8_t(std::max(0.0, std::min(255.0, std::trunc(fround(v * 255))))); };
                c.setGlobalAlpha(alphaOf(d.argb[0]));
                c.fillRect(d.x, d.y, d.w * d.scale[0], d.h * d.scale[1], {q(d.argb[1]), q(d.argb[2]), q(d.argb[3])});
            } else if (d.kind == Draw::Quad) {
                auto q = [](double v) { return uint8_t(std::max(0.0, std::min(255.0, std::trunc(v * 255)))); };
                c.setGlobalAlpha(alphaOf(d.argb[0]));
                c.fillRect(d.x, d.y, (*d.size)[0], (*d.size)[1], {q(d.argb[1]), q(d.argb[2]), q(d.argb[3])});
            }
        }
        c.restore();
    }
};

// web/race-place-hud.js drawRacePlace (the place 0..5, the rider count, the change timer and the 1st-place glow timer)
struct PlaceHud {
    int place = -1;
    int total = 0;
    double timer = -1;
    double glow = -1;
};
inline const double kGlowStep = fround(0.026782158762216568);
inline const double kGlowScale = fround(1.8101750612258911);
// 0x1EBC10
inline double placeGlowStep(double g, int place) {
    if (place != 0) {
        return -1;
    }
    if (!(g >= 0)) {
        return 0;
    }
    double t = fround(g + kGlowStep);
    while (t > 2) {
        t = fround(t - 2);
    }
    return t;
}
inline void drawRacePlace(HudCanvas& c, HudImages& images, const PlaceHud& hud) {
    if (hud.place < 0 || hud.place > 5) {
        return;
    }
    struct R {
        double u0, v0, u1, v1;
    };
    static const std::map<std::string, R> rects = {
        {"nm1w", {0.5, 84.5, 25.5, 125.5}},     {"nm2w", {30.5, 84.5, 73.5, 125.5}},   {"nm3w", {77.5, 84.5, 120.5, 125.5}},
        {"nm4w", {125.5, 84.5, 168.5, 125.5}},  {"nm5w", {176.5, 84.5, 219.5, 125.5}}, {"nm6w", {211.5, 175.5, 254.5, 216.5}},
        {"st_w", {216.5, 62.5, 255.5, 83.5}},   {"nd_w", {215.5, 129.5, 255.5, 150.5}}, {"rd_w", {216.5, 39.5, 255.5, 60.5}},
        {"th_w", {214.5, 152.5, 254.5, 173.5}}, {"slash", {228.5, 85.5, 251.5, 125.5}},
    };
    static const char* suffix[4] = {"st_w", "nd_w", "rd_w", "th_w"};
    auto quad = [&](int image, const std::string& name, double x, double y, double w, double h) {
        const R& r = rects.at(name);
        c.drawImage(image, r.u0, r.v0, r.u1 - r.u0, r.v1 - r.v0, x, y, w, h);
    };
    const int place = hud.place;
    const bool first = place == 0;
    const double W = first ? 26 : 40;
    const double x = 20;
    const double y = 20;
    const double t = hud.timer;
    // GOLD [1, 0.8, 0] / BLUE [0, 0.494, 0.7] as rgb(round(v x 255))
    const std::array<uint8_t, 3> rgb = first ? std::array<uint8_t, 3>{255, 204, 0} : std::array<uint8_t, 3>{0, 126, 179};
    const int tint = images.multiply("OV_1-3", rgb);
    const int atlas = images.png("OV_1-3");
    if (first && hud.glow >= 0) {
        c.save();
        // tintedGlow(glow, GOLD): ImageData x trunc(v x 128) / 128 (128, 102, 0)
        const int glowPng = images.png("part-glow");
        const int k[3] = {128, 102, 0};
        const int im = images.imageData("glow-gold", glowPng, [&](uint8_t v, int ch) { return ch < 3 ? clampByte(v * double(k[ch]) / 128) : v; });
        auto drawGlow = [&](double gx, double gy, double w, double h) {
            const double dw = fround(w * fround(kGlowScale - 1));
            const double dh = fround(h * fround(kGlowScale - 1));
            gx = fround(gx - fround(dw * 0.5));
            gy = fround(gy - fround(dh * 0.5));
            w = fround(w + dw);
            h = fround(h + dh);
            const double g = hud.glow;
            const double pulse = fround(0.5 + fround(0.5 * (g > 1 ? fround(2 - g) : g)));
            c.setGlobalAlpha(std::trunc(fround(1 * pulse) * 128) / 128);
            const HudImage& image = c.image(im);
            const double iw = image.width;
            const double ih = image.height;
            if (w > 72) {
                c.drawImage(im, 0, 0, iw / 2, ih, gx, gy, 36, h);
                c.drawImage(im, iw / 2 - 0.5, 0, 1, ih, gx + 36, gy, fround(w - 72), h);
                c.drawImage(im, iw / 2, 0, iw / 2, ih, fround(gx + fround(w - 72)) + 36, gy, 36, h);
            } else {
                c.drawImage(im, 0, 0, iw, ih, gx, gy, w, h);
            }
        };
        drawGlow(x, y, W, 42);
        drawGlow(x + W, y, 40, 22);
        c.restore();
    }
    c.save();
    if (t >= 0 && !first) {
        c.setGlobalAlpha(0.5 * t + 0.5);
    }
    if (t >= 0 && first) {
        const double s = 1 + std::sin(4 * M_PI * t) * 0.19963631 * (1 - t * t);
        c.translate(x, y);
        c.scale(s, s);
        c.translate(-x, -y);
    }
    quad(tint, "nm" + std::to_string(place + 1) + "w", x, y, W, 42);
    quad(tint, suffix[place < 4 ? place : 3], x + W, y, 40, 22);
    c.restore();
    if (hud.total >= 1 && hud.total <= 6) {
        quad(atlas, "slash", x + W, y + 22, 20, 20);
        quad(atlas, "nm" + std::to_string(hud.total) + "w", x + W + 20, y + 22, 20, 20);
    }
}

// web/boost-gauge.js, boost-orb.js, boost-letters.js
class BoostGauge {
public:
    void load(const nlohmann::json& profile, HudImages& images) {
        p_ = profile;
        images_ = &images;
        atlas_ = images.png(p_.value("atlas", "OV_1-4"));
        glow_ = images.png("part-glow");
        red_ = tinted({1, 1, 0, 0});
        for (const auto& colour : p_["flashColours"]) {
            const Argb c = {colour[0].get<double>(), colour[1].get<double>(), colour[2].get<double>(), colour[3].get<double>()};
            orbTints_.push_back(tinted(c));
            // the part texture x trunc(colour x 128) / 128 (ImageData)
            const double k[3] = {std::trunc(c[1] * 128) / 128, std::trunc(c[2] * 128) / 128, std::trunc(c[3] * 128) / 128};
            const std::string key = "gauge-glow:" + std::to_string(orbTints_.size());
            const int g = images.imageData(key, glow_, [&](uint8_t v, int ch) { return ch < 3 ? clampByte(v * k[ch]) : v; });
            glowTints_.push_back(g);
            // the middle column pair averaged by alpha (1 x height)
            const HudImage& im = images.canvas->image(g);
            const std::vector<uint8_t> src = unpremultiply(im.premul);
            std::vector<uint8_t> col(size_t(im.height) * 4, 0);
            const int x0 = im.width / 2 - 1;
            for (int y = 0; y < im.height; ++y) {
                const size_t i = (size_t(y) * im.width + size_t(x0)) * 4;
                const size_t j = size_t(y) * 4;
                const double a = double(src[i + 3]) + src[i + 7];
                col[j + 3] = clampByte(a / 2);
                for (int ch = 0; ch < 3; ++ch) {
                    col[j + size_t(ch)] = a != 0 ? clampByte((double(src[i + size_t(ch)]) * src[i + 3] + double(src[i + 4 + size_t(ch)]) * src[i + 7]) / a) : 0;
                }
            }
            glowColumns_.push_back(images.cachedCanvas(key + ":column", 1, im.height, col));
        }
    }
    const nlohmann::json& profile() const {
        return p_;
    }

    // the orb (1EFC58) and its glow
    void drawOrb(HudCanvas& c, double phase, double tier) {
        const Orb o = orb(phase, tier);
        const std::optional<Glow> glow = orbGlow(phase, tier);
        const int atlas = o.palette < 0 ? atlas_ : orbTints_[size_t(o.palette)];
        const HudImage& im = c.image(atlas);
        c.save();
        c.scale(1, 448.0 / 480);
        if (o.size[0] > 0) {
            c.drawImage(atlas, o.uv[0] * im.width, o.uv[1] * im.height, (o.uv[2] - o.uv[0]) * im.width, (o.uv[3] - o.uv[1]) * im.height, o.position[0],
                        o.position[1], o.size[0], o.size[1]);
        }
        if (glow) {
            c.setGlobalAlpha(std::trunc(glow->argb[0] * 128) / 128);
            c.drawImage(glowTints_[size_t(glow->palette)], glow->position[0], glow->position[1], glow->size[0], glow->size[1]);
        }
        c.restore();
    }
    void draw(HudCanvas& c, double preview, double stored, double phase, double tier) {
        const bool flashing = phase >= 0;
        const int palette = orb(phase, tier).palette;
        c.save();
        c.scale(1, 448.0 / 480);
        if (const auto glow = orbGlow(phase, tier)) {
            for (const auto& g : p_["coilGlows"]) {
                const double u = g["uv"][0].get<double>();
                const double v = g["uv"][1].get<double>();
                const double u1 = g["uv"][2].get<double>();
                const double v1 = g["uv"][3].get<double>();
                const int im = glowTints_[size_t(glow->palette)];
                const HudImage& image = c.image(im);
                c.setGlobalAlpha(std::trunc(glow->argb[0] * 128) / 128);
                const double px = g["position"][0].get<double>();
                const double py = g["position"][1].get<double>();
                const double sw = g["size"][0].get<double>();
                const double sh = g["size"][1].get<double>();
                if (u == u1) {
                    c.drawImage(glowColumns_[size_t(glow->palette)], px, py, sw, sh);
                } else {
                    c.drawImage(im, u * image.width, v * image.height, (u1 - u) * image.width, (v1 - v) * image.height, px, py, sw, sh);
                }
            }
        }
        c.setGlobalAlpha(1);
        for (const auto& layer : p_["layers"]) {
            const int widget = layer["widget"].get<int>();
            const double fraction = widget == 6 ? stored : widget == 7 ? preview : 1;
            const bool background = flashing && widget <= 5;
            int atlas;
            if (background) {
                atlas = orbTints_[size_t(palette)];
            } else if (flashing) {
                atlas = atlas_;
            } else if (widget == 6) {
                atlas = tinted(gaugeColour(stored));
            } else if (widget == 7) {
                atlas = red_;
            } else {
                atlas = atlas_;
            }
            const HudImage& im = c.image(atlas);
            std::vector<Rect> rects;
            if (background) {
                const auto& fb = p_["flashBackgrounds"];
                const std::string key = std::to_string(widget);
                if (fb.contains(key)) {
                    for (const auto& r : fb[key]) {
                        rects.push_back(rect(r));
                    }
                }
            } else {
                rects = gaugeRects(layer, fraction);
            }
            for (const auto& r : rects) {
                c.drawImage(atlas, r.u * im.width, r.v * im.height, r.uw * im.width, r.vh * im.height, r.x, r.y, r.w, r.h);
            }
        }
        c.restore();
    }

    // ui.js drawBoostLetters (the type 8 loop 1ED7D4)
    void drawLetters(HudCanvas& c, UiText& ui, HudImages& images, double count, double fraction, bool removed, double pendingCount, double pendingPhase) {
        struct Letter {
            int letter;
            double scale;
            bool active;
            double outlineScaleX;
            const nlohmann::json* draws;
        };
        auto submissions = [&](int letter, int mode) -> const nlohmann::json* {
            for (const auto& s : p_["letterSubmissions"]) {
                if (s["letter"].get<int>() == letter && s["mode"].get<int>() == mode) {
                    return &s["draws"];
                }
            }
            return nullptr;
        };
        const double ninth = fround(1.0 / 9);
        const double pop = 1.8001313209533691;
        double lower = 0;
        double upper = ninth;
        std::vector<Letter> letters;
        for (int letter = 0; letter < 9; ++letter) {
            const bool active = !removed && letter < count && fraction >= lower;
            const double scale = active && fraction < upper ? 1 + (upper - fraction) * pop : 1;
            lower = upper;
            upper = fround(upper + ninth);
            letters.push_back({letter, scale, active, 1, submissions(letter, active ? 2 : 0)});
        }
        int firstPending = 0;
        for (const auto& l : letters) {
            firstPending += l.active ? 1 : 0;
        }
        if (pendingPhase >= 0) {
            for (int i = firstPending; i < std::min(9.0, firstPending + pendingCount); ++i) {
                letters[size_t(i)] = {i, 1, false, (pendingPhase - 0.5) * 2, submissions(i, 1)};
            }
        }
        const int ov = images.png("OV_1-4");
        c.save();
        c.scale(1, 448.0 / 480);
        for (const auto& item : letters) {
            if (!item.draws) {
                continue;
            }
            const double cx = item.letter < 4 ? 558 + 16 * item.letter : 550 + 16 * (item.letter - 4);
            const double cy = item.letter < 4 ? 90 : 68;
            c.save();
            c.translate(cx, cy);
            c.scale(item.scale, item.scale);
            c.translate(-cx, -cy);
            std::vector<const nlohmann::json*> draws;
            for (const auto& d : *item.draws) {
                draws.push_back(&d);
            }
            std::stable_sort(draws.begin(), draws.end(), [](const nlohmann::json* a, const nlohmann::json* b) { return (*a)["order"].get<double>() < (*b)["order"].get<double>(); });
            for (const auto* dp : draws) {
                const auto& d = *dp;
                c.save();
                c.setGlobalAlpha(std::trunc(d["argb"][0].get<double>() * 128) / 128);
                if (d.contains("text")) {
                    std::string colour = "#";
                    for (int k = 1; k < 4; ++k) {
                        colour += hex2(int(jsRound(std::trunc(d["argb"][size_t(k)].get<double>() * 128) * 255 / 128)));
                    }
                    c.translate(d["position"][0].get<double>(), d["position"][1].get<double>());
                    c.scale(d["scale"][0].get<double>(), d["scale"][1].get<double>());
                    ui.text(c, d["text"].get<std::string>(), 0, 0, 17, colour, "HUDFONT");
                } else {
                    c.translate(cx, cy);
                    c.scale(item.outlineScaleX, 1);
                    c.translate(-cx, -cy);
                    // [v, u, u1, v1]
                    const double v = d["uv"][0].get<double>();
                    const double u = d["uv"][1].get<double>();
                    const double u1 = d["uv"][2].get<double>();
                    const double v1 = d["uv"][3].get<double>();
                    c.drawImage(ov, u * 256, v * 256, (u1 - u) * 256, (v1 - v) * 256, d["position"][0].get<double>(), d["position"][1].get<double>(),
                                d["size"][0].get<double>() * d["scale"][0].get<double>(), d["size"][1].get<double>() * d["scale"][1].get<double>());
                }
                c.restore();
            }
            c.restore();
        }
        c.restore();
    }

private:
    struct Orb {
        Vec2 position;
        Vec2 size;
        std::array<double, 4> uv;
        int palette;
        Argb argb;
    };
    struct Glow {
        Vec2 position;
        Vec2 size;
        int palette;
        Argb argb;
    };
    struct Rect {
        double x, y, w, h, u, v, uw, vh;
    };
    static Rect rect(const nlohmann::json& r) {
        return {r["x"].get<double>(), r["y"].get<double>(), r["w"].get<double>(), r["h"].get<double>(),
                r["u"].get<double>(), r["v"].get<double>(), r["uw"].get<double>(), r["vh"].get<double>()};
    }
    Orb orb(double phase, double tier) const {
        const auto& o = p_["orb"];
        const bool active = phase >= 0;
        const double s0 = o["size"][0].get<double>();
        const double s1 = o["size"][1].get<double>();
        const double width = s0 * (active ? (phase < 1 ? phase : 2 - phase) : 1);
        const int palette = tier < 5 ? 0 : tier < 10 ? 1 : tier == 10 ? 2 : 3;
        Orb r;
        r.position = {o["position"][0].get<double>() + (s0 - width) / 2, o["position"][1].get<double>()};
        r.size = {width, s1};
        r.uv = {o["uv"][0].get<double>(), o["uv"][1].get<double>(), o["uv"][2].get<double>(), o["uv"][3].get<double>()};
        r.palette = active ? palette : -1;
        if (active) {
            const auto& fc = p_["flashColours"][size_t(palette)];
            r.argb = {fc[0].get<double>(), fc[1].get<double>(), fc[2].get<double>(), fc[3].get<double>()};
        } else {
            r.argb = {1, 1, 1, 1};
        }
        return r;
    }
    std::optional<Glow> orbGlow(double phase, double tier) const {
        if (phase < 0) {
            return std::nullopt;
        }
        const Orb o = orb(phase, tier);
        const auto& g = p_["orbGlow"];
        const double pulse = phase > 1 ? 2 - phase : phase;
        const double padding = g["padding"].get<double>();
        const double extent = g["extent"].get<double>();
        const double lo = g["alphaLow"].get<double>();
        const double hi = g["alphaHigh"].get<double>();
        Glow r;
        r.position = {o.position[0] - padding, o.position[1] - padding};
        r.size = {o.size[0] + extent, o.size[1] + extent};
        r.palette = o.palette;
        r.argb = {o.argb[0] * (lo + (hi - lo) * pulse), o.argb[1], o.argb[2], o.argb[3]};
        return r;
    }
    // 21D1A0's bottom-up clip
    std::vector<Rect> gaugeRects(const nlohmann::json& layer, double fraction) const {
        const double fr = std::max(0.0, std::min(1.0, std::isfinite(fraction) ? fraction : 0));
        std::vector<Rect> out;
        if (fr == 0) {
            return out;
        }
        const double top = p_["bottom"].get<double>() - p_["fillHeight"].get<double>() * fr;
        for (const auto& rj : layer["rects"]) {
            Rect r = rect(rj);
            if (!(r.y + r.h > top)) {
                continue;
            }
            const double cut = fr == 1 ? 0 : std::max(0.0, top - r.y);
            Rect o = r;
            o.y = r.y + cut;
            o.h = r.h - cut;
            o.v = r.v + r.vh * cut / r.h;
            o.vh = r.vh * (1 - cut / r.h);
            out.push_back(o);
        }
        return out;
    }
    // the ordinary type 6 palette (1ECD60)
    Argb gaugeColour(double fr) const {
        const auto& cs = p_["colours"];
        auto colour = [&](int k) {
            return Argb{cs[size_t(k)][0].get<double>(), cs[size_t(k)][1].get<double>(), cs[size_t(k)][2].get<double>(), cs[size_t(k)][3].get<double>()};
        };
        const Argb a = colour(0);
        const Argb b = colour(1);
        const Argb cc = colour(2);
        Argb lo, hi;
        double t;
        if (fr <= .3500423729419708) {
            return a;
        }
        if (fr < .4500329792499542) {
            lo = a;
            hi = b;
            t = ((fr - .4000376760959625) + .0499952957034111) * 10.000941276550293;
        } else if (fr <= .7499593496322632) {
            return b;
        } else if (fr < .8499499559402466) {
            lo = b;
            hi = cc;
            t = ((fr - .7999546527862549) + .0499952957034111) * 10.000941276550293;
        } else {
            return cc;
        }
        Argb out;
        for (int i = 0; i < 4; ++i) {
            out[size_t(i)] = lo[size_t(i)] + (hi[size_t(i)] - lo[size_t(i)]) * t;
        }
        return out;
    }
    // tinted(argb): the atlas ImageData x clamp(trunc(x x 128), 0, 128) / 128 per channel
    int tinted(const Argb& argb) {
        const double k[4] = {argb[1], argb[2], argb[3], argb[0]};
        int q[4];
        for (int i = 0; i < 4; ++i) {
            q[i] = int(std::max(0.0, std::min(128.0, std::trunc(k[i] * 128))));
        }
        const std::string key = "gauge:" + std::to_string(q[0]) + "," + std::to_string(q[1]) + "," + std::to_string(q[2]) + "," + std::to_string(q[3]);
        return images_->imageData(key, atlas_, [&](uint8_t v, int ch) { return clampByte(v * double(q[ch]) / 128); });
    }

    nlohmann::json p_;
    HudImages* images_ = nullptr;
    int atlas_ = -1;
    int glow_ = -1;
    int red_ = -1;
    std::vector<int> orbTints_;
    std::vector<int> glowTints_;
    std::vector<int> glowColumns_;
};

// web/progress-meter-hud.js ProgressMeter (the course's progress-meter.json; EE float arithmetic)
class ProgressMeter {
public:
    struct Rider {
        float remaining = 0;
        float x = 0;
        float y = 0;
        bool human = false;
        bool hasPlacements = false;
        float placements = 0;
    };
    bool load(const nlohmann::json& data) {
        total_ = float(data["total"].get<double>());
        segments_.clear();
        for (const auto& s : data["segments"]) {
            std::array<float, 5> seg{};
            for (int k = 0; k < 5; ++k) {
                seg[size_t(k)] = float(s[size_t(k)].get<double>());
            }
            segments_.push_back(seg);
        }
        markers_.clear();
        for (const auto& m : data["markers"]) {
            markers_.push_back({m[0].get<int>(), float(m[1].get<double>())});
        }
        reset();
        return !segments_.empty();
    }
    // 0x2108F8
    void reset() {
        for (auto& e : entries_) {
            e = Entry{};
        }
        pending_ = true;
        placements_.clear();
        count_ = -1;
    }
    void tick(const std::vector<Rider>& riders, int humanSlot = 0, bool drawn = true) {
        placements_.resize(std::max(placements_.size(), riders.size()));
        for (size_t i = 0; i < riders.size(); ++i) {
            const Rider& r = riders[i];
            auto& known = placements_[i];
            if (r.hasPlacements && known.first && r.placements != known.second) {
                place(int(i));
            }
            known = {r.hasPlacements, r.placements};
        }
        for (size_t i = 0; i < riders.size(); ++i) {
            updateEntry(int(i), riders[i]);
            entries_[i].reset = 0;
        }
        count_ = int(riders.size());
        humanSlot_ = humanSlot;
        humans_.assign(riders.size(), false);
        for (size_t i = 0; i < riders.size(); ++i) {
            humans_[i] = riders[i].human;
        }
        pending_ = false;
        if (drawn) {
            smooth();
        }
    }
    void draw(HudCanvas& c, HudImages& images, const TrickHud& trick, TrickHudRenderer& renderer, bool showCheckpoints = true) {
        using namespace ee;
        if (pending_ || count_ < 0) {
            return;
        }
        struct Item {
            int kind;
            int order;
            std::string text;
            float x, y, scale;
            std::string name, page;
            float x0, x1, y0, y1, u0, u1, v0, v1;
        };
        std::vector<Item> out;
        const int hs = humanSlot_;
        const float self = entries_[size_t(hs)].progress;
        const float lo = sub(self, kWindowBack);
        const float hi = add(lo, kWindow);
        const float q = div(entries_[size_t(hs)].progress, total_);
        {
            Item t{};
            t.kind = 2;
            t.order = 10;
            t.text = std::to_string(int32_t(std::trunc(add(mul(q, 100), 0.5f)))) + "%";
            t.x = kTextX;
            t.y = kTextY;
            t.scale = kTextScale;
            out.push_back(t);
        }
        auto quad = [&](const std::string& name, float x0, float x1, float y0, float y1, float v0, float v1, int order) {
            const Sprite& s = sprite(name);
            Item it{};
            it.kind = 0;
            it.page = s.page;
            it.name = name;
            it.x0 = x0;
            it.x1 = x1;
            it.y0 = y0;
            it.y1 = y1;
            it.u0 = s.uv[1];
            it.u1 = s.uv[2];
            it.v0 = v0;
            it.v1 = v1;
            it.order = order;
            out.push_back(it);
        };
        const Sprite& off = sprite("offup");
        const float offHalf = mul(off.w, 0.5f);
        for (int i = 0; i < count_; ++i) {
            const float p = entries_[size_t(i)].progress;
            if (lo <= p && p <= hi) {
                continue;
            }
            const float x = add(kBarX, mul(-entries_[size_t(i)].smooth, kLatX));
            const bool ahead = hi < p;
            const float y0 = ahead ? kTop : kBottom;
            quad("offup", sub(x, offHalf), add(x, offHalf), y0, add(y0, off.h), ahead ? off.uv[0] : off.uv[3], ahead ? off.uv[3] : off.uv[0], 10);
        }
        float yBottom = kBarBottom;
        float yTop = kBarTop;
        if (total_ < hi) {
            yTop = add(yTop, mul(sub(hi, total_), kScale));
        }
        if (lo < 0) {
            yBottom = add(yBottom, mul(lo, kScale));
        }
        const Sprite& bar = sprite("radarline");
        quad("radarline", sub(kBarX, kBarHalf), add(kBarX, kBarHalf), yTop, yBottom, bar.uv[3], bar.uv[0], 9);
        {
            Item r{};
            r.kind = 1;
            r.x0 = sub(kBarX, kFillHalf);
            r.x1 = add(kBarX, kFillHalf);
            r.y0 = add(yBottom, mul(sub(yTop, yBottom), q));
            r.y1 = yBottom;
            r.order = 10;
            out.push_back(r);
        }
        const float grow = div(kGrowPx, kScale);
        const float hi2 = add(hi, grow);
        const float lo2 = sub(lo, grow);
        auto clip = [&](const Sprite& s, float y, float vTop, float vBottom, bool twoH, std::array<float, 4>& r) {
            const float half = mul(s.h, 0.5f);
            float top = sub(y, half);
            float bottom = add(y, half);
            if (kBarBottom <= top || bottom <= kBarTop) {
                return false;
            }
            if (top < kBarTop) {
                vTop = add(vBottom, div(mul(sub(vTop, vBottom), sub(bottom, kBarTop)), twoH ? add(half, half) : s.h));
                top = kBarTop;
            } else if (kBarBottom < bottom) {
                vBottom = add(vTop, div(mul(sub(vBottom, vTop), sub(kBarBottom, top)), twoH ? add(half, half) : s.h));
                bottom = kBarBottom;
            }
            r = {top, bottom, vTop, vBottom};
            return true;
        };
        for (int i = 0; i < count_; ++i) {
            const float p = entries_[size_t(i)].progress;
            if (p < lo2 || hi2 < p) {
                continue;
            }
            const float y = sub(kBarBottom, mul(sub(p, lo), kScale));
            float x = kBarX;
            std::string name;
            int order;
            if (i == hs) {
                name = "ply";
                order = 12;
            } else {
                x = sub(x, mul(entries_[size_t(i)].smooth, kLatX));
                name = humans_[size_t(i)] ? "enply" : "encpu";
                order = humans_[size_t(i)] ? 13 : 14;
            }
            const Sprite& s = sprite(name);
            std::array<float, 4> r;
            if (!clip(s, y, s.uv[0], s.uv[3], true, r)) {
                continue;
            }
            const float hw = mul(s.w, 0.5f);
            quad(name, sub(x, hw), add(x, hw), r[0], r[1], r[2], r[3], order);
        }
        static const char* markerSprite[3] = {"chkstart", "chkpt", "chkstart"};
        for (const auto& [type, d] : markers_) {
            if (d < lo2 || hi2 < d) {
                continue;
            }
            if (type == 1 && !showCheckpoints) {
                continue;
            }
            if (type < 0 || type > 2) {
                continue;
            }
            const std::string name = markerSprite[type];
            const Sprite& s = sprite(name);
            const float y = sub(kBarBottom, mul(sub(d, lo), kScale));
            std::array<float, 4> r;
            if (!clip(s, y, s.uv[0], s.uv[3], false, r)) {
                continue;
            }
            const float hw = mul(s.w, 0.5f);
            quad(name, sub(kBarX, hw), add(kBarX, hw), r[0], r[1], r[2], r[3], 11);
        }
        // the canvas draw (the submissions by GS priority, text above sprites; stable within)
        c.save();
        c.scale(1, 448.0 / 480);
        c.setGlobalAlpha(1);
        std::vector<size_t> idx(out.size());
        for (size_t k = 0; k < idx.size(); ++k) {
            idx[k] = k;
        }
        auto rank = [&](const Item& d) { return d.order * 2 + (d.kind == 2 ? 1 : 0); };
        std::stable_sort(idx.begin(), idx.end(), [&](size_t a, size_t b) { return rank(out[a]) < rank(out[b]); });
        for (size_t k : idx) {
            const Item& d = out[k];
            if (d.kind == 1) {
                c.fillRect(d.x0, d.y0, double(d.x1) - d.x0, double(d.y1) - d.y0, {128, 0, 0});
            } else if (d.kind == 0) {
                const int im = images.png(d.page);
                const HudImage& image = c.image(im);
                const double W = image.width;
                const double H = image.height;
                const bool flip = d.v1 < d.v0;
                const double sy = std::min(d.v0, d.v1) * H;
                const double sh = std::abs(double(d.v1) - d.v0) * H;
                if (sh <= 0 || d.y1 <= d.y0) {
                    continue;
                }
                c.save();
                c.translate(d.x0, flip ? d.y1 : d.y0);
                c.scale(1, flip ? -1 : 1);
                c.drawImage(im, d.u0 * W, sy, (double(d.u1) - d.u0) * W, sh, 0, 0, double(d.x1) - d.x0, double(d.y1) - d.y0);
                c.restore();
            } else {
                const double w = trick.measure("FEFONT", d.text, d.scale, d.scale).w;
                const float x = sub(d.x, mul(float(w), 0.5f));
                Draw t;
                t.font = "FEFONT";
                t.text = d.text;
                t.x = x;
                t.y = d.y;
                t.scale = {d.scale, d.scale};
                renderer.text(c, t, 2, 2, {1, 0, 0, 0});
                renderer.text(c, t, 0, 0, {1, 1, 1, 1});
            }
        }
        c.restore();
    }

private:
    struct Entry {
        int reset = 1;
        int seg = 0;
        float progress = 0;
        float lateral = 0;
        float smooth = 0;
        int flag = 0;
    };
    struct Sprite {
        float w, h;
        std::string page;
        std::array<float, 4> uv;
    };
    static const Sprite& sprite(const std::string& name) {
        static const std::map<std::string, Sprite> s = {
            {"radarline", {8, 8, "OV_1-4", {0.517578125f, 0.583984375f, 0.677734375f, 0.537109375f}}},
            {"ply", {43, 23, "OV_1-3", {0.505859375f, 0.677734375f, 0.818359375f, 0.583984375f}}},
            {"encpu", {30, 18, "OV_1-3", {0.935546875f, 0.595703125f, 0.689453125f, 0.994140625f}}},
            {"enply", {24, 15, "OV_1-3", {0.857421875f, 0.595703125f, 0.689453125f, 0.912109375f}}},
            {"offup", {20, 12, "OV_1-3", {0.009765625f, 0.755859375f, 0.845703125f, 0.060546875f}}},
            {"chkstart", {40, 8, "OV_1-4", {0.630859375f, 0.861328125f, 0.998046875f, 0.650390625f}}},
            {"chkpt", {40, 8, "OV_1-4", {0.607421875f, 0.861328125f, 0.998046875f, 0.626953125f}}},
        };
        return s.at(name);
    }
    void place(int slot) {
        if (slot >= 0 && slot < 6) {
            entries_[size_t(slot)].reset = 1;
        }
    }
    void updateEntry(int slot, const Rider& r) {
        using namespace ee;
        Entry& e = entries_[size_t(slot)];
        const size_t n = segments_.size();
        float p = sub(total_, r.remaining);
        if (!e.reset) {
            const float d = sub(p, e.progress);
            if (kStep < d) {
                p = add(e.progress, kStep);
            } else if (d < -kStep) {
                p = sub(e.progress, kStep);
            }
        }
        if (p <= 0) {
            e.seg = 0;
            e.progress = 0;
        } else if (segments_[n - 1][4] <= p) {
            e.seg = int(n) - 1;
            e.progress = total_;
        } else {
            int a = e.seg;
            if (segments_[size_t(a)][4] <= p) {
                do {
                    a++;
                } while (segments_[size_t(a)][4] <= p);
            }
            if (p < segments_[size_t(a)][4]) {
                do {
                    a--;
                } while (p < segments_[size_t(a)][4]);
            }
            e.seg = a;
            e.progress = p;
        }
        const auto& s = segments_[size_t(e.seg)];
        e.lateral = add(mul(sub(r.y, s[3]), s[1]), mul(sub(r.x, s[2]), s[0]));
        if (e.reset) {
            e.flag = 0;
        }
    }
    void smooth() {
        using namespace ee;
        const float self = entries_[size_t(humanSlot_)].lateral;
        for (int i = 0; i < 6; ++i) {
            if (i >= count_) {
                continue;
            }
            Entry& e = entries_[size_t(i)];
            float d = sub(e.lateral, self);
            if (d < -kLatClamp) {
                d = -kLatClamp;
            } else if (kLatClamp < d) {
                d = kLatClamp;
            }
            if (e.flag) {
                const float g = sub(d, e.smooth);
                if (kLatStep < g) {
                    d = add(e.smooth, kLatStep);
                } else if (g < -kLatStep) {
                    d = sub(e.smooth, kLatStep);
                }
            }
            e.smooth = d;
            e.flag = 1;
        }
    }
    static constexpr float kStep = 138.88890075683594f;
    static constexpr float kLatClamp = 2000;
    static constexpr float kLatStep = 46.29629898071289f;
    static constexpr float kLatX = 0.005000000353902578f;
    static constexpr float kWindowBack = 5000;
    static constexpr float kWindow = 10000;
    static constexpr float kGrowPx = 20;
    static constexpr float kBarHalf = 5.499532222747803f;
    static constexpr float kFillHalf = 2.7497661113739014f;
    static constexpr float kTextScale = 0.666700005531311f;
    static constexpr float kTextX = 60;
    static constexpr float kTextY = 330.66571044921875f;
    static constexpr float kTop = 110;
    static constexpr float kBottom = 318.66571044921875f;
    static constexpr float kBarX = 60;
    static constexpr float kBarTop = 122;
    static constexpr float kBarBottom = 318.66571044921875f;
    static constexpr float kScale = 0.01966656930744648f;

    float total_ = 0;
    std::vector<std::array<float, 5>> segments_;
    std::vector<std::pair<int, float>> markers_;
    std::array<Entry, 6> entries_{};
    bool pending_ = true;
    std::vector<std::pair<bool, float>> placements_;
    int count_ = -1;
    int humanSlot_ = 0;
    std::vector<bool> humans_;
};

}  // namespace hud

}  // namespace ssx_native
