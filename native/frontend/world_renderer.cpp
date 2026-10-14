#include "world_renderer.hpp"
#include "set_piece_fx.hpp"
#include "terrain_sparkle.hpp"
#include "rider_shadow.hpp"
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
std::vector<uint8_t> base64(const std::string& text) {
    std::vector<uint8_t> out;
    uint32_t acc = 0;
    int bits = 0;
    for (char c : text) {
        int v;
        if (c >= 'A' && c <= 'Z') {
            v = c - 'A';
        } else if (c >= 'a' && c <= 'z') {
            v = c - 'a' + 26;
        } else if (c >= '0' && c <= '9') {
            v = c - '0' + 52;
        } else if (c == '+') {
            v = 62;
        } else if (c == '/') {
            v = 63;
        } else {
            continue;
        }
        acc = (acc << 6) | uint32_t(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(uint8_t((acc >> bits) & 0xff));
        }
    }
    return out;
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
// the object records (first instance): the mesh's projection x view x matrixWorld and its UVScroll translation (static records:
// index = scroll group + 1, 0 none; moved / copied batches after them)
struct Object { mvp: mat4x4f, scroll: vec2f, pad: vec2f };
@group(0) @binding(1) var<storage, read> objects: array<Object>;
// the terrain's frame values: the camera (native metres, not origin-relative), the origin, the rider shadows (web/rider-shadow.js:
// x the slot count; per slot the u, v and depth-below-feet rows over the origin-relative scene position)
struct Frame { camera: vec4f, origin: vec4f, shadow: vec4f, rows: array<vec4f, 18> };
@group(0) @binding(0) var<uniform> frame: Frame;
@group(0) @binding(2) var shadowAtlas: texture_2d<f32>;
@group(0) @binding(3) var clampSampler: sampler;
@group(0) @binding(4) var glintGrey: texture_2d<f32>;
@group(0) @binding(5) var glintBlue: texture_2d<f32>;
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
  // terrain-glint.json: the patch's glint image (1 grey, 2 blue, 0 none)
  @location(7) glint: f32,
};
struct Out {
  @builtin(position) position: vec4f,
  @location(0) uv: vec2f,
  @location(1) color: vec4f,
  @location(2) lightUv: vec2f,
  @location(3) vertexAlpha: f32,
  @location(4) world: vec3f,
  @location(5) glintSt: vec2f,
  @location(6) glint: f32,
};
fn modTau(x: f32) -> f32 {
  let tau = 6.283185307179586;
  return x - tau * floor(x / tau);
}
@vertex fn vs(v: In, @builtin(instance_index) instance: u32) -> Out {
  var o: Out;
  let ob = objects[instance];
  o.position = ob.mvp * vec4f(v.position, 1.0);
  o.uv = v.uv + ob.scroll;
  o.color = v.color;
  o.lightUv = v.lightUv;
  o.vertexAlpha = v.vertexAlpha;
  // the terrain's scene position (its meshes at -origin)
  o.world = v.position - frame.origin.xyz;
  // web/world-material.js glintBytes: UV = (n, 1) x E from the package normal and the camera (source axes)
  let n = vec3f(-v.normal.x, v.normal.z, -v.normal.y);
  let cam = frame.camera.xyz;
  let a = modTau(cam.z * 0.05);
  let b = modTau(cam.x * -0.05);
  let sa = sin(a);
  let ca = cos(a);
  let sb = sin(b);
  let cb = cos(b);
  o.glintSt = vec2f((n.x * sb + n.z * cb) * 0.5 + 0.5, 0.5 - (n.y * sa + n.x * cb * ca - n.z * sb * ca) * 0.5);
  o.glint = v.glint;
  return o;
}
// web/rider-shadow.js riderShadowReceiver: max(bytes - the sum of the shadow texels, 0), each rider's within its box and the 30 m
// slab below its feet (the GS CLAMP to its own texture)
fn riderShadows(bytes: vec3f, world: vec3f) -> vec3f {
  let p = vec4f(world, 1.0);
  var sum = 0.0;
  for (var i = 0; i < i32(frame.shadow.x); i++) {
    let u = dot(frame.rows[i * 3], p);
    let v = dot(frame.rows[i * 3 + 1], p);
    let lz = dot(frame.rows[i * 3 + 2], p);
    let t = clamp(vec2f(u, v), vec2f(0.5 / 128.0), vec2f(1.0 - 0.5 / 128.0));
    let s = round(textureSampleLevel(shadowAtlas, clampSampler, vec2f((t.x + f32(i)) / 6.0, t.y), 0.0).r * 255.0);
    let inside = u >= 0.0 && u <= 1.0 && v >= 0.0 && v <= 1.0;
    sum += select(0.0, s, lz <= 3000.0 && inside);
  }
  return max(bytes - sum, vec3f(0.0));
}
fn frameBytes(bytes: vec3f) -> vec3f { return clamp(bytes, vec3f(0.0), vec3f(255.0)) / 255.0; }
// terrain: ctx1 base texture, ctx2 PS2 light page ALPHA 0x81 (Cd - Cl) * Al >> 7
@fragment fn terrain(i: Out) -> @location(0) vec4f {
  let base = round(textureSample(mapTex, mapSampler, i.uv) * 255.0);
  let light = round(textureSample(atlasTex, atlasSampler, i.lightUv) * 255.0);
  let lit0 = clamp(floor((base.rgb - light.rgb) * light.a / 128.0), vec3f(0.0), vec3f(255.0));
  // the glint pass 38D168: Cd + Cs x Ad >> 7 (Ad the base texel's alpha, 128 for 255), clamped
  var lit = lit0;
  if (i.glint > 0.5) {
    let grey = textureSampleLevel(glintGrey, clampSampler, i.glintSt, 0.0);
    let blue = textureSampleLevel(glintBlue, clampSampler, i.glintSt, 0.0);
    let ct = round(select(grey, blue, i.glint > 1.5).rgb * 255.0);
    let ad = select(base.a * 0.5, 128.0, base.a >= 255.0);
    lit = clamp(lit0 + floor(ct * ad / 128.0), vec3f(0.0), vec3f(255.0));
  }
  return vec4f(frameBytes(riderShadows(lit, i.world)), 1.0);
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
// the fogged world with the rider effects (the fog pass binds a stand-in here)
@group(0) @binding(5) var foggedTex: texture_2d<f32>;
// post_effects.hpp: the light glow target, the sun sprites (a header, then ten sprites of three rows: centre / half / alpha, uv, rgb),
// the sun textures, the sun's visibility
struct Sprite { place: vec4f, uv: vec4f, rgb: vec4f };
struct Sprites { header: vec4f, sprite: array<Sprite, 10> };
@group(1) @binding(0) var glowTex: texture_2d<f32>;
@group(1) @binding(1) var<uniform> sprites: Sprites;
@group(1) @binding(2) var sun1: texture_2d<f32>;
@group(1) @binding(3) var sun2: texture_2d<f32>;
@group(1) @binding(4) var lens: texture_2d<f32>;
@group(1) @binding(5) var spriteSampler: sampler;
@group(1) @binding(6) var sunVis: texture_2d<f32>;
@vertex fn vs(@builtin(vertex_index) i: u32) -> @builtin(position) vec4f {
  let p = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  return vec4f(p[i], 0.0, 1.0);
}
// web/sun-flare.js spriteAdd: the GS texture as bytes, Cs = T x Vc >> 7, As = Ta x Va >> 7, add = Cs x As >> 7
fn spriteAdd(t0: vec4f, p: vec2f, s: Sprite, va: f32) -> vec3f {
  let local = (p - (s.place.xy - s.place.z)) / (s.place.z * 2.0);
  let inside = local.x >= 0.0 && local.x < 1.0 && local.y >= 0.0 && local.y < 1.0;
  let t = round(t0 * 255.0);
  let cs = min(floor(t.rgb * s.rgb.rgb / 128.0), vec3f(255.0));
  let as_ = floor(t.a * va / 128.0);
  return select(vec3f(0.0), floor(cs * as_ / 128.0), inside);
}
fn spriteUv(p: vec2f, s: Sprite) -> vec2f {
  let local = (p - (s.place.xy - s.place.z)) / (s.place.z * 2.0);
  return vec2f(s.uv.x + local.x * (s.uv.z - s.uv.x), s.uv.y + local.y * (s.uv.w - s.uv.y));
}
fn sunAdd(frag: vec2f, size: vec2f) -> vec3f {
  let screenUv = frag / size;
  let p = vec2f(screenUv.x * sprites.header.z, screenUv.y * sprites.header.w);
  let visibility = textureLoad(sunVis, vec2i(0, 0), 0).r;
  let glow = sprites.sprite[0];
  let glowUv = spriteUv(p, glow);
  let a = textureSampleLevel(sun1, spriteSampler, glowUv, 0.0);
  let b = textureSampleLevel(sun2, spriteSampler, glowUv, 0.0);
  var sum = spriteAdd(select(a, b, sprites.header.y > 0.5), p, glow, floor(glow.place.w * visibility * 128.0));
  for (var k = 1; k < 10; k++) {
    let s = sprites.sprite[k];
    let t = textureSampleLevel(lens, spriteSampler, spriteUv(p, s), 0.0);
    sum += spriteAdd(t, p, s, floor(s.place.w * visibility * 128.0));
  }
  return sum;
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
  return vec4f(out / 255.0, 1.0);
}
// after the encoded pass (rider effects drawn onto the fogged world, web/snow-composite.js)
@fragment fn finish(@builtin(position) p: vec4f) -> @location(0) vec4f {
  let xy = vec2i(p.xy);
  var out = round(textureLoad(foggedTex, xy, 0).rgb * 255.0);
  // web/fog-renderer.js: the light glow halos (priority 7), then the sun glow and lens flare (priority 8), then ScreenTint
  out = min(out + textureLoad(glowTex, xy, 0).rgb, vec3f(255.0));
  if (sprites.header.x > 0.5) {
    out = min(out + sunAdd(p.xy, vec2f(textureDimensions(worldTex))), vec3f(255.0));
  }
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

// web/set-piece-cables.js cableMaterial: each sub-line a screen-space quad one PS2 pixel thick (at least one device pixel), dropped
// with an end outside the guard band or the near .. far range (the VU's ADC); black, opaque, depth tested and written
const char* kCableShader = R"(
struct Cable { view: mat4x4f, projection: mat4x4f, near: f32, far: f32, vw: f32, vh: f32 };
@group(0) @binding(0) var<uniform> u: Cable;
struct In {
  @location(0) own: vec3f,
  @location(1) other: vec3f,
  @location(2) corner: vec2f,
};
fn outside(clip: vec4f, view: vec4f) -> bool {
  return abs(clip.x) > clip.w * 4.0 || abs(clip.y) > clip.w * (2048.0 / 448.0) || -view.z < u.near || -view.z > u.far;
}
@vertex fn vs(v: In) -> @builtin(position) vec4f {
  let viewOwn = u.view * vec4f(v.own, 1.0);
  let viewOther = u.view * vec4f(v.other, 1.0);
  let clipOwn = u.projection * viewOwn;
  let clipOther = u.projection * viewOther;
  let dropped = outside(clipOwn, viewOwn) || outside(clipOther, viewOther);
  let d = (clipOther.xy / clipOther.w - clipOwn.xy / clipOwn.w) * vec2f(256.0, 224.0);
  let half = vec2f(max(1.0 / 512.0, 1.0 / u.vw), max(1.0 / 448.0, 1.0 / u.vh));
  let offset = select(vec2f(v.corner.x * half.x, 0.0), vec2f(0.0, v.corner.x * half.y), abs(d.x) >= abs(d.y));
  let placed = vec4f(clipOwn.xy + offset * clipOwn.w, clipOwn.zw);
  return select(placed, vec4f(2.0, 2.0, 2.0, 1.0), dropped);
}
@fragment fn fs() -> @location(0) vec4f {
  return vec4f(0.0, 0.0, 0.0, 1.0);
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
    writeLevels(t.texture, w, h, rgba, levels);
    WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
    t.view = wgpuTextureCreateView(t.texture, &vd);
    return t;
}

void WorldRenderer::writeLevels(WGPUTexture texture, uint32_t w, uint32_t h, const std::vector<uint8_t>& rgba, uint32_t levels) {
    gpu_->writeTexture(texture, w, h, rgba.data(), 0);
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
        gpu_->writeTexture(texture, nw, nh, next.data(), k);
        level.swap(next);
        lw = nw;
        lh = nh;
    }
}

void WorldRenderer::swapCrowdFrame(int frame) {
    if (frame == crowdFrame_ || crowdTargets_.empty()) {
        return;
    }
    crowdFrame_ = frame;
    for (auto& t : crowdTargets_) {
        const std::vector<uint8_t>* image = &t.statics;
        if (frame >= 0 && size_t(frame) < crowdFrames_.size()) {
            image = &crowdFrames_[size_t(frame)];
        }
        if (image->size() == size_t(t.width) * t.height * 4) {
            writeLevels(t.texture, t.width, t.height, *image, t.levels);
        }
    }
}

void WorldRenderer::loadCrowd(const std::string& assetsRoot, const std::string& code, const nlohmann::json& world) {
    // web/crowd-2d.js: the crowd2d / crowdpod slots (world textures listed in CROWD/crowd.json) take the animator's current frame
    const std::string path = assetsRoot + "/" + code + "/CROWD/crowd.json";
    if (!std::ifstream(path)) {
        return;
    }
    const json crowd = readJson(path);
    for (const auto& f : crowd["frames"]) {
        crowdFrames_.push_back(readPng(assetsRoot + "/FX/crowd/" + f["file"].get<std::string>()).rgba);
    }
    for (const auto& k : crowd["textures"]) {
        const std::string key = k.get<std::string>();
        if (!world["textures"].contains(key)) {
            continue;
        }
        const json& entry = world["textures"][key];
        const std::string archive = entry["pack"].get<std::string>() + "#" + TextureArchive::key(entry["id"]);
        auto it = archiveTextures_.find(archive);
        if (it == archiveTextures_.end()) {
            continue;
        }
        CrowdTarget t;
        t.texture = it->second.texture;
        t.width = wgpuTextureGetWidth(t.texture);
        t.height = wgpuTextureGetHeight(t.texture);
        t.levels = wgpuTextureGetMipLevelCount(t.texture);
        static std::map<std::string, std::unique_ptr<TextureArchive>> archives;
        const std::string pack = entry["pack"].get<std::string>();
        const std::string file = assetsRoot + "/" + (pack.rfind("/assets/", 0) == 0 ? pack.substr(8) : pack);
        auto& a = archives[file];
        if (!a) {
            a = std::make_unique<TextureArchive>(file);
        }
        t.statics = a->decode(entry["id"]).rgba;
        crowdTargets_.push_back(std::move(t));
    }
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
    WGPUBindGroupLayoutEntry fe[6];
    for (int k = 0; k < 6; ++k) {
        fe[k] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
        fe[k].binding = uint32_t(k);
        fe[k].visibility = WGPUShaderStage_Fragment;
    }
    fe[0].visibility = WGPUShaderStage_Vertex | WGPUShaderStage_Fragment;
    fe[0].buffer.type = WGPUBufferBindingType_Uniform;
    fe[1].visibility = WGPUShaderStage_Vertex;
    fe[1].buffer.type = WGPUBufferBindingType_ReadOnlyStorage;
    for (int k : {2, 4, 5}) {
        fe[k].texture.sampleType = WGPUTextureSampleType_Float;
        fe[k].texture.viewDimension = WGPUTextureViewDimension_2D;
    }
    fe[3].sampler.type = WGPUSamplerBindingType_Filtering;
    WGPUBindGroupLayoutDescriptor fd = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    fd.entryCount = 6;
    fd.entries = fe;
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
    WGPUVertexAttribute a2[2] = {{nullptr, WGPUVertexFormat_Float32x2, 0, 5}, {nullptr, WGPUVertexFormat_Float32, 8, 7}};
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
    vbl[2].arrayStride = 12;
    vbl[2].attributeCount = 2;
    vbl[2].attributes = a2;
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
    WGPUBindGroupLayoutEntry ce[6];
    for (int k = 0; k < 6; ++k) {
        ce[k] = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
        ce[k].binding = uint32_t(k);
        ce[k].visibility = WGPUShaderStage_Fragment;
    }
    ce[0].buffer.type = WGPUBufferBindingType_Uniform;
    for (int k : {1, 3, 4, 5}) {
        ce[k].texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
        ce[k].texture.viewDimension = WGPUTextureViewDimension_2D;
    }
    ce[2].texture.sampleType = WGPUTextureSampleType_Depth;
    ce[2].texture.viewDimension = WGPUTextureViewDimension_2D;
    WGPUBindGroupLayoutDescriptor cd = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    cd.entryCount = 6;
    cd.entries = ce;
    compositeLayout_ = wgpuDeviceCreateBindGroupLayout(d, &cd);
    // group 1: the glow and sun (post_effects.hpp)
    post_.init(*gpu_);
    post_.attach(nullptr, nullptr);
    const WGPUBindGroupLayout compositeLayouts[2] = {compositeLayout_, post_.layout()};
    WGPUPipelineLayoutDescriptor cpl = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    cpl.bindGroupLayoutCount = 2;
    cpl.bindGroupLayouts = compositeLayouts;
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
    for (int which = 0; which < 3; ++which) {
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
        fs.entryPoint = sv(which == 0 ? "copySky" : (which == 1 ? "fog" : "finish"));
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
            (which == 1 ? fog_ : composite_) = wgpuDeviceCreateRenderPipeline(d, &pd);
        }
    }
    worldUniform_ = gpu_->buffer(kFrameBytes, WGPUBufferUsage_Uniform, nullptr, "world frame");
    skyUniform_ = gpu_->buffer(kFrameBytes, WGPUBufferUsage_Uniform, nullptr, "sky frame");
    // web/rider-shadow.js: the 768 x 128 atlas (six 128-pixel slots; LinearFilter) and its depth; the glint images (32 x 32, black until
    // the package brings them; LinearFilter, ClampToEdge)
    {
        WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
        shadowAtlas_.texture = gpu_->texture(768, 128, WGPUTextureFormat_RGBA8Unorm, WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding, 1,
                                             "rider shadow atlas");
        shadowAtlas_.view = wgpuTextureCreateView(shadowAtlas_.texture, &vd);
        shadowDepth_.texture = gpu_->texture(768, 128, WGPUTextureFormat_Depth24Plus, WGPUTextureUsage_RenderAttachment, 1, "rider shadow depth");
        shadowDepth_.view = wgpuTextureCreateView(shadowDepth_.texture, &vd);
        for (int k = 0; k < 2; ++k) {
            Texture& t = k == 0 ? glintGrey_ : glintBlue_;
            t.texture = gpu_->texture(32, 32, WGPUTextureFormat_RGBA8Unorm, WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst, 1, "glint");
            const std::vector<uint8_t> black(32 * 32 * 4, 0);
            gpu_->writeTexture(t.texture, 32, 32, black.data());
            t.view = wgpuTextureCreateView(t.texture, &vd);
        }
        WGPUSamplerDescriptor cs = WGPU_SAMPLER_DESCRIPTOR_INIT;
        cs.magFilter = WGPUFilterMode_Linear;
        cs.minFilter = WGPUFilterMode_Linear;
        cs.addressModeU = WGPUAddressMode_ClampToEdge;
        cs.addressModeV = WGPUAddressMode_ClampToEdge;
        linearClamp_ = wgpuDeviceCreateSampler(d, &cs);
    }
    compositeUniform_ = gpu_->buffer(80, WGPUBufferUsage_Uniform, nullptr, "composite");
    buildCablePipeline();
    worldObjects_ = gpu_->buffer(kObjects * kObjectFloats * 4, WGPUBufferUsage_Storage, nullptr, "world objects");
    skyObjects_ = gpu_->buffer(kScrollGroups * kObjectFloats * 4, WGPUBufferUsage_Storage, nullptr, "sky objects");
    for (int k = 0; k < 2; ++k) {
        WGPUBindGroupEntry e[6];
        for (int j = 2; j < 6; ++j) {
            e[j] = WGPU_BIND_GROUP_ENTRY_INIT;
            e[j].binding = uint32_t(j);
        }
        e[2].textureView = shadowAtlas_.view;
        e[3].sampler = linearClamp_;
        e[4].textureView = glintGrey_.view;
        e[5].textureView = glintBlue_.view;
        e[0] = WGPU_BIND_GROUP_ENTRY_INIT;
        e[0].binding = 0;
        e[0].buffer = k == 0 ? worldUniform_ : skyUniform_;
        e[0].size = kFrameBytes;
        e[1] = WGPU_BIND_GROUP_ENTRY_INIT;
        e[1].binding = 1;
        e[1].buffer = k == 0 ? worldObjects_ : skyObjects_;
        e[1].size = (k == 0 ? kObjects : kScrollGroups) * kObjectFloats * 4;
        WGPUBindGroupDescriptor bd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        bd.layout = frameLayout_;
        bd.entryCount = 6;
        bd.entries = e;
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
    // the PS2 light UV and the glint kind (terrain-glint.json) per vertex
    std::vector<float> lightUv(count * 3, 0.f);
    std::vector<float> alpha(count, 1.f);
    if (world) {
        // web/world-material.js: the package's glint patches and its glint images
        std::map<long long, float> glintOf;
        const std::string glintPath = root + "terrain-glint.json";
        if (std::ifstream(glintPath)) {
            const json glint = readJson(glintPath);
            for (const auto& p : glint["patches"]) {
                glintOf[p[0].get<long long>()] = p[1].get<float>();
            }
            for (const auto& [kind, image] : glint["textures"].items()) {
                if (image["width"] != 32 || image["height"] != 32) {
                    continue;
                }
                const auto rgba = base64(image["rgba"].get<std::string>());
                if (rgba.size() == 32 * 32 * 4) {
                    gpu_->writeTexture((kind == "blue" ? glintBlue_ : glintGrey_).texture, 32, 32, rgba.data());
                }
            }
        }
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
            auto g2 = glintOf.find(patch["resource"].get<long long>());
            const float glint = g2 == glintOf.end() ? 0.f : g2->second;
            for (size_t v = first; v < first + 81 && v < count; ++v) {
                lightUv[v * 3] = float(map0 + double(vertices[v * 10 + 8]) * sx);
                lightUv[v * 3 + 1] = float(map2 + double(vertices[v * 10 + 9]) * sy);
                lightUv[v * 3 + 2] = glint;
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
        // runtime flag bit 0 clear: not in the scene (main.js hiddenMeshes) unless a set piece shows it
        const bool defaultHidden = b.contains("hidden_resource");
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
        {
            // main.js asset(): the batch's box over its indexed vertices (float32 positions), its sphere centre
            double lo[3] = {1e300, 1e300, 1e300};
            double hi[3] = {-1e300, -1e300, -1e300};
            for (uint32_t i = draw.first; i < draw.first + draw.count && i < indices.size(); ++i) {
                const float* v = vertices.data() + size_t(indices[i]) * 10;
                for (int k = 0; k < 3; ++k) {
                    lo[k] = std::min(lo[k], double(v[k]));
                    hi[k] = std::max(hi[k], double(v[k]));
                }
            }
            for (int k = 0; k < 3; ++k) {
                draw.centre[size_t(k)] = (lo[k] + hi[k]) * 0.5;
            }
        }
        draw.material = g->second;
        draw.defaultHidden = defaultHidden;
        // web/set-pieces-renderer.js chunkMeshes: the chunk residency hides the plain static batches only (not the LiveComp, hidden,
        // pickup, event-dead or moving ones)
        const bool special = b.contains("livecomp_resource") || defaultHidden || b.contains("pickup_resource") || b.contains("event_dead_resource") ||
                             b.contains("moving_resource");
        draw.chunk = b.contains("chunk") && !special ? b["chunk"].get<int>() : -1;
        if (world && instance && b.contains("uv_scroll_group")) {
            draw.scroll = std::min<int>(b["uv_scroll_group"].get<int>() + 1, int(kScrollGroups) - 1);
        }
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
    if (world) {
        // the flag meshes draw with their source batch's material (the first draw: a blended source's pass, not its fringe)
        batchDraw_.clear();
        for (const auto* list : {&p.opaque, &p.transparent}) {
            for (const auto& d : *list) {
                batchDraw_.emplace(d.batch, &d);
            }
        }
    }
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
    fx_.load(gpu, assetsRoot);
    loadCrowd(assetsRoot, code, readJson(assetsRoot + "/" + code + "/world.json"));
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
    fogged_.texture = gpu_->texture(w, h, WGPUTextureFormat_RGBA8Unorm, usage, 1, "fogged world");
    fogged_.view = wgpuTextureCreateView(fogged_.texture, &vd);
    WGPUBindGroupEntry e[6];
    for (int k = 0; k < 6; ++k) {
        e[k] = WGPU_BIND_GROUP_ENTRY_INIT;
        e[k].binding = uint32_t(k);
    }
    e[0].buffer = compositeUniform_;
    e[0].size = 80;
    e[1].textureView = worldColor_.view;
    e[2].textureView = depthSample_;
    e[3].textureView = palette_.view;
    e[4].textureView = skyColor_.view;
    // the fog pass writes the fogged world: the palette stands in for it there
    e[5].textureView = palette_.view;
    WGPUBindGroupDescriptor bd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bd.layout = compositeLayout_;
    bd.entryCount = 6;
    bd.entries = e;
    compositeGroup_ = wgpuDeviceCreateBindGroup(gpu_->device, &bd);
    e[5].textureView = fogged_.view;
    finishGroup_ = wgpuDeviceCreateBindGroup(gpu_->device, &bd);
    WGPUBindGroupEntry ke = WGPU_BIND_GROUP_ENTRY_INIT;
    ke.binding = 0;
    ke.textureView = skyColor_.view;
    WGPUBindGroupDescriptor kd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    kd.layout = copyLayout_;
    kd.entryCount = 1;
    kd.entries = &ke;
    copyGroup_ = wgpuDeviceCreateBindGroup(gpu_->device, &kd);
}

void WorldRenderer::collect(const Package& p, const Mat4d& vp, const Mat4d& staticModel, const WorldFrameState& state, bool world,
                            std::vector<Item>& opaque, std::vector<Item>& transparent) {
    opaque.clear();
    transparent.clear();
    // three's _projScreenMatrix x matrixWorld applied to the sphere centre (w 1): the clip z, not divided
    auto clipZ = [&](const Mat4d& model, const std::array<double, 3>& c) {
        double w[4];
        for (int r = 0; r < 4; ++r) {
            w[r] = model[size_t(r)] * c[0] + model[size_t(4 + r)] * c[1] + model[size_t(8 + r)] * c[2] + model[size_t(12 + r)];
        }
        return vp[2] * w[0] + vp[6] * w[1] + vp[10] * w[2] + vp[14] * w[3];
    };
    // a moved or copied batch: its own object record (projection x view x matrixWorld, the batch's scroll)
    auto record = [&](const Mat4d& model, int scroll) {
        const size_t index = objectData_.size() / kObjectFloats;
        if (index >= kObjects) {
            throw std::runtime_error("world object records exhausted");
        }
        const Mat4d mvp = multiply(vp, model);
        for (int k = 0; k < 16; ++k) {
            objectData_.push_back(float(mvp[size_t(k)]));
        }
        const float* base = objectData_.data() + size_t(scroll) * kObjectFloats;
        const float su = base[16];
        const float sv = base[17];
        objectData_.insert(objectData_.end(), {su, sv, 0.f, 0.f});
        return uint32_t(index);
    };
    auto add = [&](const Draw& d, std::vector<Item>& list) {
        if (d.chunk >= 0) {
            // texture chunks the original has not streamed in are not drawn (0x22A5A0; an unknown chunk is drawn)
            auto c = state.chunks.find(uint32_t(d.chunk));
            if (c != state.chunks.end() && !c->second) {
                return;
            }
        }
        const BatchOverride* o = nullptr;
        if (world && state.batches) {
            auto it = state.batches->find(d.batch);
            if (it != state.batches->end()) {
                o = &it->second;
            }
        }
        const bool present = !d.defaultHidden || (o && o->shown);
        if (present && !(o && o->hidden)) {
            Item item;
            item.draw = &d;
            item.package = &p;
            item.id = d.batch;
            if (o && o->moved) {
                item.object = record(o->matrix, d.scroll);
                item.z = clipZ(o->matrix, d.centre);
            } else {
                item.object = uint32_t(d.scroll);
                item.z = clipZ(staticModel, d.centre);
            }
            list.push_back(item);
        }
        if (o && present) {
            // the clones (mesh.clone(), added to the scene after every authored mesh: later ids)
            for (size_t k = 0; k < o->copies.size(); ++k) {
                Item item;
                item.draw = &d;
                item.package = &p;
                item.id = 1000000 + d.batch * 64 + k;
                item.object = record(o->copies[k], d.scroll);
                item.z = clipZ(o->copies[k], d.centre);
                list.push_back(item);
            }
        }
    };
    for (const auto& d : p.opaque) {
        add(d, opaque);
    }
    for (const auto& d : p.transparent) {
        add(d, transparent);
    }
    sortItems(opaque, transparent);
}

void WorldRenderer::sortItems(std::vector<Item>& opaque, std::vector<Item>& transparent) {
    // painterSortStable / reversePainterSortStable (renderOrder, z, id; stable: a mesh's material groups keep their order)
    std::stable_sort(opaque.begin(), opaque.end(), [](const Item& a, const Item& b) {
        if (a.draw->order != b.draw->order) {
            return a.draw->order < b.draw->order;
        }
        if (a.z != b.z) {
            return a.z < b.z;
        }
        return a.id < b.id;
    });
    std::stable_sort(transparent.begin(), transparent.end(), [](const Item& a, const Item& b) {
        if (a.draw->order != b.draw->order) {
            return a.draw->order < b.draw->order;
        }
        if (a.z != b.z) {
            return a.z > b.z;
        }
        return a.id < b.id;
    });
}

void WorldRenderer::collectFlags(const WorldFrameState& state, const Mat4d& vp) {
    if (!state.flags || state.flags->empty()) {
        return;
    }
    const Mat4 model = translate(-origin[0], -origin[1], -origin[2]);
    size_t order = 0;
    for (const auto& f : *state.flags) {
        ++order;
        auto src = batchDraw_.find(f.sourceBatch);
        if (src == batchDraw_.end() || f.positions.empty()) {
            continue;
        }
        FlagGpu& g = flagGpu_[f.resource];
        const size_t n = f.positions.size() / 3;
        if (!g.package || g.capacity != n) {
            g.package = std::make_unique<Package>();
            g.capacity = n;
            g.revision = ~uint64_t(0);
            Package& p = *g.package;
            p.vertices = gpu_->buffer(n * 40, WGPUBufferUsage_Vertex, nullptr, "flag vertices");
            // the flag's own colours (bilinear over the group's corners), no light page, vertex alpha 1
            p.colors = gpu_->buffer(n * 16, WGPUBufferUsage_Vertex, f.colour.data(), "flag colours");
            const std::vector<float> zero(n * 3, 0.f);
            p.lightUv = gpu_->buffer(n * 12, WGPUBufferUsage_Vertex, zero.data(), "flag light uv");
            const std::vector<float> one(n, 1.f);
            p.vertexAlpha = gpu_->buffer(n * 4, WGPUBufferUsage_Vertex, one.data(), "flag vertex alpha");
            p.indices = gpu_->buffer(f.indices.size() * 4, WGPUBufferUsage_Index, f.indices.data(), "flag indices");
            g.draw = *src->second;
            g.draw.first = 0;
            g.draw.count = uint32_t(f.indices.size());
            g.draw.chunk = -1;
            g.draw.defaultHidden = false;
            // three computes the geometry's bounding sphere on its first projection: these positions
            double lo[3] = {1e300, 1e300, 1e300};
            double hi[3] = {-1e300, -1e300, -1e300};
            for (size_t i = 0; i < n; ++i) {
                for (size_t k = 0; k < 3; ++k) {
                    lo[k] = std::min(lo[k], double(f.positions[i * 3 + k]));
                    hi[k] = std::max(hi[k], double(f.positions[i * 3 + k]));
                }
            }
            for (size_t k = 0; k < 3; ++k) {
                g.draw.centre[k] = (lo[k] + hi[k]) * 0.5;
            }
        }
        if (g.revision != f.revision) {
            std::vector<float> v(n * 10, 0.f);
            for (size_t i = 0; i < n; ++i) {
                v[i * 10] = f.positions[i * 3];
                v[i * 10 + 1] = f.positions[i * 3 + 1];
                v[i * 10 + 2] = f.positions[i * 3 + 2];
                v[i * 10 + 6] = f.uv[i * 2];
                v[i * 10 + 7] = f.uv[i * 2 + 1];
            }
            wgpuQueueWriteBuffer(gpu_->queue, g.package->vertices, 0, v.data(), v.size() * 4);
            g.revision = f.revision;
        }
        Item item;
        item.draw = &g.draw;
        item.package = g.package.get();
        item.object = uint32_t(g.draw.scroll);
        double w[4];
        for (int r = 0; r < 4; ++r) {
            w[r] = model[size_t(r)] * g.draw.centre[0] + model[size_t(4 + r)] * g.draw.centre[1] + model[size_t(8 + r)] * g.draw.centre[2] + model[size_t(12 + r)];
        }
        item.z = vp[2] * w[0] + vp[6] * w[1] + vp[10] * w[2] + vp[14] * w[3];
        // added to the scene after every batch mesh
        item.id = 2000000 + order;
        const bool transparent = g.draw.kind >= 4;
        (transparent ? worldTransparent_ : worldOpaque_).push_back(item);
    }
    sortItems(worldOpaque_, worldTransparent_);
}

void WorldRenderer::buildCablePipeline() {
    WGPUDevice d = gpu_->device;
    WGPUBindGroupLayoutEntry e = WGPU_BIND_GROUP_LAYOUT_ENTRY_INIT;
    e.binding = 0;
    e.visibility = WGPUShaderStage_Vertex;
    e.buffer.type = WGPUBufferBindingType_Uniform;
    WGPUBindGroupLayoutDescriptor ld = WGPU_BIND_GROUP_LAYOUT_DESCRIPTOR_INIT;
    ld.entryCount = 1;
    ld.entries = &e;
    cableLayout_ = wgpuDeviceCreateBindGroupLayout(d, &ld);
    WGPUPipelineLayoutDescriptor pl = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pl.bindGroupLayoutCount = 1;
    pl.bindGroupLayouts = &cableLayout_;
    WGPUPipelineLayout layout = wgpuDeviceCreatePipelineLayout(d, &pl);
    WGPUShaderModule module = gpu_->shader(kCableShader, "cables");
    WGPUVertexAttribute own = {nullptr, WGPUVertexFormat_Float32x3, 0, 0};
    WGPUVertexAttribute other = {nullptr, WGPUVertexFormat_Float32x3, 0, 1};
    WGPUVertexAttribute corner = {nullptr, WGPUVertexFormat_Float32x2, 0, 2};
    WGPUVertexBufferLayout vb[3];
    const WGPUVertexAttribute* attrs[3] = {&own, &other, &corner};
    const uint64_t strides[3] = {12, 12, 8};
    for (int k = 0; k < 3; ++k) {
        vb[k] = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
        vb[k].arrayStride = strides[k];
        vb[k].stepMode = WGPUVertexStepMode_Vertex;
        vb[k].attributeCount = 1;
        vb[k].attributes = attrs[k];
    }
    WGPURenderPipelineDescriptor pd = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    pd.layout = layout;
    pd.vertex.module = module;
    pd.vertex.entryPoint = sv("vs");
    pd.vertex.bufferCount = 3;
    pd.vertex.buffers = vb;
    pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
    pd.primitive.cullMode = WGPUCullMode_None;
    WGPUColorTargetState ct = WGPU_COLOR_TARGET_STATE_INIT;
    ct.format = WGPUTextureFormat_RGBA8Unorm;
    ct.writeMask = WGPUColorWriteMask_All;
    WGPUFragmentState fs = WGPU_FRAGMENT_STATE_INIT;
    fs.module = module;
    fs.entryPoint = sv("fs");
    fs.targetCount = 1;
    fs.targets = &ct;
    pd.fragment = &fs;
    WGPUDepthStencilState ds = WGPU_DEPTH_STENCIL_STATE_INIT;
    ds.format = WGPUTextureFormat_Depth24PlusStencil8;
    ds.depthWriteEnabled = WGPUOptionalBool_True;
    ds.depthCompare = WGPUCompareFunction_LessEqual;
    ds.stencilWriteMask = 0;
    pd.depthStencil = &ds;
    pd.multisample.count = 1;
    pd.multisample.mask = 0xffffffffu;
    cablePipeline_ = wgpuDeviceCreateRenderPipeline(d, &pd);
    cableUniform_ = gpu_->buffer(144, WGPUBufferUsage_Uniform, nullptr, "cables");
    WGPUBindGroupEntry be = WGPU_BIND_GROUP_ENTRY_INIT;
    be.binding = 0;
    be.buffer = cableUniform_;
    be.size = 144;
    WGPUBindGroupDescriptor bd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bd.layout = cableLayout_;
    bd.entryCount = 1;
    bd.entries = &be;
    cableGroup_ = wgpuDeviceCreateBindGroup(d, &bd);
}

void WorldRenderer::drawCables(WGPURenderPassEncoder pass, const WorldFrameState& state) {
    if (!state.cables || state.cables->empty()) {
        return;
    }
    bool any = false;
    for (size_t k = 0; k < state.cables->size(); ++k) {
        const CableMesh& c = (*state.cables)[k];
        if (!c.visible || c.indices.empty()) {
            continue;
        }
        CableGpu& g = cableGpu_[k];
        if (!g.own) {
            g.own = gpu_->buffer(c.own.size() * 4, WGPUBufferUsage_Vertex, c.own.data(), "cable own");
            g.other = gpu_->buffer(c.other.size() * 4, WGPUBufferUsage_Vertex, c.other.data(), "cable other");
            g.corner = gpu_->buffer(c.corner.size() * 4, WGPUBufferUsage_Vertex, c.corner.data(), "cable corner");
            g.indices = gpu_->buffer(c.indices.size() * 4, WGPUBufferUsage_Index, c.indices.data(), "cable indices");
            g.count = uint32_t(c.indices.size());
        }
        if (!any) {
            wgpuRenderPassEncoderSetPipeline(pass, cablePipeline_);
            wgpuRenderPassEncoderSetBindGroup(pass, 0, cableGroup_, 0, nullptr);
            any = true;
        }
        wgpuRenderPassEncoderSetVertexBuffer(pass, 0, g.own, 0, WGPU_WHOLE_SIZE);
        wgpuRenderPassEncoderSetVertexBuffer(pass, 1, g.other, 0, WGPU_WHOLE_SIZE);
        wgpuRenderPassEncoderSetVertexBuffer(pass, 2, g.corner, 0, WGPU_WHOLE_SIZE);
        wgpuRenderPassEncoderSetIndexBuffer(pass, g.indices, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
        wgpuRenderPassEncoderDrawIndexed(pass, g.count, 1, 0, 0, 0);
    }
}

void WorldRenderer::bindPackage(WGPURenderPassEncoder pass, const Package& p) {
    wgpuRenderPassEncoderSetVertexBuffer(pass, 0, p.vertices, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 1, p.colors, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 2, p.lightUv, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetVertexBuffer(pass, 3, p.vertexAlpha, 0, WGPU_WHOLE_SIZE);
    wgpuRenderPassEncoderSetIndexBuffer(pass, p.indices, WGPUIndexFormat_Uint32, 0, WGPU_WHOLE_SIZE);
}

void WorldRenderer::drawItems(WGPURenderPassEncoder pass, const std::vector<Item>& items, int side) {
    WGPURenderPipeline bound = nullptr;
    const Package* package = nullptr;
    for (const auto& item : items) {
        if (item.special) {
            // the crowd's camera flashes (renderOrder 0, a camera-facing DoubleSide quad: its front faces)
            if (side != 0) {
                fx_.drawFlashes(pass);
                wgpuRenderPassEncoderSetBindGroup(pass, 0, worldFrame_, 0, nullptr);
                bound = nullptr;
                package = nullptr;
            }
            continue;
        }
        const Draw& d = *item.draw;
        if (item.package != package) {
            bindPackage(pass, *item.package);
            package = item.package;
        }
        auto it = pipelines_.find(d.kind * 10 + std::max(side, 0));
        if (it == pipelines_.end()) {
            continue;
        }
        if (it->second != bound) {
            wgpuRenderPassEncoderSetPipeline(pass, it->second);
            bound = it->second;
        }
        wgpuRenderPassEncoderSetBindGroup(pass, 1, d.material, 0, nullptr);
        wgpuRenderPassEncoderDrawIndexed(pass, d.count, 1, d.first, 0, item.object);
    }
}

void WorldRenderer::writeFrame(const WorldFrameState& state, const double eye[3], const Mat4d& P, const Mat4d& V) {
    // web/rider-shadow.js update: each drawn rider's fit from the pose it draws (the bones blended by the render alpha, the current
    // physical frame), kept when its 3 m sphere is in view; the nearest six by distance to the camera get the slots
    struct Candidate {
        RiderRenderer* renderer;
        shadow::Fit fit;
        bool grounded;
        double d;
    };
    std::vector<Candidate> candidates;
    const Mat4 vp = multiply(P, V);
    std::array<std::array<double, 4>, 6> planes{};
    {
        const double* m = vp.data();
        const double rows[4][4] = {{m[0], m[4], m[8], m[12]}, {m[1], m[5], m[9], m[13]}, {m[2], m[6], m[10], m[14]}, {m[3], m[7], m[11], m[15]}};
        const int pick[6][2] = {{0, -1}, {0, 1}, {1, 1}, {1, -1}, {2, -1}, {2, 1}};
        for (int k = 0; k < 6; ++k) {
            std::array<double, 4> plane{};
            for (int j = 0; j < 4; ++j) {
                plane[size_t(j)] = rows[3][j] + (pick[k][1] < 0 ? -rows[pick[k][0]][j] : rows[pick[k][0]][j]);
            }
            const double l = std::sqrt(plane[0] * plane[0] + plane[1] * plane[1] + plane[2] * plane[2]);
            for (double& c : plane) {
                c /= l;
            }
            planes[size_t(k)] = plane;
        }
    }
    for (const auto& [r, frame] : state.riders) {
        const auto& a = frame->bonesPrevious;
        const auto& b = frame->bonesCurrent;
        if (b.empty() || a.size() != b.size() || b[0] < 22) {
            continue;
        }
        const double t = std::max(0.0, std::min(1.0, double(frame->alpha)));
        std::array<shadow::V3, 6> bones{};
        bool finite = true;
        for (int i = 0; i < 6; ++i) {
            const size_t at = 1 + 7 * size_t(shadow::kBones[i]);
            for (size_t k = 0; k < 3; ++k) {
                bones[size_t(i)][k] = float(double(a[at + k]) + (double(b[at + k]) - a[at + k]) * t);
                finite = finite && std::isfinite(bones[size_t(i)][k]);
            }
        }
        const auto& P8 = frame->physical;
        const double q0 = P8[3], q1 = P8[4], q2 = P8[5], q3 = P8[6];
        const shadow::V3 up = {float(2 * (q0 * q2 + q3 * q1)), float(2 * (q1 * q2 - q3 * q0)), float(1 - 2 * (q0 * q0 + q1 * q1))};
        if (!finite || !std::isfinite(up[0]) || !std::isfinite(up[1]) || !std::isfinite(up[2])) {
            continue;
        }
        const auto fit = shadow::fitShadow(bones, up);
        if (!fit) {
            continue;
        }
        const double c[3] = {fit->C[0] / 100.0 - origin[0], fit->C[2] / 100.0 - origin[1], -fit->C[1] / 100.0 - origin[2]};
        bool inView = true;
        for (const auto& pl : planes) {
            if (pl[0] * c[0] + pl[1] * c[1] + pl[2] * c[2] + pl[3] < -3) {
                inView = false;
            }
        }
        if (!inView) {
            continue;
        }
        const double d = (eye[0] - c[0]) * (eye[0] - c[0]) + (eye[1] - c[1]) * (eye[1] - c[1]) + (eye[2] - c[2]) * (eye[2] - c[2]);
        candidates.push_back({r, *fit, P8[8] != 0, d});
    }
    std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& x, const Candidate& y) { return x.d < y.d; });
    const size_t slots = std::min<size_t>(candidates.size(), shadow::kMax);
    shadowSlots_.clear();
    std::vector<float> u(kFrameBytes / 4, 0.f);
    u[0] = float(eye[0] + origin[0]);
    u[1] = float(eye[1] + origin[1]);
    u[2] = float(eye[2] + origin[2]);
    u[4] = float(origin[0]);
    u[5] = float(origin[1]);
    u[6] = float(origin[2]);
    u[8] = float(slots);
    for (size_t k = 0; k < slots; ++k) {
        const auto& cand = candidates[k];
        shadowSlots_.push_back({cand.renderer, shadow::slotRows(cand.fit, int(k), cand.grounded)});
        const auto rows = shadow::receiverRows(cand.fit, origin);
        for (size_t r = 0; r < 3; ++r) {
            std::memcpy(u.data() + 12 + (k * 3 + r) * 4, rows[r].data(), 16);
        }
    }
    wgpuQueueWriteBuffer(gpu_->queue, worldUniform_, 0, u.data(), kFrameBytes);
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
    writeFrame(state, eye, P, V);
    {
        // the static records (the world's matrix, each scroll group's translation), then the frame's moved / copied batches
        objectData_.assign(kScrollGroups * kObjectFloats, 0.f);
        for (size_t g = 0; g < kScrollGroups; ++g) {
            std::memcpy(objectData_.data() + g * kObjectFloats, wf, 64);
            if (g >= 1 && g - 1 < state.scroll.size()) {
                objectData_[g * kObjectFloats + 16] = state.scroll[g - 1][0];
                objectData_[g * kObjectFloats + 17] = state.scroll[g - 1][1];
            }
        }
        const Mat4 vp = multiply(P, V);
        collect(world_, vp, translate(-origin[0], -origin[1], -origin[2]), state, true, worldOpaque_, worldTransparent_);
        collectFlags(state, vp);
        // the cables: the camera's view and projection, near / far, the target's size (three's viewportSize)
        float cu[36];
        for (int k = 0; k < 16; ++k) {
            cu[k] = float(V[size_t(k)]);
            cu[16 + k] = float(P[size_t(k)]);
        }
        cu[32] = float(near);
        cu[33] = float(far);
        cu[34] = float(width);
        cu[35] = float(height);
        wgpuQueueWriteBuffer(gpu_->queue, cableUniform_, 0, cu, sizeof cu);
        wgpuQueueWriteBuffer(gpu_->queue, worldObjects_, 0, objectData_.data(), objectData_.size() * 4);
        // the sky: its meshes at -origin inside the sky group at eye + origin
        std::vector<float> sky(kScrollGroups * kObjectFloats, 0.f);
        for (size_t g = 0; g < kScrollGroups; ++g) {
            std::memcpy(sky.data() + g * kObjectFloats, sf, 64);
        }
        wgpuQueueWriteBuffer(gpu_->queue, skyObjects_, 0, sky.data(), sky.size() * 4);
        const std::vector<float> keep = objectData_;
        collect(sky_, vp, translate(eye[0], eye[1], eye[2]), state, false, skyOpaque_, skyTransparent_);
        objectData_ = keep;
    }
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
    {
        const std::vector<float> zero(kFrameBytes / 4, 0.f);
        wgpuQueueWriteBuffer(gpu_->queue, skyUniform_, 0, zero.data(), kFrameBytes);
    }
    const auto& tn = state.tint;
    const float params[20] = {float(near), float(far), state.fogSlope, state.fogOffset,
                              state.fogEnabled ? 1.f : 0.f, float(tn.enabled), tn.flashOn ? 1.f : 0.f, tn.flashA,
                              tn.scaleBytes[0], tn.scaleBytes[1], tn.scaleBytes[2], 0,
                              tn.addBytes[0], tn.addBytes[1], tn.addBytes[2], 0,
                              tn.flashRGB[0], tn.flashRGB[1], tn.flashRGB[2], 0};
    wgpuQueueWriteBuffer(gpu_->queue, compositeUniform_, 0, params, sizeof params);
    {
        // three's camera.matrixWorld: the inverse of the view (columns right, up, back, eye)
        const Mat4 W = {V[0], V[4], V[8], 0, V[1], V[5], V[9], 0, V[2], V[6], V[10], 0, eye[0], eye[1], eye[2], 1};
        post_.prepare(W.data(), V.data(), P.data(), near, far, width, height, depthSample_);
        const std::vector<QuadBatch>* quads = state.setPieceFx ? &state.setPieceFx->quads(V.data(), P.data(), near, far) : nullptr;
        if (state.setPieceFx) {
            swapCrowdFrame(state.setPieceFx->crowdFrame());
            const auto& flashes = state.setPieceFx->flashes();
            fx_.prepareFlashes(flashes);
            if (!flashes.empty()) {
                // the flash holder at -origin (its PlaneGeometry centre): the transparent sort's depth
                Item item;
                item.special = true;
                item.id = 4000000;
                const Mat4 vp = multiply(P, V);
                item.z = vp[2] * -origin[0] + vp[6] * -origin[1] + vp[10] * -origin[2] + vp[14];
                static Draw flashDraw;
                item.draw = &flashDraw;
                worldTransparent_.push_back(item);
                sortItems(worldOpaque_, worldTransparent_);
            }
        }
        if (state.sparkle) {
            fx_.prepareSparkle(state.sparkle->update(W.data(), P.data(), state.fogInfo ? state.fogInfo->data() : nullptr, state.resets));
        } else {
            fx_.prepareSparkle({});
        }
        fx_.prepare(state.fx, eye, look, V.data(), P.data(), origin, quads, state.startfire);
    }
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
    // the rider shadows' atlas (cleared to 0; each rider's silhouette in its slot)
    if (!shadowSlots_.empty()) {
        WGPURenderPassColorAttachment ca = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        ca.view = shadowAtlas_.view;
        ca.loadOp = WGPULoadOp_Clear;
        ca.storeOp = WGPUStoreOp_Store;
        ca.clearValue = {0, 0, 0, 0};
        WGPURenderPassDepthStencilAttachment da = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
        da.view = shadowDepth_.view;
        da.depthLoadOp = WGPULoadOp_Clear;
        da.depthStoreOp = WGPUStoreOp_Store;
        da.depthClearValue = 1;
        WGPURenderPassDescriptor rd = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        rd.colorAttachmentCount = 1;
        rd.colorAttachments = &ca;
        rd.depthStencilAttachment = &da;
        WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &rd);
        for (const auto& [r, slot] : shadowSlots_) {
            r->drawShadow(pass, slot);
        }
        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);
    }
    // the sky pass (cleared to black)
    {
        WGPURenderPassEncoder pass = colorPass(skyColor_.view, false);
        wgpuRenderPassEncoderSetBindGroup(pass, 0, skyFrame_, 0, nullptr);
        bindPackage(pass, sky_);
        drawItems(pass, skyOpaque_, -1);
        // three's transparent double pass: every DoubleSide transparent mesh's back faces, then the list with front faces
        drawItems(pass, skyTransparent_, 0);
        drawItems(pass, skyTransparent_, 1);
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
        bindPackage(pass, world_);
        drawItems(pass, worldOpaque_, -1);
        // the cables (opaque, renderOrder 0, added after the world's batches)
        drawCables(pass, state);
        // the board trails (renderOrder 500 .. 504): after the world's opaque draws, before the riders (600)
        fx_.drawTrails(pass);
        for (const auto& [r, frame] : state.riders) {
            r->drawOpaque(pass);
        }
        // three's transparent double pass over the world's blended batches (renderOrder 0 / 0.5) and the riders' blended ones (600):
        // every back face first, then the list with front faces
        for (int side = 0; side < 2; ++side) {
            wgpuRenderPassEncoderSetBindGroup(pass, 0, worldFrame_, 0, nullptr);
            bindPackage(pass, world_);
            drawItems(pass, worldTransparent_, side);
            for (const auto& [r, frame] : state.riders) {
                r->drawTransparent(pass, side);
            }
        }
        // the terrain sparkle (renderOrder 640, forceSinglePass: after the riders' front faces)
        fx_.drawSparkle(pass);
        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);
    }
    // the fog composite (web/fog-renderer.js fogBytes) into the fogged world
    {
        WGPURenderPassEncoder pass = colorPass(fogged_.view, false);
        wgpuRenderPassEncoderSetPipeline(pass, fog_);
        wgpuRenderPassEncoderSetBindGroup(pass, 0, compositeGroup_, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(pass, 1, post_.group(), 0, nullptr);
        wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);
    }
    // the encoded pass (web/snow-composite.js): the rider effects blended onto the fogged bytes, tested against the world depth
    {
        WGPURenderPassColorAttachment ca = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        ca.view = fogged_.view;
        ca.loadOp = WGPULoadOp_Load;
        ca.storeOp = WGPUStoreOp_Store;
        WGPURenderPassDepthStencilAttachment da = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
        da.view = depth_.view;
        da.depthReadOnly = true;
        da.stencilReadOnly = true;
        da.depthLoadOp = WGPULoadOp_Undefined;
        da.depthStoreOp = WGPUStoreOp_Undefined;
        da.stencilLoadOp = WGPULoadOp_Undefined;
        da.stencilStoreOp = WGPUStoreOp_Undefined;
        WGPURenderPassDescriptor rd = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        rd.colorAttachmentCount = 1;
        rd.colorAttachments = &ca;
        rd.depthStencilAttachment = &da;
        WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &rd);
        fx_.drawEncoded(pass);
        wgpuRenderPassEncoderEnd(pass);
        wgpuRenderPassEncoderRelease(pass);
    }
    // the depth queries and the glow halos on the world depth
    post_.encode(enc);
    // the glows, the sun and ScreenTint into the target
    {
        WGPURenderPassEncoder pass = colorPass(target, false);
        wgpuRenderPassEncoderSetPipeline(pass, composite_);
        wgpuRenderPassEncoderSetBindGroup(pass, 0, finishGroup_, 0, nullptr);
        wgpuRenderPassEncoderSetBindGroup(pass, 1, post_.group(), 0, nullptr);
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
