#include "rider_fx.hpp"
#include "../verify/core_api.hpp"
#include "draw_order.hpp"
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace ssx_native {

namespace {

const char* kFxShader = R"(
struct Frame { viewProjection: mat4x4f };
@group(0) @binding(0) var<uniform> frame: Frame;
@group(1) @binding(0) var mapTex: texture_2d<f32>;
@group(1) @binding(1) var mapSampler: sampler;
struct Out {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
  @location(1) colour: vec4f,
  @location(2) alphaScale: f32,
};
// the trail ribbon / roof and the wake strip: origin-relative native positions
struct Strip {
  @location(0) position: vec3f,
  @location(1) uv: vec2f,
  @location(2) colour: vec4f,
  @location(3) alphaScale: f32,
};
@vertex fn strip(v: Strip) -> Out {
  var o: Out;
  o.position = frame.viewProjection * vec4f(v.position, 1.0);
  o.uv = v.uv;
  o.colour = v.colour;
  o.alphaScale = v.alphaScale;
  return o;
}
// a snow billboard: PlaneGeometry(2, 2) (uv y flipped) through the instance matrix (position, the camera's rotation, the extent)
struct Billboard {
  @location(0) m0: vec4f,
  @location(1) m1: vec4f,
  @location(2) m2: vec4f,
  @location(3) m3: vec4f,
  @location(4) colour: vec4f,
  @location(5) alphaScale: f32,
};
@vertex fn billboard(@builtin(vertex_index) k: u32, i: Billboard) -> Out {
  let corners = array<vec2f, 4>(vec2f(-1.0, 1.0), vec2f(1.0, 1.0), vec2f(-1.0, -1.0), vec2f(1.0, -1.0));
  let uvs = array<vec2f, 4>(vec2f(0.0, 0.0), vec2f(1.0, 0.0), vec2f(0.0, 1.0), vec2f(1.0, 1.0));
  let order = array<u32, 6>(0u, 2u, 1u, 2u, 3u, 1u);
  let index = order[k];
  let m = mat4x4f(i.m0, i.m1, i.m2, i.m3);
  var o: Out;
  o.position = frame.viewProjection * (m * vec4f(corners[index], 0.0, 1.0));
  o.uv = uvs[index];
  o.colour = i.colour;
  o.alphaScale = i.alphaScale;
  return o;
}
// the encoded colour: texel x vertex colour (GS MODULATE), the PS2 texture alpha scaled to 0..1 times the vertex alpha
@fragment fn shade(i: Out) -> @location(0) vec4f {
  let t = textureSample(mapTex, mapSampler, i.uv);
  return vec4f(clamp(t.rgb * i.colour.rgb, vec3f(0.0), vec3f(1.0)), clamp(t.a * i.alphaScale * i.colour.a, 0.0, 1.0));
}
// the trail's stencil passes (no colour written)
@fragment fn mask() -> @location(0) vec4f {
  return vec4f(0.0);
}
struct Far {
  @location(0) colour: vec4f,
  @builtin(frag_depth) depth: f32,
};
@fragment fn maskFar() -> Far {
  var o: Far;
  o.colour = vec4f(0.0);
  o.depth = 1.0;
  return o;
}
)";

constexpr uint32_t kStripFloats = 10;
constexpr uint32_t kInstanceFloats = 24;
constexpr uint32_t kTrailCapacity = 2000;
constexpr uint32_t kWakeCapacity = 1000;
constexpr uint32_t kBoostCapacity = 180;
// web/boost-renderer.js createRiderFxMeshes: aura faces 9 x 3, streamers 72 x 3 vertices
constexpr uint32_t kAuraCapacity = 27;
constexpr uint32_t kStreamerCapacity = 216;

std::vector<uint8_t> readBytes(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw std::runtime_error("cannot read " + path);
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// parsed from a std::string (nlohmann's byte-vector input needs char_traits<unsigned char>, which newer libc++ drops)
nlohmann::json parseJson(const std::string& path) {
    const auto bytes = readBytes(path);
    return nlohmann::json::parse(std::string(bytes.begin(), bytes.end()));
}

using Mat = std::array<double, 16>;

// three.js Matrix4.lookAt (up +Y) then Quaternion.setFromRotationMatrix: the camera's quaternion after camera.lookAt
std::array<double, 4> lookAtQuaternion(const double eye[3], const double target[3]) {
    double z[3] = {eye[0] - target[0], eye[1] - target[1], eye[2] - target[2]};
    auto length = [](const double* v) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); };
    if (z[0] * z[0] + z[1] * z[1] + z[2] * z[2] == 0) {
        z[2] = 1;
    }
    double l = length(z);
    for (double& c : z) {
        c /= l;
    }
    auto cross = [](const double* a, const double* b, double* o) {
        o[0] = a[1] * b[2] - a[2] * b[1];
        o[1] = a[2] * b[0] - a[0] * b[2];
        o[2] = a[0] * b[1] - a[1] * b[0];
    };
    const double up[3] = {0, 1, 0};
    double x[3];
    cross(up, z, x);
    if (x[0] * x[0] + x[1] * x[1] + x[2] * x[2] == 0) {
        z[2] += 0.0001;
        l = length(z);
        for (double& c : z) {
            c /= l;
        }
        cross(up, z, x);
    }
    l = length(x);
    for (double& c : x) {
        c /= l;
    }
    double y[3];
    cross(z, x, y);
    const double m11 = x[0], m12 = y[0], m13 = z[0];
    const double m21 = x[1], m22 = y[1], m23 = z[1];
    const double m31 = x[2], m32 = y[2], m33 = z[2];
    const double trace = m11 + m22 + m33;
    std::array<double, 4> q{};
    if (trace > 0) {
        const double s = 0.5 / std::sqrt(trace + 1.0);
        q = {(m32 - m23) * s, (m13 - m31) * s, (m21 - m12) * s, 0.25 / s};
    } else if (m11 > m22 && m11 > m33) {
        const double s = 2.0 * std::sqrt(1.0 + m11 - m22 - m33);
        q = {0.25 * s, (m12 + m21) / s, (m13 + m31) / s, (m32 - m23) / s};
    } else if (m22 > m33) {
        const double s = 2.0 * std::sqrt(1.0 + m22 - m11 - m33);
        q = {(m12 + m21) / s, 0.25 * s, (m23 + m32) / s, (m13 - m31) / s};
    } else {
        const double s = 2.0 * std::sqrt(1.0 + m33 - m11 - m22);
        q = {(m13 + m31) / s, (m23 + m32) / s, 0.25 * s, (m21 - m12) / s};
    }
    return q;
}

