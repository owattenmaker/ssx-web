// The event and career rules (engine/game_rules.hpp) for the page (docs/core-migration.md slice M1, pv coreRules).
// One export, rules_eval(fn, io, n): the inputs are n doubles at io, the outputs are written over them from io[0], and the
// return value is the output count (-1: inputs this port does not take, the caller keeps its JS version).
// web/core-rules.js has the JS side; web/test-core-rules.mjs checks every function against the JS versions.
// No state: nothing here is per rider.
#include "game_rules.hpp"
// M5c (pv coreCareerResults): the career's result kernel
#include "career_rules.hpp"
#include <emscripten/emscripten.h>
#include <cstdint>
#include <string>
#include <vector>

namespace {
using namespace ssx::rules;
// Function ids (web/core-rules.js RULE).
enum RuleFunction : int {
    kSeededWords = 1,
    kNextWord = 2,
    kWordsAfterDraws = 3,
    kClockSeed = 4,
    kPeakRival = 5,
    kBuildRoster = 6,
    kRoundEntries = 7,
    kFreestyleRoster = 8,
    kRelationshipAge = 9,
    kRelationshipEvent = 10,
    kNpcRoundRole = 11,
    kNpcDifficulty = 12,
    kEventAnchor = 13,
    kEstimateRace = 14,
    kEstimateCareer = 15,
    kEstimatePoints = 16,
    kRankEntries = 17,
    kRankBy = 18,
    kOrderByTime = 19,
    kPlacementMedal = 20,
    kExploreMedal = 21,
    kAdaptLevel = 22,
    kAiFreestyleScore = 23,
    kMountainPercent = 24,
    kPlatinumThreshold = 25,
    kRecordSlot = 26,
    kRivalCharacter = 27,
    kRaceRound = 28,
    kFreestyleRound = 29,
    kEventMedal = 30,
    kRecordInsert = 31,
    kPeakResult = 32,
};
Words readWords(const double* io) {
    Words w{};
    for (unsigned k = 0; k < 6; ++k) {
        w[k] = uint32_t(io[k]);
    }
    return w;
}
void writeWords(double* io, const Words& w) {
    for (unsigned k = 0; k < 6; ++k) {
        io[k] = double(w[k]);
    }
}
template <class T>
int writeList(double* io, const T& list) {
    for (unsigned k = 0; k < list.size(); ++k) {
        io[k] = double(list[k]);
    }
    return int(list.size());
}
}  // namespace

