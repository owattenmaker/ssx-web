#pragma once
// The human rider as the web build draws it (docs/native.md "N3"):
// - web/rider-skinning.js: the source vertices (PS2 cm, Z up) through the core's skin palette (one 4 x 4 world matrix per
//   influence group, rider_skin_palette), previous and current palette blended by the render alpha, VU chop arithmetic
//   (web/rider-lighting-nodes.js), then the origin-relative native position;
// - web/rider-lighting-nodes.js / rider-material.js: the per-vertex irradiance of the core's ten lighting rows
//   (rider_lighting_gpu_coefficients) on the palette-transformed ITOF15 normal, quantized before interpolation; GS HIGHLIGHT2
//   on the PS2-domain texels; riderDrawState (opaque, or 'alph' / 'ea*' in two blended passes).
#include "gpu.hpp"
#include "json.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ssx_native {

struct RiderFrame {
    // the two palettes (previous, current), 16 floats per influence group each
    std::vector<float> previous;
    std::vector<float> current;
    // the ten lighting rows (40 floats)
    std::array<float, 40> lighting{};
    bool lit = false;
    float alpha = 1;
};

class RiderRenderer {
public:
    // package: RIDER_ZOE; paletteIndices: the core's rider_skin_palette_indices (one group per vertex)
    void load(Gpu& gpu, const std::string& assetsRoot, const std::string& package, const uint32_t* paletteIndices, int groupCount);
    // the frame's uniforms: the camera's view-projection (origin-relative) and the origin
    void update(const std::array<float, 16>& viewProjection, const std::array<double, 3>& origin, const RiderFrame& frame);
    void drawOpaque(WGPURenderPassEncoder pass);
    void drawTransparent(WGPURenderPassEncoder pass);

private:
    struct Batch {
        uint32_t first = 0;
        uint32_t count = 0;
        bool blended = false;
        WGPUBindGroup material = nullptr;
    };
    Gpu* gpu_ = nullptr;
    int groups_ = 0;
    WGPUBuffer vertices_ = nullptr;
    WGPUBuffer indices_ = nullptr;
    WGPUBuffer uniform_ = nullptr;
    WGPUBuffer palette_ = nullptr;
    WGPUBindGroup frame_ = nullptr;
    WGPUBindGroupLayout frameLayout_ = nullptr;
    WGPUBindGroupLayout materialLayout_ = nullptr;
    WGPUSampler sampler_ = nullptr;
    // 0 opaque, 1 high pass (alpha test, depth written), 2 low pass; the blended ones twice (back faces, then front)
    WGPURenderPipeline pipelines_[3][2] = {};
    std::vector<Batch> batches_;
};

}  // namespace ssx_native
