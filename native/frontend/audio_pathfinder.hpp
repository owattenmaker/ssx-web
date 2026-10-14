#pragma once
// The SSX 3 music runtime, EA Pathfinder (.mpf v4) song graphs (web/pathfinder.js, 1:1; docs/audio-logic.md section 8): the timed
// core state machine (createPathfinderCore: voices, commit rule, branch picks, loop counters, events and their actions, beat-
// synced loop slices) and the Web Audio player (createPathfinderPlayer: the deterministic look-ahead with snapshot / replay,
// reconciling sources with the committed segments, levels, ramps, pause / resume, fade out, stop, the lowpass, the loops).
// Natively a song's .mus is read whole and bars decode when they are committed (the page's streaming and worker decode serve
// the same bytes).
#include "audio_common.hpp"
#include "audio_decode.hpp"
#include "webaudio.hpp"
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace ssx_native {

namespace audio {

namespace pf {

struct Branch {
    int lo, hi, node;
};
struct Node {
    int index = 0, sample = 0, flags = 0, track = 0, section = 0, loopInit = 0, router = 0, measures = 0, beats = 0;
    bool random = false;
    int sync = 0;
    int group = -1;
    int trigger = 0;
    bool sliced = false;
    uint32_t w12 = 0;
    std::vector<Branch> branches;
};
struct Action {
    uint32_t mask = 0;
    uint32_t timeout = 0;
    int op = 0;
    uint32_t word = 0;
    int value = 0;
    int arg = 0;
    int argU = 0;
    int flags = 0;
    int b12 = 0;
    int b13 = 0;
    int u16_14 = 0;
};
struct EventDef {
    int id = 0;
    std::vector<Action> actions;
};
struct Sample {
    int index = 0;
    int track = 0;
    std::string kind;
    double durMs = 0;
    size_t offset = 0;
    size_t size = 0;
    std::optional<int> bankIndex;
    int value = 0;
    std::string codec;
};
struct Graph {
    std::string id;
    Json inf;
    std::vector<Node> nodes;
    std::vector<EventDef> events;
    std::vector<Sample> samples;
    std::vector<std::vector<std::pair<int, int>>> routers;
};

Graph buildGraph(const Json& json);

struct Song {
    std::string id;
    Json inf;
    Graph graph;
    Bytes mus;
    Bytes loops;
    std::optional<Bank> bank;
    // decoded PCM for a committed segment (a whole stream bar, or one loop-bank entry)
    std::optional<Decoded> decodeSegment(int sample, std::optional<int> bankIndex);
};
std::shared_ptr<Song> loadSong(Files& files, const std::string& id);

// 28F000: trunc(min(frames / N, 1) x 127)
inline int raceIntensity(int framesSinceReset, int tier = 0) {
    static const int n[3] = {800, 1000, 1200};
    const int N = tier >= 0 && tier < 3 ? n[tier] : 800;
    return int(std::trunc(std::min(std::max(double(framesSinceReset) / N, 0.0), 1.0) * 127));
}
// 3D5A98 ramp curve
double rampLevel(double from, double to, double t, int curve);

struct Seg {
    uint64_t id = 0;
    int voice = 0;
    int node = 0;
    int sample = 0;
    int section = 0;
    int group = -1;
    double start = 0, end = 0, commit = 0;
    std::string kind;
    int scale = 127;
    std::optional<double> beat0;
    std::optional<int> slice;
    std::optional<int> bankIndex;
};

struct Ramp {
    double time, from, to, durMs;
    int curve;
};

struct Voice {
    int track = 0;
    double latency = 0;
    int capacity = 0;
    bool sync = false;
    int node = 0;
    bool active = false;
    int group = -1;
    int loopNode = -1;
    int loopCount = 0;
    int slice = 0;
    int intensity = 0;
    int prevIntensity = 0;
    int scale = 127;
    bool paused = false;
    std::vector<Seg> queue;
    std::optional<double> nextAt, startAt;
    bool pendingJump = false;
    double beats = 0;
    std::vector<Ramp> ramps;
};

struct EventInst {
    int e = 0;
    size_t idx = 0;
    double post = 0;
    double last = 0;
    std::optional<double> wake;
    struct A {
        Action a;
        double deadline = 0;
        bool done = false;
    };
    std::vector<A> actions;
};

struct State {
    double time = 0;
    uint64_t seq = 0;
    uint64_t rngIdx = 0;
    std::array<int, 16> regs{};
    bool started = false;
    std::vector<Voice> voices;
    std::vector<std::shared_ptr<EventInst>> events;
    bool armed = false;
    // deep copy (the JS's structuredClone)
    State clone() const {
        State s = *this;
        for (auto& e : s.events) {
            e = std::make_shared<EventInst>(*e);
        }
        return s;
    }
};

class Core {
public:
    Core(const Graph& g, std::function<double()> random, int intensity, std::function<double(double)> clock = nullptr);
    double time() const {
        return st_.time;
    }
    State& state() {
        return st_;
    }
    Voice* voice(int t) {
        return t >= 0 && t < int(st_.voices.size()) ? &st_.voices[size_t(t)] : nullptr;
    }
    bool sendEvent(int e, std::optional<double> t = std::nullopt);
    void setIntensity(int value, std::optional<double> t = std::nullopt);
    void setArmed(bool on) {
        st_.armed = on;
    }
    void advanceTo(double T);
    double nextDue();
    bool finished(std::optional<double> t = std::nullopt);
    const Seg* playing(int track, std::optional<double> t = std::nullopt);
    State snapshot() const {
        return st_.clone();
    }
    // QA: every committed segment as committed (the JS's 'segment' outputs)
    std::vector<Seg>* segmentLog = nullptr;
    void restore(const State& s) {
        st_ = s.clone();
    }
    void trimRng();

private:
    const Node* node(int i) const {
        return i >= 0 && i < int(g_.nodes.size()) ? &g_.nodes[size_t(i)] : nullptr;
    }
    int rand7();
    int route(int prev, int n);
    int choose(Voice& v, int n, int value);
    int resolve(Voice& v, int prev, int n, int value, double t);
    Voice* master();
    struct Beat {
        double at;
        int index, inMeasure, bpm;
        double beatMs;
    };
    std::optional<Beat> masterBeat(double t, bool strict = false);
    const Seg* segAt(const Voice& v, double t) const;
    double lastEnd(const Voice& v, double t) const;
    Seg& pushSeg(Voice& v, int n, double start, double dur, std::optional<int> slice = std::nullopt, std::optional<int> bankIndex = std::nullopt);
    void stopVoice(Voice& v);
    void cutVoice(Voice& v, double t);
    // 'queued' | 'defer' | 'stopped'
    std::string commit(Voice& v, int n, double t);
    std::string advance(Voice& v, double t);
    double voiceDue(const Voice& v) const;
    bool postEvent(int e, double t);
    bool runOp(EventInst& inst, EventInst::A& a, Voice& v, double t);
    void processEvents(double t);
    int sectionOf();

    const Graph& g_;
    std::function<double()> random_;
    std::function<double(double)> clock_;
    std::vector<double> rngLog_;
    uint64_t rngBase_ = 0;
    State st_;
};

struct PlayerOptions {
    wa::Context* context = nullptr;
    std::shared_ptr<wa::GainNode> destination;
    std::shared_ptr<Song> song;
    std::function<double()> random;
    int intensity = 0;
    double lookahead = 1.0;
    double interval = 0.05;
    int cacheSize = 48;
    bool ambience = false;
    std::shared_ptr<wa::GainNode> loopDestination;
    bool clockRandom = false;
    double declickMs = 0;
    bool eventFirst = false;
    Timers* timers = nullptr;
};

class Player : public std::enable_shared_from_this<Player> {
public:
    explicit Player(PlayerOptions o);
    ~Player();
    bool finished();
    int intensity();
    void start(int event = 0, std::optional<double> when = std::nullopt);
    // 2B3B88: straight to the song, no repeat filter
    bool send(int e);
    // 2B3BC0 SendEvent: ignored when equal to the previous one
    bool event(int e);
    void setIntensity(int v);
    void pause();
    void resume();
    void fadeOut(double seconds);
    void stop();
    void setLevel(double v);
    void setLowpass(double value);
    void pump();
    // the loop overlay (track 1)
    void loopsStart();
    void loopsStop() {
        requested_ = false;
    }
    bool loopsRequested() const {
        return requested_;
    }
    void loopsSetLevel(double v);
    bool loopsArmed() {
        return core_.state().armed;
    }
    void warmEvents(const std::vector<int>& list);
    Core& core() {
        return core_;
    }
    bool hasLoops() {
        return core_.voice(1) != nullptr;
    }

private:
    struct Rec {
        std::shared_ptr<wa::BufferSourceNode> src;
        std::shared_ptr<wa::GainNode> gain;
        std::shared_ptr<wa::GainNode> fade;
        double at = 0;
        Seg seg;
        std::optional<double> stopAt;
    };
    double songNow() const;
    double ctxTime(double ms) const {
        return base_ + ms / 1000;
    }
    std::shared_ptr<const wa::Buffer> buffer(const Seg& seg, double now);
    void applyLevels();
    double sliceGain(const Seg& seg) const;
    void applyRamps();
    std::optional<Rec> startSource(const Seg& seg, double now, bool shift = false);
    bool halt(Rec& rec, double at);
    void kill(Rec& rec, double atMs);
    static void letGo(Rec& rec);
    void reconcile(double now);
    void startDue(double now);
    void simulate(double now, const std::function<void(double)>& input = nullptr);
    void schedulePump();

    PlayerOptions o_;
    wa::Context* ctx_;
    std::shared_ptr<Song> song_;
    Core core_;
    double clockMs_ = 0;
    double dk_ = 0;
    double pathLevelPct_ = 100;
    std::shared_ptr<wa::GainNode> out_;
    bool byteLoops_ = false;
    std::shared_ptr<wa::GainNode> loopOut_;
    struct Chain {
        std::shared_ptr<wa::GainNode> ramp, level;
        size_t rampsApplied = 0;
    };
    std::vector<Chain> chains_;
    std::shared_ptr<wa::BiquadFilterNode> lowpass_;
    double lowpassValue_ = 0xffff;
    double base_ = 0;
    bool started_ = false, paused_ = false, stopped_ = false, fading_ = false;
    double pausedAt_ = 0;
    int lastEvent_ = -1;
    std::optional<int> pending_;
    std::optional<State> snap_;
    double level_ = 127, loopsPct_ = 100;
    bool requested_ = false;
    OrderedMap<uint64_t, Rec> scheduled_;
    OrderedMap<uint64_t, Seg> waiting_;
    OrderedMap<std::string, std::shared_ptr<const wa::Buffer>> cache_;
    bool resuming_ = false;
    std::vector<int> resumed_;
    std::optional<Timers::Id> timer_;
};

}  // namespace pf

}  // namespace audio

}  // namespace ssx_native
