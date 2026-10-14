#include "post_effects.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace ssx_native {

namespace {

// the depth queries: web/light-glow.js / sun-flare.js query fragments (2EC478), on the world depth
const char* kQueryShader = R"(
struct Params {
  near: f32, far: f32, vw: f32, vh: f32,
  // the depth target's size, the light count, the read rect's width / height
  depthW: f32, depthH: f32, count: f32, pad: f32,
  readW: f32, readH: f32, pad1: f32, pad2: f32,
};
@group(0) @binding(0) var<uniform> params: Params;
@group(0) @binding(1) var<storage, read> query: array<vec4f>;
@group(0) @binding(2) var depthTex: texture_depth_2d;
@vertex fn vs(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
  let p = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  return vec4f(p[i], 0.0, 1.0);
}
fn sceneDepth(d: f32) -> f32 {
  return -(params.near * params.far / ((params.far - params.near) * d - params.far));
}
// texture(depth, ((px + .5) / VW, v)) of the 512 x 448 viewport on the depth target (nearest)
fn depthAt(px: f32, v: f32) -> f32 {
  let u = (px + 0.5) / params.vw;
  let x = clamp(i32(floor(u * params.depthW)), 0, i32(params.depthW) - 1);
  let y = clamp(i32(floor(clamp(v, 0.0, 1.0) * params.depthH)), 0, i32(params.depthH) - 1);
  return textureLoad(depthTex, vec2i(x, y), 0);
}
// rect: count rect (x, y, w, h); read: read origin + reference depth (m)
fn visibility(rect: vec4f, read: vec4f) -> f32 {
  var open = 0.0;
  var counted = 0.0;
  for (var gy = 0; gy < i32(params.readH); gy++) {
    for (var gx = 0; gx < i32(params.readW); gx++) {
      let px = read.x + f32(gx);
      let py = read.y + f32(gy);
      let inRect = px >= rect.x && px < rect.x + rect.z && py >= rect.y && py < rect.y + rect.w;
      let v = (py + 0.5) / params.vh;
      let open1 = sceneDepth(depthAt(px, v)) >= read.z || v < 0.0 || v > 1.0;
      counted += select(0.0, 1.0, inRect);
      open += select(0.0, 1.0, inRect && open1);
    }
  }
  let area = rect.z * rect.w;
  return select(max((open - area * 0.5) / (area * 0.5), 0.0), open / area, counted == area);
}
@fragment fn lights(@builtin(position) p: vec4f) -> @location(0) vec4f {
  let i = u32(p.x);
  return vec4f(visibility(query[i], query[u32(params.count) + i]), 0.0, 0.0, 1.0);
}
@fragment fn sun() -> @location(0) vec4f {
  return vec4f(visibility(query[0], query[1]), 0.0, 0.0, 1.0);
}
)";

// web/light-glow.js glowMaterial: the halo sprites, ALPHA 0x48, emitted twice (copies)
const char* kGlowShader = R"(
struct Params { near: f32, far: f32, copies: f32, pad: f32 };
@group(0) @binding(0) var<uniform> params: Params;
@group(0) @binding(1) var shal: texture_2d<f32>;
@group(0) @binding(2) var mhal: texture_2d<f32>;
@group(0) @binding(3) var linearSampler: sampler;
@group(0) @binding(4) var vis: texture_2d<f32>;
@group(0) @binding(5) var depthTex: texture_depth_2d;
struct In {
  @location(0) position: vec3f,
  @location(1) uv: vec2f,
  @location(2) rgb: vec3f,
  @location(3) info: vec4f,
};
struct Out {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
  @location(1) rgb: vec3f,
  @location(2) @interpolate(flat) info: vec4f,
};
@vertex fn vs(v: In) -> Out {
  var o: Out;
  o.position = vec4f(v.position.xy, 0.0, 1.0);
  o.uv = v.uv;
  o.rgb = v.rgb;
  o.info = v.info;
  return o;
}
@fragment fn fs(i: Out) -> @location(0) vec4f {
  let a = textureSample(shal, linearSampler, i.uv);
  let b = textureSample(mhal, linearSampler, i.uv);
  let t = round(select(a, b, i.info.w > 0.5) * 255.0);
  let visibility = textureLoad(vis, vec2i(i32(i.info.x), 0), 0).r;
  let farGlow = i.info.y > 0.5;
  let va = select(floor(visibility * 128.0), 128.0, farGlow);
  let d = textureLoad(depthTex, vec2i(i.position.xy), 0);
  let sceneDepth = -(params.near * params.far / ((params.far - params.near) * d - params.far));
  let passes = !farGlow || sceneDepth >= i.info.z;
  let cs = min(floor(t.rgb * i.rgb / 128.0), vec3f(255.0));
  let as_ = floor(t.a * va / 128.0);
  return vec4f(select(vec3f(0.0), floor(cs * as_ / 128.0) * params.copies, passes), 1.0);
}
)";

}  // namespace

