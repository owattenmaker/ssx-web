// The rider manager 0x128AF0 in the core (docs/core-migration.md slice M3, pv coreRiderManager): web/ai-racers.js's per-tick
// choreography of the six riders of one race, made here with the same calls in the same order. The riders are rider contexts of
// one core (web/rider_context.cpp); this file switches between them itself (rider_tls_set) where ai-racers.js switched them from JS.
//
// Every pass of 128AF0 runs over all riders in slot order: 120F20, 121068 (providers / controllers), 1210B0, 1211F8, 1216E0
// (motion), 121700 / 121728 (pose), then 121750 (second motion: touchdown, 105398, rider pairs 107888, 13F358 clamp), then
// 1217F8 / 121818 (course progress, route, triggers). The cores run it in three stages:
//   stage 1: each rider up to and including its pose (the human: its own tick until its pose; the computer riders: npc_tick,
//            race_begin, step_rider, animation_pose), from the human's pose (rider_manager_after_pose);
//   stage 2: each computer rider's animation_post (121750), from the start of the human's race_end (rider_manager_before_progress);
//            rider pairs are dispatched from inside each rider's 121750 (rider_manager_pairs);
//   stage 3: each computer rider's race_end (121818), then the FX passes phase by phase and section activation (manager_end_tick).
// The one shared game RNG 0x4FF030: stage-1 draws are controller-pass draws and continue one controller cursor in slot order; the
// rare other stage-1 draws (motion kind) start after every rider's controller draws (the later riders' counts predicted from their
// previous tick, docs/ai-racers.md "Known gaps"). Stages 2 and 3 draw in slot order from one cursor. The one visual stream (0x4FF018
// and the gp+0xA0C LCG) lives in the human's context; a computer rider's context borrows it for its passes that draw.
// The page's JS keeps the event set-up (configure, start, lineups, relationships) and is called back for the pair reactions'
// presentation and relationship notices (js_manager_react) and for the QA hooks (draw counts, injected world draws).
#include "../engine/game_rules.hpp"
#include "../engine/original_random.hpp"
#include "json.hpp"
#include "rider_local.hpp"
#include <emscripten/emscripten.h>
#include <cmath>
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

extern "C" {
void rider_tls_set(void*);
float* rider_world_state();
float* race_progress_info();
float* npc_world_buffer();
uint32_t* animation_rng_words();
void animation_rng_split(int enabled);
void animation_rng_defer(int draws);
uint32_t* animation_rng_split_info();
uint32_t* visual_rng_words();
uint32_t* visual_lcg_word();
uint32_t* world_triggers();
void world_trigger_apply(uint32_t key);
uint32_t* world_events();
void world_event_apply(const uint32_t* e);
void set_fx_deferred(int on);
int race_world_frame(int32_t tick, const float* riders);
float* race_world_state();
void race_world_rank_mode(int mode);
void race_world_score(unsigned slot, int32_t total);
float* race_world_pairs(int32_t tick, int slot);
uint32_t* score_object_dump();
void set_humans_finished(int finished);
void rider_world_peers();
float* npc_tick();
float* npc_tick_info();
void race_begin();
float* step_rider(float steering, int jump, int brake, int boost);
void animation_pose(float speed, float turn, float braking, float charge, int grounded, int jumpHeld, int mask, int tweak, int boost, float predictedLanding,
                    float impact, float flip);
float* animation_post();
float* race_end();
int fx_pass(int phase);
int section_pass();
uint32_t* pair_view();
void pair_translate(float x, float y, float z);
void pair_set_velocity(float x, float y, float z, int reseed);
void pair_react(int kind, int animation, int attack, const float* e);
void pair_knockout();
void race_world_set_relationships(const float* m);
void npc_configure(const char* riderText, const char* documentText);
void set_rider_attributes(const int32_t* raw, int32_t override);
void reset_pad_history();
void event_clock_hold(int on);
void npc_grid_start();
void npc_start_event();
void race_world_reset(const float* init);
void race_world_pair_setup(const float* p);
void npc_set_relationships(const float* row);
}

// The page's hooks (web/ai-racers.js Module.riderManagerHost), called in the human's context only.
EM_JS(void, js_manager_react, (int slot, int other, int kind, int attack), {
  const h = Module.riderManagerHost;
  if (h && h.react) h.react(slot, other, kind, attack);
});
EM_JS(void, js_manager_draws, (int slot, const uint32_t* before, int controller, int motion), {
  const h = Module.riderManagerHost;
  if (h && h.draws) h.draws(slot, before, controller, motion);
});
EM_JS(int, js_manager_after_rider, (int slot), {
  const h = Module.riderManagerHost;
  return h && h.afterRider ? h.afterRider(slot) | 0 : 0;
});

