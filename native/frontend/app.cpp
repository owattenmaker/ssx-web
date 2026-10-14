// The native front end, minimal (docs/native.md "N3"): an SDL3 window, the core ticking at 60 Hz with the page's pacing
// (web/tick-lock.js / fixed-step-clock.js), the pad from the keyboard and gamepads exactly as web/pad-input.js builds it, and the
// course, sky, fog, ScreenTint, the skinned riders and the race HUD drawn with webgpu.h (wgpu-native), and the game audio
// (native/frontend/game_audio.hpp) out through SDL3's audio stream.
//   ssx3 ASSETS_DIR [--course ARA1] [--rider zoe]   a live Snow Jam time trial (the solo lineup)
//        [--start RUN.ssxr]                          the start (course, rider, lineup, start state) of a replay, played live
//        [--watch RUN.ssxr]                          a replay's own pad
//        [--shot OUT.png --shot-tick N]              write the frame after tick N and quit
//        [--hidden]                                  no window on screen (automated runs)
//        [--focus]                                   take keyboard focus (by default the window never takes it unasked)
//        [--at X,Y]                                  the window's position
//        [--quality low|medium|high]                 the page's quality tier (default high, a desktop's: frontend/quality.hpp)
//        [--riders ps2|xbox] [--bc 0|1]              the rider texture set (tier default), BC upload (default on where supported)
//        [--mute]                                    no audio device (the audio renders silently; --hidden mutes too)
//        [--audio-log OUT.json]                      the director timeline and the voice requests, written with the shot
// Keys: web/pad-input.js Classic (WASD / arrows left stick, Space Cross, Shift Square, ...); Escape quits.
#include "gpu.hpp"
#include "image_io.hpp"
#include "pacing.hpp"
#include "pad_input.hpp"
#include "quality.hpp"
#include "replay_session.hpp"
#include "game_audio.hpp"
#include "race_hud.hpp"
#include "race_presenter.hpp"
#include "world_renderer.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_metal.h>
#include <atomic>
#include <thread>
#include <cstdio>
#include <fstream>
#include <memory>
#include <set>
#include <string>

using namespace ssx_native;
using namespace ssx_native::session;

namespace {

// SDL scancodes -> KeyboardEvent.code (the names web/pad-input.js reads)
std::string codeOf(SDL_Scancode s) {
    if (s >= SDL_SCANCODE_A && s <= SDL_SCANCODE_Z) {
        return std::string("Key") + char('A' + (s - SDL_SCANCODE_A));
    }
    switch (s) {
        case SDL_SCANCODE_LEFT:
            return "ArrowLeft";
        case SDL_SCANCODE_RIGHT:
            return "ArrowRight";
        case SDL_SCANCODE_UP:
            return "ArrowUp";
        case SDL_SCANCODE_DOWN:
            return "ArrowDown";
        case SDL_SCANCODE_SPACE:
            return "Space";
        case SDL_SCANCODE_LSHIFT:
            return "ShiftLeft";
        case SDL_SCANCODE_RSHIFT:
            return "ShiftRight";
        case SDL_SCANCODE_BACKSPACE:
            return "Backspace";
        case SDL_SCANCODE_RETURN:
            return "Enter";
        default:
            return "";
    }
}

// an SDL gamepad as the W3C standard layout (the browsers' "standard" mapping)
StandardPad padOf(SDL_Gamepad* g) {
    StandardPad p;
    if (!g) {
        return p;
    }
    p.connected = true;
    const SDL_GamepadButton buttons[17] = {
        SDL_GAMEPAD_BUTTON_SOUTH,      SDL_GAMEPAD_BUTTON_EAST,      SDL_GAMEPAD_BUTTON_WEST,       SDL_GAMEPAD_BUTTON_NORTH,
        SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, SDL_GAMEPAD_BUTTON_INVALID, SDL_GAMEPAD_BUTTON_INVALID,
        SDL_GAMEPAD_BUTTON_BACK,       SDL_GAMEPAD_BUTTON_START,     SDL_GAMEPAD_BUTTON_LEFT_STICK, SDL_GAMEPAD_BUTTON_RIGHT_STICK,
        SDL_GAMEPAD_BUTTON_DPAD_UP,    SDL_GAMEPAD_BUTTON_DPAD_DOWN, SDL_GAMEPAD_BUTTON_DPAD_LEFT,  SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
        SDL_GAMEPAD_BUTTON_GUIDE,
    };
    for (int k = 0; k < 17; ++k) {
        if (k == 6 || k == 7) {
            // the triggers: an axis 0..32767, "pressed" past Chromium's threshold
            const double v = SDL_GetGamepadAxis(g, k == 6 ? SDL_GAMEPAD_AXIS_LEFT_TRIGGER : SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32767.0;
            p.buttons[size_t(k)].value = std::clamp(v, 0.0, 1.0);
            p.buttons[size_t(k)].pressed = v > 30.0 / 255.0;
        } else {
            const bool down = SDL_GetGamepadButton(g, buttons[k]);
            p.buttons[size_t(k)].pressed = down;
            p.buttons[size_t(k)].value = down ? 1 : 0;
        }
    }
    const SDL_GamepadAxis axes[4] = {SDL_GAMEPAD_AXIS_LEFTX, SDL_GAMEPAD_AXIS_LEFTY, SDL_GAMEPAD_AXIS_RIGHTX, SDL_GAMEPAD_AXIS_RIGHTY};
    for (int k = 0; k < 4; ++k) {
        p.axes[size_t(k)] = std::clamp(SDL_GetGamepadAxis(g, axes[k]) / 32767.0, -1.0, 1.0);
    }
    return p;
}

const char* kBlitShader = R"(
@group(0) @binding(0) var frameTex: texture_2d<f32>;
@group(0) @binding(1) var frameSampler: sampler;
struct Out { @builtin(position) position: vec4f, @location(0) uv: vec2f };
@vertex fn vs(@builtin(vertex_index) i: u32) -> Out {
  let p = array<vec2f, 3>(vec2f(-1.0, -1.0), vec2f(3.0, -1.0), vec2f(-1.0, 3.0));
  var o: Out;
  o.position = vec4f(p[i], 0.0, 1.0);
  o.uv = vec2f(p[i].x * 0.5 + 0.5, 0.5 - p[i].y * 0.5);
  return o;
}
@fragment fn fs(i: Out) -> @location(0) vec4f {
  return vec4f(textureSample(frameTex, frameSampler, i.uv).rgb, 1.0);
}
)";

}  // namespace

