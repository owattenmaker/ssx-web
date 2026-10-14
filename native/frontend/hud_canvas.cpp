#include "hud_canvas.hpp"
#include "image_io.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace ssx_native {

namespace {

// one quad: the destination rect grown by a pixel (the edge coverage), the texel coordinate interpolated in canvas pixels,
// clamped to the strict source rect, sampled bilinearly; premultiplied output for source-over (One, OneMinusSrcAlpha)
const char* kQuadShader = R"(
struct Quad {
  dst: vec4f,
  uv: vec4f,
  clampRect: vec4f,
  colour: vec4f,
  info: vec4f,
};
@group(0) @binding(0) var<storage, read> quads: array<Quad>;
@group(1) @binding(0) var image: texture_2d<f32>;
@group(1) @binding(1) var linearSampler: sampler;
struct Out {
  @builtin(position) position: vec4f,
  @location(0) @interpolate(flat) index: u32,
};
@vertex fn vs(@builtin(vertex_index) v: u32, @builtin(instance_index) i: u32) -> Out {
  let q = quads[i];
  let corner = vec2f(f32(v & 1u), f32((v >> 1u) & 1u));
  let lo = q.dst.xy - vec2f(1.0);
  let hi = q.dst.zw + vec2f(1.0);
  let p = mix(lo, hi, corner);
  var o: Out;
  o.position = vec4f(p.x / 320.0 - 1.0, 1.0 - p.y / 224.0, 0.0, 1.0);
  o.index = i;
  return o;
}
@fragment fn fs(i: Out) -> @location(0) vec4f {
  let q = quads[i.index];
  let p = i.position.xy;
  let cx = clamp(min(p.x + 0.5, q.dst.z) - max(p.x - 0.5, q.dst.x), 0.0, 1.0);
  let cy = clamp(min(p.y + 0.5, q.dst.w) - max(p.y - 0.5, q.dst.y), 0.0, 1.0);
  let coverage = cx * cy;
  let alpha = q.colour.a * coverage;
  if (q.info.z > 0.5) {
    return vec4f(q.colour.rgb * alpha, alpha);
  }
  let t = (p - q.dst.xy) / (q.dst.zw - q.dst.xy);
  let texel = clamp(mix(q.uv.xy, q.uv.zw, t), q.clampRect.xy, q.clampRect.zw);
  let s = textureSampleLevel(image, linearSampler, texel / q.info.xy, 0.0);
  return s * alpha;
}
)";

// the stage composite: the 640 x 448 canvas stretched over the frame, bilinear, premultiplied over
const char* kCompositeShader = R"(
@group(0) @binding(0) var hud: texture_2d<f32>;
@group(0) @binding(1) var linearSampler: sampler;
struct Size {
  frame: vec4f,
};
@group(0) @binding(2) var<uniform> size: Size;
@vertex fn vs(@builtin(vertex_index) v: u32) -> @builtin(position) vec4f {
  let p = vec2f(f32((v << 1u) & 2u), f32(v & 2u));
  return vec4f(p * 2.0 - 1.0, 0.0, 1.0);
}
@fragment fn fs(@builtin(position) p: vec4f) -> @location(0) vec4f {
  return textureSampleLevel(hud, linearSampler, p.xy / size.frame.xy, 0.0);
}
)";

WGPUBlendState premultipliedOver() {
    WGPUBlendState b{};
    b.color = {WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_OneMinusSrcAlpha};
    b.alpha = {WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_OneMinusSrcAlpha};
    return b;
}

nlohmann::json number(double v) {
    // the log's numbers as JS prints them (an integer-valued double as an integer)
    if (std::isfinite(v) && v == std::floor(v) && std::abs(v) < 9007199254740992.0) {
        return nlohmann::json(int64_t(v));
    }
    return nlohmann::json(v);
}

}  // namespace

uint8_t mulDiv255Round(uint32_t a, uint32_t b) {
    const uint32_t prod = a * b + 128;
    return uint8_t((prod + (prod >> 8)) >> 8);
}

