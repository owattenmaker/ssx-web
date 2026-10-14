#pragma once
// The career tables and the original locale texts (web/career.js data, web/locale.js; CAREER/career.json from
// tools/export_career.py): the courses, the characters, the record defaults and slots, the race rules, and the FEAMER / OVAMER /
// CMNAMER strings keyed by the ELF hash of their kT_ id. Also web/race-time.mjs raceTime.
#include "json.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace ssx_native {

namespace fe {

// web/locale.js nameHash: the ELF hash of a kT_ id (tools/sam_ps2/loc_file.py name_hash)
inline uint32_t nameHash(const std::string& name) {
    uint32_t h = 0;
    for (unsigned char ch : name) {
        h = (h << 4) + ch;
        const uint32_t g = h & 0xf0000000u;
        if (g) {
            h = h ^ (g >> 23) ^ g;
        }
    }
    return h;
}

// web/locale.js clean: '\\' (a doubled backslash in the LOC source) is the line break; typographic quotes, dashes and the ellipsis
// as the ASCII the bitmap fonts have
inline std::string cleanText(const std::string& text) {
    static const std::vector<std::pair<std::string, std::string>> swaps = {
        {"\\\\", "\n"},
        {"\xE2\x80\x98", "'"},
        {"\xE2\x80\x99", "'"},
        {"\xE2\x80\x9C", "\""},
        {"\xE2\x80\x9D", "\""},
        {"\xE2\x80\x93", "-"},
        {"\xE2\x80\x94", "-"},
        {"\xE2\x80\xA6", "..."},
    };
    std::string out;
    out.reserve(text.size());
    for (size_t i = 0; i < text.size();) {
        bool swapped = false;
        for (const auto& [from, to] : swaps) {
            if (text.compare(i, from.size(), from) == 0) {
                out += to;
                i += from.size();
                swapped = true;
                break;
            }
        }
        if (!swapped) {
            out += text[i];
            ++i;
        }
    }
    return out;
}

// web/race-time.mjs raceTime: integer 60 Hz ticks as MM:SS (and :CC), '--:--' for anything else
inline std::string raceTime(double ticks, bool centiseconds = true) {
    if (!(std::isfinite(ticks) && ticks == std::floor(ticks)) || ticks < 0) {
        return "--:--";
    }
    const long long t = (long long)ticks;
    const long long seconds = t / 60;
    const long long minutes = seconds / 60;
    char buf[48];
    if (centiseconds) {
        std::snprintf(buf, sizeof buf, "%02lld:%02lld:%02lld", minutes, seconds % 60, (t % 60) * 100 / 60);
    } else {
        std::snprintf(buf, sizeof buf, "%02lld:%02lld", minutes, seconds % 60);
    }
    return buf;
}

struct CareerCourse {
    int index = 0;
    std::string name, code, kind;
};

class CareerData {
public:
    bool load(const nlohmann::json& data) {
        if (!data.is_object() || !data.contains("courses") || !data.contains("rules")) {
            return false;
        }
        for (const auto& c : data["courses"]) {
            CareerCourse cc;
            cc.index = c.value("index", 0);
            cc.name = c.value("name", "");
            cc.code = c.value("code", "");
            cc.kind = c.value("kind", "");
            courses.push_back(cc);
        }
        for (const auto& c : data.value("characters", nlohmann::json::array())) {
            characterFirst.push_back(c.value("first", ""));
        }
        rules = data["rules"];
        const nlohmann::json& s = data.value("strings", nlohmann::json::object());
        for (const char* file : {"OVAMER", "FEAMER", "CMNAMER"}) {
            auto& table = strings_[file];
            if (!s.contains(file)) {
                continue;
            }
            for (auto it = s[file].begin(); it != s[file].end(); ++it) {
                if (it.value().is_string()) {
                    table[it.key()] = it.value().get<std::string>();
                }
            }
        }
        return true;
    }
    // web/locale.js Locale.text: overlay, front end, common; cleaned
    std::string text(uint32_t hash, const std::string& fallback = "") const {
        char hex[9];
        std::snprintf(hex, sizeof hex, "%08x", hash);
        for (const char* file : {"OVAMER", "FEAMER", "CMNAMER"}) {
            auto t = strings_.find(file);
            if (t == strings_.end()) {
                continue;
            }
            auto it = t->second.find(hex);
            if (it != t->second.end()) {
                return cleanText(it->second);
            }
        }
        return fallback;
    }
    std::string text(const std::string& key, const std::string& fallback = "") const {
        return text(nameHash(key), fallback);
    }
    // web/career-ui.js t(): the text, or the fallback when it is missing or empty
    std::string t(const std::string& key, const std::string& fallback = "") const {
        const std::string s = text(key, fallback);
        return s.empty() ? fallback : s;
    }
    std::string t(uint32_t hash, const std::string& fallback = "") const {
        const std::string s = text(hash, fallback);
        return s.empty() ? fallback : s;
    }
    // web/career.js standardMode (KIND_MODE): race 0, slope style 1, super pipe 2, big air 3; -1 for the rest
    int standardMode(int course) const {
        if (course < 0 || size_t(course) >= courses.size()) {
            return -1;
        }
        const std::string& k = courses[size_t(course)].kind;
        return k == "race" ? 0 : k == "slopestyle" ? 1 : k == "superpipe" ? 2 : k == "bigair" ? 3 : -1;
    }
    int courseOf(const std::string& code) const {
        for (const auto& c : courses) {
            if (c.code == code) {
                return c.index;
            }
        }
        return -1;
    }
    // web/career.js eventName for the standard events and the rival runs
    std::string eventName(int mode, int course) const {
        if (mode >= 6) {
            static const char* names[6] = {"Peak 1 Race", "Peak 2 Race", "All Peak Race", "Peak 1 Jam", "Peak 2 Jam", "All Peak Jam"};
            return mode - 6 < 6 ? names[mode - 6] : "";
        }
        const std::string name = course >= 0 && size_t(course) < courses.size() ? courses[size_t(course)].name : "";
        return mode == 4 ? name + " Race" : mode == 5 ? name + " Jam" : name;
    }

    std::vector<CareerCourse> courses;
    std::vector<std::string> characterFirst;
    nlohmann::json rules;

private:
    std::map<std::string, std::map<std::string, std::string>> strings_;
};

}  // namespace fe

}  // namespace ssx_native
