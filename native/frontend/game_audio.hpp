#pragma once
// The game audio director (web/game-audio.js, 1:1; docs/audio-logic.md 3.3-3.9, 4.3, 9): the music flow (song choice, radio
// modes, PlayMusic / hub songs / podium chartune, the countdown and GO rules, MusicTrigger / Mix / Ambience painters, the request
// and DJ timers), the speech OnIdle flags, the world banks, the loading loop, and the per-frame (listener) and per-tick (gameTick)
// entry points, on audio_engine / audio_sfx / audio_pathfinder / audio_speech / audio_crowd / audio_world / audio_sfx_game.
// The page's async loads resolve at once natively (the files are read synchronously).
#include "audio_common.hpp"
#include "audio_crowd.hpp"
#include "audio_engine.hpp"
#include "audio_painters.hpp"
#include "audio_pathfinder.hpp"
#include "audio_sfx.hpp"
#include "audio_sfx_game.hpp"
#include "audio_speech.hpp"
#include "audio_world.hpp"
#include "webaudio.hpp"
#include <functional>
#include <memory>

namespace ssx_native {

namespace audio {

// 28D8A0 / 0x535C10
inline int courseCategory(int c) {
    return c <= 4 ? 0 : c <= 7 ? 1 : c <= 10 ? 2 : c <= 13 ? 3 : 4;
}
inline int eventKind(int c) {
    return courseCategory(c);
}
inline int courseIndexOf(const std::string& code) {
    static const std::map<std::string, int> m = {{"ARA1", 0}, {"BRA2", 1}, {"CRA3", 2}, {"DRA4", 3}, {"ERA5", 4}, {"ASS1", 5},
                                                 {"DSS2", 6}, {"ESS3", 7}, {"ABA1", 8}, {"CBA2", 9}, {"EBA3", 10}, {"BHP1", 11},
                                                 {"CHP2", 12}, {"EHP3", 13}, {"ABC1", 14}, {"DBC2", 15}, {"EBC3", 16}};
    auto it = m.find(code);
    return it == m.end() ? -1 : it->second;
}

struct AudioSettings {
    int music = kSliderDefault, effects = kSliderDefault, speech = kSliderDefault;
    int radioMode = 0;
    bool dj = true;
    bool arcadeAudio = true;
    std::vector<bool> playlist = std::vector<bool>(35, true);
};

// 2A1820's records (web/game-audio.js relSnapshot): {scores 155B50(a, b) levels, kinds 155AB0(a, b)} 6 x 6, kind -1 = none
struct Relations {
    std::array<std::array<int, 6>, 6> scores{};
    std::array<std::array<int, 6>, 6> kinds{};
};
// web/sfx-game.js relationOf: 2 foe, 1 / 0 friend; none without records
inline std::optional<int> relationOf(const std::optional<Relations>& m, int a, int b) {
    if (!m) {
        return std::nullopt;
    }
    auto in = [](int i) { return i >= 0 && i < 6; };
    auto score = [&](int x, int y) { return in(x) && in(y) ? m->scores[size_t(x)][size_t(y)] : 0; };
    if (score(a, b) >= 2 || score(b, a) >= 2) {
        return 2;
    }
    const int k = in(a) && in(b) ? m->kinds[size_t(a)][size_t(b)] : -1;
    return k == 0 ? 0 : k == 1 ? 1 : 2;
}
// one rider pair reaction of the tick (web/ai-racers.js onPairAudio with the core's records: the core's relation of that
// moment, -1 = none)
struct PairEvent {
    int target = 0;
    int other = 0;
    int audio = -1;
};

// one rider of the game tick (gameTick's list): its core context and state
struct GameTickRider {
    const void* key = nullptr;
    std::string character;
    int slot = 0;
    bool human = false;
    bool finished = false;
    std::function<void(const std::function<void()>&)> in;
};

class GameAudio {
public:
    struct Options {
        std::string root;
        float sampleRate = 48000;
        bool muted = false;
        AudioSettings settings;
    };
    explicit GameAudio(Options o);
    ~GameAudio();