WGPUTexture PostEffects::makeTexture(uint32_t w, uint32_t h, const std::vector<uint8_t>& rgba, WGPUTextureView& view) {
    WGPUTexture t = gpu_->texture(w, h, WGPUTextureFormat_RGBA8Unorm, WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst);
    if (!rgba.empty()) {
        gpu_->writeTexture(t, w, h, rgba.data());
    }
    WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    view = wgpuTextureCreateView(t, &vd);
    return t;
}

void PostEffects::init(Gpu& gpu) {
    gpu_ = &gpu;
    WGPUDevice d = gpu.device;
    WGPUSamplerDescriptor sd = WGPU_SAMPLER_DESCRIPTOR_INIT;
    sd.magFilter = WGPUFilterMode_Linear;
    sd.minFilter = WGPUFilterMode_Linear;
    sd.addressModeU = WGPUAddressMode_ClampToEdge;
    sd.addressModeV = WGPUAddressMode_ClampToEdge;
    linear_ = wgpuDeviceCreateSampler(d, &sd);
    glowParams_ = gpu.buffer(48, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst, nullptr, "glow query params");
    sunParams_ = gpu.buffer(48, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst, nullptr, "sun query params");
    sunRows_ = gpu.buffer(32, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, nullptr, "sun query");
    sprites_ = gpu.buffer(16 * 31, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst, nullptr, "sun sprites");
    glowDrawParams_ = gpu.buffer(16, WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst, nullptr, "glow params");
    WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    sunVis_ = gpu.texture(1, 1, WGPUTextureFormat_RGBA16Float, WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding, 1, "sun visibility");
    sunVisView_ = wgpuTextureCreateView(sunVis_, &vd);

    // the query pipelines (group 0: params, query rows, depth)
    WGPUBindGroupLayoutEntry qe[3];
    for (int k = 0; k < 3; ++k) {
        qe[k] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
        qe[k].binding = uint32_t(k);
        qe[k].visibility = WGPUShaderStage_Fragment;
    }
    qe[0].buffer.type = WGPUBufferBindingType_Uniform;
    qe[1].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    qe[2].texture.sampleType = WGPUTextureSampleType_Depth;
    qe[2].texture.viewDimension = WGPUTextureViewDimension_2D;
    WGPUBindGroupLayoutDescriptor qd = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    qd.entryCount = 3;
    qd.entries = qe;
    queryLayout_ = wgpuDeviceCreateBindGroupLayout(d, &qd);
    WGPUPipelineLayoutDescriptor qpl = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    qpl.bindGroupLayoutCount = 1;
    qpl.bindGroupLayouts = &queryLayout_;
    WGPUPipelineLayout queryPipelineLayout = wgpuDeviceCreatePipelineLayout(d, &qpl);
    WGPUShaderModule qm = gpu.shader(kQueryShader, "depth queries");
    for (int which = 0; which < 2; ++which) {
        WGPURenderPipelineDescriptor pd = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        pd.layout = queryPipelineLayout;
        pd.vertex.module = qm;
        pd.vertex.entryPoint = sv("vs");
        pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        WGPUColorTargetState ct = WGPU_COLOR_TARGET_STATE_INIT;
        ct.format = WGPUTextureFormat_RGBA16Float;
        ct.writeMask = WGPUColorWriteMask_All;
        WGPUFragmentState fs = WGPU_FRAGMENT_STATE_INIT;
        fs.module = qm;
        fs.entryPoint = sv(which == 0 ? "lights" : "sun");
        fs.targetCount = 1;
        fs.targets = &ct;
        pd.fragment = &fs;
        pd.multisample.count = 1;
        pd.multisample.mask = 0xffffffffu;
        (which == 0 ? glowQuery_ : sunQuery_) = wgpuDeviceCreateRenderPipeline(d, &pd);
    }
    // the glow sprites (group 0: params, shal, mhal, sampler, visibility, depth)
    WGPUBindGroupLayoutEntry ge[6];
    for (int k = 0; k < 6; ++k) {
        ge[k] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
        ge[k].binding = uint32_t(k);
        ge[k].visibility = WGPUShaderStage_Fragment;
    }
    ge[0].buffer.type = WGPUBufferBindingType_Uniform;
    ge[1].texture.sampleType = WGPUTextureSampleType_Float;
    ge[1].texture.viewDimension = WGPUTextureViewDimension_2D;
    ge[2].texture.sampleType = WGPUTextureSampleType_Float;
    ge[2].texture.viewDimension = WGPUTextureViewDimension_2D;
    ge[3].sampler.type = WGPUSamplerBindingType_Filtering;
    ge[4].texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
    ge[4].texture.viewDimension = WGPUTextureViewDimension_2D;
    ge[5].texture.sampleType = WGPUTextureSampleType_Depth;
    ge[5].texture.viewDimension = WGPUTextureViewDimension_2D;
    WGPUBindGroupLayoutDescriptor gd = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    gd.entryCount = 6;
    gd.entries = ge;
    glowDrawLayout_ = wgpuDeviceCreateBindGroupLayout(d, &gd);
    WGPUPipelineLayoutDescriptor gpl = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    gpl.bindGroupLayoutCount = 1;
    gpl.bindGroupLayouts = &glowDrawLayout_;
    WGPUPipelineLayout glowPipelineLayout = wgpuDeviceCreatePipelineLayout(d, &gpl);
    WGPUShaderModule gm = gpu.shader(kGlowShader, "light glows");
    WGPUVertexAttribute attrs[4] = {
        {nullptr, WGPUVertexFormat_Float32x3, 0, 0},
        {nullptr, WGPUVertexFormat_Float32x2, 12, 1},
        {nullptr, WGPUVertexFormat_Float32x3, 20, 2},
        {nullptr, WGPUVertexFormat_Float32x4, 32, 3},
    };
    WGPUVertexBufferLayout vbl = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
    vbl.arrayStride = sizeof(GlowVertex);
    vbl.stepMode = WGPUVertexStepMode_Vertex;
    vbl.attributeCount = 4;
    vbl.attributes = attrs;
    WGPUBlendState add{};
    add.color = {WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_One};
    add.alpha = {WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_One};
    WGPURenderPipelineDescriptor pd = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    pd.layout = glowPipelineLayout;
    pd.vertex.module = gm;
    pd.vertex.entryPoint = sv("vs");
    pd.vertex.bufferCount = 1;
    pd.vertex.buffers = &vbl;
    pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pd.primitive.cullMode = WGPUCullMode_None;
    WGPUColorTargetState ct = WGPU_COLOR_TARGET_STATE_INIT;
    ct.format = WGPUTextureFormat_RGBA16Float;
    ct.blend = &add;
    ct.writeMask = WGPUColorWriteMask_All;
    WGPUFragmentState fs = WGPU_FRAGMENT_STATE_INIT;
    fs.module = gm;
    fs.entryPoint = sv("fs");
    fs.targetCount = 1;
    fs.targets = &ct;
    pd.fragment = &fs;
    pd.multisample.count = 1;
    pd.multisample.mask = 0xffffffffu;
    glowDraw_ = wgpuDeviceCreateRenderPipeline(d, &pd);

    // the composite's group 1: the glow target, the sun sprites, the sun textures, the sampler, the sun visibility
    WGPUBindGroupLayoutEntry ce[7];
    for (int k = 0; k < 7; ++k) {
        ce[k] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
        ce[k].binding = uint32_t(k);
        ce[k].visibility = WGPUShaderStage_Fragment;
    }
    ce[0].texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
    ce[0].texture.viewDimension = WGPUTextureViewDimension_2D;
    ce[1].buffer.type = WGPUBufferBindingType_Uniform;
    for (int k : {2, 3, 4}) {
        ce[k].texture.sampleType = WGPUTextureSampleType_Float;
        ce[k].texture.viewDimension = WGPUTextureViewDimension_2D;
    }
    ce[5].sampler.type = WGPUSamplerBindingType_Filtering;
    ce[6].texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
    ce[6].texture.viewDimension = WGPUTextureViewDimension_2D;
    WGPUBindGroupLayoutDescriptor cd = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    cd.entryCount = 7;
    cd.entries = ce;
    layout_ = wgpuDeviceCreateBindGroupLayout(d, &cd);
}

