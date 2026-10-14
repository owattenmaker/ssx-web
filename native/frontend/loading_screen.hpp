#pragma once
// The event load screen (web/loading-screen.js and web/load-meter.js, 1:1; docs/loading-screen.md): GL.LUI 110ctrl_load played from
// its exported layout (LOADING/loading.json: elements, frame-0 properties, timeline events, animations, the GL_1 pages), the
// controls panel for the device (the DualShock picture, or the keyboard rows for the Classic / Simple layout), one original load
// hint (kT_FEHINT*, rotated like 0x245950), the percentage of the load's work (pv loadMeter: the stages weighted by their typical
// time, creeping between reports, never backwards) and the 20-frame fades. Drawn on the 640 x 448 UI canvas over black.
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

// web/load-meter.js createLoadMeter: the work stages only (native loads read local files: no download stages)
class LoadMeter {
public:
    static constexpr double kCreepMax = 0.9;
    static constexpr double kLead = 0.02;
    static constexpr double kLeadRate = 0.005 / 1000;
    static double stageMs(const std::string& id) {
        static const std::map<std::string, double> ms = {{"course", 2500}, {"rider", 100}, {"lineup", 100}, {"warm", 3000}, {"intro", 600},
                                                          {"unload", 100},  {"ride", 700},  {"world", 1800},  {"gc", 200},     {"pending", 3800}};
        auto it = ms.find(id);
        return it == ms.end() ? 0 : it->second;
    }
    void plan(const std::vector<std::pair<std::string, double>>& list, double t, double showing) {
        std::vector<std::pair<std::string, double>> items;
        for (const auto& [id, w] : list) {
            if (w > 0) {
                items.push_back({id, w});
            }
        }
        auto pending = find("pending");
        if (pending && !pending->done && !items.empty()) {
            double sum = 0;
            for (const auto& i : items) {
                sum += i.second;
            }
            const double pms = pending->ms;
            erase("pending");
            for (const auto& [id, ms] : items) {
                erase(id);
                stages_.push_back({id, fresh(sum > 0 ? pms * ms / sum : ms)});
            }
            return;
        }
        double from = !stages_.empty() ? fraction(t) : std::max(base_, showing);
        if (!stages_.empty() && showing > 0) {
            from = std::min(from, showing);
        }
        base_ = std::min(0.999, from);
        stages_.clear();
        for (const auto& [id, ms] : items) {
            stages_.push_back({id, fresh(ms)});
        }
    }
    void begin(const std::string& id, double t) {
        if (auto* s = find(id); s && !s->start) {
            s->start = t;
        }
    }
    void step(const std::string& id, double fraction, double ceiling, double t) {
        Stage* s = find(id);
        if (!s || s->done) {
            return;
        }
        if (!s->start) {
            s->start = t;
        }
        if (!s->reported) {
            s->floor = creep(*s, t);
        } else {
            s->floor = std::max(s->floor, stageFraction(*s, t));
        }
        s->reported = true;
        s->f = std::max(s->f, std::min(1.0, fraction));
        s->ceiling = std::min(1.0, ceiling);
        s->at = t;
        if (s->f >= 1) {
            s->done = true;
        }
    }
    void done(const std::string& id, double t) {
        Stage* s = find(id);
        if (!s) {
            return;
        }
        if (!s->start) {
            s->start = t;
        }
        s->done = true;
        s->f = 1;
    }
    double fraction(double t) const {
        if (stages_.empty()) {
            return base_;
        }
        double total = 0;
        double sum = 0;
        for (const auto& [id, s] : stages_) {
            total += s.ms;
            sum += s.ms * stageFraction(s, t);
        }
        return base_ + (1 - base_) * (total > 0 ? sum / total : 0);
    }
    double value(double t) {
        const double target = fraction(t);
        const double dt = lastAt_ ? std::max(0.0, t - *lastAt_) : 1000;
        lastAt_ = t;
        const double k = 1 - std::exp(-dt / 150);
        shown_ = std::max(shown_, shown_ + (target - shown_) * k);
        if (target >= 1) {
            shown_ = 1;
        } else if (!stages_.empty()) {
            shown_ = std::max(shown_, std::min({target + kLead, 0.99, shown_ + kLeadRate * dt}));
        }
        return shown_;
    }
    // the stage under way: the first begun and not done, in plan order
    std::optional<std::string> current() const {
        for (const auto& [id, s] : stages_) {
            if (!s.done && s.start) {
                return id;
            }
        }
        return std::nullopt;
    }

private:
    struct Stage {
        double ms = 0;
        double f = 0, floor = 0, ceiling = 0, at = 0;
        std::optional<double> start;
        bool reported = false, done = false;
    };
    static Stage fresh(double ms) {
        Stage s;
        s.ms = ms;
        return s;
    }
    Stage* find(const std::string& id) {
        for (auto& [k, s] : stages_) {
            if (k == id) {
                return &s;
            }
        }
        return nullptr;
    }
    void erase(const std::string& id) {
        stages_.erase(std::remove_if(stages_.begin(), stages_.end(), [&](const auto& p) { return p.first == id; }), stages_.end());
    }
    static double creep(const Stage& s, double t) {
        return kCreepMax * std::min(1.0, (t - *s.start) / std::max(1.0, s.ms));
    }
    static double stageFraction(const Stage& s, double t) {
        if (s.done) {
            return 1;
        }
        if (!s.start) {
            return 0;
        }
        if (!s.reported) {
            return creep(s, t);
        }
        double x = std::max(s.f, s.floor);
        if (s.ceiling > x) {
            x += (s.ceiling - x) * kCreepMax * std::min(1.0, (t - s.at) / std::max(1.0, s.ms * (s.ceiling - x)));
        }
        return std::min(0.999, x);
    }
    // insertion order (a JS Map)
    std::vector<std::pair<std::string, Stage>> stages_;
    double base_ = 0;
    double shown_ = 0;
    std::optional<double> lastAt_;
};