namespace {
constexpr unsigned kSlots = 6;
constexpr unsigned kMaxTriggers = 1024;
constexpr unsigned kEventKinds = 16;
using Words = std::array<uint32_t, 6>;
// A float argument to an int parameter as the JS call made it (ToInt32 of the number).
int jsInt(float x) {
    return ssx::rules::jsToInt32(double(x));
}

// A rider of the race: its context and slot (position k of [human, ...computer riders] is slot k, as ai-racers.js indexes cores()).
struct Rider {
    void* block = nullptr;
    int slot = 0;
    bool human = false;
    // the tick-start record (snapshot): rider_world_state [0..15], race_progress_info [0..7]
    float startState[16] = {};
    float startProgress[8] = {};
    // the computer rider's last provider command and its info (ai-racers.js n.command / n.info), and its finish
    float command[24] = {};
    float info[9] = {};
    bool finished = false;
};

// pv coreRelationships (docs/core-migration.md M4a): the race's relationship records (0x4A6CA8 + bank * 0x9B50 + char * 0xF88 +
// other * 3 + 0xBC1: kind, level, score; web/lineup.js), changed by the pair reactions 0x155BF0 in the tick, and the 6x6 levels /
// kinds 0x155B50 / 0x155AB0 they give. The page loads them at each start and reads them back after each tick (its storage).
struct RelationshipEvent {
    int slot = 0;
    int other = 0;
    int kind = 0;
    int attack = 0;
    int audio = -1;   // the hit speech's relation 2A1820 of (other, target) before the change (web/sfx-game.js relationOf), -1 none
    int notice = 0;   // 0x155BF0 -> 0x1E2A08: the human's own record about the target rose a level
    int score = 0;    // the changed record's score
};
constexpr unsigned kRelationshipEvents = 64;
struct Relationships {
    bool loaded = false;
    bool enabled = false;  // the page has tables (else only the speech's values are kept)
    int characters[6] = {};
    int banks[6] = {};
    int rival = -1;
    int8_t table[3][10][10][3] = {};
    float scores[6][6] = {};
    int kinds[6][6] = {};
    unsigned events = 0;
    RelationshipEvent event[kRelationshipEvents] = {};
};

struct RiderManager {
    bool active = false;
    Relationships rel{};
    void* humanBlock = nullptr;
    unsigned riders = 0;  // the human and the computer riders
    int count = 0;        // the race's rider count (the world buffer's [1])
    bool pairsEnabled = false;
    bool sharedVisual = false;
    bool drawsHook = false;
    bool afterRiderHook = false;
    std::array<Rider, kSlots> rider{};
    int tick = 0;
    int stage = 0;
    int returnTick = -1;
    Words tickStart{};
    Words cursor{};
    uint32_t motionTotal = 0;
    std::array<uint32_t, kSlots> lastControllerDraws{};
    bool anchor = false;
    Words anchorWords{};
    float worldState[396] = {};
    float riderBuffer[36] = {};
    uint32_t knownCount = 0;
    std::array<uint32_t, kMaxTriggers> known{};
    uint32_t worldEvents = 0;
    std::array<uint32_t, kEventKinds> worldEventKinds{};
    std::array<float, 5> pairCounts{};
    // the context whose 121750 is running (its cursor is the shared one; pair draws come from it)
    int activeSlot = 0;
};

RIDER_LOCAL RiderManager manager;
// In every context of a race: the human context's manager (set by manager_configure), for the hooks a computer rider's pass reaches.
RIDER_LOCAL RiderManager* hostManager = nullptr;

// f's body in a function of its own: the compiler takes a function's thread-local addresses as fixed for the whole function
// (one thread), so the work after a context switch must not be inlined into the code that switched (it would reach the previous
// context's variables).
template <class F>
__attribute__((noinline)) void runOpaque(F* f) {
    (*f)();
}
// Runs f in a rider's context and comes back (also on an exception).
template <class F>
void inContext(void* block, F f) {
    void* previous = __builtin_wasm_tls_base();
    if (previous == block) {
        runOpaque(&f);
        return;
    }
    rider_tls_set(block);
    try {
        runOpaque(&f);
    } catch (...) {
        rider_tls_set(previous);
        throw;
    }
    rider_tls_set(previous);
}
// A context's fixed pointers (pure getters: per-context statics)
uint32_t* rngOf(const Rider& r) {
    uint32_t* p = nullptr;
    inContext(r.block, [&] { p = animation_rng_words(); });
    return p;
}
uint32_t next(uint32_t* words) {
    ssx::OriginalRandomState s;
    std::memcpy(s.words.data(), words, 24);
    const uint32_t v = s.next();
    std::memcpy(words, s.words.data(), 24);
    return v;
}
void advance(Words& w, uint32_t n) {
    for (uint32_t k = 0; k < n; ++k) {
        next(w.data());
    }
}
uint32_t countDraws(Words a, const Words& b) {
    for (uint32_t n = 0; n <= 256; ++n) {
        if (a == b) {
            return n;
        }
        next(a.data());
    }
    return 0xffffffffu;
}
bool excluded(const RiderManager& M) {
    return M.returnTick == 3 || M.returnTick == 4;
}
bool knownHas(const RiderManager& M, uint32_t key) {
    for (uint32_t k = 0; k < M.knownCount; ++k) {
        if (M.known[k] == key) {
            return true;
        }
    }
    return false;
}
void knownAdd(RiderManager& M, uint32_t key) {
    if (M.knownCount >= kMaxTriggers) {
        throw std::runtime_error("rider manager: too many shared triggers");
    }
    M.known[M.knownCount++] = key;
}
void knownDelete(RiderManager& M, uint32_t key) {
    for (uint32_t k = 0; k < M.knownCount; ++k) {
        if (M.known[k] == key) {
            for (uint32_t j = k + 1; j < M.knownCount; ++j) {
                M.known[j - 1] = M.known[j];
            }
            M.knownCount--;
            return;
        }
    }
}

// One shared world (web/shared_world.inc): a course-script trigger fired in one context, and every change a rider made to a
// shared entity, replayed at once in every other context, right after the pass that made it.
void syncWorld(RiderManager& M, unsigned from) {
    std::vector<uint32_t> keys;
    inContext(M.rider[from].block, [&] {
        const uint32_t* p = world_triggers();
        keys.assign(p + 1, p + 1 + p[0]);
    });
    for (uint32_t key : keys) {
        if (knownHas(M, key)) {
            continue;
        }
        knownAdd(M, key);
        for (unsigned c = 0; c < M.riders; ++c) {
            if (c != from) {
                inContext(M.rider[c].block, [&] { world_trigger_apply(key); });
            }
        }
    }
    std::vector<uint32_t> words;
    inContext(M.rider[from].block, [&] {
        const uint32_t* p = world_events();
        words.assign(p + 1, p + 1 + p[0]);
    });
    for (size_t at = 0; at < words.size();) {
        const size_t len = 2 + words[at + 1];
        const uint32_t* event = words.data() + at;
        for (unsigned c = 0; c < M.riders; ++c) {
            if (c != from) {
                inContext(M.rider[c].block, [&] { world_event_apply(event); });
            }
        }
        // a released Spline piece's owner: its next launch replicates again
        if (words[at] == 10) {
            knownDelete(M, words[at + 3]);
        }
        M.worldEvents++;
        if (words[at] < kEventKinds) {
            M.worldEventKinds[words[at]]++;
        }
        at += len;
    }
}

// The view of peer r a computer rider's provider reads: its position, velocity, path, upper class, control, motion, and progress.
struct View {
    const float* state;
    const float* progress;
};
// rider_world_state of r, live (a pure read)
const float* liveState(const Rider& r) {
    const float* p = nullptr;
    inContext(r.block, [&] { p = rider_world_state(); });
    return p;
}
const float* liveProgress(const Rider& r) {
    const float* p = nullptr;
    inContext(r.block, [&] { p = race_progress_info(); });
    return p;
}
// npc_world_buffer for `target` (ai-racers.js worldBuffer). view(k) picks which state of rider k the reading pass sees;
// upper(k, out) gives the class word (+7) to use instead, when it returns true.
template <class ViewOf, class UpperOf>
void worldBuffer(RiderManager& M, unsigned target, ViewOf view, UpperOf upper) {
    float* w = nullptr;
    inContext(M.rider[target].block, [&] { w = npc_world_buffer(); });
    std::fill(w, w + 160, 0.f);
    w[0] = float(M.tick);
    w[1] = float(M.count);
    const int self = M.rider[target].slot;
    constexpr int per = 360;
    w[2] = M.worldState[per + self * 6 + 1];
    w[3] = M.worldState[per + self * 6 + 2];
    w[4] = M.worldState[per + self * 6 + 3];
    w[5] = M.worldState[per + self * 6 + 4];
    for (unsigned k = 0; k < M.riders; ++k) {
        const Rider& r = M.rider[k];
        const int p = 8 + 12 * r.slot;
        const View v = view(k);
        const float* s = v.state;
        w[p] = s[0];
        w[p + 1] = s[1];
        w[p + 2] = s[2];
        w[p + 3] = s[3];
        w[p + 4] = s[4];
        w[p + 5] = s[5];
        float cls = 0;
        const bool hasClass = upper(k, cls);
        w[p + 6] = s[6];
        w[p + 7] = r.human ? 1.f : 0.f;
        w[p + 8] = hasClass ? cls : s[7];
        w[p + 9] = v.progress[0];
        w[p + 10] = 1;
        const int rec = (self * 6 + r.slot) * 10;
        for (int j = 0; j < 10; ++j) {
            w[80 + 10 * r.slot + j] = M.worldState[rec + j];
        }
    }
}
View startView(const Rider& r) {
    return {r.startState, r.startProgress};
}
// r's tick-start record with +0x1E0 as its controller left it this tick (rider_world_state [16..19]: velocity, set)
View afterController(const Rider& r, float* scratch) {
    const float* now = liveState(r);
    if (now[19] != 1) {
        return startView(r);
    }
    std::memcpy(scratch, r.startState, 16 * sizeof(float));
    scratch[3] = now[16];
    scratch[4] = now[17];
    scratch[5] = now[18];
    return {scratch, r.startProgress};
}
uint32_t later(const RiderManager& M, int slot) {
    uint32_t sum = 0;
    for (unsigned j = 0; j < kSlots; ++j) {
        if (int(j) > slot) {
            sum += M.lastControllerDraws[j];
        }
    }
    return sum;
}
void enter(RiderManager& M, unsigned k) {
    const Rider& r = M.rider[k];
    std::memcpy(rngOf(r), M.cursor.data(), 24);
    inContext(r.block, [&] {
        animation_rng_split(1);
        animation_rng_defer(int(later(M, r.slot) + M.motionTotal));
    });
}
void leaveStage1(RiderManager& M, unsigned k) {
    const Rider& r = M.rider[k];
    uint32_t counts[3] = {};
    inContext(r.block, [&] { std::memcpy(counts, animation_rng_split_info(), sizeof counts); });
    const Words before = M.cursor;
    std::memcpy(M.cursor.data(), rngOf(r), 24);
    M.lastControllerDraws[r.slot] = counts[0];
    M.motionTotal += counts[1];
    if (M.drawsHook) {
        js_manager_draws(r.slot, before.data(), int(counts[0]), int(counts[1]));
    }
}
// Runs fn in rider k's context with the shared cursor `words`; returns the cursor after it.
template <class F>
Words onShared(RiderManager& M, unsigned k, Words words, F fn) {
    uint32_t* v = rngOf(M.rider[k]);
    std::memcpy(v, words.data(), 24);
    const int previous = M.activeSlot;
    M.activeSlot = int(k);
    try {
        inContext(M.rider[k].block, fn);
    } catch (...) {
        M.activeSlot = previous;
        throw;
    }
    M.activeSlot = previous;
    Words after{};
    std::memcpy(after.data(), v, 24);
    if (M.drawsHook) {
        const uint32_t n = countDraws(words, after);
        if (n) {
            js_manager_draws(M.rider[k].slot, words.data(), 0, int(n));
        }
    }
    return after;
}
// The one visual stream: rider k's context borrows the human's six words and LCG word for fn, and hands them back.
template <class F>
void onVisual(RiderManager& M, unsigned k, F fn) {
    if (!M.sharedVisual || k == 0) {
        inContext(M.rider[k].block, fn);
        return;
    }
    uint32_t* vw = nullptr;
    uint32_t* vl = nullptr;
    uint32_t* hw = nullptr;
    uint32_t* hl = nullptr;
    inContext(M.rider[k].block, [&] {
        vw = visual_rng_words();
        vl = visual_lcg_word();
    });
    inContext(M.rider[0].block, [&] {
        hw = visual_rng_words();
        hl = visual_lcg_word();
    });
    std::memcpy(vw, hw, 24);
    *vl = *hl;
    try {
        inContext(M.rider[k].block, fn);
    } catch (...) {
        std::memcpy(hw, vw, 24);
        *hl = *vl;
        throw;
    }
    std::memcpy(hw, vw, 24);
    *hl = *vl;
}

void stage1(RiderManager& M) {
    if (M.stage != 1) {
        return;
    }
    leaveStage1(M, 0);
    syncWorld(M, 0);
    if (!excluded(M)) {
        for (unsigned k = 1; k < M.riders; ++k) {
            Rider& n = M.rider[k];
            float scratch[kSlots][16];
            worldBuffer(
                M, k,
                [&](unsigned j) { return M.rider[j].slot < n.slot ? afterController(M.rider[j], scratch[j]) : startView(M.rider[j]); },
                [&](unsigned j, float& cls) {
                    if (M.rider[j].slot >= n.slot) {
                        return false;
                    }
                    cls = liveState(M.rider[j])[7];
                    return true;
                });
            enter(M, k);
            inContext(n.block, [&] {
                std::memcpy(n.command, npc_tick(), sizeof n.command);
                std::memcpy(n.info, npc_tick_info(), sizeof n.info);
                const float* c = n.command;
                race_begin();
                const float* state = step_rider(c[0], jsInt(c[6]), c[2] != 0 && c[2] == c[2] ? 1 : 0, jsInt(c[7]));
                animation_pose(state[7], c[10], c[11], state[9], jsInt(state[8]), jsInt(c[6]), jsInt(c[8]), jsInt(c[12]), jsInt(c[7]), 0, state[15],
                               c[13]);
            });
            leaveStage1(M, k);
            syncWorld(M, k);
        }
    }
    for (unsigned k = 0; k < M.riders; ++k) {
        inContext(M.rider[k].block, [&] {
            animation_rng_split(0);
            animation_rng_defer(0);
        });
    }
    Words words = M.cursor;
    advance(words, M.motionTotal);
    std::memcpy(rngOf(M.rider[0]), words.data(), 24);
    M.stage = 2;
}

void stage2(RiderManager& M) {
    if (M.stage == 1) {
        stage1(M);
    }
    if (M.stage != 2) {
        return;
    }
    Words words{};
    std::memcpy(words.data(), rngOf(M.rider[0]), 24);
    syncWorld(M, 0);
    if (!excluded(M)) {
        for (unsigned k = 1; k < M.riders; ++k) {
            words = onShared(M, k, words, [] { animation_post(); });
            syncWorld(M, k);
        }
    }
    std::memcpy(rngOf(M.rider[0]), words.data(), 24);
    M.stage = 3;
}
}  // namespace

