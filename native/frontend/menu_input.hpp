#pragma once
// The menus' input off the race (web/gamepad-menus.js, 1:1): the pad through the PS2 pad history 0x321298 (one update per 60 Hz frame
// on 24 channels: after an edge the next three updates ignore the input; a held channel repeats on the press, 24 updates later, then
// every 12), sent as the keys the screens read (web/touch-controls.js MENU_KEYS: D-pad / left stick = arrows, Cross = Space, Triangle =
// Escape, Square = Shift, Circle = Backspace, L1 / R1 / L2 / R2 = Q / E / Z / X, Start = Enter); a held arrow key repeats the same way
// in time (400 ms, then every 200 ms) as fresh presses. onFrame: the UI frame counter's pass (web/screen-phases.js step), run before
// each update so a key meets its own frame's phase.
#include "pad_input.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ssx_native {

namespace fe {

class MenuInput {
public:
    using Send = std::function<void(const std::string& code)>;

    // one call a display frame: the 60 Hz updates due since the last (at most 4; a long gap counts one)
    void step(const StandardPad& pad, double nowMs, bool inMenu, const std::function<void()>& onFrame, const Send& keydown) {
        constexpr double kFrameMs = 1000.0 / 60;
        if (!started_ || nowMs - last_ > 250) {
            last_ = nowMs - kFrameMs;
            started_ = true;
        }
        const auto values = held(pad);
        const int n = std::min(4, int(std::lround((nowMs - last_) / kFrameMs)));
        for (int k = 0; k < n; ++k) {
            last_ += kFrameMs;
            if (onFrame) {
                onFrame();
            }
            update(values);
            emit(inMenu, keydown);
        }
    }
    // the keyboard's arrows: a fresh press (not the browser's auto-repeat) arms the repeat; keyUp ends it
    void keyDown(const std::string& code, bool repeat, double nowMs, bool inMenu) {
        if (repeat || !inMenu || !isArrow(code)) {
            return;
        }
        keys_[code] = nowMs + 400;
    }
    void keyUp(const std::string& code) {
        keys_.erase(code);
    }
    // the held arrows' repeats due now (each a fresh press)
    void keyRepeats(double nowMs, bool inMenu, const Send& keydown) {
        if (keys_.empty()) {
            return;
        }
        if (!inMenu) {
            keys_.clear();
            return;
        }
        for (auto& [code, next] : keys_) {
            if (nowMs >= next) {
                next = nowMs + 200;
                keydown(code);
            }
        }
    }
    void releaseAll() {
        keys_.clear();
    }

private:
    struct Channel {
        int held = 0;
        int pressed = 0;
        int released = 0;
        int repeat = 0;
        int timer = 0;
        int age = 0;
    };
    // web/gamepad-menus.js BUTTON_CONTROLS (standard index -> control) and MENU_KEYS
    struct Control {
        int button;
        const char* name;
        const char* code;
        bool direction;
    };
    static const std::vector<Control>& controls() {
        static const std::vector<Control> list = {
            {0, "cross", "Space", false},       {1, "circle", "Backspace", false}, {2, "square", "ShiftLeft", false}, {3, "triangle", "Escape", false},
            {4, "l1", "KeyQ", false},           {5, "r1", "KeyE", false},          {6, "l2", "KeyZ", false},          {7, "r2", "KeyX", false},
            {9, "start", "Enter", false},       {12, "up", "ArrowUp", true},       {13, "down", "ArrowDown", true},   {14, "left", "ArrowLeft", true},
            {15, "right", "ArrowRight", true},
        };
        return list;
    }
    static bool isArrow(const std::string& code) {
        return code == "ArrowUp" || code == "ArrowDown" || code == "ArrowLeft" || code == "ArrowRight";
    }
    // ps2Held: each control held, and the left stick's directions past the pad's raw thresholds (raw < 79 / > 176 of 0..255)
    static std::vector<std::pair<std::string, bool>> held(const StandardPad& pad) {
        std::vector<std::pair<std::string, bool>> out;
        if (!pad.connected) {
            return out;
        }
        for (const auto& c : controls()) {
            const auto& b = pad.buttons[size_t(c.button)];
            out.push_back({c.name, b.pressed || b.value > 0});
        }
        auto raw = [](double a) { return std::max(0.0, std::min(255.0, double(std::lround(128 + a * 127.5)))); };
        const double x = raw(pad.axes[0]);
        const double y = raw(pad.axes[1]);
        out.push_back({"s-left", x < 79});
        out.push_back({"s-right", x > 176});
        out.push_back({"s-up", y < 79});
        out.push_back({"s-down", y > 176});
        return out;
    }
    Channel& channel(const std::string& c) {
        return channels_[c];
    }
    void update(const std::vector<std::pair<std::string, bool>>& values) {
        for (const auto& [c, v] : values) {
            Channel& ch = channel(c);
            if (ch.age < 3) {
                ch.age++;
                ch.pressed = 0;
                ch.released = 0;
            } else {
                const int now = v ? 1 : 0;
                if (now == ch.held) {
                    ch.pressed = 0;
                    ch.released = 0;
                } else {
                    ch.held = now;
                    ch.pressed = now;
                    ch.released = 1 - now;
                    ch.age = 0;
                }
            }
            if (ch.held) {
                if (ch.timer == 0) {
                    ch.repeat = 1;
                    ch.timer = 24;
                } else {
                    ch.timer--;
                    if (ch.timer > 0) {
                        ch.repeat = 0;
                    } else {
                        ch.repeat = 1;
                        ch.timer = 12;
                    }
                }
            } else {
                ch.repeat = 0;
                ch.timer = 0;
            }
        }
    }
    // emit: in a menu, the directions' repeats and the buttons' presses as keys (Start is the menu's accept on every menu screen)
    void emit(bool inMenu, const Send& keydown) {
        if (!inMenu) {
            return;
        }
        for (const auto& c : controls()) {
            if (c.direction) {
                if (channel(c.name).repeat || channel(std::string("s-") + c.name).repeat) {
                    keydown(c.code);
                }
                continue;
            }
            if (channel(c.name).pressed) {
                keydown(c.code);
            }
        }
    }
    std::map<std::string, Channel> channels_;
    std::map<std::string, double> keys_;
    double last_ = 0;
    bool started_ = false;
};

}  // namespace fe

}  // namespace ssx_native