std::vector<uint8_t> premultiply(const std::vector<uint8_t>& rgba) {
    std::vector<uint8_t> out(rgba.size());
    for (size_t i = 0; i + 3 < rgba.size(); i += 4) {
        const uint32_t a = rgba[i + 3];
        out[i] = mulDiv255Round(rgba[i], a);
        out[i + 1] = mulDiv255Round(rgba[i + 1], a);
        out[i + 2] = mulDiv255Round(rgba[i + 2], a);
        out[i + 3] = uint8_t(a);
    }
    return out;
}

std::vector<uint8_t> unpremultiply(const std::vector<uint8_t>& premul) {
    // getImageData's conversion as Chrome's readback runs it (matched on every texel of OV_1-4): float32 stages, the colour
    // x (1 / 255) x 1 / (alpha x (1 / 255)), x 255, rounded to nearest even
    const float k = 1.0f / 255.0f;
    std::vector<uint8_t> out(premul.size());
    for (size_t i = 0; i + 3 < premul.size(); i += 4) {
        const uint32_t a = premul[i + 3];
        out[i + 3] = uint8_t(a);
        if (a == 0) {
            out[i] = out[i + 1] = out[i + 2] = 0;
            continue;
        }
        const float inv = 1.0f / (float(a) * k);
        for (int c = 0; c < 3; ++c) {
            const float r = (float(premul[i + size_t(c)]) * k) * inv;
            const float x = r * 255.0f;
            out[i + size_t(c)] = uint8_t(std::min(255.0f, std::nearbyint(x)));
        }
    }
    return out;
}

uint32_t imageDataHash(const std::vector<uint8_t>& unpremul) {
    uint32_t h = 2166136261u;
    for (uint8_t b : unpremul) {
        h ^= b;
        h *= 16777619u;
    }
    return h;
}

void HudCanvas::init(Gpu& gpu) {
    gpu_ = &gpu;
    WGPUDevice d = gpu.device;
    target_ = gpu.texture(kWidth, kHeight, WGPUTextureFormat_RGBA8Unorm, WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding, 1, "hud canvas");
    WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    targetView_ = wgpuTextureCreateView(target_, &vd);
    WGPUSamplerDescriptor sd = WGPU_SAMPLER_DESCRIPTOR_INIT;
    sd.magFilter = WGPUFilterMode_Linear;
    sd.minFilter = WGPUFilterMode_Linear;
    sd.addressModeU = sd.addressModeV = WGPUAddressMode_ClampToEdge;
    linear_ = wgpuDeviceCreateSampler(d, &sd);

    WGPUBindGroupLayoutEntry qe = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    qe.binding = 0;
    qe.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    qe.buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    WGPUBindGroupLayoutDescriptor qd = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    qd.entryCount = 1;
    qd.entries = &qe;
    quadLayout_ = wgpuDeviceCreateBindGroupLayout(d, &qd);
    WGPUBindGroupLayoutEntry ie[2];
    ie[0] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    ie[0].binding = 0;
    ie[0].visibility = WGPUShaderStage_Fragment;
    ie[0].texture.sampleType = WGPUTextureSampleType_Float;
    ie[0].texture.viewDimension = WGPUTextureViewDimension_2D;
    ie[1] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    ie[1].binding = 1;
    ie[1].visibility = WGPUShaderStage_Fragment;
    ie[1].sampler.type = WGPUSamplerBindingType_Filtering;
    WGPUBindGroupLayoutDescriptor id = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    id.entryCount = 2;
    id.entries = ie;
    imageLayout_ = wgpuDeviceCreateBindGroupLayout(d, &id);
    WGPUBindGroupLayoutEntry ce[3];
    ce[0] = ie[0];
    ce[1] = ie[1];
    ce[2] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    ce[2].binding = 2;
    ce[2].visibility = WGPUShaderStage_Fragment;
    ce[2].buffer.type = WGPUBufferBindingType_Uniform;
    WGPUBindGroupLayoutDescriptor cd = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    cd.entryCount = 3;
    cd.entries = ce;
    compositeLayout_ = wgpuDeviceCreateBindGroupLayout(d, &cd);

    const WGPUBlendState over = premultipliedOver();
    auto pipeline = [&](const char* source, const char* label, std::vector<WGPUBindGroupLayout> layouts, WGPUPrimitiveTopology topology) {
        WGPUShaderModule module = gpu.shader(source, label);
        WGPUPipelineLayoutDescriptor pl = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
        pl.bindGroupLayoutCount = layouts.size();
        pl.bindGroupLayouts = layouts.data();
        WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(d, &pl);
        WGPURenderPipelineDescriptor pd = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        pd.layout = layout;
        pd.vertex.module = module;
        pd.vertex.entryPoint = sv("vs");
        pd.primitive.topology = topology;
        pd.primitive.cullMode = WGPUCullMode_None;
        WGPUColorTargetState ct = WGPU_COLOR_TARGET_STATE_INIT;
        ct.format = WGPUTextureFormat_RGBA8Unorm;
        ct.blend = &over;
        ct.writeMask = WGPUColorWriteMask_All;
        WGPUFragmentState fs = WGPU_FRAGMENT_STATE_INIT;
        fs.module = module;
        fs.entryPoint = sv("fs");
        fs.targetCount = 1;
        fs.targets = &ct;
        pd.fragment = &fs;
        pd.multisample.count = 1;
        pd.multisample.mask = 0xffffffffu;
        WGPURenderPipeline p = wgpuDeviceCreateRenderPipeline(d, &pd);
        wgpuPipelineLayoutRelease(layout);
        wgpuShaderModuleRelease(module);
        return p;
    };
    quadPipeline_ = pipeline(kQuadShader, "hud quads", {quadLayout_, imageLayout_}, WGPUPrimitiveTopology_TriangleStrip);
    compositePipeline_ = pipeline(kCompositeShader, "hud composite", {compositeLayout_}, WGPUPrimitiveTopology_TriangleList);
    // a white texel for the fills (bound with them; unused by the shader's fill branch)
    white_ = addCanvas(1, 1, {255, 255, 255, 255});
}