void PostEffects::attach(LightGlow* glow, SunFlare* sun) {
    glow_ = glow;
    sun_ = sun;
    Gpu& gpu = *gpu_;
    // the textures (a 1 x 1 placeholder when a package is absent)
    const std::vector<uint8_t> blank(4, 0);
    if (glow) {
        makeTexture(glow->shal.width, glow->shal.height, glow->shal.rgba, shalView_);
        makeTexture(glow->mhal.width, glow->mhal.height, glow->mhal.rgba, mhalView_);
    } else {
        makeTexture(1, 1, blank, shalView_);
        makeTexture(1, 1, blank, mhalView_);
    }
    const char* sunNames[3] = {"sun1", "sun2", "lens"};
    for (int k = 0; k < 3; ++k) {
        bool found = false;
        if (sun) {
            for (const auto& t : sun->textures) {
                if (t.name == sunNames[k]) {
                    makeTexture(t.width, t.height, t.rgba, sunViews_[k]);
                    found = true;
                }
            }
        }
        if (!found) {
            makeTexture(1, 1, blank, sunViews_[k]);
        }
    }
    // the light records and the visibility target: one texel per light
    const uint32_t lights = glow ? uint32_t(glow->count()) : 1;
    glowQueryBuffer_ = gpu.buffer(uint64_t(lights) * 2 * 16, WGPUBufferUsage_Storage | WGPUBufferUsage_CopyDst, nullptr, "glow query");
    glowVis_ = gpu.texture(lights, 1, WGPUTextureFormat_RGBA16Float, WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding, 1, "glow visibility");
    WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    glowVisView_ = wgpuTextureCreateView(glowVis_, &vd);
    // the groups are made again on the next frame
    w_ = 0;
    h_ = 0;
}

