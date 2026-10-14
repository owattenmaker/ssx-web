#pragma once
// The set pieces' camera-facing quads in the encoded pass (docs/native.md "N3"), built per drawn frame:
// - web/set-piece-particles.js: the stage programs' particle effects (particles.hpp), one batch per (texture, blend, kind) in the
//   page's mesh creation order;
// - web/fog-puffs.js: the kind-5 fog-particle puffs (0x22A270 cull, 0x2DC190 build, 0x2DBF98 far-to-near order) while their
//   instance's texture chunk is resident;
// - web/set-piece-halos.js: the HaloModifier glow sprites (stage_world_halos; a LiveComp node's position from its player).
#include "avalanche.hpp"
#include "draw_order.hpp"
#include "json.hpp"
#include "particles.hpp"
#include "rider_fx.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <map>
#include <optional>
#include <vector>

extern "C" {
float* stage_world_halos();
uint32_t* stage_world_crowd();
float* set_piece_info();
}

namespace ssx_native {

class SetPieceFx {
public:
    // particlesDoc: PARTICLES/particles.json (null: no stage world); fogPuffs: the course's fog-puffs.json (null: none)
    // crowd: CROWD/crowd.json (web/crowd-2d.js; null: none)
    SetPieceFx(const nlohmann::json& particlesDoc, const nlohmann::json& fogPuffs, const nlohmann::json& crowd, const std::array<double, 3>& origin)
        : origin_(origin) {
        if (crowd.is_object() && particlesDoc.is_object()) {
            crowd_ = true;
            for (const auto& a : crowd["flashes"]) {
                Area area;
                for (size_t k = 0; k < 3; ++k) {
                    area.centre[k] = a["centre"][k].get<double>();
                    area.axis1[k] = a["axis1"][k].get<double>();
                    area.axis2[k] = a["axis2"][k].get<double>();
                }
                areas_[a["resource"].get<uint32_t>()] = area;
            }
        }
        if (particlesDoc.is_object()) {
            stageWorld_ = true;
            combos_ = particles::combinations(particlesDoc);
        }
        if (fogPuffs.is_object()) {
            layoutPuffs(fogPuffs);
        }
    }
    // nodePosition(resource, node, out): the source-cm row 3 of a LiveComp player's node (set_pieces.hpp); resident(chunk): false
    // when the original has streamed the chunk out
    std::function<bool(uint32_t, uint32_t, float*)> nodePosition = [](uint32_t, uint32_t, float*) { return false; };
    std::function<std::optional<bool>(int)> resident = [](int) { return std::optional<bool>(); };
    // the avalanches (set_pieces.hpp owns them; null: none)
    const Avalanche* avalanche = nullptr;

    // the frame's batches for the displayed camera (three's view and projection, double column-major)
    const std::vector<QuadBatch>& quads(const double* view, const double* projection, double near, double far) {
        out_.clear();
        if (stageWorld_) {
            particleBatches();
            haloBatches();
        }
        if (avalanche && avalanche->active()) {
            avalancheBatches();
        }
        if (!puffs_.empty()) {
            puffBatch(view, projection, near, far);
        }
        return out_;
    }

    // 0x2DBAC0: the crowd texture's frame (the animator steps once every 3 game ticks, a 16-frame loop); -1 without a crowd
    int crowdFrame() const {
        if (!crowd_) {
            return -1;
        }
        return int(std::floor(set_piece_info()[1] / 3)) % 16;
    }
    // the camera flashes alive in the core's 40-entry ring: [128 slots, n, n resources, 40 x (resource, c1, c2, life)]
    const std::vector<float>& flashes() {
        flashes_.clear();
        if (!crowd_) {
            return flashes_;
        }
        const uint32_t* U = stage_world_crowd();
        size_t at = 128;
        at += 1 + U[at];
        for (int k = 0; k < 40; ++k, at += 4) {
            auto a = areas_.find(U[at]);
            if (int32_t(U[at + 3]) <= 0 || a == areas_.end()) {
                continue;
            }
            float c1, c2;
            std::memcpy(&c1, U + at + 1, 4);
            std::memcpy(&c2, U + at + 2, 4);
            double p[3];
            for (size_t i = 0; i < 3; ++i) {
                p[i] = a->second.centre[i] + a->second.axis1[i] * c1 + a->second.axis2[i] * c2;
            }
            flashes_.insert(flashes_.end(), {float(p[0] / 100 - origin_[0]), float(p[2] / 100 - origin_[1]), float(-p[1] / 100 - origin_[2])});
        }
        return flashes_;
    }

private:
    struct Area {
        std::array<double, 3> centre{}, axis1{}, axis2{};
    };
    struct Puff {
        std::array<float, 3> pos;
        float size;
        std::array<uint8_t, 3> rgb;
        size_t owner;
    };
    struct PuffInstance {
        std::array<float, 24> corners;
        std::array<float, 3> centre;
        std::optional<int> chunk;
    };

