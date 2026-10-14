#include "gpu.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace ssx_native {

namespace {
std::string text(WGPUStringView s) {
    if (!s.data) {
        return {};
    }
    return s.length == WGPU_STRLEN ? std::string(s.data) : std::string(s.data, s.length);
}
void onError(WGPUDevice const*, WGPUErrorType type, WGPUStringView message, void*, void*) {
    // a callback from inside the implementation: no exception may cross it
    std::fprintf(stderr, "webgpu error %d: %s\n", int(type), text(message).c_str());
    std::fflush(stderr);
    std::abort();
}
}  // namespace

void Gpu::process() {
    wgpuInstanceProcessEvents(instance);
}

void Gpu::init(WGPUSurface compatibleSurface) {
    // an instance made before (for a window's surface) is kept
    if (!instance) {
        WGPUInstanceDescriptor desc = WGPU_INSTANCE_DESCRIPTOR_INIT;
        instance = wgpuCreateInstance(&desc);
    }
    if (!instance) {
        throw std::runtime_error("wgpuCreateInstance failed");
    }
    WGPURequestAdapterOptions options = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
    options.compatibleSurface = compatibleSurface;
    options.powerPreference = WGPUPowerPreference_HighPerformance;
    struct AdapterResult {
        bool done = false;
        WGPUAdapter adapter = nullptr;
        std::string message;
    } ar;
    WGPURequestAdapterCallbackInfo acb = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    acb.mode = WGPUCallbackMode_AllowProcessEvents;
    acb.callback = [](WGPURequestAdapterStatus status, WGPUAdapter a, WGPUStringView message, void* u1, void*) {
        auto* r = static_cast<AdapterResult*>(u1);
        r->done = true;
        r->adapter = status == WGPURequestAdapterStatus_Success ? a : nullptr;
        r->message = text(message);
    };
    acb.userdata1 = &ar;
    wgpuInstanceRequestAdapter(instance, &options, acb);
    while (!ar.done) {
        process();
    }
    if (!ar.adapter) {
        throw std::runtime_error("no WebGPU adapter: " + ar.message);
    }
    adapter = ar.adapter;
    WGPUDeviceDescriptor ddesc = WGPU_DEVICE_DESCRIPTOR_INIT;
    ddesc.label = sv("ssx3");
    ddesc.uncapturedErrorCallbackInfo.callback = onError;
    // the Xbox HD rider textures upload as BC blocks where the device has them (web/texture-archive.js configureTextureDecode)
    const WGPUFeatureName bcFeature = WGPUFeatureName_TextureCompressionBC;
    bc = allowBC && wgpuAdapterHasFeature(adapter, bcFeature);
    if (bc) {
        ddesc.requiredFeatureCount = 1;
        ddesc.requiredFeatures = &bcFeature;
    }
    struct DeviceResult {
        bool done = false;
        WGPUDevice device = nullptr;
        std::string message;
    } dr;
    WGPURequestDeviceCallbackInfo dcb = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    dcb.mode = WGPUCallbackMode_AllowProcessEvents;
    dcb.callback = [](WGPURequestDeviceStatus status, WGPUDevice d, WGPUStringView message, void* u1, void*) {
        auto* r = static_cast<DeviceResult*>(u1);
        r->done = true;
        r->device = status == WGPURequestDeviceStatus_Success ? d : nullptr;
        r->message = text(message);
    };
    dcb.userdata1 = &dr;
    wgpuAdapterRequestDevice(adapter, &ddesc, dcb);
    while (!dr.done) {
        process();
    }
    if (!dr.device) {
        throw std::runtime_error("no WebGPU device: " + dr.message);
    }
    device = dr.device;
    queue = wgpuDeviceGetQueue(device);
}

void Gpu::release() {
    if (queue) {
        wgpuQueueRelease(queue);
    }
    if (device) {
        wgpuDeviceRelease(device);
    }
    if (adapter) {
        wgpuAdapterRelease(adapter);
    }
    if (instance) {
        wgpuInstanceRelease(instance);
    }
    queue = nullptr;
    device = nullptr;
    adapter = nullptr;
    instance = nullptr;
}

WGPUBuffer Gpu::buffer(uint64_t size, WGPUBufferUsage usage, const void* data, const char* label) {
    WGPUBufferDescriptor d = WGPU_BUFFER_DESCRIPTOR_INIT;
    d.label = sv(label ? label : "");
    // writeBuffer needs a size that is a multiple of 4
    d.size = (size + 3) & ~uint64_t(3);
    d.usage = usage | WGPUBufferUsage_CopyDst;
    WGPUBuffer b = wgpuDeviceCreateBuffer(device, &d);
    if (data && size) {
        if (size % 4 == 0) {
            wgpuQueueWriteBuffer(queue, b, 0, data, size_t(size));
        } else {
            std::vector<uint8_t> padded(size_t(d.size), 0);
            std::memcpy(padded.data(), data, size_t(size));
            wgpuQueueWriteBuffer(queue, b, 0, padded.data(), padded.size());
        }
    }
    return b;
}

