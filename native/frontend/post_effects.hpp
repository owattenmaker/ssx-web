#pragma once
// The priority-7 / 8 additive effects after the fog composite (docs/native.md "N3"): the light glow halos (web/light-glow.js:
// the depth query per light, the sprites into the glow target) and the sun glow with its lens flare (web/sun-flare.js: the
// 16 x 16 depth query, the ten sprites the composite adds). The composite reads them through its group 1 (layout()).
#include "gpu.hpp"
#include "light_glow.hpp"
#include "sun_flare.hpp"
#include <array>
#include <memory>

namespace ssx_native {

class PostEffects {
public:
    // the pipelines and the composite's group-1 layout (no glow, no sun until attach)
    void init(Gpu& gpu);
    // glow / sun: the course's packages (either may be null)
    void attach(LightGlow* glow, SunFlare* sun);
    WGPUBindGroupLayout layout() const {
        return layout_;
    }
    // the frame's queries and sprites from the displayed camera (three's matrixWorld, view and projection, double column-major)
    void prepare(const double* world, const double* view, const double* projection, double near, double far, uint32_t width, uint32_t height,
                 WGPUTextureView depthSample);
    // the depth queries and the glow sprites, after the world pass
    void encode(WGPUCommandEncoder enc);
    WGPUBindGroup group() const {
        return group_;
    }
    // this frame's light glow sprites, and whether the sun is in view
    uint32_t glows() const {
        return glowVertexCount_ / 6;
    }
    bool sunVisible() const {
        return sunVisible_;
    }

private:
    void ensureTargets(uint32_t w, uint32_t h, WGPUTextureView depthSample);
    WGPUTexture makeTexture(uint32_t w, uint32_t h, const std::vector<uint8_t>& rgba, WGPUTextureView& view);
    Gpu* gpu_ = nullptr;
    LightGlow* glow_ = nullptr;
    SunFlare* sun_ = nullptr;
    WGPUBindGroupLayout layout_ = nullptr;
    WGPUBindGroup group_ = nullptr;
    WGPUSampler linear_ = nullptr;
    // light glow
    WGPURenderPipeline glowQuery_ = nullptr;
    WGPURenderPipeline glowDraw_ = nullptr;
    WGPUBindGroupLayout queryLayout_ = nullptr;
    WGPUBindGroup glowGroup_ = nullptr;
    WGPUBindGroupLayout glowDrawLayout_ = nullptr;
    WGPUBindGroup glowDrawGroup_ = nullptr;
    WGPUBuffer glowDrawParams_ = nullptr;
    WGPUBuffer glowQueryBuffer_ = nullptr;
    WGPUBuffer glowParams_ = nullptr;
    WGPUBuffer glowVertices_ = nullptr;
    uint32_t glowVertexCount_ = 0;
    uint32_t glowCapacity_ = 0;
    bool glowQueried_ = false;
    WGPUTexture glowVis_ = nullptr;
    WGPUTextureView glowVisView_ = nullptr;
    WGPUTexture glowTarget_ = nullptr;
    WGPUTextureView glowTargetView_ = nullptr;
    WGPUTextureView shalView_ = nullptr;
    WGPUTextureView mhalView_ = nullptr;
    // sun
    WGPURenderPipeline sunQuery_ = nullptr;
    WGPUBindGroup sunGroup_ = nullptr;
    WGPUBuffer sunRows_ = nullptr;
    WGPUBuffer sunParams_ = nullptr;
    WGPUBuffer sprites_ = nullptr;
    bool sunVisible_ = false;
    WGPUTexture sunVis_ = nullptr;
    WGPUTextureView sunVisView_ = nullptr;
    WGPUTextureView sunViews_[3] = {};
    // shared
    uint32_t w_ = 0;
    uint32_t h_ = 0;
    WGPUTextureView depth_ = nullptr;
    float near_ = 0.1f;
    float far_ = 300;
};

}  // namespace ssx_native
