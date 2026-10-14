#include "world_renderer.hpp"
#include "image_io.hpp"
#include "texture_archive.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>

namespace ssx_native {

using nlohmann::json;

namespace {

std::vector<uint8_t> readBytes(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw std::runtime_error("cannot read " + path);
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
json readJson(const std::string& path) {
    const auto b = readBytes(path);
    return json::parse(std::string(reinterpret_cast<const char*>(b.data()), b.size()));
}
template <typename T>
std::vector<T> readArray(const std::string& path) {
    const auto b = readBytes(path);
    std::vector<T> out(b.size() / sizeof(T));
    std::memcpy(out.data(), b.data(), out.size() * sizeof(T));
    return out;
}

// The shaders: each web/world-material.js combine on GS bytes, written as its TSL graph computes it.
const char* kWorldShader = R"(
struct Frame { mvp: mat4x4f };
@group(0) @binding(0) var<uniform> frame: Frame;
@group(1) @binding(0) var mapTex: texture_2d<f32>;
@group(1) @binding(1) var mapSampler: sampler;
@group(1) @binding(2) var atlasTex: texture_2d<f32>;
@group(1) @binding(3) var atlasSampler: sampler;

struct In {
  @location(0) position: vec3f,
  @location(1) normal: vec3f,
  @location(2) uv: vec2f,
  @location(3) uv1: vec2f,
  @location(4) color: vec4f,
  @location(5) lightUv: vec2f,
  @location(6) vertexAlpha: f32,
};
struct Out {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
  @location(1) color: vec4f,
  @location(2) lightUv: vec2f,
  @location(3) vertexAlpha: f32,
};
@vertex fn vs(v: In) -> Out {
  var o: Out;
  o.position = frame.mvp * vec4f(v.position, 1.0);
  o.uv = v.uv;
  o.color = v.color;
  o.lightUv = v.lightUv;
  o.vertexAlpha = v.vertexAlpha;
  return o;
}
fn frameBytes(bytes: vec3f) -> vec3f { return clamp(bytes, vec3f(0.0), vec3f(255.0)) / 255.0; }
// terrain: ctx1 base texture, ctx2 PS2 light page ALPHA 0x81 (Cd - Cl) * Al >> 7
@fragment fn terrain(i: Out) -> @location(0) vec4f {
  let base = round(textureSample(mapTex, mapSampler, i.uv) * 255.0);
  let light = round(textureSample(atlasTex, atlasSampler, i.lightUv) * 255.0);
  let lit = clamp(floor((base.rgb - light.rgb) * light.a / 128.0), vec3f(0.0), vec3f(255.0));
  return vec4f(frameBytes(lit), 1.0);
}
// static models: TFX MODULATE with the c5 << 3 vertex colour
fn modelBytes(i: Out) -> vec4f {
  let base = round(textureSample(mapTex, mapSampler, i.uv) * 255.0);
  let alpha = base.a / 255.0 * i.vertexAlpha;
  let bytes = floor(base.rgb * (round(i.color.rgb * 31.0) * 8.0) / 128.0);
  return vec4f(frameBytes(bytes), alpha);
}
@fragment fn instanceOpaque(i: Out) -> @location(0) vec4f {
  return vec4f(modelBytes(i).rgb, 1.0);
}
// blend classes 1 / 2: the pass (alpha test GREATER ref, depth written) and the fringe (alpha <= ref, no depth)
override reference: f32 = 0.71875;
@fragment fn instancePass(i: Out) -> @location(0) vec4f {
  let c = modelBytes(i);
  if (c.a <= reference) { discard; }
  return c;
}
@fragment fn instanceFringe(i: Out) -> @location(0) vec4f {
  let c = modelBytes(i);
  if (!(c.a <= reference)) { discard; }
  return c;
}
@fragment fn instanceAdditive(i: Out) -> @location(0) vec4f {
  return modelBytes(i);
}
// the sky: the model combine without vertex alpha
@fragment fn skyOpaque(i: Out) -> @location(0) vec4f {
  let base = round(textureSample(mapTex, mapSampler, i.uv) * 255.0);
  let bytes = floor(base.rgb * (round(i.color.rgb * 31.0) * 8.0) / 128.0);
  return vec4f(frameBytes(bytes), 1.0);
}
@fragment fn skyBlend(i: Out) -> @location(0) vec4f {
  let base = round(textureSample(mapTex, mapSampler, i.uv) * 255.0);
  let bytes = floor(base.rgb * (round(i.color.rgb * 31.0) * 8.0) / 128.0);
  return vec4f(frameBytes(bytes), base.a / 255.0);
}
)";

// web/fog-renderer.js: the world pass (its background the sky pass) fogged by the CLUT on the world depth
const char* kCompositeShader = R"(
struct Params {
  near: f32, far: f32, slope: f32, offset: f32,
  enabled: f32, tintEnabled: f32, flashOn: f32, flashA: f32,
  tintScale: vec4f,
  tintAdd: vec4f,
  flashRGB: vec4f,
};
@group(0) @binding(0) var<uniform> params: Params;
@group(0) @binding(1) var worldTex: texture_2d<f32>;
@group(0) @binding(2) var depthTex: texture_depth_2d;
@group(0) @binding(3) var paletteTex: texture_2d<f32>;
@group(0) @binding(4) var skyTex: texture_2d<f32>;
@vertex fn vs(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
  let p = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  return vec4f(p[i], 0.0, 1.0);
}
@fragment fn fog(@builtin(position) p: vec4f) -> @location(0) vec4f {
  let xy = vec2i(p.xy);
  let colour = textureLoad(worldTex, xy, 0);
  let depth = textureLoad(depthTex, xy, 0);
  // three.js perspectiveDepthToViewZ (WebGPU depth 0..1, not reversed)
  let viewZ = params.near * params.far / ((params.far - params.near) * depth - params.far);
  let depthCm = max(-viewZ * 100.0, 0.0001);
  let originalZ = max(floor((depthCm * params.slope + params.offset) * (1.0 / depthCm)), 0.0);
  let index = i32(floor(originalZ / 256.0) % 256.0);
  let entry = textureLoad(paletteTex, vec2i(index, 0), 0);
  let source = round(clamp(colour.rgb, vec3f(0.0), vec3f(1.0)) * 255.0);
  let fogBytes = round(entry.rgb * 255.0);
  let alpha = round(entry.a * 255.0);
  let blended = clamp(floor((source - fogBytes) * alpha / 128.0) + fogBytes, vec3f(0.0), vec3f(255.0));
  var out = source;
  if (originalZ < 65535.0) { out = mix(source, blended, params.enabled); }
  // web/screen-tint.js apply: C * s >> 7 (then + a), clamped; the lightning flash (ALPHA 0x44) over it
  let scaled = clamp(floor(out * params.tintScale.rgb / 128.0), vec3f(0.0), vec3f(255.0));
  var tinted = out;
  if (params.tintEnabled > 0.0) {
    tinted = scaled;
    if (params.tintEnabled > 1.0) { tinted = clamp(scaled + params.tintAdd.rgb, vec3f(0.0), vec3f(255.0)); }
  }
  if (params.flashOn > 0.0) {
    tinted = clamp(floor((params.flashRGB.rgb - tinted) * params.flashA / 128.0) + tinted, vec3f(0.0), vec3f(255.0));
  }
  return vec4f(tinted / 255.0, 1.0);
}
)";

// the world pass's background: the sky pass's texture (scene.backgroundNode = the sky pass sample)
const char* kCopyShader = R"(
@group(0) @binding(0) var skyTex: texture_2d<f32>;
@vertex fn vs(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
  let p = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  return vec4f(p[i], 1.0, 1.0);
}
@fragment fn copySky(@builtin(position) p: vec4f) -> @location(0) vec4f {
  return textureLoad(skyTex, vec2i(p.xy), 0);
}
)";