namespace {
// web/sfx-game.js relationOf(m, a, b): 2 when either level is 2 or more, else the kind 0 / 1, else 2; -1 when a kind is unknown.
int audioRelation(const Relationships& R, int a, int b) {
    for (int x = 0; x < 6; ++x) {
        for (int y = 0; y < 6; ++y) {
            if (x != y && R.kinds[x][y] < 0) {
                return -1;
            }
        }
    }
    if (R.scores[a][b] >= 2 || R.scores[b][a] >= 2) {
        return 2;
    }
    const int k = R.kinds[a][b];
    return k == 0 ? 0 : k == 1 ? 1 : 2;
}
// The levels to the race world 10F560 and to each computer rider's own row (ai-racers.js setRelationships).
void pushRelationships(RiderManager& M) {
    float flat[36];
    for (int a = 0; a < 6; ++a) {
        for (int b = 0; b < 6; ++b) {
            flat[a * 6 + b] = M.rel.scores[a][b];
        }
    }
    inContext(M.humanBlock, [&] { race_world_set_relationships(flat); });
    for (unsigned k = 1; k < M.riders; ++k) {
        const float* row = flat + M.rider[k].slot * 6;
        inContext(M.rider[k].block, [&] { npc_set_relationships(row); });
    }
}
// 107E70's handlers: the speech's relation first (2A0A30), then the relationship event 155BF0 (web/ai-race.js onReact): `other`'s
// record about `target`; event 0 soft, 1 crash, 2 soft attack, 3 crash attack; the levels and kinds again, pushed when changed.
void relationshipReaction(RiderManager& M, int slot, int other, int kind, int attack) {
    Relationships& R = M.rel;
    if (R.events >= kRelationshipEvents) {
        throw std::runtime_error("rider manager: too many pair reactions in a tick");
    }
    RelationshipEvent& e = R.event[R.events++];
    e = RelationshipEvent{};
    e.slot = slot;
    e.other = other;
    e.kind = kind;
    e.attack = attack;
    e.audio = other >= 0 && other < 6 && slot >= 0 && slot < 6 ? audioRelation(R, other, slot) : -1;
    if (!R.enabled) {
        return;
    }
    const int event = (kind == 2 ? 1 : 0) + (attack ? 2 : 0);
    int8_t* record = R.table[R.banks[other]][R.characters[other]][R.characters[slot]];
    const int before = record[1];
    const ssx::rules::Relationship next = ssx::rules::relationshipEvent(record[0], record[2], event);
    // stored as the page's hex bytes are (each & 255, read back signed)
    record[0] = int8_t(next.kind);
    record[1] = int8_t(next.level);
    record[2] = int8_t(next.score);
    e.notice = other == 0 && before < record[1] ? 1 : 0;
    e.score = record[2];
    bool changed = false;
    for (int a = 0; a < 6; ++a) {
        for (int p = 0; p < 6; ++p) {
            const int own = R.characters[a];
            const int8_t* r = R.table[R.banks[p]][R.characters[p]][own];
            const float level = own == R.rival ? 3.f : float(r[1]);
            R.kinds[a][p] = own == R.rival ? 2 : r[0];
            if (level != R.scores[a][p]) {
                changed = true;
            }
            R.scores[a][p] = level;
        }
    }
    if (changed) {
        pushRelationships(M);
    }
}
}  // namespace

