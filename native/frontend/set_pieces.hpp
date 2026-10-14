#pragma once
// The set pieces of the course scene, natively (docs/native.md "N3"), from the core as the page reads it each frame:
// - main.js frame: the pickups (pickup_visible), the event-dead batches (hidden from the event start), the moving instances
//   (web/moving-instances.js: moving_instances deltas, the copies' clones);
// - web/set-pieces-renderer.js update, one step per game tick up to the core's set-piece clock (set_piece_info()[1]):
//   - the LiveComp players (livecomp.hpp; the go tick, the section starts, the core's trigger / timer starts, the section clocks),
//     the log teeters, the attached set pieces (set_piece_attached, ParentModifier children of LiveComp parents);
//   - the flag cloth (flags.hpp; the section enters / leaves, the core's visual words) and the flags' static draws;
//   - the stage programs' instance draw states, the magnet pickups in flight, the MeshAnim break pieces, the entity log;
//   - the MultiSpline cables (web/set-piece-cables.js) while their owner lives and its chunk is resident.
// The result: the renderer's per-batch override map (world_renderer.hpp BatchOverride), the flag meshes and the cables.
#include "../verify/core_api.hpp"
#include "avalanche.hpp"
#include "ee_float.hpp"
#include "flags.hpp"
#include "json.hpp"
#include "livecomp.hpp"
#include "world_renderer.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <vector>

extern "C" {
float* moving_instances();
uint32_t* stage_world_instances();
float* stage_world_magnets();
float* stage_world_meshanims();
uint32_t* stage_world_entity_log();
uint32_t* stage_world_livecomps();
uint32_t* stage_world_section_clocks();
uint32_t* stage_world_flag_words();
float* set_piece_teeters();
float* set_piece_attached();
uint32_t* set_piece_multi_bits();
uint32_t* set_piece_triggers();
}

namespace ssx_native {

// What the set pieces need from the course's assets (read by the presenter)
struct SetPieceAssets {
    nlohmann::json world;
    // LIVECOMP/livecomp.json, LIVECOMP/attached.json, FLAGS/flags.json (null when absent)
    nlohmann::json liveComp;
    nlohmann::json attached;
    nlohmann::json flags;
    // SECTIONS/sections.json and ready-state.json (null without sections), rails.json
    nlohmann::json sections;
    nlohmann::json ready;
    nlohmann::json rails;
    // avalanches.json (null: none)
    nlohmann::json avalanches;
    std::string code;
    // an event course's start hides the event-dead batches; the core's stage world (PARTICLES) is loaded; its LiveComp starts drive
    // the players (init_stage_world with a livecomp document)
    bool eventStart = true;
    bool stageWorld = false;
    bool coreStarts = false;
};

class SetPieces {
public:
    SetPieces(const SetPieceAssets& a, const std::array<double, 3>& origin) : a_(a), origin_(origin), avalanche_(a.code, a.avalanches) {
        sectionsNative_ = a.sections.is_object() && a.ready.is_object();
        size_t index = 0;
        for (const auto& b : a.world["batches"]) {
            const size_t batch = index++;
            const bool hidden = b.contains("hidden_resource");
            if (b.contains("pickup_resource")) {
                pickups_.push_back({batch, b["pickup_resource"].get<uint32_t>()});
            }
            if (b.contains("event_dead_resource")) {
                eventDead_.push_back(batch);
            }
            if (b.contains("moving_resource")) {
                moving_.push_back({batch, b["moving_resource"].get<uint32_t>()});
                if (hidden) {
                    hiddenMoving_.insert(batch);
                }
            }
            if (b.contains("script_resource")) {
                script_[b["script_resource"].get<uint32_t>()].push_back(batch);
            }
            if (hidden && b.contains("meshanim_resource")) {
                pieces_[b["meshanim_resource"].get<uint32_t>()].push_back({batch, b["meshanim_node"].get<uint32_t>()});
            }
            if (hidden) {
                const uint32_t r = b["hidden_resource"].get<uint32_t>();
                hiddenOf_[r].push_back(batch);
                const int texture = b["texture"].get<int>();
                if ((texture == 138 || texture == 268) && !flagSource_.count(r)) {
                    flagSource_[r] = batch;
                }
            }
            if (b.contains("livecomp_resource")) {
                LiveMesh m;
                m.batch = batch;
                m.resource = b["livecomp_resource"].get<uint32_t>();
                m.node = b["livecomp_node"].get<uint32_t>();
                m.hidden = hidden;
                liveMeshes_.push_back(m);
            }
        }
        if (a.liveComp.is_object()) {
            for (const auto& x : a.liveComp["instances"]) {
                for (const auto& st : x["starts"]) {
                    if (st["trigger"] == "contact") {
                        contactOwners_.insert(livecomp::number(st["ownerResource"]));
                    }
                }
            }
        }
        if (a.attached.is_object()) {
            for (const auto& p : a.attached.value("parents", nlohmann::json::array())) {
                attachedResources_.insert(p["child"].get<uint32_t>());
                if (p.value("parentKind", std::string()) == "livecomp") {
                    JsParent j;
                    j.child = p["child"].get<uint32_t>();
                    j.parent = p["parent"].get<uint32_t>();
                    j.node = p["node"].get<uint32_t>();
                    j.rest = livecomp::rowsOf(p["childMatrix"]);
                    for (size_t k = 0; k < 4; ++k) {
                        j.offset[k] = p["offset"][k].get<float>();
                    }
                    j.pre = livecomp::mulRows(livecomp::kSI, livecomp::invertAffine(j.rest));
                    jsParents_.push_back(j);
                }
            }
            for (const auto& x : a.attached.value("splineLiveComps", nlohmann::json::array())) {
                attachedResources_.insert(x["resource"].get<uint32_t>());
            }
        }
        buildCables();
        reset();
    }

