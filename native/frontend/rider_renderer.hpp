#pragma once
// The human rider as the web build draws it (docs/native.md "N3"):
// - web/rider-skinning.js: the source vertices (PS2 cm, Z up) through the core's skin palette (one 4 x 4 world matrix per
//   influence group, rider_skin_palette), previous and current palette blended by the render alpha, VU chop arithmetic
//   (web/rider-lighting-nodes.js), then the origin-relative native position;
// - web/rider-lighting-nodes.js / rider-material.js: the per-vertex irradiance of the core's ten lighting rows
//   (rider_lighting_gpu_coefficients) on the palette-transformed ITOF15 normal, quantized before interpolation; GS HIGHLIGHT2
//   on the PS2-domain texels; riderDrawState (opaque, or 'alph' / 'ea*' in two blended passes).
#include "../verify/board_flex.hpp"
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
    // web/rider-skinning.js shadowPose's inputs: world_pose_bones of the previous and the current capture (count, then 7 floats a
    // bone) and the current pose_physical (12 floats)
    std::vector<float> bonesPrevious;
    std::vector<float> bonesCurrent;
    std::array<float, 12> physical{};
    // pv boardFlex: the morph weights (27 renderer columns) of the previous and the current palette's tick
    std::vector<float> flexPrevious;
    std::vector<float> flexCurrent;
};

// a rider's slot in the shadow atlas this frame (web/rider-shadow.js update): the silhouette rows over the skinned source position
// (PS2 cm), the slot and the board's value (0 grounded)
struct ShadowSlot {
    std::array<float, 4> rowX{};
    std::array<float, 4> rowY{};
    float slot = 0;
    float board = 0;
};

class RiderRenderer {
public:
    // the rider texture set (quality.riderTextures, frontend/quality.hpp), read by load: 'xbox' (the desktop default) reads a
    // WARDROBE textures.tex / gear.tex from its Xbox HD twin where that exists (web/texture-archive.js riderArchiveUrl)
    std::string textureSet = "xbox";
    // package: RIDER_ZOE; paletteIndices: the core's rider_skin_palette_indices (one group per vertex)
    // flex: the rider's morph parts (pv boardFlex; null: the rest shape)
    void load(Gpu& gpu, const std::string& assetsRoot, const std::string& package, const uint32_t* paletteIndices, int groupCount,
              const flex::Flex* flex = nullptr);
    // the frame's uniforms: the camera's view-projection (origin-relative) and the origin
    void update(const std::array<float, 16>& viewProjection, const std::array<double, 3>& origin, const RiderFrame& frame);
    void drawOpaque(WGPURenderPassEncoder pass);
    // the silhouette into the shadow atlas (after update: the drawn palettes)
    void drawShadow(WGPURenderPassEncoder pass, const ShadowSlot& slot);
    // side 0: back faces, 1: front faces (three's transparent double pass; the world's blended batches interleave between the two)
    void drawTransparent(WGPURenderPassEncoder pass, int side);

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
    WGPURenderPipeline shadowPipeline_ = nullptr;
    WGPUBuffer shadowUniform_ = nullptr;
    WGPUBuffer morphs_ = nullptr;
    static constexpr size_t kUniformBytes = (64 + 56) * 4;
    WGPUBindGroup shadowGroup_ = nullptr;
    uint32_t indexCount_ = 0;
    std::vector<Batch> batches_;
};

}  // namespace ssx_native
