#pragma once
// The in-race HUD (docs/native.md section 7 "HUD"), 1:1 with the page:
//   - per tick (web/game-tick.js simulate / present, web/ai-race.js endTick): the HUD pre-pass (the boost HUD and the slot bank read
//     before the tick), the race place's change and glow timers, the progress meter's inputs after the riders, the countdown's GO
//     (command 6) and the big message;
//   - per frame (main.js ui.draw's state, web/ui.js draw's 'game' branch): the place, the race clock and speed, the progress meter,
//     the boost orb, gauge and letters, the switch icon, the countdown, the trick slots, the finish banner and the reset fade, drawn
//     on the 640 x 448 UI canvas (hud_canvas.hpp) the stage puts over the 3D frame.
// HUD options: Full, speed in mph (the FE Options defaults). The key caps replace the button icons while the keyboard is the device.
// The EA RADIO BIG now-playing popup (now_playing.hpp, web/now-playing.js) over the rest, as the page's microtask draws it.
// Not drawn (yet): the career / free-ride / freestyle / online parts (no Single Event race draws them).
#include "core_api.hpp"
#include "hud_canvas.hpp"
#include "hud_widgets.hpp"
#include "now_playing.hpp"
#include "replay_session.hpp"
#include "trick_hud.hpp"
#include <cstring>
#include <fstream>

namespace ssx_native {

class RaceHud {
public:
    // the keyboard is the device (web/input-glyphs.js inputDevice): the hints show key caps
    bool keyboard = true;
    // FE Options HUD level (0 Full, 1 Minimal, 2 None) and speed units (0 mph, 1 km/h)
    int hudLevel = 0;
    bool kmh = false;

    void load(Gpu& gpu, const session::Verifier& v) {
        canvas.init(gpu);
        const std::string ui = v.assets.path(v.assetPath("/assets/UI"));
        images_.canvas = &canvas;
        images_.dir = ui;
        auto readJson = [&](const std::string& name) { return nlohmann::json::parse(v.assets.text(v.assetPath("/assets/UI/" + name))); };
        const auto feRaw = readJson("FEFONT-glyphs.json");
        const auto hudRaw = readJson("HUDFONT-glyphs.json");
        text_.images = &images_;
        text_.fonts["FEFONT"] = hud::loadGlyphs(feRaw, false);
        text_.fonts["HUDFONT"] = hud::loadGlyphs(hudRaw, false);
        trick_.load(readJson("trick-hud.json"), hud::loadGlyphs(feRaw, true), hud::loadGlyphs(hudRaw, true));
        // ui.fonts as glyphOf reads them (signed dx, dy, advance)
        feSigned_ = hud::loadGlyphs(feRaw, true);
        hudSigned_ = hud::loadGlyphs(hudRaw, true);
        nowPlaying.fe = &feSigned_;
        nowPlaying.hudFont = &hudSigned_;
        nowPlaying.images = &images_;
        renderer_.images = &images_;
        renderer_.hud = &trick_;
        gauge_.load(readJson("boost-gauge.json"), images_);
        // the course's progress meter (main.js loadProgressMeter: course.root + 'progress-meter.json'; none without it)
        const std::string root = session::str(session::at(v.course, "root"), "/assets/" + v.code + "/");
        const std::string meterPath = v.assetPath(root + "progress-meter.json");
        hasMeter_ = v.assets.exists(meterPath) && meter_.load(nlohmann::json::parse(v.assets.text(meterPath)));
    }

    // main.js startRun (a live run): the meter's entries reset, the Uber hint's bit from the visual RNG (0x1E9AD0)
    void begin(const session::Verifier& v) {
        if (hasMeter_) {
            meter_.reset();
        }
        trick_.resetUberHint(visualRandPeek() & 1);
        placeTimer_ = -1;
        lastPlace_ = -1;
        glowTimer_ = -1;
        goPhase_ = -1;
        (void)v;
    }

    // web/game-tick.js simulate's rec: the pre-pass reads before the tick's core calls
    void beforeTick() {
        std::memcpy(prepassBoost_, boost_hud_info(), sizeof prepassBoost_);
        prepassSlots_ = readSlots(nullptr);
    }
    // after the riders' passes (Verifier::afterMid): the progress meter's inputs
    void afterMid(session::Verifier& v) {
        if (!hasMeter_) {
            return;
        }
        meterRiders_.clear();
        meterRiders_.push_back(readMeterRider(true));
        if (v.aiActive) {
            for (const auto& n : v.npcs) {
                if (size_t(n.slot) >= meterRiders_.size()) {
                    meterRiders_.resize(size_t(n.slot) + 1);
                }
                v.inNpc(n, [&] { meterRiders_[size_t(n.slot)] = readMeterRider(false); });
            }
        }
        meterRead_ = true;
    }
    // web/game-tick.js present (and ai-race.js endTick's place poll, the same tick)
    void afterTick(const session::Verifier& v) {
        if (v.aiActive) {
            // 0x1EA930 polls +0xEC every tick
            const int place = int(v.worldState[360]);
            if (placeTimer_ >= 0) {
                placeTimer_ = placeTimer_ >= 1 ? -1 : std::min(1.0, placeTimer_ + (place == 0 ? 0.026456889 : 0.14841716));
            }
            if (lastPlace_ >= 0 && place != lastPlace_) {
                placeTimer_ = 0;
            }
            lastPlace_ = place;
            glowTimer_ = hud::placeGlowStep(glowTimer_, place);
        }
        prepassBoostShown_ = true;
        std::memcpy(shownBoost_, prepassBoost_, sizeof shownBoost_);
        // the countdown's GO: HUD command 6 on the tick the race phase goes 4 -> 5
        const int phase = int(v.raceInfo[5]);
        if (phase == 5 && goPhase_ == 4) {
            trick_.command(6);
        }
        goPhase_ = phase;
        trick_.setPrepassSlots(prepassSlots_.has_value() ? std::optional<hud::Slots>(prepassSlots_->slots) : std::nullopt);
        if (meterRead_) {
            meter_.tick(meterRiders_);
            meterRead_ = false;
        }
    }

