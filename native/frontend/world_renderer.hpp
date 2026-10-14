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
#include <memory>
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

// a flag cloth mesh (web/set-pieces-renderer.js addFlagMesh): drawn with its hidden source batch's material
struct FlagMesh {
    uint32_t resource = 0;
    size_t sourceBatch = 0;
    // native metres (not origin-relative), uv, rgba
    std::vector<float> positions;
    std::vector<float> uv;
    std::vector<float> colour;
    std::vector<uint32_t> indices;
    bool uploaded = false;
    uint64_t revision = 0;
};

// a MultiSpline cable (web/set-piece-cables.js cableGeometry): per quad 4 vertices of (own end, other end, corner), origin-relative
struct CableMesh {
    uint32_t owner = 0;
    std::vector<float> own;
    std::vector<float> other;
    std::vector<float> corner;
    std::vector<uint32_t> indices;
    bool visible = true;
    std::array<double, 3> centre{};
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
    // the flag cloth meshes and the cables (set_pieces.hpp)
    const std::vector<FlagMesh>* flags = nullptr;
    const std::vector<CableMesh>* cables = nullptr;
    // the set pieces' particles, fog puffs and halos (built for the frame's camera)
    class SetPieceFx* setPieceFx = nullptr;
    // the terrain sparkle, the camera tick's fog_info (null: no camera tick) and weather_info()[46] (its painter's restarts)
    class TerrainSparkle* sparkle = nullptr;
    // the start-gate fountains' particles (startfire.hpp: x, y, z cm, size cm, colour bytes)
    const std::vector<double>* startfire = nullptr;
    const std::array<float, 11>* fogInfo = nullptr;
    float resets = 0;
};

class WorldRenderer {
public:
    // assetsRoot: web/public/assets; code: the course (ARA1)
    void load(Gpu& gpu, const std::string& assetsRoot, const std::string& code);
    // the frame into target (an RGBA8Unorm view of width x height); camera aspect: the stage's (4/3, widescreen Off)
    void render(const CameraView& camera, const WorldFrameState& state, WGPUTextureView target, uint32_t width, uint32_t height, double aspect);
    // the frame's camera as three.js has it (main.js: position eye - origin, lookAt look - origin): matrixWorldInverse (column-major)
    // and the position, scene metres (the audio listener, web/game-audio.js listener)
    void cameraOf(const CameraView& camera, std::array<double, 16>& inverse, std::array<double, 3>& position) const;
    // three's camera.lookAt view matrix (scene coordinates, up y) of eye looking at target
    static std::array<double, 16> lookAt(const double eye[3], const double target[3]);

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
    struct Package;
    struct Item {
        const Draw* draw = nullptr;
        // the package whose buffers it draws from
        const Package* package = nullptr;
        uint32_t object = 0;
        double z = 0;
        size_t id = 0;
        // the crowd's camera flashes (drawn by the effects renderer)
        bool special = false;
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
    void writeLevels(WGPUTexture texture, uint32_t w, uint32_t h, const std::vector<uint8_t>& rgba, uint32_t levels);
    // web/crowd-2d.js: the crowd textures' frames (-1: the archive's own image)
    void loadCrowd(const std::string& assetsRoot, const std::string& code, const nlohmann::json& world);
    void swapCrowdFrame(int frame);
    void buildPipelines();
    void ensureTargets(uint32_t w, uint32_t h);
    // three.js's render lists for a package this frame: opaque by renderOrder, depth (near first), id; transparent by renderOrder,
    // depth (far first), id. Moved and copied batches get object records after the static ones.
    void collect(const Package& p, const Mat4d& vp, const Mat4d& staticModel, const WorldFrameState& state, bool world, std::vector<Item>& opaque,
                 std::vector<Item>& transparent);
    void bindPackage(WGPURenderPassEncoder pass, const Package& p);
    // the flag cloth meshes into the frame's lists (their source batch's material and pipeline) and the cables
    void collectFlags(const WorldFrameState& state, const Mat4d& vp);
    static void sortItems(std::vector<Item>& opaque, std::vector<Item>& transparent);
    void drawCables(WGPURenderPassEncoder pass, const WorldFrameState& state);
    void buildCablePipeline();
    // the frame uniform (the camera, the origin, the rider shadows' receiver rows) and the shadow slots
    void writeFrame(const WorldFrameState& state, const double eye[3], const Mat4d& P, const Mat4d& V);
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
    struct FlagGpu {
        uint64_t revision = ~uint64_t(0);
        size_t capacity = 0;
        std::unique_ptr<Package> package;
        Draw draw;
    };
    std::map<uint32_t, FlagGpu> flagGpu_;
    struct CrowdTarget {
        WGPUTexture texture = nullptr;
        uint32_t width = 0, height = 0, levels = 1;
        std::vector<uint8_t> statics;
    };
    std::vector<CrowdTarget> crowdTargets_;
    std::vector<std::vector<uint8_t>> crowdFrames_;
    int crowdFrame_ = -1;
    // the batch index -> its draws (a flag mesh takes its source batch's)
    std::map<size_t, const Draw*> batchDraw_;
    struct CableGpu {
        WGPUBuffer own = nullptr, other = nullptr, corner = nullptr, indices = nullptr;
        uint32_t count = 0;
    };
    std::map<size_t, CableGpu> cableGpu_;
    WGPURenderPipeline cablePipeline_ = nullptr;
    WGPUBindGroupLayout cableLayout_ = nullptr;
    WGPUBuffer cableUniform_ = nullptr;
    WGPUBindGroup cableGroup_ = nullptr;
    static constexpr uint64_t kFrameBytes = 336;
    Texture shadowAtlas_;
    Texture shadowDepth_;
    Texture glintGrey_;
    Texture glintBlue_;
    WGPUSampler linearClamp_ = nullptr;
    std::vector<std::pair<RiderRenderer*, ShadowSlot>> shadowSlots_;
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