void PostEffects::ensureTargets(uint32_t w, uint32_t h, WGPUTextureView depthSample) {
    if (w == w_ && h == h_ && depthSample == depth_) {
        return;
    }
    w_ = w;
    h_ = h;
    depth_ = depthSample;
    WGPUDevice d = gpu_->device;
    glowTarget_ = gpu_->texture(w, h, WGPUTextureFormat_RGBA16Float, WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding, 1, "glow target");
    WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    glowTargetView_ = wgpuTextureCreateView(glowTarget_, &vd);
    auto group = [&](WGPUBindGroupLayout layout, std::vector<WGPUBindGroupEntry> e) {
        for (size_t k = 0; k < e.size(); ++k) {
            e[k].binding = uint32_t(k);
        }
        WGPUBindGroupDescriptor bd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        bd.layout = layout;
        bd.entryCount = e.size();
        bd.entries = e.data();
        return wgpuDeviceCreateBindGroup(d, &bd);
    };
    auto buf = [](WGPUBuffer b, uint64_t size) {
        WGPUBindGroupEntry e = WGPU_BIND_GROUP_ENTRY_INIT;
        e.buffer = b;
        e.size = size;
        return e;
    };
    auto tex = [](WGPUTextureView v) {
        WGPUBindGroupEntry e = WGPU_BIND_GROUP_ENTRY_INIT;
        e.textureView = v;
        return e;
    };
    auto smp = [](WGPUSampler s) {
        WGPUBindGroupEntry e = WGPU_BIND_GROUP_ENTRY_INIT;
        e.sampler = s;
        return e;
    };
    const uint32_t lights = glow_ ? uint32_t(glow_->count()) : 1;
    glowGroup_ = group(queryLayout_, {buf(glowParams_, 48), buf(glowQueryBuffer_, uint64_t(lights) * 32), tex(depthSample)});
    sunGroup_ = group(queryLayout_, {buf(sunParams_, 48), buf(sunRows_, 32), tex(depthSample)});
    glowDrawGroup_ = group(glowDrawLayout_, {buf(glowDrawParams_, 16), tex(shalView_), tex(mhalView_), smp(linear_), tex(glowVisView_), tex(depthSample)});
    group_ = group(layout_, {tex(glowTargetView_), buf(sprites_, 16 * 31), tex(sunViews_[0]), tex(sunViews_[1]), tex(sunViews_[2]), smp(linear_), tex(sunVisView_)});
}