    const std::map<size_t, BatchOverride>& update() {
        out_.clear();
        step();
        // main.js frame: pickups shown while the core draws them
        for (const auto& p : pickups_) {
            if (!pickup_visible(p.resource)) {
                out_[p.batch].hidden = true;
            }
        }
        if (a_.eventStart) {
            for (size_t b : eventDead_) {
                out_[b].hidden = true;
            }
        }
        movingInstances();
        // the magnets move their pickups' batches first; a LiveComp player's matrix (collecta) replaces it
        if (a_.stageWorld) {
            magnetMeshes();
        }
        liveMeshes();
        if (a_.stageWorld) {
            scriptStates();
            breakPieces();
        }
        // the flags' static draws (a flag the manager does not hold) and the cloth meshes
        for (const auto& [r, on] : flagStatic_) {
            if (!on) {
                continue;
            }
            for (size_t b : hiddenOf_[r]) {
                out_[b].shown = true;
            }
        }
        for (auto& m : flagMeshes_) {
            if (m.uploaded && !changedSlots_.count(clothSlot(m.resource))) {
                continue;
            }
            if (flags_ && flags_->writeWorld(m.resource, m.positions)) {
                m.uploaded = true;
                m.revision++;
            }
        }
        cablesAlive();
        // web/avalanche-state.js: a follower hidden at the start shows while its tumbler drives it
        if (avalanche_.active()) {
            avalanche_.read();
            for (const auto& m : moving_) {
                if (avalanche_.follower(m.resource) && hiddenMoving_.count(m.batch) && avalanche_.hiddenPieceDrawn(m.resource)) {
                    out_[m.batch].shown = true;
                }
            }
        }
        return out_;
    }
    const Avalanche& avalanche() const {
        return avalanche_;
    }
    // web/set-pieces-renderer.js haloNodePosition: row 3 (source cm) of a JS-animated LiveComp node
    bool nodePosition(uint32_t resource, uint32_t node, float* out) {
        const auto* m = live_ ? live_->nodeMatrices(resource) : nullptr;
        if (!m || node >= m->size()) {
            return false;
        }
        const auto& r = (*m)[node][3];
        out[0] = r[0];
        out[1] = r[1];
        out[2] = r[2];
        return true;
    }
    const std::vector<FlagMesh>& flagMeshes() const {
        return flagMeshes_;
    }
    const std::vector<CableMesh>& cables() const {
        return cables_;
    }

private:
    struct Pickup {
        size_t batch;
        uint32_t resource;
    };
    struct Piece {
        size_t batch;
        uint32_t node;
    };
    struct LiveMesh {
        size_t batch = 0;
        uint32_t resource = 0;
        uint32_t node = 0;
        // a hidden batch (world.json hidden_resource) a LiveComp 'object' player draws while it lives
        bool hidden = false;
        bool inScene = false;
        // a player's matrix (else the rest draw)
        bool playing = false;
        Mat4d matrix{};
    };
    struct JsParent {
        uint32_t child = 0, parent = 0, node = 0;
        livecomp::Rows rest{};
        std::array<float, 4> offset{};
        livecomp::DRows pre{};
    };
    struct CableInfo {
        std::optional<int> chunk;
        bool activation = false;
        bool atStart = false;
    };

    // the mesh's rest matrixWorld: position -origin (+ offset), identity rotation
    Mat4d rest(double x = 0, double y = 0, double z = 0) const {
        Mat4d m{};
        m[0] = m[5] = m[10] = m[15] = 1;
        m[12] = x - origin_[0];
        m[13] = y - origin_[1];
        m[14] = z - origin_[2];
        return m;
    }
    // a native delta (three.js column-major) placed at the mesh's rest position (-origin)
    template <typename T>
    Mat4d placed(const T* delta) const {
        Mat4d m{};
        for (int k = 0; k < 16; ++k) {
            m[size_t(k)] = double(delta[k]);
        }
        m[12] -= origin_[0];
        m[13] -= origin_[1];
        m[14] -= origin_[2];
        return m;
    }
    static uint32_t readyBits(const nlohmann::json& entry, const char* key, bool& has) {
        has = entry.contains("bits") && entry["bits"].contains(key) && entry["bits"][key].is_number();
        return has ? entry["bits"][key].get<uint32_t>() : 0;
    }
    static float readyFloat(const nlohmann::json& entry, const char* key) {
        bool has = false;
        const uint32_t b = readyBits(entry, key, has);
        return has ? ee::fromBits(b) : entry["state"][key].get<float>();
    }
    int clothSlot(uint32_t resource) const {
        if (!flags_) {
            return -1;
        }
        auto it = flags_->instances.find(resource);
        return it == flags_->instances.end() ? -1 : it->second.slot;
    }

