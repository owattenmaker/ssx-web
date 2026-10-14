#pragma once
// What the page does with a tick and a frame besides the simulation (web/game-tick.js present, main.js frame), shared by the window
// (app.cpp) and the headless shots (render_shot.cpp): ScreenTint's step, the human's and each computer rider's palette and
// lighting capture (web/rider-skinning.js, web/opponent-riders.js), the camera pair, and the frame's core state for the renderer.
#include "replay_session.hpp"
#include "rider_capture.hpp"
#include "light_glow.hpp"
#include "screen_tint.hpp"
#include "set_piece_fx.hpp"
#include "set_pieces.hpp"
#include "startfire.hpp"
#include "terrain_sparkle.hpp"
#include "sun_flare.hpp"
#include "uv_scroll.hpp"
#include "world_renderer.hpp"
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

namespace ssx_native {

class RacePresenter {
public:
    explicit RacePresenter(session::Verifier& v) : v_(v) {
        if (v.assets.exists(v.courseRoot() + "screen-tint.json")) {
            tint_ = std::make_unique<ScreenTint>(nlohmann::json::parse(v.assets.text(v.courseRoot() + "screen-tint.json")));
        }
        // UVScroll (Snow Jam's exports live in /assets/UVSCROLL, other locations under their root; web/set-pieces-renderer.js)
        const std::string scrollBase = v.code == "ARA1" ? std::string() : v.courseRoot();
        if (v.assets.exists(scrollBase + "UVSCROLL/uv-scroll.json")) {
            const bool sections = v.assets.exists(v.courseRoot() + "SECTIONS/sections.json") && v.assets.exists(v.courseRoot() + "SECTIONS/ready-state.json");
            const nlohmann::json ready = sections ? nlohmann::json::parse(v.assets.text(v.courseRoot() + "SECTIONS/ready-state.json")) : nlohmann::json();
            scroll_ = std::make_unique<UvScrollSet>(nlohmann::json::parse(v.assets.text(scrollBase + "UVSCROLL/uv-scroll.json")), ready);
        }
        // main.js loadCourse: the sun flare (course.sunFlare, default /assets/SUN_FLARE/); its painter steps from the first tick
        const std::string sunDir = v.assetPath(session::str(session::at(v.course, "sunFlare"), "/assets/SUN_FLARE/"));
        if (v.assets.exists(sunDir + "sun-flare.json")) {
            sun_ = std::make_unique<SunFlare>(v.assets.path(sunDir));
        }
        // web/startfire-renderer.js: a course whose courses.json entry has startfire (main.js loads its courses from courses.json, whose
        // entries carry none at present: only main.js's built-in Snow Jam entry, replaced at boot, had it)
        if (session::truthy(session::at(v.course, "startfire")) && v.assets.exists("STARTFIRE/startfire.json")) {
            startfire_ = std::make_unique<Startfire>(nlohmann::json::parse(v.assets.text("STARTFIRE/startfire.json")));
        }
        const auto& irradiance = v.environmentMeta["irradiance"];
        human_ = std::make_unique<RiderCapture>(irradiance);
        for (size_t k = 0; k < v.npcs.size(); ++k) {
            npcs_.push_back(std::make_unique<RiderCapture>(irradiance));
            placements_.push_back(-1);
            // web/opponent-riders.js: the computer rider's morph parts (pv boardFlex), configured at its first capture
            const std::string package = session::str(session::at(v.npcs[k].record, "package"));
            npcFlex_.push_back(flex::load(v.assets.path(package + "/"), nlohmann::json::parse(v.assets.text(package + "/rider.json"))));
        }
    }
    // after each core tick
    void afterTick() {
        if (tint_) {
            tint_->step(fog_info(), weather_info(), weather_lightning(), environment_world_loads());
        }
        // web/startfire-renderer.js update: one step per snow update serial (Snow Jam's start-gate fountains)
        if (startfire_) {
            startfire_->afterTick(snow_info()[20], rider_state());
        }
        // web/game-tick.js present: the sun painter once per camera tick, after ScreenTint
        if (sun_ && v_.hasCamera) {
            sun_->tick(fog_info(), environment_world_loads());
        }
        const bool reset = (v_.tickFlags & 3) != 0;
        human_->capture(reset, nullptr, true, &v_.humanFlex);
        // the computer riders (ai-race.js endTick / opponent-riders.js capture): a reset placement (reset_info[2] changed) or the
        // race start resets the palette pair; the view is the human core's
        if (v_.aiActive) {
            float view[16];
            std::memcpy(view, camera_render_view(), sizeof view);
            for (size_t k = 0; k < v_.npcs.size(); ++k) {
                v_.inNpc(v_.npcs[k], [&] {
                    const float placements = reset_info()[2];
                    const bool npcReset = placements_[k] != placements;
                    placements_[k] = placements;
                    // init_animation (web/ai-racers.js configureRider) clears the morph parts: set them again
                    if (!npcFlex_[k].parts.empty() && rider_skin_palette_count() && flex::unconfigured()) {
                        flex::configure(npcFlex_[k]);
                    }
                    npcs_[k]->capture(npcReset, view, false, &npcFlex_[k]);
                });
            }
        }
        if (v_.hasCamera) {
            std::memcpy(cameraPrev_, cameraValid_ && !reset ? cameraCur_ : v_.camera, sizeof cameraPrev_);
            std::memcpy(cameraCur_, v_.camera, sizeof cameraCur_);
            cameraValid_ = true;
        }
    }
    bool cameraValid() const {
        return cameraValid_;
    }
    // quality.riderTextures (frontend/quality.hpp), read when the riders' renderers are made
    std::string riderTextures = "xbox";
    // the riders' renderers, once their palettes exist (the palette groups come from each context)
    void loadRiders(Gpu& gpu) {
        if (!humanRenderer_ && human_->available()) {
            humanRenderer_ = std::make_unique<RiderRenderer>();
            humanRenderer_->textureSet = riderTextures;
            humanRenderer_->load(gpu, v_.assets.root, session::str(session::at(v_.entry, "package")), rider_skin_palette_indices(), rider_skin_palette_count(),
                                 &v_.humanFlex);
        }
        if (v_.aiActive && npcRenderers_.empty()) {
            for (size_t k = 0; k < v_.npcs.size(); ++k) {
                auto r = std::make_unique<RiderRenderer>();
                r->textureSet = riderTextures;
                const std::string package = session::str(session::at(v_.npcs[k].record, "package"));
                v_.inNpc(v_.npcs[k], [&] { r->load(gpu, v_.assets.root, package, rider_skin_palette_indices(), rider_skin_palette_count(), &npcFlex_[k]); });
                npcRenderers_.push_back(std::move(r));
            }
        }
    }
    // the light glows (course.lightGlow, default /assets/LIGHT_GLOW/; none when course.lightGlowCount is 0 or the package has no
    // lights) at the world's origin, and the sun, attached to the renderer
    void loadEffects(WorldRenderer& renderer) {
        const auto& count = session::at(v_.course, "lightGlowCount");
        const std::string glowDir = v_.assetPath(session::str(session::at(v_.course, "lightGlow"), "/assets/LIGHT_GLOW/"));
        if (!glow_ && !(count.is_number() && count.get<double>() == 0) && v_.assets.exists(glowDir + "light-glow.json")) {
            try {
                glow_ = std::make_unique<LightGlow>(v_.assets.path(glowDir), renderer.origin);
            } catch (const std::exception& e) {
                std::fprintf(stderr, "No light glows: %s\n", e.what());
            }
        }
        renderer.attachEffects(glow_.get(), sun_.get());
        // the set pieces' batches (main.js: an event course's start hides the event-dead batches; the stage world when PARTICLES exists)
        SetPieceAssets a;
        const auto& eventStart = session::at(v_.course, "eventStart");
        a.eventStart = !(eventStart.is_boolean() && !eventStart.get<bool>());
        const std::string root = v_.courseRoot();
        // Snow Jam's FLAGS / UVSCROLL / LIVECOMP exports live in /assets/, other locations' under their root
        const std::string base = v_.code == "ARA1" ? std::string() : root;
        auto doc = [&](const std::string& path) { return v_.assets.exists(path) ? nlohmann::json::parse(v_.assets.text(path)) : nlohmann::json(); };
        a.code = v_.code;
        a.world = doc(v_.code + "/world.json");
        a.liveComp = doc(base + "LIVECOMP/livecomp.json");
        a.attached = doc(base + "LIVECOMP/attached.json");
        a.flags = doc(base + "FLAGS/flags.json");
        if (v_.assets.exists(root + "SECTIONS/sections.json") && v_.assets.exists(root + "SECTIONS/ready-state.json")) {
            a.sections = doc(root + "SECTIONS/sections.json");
            a.ready = doc(root + "SECTIONS/ready-state.json");
        }
        a.rails = doc(root + "rails.json");
        a.avalanches = doc(root + "avalanches.json");
        a.stageWorld = v_.assets.exists(root + "PARTICLES/particles.json");
        a.coreStarts = a.stageWorld && a.liveComp.is_object();
        setPieces_ = std::make_unique<SetPieces>(a, renderer.origin);
        setPieceFx_ = std::make_unique<SetPieceFx>(doc(root + "PARTICLES/particles.json"), doc(root + "fog-puffs.json"), doc(root + "CROWD/crowd.json"),
                                                   renderer.origin);
        // web/terrain-sparkle.js (a course: its terrain-sparkle.bin)
        sparkle_ = std::make_unique<TerrainSparkle>(v_.assets.path(root + "terrain-sparkle.bin"), renderer.origin);
        setPieceFx_->avalanche = &setPieces_->avalanche();
        setPieceFx_->nodePosition = [this](uint32_t r, uint32_t n, float* out) { return setPieces_->nodePosition(r, n, out); };
        setPieceFx_->resident = [this](int chunk) {
            auto it = chunks_.find(uint32_t(chunk));
            return it == chunks_.end() ? std::optional<bool>() : std::optional<bool>(it->second);
        };
    }
    // the frame at render alpha a (0: the previous tick's camera and palettes, 1: the newest)
    WorldFrameState frame(double alpha, CameraView& camera) {
        WorldFrameState state;
        const float* fog = fog_palette_info();
        state.fogEnabled = fog[0] != 0;
        state.fogRevision = uint32_t(fog[1]);
        state.fogSlope = fog[4];
        state.fogOffset = fog[5];
        state.fogPalette = fog_palette_rgba();
        if (const uint32_t* c = section_chunks()) {
            for (uint32_t k = 0; k < c[0]; ++k) {
                state.chunks[c[1 + 2 * k]] = c[2 + 2 * k] == 3;
            }
        }
        if (tint_) {
            state.tint = tint_->output();
        }
        chunks_ = state.chunks;
        state.setPieceFx = setPieceFx_.get();
        if (startfire_) {
            startfireParticles_ = startfire_->particles();
            state.startfire = &startfireParticles_;
        }
        if (sparkle_ && sparkle_->loaded()) {
            state.sparkle = sparkle_.get();
            std::memcpy(fogInfo_.data(), fog_info(), sizeof(float) * 11);
            state.fogInfo = &fogInfo_;
            state.resets = weather_info()[46];
        }
        if (setPieces_) {
            state.batches = &setPieces_->update();
            state.flags = &setPieces_->flagMeshes();
            state.cables = &setPieces_->cables();
        }
        // the set pieces' clock runs to the core's (main.js frame: setPieceRenderer.update)
        if (scroll_) {
            scroll_->update();
            state.scroll = scroll_->offsets();
        }
        frames_.clear();
        frames_.reserve(1 + npcs_.size());
        if (humanRenderer_) {
            frames_.push_back(human_->frame());
            frames_.back().alpha = float(alpha);
            state.riders.push_back({humanRenderer_.get(), nullptr});
        }
        for (size_t k = 0; k < npcRenderers_.size(); ++k) {
            if (!npcs_[k]->available()) {
                continue;
            }
            frames_.push_back(npcs_[k]->frame());
            frames_.back().alpha = float(alpha);
            state.riders.push_back({npcRenderers_[k].get(), nullptr});
        }
        for (size_t k = 0; k < state.riders.size(); ++k) {
            state.riders[k].second = &frames_[k];
        }
        // the trails, wakes and snow (main.js frame: boardTrail / wakeRenderer / snowRenderer.update, opponentFx.update): the human's
        // while playing, each computer rider's from its own core while the AI race runs
        fx_.resize(1 + v_.npcs.size());
        fx_[0].read(true);
        fx_[0].visible = true;
        for (size_t k = 0; k < v_.npcs.size(); ++k) {
            RiderFxFrame& f = fx_[k + 1];
            f.cullable = true;
            f.visible = v_.aiActive;
            if (f.visible) {
                v_.inNpc(v_.npcs[k], [&] { f.read(false); });
            }
        }
        for (const auto& f : fx_) {
            state.fx.push_back(&f);
        }
        for (int k = 0; k < 9; ++k) {
            camera.v[size_t(k)] = float(cameraPrev_[k] + (cameraCur_[k] - cameraPrev_[k]) * alpha);
        }
        return state;
    }

private:
    session::Verifier& v_;
    std::unique_ptr<ScreenTint> tint_;
    std::unique_ptr<UvScrollSet> scroll_;
    std::unique_ptr<SunFlare> sun_;
    std::unique_ptr<Startfire> startfire_;
    std::vector<double> startfireParticles_;
    std::unique_ptr<SetPieces> setPieces_;
    std::unique_ptr<SetPieceFx> setPieceFx_;
    std::unique_ptr<TerrainSparkle> sparkle_;
    std::array<float, 11> fogInfo_{};
    std::map<uint32_t, bool> chunks_;
    std::unique_ptr<LightGlow> glow_;
    std::unique_ptr<RiderCapture> human_;
    std::vector<std::unique_ptr<RiderCapture>> npcs_;
    std::vector<float> placements_;
    std::vector<flex::Flex> npcFlex_;
    std::unique_ptr<RiderRenderer> humanRenderer_;
    std::vector<std::unique_ptr<RiderRenderer>> npcRenderers_;
    std::vector<RiderFrame> frames_;
    std::vector<RiderFxFrame> fx_;
    float cameraPrev_[9] = {};
    float cameraCur_[9] = {};
    bool cameraValid_ = false;
};

}  // namespace ssx_native
