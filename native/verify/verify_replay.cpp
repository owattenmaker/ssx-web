// The native replay verifier (docs/native.md "N2"): a portable run replay (web/server/replay-file.mjs) re-simulated headless on the
// native core the way the page's ?verify path does it (web/online-replay.js begin / load / play / verifyRun), with the result the
// browser verifier reports: {ok, reason, value, finishTick}. Every step names the page code it mirrors.
//
//   ssx3_verify_replay ASSETS_DIR REPLAY.ssxr [--trace OUT.json] [--max-ticks N]
//     ASSETS_DIR  the exported game data (web/public/assets)
//     --trace     the page's ?simtrace hash of every replayed tick (web/game-tick.js gameTrace), as a JSON array
//     --all-events  run a course this build has not been validated on (otherwise ok: null)
//   ssx3_verify_replay --core   prints the wasm core id this build corresponds to
// Prints one JSON line {ok, reason, value, finishTick, ms, core}. Exit 0 when it ran (whatever the verdict), 2 when the replay
// cannot be run here (ok: null: the caller falls back to the browser verifier), 1 on a simulation error (also ok: null).
// core: the id of the wasm core (the first 8 bytes of SHA-256 of core.wasm, hex, as the page reports it) built from the same
// sources as this binary (CMake SSX_CORE_ID); a verdict means something only for runs recorded on that core.
#include "core_api.hpp"
#include "game_rules.hpp"
#include "js_json.hpp"
#include "replay_file.hpp"
#include <cctype>
#include <chrono>
#include <climits>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using ssx_native::json;
using ssx_native::jsStringify;

#ifndef SSX_CORE_ID
#define SSX_CORE_ID "unknown"
#endif
// The courses this verifier matches the browser verifier on (docs/native.md "N2" fixtures); any other gives ok: null unless
// --all-events. Extend with each course's fixture.
static const char* const kValidatedCourses[] = {"ARA1", "BRA2", "ABA1", "ABC1", "ASS1"};

#include "replay_session.hpp"

using namespace ssx_native::session;

namespace {
std::string jsonString(const std::string& s) {
    return json(s).dump();
}

}  // namespace

