#pragma once
// The sound-effect voices (web/sfx.js, 1:1; docs/audio-logic.md section 5 and 9.2): the bank slots, the game voice manager
// (2906B8 / 2AC868) and the EA SND patch voices (3BB588 / 3B55F0) on the audio_engine.hpp buses: every layer of a bank entry,
// patch volume / pitch / pan with their random spreads, the 100 Hz envelope, the volume and pitch LFO tables, the 3D gain and pan
// from the listener, the doppler bend overwrite, fades, pause, and the hardware voice pools (SPU 48, IOP 8) with stealing.
// The SPU voices' Gaussian interpolation is web/spu-interp.js (spuResample).
#include "audio_common.hpp"
#include "audio_decode.hpp"
#include "audio_engine.hpp"
#include "webaudio.hpp"
#include <array>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace ssx_native {

namespace audio {

namespace slot {
constexpr int MAIN = 0, BOARD = 1, MOUNTAIN = 2, DYNAMIC = 3, TRANSPORT = 4, CROWD = 5, AUX = 6, TRICKY = 7, WORLD8 = 8, WORLD9 = 9, LAND = 0xA,
              DYNAMIC_C = 0xC, LOADING = 0xD, GRUNT = 0xE, GRUNT_AI = 0xF, SPUBOARD = 0x10;
}

// 290B58: a 5-point piecewise-linear curve
struct Curve {
    std::array<double, 5> x;
    std::array<double, 5> y;
};
double curve(const Curve& c, double v);
namespace curves {
extern const Curve loopA_vol_482AF8, loopA_pitch_482B20, glide0_vol_482B78, glide0_pitch_482BA0, glide1_vol_482BC8, glide1_pitch_482BF0,
    glide2_vol_482C18, glide2_pitch_482C40, carve0_vol_482C68, carve0_pitch_482C90, carve1_vol_482CB8, carve1_pitch_482CE0, carve2_pitch_482D08,
    landing_vol_482D30, crash_vol_482DA0, contact_445898, bump_4458C0, t2_4458E8, grunt_445910;
}
// 291710 / 2934D0
int surfaceClass(double surface);
int boardFamily(int cls, double motion);
// 3C72A0: ((rand & 0x7FFF) - 0x4000) x x >> 14
int rndRange(int x, Random& random);

// web/spu-interp.js spuResample (48 kHz)
Decoded spuResample(const Decoded& d, int outRate = 48000);

class Sfx;

// a voice handle (the page's voice object, or the delayed request's handle)
class Voice : public std::enable_shared_from_this<Voice> {
public:
    void setVolume(double v);
    void setBend(double b);
    void setWheel(double b);
    void stop(double seconds = 0.25);
    bool playing() const;
    int slot = 0;
    int sound = 0;
    std::string bus;
    std::string tag;
    const void* owner = nullptr;
    // the world ambience's keep-alive mark (audio-world.js voice.keep)
    bool keep = false;

private:
    friend class Sfx;
    struct Layer {
        std::shared_ptr<wa::BufferSourceNode> source;
        std::shared_ptr<wa::StereoPannerNode> pan;
        double patchPan = 0;
        std::vector<std::shared_ptr<wa::BufferSourceNode>> lfo;
        std::shared_ptr<wa::GainNode> lg;
        double cents = 0;
        double wheelRange = 0;
        std::vector<std::shared_ptr<struct HwVoice>> hw;
        bool done = false;
        std::vector<std::shared_ptr<wa::Node>> nodes;
    };
    Sfx* sfx_ = nullptr;
    // a delayed request (2ADCA0): the real voice once its timer fired
    bool deferred_ = false;
    bool cancelled_ = false;
    double deferredBase_ = 127;
    std::shared_ptr<Voice> real_;
    Position position_;
    Position posRef_;
    double base_ = 127;
    double bend_ = 0x1000;
    std::function<void(Voice&, double)> callback_;
    bool ended_ = false;
    bool fading_ = false;
    double sent_ = -1;
    std::shared_ptr<wa::GainNode> out_;
    std::vector<std::shared_ptr<Layer>> layers_;
    double started_ = 0;
    double priority_ = 0;
    int wheel_ = 64;
    bool paused_ = false;
    double vanish_ = 30;
    friend struct HwVoice;
};

struct HwVoice {
    std::string pool;
    double priority = 0;
    uint64_t serial = 0;
    bool freed = false;
    std::weak_ptr<Voice::Layer> layer;
    std::weak_ptr<Voice> voice;
};

struct PlayOptions {
    int speaker = -1;
    int slot = 0;
    int sound = 0;
    std::string bus = "UI";
    double volume = 127;
    double bend = 0x1000;
    Position position;
    bool posStatic = false;
    std::function<void(Voice&, double)> callback;
    std::string tag;
    const void* owner = nullptr;
    int note = 60;
    double velocity = 127;
    int wheel = 64;
    double vanish = 30;
    double delayMs = 0;
};

struct Spatial {
    double g = 1;
    double az = 0;
    double d = -1;
    bool hasD = false;
};

class Sfx {
public:
    Sfx(AudioEngine& engine, Files& files, Timers& timers, Random& random, bool startAfterDecode = true, double evictMs = 30000)
        : engine_(engine), files_(files), timers_(timers), random_(random), startAfterDecode_(startAfterDecode), evictMs_(evictMs) {}