// three.js PerspectiveCamera (WebGPU clip depth 0..1) and Matrix4.lookAt, in double
using Mat4 = std::array<double, 16>;
Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int c = 0; c < 4; ++c) {
        for (int rr = 0; rr < 4; ++rr) {
            double s = 0;
            for (int k = 0; k < 4; ++k) {
                s += a[size_t(k * 4 + rr)] * b[size_t(c * 4 + k)];
            }
            r[size_t(c * 4 + rr)] = s;
        }
    }
    return r;
}
Mat4 perspective(double fovYDeg, double aspect, double near, double far) {
    const double top = near * std::tan(fovYDeg * M_PI / 180.0 * 0.5);
    const double height = 2 * top;
    const double width = aspect * height;
    const double left = -0.5 * width;
    const double right = left + width;
    const double bottom = top - height;
    Mat4 m{};
    m[0] = 2 * near / (right - left);
    m[5] = 2 * near / (top - bottom);
    m[8] = (right + left) / (right - left);
    m[9] = (top + bottom) / (top - bottom);
    m[10] = -far / (far - near);
    m[11] = -1;
    m[14] = -far * near / (far - near);
    return m;
}
Mat4 view(const double eye[3], const double target[3]) {
    double z[3] = {eye[0] - target[0], eye[1] - target[1], eye[2] - target[2]};
    double zl = std::sqrt(z[0] * z[0] + z[1] * z[1] + z[2] * z[2]);
    if (zl == 0) {
        z[2] = 1;
        zl = 1;
    }
    for (double& c : z) {
        c /= zl;
    }
    const double up[3] = {0, 1, 0};
    double x[3] = {up[1] * z[2] - up[2] * z[1], up[2] * z[0] - up[0] * z[2], up[0] * z[1] - up[1] * z[0]};
    double xl = std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
    if (xl == 0) {
        z[0] += 0.0001;
        x[0] = up[1] * z[2] - up[2] * z[1];
        x[1] = up[2] * z[0] - up[0] * z[2];
        x[2] = up[0] * z[1] - up[1] * z[0];
        xl = std::sqrt(x[0] * x[0] + x[1] * x[1] + x[2] * x[2]);
    }
    for (double& c : x) {
        c /= xl;
    }
    const double y[3] = {z[1] * x[2] - z[2] * x[1], z[2] * x[0] - z[0] * x[2], z[0] * x[1] - z[1] * x[0]};
    // the inverse of the camera's world matrix [x y z eye]
    Mat4 m{};
    m[0] = x[0];
    m[4] = x[1];
    m[8] = x[2];
    m[1] = y[0];
    m[5] = y[1];
    m[9] = y[2];
    m[2] = z[0];
    m[6] = z[1];
    m[10] = z[2];
    m[12] = -(x[0] * eye[0] + x[1] * eye[1] + x[2] * eye[2]);
    m[13] = -(y[0] * eye[0] + y[1] * eye[1] + y[2] * eye[2]);
    m[14] = -(z[0] * eye[0] + z[1] * eye[1] + z[2] * eye[2]);
    m[15] = 1;
    return m;
}
Mat4 translate(double x, double y, double z) {
    Mat4 m{};
    m[0] = m[5] = m[10] = m[15] = 1;
    m[12] = x;
    m[13] = y;
    m[14] = z;
    return m;
}

