#pragma once
// World sounds of the resident course locations (web/audio-world.js, 1:1; docs/audio-logic.md 5.5): the instance emitters per
// listener (2B7908 shapes and falloffs), contacts (296088), the location ambience (29D290), the named bank of slot 3 (29B818), the
// stage-script sounds (2974A0 / 297950 / 297EB8), the avalanche rumble (0x29DEF0) and thunder (291438).
#include "audio_crowd.hpp"
#include "audio_sfx.hpp"
#include <cmath>
#include <functional>
#include <set>

namespace ssx_native {

namespace audio {

// 2B82B8 falloff shapes
inline double falloff(int shape, double t) {
    switch (shape) {
    case 0:
        return 1 - t * t;
    case 1:
        return 1 - t / (1.5 - 0.5 * t);
    case 2:
        return 1 - t;
    case 3: {
        const double u = 1 - t;
        return u / (1.5 - 0.5 * u);
    }
    case 4:
        return (1 - t) * (1 - t);
    case 5:
        return t <= 0.7 ? 1 : (1 - t) * 3.3333;
    default:
        return 0;
    }
}

using Vec = std::array<double, 3>;
inline Vec vsub(const Vec& a, const Vec& b) {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}
inline double vdot(const Vec& a, const Vec& b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
inline double vlen(const Vec& a) {
    return std::sqrt(vdot(a, a));
}
inline Vec vcross(const Vec& a, const Vec& b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
inline Vec vnorm(const Vec& a) {
    double l = vlen(a);
    if (l == 0) {
        l = 1;
    }
    return {a[0] / l, a[1] / l, a[2] / l};
}
inline Vec vec(const Json& j) {
    return {j[0].get<double>(), j[1].get<double>(), j[2].get<double>()};
}

// 2B7908: an emitter's volume (0..1) at listener L, 0 outside
inline double emitterVolume(const Json& e, const Vec& L) {
    const Vec d = vsub(L, vec(e["pos"]));
    const std::string shape = e.value("shape", "");
    if (shape == "sphere") {
        const double r = vlen(d);
        const double radius = e["radius"].get<double>();
        return r < radius ? std::max(falloff(e.value("falloff", 0), r / radius), 0.0) : 0;
    }
    if (shape == "ellipsoid") {
        const Vec D = vnorm(vec(e["axis"]));
        const Vec S = vnorm(vcross({0, 0, 1}, D));
        const Vec U = vcross(D, S);
        double t = 0;
        const Vec ex = vec(e["extents"]);
        if (ex[0] != 0) {
            t += std::pow(vdot(d, D) / ex[0], 2);
        }
        if (ex[1] != 0) {
            t += std::pow(vdot(d, S) / ex[1], 2);
        }
        if (ex[2] != 0) {
            t += std::pow(vdot(d, U) / ex[2], 2);
        }
        return t < 1 ? std::max(falloff(e.value("falloff", 0), t), 0.0) : 0;
    }
    if (shape == "cone") {
        const double r = vlen(d);
        const double radius = e["radius"].get<double>();
        if (!(r < radius)) {
            return 0;
        }
        const Vec n = r != 0 ? Vec{d[0] / r, d[1] / r, d[2] / r} : Vec{0, 0, 0};
        const double c = vdot(n, vec(e["dir"]));
        const double cosHalf = e["cosHalfAngle"].get<double>();
        if (!(c > cosHalf)) {
            return 0;
        }
        const double ang = (M_PI / 2 - std::asin(c)) / (M_PI / 2 - std::asin(cosHalf));
        return std::max(falloff(e.value("falloff", 0), r / radius) * falloff(e.value("angleFalloff", 0), ang), 0.0);
    }
    if (shape == "zone") {
        const double r = vlen(d);
        return r < e["radius"].get<double>() ? (r <= 0.9 ? 1 : (1 - r) * 10) : 0;
    }
    return 0;
}

struct ScriptRoute {
    int slot = -1;
    int sound = 0;
};
// 2974A0 id routing
inline ScriptRoute scriptSoundRoute(int id) {
    if (id >= 1 && id < 100) {
        return {slot::MOUNTAIN, id};
    }
    if (id >= 100 && id < 150) {
        return {slot::WORLD8, id - 100};
    }
    if (id >= 150 && id < 200) {
        return {slot::WORLD9, id - 150};
    }
    if (id >= 200) {
        return {slot::TRANSPORT, id - 200};
    }
    return {};
}

// web/avalanche-state.js avalancheRumble: {volume 0..127, centroid} for the listener L (float32 like the EE)
inline std::pair<int, std::optional<Vec>> avalancheRumble(const std::vector<std::array<double, 4>>& tumblers, const Vec& L) {
    if (tumblers.empty()) {
        return {0, std::nullopt};
    }
    auto F = [](double v) { return double(float(v)); };
    double sx = 0, sy = 0, sz = 0, ss = 0, dmin = 999999;
    for (const auto& t : tumblers) {
        const double dx = F(L[0] - t[0]);
        const double dy = F(L[1] - t[1]);
        const double dz = F(L[2] - t[2]);
        const double d = F(std::sqrt(F(F(F(dx * dx) + F(dy * dy)) + F(dz * dz))));
        sx = F(sx + t[0]);
        sy = F(sy + t[1]);
        sz = F(sz + t[2]);
        ss = F(ss + t[3]);
        if (d < dmin) {
            dmin = d;
        }
    }
    const double n = double(tumblers.size());
    const double scale = F(ss / n);
    int v = int(std::trunc(F(F(F(F(100 - F(dmin * F(0.01))) * F(1.27)) * scale) * F(8.466667175292969))));
    if (v >= 128) {
        v = 127;
    }
    if (!(v > -1)) {
        v = 0;
    }
    return {v, Vec{F(sx / n), F(sy / n), F(sz / n)}};
}

class WorldAudio {
public:
    WorldAudio(Sfx& sfx, Crowd& crowd, Files& files, std::function<void(int)> onSpecial) : sfx_(sfx), crowd_(crowd), files_(files), onSpecial_(std::move(onSpecial)) {}

    void load(const std::string& event) {
        if (event == loadedEvent_) {
            return;
        }
        loadedEvent_ = event;
        doc_ = nullptr;
        stopAll(0.25);
        locationByTrack_.clear();
        track_ = -1;
        bankOwner_.clear();
        ambienceWanted_ = false;
        try {
            doc_ = files_.json("world/" + event + ".json");
        } catch (const std::exception& e) {
            std::fprintf(stderr, "World audio %s unavailable: %s\n", event.c_str(), e.what());
            return;
        }
        for (auto it = (*doc_)["locations"].begin(); it != (*doc_)["locations"].end(); ++it) {
            locationByTrack_[it.value()["track"].get<int>()] = {it.key(), &it.value()};
            if (isResident(it.value())) {
                loadBanks(it.key(), it.value());
            }
        }
    }
    void unload() {
        loadedEvent_.clear();
        doc_ = nullptr;
        resident_.reset();
        bankOwner_.clear();
        stopAll(0);
    }
    bool loaded() const {
        return doc_ != nullptr;
    }
    const Json* doc() const {
        return doc_.get();
    }
    void setResident(const std::optional<std::set<int>>& tracks) {
        const auto before = resident_;
        resident_ = tracks;
        if (!doc_) {
            return;
        }
        for (auto it = (*doc_)["locations"].begin(); it != (*doc_)["locations"].end(); ++it) {
            const int t = it.value()["track"].get<int>();
            if ((!tracks || tracks->count(t)) && before && !before->count(t)) {
                loadBanks(it.key(), it.value());
            }
        }
    }
    // per frame at listener L (source cm)
    void update(const std::optional<Vec>& L) {
        if (!doc_ || !L) {
            return;
        }
        std::set<std::string> touched;
        for (auto it = (*doc_)["locations"].begin(); it != (*doc_)["locations"].end(); ++it) {
            const Json& loc = it.value();
            if (!isResident(loc)) {
                continue;
            }
            const auto& emitters = loc["emitters"];
            for (size_t index = 0; index < emitters.size(); ++index) {
                const Json& e = emitters[index];
                const double v = emitterVolume(e, *L);
                if (!(v > 0)) {
                    continue;
                }
                const std::string key = jsKey(e["resource"]) + ":" + std::to_string(index);
                const std::string adl = jsKey(e["adl"]);
                const auto& watrig = (*doc_)["watrig"];
                if (!watrig.contains(adl) || watrig[adl].is_null()) {
                    continue;
                }
                const Json& w = watrig[adl];
                touched.insert(key);
                Instance* inst = instances_.get(key);
                if (!inst) {
                    inst = &instances_.set(key, Instance{});
                }
                const int type = w.value("type", 0);
                if (type == 1 && w.value("bank", -1) == slot::CROWD) {
                    inst->crowd = true;
                    crowd_.emitter(key, w["snd"].get<int>(), vec(e["pos"]), v);
                    continue;
                }
                if (type == 4) {
                    if (!inst->special) {
                        inst->special = true;
                        if (onSpecial_) {
                            onSpecial_(e["adl"].get<int>());
                        }
                    }
                    continue;
                }
                const double vol = std::min(std::max(std::trunc(v * 127), 0.0), 127.0);
                int slotNo = w.value("bank", 0);
                int sound = w.value("snd", 0);
                if (type == 3) {
                    // 2B4C38 named-bank gate: the first claim wins while it is in range
                    if (!namedOwner_.empty() && namedOwner_ != key && instances_.has(namedOwner_)) {
                        continue;
                    }
                    const bool ready = namedBank(w["bank"].get<std::string>());
                    namedOwner_ = key;
                    if (!ready) {
                        continue;
                    }
                    slotNo = slot::DYNAMIC;
                    sound = 0;
                }
                if (inst->voice && inst->voice->playing()) {
                    inst->voice->setVolume(vol);
                } else {
                    PlayOptions o;
                    o.slot = slotNo;
                    o.sound = sound;
                    o.bus = "UI";
                    o.volume = vol;
                    o.position = std::make_shared<Vec>(vec(e["pos"]));
                    o.posStatic = true;
                    o.tag = "world";
                    inst->voice = sfx_.play(o);
                }
            }
        }
        std::vector<std::string> gone;
        for (auto& [key, inst] : instances_) {
            if (!touched.count(key)) {
                gone.push_back(key);
            }
        }
        for (const auto& key : gone) {
            Instance* inst = instances_.get(key);
            if (inst->crowd) {
                crowd_.removeEmitter(key);
            } else if (inst->voice) {
                inst->voice->stop(0.25);
            }
            if (namedOwner_ == key) {
                namedOwner_.clear();
            }
            instances_.erase(key);
        }
        if (ambience_ && !ambience_->playing() && ambience_->keep) {
            ambienceStart();
        }
    }
    // 29D290: the focus rider's contact track
    void focusTrack(int t) {
        if (t < 0 || t == track_ || !doc_) {
            return;
        }
        track_ = t;
        auto it = locationByTrack_.find(t);
        const int id = it != locationByTrack_.end() ? it->second.loc->value("locationId", 99) : 99;
        if (id < 22) {
            ambienceWanted_ = true;
            ambienceStart();
        } else {
            ambienceWanted_ = false;
            if (ambience_) {
                ambience_->stop(5.03);
                ambience_->keep = false;
            }
            ambience_ = nullptr;
            Sfx::StopFilter f8;
            f8.slot = slot::WORLD8;
            f8.fade = 5.03;
            sfx_.stopAll(f8);
            Sfx::StopFilter f9;
            f9.slot = slot::WORLD9;
            f9.fade = 5.03;
            sfx_.stopAll(f9);
        }
    }
    void ambienceStop(double fade = 5.03) {
        ambienceWanted_ = false;
        if (ambience_) {
            ambience_->stop(fade);
            ambience_->keep = false;
        }
        ambience_ = nullptr;
    }
    void ambienceStart() {
        Sfx::Bank* b = sfx_.bankOf(slot::WORLD9);
        if (!b || !b->ready()) {
            return;
        }
        PlayOptions o;
        o.slot = slot::WORLD9;
        o.sound = 0;
        o.bus = "UI";
        o.volume = 127;
        o.tag = "ambience";
        if (auto v = sfx_.play(o)) {
            v->keep = true;
            ambience_ = v;
        }
    }
    // 296088: an instance contact
    void contact(const std::string& resource, int node, double speed, const Vec& position, bool inRange = true) {
        if (!doc_ || !inRange) {
            return;
        }
        const Json* list = nullptr;
        for (auto it = (*doc_)["locations"].begin(); it != (*doc_)["locations"].end(); ++it) {
            if (isResident(it.value()) && it.value()["contacts"].contains(resource)) {
                list = &it.value()["contacts"][resource];
                break;
            }
        }
        if (!list || list->empty()) {
            return;
        }
        const int id = (*list)[size_t(std::min(std::max(node, 0), int(list->size()) - 1))].get<int>();
        const std::string ids = std::to_string(id);
        const auto& watrig = (*doc_)["watrig"];
        if (!id || !watrig.contains(ids) || watrig[ids].is_null() || watrig[ids].value("type", 0) != 1) {
            return;
        }
        const Json& w = watrig[ids];
        const std::string key = resource + ":" + std::to_string(node);
        auto prev = contactVoices_.find(key);
        if (prev != contactVoices_.end() && prev->second->playing()) {
            return;
        }
        PlayOptions o;
        o.slot = w["bank"].get<int>();
        o.sound = w["snd"].get<int>();
        o.bus = "COLLISION";
        o.volume = std::min(std::max(std::trunc(curve(curves::contact_445898, speed)), 0.0), 127.0);
        o.position = std::make_shared<Vec>(position);
        o.posStatic = true;
        o.tag = "contact";
        if (auto v = sfx_.play(o)) {
            if (contactVoices_.size() >= 64) {
                sweep(contactVoices_);
            }
            contactVoices_[key] = v;
        }
    }
    // stage-script builtins: kind 0 play (2974A0), 1 loop (297950), 2 stop (297EB8)
    void script(int kind, int id, uint32_t resource, const Vec& position, int location = 0) {
        const ScriptRoute r = scriptSoundRoute(id);
        if (r.slot < 0) {
            return;
        }
        if (kind == 0) {
            if (id == 102 && location == 0) {
                return;
            }
            auto s = scriptVoices_.find(resource);
            if (s != scriptVoices_.end() && s->second->playing()) {
                return;
            }
        }
        if (kind == 2) {
            auto l = scriptLoops_.find(resource);
            if (l != scriptLoops_.end() && l->second.first == id) {
                if (l->second.second) {
                    l->second.second->stop(0);
                }
                scriptLoops_.erase(l);
            }
            return;
        }
        if (kind == 1) {
            auto l = scriptLoops_.find(resource);
            if (l != scriptLoops_.end()) {
                if (l->second.second) {
                    l->second.second->stop(0);
                }
                scriptLoops_.erase(l);
            }
        }
        PlayOptions o;
        o.slot = r.slot;
        o.sound = r.sound;
        o.bus = "UI";
        o.volume = 127;
        o.position = std::make_shared<Vec>(position);
        o.posStatic = true;
        o.vanish = 300;
        o.tag = "script";
        auto v = sfx_.play(o);
        if (kind == 1 && v) {
            scriptLoops_[resource] = {id, v};
        } else if (v) {
            if (scriptVoices_.size() >= 64) {
                sweep(scriptVoices_);
            }
            scriptVoices_[resource] = v;
        }
    }
    // 0x29DEF0 / 0x29E4A0: the avalanche rumble (once a tick)
    void avalanche(bool on, const std::vector<std::array<double, 4>>& tumblers, const std::optional<Vec>& L, const std::vector<double>& events) {
        bool drop = false;
        for (double v : events) {
            drop = drop || v <= 0;
        }
        if (rumble_ && drop) {
            if (rumble_->voice) {
                rumble_->voice->stop(2.0);
            }
            rumble_.reset();
        }
        if (!on) {
            if (rumble_) {
                if (rumble_->voice) {
                    rumble_->voice->stop(2.0);
                }
                rumble_.reset();
            }
            return;
        }
        if (!L) {
            return;
        }
        const auto [volume, centroid] = avalancheRumble(tumblers, *L);
        if (!rumble_) {
            Rumble r;
            r.position = std::make_shared<Vec>(centroid ? *centroid : *L);
            PlayOptions o;
            o.slot = slot::WORLD8;
            o.sound = 2;
            o.bus = "UI";
            o.volume = volume;
            o.position = r.position;
            o.posStatic = false;
            o.vanish = 100;
            o.tag = "avalanche";
            r.voice = sfx_.play(o);
            r.volume = volume;
            rumble_ = r;
        } else {
            if (centroid) {
                *rumble_->position = *centroid;
            }
            if (rumble_->voice) {
                rumble_->voice->setVolume(volume);
            }
            rumble_->volume = volume;
        }
    }
    // 291438: thunder at distanceCm
    void thunder(double distanceCm) {
        auto it = locationByTrack_.find(track_);
        if (it == locationByTrack_.end() || !it->second.loc->value("thunder", false)) {
            return;
        }
        PlayOptions o;
        o.slot = slot::WORLD8;
        o.sound = 16;
        o.bus = "UI";
        o.volume = 127;
        o.delayMs = (std::trunc(distanceCm * 0.0018072) * 1000) / 60;
        sfx_.play(o);
    }
    void stopAll(double fade = 0.25) {
        for (auto& [key, i] : instances_) {
            if (i.crowd) {
                crowd_.removeEmitter(key);
            } else if (i.voice) {
                i.voice->stop(fade);
            }
        }
        instances_.clear();
        for (auto& [k, l] : scriptLoops_) {
            if (l.second) {
                l.second->stop(0);
            }
        }
        scriptLoops_.clear();
        if (ambience_) {
            ambience_->stop(fade);
        }
        ambience_ = nullptr;
        if (rumble_ && rumble_->voice) {
            rumble_->voice->stop(0);
        }
        rumble_.reset();
        named_ = {};
        namedOwner_.clear();
    }

private:
    struct Instance {
        std::shared_ptr<Voice> voice;
        bool crowd = false;
        bool special = false;
    };
    struct Location {
        std::string name;
        const Json* loc = nullptr;
    };
    struct Named {
        std::string name;
        bool ready = false;
    };
    struct Rumble {
        std::shared_ptr<Voice> voice;
        Position position;
        int volume = 0;
    };
    static std::string jsKey(const Json& v) {
        if (v.is_string()) {
            return v.get<std::string>();
        }
        if (v.is_number_integer() || v.is_number_unsigned()) {
            return std::to_string(v.get<int64_t>());
        }
        const double d = v.get<double>();
        if (d == std::trunc(d)) {
            return std::to_string(int64_t(d));
        }
        return v.dump();
    }
    template <class M>
    static void sweep(M& m) {
        for (auto it = m.begin(); it != m.end();) {
            it = it->second->playing() ? std::next(it) : m.erase(it);
        }
    }
    bool isResident(const Json& loc) const {
        return !resident_ || resident_->count(loc["track"].get<int>());
    }
    void loadBanks(const std::string& name, const Json& loc) {
        const auto& banks = loc["banks"];
        auto file = [](const Json& b, const char* k) -> std::string {
            if (!b.contains(k) || !b[k].is_string()) {
                return "";
            }
            std::string s = b[k].get<std::string>();
            if (s.size() > 4 && s.substr(s.size() - 4) == ".bnk") {
                s = s.substr(0, s.size() - 4);
            }
            return s;
        };
        const std::string s8 = file(banks, "slot8");
        const std::string s9 = file(banks, "slot9");
        if (s8.empty() && s9.empty()) {
            return;
        }
        bankOwner_ = name;
        if (!s8.empty()) {
            sfx_.loadBank(slot::WORLD8, s8);
        }
        if (!s9.empty()) {
            sfx_.loadBank(slot::WORLD9, s9);
            if (ambienceWanted_ && !ambience_ && bankOwner_ == name) {
                ambienceStart();
            }
        }
    }
    // 29B818: a named bank into slot 3 (one at a time), true once loaded (at once natively)
    bool namedBank(const std::string& name) {
        if (named_.name != name) {
            named_ = {name, false};
            namedOwner_.clear();
            std::string n = name;
            if (n.size() > 4 && n.substr(n.size() - 4) == ".bnk") {
                n = n.substr(0, n.size() - 4);
            }
            sfx_.loadBank(slot::DYNAMIC, n);
            named_.ready = true;
        }
        return named_.ready;
    }

    Sfx& sfx_;
    Crowd& crowd_;
    Files& files_;
    std::function<void(int)> onSpecial_;
    std::shared_ptr<const Json> doc_;
    std::string loadedEvent_;
    OrderedMap<std::string, Instance> instances_;
    Named named_;
    std::string namedOwner_;
    int track_ = -1;
    std::shared_ptr<Voice> ambience_;
    std::map<std::string, std::shared_ptr<Voice>> contactVoices_;
    std::map<uint32_t, std::pair<int, std::shared_ptr<Voice>>> scriptLoops_;
    std::map<uint32_t, std::shared_ptr<Voice>> scriptVoices_;
    std::optional<Rumble> rumble_;
    std::map<int, Location> locationByTrack_;
    std::optional<std::set<int>> resident_;
    std::string bankOwner_;
    bool ambienceWanted_ = false;
};

}  // namespace audio

}  // namespace ssx_native
