#pragma once
// The career's award cascade and messages (docs/core-migration.md M5d, pv coreCareerCascade), in the rules module: web/career.js
// complete1591E8 / earn159818 / exploreGoal1599A0 / grant159CD0 with the goal predicates, the gear inventory (web/lodge.js
// GearInventory) and the reward record, and web/career-messages.js's hooks (eventComplete, postAward, earnings, notify) with
// MessageInbox / MessageRules, the lodge's purchases (M5e, pv coreCareerLodge) and the world's career events (M5f, pv coreCareerWorld:
// markCollected, challengeStatus). It works on the career save as the page has it (an ordered JSON document: the page's key order is
// kept, so the save blob stays byte-identical), and draws from the session's presentation generator (engine/presentation_rules.hpp,
// the rules module's). Same results as the JS bit for bit (web/test-core-career-cascade.mjs, the career-session differential).
#include "presentation_rules.hpp"
#include "json.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ssx::career {

using json = nlohmann::ordered_json;

constexpr int kNone = -1;
constexpr int kPlatinum = 0;
constexpr int kGold = 1;
constexpr int kSilver = 2;

// ---- JS helpers ----------------------------------------------------------------------------------------------------------------
// a JS number read from the document (absent / null: the caller's default)
inline double num(const json& v, double fallback = 0) {
    return v.is_number() ? v.get<double>() : fallback;
}
inline bool has(const json& o, const char* key) {
    return o.is_object() && o.contains(key) && !o[key].is_null();
}
// `o[key] ??= value`: the member, made when absent or null
inline json& orMake(json& o, const char* key, const json& value) {
    if (!o.contains(key) || o[key].is_null()) {
        o[key] = value;
    }
    return o[key];
}
inline bool includes(const json& list, double v) {
    if (!list.is_array()) {
        return false;
    }
    for (const auto& e : list) {
        if (e.is_number() && e.get<double>() == v) {
            return true;
        }
    }
    return false;
}
// JS truthiness of a JSON value
inline bool truthy(const json& v) {
    if (v.is_null()) {
        return false;
    }
    if (v.is_boolean()) {
        return v.get<bool>();
    }
    if (v.is_number()) {
        const double d = v.get<double>();
        return d != 0 && !std::isnan(d);
    }
    if (v.is_string()) {
        return !v.get<std::string>().empty();
    }
    return true;
}
// a JS number stored the way JSON.parse would give it back (integers as integers)
inline json number(double v) {
    if (std::isfinite(v) && v == std::trunc(v) && std::fabs(v) < 9007199254740992.0) {
        return json(int64_t(v));
    }
    return json(v);
}
// the event key of web/career.js eventKey(mode, course)
inline std::string eventKey(int mode, const json& course) {
    if (course.is_number()) {
        return std::to_string(mode) + ":" + std::to_string(int64_t(course.get<double>()));
    }
    return std::to_string(mode) + ":-";
}

// ---- the gear inventory (web/lodge.js GearInventory) ------------------------------------------------------------------------
struct GearEntry {
    int item, cls, parent, order, flags, price, tier;
    json name;
    bool hasName;
};
struct GearRule {
    int on, item, cond, condState, target, desired;
};
struct GearInventory {
    std::vector<GearEntry> entries;
    std::map<int, unsigned> by;  // item -> first entry
    std::vector<GearRule> rules;
    std::vector<int> defaults;
    std::vector<std::pair<int, int>> f;  // the flags in the Map's order
    std::map<int, unsigned> fIndex;
    int flags(int item) const {
        auto it = fIndex.find(item);
        return it == fIndex.end() ? 0 : f[it->second].second;
    }
    void set(int item, int v) {
        auto it = fIndex.find(item);
        if (it == fIndex.end()) {
            fIndex[item] = unsigned(f.size());
            f.push_back({item, v});
        } else {
            f[it->second].second = v;
        }
    }
    bool owned(int item) const {
        return (flags(item) & 2) != 0;
    }
    const GearEntry* entry(int item) const {
        auto it = by.find(item);
        return it == by.end() ? nullptr : &entries[it->second];
    }
    int equip(int item, bool on) {
        const GearEntry* e = entry(item);
        if (!e) {
            return 0;
        }
        const int was = flags(item) & 0x10;
        set(item, on ? flags(item) | 0x10 : flags(item) & ~0x10);
        if (on) {
            if (was) {
                return 1;
            }
            if (e->flags & 1) {
                for (const auto& s : entries) {
                    if (s.cls == e->cls && s.item != item && ((flags(s.item) & 0x10) || (s.flags & 8))) {
                        if (!equip(s.item, false)) {
                            return 0;
                        }
                    }
                }
            }
            return apply(item, true);
        }
        if (was || (e->flags & 8)) {
            for (const auto& k : entries) {
                if (k.cls == item && !equip(k.item, false)) {
                    return 0;
                }
            }
        }
        return was ? apply(item, false) : 1;
    }
    int apply(int item, bool on) {
        for (const auto& r : rules) {
            if (r.item != item || (r.on != 0) != on || r.target == -1) {
                continue;
            }
            if (r.cond != -1 && ((flags(r.cond) & 0x10) != 0) != (r.condState != 0)) {
                continue;
            }
            const bool desired = r.desired != 0;
            const bool current = (flags(r.target) & 0x10) != 0;
            if (current == desired) {
                if (current) {
                    continue;
                }
                const GearEntry* te = entry(r.target);
                if (!te || !(te->flags & 8)) {
                    continue;
                }
            }
            if (r.target != item) {
                if (!equip(r.target, desired)) {
                    return 0;
                }
            } else {
                set(item, desired ? flags(item) | 0x10 : flags(item) & ~0x10);
            }
        }
        return 1;
    }
    void init() {
        for (int item : defaults) {
            equip(item, true);
        }
        for (auto& [k, v] : f) {
            if (v & 0x10) {
                v |= 0x26;
            }
        }
        bool found = false;
        for (const auto& e : entries) {
            if (e.price == 0) {
                set(e.item, flags(e.item) | 2);
            } else if (e.price == -1) {
                set(e.item, flags(e.item) | 2);
                equip(e.item, true);
                found = true;
            }
        }
        if (found) {
            for (auto& [k, v] : f) {
                v = (v & 0x10) ? v | 6 : v & ~4;
            }
        }
    }
    void buy(int item) {
        set(item, flags(item) | 2);
        for (const auto& e : entries) {
            if (e.price == -item) {
                set(e.item, flags(e.item) | 2);
            }
        }
    }
    std::vector<const GearEntry*> pool(int flag) const {
        std::vector<const GearEntry*> out;
        for (const auto& e : entries) {
            if ((e.flags & flag) && !owned(e.item)) {
                out.push_back(&e);
            }
        }
        return out;
    }
    // serialize(): the nonzero flags (an object with integer keys: JSON orders them ascending, as JS does)
    json serialize() const {
        std::map<int, int> sorted;
        for (const auto& [k, v] : f) {
            if (v) {
                sorted[k] = v;
            }
        }
        json o = json::object();
        for (const auto& [k, v] : sorted) {
            o[std::to_string(k)] = v;
        }
        return o;
    }
    static GearInventory make(const json& table, const json* saved) {
        GearInventory g;
        for (const auto& e : table["entries"]) {
            g.entries.push_back({int(num(e[0])), int(num(e[1])), int(num(e[2])), int(num(e[3])), int(num(e[4])), int(num(e[5])), int(num(e[6])),
                                 e.size() > 7 ? e[7] : json(nullptr), e.size() > 7});
        }
        for (unsigned k = 0; k < g.entries.size(); ++k) {
            g.by.emplace(g.entries[k].item, k);
        }
        for (const auto& r : table["rules"]) {
            g.rules.push_back({int(num(r[0])), int(num(r[1])), int(num(r[2])), int(num(r[3])), int(num(r[4])), int(num(r[5]))});
        }
        for (const auto& d : table["defaults"]) {
            g.defaults.push_back(int(num(d)));
        }
        for (const auto& e : g.entries) {
            if (!g.fIndex.count(e.item)) {
                g.set(e.item, 0);
            }
        }
        if (saved && saved->is_object()) {
            for (const auto& [k, v] : saved->items()) {
                g.set(std::stoi(k), int(num(v)));
            }
        } else {
            g.init();
        }
        return g;
    }
};