// ---- hooks from the riders' passes (web/animation_bridge.cpp, web/race_bridge.cpp, web/race_world.cpp) ----
// (C linkage: web/race_world.cpp and web/race_bridge.cpp declare them inside their extern "C" blocks)
extern "C" {
// Whether this context's race runs on the core's rider manager.
bool rider_manager_native() {
    return hostManager != nullptr && hostManager->active;
}
// The human's pose is done: the computer riders' stage 1.
void rider_manager_after_pose() {
    if (rider_manager_native() && __builtin_wasm_tls_base() == hostManager->humanBlock) {
        stage1(*hostManager);
    }
}
// The human's race_end begins: the computer riders' stage 2.
void rider_manager_before_progress() {
    if (rider_manager_native() && __builtin_wasm_tls_base() == hostManager->humanBlock) {
        stage2(*hostManager);
    }
}
// Rider pairs 0x107888 of the context whose 121750 is running (the human's race world dispatches them).
void rider_manager_pairs() {
    if (!rider_manager_native()) {
        return;
    }
    RiderManager& M = *hostManager;
    if (!M.pairsEnabled || M.stage != 2) {
        return;
    }
    void* current = __builtin_wasm_tls_base();
    int slot = -1;
    for (unsigned k = 0; k < M.riders; ++k) {
        if (M.rider[k].block == current) {
            slot = M.rider[k].slot;
        }
    }
    float out[5] = {};
    inContext(M.humanBlock, [&] { std::memcpy(out, race_world_pairs(M.tick, slot), sizeof out); });
    for (unsigned k = 0; k < 5; ++k) {
        M.pairCounts[k] += out[k];
    }
}
// The pair dispatch's callbacks (ai-racers.js pairHost), slot = position in [human, ...computer riders].
void rider_manager_pair_view(int slot, uint32_t* out) {
    RiderManager& M = *hostManager;
    inContext(M.rider[slot].block, [&] { std::memcpy(out, pair_view(), 560); });
}
void rider_manager_pair_translate(int slot, float x, float y, float z) {
    RiderManager& M = *hostManager;
    inContext(M.rider[slot].block, [&] { pair_translate(x, y, z); });
}
void rider_manager_pair_velocity(int slot, float x, float y, float z, int reseed) {
    RiderManager& M = *hostManager;
    inContext(M.rider[slot].block, [&] { pair_set_velocity(x, y, z, reseed); });
}
// 107E70 -> 10E228 / 10E2E8 / 10E3A8 / 10E468: speech 2A0A30 and the relationship 155BF0 (the page's), the KO count, the reaction.
void rider_manager_pair_react(int slot, int kind, int animation, int attack, const float* event, int other) {
    RiderManager& M = *hostManager;
    if (M.rel.loaded) {
        relationshipReaction(M, slot, other, kind, attack);
    } else {
        js_manager_react(slot, other, kind, attack);
    }
    if (kind == 2 && attack && other >= 0 && unsigned(other) < M.riders) {
        inContext(M.rider[other].block, [] { pair_knockout(); });
    }
    float e[10];
    std::memcpy(e, event, sizeof e);
    if (slot == M.activeSlot) {
        inContext(M.rider[slot].block, [&] { pair_react(kind, animation, attack, e); });
    } else {
        uint32_t* a = rngOf(M.rider[M.activeSlot]);
        uint32_t* v = rngOf(M.rider[slot]);
        std::memcpy(v, a, 24);
        inContext(M.rider[slot].block, [&] { pair_react(kind, animation, attack, e); });
        std::memcpy(a, v, 24);
    }
}
uint32_t rider_manager_pair_random() {
    RiderManager& M = *hostManager;
    return next(rngOf(M.rider[M.activeSlot]));
}

}

