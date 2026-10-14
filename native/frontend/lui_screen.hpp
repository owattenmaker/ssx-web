#pragma once
// The original front-end LUI screens (web/lui-player.js LuiScreen, 1:1): frame-0 properties, the live timeline events, animation
// tracks, sprites cut from their atlas pages (tinted copies through multiply / destination-in), gouraud shapes (each triangle a
// two-stop linear gradient along the plane of its most varying channel, a flat shape's triangles as one path with unionFlat, a
// line loop), bars and FEFONT text (word wrap, PS2 wrap measure, the 0.8 texel colour, the tall glyphs), drawn on the UI canvas in
// the 640 x 480 LUI frame. The keyboard device turns the PS2 button icons into key caps (web/input-glyphs.js drawGlyphAsKey).
#include "hud_canvas.hpp"
#include "hud_widgets.hpp"
#include "trick_hud.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace ssx_native {

namespace fe {

using Props = std::map<int, double>;

inline Props propsOf(const nlohmann::json& j) {
    Props p;
    if (!j.is_object()) {
        return p;
    }
    for (auto it = j.begin(); it != j.end(); ++it) {
        if (it.value().is_number() && !it.key().empty() && std::isdigit(uint8_t(it.key()[0]))) {
            p[std::stoi(it.key())] = it.value().get<double>();
        }
    }
    return p;
}
inline double prop(const Props& p, int k, double fallback) {
    auto it = p.find(k);
    return it == p.end() ? fallback : it->second;
}

// web/loading-screen.js trackValue / animationProps
inline double trackValue(const nlohmann::json& keys, double frame) {
    if (!keys.is_array() || keys.empty()) {
        return 0;
    }
    double t = std::max(0.0, frame);
    for (size_t i = 0; i + 1 < keys.size(); ++i) {
        const double v = keys[i][0].get<double>();
        const double d = keys[i][1].get<double>();
        if (t < d) {
            return v + (keys[i + 1][0].get<double>() - v) * (t / d);
        }
        t -= d;
    }
    return keys.back()[0].get<double>();
}
inline void animationProps(const nlohmann::json& anim, double frame, int mode, Props& out) {
    const double length = std::max(1.0, anim.value("frames", 1.0));
    const double t = (mode & 8) ? std::fmod(frame, length) : std::min(frame, length);
    for (auto it = anim["tracks"].begin(); it != anim["tracks"].end(); ++it) {
        out[std::stoi(it.key())] = trackValue(it.value(), t);
    }
}

// ---- web/input-glyphs.js: the PS2 button icons of FE_1-14 / OV_1-2, and their key caps for the menus ----
struct GlyphButton {
    std::string button;
    std::array<double, 4> box{};
};
inline std::optional<GlyphButton> glyphButton(const std::string& page, double sx, double sy, double sw, double sh) {
    if ((page != "FE_1-14" && page != "OV_1-2") || !(sw > 0) || !(sh > 0)) {
        return std::nullopt;
    }
    const double cx = sx + sw / 2;
    const double cy = sy + sh / 2;
    static const char* face[4] = {"triangle", "square", "cross", "circle"};
    if (sw <= 26 && sh <= 26 && cy >= 118 && cy <= 148 && cx >= 12 && cx < 100) {
        const int k = int(std::floor((cx - 11) / 22));
        return GlyphButton{face[k], {12.0 + 22 * k, 123, 20, 20}};
    }
    if (sw <= 32 && sh <= 20 && cy >= 150 && cy <= 190 && cx >= 118 && cx < 182) {
        const bool left = cx >= 150;
        const bool two = cy >= 170;
        return GlyphButton{std::string(left ? "l" : "r") + (two ? "2" : "1"), {left ? 151.0 : 121.0, two ? 174.0 : 157.0, 28, 15}};
    }
    return std::nullopt;
}
// MENU_BUTTON_KEYS + keyLabel (the menus' context)
inline std::string menuKeyLabel(const std::string& button) {
    static const std::map<std::string, std::string> m = {{"cross", "Space"}, {"triangle", "Esc"}, {"square", "Shift"}, {"circle", "Backspace"},
                                                         {"start", "Enter"}, {"select", "Backspace"}, {"l1", "Q"},        {"r1", "E"},
                                                         {"l2", "Z"},        {"r2", "X"},         {"up", "↑"},          {"down", "↓"},
                                                         {"left", "←"},      {"right", "→"}};
    auto it = m.find(button);
    return it == m.end() ? std::string() : it->second;
}

class LuiScreen {
public:
    static constexpr double kFontK = 0.79;
    static constexpr double kPs2X = 640.0 / 512;
    static constexpr double kPs2Y = 480.0 / 448;
    static constexpr double kLine = 40;
    static constexpr double kTextTexel = 0.8;
    static constexpr double kTextTall = kPs2X / kPs2Y;

