#pragma once
// The course world, the sky and the fog composite, drawn as the web build draws them (docs/native.md "N3"):
// - web/main.js asset() packages (world.json, vertices.bin 40-byte stride, indices.bin, colors.bin), the textures from the
//   archives (SSXTEX01);
// - web/world-material.js: the terrain combine with the PS2 light page (ctx2 0x81), the static-model modulate and its blend
//   classes (opaque; pass + fringe; additive), the sky;
// - web/fog-renderer.js: the sky pass, the world pass over it, the CLUT fog composite on the world depth.
// All colour work is on GS bytes in RGBA8Unorm targets (pv encodedBlend: no sRGB, output = the bytes).
#include "gpu.hpp"
#include "json.hpp"
#include "screen_tint.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ssx_native {

// The tick's camera (tick_end / step_camera_head: eye, look-at, half horizontal FOV in radians, near, far; native meters)
struct CameraView {
    std::array<float, 9> v{};
};

// What the core says each frame (fog palette, chunk residency, pickups)
struct WorldFrameState {
    bool fogEnabled = false;
    float fogSlope = 0;
    float fogOffset = 0;
    uint32_t fogRevision = 0;
    const uint8_t* fogPalette = nullptr;
    // chunk id -> drawn (state 3)
    std::map<uint32_t, bool> chunks;
    // ScreenTint and the lightning flash (web/screen-tint.js)
    TintOutput tint;
};

class WorldRenderer {
public:
    // assetsRoot: web/public/assets; code: the course (ARA1)
    void load(Gpu& gpu, const std::string& assetsRoot, const std::string& code);
    // the frame into target (an RGBA8Unorm view of width x height); camera aspect: the stage's (4/3, widescreen Off)
    void render(const CameraView& camera, const WorldFrameState& state, WGPUTextureView target, uint32_t width, uint32_t height, double aspect);

    std::array<double, 3> origin{};

private:
    struct Texture {
        WGPUTexture texture = nullptr;
        WGPUTextureView view = nullptr;
    };
    struct Draw {
        uint32_t first = 0;
        uint32_t count = 0;
        // the pipeline kind
        int kind = 0;
        WGPUBindGroup material = nullptr;
        int chunk = -1;
        int order = 0;
        size_t batch = 0;
    };
    struct Package {
        WGPUBuffer vertices = nullptr;
        WGPUBuffer colors = nullptr;
        WGPUBuffer lightUv = nullptr;
        WGPUBuffer vertexAlpha = nullptr;
        WGPUBuffer indices = nullptr;
        std::vector<Draw> opaque;
        std::vector<Draw> transparent;
    };
    Texture loadTexture(const std::string& assetsRoot, const nlohmann::json& entry);
    Texture upload(uint32_t w, uint32_t h, const std::vector<uint8_t>& rgba, bool mips);
    void buildPipelines();
    void ensureTargets(uint32_t w, uint32_t h);
    void drawPackage(WGPURenderPassEncoder pass, const Package& p, const std::vector<Draw>& draws, const WorldFrameState& state);
    void loadPackage(Package& p, const std::string& assetsRoot, const std::string& dir, bool world);
    WGPUBindGroup materialGroup(const Texture& map, bool clampMap);

    Gpu* gpu_ = nullptr;
    Package world_;
    Package sky_;
    std::map<std::string, Texture> archiveTextures_;
    Texture atlas_;
    Texture palette_;
    uint32_t paletteRevision_ = 0xffffffffu;
    WGPUSampler repeat_ = nullptr;
    WGPUSampler clamp_ = nullptr;
    WGPUSampler atlasSampler_ = nullptr;
    WGPUSampler nearest_ = nullptr;
    WGPUBindGroupLayout frameLayout_ = nullptr;
    WGPUBindGroupLayout materialLayout_ = nullptr;
    WGPUBindGroupLayout compositeLayout_ = nullptr;
    WGPUBuffer worldUniform_ = nullptr;
    WGPUBuffer skyUniform_ = nullptr;
    WGPUBuffer compositeUniform_ = nullptr;
    WGPUBindGroup worldFrame_ = nullptr;
    WGPUBindGroup skyFrame_ = nullptr;
    // pipelines by kind: 0 terrain, 1 instance opaque, 2 blend pass (alpha test), 3 blend fringe, 4 additive, 5 sky opaque, 6 sky blend;
    // each transparent kind twice: back faces, then front faces (three.js draws a transparent DoubleSide material in two passes)
    std::map<int, WGPURenderPipeline> pipelines_;
    WGPURenderPipeline composite_ = nullptr;
    WGPURenderPipeline copySky_ = nullptr;
    WGPUBindGroupLayout copyLayout_ = nullptr;
    WGPUBindGroup copyGroup_ = nullptr;
    uint32_t targetW_ = 0;
    uint32_t targetH_ = 0;
    Texture skyColor_;
    Texture worldColor_;
    Texture depth_;
    WGPUTextureView depthSample_ = nullptr;
    WGPUBindGroup compositeGroup_ = nullptr;
};

}  // namespace ssx_native
