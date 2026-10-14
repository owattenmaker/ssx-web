#pragma once
// The one speech voice (web/audio-speech.js, 1:1; docs/audio-logic.md section 4 and 9.3): DJ Atomika, the PA announcer, rider and
// arcade speech posted through the category functions (bus gates, the game's request table, the EA pending table), resolved on the
// voice's free edge (2AFC88: < 500 ms left), streamed back to back the next free frame; DJ lines duck the mode-0 group to 65 %
// and release it 0.5 s after the stream. A bank's .dat is read whole (the page's range requests serve the same bytes).
#include "audio_common.hpp"
#include "audio_decode.hpp"
#include "audio_engine.hpp"
#include "audio_speech_events.hpp"
#include <functional>
#include <set>

namespace ssx_native {

namespace audio {

namespace ev {
constexpr uint32_t POST_OBJECT_COLLISION = 0x2077, HIT_BY_FRIEND = 0x2078, PASSED_BY_FRIEND = 0x207d, AGGRESSION_RESPONSE = 0x207e,
                   PASSED_BY_FOE = 0x207f, HEY_PASS_FRIEND = 0x2080, PASS_FOE = 0x2081, HIT_FOE = 0x208a, HIT_FRIEND = 0x208e, HIT_BY_FOE = 0x208f,
                   ARCADE_POWER_UPS = 0x20a7, ARCADE_PROMPTS = 0x20a8, ARCADE_BONUS = 0x20a9, WHOOH = 0x20b8, FINISH_LINE_RIDER = 0x20b9,
                   FIRST_SPOKE = 0x20ba, PA_VENUE_INTRO = 0x20bb, POST_SELECTION = 0x20bc, CUSTOMIZE = 0x20bd, PA_SPONSOR_INTRO = 0x20c0,
                   PA_RIDER_INTRO = 0x20c1, PA_RIDER_RACE_INTRO = 0x20c2, PA_MEDAL_RUN_INTRO = 0x20c3, PA_RIDER_POSITION = 0x20c4,
                   PA_FINISH_LINE = 0x20c5, PA_MEDALS = 0x20c7, HUB_WEATHER = 0x20c8, HUB_MTN_HISTORY = 0x20c9, HUB_CHAR_STORIES = 0x20ca,
                   EVENT_INTRO = 0x20cc, ARTIST_INTRO = 0x20cf, BC_CHALLENGE = 0x20d3, HEY = 0x20d4, RADIO_BIG_INTRO = 0x20e5, RADIO_BIG_OUTRO = 0x20e6,
                   HUB_GOING_ONS = 0x20e7, HUB_LOCAL_STORIES = 0x20e8, HUB_TERRAIN_INFO = 0x20e9, HUB_CHAR_PROGRESS = 0x2102, PEAK_BOSS = 0x210b,
                   BC_INTRO = 0x212b, TEXT_MESSAGE = 0x212c, FREE_RIDE_INTRO = 0x212d, AGGRESSION = 0x212e, HIGH_TRICK_SCORE = 0x212f,
                   ARCADE_UBER = 0x2133, ARCADE_ICONS = 0x2145;
constexpr uint32_t BIG_AIR[3] = {0x2084, 0x2083, 0x2091};
constexpr uint32_t TRICK_EASY[3] = {0x2082, 0x209a, 0x209c};
constexpr uint32_t TRICK_DIFFICULT[3] = {0x2090, 0x209b, 0x209d};
constexpr uint32_t WIPEOUT[3] = {0x2088, 0x20a0, 0x20a1};
}  // namespace ev

// 123128 CHARDB ids
inline int charId(const std::string& name) {
    static const std::map<std::string, int> ids = {{"moby", 0}, {"kaori", 1}, {"allegra", 2}, {"mac", 3}, {"zoe", 4},
                                                   {"griff", 5}, {"elise", 6}, {"nate", 7}, {"psymon", 8}, {"viggo", 9}};
    auto it = ids.find(name);
    return it == ids.end() ? -1 : it->second;
}
inline uint32_t speakerMask(int id) {
    static const uint32_t m[10] = {0x4, 0x10, 0x200, 0x2, 0x20, 0x100, 0x1, 0x80, 0x8, 0x40};
    return id >= 0 && id < 10 ? m[id] : 0;
}
inline uint32_t subjectMask(int id) {
    return id >= 0 && id < 10 ? speakerMask(id) : 0x1000;
}
// 2A1E20
inline int peakMask(int loc) {
    for (int x : {0, 1, 5, 8, 11, 14, 17, 18}) {
        if (loc == x) {
            return 1;
        }
    }
    for (int x : {2, 3, 6, 9, 12, 15, 19, 20}) {
        if (loc == x) {
            return 2;
        }
    }
    return 4;
}
// 2A1BD8
inline uint32_t courseBit(int loc, int region = -1) {
    if (loc >= 17) {
        static const std::map<int, std::map<int, int>> gates = {{17, {{0xe, 0}, {0xf, 5}, {0x10, 8}}},
                                                                {18, {{0xe, 1}, {0x11, 11}}},
                                                                {19, {{0xe, 2}, {0x10, 9}, {0x11, 12}}},
                                                                {20, {{0xe, 3}, {0xf, 6}}},
                                                                {21, {{0xe, 4}, {0xf, 7}, {0x10, 10}, {0x11, 13}}}};
        auto g = gates.find(loc);
        if (g == gates.end()) {
            return 0;
        }
        auto c = g->second.find(region);
        if (c == g->second.end()) {
            return 0;
        }
        loc = c->second;
    }
    return loc < 0 ? 0 : loc <= 4 ? (1u << loc) : loc <= 16 ? (1u << (loc + 1)) : 0;
}
inline uint32_t placeMask(int i) {
    static const uint32_t m[10] = {1, 2, 4, 8, 0x10, 0x200, 0x40, 0x80, 0x100, 0x200};
    return i >= 0 && i < 10 ? m[i] : 0;
}
inline std::string busOfBank(const std::string& name) {
    return name.rfind("DJ_", 0) == 0 ? "DJ" : name.rfind("PA_", 0) == 0 ? "PA" : name.rfind("Arcade", 0) == 0 ? "ARCADESPEECH" : "CHARACTER";
}

// a rider as the speech functions see it
struct SpeechRider {
    int id = -1;
    bool human = false;
    bool listener = false;
    bool upright = true;
    bool inRange = true;
};

class Speech {
public:
    Speech(AudioEngine& engine, Files& files, Timers& timers, Random& random, double declickMs = 0, std::function<bool()> radioBigStops = nullptr)
        : engine_(engine), files_(files), timers_(timers), rng_(uint32_t(std::floor(random() * 0x7fff))), declick_(declickMs / 1000),
          radioBigStops_(std::move(radioBigStops)) {}

