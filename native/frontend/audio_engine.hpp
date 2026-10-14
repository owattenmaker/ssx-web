#pragma once
// The original 11-channel mixer as one Web Audio graph (web/audio-engine.js, 1:1; docs/audio-logic.md section 2):
//   sources -> bus mix gain (MIX.INF, 2883B0) -> duck gain (287F00) -> user slider gain -> declick fader -> master -> output.
// The native front end has no user-gesture unlock: the context runs from the start (unlock() builds the graph), and a muted
// run (--mute, automated runs) renders without an output device.
#include "webaudio.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ssx_native {

namespace audio {

inline const std::vector<std::string>& audioBuses() {
    static const std::vector<std::string> b = {"MUSIC", "DJ", "PA", "CHARACTER", "UI", "BOARD", "COLLISION", "AMBIENT", "ARCADESFX", "ARCADESPEECH"};
    return b;
}
constexpr int kSliderSteps = 11;
constexpr int kSliderDefault = 10;
// SND master byte 0x50AA5D = trunc(10 / 11 x 127)
inline const double kMasterGain = std::trunc((10.0 / 11) * 127) / 127;

// 288D18 per-channel base scales: 'FE' and 'WORLD'
struct ChannelScale {
    std::map<std::string, double> bus;
    std::array<double, 10> speakers;
};
inline const ChannelScale& channelScale(const std::string& mode) {
    static const ChannelScale fe{{{"MUSIC", 1}, {"DJ", 1}, {"PA", 1}, {"CHARACTER", 0.6}, {"UI", 1}, {"BOARD", 1}, {"COLLISION", 1}, {"AMBIENT", 1},
                                  {"ARCADESFX", 1}, {"ARCADESPEECH", 1}},
                                 {1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};
    static const ChannelScale world{{{"MUSIC", 1}, {"DJ", 0.55}, {"PA", 0.43}, {"CHARACTER", 0.8}, {"UI", 0.8}, {"BOARD", 0.75}, {"COLLISION", 0.8},
                                     {"AMBIENT", 0.5}, {"ARCADESFX", 0.75}, {"ARCADESPEECH", 0.75}},
                                    {0.9, 0.9, 0.9, 0.9, 0.8, 0.85, 0.9, 1, 1, 0.95}};
    return mode == "WORLD" ? world : fe;
}

struct Mix {
    int index = 0;
    double timeMs = 0;
    // a missing bus: -1 in MIX.INF (keep the channel)
    std::map<std::string, double> levels;
};

class AudioEngine {
public:
    struct Options {
        std::vector<Mix> mixes;
        int music = kSliderDefault;
        int effects = kSliderDefault;
        int speech = kSliderDefault;
        bool djEnabled = true;
        int radioMode = 0;
        bool arcadeAudio = true;
        double declickMs = 0;
        bool muted = false;
    };
    AudioEngine(wa::Context& ctx, Options o) : ctx_(ctx), mixes_(std::move(o.mixes)), declick_(std::max(0.0, o.declickMs) / 1000), muted_(o.muted) {
        sliders_["music"] = o.music;
        sliders_["effects"] = o.effects;
        sliders_["speech"] = o.speech;
        djEnabled_ = o.djEnabled;
        radioMode_ = o.radioMode;
        arcadeAudio_ = o.arcadeAudio;
    }

    wa::Context* context() const {
        return built_ ? &ctx_ : nullptr;
    }
    bool unlocked() const {
        return unlocked_;
    }
    // voices may start
    bool live() const {
        return true;
    }
    int mixIndex() const {
        return mixIndex_;
    }
    std::shared_ptr<wa::GainNode> master() const {
        return master_;
    }
    std::shared_ptr<wa::GainNode> busOutput(const std::string& name) const {
        auto it = nodes_.find(name);
        return it == nodes_.end() ? nullptr : it->second.user;
    }
    bool unlock() {
        if (!built_) {
            build();
        }
        unlocked_ = true;
        return true;
    }
    // the input node of a bus (CHARACTER with a speaker: its per-speaker gain)
    std::shared_ptr<wa::GainNode> bus(const std::string& name, int speaker = -1) {
        if (!built_) {
            return nullptr;
        }
        auto it = nodes_.find(name);
        if (it == nodes_.end()) {
            throw std::runtime_error("Unknown audio bus " + name);
        }
        return name == "CHARACTER" && speaker >= 0 && speaker < 10 ? speakers_[size_t(speaker)] : it->second.mix;
    }
    void setScaleMode(const std::string& mode) {
        if ((mode != "FE" && mode != "WORLD") || mode == scaleMode_) {
            return;
        }
        scaleMode_ = mode;
        refreshSliders();
    }
    const std::string& scaleMode() const {
        return scaleMode_;
    }
    double speakerGain(int k) const {
        return k >= 0 && k < int(speakers_.size()) && speakers_[size_t(k)] ? speakers_[size_t(k)]->gain.value() : 0;
    }
    // 287968: slider x mix target
    double busGain(const std::string& name) const {
        double lv = 1;
        if (name != "UI") {
            auto m = mixLevels_.find(name);
            if (m != mixLevels_.end()) {
                lv = m->second;
            } else if (size_t(mixIndex_) < mixes_.size()) {
                auto l = mixes_[size_t(mixIndex_)].levels.find(name);
                if (l != mixes_[size_t(mixIndex_)].levels.end()) {
                    lv = l->second;
                }
            }
        }
        return sliderLevel(name) * lv;
    }
    // 2883B0 SetMix
    void setMix(int index) {
        index = std::max(0, index);
        if (index == mixIndex_) {
            return;
        }
        if (size_t(index) >= mixes_.size()) {
            throw std::runtime_error("Unknown mix " + std::to_string(index));
        }
        const Mix& mix = mixes_[size_t(index)];
        mixIndex_ = index;
        for (const auto& bus : audioBuses()) {
            auto l = mix.levels.find(bus);
            if (l != mix.levels.end()) {
                mixLevels_[bus] = l->second;
            }
        }
        if (!built_) {
            return;
        }
        for (const auto& bus : audioBuses()) {
            auto l = mix.levels.find(bus);
            if (l != mix.levels.end()) {
                ramp(nodes_[bus].mix->gain, l->second, mix.timeMs / 1000);
            }
        }
    }
    // 287F00 duck (the mode 0 group) / release
    void duck(double level = 0.65, double seconds = 0.9999) {
        if (!built_) {
            return;
        }
        for (const char* bus : {"MUSIC", "UI", "AMBIENT", "BOARD", "COLLISION", "ARCADESFX"}) {
            ramp(nodes_[bus].duck->gain, level, seconds);
        }
    }
    void release(double seconds = 0.5) {
        if (!built_) {
            return;
        }
        for (const auto& bus : audioBuses()) {
            ramp(nodes_[bus].duck->gain, 1, seconds);
        }
    }
    void setSlider(const std::string& group, int steps) {
        sliders_[group] = std::clamp(steps, 0, kSliderSteps);
        refreshSliders();
    }
    int slider(const std::string& group) const {
        return sliders_.at(group);
    }
    void setDj(bool on) {
        djEnabled_ = on;
        refreshSliders();
    }
    void setRadioMode(int mode) {
        radioMode_ = mode;
        refreshSliders();
    }
    void setArcadeAudio(bool on) {
        arcadeAudio_ = on;
        refreshSliders();
    }
    // a hold (a movie, the hidden window): the declick fade, then the context stops
    void suspend(const std::string& reason = "hold") {
        holds_.push_back(reason);
        if (!built_ || ctx_.state != "running") {
            return;
        }
        fadeTo(0);
        ctx_.state = "suspended";
    }
    void resume(const std::string& reason = "hold") {
        holds_.erase(std::remove(holds_.begin(), holds_.end(), reason), holds_.end());
        if (!holds_.empty() || !built_) {
            return;
        }
        ctx_.state = "running";
        fadeTo(1);
    }

private:
    struct BusNodes {
        std::shared_ptr<wa::GainNode> mix, duck, user;
    };
    static const std::string& sliderOf(const std::string& bus) {
        static const std::map<std::string, std::string> s = {{"MUSIC", "music"},       {"DJ", "music"},      {"AMBIENT", "music"}, {"PA", "effects"},
                                                             {"UI", "effects"},        {"BOARD", "effects"}, {"COLLISION", "effects"},
                                                             {"ARCADESFX", "effects"}, {"ARCADESPEECH", "effects"}, {"CHARACTER", "speech"}};
        return s.at(bus);
    }
    // 2873D8 (step x 1/11, float) x 287700 scale (mul.s)
    double sliderLevel(const std::string& bus) const {
        auto f32 = [](double v) { return double(float(v)); };
        const auto& scale = channelScale(scaleMode_).bus;
        auto sc = scale.find(bus);
        const double v = f32(std::min(std::max(f32(sliders_.at(sliderOf(bus)) * f32(1.0 / kSliderSteps)), 0.0), 1.0) * f32(sc == scale.end() ? 1 : sc->second));
        if (bus == "DJ" && (!djEnabled_ || radioMode_ == 2 || radioMode_ == 3)) {
            return 0;
        }
        if ((bus == "ARCADESFX" || bus == "ARCADESPEECH") && !arcadeAudio_) {
            return 0;
        }
        return v;
    }
    void build() {
        master_ = ctx_.createGain();
        master_->gain.setValue(muted_ ? 0 : kMasterGain);
        master_->connect(ctx_.destination);
        if (declick_ > 0) {
            fader_ = ctx_.createGain();
            fader_->connect(master_);
        }
        const std::map<std::string, double>* levels = size_t(mixIndex_) < mixes_.size() ? &mixes_[size_t(mixIndex_)].levels : nullptr;
        for (const auto& bus : audioBuses()) {
            BusNodes n{ctx_.createGain(), ctx_.createGain(), ctx_.createGain()};
            double lv = 1;
            if (levels) {
                auto l = levels->find(bus);
                if (l != levels->end()) {
                    lv = l->second;
                }
            }
            n.mix->gain.setValue(lv);
            n.user->gain.setValue(sliderLevel(bus));
            n.mix->connect(n.duck);
            n.duck->connect(n.user);
            if (fader_) {
                n.user->connect(fader_);
            } else {
                n.user->connect(master_);
            }
            nodes_[bus] = n;
        }
        for (int k = 0; k < 10; ++k) {
            auto g = ctx_.createGain();
            g->gain.setValue(channelScale(scaleMode_).speakers[size_t(k)]);
            g->connect(nodes_["CHARACTER"].mix);
            speakers_.push_back(g);
        }
        built_ = true;
    }
    void ramp(wa::Param& param, double target, double seconds) {
        const double now = ctx_.currentTime();
        param.cancelScheduledValues(now);
        param.setValueAtTime(param.value(), now);
        if (seconds > 0) {
            param.linearRampToValueAtTime(target, now + seconds);
        } else {
            param.setValueAtTime(target, now);
        }
    }
    void refreshSliders() {
        if (!built_) {
            return;
        }
        for (const auto& bus : audioBuses()) {
            ramp(nodes_[bus].user->gain, sliderLevel(bus), 0.02);
        }
        for (size_t k = 0; k < speakers_.size(); ++k) {
            ramp(speakers_[k]->gain, channelScale(scaleMode_).speakers[k], 0.02);
        }
    }
    void fadeTo(double v) {
        if (!fader_) {
            return;
        }
        auto& g = fader_->gain;
        const double t = ctx_.currentTime();
        g.cancelAndHoldAtTime(t);
        g.linearRampToValueAtTime(v, t + declick_);
    }

    wa::Context& ctx_;
    std::vector<Mix> mixes_;
    double declick_;
    bool muted_;
    bool built_ = false;
    bool unlocked_ = false;
    int mixIndex_ = 0;
    std::string scaleMode_ = "FE";
    std::map<std::string, int> sliders_;
    bool djEnabled_ = true;
    int radioMode_ = 0;
    bool arcadeAudio_ = true;
    std::map<std::string, double> mixLevels_;
    std::map<std::string, BusNodes> nodes_;
    std::vector<std::shared_ptr<wa::GainNode>> speakers_;
    std::shared_ptr<wa::GainNode> master_;
    std::shared_ptr<wa::GainNode> fader_;
    std::vector<std::string> holds_;
};

}  // namespace audio

}  // namespace ssx_native
