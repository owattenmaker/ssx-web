#pragma once
// The light glow halos (web/light-glow.js natively): the package's lights (LIGHT_GLOW/light-glow.json, the raw GS textures),
// and per frame with the displayed camera (2E2B00 / 2E2868) each visible light's sprite quad in the 512 x 448 viewport and its
// depth-query record. The renderer runs the query (2EC478) on the world depth and draws the sprites additively (ALPHA 0x48)
// into the glow target the composite adds after the fog.
#include "json.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ssx_native {

struct GlowVertex {
    // the 3D band's NDC, the sprite ST, the GS vertex RGB, then (light index, far glow, depth m, mhal)
    float x, y, z, u, v, r, g, b, index, far, depth, mhal;
};

class LightGlow {
public:
    LightGlow(const std::string& dir, const std::array<double, 3>& origin) {
        std::ifstream f(dir + "light-glow.json", std::ios::binary);
        if (!f) {
            throw std::runtime_error("cannot read " + dir + "light-glow.json");
        }
        pkg_ = nlohmann::json::parse(f);
        if (pkg_["version"] != 1 || pkg_["lights"].empty()) {
            throw std::runtime_error("Unsupported light glow package");
        }
        for (const auto& [name, t] : pkg_["textures"].items()) {
            std::ifstream tf(dir + t["file"].get<std::string>(), std::ios::binary);
            std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(tf)), std::istreambuf_iterator<char>());
            if (bytes.size() != t["width"].get<size_t>() * t["height"].get<size_t>() * 4) {
                throw std::runtime_error("Light glow texture extent");
            }
            (name == "mhal" ? mhal : shal) = {t["width"].get<uint32_t>(), t["height"].get<uint32_t>(), std::move(bytes)};
        }
        const auto& c = pkg_["constants"];
        int index = 0;
        for (const auto& l : pkg_["lights"]) {
            Light light;
            light.index = index++;
            const auto& cls = c["classes"][std::to_string(l["flags"].get<int>() & 0x70)];
            if (cls.is_null()) {
                throw std::runtime_error("Light glow class");
            }
            light.world = {l["position"][0].get<double>() / 100 - origin[0], l["position"][2].get<double>() / 100 - origin[1],
                           -l["position"][1].get<double>() / 100 - origin[2]};
            light.halfSize = cls["half_size"].get<double>();
            light.pull = cls["pull_cm"].get<double>();
            light.mhal = cls["texture"] == "mhal";
            // glowColour: trunc(128 x the normalized colour), in fround steps
            const float r = l["colour"][0].get<float>();
            const float g = l["colour"][1].get<float>();
            const float b = l["colour"][2].get<float>();
            const float n = float(std::sqrt(double(float(float(float(r * r) + float(g * g)) + float(b * b)))));
            const float k = n == 0 ? 1.f : float(1.0 / double(n));
            const float rgb[3] = {n == 0 ? r : float(r * k), n == 0 ? g : float(g * k), n == 0 ? b : float(b * k)};
            for (int j = 0; j < 3; ++j) {
                light.rgb[size_t(j)] = std::trunc(float(rgb[j] * 128.f));
            }
            lights_.push_back(light);
        }
        query.assign(lights_.size() * 2 * 4, 0.f);
    }
    struct Texture {
        uint32_t width = 0, height = 0;
        std::vector<uint8_t> rgba;
    };
    Texture shal, mhal;
    size_t count() const {
        return lights_.size();
    }
    int copies() const {
        return pkg_["constants"]["copies"].get<int>();
    }
    // this frame's sprites (6 vertices each, two triangles) and query rows (light index x 2 rows of 4)
    std::vector<GlowVertex> vertices;
    std::vector<float> query;
    int queried = 0;

    // camera: three's matrixWorld (column-major: right, up, back, eye) and its view / projection (double, column-major)
    void update(const double* world, const double* view, const double* projection, double near, double far) {
        vertices.clear();
        queried = 0;
        const auto& c = pkg_["constants"];
        const double VW = c["viewport"][0].get<double>();
        const double VH = c["viewport"][1].get<double>();
        auto norm = [](std::array<double, 3> v) {
            const double l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
            return l == 0 ? v : std::array<double, 3>{v[0] / l, v[1] / l, v[2] / l};
        };
        const auto right = norm({world[0], world[1], world[2]});
        const auto up = norm({world[4], world[5], world[6]});
        auto apply = [](const double* m, const std::array<double, 3>& p) {
            std::array<double, 4> o{};
            for (int r = 0; r < 4; ++r) {
                o[size_t(r)] = m[r] * p[0] + m[4 + r] * p[1] + m[8 + r] * p[2] + m[12 + r];
            }
            return o;
        };
        auto ndcOf = [&](const std::array<double, 3>& p) {
            const auto v = apply(view, p);
            std::array<double, 4> clip{};
            for (int r = 0; r < 4; ++r) {
                clip[size_t(r)] = projection[r] * v[0] + projection[4 + r] * v[1] + projection[8 + r] * v[2] + projection[12 + r] * v[3];
            }
            return std::array<double, 3>{clip[0] / clip[3], clip[1] / clip[3], clip[2] / clip[3]};
        };
        // glowPixel: trunc(v - 0.5)
        auto pixel = [&](const std::array<double, 3>& p) {
            const auto n = ndcOf(p);
            return std::array<double, 2>{std::trunc((n[0] + 1) * 0.5 * VW - 0.5), std::trunc((1 - n[1]) * 0.5 * VH - 0.5)};
        };
        const auto readSize = c["read_size"];
        const double rw = readSize[0].get<double>();
        const double rh = readSize[1].get<double>();
        for (const auto& light : lights_) {
            const auto v = apply(view, light.world);
            const double depth = -v[2];
            if (!(depth > near && depth < far)) {
                continue;
            }
            const auto n = ndcOf(light.world);
            if (std::abs(n[0]) > 1 || std::abs(n[1]) > 1) {
                continue;
            }
            const auto P = pixel(light.world);
            // glowDepthFraction, glowRecord
            const float zf = float(float(1.0 / (depth * 100) - 1.0 / (far * 100)) / float(1.0 / (near * 100) - 1.0 / (far * 100)));
            const bool farGlow = !(zf > c["far_threshold"].get<float>());
            double angle = 0;
            if (!farGlow) {
                auto clampTo = [](double x, double hi) { return x <= 0 ? 1.0 : std::min(x, hi); };
                const double w = clampTo(std::trunc(float(zf * c["count_scale"][0].get<float>())), c["count_max"][0].get<double>());
                const double h = clampTo(std::trunc(float(zf * c["count_scale"][1].get<float>())), c["count_max"][1].get<double>());
                auto read = [](double x, double lo, double extent, double size) { return x >= 0 ? std::min(x, lo + extent - size) : 0.0; };
                float* q = query.data() + size_t(light.index) * 4;
                q[0] = float(P[0] - double(int(w) >> 1));
                q[1] = float(P[1] - double(int(h) >> 1));
                q[2] = float(w);
                q[3] = float(h);
                float* r = query.data() + (lights_.size() + size_t(light.index)) * 4;
                r[0] = float(read(P[0] - rw / 2, 0, VW, rw));
                r[1] = float(read(P[1] - rh / 2, 0, VH, rh));
                r[2] = float(depth - light.pull / 100);
                r[3] = 1;
                ++queried;
                // ps2Mul(((x - vx) * 2 / vw) - 1, rotation scale): MUL.S toward zero
                const float base = float(float(float(float(P[0]) * 2) / float(VW)) - 1);
                const double p = double(base) * double(c["rotation_scale"].get<float>());
                float rr = float(p);
                if (std::abs(double(rr)) > std::abs(p)) {
                    uint32_t bits;
                    std::memcpy(&bits, &rr, 4);
                    bits -= 1;
                    std::memcpy(&rr, &bits, 4);
                }
                angle = rr;
            }
            const double cs = std::cos(angle);
            const double sn = std::sin(angle);
            const double size = light.halfSize / 100;
            std::array<double, 3> d1{right[0] * cs + up[0] * sn, right[1] * cs + up[1] * sn, right[2] * cs + up[2] * sn};
            const auto A = pixel({light.world[0] + d1[0] * size, light.world[1] + d1[1] * size, light.world[2] + d1[2] * size});
            std::array<double, 3> d2{-right[0] * sn + up[0] * cs, -right[1] * sn + up[1] * cs, -right[2] * sn + up[2] * cs};
            const auto B = pixel({light.world[0] + d2[0] * size, light.world[1] + d2[1] * size, light.world[2] + d2[2] * size});
            const double dx[2] = {A[0] - P[0], A[1] - P[1]};
            const double dy[2] = {B[0] - P[0], B[1] - P[1]};
            // glowQuad corners (P -dx -dy, +dx -dy, -dx +dy, +dx +dy), two triangles 0 1 2, 1 3 2
            const double corners[4][4] = {{P[0] - dx[0] - dy[0], P[1] - dx[1] - dy[1], 0, 0},
                                          {P[0] + dx[0] - dy[0], P[1] + dx[1] - dy[1], 1, 0},
                                          {P[0] - dx[0] + dy[0], P[1] - dx[1] + dy[1], 0, 1},
                                          {P[0] + dx[0] + dy[0], P[1] + dx[1] + dy[1], 1, 1}};
            for (int k : {0, 1, 2, 1, 3, 2}) {
                GlowVertex g;
                g.x = float(corners[k][0] / VW * 2 - 1);
                g.y = float(1 - corners[k][1] / VH * 2);
                g.z = 0;
                g.u = float(corners[k][2]);
                g.v = float(corners[k][3]);
                g.r = light.rgb[0];
                g.g = light.rgb[1];
                g.b = light.rgb[2];
                g.index = float(light.index);
                g.far = farGlow ? 1.f : 0.f;
                g.depth = float(depth);
                g.mhal = light.mhal ? 1.f : 0.f;
                vertices.push_back(g);
            }
        }
    }

private:
    struct Light {
        int index = 0;
        std::array<double, 3> world{};
        double halfSize = 0;
        double pull = 0;
        bool mhal = false;
        std::array<float, 3> rgb{};
    };
    nlohmann::json pkg_;
    std::vector<Light> lights_;
};

}  // namespace ssx_native