    struct Override {
        bool hidden = false;
        std::optional<std::string> text;
        Props props;
        std::optional<nlohmann::json> sprite;
        std::optional<double> fill;
        std::optional<double> alpha;
        std::optional<int> layer;
        bool wrap = false;
    };
    using OverrideFn = std::function<std::optional<Override>(const nlohmann::json& element)>;
    struct Event {
        const nlohmann::json* ev = nullptr;
        double start = 0;
    };

    // options (results-lui.js sets them on its screens)
    bool shapeScale = false;
    bool unionFlat = false;
    bool keepLead = false;
    bool flagWrap = false;
    bool ps2Wrap = false;
    // the device: key caps for the button icons while the keyboard is in use
    bool keyboard = true;

    LuiScreen(const nlohmann::json& screen, hud::HudImages& images, hud::UiText& text) : screen_(screen), images_(images), text_(text) {
        size_t i = 0;
        for (const auto& e : screen_["elements"]) {
            byName_[e.value("name", "")] = &e;
            index_[&e] = e.contains("index") && e["index"].is_number() ? e["index"].get<double>() : double(i);
            ++i;
            if (e.contains("bar") && e["bar"].is_object()) {
                bars_[e["bar"].value("background", "")] = {&e, false};
                bars_[e["bar"].value("fill", "")] = {&e, true};
            }
        }
    }
    const nlohmann::json& screen() const {
        return screen_;
    }

    // events: [{ev, start}] in application order; an animation event plays from start (a frame on now's clock)
    Props props(const nlohmann::json& e, const std::vector<Event>& events, double now) const {
        Props p = propsOf(e.contains("props") ? e["props"] : nlohmann::json());
        std::optional<std::tuple<std::string, int, double>> anim;
        if (e.contains("anim") && e["anim"].is_object()) {
            anim = std::make_tuple(e["anim"].value("hash", ""), e["anim"].value("mode", 0), 0.0);
        }
        const std::string name = e.value("name", "");
        for (const auto& [ev, start] : events) {
            if (ev->value("element", "") != name) {
                continue;
            }
            if (ev->contains("props")) {
                p = propsOf((*ev)["props"]);
                anim.reset();
            } else {
                anim = std::make_tuple(ev->value("anim", ""), ev->value("mode", 0), start);
            }
        }
        if (anim && screen_["animations"].contains(std::get<0>(*anim))) {
            animationProps(screen_["animations"][std::get<0>(*anim)], now - std::get<2>(*anim), std::get<1>(*anim), p);
        }
        return p;
    }

