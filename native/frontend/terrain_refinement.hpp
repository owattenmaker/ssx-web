#pragma once
// The near-rider terrain detail (web/terrain-refinement.js, terrain-detail.js, terrain-mesh.js, terrain-worker-core.js, 1:1): the
// bounded plan of the patches within 30 m of the rider (and its contact patch) refined to a 32 x 32 grid, their neighbours' edges
// matched, each patch's bicubic mesh from the authored polynomial (renderer only, never physics), and the client's 0.1 s / plan
// gate. The page builds the meshes in a worker and commits them on the next frame after they arrive; natively they are built at
// once and committed by the next frame (world_renderer.cpp terrainCommit swaps them in, as web/terrain-overlays.js does).
#include "bc_texels.hpp"
#include "json.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace ssx_native {

namespace terrain {

using bc::jsHypot;
using bc::jsRound;

struct Neighbor {
    int patch = 0;
    int edge = 0;
};

struct Patch {
    int64_t resource = 0;
    std::array<std::array<double, 3>, 16> coefficients{};
    std::array<std::array<double, 2>, 4> textureUv{};
    std::array<double, 4> lightUv{};
    std::array<double, 4> color{1, 1, 1, 1};
    std::array<double, 3> lo{};
    std::array<double, 3> hi{};
    std::array<std::optional<Neighbor>, 4> neighbors;
    uint32_t baselineVertexStart = 0;
};

inline std::vector<Patch> loadPatches(const nlohmann::json& render) {
    std::vector<Patch> out;
    for (const auto& p : render["patches"]) {
        Patch q;
        q.resource = p["resource"].get<int64_t>();
        for (size_t i = 0; i < 16; ++i) {
            for (size_t k = 0; k < 3; ++k) {
                q.coefficients[i][k] = p["coefficients"][i][k].get<double>();
            }
        }
        for (size_t i = 0; i < 4; ++i) {
            q.textureUv[i] = {p["textureUv"][i][0].get<double>(), p["textureUv"][i][1].get<double>()};
            q.lightUv[i] = p["lightUv"][i].get<double>();
        }
        if (p.contains("color") && p["color"].is_array()) {
            for (size_t i = 0; i < 4; ++i) {
                q.color[i] = p["color"][i].get<double>();
            }
        }
        for (size_t k = 0; k < 3; ++k) {
            q.lo[k] = p["bounds"][0][k].get<double>();
            q.hi[k] = p["bounds"][1][k].get<double>();
        }
        for (size_t e = 0; e < 4; ++e) {
            const auto& n = p["neighbors"][e];
            if (!n.is_null()) {
                q.neighbors[e] = Neighbor{n["patch"].get<int>(), n["edge"].get<int>()};
            }
        }
        q.baselineVertexStart = p["baseline_vertex_start"].get<uint32_t>();
        out.push_back(q);
    }
    return out;
}

struct PlanItem {
    int index = 0;
    int64_t resource = 0;
    int resolution = 8;
    std::array<int, 4> edges{8, 8, 8, 8};
    bool operator==(const PlanItem& o) const {
        return index == o.index && resource == o.resource && resolution == o.resolution && edges == o.edges;
    }
};

// planTerrainDetail(patches, position, {focusResource, previousRefined}) with the defaults radius 30, maxRefined 8, resolution 32,
// hysteresis 2
inline std::vector<PlanItem> planDetail(const std::vector<Patch>& patches, const std::array<double, 3>& position, int64_t focusResource,
                                        const std::vector<int>& previousRefined) {
    const double radius = 30;
    const size_t maxRefined = 8;
    const int resolution = 32;
    const double hysteresis = 2;
    const std::set<int> retained(previousRefined.begin(), previousRefined.end());
    struct Candidate {
        int index;
        double distance;
    };
    std::vector<Candidate> candidates;
    const double px = position[0];
    const double py = position[1];
    const double pz = position[2];
    for (size_t index = 0; index < patches.size(); ++index) {
        const Patch& patch = patches[index];
        const double d = jsHypot(std::max({patch.lo[0] - px, 0.0, px - patch.hi[0]}), std::max({patch.lo[1] - py, 0.0, py - patch.hi[1]}),
                                 std::max({patch.lo[2] - pz, 0.0, pz - patch.hi[2]}));
        if (d <= radius + (retained.count(int(index)) ? hysteresis : 0) || patch.resource == focusResource) {
            candidates.push_back({int(index), d});
        }
    }
    std::vector<Candidate> eligible;
    for (const auto& c : candidates) {
        bool all = true;
        for (const auto& n : patches[size_t(c.index)].neighbors) {
            all = all && n.has_value();
        }
        if (all) {
            eligible.push_back(c);
        }
    }
    // Array.prototype.sort (stable) with the page's comparator
    std::stable_sort(eligible.begin(), eligible.end(), [&](const Candidate& a, const Candidate& b) {
        const Patch& pa = patches[size_t(a.index)];
        const Patch& pb = patches[size_t(b.index)];
        const double focus = double(pb.resource == focusResource) - double(pa.resource == focusResource);
        if (focus != 0) {
            return focus < 0;
        }
        const double near = (a.distance - (retained.count(a.index) ? hysteresis : 0)) - (b.distance - (retained.count(b.index) ? hysteresis : 0));
        if (near != 0) {
            return near < 0;
        }
        return pa.resource - pb.resource < 0;
    });
    std::set<int> selected;
    for (size_t k = 0; k < eligible.size() && k < maxRefined; ++k) {
        selected.insert(eligible[k].index);
    }
    std::set<int> affected = selected;
    for (int i : selected) {
        for (const auto& n : patches[size_t(i)].neighbors) {
            affected.insert(n->patch);
        }
    }
    std::vector<PlanItem> plan;
    for (int index : affected) {
        PlanItem item;
        item.index = index;
        item.resource = patches[size_t(index)].resource;
        item.resolution = selected.count(index) ? resolution : 8;
        for (size_t e = 0; e < 4; ++e) {
            const auto& nb = patches[size_t(index)].neighbors[e];
            item.edges[e] = std::max(item.resolution, nb && selected.count(nb->patch) ? resolution : 8);
        }
        plan.push_back(item);
    }
    return plan;
}

// a built patch (web/terrain-mesh.js buildTerrainPatch): 10 floats a vertex (position, normal, texture uv, light uv), 4 colour
// floats a vertex, the triangle indices
struct Mesh {
    std::vector<float> vertices;
    std::vector<uint32_t> indices;
    std::vector<float> colors;
};

inline Mesh buildPatch(const Patch& patch, int resolution, const std::array<int, 4>& edges) {
    const auto& c = patch.coefficients;
    std::vector<double> values;
    std::vector<double> colors;
    std::vector<uint32_t> indices;
    std::map<long long, uint32_t> lookup;
    auto at = [&](double u, double v, int du, int dv) {
        std::array<double, 3> out{};
        for (int k = 0; k < 3; ++k) {
            double sum = 0;
            for (int j = dv; j < 4; ++j) {
                for (int i = du; i < 4; ++i) {
                    sum += c[size_t(j * 4 + i)][size_t(k)] * (du ? i : 1) * (dv ? j : 1) * std::pow(u, i - du) * std::pow(v, j - dv);
                }
            }
            out[size_t(k)] = sum;
        }
        return out;
    };
    auto vertex = [&](double u, double v) -> uint32_t {
        const long long key = (long long)(jsRound(u * 128)) + 129 * (long long)(jsRound(v * 128));
        auto it = lookup.find(key);
        if (it != lookup.end()) {
            return it->second;
        }
        const auto p = at(u, v, 0, 0);
        const auto a = at(u, v, 1, 0);
        const auto b = at(u, v, 0, 1);
        const double normal[3] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
        const double length = jsHypot(normal[0], normal[1], normal[2]);
        double n[3] = {0, 1, 0};
        if (length > 1e-10) {
            for (int k = 0; k < 3; ++k) {
                n[k] = normal[k] / length;
            }
        }
        const auto& uv = patch.textureUv;
        double tex[2];
        for (int k = 0; k < 2; ++k) {
            tex[k] = uv[0][size_t(k)] * (1 - u) * (1 - v) + uv[1][size_t(k)] * (1 - u) * v + uv[2][size_t(k)] * u * (1 - v) + uv[3][size_t(k)] * u * v;
        }
        const auto& light = patch.lightUv;
        const uint32_t id = uint32_t(values.size() / 10);
        lookup[key] = id;
        values.insert(values.end(), {p[0], p[1], p[2], n[0], n[1], n[2], tex[0], tex[1], light[0] + u * light[2], light[1] + v * light[3]});
        colors.insert(colors.end(), patch.color.begin(), patch.color.end());
        return id;
    };
    for (int y = 0; y < resolution; ++y) {
        for (int x = 0; x < resolution; ++x) {
            const double u = double(x) / resolution;
            const double v = double(y) / resolution;
            const double h = 1.0 / resolution;
            const int segments[4] = {y == 0 ? edges[0] / resolution : 1, x == resolution - 1 ? edges[1] / resolution : 1,
                                     y == resolution - 1 ? edges[2] / resolution : 1, x == 0 ? edges[3] / resolution : 1};
            std::vector<uint32_t> boundary;
            for (int j = 0; j < segments[0]; ++j) {
                boundary.push_back(vertex(u + h * j / segments[0], v));
            }
            for (int j = 0; j < segments[1]; ++j) {
                boundary.push_back(vertex(u + h, v + h * j / segments[1]));
            }
            for (int j = 0; j < segments[2]; ++j) {
                boundary.push_back(vertex(u + h - h * j / segments[2], v + h));
            }
            for (int j = 0; j < segments[3]; ++j) {
                boundary.push_back(vertex(u, v + h - h * j / segments[3]));
            }
            if (boundary.size() == 4) {
                indices.insert(indices.end(), {boundary[0], boundary[1], boundary[3], boundary[1], boundary[2], boundary[3]});
            } else {
                const uint32_t centre = vertex(u + h / 2, v + h / 2);
                for (size_t i = 0; i < boundary.size(); ++i) {
                    indices.insert(indices.end(), {centre, boundary[i], boundary[(i + 1) % boundary.size()]});
                }
            }
        }
    }
    Mesh m;
    m.vertices.assign(values.begin(), values.end());
    m.colors.assign(colors.begin(), colors.end());
    m.indices = std::move(indices);
    return m;
}

// web/terrain-worker-core.js validate: a plan the worker refuses (the page's refinement then stops)
inline bool validPlan(const std::vector<Patch>& patches, const std::vector<PlanItem>& plan) {
    if (plan.size() > 40) {
        return false;
    }
    auto ok = [](int n) { return n == 8 || n == 16 || n == 32 || n == 64; };
    std::map<int, const PlanItem*> byIndex;
    for (const auto& item : plan) {
        if (item.index < 0 || size_t(item.index) >= patches.size() || patches[size_t(item.index)].resource != item.resource || byIndex.count(item.index)) {
            return false;
        }
        byIndex[item.index] = &item;
        if (!ok(item.resolution)) {
            return false;
        }
        for (int n : item.edges) {
            if (!ok(n) || n < item.resolution) {
                return false;
            }
        }
    }
    for (const auto& item : plan) {
        for (int edge = 0; edge < 4; ++edge) {
            const auto& nb = patches[size_t(item.index)].neighbors[size_t(edge)];
            if (!nb) {
                if (item.edges[size_t(edge)] != 8) {
                    return false;
                }
                continue;
            }
            const auto& back = patches[size_t(nb->patch)].neighbors[size_t(nb->edge)];
            if (!back || back->patch != item.index || back->edge != edge) {
                return false;
            }
            auto it = byIndex.find(nb->patch);
            const int other = it == byIndex.end() ? 8 : it->second->edges[size_t(nb->edge)];
            if (item.edges[size_t(edge)] != other) {
                return false;
            }
        }
    }
    return true;
}

struct Built {
    PlanItem item;
    std::shared_ptr<const Mesh> mesh;
};

// createTerrainRefinementClient: update (per game tick, at most every 0.1 s of the game clock unless the focus patch changed; a new
// plan only when it differs), the plan's meshes (the worker's cache: built once per index, resolution and edges), commit
class Refinement {
public:
    explicit Refinement(std::vector<Patch> patches) : patches_(std::move(patches)) {}
    const std::vector<Patch>& patches() const {
        return patches_;
    }
    bool failed() const {
        return failed_;
    }
    void update(const std::array<double, 3>& position, int64_t focusResource, double seconds) {
        if (failed_ || (seconds - lastTime_ < 0.1 && focusResource == lastFocus_)) {
            return;
        }
        lastTime_ = seconds;
        lastFocus_ = focusResource;
        auto plan = planDetail(patches_, position, focusResource, previousRefined_);
        if (hasKey_ && plan == key_) {
            return;
        }
        key_ = plan;
        hasKey_ = true;
        if (!validPlan(patches_, plan)) {
            failed_ = true;
            pending_.reset();
            return;
        }
        std::vector<Built> built;
        for (const auto& item : plan) {
            built.push_back({item, mesh(item)});
        }
        pending_ = std::move(built);
    }
    // the reply the next frame commits (none: nothing new)
    std::optional<std::vector<Built>> take() {
        auto p = std::move(pending_);
        pending_.reset();
        if (p) {
            previousRefined_.clear();
            for (const auto& b : *p) {
                if (b.item.resolution > 8) {
                    previousRefined_.push_back(b.item.index);
                }
            }
        }
        return p;
    }
    bool pending() const {
        return pending_.has_value();
    }
    // reset(): the empty plan
    void reset() {
        hasKey_ = false;
        key_.clear();
        previousRefined_.clear();
        lastTime_ = -INFINITY;
        lastFocus_ = -1;
        pending_ = std::vector<Built>{};
    }

private:
    std::shared_ptr<const Mesh> mesh(const PlanItem& item) {
        const std::string k = std::to_string(item.index) + ":" + std::to_string(item.resolution) + ":" + std::to_string(item.edges[0]) + ":" +
                              std::to_string(item.edges[1]) + ":" + std::to_string(item.edges[2]) + ":" + std::to_string(item.edges[3]);
        auto it = cache_.find(k);
        if (it == cache_.end()) {
            // (the worker's cache holds 8 MB; only the memory differs)
            if (cache_.size() >= 256) {
                cache_.clear();
            }
            it = cache_.emplace(k, std::make_shared<const Mesh>(buildPatch(patches_[size_t(item.index)], item.resolution, item.edges))).first;
        }
        return it->second;
    }
    std::vector<Patch> patches_;
    std::map<std::string, std::shared_ptr<const Mesh>> cache_;
    std::vector<PlanItem> key_;
    bool hasKey_ = false;
    std::vector<int> previousRefined_;
    double lastTime_ = -INFINITY;
    int64_t lastFocus_ = -1;
    std::optional<std::vector<Built>> pending_;
    bool failed_ = false;
};

}  // namespace terrain

}  // namespace ssx_native