// three.js Matrix4.compose
Mat compose(const double p[3], const std::array<double, 4>& q, double sx, double sy, double sz) {
    const double x = q[0], y = q[1], z = q[2], w = q[3];
    const double x2 = x + x, y2 = y + y, z2 = z + z;
    const double xx = x * x2, xy = x * y2, xz = x * z2;
    const double yy = y * y2, yz = y * z2, zz = z * z2;
    const double wx = w * x2, wy = w * y2, wz = w * z2;
    return {(1 - (yy + zz)) * sx, (xy + wz) * sx, (xz - wy) * sx, 0, (xy - wz) * sy, (1 - (xx + zz)) * sy, (yz + wx) * sy, 0,
            (xz + wy) * sz, (yz - wx) * sz, (1 - (xx + yy)) * sz, 0, p[0], p[1], p[2], 1};
}

Mat multiply(const double* a, const double* b) {
    Mat r{};
    for (int c = 0; c < 4; ++c) {
        for (int k = 0; k < 4; ++k) {
            double s = 0;
            for (int j = 0; j < 4; ++j) {
                s += a[j * 4 + k] * b[c * 4 + j];
            }
            r[size_t(c * 4 + k)] = s;
        }
    }
    return r;
}

// web/snow-billboard.js snowBillboardScale: the half extent (m), capped in the original framebuffer's pixels
std::array<double, 2> billboardScale(double radius, double depth, double projectionX, double projectionY, double vw, double vh, double limit) {
    if (depth <= 0) {
        return {0, 0};
    }
    const double x = std::abs(projectionX) * vw / 2;
    const double y = std::abs(projectionY) * vh / 2;
    return {std::min(radius, limit * depth / x), std::min(radius, limit * depth / y)};
}

}  // namespace

void RiderFxFrame::read(bool boostRibbons) {
    const float* world = rider_world_state();
    position = {world[0], world[1], world[2]};
    const float* ti = trail_info();
    const size_t ribbonCount = size_t(ti[0]);
    const size_t roofCount = size_t(ti[1]);
    if (ribbonCount > kTrailCapacity || roofCount > kTrailCapacity) {
        throw std::runtime_error("Original trail exceeded source ring capacity");
    }
    const float* r = trail_ribbon();
    ribbon.assign(r, r + ribbonCount * 9);
    const float* f = trail_roof();
    roof.assign(f, f + roofCount * 9);
    const float* wi = wake_info();
    const size_t wakeCount = size_t(wi[11]);
    if (wakeCount > kWakeCapacity) {
        throw std::runtime_error("Wake exceeded original ribbon capacity");
    }
    const float* w = wake_vertices();
    wake.assign(w, w + wakeCount * 9);
    if (boostRibbons) {
        const float* bi = boost_fx_info();
        boostTexture = int(bi[0]);
        for (unsigned s = 0; s < 3; ++s) {
            const size_t count = size_t(bi[6 + s]);
            if (count > kBoostCapacity) {
                throw std::runtime_error("Original boost ribbon capacity exceeded");
            }
            const float* v = count ? boost_fx_vertices(s) : nullptr;
            boost[s].assign(v, v + count * 9);
        }
    }
    const float* fi = rider_fx_info();
    streamerTexture = int(fi[1]);
    for (unsigned s = 0; s < 6; ++s) {
        const size_t count = std::min<size_t>(size_t(fi[2 + s]), s < 4 ? kAuraCapacity : kStreamerCapacity);
        const float* v = count ? rider_fx_vertices(s) : nullptr;
        fx[s].assign(v, v + count * 9);
    }
    const float* flip = snow_flipbook_info();
    const float* si = snow_info();
    for (unsigned i = 0; i < 10; ++i) {
        snowFrame[i] = int(flip[i]);
        const size_t count = size_t(si[i]);
        if (count > size_t(si[10 + i])) {
            throw std::runtime_error("Snow history exceeded authored birth capacity");
        }
        const float* p = count ? snow_particles(i) : nullptr;
        snow[i].assign(p, p + count * 8);
    }
}

