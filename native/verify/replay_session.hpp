#pragma once
// A race session on the native core set up the way the page sets up a replay (docs/native.md "N2"): web/online-replay.js begin /
// load / play, main.js loadCourse / startRun(R), web/ai-race.js / ai-racers.js / lineup.js / character-roster.js, and the 60 Hz
// tick of web/game-tick.js simulateInCore. Each step names the page code it mirrors. Used by the replay verifier
// (verify_replay.cpp) and the native front end.
#include "board_flex.hpp"
#include "core_api.hpp"
#include "game_rules.hpp"
#include "js_json.hpp"
#include "replay_file.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ssx_native::session {

using ssx_native::json;
using ssx_native::jsStringify;

// A replay this build does not handle: reported as {ok: null}, never as a verdict.
struct Unsupported : std::runtime_error {
    using std::runtime_error::runtime_error;
};

// ---- the game data (the page's fetch of /assets/...) ----
struct Assets {
    std::string root;
    std::string path(const std::string& rel) const {
        return root + "/" + rel;
    }
    bool exists(const std::string& rel) const {
        std::ifstream f(path(rel), std::ios::binary);
        return bool(f);
    }
    std::string text(const std::string& rel) const {
        std::ifstream f(path(rel), std::ios::binary);
        if (!f) {
            throw std::runtime_error("missing " + rel);
        }
        std::ostringstream s;
        s << f.rdbuf();
        return s.str();
    }
    std::string textOr(const std::string& rel, const std::string& fallback) const {
        return exists(rel) ? text(rel) : fallback;
    }
    json doc(const std::string& rel) const {
        return json::parse(text(rel));
    }
};

