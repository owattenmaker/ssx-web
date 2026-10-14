#pragma once
// The human rider's per-tick capture after the core's tick (web/game-tick.js present): the skin palette pair
// (web/rider-skinning.js capture: previous <- current, current <- the core's; a reset or the first capture fills both) and the
// rider lighting (web/rider-material.js createRiderLightingUpdate.update, then capture: the ten GPU rows).
#include "core_api.hpp"
#include "json.hpp"
#include "rider_renderer.hpp"
#include <cstring>
#include <stdexcept>

namespace ssx_native {

class RiderCapture {
public:
    // irradiance: environment.json's "irradiance" (rim_scale, rim_constants)
    explicit RiderCapture(const nlohmann::json& irradiance) {
        rimScale_ = irradiance["rim_scale"].get<float>();
        for (int k = 0; k < 5; ++k) {
            constants_[k] = irradiance["rim_constants"][size_t(k)].get<float>();
        }
    }
    // after each tick, in the rider's context; reset: the tick was a rescue / placement (no blend across it). A computer rider
    // (web/opponent-riders.js) passes the human core's view (it steps no camera) and keeps the packaged rim scale.
    void capture(bool reset, const float* view = nullptr, bool human = true) {
        reset = reset || resetPending_;
        resetPending_ = false;
        const int size = rider_skin_palette_count();
        const float* pointer = size ? rider_skin_palette() : nullptr;
        const size_t stride = size_t(size) * 16;
        if (!size || !pointer) {
            // a tick with no pose draws the last palette again; the reset waits for the next capture
            resetPending_ = reset;
            if (available_) {
                frame_.previous = frame_.current;
            }
        } else {
            const std::vector<float> now(pointer, pointer + stride);
            if (available_ && !reset) {
                frame_.previous = frame_.current;
            } else {
                frame_.previous = now;
            }
            frame_.current = now;
            available_ = true;
        }
        // the lighting rows, every pose tick
        const float* physical = pose_physical();
        const float point[4] = {physical[9], physical[10], physical[11], 1};
        refresh_rider_lighting(rider_query_bounds(), point);
        const float* lighting = human ? environment_lighting_info() : nullptr;
        const float rim = lighting && lighting[0] ? lighting[3] : rimScale_;
        shade_rider_lighting(environment_irradiance(), view ? view : camera_render_view(), point, rim, constants_, nullptr, 0);
        std::memcpy(frame_.lighting.data(), rider_lighting_gpu_coefficients(), 160);
        frame_.lit = true;
    }
    RiderFrame& frame() {
        return frame_;
    }
    bool available() const {
        return available_;
    }

private:
    RiderFrame frame_;
    bool available_ = false;
    bool resetPending_ = false;
    float rimScale_ = 0;
    float constants_[5] = {};
};

}  // namespace ssx_native