RiderFx::Texture RiderFx::loadTexture(const std::string& path, uint32_t w, uint32_t h, float alphaScale, bool repeat, bool flipRows) {
    auto bytes = readBytes(path);
    if (bytes.size() != size_t(w) * h * 4) {
        throw std::runtime_error("Original effect texture extent: " + path);
    }
    if (flipRows) {
        // web/boost-renderer.js: the GS samples strm with its bright rows at T = 1, the export has them at T = 0
        std::vector<uint8_t> flipped(bytes.size());
        const size_t row = size_t(w) * 4;
        for (uint32_t y = 0; y < h; ++y) {
            std::memcpy(flipped.data() + (h - 1 - y) * row, bytes.data() + y * row, row);
        }
        bytes.swap(flipped);
    }
    Texture t;
    t.alphaScale = alphaScale;
    t.texture = gpu_->texture(w, h, WGPUTextureFormat_RGBA8Unorm, WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst, 1, "effect texture");
    gpu_->writeTexture(t.texture, w, h, bytes.data());
    WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    WGPUBindGroupEntry e[2];
    e[0] = WGPU_BIND_GROUP_ENTRY_INIT;
    e[0].binding = 0;
    e[0].textureView = wgpuTextureCreateView(t.texture, &vd);
    e[1] = WGPU_BIND_GROUP_ENTRY_INIT;
    e[1].binding = 1;
    e[1].sampler = repeat ? repeat_ : clamp_;
    WGPUBindGroupDescriptor bd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bd.layout = materialLayout_;
    bd.entryCount = 2;
    bd.entries = e;
    t.group = wgpuDeviceCreateBindGroup(gpu_->device, &bd);
    return t;
}

