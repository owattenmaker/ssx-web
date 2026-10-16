#pragma once
// The PDA overlays from the original OV.LUI records (web/ctm-pda.js CtmPda, 1:1 so far for the frame and the Yes / No popup;
// UI/ctm-screens.json): PDATemplate (the MCOMM frame every PDA overlay sits in, 0x20A778) with bganim1's snowflakes behind it, and
// 87yndialog. The template's layers under 11 and the snowflakes draw on the page's #ui-bg canvas, which lies under the world's
// canvas: under a PDA screen over the race (the pause, its prompts, the results' Restart) the world covers them, so native draws
// only the layers from 11 on, over the world, as the page's #ui does.
#include "lui_screen.hpp"
#include <cmath>
#include <memory>

namespace ssx_native {

namespace fe {

// web/ctm-pda.js pdaTemperature: the badge temperature 0x20A854, -10 - (G+0x1C / 3600) % 15 (G+0x1C in 60 Hz frames)
inline int pdaTemperature(double frames) {
    return -10 - int(std::fmod(std::floor(frames / 3600), 15));
}

class CtmPda {
public:
    static constexpr double kSy = 448.0 / 480;

    bool load(const nlohmann::json& data, hud::HudImages& images, hud::UiText& text) {
        data_ = data;
        if (!data_.is_object() || !data_.contains("screens")) {
            return false;
        }
        for (auto it = data_["screens"].begin(); it != data_["screens"].end(); ++it) {
            auto lui = std::make_unique<LuiScreen>(it.value(), images, text);
            lui->shapeScale = true;
            lui_[it.key()] = std::move(lui);
        }
        return ready();
    }
    bool ready() const {
        return lui_.count("PDATemplate") != 0;
    }
    // opening(): the PDA frame's intro starts (0x20A778), the temperature written once
    void opening(double nowFrames) {
        opened_ = nowFrames;
        temperature_ = pdaTemperature(opened_);
        hasTemperature_ = true;
    }
    // frame(): PDATemplate at the time the PDA opened (its layers from 11 on: see above)
    bool frame(HudCanvas& c, double nowFrames) {
        auto it = lui_.find("PDATemplate");
        if (it == lui_.end()) {
            return false;
        }
        LuiScreen& tmpl = *it->second;
        const double t = nowFrames - opened_;
        const int temperature = hasTemperature_ ? temperature_ : pdaTemperature(nowFrames);
        std::vector<LuiScreen::Event> events;
        for (const auto& ev : tmpl.screen()["events"]) {
            if (ev.value("frame", 0.0) <= t) {
                events.push_back({&ev, ev.value("frame", 0.0)});
            }
        }
        // '%d°%C' (the badge text): the temperature in degrees
        auto text = [&](const nlohmann::json& e) -> std::optional<LuiScreen::Override> {
            if (e.value("kind", "") == "text" && e.value("text", "").find("%d") != std::string::npos) {
                LuiScreen::Override o;
                o.text = std::to_string(temperature) + "\xC2\xB0" + "C";
                return o;
            }
            return std::nullopt;
        };
        c.save();
        c.scale(1, kSy);
        tmpl.draw(c, events, t, text, [](int layer) { return layer >= 11; });
        c.restore();
        return true;
    }
    // dialog(): the Yes / No popup (87yndialog): up to two title lines, Yes / No with the Cross on the focused one
    bool dialog(HudCanvas& c, const std::vector<std::string>& lines, int index, double nowFrames, const std::string& yes, const std::string& no) {
        auto it = lui_.find("87yndialog");
        if (it == lui_.end()) {
            return false;
        }
        LuiScreen& lui = *it->second;
        std::string key;
        for (size_t k = 0; k < lines.size(); ++k) {
            key += (k ? "|" : "") + lines[k];
        }
        if (key != popupKey_) {
            popupKey_ = key;
            popupAt_ = nowFrames;
        }
        const double t = nowFrames - popupAt_;
        const double focusFrame = index == 0 ? 35 : 41;
        std::vector<LuiScreen::Event> events;
        for (const auto& ev : lui.screen()["events"]) {
            const double f = ev.value("frame", 0.0);
            if (f <= 30 ? f <= t : f == focusFrame) {
                events.push_back({&ev, f});
            }
        }
        const std::string a = lines.empty() ? "" : lines[0];
        const std::string b = lines.size() > 1 ? lines[1] : "";
        auto override = [&](const nlohmann::json& e) -> std::optional<LuiScreen::Override> {
            const std::string label = e.value("label", "");
            LuiScreen::Override o;
            // one line each, as on the PS2
            if (label == "title_quit") {
                o.text = a;
                o.props[6] = 440;
                return o;
            }
            if (label == "title_quitchallenge") {
                if (b.empty()) {
                    o.hidden = true;
                } else {
                    o.text = b;
                    o.props[6] = 440;
                }
                return o;
            }
            if (label == "yes") {
                o.text = yes;
                return o;
            }
            if (label == "no") {
                o.text = no;
                return o;
            }
            return std::nullopt;
        };
        c.save();
        c.scale(1, kSy);
        lui.draw(c, events, t, override);
        c.restore();
        return true;
    }
    // QA: the popup's clock as the page's when its key is already set
    void fixPopup(const std::string& key, double at) {
        popupKey_ = key;
        popupAt_ = at;
    }

private:
    nlohmann::json data_;
    std::map<std::string, std::unique_ptr<LuiScreen>> lui_;
    double opened_ = 0;
    int temperature_ = 0;
    bool hasTemperature_ = false;
    std::string popupKey_;
    double popupAt_ = 0;
};

}  // namespace fe

}  // namespace ssx_native
