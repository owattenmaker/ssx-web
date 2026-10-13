#pragma once
// The career's result kernel (docs/core-migration.md M5c, pv coreCareerResults): a round's result as web/career.js raceResult /
// freestyleResult / completeEvent / addRecord and web/peak-run.js peakResult compute it, without the save (the page applies the
// outputs to its event and rider records, and keeps the persisting, the records list and the award cascade). Same results as the JS
// bit for bit (web/test-core-rules.mjs); inputs the JS would coerce oddly (NaN times, missing rows) stay in the JS.
#include "game_rules.hpp"
#include <cmath>
#include <cstdint>
#include <vector>

namespace ssx::rules {

// raceResult's message (kT_ string ids of the page; 0 none)
enum RaceMessage : int {
    kRaceMessageNone = 0,
    kRaceMessageGoToSemi = 1,       // kT_OVRCongratGoToSemi
    kRaceMessageObjectives7 = 2,    // kT_OVRRaceObjectives7
    kRaceMessageSorrySemi = 3,      // kT_OVRCMNSorryGetThirdSemi
    kRaceMessageSorryThird = 4,     // kT_OVRCMNSorryGetThird
};
struct RaceRider {
    bool dnf = false;
    bool finished = false;  // finishTicks not null
    double finishTicks = 0;
    double remaining = 0;
    double place = 0;   // s.place ?? 0
    double origin = 0;  // s.origin ?? the round's origin
};
struct RaceRound {
    std::vector<double> times;
    std::vector<int> rank;
    int place = 0;
    bool advance = false;
    int round = 1;  // the event's round after this one
    int level = 1;
    int counter = 0;
    int message = kRaceMessageNone;
    bool complete = false;  // round 3: completeEvent follows
};
// 0x23A760 (web/career.js raceResult): unfinished riders get the 0x122D78 estimate at the human's finish, DNF riders the DNF ticks.
// Returns false when a time is NaN (the JS ranks those by its comparator; the caller keeps its version).
inline bool raceRound(int round, bool career, int level, int counter, double dnfTicks, double minimumSpeed, const double* margin,
                      double raceTicks, const std::vector<RaceRider>& riders, int human, RaceRound& out) {
    const unsigned n = unsigned(riders.size());
    out.times.assign(n, 0);
    for (unsigned k = 0; k < n; ++k) {
        const RaceRider& s = riders[k];
        if (s.dnf) {
            out.times[k] = dnfTicks;
        } else if (s.finished) {
            out.times[k] = s.finishTicks;
        } else {
            out.times[k] = estimateFinishTicksCareer(raceTicks, s.origin, s.remaining, s.place, minimumSpeed);
        }
        if (std::isnan(out.times[k])) {
            return false;
        }
    }
    out.rank = rankEntries(out.times.data(), n, true);
    out.place = out.rank[unsigned(human)];
    out.round = round;
    out.level = level;
    out.counter = counter;
    if (out.place < 3) {
        out.advance = round < 3;
        // 0x23AA24: a win moves the level up only by the margin between result slots 1 and 0
        if (career && out.place == 0 && n > 1 && out.times[1] - out.times[0] >= margin[round - 1]) {
            adaptLevel(out.level, out.counter, true);
        }
        out.message = round == 1 ? kRaceMessageGoToSemi : round == 2 ? kRaceMessageObjectives7 : kRaceMessageNone;
        if (round < 3) {
            out.round = round + 1;
        }
    } else {
        out.advance = false;
        if (career && (round == 1 || (round == 2 && out.place >= 4) || (round == 3 && out.place == 5))) {
            adaptLevel(out.level, out.counter, false);
        }
        out.message = round == 1 ? kRaceMessageSorrySemi : round == 2 ? kRaceMessageSorryThird : kRaceMessageNone;
    }
    out.complete = round == 3;
    return true;
}

// freestyleResult's message (0 none; OVAMER 0x0812F644 "made it to the final round"; kT_OVRCMNSorryGetThird)
enum FreestyleMessage : int {
    kFreestyleMessageNone = 0,
    kFreestyleMessageFinal = 1,
    kFreestyleMessageSorryThird = 2,
};
struct FreestyleRound {
    std::vector<double> totals;
    std::vector<double> shown;
    std::vector<int> rank;
    int place = 0;
    bool advance = false;
    int round = 1;
    int level = 1;
    int counter = 0;
    int message = kFreestyleMessageNone;
    bool complete = false;
    std::array<double, 3> player{};
    std::array<double, 3> opponent{};
};
// 0x239230 (web/career.js freestyleResult). field: the opponent first when there is one, then the posted riders, three scores each.
// opponentUpdate: the live opponent's result was passed (finished: its score | 0; else in the final its estimate | 0).
inline void freestyleRound(int round, bool career, int level, int counter, double score, const std::array<double, 3>& player,
                           bool hasOpponent, const std::array<double, 3>& opponent, bool opponentUpdate, bool opponentFinished,
                           double opponentScore, double opponentEstimate, const std::vector<std::array<double, 3>>& posted,
                           FreestyleRound& out) {
    out.player = player;
    out.player[unsigned(round - 1)] = score;
    out.opponent = opponent;
    if (hasOpponent && opponentUpdate) {
        if (opponentFinished) {
            out.opponent[unsigned(round - 1)] = jsToInt32(opponentScore);
        } else if (round == 3) {
            out.opponent[2] = jsToInt32(opponentEstimate);
        }
    }
    std::vector<std::array<double, 3>> field;
    if (hasOpponent) {
        field.push_back(out.opponent);
    }
    field.insert(field.end(), posted.begin(), posted.end());
    out.totals.clear();
    out.shown.clear();
    if (round == 1) {
        out.totals.push_back(score + 0);
        out.shown.push_back(score);
        for (const auto& a : field) {
            out.totals.push_back(a[0] + a[1]);
            out.shown.push_back(a[0]);
        }
    } else if (round == 2) {
        out.totals.push_back(out.player[0] + score);
        out.shown.push_back(score);
        for (const auto& a : field) {
            out.totals.push_back(a[0] + a[1]);
            out.shown.push_back(a[1]);
        }
    } else {
        out.totals.push_back(score);
        for (const auto& a : field) {
            out.totals.push_back(a[2]);
        }
        out.shown = out.totals;
    }
    out.rank = rankEntries(out.totals.data(), unsigned(out.totals.size()), false);
    out.place = out.rank[0];
    out.level = level;
    out.counter = counter;
    out.round = round;
    out.complete = false;
    if (round == 1) {
        out.advance = true;
        out.round = out.place < 3 ? 3 : 2;
        out.message = out.place < 3 ? kFreestyleMessageFinal : kFreestyleMessageNone;
    } else if (round == 2) {
        if (out.place < 3) {
            out.round = 3;
            out.advance = true;
            out.message = kFreestyleMessageFinal;
            if (career && out.place == 0) {
                adaptLevel(out.level, out.counter, true);
            }
        } else {
            out.round = 1;
            out.advance = false;
            out.message = kFreestyleMessageSorryThird;
        }
    } else {
        out.advance = false;
        out.message = kFreestyleMessageNone;
        if (career) {
            adaptLevel(out.level, out.counter, out.place < 3);
        }
        out.complete = true;
    }
}

// completeEvent's medal (+0x9C, 0x154EE8): the placement medal, platinum when a career event beats the course's threshold.
// threshold: 0 none, 1 ticks (at most), 2 score (at least).
inline int eventMedal(int mode, int place, bool career, int threshold, double amount, double ticks, double score) {
    int medal = placementMedal(mode, place);
    if (medal != -1 && career && threshold != 0) {
        if (threshold == 1 ? ticks <= amount : score >= amount) {
            medal = 0;
        }
    }
    return medal;
}

// addRecord's entry (0x154CD8..0x154D58): a time in whole seconds (f32 ticks x f32 1/60, truncated), then the first entry the run is
// not strictly worse than. Returns the index (-1: not in the top five); secs is the stored value.
inline int recordInsert(bool timed, double value, const double* entries, unsigned n, double& secs) {
    secs = timed ? std::trunc(double(float(float(value) * 0.016666668f))) : value;
    for (unsigned k = 0; k < n; ++k) {
        const bool at = timed ? !(entries[k] < secs) : !(value < entries[k]);
        if (at) {
            return int(k);
        }
    }
    return -1;
}

// 23B468 / 23C2D8 (web/peak-run.js peakResult): +0x480 (time up / give up) fails; the time challenge fails above the tier's limit,
// the points challenge below its target; else place 0 / 1 / 2 against the tier-3 and tier-2 rows (seconds / hundreds of points).
struct PeakResult {
    int place = 3;
    bool fail = true;
    double value = 0;
};
inline PeakResult peakResult(bool timeChallenge, double target, double tier3, double tier2, double ticks, double score, bool dnf) {
    PeakResult r;
    if (timeChallenge) {
        r.value = dnf ? 360000 : ticks;
        if (dnf || ticks > target) {
            return r;
        }
        r.fail = false;
        r.place = ticks <= tier3 * 60 ? 0 : ticks <= tier2 * 60 ? 1 : 2;
        return r;
    }
    r.value = dnf ? 0 : std::trunc(score);
    if (dnf || r.value < target) {
        return r;
    }
    r.fail = false;
    r.place = r.value >= tier3 * 100 ? 0 : r.value >= tier2 * 100 ? 1 : 2;
    return r;
}

}  // namespace ssx::rules
