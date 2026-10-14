#pragma once
// Gameplay sound dispatch (web/sfx-game.js, 1:1; docs/audio-logic.md 3.6, 5.2-5.5, 9.4): the SSXAudioSystem 29xxxx dispatchers
// fed by each rider core's audio event queue (web/audio_events.inc) and 24-float telemetry: board loops, take-off / landing,
// crash sounds, grunts, boost, the HUD / score sounds, the countdown and GO, overtakes, rider pairs, the big-air music duck to the
// loops, the crowd anticipation.
#include "audio_crowd.hpp"
#include "audio_engine.hpp"
#include "audio_sfx.hpp"
#include "audio_speech.hpp"
#include "audio_world.hpp"
#include <cstring>
#include <functional>

// the core's audio queue and telemetry (web/audio_events.inc; the rider context current when called)
extern "C" {
float* audio_events();
void audio_events_clear();
float* audio_telemetry();
}

namespace ssx_native {

namespace audio {

namespace ae {
constexpr int TAKEOFF = 1, LANDING = 2, CRASH = 3, CRASH_LOOP = 4, CRASH_AIR = 5, CRASH_GRUNT = 6, CRASH_EXIT = 7, CRASH_METER = 8, BOOST = 9,
              FILL_RESET = 10, GRAB = 11, UBER_DENIED = 12, HANDPLANT = 13, COMBO = 14, NAMED_TRICK = 15, TRICK_SPEECH = 16, WHOOH = 17, SOFT = 18,
              OBSTACLE = 19, CONTACT = 20, RESET = 21, PICKUP = 22, ANIM_EVENT = 23, PAIR = 24, TIME_UP = 25, FINISH = 26, OVERTAKE = 27,
              SCRIPT_SOUND = 28, ARCADE_BONUS = 29;
}

inline int gruntIndex(const std::string& name) {
    const int id = charId(name);
    return id >= 0 ? id : 7;
}

// the director's hooks into the music (game-audio.js musicAdapter)
struct MusicAdapter {
    std::function<bool()> duckToLoops;
    std::function<void(double)> streamLevel;
    std::function<void(double)> loopsLevel;
    std::function<bool()> loopsStart;
    std::function<void()> loopsStop;
    std::function<bool()> loopsRequested;
    std::function<void(int)> event;
    std::function<void()> resetIntensity;
    std::function<void(int)> countdown;
};

// one rider of the tick (game-audio.js gameTick list)
struct TickRider {
    const void* key = nullptr;
    std::string character;
    int slot = 0;
    bool human = false;
    bool finished = false;
    int place = 0;
    // runs fn with this rider's core current (its audio_events / audio_telemetry)
    std::function<void(const std::function<void()>&)> in;
};

class SfxGame {
public:
    SfxGame(AudioEngine& engine, Sfx& sfx, Speech& speech, Crowd& crowd, WorldAudio& world, MusicAdapter music, Random& random,
            std::function<std::optional<int>(int, int)> animationEvents)
        : engine_(engine), sfx_(sfx), speech_(speech), crowd_(crowd), world_(world), music_(std::move(music)), random_(random),
          clipEvents_(std::move(animationEvents)) {}

    struct Globals {
        int courseIndex = 0;
        bool bigAirEvent = false, halfPipeEvent = false, backcountry = false;
        int raceKind = 0;
        std::function<bool()> zoneActive;
        std::function<std::optional<Vec>(uint32_t)> instancePosition;
        std::function<std::optional<int>(int, int)> relation;
        std::function<std::optional<int>(int, int)> pairRelation;
        bool running = false, paused = false, replay = false, rollingGo = false;
        int countdown = 0;
        std::optional<double> goPhase;
        bool timedOut = false, finished = false;
        int sinceGo = -1;
        struct Points {
            int last = 0;
            std::shared_ptr<Voice> voice;
        } points;
        struct Hud {
            std::optional<std::vector<double>> prev;
            std::shared_ptr<Voice> uberVoice;
        } hud;
        struct Fill {
            int prev = 0;
            int bend = 0x1000;
            int tricky = 0;
        } fill;
        std::optional<double> uberTier;
        std::optional<std::vector<float>> frameSlots;
        struct Air {
            int flag = 0;
            double T = 0, start = 0, end = 0;
            std::shared_ptr<Voice> voice;
            double gain = 0;
            bool loopsOn = false;
        } air;
    } G;

    struct Rider {
        const void* key = nullptr;
        int slot = 0;
        std::string character;
        int id = -1;
        bool human = false;
        std::array<float, 24> t{};
        std::array<float, 24> prevT{};
        bool hasT = false, prevValid = false;
        std::shared_ptr<Voice> loopA, loopB, loopC;
        int loopClass = -1;
        double smoothB = 0, smoothC = 0;
        std::shared_ptr<Voice> crashA, crashB;
        int boost = 0;
        std::shared_ptr<Voice> boostVoice, landVoice, takeoffVoice, gruntVoice, obstacleVoice, bumpVoice, attackVoice;
        bool retry = false;
        int grunt = 0;
        double meterPrev = 1;
        bool finished = false;
        int place = 0;
        std::function<void(const std::function<void()>&)> in;
    };

