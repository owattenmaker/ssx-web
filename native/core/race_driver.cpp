// A native race (docs/core-migration.md "Native core"): the course core built for macOS arm64, driven headless the way the page
// drives the wasm build (web/test-core-tick.mjs makeGame and web/game-tick.js simulateInCore with pv coreTick, the core's rider
// manager and its event start): Snow Jam, the human (Zoe) alone or with the course's five computer riders, a scripted pad, a pause
// Give Up. Prints one line per tick: the tick and the page's ?simtrace hash (web/game-tick.js gameTrace) of that tick, which a wasm
// run of the same script prints too (local/core-migration/qa/ref-trace.mjs), so the two builds can be compared tick for tick.
//   race_driver ASSETS_DIR [--ai] [--ticks N] [--give-up T]
#include "json.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using nlohmann::json;
void native_run_in_context(void* block, const std::function<void()>& fn);

extern "C" {
void* rider_context_create();
void init_animation(const char* metadata, const char* rig, const char* settings, const uint8_t* packets, uint32_t size);
void init_race(const char* text);
void animation_use_physics(int on);
void init_world(const float* triangles, uint32_t count);
void init_terrain(const char* text);
void init_world_collision(const char* text, const char* hash);
void init_body_terrain(const char* text);
void init_rails(const char* text, const char* hash);
void npc_configure(const char* rider, const char* document);
void set_rider_attributes(const int32_t* raw, int32_t override);
void rider_host(int flags);
void rider_parse_cache_clear();
void manager_configure(void* const* blocks, const int* slots, int riders, int count, int flags);
void manager_event_start(const char* const* riders, const char* context, const char* world, int gridStart, int hold, int isolate, const int32_t* ranks);
float* manager_begin_tick(int anchorTick, int scores);
void manager_end_tick();
void set_fx_deferred(int on);
void reset_pad_history();
void start_event();
float* animation_tick(float, float, float, float, int, int, int, int, int, float, float, float);
float* animation_info();
float* pose_physical();
float* rider_state();
float* rider_world_state();
uint32_t* visual_rng_words();
uint32_t* animation_rng_words();
uint32_t* score_object_dump();
float* tick_begin(const float* pad, float lastRescues, float lastResetPlacement, int animationReady, int boneCount, int teleportKnown, float teleportCount,
                  int tweakWord);
void tick_mid();
float* tick_end(int hasAnimPose, float pose7, float hx, float hy, float hz, int replayCamera, float mx, float my, float mz, int animationReady, int finished);
void tick_finish(int freestyle, int aiActive, float aiPlace, int backcountry);
void tick_results(float* io);
int race_timed_out();
float* race_result_info();
void race_give_up();
}