int HudCanvas::loadPng(const std::string& path, const std::string& name) {
    const Image img = readPng(path);
    HudImage im;
    im.name = name;
    im.width = img.width;
    im.height = img.height;
    im.premul = premultiply(img.rgba);
    images_.push_back(std::move(im));
    return int(images_.size()) - 1;
}

int HudCanvas::addCanvas(int width, int height, std::vector<uint8_t> premul) {
    HudImage im;
    im.width = width;
    im.height = height;
    im.premul = std::move(premul);
    im.canvas = true;
    char name[32];
    std::snprintf(name, sizeof name, "canvas:%08x", imageDataHash(unpremultiply(im.premul)));
    im.name = name;
    images_.push_back(std::move(im));
    return int(images_.size()) - 1;
}

WGPUBindGroup HudCanvas::groupFor(HudImage& im) {
    if (im.group) {
        return im.group;
    }
    im.texture = gpu_->texture(uint32_t(im.width), uint32_t(im.height), WGPUTextureFormat_RGBA8Unorm,
                               WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst, 1, "hud image");
    gpu_->writeTexture(im.texture, uint32_t(im.width), uint32_t(im.height), im.premul.data(), 0);
    WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    WGPUTextureView view = wgpuTextureCreateView(im.texture, &vd);
    WGPUBindGroupEntry e[2];
    e[0] = WGPU_BIND_GROUP_ENTRY_INIT;
    e[0].binding = 0;
    e[0].textureView = view;
    e[1] = WGPU_BIND_GROUP_ENTRY_INIT;
    e[1].binding = 1;
    e[1].sampler = linear_;
    WGPUBindGroupDescriptor bd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bd.layout = imageLayout_;
    bd.entryCount = 2;
    bd.entries = e;
    im.group = wgpuDeviceCreateBindGroup(gpu_->device, &bd);
    wgpuTextureViewRelease(view);
    return im.group;
}

void HudCanvas::save() {
    stack_.push_back(state_);
}

void HudCanvas::restore() {
    if (!stack_.empty()) {
        state_ = stack_.back();
        stack_.pop_back();
    }
}

