#pragma once
// The world batches the set pieces move, show and hide each frame (docs/native.md "N3"), from the core as the page reads it:
// - main.js frame: the pickups (pickup_visible), the event-dead batches (hidden from the event start), the moving instances
//   (web/moving-instances.js: moving_instances deltas, the copies' clones);
// - web/set-pieces-renderer.js update: the stage programs' instance draw states (stage_world_instances), the magnet pickups in flight
//   (stage_world_magnets), the MeshAnim break pieces (stage_world_meshanims on the hidden per-node batches).
// The result is the renderer's per-batch override map (world_renderer.hpp BatchOverride).
#include "../verify/core_api.hpp"
#include "json.hpp"
#include "world_renderer.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <vector>

extern "C" {
float* moving_instances();
uint32_t* stage_world_instances();
float* stage_world_magnets();
float* stage_world_meshanims();
}

namespace ssx_native {

class SetPieces {
public:
    // world: the course's world.json; eventStart: the run started an event (main.js startRun hides the event-dead batches);
    // stageWorld: the core's stage world is loaded (init_stage_world)
    SetPieces(const nlohmann::json& world, const std::array<double, 3>& origin, bool eventStart, bool stageWorld)
        : origin_(origin), eventStart_(eventStart), stageWorld_(stageWorld) {
        size_t index = 0;
        for (const auto& b : world["batches"]) {
            const size_t batch = index++;
            if (b.contains("pickup_resource")) {
                pickups_.push_back({batch, b["pickup_resource"].get<uint32_t>()});
            }
            if (b.contains("event_dead_resource")) {
                eventDead_.push_back(batch);
            }
            if (b.contains("moving_resource")) {
                moving_.push_back({batch, b["moving_resource"].get<uint32_t>()});
            }
            if (b.contains("script_resource")) {
                script_[b["script_resource"].get<uint32_t>()].push_back(batch);
            }
            if (b.contains("hidden_resource") && b.contains("meshanim_resource")) {
                pieces_[b["meshanim_resource"].get<uint32_t>()].push_back({batch, b["meshanim_node"].get<uint32_t>()});
            }
        }
    }

    const std::map<size_t, BatchOverride>& update() {
        out_.clear();
        // main.js frame: pickups shown while the core draws them
        for (const auto& p : pickups_) {
            if (!pickup_visible(p.resource)) {
                out_[p.batch].hidden = true;
            }
        }
        if (eventStart_) {
            for (size_t b : eventDead_) {
                out_[b].hidden = true;
            }
        }
        movingInstances();
        if (stageWorld_) {
            scriptStates();
            magnets();
            breakPieces();
        }
        return out_;
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
    // the mesh's rest matrixWorld: position -origin (+ offset), identity rotation
    Mat4d rest(double x = 0, double y = 0, double z = 0) const {
        Mat4d m{};
        m[0] = m[5] = m[10] = m[15] = 1;
        m[12] = x - origin_[0];
        m[13] = y - origin_[1];
        m[14] = z - origin_[2];
        return m;
    }
    // a native delta (three.js column-major) placed at the rest position (web/moving-instances.js place: e[12..14] -= origin;
    // set-pieces-renderer.js: e[12..14] += restPosition)
    Mat4d placed(const float* delta) const {
        Mat4d m{};
        for (int k = 0; k < 16; ++k) {
            m[size_t(k)] = delta[k];
        }
        m[12] -= origin_[0];
        m[13] -= origin_[1];
        m[14] -= origin_[2];
        return m;
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
    }
    // stage_world_magnets: [count, then (resource, x, y, z, target, reached, time, rest x, y, z)]: the pickup's batches follow the
    // magnet's translation (source cm, Z up) away from the authored one
    void magnets() {
        const float* f = stage_world_magnets();
        const size_t n = size_t(f[0]);
        for (size_t k = 0; k < n; ++k) {
            const float* m = f + 1 + 10 * k;
            const double d[3] = {double(m[1]) - double(m[7]), double(m[2]) - double(m[8]), double(m[3]) - double(m[9])};
            if (!d[0] && !d[1] && !d[2]) {
                continue;
            }
            auto it = script_.find(uint32_t(m[0]));
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
    // stage_world_meshanims: [count, then per MeshAnim: resource, node count, 16 floats per node]: the flying pieces' hidden
    // per-node batches drawn with their node deltas
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
                    // e[12..14] += the piece's rest position (-origin)
                    o.matrix = placed(f + at + 16 * p.node);
                }
            }
            at += 16 * size_t(nodes);
        }
    }

    std::array<double, 3> origin_;
    bool eventStart_;
    bool stageWorld_;
    std::vector<Pickup> pickups_;
    std::vector<size_t> eventDead_;
    std::vector<Pickup> moving_;
    std::map<size_t, unsigned> clones_;
    std::map<uint32_t, std::vector<size_t>> script_;
    std::map<uint32_t, std::vector<Piece>> pieces_;
    std::map<size_t, BatchOverride> out_;
};

}  // namespace ssx_native
