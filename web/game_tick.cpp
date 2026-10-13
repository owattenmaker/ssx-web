// The game tick's core sequence (docs/core-migration.md slice M2, pv coreTick): the core calls web/game-tick.js simulate
// made, in its order, as a few exports the page calls with its own state (the host state stays in JS for now):
//   tick_begin   pad_tick, race_begin, step_rider, a rescue's reset_animation, animation_tick and its reads, race_end, the
//                stage-teleport re-read (one PS2 game update up to the rider pass 121818); also as its four parts, tick_input,
//                race_begin, tick_rider and tick_progress
//   tick_mid     a solo run's rider FX passes and section activation 0x101B60 after 121818 (with computer riders the rider
//                manager runs them: web/ai-racers.js endTick)
//   tick_end     the camera (DEFAULT_3 after every rider pass, 0x230D7C), a replay's camera, the trick name, the finish reads
//   tick_finish  the finish celebration rule (rider +0x100; 23B8C8 / 23BDB8, 0x23A760 places, 0x239230 freestyle)
//   tick_results the results timing: EndRace's results 408 ticks after the finish, 288 after TIME'S UP, the live stop at 409, and
//                pv finishSkip's Cross edge (0x321298: an edge, then three updates that ignore the pad)
// Between the calls the page runs its own hooks (free ride, Big Challenges, the NIS hold, the computer riders, online races) as
// before. Every call here is one the page made itself, with the same arguments, so the results are the same with the switch
// off and on (docs/core-migration.md "M2").
#include "game_rules.hpp"
#include <emscripten/emscripten.h>
#include "rider_local.hpp"
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

extern "C" {
float* pad_tick(const float* values);
void race_begin();
float* step_rider(float steering, int jump, int brake, int boost);
void reset_animation();
float* animation_tick(float speed, float turn, float braking, float charge, int grounded, int jumpHeld, int mask, int tweak, int boost,
                      float predictedLanding, float impact, float flip);
float* animation_info();
float* pose_physical();
float* rider_state();
float* reset_info();
float* race_end();
float* stage_teleport_info();
int fx_pass(int phase);
int section_pass();
float* step_camera_head(float x, float y, float z);
float* replay_camera_step(int paused, float mx, float my, float mz);
const char* trick_name();
float* race_result_info();
uint32_t* score_object_dump();
int race_timed_out();
void finish_celebrate(int celebrate);
}

namespace {
// A float argument to an int parameter as the JS call made it (ToInt32 of the number).
int jsInt(float x) {
    return ssx::rules::jsToInt32(double(x));
}
// JS truthiness of a float (0 and NaN are false).
bool truthy(float x) {
    return x != 0 && !std::isnan(x);
}
float bitsOf(const void* p) {
    return std::bit_cast<float>(uint32_t(reinterpret_cast<uintptr_t>(p)));
}
float bitsOf(uint32_t v) {
    return std::bit_cast<float>(v);
}
}  // namespace

// tick_begin's record (floats; pointers and counts as their uint32 bits where noted):
//   [0..23] the pad_tick command   [24..39] the rider state (the step's, the animation tick's, or a teleport's)
//   [40..58] animation_info        [59..70] pose_physical (animated, or re-read after a teleport)   [71..78] race_end
//   [79] flags: 1 rescue, 2 placement, 4 animated, 8 pose re-read after a teleport, 16 teleport count known
//   [80] lastRescues out  [81] lastResetPlacement out  [82] the teleport count  [83] the pose copy (pointer bits, boneCount x 7)
// tick_begin is tick_input, race_begin, tick_rider and tick_progress in a row; a comparer with its own reads between them calls
// the four itself (the same code).
namespace {
float* tickRecord() {
    RIDER_LOCAL static float out[96];
    return out;
}
std::vector<float>& tickPose() {
    RIDER_LOCAL_LAZY static std::vector<float> pose;
    return pose;
}
uint32_t recordFlags() {
    return std::bit_cast<uint32_t>(tickRecord()[79]);
}
void setRecordFlags(uint32_t flags) {
    tickRecord()[79] = bitsOf(flags);
}
}  // namespace