// ---- the page's calls (web/ai-racers.js with pv coreRiderManager), in the human's context ----
extern "C" {
// The race's riders: blocks (the human's first) with their slots, the rider count, flags: 1 pairs, 2 the shared visual stream,
// 4 the draws hook, 8 the after-rider hook. Every context of the race points at this manager.
EMSCRIPTEN_KEEPALIVE void manager_configure(void* const* blocks, const int* slots, int riders, int count, int flags) {
    RiderManager& M = manager;
    if (riders < 1 || riders > int(kSlots)) {
        throw std::runtime_error("manager_configure: rider count");
    }
    M.humanBlock = __builtin_wasm_tls_base();
    M.riders = unsigned(riders);
    M.count = count;
    M.pairsEnabled = (flags & 1) != 0;
    M.sharedVisual = (flags & 2) != 0;
    M.drawsHook = (flags & 4) != 0;
    M.afterRiderHook = (flags & 8) != 0;
    for (unsigned k = 0; k < kSlots; ++k) {
        M.rider[k] = Rider{};
    }
    for (int k = 0; k < riders; ++k) {
        M.rider[k].block = blocks[k];
        M.rider[k].slot = slots[k];
        M.rider[k].human = k == 0;
        if (slots[k] != k) {
            throw std::runtime_error("manager_configure: slot order");
        }
    }
    M.rider[0].block = M.humanBlock;
    RiderManager* host = &manager;
    for (int k = 0; k < riders; ++k) {
        inContext(M.rider[k].block, [&] { hostManager = host; });
    }
    M.active = true;
}
// Off (the in-world race's detach): the hooks go back to the page.
EMSCRIPTEN_KEEPALIVE void manager_release() {
    RiderManager& M = manager;
    for (unsigned k = 0; k < M.riders; ++k) {
        inContext(M.rider[k].block, [&] { hostManager = nullptr; });
    }
    M.active = false;
}
// The event start (ai-racers.js start): the tick, the stage, the predicted counts, the known triggers and the event counts.
EMSCRIPTEN_KEEPALIVE void manager_start() {
    RiderManager& M = manager;
    M.tick = 0;
    M.stage = 0;
    M.lastControllerDraws.fill(0);
    M.knownCount = 0;
    M.worldEvents = 0;
    M.worldEventKinds.fill(0);
    for (unsigned k = 0; k < M.riders; ++k) {
        M.rider[k].finished = false;
    }
}
// WS15's return (ai-racers.js resume): the tick restarts with the return's tick count, no anchor words.
EMSCRIPTEN_KEEPALIVE void manager_resume() {
    RiderManager& M = manager;
    M.anchor = false;
    M.tick = 0;
    M.stage = 0;
    M.returnTick = 0;
}
// The game RNG the original has at the countdown anchor (null: none).
EMSCRIPTEN_KEEPALIVE void manager_set_anchor(const uint32_t* words) {
    RiderManager& M = manager;
    M.anchor = words != nullptr;
    if (words) {
        std::memcpy(M.anchorWords.data(), words, 24);
    }
}
// Before the human's tick: the manager refresh 10F560 on the tick-start state, the human's 115D48 peers, the RNG cursors.
// anchorTick: the document's anchor tick (-1 none); scores: rank mode 2 (freestyle, R&B: every slot's run score at the tick start).
EMSCRIPTEN_KEEPALIVE float* manager_begin_tick(int anchorTick, int scores) {
    RiderManager& M = manager;
    M.rel.events = 0;
    // WS1 phase 1's 128A48(C, 0) (0x234570): rank mode 0, every rank 0
    if (M.returnTick == 3) {
        race_world_rank_mode(0);
    }
    if (M.anchor && M.tick == anchorTick) {
        std::memcpy(rngOf(M.rider[0]), M.anchorWords.data(), 24);
    }
    for (unsigned k = 0; k < M.riders; ++k) {
        Rider& r = M.rider[k];
        inContext(r.block, [&] {
            std::memcpy(r.startState, rider_world_state(), sizeof r.startState);
            std::memcpy(r.startProgress, race_progress_info(), sizeof r.startProgress);
        });
    }
    std::fill(std::begin(M.riderBuffer), std::end(M.riderBuffer), 0.f);
    for (unsigned k = 0; k < M.riders; ++k) {
        const Rider& r = M.rider[k];
        float* b = M.riderBuffer + r.slot * 6;
        b[0] = r.startState[0];
        b[1] = r.startState[1];
        b[2] = r.startState[2];
        b[3] = r.startProgress[0];
        b[4] = 0;
        b[5] = 1;
    }
    if (scores) {
        for (unsigned k = 0; k < M.riders; ++k) {
            int32_t total = 0;
            inContext(M.rider[k].block, [&] { total = int32_t(score_object_dump()[0x198 / 4]); });
            race_world_score(unsigned(M.rider[k].slot), total);
        }
    }
    // 12A250 (11A228 early return): every human finished; the human context holds its +0x470
    {
        const int done = race_progress_info()[3] >= 0 ? 1 : 0;
        for (unsigned k = 1; k < M.riders; ++k) {
            inContext(M.rider[k].block, [&] { set_humans_finished(done); });
        }
    }
    race_world_frame(M.tick, M.riderBuffer);
    std::memcpy(M.worldState, race_world_state(), sizeof M.worldState);
    worldBuffer(M, 0, [&](unsigned j) { return startView(M.rider[j]); }, [](unsigned, float&) { return false; });
    rider_world_peers();
    if (M.sharedVisual) {
        for (unsigned k = 0; k < M.riders; ++k) {
            inContext(M.rider[k].block, [] { set_fx_deferred(1); });
        }
    }
    std::memcpy(M.tickStart.data(), rngOf(M.rider[0]), 24);
    M.cursor = M.tickStart;
    M.motionTotal = 0;
    enter(M, 0);
    M.stage = 1;
    return M.worldState;
}
// After the human's tick (race_end): stage 3, each computer rider's 121818 in slot order, then the FX passes and section activation.
EMSCRIPTEN_KEEPALIVE void manager_end_tick() {
    RiderManager& M = manager;
    if (M.stage == 1) {
        stage1(M);
    }
    if (M.stage == 2) {
        stage2(M);
    }
    Words words{};
    std::memcpy(words.data(), rngOf(M.rider[0]), 24);
    syncWorld(M, 0);
    if (M.afterRiderHook) {
        advance(words, uint32_t(js_manager_after_rider(0)));
    }
    if (!excluded(M)) {
        for (unsigned k = 1; k < M.riders; ++k) {
            Rider& n = M.rider[k];
            worldBuffer(M, k, [&](unsigned j) { return View{liveState(M.rider[j]), liveProgress(M.rider[j])}; }, [](unsigned, float&) { return false; });
            words = onShared(M, k, words, [&] {
                onVisual(M, k, [&] {
                    const float* race = race_end();
                    if (race[2] != 0 && race[2] == race[2]) {
                        n.finished = true;
                    }
                });
            });
            syncWorld(M, k);
            if (M.afterRiderHook) {
                advance(words, uint32_t(js_manager_after_rider(n.slot)));
            }
        }
    }
    if (M.afterRiderHook) {
        advance(words, uint32_t(js_manager_after_rider(-1)));
    }
    std::memcpy(rngOf(M.rider[0]), words.data(), 24);
    // the rider FX passes over every rider, phase-major, on the shared visual stream (deferred in every context)
    if (M.sharedVisual) {
        const unsigned list = excluded(M) ? 1 : M.riders;
        for (int phase = 0; phase <= 4; ++phase) {
            for (unsigned k = 0; k < list; ++k) {
                onVisual(M, k, [&] { fx_pass(phase); });
            }
        }
    }
    // section activation 0x101B60 (web/section_gameplay.inc): the end of the rider manager, after every rider
    section_pass();
    M.stage = 0;
    M.tick++;
    if (M.returnTick >= 0) {
        M.returnTick++;
    }
}
// pv coreRelationships: the race's records and levels (web/ai-race.js / compare-ai-capture.mjs at each start). spec (int32):
// [enabled, characters x 6, banks x 6, rival, the tables 3 x 10 x 10 x (kind, level, score), the levels 6 x 6, the kinds 6 x 6 (-1
// unknown)]. Returns 0 (nothing loaded: the page keeps them) for characters or banks out of range.
EMSCRIPTEN_KEEPALIVE int manager_relationships(const int32_t* spec) {
    Relationships& R = manager.rel;
    R = Relationships{};
    const int32_t* p = spec;
    R.enabled = *p++ != 0;
    for (int k = 0; k < 6; ++k) {
        R.characters[k] = *p++;
    }
    for (int k = 0; k < 6; ++k) {
        R.banks[k] = *p++;
    }
    R.rival = *p++;
    for (int k = 0; k < 6; ++k) {
        if (R.characters[k] < 0 || R.characters[k] > 9 || R.banks[k] < 0 || R.banks[k] > 2) {
            R = Relationships{};
            return 0;
        }
    }
    for (int b = 0; b < 3; ++b) {
        for (int c = 0; c < 10; ++c) {
            for (int o = 0; o < 10; ++o) {
                for (int f = 0; f < 3; ++f) {
                    R.table[b][c][o][f] = int8_t(*p++);
                }
            }
        }
    }
    for (int a = 0; a < 6; ++a) {
        for (int b = 0; b < 6; ++b) {
            R.scores[a][b] = float(*p++);
        }
    }
    for (int a = 0; a < 6; ++a) {
        for (int b = 0; b < 6; ++b) {
            R.kinds[a][b] = *p++;
        }
    }
    R.loaded = true;
    return 1;
}
// The page's relationships again (the reactions call it back, ai-racers.js onPairAudio / onReact).
EMSCRIPTEN_KEEPALIVE void manager_relationships_off() {
    manager.rel = Relationships{};
}
// After a tick: [count, then per reaction: slot, other, kind, attack, audio, notice, score]; the queue starts empty each tick.
EMSCRIPTEN_KEEPALIVE int32_t* manager_relationship_events() {
    RIDER_LOCAL static int32_t out[1 + 7 * kRelationshipEvents];
    const Relationships& R = manager.rel;
    out[0] = int32_t(R.events);
    for (unsigned k = 0; k < R.events; ++k) {
        const RelationshipEvent& e = R.event[k];
        int32_t* q = out + 1 + 7 * k;
        q[0] = e.slot;
        q[1] = e.other;
        q[2] = e.kind;
        q[3] = e.attack;
        q[4] = e.audio;
        q[5] = e.notice;
        q[6] = e.score;
    }
    return out;
}
// The records and levels as they stand: the tables 3 x 10 x 10 x 3, the levels 6 x 6, the kinds 6 x 6 (int32).
EMSCRIPTEN_KEEPALIVE int32_t* manager_relationship_state() {
    RIDER_LOCAL static int32_t out[900 + 72];
    const Relationships& R = manager.rel;
    int32_t* p = out;
    for (int b = 0; b < 3; ++b) {
        for (int c = 0; c < 10; ++c) {
            for (int o = 0; o < 10; ++o) {
                for (int f = 0; f < 3; ++f) {
                    *p++ = R.table[b][c][o][f];
                }
            }
        }
    }
    for (int a = 0; a < 6; ++a) {
        for (int b = 0; b < 6; ++b) {
            *p++ = int32_t(R.scores[a][b]);
        }
    }
    for (int a = 0; a < 6; ++a) {
        for (int b = 0; b < 6; ++b) {
            *p++ = R.kinds[a][b];
        }
    }
    return out;
}
}