    void init() {
        if (sched_) {
            return;
        }
        auto evt = files_.bytes("speech/Events.evt");
        auto reg = files_.json("speech/registry.json");
        engineEv_ = std::make_unique<speech::Engine>(speech::parseEvents(*evt), speech::makeBankTable(*reg), rng_, st_);
        sched_ = std::make_unique<speech::Scheduler>(*engineEv_, rng_, st_);
    }
    bool ready() const {
        return sched_ != nullptr;
    }
    std::function<void()> onIdle;
    // QA hooks (the page's onPost / onLines): an event posted, and the lines a dispatch sends
    std::function<void(uint32_t eventId, int speaker)> onPost;
    std::function<void(const std::vector<std::string>& banks, int speaker)> onLines;
    bool replay = false;
    const Json& posts() const {
        return posts_;
    }
    // 2B0290: free when idle or the playing line has < 500 ms left
    bool free() const {
        return !voice_ || (!voice_->loading && voice_->end - now() < 0.5);
    }
    bool busy() const {
        return voice_ && (voice_->loading || voice_->end > now());
    }
    bool audible(const std::string& bus) const {
        return engine_.busGain(bus) > 0;
    }

    // 2B1458 + 3D7418
    bool post(uint32_t eventId, const std::vector<uint32_t>& args, int speaker = 0, bool refreshDup = false) {
        if (!sched_ || replay) {
            return false;
        }
        if (!requests_.admit(0, eventId, refreshDup, speaker)) {
            return false;
        }
        const bool ok = sched_->post(eventId, args, tick_, 0);
        if (ok) {
            // QA: the last posted events [tick, id, speaker, ...args] (the page's debug().posts)
            char hex[16];
            std::snprintf(hex, sizeof hex, "%x", eventId);
            Json p = Json::array({tick_, hex, speaker});
            for (uint32_t a : args) {
                p.push_back(a);
            }
            posts_.push_back(p);
            if (posts_.size() > 40) {
                posts_.erase(posts_.begin());
            }
        }
        if (ok && onPost) {
            onPost(eventId, speaker);
        }
        return ok;
    }
    // 2B1758
    void flush() {
        if (!sched_) {
            return;
        }
        auto out = sched_->dispatch(0, tick_);
        if (out && hasItems(*out)) {
            pending_ = out;
            busyFlag_ = 0;
        }
    }
    void stop() {
        auto v = voice_;
        voice_.reset();
        pending_.reset();
        wa::Context* c = engine_.context();
        const double t = c ? c->currentTime() : 0;
        if (v) {
            for (auto& s : v->sources) {
                try {
                    if (declick_ > 0 && s.fade && s.at < t) {
                        auto& g = s.fade->gain;
                        g.cancelScheduledValues(t);
                        g.setValueAtTime(1, t);
                        g.linearRampToValueAtTime(0, t + declick_);
                        s.src->stop(t + declick_);
                    } else {
                        s.src->stop();
                        letGo(s);
                    }
                } catch (...) {
                }
            }
        }
        if (v && v->buses.count("DJ")) {
            engine_.release(0.5);
        }
    }
    // per 60 Hz frame (2B0C78 -> 2AFC88, 2B1720)
    void update() {
        if (!sched_) {
            return;
        }
        tick_++;
        requests_.tick();
        if (voice_ && !voice_->loading && voice_->end <= now()) {
            if (voice_->buses.count("DJ")) {
                engine_.release(0.5);
            }
            voice_.reset();
        }
        if (!free()) {
            busyFlag_ = 1;
            return;
        }
        if (busyFlag_ == 1) {
            if (onIdle) {
                onIdle();
            }
            auto out = sched_->dispatch(0, tick_);
            if (out && hasItems(*out)) {
                pending_ = out;
                busyFlag_ = 0;
            }
        } else if (pending_) {
            auto p = *pending_;
            pending_.reset();
            busyFlag_ = 1;
            send(p);
        }
    }
    // pause resume (289BB8): queued speech cancelled
    void cancelQueued() {
        pending_.reset();
        if (!sched_) {
            return;
        }
        for (int i = 0; i < 16; ++i) {
            if (sched_->slots[size_t(i)].active) {
                sched_->kill(i);
            }
        }
        for (auto& x : requests_.e) {
            x.active = false;
        }
    }
    // ---- category functions ----
    bool riderGate(const SpeechRider& r, bool needFree = true, bool needUpright = true, bool needListener = true) const {
        if (!audible("CHARACTER") || replay) {
            return false;
        }
        if (needListener && !r.human && !r.listener) {
            return false;
        }
        if (needUpright && !r.upright) {
            return false;
        }
        if (needFree && !free()) {
            return false;
        }
        return r.inRange;
    }
    static int peakIndex(int peak) {
        return peak == 1 ? 0 : peak == 2 ? 1 : 2;
    }
    void bigAir(const SpeechRider& r, int peak) {
        if (riderGate(r)) {
            post(ev::BIG_AIR[peakIndex(peak)], {speakerMask(r.id), 3}, r.id);
        }
    }
    void trick(const SpeechRider& r, int peak, bool difficult) {
        if (riderGate(r)) {
            post((difficult ? ev::TRICK_DIFFICULT : ev::TRICK_EASY)[peakIndex(peak)], {speakerMask(r.id)}, r.id);
        }
    }
    void wipeout(const SpeechRider& r, int peak, bool hub = false) {
        if (riderGate(r, true, false)) {
            post(ev::WIPEOUT[peakIndex(peak)], {speakerMask(r.id), hub ? 1u : 3u}, r.id);
        }
    }
    void whooh(const SpeechRider& r) {
        if (riderGate(r, true, false)) {
            post(ev::WHOOH, {speakerMask(r.id)}, r.id);
        }
    }
    void objectHit(const SpeechRider& r) {
        if (riderGate(r)) {
            post(ev::POST_OBJECT_COLLISION, {speakerMask(r.id)}, r.id);
        }
    }
    void pass(const SpeechRider& A, const SpeechRider& B, int rel, int fast) {
        if (rel == 2) {
            post(ev::PASS_FOE, {speakerMask(A.id), uint32_t(fast)}, A.id);
            post(ev::PASSED_BY_FOE, {speakerMask(B.id), uint32_t(fast)}, B.id);
        } else {
            post(ev::HEY_PASS_FRIEND, {speakerMask(A.id), uint32_t(fast), subjectMask(B.id) | 0x1000}, A.id);
            post(ev::PASSED_BY_FRIEND, {speakerMask(B.id), uint32_t(fast)}, B.id);
        }
    }
    void hit(const SpeechRider& A, const SpeechRider& B, int rel) {
        const std::vector<uint32_t> a = {speakerMask(A.id), 3};
        const std::vector<uint32_t> b = {speakerMask(B.id), 3};
        if (rel == 2) {
            post(ev::HIT_BY_FOE, b, B.id);
            post(ev::HIT_FOE, a, A.id);
        } else if (rel == 1) {
            post(ev::AGGRESSION_RESPONSE, b, B.id);
            post(ev::HIT_FOE, a, A.id);
        } else {
            post(ev::HIT_BY_FRIEND, b, B.id);
            post(ev::HIT_FRIEND, a, A.id);
        }
    }
    void finishLineRider(const SpeechRider& r, int place, bool rival = false, bool mp = false) {
        if (!audible("CHARACTER") || replay) {
            return;
        }
        const uint32_t p = placeMask(place);
        post(ev::FINISH_LINE_RIDER, {speakerMask(r.id), p == 1 ? 1u : rival || mp ? 2u : p == 2 || p == 4 ? 4u : 2u}, r.id);
    }
    void hey(const SpeechRider& r, const SpeechRider& to) {
        if (audible("CHARACTER") && !replay) {
            post(ev::HEY, {speakerMask(r.id), subjectMask(to.id)}, r.id);
        }
    }
    void bcChallenge(const SpeechRider& r, const SpeechRider& to) {
        if (audible("CHARACTER") && !replay) {
            post(ev::BC_CHALLENGE, {speakerMask(r.id), subjectMask(to.id), 4}, r.id);
        }
    }
    void postSelection(int id) {
        if (audible("CHARACTER") && free()) {
            post(ev::POST_SELECTION, {speakerMask(id)}, id, true);
        }
    }
    void customize(int id) {
        if (audible("CHARACTER") && free()) {
            post(ev::CUSTOMIZE, {speakerMask(id)}, id, true);
        }
    }
    void arcade(uint32_t eventId, uint32_t mask) {
        if (audible("ARCADESPEECH") && !replay && mask) {
            post(eventId, {mask}, 0xc);
        }
    }
    void dj(uint32_t eventId, std::vector<uint32_t> args) {
        if (audible("DJ") && !replay) {
            args.insert(args.begin(), 1u);
            post(eventId, args, 0xa);
        }
    }
    void radioBigIntro(int a1, int a2) {
        if (radioBigStops_ && radioBigStops_() && audible("DJ")) {
            stop();
        }
        dj(ev::RADIO_BIG_INTRO, {a1 ? 1u : 2u, a2 ? 2u : 1u});
    }
    void artistIntro(int sed) {
        const int s = sed;
        // (2 ** s) >>> 0: 0 from 2^32 up
        auto bit = [](int k) { return k >= 0 && k < 32 ? 1u << k : 0u; };
        const uint32_t a = s < 0 || s == 999 ? 0 : s < 100 ? bit(s) : 0;
        const uint32_t b = s < 0 || s == 999 || s < 100 ? 0 : bit(s - 100);
        dj(ev::ARTIST_INTRO, {3, a, b});
    }
    void eventIntro(int loc, int region = -1) {
        dj(ev::EVENT_INTRO, {courseBit(loc, region), 3});
    }
    void radioBigOutro() {
        dj(ev::RADIO_BIG_OUTRO, {});
    }
    void pa(uint32_t eventId, std::vector<uint32_t> args) {
        if (audible("PA") && !replay) {
            args.insert(args.begin(), 1u);
            post(eventId, args, 0xb);
        }
    }
    void venueIntro(int loc) {
        pa(ev::PA_VENUE_INTRO, {courseBit(loc)});
    }
    void sponsorIntro() {
        pa(ev::PA_SPONSOR_INTRO, {1});
    }
    void medalRunIntro(int loc) {
        pa(ev::PA_MEDAL_RUN_INTRO, {courseBit(loc)});
    }
    void riderIntro(int id) {
        pa(ev::PA_RIDER_INTRO, {subjectMask(id)});
    }
    void riderRaceIntro(int id) {
        pa(ev::PA_RIDER_RACE_INTRO, {subjectMask(id)});
    }
    void medals(int id, int place, bool career = true) {
        if (!replay) {
            pa(ev::PA_MEDALS, {subjectMask(id), !career ? 8u : place == 0 ? 1u : place == 1 ? 2u : place == 2 ? 4u : 0u});
        }
    }
    void paFinishLine(int id, int place) {
        if (id >= -1) {
            pa(ev::PA_FINISH_LINE, {subjectMask(id), placeMask(place)});
        }
    }
    void riderPosition(int id, int place, bool fromListener = false) {
        static const uint32_t riderPlace[6] = {1, 2, 4, 8, 0x10, 0x200};
        pa(ev::PA_RIDER_POSITION, {subjectMask(id), fromListener ? (place >= 0 && place < 6 ? riderPlace[place] : 0u) : placeMask(place)});
    }
    // the request being spoken is that speaker's (2A10C0)
    bool speaking(int speaker) const {
        return voice_ && voice_->speaker == speaker;
    }

private:
    struct Source {
        std::shared_ptr<wa::BufferSourceNode> src;
        std::shared_ptr<wa::GainNode> fade;
        double at = 0;
    };
    struct VoiceState {
        bool loading = true;
        double end = INFINITY;
        std::vector<Source> sources;
        std::set<std::string> buses;
        int speaker = -1;
    };
    double now() const {
        wa::Context* c = engine_.context();
        return c ? c->currentTime() : 0;
    }
    static bool hasItems(const std::vector<speech::Resolved>& out) {
        for (const auto& r : out) {
            if (!r.items.empty()) {
                return true;
            }
        }
        return false;
    }
    static void letGo(Source& s) {
        s.src->disconnect();
        if (s.fade) {
            s.fade->disconnect();
        }
    }
    // 3D9BD8 -> 2AF8A8 -> 2B04D8: the resolved lines back to back on the one voice
    void send(const std::vector<speech::Resolved>& list) {
        struct It {
            std::string bank;
            int line;
            int speaker;
        };
        std::vector<It> items;
        for (const auto& r : list) {
            const auto* req = requests_.lookup(0, r.eventId);
            if (req) {
                for (const auto& it : r.items) {
                    items.push_back({it.bank, it.line, req->speaker});
                }
            }
        }
        if (items.empty()) {
            return;
        }
        if (onLines) {
            std::vector<std::string> banks;
            for (const auto& it : items) {
                banks.push_back(it.bank);
            }
            onLines(banks, items[0].speaker);
        }
        wa::Context* context = engine_.context();
        if (!context || !engine_.live()) {
            return;
        }
        auto current = std::make_shared<VoiceState>();
        current->speaker = items[0].speaker;
        voice_ = current;
        // the lines' data and PCM first (the page awaits lineData, then decodes), outside the context's lock: the audio thread
        // renders on meanwhile, and the lines start after them (t from the clock as it then stands)
        std::vector<std::shared_ptr<wa::Buffer>> buffers(items.size());
        for (size_t i = 0; i < items.size(); ++i) {
            const auto& it = items[i];
            try {
                auto j = files_.json("speech/" + it.bank + ".json");
                const auto& lines = (*j)["lines"];
                if (it.line < 0 || size_t(it.line) >= lines.size()) {
                    continue;
                }
                const auto& line = lines[size_t(it.line)];
                auto dat = files_.bytes("speech/" + it.bank + ".dat");
                const size_t off = line["offset"].get<size_t>();
                const size_t size = line.value("size", size_t(0));
                const Decoded d = decodeStream(dat->data() + off, size ? size : dat->size() - off);
                auto buffer = std::make_shared<wa::Buffer>();
                buffer->sampleRate = float(d.sampleRate);
                buffer->length = std::max<size_t>(1, d.length);
                for (const auto& c : d.data) {
                    std::vector<float> v = c;
                    v.resize(buffer->length, 0.f);
                    for (float& x : v) {
                        if (!std::isfinite(x)) {
                            x = 0;
                        }
                    }
                    buffer->channels.push_back(std::move(v));
                }
                buffers[i] = buffer;
            } catch (const std::exception& e) {
                std::fprintf(stderr, "Speech bank unavailable: %s\n", e.what());
                if (voice_ == current) {
                    voice_.reset();
                }
                return;
            }
        }
        std::lock_guard<std::recursive_mutex> lock(context->mutex());
        double t = context->currentTime() + 0.02;
        for (size_t i = 0; i < items.size(); ++i) {
            const auto& it = items[i];
            const std::shared_ptr<wa::Buffer> buffer = buffers[i];
            if (!buffer) {
                continue;
            }
            const std::string bus = busOfBank(it.bank);
            Source s;
            s.src = context->createBufferSource();
            s.src->buffer = buffer;
            if (declick_ > 0) {
                s.fade = context->createGain();
                s.src->connect(s.fade);
                s.fade->connect(engine_.bus(bus, it.speaker));
            } else {
                s.src->connect(engine_.bus(bus, it.speaker));
            }
            s.src->start(t);
            s.at = t;
            auto src = s.src;
            auto fade = s.fade;
            s.src->onended = [src, fade] {
                src->disconnect();
                if (fade) {
                    fade->disconnect();
                }
            };
            if (bus == "DJ" && !current->buses.count("DJ")) {
                const double at = t;
                std::weak_ptr<VoiceState> wc = current;
                timers_.post(std::max(0.0, (at - context->currentTime()) * 1000), [this, wc] {
                    if (auto c = wc.lock(); c && voice_ == c) {
                        engine_.duck(0.65, 0.9999);
                    }
                });
            }
            current->buses.insert(bus);
            current->sources.push_back(s);
            t += buffer->duration();
        }
        current->loading = false;
        current->end = t;
    }

    AudioEngine& engine_;
    Files& files_;
    Timers& timers_;
    speech::EaRng rng_;
    speech::State st_;
    std::unique_ptr<speech::Engine> engineEv_;
    std::unique_ptr<speech::Scheduler> sched_;
    speech::RequestTable requests_;
    int64_t tick_ = 0;
    int busyFlag_ = 1;
    std::optional<std::vector<speech::Resolved>> pending_;
    std::shared_ptr<VoiceState> voice_;
    Json posts_ = Json::array();
    double declick_ = 0;
    std::function<bool()> radioBigStops_;
};

}  // namespace audio

}  // namespace ssx_native