// ---- the reward record (0x4C3EF0; web/career.js clearRewardRecord / recordReward), with the JS Maps' / Sets' orders ---------------
struct RewardRecord {
    bool exists = false;
    double cash = 0;
    int medal = kNone;
    std::vector<std::pair<int, std::array<int, 8>>> counts;
    std::array<std::vector<int>, 10> items;
    std::vector<std::pair<int, int>> cheats;
    std::vector<int> gear;
    json gearItem = nullptr;
    void clear() {
        *this = RewardRecord{};
        exists = true;
    }
    static void addSet(std::vector<int>& s, int v) {
        if (std::find(s.begin(), s.end(), v) == s.end()) {
            s.push_back(v);
        }
    }
    void record(int award, int category, int item) {
        if (!exists) {
            clear();
        }
        if (category == 8) {
            for (auto& c : cheats) {
                if (c.first == award) {
                    c.second = item;
                    return;
                }
            }
            cheats.push_back({award, item});
            return;
        }
        if (category == 9) {
            addSet(gear, award);
            gearItem = item;
            return;
        }
        if (!(category >= 0 && category < 8)) {
            return;
        }
        addSet(items[unsigned(category)], item);
        for (auto& c : counts) {
            if (c.first == award) {
                c.second[unsigned(category)]++;
                return;
            }
        }
        std::array<int, 8> c{};
        c[unsigned(category)] = 1;
        counts.push_back({award, c});
    }
    json toJson() const {
        if (!exists) {
            return nullptr;
        }
        json o = json::object();
        o["cash"] = number(cash);
        o["medal"] = medal;
        json c = json::array();
        for (const auto& [award, n] : counts) {
            c.push_back(json::array({award, json(n)}));
        }
        o["counts"] = c;
        json it = json::array();
        for (const auto& s : items) {
            it.push_back(json(s));
        }
        o["items"] = it;
        json ch = json::array();
        for (const auto& [a, v] : cheats) {
            ch.push_back(json::array({a, v}));
        }
        o["cheats"] = ch;
        o["gear"] = json(gear);
        o["gearItem"] = gearItem;
        return o;
    }
    static RewardRecord fromJson(const json& o) {
        RewardRecord r;
        if (!o.is_object()) {
            return r;
        }
        r.exists = true;
        r.cash = num(o["cash"]);
        r.medal = int(num(o["medal"], kNone));
        for (const auto& c : o["counts"]) {
            std::array<int, 8> n{};
            for (unsigned k = 0; k < 8; ++k) {
                n[k] = int(num(c[1][k]));
            }
            r.counts.push_back({int(num(c[0])), n});
        }
        for (unsigned k = 0; k < 10 && k < o["items"].size(); ++k) {
            for (const auto& v : o["items"][k]) {
                r.items[k].push_back(int(num(v)));
            }
        }
        for (const auto& c : o["cheats"]) {
            r.cheats.push_back({int(num(c[0])), int(num(c[1]))});
        }
        for (const auto& g : o["gear"]) {
            r.gear.push_back(int(num(g)));
        }
        r.gearItem = o.contains("gearItem") ? o["gearItem"] : json(nullptr);
        return r;
    }
};

// ---- the session's static data (career.json, shop.json, messages.json, the Big Challenge rows) -----------------------------------
struct Data {
    json rules;
    json courses;
    json rewards;
    json shop;  // null: no lodge (the cascade's gear awards do nothing)
    json messages;  // null: no message hooks
    json challengeRows;  // null: not loaded (no challenge counts)
};

// ---- the cascade over one career save -----------------------------------------------------------------------------------------
struct Cascade {
    const Data& data;
    json& save;
    RewardRecord& record;
    rules::PresentationGenerator& presentation;
    bool messagesOn = false;
    std::vector<int>* awardsCollect = nullptr;  // CareerMessages.awards while completeEvent runs
    int posts = 0;  // message posts made (the page's HUD icon)
    // the gear cache (web/career.js gear(): one inventory for the rider it was made for)
    bool gearMade = false;
    std::string gearId;
    GearInventory gearInv;