class LoadingScreen {
public:
    static constexpr double kFps = 60;
    static constexpr int kHintCount = 15;
    static constexpr int kHintSkip = 12;
    static constexpr double kFontK = 0.79;
    static constexpr double kPs2X = 640.0 / 512;
    static constexpr double kPs2Y = 480.0 / 448;
    static constexpr double kFadeFrames = 20;
    static constexpr double kDoneFrames = 12;

    // the UI's FEFONT (raw glyph table and its source-in tints), the load screen's pages
    hud::UiText* text = nullptr;
    hud::HudImages* pages = nullptr;
    // input-glyphs.js inputDevice: the keyboard unless a pad was the last device; pad-input.js keyboard mode (Classic by default)
    bool gamepad = false;
    std::string keyboardMode = "Classic";

    bool load(const nlohmann::json& data) {
        data_ = data;
        if (!data_.is_object() || !data_.contains("elements")) {
            return false;
        }
        size_t i = 0;
        for (const auto& e : data_["elements"]) {
            Element el;
            el.json = &e;
            el.index = i++;
            el.name = e.value("name", "");
            el.kind = e.value("kind", "");
            el.layer = e.value("layer", 0);
            el.parent = e.contains("parent") && e["parent"].is_string() ? e["parent"].get<std::string>() : "";
            byName_[el.name] = elements_.size();
            elements_.push_back(el);
        }
        return true;
    }
    bool active() const {
        return session_.has_value();
    }

    // open(): the session, its hint (0x245950: n = counter + 1, hint 12 skipped; the counter is the page's sessionStorage value)
    void open(double nowMs) {
        if (session_) {
            return;
        }
        int c = hintCounter_ >= 0 && hintCounter_ < kHintCount ? hintCounter_ : 0;
        int hint = c + 1;
        if (hint == kHintSkip) {
            c += 1;
            hint = c + 1;
        }
        hintCounter_ = (c + 1) % kHintCount;
        Session s;
        s.start = nowMs;
        for (const auto& h : data_["hints"]) {
            if (h.value("index", -1) == hint) {
                s.hintTitle = h.value("title", "");
                s.hintBody = h.value("body", "");
                s.hasHint = true;
            }
        }
        session_ = s;
    }
    // the load's work: the stages (load-meter.js plan / begin / step / done)
    void plan(const std::vector<std::string>& ids, double nowMs) {
        if (!session_) {
            return;
        }
        std::vector<std::pair<std::string, double>> list;
        for (const auto& id : ids) {
            list.push_back({id, LoadMeter::stageMs(id)});
        }
        session_->meter.plan(list, nowMs, session_->shownPercent / 99.0);
    }
    void begin(const std::string& id, double nowMs) {
        if (session_) {
            session_->meter.begin(id, nowMs);
        }
    }
    void step(const std::string& id, double f, double ceiling, double nowMs) {
        if (session_) {
            session_->meter.step(id, f, ceiling, nowMs);
        }
    }
    void done(const std::string& id, double nowMs) {
        if (session_) {
            session_->meter.done(id, nowMs);
        }
    }
    // run(next): the load's work has settled (the page's pending promises: 0) and next() runs after 100% and the fade
    void finish(std::function<void()> next) {
        if (session_) {
            session_->next = std::move(next);
            session_->settled = true;
        }
    }