extern "C" EMSCRIPTEN_KEEPALIVE int rules_eval(int fn, double* io, int n) {
    switch (fn) {
        case kSeededWords: {
            // in: seed; out: six words
            writeWords(io, seededWords(uint32_t(io[0])));
            return 6;
        }
        case kNextWord: {
            // in: six words; out: the draw, then the six words after it
            Words w = readWords(io);
            const uint32_t value = nextWord(w);
            io[0] = double(value);
            for (unsigned k = 0; k < 6; ++k) {
                io[1 + k] = double(w[k]);
            }
            return 7;
        }
        case kWordsAfterDraws: {
            // in: seed, draws; out: six words
            writeWords(io, wordsAfterDraws(uint32_t(io[0]), uint32_t(io[1])));
            return 6;
        }
        case kClockSeed: {
            // in: seconds, minutes, hours, day of month, month 1..12, full year
            io[0] = double(clockSeed(int(io[0]), int(io[1]), int(io[2]), int(io[3]), int(io[4]), int(io[5])));
            return 1;
        }
        case kPeakRival: {
            io[0] = peakRival(int(io[0]), int(io[1]));
            return 1;
        }
        case kBuildRoster: {
            // in: six words, human, peak; out: ten entries, then the six words after the build
            Words w = readWords(io);
            const auto entries = buildRosterFrom(w, int(io[6]), int(io[7]));
            writeList(io, entries);
            for (unsigned k = 0; k < 6; ++k) {
                io[10 + k] = double(w[k]);
            }
            return 16;
        }
        case kRoundEntries: {
            // in: ten entries, round, career, rival, previous count, previous values (-1 the human); out: five entries
            std::array<int, 10> entries{};
            for (unsigned k = 0; k < 10; ++k) {
                entries[k] = int(io[k]);
            }
            const unsigned count = unsigned(io[13]);
            std::vector<int> previous(count);
            for (unsigned k = 0; k < count; ++k) {
                previous[k] = int(io[14 + k]);
            }
            const auto out = roundEntries(entries, int(io[10]), io[11] != 0, int(io[12]), previous.data(), count);
            return writeList(io, out);
        }
        case kFreestyleRoster: {
            // in: six words, human, rival; out: count, the entries, then the six words after the shuffle
            Words w = readWords(io);
            const auto e = freestyleRoster(w, int(io[6]), int(io[7]));
            io[0] = double(e.size());
            for (unsigned k = 0; k < e.size(); ++k) {
                io[1 + k] = double(e[k]);
            }
            for (unsigned k = 0; k < 6; ++k) {
                io[1 + e.size() + k] = double(w[k]);
            }
            return int(1 + e.size() + 6);
        }
        case kRelationshipAge: {
            // in: kind, score; out: kind, level, score
            const Relationship r = relationshipAge(int(io[0]), int(io[1]));
            io[0] = r.kind;
            io[1] = r.level;
            io[2] = r.score;
            return 3;
        }
        case kRelationshipEvent: {
            // in: kind, score, event
            const Relationship r = relationshipEvent(int(io[0]), int(io[1]), int(io[2]));
            io[0] = r.kind;
            io[1] = r.level;
            io[2] = r.score;
            return 3;
        }
        case kNpcRoundRole: {
            io[0] = npcRoundRole(int(io[0]), int(io[1]));
            return 1;
        }
        case kNpcDifficulty: {
            // in: slot, level, course index; out: df8, dfc
            Difficulty d{};
            if (!npcDifficulty(int(io[0]), int(io[1]), int(io[2]), d)) {
                return -1;
            }
            io[0] = double(d.df8);
            io[1] = double(d.dfc);
            return 2;
        }
        case kEventAnchor: {
            // in: four course-code characters, human base, cheat (0 / 1); out: tick, draws, six words (none: 0)
            char code[5] = {char(io[0]), char(io[1]), char(io[2]), char(io[3]), 0};
            const EventAnchor* a = eventAnchor(code);
            if (!a) {
                return 0;
            }
            const int draws = eventAnchorDraws(*a, int(io[4]), io[5] != 0);
            io[0] = a->tick;
            io[1] = draws;
            const Words w = wordsAfterDraws(0, uint32_t(draws));
            for (unsigned k = 0; k < 6; ++k) {
                io[2 + k] = double(w[k]);
            }
            return 8;
        }
        case kEstimateRace: {
            // in: race ticks, origin, remaining, slot
            io[0] = estimateFinishTicksRace(io[0], io[1], io[2], io[3]);
            return 1;
        }
        case kEstimateCareer: {
            // in: race ticks, origin, remaining, place, minimum speed
            io[0] = estimateFinishTicksCareer(io[0], io[1], io[2], io[3], io[4]);
            return 1;
        }
        case kEstimatePoints: {
            // in: score, remaining, origin
            io[0] = estimatePoints(io[0], io[1], io[2]);
            return 1;
        }
        case kRankEntries:
        case kRankBy: {
            // in: ascending, count, values; out: the places by slot
            const bool ascending = io[0] != 0;
            const unsigned count = unsigned(io[1]);
            std::vector<double> values(io + 2, io + 2 + count);
            const auto rank = fn == kRankEntries ? rankEntries(values.data(), count, ascending) : rankBy(values.data(), count, ascending);
            return writeList(io, rank);
        }
        case kOrderByTime: {
            // in: count, times; out: the slots in order
            const unsigned count = unsigned(io[0]);
            std::vector<double> times(io + 1, io + 1 + count);
            return writeList(io, orderByTime(times.data(), count));
        }
        case kPlacementMedal: {
            io[0] = placementMedal(int(io[0]), int(io[1]));
            return 1;
        }
        case kExploreMedal: {
            // in: count, row length, row
            const unsigned length = unsigned(io[1]);
            std::vector<double> row(io + 2, io + 2 + length);
            io[0] = exploreMedal(io[0], row.data(), length);
            return 1;
        }
        case kAdaptLevel: {
            // in: level, counter, up; out: level, counter
            int level = int(io[0]);
            int counter = int(io[1]);
            adaptLevel(level, counter, io[2] != 0);
            io[0] = level;
            io[1] = counter;
            return 2;
        }
        case kAiFreestyleScore: {
            // in: table score, AI index, level, level-0 scale, level-2 scale, jitter, the draw
            io[0] = aiFreestyleScore(io[0], int(io[1]), int(io[2]), io[3], io[4], io[5], uint32_t(io[6]));
            return 1;
        }
        case kMountainPercent: {
            io[0] = mountainPercent(io[0], io[1], io[2], io[3], io[4], io[5], io[6]);
            return 1;
        }
        case kPlatinumThreshold: {
            // in: mode, course, row count, the platinum column; out: kind (0 none, 1 ticks, 2 score), amount
            const unsigned rows = unsigned(io[2]);
            std::vector<double> column(io + 3, io + 3 + rows);
            double value = 0;
            const int kind = platinumThreshold(int(io[0]), int(io[1]), column.data(), rows, value);
            if (kind < 0) {
                return -1;
            }
            io[0] = kind;
            io[1] = value;
            return 2;
        }
        case kRecordSlot: {
            // in: mode, course, has row, row (four words)
            const int row[4] = {int(io[3]), int(io[4]), int(io[5]), int(io[6])};
            io[0] = recordSlot(int(io[0]), int(io[1]), io[2] != 0 ? row : nullptr);
            return 1;
        }
        case kRivalCharacter: {
            const int peak = int(io[0]);
            if (peak < 1 || peak > 3) {
                return -1;
            }
            io[0] = rivalCharacter(peak, int(io[1]));
            return 1;
        }
        case kRaceRound: {
            // in: round, career, level, counter, DNF ticks, minimum speed, margins (3), race ticks, human, count, then per rider
            // dnf, finished, finish ticks, remaining, place, origin; out: place, advance, round, level, counter, message, complete,
            // count, times, ranks
            const unsigned count = unsigned(io[11]);
            if (count < 1 || count > 8 || n != int(12 + 6 * count)) {
                return -1;
            }
            std::vector<RaceRider> riders(count);
            for (unsigned k = 0; k < count; ++k) {
                const double* s = io + 12 + 6 * k;
                riders[k] = {s[0] != 0, s[1] != 0, s[2], s[3], s[4], s[5]};
            }
            const int round = int(io[0]);
            const int human = int(io[10]);
            if (round < 1 || round > 3 || human < 0 || unsigned(human) >= count) {
                return -1;
            }
            RaceRound r;
            const double margin[3] = {io[6], io[7], io[8]};
            if (!raceRound(round, io[1] != 0, int(io[2]), int(io[3]), io[4], io[5], margin, io[9], riders, human, r)) {
                return -1;
            }
            const double head[8] = {double(r.place), r.advance ? 1.0 : 0.0, double(r.round), double(r.level), double(r.counter),
                                    double(r.message), r.complete ? 1.0 : 0.0, double(count)};
            for (unsigned k = 0; k < 8; ++k) {
                io[k] = head[k];
            }
            for (unsigned k = 0; k < count; ++k) {
                io[8 + k] = r.times[k];
                io[8 + count + k] = double(r.rank[k]);
            }
            return int(8 + 2 * count);
        }
        case kFreestyleRound: {
            // in: round, career, level, counter, score, the player's three, has opponent, its three, update, finished, its score,
            // its estimate, posted count, three each; out: place, advance, round, level, counter, message, complete, the player's
            // three, the opponent's three, count, totals, shown, ranks
            const unsigned posted = unsigned(io[16]);
            if (posted > 14 || n != int(17 + 3 * posted)) {
                return -1;
            }
            const int round = int(io[0]);
            if (round < 1 || round > 3) {
                return -1;
            }
            std::vector<std::array<double, 3>> field(posted);
            for (unsigned k = 0; k < posted; ++k) {
                field[k] = {io[17 + 3 * k], io[18 + 3 * k], io[19 + 3 * k]};
            }
            FreestyleRound r;
            freestyleRound(round, io[1] != 0, int(io[2]), int(io[3]), io[4], {io[5], io[6], io[7]}, io[8] != 0, {io[9], io[10], io[11]},
                           io[12] != 0, io[13] != 0, io[14], io[15], field, r);
            const unsigned count = unsigned(r.totals.size());
            const double head[14] = {double(r.place), r.advance ? 1.0 : 0.0, double(r.round), double(r.level), double(r.counter),
                                     double(r.message), r.complete ? 1.0 : 0.0, r.player[0], r.player[1], r.player[2],
                                     r.opponent[0], r.opponent[1], r.opponent[2], double(count)};
            for (unsigned k = 0; k < 14; ++k) {
                io[k] = head[k];
            }
            for (unsigned k = 0; k < count; ++k) {
                io[14 + k] = r.totals[k];
                io[14 + count + k] = r.shown[k];
                io[14 + 2 * count + k] = double(r.rank[k]);
            }
            return int(14 + 3 * count);
        }
        case kEventMedal: {
            // in: mode, place, career, threshold kind, amount, ticks, score
            io[0] = eventMedal(int(io[0]), int(io[1]), io[2] != 0, int(io[3]), io[4], io[5], io[6]);
            return 1;
        }
        case kRecordInsert: {
            // in: timed, value, count, the entries' values; out: index, the stored value
            const unsigned count = unsigned(io[2]);
            if (count > 16 || n != int(3 + count)) {
                return -1;
            }
            double secs = 0;
            std::vector<double> entries(io + 3, io + 3 + count);
            io[0] = recordInsert(io[0] != 0, io[1], entries.data(), count, secs);
            io[1] = secs;
            return 2;
        }
        case kPeakResult: {
            // in: time challenge, target, tier-3 limit, tier-2 limit, ticks, score, dnf; out: place, fail, value
            const PeakResult r = peakResult(io[0] != 0, io[1], io[2], io[3], io[4], io[5], io[6] != 0);
            io[0] = r.place;
            io[1] = r.fail ? 1 : 0;
            io[2] = r.value;
            return 3;
        }
        default:
            (void)n;
            return -1;
    }
}