// One consumed pad sample: the provider 0x127998 and the controller's command words (pad_tick). Starts a new record.
extern "C" EMSCRIPTEN_KEEPALIVE float* tick_input(const float* pad) {
    float* out = tickRecord();
    std::memset(out, 0, 96 * sizeof(float));
    std::memcpy(out, pad_tick(pad), 24 * sizeof(float));
    return out;
}

// The rider's motion and pose (121068..121728 for this rider): step_rider, a rescue's reset_animation, animation_tick and the reads
// after it. command: pad_tick's 24 floats (tick_begin: the record's own). tweakWord: 1 the command's tweak word as animation_tick's
// tweak (pv padTweak), 0 the boost word (the page before it).
extern "C" EMSCRIPTEN_KEEPALIVE float* tick_rider(const float* command, float lastRescues, float lastResetPlacement, int animationReady, int boneCount,
                                                  int tweakWord) {
    float* out = tickRecord();
    float cmd[24];
    std::memcpy(cmd, command, sizeof cmd);
    uint32_t flags = recordFlags();
    float state[16];
    std::memcpy(state, step_rider(cmd[0], jsInt(cmd[6]), truthy(cmd[2]) ? 1 : 0, jsInt(cmd[7])), sizeof state);
    // a rescue (the forced reset's count, state [13]): the page's animation restarts (web/game-tick.js)
    if (!(state[13] == lastRescues)) {
        lastRescues = state[13];
        flags |= 1;
        if (animationReady) {
            reset_animation();
        }
    }
    if (animationReady) {
        const int tweak = tweakWord ? jsInt(cmd[12]) : jsInt(cmd[7]);
        const float* p = animation_tick(state[7], cmd[10], cmd[11], state[9], jsInt(state[8]), jsInt(cmd[6]), jsInt(cmd[8]), tweak, jsInt(cmd[7]), 0,
                                        state[15], cmd[13]);
        tickPose().assign(p, p + size_t(boneCount > 0 ? boneCount : 0) * 7);
        std::memcpy(out + 40, animation_info(), 19 * sizeof(float));
        std::memcpy(out + 59, pose_physical(), 12 * sizeof(float));
        std::memcpy(state, rider_state(), sizeof state);
        const float placements = reset_info()[2];
        if (!(placements == lastResetPlacement)) {
            lastResetPlacement = placements;
            flags |= 2;
        }
        flags |= 4;
    }
    std::memcpy(out + 24, state, sizeof state);
    out[80] = lastRescues;
    out[81] = lastResetPlacement;
    out[83] = bitsOf(tickPose().data());
    setRecordFlags(flags);
    return out;
}

// The rider's course progress 121818 (race_end: the course events, the finish 0x125108, the stage triggers), then the
// stage-teleport re-read: builtin 34 (a Metro-City phone booth / water tower) placed the rider inside race_end.
extern "C" EMSCRIPTEN_KEEPALIVE float* tick_progress(int teleportKnown, float teleportCount) {
    float* out = tickRecord();
    uint32_t flags = recordFlags();
    std::memcpy(out + 71, race_end(), 8 * sizeof(float));
    const float n = stage_teleport_info()[0];
    if (!teleportKnown || !(n == teleportCount)) {
        if (teleportKnown && n > 0) {
            flags |= 2 | 8;
            out[81] = reset_info()[2];
            std::memcpy(out + 24, rider_state(), 16 * sizeof(float));
            std::memcpy(out + 59, pose_physical(), 12 * sizeof(float));
        }
        teleportCount = n;
    }
    flags |= 16;
    out[82] = teleportCount;
    setRecordFlags(flags);
    return out;
}

extern "C" EMSCRIPTEN_KEEPALIVE float* tick_begin(const float* pad, float lastRescues, float lastResetPlacement, int animationReady, int boneCount,
                                                  int teleportKnown, float teleportCount, int tweakWord) {
    tick_input(pad);
    race_begin();
    tick_rider(tickRecord(), lastRescues, lastResetPlacement, animationReady, boneCount, tweakWord);
    return tick_progress(teleportKnown, teleportCount);
}

// A solo run's passes after 121818 (web/game-tick.js: no computer riders): the rider FX pass, then section activation.
extern "C" EMSCRIPTEN_KEEPALIVE void tick_mid() {
    fx_pass(-1);
    section_pass();
}

