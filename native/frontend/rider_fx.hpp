#pragma once
// The riders' board trails, wakes, snow spray, boost ribbons, power-up aura and air streamers as the web build draws them
// (docs/native.md "N3"):
// - web/board-trail.js: the core's ribbon and roof (386E78 order) in five stencil passes in the world pass, after the world's
//   opaque draws and before the riders (renderOrder 500 .. 504);
// - web/wake-renderer.js, web/snow-renderer.js, web/boost-renderer.js: the wake strip, the ten snow emitters (camera-facing
//   billboards with the original's projected-size cap, the current flipbook frame), the human's three boost ribbons and every
//   rider's aura faces and streamers, in the encoded post-fog pass (web/snow-composite.js): drawn onto the fogged world bytes with
//   the world depth, in three.js's transparent order (web/ps2-draw-order.js renderOrder, then the depth of each geometry's bounding
//   sphere centre as three first computed it, then creation order);
// - web/opponent-fx.js: the same (no boost ribbons) for each computer rider from its own core, hidden while its 100 m sphere is out
//   of view (pv fxCull).
#include "gpu.hpp"
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ssx_native {

// One rider's effects this frame, read from its core (the current rider context)
struct RiderFxFrame {
    // drawn this frame (the human while playing; a computer rider while the AI race runs)
    bool visible = false;
    // a computer rider (pv fxCull applies; no boost ribbons)
    bool cullable = false;
    // rider_world_state: the rider's position (cm, Z up)
    std::array<float, 3> position{};
    // 9 floats per vertex in the core's layout (cm, Z up; uv; rgba)
    std::vector<float> ribbon;
    std::vector<float> roof;
    std::vector<float> wake;
    // boost_fx_info / boost_fx_vertices (the human's): the palette texture and three ribbons
    int boostTexture = 57;
    std::array<std::vector<float>, 3> boost;
    // rider_fx_info / rider_fx_vertices: four aura faces, two streamers on the streamer texture (61 prbn / 63 strm)
    int streamerTexture = 63;
    std::array<std::vector<float>, 6> fx;
    // 8 floats per particle; the emitter's current flipbook texture
    std::array<std::vector<float>, 10> snow;
    std::array<int, 10> snowFrame{};
    // the core's buffers (web/board-trail.js, wake-renderer.js, snow-renderer.js, boost-renderer.js update)
    void read(bool boostRibbons);
};

class RiderFx {
public:
    // assetsRoot: web/public/assets (BOARD_TRAIL, SNOW_FX, FX)
    void load(Gpu& gpu, const std::string& assetsRoot);
    // the frame's riders (index 0 the human) with the displayed camera (origin-relative eye and look-at; three's view and
    // projection, double column-major)
    void prepare(const std::vector<const RiderFxFrame*>& riders, const double eye[3], const double look[3], const double* view,
                 const double* projection, const std::array<double, 3>& origin);
    // in the world pass, after the world's opaque draws
    void drawTrails(WGPURenderPassEncoder pass);
    // in the encoded pass (the fogged world, the world depth read-only)
    void drawEncoded(WGPURenderPassEncoder pass);

private:
    struct Range {
        uint64_t first = 0;
        uint32_t count = 0;
    };
    // a strip mesh of the page (wake, boost ribbon, aura face, streamer): its position attribute as three keeps it (its capacity,
    // stale past the drawn count) and its bounding sphere centre, computed by three on the mesh's first projection and kept
    struct Strip {
        Range range;
        std::vector<float> positions;
        std::optional<std::array<double, 3>> centre;
    };
    struct RiderGpu {
        Range ribbon, roof;
        Strip wake;
        std::array<Strip, 3> boost;
        std::array<Strip, 6> fx;
        std::array<Range, 10> snow;
        bool drawn = false;
    };
    struct Texture {
        WGPUTexture texture = nullptr;
        WGPUBindGroup group = nullptr;
        float alphaScale = 1;
    };
    struct EncodedDraw {
        double order = 0;
        double z = 0;
        int id = 0;
        bool additive = false;
        // a strip (vertices) or a snow emitter (instances)
        bool billboard = false;
        Range range;
        const Texture* texture = nullptr;
    };
    Texture loadTexture(const std::string& path, uint32_t w, uint32_t h, float alphaScale, bool repeat, bool flipRows);
    void buildPipelines();

    Gpu* gpu_ = nullptr;
    WGPUSampler repeat_ = nullptr;
    WGPUSampler clamp_ = nullptr;
    WGPUBindGroupLayout frameLayout_ = nullptr;
    WGPUBindGroupLayout materialLayout_ = nullptr;
    WGPUBuffer uniform_ = nullptr;
    WGPUBindGroup frame_ = nullptr;
    WGPURenderPipeline trail_[5] = {};
    // the encoded pass: strip / billboard, normal / additive
    WGPURenderPipeline strip_ = nullptr;
    WGPURenderPipeline stripAdd_ = nullptr;
    WGPURenderPipeline snow_ = nullptr;
    Texture trailTexture_;
    float trailAlpha_ = 1;
    // SNOW_FX textures by FX id (snow frames 5 .. 25, wake 56, boost palettes 57 .. 61)
    std::map<int, Texture> snowTextures_;
    // FX textures: psmr (aura, clamp), 63 strm (clamp, rows flipped), 61 prbn (repeat)
    Texture aura_;
    std::map<int, Texture> streamers_;
    // each emitter's initial texture (its alpha scale) and its flipbook frames
    std::array<int, 10> emitterTexture_{};
    std::array<std::vector<int>, 10> flipbooks_{};
    // the frame's vertex and instance streams
    WGPUBuffer vertices_ = nullptr;
    uint64_t vertexCapacity_ = 0;
    WGPUBuffer instances_ = nullptr;
    uint64_t instanceCapacity_ = 0;
    std::vector<RiderGpu> riders_;
    std::vector<EncodedDraw> encoded_;
};

}  // namespace ssx_native