    // web/career.js rider(id): made with newRider when absent
    json& rider(const std::string& id) {
        json& riders = save["riders"];
        if (!riders.contains(id) || riders[id].is_null()) {
            static const std::map<std::string, int> kRiderCharacter{{"moby", 0}, {"kaori", 1}, {"allegra", 2}, {"mac", 3},  {"sam", 3}, {"zoe", 4},
                                                                     {"griff", 5}, {"elise", 6}, {"nate", 7},     {"psymon", 8}, {"viggo", 9}};
            auto it = kRiderCharacter.find(id);
            const int character = it == kRiderCharacter.end() ? 3 : it->second;
            json r = json::object();
            r["character"] = character;
            r["cash"] = 0;
            r["earned"] = 0;
            r["medals"] = json::object();
            r["best"] = json::object();
            r["events"] = json::object();
            r["attributes"] = json::array({5, 5, 5, 5, 5, 5, 5});
            json level = json::object();
            level["race"] = json::object({{"level", 1}, {"counter", 0}});
            level["freestyle"] = json::object({{"level", 1}, {"counter", 0}});
            r["level"] = level;
            r["peaks"] = json::array({true, false, false});
            r["lodgePeak"] = 1;
            riders[id] = r;
        }
        return riders[id];
    }
    uint32_t random() {
        return presentation.draw();
    }
    int peakOf(int course) {
        return int(num(data.courses[unsigned(course)]["field54"])) + 1;
    }
    int medal(const std::string& id, int mode, const json& course) {
        const json& medals = rider(id)["medals"];
        const std::string key = eventKey(mode, course);
        return medals.contains(key) && medals[key].is_number() ? int(num(medals[key])) : kNone;
    }
    struct GoalEvent {
        int mode;
        json course;
        int medal;
    };
    std::vector<GoalEvent> goalEvents(const std::string& id, int peak, const std::string& goal) {
        std::vector<GoalEvent> out;
        for (const auto& g : data.rules["goal_events"]) {
            if (int(num(g["peak"])) == peak && g["goal"] == goal) {
                for (const auto& e : g["events"]) {
                    const int mode = int(num(e["mode"]));
                    const json course = e.contains("course") ? e["course"] : json(nullptr);
                    out.push_back({mode, course, medal(id, mode, course)});
                }
                break;
            }
        }
        return out;
    }
    int exploreMedal(double count, const json& row) {
        for (unsigned i = 0; i < row.size(); ++i) {
            if (count >= num(row[i])) {
                return int(i);
            }
        }
        return kNone;
    }
    int collectCount(const std::string& id, int course) {
        const json& r = rider(id);
        if (!has(r, "collected")) {
            return 0;
        }
        const std::string key = std::to_string(course);
        if (!r["collected"].contains(key) || !r["collected"][key].is_array()) {
            return 0;
        }
        int n = 0;
        for (const auto& w : r["collected"][key]) {
            uint32_t v = uint32_t(int64_t(num(w)));
            for (; v; v &= v - 1) {
                n++;
            }
        }
        return n;
    }
    double peakCollected(const std::string& id, int peak) {
        double n = 0;
        for (const auto& c : data.courses) {
            if (num(c["index"]) < 22 && int(num(c["field54"])) == peak - 1) {
                n += collectCount(id, int(num(c["index"])));
            }
        }
        return n;
    }
    double challengesDone(const std::string& id, int peak) {
        const json& r = rider(id);
        if (!r.contains("bigChallenges") || !r["bigChallenges"].is_array() || !data.challengeRows.is_array()) {
            return 0;
        }
        const json& w = r["bigChallenges"];
        double n = 0;
        for (unsigned i = 0; i < data.challengeRows.size(); ++i) {
            const json& x = data.challengeRows[i];
            const double course = num(x["course"], -1);
            if (course >= 0 && course < double(data.courses.size()) && int(num(data.courses[unsigned(course)]["field54"])) == peak - 1 &&
                i < w.size() && (int64_t(num(w[i])) & 8)) {
                n++;
            }
        }
        return n;
    }
    int collectMedal(const std::string& id, int peak) {
        return exploreMedal(peakCollected(id, peak), data.rules["collectible_medals"][unsigned(peak - 1)]);
    }
    int challengeMedal(const std::string& id, int peak) {
        return exploreMedal(challengesDone(id, peak), data.rules["challenge_medals"][unsigned(peak - 1)]);
    }
    bool goalComplete(const std::string& id, int peak, const std::string& goal) {
        json& r = rider(id);
        if (goal == "earnings") {
            return num(r["earned"]) >= num(data.rules["earnings_goal"][unsigned(peak - 1)]);
        }
        if (goal == "freeride") {
            return collectMedal(id, peak) != kNone && challengeMedal(id, peak) != kNone;
        }
        for (const auto& e : goalEvents(id, peak, goal)) {
            if (e.medal == kNone) {
                return false;
            }
        }
        return true;
    }
    bool granted(const std::string& id, int award) {
        json& r = rider(id);
        return r.contains("awards") && includes(r["awards"], award);
    }
    bool peakConquered(const std::string& id, int peak) {
        for (const char* goal : {"race", "freestyle"}) {
            for (const auto& e : goalEvents(id, peak, goal)) {
                const int m = e.medal;
                if (e.mode >= 4 ? m == kNone : !(m != kNone && m <= kGold)) {
                    return false;
                }
            }
        }
        return collectMedal(id, peak) == kPlatinum && challengeMedal(id, peak) == kPlatinum;
    }
    json& monster(const std::string& id) {
        json& r = rider(id);
        if (!r.contains("monster") || r["monster"].is_null()) {
            json m = json::object();
            json bests = json::array();
            for (int k = 0; k < 10; ++k) {
                bests.push_back(nullptr);
            }
            m["bests"] = bests;
            m["medals"] = json::array({0, 0, 0, 0, 0, 0, 0, 0});
            m["bestCombo"] = 0;
            r["monster"] = m;
        }
        return r["monster"];
    }
    bool mountainConquered(const std::string& id) {
        for (int p = 1; p <= 3; ++p) {
            if (!peakConquered(id, p)) {
                return false;
            }
        }
        double sum = 0;
        for (const auto& m : monster(id)["medals"]) {
            // (b | 0)
            sum += double(rules::jsToInt32(m.is_number() ? m.get<double>() : (m.is_boolean() ? double(m.get<bool>()) : 0)));
        }
        return sum >= 24;
    }
    bool peakAllGoals(const std::string& id, int peak) {
        for (const char* g : {"race", "freestyle", "freeride", "earnings"}) {
            if (!goalComplete(id, peak, g)) {
                return false;
            }
        }
        return true;
    }
    // ---- rewards ----
    json& owned(const std::string& id, const std::string& category) {
        json& r = rider(id);
        json& rewards = orMake(r, "rewards", json::object());
        return orMake(rewards, category.c_str(), json::array());
    }
    const json& rewardItems(const std::string& category) {
        static const json empty = json::array();
        if (data.rewards.is_object() && data.rewards.contains(category) && data.rewards[category].contains("items")) {
            return data.rewards[category]["items"];
        }
        return empty;
    }
    int randomUnowned(const std::string& id, const std::string& category) {
        const unsigned all = unsigned(rewardItems(category).size());
        const json& own = owned(id, category);
        if (own.size() >= all) {
            return -1;
        }
        uint32_t n = random() % uint32_t(all - own.size());
        for (unsigned i = 0; i < all; ++i) {
            if (!includes(own, i)) {
                if (n == 0) {
                    return int(i);
                }
                n--;
            }
        }
        return -1;
    }
    void grantReward(const std::string& id, const std::string& category, int index) {
        json& list = owned(id, category);
        if (!includes(list, index)) {
            list.push_back(index);
            std::vector<double> v;
            for (const auto& e : list) {
                v.push_back(num(e));
            }
            std::stable_sort(v.begin(), v.end());
            list = json::array();
            for (double x : v) {
                list.push_back(number(x));
            }
        }
    }
    int cheatIndex(int character) {
        const json& items = rewardItems("cheat_character");
        for (unsigned i = 0; i < items.size(); ++i) {
            if (items[i].contains("character") && items[i]["character"].is_number() && num(items[i]["character"]) == character) {
                return int(i);
            }
        }
        return -1;
    }
    json collectionBonus(const std::string& id, const std::string& category) {
        int cid = -1;
        if (category == "trading_card") {
            cid = 0x11;
        } else if (category == "toy") {
            cid = 0x13;
        } else if (category == "poster") {
            cid = 0x14;
        } else if (category == "art") {
            cid = 0x16;
        }
        if (cid < 0) {
            return json::array();
        }
        if (owned(id, category).size() < rewardItems(category).size()) {
            return json::array();
        }
        const int i = cheatIndex(cid);
        if (i < 0 || includes(owned(id, "cheat_character"), i)) {
            return json::array();
        }
        grantReward(id, "cheat_character", i);
        return json::array({json::object({{"category", "cheat_character"}, {"index", i}})});
    }
    void openPass(const std::string& id, int award, int arg) {
        json& r = rider(id);
        static const std::map<std::string, int> kRiderCharacter{{"moby", 0}, {"kaori", 1}, {"allegra", 2}, {"mac", 3},  {"sam", 3}, {"zoe", 4},
                                                                 {"griff", 5}, {"elise", 6}, {"nate", 7},     {"psymon", 8}, {"viggo", 9}};
        int c = 3;
        if (has(r, "character")) {
            c = int(num(r["character"]));
        } else if (kRiderCharacter.count(id)) {
            c = kRiderCharacter.at(id);
        }
        json& peaks = r["peaks"];
        if (arg == 2 && !peaks[2].get<bool>()) {
            peaks[2] = true;
            record.record(award, 2, c * 3 + 2);
        }
        if ((arg == 1 || arg == 2) && !peaks[1].get<bool>()) {
            peaks[1] = true;
            record.record(award, 2, c * 3 + 1);
        }
    }
    GearInventory* gear(const std::string& id) {
        if (!data.shop.is_object()) {
            return nullptr;
        }
        json& r = rider(id);
        if (!gearMade || gearId != id) {
            const json& table = data.shop["gear"]["runtime"][std::to_string(int(num(r["character"])))];
            const bool saved = r.contains("gearFlags") && r["gearFlags"].is_object() && true;
            // (gearFlags || null): an empty object is truthy in the JS
            gearInv = GearInventory::make(table, saved ? &r["gearFlags"] : nullptr);
            gearMade = true;
            gearId = id;
            if (!saved) {
                r["gearFlags"] = gearInv.serialize();
            }
        }
        return &gearInv;
    }