    // set-pieces-renderer.js reset(): a new race (the set-piece clock went back)
    void reset() {
        ticks_ = 0;
        sectionCursor_ = 0;
        startCursor_ = 0;
        contactCursor_ = 0;
        entityCursor_ = 0;
        fired_.clear();
        live_.reset();
        if (a_.liveComp.is_object()) {
            live_ = std::make_unique<livecomp::Animation>(a_.liveComp);
            if (a_.coreStarts) {
                live_->chainStarts = false;
            }
            if (sectionsNative_) {
                // race tick 0 = the ready savestate: the pieces built before the race keep their phase
                for (const auto& entry : a_.ready.value("livecomp", nlohmann::json::array())) {
                    auto it = live_->byResource.find(entry["resource"].get<uint32_t>());
                    if (it == live_->byResource.end()) {
                        continue;
                    }
                    const livecomp::Instance& inst = *it->second;
                    const livecomp::Start* st = nullptr;
                    for (const auto& s : inst.starts) {
                        if (!st && s.trigger == "section") {
                            st = &s;
                        }
                    }
                    if (!st) {
                        continue;
                    }
                    livecomp::State s = livecomp::construct(inst, st->words, [] { return 0u; }, live_->fps);
                    const auto& state = entry["state"];
                    for (const auto& [key, value] : state.items()) {
                        if (key == "mode") {
                            s.mode = value.get<int>();
                        } else if (key == "enabled") {
                            s.enabled = value.get<int>();
                        } else if (key == "done") {
                            s.done = value.get<int>();
                        } else if (key == "delay") {
                            s.delay = value.get<int>();
                        } else if (key == "rate") {
                            s.rate = readyFloat(entry, "rate");
                        } else if (key == "low") {
                            s.low = readyFloat(entry, "low");
                        } else if (key == "high") {
                            s.high = readyFloat(entry, "high");
                        } else if (key == "time") {
                            s.time = readyFloat(entry, "time");
                        } else if (key == "sampleTime") {
                            s.sampleTime = readyFloat(entry, "sampleTime");
                        } else if (key == "previous") {
                            s.previous = readyFloat(entry, "previous");
                        } else if (key == "unclamped") {
                            s.unclamped = readyFloat(entry, "unclamped");
                        }
                    }
                    s.dirty = true;
                    s.advanced = false;
                    live_->set(inst.resource, std::move(s));
                }
            }
        }
        for (auto& m : liveMeshes_) {
            m.playing = false;
            m.matrix = rest();
        }
        magnetBase_.clear();
        magnetPrev_.clear();
        avalanche_.reset();
        // the cables' owners (web/set-piece-cables.js CableLife.reset)
        cableAlive_.clear();
        for (const auto& [owner, i] : cableInfo_) {
            if (!i.activation || i.atStart) {
                cableAlive_[owner] = true;
            }
        }
        // the flags
        flagMeshes_.clear();
        flagStatic_.clear();
        flags_.reset();
        if (!a_.flags.is_object()) {
            return;
        }
        flags_ = std::make_unique<flags::Animation>(a_.flags);
        const bool coreFlags = sectionsNative_ && a_.ready.contains("flags") && a_.ready["flags"].is_object();
        flagWordCursor_ = 0;
        if (coreFlags) {
            flags_->random = [this] {
                const uint32_t* u = stage_world_flag_words();
                if (flagWordCursor_ < u[0]) {
                    return u[1 + flagWordCursor_++];
                }
                // the page's Math.random stand-in once the core's words run out (not reproduced: 0)
                return 0u;
            };
        }
        if (sectionsNative_ && a_.ready.contains("flags") && a_.ready["flags"].is_object()) {
            const auto& rf = a_.ready["flags"];
            const auto& w = rf["wind"];
            flags_->wind = {w["wind"].get<float>(), w["base"].get<float>(), w["delta"].get<float>(), w["timer"].get<float>()};
            if (rf.contains("wind_bits")) {
                const auto& b = rf["wind_bits"];
                flags_->wind = {ee::fromBits(b["wind"].get<uint32_t>()), ee::fromBits(b["base"].get<uint32_t>()), ee::fromBits(b["delta"].get<uint32_t>()),
                                ee::fromBits(b["timer"].get<uint32_t>())};
            }
            // the ready-state grids: their phases come from the savestate (the page draws Math.random words for the build: 0 here)
            const auto random = flags_->random;
            flags_->random = [] { return 0u; };
            for (const auto& slot : rf["slots"]) {
                const auto instances = slot["instances"].get<std::vector<uint32_t>>();
                if (instances.empty()) {
                    continue;
                }
                const int k = flags_->activate(instances[0]);
                if (k < 0) {
                    continue;
                }
                if (k != slot["slot"].get<int>()) {
                    throw std::runtime_error("Flag slot order differs from the ready state");
                }
                flags::Cloth& cloth = flags_->slots[size_t(k)];
                for (size_t i = 0; i < 4; ++i) {
                    cloth.phase[i] = slot["phase"][i].get<float>();
                }
                cloth.uvOffset = {slot["uvOffset"][0].get<float>(), slot["uvOffset"][1].get<float>()};
                cloth.parity = slot["parity"].get<int>();
                for (size_t i = 1; i < instances.size(); ++i) {
                    flags_->activate(instances[i]);
                }
            }
            flags_->random = random;
        } else {
            flags_->activateAll();
        }
        if (sectionsNative_) {
            for (uint32_t r : flags_->order) {
                flagStatic_[r] = true;
            }
        }
        for (uint32_t r : flags_->order) {
            addFlagMesh(r);
        }
    }
    void addFlagMesh(uint32_t resource) {
        const flags::Cloth* slot = flags_ ? flags_->clothOf(resource) : nullptr;
        if (!slot) {
            return;
        }
        for (const auto& m : flagMeshes_) {
            if (m.resource == resource) {
                return;
            }
        }
        auto src = flagSource_.find(resource);
        if (src == flagSource_.end()) {
            return;
        }
        FlagMesh m;
        m.resource = resource;
        m.sourceBatch = src->second;
        const size_t n = size_t(slot->width * slot->height);
        m.positions.assign(n * 3, 0.f);
        m.uv = slot->uv;
        m.colour.resize(n * 4);
        for (size_t i = 0; i < n; ++i) {
            m.colour[i * 4] = slot->colour[i * 4 + 1];
            m.colour[i * 4 + 1] = slot->colour[i * 4 + 2];
            m.colour[i * 4 + 2] = slot->colour[i * 4 + 3];
            m.colour[i * 4 + 3] = slot->colour[i * 4];
        }
        m.indices = flags::triangleIndices(slot->width, slot->height);
        flagMeshes_.push_back(std::move(m));
        flagStatic_[resource] = false;
    }
    void removeFlagMesh(uint32_t resource) {
        for (auto it = flagMeshes_.begin(); it != flagMeshes_.end(); ++it) {
            if (it->resource == resource) {
                flagMeshes_.erase(it);
                flagStatic_[resource] = true;
                return;
            }
        }
    }