// tick_end's record: [0] status (0, or 1 "Missing posed physical frame": nothing stepped) [1..9] the camera [10..19] a replay's
// camera [20] flags: 1 camera, 2 replay camera, 4 animation read, 8 finish read [21] pending [22] the trick name (pointer bits)
// [23] the finish score (+0x198, int32 bits) [24] race_result_info [1] (ticks) [25] timed out [26] the score object (pointer bits)
extern "C" EMSCRIPTEN_KEEPALIVE float* tick_end(int hasAnimPose, float pose7, float headX, float headY, float headZ, int replayCamera, float mx, float my,
                                                float mz, int animationReady, int finished) {
    RIDER_LOCAL static float out[32];
    std::memset(out, 0, sizeof out);
    uint32_t flags = 0;
    if (hasAnimPose) {
        if (!truthy(pose7)) {
            out[0] = 1;
            return out;
        }
        std::memcpy(out + 1, step_camera_head(headX, headY, headZ), 9 * sizeof(float));
        flags |= 1;
        // a replay draws through its replay camera (web/replay_camera.inc), stepped from this tick's camera input
        if (replayCamera) {
            std::memcpy(out + 10, replay_camera_step(0, mx, my, mz), 10 * sizeof(float));
            flags |= 2;
        }
    }
    if (animationReady) {
        out[21] = animation_info()[12];
        out[22] = bitsOf(trick_name());
        flags |= 4;
    }
    if (finished) {
        const float* r = race_result_info();
        const uint32_t* dump = score_object_dump();
        out[23] = bitsOf(dump[0x198 / 4]);
        out[24] = r[1];
        out[25] = float(race_timed_out());
        out[26] = bitsOf(dump);
        flags |= 8;
    }
    out[20] = bitsOf(flags);
    return out;
}

// The finish celebration (rider +0x100): a freestyle run (the page has its place) was given its place by race_end itself
// (finish_standing, 0x239230), so nothing more; a race with computer riders celebrates in the top three (the backcountry rival:
// only the winner; 23B8C8 / 23BDB8, 0x23A760 for a human placing 4th or worse).
extern "C" EMSCRIPTEN_KEEPALIVE void tick_finish(int freestyle, int aiActive, float aiPlace, int backcountry) {
    if (freestyle) {
        return;
    }
    if (aiActive) {
        finish_celebrate(aiPlace < (backcountry ? 1 : 3) ? 1 : 0);
    }
}

// The results timing (web/game-tick.js). in: [0] race_end [3] (EndRace), [1] resultsPending, [2] finished, [3] postFinishTicks,
// [4] Cross held, [5] Cross age, [6] Cross down this tick, [7] pv finishSkip, [8] timed out, [9] the live stop (-1: 409).
// out (over the same floats): [0] resultsPending, [1] Cross held, [2] Cross age, [3] show the results now, [4] postFinishTicks,
// [5] the run stops.
extern "C" EMSCRIPTEN_KEEPALIVE void tick_results(float* io) {
    constexpr int kResultsFinish = 408;
    constexpr int kResultsTimeUp = 288;
    constexpr int kPostFinish = 409;
    constexpr int kSkipFrom = 228;
    bool pending = io[1] != 0;
    const bool finished = io[2] != 0;
    double post = io[3];
    int held = int(io[4]);
    int age = int(io[5]);
    const bool timedOut = io[8] != 0;
    if (truthy(io[0])) {
        pending = true;
    }
    bool pressed = false;
    if (age < 3) {
        age++;
    } else {
        const int down = io[6] != 0 ? 1 : 0;
        if (down != held) {
            held = down;
            pressed = held == 1;
            age = 0;
        }
    }
    const bool skip = pressed && pending && finished && post + 1 >= kSkipFrom && io[7] != 0;
    bool show = false;
    if (pending && finished && (skip || post + 1 >= (timedOut ? kResultsTimeUp : kResultsFinish))) {
        pending = false;
        show = true;
    }
    bool stop = false;
    if (finished) {
        post += 1;
        const double stopAt = io[9] >= 0 ? io[9] : kPostFinish;
        stop = post >= stopAt;
    }
    io[0] = pending ? 1 : 0;
    io[1] = float(held);
    io[2] = float(age);
    io[3] = show ? 1 : 0;
    io[4] = float(post);
    io[5] = stop ? 1 : 0;
}