int main(int argc, char** argv) {
    const auto t0 = std::chrono::steady_clock::now();
    auto ms = [&] { return long(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count()); };
    std::string tracePath;
    std::string dumpPath;
    bool allEvents = false;
    if (argc == 2 && std::string(argv[1]) == "--core") {
        std::printf("%s\n", SSX_CORE_ID);
        return 0;
    }
    long maxTicks = -1;
    std::vector<std::string> positional;
    for (int k = 1; k < argc; ++k) {
        const std::string a = argv[k];
        if (a == "--trace" && k + 1 < argc) {
            tracePath = argv[++k];
        } else if (a == "--all-events") {
            allEvents = true;
        } else if (a == "--dump" && k + 1 < argc) {
            dumpPath = argv[++k];
        } else if (a == "--max-ticks" && k + 1 < argc) {
            maxTicks = std::stol(argv[++k]);
        } else {
            positional.push_back(a);
        }
    }
    if (positional.size() != 2) {
        std::fprintf(stderr, "usage: ssx3_verify_replay ASSETS_DIR REPLAY.ssxr [--trace OUT.json] [--max-ticks N]\n");
        return 2;
    }
    auto report = [&](const char* ok, const std::string& reason, const std::string& value, long finishTick) {
        std::printf("{\"ok\":%s,\"reason\":%s,\"value\":%s,\"finishTick\":%ld,\"ms\":%ld,\"core\":\"%s\"}\n", ok, jsonString(reason).c_str(), value.c_str(),
                    finishTick, ms(), SSX_CORE_ID);
        std::fflush(stdout);
    };
    try {
        Verifier v;
        v.assets.root = positional[0];
        auto file = ssx_native::readReplayFile(readFile(positional[1]));
        v.meta = file.meta;
        v.pad = std::move(file.pad);
        v.begin();
        if (!allEvents && std::find(std::begin(kValidatedCourses), std::end(kValidatedCourses), v.code) == std::end(kValidatedCourses)) {
            throw Unsupported("course " + v.code + " not validated natively");
        }
        v.loadCourse();
        // web/online-replay.js load(): the lineup for the next prepare, then the warm-up (the rider, then the lineup)
        v.loadRider();
        v.prepare();
        // play(): the attribute bytes (main.js ui.cb.attributes -> set_rider_attributes(raw, 0)), then the replay's start
        const json& attributes = at(v.meta["rider"], "attributes");
        if (attributes.is_array() && attributes.size() == 7) {
            int32_t raw[7];
            for (size_t k = 0; k < 7; ++k) {
                raw[k] = ssx_native::jsInt(attributes[k]);
            }
            set_rider_attributes(raw, 0);
        }
        v.presentationFlags();
        // web/replay.js load -> host.prepare (the replay camera triggers), start('full') -> rewind(true): startRun(R), the camera begin
        const std::string triggers = v.assets.textOr(v.code + "/camera-triggers.json", "");
        if (!triggers.empty()) {
            replay_camera_triggers(triggers.c_str());
        }
        v.startRun();
        replay_camera_begin(0x5d, 1);
        // web/online-replay.js verifyRun: every recorded tick; the first finish decides (tick 0 is start()'s own step)
        const long end = ssx_native::jsInt(at(v.meta, "finishTick"));
        const bool timed = !at(at(v.meta, "claim"), "ticks").is_null();
        const json& claimValue = timed ? at(v.meta["claim"], "ticks") : at(at(v.meta, "claim"), "score");
        ssx_native::PadReader reader(v.pad);
        std::vector<uint32_t> trace;
        std::string dump = "[";
        Verifier::Finish fin;
        long at_ = -1;
        float input[24];
        const json& events = at(v.meta, "events");
        const long last = maxTicks >= 0 ? std::min(end, maxTicks - 1) : end;
        for (long t = 0; t <= last; ++t) {
            // web/replay.js step: the calls recorded before this tick (main.js replay host event)
            if (events.is_array()) {
                for (const auto& e : events) {
                    if (ssx_native::jsInt(at(e, "tick")) != t) {
                        continue;
                    }
                    const std::string kind = str(at(e, "kind"));
                    if (kind == "giveUp") {
                        race_give_up();
                    } else if (kind == "camera") {
                        set_camera_view(ssx_native::jsInt(at(e, "value")));
                    }
                }
            }
            reader.read(t, input);
            const auto f = v.tick(input);
            if (!tracePath.empty()) {
                trace.push_back(v.traceHash());
            }
            if (!dumpPath.empty()) {
                v.dumpRow(dump);
            }
            // the first finish decides and ends the run (verifyRun stops stepping there)
            if (t > 0 && f.seen && !fin.seen) {
                fin = f;
                at_ = t;
                break;
            }
        }
        if (!tracePath.empty()) {
            std::ofstream out(tracePath);
            out << json(trace).dump() << "\n";
        }
        if (!dumpPath.empty()) {
            std::ofstream out(dumpPath);
            out << dump << "]\n";
        }
        std::string value = "null";
        long claim = claimValue.is_number() ? long(claimValue.get<double>()) : LONG_MIN;
        long got = 0;
        if (fin.seen) {
            got = timed ? long(std::nearbyint(double(fin.ticks)) + 0) : long(fin.score);
            if (timed) {
                // Math.round: halves round up
                got = long(std::floor(double(fin.ticks) + 0.5));
            }
            value = std::to_string(got);
        }
        const bool ok = fin.seen && !fin.dnf && at_ == end && got == claim;
        std::string reason;
        if (!fin.seen) {
            reason = "no finish in " + std::to_string(end + 1) + " ticks";
        } else if (fin.dnf) {
            reason = "DNF";
        } else if (at_ != end) {
            reason = "finished at tick " + std::to_string(at_) + ", the run ends at " + std::to_string(end);
        } else if (got != claim) {
            reason = std::string(timed ? "time " : "score ") + value + ", claimed " + (claimValue.is_null() ? "undefined" : jsStringify(claimValue));
        } else {
            reason = "reproduced";
        }
        report(ok ? "true" : "false", reason, value, at_);
        std::fflush(stdout);
        std::_Exit(0);
    } catch (const Unsupported& e) {
        report("null", e.what(), "null", -1);
        std::_Exit(2);
    } catch (const std::exception& e) {
        report("null", std::string("simulation: ") + e.what(), "null", -1);
        std::_Exit(1);
    }
}