    // 286E20 -> 2929D8 -> 292A50
    void start(int courseIndex, bool bigAir, bool halfPipe, bool backcountry, int raceKind) {
        stop();
        G.courseIndex = courseIndex;
        G.bigAirEvent = bigAir;
        G.halfPipeEvent = halfPipe;
        G.backcountry = backcountry;
        G.raceKind = raceKind;
        G.countdown = 0;
        G.goPhase.reset();
        G.timedOut = false;
        G.finished = false;
        G.sinceGo = -1;
        G.points = {};
        G.hud = {};
        G.fill = {};
        G.uberTier.reset();
        G.frameSlots.reset();
        G.air = {};
    }
    void stop() {
        for (auto& [k, r] : riders_) {
            stopLoops(r);
            stopCrashLoops(r);
            if (r.boostVoice) {
                r.boostVoice->stop(0.25);
            }
        }
        riders_.clear();
        if (G.air.voice) {
            G.air.voice->stop(0.25);
        }
        Sfx::StopFilter f;
        f.tag = "boost";
        sfx_.stopAll(f);
    }
    // one 60 Hz tick after the cores stepped
    void tick(const std::vector<TickRider>& list, const float* raceInfo, bool running, bool paused, double pending, const std::optional<std::vector<float>>& hudSlots,
              bool finished, bool timedOut) {
        G.running = running;
        G.paused = paused;
        if (G.sinceGo >= 0) {
            G.sinceGo++;
        }
        for (const auto& x : list) {
            Rider& r = rider(x);
            r.finished = x.finished;
            r.place = x.place;
            drain(r);
        }
        Rider* h = human();
        if (!h) {
            return;
        }
        for (auto& [k, r] : riders_) {
            if (r.retry && (r.t[7] == 1 || r.t[7] == 3)) {
                takeoffFollowup(r, r.t[8]);
            }
            if (r.retry && r.t[4] != 1) {
                r.retry = false;
            }
            updateLoops(r);
        }
        // countdown (29C420) and GO (29C7B0)
        const std::optional<double> phase = raceInfo ? std::optional<double>(raceInfo[5]) : std::nullopt;
        const int digit = phase && *phase == 4 ? int(std::ceil(raceInfo[6] / 60.0)) : 0;
        auto beep = [&](int d) {
            if (G.replay) {
                return;
            }
            PlayOptions o;
            o.slot = slot::MAIN;
            o.sound = 0x4e;
            o.bus = "UI";
            o.volume = 127;
            sfx_.play(o);
            if (d > 0 && music_.countdown) {
                music_.countdown(d);
            }
        };
        if (phase && *phase == 4 && G.goPhase != 4.0) {
            beep(3);
        } else if (phase && *phase == 4 && raceInfo[6] > 0 && std::fmod(raceInfo[6] + 1, 60) == 0) {
            beep(int((raceInfo[6] + 1) / 60));
        } else if (phase && *phase == 5 && G.goPhase == 4.0) {
            beep(0);
        }
        G.countdown = digit;
        const bool rolling = G.rollingGo ? (phase == 5.0 && G.goPhase != 4.0 && G.goPhase != 5.0) : (G.goPhase == 3.0 && phase == 5.0);
        if (rolling && G.backcountry) {
            G.sinceGo = 0;
        }
        if (G.goPhase == 4.0 && phase == 5.0) {
            G.sinceGo = 0;
            if (!G.replay) {
                arcadeSound(0x5e, 0, -1);
            }
            PlayOptions o;
            o.slot = slot::MAIN;
            o.sound = 0x62;
            o.bus = "UI";
            o.volume = 127;
            sfx_.play(o);
        }
        G.goPhase = phase;
        if (timedOut && !G.timedOut) {
            speech_.arcade(ev::ARCADE_PROMPTS, 2);
        }
        G.timedOut = timedOut;
        if (h->prevValid) {
            frameSounds(*h, G.frameSlots, h->prevT[22]);
        }
        hudSounds(*h, hudSlots, pending, h->t[22], h->t[23]);
        if (h->prevValid) {
            const double tier = h->prevT[23];
            if (G.uberTier && tier == 10 && *G.uberTier != 10 && running && !paused) {
                speech_.arcade(ev::ARCADE_UBER, 2);
            }
            G.uberTier = tier;
        }
        G.frameSlots = hudSlots;
        bigAir(h->prevT);
        crowd_.update([h, pending] {
            if (h->t[4] != 1) {
                return 3;
            }
            const double t = h->t[7] == 1 || h->t[7] == 3 ? double(h->t[8]) - h->t[10] : 4;
            if (t > 3) {
                return 3;
            }
            const double p = pending / 10000;
            return p <= 0 ? 3 : p < 0.25 ? 0 : p < 0.5 ? 1 : 2;
        });
        if (finished && !G.finished) {
            crowd_.trick(2);
            stopCrashLoops(*h);
            if (G.hud.uberVoice) {
                G.hud.uberVoice->stop(0.25);
            }
            stopLoops(*h);
        }
        G.finished = finished;
    }
    // 29ED18
    void pauseStops() {
        Rider* h = human();
        if (!h) {
            return;
        }
        stopCrashLoops(*h);
        if (G.hud.uberVoice) {
            G.hud.uberVoice->stop(0.25);
        }
        G.hud.uberVoice = nullptr;
        if (h->boost > 0) {
            if (h->boostVoice) {
                h->boostVoice->stop(0);
            }
            h->boostVoice = nullptr;
            h->boost = 0;
        }
    }
    void challengeStops() {
        if (Rider* h = human()) {
            stopCrashLoops(*h);
        }
        if (G.hud.uberVoice) {
            G.hud.uberVoice->stop(0.25);
        }
        G.hud.uberVoice = nullptr;
    }
    std::shared_ptr<Voice> challengeComplete() {
        return arcadeSound(0x6d, 0, -1);
    }
    const Rider* humanRider() const {
        for (const auto& [k, r] : riders_) {
            if (r.human) {
                return &r;
            }
        }
        return nullptr;
    }

private:
    Rider* human() {
        for (auto& [k, r] : riders_) {
            if (r.human) {
                return &r;
            }
        }
        return nullptr;
    }
    Rider& rider(const TickRider& x) {
        if (Rider* r = riders_.get(x.key)) {
            r->in = x.in;
            return *r;
        }
        Rider r;
        r.key = x.key;
        r.slot = x.slot;
        r.character = x.character;
        r.id = charId(x.character);
        r.human = x.human;
        r.in = x.in;
        return riders_.set(x.key, r);
    }
    Position pos(const Rider& r) const {
        return position(r.t[12], r.t[13], r.t[14]);
    }
    int rand15() {
        return int(std::floor(random_() * 0x8000));
    }
    // 288B40: the 100 m gate
    bool inRange(const Rider& r) const {
        const std::array<double, 3> p = {r.t[12], r.t[13], r.t[14]};
        const Spatial s = sfx_.spatial(&p, 100);
        return !s.hasD || s.d < 100;
    }
    SpeechRider speechRider(const Rider& r) const {
        return {r.id, r.human, r.human, r.t[4] != 2 && !r.finished, inRange(r)};
    }
    int peak() const {
        return peakMask(G.courseIndex);
    }
    std::shared_ptr<Voice> play(PlayOptions o) {
        return sfx_.play(std::move(o));
    }
    PlayOptions opts(int slotNo, int sound, const std::string& bus, double volume) const {
        PlayOptions o;
        o.slot = slotNo;
        o.sound = sound;
        o.bus = bus;
        o.volume = volume;
        return o;
    }
    // 288AE0 blocks the arcade feedback in a replay
    std::shared_ptr<Voice> arcadeSound(int sound, double delayMs = 0, int bend = -1) {
        if (G.replay) {
            return nullptr;
        }
        PlayOptions o = opts(slot::MAIN, sound, "ARCADESFX", 127);
        o.delayMs = delayMs;
        if (bend >= 0) {
            o.bend = bend;
        }
        return play(o);
    }
    // ---- board loops ----
    int riderClass(const Rider& r) const {
        return surfaceClass(r.t[4] == 4 ? r.t[6] : r.t[5]);
    }
    static int loopBank(int c) {
        return c <= 3 ? slot::SPUBOARD : slot::BOARD;
    }
    std::shared_ptr<Voice> boardVoice(const Rider& r, int slotNo, int sound) {
        PlayOptions o = opts(slotNo, sound, "BOARD", 0);
        o.position = pos(r);
        o.tag = "board";
        o.owner = r.key;
        return play(o);
    }
    void startLoops(Rider& r) {
        const int c = r.loopClass = riderClass(r);
        if (r.human) {
            r.loopA = boardVoice(r, slot::SPUBOARD, 0);
        }
        r.loopB = boardVoice(r, loopBank(c), c + (r.human ? 4 : 6));
        if (r.human) {
            r.loopC = boardVoice(r, loopBank(c), c + 3);
        }
    }
    static void stopLoops(Rider& r) {
        for (auto* v : {&r.loopA, &r.loopB, &r.loopC}) {
            if (*v) {
                (*v)->stop(0.25);
            }
            *v = nullptr;
        }
    }
    bool muted(const Rider& r) const {
        return r.t[4] == 2 || r.finished || G.paused;
    }
    void updateLoops(Rider& r) {
        if (!r.loopB && !r.loopA) {
            auto* b1 = sfx_.bankOf(slot::BOARD);
            auto* b2 = sfx_.bankOf(slot::SPUBOARD);
            if (b1 && b1->ready() && b2 && b2->ready()) {
                startLoops(r);
            }
            return;
        }
        const double motion = r.t[4], spd = r.t[0], turn = r.t[1], brake = r.t[2], comp = r.t[3];
        int c = riderClass(r);
        const int fam = boardFamily(c, motion);
        if ((motion == 1 || motion == 5) && fam == 2) {
            c = 9;
        }
        if (c != r.loopClass && c != 9 && r.loopClass != -1) {
            if (r.loopB) {
                r.loopB->stop(0.25);
            }
            if (r.loopC) {
                r.loopC->stop(0.25);
            }
            r.loopClass = c;
            r.loopB = boardVoice(r, loopBank(c), c + (r.human ? 4 : 6));
            if (r.human) {
                r.loopC = boardVoice(r, loopBank(c), c + 3);
            }
        } else if (c == 9) {
            r.loopClass = 9;
        }
        const bool m = muted(r);
        if (r.loopA) {
            const double vol = !m && motion == 0 ? std::trunc(curve(curves::loopA_vol_482AF8, spd)) : 0;
            r.loopA->setVolume(vol);
            r.loopA->setBend(std::trunc(curve(curves::loopA_pitch_482B20, spd) * 4096));
        }
        if (r.loopB) {
            double x = comp * (1 - (brake + turn));
            if (spd < 0.1) {
                x *= spd * 10;
            }
            if (std::abs(r.smoothB - x) > 0.1) {
                x = r.smoothB - 0.25 * (r.smoothB - x);
            }
            r.smoothB = x;
            const Curve& V = fam == 0 ? curves::glide0_vol_482B78 : fam == 1 ? curves::glide1_vol_482BC8 : curves::glide2_vol_482C18;
            const Curve& P = fam == 0 ? curves::glide0_pitch_482BA0 : fam == 1 ? curves::glide1_pitch_482BF0 : curves::glide2_pitch_482C40;
            r.loopB->setVolume(m ? 0 : std::trunc(curve(V, x)));
            r.loopB->setBend(std::trunc(curve(P, x) * 4096));
        }
        if (r.loopC) {
            double y = brake + turn;
            const double d = y - r.smoothC;
            if (std::abs(d) > 0.1) {
                y = r.smoothC + (d < 0 ? 0.5 : 0.125) * d;
            }
            r.smoothC = y;
            double vol, bend;
            if (fam == 0) {
                vol = curve(curves::carve0_vol_482C68, y);
                bend = curve(curves::carve0_pitch_482C90, comp);
                if (spd < 0.1) {
                    vol *= spd * 10;
                }
            } else if (fam == 1) {
                const double yy = spd < 0.1 ? y * spd * 10 : y;
                vol = curve(curves::carve1_vol_482CB8, yy);
                bend = curve(curves::carve1_pitch_482CE0, comp);
            } else {
                vol = 127 * (spd < 0.1 ? spd * 10 : 1);
                bend = curve(curves::carve2_pitch_482D08, comp);
            }
            r.loopC->setVolume(m ? 0 : std::trunc(vol));
            r.loopC->setBend(std::trunc(bend * 4096));
        }
    }
    // ---- grunts (29F660) ----
    void grunt(Rider& r, int kind) {
        if (!(engine_.busGain("CHARACTER") > 0) || r.finished) {
            return;
        }
        const double speed = std::hypot(r.t[19], r.t[20], r.t[21]);
        const double vol = std::min(std::max(std::trunc(curve(curves::grunt_445910, speed)), 0.0), 127.0);
        int slotNo, sound;
        if (r.human) {
            slotNo = slot::GRUNT;
            if (kind == 1) {
                sound = int(std::floor((rand15() * 3.0) / 0x7fff));
            } else if (kind == 0) {
                sound = int(std::floor((rand15() * 3.0) / 0x7fff)) + 3;
            } else {
                r.grunt = (r.grunt + 1) % 6;
                sound = (r.grunt + 5) % 6;
            }
        } else {
            slotNo = slot::GRUNT_AI;
            sound = gruntIndex(r.character) * 2 + (kind == 1 ? 1 : 0);
        }
        if (r.gruntVoice && r.gruntVoice->playing()) {
            return;
        }
        PlayOptions o = opts(slotNo, sound, "CHARACTER", vol);
        o.speaker = r.id;
        o.position = pos(r);
        o.tag = "grunt";
        o.owner = r.key;
        r.gruntVoice = play(o);
    }
    // ---- crash ----
    void crashSound(Rider& r, std::optional<double> surface = std::nullopt) {
        if (!inRange(r)) {
            return;
        }
        const double s = surface ? *surface : r.t[5];
        PlayOptions o;
        if (s == 2 || s == 3) {
            o = opts(slot::LAND, 9, "UI", 127);
        } else {
            o = opts(slot::MAIN, 0x30 + int(std::floor(rand15() * 3.0 / 0x8000)), "UI", 127);
        }
        o.position = pos(r);
        o.tag = "crash";
        o.owner = r.key;
        play(o);
    }
    void crashLoop(Rider& r, int which) {
        auto& v = which == 0x33 ? r.crashA : r.crashB;
        if (v && v->playing()) {
            return;
        }
        PlayOptions o = opts(slot::MAIN, which, "UI", 127);
        o.position = pos(r);
        o.tag = "crash-loop";
        o.owner = r.key;
        v = play(o);
    }
    static void stopCrashLoops(Rider& r) {
        if (r.crashA) {
            r.crashA->stop(0.75);
        }
        if (r.crashB) {
            r.crashB->stop(0.75);
        }
        r.crashA = r.crashB = nullptr;
    }
    // ---- boost ----
    void boost(Rider& r, int kind, bool pressed) {
        if (!r.human) {
            return;
        }
        if (kind == 1) {
            if (r.boost == 0) {
                PlayOptions o = opts(slot::MAIN, 0x7a, "ARCADESFX", 127);
                o.tag = "boost";
                r.boostVoice = play(o);
            }
            if (pressed) {
                play(opts(slot::MAIN, 0x4a, "ARCADESFX", 127));
            }
            r.boost++;
        } else if (kind == 2) {
            play(opts(slot::MAIN, 0x6c, "ARCADESFX", 127));
        } else if (kind == 3) {
            if (r.boost > 0 && --r.boost == 0) {
                if (r.boostVoice) {
                    r.boostVoice->stop(0.25);
                }
                r.boostVoice = nullptr;
            }
        }
    }
    // ---- take-off / landing ----
    void takeoff(Rider& r, double speed, bool predValid, double T) {
        if (r.human) {
            if (r.takeoffVoice) {
                r.takeoffVoice->stop(0);
            }
            const int c = riderClass(r);
            PlayOptions o = opts(slot::LAND, c * 8 + 2, "BOARD", std::min(std::max(std::trunc(speed * 127), 64.0), 127.0));
            o.position = pos(r);
            o.tag = "land";
            o.owner = r.key;
            r.takeoffVoice = play(o);
        }
        if (predValid) {
            takeoffFollowup(r, T);
        } else {
            r.retry = true;
        }
    }
    void takeoffFollowup(Rider& r, double T) {
        r.retry = false;
        if (r.human && T > 2) {
            crowd_.arm();
        }
        if (T >= 4 && !G.backcountry) {
            speech_.bigAir(speechRider(r), peak());
        }
    }
    void landing(Rider& r, double value, double impact) {
        if (r.human) {
            if (value <= 0) {
                crowd_.cancel();
            } else {
                crowd_.trick(value < 0.25 ? 0 : value < 0.65 ? 1 : 2);
            }
        }
        if (!inRange(r)) {
            return;
        }
        if (r.landVoice) {
            r.landVoice->stop(0);
        }
        const int c = riderClass(r);
        PlayOptions o = opts(slot::LAND, c * 8 + (r.human ? 1 : 5), "BOARD", std::min(std::max(std::trunc(curve(curves::landing_vol_482D30, std::abs(impact))), 0.0), 127.0));
        o.position = pos(r);
        o.tag = "land";
        o.owner = r.key;
        r.landVoice = play(o);
    }
    // ---- the event dispatch ----
    void dispatch(Rider& r, int type, double a, double b, double c, double d) {
        switch (type) {
        case ae::TAKEOFF:
            takeoff(r, a, b > 0, c);
            break;
        case ae::LANDING:
            r.retry = false;
            landing(r, a, b);
            break;
        case ae::CRASH:
            crashSound(r, b);
            grunt(r, 0);
            if (r.human) {
                const double f = std::abs(a);
                if (f <= 0) {
                    crowd_.cancel();
                } else {
                    crowd_.fall(f < 0.25 ? 0 : f < 0.65 ? 1 : 2);
                }
                if (music_.resetIntensity) {
                    music_.resetIntensity();
                }
            }
            break;
        case ae::CRASH_LOOP:
            if (b == 0) {
                crashLoop(r, a == 0x19 || a == 0x1d ? 0x33 : 0x34);
            } else {
                crashLoop(r, c != 0 ? 0x33 : 0x34);
                crashSound(r);
            }
            break;
        case ae::CRASH_AIR:
            stopCrashLoops(r);
            break;
        case ae::CRASH_GRUNT:
            grunt(r, int(a));
            break;
        case ae::CRASH_EXIT:
            speech_.wipeout(speechRider(r), peak());
            break;
        case ae::CRASH_METER:
            if (!r.human) {
                break;
            }
            if (a != 0) {
                arcadeSound(0x60);
            } else {
                arcadeSound(0x5f, 0, 0x1000 + int(std::trunc(std::min(b, r.meterPrev) * 5)) * 0x50);
            }
            r.meterPrev = b;
            break;
        case ae::BOOST:
            if (!G.replay) {
                boost(r, int(a), b > 0);
            }
            break;
        case ae::FILL_RESET:
            if (r.human) {
                G.fill.bend = 0x1000;
            }
            break;
        case ae::GRAB:
            if (r.human) {
                arcadeSound(0x77);
            }
            break;
        case ae::UBER_DENIED:
            if (r.human) {
                arcadeSound(0x6c);
            }
            break;
        case ae::HANDPLANT:
            if (r.human) {
                arcadeSound(0x61);
            }
            break;
        case ae::COMBO:
            if (!r.human) {
                break;
            }
            arcadeSound(0x67);
            if (b < 4 && a + b >= 4 && a + b <= 8) {
                speech_.arcade(ev::ARCADE_UBER, 1);
            }
            break;
        case ae::NAMED_TRICK:
            if (r.human) {
                speech_.arcade(ev::ARCADE_UBER, 8);
            }
            break;
        case ae::TRICK_SPEECH:
            speech_.trick(speechRider(r), peak(), a > 0);
            break;
        case ae::WHOOH:
            speech_.whooh(speechRider(r));
            break;
        case ae::SOFT:
            grunt(r, 2);
            if (a != 0 && r.human) {
                speech_.objectHit(speechRider(r));
            }
            break;
        case ae::OBSTACLE: {
            if (!r.human || (r.obstacleVoice && r.obstacleVoice->playing())) {
                break;
            }
            PlayOptions o = opts(slot::MAIN, 0x36, "UI", 127);
            o.position = pos(r);
            o.tag = "obstacle";
            o.owner = r.key;
            r.obstacleVoice = play(o);
            break;
        }
        case ae::CONTACT: {
            const std::array<double, 3> p = {r.t[12], r.t[13], r.t[14]};
            char key[32];
            std::snprintf(key, sizeof key, "%.0f", a);
            world_.contact(key, int(b), c, p, inRange(r));
            break;
        }
        case ae::RESET:
            if (r.human && !G.replay) {
                play(opts(slot::MAIN, 0x7b, "UI", 127));
            }
            stopCrashLoops(r);
            if (r.boost) {
                r.boost = 1;
                boost(r, 3, false);
            }
            if (r.human && music_.resetIntensity) {
                music_.resetIntensity();
            }
            break;
        case ae::PICKUP: {
            if (!r.human) {
                break;
            }
            static const int sounds[6] = {0x74, 0x71, 0x75, 0x76, 0x70, 0x70};
            const int ai = int(a);
            arcadeSound(ai >= 0 && ai < 6 ? sounds[ai] : 0x70);
            if (ai == 0) {
                speech_.arcade(ev::ARCADE_POWER_UPS, 1);
            } else if (ai == 1) {
                speech_.arcade(ev::ARCADE_POWER_UPS, 2);
            }
            if (ai == 2) {
                speech_.arcade(ev::ARCADE_ICONS, b == 2 ? 1 : b == 3 ? 2 : b == 5 ? 4 : b == 10 ? 8 : 0);
            }
            break;
        }
        case ae::ANIM_EVENT: {
            if (!r.human || !clipEvents_) {
                break;
            }
            const auto id = clipEvents_(int(a), int(b));
            if (!id || !(*id & 0x8000)) {
                break;
            }
            const int code = *id & 0x7fff;
            const int sound = code == 0x50 ? 0x51 : code == 0x51 ? 0x50 : code == 0x52 ? 0x52 : -1;
            if (sound < 0) {
                break;
            }
            PlayOptions o = opts(slot::MAIN, sound, "UI", 127);
            o.position = pos(r);
            o.tag = "anim";
            o.owner = r.key;
            play(o);
            break;
        }
        case ae::PAIR:
            pair(int(a), int(c), int(d));
            break;
        case ae::ARCADE_BONUS:
            if (r.human) {
                speech_.arcade(ev::ARCADE_BONUS, 1);
            }
            break;
        case ae::OVERTAKE:
            overtake(int(a), int(b) >> 3, int(b) & 7, int(c), int(d));
            break;
        case ae::SCRIPT_SOUND:
            if (r.human && G.instancePosition) {
                if (auto p = G.instancePosition(uint32_t(c))) {
                    world_.script(int(a), int(b), uint32_t(c), *p, G.courseIndex);
                }
            }
            break;
        default:
            break;
        }
    }
    Rider* coreOfSlot(int s) {
        for (auto& [k, r] : riders_) {
            if (r.slot == s) {
                return &r;
            }
        }
        return nullptr;
    }
    // 107E70: rider pairs
    void pair(int kind, int targetSlot, int otherSlot) {
        Rider* B = coreOfSlot(targetSlot);
        Rider* A = coreOfSlot(otherSlot);
        if (!A || !B || !(A->human || B->human)) {
            return;
        }
        if (kind < 2) {
            if (inRange(*B) && !(B->bumpVoice && B->bumpVoice->playing())) {
                const double rel = std::hypot(double(B->t[19]) + A->t[19], double(B->t[20]) + A->t[20], double(B->t[21]) + A->t[21]);
                PlayOptions o = opts(slot::MAIN, 0x36, "UI", std::min(std::max(std::trunc(curve(curves::bump_4458C0, std::trunc(rel))), 0.0), 127.0));
                o.position = pos(*B);
                o.tag = "bump";
                o.owner = B->key;
                B->bumpVoice = play(o);
            }
        } else if (!(B->attackVoice && B->attackVoice->playing())) {
            PlayOptions o = opts(slot::MAIN, 0x5a, "UI", 127);
            o.position = pos(*B);
            o.tag = "attack";
            o.owner = B->key;
            B->attackVoice = play(o);
        }
        if (engine_.busGain("CHARACTER") > 0 && B->t[4] != 2 && A->t[4] != 2) {
            grunt(*B, 0);
            if (kind == 2 && speech_.free() && (inRange(*A) || inRange(*B))) {
                speech_.hit(speechRider(*A), speechRider(*B), relation(*A, *B, true));
            }
        }
    }
    int relation(const Rider& A, const Rider& B, bool pairSource) {
        if (riders_.size() == 2 || (A.human && B.human)) {
            return 2;
        }
        std::optional<int> v;
        if (pairSource && G.pairRelation) {
            v = G.pairRelation(A.slot, B.slot);
        } else if (!pairSource && G.relation) {
            v = G.relation(A.slot, B.slot);
        }
        return v ? *v : 2;
    }
    // 299E28: a rank change of `slot`
    void overtake(int slotNo, int oldPlace, int newPlace, int behind, int ahead) {
        Rider* r = coreOfSlot(slotNo);
        if (!r || !r->human || riders_.size() < 2 || !G.running || G.paused || G.raceKind != 0) {
            return;
        }
        if (newPlace == 0 && oldPlace != 0) {
            arcadeSound(0x6d);
        }
        if (!(G.sinceGo >= 600)) {
            return;
        }
        const bool gained = newPlace < oldPlace;
        Rider* other = coreOfSlot(gained ? behind : ahead);
        if (!other) {
            return;
        }
        Rider& A = gained ? *r : *other;
        Rider& Bq = gained ? *other : *r;
        if (A.t[4] == 2 || Bq.t[4] == 2 || !speech_.free()) {
            return;
        }
        const double heading = std::atan2(Bq.t[20], Bq.t[19]);
        const double d = (double(A.t[19]) - Bq.t[19]) * std::cos(heading) + (double(A.t[20]) - Bq.t[20]) * std::sin(heading);
        if (!(d > 670.56 && d < 1788.16) || !inRange(A) || inRange(A) != inRange(Bq)) {
            return;
        }
        speech_.pass(speechRider(A), speechRider(Bq), relation(A, Bq, false), d >= 1341.12 ? 2 : 1);
    }
    // ---- the HUD / score sounds (117FE0, 29AB40) ----
    void hudSounds(Rider& r, const std::optional<std::vector<float>>& slots, double pending, double superTime, double tier) {
        (void)r;
        if (std::trunc(pending) > 0) {
            const int m = int(std::trunc(pending)) % 100;
            if (m < G.points.last) {
                if (G.points.voice) {
                    G.points.voice->stop(0);
                }
                G.points.voice = arcadeSound(0x4d);
            }
            G.points.last = m;
        }
        if (!slots) {
            return;
        }
        auto type = [&](int k) { return double((*slots)[size_t(6 * k)]); };
        if (G.hud.prev) {
            const auto& prev = *G.hud.prev;
            if (prev[10] == 0x34 && type(10) != 0x34) {
                G.hud.uberVoice = arcadeSound(0x66);
            }
            if (prev[10] != 0x34 && type(10) == 0x34) {
                if (G.hud.uberVoice) {
                    G.hud.uberVoice->stop(0.25);
                }
                G.hud.uberVoice = nullptr;
            }
            if (prev[9] == 0x34 && type(9) != 0x34 && superTime > 0) {
                arcadeSound(0x6b, 400);
                speech_.arcade(ev::ARCADE_PROMPTS, tier == 10 ? 0x10 : tier < 5 ? 4 : 8);
            }
            if (prev[9] != 0x34 && type(9) == 0x34 && superTime == 0) {
                arcadeSound(0x69);
            }
        }
        std::vector<double> p(44);
        for (int k = 0; k < 44; ++k) {
            p[size_t(k)] = type(k);
        }
        G.hud.prev = p;
    }
    void frameSounds(Rider& r, const std::optional<std::vector<float>>& slots, double superTime) {
        if (!slots) {
            return;
        }
        const double max = (*slots)[6 * 6 + 1];
        const double value = (*slots)[6 * 6 + 2];
        const int n = int(std::floor(double(float(value / max)) * 10));
        if (G.fill.prev < n && n > 0) {
            arcadeSound(0x65, 150, G.fill.bend);
            G.fill.bend = (G.fill.bend + 0x100) & 0xffff;
        }
        G.fill.prev = n;
        const int t = int(std::floor(superTime * 2));
        if (t > 0 && t == G.fill.tricky - 1 && t < 13 && (!(t & 1) || t < 5) && !r.finished && G.running && !G.paused) {
            arcadeSound(0x68);
        }
        G.fill.tricky = t;
    }
    // ---- 28C8C8: big air ----
    void bigAir(const std::array<float, 24>& t) {
        auto& A = G.air;
        const double motion = t[4], status = t[7];
        double T = 0;
        if (motion == 1) {
            if (status == 1 || status == 3) {
                if (A.flag == 1) {
                    T = A.T;
                } else {
                    T = t[8];
                    A.start = 0;
                    A.T = T;
                }
            } else if (A.flag == 1) {
                T = A.T;
            }
        }
        const double musicVol = engine_.busGain("MUSIC");
        const int prev = A.flag;
        A.flag = 0;
        const bool s3 = T > 3.9995 && music_.duckToLoops && music_.duckToLoops() && !G.bigAirEvent && !G.halfPipeEvent && !(G.zoneActive && G.zoneActive());
        if (G.replay) {
            return;
        }
        if (T > 2) {
            A.flag = 1;
            double e = t[10];
            if (e < A.start) {
                e = A.start;
            }
            A.start = e;
            if (!prev) {
                A.end = e + std::max((double(t[9]) - t[10]) * 0.5, 0.5);
            }
            const double floorV = std::min(std::max((1 - T * 0.8425197 * 0.5) * 127, 20.0), 127.0);
            double stream;
            if (e < A.end) {
                const double p = e / A.end;
                const int v = int(int32_t(uint32_t(int32_t(std::trunc(std::min(std::max(127 - p * 107, 20.0), 127.0)))) << 24) >> 24);
                if (s3) {
                    music_.loopsLevel(double(int32_t(((127 - (v - 20)) * musicVol / 127 * 100))));
                }
                stream = double(int32_t(uint32_t(int32_t(std::trunc(std::min(std::max(127 - (127 - floorV) * p, floorV), 127.0)))) << 24) >> 24);
            } else {
                stream = std::trunc(floorV);
                if (s3) {
                    music_.loopsLevel(double(int32_t(musicVol * 100)));
                }
            }
            A.gain = (127 - stream) / 127;
            if (s3) {
                music_.streamLevel(stream);
            }
            if (!prev) {
                PlayOptions o = opts(slot::MAIN, 0x20, "UI", std::trunc(127 * A.gain));
                o.tag = "air";
                A.voice = play(o);
                if (s3 && music_.loopsStart && music_.loopsStart()) {
                    A.loopsOn = true;
                    music_.event(7);
                }
            }
            if (A.voice) {
                A.voice->setVolume(std::trunc(127 * A.gain));
            }
        } else if (prev == 1) {
            if (A.voice) {
                A.voice->stop(0.25);
            }
            A.voice = nullptr;
            A.gain = 0;
            if (A.loopsOn || (music_.loopsRequested && music_.loopsRequested())) {
                music_.event(8);
                music_.loopsStop();
            }
            A.loopsOn = false;
            music_.streamLevel(127);
            music_.loopsLevel(double(int32_t(musicVol * 100)));
        }
    }
    // one core's queue
    void drain(Rider& r) {
        float events[1 + 64 * 5];
        float tele[24];
        int n = 0;
        r.in([&] {
            const float* q = audio_events();
            n = std::min(int(q[0]), 64);
            std::memcpy(events, q, sizeof(float) * size_t(1 + n * 5));
            audio_events_clear();
            std::memcpy(tele, audio_telemetry(), sizeof tele);
        });
        r.prevT = r.t;
        r.prevValid = r.hasT;
        r.hasT = true;
        std::memcpy(r.t.data(), tele, sizeof tele);
        for (int i = 0; i < n; ++i) {
            const float* e = events + 1 + 5 * i;
            dispatch(r, int(e[0]), e[1], e[2], e[3], e[4]);
        }
    }

    AudioEngine& engine_;
    Sfx& sfx_;
    Speech& speech_;
    Crowd& crowd_;
    WorldAudio& world_;
    MusicAdapter music_;
    Random& random_;
    std::function<std::optional<int>(int, int)> clipEvents_;
    OrderedMap<const void*, Rider> riders_;
};

}  // namespace audio

}  // namespace ssx_native
