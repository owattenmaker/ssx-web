#pragma once
// The front end's GPU context over the standard webgpu.h (docs/native.md "N3"): wgpu-native today; Dawn implements the same
// header. Synchronous helpers for set-up and readback (the async calls are waited on with wgpuInstanceProcessEvents).
#include <webgpu/webgpu.h>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace ssx_native {

inline WGPUStringView sv(const char* s) {
    return WGPUStringView{s, WGPU_STRLEN};
}
inline WGPUStringView sv(const std::string& s) {
    return WGPUStringView{s.data(), s.size()};
}

struct Gpu {
    WGPUInstance instance = nullptr;
    WGPUAdapter adapter = nullptr;
    WGPUDevice device = nullptr;
    WGPUQueue queue = nullptr;
    // texture-compression-bc: asked for before init (false: the page's ?bc=0, the RGBA decode), granted after it
    bool allowBC = true;
    bool bc = false;
    // surface: when given (a window's), the adapter must be able to present to it
    void init(WGPUSurface compatibleSurface = nullptr);
    void release();
    // the next events (callbacks) handled
    void process();
    WGPUBuffer buffer(uint64_t size, WGPUBufferUsage usage, const void* data = nullptr, const char* label = nullptr);
    WGPUTexture texture(uint32_t w, uint32_t h, WGPUTextureFormat format, WGPUTextureUsage usage, uint32_t mips = 1, const char* label = nullptr);
    void writeTexture(WGPUTexture t, uint32_t w, uint32_t h, const void* rgba, uint32_t mip = 0);
    // one mip level of a block-compressed texture (blockBytes per 4 x 4 block, rows of blocks tightly packed)
    void writeBlocks(WGPUTexture t, uint32_t w, uint32_t h, const void* blocks, uint32_t blockBytes, uint32_t mip);
    WGPUShaderModule shader(const std::string& wgsl, const char* label);
    // an RGBA8 texture's pixels (rows tightly packed)
    // bytesPerTexel: 4 for the RGBA8 targets, 8 for an RGBA16Float one
    std::vector<uint8_t> readback(WGPUTexture t, uint32_t w, uint32_t h, uint32_t bytesPerTexel = 4);
    // queued work done
    void wait();
};

}  // namespace ssx_native
