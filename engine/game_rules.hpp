#pragma once
// Event and career rules of the game-mode manager, the lineup and the relationships (docs/core-migration.md slice M1).
// Ported from the browser's JS versions (web/lineup.js, web/event-anchor-rng.js, web/ai-race.js, web/career.js,
// web/rival-mode.js) so that they give the same results bit for bit. The PS2 addresses are the ones those files give.
//
// Arithmetic: the JS versions compute in IEEE single precision with round to nearest (Math.fround), not in the EE's
// arithmetic (MUL toward zero, ADD with the guard bit). These functions keep the JS form, so a page with pv coreRules on
// or off gives the same results. Where the JS rounds toward zero on purpose (npc_difficulty), so does this.
// Build with -ffp-contract=off (no fused multiply-add), as web/build-core.sh does.
#include "original_random.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <vector>

namespace ssx::rules {

// ---- JS number conversions ---------------------------------------------------------------------------------
// ToInt32 / ToUint32 of a double (the JS `x | 0` and `x >>> 0`): modulo 2^32, NaN and infinities give 0.
inline uint32_t jsToUint32(double x) {
    if (!std::isfinite(x)) {
        return 0;
    }
    double t = std::trunc(x);
    double m = std::fmod(t, 4294967296.0);
    if (m < 0) {
        m += 4294967296.0;
    }
    return uint32_t(m);
}
inline int32_t jsToInt32(double x) {
    return int32_t(jsToUint32(x));
}
// Math.min of two doubles (NaN when either is NaN).
inline double jsMin(double a, double b) {
    if (std::isnan(a) || std::isnan(b)) {
        return NAN;
    }
    return a < b ? a : b;
}

// ---- The six-word generator 0x317A08 and its seeding 0x317958 --------------------------------------------------
using Words = std::array<uint32_t, 6>;
inline constexpr std::array<uint32_t, 6> kSeedAdd{0xF22D0E56u, 0x96041893u, 0x3DF3B646u, 0x40DDE76Du, 0x97327AE1u, 0xD1A9FBE7u};
// 0x317958: the words of a seed (web/lineup.js seededWords).
inline Words seededWords(uint32_t seed) {
    Words w{};
    uint32_t a = seed;
    for (unsigned k = 0; k < 6; ++k) {
        a += kSeedAdd[k];
        w[k] = a;
    }
    return w;
}
// 0x317A08: one draw; the words advance in place (web/lineup.js nextWord, web/ai-racers.js rngNext).
inline uint32_t nextWord(Words& w) {
    OriginalRandomState s;
    s.words = w;
    const uint32_t value = s.next();
    w = s.words;
    return value;
}
// The words after n draws from seed 0x317958(seed) (web/lineup.js anchorRandomWords / rivalAnchorWords).
inline Words wordsAfterDraws(uint32_t seed, uint32_t draws) {
    Words w = seededWords(seed);
    for (uint32_t k = 0; k < draws; ++k) {
        nextWord(w);
    }
    return w;
}
// 0x31AE94: the boot seed from sceCdReadClock's BCD bytes (web/lineup.js clockSeed). Local time fields as JS Date gives them.
inline uint32_t clockSeed(int seconds, int minutes, int hours, int day, int month, int fullYear) {
    auto bcd = [](int v) {
        const int tens = int(std::floor(double(v) / 10.0)) % 10;
        return uint32_t((tens << 4) | (v % 10));
    };
    const uint32_t w0 = (bcd(seconds) << 8) | (bcd(minutes) << 16) | (bcd(hours) << 24);
    const uint32_t w1 = (bcd(day) << 8) | (bcd(month) << 16) | (bcd(fullYear % 100) << 24);
    return w0 ^ (w1 << 3);
}

// ---- Rosters (0x23A4F0, 0x23A108, 0x238E20, 0x23A668) -----------------------------------------------------------
// 0x145750: the peak rival (course table 0x43D950 + event * 100 + 0x54 = peak index).
inline int peakRival(int peak, int human) {
    if (peak == 0) {
        return human == 3 ? 5 : 3;
    }
    if (peak == 1) {
        return human == 7 ? 4 : 7;
    }
    if (peak == 2) {
        return human == 8 ? 6 : 8;
    }
    return 3;
}
// 0x23A4F0 on a running roster generator: two distinct Tricky cheat riders, the base riders but the human and the
// rival, 25 swaps (0x23C770). 52 draws.
inline std::array<int, 10> buildRosterFrom(Words& w, int human, int peak) {
    std::array<int, 10> entries{};
    unsigned count = 0;
    const int first = 10 + int(nextWord(w) % 7u);
    const int r = int(nextWord(w) % 6u);
    entries[count++] = first;
    entries[count++] = 10 + r < first ? 10 + r : 11 + r;
    const int rival = peakRival(peak, human);
    for (int v = 0; v < 10 && count < 10; ++v) {
        if (v != human && v != rival) {
            entries[count++] = v;
        }
    }
    while (count < 10) {
        entries[count++] = 0;
    }
    for (int k = 0; k < 25; ++k) {
        const unsigned i = nextWord(w) % 10u;
        const unsigned j = nextWord(w) % 10u;
        std::swap(entries[i], entries[j]);
    }
    return entries;
}
// The five computer riders' entries of a round (0x23A108, 0x23A254 / 0x23A360; web/lineup.js roundEntries).
// previous: the previous round's roster values in finishing order, -1 for the human (JS null); at most the first three
// are read. entries: the ten roster entries.
inline std::array<int, 5> roundEntries(const std::array<int, 10>& entries, int round, bool career, int rival, const int* previous,
                                       unsigned previousCount) {
    std::array<int, 5> out{};
    if (!career || round == 1) {
        for (unsigned k = 0; k < 5; ++k) {
            out[k] = entries[k];
        }
        return out;
    }
    std::vector<int> advanced;
    for (unsigned k = 0; k < previousCount && k < 3; ++k) {
        if (previous[k] >= 0) {
            advanced.push_back(previous[k]);
        }
    }
    // semi: [x, x, entries 5..7], the advanced riders from +0x2C; final: [rival, x, x, entries 8, 9], from +0x48
    std::array<int, 5> slots{};
    std::array<bool, 5> set{};
    if (round == 2) {
        slots = {0, 0, entries[5], entries[6], entries[7]};
        set = {false, false, true, true, true};
    } else {
        slots = {rival, 0, 0, entries[8], entries[9]};
        set = {true, false, false, true, true};
    }
    const unsigned from = round == 2 ? 0 : 1;
    for (unsigned i = 0; i < advanced.size(); ++i) {
        slots[from + i] = advanced[i];
        set[from + i] = true;
    }
    // a slot nobody advanced into: a stale word in the original; the JS takes entries[base + i], else entries[i]
    const unsigned base = round == 2 ? 5 : 8;
    for (unsigned i = 0; i < 5; ++i) {
        if (set[i]) {
            out[i] = slots[i];
        } else {
            out[i] = base + i < 10 ? entries[base + i] : entries[i];
        }
    }
    return out;
}
// 0x239938: the characters that are neither the human nor the peak rival, in id order, 25 swaps (r % n, r % n).
inline std::vector<int> freestyleRoster(Words& w, int human, int rival) {
    std::vector<int> e;
    for (int v = 0; v < 10; ++v) {
        if (v != human && v != rival) {
            e.push_back(v);
        }
    }
    const uint32_t n = uint32_t(e.size());
    for (int k = 0; k < 25; ++k) {
        const uint32_t i = nextWord(w) % n;
        const uint32_t j = nextWord(w) % n;
        std::swap(e[i], e[j]);
    }
    return e;
}

// ---- Relationships (0x155E58 ageing, 0x155BF0 events; record = kind, level, score) -------------------------------
struct Relationship {
    int kind;
    int level;
    int score;
};
inline Relationship relationshipRules(int kind, int level, int score, bool ageing) {
    if (kind == 1) {
        if (level >= 4) {
            return {3, 2, 10};
        }
    } else if (kind == 0) {
        if (level >= 3) {
            return {0, 2, 10};
        }
    } else if (kind == 2) {
        if (level <= 0) {
            return ageing ? Relationship{2, 1, score} : Relationship{2, 1, 5};
        }
        if (level >= 4) {
            return {2, 3, 15};
        }
    } else if (kind == 3) {
        if (ageing ? level == 0 : level <= 0) {
            return ageing ? Relationship{1, 3, 15} : Relationship{3, 1, 5};
        }
        if (level >= 5) {
            return {3, 4, 20};
        }
    }
    return {kind, level, score};
}
// 0x155E58: score - 3 (not below 0), level = score / 5 (truncated).
inline Relationship relationshipAge(int kind, int score) {
    const int s = std::max(score - 3, 0);
    return relationshipRules(kind, s / 5, s, true);
}
// 0x155BF0: event 0 soft bump, 1 crash, 2 soft attack, 3 crash attack (+1 / +2 / +4 / +6; any other event +0).
inline Relationship relationshipEvent(int kind, int score, int event) {
    static constexpr int add[4] = {1, 2, 4, 6};
    const int s = score + (event >= 0 && event < 4 ? add[event] : 0);
    return relationshipRules(kind, s / 5, s, false);
}

// ---- Computer-rider difficulty (cComputer_updateRiderDifficulty 0x10C758) --------------------------------------
// 0x10C450 (jump table 0x456A70): owner +0xE00 route role of a slot in a round (1 qualifier, 2 semi, 3 final / Single Event).
inline int npcRoundRole(int slot, int round) {
    if (slot == 1 || slot == 2) {
        return round == 1 || round == 2 ? 1 : 2;
    }
    if (slot == 3) {
        return round == 1 ? 0 : 1;
    }
    if (slot == 4) {
        return round == 1 || round == 2 ? 0 : 1;
    }
    return 0;
}
// A double rounded to single precision toward zero (the PS2 FPU's MUL; web/lineup.js towardZero).
inline float towardZero(double x) {
    float y = float(x);
    if (std::fabs(double(y)) <= std::fabs(x)) {
        return y;
    }
    uint32_t bits = std::bit_cast<uint32_t>(y);
    bits -= 1;
    return std::bit_cast<float>(bits);
}
struct Difficulty {
    float df8;
    float dfc;
};
// 0x10C4F8 (slot tables 0x456AB0 level 0, 0x456A90 level 1, 0x456AD0 level 2) and 0x10C758's course factor
// (1.25 on course 4, 1.1 on courses 2 / 3), capped at 1. Returns false for a slot or level the tables lack.
inline bool npcDifficulty(int slot, int level, int course, Difficulty& out) {
    static constexpr double table[3][5][2] = {
        {{100, 83.89308166503906}, {55.80497360229492, 64.6436767578125}, {40.250186920166016, 60.00449752807617},
         {15.542302131652832, 52.00495529174805}, {7.954832077026367, 35.020263671875}},
        {{100, 86.91539764404297}, {80.38914489746094, 79.59983825683594}, {51.00757598876953, 68.90123748779297},
         {46.601898193359375, 56.843074798583984}, {14.000700950622559, 42.441043853759766}},
        {{100, 100}, {84.99810028076172, 82.68248748779297}, {74.99872589111328, 70.82238006591797},
         {54.99583435058594, 59.94406509399414}, {29.981433868408203, 46.99877166748047}}};
    if (level < 0 || level > 2 || slot < 1 || slot > 5) {
        return false;
    }
    const double df8 = table[level][slot - 1][0];
    const double base = table[level][slot - 1][1];
    float dfc = towardZero(double(float(base)) * double(float(0.009999999776482582)));
    bool scaled = false;
    double k = 0;
    if (course == 4) {
        k = 0.25;
        scaled = true;
    } else if (course == 2 || course == 3) {
        k = double(float(0.10000000149011612));
        scaled = true;
    }
    if (scaled) {
        dfc = towardZero(double(dfc) + double(towardZero(double(dfc) * k)));
    }
    out.df8 = float(df8);
    out.dfc = std::min(dfc, 1.0f);
    return true;
}

// ---- The game RNG at a solo event's countdown anchor (web/event-anchor-rng.js, measured per course) --------------
struct EventAnchor {
    const char* code;
    int tick;
    int base;
    int start;
};
inline constexpr std::array<EventAnchor, 8> kEventAnchors{{
    {"ABA1", 18, 10, 0},
    {"BHP1", 18, 17, 0},
    {"CBA2", 18, 12, 0},
    {"CHP2", 18, 15, 0},
    {"EBA3", 19, 12, 0},
    {"EHP3", 18, 13, 0},
    {"ERA5", 18, 17, 10},
    {"ESS3", 18, 21, 2},
}};
inline const EventAnchor* eventAnchor(const char* code) {
    for (const auto& a : kEventAnchors) {
        if (std::strcmp(a.code, code) == 0) {
            return &a;
        }
    }
    return nullptr;
}
// lineups.json load_draws rule of every course: Zoe (4) -1, Moby (0) +1, a cheat skin +4.
inline int eventAnchorDraws(const EventAnchor& a, int humanBase, bool cheat) {
    const int character = humanBase == 4 ? -1 : humanBase == 0 ? 1 : 0;
    return a.base + character + (cheat ? 4 : 0) + a.start;
}

// ---- Finish estimates and rankings ----------------------------------------------------------------------------------
// 0x122D78 as web/ai-race.js estimateFinishTicks and web/rival-mode.js estimateTicks have it (an average that is not >=
// the floor, NaN included, takes the floor). Returns the JS number (NaN / infinities kept).
inline double estimateFinishTicksRace(double raceTicks, double origin, double remaining, double slot) {
    float average = (float(origin) - float(remaining)) / float(raceTicks);
    const float floor = float(30 - slot);
    if (!(average >= floor)) {
        average = floor;
    }
    return raceTicks + std::trunc(double(float(remaining) / average));
}
// 0x122D78 as web/career.js estimateFinishTicks has it (only an average below the floor takes it; NaN stays).
inline double estimateFinishTicksCareer(double raceTicks, double origin, double remaining, double place, double minimumSpeed) {
    float speed = (float(origin) - float(remaining)) / float(raceTicks);
    const float floor = float(minimumSpeed - place);
    if (speed < floor) {
        speed = floor;
    }
    return raceTicks + std::trunc(double(float(remaining) / speed));
}
// 0x122E50: the points estimate of a rider still on course (web/rival-mode.js estimatePoints, career.js opponentEstimate).
inline int32_t estimatePoints(double score, double remaining, double origin) {
    const int32_t s1 = jsToInt32(double(jsToInt32(score)) + 1);
    const float rem = float(remaining);
    float done = float(origin) - rem;
    if (done < 1) {
        done = 1;
    }
    float left = rem - 1000;
    if (left < 0) {
        left = 0;
    }
    const float v = left * (float(s1) / done);
    double add = 0;
    if (v >= 2147483648.0f) {
        add = 0x7fffffff;
    } else if (v <= -2147483648.0f) {
        add = -2147483648.0;
    } else {
        add = std::trunc(double(v));
    }
    return jsToInt32(double(s1) + add);
}
// 0x238BF8 / 0x238B70 as web/career.js rankEntries: places by value (ascending or descending), ties by slot.
// The values must not be NaN (the JS comparator is then no order; web/core-rules.js keeps those in JS).
inline std::vector<int> rankEntries(const double* values, unsigned n, bool ascending) {
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        const double d = ascending ? values[a] - values[b] : values[b] - values[a];
        if (d != 0 && !std::isnan(d)) {
            return d < 0;
        }
        return a < b;
    });
    std::vector<int> rank(n);
    for (unsigned r = 0; r < n; ++r) {
        rank[order[r]] = int(r);
    }
    return rank;
}
// 0x238BF8 (times ascending, unsigned) / 0x238B70 (scores descending, signed) as web/rival-mode.js rankBy.
inline std::vector<int> rankBy(const double* values, unsigned n, bool ascending) {
    std::vector<double> keys(n);
    for (unsigned k = 0; k < n; ++k) {
        keys[k] = ascending ? double(jsToUint32(values[k])) : double(jsToInt32(values[k]));
    }
    return rankEntries(keys.data(), n, ascending);
}
// 0x238BF8 as web/ai-race.js orderByTime: the slots ordered by time (unsigned ascending), ties by slot.
inline std::vector<int> orderByTime(const double* times, unsigned n) {
    std::vector<int> order(n);
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
        const uint32_t ta = jsToUint32(times[a]);
        const uint32_t tb = jsToUint32(times[b]);
        if (ta != tb) {
            return ta < tb;
        }
        return a < b;
    });
    return order;
}