    // one frame (web/loading-screen.js draw): black, the screen, the keyboard panel or the pad picture, the hint, the fade
    void draw(HudCanvas& c, double nowMs) {
        const auto st = update(nowMs);
        if (!st) {
            return;
        }
        // the #ui-bg canvas: black under the screen (not the #ui canvas's calls)
        const bool logging = c.logging;
        c.logging = false;
        c.fillRect(0, 0, 640, 448, {0, 0, 0});
        c.logging = logging;
        c.save();
        c.scale(1, 448.0 / 480);
        drawScreen(c, *st);
        if (!gamepad) {
            drawKeyboard(c);
        }
        drawHint(c);
        if (st->black > 0) {
            c.setGlobalAlpha(st->black);
            c.fillRect(0, 0, 640, 480, {0, 0, 0});
        }
        c.restore();
    }

    // QA (native/tools/ui-shot.mjs, ssx3_ui_shot): a frame drawn at a given state, the hint counter as the page's sessionStorage holds it
    void setHintCounter(int counter) {
        hintCounter_ = counter;
    }
    void drawAt(HudCanvas& c, double frame, int percent, double black) {
        if (!session_) {
            open(0);
        }
        const State st{frame, percent, black};
        // the #ui-bg canvas: black under the screen (not the #ui canvas's calls)
        const bool logging = c.logging;
        c.logging = false;
        c.fillRect(0, 0, 640, 448, {0, 0, 0});
        c.logging = logging;
        c.save();
        c.scale(1, 448.0 / 480);
        drawScreen(c, st);
        if (!gamepad) {
            drawKeyboard(c);
        }
        drawHint(c);
        if (st.black > 0) {
            c.setGlobalAlpha(st.black);
            c.fillRect(0, 0, 640, 480, {0, 0, 0});
        }
        c.restore();
    }

private:
    struct Element {
        const nlohmann::json* json = nullptr;
        size_t index = 0;
        std::string name, kind, parent;
        int layer = 0;
    };
    struct Session {
        double start = 0;
        LoadMeter meter;
        bool settled = false;
        std::function<void()> next;
        std::optional<double> doneAt;
        int shownPercent = 0;
        bool hasHint = false;
        std::string hintTitle, hintBody;
    };
    struct State {
        double frame = 0;
        int percent = 0;
        double black = 0;
    };
    using Props = std::map<int, double>;

    std::optional<State> update(double nowMs) {
        if (!session_) {
            return std::nullopt;
        }
        Session& s = *session_;
        const double frame = (nowMs - s.start) * kFps / 1000;
        // pv loadMeter: no minimum time; done when the work has settled
        if (!s.doneAt && s.settled) {
            s.doneAt = frame;
        }
        const int percent = s.doneAt ? 100 : int(std::floor(99 * s.meter.value(nowMs)));
        s.shownPercent = std::max(s.shownPercent, percent);
        double black = std::max(0.0, 1 - frame / kFadeFrames);
        if (s.doneAt) {
            const double out = (frame - *s.doneAt - kDoneFrames) / kFadeFrames;
            black = std::max(black, std::min(1.0, std::max(0.0, out)));
            if (out >= 1) {
                auto next = std::move(s.next);
                session_.reset();
                if (next) {
                    next();
                }
                return std::nullopt;
            }
        }
        return State{frame, s.shownPercent, black};
    }

