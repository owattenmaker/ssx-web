#pragma once
// Crowd audio (web/audio-crowd.js, 1:1; docs/audio-logic.md 5.4): the CROWD.INF reactions (EA MIDx .eam sequences on Crowd.bnk,
// slot 5) and the crowd loops fed by world emitters, the anticipation levels and the ATTACK / DECAY / MAXBEND pitch swell.
#include "audio_sfx.hpp"
#include <map>
#include <string>
#include <vector>

namespace ssx_native {

namespace audio {

struct MidxEvent {
    int tick = 0;
    int channel = 0;
    std::string type;
    int note = 0;
    int velocity = 0;
    int controller = 0;
    int value = 0;
    int program = 0;
};

// .eam (MIDx): [wait][status][d1]([d2]); 1 tick = 10 ms
inline std::vector<MidxEvent> parseMidx(const std::vector<uint8_t>& b) {
    if (b.size() < 4 || b[0] != 0x4d || b[1] != 0x49 || b[2] != 0x44 || b[3] != 0x78) {
        throw std::runtime_error("not a MIDx file");
    }
    std::vector<MidxEvent> events;
    size_t p = 4;
    int tick = 0;
    while (p + 3 <= b.size()) {
        const int wait = b[p];
        const int status = b[p + 1];
        const int d1 = b[p + 2];
        p += 3;
        const int hi = status & 0xf0;
        MidxEvent e;
        e.tick = tick;
        e.channel = status & 0x0f;
        if (hi == 0x80 || hi == 0x90 || hi == 0xb0 || hi == 0xe0) {
            const int d2 = p < b.size() ? b[p] : 0;
            p++;
            if (hi == 0x80) {
                e.type = "noteOff";
                e.note = d1;
            } else if (hi == 0x90) {
                e.type = "noteOn";
                e.note = d1;
                e.velocity = d2;
            } else if (hi == 0xb0) {
                e.type = "control";
                e.controller = d1;
                e.value = d2;
            } else {
                e.type = "pitchBend";
                e.value = ((d2 << 7) | d1) >> 7;
            }
        } else if (hi == 0xc0) {
            e.type = "program";
            e.program = d1;
        } else if (hi == 0xf0) {
            if (d1 == 0x2f) {
                e.type = "end";
                events.push_back(e);
                break;
            }
            e.type = "nop";
        } else {
            e.type = "unknown";
        }
        events.push_back(e);
        tick += wait;
    }
    return events;
}

struct CrowdLevel {
    std::vector<std::string> trick, fall, anticipate;
    double attack = 1, decay = 4, maxBend = 32;
};

// CROWD.INF [GLOBAL]: TRICK1-3, FALL1-3, ANTICIPATE1-3, ATTACK / DECAY / MAXBEND 1-3
inline std::vector<CrowdLevel> crowdConfig(const Json& inf) {
    Json g = Json::object();
    if (inf.is_array()) {
        for (const auto& s : inf) {
            if (s.value("name", "") == "GLOBAL") {
                g = s;
            }
        }
    }
    auto list = [&](const std::string& k) {
        std::vector<std::string> out;
        if (!g.contains(k)) {
            return out;
        }
        Json v = g[k];
        if (!v.is_array()) {
            v = Json::array({v});
        }
        for (const auto& x : v) {
            if (x.is_string() && !x.get<std::string>().empty()) {
                const std::string s = x.get<std::string>();
                const size_t slash = s.find_last_of('\\');
                out.push_back(slash == std::string::npos ? s : s.substr(slash + 1));
            }
        }
        return out;
    };
    std::vector<CrowdLevel> out;
    for (int d = 1; d <= 3; ++d) {
        const std::string n = std::to_string(d);
        CrowdLevel l;
        l.trick = list("TRICK" + n);
        l.fall = list("FALL" + n);
        l.anticipate = list("ANTICIPATE" + n);
        l.attack = g.value("ATTACK" + n, 1.0);
        l.decay = g.value("DECAY" + n, 4.0);
        l.maxBend = g.value("MAXBEND" + n, 32.0);
        out.push_back(l);
    }
    return out;
}

class Crowd {
public:
    Crowd(Sfx& sfx, Files& files, std::vector<CrowdLevel> config, Random& random, int bank = 5) : sfx_(sfx), files_(files), config_(std::move(config)), random_(random), bank_(bank) {}