    // ---- the message hooks (web/career-messages.js) ----
    json& inbox(json& r) {
        if (!r.contains("messages") || r["messages"].is_null()) {
            r["messages"] = json::object({{"entries", json::array()}, {"read", 0}, {"posted", json::array()}});
        }
        return r["messages"];
    }
    // the career facts before a result (CareerMessages.snapshot)
    struct Snapshot {
        std::string id;
        bool open[3][2];
        bool peaks[3];
    };
    Snapshot snapshot(const std::string& id) {
        Snapshot s;
        s.id = id;
        for (int p = 1; p <= 3; ++p) {
            for (int g = 0; g < 2; ++g) {
                bool all = true;
                for (const auto& e : goalEvents(id, p, g == 0 ? "race" : "freestyle")) {
                    if (e.mode < 4 && e.medal == kNone) {
                        all = false;
                    }
                }
                s.open[p - 1][g] = all;
            }
        }
        json& r = rider(id);
        for (int k = 0; k < 3; ++k) {
            s.peaks[k] = r["peaks"][unsigned(k)].get<bool>();
        }
        return s;
    }
    // ---- the lodge (M5e, pv coreCareerLodge: web/career.js buyGear / equipGear / buyUber / selectUber / buySong / buyReward /
    // buyAttributePoint / buyAttribute; the page persists after them as before) ----
    bool buyGear(const std::string& id, int item, double lodgePeak) {
        GearInventory* inv = gear(id);
        if (!inv) {
            return false;
        }
        const GearEntry* e = inv->entry(item);
        if (!e) {
            return false;
        }
        const double price = e->price > 0 ? double(e->price) * 10 : 0;
        if (inv->owned(item) || e->tier != lodgePeak) {
            return false;
        }
        json& r = rider(id);
        if (!(num(r["cash"]) >= price)) {
            return false;
        }
        r["cash"] = number(num(r["cash"]) - price);
        inv->buy(item);
        r["gearFlags"] = inv->serialize();
        return true;
    }
    bool equipGear(const std::string& id, int item, bool on) {
        GearInventory* inv = gear(id);
        if (!inv || !inv->owned(item)) {
            return false;
        }
        inv->equip(item, on);
        rider(id)["gearFlags"] = inv->serialize();
        return true;
    }
    json* uber(const std::string& id) {
        if (!data.shop.is_object()) {
            return nullptr;
        }
        json& r = rider(id);
        if (!r.contains("uber") || r["uber"].is_null()) {
            const std::string hex = data.shop["uber_tricks"]["per_rider_defaults"][unsigned(num(r["character"]))]["save_bytes"].get<std::string>();
            std::vector<int> b;
            for (size_t k = 0; k + 1 < hex.size(); k += 2) {
                b.push_back(std::stoi(hex.substr(k, 2), nullptr, 16));
            }
            std::map<int, json> sorted;
            for (const auto& c : data.shop["uber_tricks"]["categories"]) {
                const int category = int(num(c["category"]));
                const unsigned o = unsigned(category * 6);
                json st = json::object();
                st["base"] = b.at(o);
                st["selected"] = b.at(o + 1);
                st["lock"] = b.at(o + 2) | b.at(o + 3) << 8;
                st["visible"] = b.at(o + 4) | b.at(o + 5) << 8;
                sorted[category] = st;
            }
            // (integer keys: the JS object orders them ascending)
            json out = json::object();
            for (auto& [k, v] : sorted) {
                out[std::to_string(k)] = v;
            }
            r["uber"] = out;
        }
        return &r["uber"];
    }
    const json* uberEntry(int category, int entry) {
        for (const auto& c : data.shop["uber_tricks"]["categories"]) {
            if (int(num(c["category"])) == category) {
                if (c.contains("entries") && entry >= 0 && unsigned(entry) < c["entries"].size()) {
                    return &c["entries"][unsigned(entry)];
                }
                return nullptr;
            }
        }
        return nullptr;
    }
    // uberStatus: 0 none, 1 selected, 2 owned, 3 buy, 4 short
    int uberStatus(const std::string& id, int category, int entry) {
        json* u = uber(id);
        if (!u) {
            return 0;
        }
        const std::string key = std::to_string(category);
        const json* e = uberEntry(category, entry);
        if (!u->contains(key) || !e) {
            return 0;
        }
        const json& st = (*u)[key];
        const bool hidden = e->contains("hidden") && truthy((*e)["hidden"]);
        const int32_t visible = rules::jsToInt32(num(st["visible"]));
        const int32_t lock = rules::jsToInt32(num(st["lock"]));
        if (hidden || !((visible >> (entry & 31)) & 1)) {
            return 0;
        }
        if (!((lock >> (entry & 31)) & 1)) {
            return num(st["selected"], NAN) == entry ? 1 : 2;
        }
        return num(rider(id)["cash"]) >= num((*e)["price"], NAN) ? 3 : 4;
    }
    bool buyUber(const std::string& id, int category, int entry) {
        if (uberStatus(id, category, entry) != 3) {
            return false;
        }
        json& r = rider(id);
        r["cash"] = number(num(r["cash"]) - num((*uberEntry(category, entry))["price"]));
        json& st = (*uber(id))[std::to_string(category)];
        st["lock"] = rules::jsToInt32(num(st["lock"])) & ~int32_t(1u << unsigned(entry & 31));
        return true;
    }
    bool selectUber(const std::string& id, int category, int entry) {
        const int state = uberStatus(id, category, entry);
        if (state == 0 || state == 3 || state == 4) {
            return false;
        }
        (*uber(id))[std::to_string(category)]["selected"] = entry;
        return true;
    }
    bool buySong(const std::string& id, int index) {
        json& r = rider(id);
        json& st = orMake(r, "songs", json::object({{"owned", json::array()}, {"playlist", json::array()}}));
        const json* songs = data.shop.is_object() && data.shop.contains("songs") ? &data.shop["songs"]["songs"] : nullptr;
        if (includes(st["owned"], index) || !songs || !songs->is_array() || index < 0 || unsigned(index) >= songs->size() || !truthy((*songs)[unsigned(index)])) {
            return false;
        }
        const bool pay = st["owned"].size() >= 6;
        if (pay && num(r["cash"]) < 5000) {
            return false;
        }
        if (pay) {
            r["cash"] = number(num(r["cash"]) - 5000);
        }
        st["owned"].push_back(index);
        st["playlist"].push_back(index);
        return true;
    }
    // buyReward; bonus: the page's lastBonus
    bool buyReward(const std::string& id, const std::string& category, int index, double lodgePeak, json& bonus) {
        const json& items = rewardItems(category);
        if (index < 0 || unsigned(index) >= items.size()) {
            return false;
        }
        const json& item = items[unsigned(index)];
        if (includes(owned(id, category), index)) {
            return false;
        }
        const double price = num(item.contains("price") ? item["price"] : json(nullptr), 0);
        const double peak = num(item.contains("peak") ? item["peak"] : json(nullptr), 0);
        if (!price || !peak || peak != lodgePeak) {
            return false;
        }
        json& r = rider(id);
        if (!(num(r["cash"]) >= price)) {
            return false;
        }
        r["cash"] = number(num(r["cash"]) - price);
        grantReward(id, category, index);
        bonus = collectionBonus(id, category);
        return true;
    }
    bool buyAttributePoint(const std::string& id, int index) {
        json& r = rider(id);
        if (index < 0 || unsigned(index) >= r["attributes"].size() || !r["attributes"][unsigned(index)].is_number()) {
            return false;
        }
        const double v = num(r["attributes"][unsigned(index)]);
        const double level = std::trunc(v / 5);
        const json& costs = data.rules["attribute_cost"];
        if (!(level >= 1 && level <= double(costs.size()))) {
            return false;
        }
        const double cost = num(costs[unsigned(level - 1)], NAN);
        if (std::isnan(cost) || num(r["cash"]) < cost || v >= 55) {
            return false;
        }
        r["cash"] = number(num(r["cash"]) - cost);
        r["attributes"][unsigned(index)] = number(v + 1);
        return true;
    }
    bool buyAttribute(const std::string& id, int index) {
        json& r = rider(id);
        if (index < 0 || unsigned(index) >= r["attributes"].size() || !r["attributes"][unsigned(index)].is_number()) {
            return false;
        }
        const double v = num(r["attributes"][unsigned(index)]);
        if (v >= 55) {
            return false;
        }
        const double slot = std::trunc(v / 5) - 1;
        const json& costs = data.rules["attribute_cost"];
        if (!(slot >= 0 && slot < double(costs.size())) || !costs[unsigned(slot)].is_number()) {
            return false;
        }
        const double cost = num(costs[unsigned(slot)]);
        if (num(r["cash"]) < cost) {
            return false;
        }
        r["cash"] = number(num(r["cash"]) - cost);
        r["attributes"][unsigned(index)] = number(v + 5);
        return true;
    }
    // ---- the world's career events (M5f, pv coreCareerWorld: web/career.js markCollected, web/big-challenges.js challengeStatus) ----
    // markCollected (30C3E0 -> 153B00, 119EF8 kind 3, 1599A0): the collect bit, the award's cash (with the earnings notices, as the
    // messages' earnCash hook), the explore goal when the peak's collectible medal changed
    bool markCollected(const std::string& id, int course, int index, double amount) {
        if (!(index >= 0 && index < 64)) {
            return false;
        }
        json& r = rider(id);
        json& collected = orMake(r, "collected", json::object());
        json& m = orMake(collected, std::to_string(course).c_str(), json::array({0, 0}));
        const int peak = course >= 0 && unsigned(course) < data.courses.size() ? peakOf(course) : 0;
        const int before = peak ? collectMedal(id, peak) : kNone;
        const unsigned w = unsigned(index >> 5);
        m[w] = number(double(uint32_t(rules::jsToInt32(num(m[w])) | int32_t(1u << unsigned(index & 31)))));
        if (amount > 0) {
            earningsMessages(id, amount);
            earn159818(id, amount);
        }
        if (peak && collectMedal(id, peak) != before) {
            exploreGoal(id, peak, json("collect"));
        }
        return true;
    }
    // challengeStatus (1540F0 ..., 307308 -> 10F2D8 -> 159B08): the status word, then the explore goal when the peak's Big Challenge
    // medal changed. Returns whether the word was stored (the page persists then).
    bool challengeStatus(const std::string& id, int index, double word) {
        const json& rows = data.challengeRows;
        const json* x = rows.is_array() && index >= 0 && unsigned(index) < rows.size() ? &rows[unsigned(index)] : nullptr;
        int peak = 0;
        if (x) {
            const double course = num((*x)["course"], NAN);
            if (course >= 0 && course < double(data.courses.size())) {
                peak = peakOf(int(course));
            }
        }
        const int before = peak ? challengeMedal(id, peak) : kNone;
        json& r = rider(id);
        bool stored = false;
        if (rows.is_array()) {
            if (!r.contains("bigChallenges") || !r["bigChallenges"].is_array() || r["bigChallenges"].size() != rows.size()) {
                json w = json::array();
                for (const auto& row : rows) {
                    const uint32_t flags = rules::jsToUint32(num(row["flags"]));
                    w.push_back(((flags >> 16) & 1) | 2 | (((flags >> 24) & 1) << 4));
                }
                r["bigChallenges"] = w;
            }
            json& w = r["bigChallenges"];
            if (index >= 0 && unsigned(index) < w.size()) {
                w[unsigned(index)] = number(double(rules::jsToUint32(word)));
                stored = true;
            }
        } else if (r.contains("bigChallenges") && r["bigChallenges"].is_array()) {
            json& w = r["bigChallenges"];
            if (index >= 0 && unsigned(index) < w.size()) {
                w[unsigned(index)] = number(double(rules::jsToUint32(word)));
                stored = true;
            }
        }
        if (peak && challengeMedal(id, peak) != before) {
            exploreGoal(id, peak, json("challenge"));
            stored = true;
        }
        return stored;
    }
    // ---- the cascade ----
    std::vector<json> grantAward(const std::string& id, int award);
    std::vector<json> grant159CD0(const std::string& id, int award);
    std::vector<json> earn159818(const std::string& id, double amount);
    std::vector<json> exploreGoal(const std::string& id, int peak, const json& kind);
    json complete1591E8(const std::string& id, const std::string& key, json& ev, int medal, const json& ticks, const json& score);
    void postAward(const std::string& id, int award, const Snapshot& before);
    void earningsMessages(const std::string& id, double amount);
    void eventComplete(const std::string& id, json& ev, const Snapshot& before, int place, const json& out, const std::vector<int>& awards);
};