    std::array<double, 3> neg() const {
        return {-origin_[0], -origin_[1], -origin_[2]};
    }

    // web/set-piece-particles.js update: the effects grouped by (texture, blend, kind) in first-appearance order
    void particleBatches() {
        const auto effects = particles::readEffects();
        std::vector<particles::Combo> keys;
        std::map<particles::Combo, std::vector<double>> sprites;
        for (const auto& e : effects) {
            const particles::Combo key{e.texture, e.blend, e.kind};
            if (!sprites.count(key)) {
                keys.push_back(key);
            }
            auto& list = sprites[key];
            if (e.kind == 0) {
                particles::burst(e, list);
            } else {
                particles::trail(e, list);
            }
        }
        for (const auto& key : keys) {
            const auto& list = sprites[key];
            const size_t n = list.size() / particles::kSpriteFloats;
            if (!n) {
                continue;
            }
            auto c = std::find(combos_.begin(), combos_.end(), key);
            if (c == combos_.end()) {
                // a combination first seen now: its mesh is made after the others
                combos_.push_back(key);
                c = combos_.end() - 1;
            }
            const auto [texture, blend, kind] = key;
            QuadBatch q;
            q.variant = kind == 1 ? 1 : 0;
            q.blend = blend == 1 ? 0 : (blend == 2 ? 2 : 1);
            q.texture = "p" + std::to_string(texture);
            // 0x364240: rank 1, the texture and the inherited 'spec' (ps2-draw-order.js setPieceParticle, SUBMIT.setPiece)
            q.order = draw_order::drawOrder({7, 0, 4, int(texture), 46}, 1);
            q.id = 3000 + int(c - combos_.begin());
            q.position = neg();
            q.instances.reserve(n * 12);
            for (size_t i = 0; i < n; ++i) {
                const double* s = list.data() + i * particles::kSpriteFloats;
                // sprite centres: course coordinates (source cm / 100, (x, z, -y)) under the group at -origin
                q.instances.insert(q.instances.end(), {float(s[0] / 100 - origin_[0]), float(s[2] / 100 - origin_[1]), float(-s[1] / 100 - origin_[2]),
                                                       float(s[3] / 100), 0, 0, 0, 0, float(s[4] / 128), float(s[5] / 128), float(s[6] / 128),
                                                       float(s[7] / 128)});
            }
            out_.push_back(std::move(q));
        }
    }

    // web/avalanche-trails.js update: one batch per texture; sprites with alpha 0 leave the destination as it is (skipped)
    void avalancheBatches() {
        int order = 0;
        for (const auto& [id, list] : avalanche->trails()) {
            QuadBatch q;
            q.variant = 5;
            q.blend = 0;
            q.texture = "p" + std::to_string(id);
            q.order = draw_order::drawOrder({7, 0, 4, id, -1}, 5);
            q.id = 2950 + order++;
            q.position = neg();
            for (size_t i = 0; i + 8 <= list.size(); i += 8) {
                const double* s = list.data() + i;
                if (!(s[7] > 0)) {
                    continue;
                }
                q.instances.insert(q.instances.end(), {float(s[0] / 100 - origin_[0]), float(s[2] / 100 - origin_[1]), float(-s[1] / 100 - origin_[2]),
                                                       float(s[3] / 100), 0, 0, 0, 0, float(s[4] / 128), float(s[5] / 128), float(s[6] / 128),
                                                       float(s[7] / 128)});
            }
            if (!q.instances.empty()) {
                out_.push_back(std::move(q));
            }
        }
    }
    // web/set-piece-halos.js update: [count, then 12 floats: resource, key, node, r, g, b, a, size, angle, P (NaN: a LiveComp node)]
    void haloBatches() {
        const float* f = stage_world_halos();
        const size_t n = size_t(f[0]);
        const float deg = ee::fromBits(0x3c8efa36);
        std::map<int, QuadBatch> batches;
        for (size_t k = 0; k < n; ++k) {
            const float* h = f + 1 + 12 * k;
            const int key = int(h[1]);
            if (key < 0 || key > 5) {
                continue;
            }
            float P[3];
            if (std::isnan(h[9])) {
                if (!nodePosition(uint32_t(h[0]), uint32_t(h[2]), P)) {
                    continue;
                }
            } else {
                P[0] = h[9];
                P[1] = h[10];
                P[2] = h[11];
            }
            const float angleDeg = h[8];
            const float angle = angleDeg == 0 ? 0.f : float(angleDeg * deg);
            const int copies = angleDeg != 0 && (key == 1 || key == 4) ? 2 : 1;
            auto byte = [](float v) { return std::trunc(float(v * 128.f)); };
            QuadBatch& q = batches[key];
            if (q.texture.empty()) {
                q.variant = 3;
                q.blend = 1;
                q.texture = "h" + std::to_string(37 + key);
                q.order = draw_order::drawOrder({8, 1023, 0, 37 + key, -1}, 3);
                q.id = 2800 + key;
                q.position = neg();
            }
            for (int copy = 0; copy < copies && q.instances.size() / 12 < 64; ++copy) {
                const double a = copy ? -double(angle) : double(angle);
                q.instances.insert(q.instances.end(), {float(double(P[0]) / 100 - origin_[0]), float(double(P[2]) / 100 - origin_[1]),
                                                       float(-double(P[1]) / 100 - origin_[2]), float(double(h[7]) / 100), float(std::cos(a)),
                                                       float(std::sin(a)), angle == 0 ? 1.f : -1.f, 0, float(byte(h[3]) / 128), float(byte(h[4]) / 128),
                                                       float(byte(h[5]) / 128), float(byte(h[6]) / 128)});
            }
        }
        for (auto& [key, q] : batches) {
            out_.push_back(std::move(q));
        }
    }