    // one frame of the UI canvas (main.js ui.draw state + web/ui.js draw 'game'); nowSeconds: the frame's clock (performance.now)
    void draw(const session::Verifier& v, double nowSeconds) {
        canvas.begin();
        // web/audio-menu.js drawHud: the popup's update, its draw after the rest
        nowPlaying.frame(nowSeconds);
        auto& c = canvas;
        const float* raceInfo = v.raceInfo;
        const float* result = race_result_info();
        const float* boostHud = boost_hud_info();
        const float* boostPrepass = prepassBoostShown_ ? shownBoost_ : boostHud;
        const bool finished = v.finished;
        const bool timedOut = race_timed_out() != 0;
        const bool message = finished;
        const double raceTicks = result[4];
        const double finishTicks = result[1];
        const double finishElapsed = result[3];
        const int countdown = raceInfo[5] == 4 ? int(std::ceil(raceInfo[6] / 60.0)) : 0;
        const bool stanceRegular = rider_stance_info()[2] != 0;
        const double speed = v.state[7];
        std::optional<SlotRead> slots;
        if (v.running) {
            slots = readSlots(nullptr);
        }
        // pv finishHudHide
        const bool finishHide = message;
        const uint32_t baseFlags = trick_.flags;
        // the in-race place (raceInfo[5] == 5, not finished, computer riders)
        if (v.aiActive && raceInfo[5] == 5 && !finished && lastPlace_ >= 0 && hudLevel < 2 && !finishHide) {
            hud::PlaceHud ph;
            ph.place = lastPlace_;
            ph.total = int(v.npcs.size()) + 1;
            ph.timer = placeTimer_;
            ph.glow = glowTimer_;
            hud::drawRacePlace(c, images_, ph);
        }
        std::vector<hud::Draw> raceHud;
        if (hudLevel < 2 && !finishHide) {
            const double ticks = raceTicks;
            const double t = std::trunc(hud::fround(hud::fround(ticks) * hud::fround(0.01666666753590107)));
            trick_.raceClock(raceHud, int64_t(std::trunc(t / 3600)), int64_t(std::trunc(std::fmod(t, 3600) / 60)), int64_t(std::fmod(t, 60)));
        }
        if (hudLevel < 1 && !finishHide) {
            trick_.speed(raceHud, speed * 100, kmh);
        }
        if (!raceHud.empty()) {
            renderer_.render(c, raceHud);
        }
        if (hasMeter_ && hudLevel < 1 && !finishHide) {
            meter_.draw(c, images_, trick_, renderer_);
        }
        if (hudLevel < 2 && !finishHide) {
            gauge_.drawOrb(c, boostPrepass[8], boostPrepass[12]);
            gauge_.draw(c, boostHud[0], boostHud[1], boostPrepass[8], boostPrepass[12]);
            gauge_.drawLetters(c, text_, images_, boostHud[2], boostHud[3], boostHud[4] != 0, boostHud[5], boostHud[6]);
        }
        if (hudLevel < 2 && !finishHide) {
            const uint32_t flags = hudLevel == 1 ? minimalHudFlags(trick_.flags) : trick_.flags;
            if (flags & 0x10000000) {
                std::vector<hud::Draw> l;
                trick_.switchIcon(l, stanceRegular);
                renderer_.render(c, l);
            }
        }
        if (countdown >= 1 && countdown <= 3) {
            static const double glyph[3][4] = {{0, 84, 24, 42}, {26, 84, 48, 42}, {76, 84, 46, 42}};
            const auto& g = glyph[countdown - 1];
            c.drawImage(images_.png("OV_1-3"), g[0], g[1], g[2], g[3], 320 - g[2] / 2, 112, g[2], g[3]);
        }
        if (hudLevel < 2 && slots) {
            uint32_t slotFlags = hudLevel == 1 ? minimalHudFlags(baseFlags) : baseFlags;
            if (finishHide) {
                slotFlags = slotFlags & 0x100000;
            }
            hud::FrameState fs;
            fs.flags = slotFlags;
            fs.finished = message || timedOut;
            if (keyboard) {
                fs.keys = [](const std::string& b) { return raceKey(b); };
            }
            renderer_.render(c, trick_.frame(slots->slots, fs));
        }
        if (message) {
            std::vector<hud::Draw> list;
            if (timedOut) {
                if (finishElapsed < 3) {
                    trick_.finishBanner(list, true);
                }
            } else {
                const double t = std::trunc(hud::fround(hud::fround(finishTicks != 0 ? finishTicks : raceTicks) * hud::fround(0.01666666753590107)));
                const std::string time = hud::pad2(int64_t(std::trunc(t / 3600))) + ":" + hud::pad2(int64_t(std::trunc(std::fmod(t, 3600) / 60))) + ":" +
                                         hud::pad2(int64_t(std::fmod(t, 60)));
                trick_.finishBanner(list, false, &time);
            }
            renderer_.render(c, list);
        }
        // ui.drawResetFade(reset_info()[8])
        const double fade = reset_info()[8];
        if (fade > 0) {
            c.save();
            c.setGlobalAlpha(std::min(fade, 1.0));
            c.fillRect(0, 0, 640, 448, {255, 255, 255});
            c.restore();
        }
        nowPlaying.draw(c);
    }