    wa::Context& context() {
        return ctx_;
    }
    AudioEngine& engine() {
        return *engine_;
    }
    Sfx& sfx() {
        return *sfx_;
    }
    Speech& speech() {
        return *speech_;
    }
    const AudioSettings& settings() const {
        return settings_;
    }
    bool raceMusic() const {
        return state_.raceMusic;
    }
    // career context (main.js gameAudio.context): {career, round (single events run round 3), mode}
    struct Context {
        bool career = false;
        int round = 3;
        int mode = -1;
    };
    std::function<Context()> contextFn;
    // the human rider's radio mode and custom playlist from its profile record at a world load (api.riderMusic: web/audio-menu.js
    // riderMusic); none: the settings stay
    struct RiderMusic {
        int radioMode = 0;
        std::vector<bool> playlist;
    };
    std::function<std::optional<RiderMusic>()> riderMusic;

    // ---- the page's api ----
    void ui(int ev);
    void uiSound(int snd);
    void screen(const std::string& name);
    void loadingStart(const std::string& courseCode, const std::string& character);
    void prepare(const std::string& courseCode, const std::string& character, bool warm = true);
    void worldLoaded(int courseIndex, bool singleEvent, const std::string& courseCode, const std::string& character, int rounds = 1);
    void runStart(int courseIndex, bool singleEvent, const std::string& courseCode, const std::string& character);
    void go(std::optional<bool> medalRun = std::nullopt);
    void finish(bool runsLeft, int place, bool timedOut, const std::string& winner = "");
    void podium(std::optional<int> place = std::nullopt, const std::string& winner = "");
    bool restartRun(bool fromResults = false);
    void leaveWorld();
    void pause(bool on);
    void setMix(int index) {
        engine_->setMix(index < 0 ? 0 : index);
    }
    void cutscene(bool on) {
        nis_ = on;
    }
    void cutsceneMusic(int code);
    void sendEvent(int e);
    // per rendered frame: the timers, speech and voice updates at 60 Hz, the listener (three.js camera inverse, scene = metres -
    // origin) and its position
    void listener(const std::array<double, 16>* cameraInverse, const std::array<double, 3>& cameraPosition, const std::array<double, 3>& origin);
    // per 60 Hz game tick (main.js simTick, after the riders and the course events)
    struct TickState {
        const float* raceInfo = nullptr;
        bool finished = false;
        double pending = 0;
        std::string character;
        bool running = true;
        std::optional<int> place;
        std::string winner;
        std::vector<GameTickRider> riders;
        // the human core's exports (its context is current)
        std::function<std::optional<std::vector<float>>()> hudSlots;
        std::function<bool()> timedOut;
        std::function<int()> contactTrack;
        std::function<std::optional<Vec>(uint32_t)> instancePosition;
        // the computer riders' race (gameTick's ai): its records after this tick (null: none or a kind missing) and the tick's
        // pair reactions (drainCoreRelationships, before this call)
        bool ai = false;
        std::function<std::optional<Relations>()> relations;
        std::vector<PairEvent> pairEvents;
        // the human core's avalanches (web/avalanche-state.js avalancheState / takeLoopEvents)
        struct Avalanche {
            bool loop = false;
            std::vector<std::array<double, 4>> tumblers;
            std::vector<double> loopEvents;
        };
        std::optional<Avalanche> avalanche;
    };
    void gameTick(const TickState& s);
    // the now-playing notice (28F478 HUD 5)
    std::function<void(const std::string& title, const std::string& artist, const std::string& album)> onNowPlaying;
    struct Title {
        std::string title, artist, album;
    };
    std::optional<Title> songTitle() const;
    // QA: the director timeline [ms, kind, args...]
    const Json& timeline() const {
        return timeline_;
    }
    void dispatchEnded() {
        ctx_.dispatch();
    }
    void runTimers() {
        timers_.run();
    }

private:
    void tl(const std::string& kind, Json args = Json::array());
    std::vector<const Json*> feSongs() const;
    bool inPlaylist(int i) const;
    bool musicMode() const {
        return settings_.radioMode != 2;
    }
    std::shared_ptr<pf::Player> playSong(const std::string& id, int event, bool ambience = false, std::optional<int> intensity = std::nullopt);
    std::string songFile(const std::string& id) const;
    void stopMusic();
    void fadeOutMusic(double seconds);
    void pickSong(int eventHint = 0);
    void playMusic(int event, bool forced = false, int songId = -1, bool t0 = false);
    void playHubSong(int peak);
    void playWorldMusic(int event);
    void loadingLoop(bool on);
    void loadWorldBanks(const std::string& character);
    void onIdle();
    void applyRiderMusic();
    void warmWorld();
    void djTimer(int kind, int sed = -1);
    void post(const std::string& key, double ms, std::function<bool()> fn);
    void cancel(const std::string& key);
    void pumpTimers();
    std::function<bool()> request(int kind, bool force);
    void executeRequest(int kind);
    bool latch(int id);
    void musicTrigger(int a, int b);
    bool changeSong(bool retry);
    void ambiencePainter(int amb);
    int currentSed() const;
    int currentCategory() const {
        return courseCategory(state_.course);
    }
    int round() const {
        return contextFn ? contextFn().round : 3;
    }
    const Json* songOf(const std::string& id) const;
    struct CourseFlags {
        bool bigAir, halfPipe, backcountry;
    };
    CourseFlags courseFlags(int c) const {
        return {(c >= 8 && c <= 10) || eventKind(c) == 2, (c >= 11 && c <= 13) || eventKind(c) == 3, c >= 14 && c <= 16};
    }
    const Json* worldDoc();