// web/world-material.js originalModelBlend (sky material flags)
int skyBlendClass(int flags) {
    switch ((flags << 16) & 0x60000) {
        case 0:
            return 0;
        case 0x20000:
            return 1;
        default:
            return 2;
    }
}

}  // namespace

WorldRenderer::Texture WorldRenderer::upload(uint32_t w, uint32_t h, const std::vector<uint8_t>& rgba, bool mips) {
    uint32_t levels = 1;
    if (mips) {
        uint32_t m = std::max(w, h);
        while (m > 1) {
            m >>= 1;
            ++levels;
        }
    }
    Texture t;
    t.texture = gpu_->texture(w, h, WGPUTextureFormat_RGBA8Unorm, WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst, levels);
    gpu_->writeTexture(t.texture, w, h, rgba.data(), 0);
    // the mip chain as a 2 x 2 box filter (three.js generates mipmaps by bilinear-sampling each level at its texel centres)
    std::vector<uint8_t> level = rgba;
    uint32_t lw = w;
    uint32_t lh = h;
    for (uint32_t k = 1; k < levels; ++k) {
        const uint32_t nw = std::max(1u, lw / 2);
        const uint32_t nh = std::max(1u, lh / 2);
        std::vector<uint8_t> next(size_t(nw) * nh * 4);
        for (uint32_t y = 0; y < nh; ++y) {
            for (uint32_t x = 0; x < nw; ++x) {
                for (int c = 0; c < 4; ++c) {
                    const uint32_t x0 = std::min(lw - 1, 2 * x);
                    const uint32_t x1 = std::min(lw - 1, 2 * x + 1);
                    const uint32_t y0 = std::min(lh - 1, 2 * y);
                    const uint32_t y1 = std::min(lh - 1, 2 * y + 1);
                    const uint32_t s = uint32_t(level[(size_t(y0) * lw + x0) * 4 + c]) + level[(size_t(y0) * lw + x1) * 4 + c] +
                                       level[(size_t(y1) * lw + x0) * 4 + c] + level[(size_t(y1) * lw + x1) * 4 + c];
                    next[(size_t(y) * nw + x) * 4 + c] = uint8_t((s + 2) / 4);
                }
            }
        }
        gpu_->writeTexture(t.texture, nw, nh, next.data(), k);
        level.swap(next);
        lw = nw;
        lh = nh;
    }
    WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    t.view = wgpuTextureCreateView(t.texture, &vd);
    return t;
}

WorldRenderer::Texture WorldRenderer::loadTexture(const std::string& assetsRoot, const json& entry) {
    if (!entry.contains("pack")) {
        throw std::runtime_error("texture without an archive: " + entry.dump());
    }
    std::string pack = entry["pack"].get<std::string>();
    const std::string key = pack + "#" + TextureArchive::key(entry["id"]);
    auto it = archiveTextures_.find(key);
    if (it != archiveTextures_.end()) {
        return it->second;
    }
    static std::map<std::string, std::unique_ptr<TextureArchive>> archives;
    const std::string path = assetsRoot + "/" + (pack.rfind("/assets/", 0) == 0 ? pack.substr(8) : pack);
    auto& a = archives[path];
    if (!a) {
        a = std::make_unique<TextureArchive>(path);
    }
    const Image img = a->decode(entry["id"]);
    Texture t = upload(uint32_t(img.width), uint32_t(img.height), img.rgba, true);
    archiveTextures_[key] = t;
    return t;
}

