#pragma once
// The in-game "EA RADIO BIG" now-playing popup (web/now-playing.js, 1:1; single-player HUD cOVStateHUD1P, ctor 0x1E9A30): HUD
// command 5 from the audio's 28F478 when a playlist song starts (show), the 60 Hz update 0x1EA930 (the lines laid out at time 0,
// the slide in, the text fade, the hold to 7.2 s, the fade and slide out), and the draw 0x2204A0: three OV_1-4 slices and the
// HUDFONT header, FEFONT title / artist / album lines, in PS2 HUD space (640 x 480) scaled by 448 / 480 onto the UI canvas.
// The page draws it after the rest of the HUD (a microtask after ui.draw). The arithmetic is the page's: doubles chopped to float32
// where its f() chops.
#include "hud_canvas.hpp"
#include "hud_widgets.hpp"
#include "trick_hud.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace ssx_native {

namespace hud {

struct NowPlayingSong {
    std::string title, artist, album;
};

class NowPlayingHud {
public:
    // NOW_PLAYING
    static constexpr double kScale = 0.800000011920929;
    static constexpr double kFeScale = 0.7500171661376953;
    static constexpr double kHeaderHeight = 20.99884033203125;
    static constexpr double kTitleGap = 2.999871015548706;
    static constexpr double kPadWidth = 40;
    static constexpr double kPadHeight = 28.00471305847168;
    static constexpr double kInsetX = 20;
    static constexpr double kInsetY = 14.00235652923584;
    static constexpr double kWrapWidth = 300;
    static constexpr int kWrapChars = 0x60;
    static constexpr double kFrame = 0.01666666753590107;
    static constexpr double kRate = 2.5;
    static constexpr double kSlideMargin = 30;
    static constexpr double kAnchorX = 20;
    static constexpr double kAnchorY = 460;

    // ui.fonts {FEFONT, HUDFONT} (the raw tables with the signed bytes, glyphOf) and the UI images
    const GlyphTable* fe = nullptr;
    const GlyphTable* hudFont = nullptr;
    HudImages* images = nullptr;
    // command 3 / 4 counter (the in-game Audio / Radio menus): suppresses drawing only
    int hidden = 0;