    HudCanvas canvas;
    hud::NowPlayingHud nowPlaying;

private:
    struct SlotRead {
        hud::Slots slots;
        double total = 0;
    };
    // web/game-tick.js readTrickHudSlots
    static std::optional<SlotRead> readSlots(const void*) {
        const float* raw = score_hud_slots();
        if (!raw) {
            return std::nullopt;
        }
        SlotRead r;
        r.slots.assign(44, std::nullopt);
        for (int k = 0; k < 44; ++k) {
            const float* o = raw + 6 * k;
            if (o[0] == 0x34) {
                continue;
            }
            hud::Slot s;
            s.type = o[0];
            s.maximum = o[1];
            s.value = o[2];
            s.arg = o[3];
            s.field10 = o[4];
            s.points = o[5];
            if (s.type == 0 || s.type == 0xe) {
                const char* p = score_hud_text(k);
                std::string t;
                for (int i = 0; i < 0x80 && p && p[i]; ++i) {
                    t += char(uint8_t(p[i]));
                }
                // String.fromCharCode of each byte: Latin-1 into the UTF-8 the glyph tables are keyed in
                std::string u;
                for (unsigned char ch : t) {
                    u += hud::utf8(char32_t(ch));
                }
                s.text = u;
            } else {
                s.text = hud::jsNumberString(s.points);
            }
            r.slots[size_t(k)] = s;
        }
        r.total = raw[44 * 6];
        return r;
    }
    static hud::ProgressMeter::Rider readMeterRider(bool human) {
        hud::ProgressMeter::Rider r;
        r.remaining = race_progress_info()[0];
        const float* w = rider_world_state();
        r.x = w[0];
        r.y = w[1];
        r.human = human;
        r.hasPlacements = true;
        r.placements = reset_info()[2];
        return r;
    }
    // main.js visualRandPeek
    static uint32_t visualRandPeek() {
        const uint32_t* w = visual_rng_words();
        uint32_t v = w[5] + w[4];
        uint32_t c = v < w[5] || v < w[4] ? 1 : 0;
        for (int k = 3; k >= 1; --k) {
            const uint32_t n = v + w[k] + c;
            c = n < w[k] ? 1 : 0;
            v = n;
        }
        return v + w[0] + c;
    }
    // web/input-glyphs.js keyFor(button, {context: 'race', mode: 'Classic'})
    static std::string raceKey(const std::string& b) {
        if (b == "l1") {
            return "Q";
        }
        if (b == "r1") {
            return "E";
        }
        if (b == "l2") {
            return "Z";
        }
        if (b == "r2") {
            return "X";
        }
        if (b == "square") {
            return "Shift";
        }
        if (b == "cross") {
            return "Space";
        }
        if (b == "triangle") {
            return "Y";
        }
        if (b == "circle") {
            return "C";
        }
        if (b == "start") {
            return "Esc";
        }
        return "";
    }
    // web/fe-screens.js minimalHudFlags (HUD level 1)
    static uint32_t minimalHudFlags(uint32_t flags) {
        return flags & ~0x0510c040u;
    }

    hud::HudImages images_;
    hud::GlyphTable feSigned_;
    hud::GlyphTable hudSigned_;
    hud::UiText text_;
    hud::TrickHud trick_;
    hud::TrickHudRenderer renderer_;
    hud::BoostGauge gauge_;
    hud::ProgressMeter meter_;
    bool hasMeter_ = false;
    float prepassBoost_[13] = {};
    float shownBoost_[13] = {};
    bool prepassBoostShown_ = false;
    std::optional<SlotRead> prepassSlots_;
    std::vector<hud::ProgressMeter::Rider> meterRiders_;
    bool meterRead_ = false;
    double placeTimer_ = -1;
    int lastPlace_ = -1;
    double glowTimer_ = -1;
    int goPhase_ = -1;
};

}  // namespace ssx_native