void PostEffects::prepare(const double* world, const double* view, const double* projection, double near, double far, uint32_t width,
                          uint32_t height, WGPUTextureView depthSample) {
    ensureTargets(width, height, depthSample);
    near_ = float(near);
    far_ = float(far);
    // the light glows: each visible light's quad and query record
    glowVertexCount_ = 0;
    glowQueried_ = false;
    if (glow_) {
        glow_->update(world, view, projection, near, far);
        glowVertexCount_ = uint32_t(glow_->vertices.size());
        if (glowVertexCount_ > glowCapacity_) {
            glowCapacity_ = std::max(glowVertexCount_, 6u * 64u);
            glowVertices_ = gpu_->buffer(uint64_t(glowCapacity_) * sizeof(GlowVertex), WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst, nullptr, "glow sprites");
        }
        if (glowVertexCount_) {
            wgpuQueueWriteBuffer(gpu_->queue, glowVertices_, 0, glow_->vertices.data(), size_t(glowVertexCount_) * sizeof(GlowVertex));
        }
        if (glow_->queried) {
            glowQueried_ = true;
            wgpuQueueWriteBuffer(gpu_->queue, glowQueryBuffer_, 0, glow_->query.data(), glow_->query.size() * 4);
        }
        const float qp[12] = {near_, far_, 512, 448, float(width), float(height), float(glow_->count()), 0, 16, 8, 0, 0};
        wgpuQueueWriteBuffer(gpu_->queue, glowParams_, 0, qp, sizeof qp);
        const float gp[4] = {near_, far_, float(glow_->copies()), 0};
        wgpuQueueWriteBuffer(gpu_->queue, glowDrawParams_, 0, gp, sizeof gp);
    }
    // the sun (sun-flare.js update): the point at far - 500 cm along the painter's direction, its sprites and query
    float sprites[4 * 31] = {};
    sunVisible_ = false;
    if (sun_) {
        const auto& v = sun_->values();
        const auto& c = sun_->package()["constants"];
        const auto& textures = sun_->package()["sun_textures"];
        const int texture = int(v[6]);
        if (texture != -1 && texture >= 0 && size_t(texture) < textures.size()) {
            const float d2r = c["degrees_to_radians"].get<float>();
            float el = float(v[0] * d2r);
            float az = float(v[1] * d2r);
            if (el == 0 && az == 0) {
                az = c["default_azimuth_rad"].get<float>();
                el = c["default_elevation_rad"].get<float>();
            }
            const double ce = std::cos(double(el));
            const double ps2[3] = {std::cos(double(az)) * ce, std::sin(double(az)) * ce, std::sin(double(el))};
            const double dir[3] = {ps2[0], ps2[2], -ps2[1]};
            const double reach = far - c["far_inset_cm"].get<double>() / 100;
            const double point[3] = {world[12] + dir[0] * reach, world[13] + dir[1] * reach, world[14] + dir[2] * reach};
            double vp[4] = {};
            for (int r = 0; r < 4; ++r) {
                vp[r] = view[r] * point[0] + view[4 + r] * point[1] + view[8 + r] * point[2] + view[12 + r];
            }
            double clip[4] = {};
            for (int r = 0; r < 4; ++r) {
                clip[r] = projection[r] * vp[0] + projection[4 + r] * vp[1] + projection[8 + r] * vp[2] + projection[12 + r] * vp[3];
            }
            const double ndc[3] = {clip[0] / clip[3], clip[1] / clip[3], clip[2] / clip[3]};
            const double VW = c["viewport"][0].get<double>();
            const double VH = c["viewport"][1].get<double>();
            if (vp[2] < 0 && std::abs(ndc[0]) <= 1 && std::abs(ndc[1]) <= 1 && ndc[2] <= 1) {
                sunVisible_ = true;
                const double screen[2] = {(ndc[0] + 1) * 0.5 * VW, (1 - ndc[1]) * 0.5 * VH};
                const double n = c["query_size"].get<double>();
                const double ox = std::trunc(screen[0] - n / 2);
                const double oy = std::trunc(screen[1] - n / 2);
                auto clampTo = [&](double x, double limit) { return std::trunc(x >= 0 ? std::min(x, limit - n) : 0.0); };
                const float rows[8] = {float(ox), float(oy), float(n), float(n), float(clampTo(screen[0] - n / 2, VW)), float(clampTo(screen[1] - n / 2, VH)),
                                       float(-vp[2]), 1};
                wgpuQueueWriteBuffer(gpu_->queue, sunRows_, 0, rows, sizeof rows);
                const float qp[12] = {near_, far_, float(VW), float(VH), float(width), float(height), 1, 0, float(n), float(n), 0, 0};
                wgpuQueueWriteBuffer(gpu_->queue, sunParams_, 0, qp, sizeof qp);
                // sunSprites: the glow, then the nine lens quadrants (GS vertex bytes trunc(x x 128))
                auto byte = [](float x) { return std::trunc(float(x * 128.f)); };
                sprites[0] = 1;
                sprites[1] = texture == 1 ? 1.f : 0.f;
                sprites[2] = float(VW);
                sprites[3] = float(VH);
                auto put = [&](int k, double cx, double cy, double half, float alpha, std::array<float, 4> uv, std::array<float, 3> rgb) {
                    float* s = sprites + 4 + k * 12;
                    s[0] = float(cx);
                    s[1] = float(cy);
                    s[2] = float(half);
                    s[3] = alpha;
                    std::memcpy(s + 4, uv.data(), 16);
                    std::memcpy(s + 8, rgb.data(), 12);
                };
                const double half = v[8] == 1 ? c["default_glow_half_size"].get<double>() : double(v[8]);
                put(0, screen[0], screen[1], half, v[5], {0, 0, 1, 1}, {byte(v[2]), byte(v[3]), byte(v[4])});
                const double cx = VW * 0.5;
                const double cy = VH * 0.5;
                const std::array<float, 4> quad[4] = {{0, 0, .5f, .5f}, {.5f, 0, 1, .5f}, {0, .5f, .5f, 1}, {.5f, .5f, 1, 1}};
                int k = 1;
                for (const auto& e : sun_->package()["flares"]) {
                    const int q = e["quadrant"].get<int>();
                    const std::array<float, 4> uv = q >= 0 && q < 4 ? quad[q] : std::array<float, 4>{0, 0, .5f, 1};
                    const double pos = e["position"].get<double>();
                    const auto& argb = e["argb"];
                    put(k++, cx + (screen[0] - cx) * pos, cy + (screen[1] - cy) * pos, e["size"].get<double>() * c["flare_size_scale"].get<double>(),
                        float(argb[0].get<float>() * v[7]), uv, {byte(argb[1].get<float>()), byte(argb[2].get<float>()), byte(argb[3].get<float>())});
                }
            }
        }
    }
    // header word 0: the sun is in view (the composite adds its sprites)
    sprites[0] = sunVisible_ ? 1.f : 0.f;
    wgpuQueueWriteBuffer(gpu_->queue, sprites_, 0, sprites, sizeof sprites);
}

