#pragma once
// The original single-player trick-scoring HUD (web/trick-hud.js TrickHud, 1:1; docs/tricks-scoring.md): the slot cases of the
// HUD draw 0x1EC3F8 for the score object's message bank, the race clock 0x1F16C0, the speed widget 0x2200C0, the switch icon,
// the finish banner 0x21F660, the big centre message, the Uber hint and the crash recover meter, as a draw list in PS2 640 x 480
// HUD space. JS numbers are doubles: the arithmetic here is in double, chopped to float32 exactly where the page's f() chops.
// The freestyle standings (TrickHud.standings, career-ui.js) are not ported: no Single Event race draws them.
#include "ee_float.hpp"
#include "json.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ssx_native {

namespace hud {

// the page's f(): an exact double chopped to float32 (round toward zero), kept as a double
inline double f(double v) {
    if (!std::isfinite(v)) {
        return v;
    }
    return double(ee::chop(v));
}
inline double fround(double v) {
    return double(float(v));
}
// JS ToInt32
inline int32_t toInt32(double v) {
    if (!std::isfinite(v)) {
        return 0;
    }
    const double t = std::trunc(v);
    const double m = std::fmod(t, 4294967296.0);
    const double u = m < 0 ? m + 4294967296.0 : m;
    return int32_t(uint32_t(u));
}
// UTF-8 code points (JS iterates strings by code point)
inline std::vector<char32_t> codePoints(const std::string& s) {
    std::vector<char32_t> out;
    for (size_t i = 0; i < s.size();) {
        const uint8_t c = uint8_t(s[i]);
        char32_t cp = c;
        int n = 1;
        if (c >= 0xF0) {
            cp = c & 0x07;
            n = 4;
        } else if (c >= 0xE0) {
            cp = c & 0x0F;
            n = 3;
        } else if (c >= 0xC0) {
            cp = c & 0x1F;
            n = 2;
        }
        for (int k = 1; k < n && i + size_t(k) < s.size(); ++k) {
            cp = (cp << 6) | (uint8_t(s[i + size_t(k)]) & 0x3F);
        }
        out.push_back(cp);
        i += size_t(n);
    }
    return out;
}
inline std::string utf8(char32_t cp) {
    std::string s;
    if (cp < 0x80) {
        s += char(cp);
    } else if (cp < 0x800) {
        s += char(0xC0 | (cp >> 6));
        s += char(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        s += char(0xE0 | (cp >> 12));
        s += char(0x80 | ((cp >> 6) & 0x3F));
        s += char(0x80 | (cp & 0x3F));
    } else {
        s += char(0xF0 | (cp >> 18));
        s += char(0x80 | ((cp >> 12) & 0x3F));
        s += char(0x80 | ((cp >> 6) & 0x3F));
        s += char(0x80 | (cp & 0x3F));
    }
    return s;
}
// String(n) of an integer-valued double
inline std::string intString(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.0f", v);
    return b;
}
// JS String(number) for the values the HUD prints (float32 slot points: integers in practice)
inline std::string jsNumberString(double v) {
    if (std::isfinite(v) && v == std::trunc(v) && std::abs(v) < 1e21) {
        return intString(v);
    }
    // shortest round-trip, as JS prints a non-integer
    char b[40];
    for (int p = 1; p <= 17; ++p) {
        std::snprintf(b, sizeof b, "%.*g", p, v);
        if (std::strtod(b, nullptr) == v) {
            break;
        }
    }
    return b;
}
inline std::string pad2(int64_t n) {
    char b[24];
    std::snprintf(b, sizeof b, "%02lld", static_cast<long long>(n));
    return b;
}

struct Glyph {
    int x = 0, y = 0, w = 0, h = 0;
    int advance = 0;
    int dx = 0;
    int dy = 0;
};
// a glyph table keyed by code point; `signedFix` as TrickHud's constructor (values over 127 are negative bytes)
using GlyphTable = std::map<char32_t, Glyph>;
inline GlyphTable loadGlyphs(const nlohmann::json& j, bool signedFix) {
    GlyphTable t;
    for (auto it = j.begin(); it != j.end(); ++it) {
        const auto cps = codePoints(it.key());
        if (cps.size() != 1) {
            continue;
        }
        const auto& g = it.value();
        Glyph q;
        q.x = g.value("x", 0);
        q.y = g.value("y", 0);
        q.w = g.value("w", 0);
        q.h = g.value("h", 0);
        q.advance = g.value("advance", 0);
        q.dx = g.value("dx", 0);
        q.dy = g.value("dy", 0);
        if (signedFix) {
            q.dy = q.dy > 127 ? q.dy - 256 : q.dy;
            q.dx = q.dx > 127 ? q.dx - 256 : q.dx;
            q.advance = q.advance > 127 ? q.advance - 256 : q.advance;
        }
        t[cps[0]] = q;
    }
    return t;
}

using Vec2 = std::array<double, 2>;
using Argb = std::array<double, 4>;

struct SpriteRef {
    std::string page;
    // v0, u0, u1, v1 (as the owner sprite records store them)
    std::array<double, 4> uv{};
};

// one draw of the list (web/trick-hud.js out.push records)
struct Draw {
    enum Kind { Text, Sprite, Rect, Quad, Recover } kind = Text;
    std::string font;
    std::string text;
    double x = 0;
    double y = 0;
    Vec2 scale{1, 1};
    Argb argb{};
    Vec2 shadow{0, 0};
    std::optional<Argb> shadowArgb;
    int order = 0;
    std::optional<SpriteRef> sprite;
    // the sprite's size (`size`), or w / h (stars, rects)
    std::optional<Vec2> size;
    double w = 0;
    double h = 0;
    // a key cap label in the icon's place (keyboard)
    std::string key;
    // the placeholder recover bar
    double fill = 0;
};

struct Descriptor {
    double x = 0, y = 0, w = 0, h = 0;
    Vec2 scale{1, 1};
    Argb argb{1, 1, 1, 1};
    std::array<int, 2> align{-1, -1};
    int order = 0;
};

// a score-object message slot (game-tick.js readTrickHudSlots)
struct Slot {
    double type = 0x34;
    double maximum = 0;
    double value = 0;
    double arg = 0;
    double field10 = 0;
    double points = 0;
    std::string text;
};
using Slots = std::vector<std::optional<Slot>>;

struct FrameState {
    std::optional<uint32_t> flags;
    int players = 1;
    std::optional<double> flash;
    std::optional<double> pulse25;
    std::optional<double> pulse70;
    double extraPoints7C = 0;
    bool finished = false;
    std::optional<int> collectMode;
    bool bigMessage = true;
    // keys(button) -> the key cap label while the keyboard is the device ('' when none)
    std::function<std::string(const std::string&)> keys;
    const Slots* prepassSlots = nullptr;
};

struct TextOpts {
    std::optional<Vec2> scale;
    std::optional<int> alignH;
    std::optional<int> alignV;
    std::optional<Vec2> offset;
    std::optional<Argb> colour;
};
struct Box {
    double x = 0, y = 0, w = 0, h = 0;
};

constexpr double kFree = 0x34;
// 1/60 s (0x3C888889)
inline const double kHudDt = fround(0.01666666753590107);
constexpr int kFontHeightFE = 29;
constexpr int kFontHeightHUD = 21;
inline int fontHeight(const std::string& font) {
    return font == "FEFONT" ? kFontHeightFE : kFontHeightHUD;
}
// a font draw's vertex RGB byte (renderer 0x378808: trunc(colour x 204.0))
inline int hudTextByte(double v) {
    return int(std::max(0.0, std::min(255.0, std::trunc(fround(v * 204)))));
}
// 0x198AF0: "$ n" with thousands separators
inline std::string money(double n) {
    const std::string digits = intString(std::max(0.0, std::trunc(n)));
    std::string out;
    const size_t len = digits.size();
    for (size_t i = 0; i < len; ++i) {
        out += digits[i];
        const size_t left = len - 1 - i;
        if (left > 0 && left % 3 == 0) {
            out += ',';
        }
    }
    return "$ " + out;
}

// web/input-glyphs.js keyCapWidth (h = 16, stretch LUI_STRETCH in the trick HUD)
constexpr double kPs2X = 640.0 / 512;
constexpr double kPs2Y = 480.0 / 448;
constexpr double kLuiStretch = kPs2X / kPs2Y;
constexpr double kCapHeight = 0.85;
constexpr double kMinCap = 14;
inline const double kTextK = 22 * 0.79 * 0.36 * kPs2Y / 16;
inline double keyCapWidth(const GlyphTable* glyphs, const std::string& label, double h, double stretch = kPs2X) {
    const auto cps = codePoints(label);
    if (cps.size() == 1) {
        return 17 * h / 16;
    }
    double advance = 0;
    for (char32_t ch : cps) {
        int a = 0;
        if (glyphs) {
            auto it = glyphs->find(ch);
            if (it != glyphs->end()) {
                a = it->second.advance;
            }
        }
        // `advance || 10`
        advance += a != 0 ? a : 10;
    }
    return advance * kTextK * h / 22 * stretch + 10 * h / 16;
}

class TrickHud {
public:
    void load(const nlohmann::json& data, const GlyphTable& fe, const GlyphTable& hudFont) {
        d_.clear();
        for (const auto& e : data["descriptors"]) {
            Descriptor q;
            q.x = e.value("x", 0.0);
            q.y = e.value("y", 0.0);
            q.w = e.value("w", 0.0);
            q.h = e.value("h", 0.0);
            q.scale = {e["scale"][0].get<double>(), e["scale"][1].get<double>()};
            for (int k = 0; k < 4; ++k) {
                q.argb[size_t(k)] = e["argb"][size_t(k)].get<double>();
            }
            q.align = {e["align"][0].get<int>(), e["align"][1].get<int>()};
            q.order = e.value("order", 0);
            d_.push_back(q);
        }
        const auto& sp = data["sprites"];
        auto sprite = [](const nlohmann::json& s) {
            SpriteRef r;
            r.page = s.value("page", "");
            for (int k = 0; k < 4; ++k) {
                r.uv[size_t(k)] = s["uv"][size_t(k)].get<double>();
            }
            return r;
        };
        for (auto it = sp.begin(); it != sp.end(); ++it) {
            if (it.value().is_array()) {
                std::vector<SpriteRef> list;
                for (const auto& s : it.value()) {
                    list.push_back(sprite(s));
                }
                spriteLists_[it.key()] = list;
            } else {
                sprites_[it.key()] = sprite(it.value());
            }
        }
        const auto& cs = data["colours"];
        for (auto it = cs.begin(); it != cs.end(); ++it) {
            const auto& v = it.value();
            if (!v.empty() && v[0].is_array()) {
                std::vector<Argb> list;
                for (const auto& c : v) {
                    list.push_back({c[0].get<double>(), c[1].get<double>(), c[2].get<double>(), c[3].get<double>()});
                }
                colourLists_[it.key()] = list;
            } else {
                colours_[it.key()] = {v[0].get<double>(), v[1].get<double>(), v[2].get<double>(), v[3].get<double>()};
            }
        }
        for (const auto& v : data["sin"]) {
            sin_.push_back(v.get<double>());
        }
        strings_ = data.value("strings", nlohmann::json::object());
        flags = uint32_t(data["flags"].get<double>());
        if (data.contains("clock") && data["clock"].is_object()) {
            const auto& c = data["clock"];
            clock_ = {c.value("gap", 3.0), c.value("pair", 37.0), c.value("sign", 11.0), c.value("one", 4.75)};
        }
        glyphs_["FEFONT"] = fe;
        glyphs_["HUDFONT"] = hudFont;
        font_["FEFONT"] = {2, 2};
        font_["HUDFONT"] = {2, 2};
        big_ = {0x50, 0, 0};
        bigSeen_.clear();
        prepass_.reset();
    }

    uint32_t flags = 0;
    const GlyphTable& glyphs(const std::string& font) const {
        return glyphs_.at(font);
    }

    // the host hands over the bank as the next game tick starts (web/game-tick.js present)
    void setPrepassSlots(const std::optional<Slots>& slots) {
        prepass_ = slots;
        tickBigMessage(prepass_ ? &*prepass_ : nullptr);
    }
    const Slots* prepassSlots() const {
        return prepass_ ? &*prepass_ : nullptr;
    }
    void tickBigMessage(const Slots* slots) {
        auto& m = big_;
        if (m.id != 0x50) {
            if (1 <= m.t) {
                m.id = 0x50;
                m.t = 0;
            } else {
                const double t = f(m.t + kHudDt);
                m.t = 1 < t ? 1 : t;
            }
        }
        if (!slots) {
            bigSeen_[0x1a] = -1;
            bigSeen_[0x1b] = -1;
            return;
        }
        auto fresh = [&](int k) {
            const auto& s = size_t(k) < slots->size() ? (*slots)[size_t(k)] : std::optional<Slot>();
            const double v = s && s->type != kFree ? s->value : -1;
            auto it = bigSeen_.find(k);
            const double before = it == bigSeen_.end() ? -1 : it->second;
            bigSeen_[k] = v;
            return v >= 0 && v <= kHudDt && !(before >= 0 && before <= v);
        };
        const bool go = fresh(0x1a);
        const bool success = fresh(0x1b);
        if (go) {
            m.id = 8;
            m.t = 0;
            m.cash = 0;
        } else if (success) {
            m.id = 0x3a;
            m.t = 0;
            m.cash = double(toInt32((*slots)[0x1b]->points));
        }
    }
    void command(int k) {
        if (k == 6) {
            big_.id = 8;
            big_.t = 0;
            big_.cash = 0;
        }
    }
    void resetUberHint(uint32_t bit) {
        hint55C_ = int(bit & 1);
    }

    static double align(double x, double size, double scale, int a) {
        if (a == 0) {
            return f(x - f(f(scale * size) * 0.5));
        }
        if (a > 0) {
            return f(x - f(scale * size));
        }
        return x;
    }

    // 0x391FB0
    Box measure(const std::string& font, const std::string& text, double sx, double sy, bool advanceOnly = false) const {
        const auto& g = glyphs_.at(font);
        double pen = 0, minX = 0, minY = 0, maxX = 0, maxY = 0;
        for (char32_t ch : codePoints(text)) {
            auto it = g.find(ch);
            if (it == g.end()) {
                continue;
            }
            const Glyph& q = it->second;
            minX = std::min(f(pen + f(q.dx * sx)), minX);
            const double adv = f(q.advance * sy == 0 ? 0 : q.advance * sx);
            minY = std::min(f(q.dy * sy), minY);
            maxY = std::max(f((q.dy + q.h) * sy), maxY);
            if (!advanceOnly) {
                const double next = f(pen + adv);
                maxX = std::max(f(pen + f((q.dx + q.w) * sx)), maxX);
                pen = next;
            } else {
                pen = f(pen + adv);
                maxX = std::max(pen, maxX);
            }
        }
        double w = f(maxX + minX);
        double h = f(maxY + minY);
        const Vec2& sh = font_.at(font);
        w = sh[0] < 0 ? f(w - sh[0]) : f(w + sh[0]);
        h = sh[1] < 0 ? f(h - sh[1]) : f(h + sh[1]);
        return {0, 0, w, h};
    }

    void switchIcon(std::vector<Draw>& out, bool regular) const {
        auto it = sprites_.find("switchIcon");
        if (it == sprites_.end()) {
            return;
        }
        const Descriptor& d = desc(0x4f);
        Argb argb = d.argb;
        if (regular) {
            argb[0] = f(argb[0] * f(0.200000003));
        }
        const double w = f(d.w * d.scale[0]);
        const double h = f(d.h * d.scale[1]);
        Draw q;
        q.kind = Draw::Sprite;
        q.sprite = it->second;
        q.x = align(d.x, w, 1, d.align[0]);
        q.y = align(d.y, h, 1, d.align[1]);
        q.size = Vec2{w, h};
        q.scale = {1, 1};
        q.argb = argb;
        q.order = d.order;
        out.push_back(q);
    }

    // 0x1F1B30
    Box text(std::vector<Draw>& out, const std::string& font, int k, const std::string& text, const TextOpts& o = {}) const {
        const Descriptor& d = desc(k);
        const Vec2 scale = o.scale ? Vec2{f(d.scale[0] * (*o.scale)[0]), f(d.scale[1] * (*o.scale)[1])} : d.scale;
        const Box size = measure(font, text, scale[0], scale[1]);
        const int ah = o.alignH ? *o.alignH : d.align[0];
        const int av = o.alignV ? *o.alignV : d.align[1];
        double x = align(d.x, size.w, 1, ah);
        double y = align(d.y, size.h, 1, av);
        if (o.offset) {
            x = f(x + (*o.offset)[0]);
            y = f(y + (*o.offset)[1]);
        }
        const Argb argb = o.colour ? *o.colour : d.argb;
        Draw q;
        q.kind = Draw::Text;
        q.font = font;
        q.text = text;
        q.x = x;
        q.y = y;
        q.scale = scale;
        q.argb = argb;
        q.shadow = font_.at(font);
        q.shadowArgb = Argb{argb[0], 0, 0, 0};
        q.order = d.order;
        out.push_back(q);
        return {x, y, size.w, size.h};
    }

    // 0x1F1E28
    Box textLerp(std::vector<Draw>& out, const std::string& font, int a, int b, double t, const std::string& text, const TextOpts& o = {}) const {
        const Box size = measure(font, text, 1, 1);
        const Descriptor& da = desc(a);
        const Descriptor& db = desc(b);
        const Vec2 sa = o.scale ? Vec2{f(da.scale[0] * (*o.scale)[0]), f(da.scale[1] * (*o.scale)[1])} : da.scale;
        const Vec2 sb = o.scale ? Vec2{f(db.scale[0] * (*o.scale)[0]), f(db.scale[1] * (*o.scale)[1])} : db.scale;
        auto ah = [&](const Descriptor& d) { return o.alignH ? *o.alignH : d.align[0]; };
        auto av = [&](const Descriptor& d) { return o.alignV ? *o.alignV : d.align[1]; };
        const Vec2 pa = {align(da.x, f(size.w * sa[0]), 1, ah(da)), align(da.y, f(size.h * sa[1]), 1, av(da))};
        const Vec2 pb = {align(db.x, f(size.w * sb[0]), 1, ah(db)), align(db.y, f(size.h * sb[1]), 1, av(db))};
        const Vec2 scale = {f(f(f(sb[0] - sa[0]) * t) + sa[0]), f(f(f(sb[1] - sa[1]) * t) + sa[1])};
        double x = f(f(f(pb[0] - pa[0]) * t) + pa[0]);
        double y = f(f(f(pb[1] - pa[1]) * t) + pa[1]);
        if (o.offset) {
            x = f(x + (*o.offset)[0]);
            y = f(y + (*o.offset)[1]);
        }
        Argb argb;
        if (o.colour) {
            argb = *o.colour;
            argb[0] = f(argb[0] * f(1 - t));
        } else {
            const Argb to = {0, db.argb[1], db.argb[2], db.argb[3]};
            for (int i = 0; i < 4; ++i) {
                argb[size_t(i)] = f(f(f(to[size_t(i)] - da.argb[size_t(i)]) * t) + da.argb[size_t(i)]);
            }
        }
        Draw q;
        q.kind = Draw::Text;
        q.font = font;
        q.text = text;
        q.x = x;
        q.y = y;
        q.scale = scale;
        q.argb = argb;
        q.shadow = font_.at(font);
        q.order = d_[0x1a].order;
        out.push_back(q);
        return {x, y, f(size.w * scale[0]), f(size.h * scale[1])};
    }

    // 0x1F2AA0
    void wrapped(std::vector<Draw>& out, const std::string& font, int k, const std::string& text) const {
        const Descriptor& d = desc(k);
        const double sx = d.scale[0];
        const double sy = d.scale[1];
        const double limit = f(d.w - std::abs(font_.at(font)[0]));
        std::vector<std::string> lines;
        std::string line;
        // text.split(' ')
        std::vector<std::string> words;
        {
            size_t start = 0;
            while (true) {
                const size_t sp = text.find(' ', start);
                words.push_back(text.substr(start, sp == std::string::npos ? std::string::npos : sp - start));
                if (sp == std::string::npos) {
                    break;
                }
                start = sp + 1;
            }
        }
        for (const auto& word : words) {
            const std::string next = !line.empty() ? line + " " + word : word;
            if (!line.empty() && measure(font, next, sx, sy).w > limit) {
                lines.push_back(line);
                line = word;
            } else {
                line = next;
            }
        }
        if (!line.empty() || lines.empty()) {
            lines.push_back(line);
        }
        const double lineH = f(f(fontHeight(font) * 1) * sy);
        const double total = f(lineH * double(lines.size()));
        double y = d.align[1] == 0 ? f(d.y - f(total * 0.5)) : d.align[1] > 0 ? f(d.y - total) : d.y;
        for (const auto& l : lines) {
            const double w = measure(font, l, sx, sy).w;
            const double x = d.align[0] == 0 ? f(d.x - f(w * 0.5)) : d.align[0] > 0 ? f(d.x - w) : d.x;
            Draw q;
            q.kind = Draw::Text;
            q.font = font;
            q.text = l;
            q.x = x;
            q.y = y;
            q.scale = {sx, sy};
            q.argb = d.argb;
            q.shadow = font_.at(font);
            q.shadowArgb = Argb{d.argb[0], 0, 0, 0};
            q.order = d.order;
            out.push_back(q);
            y = f(y + lineH);
        }
    }

    // 0x21F338
    void stars(std::vector<Draw>& out, double grade, std::optional<Vec2> scale, double alpha, std::optional<Vec2> at = std::nullopt) const {
        if (grade <= 0) {
            return;
        }
        const Descriptor& d = desc(0x10);
        auto sp = sprites_.find("star");
        const Vec2 sc = scale ? Vec2{f(d.scale[0] * (*scale)[0]), f(d.scale[1] * (*scale)[1])} : d.scale;
        const double total = f(f(d.w * grade) + f(grade - 1 + (grade - 1)));
        double x, y;
        if (!at) {
            x = align(d.x, total, sc[0], d.align[0]);
            y = align(d.y, d.h, sc[1], d.align[1]);
        } else {
            x = f((*at)[0] - f(f(total * sc[0]) * 0.5));
            y = f((*at)[1] - f(f(d.h * sc[1]) * 0.5));
        }
        const auto& table = colourLists_.at("stars");
        const int gi = int(grade) - 1;
        if (gi < 0 || size_t(gi) >= table.size()) {
            return;
        }
        Argb colour = table[size_t(gi)];
        colour[0] = f(colour[0] * alpha);
        const double step = f(f(d.w + 2) * sc[0]);
        for (int k = 0; k < grade; ++k) {
            Draw q;
            q.kind = Draw::Sprite;
            if (sp != sprites_.end()) {
                q.sprite = sp->second;
            }
            q.x = x;
            q.y = y;
            q.w = d.w;
            q.h = d.h;
            q.scale = sc;
            q.argb = colour;
            q.order = d.order;
            out.push_back(q);
            x = f(x + step);
        }
    }

    double pulse(double t, double amplitude = 0.2, double rate = 12.566371) const {
        const int i = toInt32(std::trunc(f(f(t * f(rate)) * 81.4873276))) & 511;
        return f(f(f(sin_[size_t(i)] * amplitude) * f(1 - f(t * t))) + 1);
    }

    // the draw list for one frame (0x1EC3F8's slot loop)
    std::vector<Draw> frame(const Slots& slots, FrameState state = {}) {
        std::vector<Draw> out;
        const uint32_t fl = state.flags ? *state.flags : flags;
        const int players = state.players;
        auto ratio = [](const Slot& s) { return fround(s.value / s.maximum); };
        auto live = [&](size_t k) -> const Slot* {
            if (k >= slots.size() || !slots[k] || slots[k]->type == kFree) {
                return nullptr;
            }
            return &*slots[k];
        };
        const double flash = state.flash ? *state.flash : (live(0x18) ? ratio(*live(0x18)) : -1);
        const double f25 = state.pulse25 ? *state.pulse25 : (flash >= 0 ? pulse(flash) : 1);
        std::string label;
        bool hasLabel = false;
        double f26 = -1;
        double f28 = 0;
        int recovered = 0;
        for (size_t k = 0; k < slots.size(); ++k) {
            if (!slots[k]) {
                continue;
            }
            const Slot& s = *slots[k];
            if (s.type == kFree || s.type >= 0x34) {
                continue;
            }
            const double r = ratio(s);
            const int t = int(s.type);
            // readTrickHudSlots sets text for every slot (the C string for 0 / 0xE, else String(points))
            const std::string& txt = s.text;
            if (t == 0) {
                if (fl & 0x4000) {
                    wrapped(out, "FEFONT", 0x1d, txt);
                }
            } else if (t == 1) {
                if (fl & 0x4000000) {
                    double sc = 1;
                    if (state.pulse70 && *state.pulse70 >= 0) {
                        sc = pulse(*state.pulse70);
                    }
                    TextOpts o;
                    o.scale = Vec2{sc, sc};
                    text(out, "HUDFONT", 0x12, txt, o);
                    stars(out, s.arg, Vec2{sc, sc}, 1);
                }
            } else if (t == 2) {
                if (fl & 0x4000000) {
                    bonus(out, s, r);
                }
            } else if (t == 4) {
                if (fl & 0x4000000) {
                    double sc = 1;
                    std::optional<Argb> colour;
                    if (r < 1) {
                        sc = pulse(r, f(0.150000006));
                        colour = colours_.at("multiplier");
                    }
                    TextOpts o;
                    o.scale = Vec2{sc, sc};
                    o.colour = colour;
                    text(out, "HUDFONT", 0x1c, "X" + txt, o);
                }
            } else if (t == 3) {
                if (fl & 0x4000000) {
                    combo(out, s, r);
                }
            } else if (t == 7) {
                if (fl & 0x402) {
                    const std::string value = intString(double(toInt32(s.points + state.extraPoints7C)));
                    if (flash < 0) {
                        text(out, "HUDFONT", 0x11, value);
                    } else {
                        TextOpts o;
                        o.scale = Vec2{f25, f25};
                        o.colour = colours_.at("green");
                        text(out, "HUDFONT", 0x11, value, o);
                    }
                }
            } else if (t == 0xb) {
                if (f26 < 0) {
                    f28 = r;
                    f26 = 0;
                }
            } else if (t == 0xe) {
                if (players == 1 && (fl & 0x4000000)) {
                    text(out, "FEFONT", 0x46, txt);
                }
            } else if (t >= 0x1c && t <= 0x20) {
                label = strings_["bonus"][size_t(t - 0x1c)].get<std::string>();
                hasLabel = true;
            } else if (t == 0x21) {
                text(out, "FEFONT", 0x35, strings_.value("recovered", "RECOVERED!"));
                f26 = r;
                f28 = 1;
                recovered = 1;
            } else if (t == 0x23 || t == 0x24) {
                if ((fl & 0x4000000) && !(fl & 0x200)) {
                    popup(out, s, r);
                }
            } else if (t == 0x25 || t == 0x27 || t == 0x28) {
                if (fl & 0x4000000) {
                    lost(out, s, r);
                }
            } else if (t == 0x26) {
                if ((fl & 0x4000000) && !(fl & 0x200)) {
                    comboBonus(out, s, r, txt);
                }
            } else if (t == 0x30) {
                if ((fl & 0x4000000) && (fl & 0x200)) {
                    comboBonus(out, s, r, money(s.points));
                }
            } else if (t == 0x2e || t == 0x2f) {
                if ((fl & 0x4000000) && (fl & 0x200)) {
                    cashPopup(out, s, r);
                }
            } else if (t == 0x31) {
                const int mode = state.collectMode ? *state.collectMode : (state.finished ? 1 : 3);
                bool split = false;
                for (const auto& x : slots) {
                    if (x && x->type >= 0x29 && x->type <= 0x2b) {
                        split = true;
                    }
                }
                if ((fl & 0x100000) && mode == 3 && !((fl & 0x4) && split)) {
                    text(out, "HUDFONT", 1, "Collect +" + money(s.points));
                }
            } else if (t == 0x29 || t == 0x2a) {
                if (fl & 0x4) {
                    splitTime(out, s, r);
                }
            } else if (t == 0x2b) {
                if (fl & 0x2) {
                    splitTime(out, s, r);
                }
            }
        }
        if (hasLabel && players == 1 && (fl & 0x4000000)) {
            text(out, "FEFONT", 0, label);
        }
        uberHint(out, slots, fl, state);
        if (f26 >= 0) {
            if (sprites_.count("recoverBar")) {
                recoverMeter(out, f28, f26, !recovered && (fl & 0x1000000) != 0, state);
            } else {
                Draw q;
                q.kind = Draw::Recover;
                q.fill = f28;
                out.push_back(q);
            }
        }
        if (state.bigMessage && sprites_.count("go")) {
            bigMessage(out);
        }
        return out;
    }

    // 0x1F07EC: the big centre message
    void bigMessage(std::vector<Draw>& out) const {
        const auto& m = big_;
        const double t = m.t;
        const double grow = f(f(t * f(0.600000024)) + f(0.400000006));
        if (m.id == 8 && sprites_.count("go")) {
            const Descriptor& d = desc(8);
            const double w = f(d.w * grow);
            const double h = f(d.h * grow);
            Argb argb = d.argb;
            argb[0] = f(1 - f(f(t - f(0.600000024)) * f(1.66666663)));
            Draw q;
            q.kind = Draw::Sprite;
            q.sprite = sprites_.at("go");
            q.x = align(d.x, w, d.scale[0], d.align[0]);
            q.y = align(d.y, h, d.scale[1], d.align[1]);
            q.size = Vec2{w, h};
            q.scale = d.scale;
            q.argb = argb;
            q.order = d.order;
            out.push_back(q);
        } else if (m.id == 0x3a) {
            const double fade = f(0.600000024) < t ? f(1 - f(f(t - f(0.600000024)) * f(1.66666663))) : 1;
            Argb c1 = desc(0x3a).argb;
            c1[0] = f(c1[0] * fade);
            TextOpts o;
            o.scale = Vec2{grow, grow};
            o.colour = c1;
            text(out, "HUDFONT", 0x3a, strings_.value("missionSuccess", "MISSION SUCCESS"), o);
            if (m.cash > 0) {
                Argb c2 = desc(0x3b).argb;
                c2[0] = f(c2[0] * fade);
                TextOpts o2;
                o2.scale = Vec2{grow, grow};
                o2.colour = c2;
                text(out, "HUDFONT", 0x3b, money(m.cash), o2);
            }
        }
    }

    // the Uber trick hint (0x1E95A0 at descriptor 73)
    struct HintIcon {
        double w, h;
        std::array<double, 4> uv;
    };
    static HintIcon hintIcon(const std::string& k) {
        if (k == "l1") {
            return {28, 16, {0.681640625, 0.591796875, 0.697265625, 0.740234375}};
        }
        if (k == "r1") {
            return {28, 16, {0.615234375, 0.474609375, 0.580078125, 0.673828125}};
        }
        return {21, 20, {0.482421875, 0.134765625, 0.212890625, 0.556640625}};
    }
    struct Piece {
        std::string text;
        std::string icon;
        double w = 0, h = 0;
    };
    struct Layout {
        std::vector<Piece> pieces;
        double sc = 1, W = 0, H = 0, offY = 0;
    };
    void uberHint(std::vector<Draw>& out, const Slots& current, uint32_t fl, const FrameState& state) {
        const Slots& slots = state.prepassSlots ? *state.prepassSlots : (prepass_ ? *prepass_ : current);
        auto live = [&](size_t k) { return k < slots.size() && slots[k] && slots[k]->type != kFree; };
        const Slot* s9 = live(9) ? &*slots[9] : nullptr;
        if (s9 && fround(s9->value / s9->maximum) == 1) {
            hint55C_ = (hint55C_ >= 0 ? hint55C_ : 1) ^ 1;
        }
        if (state.finished || !(fl & 0x200000) || !(fl & 0x1000000) || !s9 || fround(s9->value / s9->maximum) == 1) {
            return;
        }
        for (const auto& x : slots) {
            if (x && x->type != kFree && (x->type == 0 || x->type == 0x21 || x->type == 0xb)) {
                return;
            }
        }
        const std::string icon = (hint55C_ >= 0 ? hint55C_ : 1) ? "r1" : "l1";
        const double sc = f(0.7);
        const std::string runs[3] = {"UBER TRICK = ", " + ", ""};
        const std::string icons[2] = {icon, "square"};
        auto keyOf = [&](const std::string& b) { return state.keys ? state.keys(b) : std::string(); };
        if (keyOf(icon).empty() && keyOf("square").empty()) {
            auto& layout = uberLayouts_[icon];
            if (layout.pieces.empty()) {
                layout = iconLayout("UBER TRICK = @" + icon + " + @square", sc);
            }
            iconLabel(out, layout, 73, state);
            return;
        }
        struct Dims {
            double w, h;
            std::string label;
        };
        auto dims = [&](const std::string& k) {
            const std::string label = keyOf(k);
            if (!label.empty()) {
                return Dims{f(keyCapWidth(&glyphs_.at("FEFONT"), label, 16, kLuiStretch)), 16, label};
            }
            const HintIcon h = hintIcon(k);
            return Dims{h.w, h.h, ""};
        };
        double widths[3];
        for (int k = 0; k < 3; ++k) {
            widths[k] = measure("FEFONT", runs[k], sc, sc, k < 2).w;
        }
        double W = 0;
        for (int k = 0; k < 3; ++k) {
            W = f(W + widths[k]);
            if (k < 2) {
                W = f(W + dims(icons[k]).w);
            }
        }
        const double textH = f(f(kFontHeightFE * 1) * sc);
        const double H = std::max({textH, dims(icons[0]).h, dims(icons[1]).h});
        const double offY = f(f(H - textH) * 0.5);
        const Descriptor& d = desc(73);
        double x = align(d.x, W, d.scale[0], d.align[0]);
        const double y = align(d.y, H, d.scale[1], d.align[1]);
        const double cy = f(y + f(f(H * d.scale[1]) * 0.5));
        for (int k = 0; k < 3; ++k) {
            if (!runs[k].empty()) {
                Draw q;
                q.kind = Draw::Text;
                q.font = "FEFONT";
                q.text = runs[k];
                q.x = x;
                q.y = f(y + f(offY * d.scale[1]));
                q.scale = {sc, sc};
                q.argb = d.argb;
                q.shadow = {2, 2};
                q.shadowArgb = Argb{d.argb[0], 0, 0, 0};
                q.order = d.order;
                out.push_back(q);
            }
            x = f(x + f(widths[k] * d.scale[0]));
            if (k < 2) {
                const Dims ic = dims(icons[k]);
                Draw q;
                q.kind = Draw::Sprite;
                q.sprite = SpriteRef{"OV_1-2", hintIcon(icons[k]).uv};
                q.key = ic.label;
                q.x = x;
                q.y = f(cy - f(f(ic.h * d.scale[1]) * 0.5));
                q.size = Vec2{ic.w, ic.h};
                q.scale = d.scale;
                q.argb = d.argb;
                q.order = d.order;
                out.push_back(q);
                x = f(x + f(ic.w * d.scale[0]));
            }
        }
    }

    // 0x1E92A8
    Layout iconLayout(const std::string& source, double sc) {
        // source.split(/@(\w+)/): text, icon, text, ...
        std::vector<std::string> parts;
        {
            std::string cur;
            size_t i = 0;
            while (i < source.size()) {
                if (source[i] == '@' && i + 1 < source.size() && (std::isalnum(uint8_t(source[i + 1])) || source[i + 1] == '_')) {
                    parts.push_back(cur);
                    cur.clear();
                    size_t j = i + 1;
                    while (j < source.size() && (std::isalnum(uint8_t(source[j])) || source[j] == '_')) {
                        ++j;
                    }
                    parts.push_back(source.substr(i + 1, j - i - 1));
                    i = j;
                } else {
                    cur += source[i];
                    ++i;
                }
            }
            parts.push_back(cur);
        }
        Layout layout;
        const Vec2 shadow = font_["FEFONT"];
        font_["FEFONT"] = {0, 0};
        for (size_t k = 0; k < parts.size(); ++k) {
            const std::string& p = parts[k];
            if (k & 1) {
                if (p == "l1" || p == "r1" || p == "square") {
                    const HintIcon ic = hintIcon(p);
                    layout.pieces.push_back({"", p, ic.w, ic.h});
                }
            } else if (!p.empty()) {
                layout.pieces.push_back({p, "", measure("FEFONT", p, sc, sc, k < parts.size() - 1).w, 0});
            }
        }
        font_["FEFONT"] = shadow;
        double W = 0;
        double H = f(f(kFontHeightFE * 1) * sc);
        const double textH = H;
        for (const auto& q : layout.pieces) {
            W = f(W + q.w);
            if (!q.icon.empty() && H < q.h) {
                H = q.h;
            }
        }
        layout.sc = sc;
        layout.W = W;
        layout.H = H;
        layout.offY = f(f(H - textH) * 0.5);
        return layout;
    }

    // 0x1E95A0
    void iconLabel(std::vector<Draw>& out, const Layout& layout, int k, const FrameState& state) const {
        const Descriptor& d = desc(k);
        const Vec2 S = d.scale;
        const Argb argb = d.argb;
        const Vec2 sc = {f(layout.sc * S[0]), f(layout.sc * S[1])};
        double x = align(d.x, layout.W, S[0], d.align[0]);
        const double y = align(d.y, layout.H, S[1], d.align[1]);
        const double cy = f(y + f(f(layout.H * S[1]) * 0.5));
        for (const auto& q : layout.pieces) {
            if (!q.text.empty()) {
                Draw t;
                t.kind = Draw::Text;
                t.font = "FEFONT";
                t.text = q.text;
                t.x = x;
                t.y = f(y + f(layout.offY * S[1]));
                t.scale = sc;
                t.argb = argb;
                t.shadow = {2, 2};
                t.shadowArgb = Argb{argb[0], 0, 0, 0};
                t.order = d.order;
                out.push_back(t);
            } else {
                const std::string label = state.keys ? state.keys(q.icon) : std::string();
                const double w = !label.empty() ? f(keyCapWidth(&glyphs_.at("FEFONT"), label, 16, kLuiStretch)) : q.w;
                const double h = !label.empty() ? 16 : q.h;
                Draw s;
                s.kind = Draw::Sprite;
                s.sprite = SpriteRef{"OV_1-2", hintIcon(q.icon).uv};
                s.key = label;
                s.x = x;
                s.y = f(cy - f(f(h * S[1]) * 0.5));
                s.size = Vec2{w, h};
                s.scale = S;
                s.argb = argb;
                s.order = d.order;
                out.push_back(s);
                x = f(x + f(w * S[0]));
                continue;
            }
            x = f(x + f(q.w * S[0]));
        }
    }

    // the crash recover meter 0x21D9A0
    void recoverMeter(std::vector<Draw>& out, double fill, double r, bool label, const FrameState& state) {
        double p = 1;
        double fade = 1;
        if (r < f(0.3000219166278839)) {
            p = pulse(f(r * f(3.333089828491211)), f(0.15003453195095062), f(12.566370964050293));
        }
        if (f(0.5000368356704712) < r) {
            fade = 0;
        } else if (f(0.299963116645813) < r) {
            fade = f(1 - f(f(r - f(0.299963116645813)) * f(4.998157501220703)));
        }
        if (label) {
            if (recoverLabel_.pieces.empty()) {
                recoverLabel_ = iconLayout(strings_.value("recover", "RECOVER = @square"), f(0.699999988079071));
            }
            iconLabel(out, recoverLabel_, 55, state);
        }
        const Descriptor& d = desc(54);
        const double rest = f(d.w - 23);
        const bool full = f(rest - 18) >= 0;
        const double count = full ? 1 : f(rest * f(0.0555555559694767));
        const double sx = f(d.scale[0] * p);
        const double sy = f(d.scale[1] * p);
        const double W = f(d.w * sx);
        const double H = f(d.h * sy);
        const double x = align(d.x, W, 1, d.align[0]);
        const double y = align(d.y, H, 1, d.align[1]);
        const Argb argb = {fade, 1, 1, 1};
        auto quad = [&](const std::string& page, const std::array<double, 4>& uv, double qx, double qy, double w, double h) {
            Draw q;
            q.kind = Draw::Sprite;
            q.sprite = SpriteRef{page, uv};
            q.x = qx;
            q.y = qy;
            q.size = Vec2{w, h};
            q.scale = {1, 1};
            q.argb = argb;
            q.order = d.order;
            out.push_back(q);
        };
        const double bw = f(sx * 20);
        const double bh = f(sy * 20);
        SpriteRef button{"OV_1-2", hintIcon("square").uv};
        auto rb = spriteLists_.find("recoverButton");
        if (rb != spriteLists_.end() && !rb->second.empty()) {
            button = rb->second[0];
        }
        quad(button.page, button.uv, x, f(y + f(f(H - bh) * 0.5)), bw, bh);
        const double lead = f(bw + f(sx * 3));
        const double x0 = f(x + lead);
        const double inner = f(W - lead);
        const double cap = f(sx * f(count * 9));
        const SpriteRef& end = sprites_.at("recoverEnd");
        const auto uvEnd = end.uv;
        double px = x0;
        quad(end.page, end.uv, px, y, cap, H);
        px = f(px + cap);
        if (full) {
            const double mid = f(inner - f(cap + cap));
            const SpriteRef& bar = sprites_.at("recoverBar");
            quad(bar.page, bar.uv, px, y, mid, H);
            px = f(px + mid);
        }
        quad(end.page, {uvEnd[0], uvEnd[2], uvEnd[1], uvEnd[3]}, px, y, cap, H);
        if (0 < fill) {
            const double ix = f(3 * sx);
            const double iy = f(3 * sy);
            Draw q;
            q.kind = Draw::Quad;
            q.x = f(x0 + ix);
            q.y = f(y + iy);
            q.size = Vec2{f(f(inner - f(ix + ix)) * fill), f(H - f(iy + iy))};
            q.argb = {f(1 * fade), 1, 0, 0};
            q.order = d.order + 1;
            out.push_back(q);
        }
    }

    // case 2 (0x1EEE44)
    void bonus(std::vector<Draw>& out, const Slot& s, double r) const {
        double sc = 1;
        std::optional<Argb> colour;
        if (r < 1) {
            sc = pulse(r);
            colour = colours_.at("bonusGreen");
        }
        TextOpts o;
        o.scale = Vec2{sc, sc};
        o.colour = colour;
        text(out, "HUDFONT", 0xe, "+" + s.text, o);
        const Descriptor& d = desc(0xf);
        const Vec2 scale = {f(d.scale[0] * sc), f(d.scale[1] * sc)};
        const double w = f(d.w * scale[0]);
        const double h = f(d.h * scale[1]);
        Draw q;
        q.kind = Draw::Sprite;
        q.sprite = sprites_.at("bonusBox");
        q.x = align(d.x, w, 1, d.align[0]);
        q.y = align(d.y, h, 1, d.align[1]);
        q.size = Vec2{w, h};
        q.scale = {1, 1};
        q.argb = colour ? *colour : d.argb;
        q.order = d.order;
        out.push_back(q);
    }

    // case 3 (0x1EE8A0)
    void combo(std::vector<Draw>& out, const Slot& s, double r) const {
        std::optional<Argb> pulseColour;
        double sc = 1;
        if (r < 1) {
            sc = pulse(r);
            pulseColour = colours_.at("comboGreen");
        }
        const Descriptor& d = desc(0xd);
        const auto& hands = spriteLists_.at("hands");
        auto watch = [&](double size, double x) {
            const double px = align(x, size, 1, 1);
            const double py = align(d.y, size, 1, 0);
            Draw q;
            q.kind = Draw::Sprite;
            q.sprite = sprites_.at("watch");
            q.x = px;
            q.y = py;
            q.size = Vec2{size, size};
            q.scale = {1, 1};
            q.argb = d.argb;
            q.order = d.order;
            out.push_back(q);
            const int f10 = int(s.field10);
            // hands[s.field10]: 0 -> +0x4C8 (hands[3]), 1 -> hands[2], 2 -> hands[1], 3 -> hands[0]
            if (s.field10 == f10 && f10 >= 0 && f10 <= 3) {
                Draw hq = q;
                hq.sprite = hands[size_t(3 - f10)];
                hq.order = d.order + 1;
                out.push_back(hq);
            }
        };
        int alignH = 0;
        if (s.arg >= 2) {
            TextOpts o;
            o.scale = Vec2{f(sc * 2), f(sc * f(2.4000001))};
            o.colour = pulseColour;
            o.offset = Vec2{0, -1};
            o.alignH = 1;
            o.alignV = 0;
            const Box box = text(out, "HUDFONT", 0xd, jsNumberString(s.arg) + "X", o);
            watch(box.h, f(d.x + f(-4 - box.w)));
            alignH = -1;
        }
        TextOpts po;
        po.scale = Vec2{sc, sc};
        po.colour = pulseColour;
        po.alignH = alignH;
        po.alignV = -1;
        const Box plus = text(out, "HUDFONT", 0xd, "+" + s.text, po);
        TextOpts lo = po;
        lo.offset = Vec2{0, f(-plus.h)};
        const Box label = text(out, "HUDFONT", 0xd, strings_.value("combo", "Combo"), lo);
        if (s.arg == 1) {
            watch(f(label.h * f(2.4000001)), f(d.x + f(-16 - f(label.w * 0.5))));
        }
    }

    // cases 0x23 / 0x24 (0x1EE200)
    void popup(std::vector<Draw>& out, const Slot& s, double r) {
        const Vec2 shadow = font_["HUDFONT"];
        font_["HUDFONT"] = {0, 0};
        const int from = s.type == 0x24 ? 0x16 : 0x15;
        double sc = 1;
        Box box;
        if (f(0.207151964) <= r) {
            box = textLerp(out, "HUDFONT", from, 0x11, r, s.text);
        } else {
            sc = pulse(f(r * f(4.82737398)));
            TextOpts o;
            o.scale = Vec2{sc, sc};
            o.colour = colours_.at(s.type == 0x24 ? "repeatPopup" : "popup");
            box = textLerp(out, "HUDFONT", from, 0x11, r, s.text, o);
        }
        double alpha = 1;
        if (1 < r) {
            alpha = 0;
        } else if (0 < r) {
            alpha = f(1 - r);
        }
        stars(out, s.arg, Vec2{sc, sc}, alpha, Vec2{f(box.x + f(box.w * 0.5)), f(box.y + f(box.h * 0.5))});
        font_["HUDFONT"] = shadow;
    }

    // cases 0x29 / 0x2A / 0x2B (0x1ED990 / 0x1EDC6C)
    void splitTime(std::vector<Draw>& out, const Slot& s, double r) const {
        const int t = int(s.type);
        const Descriptor& d = desc(t == 0x29 ? 0x41 : 0x40);
        const SpriteRef& sprite = sprites_.at(t == 0x29 ? "timeBonus" : "checkpoint");
        const double pl = r < 0.5 ? pulse(f(r + r), f(0.100000001)) : 1;
        const Vec2 sc = {f(d.scale[0] * pl), f(d.scale[1] * pl)};
        const double w = f(d.w * sc[0]);
        const double h = f(d.h * sc[1]);
        const double x = align(d.x, w, 1, d.align[0]);
        const double y = align(d.y, h, 1, d.align[1]);
        Draw q;
        q.kind = Draw::Sprite;
        q.sprite = sprite;
        q.x = x;
        q.y = y;
        q.size = Vec2{w, h};
        q.scale = {1, 1};
        q.argb = d.argb;
        q.order = d.order;
        out.push_back(q);
        const double cx = f(x + f(w * 0.5));
        const double top = f(y + h);
        const double k = f(1.00012457);
        const Vec2 ts = {f(sc[0] * k), f(sc[1] * k)};
        const int32_t value = toInt32(s.points);
        const Argb colour = colours_.at(t == 0x29 ? "timeBonus" : ((value >= 0) == (t == 0x2a)) ? "splitPositive" : "splitNegative");
        const Vec2 shadow = font_.at("HUDFONT");
        auto piece = [&](const std::string& text, double px) {
            Draw p;
            p.kind = Draw::Text;
            p.font = "HUDFONT";
            p.text = text;
            p.x = px;
            p.y = top;
            p.scale = ts;
            p.argb = colour;
            p.shadow = shadow;
            p.shadowArgb = Argb{colour[0], 0, 0, 0};
            p.order = d.order;
            out.push_back(p);
        };
        if (t == 0x2b) {
            const std::string text = value >= 0 ? "+" + std::to_string(value) : std::to_string(value);
            const Box m = measure("HUDFONT", text, ts[0], ts[1]);
            piece(text, f(cx - f(m.w * 0.5)));
            return;
        }
        const double a = std::abs(double(value));
        const double width = f(f(clock_.pair * 3) + f(f(f(clock_.gap + 2) + 2) * 2));
        double px = f(cx - f(f(width * ts[0]) * 0.5));
        piece(value >= 0 ? strings_.value("plus", "+") : strings_.value("minus", "-"), px);
        px = f(px + clock_.sign);
        const double pair = f(f(clock_.pair + 2) * ts[0]);
        const double colon = f(f(clock_.gap + 2) * ts[0]);
        const double one = f(clock_.one * ts[0]);
        auto digits = [&](const std::string& text) {
            double o = 0;
            if (text[0] == '1') {
                o = one;
            }
            if (text[1] == '1') {
                o = f(o + one);
            }
            piece(text, f(px + o));
        };
        digits(pad2(int64_t(std::trunc(a / 3600))));
        const double rest[2] = {std::fmod(std::trunc(a / 60), 60), std::fmod(a, 60)};
        for (double n : rest) {
            px = f(px + pair);
            piece(strings_.value("colon", ":"), px);
            px = f(px + colon);
            digits(pad2(int64_t(n)));
        }
    }

    // the race HUD clock (0x1F16C0 -> 0x1F1840)
    void raceClock(std::vector<Draw>& out, int64_t h, int64_t m, int64_t s, std::optional<Argb> colour = std::nullopt, int k = 0x1a) const {
        const Descriptor& d = desc(k);
        const Vec2 sc = d.scale;
        const double width = f(f(clock_.pair * 3) + f(f(f(clock_.gap + 2) + 2) * 2));
        const double height = f(kFontHeightHUD * 1);
        double px = align(d.x, width, sc[0], d.align[0]);
        const double py = align(d.y, height, sc[1], d.align[1]);
        const Argb argb = colour ? *colour : d.argb;
        const double pair = f(f(clock_.pair + 2) * sc[0]);
        const double colon = f(f(clock_.gap + 2) * sc[0]);
        const double one = f(clock_.one * sc[0]);
        auto piece = [&](const std::string& text, double x) {
            Draw q;
            q.kind = Draw::Text;
            q.font = "HUDFONT";
            q.text = text;
            q.x = x;
            q.y = py;
            q.scale = sc;
            q.argb = argb;
            q.shadow = font_.at("HUDFONT");
            q.shadowArgb = Argb{argb[0], 0, 0, 0};
            q.order = d.order;
            out.push_back(q);
        };
        auto digits = [&](const std::string& text) {
            double o = 0;
            if (text[0] == '1') {
                o = one;
            }
            if (text[1] == '1') {
                o = f(o + one);
            }
            piece(text, f(px + o));
            px = f(px + pair);
        };
        digits(pad2(h));
        for (int64_t n : {m, s}) {
            piece(strings_.value("colon", ":"), px);
            px = f(px + colon);
            digits(pad2(n));
        }
    }

    // the finish / time-up banner 0x21F660
    void finishBanner(std::vector<Draw>& out, bool timeUp, const std::string* text = nullptr) const {
        const Descriptor& d = desc(timeUp ? 0x1b : 0x42);
        const double w = f(d.w * d.scale[0]);
        const double h = f(d.h * d.scale[1]);
        const double x = align(d.x, w, 1, d.align[0]);
        const double y = align(d.y, h, 1, d.align[1]);
        const double r[4] = {timeUp ? 1.5 : 1.5, timeUp ? 127.5 : 1.5, timeUp ? 169.0 : 131.0, timeUp ? 23.0 : 24.0};
        const double P = 256;
        Draw q;
        q.kind = Draw::Sprite;
        q.sprite = SpriteRef{"OV_1-3", {r[1] / P, r[0] / P, (r[0] + r[2]) / P, (r[1] + r[3]) / P}};
        q.x = x;
        q.y = y;
        q.size = Vec2{w, h};
        q.scale = {1, 1};
        q.argb = d.argb;
        q.order = d.order;
        out.push_back(q);
        if (timeUp || !text) {
            return;
        }
        const double k2 = f(1.907578468322754);
        const Vec2 sc = {f(d.scale[0] * k2), f(d.scale[1] * k2)};
        const double tw = measure("HUDFONT", *text, sc[0], sc[1]).w;
        Draw t;
        t.kind = Draw::Text;
        t.font = "HUDFONT";
        t.text = *text;
        t.x = f(f(x + f(w * 0.5)) - f(tw * 0.5));
        t.y = f(y + h);
        t.scale = sc;
        t.argb = {1, 1, 1, 1};
        t.shadow = font_.at("HUDFONT");
        t.shadowArgb = Argb{1, 0, 0, 0};
        t.order = d.order;
        out.push_back(t);
    }

    // the speed widget 0x2200C0
    void speed(std::vector<Draw>& out, double cmPerSec, bool kmh = false, int k = 25) const {
        const Descriptor& d = desc(k);
        const std::string label = kmh ? "KM/H" : "MPH";
        const Vec2 sc = d.scale;
        double v = f(f(cmPerSec) * f(0.035999998450279236));
        if (!kmh) {
            v = f(v * f(0.6209999918937683));
        }
        const Box size = measure("HUDFONT", label, sc[0], sc[1]);
        const double x = align(d.x, size.w, 1, d.align[0]);
        const double y = align(d.y, size.h, 1, d.align[1]);
        const Vec2 shadow = font_.at("HUDFONT");
        const Argb argb = d.argb;
        Draw a;
        a.kind = Draw::Text;
        a.font = "HUDFONT";
        a.text = label;
        a.x = x;
        a.y = y;
        a.scale = sc;
        a.argb = argb;
        a.shadow = shadow;
        a.shadowArgb = Argb{argb[0], 0, 0, 0};
        a.order = d.order;
        out.push_back(a);
        const std::string text = intString(std::max(0.0, std::trunc(f(v + 0.5))));
        const double ns = text.size() < 3 ? f(1.3623675107955933) : f(1.1214120388031006);
        const Vec2 nsc = {f(sc[0] * ns), f(sc[1] * ns)};
        const double w = measure("HUDFONT", text, nsc[0], nsc[1]).w;
        const double top = f(y - f(f(f(f(kFontHeightHUD * 1) * nsc[1]) * 0.5) + f(14.003287315368652)));
        Draw b = a;
        b.text = text;
        b.x = f(f(x + f(size.w * 0.5)) - f(w * 0.5));
        b.y = top;
        b.scale = nsc;
        out.push_back(b);
    }

    // cases 0x2E / 0x2F (0x1EF924)
    void cashPopup(std::vector<Draw>& out, const Slot& s, double r) {
        const Vec2 shadow = font_["HUDFONT"];
        font_["HUDFONT"] = {0, 0};
        if (f(0.207151964) <= r) {
            textLerp(out, "HUDFONT", 0x12, 0x38, r, money(s.points));
        } else {
            const double sc = pulse(f(r * f(4.82737398)));
            TextOpts o;
            o.scale = Vec2{sc, sc};
            o.colour = colours_.at("popup");
            textLerp(out, "HUDFONT", 0x12, 0x38, r, money(s.points), o);
        }
        font_["HUDFONT"] = shadow;
    }

    // case 0x26 (0x1EE46C)
    void comboBonus(std::vector<Draw>& out, const Slot& s, double r, const std::string& value) {
        const Vec2 shadow = font_["HUDFONT"];
        font_["HUDFONT"] = {0, 0};
        Argb colour = desc(0x2).argb;
        double sc = 1;
        if (r < f(0.300000012)) {
            sc = pulse(f(r * f(3.33333325)));
            colour = colours_.at("comboGreen");
        }
        TextOpts o;
        o.scale = Vec2{sc, sc};
        o.colour = colour;
        const Box box = textLerp(out, "HUDFONT", 0x2, 0x11, r, value, o);
        if (r < f(0.600000024)) {
            if (f(0.300000012) < r) {
                colour[0] = f(colour[0] * f(1 - f(f(r - f(0.300000012)) * f(3.33333325))));
            }
            TextOpts lo;
            lo.scale = Vec2{sc, sc};
            lo.colour = colour;
            lo.offset = Vec2{0, f(-box.h)};
            text(out, "HUDFONT", 0x2, jsNumberString(s.arg) + "X " + strings_.value("combo", "Combo"), lo);
        }
        font_["HUDFONT"] = shadow;
    }

    // cases 0x25 / 0x27 / 0x28 (0x1EF084)
    void lost(std::vector<Draw>& out, const Slot& s, double r) {
        const Vec2 shadow = font_["HUDFONT"];
        font_["HUDFONT"] = {0, 0};
        const int k = s.type == 0x27 ? 0xd : s.type == 0x28 ? 0xe : 0x13;
        const Descriptor& d = desc(k);
        const Descriptor& e = desc(0x14);
        const std::string txt = s.text;
        const double w = measure("HUDFONT", txt, 1, 1).w;
        const double r2 = f(r * r);
        const Vec2 a = {align(d.x, w, 1, d.align[0]), align(d.y, 0, 1, d.align[1])};
        const Vec2 b = {align(e.x, w, 1, e.align[0]), align(e.y, 0, 1, e.align[1])};
        const Vec2 origin = {f(f(f(b[0] - a[0]) * r2) + a[0]), f(f(f(b[1] - a[1]) * r2) + a[1])};
        Argb argb;
        for (int i = 0; i < 4; ++i) {
            argb[size_t(i)] = f(f(f(e.argb[size_t(i)] - d.argb[size_t(i)]) * r) + d.argb[size_t(i)]);
        }
        const Vec2 scale = {f(f(f(e.scale[0] - d.scale[0]) * r) + d.scale[0]), f(f(f(e.scale[1] - d.scale[1]) * r) + d.scale[1])};
        const auto& g = glyphs_.at("HUDFONT");
        const auto chars = codePoints(txt);
        double x = origin[0];
        const int len = int(chars.size());
        for (int i = 0; i < len; ++i) {
            const char32_t ch = chars[size_t(i)];
            int n = i - (len >> 1);
            bool neg = false;
            if (n == 0) {
                n = 1;
            } else if (n < 0) {
                neg = true;
                n = -n;
            }
            // charCodeAt(0) & 0xff (a code point past the BMP is a surrogate pair in JS; the HUD's digits are ASCII)
            const int code = int(ch) & 0xff;
            const int m = 2 * n * ((code & 0x7f) >> 1);
            const int up = (m * 4 + m) & 0x7f;
            const int side = (n * (code & 0xf) * 4) & 0x7f;
            const double fx = neg ? -side : side;
            const double fy = -up;
            Draw q;
            q.kind = Draw::Text;
            q.font = "HUDFONT";
            q.text = utf8(ch);
            q.x = f(x + f(r * fx));
            q.y = f(origin[1] + f(r * fy));
            q.scale = scale;
            q.argb = argb;
            q.shadow = {0, 0};
            q.order = d.order;
            out.push_back(q);
            auto it = g.find(ch);
            if (it != g.end()) {
                x = f(x + f((it->second.dx + it->second.w) * scale[0]));
            }
        }
        font_["HUDFONT"] = shadow;
    }

    const Descriptor& desc(int k) const {
        return d_.at(size_t(k));
    }

private:
    struct Big {
        int id = 0x50;
        double t = 0;
        double cash = 0;
    };
    struct Clock {
        double gap = 3, pair = 37, sign = 11, one = 4.75;
    };
    std::vector<Descriptor> d_;
    std::map<std::string, SpriteRef> sprites_;
    std::map<std::string, std::vector<SpriteRef>> spriteLists_;
    std::map<std::string, Argb> colours_;
    std::map<std::string, std::vector<Argb>> colourLists_;
    std::vector<double> sin_;
    nlohmann::json strings_;
    Clock clock_;
    std::map<std::string, GlyphTable> glyphs_;
    std::map<std::string, Vec2> font_;
    Big big_;
    std::map<int, double> bigSeen_;
    std::optional<Slots> prepass_;
    // owner +0x55C (undefined until resetUberHint: -1)
    int hint55C_ = -1;
    std::map<std::string, Layout> uberLayouts_;
    Layout recoverLabel_;
};

}  // namespace hud

}  // namespace ssx_native