    // HUD command 5 (0x1EC2E0)
    void show(const NowPlayingSong& s) {
        const bool was = active_;
        active_ = !s.title.empty();
        if (active_ && was && key_ == s.title) {
            return;
        }
        key_ = s.title;
        song_ = s;
        time_ = -1;
        slide_ = 0;
        alpha_ = 0;
        layout_.reset();
    }
    void clear() {
        active_ = false;
        key_.clear();
        time_ = -1;
        slide_ = 0;
        alpha_ = 0;
        layout_.reset();
    }
    // one 60 Hz HUD update (0x1EABB0..0x1EB15C)
    void tick() {
        if (!active_) {
            return;
        }
        if (time_ < 0) {
            time_ = 0;
            slide_ = 0;
            alpha_ = 0;
            layout_ = layout(song_);
            return;
        }
        time_ = f(time_ + kFrame);
        const Phase p = phase(time_);
        if (p.done) {
            key_.clear();
            clear();
            return;
        }
        slide_ = p.slide;
        alpha_ = p.alpha;
    }
    // the page's update(dt): whole 60 Hz ticks of the frame time (at most 0.25 s carried)
    void update(double dt) {
        if (!active_) {
            acc_ = 0;
            return;
        }
        acc_ = std::min(acc_ + std::max(0.0, dt), 0.25);
        while (acc_ >= 1.0 / 60 - 1e-9) {
            acc_ -= 1.0 / 60;
            tick();
        }
    }
    // web/audio-menu.js drawHud: the time since the last draw (a gap over 0.25 s counts one frame)
    void frame(double nowSeconds) {
        const double dt = hudAt_ ? nowSeconds - *hudAt_ : 0;
        hudAt_ = nowSeconds;
        update(dt > 0.25 ? 1.0 / 60 : dt);
    }
    bool visible() const {
        return active_ && hidden == 0 && time_ > 0 && layout_.has_value();
    }
    // QA (native/tests/now-playing-cases.mjs): the state and the box nowPlayingDrawList places, each line's text, width and height
    struct View {
        bool active = false;
        double time = 0, slide = 0, alpha = 0;
        double x = 0, y = 0, w = 0, h = 0;
        std::vector<std::tuple<std::string, double, double>> lines;
    };
    View view() const {
        View o;
        o.active = active_;
        o.time = time_;
        o.slide = slide_;
        o.alpha = alpha_;
        if (layout_) {
            box(o.x, o.y);
            o.w = layout_->width;
            o.h = layout_->height;
            for (const auto& l : layout_->lines) {
                o.lines.emplace_back(l.text, l.w, l.h);
            }
        }
        return o;
    }
    void draw(HudCanvas& c) {
        if (!visible()) {
            return;
        }
        const Layout& L = *layout_;
        const double W = L.width;
        const double H = L.height;
        double x = 0;
        double y = 0;
        box(x, y);
        c.save();
        c.scale(1, 448.0 / 480);
        // the three OV_1-4 slices ([v0, u0, u1, v1]; the middle one W - 56 wide)
        static const double kUv[3][4] = {{0.689453125, 0.009765625, 0.169921875, 0.998046875},
                                         {0.689453125, 0.173828125, 0.173828125, 0.998046875},
                                         {0.689453125, 0.177734375, 0.228515625, 0.998046875}};
        const double widths[3] = {42, f(W + -56), 14};
        const int page = images->png("OV_1-4");
        const HudImage& im = c.image(page);
        double px = x;
        for (int k = 0; k < 3; ++k) {
            const double v0 = kUv[k][0];
            const double u0 = kUv[k][1];
            const double u1 = kUv[k][2];
            const double v1 = kUv[k][3];
            // a zero-width UV span (the stretched middle column) samples the texel under u0
            const double sx = u1 > u0 ? u0 * im.width : std::floor(u0 * im.width);
            const double sw = u1 > u0 ? (u1 - u0) * im.width : 1;
            c.setGlobalAlpha(1);
            c.drawImage(page, sx, v0 * im.height, sw, (v1 - v0) * im.height, px, y, widths[k], H);
            px = f(px + widths[k]);
        }
        double ty = f(y + kInsetY);
        for (const Line& l : L.lines) {
            const bool white = l.colour == 1;
            const double a = std::clamp(std::trunc(f(1 * alpha_) * 128) / 128, 0.0, 1.0);
            const double tx = f(x + kInsetX);
            if (a > 0) {
                const GlyphTable& font = l.header ? *hudFont : *fe;
                const double scale = l.header ? kScale : f(kScale * kFeScale);
                const uint8_t v = white ? 255 : 0;
                const int tinted = images->multiply(std::string(l.header ? "HUDFONT" : "FEFONT") + "-0", {v, v, v});
                c.setGlobalAlpha(a);
                double gx = tx;
                for (char32_t ch : codePoints(l.text)) {
                    auto it = font.find(ch);
                    if (it == font.end()) {
                        continue;
                    }
                    const Glyph& g = it->second;
                    c.drawImage(tinted, g.x, g.y, g.w, g.h, gx + g.dx * scale, ty + g.dy * scale, g.w * scale, g.h * scale);
                    gx += g.advance * scale;
                }
            }
            ty = f(ty + l.h);
        }
        c.restore();
    }

private:
    // nowPlayingDrawList's box corner (one player: anchored left, sliding in from the left)
    void box(double& x, double& y) const {
        const double W = layout_->width;
        const double H = layout_->height;
        x = kAnchorX;
        y = f(kAnchorY - H);
        if (slide_ < 1) {
            const double e = f(f(1 - slide_) * f(1 - slide_));
            x = f(x - f(e * f(f(x + kSlideMargin) + W)));
        }
    }
    struct Phase {
        double slide, alpha;
        bool done;
    };
    struct Line {
        std::string text;
        bool header = false;
        double w = 0;
        double h = 0;
        // 0 black (header, artist, album), 1 white (title)
        int colour = 0;
    };
    struct Layout {
        std::vector<Line> lines;
        double width = 0;
        double height = 0;
    };