void PostEffects::encode(WGPUCommandEncoder enc) {
    auto pass = [&](WGPUTextureView target, bool clear) {
        WGPURenderPassColorAttachment ca = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        ca.view = target;
        ca.loadOp = clear ? WGPULoadOp_Clear : WGPULoadOp_Load;
        ca.storeOp = WGPUStoreOp_Store;
        ca.clearValue = {0, 0, 0, 0};
        WGPURenderPassDescriptor rd = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        rd.colorAttachmentCount = 1;
        rd.colorAttachments = &ca;
        return wgpuCommandEncoderBeginRenderPass(enc, &rd);
    };
    if (glowQueried_) {
        WGPURenderPassEncoder p = pass(glowVisView_, true);
        wgpuRenderPassEncoderSetPipeline(p, glowQuery_);
        wgpuRenderPassEncoderSetBindGroup(p, 0, glowGroup_, 0, nullptr);
        wgpuRenderPassEncoderDraw(p, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(p);
        wgpuRenderPassEncoderRelease(p);
    }
    if (sunVisible_) {
        WGPURenderPassEncoder p = pass(sunVisView_, true);
        wgpuRenderPassEncoderSetPipeline(p, sunQuery_);
        wgpuRenderPassEncoderSetBindGroup(p, 0, sunGroup_, 0, nullptr);
        wgpuRenderPassEncoderDraw(p, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(p);
        wgpuRenderPassEncoderRelease(p);
    }
    // the glow target, cleared every frame (0 where nothing glows)
    WGPURenderPassEncoder p = pass(glowTargetView_, true);
    if (glowVertexCount_) {
        wgpuRenderPassEncoderSetPipeline(p, glowDraw_);
        wgpuRenderPassEncoderSetBindGroup(p, 0, glowDrawGroup_, 0, nullptr);
        wgpuRenderPassEncoderSetVertexBuffer(p, 0, glowVertices_, 0, uint64_t(glowVertexCount_) * sizeof(GlowVertex));
        wgpuRenderPassEncoderDraw(p, glowVertexCount_, 1, 0, 0);
    }
    wgpuRenderPassEncoderEnd(p);
    wgpuRenderPassEncoderRelease(p);
}

}  // namespace ssx_native
