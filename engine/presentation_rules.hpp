#pragma once
// The session's presentation generator 0x4FF018 as the page models it (web/lineup.js; docs/core-migration.md M5): the boot seeds it
// from the clock (0x31AE94) and draws once; the career's reward picks and messages draw from it (0x3177F0); each event load's roster
// seed is its draw 129 after the previous event's per-frame draws (an estimate from lineups.json presentation_model, noted when the
// race is over). The same results as the JS version bit for bit (web/test-rules-module.mjs).
#include "game_rules.hpp"
#include <cmath>
#include <cstdint>

namespace ssx::rules {

// JS Math.round: the closest integer, ties toward +infinity (NaN stays NaN).
inline double jsRound(double x) {
    if (!std::isfinite(x)) {
        return x;
    }
    const double r = std::floor(x);
    return x - r >= 0.5 ? r + 1 : r;
}
// Math.max(a, b) of the JS (NaN when either is NaN).
inline double jsMax(double a, double b) {
    if (std::isnan(a) || std::isnan(b)) {
        return NAN;
    }
    return a > b ? a : b;
}

struct PresentationGenerator {
    bool booted = false;
    Words words{};
    // the previous events' per-frame draws not taken yet (a JS number: NaN stays NaN, as the page's sum does)
    double pendingDraws = 0;
    // web/lineup.js bootPresentation: seeded from the boot seed, then one draw (before the menus)
    void boot(uint32_t seed) {
        if (booted) {
            return;
        }
        words = seededWords(seed);
        nextWord(words);
        booted = true;
    }
    // presentationDraw (0x3177F0)
    uint32_t draw() {
        return nextWord(words);
    }
    // noteEventDraws: a raced event's per-frame draws by the measured model (after_seed + per_tick x the ticks after the anchor)
    void noteEventDraws(double afterSeed, double perTick, double anchorTick, double ticks) {
        pendingDraws += jsMax(0, jsRound(afterSeed + perTick * jsMax(0, ticks - anchorTick)));
    }
    // nextRosterSeed's draws: the pending per-frame draws, then the load's 128 draws and the roster seed (the 129th)
    uint32_t rosterSeed() {
        for (; pendingDraws > 0; pendingDraws--) {
            nextWord(words);
        }
        uint32_t seed = 0;
        for (int n = 1; n < 130; ++n) {
            seed = nextWord(words);
        }
        return seed;
    }
};

}  // namespace ssx::rules
