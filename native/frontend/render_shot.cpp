// A frame of a replay, rendered headless (docs/native.md "N3"): the replay run on the native core as the page's verify path runs it
// (native/verify/replay_session.hpp), then the world, the sky, the fog composite and the riders drawn offscreen, written as a PNG.
// The frame is the one the page draws right after stepping N + 2 ticks (ssxQA.advance: render alpha 0): the camera and palettes of
// tick N over the world state of tick N + 1 (native/tools/browser-shot.mjs takes the page's). No window.
//   ssx3_render_shot ASSETS_DIR REPLAY.ssxr OUT.png [--tick N] [--size 640x480]
#include "gpu.hpp"
#include "image_io.hpp"
#include "race_presenter.hpp"
#include "replay_session.hpp"
#include "world_renderer.hpp"
#include <cstdio>
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
        // the race, as the replay verifier sets it up, with the presentation set-up (the riders' lighting)
        Verifier v;
        v.presentation = true;
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
        RacePresenter presenter(v);
        PadReader reader(v.pad);
        float input[24];
        uint32_t trace = 0;
        for (long t = 0; t <= tickTarget + 1; ++t) {
            reader.read(t, input);
            v.tick(input);
            presenter.afterTick();
            if (t == tickTarget) {
                trace = v.traceHash();
            }
        }
        if (!presenter.cameraValid()) {
            throw std::runtime_error("no camera yet");
        }
        Gpu gpu;
        gpu.init();
        WorldRenderer renderer;
        renderer.load(gpu, v.assets.root, v.code);
        presenter.loadRiders(gpu);
        CameraView camera;
        WorldFrameState state = presenter.frame(0, camera);
        WGPUTexture target = gpu.texture(width, height, WGPUTextureFormat_RGBA8Unorm, WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc, 1, "shot");
        WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
        WGPUTextureView view = wgpuTextureCreateView(target, &vd);
        renderer.render(camera, state, view, width, height, 4.0 / 3.0);
        const auto pixels = gpu.readback(target, width, height);
        writePng(positional[2], int(width), int(height), pixels.data());
        std::printf("{\"tick\":%ld,\"trace\":%u,\"riders\":%zu,\"camera\":[%g,%g,%g,%g,%g,%g,%g,%g,%g],\"out\":%s}\n", tickTarget, trace, state.riders.size(),
                    camera.v[0], camera.v[1], camera.v[2], camera.v[3], camera.v[4], camera.v[5], camera.v[6], camera.v[7], camera.v[8],
                    json(positional[2]).dump().c_str());
        std::fflush(stdout);
        std::_Exit(0);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "ssx3_render_shot: %s\n", e.what());
        return 1;
    }
}
