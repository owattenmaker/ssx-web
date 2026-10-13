// native/frontend/pad_input.hpp against web/pad-input.js (native/tests/pad-cases.mjs): every frame's 24 channels bit for bit.
//   pad_input_test CASES.json
#include "json.hpp"
#include "pad_input.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: pad_input_test CASES.json\n");
        return 2;
    }
    std::ifstream in(argv[1]);
    const auto cases = nlohmann::json::parse(in);
    unsigned frames = 0;
    for (size_t c = 0; c < cases.size(); ++c) {
        ssx_native::KeyboardContext kb;
        kb.mode = cases[c]["mode"] == "Simple" ? ssx_native::KeyboardContext::Mode::Simple : ssx_native::KeyboardContext::Mode::Classic;
        for (const auto& f : cases[c]["frames"]) {
            std::set<std::string> held;
            for (const auto& k : f["keys"]) {
                held.insert(k.get<std::string>());
            }
            kb.air = f["air"].get<bool>();
            ssx_native::StandardPad pad;
            const bool usePad = !f["pad"].is_null();
            if (usePad) {
                pad.connected = true;
                for (size_t b = 0; b < 17; ++b) {
                    pad.buttons[b].pressed = f["pad"]["buttons"][b]["pressed"].get<bool>();
                    pad.buttons[b].value = f["pad"]["buttons"][b]["value"].get<double>();
                }
                for (size_t a = 0; a < 4; ++a) {
                    pad.axes[a] = f["pad"]["axes"][a].get<double>();
                }
            }
            const auto out = ssx_native::buildPad([&](const std::string& code) { return held.count(code) > 0; }, usePad ? &pad : nullptr, &kb);
            for (size_t i = 0; i < 24; ++i) {
                uint32_t got;
                std::memcpy(&got, &out[i], 4);
                const uint32_t want = f["out"][i].get<uint32_t>();
                if (got != want) {
                    std::printf("case %zu frame %u channel %zu: native %08x, web %08x\n", c, frames, i, got, want);
                    return 1;
                }
            }
            ++frames;
        }
    }
    std::printf("pad input: %u frames identical to web/pad-input.js\n", frames);
    return 0;
}