void WorldRenderer::buildPipelines() {
    WGPUDevice d = gpu_->device;
    // samplers: the package textures (linear, trilinear mips, anisotropy 4, repeat or clamp) and the light atlas (linear, clamp)
    WGPUSamplerDescriptor sd = WGPU_SAMPLER_DESCRIPTOR_INIT;
    sd.magFilter = WGPUFilterMode_Linear;
    sd.minFilter = WGPUFilterMode_Linear;
    sd.mipmapFilter = WGPUMipmapFilterMode_Linear;
    sd.maxAnisotropy = 4;
    sd.addressModeU = sd.addressModeV = WGPUAddressMode_Repeat;
    repeat_ = wgpuDeviceCreateSampler(d, &sd);
    sd.addressModeU = sd.addressModeV = WGPUAddressMode_ClampToEdge;
    clamp_ = wgpuDeviceCreateSampler(d, &sd);
    WGPUSamplerDescriptor ad = WGPU_SAMPLER_DESCRIPTOR_INIT;
    ad.magFilter = WGPUFilterMode_Linear;
    ad.minFilter = WGPUFilterMode_Linear;
    ad.addressModeU = ad.addressModeV = WGPUAddressMode_ClampToEdge;
    atlasSampler_ = wgpuDeviceCreateSampler(d, &ad);

    // layouts
    WGPUBindGroupLayoutEntry fe = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    fe.binding = 0;
    fe.visibility = WGPUShaderStage_Vertex;
    fe.buffer.type = WGPUBufferBindingType_Uniform;
    WGPUBindGroupLayoutDescriptor fd = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    fd.entryCount = 1;
    fd.entries = &fe;
    frameLayout_ = wgpuDeviceCreateBindGroupLayout(d, &fd);
    WGPUBindGroupLayoutEntry me[4];
    for (int k = 0; k < 4; ++k) {
        me[k] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
        me[k].binding = uint32_t(k);
        me[k].visibility = WGPUShaderStage_Fragment;
        if (k % 2 == 0) {
            me[k].texture.sampleType = WGPUTextureSampleType_Float;
            me[k].texture.viewDimension = WGPUTextureViewDimension_2D;
        } else {
            me[k].sampler.type = WGPUSamplerBindingType_Filtering;
        }
    }
    WGPUBindGroupLayoutDescriptor md = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    md.entryCount = 4;
    md.entries = me;
    materialLayout_ = wgpuDeviceCreateBindGroupLayout(d, &md);
    WGPUBindGroupLayout layouts[2] = {frameLayout_, materialLayout_};
    WGPUPipelineLayoutDescriptor pld = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pld.bindGroupLayoutCount = 2;
    pld.bindGroupLayouts = layouts;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(d, &pld);

    WGPUShaderModule module = gpu_->shader(kWorldShader, "world");
    // vertex buffers: the interleaved package vertices, colours, light UVs, vertex alpha
    WGPUVertexAttribute a0[4];
    const WGPUVertexFormat f0[4] = {WGPUVertexFormat_Float32x3, WGPUVertexFormat_Float32x3, WGPUVertexFormat_Float32x2, WGPUVertexFormat_Float32x2};
    const uint64_t o0[4] = {0, 12, 24, 32};
    for (int k = 0; k < 4; ++k) {
        a0[k] = WGPUVertexAttribute{nullptr, f0[k], o0[k], uint32_t(k)};
    }
    WGPUVertexAttribute a1{nullptr, WGPUVertexFormat_Float32x4, 0, 4};
    WGPUVertexAttribute a2{nullptr, WGPUVertexFormat_Float32x2, 0, 5};
    WGPUVertexAttribute a3{nullptr, WGPUVertexFormat_Float32, 0, 6};
    WGPUVertexBufferLayout vbl[4];
    for (auto& v : vbl) {
        v = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
        v.stepMode = WGPUVertexStepMode_Vertex;
    }
    vbl[0].arrayStride = 40;
    vbl[0].attributeCount = 4;
    vbl[0].attributes = a0;
    vbl[1].arrayStride = 16;
    vbl[1].attributeCount = 1;
    vbl[1].attributes = &a1;
    vbl[2].arrayStride = 8;
    vbl[2].attributeCount = 1;
    vbl[2].attributes = &a2;
    vbl[3].arrayStride = 4;
    vbl[3].attributeCount = 1;
    vbl[3].attributes = &a3;

    WGPUBlendState normal{};
    normal.color = {WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha, WGPUBlendFactor_OneMinusSrcAlpha};
    normal.alpha = {WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_OneMinusSrcAlpha};
    WGPUBlendState additive{};
    additive.color = {WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha, WGPUBlendFactor_One};
    additive.alpha = {WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_One};

    struct Kind {
        int id;
        const char* entry;
        const WGPUBlendState* blend;
        bool depthWrite;
        bool depthTest;
        double reference;
    };
    // the alpha test references (web/world-material.js alphaReference [0, 92, 20, 20] / 128)
    const Kind kinds[] = {
        {0, "terrain", nullptr, true, true, 0},
        {1, "instanceOpaque", nullptr, true, true, 0},
        {21, "instancePass", &normal, true, true, 92.0 / 128},
        {31, "instanceFringe", &normal, false, true, 92.0 / 128},
        {22, "instancePass", &normal, true, true, 20.0 / 128},
        {32, "instanceFringe", &normal, false, true, 20.0 / 128},
        {4, "instanceAdditive", &additive, false, true, 0},
        {5, "skyOpaque", nullptr, false, false, 0},
        {6, "skyBlend", &normal, false, false, 0},
    };
    for (const auto& k : kinds) {
        // opaque kinds draw double-sided; transparent ones twice (cull front = back faces first, then cull back)
        const bool transparent = k.blend != nullptr;
        const WGPUCullMode culls[2] = {transparent ? WGPUCullMode_Front : WGPUCullMode_None, WGPUCullMode_Back};
        for (int pass = 0; pass < (transparent ? 2 : 1); ++pass) {
            WGPURenderPipelineDescriptor pd = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
            pd.layout = pipelineLayout;
            pd.vertex.module = module;
            pd.vertex.entryPoint = sv("vs");
            pd.vertex.bufferCount = 4;
            pd.vertex.buffers = vbl;
            pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
            pd.primitive.frontFace = WGPUFrontFace_CCW;
            pd.primitive.cullMode = culls[pass];
            WGPUDepthStencilState ds = WGPU_DEPTH_STENCIL_STATE_INIT;
            ds.format = WGPUTextureFormat_Depth24PlusStencil8;
            ds.depthWriteEnabled = k.depthWrite ? WGPUOptionalBool_True : WGPUOptionalBool_False;
            ds.depthCompare = k.depthTest ? WGPUCompareFunction_LessEqual : WGPUCompareFunction_Always;
            const bool sky = k.id == 5 || k.id == 6;
            pd.depthStencil = sky ? nullptr : &ds;
            WGPUColorTargetState ct = WGPU_COLOR_TARGET_STATE_INIT;
            ct.format = WGPUTextureFormat_RGBA8Unorm;
            ct.blend = k.blend;
            ct.writeMask = WGPUColorWriteMask_All;
            WGPUConstantEntry constant = WGPU_CONSTANT_ENTRY_INIT;
            constant.key = sv("reference");
            constant.value = k.reference;
            WGPUFragmentState fs = WGPU_FRAGMENT_STATE_INIT;
            fs.module = module;
            fs.entryPoint = sv(k.entry);
            fs.targetCount = 1;
            fs.targets = &ct;
            const bool usesReference = std::strcmp(k.entry, "instancePass") == 0 || std::strcmp(k.entry, "instanceFringe") == 0;
            fs.constantCount = usesReference ? 1 : 0;
            fs.constants = usesReference ? &constant : nullptr;
            pd.fragment = &fs;
            pd.multisample.count = 1;
            pd.multisample.mask = 0xffffffffu;
            pipelines_[k.id * 10 + pass] = wgpuDeviceCreateRenderPipeline(d, &pd);
        }
    }

    // the composite
    WGPUBindGroupLayoutEntry ce[5];
    for (int k = 0; k < 5; ++k) {
        ce[k] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
        ce[k].binding = uint32_t(k);
        ce[k].visibility = WGPUShaderStage_Fragment;
    }
    ce[0].buffer.type = WGPUBufferBindingType_Uniform;
    for (int k : {1, 3, 4}) {
        ce[k].texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
        ce[k].texture.viewDimension = WGPUTextureViewDimension_2D;
    }
    ce[2].texture.sampleType = WGPUTextureSampleType_Depth;
    ce[2].texture.viewDimension = WGPUTextureViewDimension_2D;
    WGPUBindGroupLayoutDescriptor cd = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    cd.entryCount = 5;
    cd.entries = ce;
    compositeLayout_ = wgpuDeviceCreateBindGroupLayout(d, &cd);
    WGPUPipelineLayoutDescriptor cpl = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    cpl.bindGroupLayoutCount = 1;
    cpl.bindGroupLayouts = &compositeLayout_;
    WGPUPipelineLayout compositeLayout = wgpuDeviceCreatePipelineLayout(d, &cpl);
    WGPUShaderModule cm = gpu_->shader(kCompositeShader, "composite");
    WGPUShaderModule copy = gpu_->shader(kCopyShader, "sky background");
    WGPUBindGroupLayoutEntry ke = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    ke.binding = 0;
    ke.visibility = WGPUShaderStage_Fragment;
    ke.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
    ke.texture.viewDimension = WGPUTextureViewDimension_2D;
    WGPUBindGroupLayoutDescriptor kd = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    kd.entryCount = 1;
    kd.entries = &ke;
    copyLayout_ = wgpuDeviceCreateBindGroupLayout(d, &kd);
    WGPUPipelineLayoutDescriptor kpl = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    kpl.bindGroupLayoutCount = 1;
    kpl.bindGroupLayouts = &copyLayout_;
    WGPUPipelineLayout copyPipelineLayout = wgpuDeviceCreatePipelineLayout(d, &kpl);
    for (int which = 0; which < 2; ++which) {
        WGPURenderPipelineDescriptor pd = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        pd.layout = which == 0 ? copyPipelineLayout : compositeLayout;
        const WGPUShaderModule module = which == 0 ? copy : cm;
        pd.vertex.module = module;
        pd.vertex.entryPoint = sv("vs");
        pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        WGPUColorTargetState ct = WGPU_COLOR_TARGET_STATE_INIT;
        ct.format = WGPUTextureFormat_RGBA8Unorm;
        ct.writeMask = WGPUColorWriteMask_All;
        WGPUFragmentState fs = WGPU_FRAGMENT_STATE_INIT;
        fs.module = module;
        fs.entryPoint = sv(which == 0 ? "copySky" : "fog");
        fs.targetCount = 1;
        fs.targets = &ct;
        pd.fragment = &fs;
        // the sky copy is the world pass's background: drawn inside it, so it carries the world pass's depth attachment
        WGPUDepthStencilState ds = WGPU_DEPTH_STENCIL_STATE_INIT;
        ds.format = WGPUTextureFormat_Depth24PlusStencil8;
        ds.depthWriteEnabled = WGPUOptionalBool_False;
        ds.depthCompare = WGPUCompareFunction_Always;
        pd.depthStencil = which == 0 ? &ds : nullptr;
        pd.multisample.count = 1;
        pd.multisample.mask = 0xffffffffu;
        if (which == 0) {
            copySky_ = wgpuDeviceCreateRenderPipeline(d, &pd);
        } else {
            composite_ = wgpuDeviceCreateRenderPipeline(d, &pd);
        }
    }
    worldUniform_ = gpu_->buffer(64, WGPUBufferUsage_Uniform, nullptr, "world frame");
    skyUniform_ = gpu_->buffer(64, WGPUBufferUsage_Uniform, nullptr, "sky frame");
    compositeUniform_ = gpu_->buffer(80, WGPUBufferUsage_Uniform, nullptr, "composite");
    for (int k = 0; k < 2; ++k) {
        WGPUBindGroupEntry e = WGPU_BIND_GROUP_ENTRY_INIT;
        e.binding = 0;
        e.buffer = k == 0 ? worldUniform_ : skyUniform_;
        e.size = 64;
        WGPUBindGroupDescriptor bd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        bd.layout = frameLayout_;
        bd.entryCount = 1;
        bd.entries = &e;
        (k == 0 ? worldFrame_ : skyFrame_) = wgpuDeviceCreateBindGroup(d, &bd);
    }
    // the fog CLUT (256 RGBA entries, nearest)
    palette_.texture = gpu_->texture(256, 1, WGPUTextureFormat_RGBA8Unorm, WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst);
    WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    palette_.view = wgpuTextureCreateView(palette_.texture, &vd);
    std::vector<uint8_t> zero(1024, 0);
    gpu_->writeTexture(palette_.texture, 256, 1, zero.data());
}