// a muted run's audio device: renders the context into a scratch buffer at the wall-clock rate, in 5 ms steps, on its own thread
// (the same lock the device callback takes); stopped and joined before the context goes
class SilentDevice {
public:
    void start(wa::Context& ctx) {
        thread_ = std::thread([this, &ctx] {
            const uint64_t start = SDL_GetTicksNS();
            uint64_t rendered = 0;
            std::vector<float> scratch;
            while (!quit_) {
                const uint64_t want = uint64_t(double(SDL_GetTicksNS() - start) / 1e9 * ctx.sampleRate());
                while (want > rendered && !quit_) {
                    const size_t n = size_t(std::min<uint64_t>(want - rendered, 1024));
                    scratch.resize(n * 2);
                    ctx.render(scratch.data(), n);
                    rendered += n;
                }
                SDL_DelayNS(5000000);
            }
        });
    }
    ~SilentDevice() {
        quit_ = true;
        if (thread_.joinable()) {
            thread_.join();
        }
    }

private:
    std::thread thread_;
    std::atomic<bool> quit_{false};
};

int main(int argc, char** argv) {
    std::string assets;
    std::string course = "ARA1";
    std::string riderId = "zoe";
    std::string startFile;
    std::string watchFile;
    std::string shotPath;
    long shotTick = -1;
    bool hidden = false;
    bool mute = false;
    std::string audioLog;
    bool focus = false;
    std::string windowAt;
    std::string qualityTier = "high";
    std::string riderSet;
    std::string bcFlag;
    for (int k = 1; k < argc; ++k) {
        const std::string a = argv[k];
        auto next = [&] { return k + 1 < argc ? std::string(argv[++k]) : std::string(); };
        if (a == "--course") {
            course = next();
        } else if (a == "--rider") {
            riderId = next();
        } else if (a == "--start") {
            startFile = next();
        } else if (a == "--watch") {
            watchFile = next();
        } else if (a == "--shot") {
            shotPath = next();
        } else if (a == "--shot-tick") {
            shotTick = std::stol(next());
        } else if (a == "--audio-log") {
            audioLog = next();
        } else if (a == "--mute") {
            mute = true;
        } else if (a == "--hidden") {
            hidden = true;
        } else if (a == "--focus") {
            focus = true;
        } else if (a == "--at") {
            windowAt = next();
        } else if (a == "--quality") {
            qualityTier = next();
        } else if (a == "--riders") {
            riderSet = next();
        } else if (a == "--bc") {
            bcFlag = next();
        } else {
            assets = a;
        }
    }
    if (assets.empty()) {
        std::fprintf(stderr, "usage: ssx3 ASSETS_DIR [--course C] [--rider R] [--start RUN.ssxr] [--watch RUN.ssxr] [--shot OUT.png --shot-tick N] [--hidden] [--focus] [--quality T] [--riders S]\n");
        return 2;
    }
    try {
        Quality quality = Quality::tierDefaults(qualityTier);
        if (!riderSet.empty()) {
            quality.setRiders(riderSet);
        }
        quality.bc = bcFlag != "0";
        // the window: never activated unasked (the user may be typing elsewhere), no Dock icon unless --focus
        if (!focus) {
            SDL_SetHint(SDL_HINT_MAC_BACKGROUND_APP, "1");
            SDL_SetHint(SDL_HINT_WINDOW_ACTIVATE_WHEN_SHOWN, "0");
        }
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
            throw std::runtime_error(std::string("SDL_Init: ") + SDL_GetError());
        }
        SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
        if (hidden) {
            flags |= SDL_WINDOW_HIDDEN;
        }
        if (!focus) {
            flags |= SDL_WINDOW_NOT_FOCUSABLE;
        }
        SDL_Window* window = SDL_CreateWindow("SSX 3", 960, 720, flags);
        if (!window) {
            throw std::runtime_error(std::string("SDL_CreateWindow: ") + SDL_GetError());
        }
        // --at X,Y: the window's place (automated runs put it on a second screen, away from the user's work)
        if (!windowAt.empty()) {
            int x = 0;
            int y = 0;
            if (std::sscanf(windowAt.c_str(), "%d,%d", &x, &y) == 2) {
                SDL_SetWindowPosition(window, x, y);
            }
        }

        // the race: a replay's start, or a live time trial
        Verifier v;
        v.presentation = true;
        v.assets.root = assets;
        const std::string from = !watchFile.empty() ? watchFile : startFile;
        if (!from.empty()) {
            auto file = readReplayFile(readFile(from));
            v.meta = file.meta;
            v.pad = std::move(file.pad);
            v.live = watchFile.empty();
        } else {
            v.meta = liveMeta(course, riderId);
            v.live = true;
        }
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
        v.presentationFast = quality.presentationFast;
        v.presentationFlags();
        v.startRun();
        // automated (hidden) runs never play sound on the user's machine
        if (hidden) {
            mute = true;
        }

        // the GPU on the window's surface
        WGPUSurfaceDescriptor sdesc = WGPU_SURFACE_DESCRIPTOR_INIT;
#if defined(__APPLE__)
        SDL_MetalView metalView = SDL_Metal_CreateView(window);
        WGPUSurfaceSourceMetalLayer metal = WGPU_SURFACE_SOURCE_METAL_LAYER_INIT;
        metal.layer = SDL_Metal_GetLayer(metalView);
        sdesc.nextInChain = &metal.chain;
#elif defined(_WIN32)
        WGPUSurfaceSourceWindowsHWND hwnd = WGPU_SURFACE_SOURCE_WINDOWS_HWND_INIT;
        hwnd.hwnd = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
        hwnd.hinstance = SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_INSTANCE_POINTER, nullptr);
        sdesc.nextInChain = &hwnd.chain;