    // 2A5D08 / 2A5CD8 / 2A62F0: emitter registration
    void emitter(const std::string& key, int patch, const std::array<double, 3>& position, double volume) {
        Emitter* e = emitters_.get(key);
        if (!e) {
            e = &emitters_.set(key, Emitter{patch, position, 0});
            Group* g = groups_.get(patch);
            if (!g) {
                g = &groups_.set(patch, Group{patch, nullptr, 0, position, 0});
            }
            g->refs++;
        }
        e->position = position;
        e->weight = std::min(std::max(std::trunc(volume * 127), 0.0), 127.0);
    }
    void removeEmitter(const std::string& key) {
        const Emitter* e = emitters_.get(key);
        if (!e) {
            return;
        }
        const int patch = e->patch;
        emitters_.erase(key);
        Group* g = groups_.get(patch);
        if (g && --g->refs <= 0) {
            if (g->voice) {
                g->voice->stop(0.25);
            }
            groups_.erase(patch);
        }
        if (emitters_.empty()) {
            const bool was = anticipation_ == 2;
            stopReaction();
            if (was) {
                anticipation_ = 1;
            }
        }
    }
    void trick(int L) {
        react(true, L);
    }
    void fall(int L) {
        react(false, L);
    }
    void arm() {
        if (anticipation_ == 0) {
            anticipation_ = 1;
        }
    }
    void cancel() {
        if (anticipation_ == 2) {
            stopReaction();
        }
        anticipation_ = 0;
    }
    // per frame (2A4E88); level(): 290FD0 (0..2, 3 = none)
    void update(const std::function<int()>& anticipationLevel) {
        if (emitters_.empty()) {
            return;
        }
        std::vector<const Emitter*> all;
        for (const auto& e : emitters_) {
            all.push_back(&e.second);
        }
        const Blend a = blend(all);
        loudness_ = a.loud;
        centroid_ = a.centroid;
        for (auto& [patch, g] : groups_) {
            Blend b = a;
            if (groups_.size() != 1) {
                std::vector<const Emitter*> list;
                for (const auto& e : emitters_) {
                    if (e.second.patch == patch) {
                        list.push_back(&e.second);
                    }
                }
                b = blend(list);
            }
            g.position = b.centroid;
            g.weight = b.loud;
            if (!g.voice || !g.voice->playing()) {
                PlayOptions o;
                o.slot = bank_;
                o.sound = patch;
                o.bus = "UI";
                o.volume = b.loud;
                o.position = std::make_shared<std::array<double, 3>>(g.position);
                o.callback = [this](Voice& v, double dt) { bendCallback(v, dt); };
                o.tag = "crowd-loop";
                g.voice = sfx_.play(o);
            } else {
                g.voice->setVolume(b.loud);
            }
        }
        if (anticipation_ != 0) {
            const int L = anticipationLevel();
            if (anticipation_ == 1 && L != 3) {
                stopReaction();
                const auto& list = config_[size_t(L)].anticipate;
                if (!list.empty()) {
                    reaction_ = playSequence(pick(list));
                    anticipation_ = 2;
                }
            } else if (anticipation_ == 2) {
                if (reactionPlaying()) {
                    if (reaction_) {
                        for (auto& v : reaction_->voices) {
                            v->setVolume(loudness_);
                        }
                    }
                } else {
                    stopReaction();
                }
            }
        }
    }
    void reset() {
        stopReaction();
        for (auto& [k, g] : groups_) {
            if (g.voice) {
                g.voice->stop(0.25);
            }
        }
        groups_.clear();
        emitters_.clear();
        T_ = 0;
    }

private:
    struct Emitter {
        int patch;
        std::array<double, 3> position;
        double weight;
    };
    struct Group {
        int patch;
        std::shared_ptr<Voice> voice;
        int refs;
        std::array<double, 3> position;
        double weight;
    };
    struct Reaction {
        std::vector<std::shared_ptr<Voice>> voices;
        bool cancelled = false;
        double started = 0;
        double lengthMs = -1;
    };
    struct Blend {
        double loud = 0;
        std::array<double, 3> centroid{};
    };
    // loudness: max weight + (sum of the others >> 2); the weight-averaged centroid
    static Blend blend(const std::vector<const Emitter*>& list) {
        double max = 0;
        int64_t sum = 0;
        double wsum = 0;
        std::array<double, 3> c{};
        for (const Emitter* e : list) {
            max = std::max(max, e->weight);
            sum += int64_t(e->weight);
            wsum += e->weight;
            for (int k = 0; k < 3; ++k) {
                c[size_t(k)] += e->position[size_t(k)] * e->weight;
            }
        }
        Blend b;
        const double loud = max + double((sum - int64_t(max)) >> 2);
        b.loud = std::min(loud, 127.0);
        if (wsum != 0) {
            for (int k = 0; k < 3; ++k) {
                b.centroid[size_t(k)] = c[size_t(k)] / wsum;
            }
        } else if (!list.empty()) {
            b.centroid = list[0]->position;
        }
        return b;
    }
    const std::string& pick(const std::vector<std::string>& list) {
        return list[size_t(int64_t(std::floor(random_() * double(list.size()))) % int64_t(list.size()))];
    }
    void stopReaction() {
        T_ = std::min(T_, 0.25);
        anticipation_ = 0;
        auto r = reaction_;
        reaction_ = nullptr;
        if (r) {
            r->cancelled = true;
            for (auto& v : r->voices) {
                v->stop(0.25);
            }
        }
    }
    std::shared_ptr<Reaction> playSequence(const std::string& file) {
        auto r = std::make_shared<Reaction>();
        r->started = nowMs();
        const double volume = loudness_;
        const auto position = centroid_;
        std::vector<MidxEvent> events;
        auto it = sequences_.find(file);
        if (it == sequences_.end()) {
            try {
                it = sequences_.emplace(file, parseMidx(*files_.bytes("banks/" + file))).first;
            } catch (const std::exception& e) {
                std::fprintf(stderr, "Crowd sequence %s unavailable: %s\n", file.c_str(), e.what());
                return r;
            }
        }
        struct Chan {
            int program = 0;
            int volume = 127;
        };
        Chan chan[16];
        int last = 0;
        for (const auto& e : it->second) {
            Chan& c = chan[e.channel];
            if (e.type == "program") {
                c.program = e.program;
            } else if (e.type == "control" && e.controller == 7) {
                c.volume = e.value & 0x7f;
            } else if (e.type == "noteOn") {
                PlayOptions o;
                o.slot = bank_;
                o.sound = c.program;
                o.bus = "UI";
                o.volume = double((c.volume * int(volume)) >> 7);
                o.position = std::make_shared<std::array<double, 3>>(position);
                o.posStatic = true;
                o.note = e.note;
                o.velocity = e.velocity;
                o.delayMs = e.tick * 10.0;
                if (auto v = sfx_.play(o)) {
                    r->voices.push_back(v);
                }
            }
            last = std::max(last, e.tick);
        }
        r->lengthMs = last * 10.0;
        return r;
    }
    bool reactionPlaying() const {
        if (!reaction_) {
            return false;
        }
        if (reaction_->lengthMs < 0) {
            return true;
        }
        return nowMs() - reaction_->started < reaction_->lengthMs;
    }
    void react(bool trick, int L) {
        stopReaction();
        if (emitters_.empty()) {
            return;
        }
        const CrowdLevel& rec = config_[size_t(L)];
        level_ = L;
        T_ = rec.attack + rec.decay;
        const auto& list = trick ? rec.trick : rec.fall;
        if (list.empty()) {
            return;
        }
        reaction_ = playSequence(pick(list));
    }
    // 2A68B0: the pitch swell after a trick / fall (T shared, decremented per voice)
    void bendCallback(Voice& voice, double dt) {
        if (T_ <= 0) {
            voice.setWheel(64);
            return;
        }
        T_ -= dt;
        const CrowdLevel& rec = config_[size_t(level_)];
        double bend = 0;
        if (T_ > rec.decay) {
            bend = rec.maxBend * (rec.attack - (T_ - rec.decay)) / rec.attack;
        } else if (T_ > 0) {
            bend = rec.maxBend * T_ / rec.decay;
        }
        voice.setWheel(0x40 + std::trunc(bend));
    }

    Sfx& sfx_;
    Files& files_;
    std::vector<CrowdLevel> config_;
    Random& random_;
    int bank_;
    std::map<std::string, std::vector<MidxEvent>> sequences_;
    OrderedMap<std::string, Emitter> emitters_;
    OrderedMap<int, Group> groups_;
    double loudness_ = 0;
    std::array<double, 3> centroid_{};
    std::shared_ptr<Reaction> reaction_;
    int anticipation_ = 0;
    int level_ = 0;
    double T_ = 0;
};

}  // namespace audio

}  // namespace ssx_native