// Blink's transform methods take their arguments as float (ClampTo<float>) and compose the matrix in double
void HudCanvas::translate(double x, double y) {
    x = double(float(x));
    y = double(float(y));
    Matrix& m = state_.m;
    m.e += m.a * x + m.c * y;
    m.f += m.b * x + m.d * y;
}

void HudCanvas::scale(double x, double y) {
    x = double(float(x));
    y = double(float(y));
    Matrix& m = state_.m;
    m.a *= x;
    m.b *= x;
    m.c *= y;
    m.d *= y;
}

void HudCanvas::rotate(double radians) {
    Matrix& m = state_.m;
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    const Matrix o = m;
    m.a = o.a * c + o.c * s;
    m.b = o.b * c + o.d * s;
    m.c = o.c * c - o.a * s;
    m.d = o.d * c - o.b * s;
}

void HudCanvas::setGlobalAlpha(double a) {
    if (std::isfinite(a) && a >= 0 && a <= 1) {
        state_.alpha = a;
    }
}

void HudCanvas::logCall(const char* op, nlohmann::json args, int image) {
    if (!logging) {
        return;
    }
    nlohmann::json e;
    e["op"] = op;
    nlohmann::json a = nlohmann::json::array();
    for (const auto& v : args) {
        a.push_back(v.is_number() ? number(v.get<double>()) : v);
    }
    e["args"] = a;
    const Matrix& m = state_.m;
    e["m"] = {number(m.a), number(m.b), number(m.c), number(m.d), number(m.e), number(m.f)};
    e["alpha"] = number(state_.alpha);
    if (image >= 0) {
        e["img"] = images_[size_t(image)].name;
    }
    log.push_back(e);
}

void HudCanvas::push(const Quad& q, int image) {
    if (batches_.empty() || batches_.back().image != image) {
        batches_.push_back({image, uint32_t(quads_.size()), 0});
    }
    quads_.push_back(q);
    ++batches_.back().count;
}

void HudCanvas::drawImage(int image, double sx, double sy, double sw, double sh, double dx, double dy, double dw, double dh) {
    logCall("drawImage", {sx, sy, sw, sh, dx, dy, dw, dh}, image);
    if (image < 0 || !(state_.alpha > 0)) {
        return;
    }
    const HudImage& im = images_[size_t(image)];
    if (sw == 0 || sh == 0 || dw == 0 || dh == 0) {
        return;
    }
    // the spec's normalised rectangles
    if (sw < 0) {
        sx += sw;
        sw = -sw;
    }
    if (sh < 0) {
        sy += sh;
        sh = -sh;
    }
    if (dw < 0) {
        dx += dw;
        dw = -dw;
    }
    if (dh < 0) {
        dy += dh;
        dh = -dh;
    }
    // the source clipped to the image, the destination with it
    const double kx = dw / sw;
    const double ky = dh / sh;
    if (sx < 0) {
        dx -= sx * kx;
        dw += sx * kx;
        sw += sx;
        sx = 0;
    }
    if (sy < 0) {
        dy -= sy * ky;
        dh += sy * ky;
        sh += sy;
        sy = 0;
    }
    if (sx + sw > im.width) {
        const double over = sx + sw - im.width;
        dw -= over * kx;
        sw -= over;
    }
    if (sy + sh > im.height) {
        const double over = sy + sh - im.height;
        dh -= over * ky;
        sh -= over;
    }
    if (!(sw > 0) || !(sh > 0)) {
        return;
    }
    const Matrix& m = state_.m;
    if (m.b != 0 || m.c != 0) {
        // the HUD draws images under scale and translate only
        return;
    }
    double x0 = m.a * dx + m.e;
    double x1 = m.a * (dx + dw) + m.e;
    double y0 = m.d * dy + m.f;
    double y1 = m.d * (dy + dh) + m.f;
    double u0 = sx;
    double u1 = sx + sw;
    double v0 = sy;
    double v1 = sy + sh;
    if (x1 < x0) {
        std::swap(x0, x1);
        std::swap(u0, u1);
    }
    if (y1 < y0) {
        std::swap(y0, y1);
        std::swap(v0, v1);
    }
    Quad q{};
    q.dst[0] = float(x0);
    q.dst[1] = float(y0);
    q.dst[2] = float(x1);
    q.dst[3] = float(y1);
    q.uv[0] = float(u0);
    q.uv[1] = float(v0);
    q.uv[2] = float(u1);
    q.uv[3] = float(v1);
    // the strict source rect: sampling stays half a texel inside it
    const double cx0 = sw >= 1 ? sx + 0.5 : sx + sw * 0.5;
    const double cx1 = sw >= 1 ? sx + sw - 0.5 : sx + sw * 0.5;
    const double cy0 = sh >= 1 ? sy + 0.5 : sy + sh * 0.5;
    const double cy1 = sh >= 1 ? sy + sh - 0.5 : sy + sh * 0.5;
    q.clamp[0] = float(cx0);
    q.clamp[1] = float(cy0);
    q.clamp[2] = float(cx1);
    q.clamp[3] = float(cy1);
    q.colour[0] = q.colour[1] = q.colour[2] = 1;
    q.colour[3] = float(state_.alpha);
    q.info[0] = float(im.width);
    q.info[1] = float(im.height);
    q.info[2] = 0;
    push(q, image);
}

