#pragma once
// What the page does with a tick and a frame besides the simulation (web/game-tick.js present, main.js frame), shared by the window
// (app.cpp) and the headless shots (render_shot.cpp): ScreenTint's step, the human's and each computer rider's palette and
// lighting capture (web/rider-skinning.js, web/opponent-riders.js), the camera pair, and the frame's core state for the renderer.
#include "replay_session.hpp"
#include "rider_capture.hpp"
#include "light_glow.hpp"
#include "screen_tint.hpp"
#include "set_pieces.hpp"
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
        const auto& irradiance = v.environmentMeta["irradiance"];
        human_ = std::make_unique<RiderCapture>(irradiance);
        for (size_t k = 0; k < v.npcs.size(); ++k) {
            npcs_.push_back(std::make_unique<RiderCapture>(irradiance));
            placements_.push_back(-1);
        }
    }
    // after each core tick
    void afterTick() {
        if (tint_) {
            tint_->step(fog_info(), weather_info(), weather_lightning(), environment_world_loads());
        }
        // web/game-tick.js present: the sun painter once per camera tick, after ScreenTint
        if (sun_ && v_.hasCamera) {
            sun_->tick(fog_info(), environment_world_loads());
        }
        const bool reset = (v_.tickFlags & 3) != 0;
        human_->capture(reset);
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
                    npcs_[k]->capture(npcReset, view, false);
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
    // the riders' renderers, once their palettes exist (the palette groups come from each context)
    void loadRiders(Gpu& gpu) {
        if (!humanRenderer_ && human_->available()) {
            humanRenderer_ = std::make_unique<RiderRenderer>();
            humanRenderer_->load(gpu, v_.assets.root, session::str(session::at(v_.entry, "package")), rider_skin_palette_indices(), rider_skin_palette_count());
        }
        if (v_.aiActive && npcRenderers_.empty()) {
            for (size_t k = 0; k < v_.npcs.size(); ++k) {
                auto r = std::make_unique<RiderRenderer>();
                const std::string package = session::str(session::at(v_.npcs[k].record, "package"));
                v_.inNpc(v_.npcs[k], [&] { r->load(gpu, v_.assets.root, package, rider_skin_palette_indices(), rider_skin_palette_count()); });
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
        const auto& eventStart = session::at(v_.course, "eventStart");
        const bool started = !(eventStart.is_boolean() && !eventStart.get<bool>());
        const bool stageWorld = v_.assets.exists(v_.courseRoot() + "PARTICLES/particles.json");
        setPieces_ = std::make_unique<SetPieces>(nlohmann::json::parse(v_.assets.text(v_.code + "/world.json")), renderer.origin, started, stageWorld);
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
        if (setPieces_) {
            state.batches = &setPieces_->update();
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
    std::unique_ptr<SetPieces> setPieces_;
    std::unique_ptr<LightGlow> glow_;
    std::unique_ptr<RiderCapture> human_;
    std::vector<std::unique_ptr<RiderCapture>> npcs_;
    std::vector<float> placements_;
    std::unique_ptr<RiderRenderer> humanRenderer_;
    std::vector<std::unique_ptr<RiderRenderer>> npcRenderers_;
    std::vector<RiderFrame> frames_;
    std::vector<RiderFxFrame> fx_;
    float cameraPrev_[9] = {};
    float cameraCur_[9] = {};
    bool cameraValid_ = false;
};

}  // namespace ssx_native
