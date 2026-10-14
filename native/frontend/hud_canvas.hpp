#pragma once
// The page's 2D UI canvas (#ui: 640 x 448, web/ui.js this.fg) natively: the subset of CanvasRenderingContext2D the race HUD uses
// (save / restore, translate / scale / rotate, globalAlpha, drawImage, fillRect with a solid colour, and the key caps'
// rounded-rect gradient and arrow paths of web/input-glyphs.js drawKeyCap), recorded as the calls are made and rasterized
// on the GPU the way Chrome's accelerated canvas draws them:
//   - the backing store is premultiplied RGBA8 (an image's texels premultiplied on upload, SkMulDiv255Round);
//   - drawImage samples bilinearly (imageSmoothingEnabled, quality 'low'), clamped half a texel inside the source rect
//     (Blink's kClampImageToSourceRect: SkCanvas::kStrict_SrcRectConstraint), with no mips;
//   - an axis-aligned destination rect's edges are anti-aliased by their pixel coverage;
//   - source-over in premultiplied space.
// The canvas is then composited over the 3D frame the way the page's stage stacks #ui over #game: stretched from 640 x 448 to
// the frame (CSS size), bilinearly, premultiplied over.
// Every call is also logged (when logging is on) in the shape native/tools/browser-shot.mjs --canvas-log records from the
// page, so the HUD port is checked call for call before the pictures are compared.
#include "gpu.hpp"
#include "json.hpp"
#include <array>
#include <functional>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ssx_native {

// A canvas image source: a decoded PNG (HTMLImageElement) or an offscreen canvas (a tint), its texels premultiplied as the
// canvas holds them
struct HudImage {
    // the page's identity for the log: the PNG's name, or the content hash of a canvas source
    std::string name;
    int width = 0;
    int height = 0;
    // premultiplied RGBA8
    std::vector<uint8_t> premul;
    WGPUTexture texture = nullptr;
    WGPUBindGroup group = nullptr;
    bool canvas = false;
};

// premultiply as Skia does (SkMulDiv255Round), unpremultiply as Chrome's getImageData (float32 stages, nearest even)
uint8_t mulDiv255Round(uint32_t a, uint32_t b);
std::vector<uint8_t> premultiply(const std::vector<uint8_t>& rgba);
std::vector<uint8_t> unpremultiply(const std::vector<uint8_t>& premul);
// FNV-1a of a canvas source's getImageData bytes (the log's identity of an offscreen canvas)
uint32_t imageDataHash(const std::vector<uint8_t>& unpremul);

class HudCanvas {
public:
    static constexpr int kWidth = 640;
    static constexpr int kHeight = 448;

    void init(Gpu& gpu);
    // a PNG under dir (its name without .png is the image's name)
    int loadPng(const std::string& path, const std::string& name);
    // an offscreen canvas's bytes (premultiplied)
    int addCanvas(int width, int height, std::vector<uint8_t> premul);
    const HudImage& image(int k) const {
        return images_[size_t(k)];
    }
    int imageCount() const {
        return int(images_.size());
    }

    // ---- CanvasRenderingContext2D ----
    void save();
    void restore();
    void translate(double x, double y);
    void scale(double x, double y);
    void rotate(double radians);
    double globalAlpha() const {
        return state_.alpha;
    }
    // the setter as the canvas takes it: non-finite or outside [0, 1] is ignored
    void setGlobalAlpha(double a);
    void drawImage(int image, double sx, double sy, double sw, double sh, double dx, double dy, double dw, double dh);
    void drawImage(int image, double dx, double dy, double dw, double dh);
    void drawImage(int image, double dx, double dy);
    // fillStyle = rgb(r, g, b) (bytes), fillRect
    void fillRect(double x, double y, double w, double h, std::array<uint8_t, 3> rgb);
    // web/input-glyphs.js drawKeyCap's paths: the rounded rect (vertical gradient #4a525a -> #101316, outline #eef3f6 of lineWidth),
    // and the arrow triangle (#f0f5f8) at the current transform
    void keyCapBox(double x, double y, double w, double h, double radius, double lineWidth);
    void keyCapArrow();
    // beginPath, a closed polygon (user space), fill with paint(ux, uy) -> straight RGBA 0..1 (a solid colour or a gradient's)
    using Paint = std::function<std::array<double, 4>(double, double)>;
    // paintKey: the paint's identity (its colours and stops); with it a repeated fill reuses its raster
    void fillPolygon(const std::vector<std::array<double, 2>>& points, const Paint& paint, bool gradient, const std::string& paintKey = "");
    // several closed paths filled as one (nonzero)
    void fillPolygons(const std::vector<std::vector<std::array<double, 2>>>& paths, const Paint& paint, bool gradient, const std::string& paintKey = "");
    // a closed path stroked (lineWidth, a solid colour)
    void strokePolygon(const std::vector<std::array<double, 2>>& points, const std::array<double, 4>& colour, double lineWidth, const std::string& paintKey = "");