    // fogPuffLayout: the puffs in scene space, the instances' boxes (VU0 0x0AC8 corner order) and centres
    void layoutPuffs(const nlohmann::json& doc) {
        auto num = [](const nlohmann::json& v) { return v.is_string() ? std::stod(v.get<std::string>()) : v.get<double>(); };
        for (const auto& x : doc["instances"]) {
            const size_t j = puffInstances_.size();
            double m[16];
            for (size_t k = 0; k < 16; ++k) {
                m[k] = num(x["matrix"][k]);
            }
            const int repeat = std::max(1, int(num(x["repeat"])));
            for (int r = 0; r < repeat; ++r) {
                for (const auto& q : x["puffs"]) {
                    const double v[3] = {num(q[0]), num(q[1]), num(q[2])};
                    double p[3];
                    // Vector3.applyMatrix4 (w 1, divided by w)
                    const double w = m[3] * v[0] + m[7] * v[1] + m[11] * v[2] + m[15];
                    for (int c = 0; c < 3; ++c) {
                        p[c] = (m[c] * v[0] + m[4 + c] * v[1] + m[8 + c] * v[2] + m[12 + c]) / w;
                    }
                    Puff puff;
                    puff.pos = {float(p[0] / 100 - origin_[0]), float(p[2] / 100 - origin_[1]), float(-p[1] / 100 - origin_[2])};
                    puff.size = float(num(q[6]) / 100);
                    for (int c = 0; c < 3; ++c) {
                        puff.rgb[size_t(c)] = uint8_t(int(std::trunc(float(float(num(q[3 + c])) * 128.f))));
                    }
                    puff.owner = j;
                    puffs_.push_back(puff);
                }
            }
            PuffInstance inst;
            const double lo[3] = {num(x["box_min"][0]), num(x["box_min"][1]), num(x["box_min"][2])};
            const double hi[3] = {num(x["box_max"][0]), num(x["box_max"][1]), num(x["box_max"][2])};
            size_t q = 0;
            for (double z : {lo[2], hi[2]}) {
                for (double y : {lo[1], hi[1]}) {
                    for (double xx : {lo[0], hi[0]}) {
                        inst.corners[q++] = float(xx / 100 - origin_[0]);
                        inst.corners[q++] = float(z / 100 - origin_[1]);
                        inst.corners[q++] = float(-y / 100 - origin_[2]);
                    }
                }
            }
            float c[3];
            for (int k = 0; k < 3; ++k) {
                c[k] = float(float(float(lo[k]) + float(hi[k])) * 0.5f);
            }
            inst.centre = {float(c[0] / 100.0 - origin_[0]), float(c[2] / 100.0 - origin_[1]), float(-c[1] / 100.0 - origin_[2])};
            if (x.contains("chunk") && !x["chunk"].is_null()) {
                inst.chunk = int(num(x["chunk"]));
            }
            puffInstances_.push_back(inst);
        }
    }
    // buildFogSprites: the sprites 0x2DC190 builds for this camera, far to near
    void puffBatch(const double* view, const double* projection, double near, double far) {
        // the frustum clip matrix of 0xDB8: the projection with its z row mapping near / far to -w / +w, times the view
        double clip[16];
        for (int k = 0; k < 16; ++k) {
            clip[k] = projection[k];
        }
        clip[2] = 0;
        clip[6] = 0;
        clip[10] = -(far + near) / (far - near);
        clip[14] = (-2 * far * near) / (far - near);
        double M[16];
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                double s = 0;
                for (int k = 0; k < 4; ++k) {
                    s += clip[k * 4 + r] * view[c * 4 + k];
                }
                M[c * 4 + r] = s;
            }
        }
        auto culled = [&](const PuffInstance& inst) {
            int all = 63;
            for (int k = 0; k < 8; ++k) {
                const double x = inst.corners[size_t(3 * k)], y = inst.corners[size_t(3 * k + 1)], z = inst.corners[size_t(3 * k + 2)];
                const double cx = M[0] * x + M[4] * y + M[8] * z + M[12];
                const double cy = M[1] * x + M[5] * y + M[9] * z + M[13];
                const double cz = M[2] * x + M[6] * y + M[10] * z + M[14];
                const double w = std::abs(M[3] * x + M[7] * y + M[11] * z + M[15]);
                all &= (cx > w ? 1 : cx < -w ? 2 : 0) | (cy > w ? 4 : cy < -w ? 8 : 0) | (cz > w ? 16 : cz < -w ? 32 : 0);
                if (!all) {
                    return false;
                }
            }
            return all != 0;
        };
        auto depth = [&](double x, double y, double z) { return -(view[2] * x + view[6] * y + view[10] * z + view[14]) * 100; };
        const float instanceRate = 0.0001428571413271129f;
        const float puffRate = 0.0005000831442885101f;
        const float puffFadeEnd = 2499.66748046875f;
        std::vector<float> fade(puffInstances_.size(), 0.f);
        for (size_t j = 0; j < puffInstances_.size(); ++j) {
            const auto& inst = puffInstances_[j];
            bool visible = true;
            if (inst.chunk) {
                const auto r = resident(*inst.chunk);
                visible = !(r && !*r);
            }
            if (!visible || culled(inst)) {
                continue;
            }
            // instanceFade
            const double d = std::trunc(depth(inst.centre[0], inst.centre[1], inst.centre[2]));
            if (25000 <= d) {
                continue;
            }
            fade[j] = d < 18000 ? 1.f : float(float(25000 - d) * instanceRate);
        }
        struct Sprite {
            size_t k;
            int depth;
            uint8_t alpha;
        };
        std::vector<Sprite> sprites;
        for (size_t k = 0; k < puffs_.size(); ++k) {
            const float f10 = fade[puffs_[k].owner];
            if (!f10) {
                continue;
            }
            const auto& p = puffs_[k].pos;
            const double dd = depth(p[0], p[1], p[2]);
            // puffFade
            const double d = std::trunc(dd);
            if (d <= 0 || d <= 500) {
                continue;
            }
            const float f0 = d < puffFadeEnd ? float(float(d - 500) * puffRate) : 1.f;
            const float f7 = float(f10 * f0);
            if (!f7) {
                continue;
            }
            sprites.push_back({k, int(std::trunc(dd)), uint8_t(int(std::trunc(float(f7 * 128.f))))});
        }
        std::stable_sort(sprites.begin(), sprites.end(), [](const Sprite& a, const Sprite& b) { return a.depth > b.depth; });
        if (sprites.empty()) {
            return;
        }
        QuadBatch q;
        q.variant = 2;
        q.blend = 0;
        q.texture = "fog0";
        q.order = draw_order::drawOrder({7, 1023, 3, 4, -1}, 2);
        q.id = 2900;
        q.position = {0, 0, 0};
        for (const auto& s : sprites) {
            const Puff& p = puffs_[s.k];
            q.instances.insert(q.instances.end(), {p.pos[0], p.pos[1], p.pos[2], p.size, 0, 0, 0, 0, float(p.rgb[0] / 128.0), float(p.rgb[1] / 128.0),
                                                   float(p.rgb[2] / 128.0), float(s.alpha / 128.0)});
        }
        out_.push_back(std::move(q));
    }

    std::array<double, 3> origin_;
    bool stageWorld_ = false;
    std::vector<particles::Combo> combos_;
    std::vector<Puff> puffs_;
    std::vector<PuffInstance> puffInstances_;
    std::vector<QuadBatch> out_;
    bool crowd_ = false;
    std::map<uint32_t, Area> areas_;
    std::vector<float> flashes_;
};

}  // namespace ssx_native