void HudCanvas::drawImage(int image, double dx, double dy, double dw, double dh) {
    const HudImage& im = images_[size_t(image)];
    drawImage(image, 0, 0, im.width, im.height, dx, dy, dw, dh);
}

void HudCanvas::drawImage(int image, double dx, double dy) {
    const HudImage& im = images_[size_t(image)];
    drawImage(image, 0, 0, im.width, im.height, dx, dy, im.width, im.height);
}

void HudCanvas::fillRect(double x, double y, double w, double h, std::array<uint8_t, 3> rgb) {
    char style[16];
    std::snprintf(style, sizeof style, "#%02x%02x%02x", rgb[0], rgb[1], rgb[2]);
    logCall("fillRect", {x, y, w, h, std::string(style)});
    if (!(state_.alpha > 0) || w == 0 || h == 0) {
        return;
    }
    if (w < 0) {
        x += w;
        w = -w;
    }
    if (h < 0) {
        y += h;
        h = -h;
    }
    const Matrix& m = state_.m;
    if (m.b != 0 || m.c != 0) {
        return;
    }
    double x0 = m.a * x + m.e;
    double x1 = m.a * (x + w) + m.e;
    double y0 = m.d * y + m.f;
    double y1 = m.d * (y + h) + m.f;
    if (x1 < x0) {
        std::swap(x0, x1);
    }
    if (y1 < y0) {
        std::swap(y0, y1);
    }
    Quad q{};
    q.dst[0] = float(x0);
    q.dst[1] = float(y0);
    q.dst[2] = float(x1);
    q.dst[3] = float(y1);
    q.colour[0] = rgb[0] / 255.f;
    q.colour[1] = rgb[1] / 255.f;
    q.colour[2] = rgb[2] / 255.f;
    q.colour[3] = float(state_.alpha);
    q.info[0] = 1;
    q.info[1] = 1;
    q.info[2] = 1;
    push(q, white_);
}

int HudCanvas::shapeImage(const std::vector<uint8_t>& premul, int w, int h) {
    // the key caps repeat frame after frame: one image per distinct raster
    std::string key(reinterpret_cast<const char*>(premul.data()), premul.size());
    key += std::to_string(w) + "x" + std::to_string(h);
    auto it = shapes_.find(key);
    if (it != shapes_.end()) {
        return it->second;
    }
    const int k = addCanvas(w, h, premul);
    shapes_[key] = k;
    return k;
}

