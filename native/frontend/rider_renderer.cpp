#include "rider_renderer.hpp"
#include "image_io.hpp"
#include "texture_archive.hpp"
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>

namespace ssx_native {

using nlohmann::json;

namespace {

std::vector<uint8_t> bytesOf(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw std::runtime_error("cannot read " + path);
    }
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}
json jsonOf(const std::string& path) {
    const auto b = bytesOf(path);
    return json::parse(std::string(reinterpret_cast<const char*>(b.data()), b.size()));
}

// web/rider-skinning.js, rider-lighting-nodes.js, rider-material.js as WGSL
const char* kRiderShader = R"(
struct Frame {
  viewProjection: mat4x4f,
  origin: vec4f,
  // x: the render alpha (previous -> current palette), y: lit, z: groups (the current palette's first row), w: texel scale
  params: vec4f,
  lighting: array<vec4f, 10>,
};
@group(0) @binding(0) var<uniform> frame: Frame;
@group(0) @binding(1) var<storage, read> palette: array<vec4f>;
@group(1) @binding(0) var mapTex: texture_2d<f32>;
@group(1) @binding(1) var mapSampler: sampler;
@group(1) @binding(2) var<uniform> material: vec4f;

fn encodeChop(sign: u32, mantissa: u32, exponent: i32) -> u32 {
  var result = sign;
  if (exponent >= 255) {
    result = sign | 0x7f7fffffu;
  } else if (exponent > 0) {
    result = sign | (u32(exponent) << 23u) | (mantissa & 0x7fffffu);
  }
  return result;
}
// VU ADD (chop, one guard bit), web/rider-lighting-nodes.js chopAdd
fn chopAdd(a: f32, b: f32) -> f32 {
  let ba = bitcast<u32>(a);
  let bb = bitcast<u32>(b);
  let mask = 0x7fffffffu;
  let swap = (ba & mask) < (bb & mask);
  let hi = select(ba, bb, swap);
  let lo = select(bb, ba, swap);
  let ea = (hi >> 23u) & 255u;
  let eb = (lo >> 23u) & 255u;
  let sign = hi & 0x80000000u;
  let different = ((hi ^ lo) & 0x80000000u) != 0u;
  var result = hi;
  if (ea == 0u) {
    result = (ba & bb) & 0x80000000u;
  } else if (eb != 0u) {
    let ma = (hi & 0x7fffffu) | 0x800000u;
    let mb = (lo & 0x7fffffu) | 0x800000u;
    let distance = ea - eb;
    let right = min(distance - 2u, 31u);
    var aligned = 0u;
    var sticky = 0u;
    if (distance <= 2u) {
      aligned = mb << (2u - distance);
    } else {
      aligned = mb >> right;
      sticky = select(select(0u, 1u, (mb & ((1u << right) - 1u)) != 0u), 1u, distance >= 34u);
    }
    let magnitude = select((ma << 2u) + aligned, (ma << 2u) - aligned - sticky, different);
    if (magnitude == 0u) {
      result = 0u;
    } else {
      let top = 31u - countLeadingZeros(magnitude);
      let shift = top - 23u;
      let mantissa = select(magnitude << (23u - top), magnitude >> shift, top >= 23u);
      result = encodeChop(sign, mantissa, i32(ea) + i32(top) - 25);
    }
  }
  return bitcast<f32>(result);
}
// VU MUL (chop), web/rider-lighting-nodes.js chopMul
fn chopMul(a: f32, b: f32) -> f32 {
  let ba = bitcast<u32>(a);
  let bb = bitcast<u32>(b);
  let ea = (ba >> 23u) & 255u;
  let eb = (bb >> 23u) & 255u;
  let sign = (ba ^ bb) & 0x80000000u;
  var result = sign;
  if (ea != 0u && eb != 0u) {
    let ma = (ba & 0x7fffffu) | 0x800000u;
    let mb = (bb & 0x7fffffu) | 0x800000u;
    let al = ma & 65535u;
    let bl = mb & 65535u;
    let ah = ma >> 16u;
    let bh = mb >> 16u;
    let p0 = al * bl;
    let p1 = ah * bl;
    let p2 = al * bh;
    let middle = (p0 >> 16u) + (p1 & 65535u) + (p2 & 65535u);
    let low = (p0 & 65535u) | ((middle & 65535u) << 16u);
    let high = ah * bh + (p1 >> 16u) + (p2 >> 16u) + (middle >> 16u);
    let extra = select(0u, 1u, high >= 32768u);
    let shift = 23u + extra;
    let mantissa = (high << (32u - shift)) | (low >> shift);
    var exponent = i32(ea) + i32(eb) - 127;
    if (high >= 32768u) {
      exponent += 1;
    }
    result = encodeChop(sign, mantissa, exponent);
  }
  return bitcast<f32>(result);
}
fn column(group: u32, row: u32, c: u32) -> vec4f {
  return palette[(group + row) * 4u + c];
}
// Program2 upper 04C0..04D8 (riderTransformPositionNode)
fn transformPosition(group: u32, row: u32, p: vec4f) -> vec4f {
  var out: vec4f;
  for (var r = 0u; r < 4u; r++) {
    var value = chopMul(column(group, row, 0u)[r], p.x);
    for (var c = 1u; c < 4u; c++) {
      value = chopAdd(value, chopMul(column(group, row, c)[r], p[c]));
    }
    out[r] = value;
  }
  return out;
}
// Program2 ITOF15 +04E0..04F0 (riderTransformNormalNode)
fn transformNormal(group: u32, row: u32, packed: vec3f) -> vec3f {
  let decoded = vec3f(chopMul(packed.x, 1.0 / 32768.0), chopMul(packed.y, 1.0 / 32768.0), chopMul(packed.z, 1.0 / 32768.0));
  var out: vec3f;
  for (var r = 0u; r < 3u; r++) {
    var value = chopMul(column(group, row, 0u)[r], decoded.x);
    value = chopAdd(value, chopMul(column(group, row, 1u)[r], decoded.y));
    value = chopAdd(value, chopMul(column(group, row, 2u)[r], decoded.z));
    out[r] = value;
  }
  return out;
}
// riderIrradianceNode: the ten rows on the transformed normal, quantized
fn irradiance(n: vec3f) -> vec4f {
  let x = n.x;
  let y = n.y;
  let z = n.z;
  let w = array<f32, 9>(chopMul(x, x), chopMul(y, y), chopMul(z, z), chopMul(x, y), chopMul(z, x), chopMul(y, z), x, y, z);
  var out: vec4f;
  for (var lane = 0u; lane < 4u; lane++) {
    var value = frame.lighting[0][lane];
    for (var i = 1u; i < 10u; i++) {
      value = chopAdd(value, chopMul(frame.lighting[i][lane], w[i - 1u]));
    }
    out[lane] = floor(clamp(value, 0.0, 255.0));
  }
  return out;
}

struct In {
  @location(0) position: vec3f,
  @location(1) normal: vec3f,
  @location(2) uv: vec2f,
  @location(3) group: u32,
};
struct Out {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
  @location(1) lighting: vec4f,
};
@vertex fn vs(v: In) -> Out {
  let groups = u32(frame.params.z);
  let point = vec4f(v.position, 1.0);
  let world = mix(transformPosition(v.group, 0u, point), transformPosition(v.group, groups, point), frame.params.x);
  let o = frame.origin.xyz;
  let relative = vec4f(world.x * 0.01 - o.x * world.w, world.z * 0.01 - o.y * world.w, world.y * -0.01 - o.z * world.w, world.w);
  var out: Out;
  out.position = frame.viewProjection * relative;
  out.uv = v.uv;
  let packed = v.normal * 32768.0;
  let n = mix(transformNormal(v.group, 0u, packed), transformNormal(v.group, groups, packed), frame.params.x);
  out.lighting = irradiance(n);
  return out;
}
// GS HIGHLIGHT2 on the PS2-domain texels (riderHighlight2Node); unlit: the texels doubled
fn shade(i: Out) -> vec4f {
  let sample = textureSample(mapTex, mapSampler, i.uv);
  let texel = vec4f(sample.rgb * material.x, sample.a * 255.0);
  if (frame.params.y > 0.0) {
    let rgb = clamp(floor(texel.rgb * i.lighting.rgb / 128.0) + i.lighting.a, vec3f(0.0), vec3f(255.0));
    return vec4f(rgb / 255.0, texel.a / 255.0);
  }
  return vec4f(min(texel.rgb * (2.0 / 255.0), vec3f(1.0)), sample.a);
}
@fragment fn opaque(i: Out) -> @location(0) vec4f {
  return vec4f(shade(i).rgb, 1.0);
}
// riderDrawState: the texels above AREF 92 blended with depth writes (alphaTest 185 / 255), then the others without
@fragment fn high(i: Out) -> @location(0) vec4f {
  let c = shade(i);
  if (c.a <= 185.0 / 255.0) { discard; }
  return c;
}
@fragment fn low(i: Out) -> @location(0) vec4f {
  let c = shade(i);
  return vec4f(c.rgb, select(c.a, 0.0, c.a > 184.5 / 255.0));
}
// web/rider-shadow.js: the silhouette into the rider's 128 x 128 slot of the shadow atlas, the parts in painter's order by depth
// (0.8 before the board, 0.5 the board and bindings, 0.2 after), PSMCT16 38 -> 32 (the board 0 while grounded), 128 x 128 scissor
struct Shadow { rowX: vec4f, rowY: vec4f, misc: vec4f };
@group(1) @binding(0) var<uniform> shadow: Shadow;
struct ShadowIn {
  @location(0) position: vec3f,
  @location(3) group: u32,
  @location(4) rank: u32,
};
struct ShadowOut {
  @builtin(position) position: vec4f,
  @location(0) ndc: vec2f,
  @location(1) rank: f32,
};
@vertex fn vsShadow(v: ShadowIn) -> ShadowOut {
  let groups = u32(frame.params.z);
  let point = vec4f(v.position, 1.0);
  let world = mix(transformPosition(v.group, 0u, point), transformPosition(v.group, groups, point), frame.params.x);
  let p = vec4f(world.xyz / world.w, 1.0);
  let n = vec2f(dot(shadow.rowX, p), dot(shadow.rowY, p));
  let r = f32(v.rank);
  var o: ShadowOut;
  o.position = vec4f((shadow.misc.x + (n.x * 0.5 + 0.5)) / 6.0 * 2.0 - 1.0, -n.y, 0.8 - r * 0.3, 1.0);
  o.ndc = n;
  o.rank = r;
  return o;
}
@fragment fn fsShadow(i: ShadowOut) -> @location(0) vec4f {
  if (abs(i.ndc.x) > 1.0 || abs(i.ndc.y) > 1.0) { discard; }
  let v = select(32.0 / 255.0, shadow.misc.y, i.rank == 1.0);
  return vec4f(v, v, v, 1.0);
}
)";

}  // namespace