WGPUBindGroup WorldRenderer::materialGroup(const Texture& map, bool clampMap) {
    WGPUBindGroupEntry e[4];
    for (int k = 0; k < 4; ++k) {
        e[k] = WGPU_BIND_GROUP_ENTRY_INIT;
        e[k].binding = uint32_t(k);
    }
    e[0].textureView = map.view;
    e[1].sampler = clampMap ? clamp_ : repeat_;
    e[2].textureView = atlas_.view;
    e[3].sampler = atlasSampler_;
    WGPUBindGroupDescriptor bd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bd.layout = materialLayout_;
    bd.entryCount = 4;
    bd.entries = e;
    return wgpuDeviceCreateBindGroup(gpu_->device, &bd);
}

// web/main.js asset() for a world or sky package (vertices.bin / colors.bin / indices.bin), with web/world-material.js's
// light UVs and vertex alpha for the course world
void WorldRenderer::loadPackage(Package& p, const std::string& assetsRoot, const std::string& dir, bool world) {
    const std::string root = assetsRoot + "/" + dir + "/";
    const json d = readJson(root + "world.json");
    const auto vertices = readArray<float>(root + "vertices.bin");
    const auto colors = readArray<float>(root + "colors.bin");
    const auto indices = readArray<uint32_t>(root + "indices.bin");
    const size_t count = vertices.size() / 10;
    std::vector<float> lightUv(count * 2, 0.f);
    std::vector<float> alpha(count, 1.f);
    if (world) {
        const auto alphaBytes = readBytes(root + "vertex-alpha.bin");
        if (alphaBytes.size() != count) {
            throw std::runtime_error("Static-model vertex alpha does not match the vertex buffer");
        }
        for (size_t k = 0; k < count; ++k) {
            alpha[k] = float(alphaBytes[k]) / 128.f;
        }
        const json lighting = readJson(root + "terrain-lighting.json");
        const json render = readJson(root + "terrain-render.json");
        const double W = lighting["atlas"]["width"].get<double>();
        const double H = lighting["atlas"]["height"].get<double>();
        for (const auto& patch : render["patches"]) {
            const json& entry = lighting["patches"][std::to_string(patch["resource"].get<long long>())];
            const json& g = patch["lightUv"];
            const json& page = lighting["pages"][entry[0].get<size_t>()];
            const double p0 = entry[1].get<double>();
            const double p1 = entry[2].get<double>();
            const double p2 = entry[3].get<double>();
            const double p3 = entry[4].get<double>();
            const double pw = page["width"].get<double>();
            const double ph = page["height"].get<double>();
            const double px = page["x"].get<double>();
            const double py = page["y"].get<double>();
            // web/world-material.js: the affine map of the installed uv1 into the PS2 light page (JS doubles, stored as float32)
            const double sx = (pw * p2) / g[2].get<double>() / W;
            const double sy = (ph * p3) / g[3].get<double>() / H;
            const double map0 = (px + pw * p0) / W - g[0].get<double>() * sx;
            const double map2 = (py + ph * p1) / H - g[1].get<double>() * sy;
            const size_t first = patch["baseline_vertex_start"].get<size_t>();
            for (size_t v = first; v < first + 81 && v < count; ++v) {
                lightUv[v * 2] = float(map0 + double(vertices[v * 10 + 8]) * sx);
                lightUv[v * 2 + 1] = float(map2 + double(vertices[v * 10 + 9]) * sy);
            }
        }
    }
    p.vertices = gpu_->buffer(vertices.size() * 4, WGPUBufferUsage_Vertex, vertices.data(), "vertices");
    p.colors = gpu_->buffer(colors.size() * 4, WGPUBufferUsage_Vertex, colors.data(), "colors");
    p.lightUv = gpu_->buffer(lightUv.size() * 4, WGPUBufferUsage_Vertex, lightUv.data(), "light uv");
    p.vertexAlpha = gpu_->buffer(alpha.size() * 4, WGPUBufferUsage_Vertex, alpha.data(), "vertex alpha");
    p.indices = gpu_->buffer(indices.size() * 4, WGPUBufferUsage_Index, indices.data(), "indices");
    std::map<std::string, WGPUBindGroup> groups;
    const json& textures = d["textures"];
    size_t index = 0;
    for (const auto& b : d["batches"]) {
        const size_t batch = index++;
        if (b.contains("hidden_resource")) {
            // runtime flag bit 0 clear: never drawn
            continue;
        }
        const std::string key = "9-" + std::to_string(b["texture"].get<int>());
        if (!textures.contains(key)) {
            continue;
        }
        const bool instance = b.value("instance", false);
        int blend = 0;
        bool clampMap = false;
        if (!world) {
            const int flags = b.value("material_flags", 1);
            blend = skyBlendClass(flags);
            clampMap = (flags & 8) && (flags & 16);
        } else if (instance) {
            blend = b.value("blend", 0);
            clampMap = b.value("wrap", 0) != 0;
        }
        const std::string gkey = key + (clampMap ? ":c" : "");
        auto g = groups.find(gkey);
        if (g == groups.end()) {
            g = groups.emplace(gkey, materialGroup(loadTexture(assetsRoot, textures[key]), clampMap)).first;
        }
        Draw draw;
        draw.first = b["first_index"].get<uint32_t>();
        draw.count = b["index_count"].get<uint32_t>();
        draw.material = g->second;
        draw.chunk = b.contains("chunk") ? b["chunk"].get<int>() : -1;
        draw.batch = batch;
        if (!world) {
            draw.kind = blend ? 6 : 5;
            (blend ? p.transparent : p.opaque).push_back(draw);
        } else if (!instance) {
            draw.kind = 0;
            p.opaque.push_back(draw);
        } else if (blend == 0) {
            draw.kind = 1;
            p.opaque.push_back(draw);
        } else if (blend == 3) {
            draw.kind = 4;
            draw.order = 1;
            p.transparent.push_back(draw);
        } else {
            // the pass and its fringe, consecutive (one mesh, two material groups)
            draw.order = blend == 2 ? 1 : 0;
            draw.kind = 20 + blend;
            p.transparent.push_back(draw);
            draw.kind = 30 + blend;
            p.transparent.push_back(draw);
        }
    }
    // three.js reversePainterSortStable: renderOrder (SORTED_CLASS_ORDER 0.5 for classes 2 / 3), then creation order
    std::stable_sort(p.transparent.begin(), p.transparent.end(), [](const Draw& a, const Draw& b) { return a.order < b.order; });
}