namespace {
using nlohmann::json;
// A JSON value as the page's JS number of it (Float32Array.set / HEAPF32.set coerce so): absent (undefined) NaN, null 0, a bool 1 / 0,
// a number itself, anything else NaN.
double jsNumber(const json* v) {
    if (!v) {
        return NAN;
    }
    if (v->is_null()) {
        return 0;
    }
    if (v->is_boolean()) {
        return v->get<bool>() ? 1 : 0;
    }
    if (v->is_number()) {
        return v->get<double>();
    }
    return NAN;
}
const json* member(const json& o, const char* key) {
    if (!o.is_object()) {
        return nullptr;
    }
    const auto it = o.find(key);
    return it == o.end() ? nullptr : &*it;
}
const json* element(const json* a, unsigned k) {
    if (!a || !a->is_array() || k >= a->size()) {
        return nullptr;
    }
    return &(*a)[k];
}
// JS truthiness of a JSON value (an object or a nonzero number / true / a non-empty string)
bool truthy(const json* v) {
    if (!v || v->is_null()) {
        return false;
    }
    if (v->is_boolean()) {
        return v->get<bool>();
    }
    if (v->is_number()) {
        const double d = v->get<double>();
        return d != 0 && !std::isnan(d);
    }
    if (v->is_string()) {
        return !v->get<std::string>().empty();
    }
    return true;
}
// `a ?? b` for a JSON value: null and absent take b
const json* defined(const json* v) {
    return v && !v->is_null() ? v : nullptr;
}

// The race world's reset (ai-racers.js world.reset / proximity): 10F560's records from the document, their distance / bearing from
// the riders' grid positions (one race_world_frame at tick 0), the ranks, the relationship levels, then the pair inputs.
void worldReset(RiderManager& M, const json& world, const json& relationships, bool isolate, const int32_t* ranks) {
    const json* records = member(world, "records");
    const double tail = jsNumber(member(world, "tail"));
    const double rankMode = jsNumber(member(world, "rank_mode"));
    const int count = M.count;
    std::vector<float> init;
    // proximity: every record from the grid positions
    init = {float(count), float(tail), float(rankMode)};
    for (int a = 0; a < 6; ++a) {
        for (int b = 0; b < 6; ++b) {
            const float on = a != b && a < count && b < count ? 1.f : 0.f;
            init.insert(init.end(), {on, 0.f, float(1e10), 0.f, 0.f, 0.f, 0.f, 0.f, 0.f});
        }
        init.push_back(float(a));
    }
    init.insert(init.end(), 36, 0.f);
    race_world_reset(init.data());
    std::fill(std::begin(M.riderBuffer), std::end(M.riderBuffer), 0.f);
    for (unsigned k = 0; k < M.riders; ++k) {
        const float* st = liveState(M.rider[k]);
        float* b = M.riderBuffer + M.rider[k].slot * 6;
        b[0] = st[0];
        b[1] = st[1];
        b[2] = st[2];
        b[3] = float(1e5);
        b[4] = 0;
        b[5] = 1;
    }
    race_world_frame(0, M.riderBuffer);
    float near[6][6][2] = {};
    {
        const float* w = race_world_state();
        for (int a = 0; a < 6; ++a) {
            for (int b = 0; b < 6; ++b) {
                near[a][b][0] = w[(a * 6 + b) * 10 + 2];
                near[a][b][1] = w[(a * 6 + b) * 10 + 3];
            }
        }
    }
    // the document's records with those distances
    init = {float(count), float(tail), float(rankMode)};
    const json* docRanks = member(world, "ranks");
    for (int a = 0; a < 6; ++a) {
        for (int b = 0; b < 6; ++b) {
            const json* r = a < count && b < count ? defined(element(element(records, unsigned(a)), unsigned(b))) : nullptr;
            double enabled = r ? jsNumber(member(*r, "enabled")) : 0;
            if (isolate && (a == 0 || b == 0) && a != b) {
                enabled = 0;
            }
            double distance = 0;
            double bearing = 0;
            if (r && a != b) {
                distance = near[a][b][0];
                bearing = near[a][b][1];
            } else {
                distance = r ? jsNumber(member(*r, "distance")) : 1e10;
                bearing = r ? jsNumber(member(*r, "bearing")) : 0;
            }
            auto field = [&](const char* key) { return r ? float(jsNumber(member(*r, key))) : 0.f; };
            init.insert(init.end(), {float(enabled), field("human"), float(distance), float(bearing), field("t10"), field("t14"), field("t18"), field("t1c"),
                                     field("t20")});
        }
        double rank = a;
        if (ranks) {
            rank = ranks[a];
        } else if (const json* v = defined(element(docRanks, unsigned(a)))) {
            rank = jsNumber(v);
        }
        init.push_back(float(rank));
    }
    const json* scores = member(relationships, "scores");
    for (int a = 0; a < 6; ++a) {
        for (int b = 0; b < 6; ++b) {
            const json* v = defined(element(element(scores, unsigned(a)), unsigned(b)));
            init.push_back(v ? float(jsNumber(v)) : 0.f);
        }
    }
    race_world_reset(init.data());
    std::vector<float> pairs;
    const json* inputs = member(world, "pair_inputs");
    for (unsigned s = 0; s < 6; ++s) {
        const json* w = element(inputs, s);
        const bool has = truthy(w);
        pairs.push_back(has ? float(jsNumber(member(*w, "weight_attribute"))) : 0.f);
        pairs.push_back(has ? float(jsNumber(member(*w, "collision_stat"))) : 0.f);
        pairs.push_back(has ? float(jsNumber(member(*w, "attack_stat"))) : 0.f);
    }
    pairs.push_back(truthy(member(world, "knockdown_cheat")) ? 1.f : 0.f);
    race_world_pair_setup(pairs.data());
}
}  // namespace