// ---- MessageInbox / MessageRules (web/career-messages.js) on a rider's inbox ---------------------------------------------------
constexpr unsigned kInboxSize = 25;
constexpr int kPeakRivals[3] = {3, 7, 8};
struct Box {
    const json& data;  // messages.json
    json& box;         // {entries, read, posted}
    rules::PresentationGenerator& presentation;
    int* posts;
    bool posted(int category) const {
        return includes(box["posted"], category);
    }
    void setPosted(int category, bool on = true) {
        json p = json::array();
        for (const auto& c : box["posted"]) {
            if (!(c.is_number() && num(c) == category)) {
                p.push_back(c);
            }
        }
        if (on) {
            p.push_back(category);
        }
        box["posted"] = p;
    }
    int count() const {
        return int(box["entries"].size());
    }
    int32_t read() const {
        return rules::jsToInt32(num(box["read"]));
    }
    void setRead(int32_t v) {
        box["read"] = v;
    }
    int hasItem(int item) const {
        const json& e = box["entries"];
        for (unsigned k = 0; k < e.size(); ++k) {
            if (num(e[k][0]) == item) {
                return int(k);
            }
        }
        return -1;
    }
    void remove(int k) {
        json& e = box["entries"];
        if (k < 0 || k >= int(e.size())) {
            return;
        }
        int32_t r = read();
        for (int i = k; i < int(e.size()) - 1; ++i) {
            e[unsigned(i)] = e[unsigned(i + 1)];
            if ((uint32_t(r) >> unsigned(i + 1)) & 1) {
                r |= int32_t(1u << unsigned(i));
            } else {
                r &= ~int32_t(1u << unsigned(i));
            }
        }
        e.erase(e.size() - 1);
        r &= int32_t((1u << unsigned(e.size())) - 1);
        setRead(r);
    }
    int categoryOf(int item) const {
        return int(num(data["records"][unsigned(item)]["category"]));
    }
    void removeCategory(int category) {
        int k = 0;
        for (int n = 0; n < count(); n++) {
            if (categoryOf(int(num(box["entries"][unsigned(k)][0]))) == category) {
                remove(k);
            } else {
                k++;
            }
        }
    }
    void add(int item) {
        if (unsigned(count()) == kInboxSize) {
            remove(0);
        }
        const json& record = data["records"][unsigned(item)];
        int variant = 0;
        if (num(record["flag"]) == 1) {
            const int kind = int(num(data["categories"][unsigned(num(record["category"]))]["kind"]));
            if (kind == 3 || kind == 4) {
                variant = int(presentation.draw() % 10);
            } else if (kind == 5) {
                variant = int(presentation.draw() % 11);
            } else if (kind == 8) {
                variant = int(presentation.draw() & 3);
            } else {
                variant = 0xFF;
            }
        }
        setRead(read() & ~int32_t(1u << unsigned(count())));
        box["entries"].push_back(json::array({item, variant}));
        (*posts)++;
    }
    int first(int category) const {
        return int(num(data["categories"][unsigned(category)]["first"]));
    }
    void backcountryResult(int mode, const json& course, int place, int character) {
        const int p = int(num(course, NAN)) - 14;
        if ((mode != 4 && mode != 5) || !course.is_number() || p < 0 || p > 2) {
            return;
        }
        const int result = (mode == 4 ? 22 : 25) + p;
        const int reminder = (mode == 4 ? 1 : 4) + p;
        if (posted(result)) {
            return;
        }
        removeCategory(reminder);
        removeCategory(result);
        int item = (mode == 4 ? 0x73 : 0x7F) + 4 * p + (character == kPeakRivals[p] ? 2 : 0);
        if (place == 0) {
            setPosted(result);
            item++;
        }
        add(item);
    }
    void pendingPeakChallenges() {
        static const int rows[6][3] = {{40, 0, 0}, {44, 0, 1}, {48, 0, 2}, {42, 1, 0}, {46, 1, 1}, {50, 1, 2}};
        for (const auto& row : rows) {
            const int flag = row[0];
            const int jam = row[1];
            const int p = row[2];
            const int silver = posted(flag) ? 0 : posted(flag + 1) ? 1 : -1;
            if (silver < 0) {
                continue;
            }
            const int category = 34 + jam + 2 * p;
            if (!posted(category)) {
                if (silver) {
                    setPosted(category);
                }
                add(first(category) + silver);
            }
            setPosted(flag + silver, false);
        }
    }
    void peakChallengeFlag(int mode, bool silver) {
        const int k = mode - 6;
        if (k < 0 || k > 5) {
            return;
        }
        const int peak = k % 3;
        const int jam = k >= 3 ? 1 : 0;
        if (!posted(34 + jam + 2 * peak)) {
            setPosted(40 + 4 * peak + 2 * jam + (silver ? 1 : 0));
        }
    }
    template <class Open, class Medalled>
    void rivalReminders(int character, Open open, Medalled medalled) {
        for (const auto& [base, list] : {std::pair<int, int>{1, 0}, std::pair<int, int>{4, 1}}) {
            int p = -1;
            for (int q = 0; q < 3; ++q) {
                if (!posted(base + q)) {
                    p = q;
                    break;
                }
            }
            if (p < 0) {
                continue;
            }
            if (!open(p, list) || medalled(p, list)) {
                continue;
            }
            setPosted(base + p);
            add(first(base + p) + (character == kPeakRivals[p] ? 3 : 0) + int(presentation.draw() % 3));
        }
    }
    void rivalTaunt(int peak, int character) {
        if (posted(28 + 2 * peak) || posted(29 + 2 * peak)) {
            return;
        }
        const int category = (character == kPeakRivals[peak] ? 29 : 28) + 2 * peak;
        const json& row = data["categories"][unsigned(category)];
        std::vector<int> pick;
        const int first = int(num(row["first"]));
        const int count = int(num(row["count"]));
        for (int k = first; k < first + count; k++) {
            if (num(data["records"][unsigned(k)]["sender"], NAN) != character && hasItem(k) < 0) {
                pick.push_back(k);
            }
        }
        if (!pick.empty()) {
            add(pick[presentation.draw() % uint32_t(pick.size())]);
        }
    }
    struct WalkEntry {
        int mode;
        bool medalled;
    };
    void goalWalk(int peak, bool raceGoal, const std::vector<WalkEntry>& entries, int character) {
        bool rival = false;
        unsigned i = 0;
        for (; i < entries.size(); i++) {
            const auto& e = entries[i];
            if (e.mode == 4 || e.mode == 5) {
                rival = true;
                if (e.medalled) {
                    continue;
                }
                break;
            }
            if (e.medalled) {
                continue;
            }
            if (rival) {
                break;
            }
            rivalTaunt(peak, character);
            return;
        }
        if (i == entries.size() && raceGoal) {
            rivalTaunt(peak, character);
        }
        setPosted(28 + 2 * peak);
        setPosted(29 + 2 * peak);
    }
    template <class PeakLocked, class FreerideDone>
    void award(int award, PeakLocked peakLocked, FreerideDone freerideDone) {
        auto once = [&](int bit, int item, int test) {
            if (!posted(test)) {
                setPosted(bit);
                add(item);
            }
        };
        if (award == 0) {
            once(56, 251, 56);
        } else if (award == 1) {
            once(60, 255, 60);
        } else if (award == 2) {
            once(57, 252, 57);
        } else if (award == 3) {
            once(57, 253, 58);
        } else if (award == 4) {
            once(59, 254, 59);
        } else if (award == 11 || award == 12) {
            if (peakLocked(1) && freerideDone(0)) {
                once(54, 249, 54);
            }
            if (peakLocked(2) && freerideDone(1)) {
                once(55, 250, 55);
            }
        }
    }
    template <class PeakLocked>
    void earnings(double amount, double earned, const json& goals, PeakLocked peakLocked) {
        if (peakLocked(1) && earned + amount >= num(goals[0], NAN) && !posted(52)) {
            setPosted(52);
            add(247);
        }
        if (peakLocked(2) && earned + amount >= num(goals[1], NAN) && !posted(53)) {
            setPosted(53);
            add(248);
        }
    }
};