WGPUTexture Gpu::texture(uint32_t w, uint32_t h, WGPUTextureFormat format, WGPUTextureUsage usage, uint32_t mips, const char* label) {
    WGPUTextureDescriptor d = WGPU_TEXTURE_DESCRIPTOR_INIT;
    d.label = sv(label ? label : "");
    d.size = {w, h, 1};
    d.format = format;
    d.usage = usage;
    d.mipLevelCount = mips;
    d.dimension = WGPUTextureDimension_2D;
    d.sampleCount = 1;
    return wgpuDeviceCreateTexture(device, &d);
}

void Gpu::writeTexture(WGPUTexture t, uint32_t w, uint32_t h, const void* rgba, uint32_t mip) {
    WGPUTexelCopyTextureInfo dst = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    dst.texture = t;
    dst.mipLevel = mip;
    WGPUTexelCopyBufferLayout layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
    layout.bytesPerRow = w * 4;
    layout.rowsPerImage = h;
    WGPUExtent3D size{w, h, 1};
    wgpuQueueWriteTexture(queue, &dst, rgba, size_t(w) * h * 4, &layout, &size);
}

void Gpu::writeBlocks(WGPUTexture t, uint32_t w, uint32_t h, const void* blocks, uint32_t blockBytes, uint32_t mip) {
    WGPUTexelCopyTextureInfo dst = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    dst.texture = t;
    dst.mipLevel = mip;
    const uint32_t bw = (w + 3) / 4;
    const uint32_t bh = (h + 3) / 4;
    WGPUTexelCopyBufferLayout layout = WGPU_TEXEL_COPY_BUFFER_LAYOUT_INIT;
    layout.bytesPerRow = bw * blockBytes;
    layout.rowsPerImage = bh;
    // the copy covers whole blocks: a level below 4 x 4 is still one block (its physical size)
    WGPUExtent3D size{bw * 4, bh * 4, 1};
    wgpuQueueWriteTexture(queue, &dst, blocks, size_t(bw) * bh * blockBytes, &layout, &size);
}

WGPUShaderModule Gpu::shader(const std::string& wgsl, const char* label) {
    WGPUShaderSourceWGSL src = WGPU_SHADER_SOURCE_WGSL_INIT;
    src.code = sv(wgsl);
    WGPUShaderModuleDescriptor d = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    d.nextInChain = &src.chain;
    d.label = sv(label);
    return wgpuDeviceCreateShaderModule(device, &d);
}

void Gpu::wait() {
    bool done = false;
    WGPUQueueWorkDoneCallbackInfo cb = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
    cb.mode = WGPUCallbackMode_AllowProcessEvents;
    cb.callback = [](WGPUQueueWorkDoneStatus, WGPUStringView, void* u1, void*) { *static_cast<bool*>(u1) = true; };
    cb.userdata1 = &done;
    wgpuQueueOnSubmittedWorkDone(queue, cb);
    while (!done) {
        process();
    }
}

std::vector<uint8_t> Gpu::readback(WGPUTexture t, uint32_t w, uint32_t h) {
    const uint32_t row = (w * 4 + 255) / 256 * 256;
    WGPUBufferDescriptor d = WGPU_BUFFER_DESCRIPTOR_INIT;
    d.size = uint64_t(row) * h;
    d.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    WGPUBuffer b = wgpuDeviceCreateBuffer(device, &d);
    WGPUCommandEncoderDescriptor ed = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(device, &ed);
    WGPUTexelCopyTextureInfo src = WGPU_TEXEL_COPY_TEXTURE_INFO_INIT;
    src.texture = t;
    WGPUTexelCopyBufferInfo dst = WGPU_TEXEL_COPY_BUFFER_INFO_INIT;
    dst.buffer = b;
    dst.layout.bytesPerRow = row;
    dst.layout.rowsPerImage = h;
    WGPUExtent3D size{w, h, 1};
    wgpuCommandEncoderCopyTextureToBuffer(enc, &src, &dst, &size);
    WGPUCommandBufferDescriptor cd = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, &cd);
    wgpuQueueSubmit(queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
    wgpuCommandEncoderRelease(enc);
    bool done = false;
    WGPUBufferMapCallbackInfo cb = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;
    cb.mode = WGPUCallbackMode_AllowProcessEvents;
    cb.callback = [](WGPUMapAsyncStatus status, WGPUStringView message, void* u1, void*) {
        if (status != WGPUMapAsyncStatus_Success) {
            std::fprintf(stderr, "map failed: %s\n", text(message).c_str());
        }
        *static_cast<bool*>(u1) = true;
    };
    cb.userdata1 = &done;
    wgpuBufferMapAsync(b, WGPUMapMode_Read, 0, size_t(d.size), cb);
    while (!done) {
        process();
    }
    const auto* p = static_cast<const uint8_t*>(wgpuBufferGetConstMappedRange(b, 0, size_t(d.size)));
    std::vector<uint8_t> out(size_t(w) * h * 4);
    for (uint32_t y = 0; y < h; ++y) {
        std::memcpy(out.data() + size_t(y) * w * 4, p + size_t(y) * row, size_t(w) * 4);
    }
    wgpuBufferUnmap(b);
    wgpuBufferRelease(b);
    return out;
}

}  // namespace ssx_native