extern "C" {
// pv coreEventStart (docs/core-migration.md M4b): the computer riders' event start (ai-racers.js start): each rider's provider set
// up again from its record (npc_configure; the event reloads on a restart), its stat bytes (attribute bank 2) when they are not the
// seeds', its pad history settled, its start (npc_start_event, or with gridStart the grid placement npc_grid_start, under hold its
// event clock held in PreRace), then the manager's tick and counts, the race world's reset from the document, the previous run's
// world events dropped. riders: each computer rider's record text (JSON); context: the provider's document text; world: {world,
// relationships} text; ranks: 6 ranks or null. In the human's context.
EMSCRIPTEN_KEEPALIVE void manager_event_start(const char* const* riders, const char* context, const char* worldText, int gridStart, int hold, int isolate,
                                              const int32_t* ranks) {
    RiderManager& M = manager;
    for (unsigned k = 1; k < M.riders; ++k) {
        const char* text = riders[k - 1];
        const json record = json::parse(text);
        inContext(M.rider[k].block, [&] {
            npc_configure(text, context);
            // the rider's own stat getters (0x1494C0..: attribute bank 2 for computer riders): only when they are not all 5
            const json* a = member(record, "attributes");
            const json* raw = a ? member(*a, "raw") : nullptr;
            if (raw && raw->is_array() && raw->size() == 7) {
                bool seeds = true;
                int32_t bytes[7];
                for (unsigned j = 0; j < 7; ++j) {
                    const double v = jsNumber(&(*raw)[j]);
                    if (!(v == 5)) {
                        seeds = false;
                    }
                    bytes[j] = ssx::rules::jsToInt32(v);
                }
                if (!seeds) {
                    set_rider_attributes(bytes, ssx::rules::jsToInt32(jsNumber(member(*a, "override"))));
                }
            }
            reset_pad_history();
            if (hold) {
                event_clock_hold(1);
            }
            try {
                if (gridStart) {
                    npc_grid_start();
                } else {
                    npc_start_event();
                }
            } catch (...) {
                if (hold) {
                    event_clock_hold(0);
                }
                throw;
            }
            if (hold) {
                event_clock_hold(0);
            }
        });
        M.rider[k].finished = false;
    }
    manager_start();
    const json doc = json::parse(worldText);
    const json* world = member(doc, "world");
    const json* relationships = member(doc, "relationships");
    worldReset(M, world ? *world : json::object(), relationships ? *relationships : json::object(), isolate != 0, ranks);
    for (unsigned k = 0; k < M.riders; ++k) {
        inContext(M.rider[k].block, [] { world_events(); });
    }
}
// [tick, stage, returnTick, worldEvents, the pair counts x 5, the event kinds x 16]
EMSCRIPTEN_KEEPALIVE float* manager_counts() {
    RIDER_LOCAL static float out[25];
    const RiderManager& M = manager;
    out[0] = float(M.tick);
    out[1] = float(M.stage);
    out[2] = float(M.returnTick);
    out[3] = float(M.worldEvents);
    for (unsigned k = 0; k < 5; ++k) {
        out[4 + k] = M.pairCounts[k];
    }
    for (unsigned k = 0; k < kEventKinds; ++k) {
        out[9 + k] = float(M.worldEventKinds[k]);
    }
    return out;
}
// Rider k's last provider command (24), its info (9) and its finish flag.
EMSCRIPTEN_KEEPALIVE float* manager_rider(int k) {
    RIDER_LOCAL static float out[34];
    const Rider& r = manager.rider[k];
    std::memcpy(out, r.command, sizeof r.command);
    std::memcpy(out + 24, r.info, sizeof r.info);
    out[33] = r.finished ? 1.f : 0.f;
    return out;
}
}

#ifdef SSX_SNAPSHOT_REGISTRY  // the rider-context snapshot's registry (web/generate-snapshot-registry.mjs, docs/replay.md §2a)
#include "generated/snapshot/rider_manager.inc"
#endif