namespace {

// a path rasterized at 4 x 4 samples a pixel into device pixels: inside(ux, uy) in user space, colour(ux, uy) its paint
template <class Inside, class Paint>
std::vector<uint8_t> rasterize(const double inv[6], int x0, int y0, int w, int h, Inside inside, Paint paint) {
    std::vector<uint8_t> out(size_t(w) * h * 4, 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            double acc[4] = {0, 0, 0, 0};
            for (int sy = 0; sy < 4; ++sy) {
                for (int sx = 0; sx < 4; ++sx) {
                    const double px = x0 + x + (sx + 0.5) / 4;
                    const double py = y0 + y + (sy + 0.5) / 4;
                    const double ux = inv[0] * px + inv[2] * py + inv[4];
                    const double uy = inv[1] * px + inv[3] * py + inv[5];
                    if (!inside(ux, uy)) {
                        continue;
                    }
                    const std::array<double, 3> c = paint(ux, uy);
                    acc[0] += c[0];
                    acc[1] += c[1];
                    acc[2] += c[2];
                    acc[3] += 1;
                }
            }
            const double cover = acc[3] / 16;
            const size_t o = (size_t(y) * w + x) * 4;
            if (acc[3] == 0) {
                continue;
            }
            // premultiplied: the mean paint of the covered samples x the coverage
            for (int k = 0; k < 3; ++k) {
                out[o + size_t(k)] = uint8_t(std::lround(acc[k] / acc[3] * cover * 255));
            }
            out[o + 3] = uint8_t(std::lround(cover * 255));
        }
    }
    return out;
}

// the signed distance to a rounded rect (negative inside)
double roundRectDistance(double ux, double uy, double x, double y, double w, double h, double r) {
    const double cx = x + w / 2;
    const double cy = y + h / 2;
    const double rr = std::min(r, std::min(w, h) / 2);
    const double qx = std::abs(ux - cx) - (w / 2 - rr);
    const double qy = std::abs(uy - cy) - (h / 2 - rr);
    const double ox = std::max(qx, 0.0);
    const double oy = std::max(qy, 0.0);
    return std::hypot(ox, oy) + std::min(std::max(qx, qy), 0.0) - rr;
}

}  // namespace

void HudCanvas::keyCapBox(double x, double y, double w, double h, double radius, double lineWidth) {
    logCall("roundRect", {x, y, w, h, radius});
    logCall("fill", {std::string("gradient")});
    logCall("stroke", {std::string("#eef3f6"), lineWidth});
    if (!(state_.alpha > 0)) {
        return;
    }
    const Matrix& m = state_.m;
    const double det = m.a * m.d - m.b * m.c;
    if (det == 0) {
        return;
    }
    const double inv[6] = {m.d / det, -m.b / det, -m.c / det, m.a / det, (m.c * m.f - m.d * m.e) / det, (m.b * m.e - m.a * m.f) / det};
    // the device bounds of the stroked rect
    const double pad = lineWidth;
    double bx0 = 1e30, by0 = 1e30, bx1 = -1e30, by1 = -1e30;
    for (int k = 0; k < 4; ++k) {
        const double ux = (k & 1) ? x + w + pad : x - pad;
        const double uy = (k & 2) ? y + h + pad : y - pad;
        const double px = m.a * ux + m.c * uy + m.e;
        const double py = m.b * ux + m.d * uy + m.f;
        bx0 = std::min(bx0, px);
        by0 = std::min(by0, py);
        bx1 = std::max(bx1, px);
        by1 = std::max(by1, py);
    }
    const int ix0 = int(std::floor(bx0)) - 1;
    const int iy0 = int(std::floor(by0)) - 1;
    const int iw = int(std::ceil(bx1)) + 1 - ix0;
    const int ih = int(std::ceil(by1)) + 1 - iy0;
    if (iw <= 0 || ih <= 0 || iw > 2048 || ih > 2048) {
        return;
    }
    // fill: the vertical gradient #4a525a -> #101316 over [y, y + h]
    const auto fill = rasterize(
        inv, ix0, iy0, iw, ih, [&](double ux, double uy) { return roundRectDistance(ux, uy, x, y, w, h, radius) <= 0; },
        [&](double, double uy) {
            const double t = std::clamp((uy - y) / h, 0.0, 1.0);
            return std::array<double, 3>{(0x4a + (0x10 - 0x4a) * t) / 255, (0x52 + (0x13 - 0x52) * t) / 255, (0x5a + (0x16 - 0x5a) * t) / 255};
        });
    const auto stroke = rasterize(
        inv, ix0, iy0, iw, ih, [&](double ux, double uy) { return std::abs(roundRectDistance(ux, uy, x, y, w, h, radius)) <= lineWidth / 2; },
        [&](double, double) { return std::array<double, 3>{0xee / 255.0, 0xf3 / 255.0, 0xf6 / 255.0}; });
    for (const auto* layer : {&fill, &stroke}) {
        const int k = shapeImage(*layer, iw, ih);
        Quad q{};
        q.dst[0] = float(ix0);
        q.dst[1] = float(iy0);
        q.dst[2] = float(ix0 + iw);
        q.dst[3] = float(iy0 + ih);
        q.uv[0] = 0;
        q.uv[1] = 0;
        q.uv[2] = float(iw);
        q.uv[3] = float(ih);
        q.clamp[0] = 0.5f;
        q.clamp[1] = 0.5f;
        q.clamp[2] = float(iw) - 0.5f;
        q.clamp[3] = float(ih) - 0.5f;
        q.colour[0] = q.colour[1] = q.colour[2] = 1;
        q.colour[3] = float(state_.alpha);
        q.info[0] = float(iw);
        q.info[1] = float(ih);
        push(q, k);
    }
}