    // popupPhase (0x1EB02C..0x1EB128)
    static Phase phase(double t) {
        const double a = 0.4000000059604645;
        const double b = 0.800000011920929;
        const double cT = 7.199999809265137;
        const double d = 7.599999904632568;
        const double e = 8;
        if (t < a) {
            return {f(t * kRate), 0, false};
        }
        if (t < b) {
            return {1, f(f(t - a) * kRate), false};
        }
        if (t < cT) {
            return {1, 1, false};
        }
        if (t < d) {
            return {1, f(1 - f(f(t - cT) * kRate)), false};
        }
        if (t < e) {
            return {f(1 - f(f(t - d) * kRate)), 0, false};
        }
        return {0, 0, true};
    }
    // 0x391FB0 ink width (shadow 0)
    static double measure(const GlyphTable& font, const std::vector<char32_t>& text, double sx) {
        double pen = 0;
        double minX = 0;
        double maxX = 0;
        for (char32_t ch : text) {
            auto it = font.find(ch);
            if (it == font.end()) {
                continue;
            }
            const Glyph& g = it->second;
            minX = std::min(f(pen + f(g.dx * sx)), minX);
            maxX = std::max(f(pen + f((g.dx + g.w) * sx)), maxX);
            pen = f(pen + f(g.advance * sx));
        }
        return f(maxX + minX);
    }
    // 0x392430: one line of text fitting the width; rest none when done
    static std::pair<std::vector<char32_t>, std::optional<std::vector<char32_t>>> wrap(const GlyphTable& font, const std::vector<char32_t>& text, double sx) {
        using Text = std::vector<char32_t>;
        auto slice = [&](size_t from, size_t to) { return Text(text.begin() + long(std::min(from, text.size())), text.begin() + long(std::min(to, text.size()))); };
        auto skip = [&](size_t i) {
            while (i < text.size() && text[i] == U' ') {
                i++;
            }
            return i;
        };
        auto breakAt = [&](long end) {
            return std::make_pair(slice(0, size_t(end + 1)), std::optional<Text>(slice(skip(size_t(end + 1)), text.size())));
        };
        if (text.empty()) {
            return {Text{}, std::nullopt};
        }
        double pen = 0;
        double carry = 0;
        long end = -1;
        size_t i = 0;
        for (;;) {
            const char32_t ch = text[i];
            if (ch == U' ') {
                if (kWrapWidth <= pen) {
                    return breakAt(end);
                }
                if (i > 0 && text[i - 1] != U' ') {
                    end = long(i) - 1;
                }
            }
            auto it = font.find(ch);
            if (it != font.end()) {
                const Glyph& g = it->second;
                const double ink = f((g.dx + g.w) * sx);
                const double adv = f(g.advance * sx);
                const double step = f(carry + ink);
                carry = f(adv - ink);
                pen = f(pen + step);
            }
            if (kWrapWidth <= pen && end == -1) {
                return {slice(0, i), std::optional<Text>(slice(i, text.size()))};
            }
            i++;
            if (i == size_t(kWrapChars - 1) || i >= text.size()) {
                break;
            }
        }
        if (kWrapWidth <= pen) {
            return breakAt(end);
        }
        return {slice(0, i), std::nullopt};
    }
    static std::string str(const std::vector<char32_t>& t) {
        std::string s;
        for (char32_t ch : t) {
            s += utf8(ch);
        }
        return s;
    }
    // layoutNowPlaying (0x1EABB0..0x1EB024)
    Layout layout(const NowPlayingSong& song) const {
        Layout L;
        const double fes = f(kScale * kFeScale);
        const double lineH = f(f(f(29.0 * 1) * kScale) * kFeScale);
        L.lines.push_back({"EA RADIO BIG", true, measure(*hudFont, codePoints("EA RADIO BIG"), kScale), kHeaderHeight, 0});
        double total = kHeaderHeight;
        std::optional<std::vector<char32_t>> rest = codePoints(song.title);
        // (a bound the page's do-while has not: a title no line can take)
        for (int guard = 0; guard < 256; ++guard) {
            auto [line, next] = wrap(*fe, *rest, fes);
            L.lines.push_back({str(line), false, measure(*fe, line, fes), lineH, 1});
            total = f(total + lineH);
            rest = next;
            if (!rest || rest->empty()) {
                break;
            }
        }
        total = f(total + kTitleGap);
        L.lines.back().h = f(L.lines.back().h + kTitleGap);
        for (const std::string* t : {&song.artist, &song.album}) {
            const auto cps = codePoints(*t);
            L.lines.push_back({*t, false, measure(*fe, cps, fes), lineH, 0});
            total = f(total + lineH);
        }
        double maxW = 0;
        for (const auto& l : L.lines) {
            maxW = std::max(maxW, l.w);
        }
        L.width = f(maxW + kPadWidth);
        L.height = f(total + kPadHeight);
        return L;
    }

    bool active_ = false;
    std::string key_;
    NowPlayingSong song_;
    double time_ = -1;
    double slide_ = 0;
    double alpha_ = 0;
    std::optional<Layout> layout_;
    double acc_ = 0;
    std::optional<double> hudAt_;
};

}  // namespace hud

}  // namespace ssx_native