void WorldRenderer::load(Gpu& gpu, const std::string& assetsRoot, const std::string& code) {
    gpu_ = &gpu;
    buildPipelines();
    const json start = readJson(assetsRoot + "/" + code + "/start.json");
    for (int k = 0; k < 3; ++k) {
        origin[size_t(k)] = start["position"][size_t(k)].get<double>();
    }
    const Image atlas = readPng(assetsRoot + "/" + code + "/terrain-light-atlas.png");
    atlas_ = upload(uint32_t(atlas.width), uint32_t(atlas.height), atlas.rgba, false);
    loadPackage(world_, assetsRoot, code, true);
    loadPackage(sky_, assetsRoot, "SKY", false);
}

void WorldRenderer::ensureTargets(uint32_t w, uint32_t h) {
    if (w == targetW_ && h == targetH_) {
        return;
    }
    targetW_ = w;
    targetH_ = h;
    WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    const WGPUTextureUsage usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding;
    skyColor_.texture = gpu_->texture(w, h, WGPUTextureFormat_RGBA8Unorm, usage, 1, "sky pass");
    skyColor_.view = wgpuTextureCreateView(skyColor_.texture, &vd);
    worldColor_.texture = gpu_->texture(w, h, WGPUTextureFormat_RGBA8Unorm, usage, 1, "world pass");
    worldColor_.view = wgpuTextureCreateView(worldColor_.texture, &vd);
    depth_.texture = gpu_->texture(w, h, WGPUTextureFormat_Depth24PlusStencil8, usage, 1, "world depth");
    depth_.view = wgpuTextureCreateView(depth_.texture, &vd);
    WGPUTextureViewDescriptor dv = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    dv.aspect = WGPUTextureAspect_DepthOnly;
    depthSample_ = wgpuTextureCreateView(depth_.texture, &dv);
    WGPUBindGroupEntry e[5];
    for (int k = 0; k < 5; ++k) {
        e[k] = WGPU_BIND_GROUP_ENTRY_INIT;
        e[k].binding = uint32_t(k);
    }
    e[0].buffer = compositeUniform_;
    e[0].size = 80;
    e[1].textureView = worldColor_.view;
    e[2].textureView = depthSample_;
    e[3].textureView = palette_.view;
    e[4].textureView = skyColor_.view;
    WGPUBindGroupDescriptor bd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bd.layout = compositeLayout_;
    bd.entryCount = 5;
    bd.entries = e;
    compositeGroup_ = wgpuDeviceCreateBindGroup(gpu_->device, &bd);
    WGPUBindGroupEntry ke = WGPU_BIND_GROUP_ENTRY_INIT;
    ke.binding = 0;
    ke.textureView = skyColor_.view;
    WGPUBindGroupDescriptor kd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    kd.layout = copyLayout_;
    kd.entryCount = 1;
    kd.entries = &ke;
    copyGroup_ = wgpuDeviceCreateBindGroup(gpu_->device, &kd);
}