void RiderFx::load(Gpu& gpu, const std::string& assetsRoot) {
    gpu_ = &gpu;
    // DataTexture: LinearFilter, no mipmaps; RepeatWrapping or ClampToEdgeWrapping
    for (int k = 0; k < 2; ++k) {
        WGPUSamplerDescriptor sd = WGPU_SAMPLER_DESCRIPTOR_INIT;
        sd.magFilter = WGPUFilterMode_Linear;
        sd.minFilter = WGPUFilterMode_Linear;
        sd.addressModeU = k == 0 ? WGPUAddressMode_Repeat : WGPUAddressMode_ClampToEdge;
        sd.addressModeV = sd.addressModeU;
        (k == 0 ? repeat_ : clamp_) = wgpuDeviceCreateSampler(gpu.device, &sd);
    }
    buildPipelines();
    // web/board-trail.js
    const auto trail = parseJson(assetsRoot + "/BOARD_TRAIL/board_trail.json");
    const auto& tt = trail["texture"];
    trailTexture_ = loadTexture(assetsRoot + "/BOARD_TRAIL/" + tt["file"].get<std::string>(), tt["width"].get<uint32_t>(), tt["height"].get<uint32_t>(), 1, true,
                                false);
    trailAlpha_ = tt["normalized_sample_alpha_multiplier"].get<float>();
    // web/snow-renderer.js / wake-renderer.js / boost-renderer.js: the SNOW_FX textures (the GS-alpha file and its scale when present)
    const auto snow = parseJson(assetsRoot + "/SNOW_FX/snow-fx.json");
    const auto& view = snow["view"];
    if (view["source_viewport"].size() != 2 || view["max_projected_half_extent"] != 128) {
        throw std::runtime_error("Missing original snow view constraints");
    }
    for (const auto& t : snow["textures"]) {
        const bool gs = t.contains("gs_alpha_file") && t["gs_alpha_file"].is_string();
        const std::string file = gs ? t["gs_alpha_file"].get<std::string>() : t["file"].get<std::string>();
        const float scale = gs ? t["gs_alpha_scale"].get<float>() : 1.f;
        snowTextures_[t["id"].get<int>()] =
            loadTexture(assetsRoot + "/SNOW_FX/" + file, t["width"].get<uint32_t>(), t["height"].get<uint32_t>(), scale, true, false);
    }
    for (int id : {56, 57, 58, 59, 60, 61}) {
        if (!snowTextures_.count(id)) {
            throw std::runtime_error("Original wake / boost texture missing");
        }
    }
    for (int i = 0; i < 10; ++i) {
        bool found = false;
        for (const auto& p : snow["profiles"]) {
            if (p["emitter_index"] == i) {
                emitterTexture_[size_t(i)] = p["parameters"]["TextureId"].get<int>();
                found = true;
            }
        }
        for (const auto& b : snow["flipbooks"]) {
            if (b["emitter_index"] == i) {
                flipbooks_[size_t(i)] = b["texture_ids"].get<std::vector<int>>();
            }
        }
        if (!found || !snowTextures_.count(emitterTexture_[size_t(i)]) || flipbooks_[size_t(i)].empty()) {
            throw std::runtime_error("Original snow emitter binding");
        }
        for (int id : flipbooks_[size_t(i)]) {
            if (!snowTextures_.count(id) || snowTextures_[id].alphaScale != snowTextures_[emitterTexture_[size_t(i)]].alphaScale) {
                throw std::runtime_error("Invalid original snow flipbook binding");
            }
        }
    }
    // web/boost-renderer.js createRiderFxMaterials: the raw GS FX textures (alpha 128 = 1.0)
    const auto fx = parseJson(assetsRoot + "/FX/fx.json");
    auto fxTexture = [&](const char* tag, bool repeat, bool flip) {
        const auto& t = fx["textures"][tag];
        return loadTexture(assetsRoot + "/FX/" + t["file"].get<std::string>(), t["width"].get<uint32_t>(), t["height"].get<uint32_t>(), 255.f / 128.f, repeat,
                           flip);
    };
    aura_ = fxTexture("psmr", false, false);
    streamers_[63] = fxTexture("strm", false, true);
    streamers_[61] = fxTexture("prbn", true, false);
}