    // a frame: cleared, the calls, then encode
    void begin();
    // draws the canvas into its 640 x 448 target, then over `frame` (fw x fh) as the stage composites #ui over #game
    void encode(WGPUCommandEncoder encoder, WGPUTextureView frame, uint32_t fw, uint32_t fh);
    bool empty() const {
        return quads_.empty();
    }

    bool logging = false;
    nlohmann::json log = nlohmann::json::array();

private:
    struct Matrix {
        double a = 1, b = 0, c = 0, d = 1, e = 0, f = 0;
    };
    struct State {
        Matrix m;
        double alpha = 1;
    };
    // one quad of the GPU list (matches the WGSL Quad)
    struct Quad {
        // destination rect in canvas pixels (x0 < x1, y0 < y1)
        float dst[4];
        // the texel coordinates at dst's corners (x0 -> u0, x1 -> u1, y0 -> v0, y1 -> v1: a flip swaps them)
        float uv[4];
        // the sampling clamp in texels (strict source rect)
        float clamp[4];
        // colour (fill: rgb 0..1; image: 1) and alpha
        float colour[4];
        // the image's size, mode (0 image, 1 fill)
        float info[4];
        // a rotated image: the canvas-to-user inverse (a b c d), (e f, 1, the user size of a device pixel), the user-space rect
        float inv[4];
        float affine[4];
        float rect[4];
    };
    struct Batch {
        int image = -1;
        uint32_t first = 0;
        uint32_t count = 0;
    };
    void logCall(const char* op, nlohmann::json args, int image = -1);
    void push(const Quad& q, int image);
    int shapeImage(const std::vector<uint8_t>& premul, int w, int h);
    WGPUBindGroup groupFor(HudImage& im);

    Gpu* gpu_ = nullptr;
    std::vector<HudImage> images_;
    State state_;
    std::vector<State> stack_;
    std::vector<Quad> quads_;
    std::vector<Batch> batches_;
    // the per-frame shape images (key caps): reused by content
    std::map<std::string, int> shapes_;
    void rasterPaths(const std::vector<std::vector<std::array<double, 2>>>& paths, const Paint& paint, const std::string& paintKey,
                     const std::function<bool(double, double)>& inside, double pad);
    // fillPolygon rasters by their inputs
    std::map<std::string, int> fills_;

    WGPUTexture target_ = nullptr;
    WGPUTextureView targetView_ = nullptr;
    WGPUBuffer quadBuffer_ = nullptr;
    size_t quadCapacity_ = 0;
    WGPUBindGroupLayout quadLayout_ = nullptr;
    WGPUBindGroupLayout imageLayout_ = nullptr;
    WGPUBindGroupLayout compositeLayout_ = nullptr;
    WGPUBindGroup quadGroup_ = nullptr;
    WGPUBindGroup compositeGroup_ = nullptr;
    WGPURenderPipeline quadPipeline_ = nullptr;
    WGPURenderPipeline compositePipeline_ = nullptr;
    WGPUSampler linear_ = nullptr;
    int white_ = -1;
};

}  // namespace ssx_native