inline std::vector<json> Cascade::grantAward(const std::string& id, int award) {
    // CareerMessages.hook's grantAward: the award's message (or, in completeEvent, its place in the list) before the grant
    if (messagesOn && !granted(id, award)) {
        if (awardsCollect) {
            awardsCollect->push_back(award);
        } else {
            postAward(id, award, snapshot(id));
        }
    }
    return grant159CD0(id, award);
}

inline void Cascade::postAward(const std::string& id, int award, const Snapshot& before) {
    json& r = rider(id);
    Box b{data.messages, inbox(r), presentation, &posts};
    b.award(award, [&](int n) { return !before.peaks[n]; }, [&](int p) { return goalComplete(id, p + 1, "freeride"); });
}

inline std::vector<json> Cascade::grant159CD0(const std::string& id, int award) {
    json& r = rider(id);
    orMake(r, "awards", json::array());
    if (!(award >= 17 && award <= 19) && includes(r["awards"], award)) {
        return {};
    }
    std::vector<json> got;
    auto pick = [&](const char* category, int categoryId, int count) {
        for (int k = 0; k < count; k++) {
            const int i = randomUnowned(id, category);
            if (i < 0) {
                break;
            }
            grantReward(id, category, i);
            collectionBonus(id, category);
            record.record(award, categoryId, i);
            got.push_back(json::object({{"category", category}, {"index", i}, {"award", award}}));
        }
    };
    static const std::map<int, int> kCheat{{0, 0x1d}, {1, 0x18}, {2, 0x12}, {3, 0x19}, {4, 0x1a}, {7, 0x1b}, {10, 0x17}, {13, 0x1c}, {16, 0x15}};
    int cheatId = -1;
    const GearEntry* gearItem = nullptr;
    if (kCheat.count(award)) {
        cheatId = kCheat.at(award);
    } else if (award >= 5 && award <= 16) {
        const int peak = (award - 5) % 3;
        if (peak == 0) {
            pick("trading_card", 4, 4);
            pick("poster", 3, 1);
            openPass(id, award, 1);
        } else if (peak == 1) {
            pick("toy", 7, 1);
            pick("art", 5, 2);
            openPass(id, award, 2);
        }
    } else if (award >= 17 && award <= 31) {
        GearInventory* inv = gear(id);
        if (inv) {
            const auto pool = inv->pool(award <= 19 ? 0x100 << (award - 17) : 0x800);
            if (!pool.empty()) {
                gearItem = pool[random() % uint32_t(pool.size())];
            }
        }
    }
    if (!includes(r["awards"], award)) {
        r["awards"].push_back(award);
    }
    if (cheatId >= 0) {
        record.record(award, 8, cheatId);
        const int i = cheatIndex(cheatId);
        if (i >= 0 && !includes(owned(id, "cheat_character"), i)) {
            grantReward(id, "cheat_character", i);
            got.push_back(json::object({{"category", "cheat_character"}, {"index", i}, {"award", award}}));
        }
    }
    if (gearItem) {
        const int item = gearItem->item;
        json name = gearItem->name;
        const bool hasName = gearItem->hasName;
        record.record(award, 9, item);
        GearInventory* inv = gear(id);
        inv->buy(item);
        rider(id)["gearFlags"] = inv->serialize();
        json g = json::object({{"category", "gear"}, {"item", item}});
        if (hasName) {
            g["name"] = name;
        }
        g["award"] = award;
        got.push_back(g);
    }
    if (got.empty() && award >= 5 && award <= 16) {
        got.push_back(json::object({{"award", award}, {"category", nullptr}}));
    }
    auto append = [&](std::vector<json> more) {
        for (auto& m : more) {
            got.push_back(std::move(m));
        }
    };
    if (mountainConquered(id) && !granted(id, 0)) {
        append(grantAward(id, 0));
    }
    if (peakAllGoals(id, 1) && peakAllGoals(id, 2) && peakAllGoals(id, 3) && !granted(id, 1)) {
        append(grantAward(id, 1));
    }
    for (int p = 1; p <= 3; ++p) {
        if (peakAllGoals(id, p) && !granted(id, 1 + p)) {
            append(grantAward(id, 1 + p));
        }
    }
    static const char* kGoals[4] = {"race", "freestyle", "freeride", "earnings"};
    for (int p = 1; p <= 3; ++p) {
        for (int goal = 0; goal < 4; ++goal) {
            const int a = 5 + goal * 3 + p - 1;
            if (goalComplete(id, p, kGoals[goal]) && !granted(id, a)) {
                append(grantAward(id, a));
            }
        }
    }
    return got;
}