void RiderFx::buildPipelines() {
    WGPUDevice d = gpu_->device;
    WGPUBindGroupLayoutEntry fe = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    fe.binding = 0;
    fe.visibility = WGPUShaderStage_Vertex;
    fe.buffer.type = WGPUBufferBindingType_Uniform;
    WGPUBindGroupLayoutDescriptor fd = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    fd.entryCount = 1;
    fd.entries = &fe;
    frameLayout_ = wgpuDeviceCreateBindGroupLayout(d, &fd);
    WGPUBindGroupLayoutEntry me[2];
    me[0] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    me[0].binding = 0;
    me[0].visibility = WGPUShaderStage_Fragment;
    me[0].texture.sampleType = WGPUTextureSampleType_Float;
    me[0].texture.viewDimension = WGPUTextureViewDimension_2D;
    me[1] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    me[1].binding = 1;
    me[1].visibility = WGPUShaderStage_Fragment;
    me[1].sampler.type = WGPUSamplerBindingType_Filtering;
    WGPUBindGroupLayoutDescriptor md = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    md.entryCount = 2;
    md.entries = me;
    materialLayout_ = wgpuDeviceCreateBindGroupLayout(d, &md);
    const WGPUBindGroupLayout layouts[2] = {frameLayout_, materialLayout_};
    WGPUPipelineLayoutDescriptor pl = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pl.bindGroupLayoutCount = 2;
    pl.bindGroupLayouts = layouts;
    WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(d, &pl);
    uniform_ = gpu_->buffer(64, WGPUBufferUsage_Uniform, nullptr, "effects frame");
    WGPUBindGroupEntry ue = WGPU_BIND_GROUP_ENTRY_INIT;
    ue.binding = 0;
    ue.buffer = uniform_;
    ue.size = 64;
    WGPUBindGroupDescriptor ud = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    ud.layout = frameLayout_;
    ud.entryCount = 1;
    ud.entries = &ue;
    frame_ = wgpuDeviceCreateBindGroup(d, &ud);
    WGPUShaderModule module = gpu_->shader(kFxShader, "rider effects");

    WGPUVertexAttribute strip[4] = {
        {nullptr, WGPUVertexFormat_Float32x3, 0, 0},
        {nullptr, WGPUVertexFormat_Float32x2, 12, 1},
        {nullptr, WGPUVertexFormat_Float32x4, 20, 2},
        {nullptr, WGPUVertexFormat_Float32, 36, 3},
    };
    WGPUVertexBufferLayout stripLayout = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
    stripLayout.arrayStride = kStripFloats * 4;
    stripLayout.stepMode = WGPUVertexStepMode_Vertex;
    stripLayout.attributeCount = 4;
    stripLayout.attributes = strip;
    WGPUVertexAttribute billboard[6] = {
        {nullptr, WGPUVertexFormat_Float32x4, 0, 0},
        {nullptr, WGPUVertexFormat_Float32x4, 16, 1},
        {nullptr, WGPUVertexFormat_Float32x4, 32, 2},
        {nullptr, WGPUVertexFormat_Float32x4, 48, 3},
        {nullptr, WGPUVertexFormat_Float32x4, 64, 4},
        {nullptr, WGPUVertexFormat_Float32, 80, 5},
    };
    WGPUVertexBufferLayout billboardLayout = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
    billboardLayout.arrayStride = kInstanceFloats * 4;
    billboardLayout.stepMode = WGPUVertexStepMode_Instance;
    billboardLayout.attributeCount = 6;
    billboardLayout.attributes = billboard;

    // three.js (premultipliedAlpha off) NormalBlending: SrcAlpha / OneMinusSrcAlpha, alpha One / OneMinusSrcAlpha;
    // AdditiveBlending: SrcAlpha / One, alpha One / One
    WGPUBlendState normal{};
    normal.color = {WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha, WGPUBlendFactor_OneMinusSrcAlpha};
    normal.alpha = {WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_OneMinusSrcAlpha};
    WGPUBlendState additive{};
    additive.color = {WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha, WGPUBlendFactor_One};
    additive.alpha = {WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_One};
    struct Make {
        bool billboard = false;
        const char* fragment = "shade";
        const WGPUBlendState* blend = nullptr;
        bool colorWrite = true;
        bool depthWrite = false;
        WGPUCompareFunction depthCompare = WGPUCompareFunction_LessEqual;
        bool stencil = false;
        WGPUCompareFunction stencilCompare = WGPUCompareFunction_Always;
        WGPUStencilOperation passOp = WGPUStencilOperation_Keep;
    };
    auto make = [&](const Make& m) {
        WGPURenderPipelineDescriptor pd = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        pd.layout = layout;
        pd.vertex.module = module;
        pd.vertex.entryPoint = sv(m.billboard ? "billboard" : "strip");
        pd.vertex.bufferCount = 1;
        pd.vertex.buffers = m.billboard ? &billboardLayout : &stripLayout;
        pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        // DoubleSide, forceSinglePass
        pd.primitive.cullMode = WGPUCullMode_None;
        WGPUColorTargetState ct = WGPU_COLOR_TARGET_STATE_INIT;
        ct.format = WGPUTextureFormat_RGBA8Unorm;
        ct.blend = m.blend;
        ct.writeMask = m.colorWrite ? WGPUColorWriteMask_All : WGPUColorWriteMask_None;
        WGPUFragmentState fs = WGPU_FRAGMENT_STATE_INIT;
        fs.module = module;
        fs.entryPoint = sv(m.fragment);
        fs.targetCount = 1;
        fs.targets = &ct;
        pd.fragment = &fs;
        WGPUDepthStencilState ds = WGPU_DEPTH_STENCIL_STATE_INIT;
        ds.format = WGPUTextureFormat_Depth24PlusStencil8;
        ds.depthWriteEnabled = m.depthWrite ? WGPUOptionalBool_True : WGPUOptionalBool_False;
        ds.depthCompare = m.depthCompare;
        ds.stencilReadMask = 0xff;
        ds.stencilWriteMask = 0;
        if (m.stencil) {
            WGPUStencilFaceState face = WGPU_STENCIL_FACE_STATE_INIT;
            face.compare = m.stencilCompare;
            face.failOp = WGPUStencilOperation_Keep;
            face.depthFailOp = WGPUStencilOperation_Keep;
            face.passOp = m.passOp;
            ds.stencilFront = face;
            ds.stencilBack = face;
            ds.stencilWriteMask = 0xff;
        }
        pd.depthStencil = &ds;
        pd.multisample.count = 1;
        pd.multisample.mask = 0xffffffffu;
        return wgpuDeviceCreateRenderPipeline(d, &pd);
    };
    // web/board-trail.js passes 0 .. 4 (renderOrder 500 + pass): the roof clears the stencil, marks its visible pixels, pushes them to
    // the far plane; the ribbon draws and marks; the roof draws where unmarked
    const WGPUStencilOperation passOps[5] = {WGPUStencilOperation_Zero, WGPUStencilOperation_Replace, WGPUStencilOperation_Zero,
                                              WGPUStencilOperation_Replace, WGPUStencilOperation_Keep};
    for (int pass = 0; pass < 5; ++pass) {
        Make m;
        m.fragment = pass >= 3 ? "shade" : (pass == 2 ? "maskFar" : "mask");
        m.blend = &normal;
        m.colorWrite = pass >= 3;
        m.depthWrite = pass >= 2;
        m.depthCompare = pass == 0 || pass == 2 ? WGPUCompareFunction_Always : WGPUCompareFunction_LessEqual;
        m.stencil = true;
        m.stencilCompare = pass == 2 || pass == 4 ? WGPUCompareFunction_Equal : WGPUCompareFunction_Always;
        m.passOp = passOps[pass];
        trail_[pass] = make(m);
    }
    // the encoded pass: transparent, depth tested, not written
    Make m;
    m.blend = &normal;
    strip_ = make(m);
    m.blend = &additive;
    stripAdd_ = make(m);
    m.blend = &normal;
    m.billboard = true;
    snow_ = make(m);
}