    wa::Context ctx_;
    Files files_;
    Timers timers_;
    Random random_;
    AudioSettings settings_;
    std::shared_ptr<const Json> catalog_;
    std::unique_ptr<AudioEngine> engine_;
    std::unique_ptr<Sfx> sfx_;
    std::unique_ptr<Speech> speech_;
    std::unique_ptr<Crowd> crowd_;
    std::unique_ptr<WorldAudio> world_;
    std::unique_ptr<SfxGame> sfxGame_;
    std::shared_ptr<const Json> clipTable_;
    Json timeline_ = Json::array();
    bool nis_ = false;
    double frameAt_ = -1;
    double gameTickAt_ = -1e9;
    struct DirectorTimer {
        std::string key;
        double at;
        std::function<bool()> fn;
    };
    std::vector<DirectorTimer> dirTimers_;
    struct State {
        std::shared_ptr<pf::Player> music;
        std::string musicId;
        std::string currentId;
        bool musicLoading = false;
        std::optional<int> feEvent;
        int previousSong = -1;
        int songIndex = -1;
        bool goSeen = false;
        int chartuneIndex = -1;
        bool paused = false;
        bool countdownHold = false;
        int resumeGo = 1;
        int course = 0;
        std::string courseCode;
        bool singleEvent = true;
        bool inWorld = false;
        bool raceMusic = false;
        bool ambienceMode = false;
        bool replay = false;
        bool restarting = false;
        bool lodge = false;
        std::string character;
        int place = 0;
        std::string winner;
        int intensityFrames = 0;
        struct Trigger {
            int a = -99, b = -99, zone = 0, zoneExit = 0, rearm = 1, latch = -1;
        } trigger;
        std::map<std::string, int> pendingDj;
        int region = -1;
        int track = -1;
        int ticks = 0;
        bool challengeMusic = false, challengePrompt = false;
        int challengeType = 0;
        std::optional<std::array<double, 3>> listenerCm;
        struct Loading {
            std::shared_ptr<wa::BufferSourceNode> source;
            std::shared_ptr<wa::GainNode> gain;
        };
        std::optional<Loading> loading;
        bool loadingWanted = false;
    } state_;
    // the relationship snapshot at the tick start and this tick's pair reactions (relTick, relPair); hookPairs ran
    std::optional<Relations> relTick_;
    std::map<std::pair<int, int>, int> relPair_;
    bool pairsHooked_ = false;
    void relationsReset() {
        relTick_.reset();
        relPair_.clear();
    }
    std::shared_ptr<const Json> worldDocCache_;
    std::string worldDocCode_;
};

}  // namespace audio

}  // namespace ssx_native