    static Props propsOf(const nlohmann::json& j) {
        Props p;
        if (!j.is_object()) {
            return p;
        }
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (it.value().is_number()) {
                p[std::stoi(it.key())] = it.value().get<double>();
            }
        }
        return p;
    }
    static double prop(const Props& p, int k, double fallback) {
        auto it = p.find(k);
        return it == p.end() ? fallback : it->second;
    }
    // trackValue: [[value, frames to next], ..., [last, 0]], linear between keys, held after the last
    static double trackValue(const nlohmann::json& keys, double frame) {
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
    // elementProps: the frame-0 properties, later property events, the element's current animation
    Props elementProps(const Element& e, double frame) const {
        const nlohmann::json& j = *e.json;
        Props props = propsOf(j.contains("props") ? j["props"] : nlohmann::json());
        bool hasProps = j.contains("props") && j["props"].is_object();
        std::optional<std::tuple<std::string, int, double>> anim;
        if (j.contains("anim") && j["anim"].is_object()) {
            anim = std::make_tuple(j["anim"].value("hash", ""), j["anim"].value("mode", 0), 0.0);
        }
        for (const auto& ev : data_["events"]) {
            if (ev.value("frame", 0.0) > frame || ev.value("element", "") != e.name) {
                continue;
            }
            if (ev.contains("props")) {
                props = propsOf(ev["props"]);
                hasProps = true;
                anim.reset();
            } else {
                anim = std::make_tuple(ev.value("anim", ""), ev.value("mode", 0), ev.value("frame", 0.0));
            }
        }
        if (anim && data_["animations"].contains(std::get<0>(*anim))) {
            const auto& a = data_["animations"][std::get<0>(*anim)];
            const double length = std::max(1.0, a.value("frames", 1.0));
            const double f = frame - std::get<2>(*anim);
            const double t = (std::get<1>(*anim) & 8) ? std::fmod(f, length) : std::min(f, length);
            for (auto it = a["tracks"].begin(); it != a["tracks"].end(); ++it) {
                props[std::stoi(it.key())] = trackValue(it.value(), t);
            }
            hasProps = true;
        }
        (void)hasProps;
        return props;
    }
    bool visibleFor(const Element& e) const {
        static const std::set<std::string> keep = {"0514b410", "07730486", "08bc5120", "07730481", "08bc5121", "07730485"};
        static const std::set<std::string> padOnly = {"0a6b545c", "02c8befe", "08976274", "05f515af", "0e303d74"};
        const Element* n = &e;
        while (n) {
            if (n->name == "0917a214") {
                return false;
            }
            if (!gamepad && padOnly.count(n->name)) {
                return false;
            }
            if (n->name == "05f515af" && !keep.count(e.name)) {
                return false;
            }
            auto it = n->parent.empty() ? byName_.end() : byName_.find(n->parent);
            n = it == byName_.end() ? nullptr : &elements_[it->second];
        }
        return true;
    }
    struct Abs {
        double x = 0, y = 0, a = 1;
        Props p;
    };
    void drawScreen(HudCanvas& c, const State& st) {
        std::map<std::string, Abs> cache;
        std::function<const Abs&(const Element&)> abs = [&](const Element& e) -> const Abs& {
            auto it = cache.find(e.name);
            if (it != cache.end()) {
                return it->second;
            }
            Abs r;
            r.p = elementProps(e, st.frame);
            Abs parent;
            if (!e.parent.empty()) {
                auto pi = byName_.find(e.parent);
                if (pi != byName_.end()) {
                    parent = abs(elements_[pi->second]);
                }
            }
            r.x = parent.x + prop(r.p, 0, 0);
            r.y = parent.y + prop(r.p, 1, 0);
            r.a = parent.a * (prop(r.p, 13, 255) / 255);
            return cache.emplace(e.name, r).first->second;
        };
        std::vector<std::pair<const Element*, Abs>> items;
        for (const auto& e : elements_) {
            if (e.kind == "group" || !visibleFor(e)) {
                continue;
            }
            const Abs r = abs(e);
            if (r.a <= 0) {
                continue;
            }
            items.push_back({&e, r});
        }
        std::stable_sort(items.begin(), items.end(), [](const auto& a, const auto& b) {
            if (a.first->layer != b.first->layer) {
                return a.first->layer < b.first->layer;
            }
            return a.first->index < b.first->index;
        });
        for (const auto& [e, r] : items) {
            if (e->kind == "sprite") {
                drawSprite(c, (*e->json)["sprite"], r);
            } else if (e->kind == "shape") {
                drawShape(c, r);
            } else if (e->kind == "text") {
                std::string t = e->name == "00000025" ? std::to_string(st.percent) + "%"
                                : e->name == "0514b410"   ? data_["strings"].value("hand_plant", "")
                                                          : e->json->value("text", "");
                if (!t.empty()) {
                    drawText(c, t, r.x, r.y, r.p, r.a);
                }
            }
        }
    }
    static std::array<double, 2> anchor(const Props& p, double w, double h) {
        const int f = int(prop(p, 12, 9));
        const double ax = f & 16 ? 0.5 : f & 32 ? 1 : 0;
        const double ay = f & 2 ? 0.5 : f & 4 ? 1 : 0;
        return {-w * ax, -h * ay};
    }
    void drawSprite(HudCanvas& c, const nlohmann::json& sprite, const Abs& r) {
        if (!sprite.is_object()) {
            return;
        }
        const Props& p = r.p;
        const double sw = sprite["sw"].get<double>();
        const double sh = sprite["sh"].get<double>();
        const double sx = sprite["sx"].get<double>();
        const double sy = sprite["sy"].get<double>();
        const double w0 = prop(p, 6, 0);
        const double h0 = prop(p, 7, 0);
        const double w = (w0 != 0 ? w0 : sw) * prop(p, 9, 100) / 100;
        const double h = (h0 != 0 ? h0 : sh) * prop(p, 10, 100) / 100;
        const std::array<uint8_t, 3> rgb = {uint8_t(std::clamp(hud::jsRound(prop(p, 14, 255)), 0.0, 255.0)), uint8_t(std::clamp(hud::jsRound(prop(p, 15, 255)), 0.0, 255.0)),
                                            uint8_t(std::clamp(hud::jsRound(prop(p, 16, 255)), 0.0, 255.0))};
        const auto [ox, oy] = anchor(p, w, h);
        c.save();
        c.setGlobalAlpha(std::min(1.0, r.a));
        c.translate(r.x, r.y);
        if (prop(p, 5, 0) != 0) {
            c.rotate(prop(p, 5, 0) * M_PI / 180);
        }
        const std::string page = sprite["page"].get<std::string>();
        if (rgb[0] >= 255 && rgb[1] >= 255 && rgb[2] >= 255) {
            c.drawImage(pages->png(page), sx, sy, sw, sh, ox, oy, w, h);
        } else {
            const int im = pages->multiplyRegion(page, sx, sy, sw, sh, rgb);
            c.drawImage(im, 0, 0, sw, sh, ox, oy, w, h);
        }
        c.restore();
    }
    // four gouraud vertices: a flat quad, or a two-colour ramp (vertical or horizontal)
    void drawShape(HudCanvas& c, const Abs& r) {
        const Props& p = r.p;
        struct V {
            double x, y, a;
            std::array<double, 3> rgb;
        };
        std::array<V, 4> v;
        for (int k = 0; k < 4; ++k) {
            v[size_t(k)] = {prop(p, 21 + 9 * k, 0), prop(p, 22 + 9 * k, 0), prop(p, 26 + 9 * k, 255) / 255,
                            {prop(p, 27 + 9 * k, 255), prop(p, 28 + 9 * k, 255), prop(p, 29 + 9 * k, 255)}};
        }
        // css(q): rgba(r, g, b, a x r.a) (the canvas keeps the alpha as a float, the colour as bytes)
        auto css = [&](const V& q) {
            return std::array<double, 4>{std::clamp(hud::jsRound(q.rgb[0]), 0.0, 255.0) / 255, std::clamp(hud::jsRound(q.rgb[1]), 0.0, 255.0) / 255,
                                         std::clamp(hud::jsRound(q.rgb[2]), 0.0, 255.0) / 255,
                                         std::round(std::clamp(q.a * r.a, 0.0, 1.0) * 255) / 255};
        };
        auto same = [](const V& a, const V& b) { return a.a == b.a && a.rgb == b.rgb; };
        std::vector<std::array<double, 2>> pts;
        for (const auto& q : v) {
            pts.push_back({r.x + q.x, r.y + q.y});
        }
        c.save();
        if (same(v[0], v[2])) {
            const auto col = css(v[0]);
            char key[96];
            std::snprintf(key, sizeof key, "s%.17g,%.17g,%.17g,%.17g", col[0], col[1], col[2], col[3]);
            c.fillPolygon(pts, [col](double, double) { return col; }, false, key);
        } else {
            const V& a = v[0];
            const V& b = same(v[0], v[1]) ? v[3] : v[1];
            const double x0 = r.x + a.x;
            const double y0 = r.y + a.y;
            const double dx = (r.x + b.x) - x0;
            const double dy = (r.y + b.y) - y0;
            const double len2 = dx * dx + dy * dy;
            const auto ca = css(a);
            const auto cb = css(b);
            char gradientKey[256];
            std::snprintf(gradientKey, sizeof gradientKey, "g%.17g,%.17g,%.17g,%.17g;%.17g,%.17g,%.17g,%.17g;%.17g,%.17g,%.17g,%.17g", ca[0], ca[1], ca[2], ca[3], cb[0], cb[1],
                          cb[2], cb[3], x0, y0, dx, dy);
            c.fillPolygon(
                pts,
                [=](double ux, double uy) {
                    const double t = len2 > 0 ? std::clamp(((ux - x0) * dx + (uy - y0) * dy) / len2, 0.0, 1.0) : 0;
                    return std::array<double, 4>{ca[0] + (cb[0] - ca[0]) * t, ca[1] + (cb[1] - ca[1]) * t, ca[2] + (cb[2] - ca[2]) * t, ca[3] + (cb[3] - ca[3]) * t};
                },
                true, gradientKey);
        }
        c.restore();
    }
    // LUI text: FEFONT at scale% x FONT_K PS2 pixels; anchor bits as for sprites
    void drawText(HudCanvas& c, const std::string& value, double x, double y, const Props& p, double alpha = 1,
                  const std::optional<std::array<int, 3>>& colour = std::nullopt) {
        const double s = kFontK * prop(p, 9, 100) / 100;
        const int f = int(prop(p, 12, 9));
        const std::string align = f & 32 ? "right" : f & 16 ? "center" : "left";
        std::array<int, 3> rgb = colour ? *colour : std::array<int, 3>{int(prop(p, 14, 37)), int(prop(p, 15, 7)), int(prop(p, 16, 5))};
        char hex[8];
        std::snprintf(hex, sizeof hex, "#%02x%02x%02x", std::clamp(int(hud::jsRound(rgb[0])), 0, 255), std::clamp(int(hud::jsRound(rgb[1])), 0, 255),
                      std::clamp(int(hud::jsRound(rgb[2])), 0, 255));
        c.save();
        c.setGlobalAlpha(std::min(1.0, alpha));
        c.translate(x, y);
        c.scale(kPs2X / kPs2Y, 1);
        text->text(c, value, 0, 0, 22 * s * kPs2Y, hex, "FEFONT", align);
        c.restore();
    }
    double textWidth(const std::string& value, double scale) const {
        const auto& g = text->fonts.at("FEFONT");
        double w = 0;
        for (char32_t ch : hud::codePoints(value)) {
            auto it = g.find(ch);
            const int a = it == g.end() ? 0 : it->second.advance;
            w += a != 0 ? a : 10;
        }
        return w * kFontK * scale / 100 * kPs2X;
    }
    // keyboardRows (web/pad-input.js KEYBOARD_BUTTONS and the Simple / Classic routing), labelled with the screen's strings
    struct Row {
        std::vector<std::string> keys, alt;
        std::string label, note;
    };
    std::vector<Row> keyboardRows() const {
        const bool simple = keyboardMode != "Classic";
        const auto& s = data_["strings"];
        auto str = [&](const char* k) { return s.value(k, std::string()); };
        std::vector<Row> rows;
        rows.push_back({{"W", "A", "S", "D"}, {"←", "↑", "↓", "→"}, simple ? str("turn_spin_flip") : str("turn"), simple ? "Steer; spin/flip in the air" : "Steer"});
        if (!simple) {
            rows.push_back({{"I", "J", "K", "L"}, {}, str("turn_spin_flip"), "Spin/flip in the air"});
        }
        rows.push_back({{"Space"}, {}, str("jump"), "Hold to crouch, release to jump"});
        rows.push_back({{"Shift"}, {}, str("boost_tweak"), "Boost; tweak a grab in the air"});
        rows.push_back({{"Q", "Z", "E", "X"}, {}, str("grab_board"), "In the air (L1 L2 R1 R2)"});
        rows.push_back({{"C"}, {}, str("hand_plant"), "Near anything you can grind"});
        rows.push_back({{"T", "F", "G", "H"}, {}, str("board_press"), "V: ollie out of a press"});
        rows.push_back({{"Backspace"}, {}, str("reset"), "Back onto the course"});
        rows.push_back({{"Esc"}, {}, str("pause"), ""});
        return rows;
    }
    double keycapWidth(const std::string& label) const {
        return hud::codePoints(label).size() == 1 ? 17 : textWidth(label, 36) + 10;
    }
    void drawKeyboard(HudCanvas& c) {
        const auto rows = keyboardRows();
        drawText(c, std::string("Keyboard - ") + (keyboardMode == "Classic" ? "Classic" : "Simple"), 55, 88, {{9, 60}, {12, 9}});
        const double right = 262;
        const double step = rows.size() > 8 ? 19 : 21;
        const double top = 122;
        for (size_t i = 0; i < rows.size(); ++i) {
            const Row& row = rows[i];
            const double y = top + double(i) * step;
            std::vector<std::string> caps = row.keys;
            caps.insert(caps.end(), row.alt.begin(), row.alt.end());
            std::vector<double> widths;
            double sum = 0;
            for (const auto& k : caps) {
                widths.push_back(keycapWidth(k));
                sum += widths.back() + 3;
            }
            const double gap = row.alt.empty() ? 0 : 8;
            double x = right - sum - gap;
            for (size_t j = 0; j < caps.size(); ++j) {
                if (j == row.keys.size()) {
                    x += gap;
                }
                hud::TrickHudRenderer::drawKeyCap(
                    c, [&](const std::string& v, double size) { text->text(c, v, 0, 0, size, "#f0f5f8", "FEFONT", "center"); },
                    caps[j], x, y, widths[j], 16, kPs2X / kPs2Y);
                x += widths[j] + 3;
            }
            c.fillRect(right + 3, y + 7, 14, 2, {255, 255, 255});
            drawText(c, row.label, right + 22, y, {{9, 50}, {12, 9}});
            if (!row.note.empty()) {
                drawText(c, row.note, right + 32 + textWidth(row.label, 50), y + 3, {{9, 38}, {12, 9}}, 1, std::array<int, 3>{44, 66, 88});
            }
        }
    }
    void drawHint(HudCanvas& c) {
        if (!session_ || !session_->hasHint) {
            return;
        }
        const double y = 304;
        const double width = 500;
        c.fillRect(56, y + 5, 4, 4, {0xe0, 0x6a, 0x1c});
        drawText(c, data_["strings"].value("hints_and_tips", std::string()) + ": " + session_->hintTitle, 66, y, {{9, 50}, {12, 9}});
        // body.replace(/\\\\/g, ' ').split(/\s+/)
        std::string body = session_->hintBody;
        for (size_t at = body.find("\\\\"); at != std::string::npos; at = body.find("\\\\", at + 1)) {
            body.replace(at, 2, " ");
        }
        // split(/\s+/): a run of white space is one separator (a leading / trailing run leaves an empty word)
        std::vector<std::string> words;
        {
            std::string w;
            bool inRun = false;
            for (char ch : body) {
                if (std::isspace(uint8_t(ch))) {
                    if (!inRun) {
                        words.push_back(w);
                        w.clear();
                    }
                    inRun = true;
                    continue;
                }
                inRun = false;
                w += ch;
            }
            words.push_back(w);
        }
        std::string line;
        int n = 0;
        auto flush = [&] {
            if (n < 3) {
                drawText(c, line, 66, y + 16 + n * 13, {{9, 42}, {12, 9}}, 1, std::array<int, 3>{30, 44, 58});
            }
            n++;
            line.clear();
        };
        for (const auto& w : words) {
            const std::string next = line.empty() ? w : line + " " + w;
            if (!line.empty() && textWidth(next, 42) > width) {
                flush();
            }
            line = line.empty() ? w : line + " " + w;
        }
        if (!line.empty()) {
            flush();
        }
    }

    nlohmann::json data_;
    std::vector<Element> elements_;
    std::map<std::string, size_t> byName_;
    std::optional<Session> session_;
    int hintCounter_ = 0;
};

}  // namespace fe

}  // namespace ssx_native