void RiderFx::prepare(const std::vector<const RiderFxFrame*>& riders, const double eye[3], const double look[3], const double* view,
                      const double* projection, const std::array<double, 3>& origin) {
    const Mat vp = multiply(projection, view);
    float vpf[16];
    for (int k = 0; k < 16; ++k) {
        vpf[k] = float(vp[size_t(k)]);
    }
    wgpuQueueWriteBuffer(gpu_->queue, uniform_, 0, vpf, sizeof vpf);
    // three.js Frustum.setFromProjectionMatrix(projection x matrixWorldInverse) (main.js cullView: the default WebGL coordinate
    // system's near plane), intersectsSphere
    std::array<std::array<double, 4>, 6> planes{};
    {
        const double* m = vp.data();
        const double rows[4][4] = {{m[0], m[4], m[8], m[12]}, {m[1], m[5], m[9], m[13]}, {m[2], m[6], m[10], m[14]}, {m[3], m[7], m[11], m[15]}};
        const int pick[6][2] = {{0, -1}, {0, 1}, {1, 1}, {1, -1}, {2, -1}, {2, 1}};
        for (int p = 0; p < 6; ++p) {
            const int r = pick[p][0];
            const bool minus = pick[p][1] < 0;
            std::array<double, 4> plane{};
            for (int k = 0; k < 4; ++k) {
                plane[size_t(k)] = rows[3][k] + (minus ? -rows[r][k] : rows[r][k]);
            }
            const double l = std::sqrt(plane[0] * plane[0] + plane[1] * plane[1] + plane[2] * plane[2]);
            for (double& c : plane) {
                c /= l;
            }
            planes[size_t(p)] = plane;
        }
    }
    auto away = [&](const RiderFxFrame& f) {
        const double c[3] = {f.position[0] * 0.01 - origin[0], f.position[2] * 0.01 - origin[1], -f.position[1] * 0.01 - origin[2]};
        for (const auto& p : planes) {
            if (p[0] * c[0] + p[1] * c[1] + p[2] * c[2] + p[3] < -100) {
                return true;
            }
        }
        return false;
    };
    // the camera's rotation (snow billboards) and three's matrixWorldInverse (their depth)
    const auto q = lookAtQuaternion(eye, look);
    const Mat world = compose(eye, q, 1, 1, 1);
    Mat inverse{};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            inverse[size_t(c * 4 + r)] = world[size_t(r * 4 + c)];
        }
    }
    for (int r = 0; r < 3; ++r) {
        inverse[size_t(12 + r)] = -(world[size_t(r * 4)] * eye[0] + world[size_t(r * 4 + 1)] * eye[1] + world[size_t(r * 4 + 2)] * eye[2]);
    }
    inverse[15] = 1;

    std::vector<float> strips;
    std::vector<float> instances;
    if (riders_.size() < riders.size()) {
        riders_.resize(riders.size());
    }
    // the core's cm, Z up -> metres, Y up, origin-relative (in double, stored as float like the page's Float32Array); positions: the
    // mesh's position attribute
    auto addStrip = [&](const std::vector<float>& src, float alphaScale, std::vector<float>* positions) {
        Range range;
        range.first = strips.size() / kStripFloats;
        range.count = uint32_t(src.size() / 9);
        for (uint32_t i = 0; i < range.count; ++i) {
            const float* o = src.data() + size_t(i) * 9;
            const float p[3] = {float(double(o[0]) / 100 - origin[0]), float(double(o[2]) / 100 - origin[1]), float(-double(o[1]) / 100 - origin[2])};
            strips.insert(strips.end(), {p[0], p[1], p[2], o[3], o[4], o[5], o[6], o[7], o[8], alphaScale});
            if (positions) {
                std::memcpy(positions->data() + size_t(i) * 3, p, 12);
            }
        }
        return range;
    };
    auto fill = [&](Strip& s, const std::vector<float>& src, uint32_t capacity, float alphaScale) {
        if (s.positions.empty()) {
            s.positions.assign(size_t(capacity) * 3, 0.f);
        }
        s.range = addStrip(src, alphaScale, &s.positions);
    };
    bool populated = false;
    for (size_t r = 0; r < riders.size(); ++r) {
        const RiderFxFrame& f = *riders[r];
        RiderGpu& g = riders_[r];
        g.drawn = f.visible && !(f.cullable && away(f));
        if (!g.drawn) {
            continue;
        }
        g.ribbon = addStrip(f.ribbon, trailAlpha_, nullptr);
        g.roof = addStrip(f.roof, trailAlpha_, nullptr);
        fill(g.wake, f.wake, kWakeCapacity, snowTextures_[56].alphaScale);
        populated = populated || g.wake.range.count > 0;
        if (!f.cullable) {
            for (size_t s = 0; s < 3; ++s) {
                if (!snowTextures_.count(f.boostTexture)) {
                    throw std::runtime_error("Unmapped original boost palette");
                }
                fill(g.boost[s], f.boost[s], kBoostCapacity, snowTextures_[f.boostTexture].alphaScale);
                populated = populated || g.boost[s].range.count > 0;
            }
        }
        for (size_t s = 0; s < 6; ++s) {
            fill(g.fx[s], f.fx[s], s < 4 ? kAuraCapacity : kStreamerCapacity, 255.f / 128.f);
            populated = populated || g.fx[s].range.count > 0;
        }
        for (int i = 0; i < 10; ++i) {
            const auto& particles = f.snow[size_t(i)];
            const int frame = f.snowFrame[size_t(i)];
            const auto& book = flipbooks_[size_t(i)];
            if (std::find(book.begin(), book.end(), frame) == book.end()) {
                throw std::runtime_error("Snow texture frame outside authored sequence");
            }
            const float alphaScale = snowTextures_[emitterTexture_[size_t(i)]].alphaScale;
            Range range;
            range.first = instances.size() / kInstanceFloats;
            range.count = uint32_t(particles.size() / 8);
            for (uint32_t j = 0; j < range.count; ++j) {
                const float* n = particles.data() + size_t(j) * 8;
                const double p[3] = {double(n[0]) / 100 - origin[0], double(n[2]) / 100 - origin[1], -double(n[1]) / 100 - origin[2]};
                const double viewZ = inverse[2] * p[0] + inverse[6] * p[1] + inverse[10] * p[2] + inverse[14];
                const auto extent = billboardScale(double(n[3]) / 100, -viewZ, projection[0], projection[5], 512, 448, 128);
                const Mat m = compose(p, q, extent[0], extent[1], 1);
                for (int k = 0; k < 16; ++k) {
                    instances.push_back(float(m[size_t(k)]));
                }
                instances.insert(instances.end(), {n[4], n[5], n[6], n[7], alphaScale, 0, 0, 0});
            }
            g.snow[size_t(i)] = range;
            populated = populated || range.count > 0;
        }
    }
    auto upload = [&](WGPUBuffer& buffer, uint64_t& capacity, const std::vector<float>& data, const char* label) {
        const uint64_t bytes = std::max<uint64_t>(data.size() * 4, 64);
        if (bytes > capacity) {
            capacity = std::max<uint64_t>(bytes, capacity * 2);
            buffer = gpu_->buffer(capacity, WGPUBufferUsage_Vertex, nullptr, label);
        }
        if (!data.empty()) {
            wgpuQueueWriteBuffer(gpu_->queue, buffer, 0, data.data(), data.size() * 4);
        }
    };
    upload(vertices_, vertexCapacity_, strips, "effect strips");
    upload(instances_, instanceCapacity_, instances, "snow instances");

    // the encoded pass's draws in three's transparent order: renderOrder, then the clip depth of the geometry's bounding sphere centre
    // (far first), then creation order (the human's snow, wake, boost ribbons, aura and streamers; the computer riders' wakes, snow,
    // aura and streamers)
    encoded_.clear();
    auto clipZ = [&](const std::array<double, 3>& p) { return vp[2] * p[0] + vp[6] * p[1] + vp[10] * p[2] + vp[14]; };
    const double originZ = clipZ({0, 0, 0});
    auto addStripDraw = [&](Strip& s, double order, int id, bool additive, const Texture* texture) {
        // three computes a geometry's bounding sphere on its first projection (an encoded render with something to draw) and keeps it
        if (populated && !s.centre) {
            std::array<double, 3> lo{1e300, 1e300, 1e300};
            std::array<double, 3> hi{-1e300, -1e300, -1e300};
            for (size_t i = 0; i < s.positions.size(); i += 3) {
                for (size_t k = 0; k < 3; ++k) {
                    lo[k] = std::min(lo[k], double(s.positions[i + k]));
                    hi[k] = std::max(hi[k], double(s.positions[i + k]));
                }
            }
            s.centre = std::array<double, 3>{(lo[0] + hi[0]) * 0.5, (lo[1] + hi[1]) * 0.5, (lo[2] + hi[2]) * 0.5};
        }
        if (!s.range.count) {
            return;
        }
        EncodedDraw e;
        e.order = order;
        e.z = s.centre ? clipZ(*s.centre) : originZ;
        e.id = id;
        e.additive = additive;
        e.range = s.range;
        e.texture = texture;
        encoded_.push_back(e);
    };
    namespace o = draw_order;
    for (size_t r = 0; r < riders.size(); ++r) {
        RiderGpu& g = riders_[r];
        if (!g.drawn) {
            continue;
        }
        const RiderFxFrame& f = *riders[r];
        const bool human = !f.cullable;
        const int k = int(r);
        addStripDraw(g.wake, o::drawOrder(o::wake(), o::kSubmitWake), human ? 110 : 200 + k, false, &snowTextures_[56]);
        if (human) {
            for (int s = 0; s < 3; ++s) {
                addStripDraw(g.boost[size_t(s)], o::drawOrder(o::boost(f.boostTexture), o::kSubmitBoost), 120 + s, true, &snowTextures_[f.boostTexture]);
            }
        }
        for (int s = 0; s < 6; ++s) {
            const int id = human ? 130 + s : 400 + 10 * k + s;
            if (s < 4) {
                addStripDraw(g.fx[size_t(s)], o::drawOrder(o::aura(), o::kSubmitAura), id, true, &aura_);
            } else {
                // the streamer texture (an unmapped one falls back to strm)
                const int texture = streamers_.count(f.streamerTexture) ? f.streamerTexture : 63;
                addStripDraw(g.fx[size_t(s)], o::drawOrder(o::streamer(texture), o::kSubmitStreamer), id, true, &streamers_[texture]);
            }
        }
        for (int i = 0; i < 10; ++i) {
            if (!g.snow[size_t(i)].count) {
                continue;
            }
            EncodedDraw e;
            const int texture = f.snowFrame[size_t(i)];
            e.order = o::drawOrder(o::snow(texture), o::kSubmitSnow);
            e.z = originZ;
            e.id = human ? 100 + i : 300 + 10 * k + i;
            e.billboard = true;
            e.range = g.snow[size_t(i)];
            e.texture = &snowTextures_[texture];
            encoded_.push_back(e);
        }
    }
    std::stable_sort(encoded_.begin(), encoded_.end(), [](const EncodedDraw& a, const EncodedDraw& b) {
        if (a.order != b.order) {
            return a.order < b.order;
        }
        if (a.z != b.z) {
            return a.z > b.z;
        }
        return a.id < b.id;
    });
}