#elif defined(__linux__)
        // X11 or Wayland, as SDL's video driver chose (SDL_VIDEO_DRIVER overrides)
        const SDL_PropertiesID props = SDL_GetWindowProperties(window);
        WGPUSurfaceSourceWaylandSurface wayland = WGPU_SURFACE_SOURCE_WAYLAND_SURFACE_INIT;
        WGPUSurfaceSourceXlibWindow xlib = WGPU_SURFACE_SOURCE_XLIB_WINDOW_INIT;
        const char* driver = SDL_GetCurrentVideoDriver();
        if (driver && std::string(driver) == "wayland") {
            wayland.display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER, nullptr);
            wayland.surface = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
            sdesc.nextInChain = &wayland.chain;
        } else if (driver && std::string(driver) == "x11") {
            xlib.display = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, nullptr);
            xlib.window = uint64_t(SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0));
            sdesc.nextInChain = &xlib.chain;
        } else {
            throw std::runtime_error(std::string("the native front end's surface: X11 or Wayland, not ") + (driver ? driver : "no video driver"));
        }
#else
#error "the native front end's surface: macOS (Metal), Windows (HWND) and Linux (X11 / Wayland) so far"
#endif
        Gpu gpu;
        WGPUInstanceDescriptor idesc = WGPU_INSTANCE_DESCRIPTOR_INIT;
        gpu.instance = wgpuCreateInstance(&idesc);
        WGPUSurface surface = wgpuInstanceCreateSurface(gpu.instance, &sdesc);
        gpu.allowBC = quality.bc;
        gpu.init(surface);
        WGPUSurfaceCapabilities caps = WGPU_SURFACE_CAPABILITIES_INIT;
        wgpuSurfaceGetCapabilities(surface, gpu.adapter, &caps);
        // a non-sRGB format: the frame is GS bytes, shown as they are (pv encodedBlend)
        WGPUTextureFormat format = caps.formats[0];
        for (size_t k = 0; k < caps.formatCount; ++k) {
            if (caps.formats[k] == WGPUTextureFormat_BGRA8Unorm || caps.formats[k] == WGPUTextureFormat_RGBA8Unorm) {
                format = caps.formats[k];
                break;
            }
        }
        int pw = 0;
        int ph = 0;
        auto configure = [&] {
            SDL_GetWindowSizeInPixels(window, &pw, &ph);
            WGPUSurfaceConfiguration sc = WGPU_SURFACE_CONFIGURATION_INIT;
            sc.device = gpu.device;
            sc.format = format;
            sc.usage = WGPUTextureUsage_RenderAttachment;
            sc.width = uint32_t(std::max(1, pw));
            sc.height = uint32_t(std::max(1, ph));
            sc.presentMode = WGPUPresentMode_Fifo;
            sc.alphaMode = WGPUCompositeAlphaMode_Auto;
            wgpuSurfaceConfigure(surface, &sc);
        };
        configure();

        WorldRenderer renderer;
        renderer.load(gpu, assets, v.code);
        // the frame: 640 x 480 (the PS2's 4:3 frame), shown scaled into the window
        const uint32_t fw = 640;
        const uint32_t fh = 480;
        WGPUTexture frameTex = gpu.texture(fw, fh, WGPUTextureFormat_RGBA8Unorm,
                                           WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopySrc, 1, "frame");
        WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
        WGPUTextureView frameView = wgpuTextureCreateView(frameTex, &vd);
        // the blit into the window
        WGPUShaderModule blit = gpu.shader(kBlitShader, "blit");
        WGPURenderPipelineDescriptor pd = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        pd.vertex.module = blit;
        pd.vertex.entryPoint = sv("vs");
        WGPUColorTargetState ct = WGPU_COLOR_TARGET_STATE_INIT;
        ct.format = format;
        ct.writeMask = WGPUColorWriteMask_All;
        WGPUFragmentState fs = WGPU_FRAGMENT_STATE_INIT;
        fs.module = blit;
        fs.entryPoint = sv("fs");
        fs.targetCount = 1;
        fs.targets = &ct;
        pd.fragment = &fs;
        pd.primitive.topology = WGPUPrimitiveTopology_TriangleList;
        pd.multisample.count = 1;
        pd.multisample.mask = 0xffffffffu;
        WGPURenderPipeline blitPipeline = wgpuDeviceCreateRenderPipeline(gpu.device, &pd);
        WGPUSamplerDescriptor sd = WGPU_SAMPLER_DESCRIPTOR_INIT;
        sd.magFilter = WGPUFilterMode_Linear;
        sd.minFilter = WGPUFilterMode_Linear;
        WGPUSampler linear = wgpuDeviceCreateSampler(gpu.device, &sd);
        WGPUBindGroupEntry be[2];
        be[0] = WGPU_BIND_GROUP_ENTRY_INIT;
        be[0].binding = 0;
        be[0].textureView = frameView;
        be[1] = WGPU_BIND_GROUP_ENTRY_INIT;
        be[1].binding = 1;
        be[1].sampler = linear;
        WGPUBindGroupDescriptor bd = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
        bd.layout = wgpuRenderPipelineGetBindGroupLayout(blitPipeline, 0);
        bd.entryCount = 2;
        bd.entries = be;
        WGPUBindGroup blitGroup = wgpuDeviceCreateBindGroup(gpu.device, &bd);

        RacePresenter presenter(v);
        presenter.riderTextures = quality.riderTextures;
        presenter.loadEffects(renderer);
        // the race HUD (native/frontend/race_hud.hpp) on the UI canvas over the frame
        // the game audio (web/game-audio.js natively): the event load, the world up, the run started
        audio::GameAudio::Options ao;
        ao.root = assets + "/AUDIO";
        ao.muted = mute;
        auto gameAudio = std::make_unique<audio::GameAudio>(ao);
        if (!audioLog.empty()) {
            gameAudio->sfx().logPlays(true);
        }
        const std::string riderName = str(at(v.entry, "id"), riderId);
        const int courseIndex = audio::courseIndexOf(v.code);
        // a fresh profile's rider record (web/audio-menu.js riderMusic: no playlist, the global radio mode); the FE flow's profile replaces it
        gameAudio->riderMusic = [&gameAudio]() -> std::optional<audio::GameAudio::RiderMusic> {
            audio::GameAudio::RiderMusic m;
            m.radioMode = gameAudio->settings().radioMode;
            m.playlist.assign(35, false);
            return m;
        };
        gameAudio->loadingStart(v.code, riderName);
        SDL_AudioStream* audioStream = nullptr;
        if (!mute) {
            if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
                SDL_AudioSpec spec{SDL_AUDIO_F32, 2, int(gameAudio->context().sampleRate())};
                audioStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
                    [](void* user, SDL_AudioStream* stream, int additional, int) {
                        auto* ctx = static_cast<wa::Context*>(user);
                        const int frames = additional / 8;
                        if (frames <= 0) {
                            return;
                        }
                        std::vector<float> buf(size_t(frames) * 2);
                        ctx->render(buf.data(), size_t(frames));
                        SDL_PutAudioStreamData(stream, buf.data(), frames * 8);
                    },
                    &gameAudio->context());
                if (audioStream) {
                    SDL_ResumeAudioStreamDevice(audioStream);
                } else {
                    std::fprintf(stderr, "No audio device: %s\n", SDL_GetError());
                }
            }
        }
        // muted (or no device): a silent device, the graph rendered on its own thread at the wall-clock rate (its clock, the voices'
        // ends, the music's pumps run on through the main thread's long calls as a device's do)
        SilentDevice silentDevice;
        if (!audioStream) {
            silentDevice.start(gameAudio->context());
        }
        float audioPhase = -1;
        std::set<const void*> npcFinished;
        // the camera of the last frame drawn (the audio listener)
        std::array<double, 16> listenerInverse{};
        std::array<double, 3> listenerEye{};
        bool listenerKnown = false;
        // main.js frame before the run's first tick (cameraPose null): the fallback chase camera, running, behind the rider (4.8 m back,
        // 2.5 m up, over the ground), settled on its first frame (lastCamera from 0 copies desired) and held above the floor; looking 1 m
        // ahead of the rider at 1.1 m
        {
            const float* st = rider_state();
            const auto& o = renderer.origin;
            const double pos[3] = {st[0] - o[0], st[1] - o[1], st[2] - o[2]};
            const bool grounded = st[8] != 0;
            const double normal[3] = {grounded ? st[4] : 0.0, grounded ? st[5] : 1.0, grounded ? st[6] : 0.0};
            double forward[3] = {std::sin(double(st[3])), 0, std::cos(double(st[3]))};
            const double d = forward[0] * normal[0] + forward[1] * normal[1] + forward[2] * normal[2];
            for (int k = 0; k < 3; ++k) {
                forward[k] -= normal[k] * d;
            }
            const double fl = std::sqrt(forward[0] * forward[0] + forward[1] * forward[1] + forward[2] * forward[2]);
            for (double& c : forward) {
                c = fl > 0 ? c / fl : 0;
            }
            double eye[3] = {pos[0] - forward[0] * 4.8, pos[1] - forward[1] * 4.8 + 2.5, pos[2] - forward[2] * 4.8};
            const double groundCam = double(height_at(float(eye[0] + o[0]), float(eye[1] + o[1] + 4), float(eye[2] + o[2]))) - o[1];
            if (groundCam > -1e8) {
                eye[1] = std::max(eye[1], groundCam + 1.2);
            }
            const double floor = double(height_at(float(eye[0] + o[0]), float(eye[1] + o[1] + 25), float(eye[2] + o[2]))) - o[1];
            if (floor > -1e8) {
                eye[1] = std::max(eye[1], floor + 1.6);
            }
            const double look[3] = {pos[0] + forward[0], pos[1] + forward[1] + 1.1, pos[2] + forward[2]};
            listenerInverse = WorldRenderer::lookAt(eye, look);
            listenerEye = {eye[0], eye[1], eye[2]};
            listenerKnown = true;
        }
        // QA (--audio-log): the sfx log's length after each tick
        std::vector<size_t> playCounts;
        audio::Json listenerLog = audio::Json::array();
        std::optional<std::array<double, 3>> heardFrom;
        bool audioFinished = false;
        RaceHud hud;
        hud.load(gpu, v);
        hud.begin(v);
        v.afterMid = [&] { hud.afterMid(v); };
        // the EA RADIO BIG popup (web/audio-menu.js subscribe: not while the race music is off)
        gameAudio->onNowPlaying = [&hud, &gameAudio](const std::string& title, const std::string& artist, const std::string& album) {
            if (!gameAudio->raceMusic()) {
                return;
            }
            hud.nowPlaying.show({title, artist, album});
        };
        // the load screen ends (main.js audioScreens 'loading' -> 'game': worldLoaded), then startRun's runStart
        gameAudio->worldLoaded(courseIndex, true, v.code, riderName);
        gameAudio->runStart(courseIndex, true, v.code, riderName);
        // the frames before the first tick (the objectives screen) hear from the fallback camera already
        gameAudio->listener(&listenerInverse, listenerEye, renderer.origin);
        heardFrom = listenerEye;
        // web/input-glyphs.js: the device of the last input (a key, a pad button), else a connected pad, else the keyboard
        std::string lastDevice;
        PadReader reader(v.pad);
        KeyboardContext keyboard;
        std::set<std::string> keys;
        SDL_Gamepad* gamepad = nullptr;
        TickLock tickLock;
        FixedStepClock clock;
        long tick = 0;
        uint64_t last = SDL_GetTicksNS();
        uint64_t pacedAt = 0;
        bool running = true;
        while (running) {
            SDL_Event e;
            while (SDL_PollEvent(&e)) {
                if (e.type == SDL_EVENT_QUIT) {
                    running = false;
                } else if (e.type == SDL_EVENT_KEY_DOWN) {
                    if (e.key.scancode == SDL_SCANCODE_ESCAPE) {
                        running = false;
                    }
                    keys.insert(codeOf(e.key.scancode));
                    lastDevice = "keyboard";
                } else if (e.type == SDL_EVENT_KEY_UP) {
                    keys.erase(codeOf(e.key.scancode));
                } else if (e.type == SDL_EVENT_GAMEPAD_ADDED && !gamepad) {
                    gamepad = SDL_OpenGamepad(e.gdevice.which);
                } else if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
                    lastDevice = "gamepad";
                } else if (e.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
                    configure();
                }
            }
            const uint64_t now = SDL_GetTicksNS();
            const double dt = double(now - last) / 1e9;
            last = now;
            // web/main.js frame: tickLock, else the fixed clock (a shot run steps exactly one tick a frame and draws render alpha 0, as
            // a page frame right after ssxQA.advance does: native/tools/browser-shot.mjs, ssx3_render_shot)
            int ticks;
            double alpha = 1;
            if (!shotPath.empty()) {
                ticks = 1;
                alpha = 0;
                // --audio-log: one tick per 60 Hz frame, as browser-shot.mjs --paced steps the page (the audio's wall-clock timings)
                if (!audioLog.empty()) {
                    const uint64_t frameNs = 1000000000ull / 60;
                    const uint64_t since = now - pacedAt;
                    if (pacedAt != 0 && since < frameNs) {
                        SDL_DelayPrecise(frameNs - since);
                    }
                    pacedAt = SDL_GetTicksNS();
                }
            } else if (auto n = tickLock.frame(dt)) {
                ticks = *n;
                clock.reset();
            } else {
                ticks = clock.advance(std::min(ps2FrameTime(dt), 0.25));
                alpha = clock.alpha();
            }
            for (int k = 0; k < ticks; ++k) {
                float pad[24];
                if (!watchFile.empty()) {
                    reader.read(tick, pad);
                } else {
                    const StandardPad sp = padOf(gamepad);
                    keyboard.air = v.command14 == 4 || v.command14 == 5;
                    const auto channels = buildPad([&](const std::string& c) { return keys.count(c) > 0; }, &sp, &keyboard);
                    std::memcpy(pad, channels.data(), sizeof pad);
                }
                hud.beforeTick();
                v.tick(pad);
                presenter.afterTick();
                hud.afterTick(v);
                // the game audio tick (web/game-tick.js present): the cores' audio queues, painters, speech
                {
                    audio::GameAudio::TickState ts;
                    ts.raceInfo = v.raceInfo;
                    ts.finished = v.finished;
                    ts.pending = v.pending;
                    ts.character = riderName;
                    ts.running = v.running;
                    ts.place = v.aiActive ? std::optional<int>(int(v.worldState[360])) : std::optional<int>(0);
                    audio::GameTickRider human;
                    human.key = &v;
                    human.character = riderName;
                    human.slot = 0;
                    human.human = true;
                    human.finished = v.finished;
                    human.in = [](const std::function<void()>& fn) { fn(); };
                    ts.riders.push_back(human);
                    if (v.aiActive) {
                        for (size_t k = 0; k < v.npcs.size(); ++k) {
                            const auto& n = v.npcs[k];
                            // web/ai-racers.js: manager_rider(k + 1)[33] latches the computer rider's finish
                            if (manager_rider(int(k) + 1)[33] != 0) {
                                npcFinished.insert(&n);
                            }
                            audio::GameTickRider r;
                            r.key = &n;
                            r.character = str(at(n.record, "character"));
                            r.slot = n.slot;
                            r.human = false;
                            r.finished = npcFinished.count(&n) > 0;
                            r.in = [&v, &n](const std::function<void()>& fn) { v.inNpc(n, fn); };
                            ts.riders.push_back(r);
                        }
                        // the core's relationship records and this tick's pair reactions (web/ai-race.js drainCoreRelationships)
                        ts.ai = true;
                        const int32_t* ev = manager_relationship_events();
                        for (int32_t e = 0; e < ev[0]; ++e) {
                            const int32_t* q = ev + 1 + 7 * e;
                            ts.pairEvents.push_back({q[0], q[1], q[4] < 0 ? -1 : q[4]});
                        }
                        ts.relations = []() -> std::optional<audio::Relations> {
                            const int32_t* st = manager_relationship_state();
                            audio::Relations m;
                            for (size_t a = 0; a < 6; ++a) {
                                for (size_t b = 0; b < 6; ++b) {
                                    m.scores[a][b] = st[900 + a * 6 + b];
                                    m.kinds[a][b] = st[936 + a * 6 + b];
                                    // relSnapshot: none while a kind is missing
                                    if (a != b && m.kinds[a][b] < 0) {
                                        return std::nullopt;
                                    }
                                }
                            }
                            return m;
                        };
                    }
                    // the human core's avalanches, read once a tick (the draw reads the same snapshot)
                    if (auto* av = presenter.avalanche()) {
                        av->read();
                        audio::GameAudio::TickState::Avalanche a;
                        a.loop = av->loop();
                        a.tumblers = av->tumblers();
                        a.loopEvents = av->takeLoopEvents();
                        ts.avalanche = a;
                    }
                    ts.hudSlots = [] {
                        const float* raw = score_hud_slots();
                        return std::optional<std::vector<float>>(std::vector<float>(raw, raw + 44 * 6));
                    };
                    ts.timedOut = [] { return race_timed_out() != 0; };
                    ts.contactTrack = [] { return int(terrain_contact_info()[0]); };
                    ts.instancePosition = [](uint32_t resource) -> std::optional<audio::Vec> {
                        const float* p = audio_instance_position(resource);
                        if (p[0] == 0 && p[1] == 0 && p[2] == 0) {
                            return std::nullopt;
                        }
                        return audio::Vec{p[0], p[1], p[2]};
                    };
                    gameAudio->gameTick(ts);
                    if (!audioLog.empty()) {
                        playCounts.push_back(gameAudio->sfx().plays().size());
                        listenerLog.push_back(heardFrom ? audio::Json::array({(*heardFrom)[0], (*heardFrom)[1], (*heardFrom)[2]}) : audio::Json());
                    }
                }
                ++tick;
                if (!v.running) {
                    running = false;
                }
            }
            if (!presenter.cameraValid()) {
                continue;
            }
            presenter.loadRiders(gpu);
            CameraView camera;
            WorldFrameState state = presenter.frame(alpha, camera);
            renderer.render(camera, state, frameView, fw, fh, 4.0 / 3.0);
            // main.js frame audio: GO, the finish, then the listener (the frame's camera)
            {
                const float phase = v.raceInfo[5];
                if (audioPhase == 4 && phase != 4) {
                    gameAudio->go();
                }
                audioPhase = phase;
                if (v.finished && !audioFinished) {
                    audioFinished = true;
                    gameAudio->finish(false, v.aiActive ? int(v.worldState[360]) : 0, race_timed_out() != 0);
                }
                // main.js frame: the listener reads the three.js camera before this frame's render moves it (the previous frame's)
                gameAudio->listener(listenerKnown ? &listenerInverse : nullptr, listenerEye, renderer.origin);
                if (listenerKnown) {
                    heardFrom = listenerEye;
                }
                renderer.cameraOf(camera, listenerInverse, listenerEye);
                listenerKnown = true;
            }
            hud.keyboard = lastDevice.empty() ? gamepad == nullptr : lastDevice == "keyboard";
            hud.draw(v, double(SDL_GetTicksNS()) / 1e9);
            {
                WGPUCommandEncoderDescriptor hed = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
                WGPUCommandEncoder henc = wgpuDeviceCreateCommandEncoder(gpu.device, &hed);
                hud.canvas.encode(henc, frameView, fw, fh);
                WGPUCommandBufferDescriptor hcd = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
                WGPUCommandBuffer hcmd = wgpuCommandEncoderFinish(henc, &hcd);
                wgpuQueueSubmit(gpu.queue, 1, &hcmd);
                wgpuCommandBufferRelease(hcmd);
                wgpuCommandEncoderRelease(henc);
            }
            if (!shotPath.empty() && tick > shotTick) {
                const auto pixels = gpu.readback(frameTex, fw, fh);
                writePng(shotPath, int(fw), int(fh), pixels.data());
                std::printf("{\"tick\":%ld,\"out\":%s}\n", tick - 1, json(shotPath).dump().c_str());
                if (!audioLog.empty()) {
                    audio::Json log;
                    log["timeline"] = gameAudio->timeline();
                    log["plays"] = gameAudio->sfx().plays();
                    audio::Json playTicks = audio::Json::array();
                    for (size_t k = 0, i = 0; k < playCounts.size(); ++k) {
                        for (; i < playCounts[k]; ++i) {
                            playTicks.push_back(k);
                        }
                    }
                    log["playTicks"] = playTicks;
                    log["posts"] = gameAudio->speech().posts();
                    log["listeners"] = listenerLog;
                    log["time"] = gameAudio->context().currentTime();
                    std::ofstream(audioLog) << log.dump();
                }
                running = false;
            }
            // the 4:3 frame letterboxed into the window (a hidden window presents nothing)
            if (hidden) {
                gpu.wait();
                continue;
            }
            WGPUSurfaceTexture st = WGPU_SURFACE_TEXTURE_INIT;
            wgpuSurfaceGetCurrentTexture(surface, &st);
            if (st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal && st.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
                configure();
                continue;
            }
            WGPUTextureView sv0 = wgpuTextureCreateView(st.texture, nullptr);
            WGPUCommandEncoderDescriptor ed = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
            WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu.device, &ed);
            WGPURenderPassColorAttachment ca = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
            ca.view = sv0;
            ca.loadOp = WGPULoadOp_Clear;
            ca.storeOp = WGPUStoreOp_Store;
            ca.clearValue = {0, 0, 0, 1};
            WGPURenderPassDescriptor rd = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
            rd.colorAttachmentCount = 1;
            rd.colorAttachments = &ca;
            WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(enc, &rd);
            const double scale = std::min(double(pw) / fw, double(ph) / fh);
            const float vw = float(fw * scale);
            const float vh = float(fh * scale);
            wgpuRenderPassEncoderSetViewport(pass, (float(pw) - vw) / 2, (float(ph) - vh) / 2, vw, vh, 0, 1);
            wgpuRenderPassEncoderSetPipeline(pass, blitPipeline);
            wgpuRenderPassEncoderSetBindGroup(pass, 0, blitGroup, 0, nullptr);
            wgpuRenderPassEncoderDraw(pass, 3, 1, 0, 0);
            wgpuRenderPassEncoderEnd(pass);
            WGPUCommandBufferDescriptor cd = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
            WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, &cd);
            wgpuQueueSubmit(gpu.queue, 1, &cmd);
            wgpuSurfacePresent(surface);
            wgpuCommandBufferRelease(cmd);
            wgpuRenderPassEncoderRelease(pass);
            wgpuCommandEncoderRelease(enc);
            wgpuTextureViewRelease(sv0);
            wgpuTextureRelease(st.texture);
        }
        std::fflush(stdout);
        std::_Exit(0);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "ssx3: %s\n", e.what());
        return 1;
    }
}
