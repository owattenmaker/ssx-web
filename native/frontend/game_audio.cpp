#include "game_audio.hpp"
#include <cstdio>
#include <regex>

namespace ssx_native {

namespace audio {

namespace {
// 28F140: charsel's event per front-end screen
const std::map<std::string, int>& feEvent() {
    static const std::map<std::string, int> m = {{"title", 0},          {"main", 1},          {"character", 1},      {"setup", 1},       {"details", 1},
                                                 {"event", 1},          {"options", 4},       {"fe-music", 4},       {"fe-playlist", 4}, {"fe-sound", 4},
                                                 {"ctm-mcomm", 1},      {"ctm-lodge", 1},     {"ctm-equip", 2},      {"ctm-gear", 2},    {"ctm-attributes", 3},
                                                 {"ctm-rewards", 6},    {"ctm-trophies", 7},  {"ctm-stats", 8}};
    return m;
}
// 294F78: UI event -> bank 0 sound
const std::map<int, int>& uiSounds() {
    static const std::map<int, int> m = {{0, 3}, {9, 3}, {1, 4}, {10, 4}, {2, 2}, {11, 2}, {3, 1}, {12, 1}, {4, 0xd}, {13, 0xd}, {7, 0x10}, {8, 0x11}, {14, 0x12}, {15, 7}};
    return m;
}
// 28CDF8: chartune event by the winner's CHARDB id
constexpr int kChartune[10] = {10, 2, 8, 1, 5, 9, 3, 7, 4, 6};
constexpr double kDeclickMs = 5;
constexpr double kSongLeadS = 0.04;
constexpr double kMusicLookaheadS = 2.5;
bool isPeakSong(const std::string& id) {
    return id == "Peak1" || id == "Peak2" || id == "Peak3";
}
}  // namespace

GameAudio::GameAudio(Options o) : ctx_(o.sampleRate), files_(o.root), settings_(o.settings) {
    catalog_ = files_.json("catalog.json");
    std::vector<Mix> mixes;
    int index = 0;
    for (const auto& m : (*catalog_)["mixes"]) {
        Mix x;
        x.index = index++;
        x.timeMs = m.value("TIME", 0.0);
        for (const char* b : {"MUSIC", "DJ", "PA", "CHARACTER", "BOARD", "COLLISION", "AMBIENT", "ARCADESFX", "ARCADESPEECH"}) {
            const double v = m.value(b, 100.0);
            if (v >= 0) {
                x.levels[b] = v / 100;
            }
        }
        mixes.push_back(x);
    }
    AudioEngine::Options eo;
    eo.mixes = mixes;
    eo.music = settings_.music;
    eo.effects = settings_.effects;
    eo.speech = settings_.speech;
    eo.djEnabled = settings_.dj;
    eo.radioMode = settings_.radioMode;
    eo.arcadeAudio = settings_.arcadeAudio;
    eo.declickMs = kDeclickMs;
    eo.muted = o.muted;
    engine_ = std::make_unique<AudioEngine>(ctx_, eo);
    // natively the context runs from the start (no user gesture to wait for)
    engine_->unlock();
    sfx_ = std::make_unique<Sfx>(*engine_, files_, timers_, random_, true);
    sfx_->loadBank(slot::MAIN, "SSX3Menu");
    speech_ = std::make_unique<Speech>(*engine_, files_, timers_, random_, kDeclickMs, [] { return false; });
    try {
        speech_->init();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Speech events unavailable: %s\n", e.what());
    }
    speech_->onIdle = [this] { onIdle(); };
    speech_->onPost = [this](uint32_t id, int speaker) {
        if (speaker == 0xa || speaker == 0xb) {
            char hex[16];
            std::snprintf(hex, sizeof hex, "%x", id);
            tl("speech", Json::array({hex}));
        }
    };
    speech_->onLines = [this](const std::vector<std::string>& banks, int speaker) {
        if (speaker == 0xa || speaker == 0xb) {
            std::string joined;
            for (size_t i = 0; i < banks.size(); i++) {
                joined += (i ? "+" : "") + banks[i];
            }
            tl("line", Json::array({joined}));
        }
    };
    crowd_ = std::make_unique<Crowd>(*sfx_, files_, crowdConfig(catalog_->contains("crowdInf") ? (*catalog_)["crowdInf"] : Json::array()), random_);
    world_ = std::make_unique<WorldAudio>(*sfx_, *crowd_, files_, [this](int) {
        // 2B6550 -> 2A34D0: the listener rider's place
        if (!(state_.course < 5 || eventKind(state_.course) == 0)) {
            return;
        }
        if (speech_->free()) {
            speech_->riderPosition(charId(state_.character), state_.place, true);
        }
    });
    MusicAdapter m;
    m.duckToLoops = [this] {
        const Json* s = songOf(state_.musicId);
        return (s ? s->value("DUCKTOLOOPS", 1) : 1) == 1 && musicMode() && state_.music != nullptr;
    };
    m.streamLevel = [this](double v) {
        if (state_.music) {
            state_.music->setLevel(v);
        }
    };
    m.loopsLevel = [this](double v) {
        if (state_.music && state_.music->hasLoops()) {
            state_.music->loopsSetLevel(v);
        }
    };
    m.loopsStart = [this] {
        if (!state_.music || !state_.music->hasLoops()) {
            return false;
        }
        state_.music->loopsStart();
        return true;
    };
    m.loopsStop = [this] {
        if (state_.music && state_.music->hasLoops()) {
            state_.music->loopsStop();
        }
    };
    m.loopsRequested = [this] { return state_.music && state_.music->hasLoops() && state_.music->loopsRequested(); };
    m.event = [this](int e) {
        if (musicMode()) {
            tl("event", {e});
            if (state_.music) {
                state_.music->send(e);
            }
        }
    };
    m.resetIntensity = [this] { state_.intensityFrames = 0; };
    // 29C420(digit): "3" fades a hub song / chartune over 2 s; "1" with no song going (or a hub song / chartune) picks the next
    // playlist song and plays it paused for GO's resume
    m.countdown = [this](int d) {
        if (!musicMode() || !state_.resumeGo) {
            return;
        }
        const bool special = isPeakSong(state_.musicId) || state_.musicId == "chartune";
        if (d == 3 && special && state_.music) {
            fadeOutMusic(2);
            return;
        }
        if (d == 1 && (special || !state_.music || state_.music->finished())) {
            state_.countdownHold = true;
            pickSong();
            playMusic(0);
        }
    };
    try {
        clipTable_ = files_.json("anim-events.json");
    } catch (...) {
    }
    sfxGame_ = std::make_unique<SfxGame>(*engine_, *sfx_, *speech_, *crowd_, *world_, m, random_, [this](int clip, int bit) -> std::optional<int> {
        if (!clipTable_ || !clipTable_->contains("clips")) {
            return std::nullopt;
        }
        const std::string k = std::to_string(clip >> 8);
        const auto& clips = (*clipTable_)["clips"];
        if (!clips.contains(k)) {
            return std::nullopt;
        }
        const auto& e = clips[k];
        const std::string b = std::to_string(bit);
        if (e.is_object() && e.contains(b) && e[b].is_number()) {
            return e[b].get<int>();
        }
        if (e.is_array() && bit >= 0 && size_t(bit) < e.size() && e[size_t(bit)].is_number()) {
            return e[size_t(bit)].get<int>();
        }
        return std::nullopt;
    });
    sfxGame_->G.zoneActive = [this] { return state_.trigger.zone != 0; };
    // overtakes: the records at the tick-start ranking; hits: the reaction's own value of that moment, else the same
    sfxGame_->G.relation = [this](int a, int b) { return relationOf(relTick_, a, b); };
    sfxGame_->G.pairRelation = [this](int a, int b) -> std::optional<int> {
        auto it = relPair_.find({a, b});
        if (it != relPair_.end()) {
            return it->second;
        }
        return relationOf(relTick_, a, b);
    };
}

GameAudio::~GameAudio() {
    std::lock_guard<std::recursive_mutex> lock(ctx_.mutex());
    stopMusic();
}

void GameAudio::tl(const std::string& kind, Json args) {
    Json e = Json::array({std::round(nowMs()), kind});
    for (auto& a : args) {
        e.push_back(a);
    }
    timeline_.push_back(e);
    if (timeline_.size() > 800) {
        timeline_.erase(timeline_.begin());
    }
}

const Json* GameAudio::songOf(const std::string& id) const {
    for (const auto& s : (*catalog_)["songs"]) {
        if (s.value("id", "") == id) {
            return &s;
        }
    }
    return nullptr;
}

std::vector<const Json*> GameAudio::feSongs() const {
    std::vector<const Json*> out;
    for (const auto& s : (*catalog_)["songs"]) {
        if (s.value("ADDTOFE", 0) != 0 && out.size() < 35) {
            out.push_back(&s);
        }
    }
    return out;
}

bool GameAudio::inPlaylist(int i) const {
    if (settings_.radioMode == 1 || settings_.radioMode == 3) {
        bool any = false;
        for (bool b : settings_.playlist) {
            any = any || b;
        }
        return any ? (size_t(i) < settings_.playlist.size() && settings_.playlist[size_t(i)]) : true;
    }
    const auto& pl = (*catalog_)["playlist"];
    if (!pl.is_array() || pl.empty() || !pl[0].contains("songs")) {
        return true;
    }
    const auto songs = feSongs();
    if (i < 0 || size_t(i) >= songs.size()) {
        return false;
    }
    std::string id = songs[size_t(i)]->value("id", "");
    std::transform(id.begin(), id.end(), id.begin(), ::tolower);
    for (const auto& n : pl[0]["songs"]) {
        std::string x = n.get<std::string>();
        std::transform(x.begin(), x.end(), x.begin(), ::tolower);
        if (x == id) {
            return true;
        }
    }
    return false;
}

std::string GameAudio::songFile(const std::string& id) const {
    for (const auto& m : (*catalog_)["music"]) {
        if (m.value("id", "") == id && m.contains("json")) {
            std::string j = m["json"].get<std::string>();
            if (j.rfind("music/", 0) == 0) {
                j = j.substr(6);
            }
            if (j.size() > 5 && j.substr(j.size() - 5) == ".json") {
                j = j.substr(0, j.size() - 5);
            }
            return j;
        }
    }
    return id;
}

std::shared_ptr<pf::Player> GameAudio::playSong(const std::string& id, int event, bool ambience, std::optional<int> intensity) {
    tl("play", {id, event});
    {
        std::lock_guard<std::recursive_mutex> lock(ctx_.mutex());
        stopMusic();
    }
    state_.currentId = id;
    state_.ambienceMode = ambience;
    // the song's files outside the context's lock (the page awaits loadSong; the audio thread renders on meanwhile)
    std::shared_ptr<pf::Song> song;
    try {
        song = pf::loadSong(files_, songFile(id));
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Music %s unavailable: %s\n", id.c_str(), e.what());
        return nullptr;
    }
    std::lock_guard<std::recursive_mutex> lock(ctx_.mutex());
    const int level = intensity ? *intensity : (state_.raceMusic ? 0 : 127);
    pf::PlayerOptions po;
    po.context = &ctx_;
    po.destination = engine_->bus(ambience ? "AMBIENT" : "MUSIC");
    po.song = song;
    po.random = [this] { return random_(); };
    po.intensity = level;
    po.ambience = ambience;
    po.clockRandom = true;
    po.loopDestination = ambience ? nullptr : engine_->master();
    po.declickMs = kDeclickMs;
    po.lookahead = kMusicLookaheadS;
    po.eventFirst = true;
    po.timers = &timers_;
    auto player = std::make_shared<pf::Player>(po);
    state_.intensityFrames = 0;
    state_.music = player;
    state_.musicId = id;
    player->start(event, ctx_.currentTime() + kSongLeadS);
    if (state_.paused || state_.countdownHold) {
        player->pause();
    }
    // 28F478: the now-playing notice
    const Json* s = songOf(id);
    if (s && s->value("ADDTOFE", 0) != 0 && state_.inWorld && engine_->busGain("MUSIC") > 0) {
        tl("nowplaying", {s->value("TITLE", "")});
        if (onNowPlaying) {
            onNowPlaying(s->value("TITLE", ""), s->value("ARTIST", ""), s->value("ALBUM", ""));
        }
    }
    return player;
}

std::optional<GameAudio::Title> GameAudio::songTitle() const {
    const Json* s = songOf(state_.musicId);
    if (!s || s->value("ADDTOFE", 0) == 0) {
        return std::nullopt;
    }
    return Title{s->value("TITLE", ""), s->value("ARTIST", ""), s->value("ALBUM", "")};
}

void GameAudio::stopMusic() {
    if (state_.music) {
        state_.music->stop();
    }
    state_.music = nullptr;
}

void GameAudio::fadeOutMusic(double seconds) {
    auto m = state_.music;
    state_.music = nullptr;
    tl("fade", {seconds});
    if (m) {
        m->fadeOut(seconds);
    }
}

void GameAudio::sendEvent(int e) {
    tl("event", {e});
    if (state_.music) {
        state_.music->event(e);
    }
}

// 28D488 PickNextSong
void GameAudio::pickSong(int) {
    const auto songs = feSongs();
    const int count = int(songs.size());
    if (!count) {
        return;
    }
    const int category = currentCategory();
    auto inCategory = [&](int i) {
        const Json* s = songs[size_t(i)];
        if (!s->contains("categories")) {
            return false;
        }
        for (const auto& c : (*s)["categories"]) {
            if (c.is_number() && c.get<int>() == category) {
                return true;
            }
        }
        return false;
    };
    bool hasCategory = false;
    for (int i = 0; i < count; ++i) {
        hasCategory = hasCategory || (inPlaylist(i) && inCategory(i));
    }
    auto next = [&](int index, bool useCategory) {
        for (int n = 0; n < count; ++n) {
            index = (index + 1) % count;
            if (inPlaylist(index) && (!useCategory || !hasCategory || inCategory(index))) {
                return index;
            }
        }
        return index;
    };
    auto rand15 = [&] { return std::floor(random_() * 0x8000); };
    int index = int(std::floor(rand15() * count / 0x7fff)) % count;
    const int steps = int(std::floor(rand15() * 10 / 0x7fff)) + 1;
    for (int i = 0; i < steps; ++i) {
        index = next(index, true);
    }
    if (index == state_.previousSong) {
        index = next(index, true);
        if (index == state_.previousSong) {
            index = next(index, false);
        }
    }
    state_.songIndex = index;
    tl("pick", {songs[size_t(index)]->value("id", "")});
}

// 28CF98 PlayMusic
void GameAudio::playMusic(int event, bool forced, int songId, bool t0) {
    if (state_.replay) {
        return;
    }
    state_.trigger.zone = 0;
    state_.trigger.zoneExit = 0;
    if (!musicMode()) {
        const int peak = songId > 0 ? songId : peakMask(state_.course);
        playSong(std::string("Peak") + (peak == 1 ? "1" : peak == 2 ? "2" : "3") + "Amb", 12, true, 127);
        return;
    }
    if (state_.course >= 17 && !forced && !t0) {
        playHubSong(peakMask(state_.course));
        return;
    }
    const auto songs = feSongs();
    if (state_.songIndex < 0) {
        pickSong();
    }
    if (state_.songIndex < 0 || size_t(state_.songIndex) >= songs.size()) {
        return;
    }
    state_.previousSong = state_.songIndex;
    playSong(songs[size_t(state_.songIndex)]->value("id", ""), t0 && state_.course >= 17 ? 36 : event);
}

void GameAudio::playHubSong(int peak) {
    playSong(peak == 1 ? "Peak1" : peak == 2 ? "Peak2" : "Peak3", 12, false, 127);
}

void GameAudio::playWorldMusic(int event) {
    if (musicMode()) {
        pickSong();
    }
    playMusic(event);
}

// 28F768 / 28FA98: LoadingScreen.bnk looped on MUSIC, 1 s fade out
void GameAudio::loadingLoop(bool on) {
    if (on != state_.loadingWanted) {
        tl("loading", {on ? 1 : 0});
    }
    state_.loadingWanted = on;
    std::lock_guard<std::recursive_mutex> lock(ctx_.mutex());
    if (!on) {
        auto l = state_.loading;
        state_.loading.reset();
        if (l) {
            const double t = ctx_.currentTime();
            l->gain->gain.setValueAtTime(l->gain->gain.value(), t);
            l->gain->gain.linearRampToValueAtTime(0, t + 1);
            l->source->stop(t + 1.05);
        }
        return;
    }
    if (state_.loading) {
        return;
    }
    try {
        auto bank = files_.json("banks/LoadingScreen.json");
        auto bnk = files_.bytes("banks/LoadingScreen.bnk");
        const auto& p = (*bank)["entries"][0]["patches"][0];
        SampleInfo info;
        info.codec = p.value("codec", 5);
        info.channels = p.value("channels", 1);
        info.sampleRate = p.value("sampleRate", 22050);
        info.sampleCount = p.value("sampleCount", int64_t(0));
        info.loopStart = p.value("loopStart", int64_t(-1));
        info.loopEnd = p.value("loopEnd", int64_t(-1));
        for (const auto& o : p["dataOffsets"]) {
            info.dataOffsets.push_back(o.get<int64_t>());
        }
        const Decoded d = decodeBankPatch(bnk->data(), bnk->size(), info);
        auto buf = std::make_shared<wa::Buffer>();
        buf->sampleRate = float(d.sampleRate);
        buf->length = std::max<size_t>(1, d.length);
        for (const auto& c : d.data) {
            buf->channels.push_back(c);
            buf->channels.back().resize(buf->length, 0.f);
        }
        State::Loading l{ctx_.createBufferSource(), ctx_.createGain()};
        l.source->buffer = buf;
        l.source->loop = true;
        l.source->connect(l.gain);
        l.gain->connect(engine_->bus("MUSIC"));
        l.source->start();
        auto src = l.source;
        auto gain = l.gain;
        l.source->onended = [src, gain] {
            src->disconnect();
            gain->disconnect();
        };
        state_.loading = l;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Loading loop unavailable: %s\n", e.what());
    }
}

void GameAudio::loadWorldBanks(const std::string& character) {
    const Json* w = nullptr;
    for (const auto& b : (*catalog_)["banksInf"]) {
        if (b.value("name", "") == "WORLD") {
            w = &b;
        }
    }
    auto name = [](std::string f) {
        if (f.size() > 4) {
            std::string ext = f.substr(f.size() - 4);
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".bnk") {
                f = f.substr(0, f.size() - 4);
            }
        }
        std::string lower = f;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        return lower == "zbxsfx" ? std::string("zBxsfx") : f;
    };
    auto bank = [&](int s, const char* key) {
        if (w && w->contains(key) && (*w)[key].is_string()) {
            sfx_->loadBank(s, name((*w)[key].get<std::string>()));
        }
    };
    bank(slot::MAIN, "MAIN");
    bank(slot::BOARD, "BOARD");
    bank(slot::SPUBOARD, "SPUBOARD");
    bank(slot::MOUNTAIN, "MOUNTAIN");
    bank(slot::CROWD, "CROWD");
    bank(slot::TRANSPORT, "TRANSPORT");
    bank(slot::LAND, "LAND");
    static const char* grnt[10] = {"MOB", "KAO", "ARI", "MAC", "ZOE", "GRF", "ELI", "NAT", "PSY", "VIG"};
    sfx_->loadBank(slot::GRUNT, std::string("GRNT_") + grnt[gruntIndex(character)]);
    sfx_->loadBank(slot::GRUNT_AI, "GRNT_AI");
}

// 2A43B8: one pending DJ flag per idle edge, in this order (the free-ride / Conquer the Mountain ones are not ported natively)
void GameAudio::onIdle() {
    auto& p = state_.pendingDj;
    auto take = [&](const char* k) {
        auto it = p.find(k);
        if (it == p.end() || !it->second) {
            return false;
        }
        it->second = 0;
        return true;
    };
    if (take("radioBigIntro")) {
        speech_->radioBigIntro(0, 0);
        return;
    }
    if (take("artist")) {
        if (musicMode()) {
            speech_->artistIntro(p.count("sed") ? p["sed"] : -1);
        }
        return;
    }
    if (take("eventIntro")) {
        speech_->eventIntro(state_.course, p.count("region") ? p["region"] : -1);
        return;
    }
    if (take("radioBigOutro")) {
        speech_->radioBigOutro();
    }
    if (take("finishLine")) {
        speech_->finishLineRider({charId(state_.character)}, state_.place, true);
    }
}

void GameAudio::post(const std::string& key, double ms, std::function<bool()> fn) {
    dirTimers_.push_back({key, nowMs() + ms, std::move(fn)});
}

void GameAudio::cancel(const std::string& key) {
    dirTimers_.erase(std::remove_if(dirTimers_.begin(), dirTimers_.end(), [&](const DirectorTimer& t) { return t.key == key; }), dirTimers_.end());
}

// 2ADCA0 / 2ADDE0: a callback returning false stays queued
void GameAudio::pumpTimers() {
    const double t = nowMs();
    for (size_t i = 0; i < dirTimers_.size();) {
        if (dirTimers_[i].at > t) {
            ++i;
            continue;
        }
        DirectorTimer e = dirTimers_[i];
        dirTimers_.erase(dirTimers_.begin() + long(i));
        if (!e.fn()) {
            e.at = t + 1;
            dirTimers_.push_back(e);
        }
    }
}

std::function<bool()> GameAudio::request(int kind, bool force) {
    return [this, kind, force] {
        if (!force && state_.music && !state_.music->finished()) {
            return false;
        }
        executeRequest(kind);
        return true;
    };
}

void GameAudio::executeRequest(int kind) {
    tl("request", {kind});
    if (kind == 0 || kind == 2) {
        // the hub / transport requests: free ride only
        return;
    }
    if (kind == 4) {
        playMusic(0);
        return;
    }
    if (musicMode()) {
        playMusic(kind == 3 ? 36 : 0);
    }
}

bool GameAudio::latch(int id) {
    auto& T = state_.trigger;
    if (T.latch == id) {
        return false;
    }
    T.latch = id;
    return true;
}

int GameAudio::currentSed() const {
    const Json* s = songOf(!state_.currentId.empty() ? state_.currentId : state_.musicId);
    return s ? s->value("SEDVALUE", -1) : -1;
}

void GameAudio::djTimer(int kind, int sed) {
    tl("dj", {kind});
    if (kind >= 3 && kind <= 5) {
        if (sed == -1) {
            return;
        }
        if (kind != 5) {
            speech_->stop();
            speech_->radioBigIntro(kind == 4 ? 0 : 1, 0);
            speech_->flush();
        }
        state_.pendingDj["artist"] = 1;
        state_.pendingDj["sed"] = sed;
    }
}

// 28D988: the MusicTrigger painter (radio modes 0 / 1 / 3)
void GameAudio::musicTrigger(int a, int b) {
    auto& T = state_.trigger;
    const bool changed = a != T.a || b != T.b;
    auto leave = [&] {
        if (T.zone) {
            sendEvent(T.zone + 1);
        }
        T.zone = 0;
        T.zoneExit = 0;
    };
    bool store = true;
    if (a == -1) {
        if (T.a == 1 || T.a == 2) {
            leave();
        }
        T.rearm = 1;
    } else if (a == 1) {
        if (changed && !(state_.challengeMusic || state_.challengePrompt)) {
            if (T.zone && T.zone != b) {
                sendEvent(T.zone + 1);
            }
            sendEvent(b);
            T.zone = b;
            T.zoneExit = 0;
        }
    } else if (a == 2) {
        if (changed) {
            if (T.zone == b) {
                T.zoneExit = b;
            } else {
                leave();
            }
        }
    } else if (a == 0) {
        if (b == 0) {
            if (!state_.music) {
                playMusic(0);
            } else {
                sendEvent(0);
            }
        } else if (b == 11 || b == 18) {
            // the hub approach (11) and the free-ride song change (18): free ride only
        } else if (b == 13) {
            if (latch(13)) {
                sendEvent(13);
            }
        } else if (b >= 14 && b <= 17) {
            if (latch(b) && changed) {
                state_.pendingDj["eventIntro"] = 1;
                state_.pendingDj["region"] = b;
                state_.pendingDj["radioBigOutro"] = 1;
            }
        } else if (b == 42) {
            if (changed && state_.music) {
                state_.music->send(42);
            }
        } else {
            sendEvent(b);
        }
    } else {
        store = false;
    }
    if (store) {
        T.a = a;
        T.b = b;
    }
}

bool GameAudio::changeSong(bool) {
    return false;
}

// 2899F8: the Ambience painter (radio mode 2)
void GameAudio::ambiencePainter(int amb) {
    if (amb < 0 || amb == 33) {
        return;
    }
    if (amb == 31) {
        if (state_.musicId != "Peak1Amb") {
            playMusic(0, false, 1);
        }
        return;
    }
    if (amb == 32) {
        if (state_.musicId != "Peak2Amb") {
            playMusic(0, false, 2);
        }
        return;
    }
    sendEvent(amb);
}

// ---- the api ----

void GameAudio::ui(int ev) {
    auto it = uiSounds().find(ev);
    if (it != uiSounds().end()) {
        uiSound(it->second);
    }
}

void GameAudio::uiSound(int snd) {
    PlayOptions o;
    o.slot = slot::MAIN;
    o.sound = snd;
    o.bus = "UI";
    o.volume = 127;
    o.tag = "ui";
    sfx_->play(o);
}

void GameAudio::screen(const std::string& name) {
    if (name == "results" || name.rfind("ctm-results", 0) == 0) {
        state_.replay = true;
        speech_->replay = true;
        sfxGame_->G.replay = true;
        podium();
        return;
    }
    if (name == "game" || name == "loading" || name == "pause" || name == "audio" || name == "audio-playlist") {
        return;
    }
    auto it = feEvent().find(name);
    if (it == feEvent().end()) {
        return;
    }
    const int event = it->second;
    if (state_.musicId != "charsel" || !state_.music) {
        state_.raceMusic = false;
        state_.feEvent = event;
        tl("fe", {event});
        playSong("charsel", event);
        return;
    }
    if (state_.feEvent == event) {
        return;
    }
    state_.feEvent = event;
    tl("fe", {event});
    sendEvent(event);
}

void GameAudio::loadingStart(const std::string& courseCode, const std::string& character) {
    fadeOutMusic(1.0);
    state_.feEvent.reset();
    if (!courseCode.empty()) {
        prepare(courseCode, character, false);
    }
    loadingLoop(true);
    // the page's prep decodes every patch in slices after this (its warm runs on under the loading loop)
    if (!courseCode.empty()) {
        warmWorld();
    }
}

void GameAudio::warmWorld() {
    sfx_->warm({slot::MAIN, slot::BOARD, slot::SPUBOARD, slot::LAND, slot::CROWD, slot::GRUNT, slot::GRUNT_AI, slot::WORLD9, slot::WORLD8, slot::MOUNTAIN,
                slot::TRANSPORT});
}

void GameAudio::prepare(const std::string& courseCode, const std::string& character, bool warm) {
    engine_->setScaleMode("WORLD");
    if (!character.empty()) {
        state_.character = character;
    }
    loadWorldBanks(state_.character);
    if (!courseCode.empty()) {
        state_.courseCode = courseCode;
        world_->load(courseCode);
    }
    if (warm) {
        warmWorld();
    }
}

// riderMusic (2867E8: 158700 -> SetRadioMode, 158750 -> 28C2D0)
void GameAudio::applyRiderMusic() {
    const auto m = riderMusic ? riderMusic() : std::nullopt;
    if (!m) {
        return;
    }
    settings_.playlist.assign(35, false);
    for (size_t i = 0; i < m->playlist.size() && i < 35; i++) {
        settings_.playlist[i] = m->playlist[i];
    }
    if (m->radioMode != settings_.radioMode) {
        settings_.radioMode = m->radioMode;
        engine_->setRadioMode(settings_.radioMode);
    }
    std::string list;
    for (size_t i = 0; i < settings_.playlist.size(); i++) {
        if (settings_.playlist[i]) {
            list += (list.empty() ? "" : ",") + std::to_string(i);
        }
    }
    tl("rider-music", Json::array({settings_.radioMode, list}));
}

void GameAudio::worldLoaded(int courseIndex, bool singleEvent, const std::string& courseCode, const std::string& character, int) {
    state_.raceMusic = true;
    state_.course = courseIndex;
    state_.singleEvent = singleEvent;
    state_.goSeen = false;
    state_.resumeGo = 1;
    state_.inWorld = true;
    state_.replay = false;
    speech_->replay = false;
    sfxGame_->G.replay = false;
    state_.chartuneIndex = -1;
    state_.courseCode = courseCode;
    if (!character.empty()) {
        state_.character = character;
    }
    state_.trigger = {};
    state_.pendingDj.clear();
    state_.track = -1;
    state_.region = -1;
    relationsReset();
    // 2867E8: the human's radio mode (158700 -> 28BF78) and custom playlist (158750 -> 28C2D0)
    applyRiderMusic();
    prepare(courseCode, character);
    const auto f = courseFlags(courseIndex);
    sfxGame_->start(courseIndex, f.bigAir, f.halfPipe, f.backcountry, eventKind(courseIndex));
    loadingLoop(false);
    tl("worldload", {courseIndex});
    playWorldMusic(singleEvent ? 36 : 0);
    if (singleEvent) {
        speech_->venueIntro(courseIndex);
    }
}

void GameAudio::replayRewind() {
    const auto f = courseFlags(state_.course);
    sfxGame_->start(state_.course, f.bigAir, f.halfPipe, f.backcountry, eventKind(state_.course));
    state_.replay = true;
    speech_->replay = true;
    sfxGame_->G.replay = true;
    relationsReset();
}

void GameAudio::runStart(int courseIndex, bool singleEvent, const std::string& courseCode, const std::string& character) {
    if (state_.restarting) {
        state_.restarting = false;
        if (!character.empty()) {
            state_.character = character;
        }
        state_.replay = false;
        speech_->replay = false;
        sfxGame_->G.replay = false;
        relationsReset();
        const auto f = courseFlags(courseIndex);
        sfxGame_->start(courseIndex, f.bigAir, f.halfPipe, f.backcountry, eventKind(courseIndex));
        return;
    }
    if (!character.empty()) {
        state_.character = character;
    }
    if (!state_.raceMusic || !state_.music) {
        worldLoaded(courseIndex, singleEvent, courseCode, character);
        return;
    }
    state_.replay = false;
    speech_->replay = false;
    sfxGame_->G.replay = false;
    relationsReset();
    const auto f = courseFlags(courseIndex);
    sfxGame_->start(courseIndex, f.bigAir, f.halfPipe, f.backcountry, eventKind(courseIndex));
    if (state_.musicId == "chartune") {
        state_.songIndex = state_.chartuneIndex;
        playMusic(36);
    } else if (state_.goSeen) {
        sendEvent(36);
    }
}

// 29C7B0 GO
void GameAudio::go(std::optional<bool> medalRunOpt) {
    const Context c = contextFn ? contextFn() : Context{};
    const bool medalRun = medalRunOpt ? *medalRunOpt : (c.career && round() == 3 && !(c.mode >= 4));
    state_.countdownHold = false;
    if (state_.music) {
        state_.music->resume();
    }
    if (musicMode()) {
        sendEvent(state_.resumeGo ? 0 : 37);
    }
    state_.resumeGo = 1;
    state_.goSeen = true;
    const Json* s = songOf(state_.musicId);
    if (state_.singleEvent) {
        speech_->radioBigIntro(0, 0);
        speech_->flush();
    } else if (medalRun) {
        speech_->medalRunIntro(state_.course);
        speech_->flush();
    }
    const int r = round();
    const int k = eventKind(state_.course);
    if (state_.singleEvent || r == 1 || (r == 2 && (k == 0 || k == 1))) {
        state_.pendingDj["artist"] = 1;
        state_.pendingDj["sed"] = s ? s->value("SEDVALUE", -1) : -1;
    }
}

void GameAudio::pause(bool on) {
    auto notUi = [](const Voice& v) { return v.tag != "ui"; };
    if (!on && !state_.paused) {
        sfx_->pauseAll(false, notUi);
        return;
    }
    state_.paused = on;
    tl(on ? "pause" : "resume");
    if (on) {
        if (state_.music) {
            state_.music->pause();
        }
        sfxGame_->pauseStops();
    } else {
        if (state_.music) {
            state_.music->resume();
        }
        speech_->cancelQueued();
    }
    sfx_->pauseAll(on, notUi);
}

// 286EA0 finish
void GameAudio::finish(bool runsLeft, int place, bool timedOut, const std::string& winner) {
    if (musicMode()) {
        sendEvent(runsLeft ? 36 : 10);
    }
    if (runsLeft) {
        state_.goSeen = false;
        state_.resumeGo = 0;
    }
    state_.place = place;
    if (!winner.empty()) {
        state_.winner = winner;
    }
    if (timedOut) {
        return;
    }
    const int me = charId(state_.character);
    if (state_.course < 5 || eventKind(state_.course) == 0) {
        speech_->paFinishLine(me, place);
    } else if (round() < 3) {
        speech_->riderPosition(me, place);
    } else {
        speech_->paFinishLine(me, place);
    }
}

// 28CDF8 podium: the final round, the human 1st-3rd -> chartune with the winner's event
void GameAudio::podium(std::optional<int> placeOpt, const std::string& winnerOpt) {
    const int place = placeOpt ? *placeOpt : state_.place;
    const std::string winner = !winnerOpt.empty() ? winnerOpt : (!state_.winner.empty() ? state_.winner : state_.character);
    if (!musicMode() || place > 2 || round() != 3) {
        return;
    }
    if (state_.musicId == "chartune" && state_.music && !state_.music->finished()) {
        return;
    }
    const int id = charId(winner);
    if (id < 0) {
        return;
    }
    state_.chartuneIndex = state_.songIndex;
    playSong("chartune", kChartune[id], false, 127);
}

bool GameAudio::restartRun(bool fromResults) {
    if (!state_.inWorld) {
        return false;
    }
    tl("restart", {fromResults ? 1 : 0});
    if (state_.paused) {
        pause(false);
    }
    sfxGame_->stop();
    engine_->release(0.5);
    state_.restarting = true;
    state_.goSeen = false;
    state_.countdownHold = false;
    return true;
}

// 286A80 leaving the world
void GameAudio::leaveWorld() {
    if (state_.restarting) {
        return;
    }
    files_.dropBytes("music/");
    tl("leave");
    if (state_.music) {
        tl("stop");
    }
    state_.lodge = false;
    state_.paused = false;
    state_.raceMusic = false;
    state_.inWorld = false;
    dirTimers_.clear();
    stopMusic();
    state_.musicId.clear();
    speech_->stop();
    engine_->setMix(0);
    engine_->setScaleMode("FE");
    sfxGame_->stop();
    world_->unload();
    crowd_->reset();
    Sfx::StopFilter f;
    f.fade = 0.25;
    sfx_->stopAll(f);
    for (int s : {slot::BOARD, slot::SPUBOARD, slot::MOUNTAIN, slot::CROWD, slot::TRANSPORT, slot::LAND, slot::GRUNT, slot::GRUNT_AI, slot::WORLD8, slot::WORLD9, slot::DYNAMIC}) {
        sfx_->unloadBank(s);
    }
    sfx_->loadBank(slot::MAIN, "SSX3Menu");
}

// NIS kind-7 music codes
void GameAudio::cutsceneMusic(int code) {
    tl("code", {code});
    if (code == 19 || code == 20) {
        return;
    }
    if (code >= 21 && code <= 25) {
        if (!musicMode()) {
            return;
        }
        pickSong(36);
        fadeOutMusic(1);
        post("request", 3000, request(3, true));
        return;
    }
    sendEvent(code);
}

void GameAudio::listener(const std::array<double, 16>* cameraInverse, const std::array<double, 3>& cameraPosition, const std::array<double, 3>& origin) {
    pumpTimers();
    timers_.run();
    ctx_.dispatch();
    // 285BF8 every frame: the speech dispatch (60 Hz) and the voice callbacks / 3D update
    const double now = nowMs();
    if (frameAt_ < 0) {
        frameAt_ = now;
    }
    int n = std::min(int(std::floor((now - frameAt_) / (1000.0 / 60))), 8);
    if (n > 0) {
        frameAt_ += n * (1000.0 / 60);
        if (now - frameAt_ > 1000) {
            frameAt_ = now;
        }
    }
    const bool idle = nis_ && state_.inWorld && now - gameTickAt_ > 100;
    while (n-- > 0) {
        speech_->update();
        sfx_->tick(1.0 / 60);
        if (idle && world_->loaded()) {
            world_->update(state_.listenerCm ? std::optional<Vec>(*state_.listenerCm) : std::nullopt);
            crowd_->update([] { return 3; });
        }
    }
    if (!cameraInverse) {
        return;
    }
    sfx_->setListener(cameraInverse, origin);
    state_.listenerCm = std::array<double, 3>{(cameraPosition[0] + origin[0]) * 100, -(cameraPosition[2] + origin[2]) * 100, (cameraPosition[1] + origin[1]) * 100};
}

const Json* GameAudio::worldDoc() {
    return world_->doc();
}

void GameAudio::gameTick(const TickState& s) {
    gameTickAt_ = nowMs();
    if (!s.character.empty()) {
        state_.character = s.character;
    }
    if (s.place) {
        state_.place = *s.place;
    }
    if (!s.winner.empty()) {
        state_.winner = s.winner;
    }
    sfxGame_->G.rollingGo = true;
    sfxGame_->G.instancePosition = s.instancePosition;
    std::vector<TickRider> list;
    for (const auto& r : s.riders) {
        TickRider t;
        t.key = r.key;
        t.character = r.human ? state_.character : r.character;
        t.slot = r.slot;
        t.human = r.human;
        t.finished = r.finished;
        t.place = r.human ? state_.place : 0;
        t.in = r.in;
        list.push_back(t);
    }
    // hookPairs: the reactions reach the director from the first tick after it saw the race (the page installs its
    // onPairAudio here, after that tick's drain)
    if (s.ai && pairsHooked_) {
        for (const auto& e : s.pairEvents) {
            if (e.audio >= 0) {
                relPair_[{e.other, e.target}] = e.audio;
            }
        }
    }
    if (s.ai) {
        pairsHooked_ = true;
        if (!relTick_ && s.relations) {
            relTick_ = s.relations();
        }
    }
    const auto hud = s.hudSlots ? s.hudSlots() : std::nullopt;
    {
        std::lock_guard<std::recursive_mutex> lock(ctx_.mutex());
        sfxGame_->tick(list, s.raceInfo, s.running, state_.paused, s.pending, hud, s.finished, s.timedOut ? s.timedOut() : false);
    }
    // the next tick's ranking sees this tick's end state
    relPair_.clear();
    relTick_ = s.ai && s.relations ? s.relations() : std::nullopt;
    pumpTimers();
    // the painters (2898A8): region = the track of the human's last contacted patch
    const int contact = s.contactTrack ? s.contactTrack() : -1;
    if (contact >= 0) {
        state_.track = contact & 0xff;
        world_->focusTrack(state_.track);
    }
    const Json* doc = world_->loaded() ? worldDoc() : nullptr;
    if (doc && state_.track >= 0) {
        const auto* h = sfxGame_->humanRider();
        std::string region;
        for (auto it = (*doc)["locations"].begin(); it != (*doc)["locations"].end(); ++it) {
            if (it.value()["track"].get<int>() == state_.track) {
                region = it.key();
                break;
            }
        }
        if (!region.empty() && h) {
            const PainterQuery q = queryAudioPainters(*doc, double(float(h->t[12])), double(float(h->t[13])), region);
            engine_->setMix(q.mix < 0 ? 0 : q.mix);
            if (settings_.radioMode != 2) {
                musicTrigger(q.triggerA, q.triggerB);
            } else {
                ambiencePainter(q.ambience);
            }
        }
    }
    world_->update(state_.listenerCm ? std::optional<Vec>(*state_.listenerCm) : std::nullopt);
    // 0x29DEF0 rumble (docs/avalanche.md "Audio"): the human core's avalanches, listener = the human rider +0x110
    if (s.avalanche) {
        const auto* me = sfxGame_->humanRider();
        std::optional<Vec> L;
        if (me) {
            L = Vec{me->t[12], me->t[13], me->t[14]};
        }
        std::lock_guard<std::recursive_mutex> lock(ctx_.mutex());
        world_->avalanche(s.avalanche->loop, s.avalanche->tumblers, L, s.avalanche->loopEvents);
    }
    // 28F000: hub songs stay at 127, the others ramp
    if (!state_.paused && state_.music && state_.raceMusic && musicMode()) {
        if (isPeakSong(state_.musicId)) {
            state_.music->setIntensity(127);
        } else {
            state_.intensityFrames++;
            state_.music->setIntensity(pf::raceIntensity(state_.intensityFrames));
        }
    }
}

}  // namespace audio

}  // namespace ssx_native