void RiderRenderer::load(Gpu& gpu, const std::string& assetsRoot, const std::string& package, const uint32_t* paletteIndices, int groupCount) {
    gpu_ = &gpu;
    groups_ = groupCount;
    const std::string root = assetsRoot + "/" + package + "/";
    const json d = jsonOf(root + "world.json");
    const json rig = jsonOf(root + "rider.json");
    const auto vb = bytesOf(root + "vertices.bin");
    const auto ib = bytesOf(root + "indices.bin");
    const size_t count = vb.size() / 40;
    std::vector<float> v(count * 10);
    std::memcpy(v.data(), vb.data(), v.size() * 4);
    std::vector<uint32_t> indices(ib.size() / 4);
    std::memcpy(indices.data(), ib.data(), indices.size() * 4);
    const bool quantized = rig.value("source_normal_quantization", 0) == 32768;
    // sourcePosition (PS2 cm, Z up) and sourceNormal from the unscaled package vertices; uv; the palette group
    struct Vertex {
        float position[3];
        float normal[3];
        float uv[2];
        uint32_t group;
        uint32_t pad;
    };
    std::vector<Vertex> vertices(count);
    for (size_t i = 0; i < count; ++i) {
        Vertex& o = vertices[i];
        o.position[0] = v[i * 10] * 100.f;
        o.position[1] = -v[i * 10 + 2] * 100.f;
        o.position[2] = v[i * 10 + 1] * 100.f;
        float n[3] = {v[i * 10 + 3], -v[i * 10 + 5], v[i * 10 + 4]};
        if (quantized) {
            for (float& c : n) {
                c = float(std::max(-32768.0, std::min(32767.0, std::round(double(c) * 32768))) / 32768);
            }
        }
        std::memcpy(o.normal, n, sizeof n);
        o.uv[0] = v[i * 10 + 6];
        o.uv[1] = v[i * 10 + 7];
        o.group = paletteIndices[i];
        o.pad = 0;
    }
    // web/main.js asset(): a rider's batches merged by texture:lightmap, in first-appearance order
    struct Merged {
        json batch;
        std::vector<uint32_t> indices;
    };
    std::vector<std::string> order;
    std::map<std::string, Merged> by;
    for (const auto& b : d["batches"]) {
        const std::string key = b["texture"].dump() + ":" + b["lightmap"].dump();
        if (!by.count(key)) {
            order.push_back(key);
            by[key].batch = b;
        }
        const uint32_t first = b["first_index"].get<uint32_t>();
        const uint32_t n = b["index_count"].get<uint32_t>();
        auto& m = by[key].indices;
        m.insert(m.end(), indices.begin() + first, indices.begin() + first + n);
    }
    // web/rider-shadow.js entryFor: each vertex's painter's rank from the original part order (before the board 0, the board and
    // bindings ('bord') 1, after it 2)
    {
        std::vector<uint32_t> rank(count, 2);
        bool seenBoard = false;
        for (const auto& b : d["batches"]) {
            const bool board = b.value("material", "") == "bord";
            const uint32_t r = board ? 1 : (seenBoard ? 2 : 0);
            if (board) {
                seenBoard = true;
            }
            const uint32_t first = b["first_index"].get<uint32_t>();
            const uint32_t n = b["index_count"].get<uint32_t>();
            for (uint32_t k = first; k < first + n; ++k) {
                rank[indices[k]] = r;
            }
        }
        for (size_t i = 0; i < count; ++i) {
            vertices[i].pad = rank[i];
        }
    }
    std::vector<uint32_t> merged;
    WGPUDevice dev = gpu.device;
    // layouts
    WGPUBindGroupLayoutEntry fe[2];
    fe[0] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    fe[0].binding = 0;
    fe[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    fe[0].buffer.type = WGPUBufferBindingType_Uniform;
    fe[1] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    fe[1].binding = 1;
    fe[1].visibility = WGPUShaderStage_Vertex;
    fe[1].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    WGPUBindGroupLayoutDescriptor fd = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    fd.entryCount = 2;
    fd.entries = fe;
    frameLayout_ = wgpuDeviceCreateBindGroupLayout(dev, &fd);
    WGPUBindGroupLayoutEntry me[3];
    for (int k = 0; k < 3; ++k) {
        me[k] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
        me[k].binding = uint32_t(k);
        me[k].visibility = WGPUShaderStage_Fragment;
    }
    me[0].texture.sampleType = WGPUTextureSampleType_Float;
    me[0].texture.viewDimension = WGPUTextureViewDimension_2D;
    me[1].sampler.type = WGPUSamplerBindingType_Filtering;
    me[2].buffer.type = WGPUBufferBindingType_Uniform;
    WGPUBindGroupLayoutDescriptor md = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    md.entryCount = 3;
    md.entries = me;
    materialLayout_ = wgpuDeviceCreateBindGroupLayout(dev, &md);
    WGPUSamplerDescriptor sd = WGPU_SAMPLER_DESCRIPTOR_INIT;
    sd.magFilter = WGPUFilterMode_Linear;
    sd.minFilter = WGPUFilterMode_Linear;
    sd.mipmapFilter = WGPUMipmapFilterMode_Linear;
    sd.maxAnisotropy = 4;
    sd.addressModeU = sd.addressModeV = WGPUAddressMode_Repeat;
    sampler_ = wgpuDeviceCreateSampler(dev, &sd);
    // textures: PS2-domain texels (128 = 1.0) scaled by 255, others by 127.5 (rider-material.js)
    static std::map<std::string, std::unique_ptr<TextureArchive>> archives;
    const json& textures = d["textures"];
    for (const auto& key : order) {
        auto& m = by[key];
        Batch batch;
        batch.first = uint32_t(merged.size());
        batch.count = uint32_t(m.indices.size());
        merged.insert(merged.end(), m.indices.begin(), m.indices.end());
        const std::string name = m.batch.value("material", "");
        batch.blended = name == "alph" || name.rfind("ea", 0) == 0;
        const std::string tkey = "9-" + m.batch["texture"].dump();
        const json& entry = textures.at(tkey);
        std::string pack = entry["pack"].get<std::string>();
        const std::string path = assetsRoot + "/" + (pack.rfind("/assets/", 0) == 0 ? pack.substr(8) : pack);
        auto& a = archives[path];
        if (!a) {
            a = std::make_unique<TextureArchive>(path);
        }
        const Image img = a->decode(entry["id"]);
        // mips as the world textures (2 x 2 box)
        uint32_t levels = 1;
        for (uint32_t s = uint32_t(std::max(img.width, img.height)); s > 1; s >>= 1) {
            ++levels;
        }
        WGPUTexture tex = gpu.texture(uint32_t(img.width), uint32_t(img.height), WGPUTextureFormat_RGBA8Unorm,
                                      WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst, levels);
        std::vector<uint8_t> level = img.rgba;
        uint32_t lw = uint32_t(img.width);
        uint32_t lh = uint32_t(img.height);
        gpu.writeTexture(tex, lw, lh, level.data(), 0);
        for (uint32_t k = 1; k < levels; ++k) {
            const uint32_t nw = std::max(1u, lw / 2);
            const uint32_t nh = std::max(1u, lh / 2);
            std::vector<uint8_t> next(size_t(nw) * nh * 4);
            for (uint32_t y = 0; y < nh; ++y) {
                for (uint32_t x = 0; x < nw; ++x) {
                    for (int c = 0; c < 4; ++c) {
                        const uint32_t x1 = std::min(lw - 1, 2 * x + 1);
                        const uint32_t y1 = std::min(lh - 1, 2 * y + 1);
                        const uint32_t s = uint32_t(level[(size_t(2 * y) * lw + 2 * x) * 4 + c]) + level[(size_t(2 * y) * lw + x1) * 4 + c] +
                                           level[(size_t(y1) * lw + 2 * x) * 4 + c] + level[(size_t(y1) * lw + x1) * 4 + c];
                        next[(size_t(y) * nw + x) * 4 + c] = uint8_t((s + 2) / 4);
                    }
                }
            }
            gpu.writeTexture(tex, nw, nh, next.data(), k);
            level.swap(next);
            lw = nw;
            lh = nh;
        }
        WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
        WGPUTextureView view = wgpuTextureCreateView(tex, &vd);
        const std::string domain = entry.value("texel_domain", "");
        const float scale[4] = {domain == "ps2" ? 255.f : 127.5f, 0, 0, 0};
        WGPUBuffer mu = gpu.buffer(16, WGPUBufferUsage_Uniform, scale, "rider material");
        WGPUBindGroupEntry e[3];
        for (int k = 0; k < 3; ++k) {
            e[k] = WGPU_BIND_GROUP_ENTRY_INIT;
            e[k].binding = uint32_t(k);
        }
        e[0].textureView = view;
        e[1].sampler = sampler_;
        e[2].buffer = mu;
        e[2].size = 16;
        WGPUBindGroupDescriptor bd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        bd.layout = materialLayout_;
        bd.entryCount = 3;
        bd.entries = e;
        batch.material = wgpuDeviceCreateBindGroup(dev, &bd);
        batches_.push_back(batch);
    }
    vertices_ = gpu.buffer(vertices.size() * sizeof(Vertex), WGPUBufferUsage_Vertex, vertices.data(), "rider vertices");
    indices_ = gpu.buffer(merged.size() * 4, WGPUBufferUsage_Index, merged.data(), "rider indices");
    uniform_ = gpu.buffer(256, WGPUBufferUsage_Uniform, nullptr, "rider frame");
    palette_ = gpu.buffer(uint64_t(groupCount) * 2 * 64, WGPUBufferUsage_Storage, nullptr, "rider palette");
    WGPUBindGroupEntry e[2];
    e[0] = WGPU_BIND_GROUP_ENTRY_INIT;
    e[0].binding = 0;
    e[0].buffer = uniform_;
    e[0].size = 256;
    e[1] = WGPU_BIND_GROUP_ENTRY_INIT;
    e[1].binding = 1;
    e[1].buffer = palette_;
    e[1].size = uint64_t(groupCount) * 2 * 64;
    WGPUBindGroupDescriptor bd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bd.layout = frameLayout_;
    bd.entryCount = 2;
    bd.entries = e;
    frame_ = wgpuDeviceCreateBindGroup(dev, &bd);

    // pipelines
    WGPUBindGroupLayout layouts[2] = {frameLayout_, materialLayout_};
    WGPUPipelineLayoutDescriptor pld = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pld.bindGroupLayoutCount = 2;
    pld.bindGroupLayouts = layouts;
    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(dev, &pld);
    WGPUShaderModule module = gpu.shader(kRiderShader, "rider");
    WGPUVertexAttribute attrs[4] = {
        {nullptr, WGPUVertexFormat_Float32x3, 0, 0},
        {nullptr, WGPUVertexFormat_Float32x3, 12, 1},
        {nullptr, WGPUVertexFormat_Float32x2, 24, 2},
        {nullptr, WGPUVertexFormat_Uint32, 32, 3},
    };
    WGPUVertexBufferLayout vbl = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
    vbl.arrayStride = sizeof(Vertex);
    vbl.stepMode = WGPUVertexStepMode_Vertex;
    vbl.attributeCount = 4;
    vbl.attributes = attrs;
    WGPUBlendState normal{};
    normal.color = {WGPUBlendOperation_Add, WGPUBlendFactor_SrcAlpha, WGPUBlendFactor_OneMinusSrcAlpha};
    normal.alpha = {WGPUBlendOperation_Add, WGPUBlendFactor_One, WGPUBlendFactor_OneMinusSrcAlpha};
    const char* entries[3] = {"opaque", "high", "low"};
    for (int kind = 0; kind < 3; ++kind) {
        for (int side = 0; side < (kind == 0 ? 1 : 2); ++side) {
            WGPURenderPipelineDescriptor pd = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
            pd.layout = pipelineLayout;
            pd.vertex.module = module;
            pd.vertex.entryPoint = sv("vs");
            pd.vertex.bufferCount = 1;
            pd.vertex.buffers = &vbl;
            pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
            pd.primitive.frontFace = WGPUFrontFace_CCW;
            pd.primitive.cullMode = kind == 0 ? WGPUCullMode_None : (side == 0 ? WGPUCullMode_Front : WGPUCullMode_Back);
            WGPUDepthStencilState ds = WGPU_DEPTH_STENCIL_STATE_INIT;
            ds.format = WGPUTextureFormat_Depth24PlusStencil8;
            ds.depthWriteEnabled = kind == 2 ? WGPUOptionalBool_False : WGPUOptionalBool_True;
            ds.depthCompare = WGPUCompareFunction_LessEqual;
            pd.depthStencil = &ds;
            WGPUColorTargetState ct = WGPU_COLOR_TARGET_STATE_INIT;
            ct.format = WGPUTextureFormat_RGBA8Unorm;
            ct.blend = kind == 0 ? nullptr : &normal;
            ct.writeMask = WGPUColorWriteMask_All;
            WGPUFragmentState fs = WGPU_FRAGMENT_STATE_INIT;
            fs.module = module;
            fs.entryPoint = sv(entries[kind]);
            fs.targetCount = 1;
            fs.targets = &ct;
            pd.fragment = &fs;
            pd.multisample.count = 1;
            pd.multisample.mask = 0xffffffffu;
            pipelines_[kind][side] = wgpuDeviceCreateRenderPipeline(dev, &pd);
        }
    }
    // the shadow silhouette (group 1: its rows)
    {
        WGPUBindGroupLayoutEntry se = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
        se.binding = 0;
        se.visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
        se.buffer.type = WGPUBufferBindingType_Uniform;
        WGPUBindGroupLayoutDescriptor sld = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
        sld.entryCount = 1;
        sld.entries = &se;
        WGPUBindGroupLayout shadowLayout = wgpuDeviceCreateBindGroupLayout(dev, &sld);
        const WGPUBindGroupLayout sl[2] = {frameLayout_, shadowLayout};
        WGPUPipelineLayoutDescriptor spl = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
        spl.bindGroupLayoutCount = 2;
        spl.bindGroupLayouts = sl;
        WGPUPipelineLayout shadowPipelineLayout = wgpuDeviceCreatePipelineLayout(dev, &spl);
        WGPUVertexAttribute sa[3] = {
            {nullptr, WGPUVertexFormat_Float32x3, 0, 0},
            {nullptr, WGPUVertexFormat_Uint32, 32, 3},
            {nullptr, WGPUVertexFormat_Uint32, 36, 4},
        };
        WGPUVertexBufferLayout svl = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
        svl.arrayStride = sizeof(Vertex);
        svl.stepMode = WGPUVertexStepMode_Vertex;
        svl.attributeCount = 3;
        svl.attributes = sa;
        WGPURenderPipelineDescriptor pd = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        pd.layout = shadowPipelineLayout;
        pd.vertex.module = module;
        pd.vertex.entryPoint = sv("vsShadow");
        pd.vertex.bufferCount = 1;
        pd.vertex.buffers = &svl;
        pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        pd.primitive.cullMode = WGPUCullMode_None;
        WGPUDepthStencilState ds = WGPU_DEPTH_STENCIL_STATE_INIT;
        ds.format = WGPUTextureFormat_Depth24Plus;
        ds.depthWriteEnabled = WGPUOptionalBool_True;
        ds.depthCompare = WGPUCompareFunction_LessEqual;
        pd.depthStencil = &ds;
        WGPUColorTargetState ct = WGPU_COLOR_TARGET_STATE_INIT;
        ct.format = WGPUTextureFormat_RGBA8Unorm;
        ct.writeMask = WGPUColorWriteMask_All;
        WGPUFragmentState fs = WGPU_FRAGMENT_STATE_INIT;
        fs.module = module;
        fs.entryPoint = sv("fsShadow");
        fs.targetCount = 1;
        fs.targets = &ct;
        pd.fragment = &fs;
        pd.multisample.count = 1;
        pd.multisample.mask = 0xffffffffu;
        shadowPipeline_ = wgpuDeviceCreateRenderPipeline(dev, &pd);
        shadowUniform_ = gpu.buffer(48, WGPUBufferUsage_Uniform, nullptr, "rider shadow");
        WGPUBindGroupEntry be = WGPU_BIND_GROUP_ENTRY_INIT;
        be.binding = 0;
        be.buffer = shadowUniform_;
        be.size = 48;
        WGPUBindGroupDescriptor sbd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        sbd.layout = shadowLayout;
        sbd.entryCount = 1;
        sbd.entries = &be;
        shadowGroup_ = wgpuDeviceCreateBindGroup(dev, &sbd);
        indexCount_ = uint32_t(merged.size());
    }
}

void RiderRenderer::drawShadow(WGPURenderPassEncoder pass, const ShadowSlot& slot) {
    const float u[12] = {slot.rowX[0], slot.rowX[1], slot.rowX[2], slot.rowX[3], slot.rowY[0], slot.rowY[1], slot.rowY[2], slot.rowY[3],
                         slot.slot, slot.board, 0, 0};
    wgpuQueueWriteBuffer(gpu_->queue, shadowUniform_, 0, u, sizeof u);
    wgpuRenderPassEncoderSetPipeline(pass, shadowPipeline_);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, frame_, 0, nullptr);
    wgpuRenderPassEncoderSetBindGroup(pass, 1, shadowGroup_, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertices_, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass, indices_, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderDrawIndexed(pass, indexCount_, 1, 0, 0, 0);
}

void RiderRenderer::update(const std::array<float, 16>& viewProjection, const std::array<double, 3>& origin, const RiderFrame& frame) {
    float u[64] = {};
    std::memcpy(u, viewProjection.data(), 64);
    u[16] = float(origin[0]);
    u[17] = float(origin[1]);
    u[18] = float(origin[2]);
    u[20] = frame.alpha;
    u[21] = frame.lit ? 1.f : 0.f;
    u[22] = float(groups_);
    std::memcpy(u + 24, frame.lighting.data(), 160);
    wgpuQueueWriteBuffer(gpu_->queue, uniform_, 0, u, sizeof u);
    const size_t bytes = size_t(groups_) * 64;
    if (frame.previous.size() * 4 >= bytes && frame.current.size() * 4 >= bytes) {
        wgpuQueueWriteBuffer(gpu_->queue, palette_, 0, frame.previous.data(), bytes);
        wgpuQueueWriteBuffer(gpu_->queue, palette_, bytes, frame.current.data(), bytes);
    }
}

void RiderRenderer::drawOpaque(WGPURenderPassEncoder pass) {
    wgpuRenderPassEncoderSetBindGroup(pass, 0, frame_, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertices_, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass, indices_, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetPipeline(pass, pipelines_[0][0]);
    for (const auto& b : batches_) {
        if (b.blended) {
            continue;
        }
        wgpuRenderPassEncoderSetBindGroup(pass, 1, b.material, 0, nullptr);
        wgpuRenderPassEncoderDrawIndexed(pass, b.count, 1, b.first, 0, 0);
    }
}

void RiderRenderer::drawTransparent(WGPURenderPassEncoder pass, int side) {
    wgpuRenderPassEncoderSetBindGroup(pass, 0, frame_, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, vertices_, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass, indices_, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
    // the high pass then the low pass of each blended batch (one mesh, two material groups)
    for (const auto& b : batches_) {
        if (!b.blended) {
            continue;
        }
        wgpuRenderPassEncoderSetBindGroup(pass, 1, b.material, 0, nullptr);
        for (int kind = 1; kind <= 2; ++kind) {
            wgpuRenderPassEncoderSetPipeline(pass, pipelines_[kind][side]);
            wgpuRenderPassEncoderDrawIndexed(pass, b.count, 1, b.first, 0, 0);
        }
    }
}

}  // namespace ssx_native