    // the ticks up to the core's set-piece clock
    void step() {
        changedSlots_.clear();
        const float target = set_piece_info()[1];
        if (target < float(ticks_)) {
            reset();
        }
        const long steps = std::min<long>(long(target) - ticks_, 3600);
        const uint32_t* sections = sectionsNative_ ? set_piece_sections() : nullptr;
        const uint32_t sectionCount = sections ? sections[0] : 0;
        const uint32_t* starts = a_.coreStarts ? stage_world_livecomps() : nullptr;
        const uint32_t startCount = starts ? starts[0] : 0;
        const int goTick = a_.liveComp.is_object() ? a_.liveComp.value("go_tick", -1) : -1;
        for (long k = 0; k < steps; ++k) {
            if (live_) {
                if (ticks_ == 0 && !sectionsNative_) {
                    for (const auto& inst : live_->instances) {
                        for (const auto& st : inst.starts) {
                            if (st.trigger == "section") {
                                live_->fire("section", inst.resource);
                                break;
                            }
                        }
                    }
                }
                if (ticks_ == goTick) {
                    live_->fire("go");
                }
                live_->tickAll();
                if (!a_.coreStarts) {
                    // the trigger contacts of this tick (set_piece_triggers: tick, owner x 8 + slot): slot-2 programs of trigger volumes
                    const uint32_t* log = set_piece_triggers();
                    const uint32_t length = 2 * log[0];
                    while (contactCursor_ < length && log[1 + contactCursor_] <= uint32_t(ticks_)) {
                        const uint32_t resource = log[2 + contactCursor_] >> 3;
                        contactCursor_ += 2;
                        if (contactOwners_.count(resource) && !fired_.count(resource)) {
                            fired_.insert(resource);
                            live_->fire("contact", resource);
                        }
                    }
                }
                // the core's trigger / timer starts of this tick: [tick, resource, 11 words, key8, key6, draws] x 16 words
                while (startCursor_ < startCount && starts[1 + 16 * startCursor_] <= uint32_t(ticks_)) {
                    const uint32_t* e = starts + 1 + 16 * startCursor_;
                    ++startCursor_;
                    auto it = live_->byResource.find(e[1]);
                    if (it == live_->byResource.end()) {
                        continue;
                    }
                    std::array<uint32_t, 11> words{};
                    for (size_t i = 0; i < 11; ++i) {
                        words[i] = e[2 + i];
                    }
                    const uint32_t drawn[2] = {e[13], e[14]};
                    size_t next = 0;
                    const auto random = live_->random;
                    live_->random = [&] { return next < 2 ? drawn[next++] : 0u; };
                    live_->start(*it->second, words);
                    live_->random = random;
                }
            }
            // the section scan at the end of this tick (0x101B60): slot-1 starts, leaves
            while (sectionCursor_ < sectionCount && sections[2 + 6 * sectionCursor_] <= uint32_t(ticks_)) {
                const uint32_t* e = sections + 2 + 6 * sectionCursor_;
                ++sectionCursor_;
                const uint32_t resource = e[1];
                const uint32_t action = e[2];
                const uint32_t draws = e[4];
                const uint32_t word = e[5];
                if (cableInfo_.count(resource)) {
                    if (action == 1) {
                        cableAlive_[resource] = true;
                    } else if (action == 3) {
                        cableAlive_[resource] = false;
                    }
                }
                if (action == 1) {
                    if (live_) {
                        const auto random = live_->random;
                        if (draws) {
                            live_->random = [word] { return word; };
                        }
                        live_->fire("section", resource);
                        live_->random = random;
                    }
                    if (flags_ && flags_->activate(resource) >= 0) {
                        addFlagMesh(resource);
                    }
                } else if (action == 3 || action == 5) {
                    if (live_) {
                        live_->erase(resource);
                    }
                    if (flags_ && flags_->deactivate(resource)) {
                        removeFlagMesh(resource);
                    }
                }
            }
            if (flags_) {
                for (int s : flags_->tick(int(ticks_ + 1))) {
                    changedSlots_.insert(s);
                }
            }
            ++ticks_;
        }
        ticks_ = long(target);
        if (live_ && a_.stageWorld) {
            syncSectionClocks();
        }
        if (a_.stageWorld) {
            magnetPlayers();
        }
    }
    // stage_world_section_clocks: [count, then per entity: resource, mode, enabled, done, delay, rate, low, high, time, sampleTime]
    void syncSectionClocks() {
        const uint32_t* u = stage_world_section_clocks();
        const uint32_t n = u[0];
        for (uint32_t k = 0; k < n; ++k) {
            const uint32_t* e = u + 1 + 10 * k;
            livecomp::State* s = live_->state(e[0]);
            if (!s) {
                continue;
            }
            s->mode = int(int16_t(uint16_t(e[1] & 0xffff)));
            s->enabled = int32_t(e[2]);
            s->done = int32_t(e[3]);
            s->delay = int32_t(e[4]);
            s->rate = ee::fromBits(e[5]);
            s->low = ee::fromBits(e[6]);
            s->high = ee::fromBits(e[7]);
            s->time = ee::fromBits(e[8]);
            if (s->sampleTime != ee::fromBits(e[9])) {
                s->sampleTime = ee::fromBits(e[9]);
                s->dirty = true;
            }
        }
    }
    // stage_world_magnets: a LiveComp pickup (collecta) animates on the magnet matrix (its player's root row 3 moves)
    void magnetPlayers() {
        const float* f = stage_world_magnets();
        const size_t n = size_t(f[0]);
        magnetNow_.clear();
        for (size_t k = 0; k < n; ++k) {
            const float* m = f + 1 + 10 * k;
            const double d[3] = {double(m[1]) - double(m[7]), double(m[2]) - double(m[8]), double(m[3]) - double(m[9])};
            if (d[0] || d[1] || d[2]) {
                magnetNow_[uint32_t(m[0])] = {d[0], d[1], d[2]};
            }
        }
        // a magnet that ended takes its pickup back to the authored translation (offset 0)
        std::map<uint32_t, std::array<double, 3>> apply = magnetNow_;
        for (uint32_t r : magnetPrev_) {
            apply.emplace(r, std::array<double, 3>{0, 0, 0});
        }
        magnetPrev_.clear();
        for (const auto& [r, d] : magnetNow_) {
            magnetPrev_.insert(r);
        }
        if (!live_) {
            return;
        }
        for (const auto& [r, d] : apply) {
            auto it = live_->byResource.find(r);
            if (it == live_->byResource.end()) {
                continue;
            }
            livecomp::Instance& inst = *it->second;
            auto b = magnetBase_.find(r);
            if (b == magnetBase_.end()) {
                b = magnetBase_.emplace(r, inst.matrix[3]).first;
            }
            const auto& base = b->second;
            inst.matrix[3] = {float(base[0] + d[0]), float(base[1] + d[1]), float(base[2] + d[2]), base[3]};
            if (livecomp::State* s = live_->state(r)) {
                s->dirty = true;
            }
        }
    }
    // web/moving-instances.js updateMovingInstances: key = resource + copy x 2^20
    void movingInstances() {
        if (moving_.empty()) {
            return;
        }
        const float* v = moving_instances();
        const size_t count = size_t(v[0]);
        std::map<double, const float*> deltas;
        for (size_t i = 0; i < count; ++i) {
            deltas[double(v[1 + i * 17])] = v + 2 + i * 17;
        }
        constexpr double kCopy = 1048576;
        for (const auto& m : moving_) {
            auto it = deltas.find(double(m.resource));
            if (it != deltas.end()) {
                BatchOverride& o = out_[m.batch];
                o.moved = true;
                o.matrix = placed(it->second);
            }
            // the clones made so far keep their copy numbers; a copy without a delta this frame is hidden
            unsigned& known = clones_[m.batch];
            for (unsigned copy = 1; deltas.count(double(m.resource) + copy * kCopy) || copy <= known; ++copy) {
                auto d = deltas.find(double(m.resource) + copy * kCopy);
                if (d == deltas.end()) {
                    continue;
                }
                known = std::max(known, copy);
                const bool meshVisible = !(out_.count(m.batch) && out_[m.batch].hidden);
                if (meshVisible) {
                    out_[m.batch].copies.push_back(placed(d->second));
                }
            }
        }
    }
    // the log teeters, the LiveComp players' node deltas, the attached set pieces (set-pieces-renderer.js update)
    void liveMeshes() {
        std::map<uint32_t, const float*> teeter;
        {
            const float* f = set_piece_teeters();
            const size_t n = size_t(f[0]);
            for (size_t k = 0; k < n; ++k) {
                teeter[uint32_t(f[4 + 17 * k])] = f + 5 + 17 * k;
            }
        }
        for (auto& m : liveMeshes_) {
            auto t = teeter.find(m.resource);
            if (t == teeter.end() || m.node != 1) {
                continue;
            }
            m.playing = true;
            m.matrix = placed(t->second);
        }
        if (live_) {
            static const std::regex heliInAir("os609_full_version_inair$");
            for (auto& m : liveMeshes_) {
                const auto* deltas = live_->nodeDeltas(m.resource);
                auto inst = live_->byResource.find(m.resource);
                const std::string draw = inst != live_->byResource.end() ? inst->second->draw : std::string();
                const bool heli = draw == "none" && inst != live_->byResource.end() && std::regex_search(inst->second->name, heliInAir);
                const bool has = deltas && m.node < deltas->size();
                if (m.hidden && (draw == "object" || heli)) {
                    m.inScene = has;
                }
                if (!has) {
                    // a section leave destroyed the player (0x34FD90): back to the static draw
                    if (sectionsNative_ && m.playing && !teeter.count(m.resource)) {
                        m.playing = false;
                        m.matrix = rest();
                    }
                    continue;
                }
                m.playing = true;
                m.matrix = placed((*deltas)[m.node].data());
            }
        }
        attachedApply();
        for (const auto& m : liveMeshes_) {
            BatchOverride& o = out_[m.batch];
            if (m.hidden && m.inScene) {
                o.shown = true;
            }
            if (m.playing) {
                o.moved = true;
                o.matrix = m.matrix;
            }
        }
    }
    // web/attached-setpieces.js apply: the LiveComp-parented children (0x357108) and the core's attached deltas
    void attachedApply() {
        if (attachedResources_.empty()) {
            return;
        }
        struct Slot {
            uint32_t resource;
            int node;
            std::array<double, 16> delta;
        };
        std::vector<Slot> frame;
        for (const auto& p : jsParents_) {
            const auto* nodes = live_ ? live_->nodeMatrices(p.parent) : nullptr;
            if (!nodes || p.node >= nodes->size()) {
                continue;
            }
            const auto& n = (*nodes)[p.node];
            std::array<double, 4> w3{};
            for (size_t k = 0; k < 4; ++k) {
                using namespace ee;
                w3[k] = vuAdd(n[3][k], vuAdd(vuAdd(vuAdd(mul(n[0][k], p.offset[0]), mul(n[1][k], p.offset[1])), mul(n[2][k], p.offset[2])), mul(n[3][k], p.offset[3])));
            }
            double b[16];
            for (size_t r = 0; r < 4; ++r) {
                const auto& a = p.pre[r];
                for (size_t k = 0; k < 4; ++k) {
                    b[4 * r + k] = a[0] * n[0][k] + a[1] * n[1][k] + a[2] * n[2][k] + a[3] * w3[k];
                }
            }
            Slot s{p.child, -1, {}};
            for (size_t r = 0; r < 4; ++r) {
                for (size_t k = 0; k < 4; ++k) {
                    // stored in a Float32Array
                    s.delta[4 * r + k] = float(b[4 * r] * livecomp::kS[0][k] + b[4 * r + 1] * livecomp::kS[1][k] + b[4 * r + 2] * livecomp::kS[2][k] +
                                               b[4 * r + 3] * livecomp::kS[3][k]);
                }
            }
            frame.push_back(s);
        }
        {
            const float* f = set_piece_attached();
            const size_t n = size_t(f[0]);
            for (size_t k = 0; k < n; ++k) {
                const float* e = f + 1 + 18 * k;
                Slot s{uint32_t(e[0]), int(e[1]), {}};
                for (size_t i = 0; i < 16; ++i) {
                    s.delta[i] = e[2 + i];
                }
                frame.push_back(s);
            }
        }
        auto find = [&](uint32_t resource, int node) -> const Slot* {
            for (auto it = frame.rbegin(); it != frame.rend(); ++it) {
                if (it->resource == resource && it->node == node) {
                    return &*it;
                }
            }
            return nullptr;
        };
        for (auto& m : liveMeshes_) {
            if (!attachedResources_.count(m.resource)) {
                continue;
            }
            const Slot* s = find(m.resource, int(m.node));
            if (!s) {
                s = find(m.resource, -1);
            }
            if (!s) {
                continue;
            }
            m.playing = true;
            m.matrix = placed(s->delta.data());
        }
    }
    // stage_world_instances: [count, then (resource, drawn, node state, flags)]; an instance the core no longer lists is shown again
    void scriptStates() {
        const uint32_t* u = stage_world_instances();
        const uint32_t count = u[0];
        for (uint32_t k = 0; k < count; ++k) {
            const uint32_t r = u[1 + 4 * k];
            const bool drawn = u[2 + 4 * k] != 0;
            if (drawn) {
                continue;
            }
            auto it = script_.find(r);
            if (it == script_.end()) {
                continue;
            }
            for (size_t b : it->second) {
                out_[b].hidden = true;
            }
        }
        // entity events: a destroyed LiveComp (or a MeshAnim on its instance) stops its player
        const uint32_t* log = stage_world_entity_log();
        const uint32_t events = log[0];
        if (entityCursor_ > events) {
            entityCursor_ = 0;
        }
        for (; entityCursor_ < events; ++entityCursor_) {
            const uint32_t* e = log + 1 + 3 * entityCursor_;
            if ((e[2] == 1 || e[2] == 2) && live_) {
                live_->erase(e[1]);
            }
        }
    }
    // the magnet pickups' batches follow the magnet's translation (source cm, Z up)
    void magnetMeshes() {
        for (const auto& [r, d] : magnetNow_) {
            auto it = script_.find(r);
            if (it == script_.end()) {
                continue;
            }
            for (size_t b : it->second) {
                BatchOverride& o = out_[b];
                o.moved = true;
                o.matrix = rest(d[0] / 100, d[2] / 100, -d[1] / 100);
            }
        }
    }
    // stage_world_meshanims: the flying pieces' hidden per-node batches drawn with their node deltas
    void breakPieces() {
        if (pieces_.empty()) {
            return;
        }
        const float* f = stage_world_meshanims();
        const size_t n = size_t(f[0]);
        size_t at = 1;
        for (size_t k = 0; k < n; ++k) {
            const uint32_t r = uint32_t(f[at]);
            const uint32_t nodes = uint32_t(f[at + 1]);
            at += 2;
            auto it = pieces_.find(r);
            if (it != pieces_.end()) {
                for (const auto& p : it->second) {
                    if (p.node >= nodes) {
                        continue;
                    }
                    BatchOverride& o = out_[p.batch];
                    o.shown = true;
                    o.moved = true;
                    o.matrix = placed(f + at + 16 * p.node);
                }
            }
            at += 16 * size_t(nodes);
        }
    }