// ---- Medals, records, difficulty (web/career.js) ------------------------------------------------------------
// Medals: -1 none, 0 platinum, 1 gold, 2 silver, 3 bronze. Modes: 0 race, 1..3 freestyle, 4 Rival Time, 5 Rival Points.
// 0x155328: 1st gold, 2nd silver, 3rd bronze; the rival challenges give gold to the winner only.
inline int placementMedal(int mode, int rank) {
    if (mode == 4 || mode == 5) {
        return rank == 0 ? 1 : -1;
    }
    return rank < 3 ? 1 + rank : -1;
}
inline bool isTimed(int mode) {
    return mode == 0 || mode == 4 || (mode >= 6 && mode <= 8);
}
// 1589B0 / 158A50: the first medal of the row [platinum, gold, silver, bronze] the count reaches, else -1.
inline int exploreMedal(double count, const double* row, unsigned n) {
    for (unsigned i = 0; i < n; ++i) {
        if (count >= row[i]) {
            return int(i);
        }
    }
    return -1;
}
// 0x147D20 / 0x147E18: adaptive difficulty (profile character +0x280 / +0x282 race, +0x284 / +0x286 freestyle).
inline void adaptLevel(int& level, int& counter, bool up) {
    if (up) {
        counter++;
        if (counter >= 2) {
            if (level < 2) {
                level++;
            }
            counter = 0;
        }
    } else {
        counter--;
        if (counter < -1) {
            if (level != 0) {
                level--;
            }
            counter = 0;
        }
    }
}
// 0x1453D0: one computer rider's posted freestyle score from its table score; random: the draw 0x237CD8 gave.
inline double aiFreestyleScore(double tableScore, int aiIndex, int level, double level0Scale, double level2Scale, double jitter,
                               uint32_t random) {
    float scale = 1;
    if (aiIndex != 0) {
        if (level == 0) {
            scale = float(level0Scale);
        } else if (level == 2) {
            scale = float(level2Scale);
        }
    }
    double s = std::trunc(double(float(tableScore) * scale));
    const double r = double(random % 200u) - 100;
    s += std::trunc(double(float(s * r) * float(jitter)));
    return s - std::fmod(s, 20.0);
}
// 0x204D40..0x204F9C: the All Mountain percentage, 100 only when exactly complete, else truncated and at most 99.
inline double mountainPercent(double golds, double rivals, double challenges, double challengeTotal, double collected, double collectTotal,
                              double highlights) {
    const float a = float(golds) * float(1.0 / 14);
    const float b = float(rivals) * float(1.0 / 12);
    const float c = float(challenges) / float(challengeTotal);
    const float d = float(collected) / float(collectTotal);
    const float e = float(highlights) * float(1.0 / 24);
    const float sum = (((a + b) + c) + d) + e;
    const float pct = sum * 20;
    if (pct == 100) {
        return 100;
    }
    return jsMin(99, std::trunc(double(pct)));
}
// 0x1456A0's threshold: T x 60 ticks (race / Rival Time, timed) or T x 100 points; Rival Points reads row course + 3.
// platinum: the second column of rules.platinum by row. Returns 0 none, 1 ticks, 2 score; amount in `value`.
inline int platinumThreshold(int mode, int course, const double* platinum, unsigned rows, double& value) {
    if (mode >= 6) {
        return 0;
    }
    const int row = mode == 5 ? course + 3 : course;
    if (row < 0 || unsigned(row) >= rows) {
        return -1;
    }
    if (mode == 5) {
        value = platinum[row] * 100;
        return 2;
    }
    if (isTimed(mode)) {
        value = platinum[row] * 60;
        return 1;
    }
    value = platinum[row] * 100;
    return 2;
}
// 0x14AB68: the record slot of an event (26 = none); modes 6..11 slots 0..5, otherwise table 0x45A2F8's row of the course.
inline int recordSlot(int mode, int course, const int* row) {
    if (mode >= 6 && mode <= 11) {
        return mode - 6;
    }
    if (course < 0 || course >= 17 || !row) {
        return 26;
    }
    if (row[0] == mode) {
        return row[1];
    }
    if (row[2] == mode) {
        return row[3];
    }
    return 26;
}
// 0x145750 as web/rival-mode.js rivalCharacter: Peak 1 Mac / Griff, Peak 2 Nate / Zoe, Peak 3 Psymon / Elise (peaks 1..3).
inline int rivalCharacter(int peak, int human) {
    static constexpr int rivals[3][2] = {{3, 5}, {7, 4}, {8, 6}};
    static constexpr int own[3] = {3, 7, 8};
    return rivals[peak - 1][human == own[peak - 1] ? 1 : 0];
}

}  // namespace ssx::rules
