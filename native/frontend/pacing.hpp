#pragma once
// The page's 60 Hz pacing (docs/native.md "N3"): web/tick-lock.js (pv tickLock: on a ~60 Hz display one tick per drawn frame, the
// newest state drawn, a debt paid with a 0- or 2-tick frame past 4 ticks), else web/fixed-step-clock.js with
// web/ps2-frame-pacing.js ps2FrameTime (a slow frame runs at most 12 ticks, the 30-slot pad ring's lap) and the render alpha
// clamp(leftover x 60, 0, 1).
#include <algorithm>
#include <cmath>
#include <optional>

namespace ssx_native {

class TickLock {
public:
    // the ticks this frame runs (0, 1 or 2), or none for a frame that is not a ~60 Hz frame
    std::optional<int> frame(double dt) {
        constexpr double step = 1.0 / 60;
        if (!(dt >= 0.75 * step && dt <= 1.25 * step)) {
            debt_ = 0;
            return std::nullopt;
        }
        debt_ += dt - step;
        if (debt_ > 4 * step) {
            debt_ -= step;
            return 2;
        }
        if (debt_ < -4 * step) {
            debt_ += step;
            return 0;
        }
        return 1;
    }
    void reset() {
        debt_ = 0;
    }

private:
    double debt_ = 0;
};

// web/ps2-frame-pacing.js ps2FrameTime
inline double ps2FrameTime(double seconds) {
    constexpr double step = 1.0 / 60;
    if (!(seconds > 0)) {
        return 0;
    }
    double ticks = seconds / step;
    if (ticks > 30) {
        ticks = std::fmod(ticks - 1, 30.0) + 1;
    }
    return std::min(ticks, 12.0) * step;
}

class FixedStepClock {
public:
    void reset() {
        pending_ = 0;
    }
    // the ticks to run for a frame of `seconds`; afterwards alpha() is the leftover's render alpha
    int advance(double seconds) {
        constexpr double step = 1.0 / 60;
        pending_ += seconds;
        int ticks = 0;
        while (pending_ + 1e-10 >= step && ticks < 12) {
            pending_ = std::max(0.0, pending_ - step);
            ++ticks;
        }
        return ticks;
    }
    double alpha() const {
        return std::clamp(pending_ * 60, 0.0, 1.0);
    }

private:
    double pending_ = 0;
};

}  // namespace ssx_native