    // web/set-piece-cables.js CABLES: the MultiSpline owners with a cable (owner instance, the rails.json path)
    void buildCables() {
        static const std::map<std::string, std::vector<std::pair<uint32_t, uint32_t>>> kCables = {
            {"ARA1", {{407560, 1544}, {711688, 1800}}},
            {"BRA2", {{707856, 16}, {252432, 528}, {820240, 272}}},
            {"ABA1", {{57093, 16645}}},
            {"CBA2", {{152086, 11542}}},
            {"CHP2", {{226327, 791}}},
            {"ESS3", {{175664, 42032}}},
            {"EBA3", {{91433, 297}, {183081, 41}}},
            {"EHP3", {{38444, 24108}}},
        };
        auto list = kCables.find(a_.code);
        if (list == kCables.end() || !a_.rails.is_object()) {
            return;
        }
        std::set<uint32_t> atStart;
        if (a_.sections.is_object() && a_.sections.contains("initial")) {
            for (const auto& r : a_.sections["initial"].value("active_after_tick0_scan", nlohmann::json::array())) {
                atStart.insert(r.get<uint32_t>());
            }
        }
        for (const auto& [owner, rail] : list->second) {
            CableInfo info;
            info.atStart = atStart.count(owner) > 0;
            if (a_.sections.is_object()) {
                for (const auto& x : a_.sections["instances"]) {
                    if (x["resource"].get<uint32_t>() == owner) {
                        if (x["chunk"].is_array()) {
                            info.chunk = x["chunk"][1].get<int>();
                        }
                        info.activation = x.value("activation", false);
                    }
                }
            }
            cableInfo_[owner] = info;
            for (const auto& r : a_.rails["rails"]) {
                if (r["packed_id"].get<uint32_t>() != rail) {
                    continue;
                }
                cables_.push_back(cableGeometry(owner, r));
            }
        }
    }
    // cableGeometry: per segment POINTS - 1 quads (own end, the other end, corner (side, end)); the VU's t sequence
    CableMesh cableGeometry(uint32_t owner, const nlohmann::json& rail) const {
        constexpr int kPoints = 10;
        CableMesh c;
        c.owner = owner;
        const float step = float(1.0 / double(float(kPoints - 1)));
        uint32_t q = 0;
        double lo[3] = {1e300, 1e300, 1e300};
        double hi[3] = {-1e300, -1e300, -1e300};
        for (const auto& s : rail["segments"]) {
            const auto& co = s["source"]["coefficients"];
            std::array<std::array<float, 3>, kPoints> p{};
            float t = 0;
            for (int k = 0; k < kPoints; ++k) {
                double v[3];
                for (size_t i = 0; i < 3; ++i) {
                    v[i] = ((co[0][i].get<double>() * t + co[1][i].get<double>()) * t + co[2][i].get<double>()) * t + co[3][i].get<double>();
                }
                p[size_t(k)] = {float(v[0] / 100 - origin_[0]), float(v[2] / 100 - origin_[1]), float(-v[1] / 100 - origin_[2])};
                t = float(t + step);
            }
            for (int k = 1; k < kPoints; ++k, ++q) {
                const auto& a = p[size_t(k - 1)];
                const auto& b = p[size_t(k)];
                for (int v = 0; v < 4; ++v) {
                    const int end = v & 1;
                    const float side = v < 2 ? 1.f : -1.f;
                    const auto& mine = end ? b : a;
                    const auto& theirs = end ? a : b;
                    c.own.insert(c.own.end(), mine.begin(), mine.end());
                    c.other.insert(c.other.end(), theirs.begin(), theirs.end());
                    c.corner.insert(c.corner.end(), {side, float(end)});
                    for (size_t i = 0; i < 3; ++i) {
                        lo[i] = std::min(lo[i], double(mine[i]));
                        hi[i] = std::max(hi[i], double(mine[i]));
                    }
                }
                c.indices.insert(c.indices.end(), {q * 4, q * 4 + 1, q * 4 + 2, q * 4 + 1, q * 4 + 3, q * 4 + 2});
            }
        }
        for (size_t i = 0; i < 3; ++i) {
            c.centre[i] = (lo[i] + hi[i]) * 0.5;
        }
        return c;
    }
    // CableLife.lives: the core's MultiSpline state when it runs one for the owner, else the section lifetime; and the owner's chunk
    void cablesAlive() {
        if (cables_.empty()) {
            return;
        }
        std::map<uint32_t, bool> active;
        {
            const uint32_t* u = set_piece_multi_bits();
            size_t at = 1;
            for (uint32_t k = 0; k < u[0]; ++k) {
                active[u[at]] = u[at + 1] != 0;
                at += 4 + 16 * size_t(u[at + 2]);
            }
        }
        std::map<int, bool> resident;
        if (const uint32_t* c = section_chunks()) {
            for (uint32_t k = 0; k < c[0]; ++k) {
                resident[int(c[1 + 2 * k])] = c[2 + 2 * k] == 3;
            }
        }
        for (auto& c : cables_) {
            const auto& info = cableInfo_[c.owner];
            auto m = active.find(c.owner);
            bool alive;
            if (m != active.end()) {
                alive = m->second;
            } else {
                auto a = cableAlive_.find(c.owner);
                alive = a != cableAlive_.end() ? a->second : !info.activation;
            }
            bool chunkOk = true;
            if (info.chunk) {
                auto r = resident.find(*info.chunk);
                chunkOk = !(r != resident.end() && !r->second);
            }
            c.visible = alive && chunkOk;
        }
    }