    void draw(HudCanvas& c, const std::vector<Event>& events, double now, const OverrideFn& override = nullptr,
              const std::function<bool(int)>& layers = nullptr) {
        struct Abs {
            double x = 0, y = 0, a = 1;
            Props p;
            Override o;
            double fill = 1;
        };
        std::map<const nlohmann::json*, Abs> cache;
        std::function<const Abs&(const nlohmann::json&)> abs = [&](const nlohmann::json& e) -> const Abs& {
            auto it = cache.find(&e);
            if (it != cache.end()) {
                return it->second;
            }
            Abs r;
            if (override) {
                if (auto o = override(e)) {
                    r.o = *o;
                }
            }
            r.p = props(e, events, now);
            for (const auto& [k, v] : r.o.props) {
                r.p[k] = v;
            }
            const std::string up = e.contains("parent") && e["parent"].is_string() ? e["parent"].get<std::string>()
                                   : e.contains("menu") && e["menu"].is_string() ? e["menu"].get<std::string>()
                                                                                  : std::string();
            Abs parent;
            auto pi = up.empty() ? byName_.end() : byName_.find(up);
            if (pi != byName_.end()) {
                parent = abs(*pi->second);
            }
            r.x = parent.x + prop(r.p, 0, 0);
            r.y = parent.y + prop(r.p, 1, 0);
            r.a = parent.a * ((r.o.alpha ? *r.o.alpha : prop(r.p, 13, 255)) / 255) * (r.o.hidden ? 0 : 1);
            return cache.emplace(&e, std::move(r)).first->second;
        };
        struct Item {
            const nlohmann::json* e;
            Abs r;
            int layer;
            double t;
        };
        std::vector<Item> items;
        for (const auto& e : screen_["elements"]) {
            const std::string kind = e.value("kind", "");
            if (kind == "group" || kind == "menu" || kind == "bar") {
                continue;
            }
            Abs r = abs(e);
            auto bar = bars_.find(e.value("name", ""));
            if (bar != bars_.end()) {
                const Abs& br = abs(*bar->second.first);
                r.x = br.x + prop(r.p, 0, 0);
                r.y = br.y + prop(r.p, 1, 0);
                r.a = r.a * br.a;
                r.fill = bar->second.second ? (br.o.fill ? *br.o.fill : 0) : 1;
            }
            if (r.a <= 0) {
                continue;
            }
            const int layer = r.o.layer ? *r.o.layer : e.value("layer", 0);
            if (layers && !layers(layer)) {
                continue;
            }
            items.push_back({&e, r, layer, 0});
        }
        // the draw order: layer; within a layer an element under a top-level group draws with that group's layer; then definition
        auto topLayer = [&](const nlohmann::json& e) {
            const nlohmann::json* x = &e;
            int l = e.value("layer", 0);
            while (x) {
                l = x->value("layer", 0);
                const std::string up = x->contains("parent") && (*x)["parent"].is_string() ? (*x)["parent"].get<std::string>()
                                       : x->contains("menu") && (*x)["menu"].is_string() ? (*x)["menu"].get<std::string>()
                                                                                          : std::string();
                auto it = up.empty() ? byName_.end() : byName_.find(up);
                x = it == byName_.end() ? nullptr : it->second;
            }
            return l;
        };
        for (auto& it : items) {
            const bool child = (it.e->contains("parent") && (*it.e)["parent"].is_string()) || (it.e->contains("menu") && (*it.e)["menu"].is_string());
            it.t = child ? topLayer(*it.e) : it.layer;
        }
        std::stable_sort(items.begin(), items.end(), [&](const Item& a, const Item& b) {
            if (a.layer != b.layer) {
                return a.layer < b.layer;
            }
            if (a.t != b.t) {
                return a.t < b.t;
            }
            return index_.at(a.e) < index_.at(b.e);
        });
        // keyboard: a button icon drawn over another at the same spot hides it (the covered one is dropped)
        if (keyboard) {
            std::set<std::string> seen;
            for (int i = int(items.size()) - 1; i >= 0; --i) {
                const Item& it = items[size_t(i)];
                if (it.e->value("kind", "") != "sprite") {
                    continue;
                }
                const nlohmann::json& sp = it.r.o.sprite ? *it.r.o.sprite : (*it.e)["sprite"];
                if (!sp.is_object() || !glyphButton(sp.value("page", ""), sp.value("sx", 0.0), sp.value("sy", 0.0), sp.value("sw", 0.0), sp.value("sh", 0.0))) {
                    continue;
                }
                std::string at;
                for (double v : {it.r.x, it.r.y, prop(it.r.p, 6, 0), prop(it.r.p, 7, 0), prop(it.r.p, 9, 0), prop(it.r.p, 10, 0), prop(it.r.p, 12, 0)}) {
                    at += std::to_string(int64_t(hud::jsRound(v * 4))) + ",";
                }
                if (seen.count(at)) {
                    items.erase(items.begin() + i);
                } else {
                    seen.insert(at);
                }
            }
        }
        for (const auto& it : items) {
            const std::string kind = it.e->value("kind", "");
            if (kind == "sprite") {
                sprite(c, it.r.o.sprite ? *it.r.o.sprite : (*it.e)["sprite"], it.r.x, it.r.y, it.r.a, it.r.p);
            } else if (kind == "shape") {
                shape(c, *it.e, it.r.x, it.r.y, it.r.a, it.r.fill, it.r.p);
            } else if (kind == "text") {
                const std::string t = it.r.o.text ? *it.r.o.text : it.e->value("text", "");
                if (!t.empty()) {
                    text(c, t, it.r.x, it.r.y, it.r.p, it.r.a, !flagWrap || (it.e->value("flags", 0) & 0x80) || it.r.o.wrap);
                }
            }
        }
    }

private:
    static std::array<double, 2> anchor(const Props& p, double w, double h) {
        const int f = int(prop(p, 12, 9));
        return {-w * (f & 16 ? 0.5 : f & 32 ? 1 : 0), -h * (f & 2 ? 0.5 : f & 4 ? 1 : 0)};
    }
    // LuiScreen.source: a zero-height (or zero-width) UV box is a one-texel line
    static std::array<double, 4> source(const nlohmann::json& s) {
        const double sw = s.value("sw", 0.0);
        const double sh = s.value("sh", 0.0);
        const double sx = sw < 1 ? std::floor(s.value("sx", 0.0)) : s.value("sx", 0.0);
        const double sy = sh < 1 ? std::floor(s.value("sy", 0.0)) : s.value("sy", 0.0);
        return {sx, sy, std::max(1.0, sw), std::max(1.0, sh)};
    }
    void sprite(HudCanvas& c, const nlohmann::json& sp, double x, double y, double a, const Props& p) {
        if (!sp.is_object()) {
            return;
        }
        const std::string page = sp.value("page", "");
        const auto [sx, sy, sw, sh] = source(sp);
        const double w = (prop(p, 6, 0) != 0 ? prop(p, 6, 0) : sw) * prop(p, 9, 100) / 100;
        const double h = (prop(p, 7, 0) != 0 ? prop(p, 7, 0) : sh) * prop(p, 10, 100) / 100;
        const std::array<uint8_t, 3> rgb = {uint8_t(std::clamp(hud::jsRound(prop(p, 14, 255)), 0.0, 255.0)), uint8_t(std::clamp(hud::jsRound(prop(p, 15, 255)), 0.0, 255.0)),
                                            uint8_t(std::clamp(hud::jsRound(prop(p, 16, 255)), 0.0, 255.0))};
        const auto [ox, oy] = anchor(p, w, h);
        // a PS2 button icon becomes its key cap while the keyboard is in use (drawGlyphAsKey, LUI_STRETCH)
        if (prop(p, 5, 0) == 0 && keyboard) {
            if (auto g = glyphButton(page, sx, sy, sw, sh)) {
                const std::string label = menuKeyLabel(g->button);
                if (!label.empty()) {
                    const double gx0 = std::max(g->box[0], sx);
                    const double gy0 = std::max(g->box[1], sy);
                    const double gx1 = std::min(g->box[0] + g->box[2], sx + sw);
                    const double gy1 = std::min(g->box[1] + g->box[3], sy + sh);
                    const double kx = w / sw;
                    const double ky = h / sh;
                    const double gx = x + ox + (gx0 - sx) * kx;
                    const double gy = y + oy + (gy0 - sy) * ky;
                    const double gw = (gx1 - gx0) * kx;
                    const double gh = (gy1 - gy0) * ky;
                    const double stretch = kPs2X / kPs2Y;
                    const double ch = std::max(gh * 0.85, 14 * kPs2Y);
                    const double cw = hud::keyCapWidth(&text_.fonts.at("FEFONT"), label, ch, stretch);
                    c.save();
                    c.setGlobalAlpha(std::min(1.0, a));
                    hud::TrickHudRenderer::drawKeyCap(
                        c, [&](const std::string& v, double size) { text_.text(c, v, 0, 0, size, "#f0f5f8", "FEFONT", "center"); }, label, gx + gw - cw,
                        gy + (gh - ch) / 2, cw, ch, stretch);
                    c.restore();
                    return;
                }
            }
        }
        c.save();
        c.setGlobalAlpha(std::min(1.0, a));
        c.translate(x, y);
        if (prop(p, 5, 0) != 0) {
            c.rotate(prop(p, 5, 0) * M_PI / 180);
        }
        // each sprite from its own canvas cut once from the page (no atlas neighbours at its edges)
        const bool white = rgb[0] >= 255 && rgb[1] >= 255 && rgb[2] >= 255;
        const int im = white ? images_.cutRegion(page, sx, sy, sw, sh) : images_.multiplyCut(page, sx, sy, sw, sh, rgb);
        c.drawImage(im, 0, 0, sw, sh, ox, oy, w, h);
        c.restore();
    }
    static std::string cssRgba(const std::array<double, 3>& rgb, double a) {
        char b[96];
        std::snprintf(b, sizeof b, "%g,%g,%g,%.17g", rgb[0], rgb[1], rgb[2], a);
        return b;
    }
    // a CSS rgba() colour as the canvas keeps it: bytes, the alpha too (Chrome reads it back as round(a x 255) / 255)
    static std::array<double, 4> rgba(const std::array<double, 3>& rgb, double a) {
        return {std::clamp(hud::jsRound(rgb[0]), 0.0, 255.0) / 255, std::clamp(hud::jsRound(rgb[1]), 0.0, 255.0) / 255, std::clamp(hud::jsRound(rgb[2]), 0.0, 255.0) / 255,
                std::round(std::clamp(a, 0.0, 1.0) * 255) / 255};
    }
    // shape: byte 0 vertex count, byte 1 triangle count, triangles from byte 4; vertex k at props 21 + 9k (x, y, A R G B)
    void shape(HudCanvas& c, const nlohmann::json& e, double rx, double ry, double ra, double fill, const Props& p) {
        std::vector<int> bytes;
        if (e.contains("shape") && e["shape"].is_array()) {
            for (const auto& b : e["shape"]) {
                bytes.push_back(b.get<int>());
            }
        }
        const int nv = bytes.empty() ? 4 : bytes[0];
        const int nt = bytes.empty() ? 2 : bytes[1];
        std::vector<std::array<int, 3>> tris;
        if (int(bytes.size()) >= 4 + 3 * nt) {
            for (int t = 0; t < nt; ++t) {
                tris.push_back({bytes[size_t(4 + 3 * t)], bytes[size_t(5 + 3 * t)], bytes[size_t(6 + 3 * t)]});
            }
        } else {
            tris = {{0, 1, 2}, {0, 2, 3}};
        }
        const double kx = shapeScale ? prop(p, 9, 100) / 100 : 1;
        const double ky = shapeScale ? prop(p, 10, 100) / 100 : 1;
        const double sx = fill * kx;
        struct V {
            double x, y, a;
            std::array<double, 3> rgb;
        };
        std::vector<V> v;
        for (int k = 0; k < nv; ++k) {
            v.push_back({prop(p, 21 + 9 * k, 0) * sx, prop(p, 22 + 9 * k, 0) * ky, prop(p, 26 + 9 * k, 255) / 255,
                         {prop(p, 27 + 9 * k, 255), prop(p, 28 + 9 * k, 255), prop(p, 29 + 9 * k, 255)}});
        }
        c.save();
        if (nt == 0) {
            // a closed line loop (the name bar frame)
            if (!v.empty()) {
                std::vector<std::array<double, 2>> pts;
                for (const auto& q : v) {
                    pts.push_back({rx + q.x, ry + q.y});
                }
                c.strokePolygon(pts, rgba(v[0].rgb, v[0].a * ra), 1.5, "l" + cssRgba(v[0].rgb, v[0].a * ra));
            }
            c.restore();
            return;
        }
        bool flat = !v.empty();
        for (const auto& q : v) {
            flat = flat && q.a == v[0].a && q.rgb == v[0].rgb;
        }
        if (unionFlat && flat) {
            std::vector<std::vector<std::array<double, 2>>> paths;
            for (const auto& t : tris) {
                if (t[0] >= nv || t[1] >= nv || t[2] >= nv) {
                    continue;
                }
                std::array<V, 3> T = {v[size_t(t[0])], v[size_t(t[1])], v[size_t(t[2])]};
                // one winding: the nonzero union
                if ((T[1].x - T[0].x) * (T[2].y - T[0].y) - (T[2].x - T[0].x) * (T[1].y - T[0].y) < 0) {
                    std::swap(T[1], T[2]);
                }
                paths.push_back({{rx + T[0].x, ry + T[0].y}, {rx + T[1].x, ry + T[1].y}, {rx + T[2].x, ry + T[2].y}});
            }
            const auto col = rgba(v[0].rgb, v[0].a * ra);
            c.fillPolygons(paths, [col](double, double) { return col; }, false, "u" + cssRgba(v[0].rgb, v[0].a * ra));
            c.restore();
            return;
        }
        for (const auto& t : tris) {
            if (t[0] >= nv || t[1] >= nv || t[2] >= nv) {
                continue;
            }
            const std::array<V, 3> T = {v[size_t(t[0])], v[size_t(t[1])], v[size_t(t[2])]};
            auto channel = [](const V& q, int k) { return k == 3 ? q.a * 255 : q.rgb[size_t(k)]; };
            int key = 3;
            double range = 0;
            for (int k : {3, 0, 1, 2}) {
                double lo = 1e300, hi = -1e300;
                for (const auto& q : T) {
                    lo = std::min(lo, channel(q, k));
                    hi = std::max(hi, channel(q, k));
                }
                if (hi - lo > range + 1e-9) {
                    range = hi - lo;
                    key = k;
                }
            }
            V lo = T[0];
            V hi = T[0];
            for (const auto& q : T) {
                if (channel(q, key) < channel(lo, key)) {
                    lo = q;
                }
                if (channel(q, key) > channel(hi, key)) {
                    hi = q;
                }
            }
            std::array<double, 3> avg{};
            for (int ch = 0; ch < 3; ++ch) {
                avg[size_t(ch)] = hud::jsRound((T[0].rgb[size_t(ch)] + T[1].rgb[size_t(ch)] + T[2].rgb[size_t(ch)]) / 3);
            }
            const double aa = (T[0].a + T[1].a + T[2].a) / 3;
            const std::vector<std::array<double, 2>> pts = {{rx + T[0].x, ry + T[0].y}, {rx + T[1].x, ry + T[1].y}, {rx + T[2].x, ry + T[2].y}};
            if (range > 0) {
                auto f = [&](const V& q) { return channel(q, key); };
                const V& p0 = T[0];
                const V& q = T[1];
                const V& w = T[2];
                const double dxq = q.x - p0.x, dyq = q.y - p0.y, dxr = w.x - p0.x, dyr = w.y - p0.y;
                const double det = dxq * dyr - dxr * dyq;
                double ex = hi.x, ey = hi.y;
                if (det != 0) {
                    const double A = ((f(q) - f(p0)) * dyr - (f(w) - f(p0)) * dyq) / det;
                    const double B = (dxq * (f(w) - f(p0)) - dxr * (f(q) - f(p0))) / det;
                    const double k = (f(hi) - f(lo)) / (A * A + B * B);
                    if (std::isfinite(k)) {
                        ex = lo.x + A * k;
                        ey = lo.y + B * k;
                    }
                }
                const double x0 = rx + lo.x, y0 = ry + lo.y;
                const double gdx = (rx + ex) - x0, gdy = (ry + ey) - y0;
                const double len2 = gdx * gdx + gdy * gdy;
                const auto ca = rgba(lo.rgb, lo.a * ra);
                const auto cb = rgba(hi.rgb, hi.a * ra);
                char gkey[320];
                std::snprintf(gkey, sizeof gkey, "g%s;%s;%.17g,%.17g,%.17g,%.17g", cssRgba(lo.rgb, lo.a * ra).c_str(), cssRgba(hi.rgb, hi.a * ra).c_str(), x0, y0, gdx, gdy);
                c.fillPolygon(
                    pts,
                    [=](double ux, double uy) {
                        const double tt = len2 > 0 ? std::clamp(((ux - x0) * gdx + (uy - y0) * gdy) / len2, 0.0, 1.0) : 0;
                        return std::array<double, 4>{ca[0] + (cb[0] - ca[0]) * tt, ca[1] + (cb[1] - ca[1]) * tt, ca[2] + (cb[2] - ca[2]) * tt, ca[3] + (cb[3] - ca[3]) * tt};
                    },
                    true, gkey);
            } else {
                const auto col = rgba(avg, aa * ra);
                c.fillPolygon(pts, [col](double, double) { return col; }, false, "s" + cssRgba(avg, aa * ra));
            }
        }
        c.restore();
    }
    // FEFONT text at scale% x FONT_K PS2 pixels; anchor bits as for sprites; word-wrapped to the element width
    void text(HudCanvas& c, const std::string& value, double x, double y, const Props& p, double alpha, bool wrap) {
        const double s = kFontK * prop(p, 9, 100) / 100;
        const int f = int(prop(p, 12, 9));
        const std::string align = f & 32 ? "right" : f & 16 ? "center" : "left";
        int rgb[3];
        for (int k = 0; k < 3; ++k) {
            rgb[k] = std::clamp(int(hud::jsRound(prop(p, 14 + k, 0) * kTextTexel)), 0, 255);
        }
        char hex[8];
        std::snprintf(hex, sizeof hex, "#%02x%02x%02x", rgb[0], rgb[1], rgb[2]);
        const auto& glyphs = text_.fonts.at("FEFONT");
        auto rawAdvance = [&](const std::string& str) {
            double w = 0;
            for (char32_t ch : hud::codePoints(str)) {
                auto it = glyphs.find(ch);
                const int a = it == glyphs.end() ? 0 : it->second.advance;
                w += a != 0 ? a : 10;
            }
            return w;
        };
        const double width = prop(p, 6, 0) != 0 ? prop(p, 6, 0) : 1e6;
        const double max = width / kPs2X;
        std::vector<std::string> lines;
        if (!wrap) {
            // split(/\r\n|\r|\n/)
            std::string cur;
            for (size_t i = 0; i < value.size(); ++i) {
                if (value[i] == '\r' || value[i] == '\n') {
                    lines.push_back(cur);
                    cur.clear();
                    if (value[i] == '\r' && i + 1 < value.size() && value[i + 1] == '\n') {
                        ++i;
                    }
                    continue;
                }
                cur += value[i];
            }
            lines.push_back(cur);
        } else {
            auto over = [&](const std::string& str) { return ps2Wrap ? rawAdvance(str) * prop(p, 9, 100) / 100 > width : rawAdvance(str) * s > max; };
            std::string line;
            size_t at = 0;
            while (true) {
                const size_t sp = value.find(' ', at);
                const std::string word = value.substr(at, sp == std::string::npos ? std::string::npos : sp - at);
                const std::string next = line.empty() ? word : line + " " + word;
                if (!line.empty() && over(next)) {
                    lines.push_back(line);
                    line = word;
                } else {
                    line = next;
                }
                if (sp == std::string::npos) {
                    break;
                }
                at = sp + 1;
            }
            if (!line.empty()) {
                lines.push_back(line);
            }
        }
        if (keepLead && wrap && !lines.empty()) {
            size_t n = 0;
            while (n < value.size() && value[n] == ' ') {
                ++n;
            }
            lines[0] = value.substr(0, n) + lines[0];
        }
        const double sy = p.count(1000) ? kFontK * p.at(1000) / 100 : s;
        const double pitch = p.count(1001) ? p.at(1001) : kLine * sy * kPs2Y;
        const double dy = (f & 2) ? -(double(lines.size()) - 1) * pitch / 2 - 11 * sy * kPs2Y * kTextTall : 0;
        c.save();
        c.setGlobalAlpha(std::min(1.0, alpha));
        c.translate(x, y + dy);
        c.scale(kPs2X / kPs2Y * s / sy / kTextTall, 1);
        for (size_t i = 0; i < lines.size(); ++i) {
            text_.text(c, lines[i], 0, double(i) * pitch, 22 * sy * kPs2Y * kTextTall, hex, "FEFONT", align);
        }
        c.restore();
    }

    const nlohmann::json& screen_;
    hud::HudImages& images_;
    hud::UiText& text_;
    std::map<std::string, const nlohmann::json*> byName_;
    std::map<const nlohmann::json*, double> index_;
    std::map<std::string, std::pair<const nlohmann::json*, bool>> bars_;
};

}  // namespace fe

}  // namespace ssx_native
