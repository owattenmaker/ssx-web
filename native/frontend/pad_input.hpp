#pragma once
// Host devices -> the 24 decoded PS2 pad channels (web/pad-input.js, natively): the same channels, the same arithmetic (JS
// numbers are doubles; MUL.S toward zero), the same keyboard layouts (Classic, Simple) and the same W3C standard gamepad layout.
// Keys are named by their KeyboardEvent.code ("KeyA", "ArrowLeft", "Space"); the platform layer maps its key codes to them.
//
// Channel order: 0 Select, 1 Start, 2 L3, 3 R3, 4 D-right, 5 D-left, 6 D-up, 7 D-down, 8 Triangle, 9 Circle, 10 Cross,
// 11 Square, 12 L1, 13 R1, 14 L2, 15 R2, 16/17 right stick left/right, 18/19 right stick up/down, 20/21 left stick
// left/right, 22/23 left stick up/down.
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace ssx_native {

using PadChannels = std::array<float, 24>;

// A pad in the W3C "standard" layout (17 buttons, 4 axes; +x right, +y down)
struct StandardPad {
    struct Button {
        bool pressed = false;
        double value = 0;
    };
    std::array<Button, 17> buttons{};
    std::array<double, 4> axes{};
    bool connected = false;
};

// W3C standard gamepad layout index for each PS2 button (web/pad-input.js GAMEPAD_BUTTONS)
inline constexpr std::array<int, 16> kGamepadButtons{8, 9, 10, 11, 15, 14, 12, 13, 3, 1, 0, 2, 4, 5, 6, 7};

// web/pad-input.js KEYBOARD_BUTTONS, in PAD_BUTTONS order
inline const std::array<std::vector<std::string>, 16>& keyboardButtons() {
    static const std::array<std::vector<std::string>, 16> keys{{
        {"Backspace"},
        {},
        {},
        {"KeyV"},
        {"KeyL"},
        {"KeyJ"},
        {"KeyI"},
        {"KeyK"},
        {"KeyY"},
        {"KeyC"},
        {"Space"},
        {"ShiftLeft", "ShiftRight"},
        {"KeyQ"},
        {"KeyE"},
        {"KeyZ"},
        {"KeyX"},
    }};
    return keys;
}

inline float reciprocal255() {
    const uint32_t bits = 0x3b808081u;
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}
// MUL.S runs with round-toward-zero on the EE (web/pad-input.js towardZero on a double)
inline float towardZero(double x) {
    float f = float(x);
    if (std::abs(double(f)) > std::abs(x)) {
        uint32_t bits;
        std::memcpy(&bits, &f, 4);
        bits -= 1;
        std::memcpy(&f, &bits, 4);
    }
    return f;
}
// Nearest byte on the symmetric 0..255 range; 0 is left / up
inline int axisByte(double value) {
    if (!std::isfinite(value)) {
        throw std::runtime_error("Nonfinite stick value");
    }
    return int(std::floor((std::max(-1.0, std::min(1.0, value)) + 1) * 127.5 + 0.5));
}
// 0x327210..0x3276BC axial response: 79..176 is dead; the rest ramps to full scale
inline std::array<float, 2> stickChannels(int byte) {
    const double negative = std::max(std::trunc((79.0 - byte) * 255 / 79), 0.0);
    const double positive = std::max(std::trunc((double(byte) - 176) * 255 / 79), 0.0);
    const double r = reciprocal255();
    return {towardZero(negative * r), towardZero(positive * r)};
}
inline float pressureChannel(double amount) {
    const double byte = std::floor(std::max(0.0, std::min(1.0, amount)) * 255 + 0.5);
    return towardZero(byte * double(reciprocal255()));
}

// The keyboard modes (web/pad-input.js KEYBOARD_MODES): Classic (the fixed layout, the default) and Simple
struct KeyboardContext {
    enum class Mode { Classic, Simple };
    Mode mode = Mode::Classic;
    // the running controller is airborne (command word 14 is 4 or 5)
    bool air = false;
    struct Steering {
        bool dpad = false;
        int order = 0;
    };
    std::map<std::string, Steering> steering;
    int serial = 0;
};

using KeyDown = std::function<bool(const std::string&)>;

inline bool anyDown(const KeyDown& down, std::initializer_list<const char*> codes) {
    for (const char* c : codes) {
        if (down(c)) {
            return true;
        }
    }
    return false;
}

// buildClassicPad
inline PadChannels buildClassicPad(const KeyDown& down, const StandardPad* pad) {
    PadChannels values{};
    const auto& keys = keyboardButtons();
    for (int i = 0; i < 16; ++i) {
        double amount = 0;
        for (const auto& code : keys[size_t(i)]) {
            if (down(code)) {
                amount = 1;
            }
        }
        if (pad && pad->connected) {
            const auto& b = pad->buttons[size_t(kGamepadButtons[size_t(i)])];
            amount = std::max(amount, b.pressed ? std::max(b.value, 1.0) : b.value);
        }
        // Select / Start / L3 / R3 are digital; D-pad, face and shoulder buttons are pressure bytes
        values[size_t(i)] = i < 4 ? (amount >= 0.5 ? 1.f : 0.f) : pressureChannel(amount);
    }
    auto axis = [&](std::initializer_list<const char*> negative, std::initializer_list<const char*> positive, int index) {
        if (anyDown(down, positive)) {
            return 1.0;
        }
        if (anyDown(down, negative)) {
            return -1.0;
        }
        const double v = pad && pad->connected ? pad->axes[size_t(index)] : NAN;
        return std::isfinite(v) ? v : 0.0;
    };
    const double sticks[4] = {axis({"KeyF"}, {"KeyH"}, 2), axis({"KeyT"}, {"KeyG"}, 3), axis({"KeyA", "ArrowLeft"}, {"KeyD", "ArrowRight"}, 0),
                              axis({"KeyW", "ArrowUp"}, {"KeyS", "ArrowDown"}, 1)};
    for (int k = 0; k < 4; ++k) {
        const auto c = stickChannels(axisByte(sticks[k]));
        values[size_t(16 + 2 * k)] = c[0];
        values[size_t(17 + 2 * k)] = c[1];
    }
    return values;
}

// buildSimplePad: each direction keeps the role (stick / D-pad) it got when it was pressed
inline PadChannels buildSimplePad(const KeyDown& down, const StandardPad* pad, KeyboardContext& keyboard) {
    static const std::vector<std::pair<std::string, std::vector<std::string>>> directions{
        {"left", {"KeyA", "ArrowLeft"}}, {"right", {"KeyD", "ArrowRight"}}, {"up", {"KeyW", "ArrowUp"}}, {"down", {"KeyS", "ArrowDown"}}};
    static const std::map<std::string, std::string> opposite{{"left", "right"}, {"right", "left"}, {"up", "down"}, {"down", "up"}};
    static const std::map<std::string, int> dpadIndex{{"right", 4}, {"left", 5}, {"up", 6}, {"down", 7}};
    std::map<std::string, bool> held;
    for (const auto& [d, codes] : directions) {
        bool h = false;
        for (const auto& c : codes) {
            h = h || down(c);
        }
        held[d] = h;
    }
    auto& roles = keyboard.steering;
    for (auto it = roles.begin(); it != roles.end();) {
        it = held[it->first] ? std::next(it) : roles.erase(it);
    }
    // in DIRECTIONS order (left, right, up, down), as Object.keys gives them
    for (const auto& [d, codes] : directions) {
        if (held[d] && !roles.count(d)) {
            bool dpad;
            if (!keyboard.air) {
                dpad = false;
            } else {
                auto o = roles.find(opposite.at(d));
                dpad = !(o != roles.end() && o->second.dpad);
            }
            roles[d] = {dpad, ++keyboard.serial};
        }
    }
    // movement keys are routed below; everything else keeps the classic layout
    auto moveCode = [&](const std::string& code) {
        for (const auto& [d, codes] : directions) {
            for (const auto& c : codes) {
                if (c == code) {
                    return true;
                }
            }
        }
        return false;
    };
    PadChannels values = buildClassicPad([&](const std::string& code) { return !moveCode(code) && down(code); }, pad);
    for (const auto& [d, index] : dpadIndex) {
        auto r = roles.find(d);
        if (r != roles.end() && r->second.dpad) {
            values[size_t(index)] = pressureChannel(1);
        }
    }
    // a stick axis: the most recently pressed stick-role direction of the pair wins (else the gamepad stick)
    auto axis = [&](const std::string& negative, const std::string& positive, int index) {
        auto n = roles.find(negative);
        auto p = roles.find(positive);
        const bool nOn = n != roles.end() && !n->second.dpad;
        const bool pOn = p != roles.end() && !p->second.dpad;
        if (pOn && (!nOn || p->second.order > n->second.order)) {
            return 1.0;
        }
        if (nOn) {
            return -1.0;
        }
        const double v = pad && pad->connected ? pad->axes[size_t(index)] : NAN;
        return std::isfinite(v) ? v : 0.0;
    };
    const double lx = axis("left", "right", 0);
    const double ly = axis("up", "down", 1);
    const double sticks[2] = {lx, ly};
    for (int k = 0; k < 2; ++k) {
        const auto c = stickChannels(axisByte(sticks[k]));
        values[size_t(20 + 2 * k)] = c[0];
        values[size_t(21 + 2 * k)] = c[1];
    }
    return values;
}

// buildPad(down, pad, keyboard)
inline PadChannels buildPad(const KeyDown& down, const StandardPad* pad, KeyboardContext* keyboard = nullptr) {
    if (keyboard && keyboard->mode == KeyboardContext::Mode::Simple) {
        return buildSimplePad(down, pad, *keyboard);
    }
    return buildClassicPad(down, pad);
}

}  // namespace ssx_native