void HudCanvas::keyCapArrow() {
    logCall("arrow", {});
    if (!(state_.alpha > 0)) {
        return;
    }
    const Matrix& m = state_.m;
    const double det = m.a * m.d - m.b * m.c;
    if (det == 0) {
        return;
    }
    const double inv[6] = {m.d / det, -m.b / det, -m.c / det, m.a / det, (m.c * m.f - m.d * m.e) / det, (m.b * m.e - m.a * m.f) / det};
    // moveTo(4, 0), lineTo(-3, -4), lineTo(-3, 4), closePath, fill #f0f5f8
    const double tx[3] = {4, -3, -3};
    const double ty[3] = {0, -4, 4};
    double bx0 = 1e30, by0 = 1e30, bx1 = -1e30, by1 = -1e30;
    for (int k = 0; k < 3; ++k) {
        const double px = m.a * tx[k] + m.c * ty[k] + m.e;
        const double py = m.b * tx[k] + m.d * ty[k] + m.f;
        bx0 = std::min(bx0, px);
        by0 = std::min(by0, py);
        bx1 = std::max(bx1, px);
        by1 = std::max(by1, py);
    }
    const int ix0 = int(std::floor(bx0)) - 1;
    const int iy0 = int(std::floor(by0)) - 1;
    const int iw = int(std::ceil(bx1)) + 1 - ix0;
    const int ih = int(std::ceil(by1)) + 1 - iy0;
    auto edge = [](double ax, double ay, double bx, double by, double px, double py) { return (bx - ax) * (py - ay) - (by - ay) * (px - ax); };
    const auto tri = rasterize(
        inv, ix0, iy0, iw, ih,
        [&](double ux, double uy) {
            const double e0 = edge(tx[0], ty[0], tx[1], ty[1], ux, uy);
            const double e1 = edge(tx[1], ty[1], tx[2], ty[2], ux, uy);
            const double e2 = edge(tx[2], ty[2], tx[0], ty[0], ux, uy);
            return (e0 >= 0 && e1 >= 0 && e2 >= 0) || (e0 <= 0 && e1 <= 0 && e2 <= 0);
        },
        [&](double, double) { return std::array<double, 3>{0xf0 / 255.0, 0xf5 / 255.0, 0xf8 / 255.0}; });
    const int k = shapeImage(tri, iw, ih);
    Quad q{};
    q.dst[0] = float(ix0);
    q.dst[1] = float(iy0);
    q.dst[2] = float(ix0 + iw);
    q.dst[3] = float(iy0 + ih);
    q.uv[2] = float(iw);
    q.uv[3] = float(ih);
    q.clamp[0] = 0.5f;
    q.clamp[1] = 0.5f;
    q.clamp[2] = float(iw) - 0.5f;
    q.clamp[3] = float(ih) - 0.5f;
    q.colour[0] = q.colour[1] = q.colour[2] = 1;
    q.colour[3] = float(state_.alpha);
    q.info[0] = float(iw);
    q.info[1] = float(ih);
    push(q, k);
}

void HudCanvas::begin() {
    quads_.clear();
    batches_.clear();
    stack_.clear();
    state_ = State{};
    log = nlohmann::json::array();
}

