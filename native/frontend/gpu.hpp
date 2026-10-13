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
    // surface: when given (a window's), the adapter must be able to present to it
    void init(WGPUSurface compatibleSurface = nullptr);
    void release();
    // the next events (callbacks) handled
    void process();
    WGPUBuffer buffer(uint64_t size, WGPUBufferUsage usage, const void* data = nullptr, const char* label = nullptr);
    WGPUTexture texture(uint32_t w, uint32_t h, WGPUTextureFormat format, WGPUTextureUsage usage, uint32_t mips = 1, const char* label = nullptr);
    void writeTexture(WGPUTexture t, uint32_t w, uint32_t h, const void* rgba, uint32_t mip = 0);
    WGPUShaderModule shader(const std::string& wgsl, const char* label);
    // an RGBA8 texture's pixels (rows tightly packed)
    std::vector<uint8_t> readback(WGPUTexture t, uint32_t w, uint32_t h);
    // queued work done
    void wait();
};

}  // namespace ssx_native