inline std::vector<json> Cascade::earn159818(const std::string& id, double amountIn) {
    json& r = rider(id);
    const int32_t amount = rules::jsToInt32(amountIn);
    if (!amount) {
        return {};
    }
    bool open[3];
    int firstOpen = -1;
    for (int p = 1; p <= 3; ++p) {
        open[p - 1] = !goalComplete(id, p, "earnings");
        if (open[p - 1] && firstOpen < 0) {
            firstOpen = p - 1;
        }
    }
    const double threshold = firstOpen >= 0 ? num(data.rules["earnings_goal"][unsigned(firstOpen)], NAN) : 0;
    r["cash"] = number(num(r["cash"]) + amount);
    r["earned"] = number(num(r["earned"]) + amount);
    if (!threshold || std::isnan(threshold) || num(r["earned"]) < threshold) {
        return {};
    }
    std::vector<json> got;
    for (int p = 1; p <= 3; ++p) {
        if (open[p - 1] && goalComplete(id, p, "earnings") && !granted(id, 14 + p - 1)) {
            for (auto& m : grantAward(id, 14 + p - 1)) {
                got.push_back(std::move(m));
            }
        }
    }
    return got;
}

inline void Cascade::earningsMessages(const std::string& id, double amount) {
    // CareerMessages.hook's earnCash: 1E3760 before 1597B0 adds the cash
    if (!messagesOn || !(amount > 0)) {
        return;
    }
    json& r = rider(id);
    Box b{data.messages, inbox(r), presentation, &posts};
    const json peaks = r["peaks"];
    b.earnings(amount, num(r["earned"]), data.rules["earnings_goal"], [&](int n) { return !peaks[unsigned(n)].get<bool>(); });
}

inline std::vector<json> Cascade::exploreGoal(const std::string& id, int peak, const json& kind) {
    std::vector<json> got;
    if (goalComplete(id, peak, "freeride")) {
        const bool challenge = kind.is_string() && kind.get<std::string>() == "challenge";
        const double count = challenge ? challengesDone(id, peak) : peakCollected(id, peak);
        const json& row = data.rules[challenge ? "challenge_medals" : "collectible_medals"][unsigned(peak - 1)];
        if ((kind.is_null() || count == num(row[3], NAN)) && !granted(id, 11 + peak - 1)) {
            got = grantAward(id, 11 + peak - 1);
        }
    }
    if (mountainConquered(id) && !granted(id, 0)) {
        for (auto& m : grantAward(id, 0)) {
            got.push_back(std::move(m));
        }
    }
    return got;
}

