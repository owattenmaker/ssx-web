// native/frontend/now_playing.hpp against web/now-playing.js (native/tests/now-playing-cases.mjs): the popup's state, box and lines
// after each 60 Hz tick, for every playlist song.
//   now_playing_test CASES.json OUT.json
#include "json.hpp"
#include "now_playing.hpp"
#include <cstdio>
#include <fstream>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: now_playing_test CASES.json OUT.json\n");
        return 2;
    }
    std::ifstream in(argv[1]);
    const auto cases = nlohmann::json::parse(in);
    const auto fe = ssx_native::hud::loadGlyphs(cases["fe"], true);
    const auto hudFont = ssx_native::hud::loadGlyphs(cases["hud"], true);
    nlohmann::json out = nlohmann::json::array();
    for (const auto& c : cases["songs"]) {
        ssx_native::hud::NowPlayingHud np;
        np.fe = &fe;
        np.hudFont = &hudFont;
        np.show({c["title"].get<std::string>(), c["artist"].get<std::string>(), c["album"].get<std::string>()});
        nlohmann::json steps = nlohmann::json::array();
        for (int k = 0; k < c["ticks"].get<int>(); ++k) {
            np.tick();
            const auto v = np.view();
            nlohmann::json lines = nlohmann::json::array();
            for (const auto& [text, w, h] : v.lines) {
                lines.push_back({text, w, h});
            }
            steps.push_back({v.active, v.time, v.slide, v.alpha, v.x, v.y, v.w, v.h, lines});
        }
        out.push_back(steps);
    }
    std::ofstream(argv[2]) << out.dump();
    return 0;
}
