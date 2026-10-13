// A frame of a replay, rendered headless (docs/native.md "N3"): the replay run on the native core to a tick as the page's verify
// path runs it (native/verify/replay_session.hpp), then the course world, the sky and the fog composite drawn offscreen, written
// as a PNG. The frame is the one the page draws right after stepping N + 2 ticks (ssxQA.advance: render alpha 0): the camera of
// tick N, the world state of tick N + 1 (native/tools/browser-shot.mjs takes the page's). No window: the automated visual-parity checks use it side by side with a page screenshot.
//   ssx3_render_shot ASSETS_DIR REPLAY.ssxr OUT.png [--tick N] [--size 640x480]
#include "gpu.hpp"
#include "image_io.hpp"
#include "replay_session.hpp"
#include "screen_tint.hpp"
#include "world_renderer.hpp"
#include <cstdio>
#include <memory>
#include <string>

using namespace ssx_native;
using namespace ssx_native::session;

int main(int argc, char** argv) {
    try {
        std::vector<std::string> positional;
        long tickTarget = 300;
        uint32_t width = 640;
        uint32_t height = 480;
        for (int k = 1; k < argc; ++k) {
            const std::string a = argv[k];
            if (a == "--tick" && k + 1 < argc) {
                tickTarget = std::stol(argv[++k]);
            } else if (a == "--size" && k + 1 < argc) {
                std::sscanf(argv[++k], "%ux%u", &width, &height);
            } else {
                positional.push_back(a);
            }
        }
        if (positional.size() != 3) {
            std::fprintf(stderr, "usage: ssx3_render_shot ASSETS_DIR REPLAY.ssxr OUT.png [--tick N] [--size 640x480]\n");
            return 2;
        }
        // the race, as the replay verifier sets it up
        Verifier v;
        v.assets.root = positional[0];
        auto file = readReplayFile(readFile(positional[1]));
        v.meta = file.meta;
        v.pad = std::move(file.pad);
        v.begin();
        v.loadCourse();
        v.loadRider();
        v.prepare();
        const json& attributes = at(v.meta["rider"], "attributes");
        if (attributes.is_array() && attributes.size() == 7) {
            int32_t raw[7];
            for (size_t k = 0; k < 7; ++k) {
                raw[k] = jsInt(attributes[k]);
            }
            set_rider_attributes(raw, 0);
        }
        v.presentationFlags();
        const std::string triggers = v.assets.textOr(v.code + "/camera-triggers.json", "");
        if (!triggers.empty()) {
            replay_camera_triggers(triggers.c_str());
        }
        v.startRun();
        replay_camera_begin(0x5d, 1);
        PadReader reader(v.pad);
        // ScreenTint steps once per camera tick after the core's tick (web/game-tick.js present)
        std::unique_ptr<ScreenTint> tint;
        if (v.assets.exists(v.courseRoot() + "screen-tint.json")) {
            tint = std::make_unique<ScreenTint>(nlohmann::json::parse(v.assets.text(v.courseRoot() + "screen-tint.json")));
        }
        float input[24];
        float cameraAt[9] = {};
        bool cameraSeen = false;
        uint32_t trace = 0;
        for (long t = 0; t <= tickTarget + 1; ++t) {
            reader.read(t, input);
            v.tick(input);
            if (tint) {
                tint->step(fog_info(), weather_info(), weather_lightning(), environment_world_loads());
            }
            if (t == tickTarget) {
                trace = v.traceHash();
                cameraSeen = v.hasCamera;
                std::memcpy(cameraAt, v.camera, sizeof cameraAt);
            }
        }
        if (!cameraSeen) {
            throw std::runtime_error("no camera on that tick");
        }
        // the frame's core state (main.js frame: fogRenderer.update, the set pieces' chunk residency)
        WorldFrameState state;
        const float* fog = fog_palette_info();
        state.fogEnabled = fog[0] != 0;
        state.fogRevision = uint32_t(fog[1]);
        state.fogSlope = fog[4];
        state.fogOffset = fog[5];
        state.fogPalette = fog_palette_rgba();
        if (tint) {
            state.tint = tint->output();
        }
        if (const uint32_t* c = section_chunks()) {
            for (uint32_t k = 0; k < c[0]; ++k) {
                state.chunks[c[1 + 2 * k]] = c[2 + 2 * k] == 3;
            }
        }
        CameraView camera;
        for (int k = 0; k < 9; ++k) {
            camera.v[size_t(k)] = cameraAt[k];
        }
        Gpu gpu;
        gpu.init();
        WorldRenderer renderer;
        renderer.load(gpu, v.assets.root, v.code);
        WGPUTexture target = gpu.texture(width, height, WGPUTextureFormat_RGBA8Unorm, WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc, 1, "shot");
        WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
        WGPUTextureView view = wgpuTextureCreateView(target, &vd);
        renderer.render(camera, state, view, width, height, 4.0 / 3.0);
        const auto pixels = gpu.readback(target, width, height);
        writePng(positional[2], int(width), int(height), pixels.data());
        std::printf("{\"tick\":%ld,\"trace\":%u,\"camera\":[%g,%g,%g,%g,%g,%g,%g,%g,%g],\"out\":%s}\n", tickTarget, trace, camera.v[0], camera.v[1], camera.v[2], camera.v[3], camera.v[4],
                    camera.v[5], camera.v[6], camera.v[7], camera.v[8], json(positional[2]).dump().c_str());
        std::fflush(stdout);
        std::_Exit(0);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "ssx3_render_shot: %s\n", e.what());
        return 1;
    }
}