    struct Bank {
        std::string name;
        std::shared_ptr<const Json> json;
        Bytes bnk;
        std::map<int, std::vector<std::map<int, std::pair<double, size_t>>>> tags;
        bool ready() const {
            return bnk != nullptr;
        }
    };
    // loads a bank into a slot (the same name again: nothing); synchronous natively
    Bank* loadBank(int slot, const std::string& name);
    void unloadBank(int slot);
    Bank* bankOf(int slot);
    std::shared_ptr<Voice> play(PlayOptions o);
    // per frame (285BF8 -> 2AB958): callbacks, then the 3D / volume update
    void tick(double dt);
    struct StopFilter {
        std::optional<int> slot;
        std::optional<std::string> tag;
        const void* owner = nullptr;
        double fade = 0.25;
        std::function<bool(const Voice&)> keep;
    };
    void stopAll(const StopFilter& f);
    // 29CE28 -> 2AD2A8 -> 2ACAC8 (filter: which voices)
    void pauseAll(bool on, const std::function<bool(const Voice&)>& filter = nullptr);
    // the loading screen's warm-up: every patch of the given slots decoded into the buffer cache
    void warm(const std::vector<int>& slots);
    // the listener: three.js camera matrixWorldInverse (scene metres) and the origin (metres)
    void setListener(const std::array<double, 16>* inverse, const std::array<double, 3>& origin = {0, 0, 0});
    void setMono(bool on) {
        mono_ = on;
    }
    size_t voiceCount() const {
        return voices_.size();
    }
    Spatial spatial(const std::array<double, 3>* position, double vanish = 30) const;
    // QA: the play requests' log (sfx.js logPlays)
    void logPlays(bool on) {
        logging_ = on;
        log_ = Json::array();
    }
    const Json& plays() const {
        return log_;
    }

private:
    friend class Voice;
    struct LayerInfo {
        const Json* patch;
        std::map<int, std::pair<double, size_t>> tags;
        int index;
    };
    std::vector<LayerInfo> layersOf(Bank& bank, int sound);
    std::shared_ptr<const wa::Buffer> bufferFor(Bank& bank, int sound, const LayerInfo& layer);
    std::shared_ptr<const wa::Buffer> lfoBuffer(const std::string& key, const std::vector<int>& table, const std::function<double(int)>& map);
    std::shared_ptr<Voice> startVoice(const PlayOptions& o);
    std::vector<std::shared_ptr<HwVoice>> allocate(const std::string& pool, int n, double priority);
    void release(HwVoice& h);
    void finish(Voice& v);
    void cutLayer(Voice::Layer& l);
    void fadeOut(Voice& v, double seconds);
    void update(Voice& v, bool immediate = false);
    double panValue(double patchPanRad, double az) const;
    void evictLater(const std::string& name);
    void keep(const std::string& name);

    AudioEngine& engine_;
    Files& files_;
    Timers& timers_;
    Random& random_;
    bool startAfterDecode_;
    double evictMs_;
    bool mono_ = false;
    std::map<int, Bank> banks_;
    std::map<std::string, std::shared_ptr<const wa::Buffer>> buffers_;
    std::map<std::string, std::shared_ptr<const wa::Buffer>> lfoBuffers_;
    std::set<std::shared_ptr<Voice>> voices_;
    std::map<std::string, std::vector<std::shared_ptr<HwVoice>>> used_;
    uint64_t serial_ = 0;
    std::map<std::string, Timers::Id> evictTimers_;
    bool hasListener_ = false;
    std::array<double, 16> inverse_{};
    std::array<double, 3> origin_{};
    bool logging_ = false;
    Json log_ = Json::array();
};

}  // namespace audio

}  // namespace ssx_native