inline json Cascade::complete1591E8(const std::string& id, const std::string& key, json& ev, int medal, const json& ticks, const json& score) {
    json out = json::object({{"medal", medal}, {"cash", 0}, {"opened", json::array()}});
    if (medal == kNone) {
        return out;
    }
    json& r = rider(id);
    const bool before[2] = {r["peaks"][1].get<bool>(), r["peaks"][2].get<bool>()};
    const int previous = r["medals"].contains(key) && r["medals"][key].is_number() ? int(num(r["medals"][key])) : kNone;
    const int mode = int(num(ev["mode"]));
    const int peak = mode >= 6 ? ((mode - 6) % 3) + 1 : peakOf(int(num(ev["course"])));
    const bool raceGoal = mode == 0 || mode == 4 || mode == 6 || mode == 7 || mode == 8;
    const char* goal = raceGoal ? "race" : "freestyle";
    const double full = mode < 6 ? num(data.rules["cash"][unsigned(num(ev["course"]))][unsigned(medal)], NAN)
                                 : num(data.rules["peak_challenges"][unsigned((mode - 6) * 3 + (medal <= kGold ? 2 : medal == kSilver ? 1 : 0))][9], NAN) * 100;
    orMake(r, "golds", json::object());
    const bool timed = mode == 0 || mode == 4 || (mode >= 6 && mode <= 8);
    auto store = [&]() {
        if (previous == kNone || medal < previous) {
            r["medals"][key] = medal;
        }
        if (medal <= kGold) {
            const double g = r["golds"].contains(key) ? num(r["golds"][key]) : 0;
            r["golds"][key] = number((g ? g : 0) + 1);
        }
        const json& value = timed ? ticks : score;
        const bool none = !r["best"].contains(key) || r["best"][key].is_null();
        const double best = none ? 0 : num(r["best"][key]);
        const double v = num(value);
        if (none || (timed ? v < best : v > best)) {
            r["best"][key] = value;
        }
    };
    std::vector<json> awards;
    auto append = [&](std::vector<json> more) {
        for (auto& m : more) {
            awards.push_back(std::move(m));
        }
    };
    const bool goldsBefore = r["golds"].contains(key) && num(r["golds"][key]) != 0;
    if (medal <= kGold && !goldsBefore) {
        append(grantAward(id, (mode < 4 ? 17 : mode == 4 ? 20 : mode == 5 ? 23 : mode <= 8 ? 26 : 29) + peak - 1));
    }
    double cash = full;
    if (previous == kNone || medal < previous) {
        const bool was = goalComplete(id, peak, goal);
        append(earn159818(id, cash));
        store();
        const int a = 5 + (raceGoal ? 0 : 1) * 3 + peak - 1;
        if (!was && goalComplete(id, peak, goal) && !granted(id, a)) {
            append(grantAward(id, a));
        }
    } else {
        store();
        cash = double(rules::jsToUint32(full) >> 1);
        append(earn159818(id, cash));
    }
    if (mountainConquered(id) && !granted(id, 0)) {
        append(grantAward(id, 0));
    }
    if (!record.exists) {
        record.clear();
    }
    record.cash = cash;
    record.medal = medal;
    out["cash"] = number(cash);
    out["awards"] = json(awards);
    json opened = json::array();
    for (int k = 1; k <= 2; ++k) {
        if (!before[k - 1] && r["peaks"][unsigned(k)].get<bool>()) {
            opened.push_back(k + 1);
        }
    }
    out["opened"] = opened;
    return out;
}

inline void Cascade::eventComplete(const std::string& id, json& ev, const Snapshot& before, int place, const json& out, const std::vector<int>& awards) {
    if (!(ev.contains("career") && ev["career"].is_boolean() && ev["career"].get<bool>())) {
        return;
    }
    json& r = rider(id);
    const int character = int(num(r["character"]));
    Box b{data.messages, inbox(r), presentation, &posts};
    const int mode = int(num(ev["mode"]));
    const json course = ev.contains("course") ? ev["course"] : json(nullptr);
    const int outMedal = int(num(out["medal"]));
    b.backcountryResult(mode, course, place, character);
    if (outMedal != kNone) {
        b.pendingPeakChallenges();
        if (mode != 4 && mode != 5) {
            b.rivalReminders(
                character, [&](int p, int list) { return before.open[p][list]; },
                [&](int p, int list) { return medal(id, list ? 5 : 4, json(14 + p)) != kNone; });
        }
        for (int a : awards) {
            b.award(a, [&](int n) { return !before.peaks[n]; }, [&](int p) { return goalComplete(id, p + 1, "freeride"); });
        }
        if (outMedal >= kSilver) {
            b.peakChallengeFlag(mode, outMedal == kSilver);
        }
        const int peak = mode >= 6 ? (mode - 6) % 3 : peakOf(int(num(course))) - 1;
        const bool raceGoal = mode == 0 || mode == 4 || (mode >= 6 && mode <= 8);
        std::vector<Box::WalkEntry> walk;
        for (const auto& e : goalEvents(id, peak + 1, raceGoal ? "race" : "freestyle")) {
            walk.push_back({e.mode, e.medal != kNone});
        }
        b.goalWalk(peak, raceGoal, walk, character);
    }
}

}  // namespace ssx::career

namespace ssx::career {

// One kernel call (web/rules_module.cpp career_kernel): in {op, save, id, record, messages, ...}; out {save, record, result, posts}.
//   op "complete": key, ev, medal, ticks, score, place -> complete1591E8's out, then the event's messages (CareerMessages.eventComplete)
//   op "earn": amount -> the earnings messages, then earn159818's awards (web/career.js earnCash with its hook)
//   op "explore": peak, kind -> exploreGoal1599A0's awards
//   op "grant": award -> grantAward's awards (its message first)
inline json kernel(const Data& data, rules::PresentationGenerator& presentation, json in) {
    json save = std::move(in["save"]);
    RewardRecord record = RewardRecord::fromJson(in["record"]);
    Cascade c{data, save, record, presentation};
    c.messagesOn = in.value("messages", false) && data.messages.is_object();
    const std::string op = in["op"].get<std::string>();
    const std::string id = in["id"].get<std::string>();
    json result;
    if (op == "complete") {
        json ev = in["ev"];
        const Cascade::Snapshot before = c.snapshot(id);
        std::vector<int> awards;
        if (c.messagesOn) {
            c.awardsCollect = &awards;
        }
        result = c.complete1591E8(id, in["key"].get<std::string>(), ev, int(num(in["medal"], kNone)), in["ticks"], in["score"]);
        c.awardsCollect = nullptr;
        if (c.messagesOn) {
            json out = result;
            c.eventComplete(id, ev, before, int(num(in["place"])), out, awards);
        }
    } else if (op == "earn") {
        const double amount = num(in["amount"], NAN);
        c.earningsMessages(id, amount);
        result = json(c.earn159818(id, amount));
    } else if (op == "explore") {
        result = json(c.exploreGoal(id, int(num(in["peak"])), in.contains("kind") ? in["kind"] : json(nullptr)));
    } else if (op == "grant") {
        result = json(c.grantAward(id, int(num(in["award"]))));
    } else if (op == "collect") {
        result = c.markCollected(id, int(num(in["course"])), int(num(in["index"], -1)), num(in["amount"]));
    } else if (op == "challenge") {
        result = c.challengeStatus(id, int(num(in["index"], -1)), num(in["word"]));
    } else if (op == "buyGear") {
        result = c.buyGear(id, int(num(in["item"])), num(in["lodgePeak"], NAN));
    } else if (op == "equipGear") {
        result = c.equipGear(id, int(num(in["item"])), in.value("on", false));
    } else if (op == "buyUber") {
        result = c.buyUber(id, int(num(in["category"])), int(num(in["entry"])));
    } else if (op == "selectUber") {
        result = c.selectUber(id, int(num(in["category"])), int(num(in["entry"])));
    } else if (op == "buySong") {
        result = c.buySong(id, int(num(in["index"])));
    } else if (op == "buyReward") {
        json bonus = nullptr;
        const bool ok = c.buyReward(id, in["category"].get<std::string>(), int(num(in["index"])), num(in["lodgePeak"], NAN), bonus);
        result = json::object({{"ok", ok}, {"bonus", bonus}});
    } else if (op == "buyAttributePoint") {
        result = c.buyAttributePoint(id, int(num(in["index"])));
    } else if (op == "buyAttribute") {
        result = c.buyAttribute(id, int(num(in["index"])));
    } else {
        throw std::runtime_error("career kernel: unknown op " + op);
    }
    json out = json::object();
    out["save"] = std::move(save);
    out["record"] = record.toJson();
    out["result"] = std::move(result);
    out["posts"] = c.posts;
    return out;
}

}  // namespace ssx::career
