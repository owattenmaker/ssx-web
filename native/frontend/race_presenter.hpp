#pragma once
// What the page does with a tick and a frame besides the simulation (web/game-tick.js present, main.js frame), shared by the window
// (app.cpp) and the headless shots (render_shot.cpp): ScreenTint's step, the human's and each computer rider's palette and
// lighting capture (web/rider-skinning.js, web/opponent-riders.js), the camera pair, and the frame's core state for the renderer.
#include "replay_session.hpp"
#include "rider_capture.hpp"
#include "screen_tint.hpp"
#include "world_renderer.hpp"
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
        for (int k = 0; k < 9; ++k) {
            camera.v[size_t(k)] = float(cameraPrev_[k] + (cameraCur_[k] - cameraPrev_[k]) * alpha);
        }
        return state;
    }

private:
    session::Verifier& v_;
    std::unique_ptr<ScreenTint> tint_;
    std::unique_ptr<RiderCapture> human_;
    std::vector<std::unique_ptr<RiderCapture>> npcs_;
    std::vector<float> placements_;
    std::unique_ptr<RiderRenderer> humanRenderer_;
    std::vector<std::unique_ptr<RiderRenderer>> npcRenderers_;
    std::vector<RiderFrame> frames_;
    float cameraPrev_[9] = {};
    float cameraCur_[9] = {};
    bool cameraValid_ = false;
};

}  // namespace ssx_native
