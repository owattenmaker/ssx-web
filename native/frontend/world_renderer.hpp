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
#include "post_effects.hpp"
#include "rider_fx.hpp"
#include "rider_renderer.hpp"
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

// three's matrixWorld of a world batch mesh (column-major, double; the static default is translate(-origin))
using Mat4d = std::array<double, 16>;

// A world batch the set pieces move, hide or copy this frame (set_pieces.hpp): its mesh's matrixWorld when moved, and the matrices of
// its clones (web/moving-instances.js copies > 0), drawn after every authored mesh
struct BatchOverride {
    bool hidden = false;
    // a batch the page keeps out of the scene by default (world.json hidden_resource) drawn this frame
    bool shown = false;
    bool moved = false;
    Mat4d matrix{};
    std::vector<Mat4d> copies;
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
    // the UVScroll groups' texture translations (uv_scroll_group order)
    std::vector<std::array<float, 2>> scroll;
    // ScreenTint and the lightning flash (web/screen-tint.js)
    TintOutput tint;
    // the riders drawn in the world pass after the world (renderOrder 600): the human first, then the computer riders (their
    // scene order); each its renderer and this frame's palettes and lighting
    std::vector<std::pair<RiderRenderer*, const RiderFrame*>> riders;
    // each rider's trail, wake and snow (the human first, then the computer riders in their slots)
    std::vector<const RiderFxFrame*> fx;
    // world.json batch index -> its set-piece state (null: everything static)
    const std::map<size_t, BatchOverride>* batches = nullptr;
};

class WorldRenderer {
public:
    // assetsRoot: web/public/assets; code: the course (ARA1)
    void load(Gpu& gpu, const std::string& assetsRoot, const std::string& code);
    // the frame into target (an RGBA8Unorm view of width x height); camera aspect: the stage's (4/3, widescreen Off)
    void render(const CameraView& camera, const WorldFrameState& state, WGPUTextureView target, uint32_t width, uint32_t height, double aspect);

    // the course's light glows and sun flare (after load; either may be null)
    void attachEffects(LightGlow* glow, SunFlare* sun) {
        post_.attach(glow, sun);
    }
    const PostEffects& effects() const {
        return post_;
    }

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
        // renderOrder: 0, or 1 for SORTED_CLASS_ORDER (0.5: the depth-sorted static-model classes 2 / 3)
        int order = 0;
        size_t batch = 0;
        // the UVScroll group + 1 (0: none): the static object record drawn as the first instance
        int scroll = 0;
        // world.json hidden_resource: not in the scene unless a set piece shows it (flag cloth sources, MeshAnim pieces, LiveComp
        // 'object' draws)
        bool defaultHidden = false;
        // the mesh's bounding sphere centre (the batch's indexed vertices' box centre, package coordinates)
        std::array<double, 3> centre{};
    };
    // one draw of the frame, in three's render-list order: its object record (first instance) and projected depth
    struct Item {
        const Draw* draw = nullptr;
        uint32_t object = 0;
        double z = 0;
        size_t id = 0;
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
    // three.js's render lists for a package this frame: opaque by renderOrder, depth (near first), id; transparent by renderOrder,
    // depth (far first), id. Moved and copied batches get object records after the static ones.
    void collect(const Package& p, const Mat4d& vp, const Mat4d& staticModel, const WorldFrameState& state, bool world, std::vector<Item>& opaque,
                 std::vector<Item>& transparent);
    void bindPackage(WGPURenderPassEncoder pass, const Package& p);
    // side: -1 opaque, 0 back faces, 1 front faces (three's transparent double pass)
    void drawItems(WGPURenderPassEncoder pass, const std::vector<Item>& items, int side);
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
    static constexpr uint64_t kScrollGroups = 256;
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
    WGPUBindGroup finishGroup_ = nullptr;
    // the object records: [0, kScrollGroups) the static world (scroll group + 1), then this frame's moved / copied batches
    static constexpr uint64_t kObjects = 8192;
    static constexpr uint64_t kObjectFloats = 20;
    WGPUBuffer worldObjects_ = nullptr;
    WGPUBuffer skyObjects_ = nullptr;
    std::vector<float> objectData_;
    std::vector<Item> worldOpaque_, worldTransparent_, skyOpaque_, skyTransparent_;
    WGPURenderPipeline fog_ = nullptr;
    Texture fogged_;
    PostEffects post_;
    RiderFx fx_;
};

}  // namespace ssx_native