void WorldRenderer::drawPackage(WGPURenderPassEncoder pass, const Package& p, const std::vector<Draw>& draws, const WorldFrameState& state) {
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, p.vertices, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 1, p.colors, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 2, p.lightUv, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 3, p.vertexAlpha, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass, p.indices, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
    for (const auto& d : draws) {
        if (d.chunk >= 0) {
            // texture chunks the original has not streamed in are not drawn (0x22A5A0; an unknown chunk is drawn)
            auto c = state.chunks.find(uint32_t(d.chunk));
            if (c != state.chunks.end() && !c->second) {
                continue;
            }
        }
        wgpuRenderPassEncoderSetBindGroup(pass, 1, d.material, 0, nullptr);
        const bool transparent = d.kind >= 4;
        for (int side = 0; side < (transparent && d.kind != 5 ? 2 : 1); ++side) {
            auto it = pipelines_.find(d.kind * 10 + side);
            if (it == pipelines_.end()) {
                continue;
            }
            wgpuRenderPassEncoderSetPipeline(pass, it->second);
            wgpuRenderPassEncoderDrawIndexed(pass, d.count, 1, d.first, 0, 0);
        }
    }
}

void WorldRenderer::render(const CameraView& camera, const WorldFrameState& state, WGPUTextureView target, uint32_t width, uint32_t height, double aspect) {
    ensureTargets(width, height);
    const auto& c = camera.v;
    // main.js: camera at eye - origin looking at look - origin (up y), fov = 2 atan(tan(half horizontal) x 0.75), near >= 0.05
    const double eye[3] = {c[0] - origin[0], c[1] - origin[1], c[2] - origin[2]};
    const double look[3] = {c[3] - origin[0], c[4] - origin[1], c[5] - origin[2]};
    const double fovY = 2 * std::atan(std::tan(double(c[6])) * 0.75) * 180 / M_PI;
    const double near = std::max(0.05, double(c[7]));
    const double far = double(c[8]);
    const Mat4 P = perspective(fovY, aspect, near, far);
    const Mat4 V = view(eye, look);
    // world meshes sit at -origin; the sky at the eye (sky.position = camera.position + origin, its meshes at -origin)
    const Mat4 worldMvp = multiply(P, multiply(V, translate(-origin[0], -origin[1], -origin[2])));
    const Mat4 skyMvp = multiply(P, multiply(V, translate(eye[0], eye[1], eye[2])));
    float wf[16];
    float sf[16];
    for (int k = 0; k < 16; ++k) {
        wf[k] = float(worldMvp[size_t(k)]);
        sf[k] = float(skyMvp[size_t(k)]);
    }
    wgpuQueueWriteBuffer(gpu_->queue, worldUniform_, 0, wf, 64);
    if (!state.riders.empty()) {
        // a rider bypasses the model matrix: origin-relative positions through the camera's projection x view
        const Mat4 vp = multiply(P, V);
        std::array<float, 16> f{};
        for (int k = 0; k < 16; ++k) {
            f[size_t(k)] = float(vp[size_t(k)]);
        }
        for (const auto& [r, frame] : state.riders) {
            r->update(f, origin, *frame);
        }
    }
    wgpuQueueWriteBuffer(gpu_->queue, skyUniform_, 0, sf, 64);
    const auto& tn = state.tint;
    const float params[20] = {float(near), float(far), state.fogSlope, state.fogOffset,
                              state.fogEnabled ? 1.f : 0.f, float(tn.enabled), tn.flashOn ? 1.f : 0.f, tn.flashA,
                              tn.scaleBytes[0], tn.scaleBytes[1], tn.scaleBytes[2], 0,
                              tn.addBytes[0], tn.addBytes[1], tn.addBytes[2], 0,
                              tn.flashRGB[0], tn.flashRGB[1], tn.flashRGB[2], 0};
    wgpuQueueWriteBuffer(gpu_->queue, compositeUniform_, 0, params, sizeof params);
    if (state.fogPalette && state.fogRevision != paletteRevision_) {
        gpu_->writeTexture(palette_.texture, 256, 1, state.fogPalette);
        paletteRevision_ = state.fogRevision;
    }

    WGPUCommandEncoderDescriptor ed = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu_->device, &ed);
    auto colorPass = [&](WGPUTextureView view, bool withDepth) {
        WGPURenderPassColorAttachment ca = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        ca.view = view;
        ca.loadOp = WGPULoadOp_Clear;
        ca.storeOp = WGPUStoreOp_Store;
        ca.clearValue = {0, 0, 0, 1};
        WGPURenderPassDepthStencilAttachment da = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
        da.view = depth_.view;
        da.depthLoadOp = WGPULoadOp_Clear;
        da.depthStoreOp = WGPUStoreOp_Store;
        da.depthClearValue = 1;
        da.stencilLoadOp = WGPULoadOp_Clear;
        da.stencilStoreOp = WGPUStoreOp_Store;
        da.stencilClearValue = 0;
        WGPURenderPassDescriptor rd = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        rd.colorAttachmentCount = 1;
        rd.colorAttachments = &ca;
        rd.depthStencilAttachment = withDepth ? &da : nullptr;
        return wgpuCommandEncoderBeginRenderPass(enc, &rd);
    };
    // the sky pass (cleared to black)
    {
        WGPURenderPassEncoder pass = colorPass(skyColor_.view, false);
        wgpuRenderPassEncoderSetBindGroup(pass, 0, skyFrame_, 0, nullptr);
        drawPackage(pass, sky_, sky_.opaque, state);
        drawPackage(pass, sky_, sky_.transparent, state);
        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);
    }
    // the world pass: the sky pass as its background, then opaque, then transparent
    {
        WGPURenderPassEncoder pass = colorPass(worldColor_.view, true);
        wgpuRenderPassEncoderSetPipeline(pass, copySky_);
        wgpuRenderPassEncoderSetBindGroup(pass, 0, copyGroup_, 0, nullptr);
        wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
        wgpuRenderPassEncoderSetBindGroup(pass, 0, worldFrame_, 0, nullptr);
        drawPackage(pass, world_, world_.opaque, state);
        for (const auto& [r, frame] : state.riders) {
            r->drawOpaque(pass);
        }
        wgpuRenderPassEncoderSetBindGroup(pass, 0, worldFrame_, 0, nullptr);
        drawPackage(pass, world_, world_.transparent, state);
        for (const auto& [r, frame] : state.riders) {
            r->drawTransparent(pass);
        }
        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);
    }
    // the fog composite into the target
    {
        WGPURenderPassEncoder pass = colorPass(target, false);
        wgpuRenderPassEncoderSetPipeline(pass, composite_);
        wgpuRenderPassEncoderSetBindGroup(pass, 0, compositeGroup_, 0, nullptr);
        wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);
    }
    WGPUCommandBufferDescriptor cd = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, &cd);
    wgpuQueueSubmit(gpu_->queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
}

}  // namespace ssx_native