namespace {
std::string readText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw std::runtime_error("cannot read " + path);
    }
    std::ostringstream s;
    s << f.rdbuf();
    return s.str();
}
// ai-racers.js mergeSettings: the rider's settings over the course's (one level deep for objects)
json mergeSettings(const json& initial, const json& overrides) {
    json out = initial;
    for (auto it = overrides.begin(); it != overrides.end(); ++it) {
        const auto base = initial.find(it.key());
        if (base != initial.end() && base->is_object()) {
            json merged = *base;
            for (auto j = it.value().begin(); j != it.value().end(); ++j) {
                merged[j.key()] = j.value();
            }
            out[it.key()] = merged;
        } else {
            out[it.key()] = it.value();
        }
    }
    return out;
}
uint32_t fnv(uint32_t h, const uint32_t* w, size_t n) {
    for (size_t k = 0; k < n; ++k) {
        h = (h ^ w[k]) * 16777619u;
    }
    return h;
}
// web/test-core-tick.mjs padAt
void padAt(int t, float* p) {
    std::memset(p, 0, 24 * sizeof(float));
    p[23] = 1;
    const int k = t % 240;
    if (k >= 150 && k < 180) {
        p[10] = 1;
    }
    if (k >= 185 && k < 215) {
        p[13] = 1;
    }
    if (t % 600 < 120) {
        p[21] = 1;
    }
    if (t == 1500 || t == 2500) {
        p[0] = 1;
    }
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            throw std::runtime_error("usage: race_driver ASSETS_DIR [--ai] [--ticks N] [--give-up T]");
        }
        const std::string root = std::string(argv[1]) + "/";
        bool ai = false;
        int ticks = 3000;
        int giveUp = -1;
        for (int k = 2; k < argc; ++k) {
            const std::string a = argv[k];
            if (a == "--ai") {
                ai = true;
            } else if (a == "--ticks" && k + 1 < argc) {
                ticks = std::stoi(argv[++k]);
            } else if (a == "--give-up" && k + 1 < argc) {
                giveUp = std::stoi(argv[++k]);
            }
        }
        const std::string packetsJson = readText(root + "ANIMATIONS/animation-packets.json");
        const std::string packetsBin = readText(root + "ANIMATIONS/animation-packets.bin");
        const std::string initialText = readText(root + "ANIMATIONS/initial.json");
        const std::string terrainText = readText(root + "ARA1/terrain.json");
        const std::string worldText = readText(root + "ARA1/world_collision.json");
        const std::string railsText = readText(root + "ARA1/rails.json");
        const std::string collision = readText(root + "ARA1/collision.bin");
        const std::string rig = readText(root + "RIDER_ZOE/rider.json");
        const std::string hash = json::parse(terrainText)["source_sha256"].get<std::string>();
        const auto* packets = reinterpret_cast<const uint8_t*>(packetsBin.data());
        // the human (makeGame): FX deferred as main.js sets it, then the course
        set_fx_deferred(1);
        init_animation(packetsJson.c_str(), rig.c_str(), initialText.c_str(), packets, uint32_t(packetsBin.size()));
        init_race(initialText.c_str());
        animation_use_physics(1);
        std::vector<float> triangles(collision.size() / 4);
        std::memcpy(triangles.data(), collision.data(), triangles.size() * 4);
        init_world(triangles.data(), uint32_t(triangles.size()));
        init_terrain(terrainText.c_str());
        init_world_collision(worldText.c_str(), hash.c_str());
        init_body_terrain(terrainText.c_str());
        init_rails(railsText.c_str(), hash.c_str());
        // the computer riders (ai-racers.js createAiRacers: rider contexts, attachWorld, configureRider, the manager)
        std::vector<void*> contexts;
        json doc;
        std::vector<std::string> riderTexts;
        int count = 1;
        if (ai) {
            doc = json::parse(readText(root + "ARA1/npc-riders.json"));
            const json initial = json::parse(initialText);
            count = std::min<int>(6, int(doc["riders"].size()) + 1);
            json context = json::object();
            if (doc.contains("event_variant")) {
                context["event_variant"] = doc["event_variant"];
            }
            if (doc.contains("relationships")) {
                context["relationships"] = doc["relationships"];
            }
            if (doc.contains("anchor_tick")) {
                context["anchor_tick"] = doc["anchor_tick"];
            }
            const std::string contextText = context.dump();
            for (int k = 0; k + 1 < count; ++k) {
                const json& rider = doc["riders"][k];
                const std::string riderText = readText(root + rider["package"].get<std::string>() + "/rider.json");
                const std::string settings = mergeSettings(initial, rider.value("settings", json::object())).dump();
                riderTexts.push_back(rider.dump());
                void* c = rider_context_create();
                const std::string& record = riderTexts.back();
                native_run_in_context(c, [&] {
                    init_race(initialText.c_str());
                    animation_use_physics(1);
                    init_world_collision(worldText.c_str(), hash.c_str());
                    init_body_terrain(terrainText.c_str());
                    init_rails(railsText.c_str(), hash.c_str());
                    init_animation(packetsJson.c_str(), riderText.c_str(), settings.c_str(), packets, uint32_t(packetsBin.size()));
                    npc_configure(record.c_str(), contextText.c_str());
                    // ai-racers.js configureNpc: the stat bytes (attribute bank 2) when they are not all 5
                    const json* a = rider.contains("attributes") ? &rider["attributes"] : nullptr;
                    if (a && a->contains("raw") && (*a)["raw"].is_array() && (*a)["raw"].size() == 7) {
                        int32_t raw[7];
                        bool seeds = true;
                        for (unsigned j = 0; j < 7; ++j) {
                            raw[j] = (*a)["raw"][j].get<int32_t>();
                            if (raw[j] != 5) {
                                seeds = false;
                            }
                        }
                        if (!seeds) {
                            set_rider_attributes(raw, a->value("override", 0));
                        }
                    }
                });
                contexts.push_back(c);
            }
            rider_host(3 | 8);
            rider_parse_cache_clear();
            for (void* c : contexts) {
                native_run_in_context(c, [] { rider_host(2 | 4 | 8); });
            }
            std::vector<void*> blocks{nullptr};
            std::vector<int> slots{0};
            for (int k = 0; k + 1 < count; ++k) {
                blocks.push_back(contexts[k]);
                slots.push_back(doc["riders"][k]["slot"].get<int>());
            }
            manager_configure(blocks.data(), slots.data(), count, count, 1 | 2);
        }
        reset_pad_history();
        start_event();
        if (ai) {
            json world = json::object();
            world["world"] = doc["world"];
            world["relationships"] = doc["relationships"];
            const std::string worldDoc = world.dump();
            json context = json::object();
            for (const char* key : {"event_variant", "relationships", "anchor_tick"}) {
                if (doc.contains(key)) {
                    context[key] = doc[key];
                }
            }
            const std::string contextText = context.dump();
            std::vector<const char*> texts;
            for (const auto& t : riderTexts) {
                texts.push_back(t.c_str());
            }
            manager_event_start(texts.data(), contextText.c_str(), worldDoc.c_str(), 0, 0, 0, nullptr);
        }
        const int boneCount = int(json::parse(rig)["bones"].size());
        // makeGame's animation_tick(0, 0, 0, 0, 1, 0, 0, 0, 0, 0): the JS call leaves the last two arguments undefined, NaN as floats
        const float* pose0 = animation_tick(0, 0, 0, 0, 1, 0, 0, 0, 0, 0, NAN, NAN);
        (void)pose0;
        // the host state of web/game-tick.js (gameHost)
        float state[16];
        std::memcpy(state, rider_state(), sizeof state);
        float posed[12];
        std::memcpy(posed, pose_physical(), sizeof posed);
        float raceInfo[8] = {};
        float lastRescues = state[13];
        float lastResetPlacement = 0;
        bool teleportKnown = false;
        float teleportCount = 0;
        bool finished = false, resultsPending = false, running = true;
        double postFinishTicks = 0;
        float crossHeld = 0, crossAge = 0;
        const int anchorTick = ai && doc.contains("anchor_tick") && doc["anchor_tick"].is_number_integer() ? doc["anchor_tick"].get<int>() : -1;
        const int scores = ai && doc["world"].value("rank_mode", 0) == 2 ? 1 : 0;
        float worldState[396] = {};
        for (int t = 0; t < ticks && running; ++t) {
            if (t == giveUp) {
                race_give_up();
            }
            float pad[24];
            padAt(t, pad);
            if (ai) {
                std::memcpy(worldState, manager_begin_tick(anchorTick, scores), sizeof worldState);
            }
            const float* b = tick_begin(pad, lastRescues, lastResetPlacement, 1, boneCount, teleportKnown ? 1 : 0, teleportKnown ? teleportCount : 0, 0);
            uint32_t flags;
            std::memcpy(&flags, b + 79, 4);
            if (flags & 1) {
                lastRescues = b[80];
            }
            if (flags & 4) {
                std::memcpy(posed, b + 59, sizeof posed);
            }
            std::memcpy(state, b + 24, sizeof state);
            std::memcpy(raceInfo, b + 71, sizeof raceInfo);
            if (flags & 2) {
                lastResetPlacement = b[81];
            }
            if (flags & 8) {
                std::memcpy(posed, b + 59, sizeof posed);
            }
            teleportKnown = true;
            teleportCount = b[82];
            if (ai) {
                manager_end_tick();
            } else {
                tick_mid();
            }
            const float* e = tick_end(1, posed[7], posed[9], posed[10], posed[11], 0, 0, 0, 0, 1, raceInfo[2] != 0 ? 1 : 0);
            if (e[0] == 1) {
                throw std::runtime_error("Missing posed physical frame");
            }
            uint32_t endFlags;
            std::memcpy(&endFlags, e + 20, 4);
            float camera[9] = {};
            if (endFlags & 1) {
                std::memcpy(camera, e + 1, sizeof camera);
            }
            if (endFlags & 8) {
                finished = true;
                postFinishTicks = 0;
                tick_finish(0, ai ? 1 : 0, ai ? worldState[360] : 0, 0);
            }
            const bool timedOut = race_timed_out() != 0;
            float io[10] = {raceInfo[3], resultsPending ? 1.f : 0.f, finished ? 1.f : 0.f, float(postFinishTicks), crossHeld, crossAge, pad[10] > 0 ? 1.f : 0.f,
                            0.f, timedOut ? 1.f : 0.f, -1.f};
            tick_results(io);
            resultsPending = io[0] != 0;
            crossHeld = io[1];
            crossAge = io[2];
            if (finished) {
                postFinishTicks = io[4];
            }
            if (io[5] != 0) {
                running = false;
            }
            // web/game-tick.js gameTrace over traceOut (state 16, posed 12, race info 8, camera 9), then every rider
            uint32_t h = 0x811c9dc5u;
            float out[45] = {};
            std::memcpy(out, state, 16 * 4);
            std::memcpy(out + 16, posed, 12 * 4);
            std::memcpy(out + 28, raceInfo, 8 * 4);
            std::memcpy(out + 36, camera, 9 * 4);
            h = fnv(h, reinterpret_cast<const uint32_t*>(out), 45);
            auto riderHash = [&] {
                h = fnv(h, reinterpret_cast<const uint32_t*>(rider_state()), 16);
                h = fnv(h, reinterpret_cast<const uint32_t*>(pose_physical()), 12);
                h = fnv(h, reinterpret_cast<const uint32_t*>(rider_world_state()), 8);
            };
            riderHash();
            for (void* c : contexts) {
                native_run_in_context(c, riderHash);
            }
            h = fnv(h, visual_rng_words(), 6);
            h = fnv(h, animation_rng_words(), 6);
            h = fnv(h, score_object_dump(), 0x1d0 / 4);
            std::printf("%d %u\n", t, h);
        }
        std::fflush(stdout);
        std::_Exit(0);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "race_driver: %s\n", e.what());
        std::fflush(stderr);
        std::_Exit(1);
    }
}