void RiderFx::drawTrails(WGPURenderPassEncoder pass) {
    const uint32_t references[5] = {0, 1, 1, 1, 0};
    bool any = false;
    for (const auto& g : riders_) {
        any = any || (g.drawn && (g.ribbon.count || g.roof.count));
    }
    if (!any) {
        return;
    }
    wgpuRenderPassEncoderSetBindGroup(pass, 0, frame_, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(pass, 1, trailTexture_.group, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertices_, 0, WGPU_WHOLE_SIZE);
    // renderOrder 500 + pass, then each rider's material (creation order: the human, then the computer riders)
    for (int p = 0; p < 5; ++p) {
        wgpuRenderPassEncoderSetPipeline(pass, trail_[p]);
        wgpuRenderPassEncoderSetStencilReference(pass, references[p]);
        for (const auto& g : riders_) {
            const Range& range = p == 3 ? g.ribbon : g.roof;
            if (!g.drawn || !range.count) {
                continue;
            }
            wgpuRenderPassEncoderDraw(pass, range.count, 1, uint32_t(range.first), 0);
        }
    }
    wgpuRenderPassEncoderSetStencilReference(pass, 0);
}

void RiderFx::drawEncoded(WGPURenderPassEncoder pass) {
    if (encoded_.empty()) {
        return;
    }
    wgpuRenderPassEncoderSetBindGroup(pass, 0, frame_, 0, nullptr);
    for (const auto& e : encoded_) {
        wgpuRenderPassEncoderSetBindGroup(pass, 1, e.texture->group, 0, nullptr);
        if (e.billboard) {
            wgpuRenderPassEncoderSetPipeline(pass, snow_);
            wgpuRenderPassEncoderSetVertexBuffer(pass, 0, instances_, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderDraw(pass, 6, e.range.count, 0, uint32_t(e.range.first));
        } else {
            wgpuRenderPassEncoderSetPipeline(pass, e.additive ? stripAdd_ : strip_);
            wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertices_, 0, WGPU_WHOLE_SIZE);
            wgpuRenderPassEncoderDraw(pass, e.range.count, 1, uint32_t(e.range.first), 0);
        }
    }
}

}  // namespace ssx_native