    SetPieceAssets a_;
    std::array<double, 3> origin_;
    bool sectionsNative_ = false;
    std::vector<Pickup> pickups_;
    std::vector<size_t> eventDead_;
    std::vector<Pickup> moving_;
    std::set<size_t> hiddenMoving_;
    Avalanche avalanche_;
    std::map<size_t, unsigned> clones_;
    std::map<uint32_t, std::vector<size_t>> script_;
    std::map<uint32_t, std::vector<Piece>> pieces_;
    std::map<uint32_t, std::vector<size_t>> hiddenOf_;
    std::map<uint32_t, size_t> flagSource_;
    std::vector<LiveMesh> liveMeshes_;
    std::set<uint32_t> contactOwners_;
    std::set<uint32_t> attachedResources_;
    std::vector<JsParent> jsParents_;
    std::unique_ptr<livecomp::Animation> live_;
    std::unique_ptr<flags::Animation> flags_;
    std::map<uint32_t, std::array<float, 4>> magnetBase_;
    std::map<uint32_t, std::array<double, 3>> magnetNow_;
    std::set<uint32_t> magnetPrev_;
    std::map<uint32_t, bool> flagStatic_;
    std::vector<FlagMesh> flagMeshes_;
    std::set<int> changedSlots_;
    std::map<uint32_t, CableInfo> cableInfo_;
    std::map<uint32_t, bool> cableAlive_;
    std::vector<CableMesh> cables_;
    std::set<uint32_t> fired_;
    long ticks_ = 0;
    uint32_t sectionCursor_ = 0;
    uint32_t startCursor_ = 0;
    uint32_t contactCursor_ = 0;
    uint32_t entityCursor_ = 0;
    uint32_t flagWordCursor_ = 0;
    std::map<size_t, BatchOverride> out_;
};

}  // namespace ssx_native