void HudCanvas::encode(WGPUCommandEncoder encoder, WGPUTextureView frame, uint32_t fw, uint32_t fh) {
    WGPUDevice d = gpu_->device;
    const size_t bytes = std::max<size_t>(1, quads_.size()) * sizeof(Quad);
    if (bytes > quadCapacity_) {
        if (quadBuffer_) {
            wgpuBufferRelease(quadBuffer_);
        }
        if (quadGroup_) {
            wgpuBindGroupRelease(quadGroup_);
        }
        quadCapacity_ = std::max(bytes, size_t(4096) * sizeof(Quad));
        quadBuffer_ = gpu_->buffer(quadCapacity_, WGPUBufferUsage_Storage, nullptr, "hud quads");
        WGPUBindGroupEntry e = WGPU_BIND_GROUP_ENTRY_INIT;
        e.binding = 0;
        e.buffer = quadBuffer_;
        e.size = quadCapacity_;
        WGPUBindGroupDescriptor bd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        bd.layout = quadLayout_;
        bd.entryCount = 1;
        bd.entries = &e;
        quadGroup_ = wgpuDeviceCreateBindGroup(d, &bd);
    }
    if (!quads_.empty()) {
        wgpuQueueWriteBuffer(gpu_->queue, quadBuffer_, 0, quads_.data(), quads_.size() * sizeof(Quad));
    }
    for (const auto& b : batches_) {
        groupFor(images_[size_t(b.image)]);
    }
    // the canvas: cleared (ui.draw's clearRect), then the calls in order
    WGPURenderPassColorAttachment ca = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    ca.view = targetView_;
    ca.loadOp = WGPULoadOp_Clear;
    ca.storeOp = WGPUStoreOp_Store;
    ca.clearValue = {0, 0, 0, 0};
    WGPURenderPassDescriptor rp = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    rp.colorAttachmentCount = 1;
    rp.colorAttachments = &ca;
    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &rp);
    wgpuRenderPassEncoderSetPipeline(pass, quadPipeline_);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, quadGroup_, 0, nullptr);
    for (const auto& b : batches_) {
        wgpuRenderPassEncoderSetBindGroup(pass, 1, images_[size_t(b.image)].group, 0, nullptr);
        wgpuRenderPassEncoderDraw(pass, 4, b.count, 0, b.first);
    }
    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);
    if (quads_.empty()) {
        return;
    }
    // the stage: #ui over #game
    if (!compositeGroup_) {
        const float size[4] = {float(fw), float(fh), 0, 0};
        WGPUBuffer ub = gpu_->buffer(16, WGPUBufferUsage_Uniform, size, "hud composite size");
        WGPUBindGroupEntry e[3];
        for (int k = 0; k < 3; ++k) {
            e[k] = WGPU_BIND_GROUP_ENTRY_INIT;
            e[k].binding = uint32_t(k);
        }
        e[0].textureView = targetView_;
        e[1].sampler = linear_;
        e[2].buffer = ub;
        e[2].size = 16;
        WGPUBindGroupDescriptor bd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        bd.layout = compositeLayout_;
        bd.entryCount = 3;
        bd.entries = e;
        compositeGroup_ = wgpuDeviceCreateBindGroup(d, &bd);
    }
    WGPURenderPassColorAttachment fa = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
    fa.view = frame;
    fa.loadOp = WGPULoadOp_Load;
    fa.storeOp = WGPUStoreOp_Store;
    WGPURenderPassDescriptor fp = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    fp.colorAttachmentCount = 1;
    fp.colorAttachments = &fa;
    WGPURenderPassEncoder comp = wgpuCommandEncoderBeginRenderPass(encoder, &fp);
    wgpuRenderPassEncoderSetPipeline(comp, compositePipeline_);
    wgpuRenderPassEncoderSetBindGroup(comp, 0, compositeGroup_, 0, nullptr);
    wgpuRenderPassEncoderDraw(comp, 3, 1, 0, 0);
    wgpuRenderPassEncoderEnd(comp);
    wgpuRenderPassEncoderRelease(comp);
}

}  // namespace ssx_native