inline std::vector<uint8_t> readFile(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) {
        throw std::runtime_error("cannot read " + p);
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

inline uint32_t fnv(uint32_t h, const void* p, size_t words) {
    const auto* w = static_cast<const uint32_t*>(p);
    for (size_t k = 0; k < words; ++k) {
        h = (h ^ w[k]) * 16777619u;
    }
    return h;
}

inline const json& at(const json& o, const char* key) {
    static const json none;
    const json* v = ssx_native::jsField(o, key);
    return v ? *v : none;
}
inline bool truthy(const json& v) {
    if (v.is_null()) {
        return false;
    }
    if (v.is_boolean()) {
        return v.get<bool>();
    }
    if (v.is_number()) {
        const double x = v.get<double>();
        return x != 0 && !std::isnan(x);
    }
    if (v.is_string()) {
        return !v.get<std::string>().empty();
    }
    return true;
}
inline std::string str(const json& v, const std::string& fallback = "") {
    return v.is_string() ? v.get<std::string>() : fallback;
}

// ---- web/lineup.js: the human's character, the lineup documents ----
struct Who {
    int base = 4;
    int cheat = 0;
};
// humanCharacter(rider, roster)
inline Who humanCharacter(const json& rider, const json& roster) {
    const json* entry = &rider;
    for (const auto& e : roster) {
        if (at(e, "id") == at(rider, "id")) {
            entry = &e;
            break;
        }
    }
    if (str(at(*entry, "kind")) == "cheat") {
        const std::string baseId = str(at(rider, "base"), "zoe").empty() ? "zoe" : str(at(rider, "base"), "zoe");
        int base = 4;
        for (const auto& e : roster) {
            if (str(at(e, "id")) == baseId && at(e, "character").is_number()) {
                base = at(e, "character").get<int>();
            }
        }
        return {base, at(*entry, "character").get<int>()};
    }
    const json& c = at(*entry, "character");
    return {c.is_number() ? c.get<int>() : 3, 0};
}
// a relationship record "kkllss" as [kind, level, score] (signed bytes) and back
inline std::array<int, 3> relRecord(const std::string& hex) {
    std::array<int, 3> r{};
    for (int i = 0; i < 3; ++i) {
        const int b = std::stoi(hex.substr(size_t(2 * i), 2), nullptr, 16);
        r[size_t(i)] = b > 127 ? b - 256 : b;
    }
    return r;
}
inline std::string relHex(int kind, int level, int score) {
    char out[7];
    std::snprintf(out, sizeof out, "%02x%02x%02x", kind & 255, level & 255, score & 255);
    return out;
}
// ageRelationships(table, characters, banks): each participant's records aged (0x155E58)
inline json ageRelationships(const json& table, const std::vector<int>& characters, const json& banks) {
    json out = table;
    for (size_t k = 0; k < characters.size(); ++k) {
        json& row = out[size_t(banks[k].get<int>())][size_t(characters[k])];
        for (auto& r : row) {
            const auto rec = relRecord(r.get<std::string>());
            const auto aged = ssx::rules::relationshipAge(rec[0], rec[2]);
            r = relHex(aged.kind, aged.level, aged.score);
        }
    }
    return out;
}
// relationshipScores: relationship(own, peer) = the peer's record about own (level; 3 for the peak rival)
inline json relationshipScores(const json& table, const std::vector<int>& characters, const json& banks, int rival) {
    json out = json::array();
    for (int own : characters) {
        json row = json::array();
        for (size_t p = 0; p < characters.size(); ++p) {
            if (own == rival) {
                row.push_back(3);
            } else {
                row.push_back(relRecord(table[size_t(banks[p].get<int>())][size_t(characters[p])][size_t(own)].get<std::string>())[1]);
            }
        }
        out.push_back(row);
    }
    return out;
}
inline std::string skinName(const json& data, int value) {
    return value < 10 ? data["names"]["base"][size_t(value)].get<std::string>() : data["names"]["cheat"][std::to_string(value)].get<std::string>();
}
// the parts of a lineups.json table entry applied in the page's order (Object.entries)
inline void applyPart(json& record, const json& part) {
    if (!part.is_object()) {
        return;
    }
    for (const auto& k : ssx_native::jsKeys(part)) {
        ssx_native::putPath(record, k, part.at(k));
    }
}
// assembleLineup(data, humanBase, values, {relationships, round}) (pv careerRival / semiFresh on, no level)
inline json assembleLineup(const json& data, int humanBase, const std::vector<int>& values, const json& relationships, int round) {
    const json& tmpl = data["template"];
    json riders = json::array();
    for (size_t k = 0; k < values.size(); ++k) {
        const int v = values[k];
        const std::string slot = std::to_string(k + 1);
        const int base = v < 10 ? v : humanBase;
        const std::string skin = skinName(data, v);
        json record = tmpl["riders"][k];
        const json& gridSlot = at(data["grid"], slot.c_str());
        json grid = json::object();
        bool gridOk = true;
        if (truthy(gridSlot)) {
            const json& scale = at(data["skin_scale"], skin.c_str());
            const json* g = scale.is_string() ? ssx_native::jsField(gridSlot, scale.get<std::string>().c_str()) : nullptr;
            gridOk = g && truthy(*g);
            if (gridOk) {
                grid = *g;
            }
        }
        const bool hasBase = !data["base"].empty();
        if (!gridOk || !truthy(at(data["skin"], skin.c_str())) || (hasBase && !truthy(at(data["base"], std::to_string(base).c_str())))) {
            throw std::runtime_error("No computer-rider data for " + skin + " in slot " + slot);
        }
        applyPart(record, at(data["slot"], slot.c_str()));
        applyPart(record, grid);
        applyPart(record, data["skin"][skin]);
        applyPart(record, at(data["base"], std::to_string(base).c_str()));
        applyPart(record, at(data["moment"], slot.c_str()));
        applyPart(record, at(data["state"], slot.c_str()));
        if (round >= 0 && record.contains("npc") && truthy(at(record["npc"], "score_state"))) {
            record["npc"]["score_state"]["role_e00"] = ssx::rules::npcRoundRole(int(k) + 1, round);
            record["npc"]["score_state"]["allow_flag0_e04"] = k == 0 && round == 3;
        }
        // pv semiFresh
        if (round == 2 && record.contains("ground") && truthy(at(record["ground"], "state")) && truthy(at(record, "identity"))) {
            record["ground"]["state"]["rider_type"] = 0x31;
            record["identity"]["rider_type434"] = 0x31;
        }
        riders.push_back(record);
    }
    json doc = json::object();
    for (auto it = tmpl.begin(); it != tmpl.end(); ++it) {
        doc[it.key()] = it.key() == "riders" ? riders : it.value();
    }
    std::vector<int> characters{humanBase};
    for (int v : values) {
        characters.push_back(v < 10 ? v : humanBase);
    }
    const size_t count = characters.size();
    const json& tmplCharacters = tmpl["relationships"]["characters"];
    for (size_t k = count; k < tmplCharacters.size(); ++k) {
        characters.push_back(tmplCharacters[k].get<int>());
    }
    json& rel = doc["relationships"];
    const json banks = rel["target_banks"];
    const int rival = ssx::rules::peakRival(data["peak"].get<int>(), humanBase);
    const json table = ageRelationships(relationships, std::vector<int>(characters.begin(), characters.begin() + long(count)), banks);
    rel["scores"] = relationshipScores(table, characters, banks, rival);
    rel["characters"] = characters;
    rel["persistent_human_character"] = humanBase;
    rel["rival_character"] = rival;
    rel["event_kind"] = data["peak"];
    json pairs = json::array();
    for (size_t k = 0; k < count; ++k) {
        const std::string c = std::to_string(characters[k]);
        const json* human = k == 0 ? ssx_native::jsField(at(data, "human_pair_inputs"), c.c_str()) : nullptr;
        pairs.push_back(human && truthy(*human) ? *human : at(data["pair_inputs"], c.c_str()));
    }
    doc["world"]["pair_inputs"] = pairs;
    json& records = doc["world"]["records"];
    for (size_t a = 0; a < records.size(); ++a) {
        for (size_t b = 0; b < records[a].size(); ++b) {
            json& r = records[a][b];
            const bool enabled = truthy(at(r, "enabled"));
            r["t1c"] = a < b && enabled && rel["scores"][a][b].get<double>() >= 2 ? 1 : 0;
        }
    }
    return doc;
}
// humanGridState(data, rider, humanBase, {career})
inline json humanGridState(const json& data, const json& rider, int humanBase, bool career) {
    if (!truthy(at(data, "human_grid"))) {
        return nullptr;
    }
    std::string key;
    if (career && str(at(rider, "kind")) == "cheat") {
        key = str(at(data["human_scale"], data["names"]["base"][size_t(humanBase)].get<std::string>().c_str()));
    } else {
        key = str(at(data["human_scale"], str(at(rider, "id")).c_str()));
    }
    const json& grid = at(data["human_grid"], key.c_str());
    const json& base = at(data["human_base"], std::to_string(humanBase).c_str());
    if (!truthy(grid) || !truthy(base)) {
        return nullptr;
    }
    json state = data["human_template"];
    applyPart(state, grid);
    applyPart(state, base);
    return state;
}

// web/character-roster.js composeCheat(baseDoc, skinDoc, {career})
inline json composeCheat(const json& baseDoc, const json& skinDoc, bool career) {
    if (skinDoc.is_null()) {
        return baseDoc;
    }
    json skin = at(skinDoc, "settings").is_object() ? skinDoc["settings"] : json::object();
    if (career) {
        if (truthy(at(skin, "original_animation"))) {
            skin["original_animation"].erase("scale");
        }
        if (truthy(at(at(skin, "original_landing"), "profile"))) {
            skin["original_landing"]["profile"].erase("body_scale");
        }
        skin.erase("original_event_start");
        if (truthy(at(skin, "original_air_entry"))) {
            skin["original_air_entry"].erase("pivot");
        }
    }
    const json& baseStance = at(at(at(baseDoc, "settings"), "original_reset"), "stance");
    if (baseStance.is_number() && baseStance.get<double>() == 1) {
        if (truthy(at(at(skin, "original_air_entry"), "pivot"))) {
            json& pivot = skin["original_air_entry"]["pivot"];
            pivot[0] = -pivot[0].get<double>();
        }
        if (truthy(at(skin, "original_event_start"))) {
            skin["original_event_start"]["state"]["reverse_stance"] = true;
        }
    }
    json out = json::object();
    out["character"] = at(skinDoc, "character");
    out["kind"] = "cheat";
    out["base"] = baseDoc.is_null() || at(baseDoc, "character").is_null() ? json("zoe") : baseDoc["character"];
    out["settings"] = ssx_native::mergeCharacterSettings(at(baseDoc, "settings").is_object() ? baseDoc["settings"] : json::object(), skin);
    out["uber_rows"] = at(skinDoc, "uber_rows").is_array() ? skinDoc["uber_rows"] : json::array();
    json identity = at(skinDoc, "identity").is_object() ? skinDoc["identity"] : json::object();
    const json& pair = at(at(baseDoc, "identity"), "pair");
    identity["pair"] = pair;
    out["identity"] = identity;
    return out;
}

// A live Single Event's replay-shaped description (the native front end without a replay file): the course, the rider with the
// profile's fresh attribute bytes, the solo lineup (time attack: no computer riders), and the start state a live start gives.
inline json liveMeta(const std::string& course, const std::string& rider) {
    json m = json::object();
    m["course"] = course;
    m["rider"] = {{"id", rider}, {"kind", "rider"}, {"base", nullptr}, {"career", false}, {"outfit", nullptr},
                  {"attributes", json::array({5, 5, 5, 5, 5, 5, 5})}, {"uber", json::array()}, {"uberKey", ""}};
    m["lineup"] = {{"values", json::array()}, {"enabled", false}, {"round", 3}, {"career", false}};
    m["snapshot"] = {{"inWorld", nullptr}, {"timeLimit", 0}, {"inputMap", 0}, {"cameraView", 61},
                     {"collect", {{"cash", 0}, {"state", json::array({1, 0, 0})}, {"rows", json::array()}}}, {"ai", nullptr}};
    m["events"] = json::array();
    m["finishTick"] = -1;
    return m;
}

// ---- the replay's set-up and run ----
struct Npc {
    int slot = 0;
    json record;
    void* block = nullptr;
};

// The race (named Verifier: the replay verifier was its first user)
struct Verifier {
    // the human rider's morph parts (pv boardFlex), configured after its init_animation
    flex::Flex humanFlex;
    // the native front end: also the presentation set-up (the environment lattice for the rider lighting)
    bool presentation = false;
    // quality.presentationFast (web/quality.js: tier low on; the replay verifier and the shots compare against quality=low)
    bool presentationFast = true;
    json environmentMeta;
    // a live run (the native front end): main.js startRun(null), no recorded start state to restore
    bool live = false;
    // the last tick's tick_begin flags (1 rescue, 2 placement, 4 pose, 8 pose again)
    uint32_t tickFlags = 0;
    // called in every tick after the riders' passes (manager_end_tick / tick_mid), before tick_end (the race HUD's reads)
    std::function<void()> afterMid;
    float command14 = 0;
    Assets assets;
    json meta;
    std::vector<uint8_t> pad;
    json courses, roster, course, entry, spawn;
    std::string code;
    // the human's core
    void* humanBlock = nullptr;
    std::string packetsJson, packetsBin, initialText, terrainText, worldText, railsText, hash;
    json initialDoc;
    int boneCount = 0;
    json humanSettingsDoc;
    // the computer riders (web/ai-race.js / ai-racers.js)
    bool hasAi = false;
    bool aiEnabled = true;
    json aiDoc, lineupData;
    bool hasLineupData = false;
    json rivals;
    std::vector<Npc> npcs;
    int count = 1;
    Who who;
    json pairZoe;
    bool backcountry = false;

    void inNpc(const Npc& n, const std::function<void()>& fn) {
        native_run_in_context(n.block, fn);
    }

    // web/online-replay.js begin: the rider and the course
    void begin() {
        courses = assets.doc("courses.json")["courses"];
        roster = assets.doc("riders.json");
        code = str(at(meta, "course"));
        for (const auto& c : courses) {
            if (str(at(c, "code")) == code && !truthy(at(c, "freeRide"))) {
                course = c;
                break;
            }
        }
        const json& rider = at(meta, "rider");
        for (const auto& r : roster) {
            if (at(r, "id") == at(rider, "id")) {
                entry = r;
                break;
            }
        }
        if (course.is_null() || entry.is_null()) {
            throw Unsupported("rider or course unknown");
        }
        if (truthy(at(meta["snapshot"], "inWorld"))) {
            throw Unsupported("in-world replay");
        }
        // the outfit (web/wardrobe.js remoteOutfitRider): the default package only
        const json& outfit = at(rider, "outfit");
        if (!outfit.is_null() && str(at(rider, "kind")) != "cheat") {
            throw Unsupported("outfit " + outfit.dump());
        }
        // main.js ui.courses: a backcountry course is listed as its Rival Time entry first (rivalMode 4), then Rival Points (5)
        backcountry = str(at(course, "event")) == "backcountry";
        if (backcountry) {
            course["rivalMode"] = 4;
        }
    }

    std::string courseRoot() const {
        // courses.json root "/assets/<CODE>/"
        std::string r = str(at(course, "root"), "/assets/" + code + "/");
        if (r.rfind("/assets/", 0) == 0) {
            r = r.substr(8);
        }
        return r;
    }
    std::string assetPath(const std::string& url) const {
        return url.rfind("/assets/", 0) == 0 ? url.substr(8) : url;
    }

    // main.js resetPhysics (an event course: no free ride, no plane drop)
    void resetPhysics() {
        const json& p = spawn["position"];
        reset_rider(p[0].get<float>(), p[1].get<float>(), p[2].get<float>(), spawn["heading"].get<float>());
        if (truthy(at(spawn, "velocity"))) {
            const json& v = spawn["velocity"];
            set_rider_velocity(v[0].get<float>(), v[1].get<float>(), v[2].get<float>());
        }
    }

    // main.js loadCourse: the human's core and the course
    void loadCourse() {
        const std::string root = courseRoot();
        spawn = assets.doc(root + "start.json");
        humanBlock = rider_context_current();
        set_fx_deferred(1);
        const std::string collision = assets.text(root + "collision.bin");
        std::vector<float> tris(collision.size() / 4);
        std::memcpy(tris.data(), collision.data(), tris.size() * 4);
        init_world(tris.data(), int(tris.size()));
        packetsJson = assets.text("ANIMATIONS/animation-packets.json");
        packetsBin = assets.text("ANIMATIONS/animation-packets.bin");
        initialText = assets.text(assetPath(str(at(course, "initial"), code == "ARA1" ? "/assets/ANIMATIONS/initial.json" : "/assets/" + code + "/initial.json")));
        initialDoc = json::parse(initialText);
        // web/set-pieces-renderer.js createSetPieceRenderer: sections, the stage world, the avalanches, the flags
        const std::string sectionsText = assets.textOr(root + "SECTIONS/sections.json", "");
        const bool ready = assets.exists(root + "SECTIONS/ready-state.json");
        bool sectionsNative = false;
        if (!sectionsText.empty() && ready) {
            sectionsNative = init_sections(sectionsText.c_str()) > 0;
        }
        const std::string particles = assets.textOr(root + "PARTICLES/particles.json", "");
        if (!particles.empty()) {
            const std::string live = assets.textOr((code == "ARA1" ? std::string() : root) + "LIVECOMP/livecomp.json", "");
            const std::string stage = assets.textOr(root + "STAGE/stage-world.json", "");
            init_stage_world(particles.c_str(), live.c_str(), stage.c_str());
        }
        // web/avalanche-load.js loadAvalanches
        static const char* avalancheCourses[] = {"ABC1", "DBC2", "DRA4", "ERA5", "ESS3", "EBA3", "EBC3"};
        bool avalanches = false;
        for (const char* a : avalancheCourses) {
            if (code == a && assets.exists(root + "avalanches.json")) {
                init_avalanches(assets.text(root + "avalanches.json").c_str(), 0);
                avalanches = true;
            }
        }
        if (!avalanches) {
            avalanche_clear();
        }
        const std::string flagBase = code == "ARA1" ? std::string() : root;
        if (sectionsNative && assets.exists(flagBase + "FLAGS/flags.json")) {
            const json readyState = assets.doc(root + "SECTIONS/ready-state.json");
            if (truthy(at(readyState, "flags"))) {
                const json flags = assets.doc(flagBase + "FLAGS/flags.json");
                init_stage_flags(jsStringify(flags).c_str(), jsStringify(readyState).c_str());
            }
        }
        // the environment lattice (presentation: the rider lighting; main.js initEnvironmentSliced, the whole document at once)
        if (presentation) {
            const std::string envRoot = assetPath(str(at(course, "environmentRoot"), "/assets/" + root));
            const std::string envText = assets.text(envRoot + "environment.json");
            const std::string envBin = assets.text(envRoot + "environment.bin");
            init_environment(envText.c_str(), reinterpret_cast<const uint8_t*>(envBin.data()), int(envBin.size()));
            environmentMeta = json::parse(envText);
        }
        resetPhysics();
        // the boot rider (Zoe) on the course settings
        const std::string bootRig = assets.text("RIDER_ZOE/rider.json");
        const std::string initialJs = jsStringify(initialDoc);
        init_animation(jsStringify(json::parse(packetsJson)).c_str(), jsStringify(json::parse(bootRig)).c_str(), initialJs.c_str(),
                       reinterpret_cast<const uint8_t*>(packetsBin.data()), uint32_t(packetsBin.size()));
        init_race(initialJs.c_str());
        animation_use_physics(1);
        terrainText = assets.text(root + "terrain.json");
        worldText = assets.text(root + "world_collision.json");
        railsText = assets.text(root + "rails.json");
        hash = json::parse(terrainText)["source_sha256"].get<std::string>();
        init_terrain(terrainText.c_str());
        init_world_collision(worldText.c_str(), hash.c_str());
        init_body_terrain(terrainText.c_str());
        init_fog(jsStringify(assets.doc(root + "fog-tree.json")).c_str());
        if (assets.exists(root + "weather.json")) {
            init_weather(jsStringify(assets.doc(root + "weather.json")).c_str());
        } else {
            init_weather(nullptr);
        }
        init_rider_lighting(jsStringify(assets.doc(root + "local-lights.json")).c_str(), jsStringify(assets.doc(root + "light-tree.json")).c_str());
        init_rails(railsText.c_str(), hash.c_str());
        createAiRace();
        animation_tick(0, 0, 0, 0, 1, 0, 0, 0, 0, 0, NAN, NAN);
    }

    // web/ai-racers.js configureNpc
    void configureNpc(const Npc& n) {
        json context = json::object();
        if (aiDoc.contains("event_variant")) {
            context["event_variant"] = aiDoc["event_variant"];
        }
        if (aiDoc.contains("relationships")) {
            context["relationships"] = aiDoc["relationships"];
        }
        if (aiDoc.contains("anchor_tick")) {
            context["anchor_tick"] = aiDoc["anchor_tick"];
        }
        const std::string riderText = jsStringify(n.record);
        const std::string contextText = jsStringify(context);
        npc_configure(riderText.c_str(), contextText.c_str());
        const json& a = at(n.record, "attributes");
        const json& raw = at(a, "raw");
        if (raw.is_array() && raw.size() == 7) {
            int32_t words[7];
            bool seeds = true;
            for (size_t j = 0; j < 7; ++j) {
                words[j] = ssx_native::jsInt(raw[j]);
                if (raw[j].get<double>() != 5) {
                    seeds = false;
                }
            }
            if (!seeds) {
                set_rider_attributes(words, ssx_native::jsInt(at(a, "override")));
            }
        }
    }
    // web/ai-racers.js configureRider
    void configureRider(const Npc& n) {
        const std::string pkg = str(at(n.record, "package"));
        const std::string riderText = assets.text(pkg + "/rider.json");
        const std::string settings = jsStringify(ssx_native::mergeSettingsShallow(initialDoc, at(n.record, "settings")));
        init_animation(packetsJson.c_str(), riderText.c_str(), settings.c_str(), reinterpret_cast<const uint8_t*>(packetsBin.data()), uint32_t(packetsBin.size()));
        configureNpc(n);
    }
    // web/ai-race.js createAiRace -> web/ai-racers.js createAiRacers (pv coreRiderManager)
    void createAiRace() {
        const std::string root = courseRoot();
        if (!assets.exists(root + "npc-riders.json")) {
            return;
        }
        hasAi = true;
        aiDoc = assets.doc(root + "npc-riders.json");
        hasLineupData = assets.exists(root + "lineups.json");
        if (hasLineupData) {
            lineupData = assets.doc(root + "lineups.json");
        }
        if (backcountry && assets.exists(root + "rivals.json")) {
            rivals = assets.doc(root + "rivals.json");
        }
        count = std::min<int>(6, int(aiDoc["riders"].size()) + 1);
        for (int k = 0; k + 1 < count; ++k) {
            Npc n;
            n.record = aiDoc["riders"][size_t(k)];
            n.slot = n.record["slot"].get<int>();
            n.block = rider_context_create();
            inNpc(n, [&] {
                // attachWorld
                init_race(initialText.c_str());
                animation_use_physics(1);
                init_world_collision(worldText.c_str(), hash.c_str());
                init_body_terrain(terrainText.c_str());
                init_rails(railsText.c_str(), hash.c_str());
                configureRider(n);
            });
            npcs.push_back(n);
        }
        // web/ai-race.js: each computer rider's lighting (presentation; opponent-riders.js initOpponentLighting)
        if (presentation) {
            const std::string envRoot = assetPath(str(at(course, "environmentRoot"), "/assets/" + root));
            const std::string envText = jsStringify(json::parse(assets.text(envRoot + "environment.json")));
            const std::string envBin = assets.text(envRoot + "environment.bin");
            const std::string localLights = jsStringify(assets.doc(root + "local-lights.json"));
            const std::string lightTree = jsStringify(assets.doc(root + "light-tree.json"));
            for (const auto& n : npcs) {
                inNpc(n, [&] {
                    init_environment(envText.c_str(), reinterpret_cast<const uint8_t*>(envBin.data()), int(envBin.size()));
                    init_rider_lighting(localLights.c_str(), lightTree.c_str());
                });
            }
        }
        const bool pairs = count > 1;
        rider_host((pairs ? 3 : 1) | 8);
        rider_parse_cache_clear();
        for (const auto& n : npcs) {
            inNpc(n, [&] { rider_host((pairs ? 2 : 0) | 4 | 8); });
        }
        std::vector<void*> blocks{humanBlock};
        std::vector<int> slots{0};
        for (const auto& n : npcs) {
            blocks.push_back(n.block);
            slots.push_back(n.slot);
        }
        manager_configure(blocks.data(), slots.data(), int(npcs.size()) + 1, count, (pairs ? 1 : 0) | 2);
        // the original's game RNG at the anchor (Zoe)
        if (hasLineupData) {
            setAnchorRng(anchorWords({4, 0}));
        } else if (!rivals.is_null()) {
            // the ready state's game RNG until prepareRival picks this human's document
            const json& firstCharacter = at(aiDoc["riders"][0], "character");
            const json* d = firstCharacter.is_string() ? ssx_native::jsField(at(rivals["documents"], "race"), firstCharacter.get<std::string>().c_str()) : nullptr;
            setAnchorRng(rivalAnchorWords(d ? *d : json(), {4, 0}));
        } else {
            eventAnchor({4, 0});
        }
        pairZoe = aiDoc["world"]["pair_inputs"][0];
    }
    // web/lineup.js anchorRandomWords
    ssx::rules::Words anchorWords(Who w) const {
        const json& d = lineupData["load_draws"];
        const json* c = ssx_native::jsField(d["character"], std::to_string(w.base).c_str());
        const int draws = d["base"].get<int>() + (c ? c->get<int>() : 0) + (w.cheat ? d["cheat"].get<int>() : 0) + d["start"].get<int>();
        return ssx::rules::wordsAfterDraws(0, uint32_t(draws));
    }
    void setAnchorRng(const ssx::rules::Words& w) {
        manager_set_anchor(w.data());
    }
    // null words: none (ai-racers.js setAnchorRng(null))
    void setAnchorRng(const std::optional<ssx::rules::Words>& w) {
        manager_set_anchor(w ? w->data() : nullptr);
    }
    // web/lineup.js rivalAnchorDraws / rivalAnchorWords
    std::optional<ssx::rules::Words> rivalAnchorWords(const json& doc, Who human) const {
        const json& a = at(doc, "anchor_rng");
        const json& rule = at(rivals, "load_draws");
        if (!truthy(a) || !(at(a, "seed").is_number() && at(a, "seed").get<double>() == 0) || !at(a, "draws").is_number_integer()) {
            return std::nullopt;
        }
        const int draws = at(a, "draws").get<int>();
        static const char* characterOf[] = {"moby", "kaori", "allegra", "mac", "zoe", "griff", "elise", "nate", "psymon", "viggo"};
        int ownBase = -1;
        const std::string ownId = str(at(doc, "human"));
        for (const auto& e : roster) {
            if (str(at(e, "id")) == ownId && at(e, "character").is_number()) {
                ownBase = at(e, "character").get<int>();
            }
        }
        if (ownBase < 0) {
            for (int k = 0; k < 10; ++k) {
                if (ownId == characterOf[k]) {
                    ownBase = k;
                }
            }
        }
        int n = draws;
        if (truthy(rule) && ownBase >= 0 && !(human.base == ownBase && !human.cheat)) {
            auto shift = [&](int base, int cheat) {
                const json* c = ssx_native::jsField(rule["character"], std::to_string(base).c_str());
                return (c ? c->get<int>() : 0) + (cheat ? rule["cheat"].get<int>() : 0);
            };
            const json& extra = at(a, "menu_extra");
            n = draws - (extra.is_number() ? extra.get<int>() : 0) + shift(human.base, human.cheat) - shift(ownBase, 0);
        }
        return ssx::rules::wordsAfterDraws(0, uint32_t(n));
    }
    // web/ai-race.js prepareRival({rider, event: {mode}})
    void prepareRival(int eventMode) {
        const std::string mode = eventMode == 5 || ssx_native::jsInt(at(course, "rivalMode")) == 5 ? "jam" : "race";
        const int peak = at(course, "peak").is_number() ? at(course, "peak").get<int>() : 1;
        static const char* ids[] = {"moby", "kaori", "allegra", "mac", "zoe", "griff", "elise", "nate", "psymon", "viggo"};
        const json& docs = rivals["documents"];
        const json& m = at(docs, mode.c_str());
        const json* d = ssx_native::jsField(m, ids[ssx::rules::rivalCharacter(peak, who.base)]);
        if (!d || d->is_null()) {
            d = ssx_native::jsField(m, ids[ssx::rules::rivalCharacter(peak, -1)]);
        }
        if (!d || d->is_null()) {
            d = &docs["race"].begin().value();
        }
        json next = *d;
        next["world"]["pair_inputs"][0] = aiDoc["world"]["pair_inputs"][0];
        setDocument(next);
        setAnchorRng(rivalAnchorWords(next, who));
        const json& kind = at(at(next, "game_mode"), "kind");
        const int k = kind.is_null() ? -1 : ssx_native::jsInt(kind);
        event_kind(k);
        for (const auto& n : npcs) {
            inNpc(n, [&] { event_kind(k); });
        }
    }
    // web/ai-race.js eventAnchor (a course without lineups.json)
    void eventAnchor(Who w) {
        if (hasLineupData || !rivals.is_null()) {
            return;
        }
        const auto* a = ssx::rules::eventAnchor(code.c_str());
        if (!a) {
            return;
        }
        setAnchorRng(ssx::rules::wordsAfterDraws(0, uint32_t(ssx::rules::eventAnchorDraws(*a, w.base, w.cheat != 0))));
    }

    // web/character-roster.js loadCharacter + humanSettings, main.js loadRiderNow / initializeRiderAnimation
    void loadRider() {
        const json& m = meta["rider"];
        json rider = entry;
        if (str(at(m, "kind")) == "cheat") {
            rider["kind"] = "cheat";
            rider["base"] = str(at(m, "base"), "zoe").empty() ? "zoe" : str(at(m, "base"), "zoe");
        }
        if (truthy(at(m, "career"))) {
            rider["career"] = true;
        }
        who = humanCharacter(rider, roster);
        // the character document
        json doc = nullptr;
        auto settingsOf = [&](const std::string& pkg) -> json {
            return assets.exists(pkg + "/settings.json") ? assets.doc(pkg + "/settings.json") : json(nullptr);
        };
        const json& settingsFlag = at(rider, "settings");
        if (truthy(at(rider, "package")) && !(settingsFlag.is_boolean() && !settingsFlag.get<bool>())) {
            if (str(at(rider, "kind")) == "cheat") {
                // a cheat skin on its base rider (web/character-roster.js loadCharacterDoc / composeCheat)
                const std::string baseId = str(at(rider, "base"));
                json baseDoc = nullptr;
                if (!baseId.empty() && baseId != "zoe" && baseId != "sam") {
                    std::string upper = baseId;
                    for (auto& c : upper) {
                        c = char(std::toupper(static_cast<unsigned char>(c)));
                    }
                    baseDoc = settingsOf("RIDER_" + upper);
                }
                doc = composeCheat(baseDoc, settingsOf(str(at(rider, "package"))), truthy(at(rider, "career")));
            } else {
                doc = settingsOf(str(at(rider, "settings_package"), str(at(rider, "package"))));
            }
        }
        json uberRows = json::array();
        for (const auto& r : at(m, "uber")) {
            uberRows.push_back(r);
        }
        json settings = initialDoc;
        if (!doc.is_null() || !uberRows.empty()) {
            json character = doc.is_null() ? json{{"settings", json::object()}} : doc;
            if (!uberRows.empty()) {
                for (const auto& r : at(character, "uber_rows")) {
                    uberRows.push_back(r);
                }
                character["uber_rows"] = uberRows;
            }
            settings = ssx_native::mergeCharacterSettings(initialDoc, at(character, "settings").is_object() ? character["settings"] : json::object());
            const json& rows = at(character, "uber_rows");
            if (rows.is_array() && !rows.empty()) {
                json& grab = settings["original_grab_control"];
                json uber = grab["profile"]["uber"];
                for (const auto& r : rows) {
                    uber[r[0].get<size_t>()][r[1].get<size_t>()] = r[2];
                }
                grab["profile"]["uber"] = uber;
            }
            const json& id = at(character, "identity");
            if (truthy(at(id, "upper_mask8c0"))) {
                settings["original_rider_identity"] = {{"upper_mask8c0", id["upper_mask8c0"]}, {"upper_mask8c8", at(id, "upper_mask8c8")},
                                                       {"upper_mask8d0", at(id, "upper_mask8d0")}};
            }
            // applyHumanPairInputs
            if (hasAi) {
                const json& pair = at(id, "pair");
                if (truthy(pair)) {
                    aiDoc["world"]["pair_inputs"][0] = {{"weight_attribute", at(pair, "weight_attribute")},
                                                        {"collision_stat", at(pair, "collision_stat")},
                                                        {"attack_stat", at(pair, "attack_stat")}};
                } else {
                    aiDoc["world"]["pair_inputs"][0] = pairZoe;
                }
            }
        } else if (hasAi) {
            aiDoc["world"]["pair_inputs"][0] = pairZoe;
        }
        humanSettingsDoc = settings;
        const std::string rig = assets.text(str(at(rider, "package")) + "/rider.json");
        boneCount = int(json::parse(rig)["bones"].size());
        const std::string settingsText = jsStringify(settings);
        init_animation(jsStringify(json::parse(packetsJson)).c_str(), jsStringify(json::parse(rig)).c_str(), settingsText.c_str(),
                       reinterpret_cast<const uint8_t*>(packetsBin.data()), uint32_t(packetsBin.size()));
        // main.js initializeRiderAnimation: the rider's morph parts (pv boardFlex, web/board-flex.js configureBoardFlex)
        humanFlex = flex::load(assets.path(str(at(rider, "package")) + "/"), json::parse(rig));
        flex::configure(humanFlex);
        init_race(settingsText.c_str());
        animation_use_physics(1);
        resetPhysics();
        animation_tick(0, 0, 0, 0, 1, 0, 0, 0, 0, 0, NAN, NAN);
        // main.js initializeRiderAnimation: the host's posed physical frame (and animation pose) from here on
        std::memcpy(posed, pose_physical(), sizeof posed);
        hasPose = true;
    }

    // web/ai-race.js humanGrid
    void humanGrid() {
        if (code == "ARA1") {
            return;
        }
        json rider = entry;
        const json grid = humanGridState(lineupData, rider, who.base, truthy(at(meta["rider"], "career")));
        if (!grid.is_null()) {
            human_grid_seed(jsStringify(grid).c_str());
        }
    }
    // web/ai-racers.js setDocument
    void setDocument(const json& next) {
        std::vector<int> changed;
        auto row = [](const json& d, int slot) { return jsStringify(d["relationships"]["scores"][size_t(slot)]); };
        for (auto& n : npcs) {
            const json* rider = nullptr;
            for (const auto& r : next["riders"]) {
                if (r["slot"].get<int>() == n.slot) {
                    rider = &r;
                }
            }
            if (!rider) {
                throw std::runtime_error("Lineup lacks slot " + std::to_string(n.slot));
            }
            const bool same = jsStringify(*rider) == jsStringify(n.record) && row(next, n.slot) == row(aiDoc, n.slot) &&
                              jsStringify(at(next, "event_variant")) == jsStringify(at(aiDoc, "event_variant")) &&
                              jsStringify(at(next, "anchor_tick")) == jsStringify(at(aiDoc, "anchor_tick"));
            if (!same) {
                changed.push_back(n.slot);
            }
        }
        aiDoc = next;
        for (auto& n : npcs) {
            if (std::find(changed.begin(), changed.end(), n.slot) == changed.end()) {
                continue;
            }
            for (const auto& r : next["riders"]) {
                if (r["slot"].get<int>() == n.slot) {
                    n.record = r;
                }
            }
            inNpc(n, [&] { configureRider(n); });
        }
    }
    // web/ai-race.js install
    void install(json next) {
        setAnchorRng(anchorWords(who));
        humanGrid();
        next["world"]["pair_inputs"][0] = aiDoc["world"]["pair_inputs"][0];
        setDocument(next);
    }
    // web/ai-race.js prepare -> prepareFixed (fixed = the replay's lineup)
    void prepare() {
        if (!hasAi) {
            return;
        }
        const json& fixed = at(meta, "lineup");
        if (fixed.is_null()) {
            throw Unsupported("no lineup in the replay");
        }
        if (!rivals.is_null()) {
            prepareRival(str(at(fixed, "mode")) == "jam" ? 5 : 4);
            return;
        }
        const json& values = at(fixed, "values");
        const bool solo = values.is_array() && values.empty() && at(fixed, "enabled").is_boolean() && !at(fixed, "enabled").get<bool>();
        if (!at(fixed, "opponent").is_null() || solo) {
            // prepareFreestyle
            if (!at(fixed, "opponent").is_null()) {
                // a Single Event slope style's opponent, assembled with the live tables: a fresh page's (lineups.json
                // relationships_fresh; the page overlays its stored bank 0, which the start's R.ai tables then replace)
                aiEnabled = true;
                if (!hasLineupData) {
                    eventAnchor(who);
                    return;
                }
                install(assembleLineup(lineupData, who.base, {fixed["opponent"].get<int>()}, lineupData["relationships_fresh"], -1));
                return;
            }
            aiEnabled = false;
            if (hasLineupData) {
                humanGrid();
            }
            return;
        }
        aiEnabled = true;
        if (!hasLineupData || !values.is_array()) {
            eventAnchor(who);
            return;
        }
        if (at(fixed, "tables").is_null()) {
            throw Unsupported("a lineup without its relationship tables");
        }
        std::vector<int> v;
        for (const auto& x : values) {
            v.push_back(x.get<int>());
        }
        // heatRound (pv heatRoles): the round only for a career race
        const int round = truthy(at(fixed, "career")) ? (at(fixed, "round").is_number() ? at(fixed, "round").get<int>() : 3) : -1;
        install(assembleLineup(lineupData, who.base, v, fixed["tables"], round));
    }

    // main.js applyPresentationFast (every frame)
    void presentationFlags() {
        const int fast = presentationFast ? 1 : 0;
        auto flags = [fast] {
            set_presentation_fast(fast);
            set_rider_fx_render_scale(1);
            set_fx_reset_kicker(1);
            stage_teleport_enable(1);
        };
        flags();
        for (const auto& n : npcs) {
            inNpc(n, flags);
        }
    }

    // web/ai-racers.js loadCoreRelationships spec (web/ai-race.js relationshipsInCore)
    void loadCoreRelationships(const json& tables) {
        const json& R = aiDoc["relationships"];
        const bool enabled = !tables.is_null() && truthy(at(R, "characters"));
        const json& characters = at(R, "characters");
        const json& banks = at(R, "target_banks");
        const json& scores = at(R, "scores");
        if (!(characters.is_array() && characters.size() == 6 && banks.is_array() && banks.size() == 6 && scores.is_array() && scores.size() == 6)) {
            manager_relationships_off();
            return;
        }
        std::vector<int32_t> words{enabled ? 1 : 0};
        for (const auto& c : characters) {
            words.push_back(ssx_native::jsInt(c));
        }
        for (const auto& b : banks) {
            words.push_back(ssx_native::jsInt(b));
        }
        const json& rival = at(R, "rival_character");
        words.push_back(rival.is_null() ? -1 : ssx_native::jsInt(rival));
        for (size_t b = 0; b < 3; ++b) {
            for (size_t c = 0; c < 10; ++c) {
                for (size_t o = 0; o < 10; ++o) {
                    const json* r = nullptr;
                    if (tables.is_array() && b < tables.size() && tables[b].is_array() && c < tables[b].size() && tables[b][c].is_array() && o < tables[b][c].size()) {
                        r = &tables[b][c][o];
                    }
                    if (r && r->is_string()) {
                        const auto rec = relRecord(r->get<std::string>());
                        words.insert(words.end(), {rec[0], rec[1], rec[2]});
                    } else {
                        words.insert(words.end(), {0, 0, 0});
                    }
                }
            }
        }
        for (size_t a = 0; a < 6; ++a) {
            for (size_t k = 0; k < 6; ++k) {
                const json& v = scores[a].is_array() && k < scores[a].size() ? scores[a][k] : json();
                words.push_back(v.is_null() ? 0 : ssx_native::jsInt(v));
            }
        }
        const json& kinds = at(R, "kinds");
        for (size_t a = 0; a < 6; ++a) {
            for (size_t k = 0; k < 6; ++k) {
                const json* v = kinds.is_array() && a < kinds.size() && kinds[a].is_array() && k < kinds[a].size() ? &kinds[a][k] : nullptr;
                words.push_back(!v || v->is_null() ? -1 : ssx_native::jsInt(*v));
            }
        }
        if (!manager_relationships(words.data())) {
            manager_relationships_off();
        }
    }
    // web/ai-race.js start({replay: R.ai}) with the core's event start (docs/core-migration.md M4b)
    void aiStart(const json& replayAi) {
        const json tables = at(replayAi, "tables");
        aiDoc["relationships"]["scores"] = at(replayAi, "scores");
        aiDoc["relationships"]["kinds"] = at(replayAi, "kinds");
        // racers.start: manager_event_start with the same documents
        std::vector<std::string> texts;
        for (const auto& n : npcs) {
            texts.push_back(jsStringify(n.record));
        }
        json context = json::object();
        if (aiDoc.contains("event_variant")) {
            context["event_variant"] = aiDoc["event_variant"];
        }
        context["relationships"] = aiDoc["relationships"];
        if (aiDoc.contains("anchor_tick")) {
            context["anchor_tick"] = aiDoc["anchor_tick"];
        }
        const std::string contextText = jsStringify(context);
        const std::string worldText = jsStringify(json{{"world", aiDoc["world"]}, {"relationships", aiDoc["relationships"]}});
        std::vector<const char*> list;
        for (const auto& t : texts) {
            list.push_back(t.c_str());
        }
        manager_event_start(list.data(), contextText.c_str(), worldText.c_str(), 0, 0, 0, nullptr);
        // syncWorldNodes (pv npcWorldNodes)
        shared_world_nodes(1);
        for (const auto& n : npcs) {
            std::vector<uint32_t> ids;
            inNpc(n, [&] {
                const uint32_t* sp = world_instance_states();
                for (uint32_t k = 0; k < sp[0]; ++k) {
                    ids.push_back(sp[1 + 3 * k]);
                }
            });
            const uint32_t* dp = world_node_states(ids.data(), int(ids.size()));
            std::vector<uint32_t> words(dp, dp + 1 + 6 * dp[0]);
            inNpc(n, [&] { world_node_states_apply(words.data()); });
        }
        // setRelationships(scores)
        if (!tables.is_null()) {
            float m[36];
            for (size_t a = 0; a < 6; ++a) {
                for (size_t b = 0; b < 6; ++b) {
                    m[a * 6 + b] = float(ssx_native::jsNumber(&aiDoc["relationships"]["scores"][a][b]));
                }
            }
            race_world_set_relationships(m);
            for (const auto& n : npcs) {
                inNpc(n, [&] { npc_set_relationships(m + n.slot * 6); });
            }
        }
        loadCoreRelationships(tables);
    }

    // main.js startRun(R): the replay restart
    bool aiActive = false;
    void startRun() {
        const json& R = meta["snapshot"];
        race_time_limit(ssx_native::jsInt(at(R, "timeLimit")));
        // web/freestyle-event.js applyFreestyleEvent
        int32_t words[12] = {};
        const std::string root = courseRoot();
        if (assets.exists(root + "freestyle-event.json")) {
            const json cfg = assets.doc(root + "freestyle-event.json");
            for (size_t k = 0; k < cfg["bonus_words"].size() && k < 12; ++k) {
                words[k] = ssx_native::jsInt(cfg["bonus_words"][k]);
            }
            set_race_bonus(words, ssx_native::jsInt(cfg["game_mode"]), ssx_native::jsInt(cfg["handler"]), ssx_native::jsInt(cfg["freestyle_kind"]),
                           uint32_t(ssx_native::jsInt(cfg["global_flags"])));
        } else {
            set_race_bonus(words, 0, 1, 0, 0);
        }
        reset_fog();
        reset_rider_lighting();
        reset_animation();
        reset_race();
        // a live start keeps the pad history (pv padCarry); a replay starts it settled
        if (!live) {
            reset_pad_history();
        }
        set_input_map(ssx_native::jsInt(at(R, "inputMap")));
        resetPhysics();
        if (!live) {
            set_camera_view(ssx_native::jsInt(at(R, "cameraView")));
        }
        // web/stage-collect.js collectApply
        const json& collect = at(R, "collect");
        if (truthy(collect)) {
            set_score_career_cash(ssx_native::jsInt(at(collect, "cash")));
            const json& st = at(collect, "state");
            set_stage_collect_state(uint32_t(ssx_native::jsInt(st[0])), uint32_t(ssx_native::jsInt(st[1])), uint32_t(ssx_native::jsInt(st[2])));
            for (const auto& r : at(collect, "rows")) {
                set_stage_collect_row(ssx_native::jsInt(r[0]), uint32_t(ssx_native::jsInt(r[1])), uint32_t(ssx_native::jsInt(r[2])));
            }
        }
        start_event();
        aiActive = hasAi && aiEnabled;
        if (aiActive) {
            aiStart(at(R, "ai"));
        }
        lastRescues = rider_state()[13];
        auto put = [](uint32_t* target, const json& v, size_t n) {
            for (size_t k = 0; k < n && k < v.size(); ++k) {
                target[k] = uint32_t(ssx_native::jsInt(v[k]));
            }
        };
        if (live) {
            // a live run keeps the words its start left
            return;
        }
        put(animation_rng_words(), at(R, "rng"), 6);
        put(visual_rng_words(), at(R, "visual"), 6);
        if (!at(R, "lcg").is_null()) {
            *visual_lcg_word() = uint32_t(ssx_native::jsInt(at(R, "lcg")));
        }
    }

    // ---- web/game-tick.js simulate / simulateInCore (pv coreTick), the replay's host state ----
    float lastRescues = 0;
    float lastResetPlacement = 0;
    bool teleportKnown = false;
    float teleportCount = 0;
    float state[16] = {};
    float posed[12] = {};
    float raceInfo[8] = {};
    bool hasPose = false;
    bool finished = false;
    bool resultsPending = false;
    float postFinishTicks = 0;
    float crossHeld = 0;
    float crossAge = 0;
    float worldState[396] = {};
    bool running = true;
    struct Finish {
        bool seen = false;
        int32_t score = 0;
        float ticks = 0;
        bool dnf = false;
    };

    // main.js gameHost.soloTickStart: a solo event's anchor RNG at its tick
    void soloTickStart() {
        const auto* e = ssx::rules::eventAnchor(code.c_str());
        if (!e || int(game_tick()) != e->tick) {
            return;
        }
        const int draws = ssx::rules::eventAnchorDraws(*e, who.base, who.cheat != 0);
        auto w = ssx::rules::wordsAfterDraws(0, uint32_t(e->start ? draws - e->start : draws));
        std::memcpy(animation_rng_words(), w.data(), 24);
    }

    Finish tick(const float* input) {
        Finish fin;
        if (aiActive) {
            const json& at_ = at(aiDoc, "anchor_tick");
            const int anchorTick = at_.is_number_integer() ? at_.get<int>() : -1;
            const int scores = at(aiDoc["world"], "rank_mode").is_number() && aiDoc["world"]["rank_mode"].get<double>() == 2 ? 1 : 0;
            std::memcpy(worldState, manager_begin_tick(anchorTick, scores), sizeof worldState);
        } else {
            soloTickStart();
        }
        const float* b = tick_begin(input, lastRescues, lastResetPlacement, 1, boneCount, teleportKnown ? 1 : 0, teleportKnown ? teleportCount : 0, 0);
        uint32_t flags;
        std::memcpy(&flags, b + 79, 4);
        tickFlags = flags;
        // the command's controller (4 passive air, 5 air): the keyboard's Simple mode routes new directions by it
        command14 = b[14];
        if (flags & 1) {
            lastRescues = b[80];
        }
        if (flags & 4) {
            std::memcpy(posed, b + 59, sizeof posed);
            hasPose = true;
        }
        std::memcpy(state, b + 24, sizeof state);
        std::memcpy(raceInfo, b + 71, sizeof raceInfo);
        if (flags & 2) {
            lastResetPlacement = b[81];
        }
        if ((flags & 8) && hasPose) {
            std::memcpy(posed, b + 59, sizeof posed);
        }
        teleportKnown = true;
        teleportCount = b[82];
        if (aiActive) {
            manager_end_tick();
        } else {
            tick_mid();
        }
        // the HUD owner update's read point (web/game-tick.js rec.meter: after every rider, before tick_end)
        if (afterMid) {
            afterMid();
        }
        const float* e = tick_end(hasPose ? 1 : 0, hasPose ? posed[7] : 0, hasPose ? posed[9] : 0, hasPose ? posed[10] : 0, hasPose ? posed[11] : 0, live ? 0 : 1, 0, 0, 0, 1,
                                  raceInfo[2] != 0 ? 1 : 0);
        if (e[0] == 1) {
            throw std::runtime_error("Missing posed physical frame");
        }
        uint32_t endFlags;
        std::memcpy(&endFlags, e + 20, 4);
        std::memset(camera, 0, sizeof camera);
        hasCamera = (endFlags & 1) != 0;
        if (hasCamera) {
            std::memcpy(camera, e + 1, sizeof camera);
        }
        if (endFlags & 8) {
            finished = true;
            postFinishTicks = 0;
            std::memcpy(&fin.score, e + 23, 4);
            fin.ticks = e[24];
            fin.dnf = e[25] != 0;
            fin.seen = true;
            tick_finish(0, aiActive ? 1 : 0, aiActive ? worldState[360] : 0, backcountry ? 1 : 0);
        }
        const bool timedOut = race_timed_out() != 0;
        float io[10] = {raceInfo[3], resultsPending ? 1.f : 0.f, finished ? 1.f : 0.f, postFinishTicks, crossHeld, crossAge, input[10] > 0 ? 1.f : 0.f, 0.f,
                        timedOut ? 1.f : 0.f, -1.f};
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
        return fin;
    }
    float camera[9] = {};
    bool hasCamera = false;
    // QA: tools/tas/page-run.mjs dump.json's row of this tick (the human's state, the shared RNG, each computer rider's state, the
    // visual RNG, the posed frame, animation_info, boost_info, the score object's hash), floats written to round-trip
    void dumpRow(std::string& out) {
        std::string row;
        auto f = [&](const float* p, int n) {
            char b[32];
            for (int k = 0; k < n; ++k) {
                std::snprintf(b, sizeof b, "%.9g,", double(p[k]));
                row += std::isfinite(p[k]) ? b : "null,";
            }
        };
        auto u = [&](const uint32_t* p, int n) {
            for (int k = 0; k < n; ++k) {
                row += std::to_string(p[k]) + ",";
            }
        };
        f(rider_state(), 16);
        u(animation_rng_words(), 6);
        if (aiActive) {
            for (const auto& n : npcs) {
                inNpc(n, [&] { f(rider_state(), 16); });
            }
        }
        u(visual_rng_words(), 6);
        f(pose_physical(), 12);
        f(animation_info(), 19);
        f(boost_info(), 8);
        row += std::to_string(fnv(0x811c9dc5u, score_object_dump(), 0x1d0 / 4));
        if (out.size() > 1) {
            out += ",";
        }
        out += "[" + row + "]";
    }
    // web/game-tick.js gameTrace (the page's ?simtrace hash of this tick)
    uint32_t traceHash() {
        float out[45] = {};
        std::memcpy(out, state, 16 * 4);
        if (hasPose) {
            std::memcpy(out + 16, posed, 12 * 4);
        }
        std::memcpy(out + 28, raceInfo, 8 * 4);
        if (hasCamera) {
            std::memcpy(out + 36, camera, 9 * 4);
        }
        uint32_t h = fnv(0x811c9dc5u, out, 45);
        auto rider = [&] {
            h = fnv(h, rider_state(), 16);
            h = fnv(h, pose_physical(), 12);
            h = fnv(h, rider_world_state(), 8);
        };
        rider();
        if (aiActive) {
            for (const auto& n : npcs) {
                inNpc(n, rider);
            }
        }
        h = fnv(h, visual_rng_words(), 6);
        h = fnv(h, animation_rng_words(), 6);
        h = fnv(h, score_object_dump(), 0x1d0 / 4);
        return h;
    }
};

}  // namespace ssx_native::session
