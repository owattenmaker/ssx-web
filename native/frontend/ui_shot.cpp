// A front-end screen drawn headless at a given state (docs/native.md "N3", FE parity): the UI canvas over a black 640 x 480 frame,
// as the stage composites #ui-bg, the hidden #game and #ui. native/tools/ui-shot.mjs draws the page's at the same state.
//   ssx3_ui_shot ASSETS_DIR OUT.png --screen loading [--frame F] [--percent P] [--black B] [--hint-counter N] [--device keyboard|gamepad]
//                [--keyboard Classic|Simple] [--canvas-log OUT.json]
//   ssx3_ui_shot ASSETS_DIR OUT.png --screen single [--ticks T] [--rider R] [--keys K1,K2,...] [--frame F]
//                a Single Event race's results (web/career-ui.js finish) for the QA standings below with the human's finish at T ticks,
//                on fresh records; then the menu keys (KeyboardEvent.code) once the screen takes input
#include "gpu.hpp"
#include "hud_canvas.hpp"
#include "hud_widgets.hpp"
#include "image_io.hpp"
#include "json.hpp"
#include "loading_screen.hpp"
#include "results_lui.hpp"
#include "single_event.hpp"
#include "trick_hud.hpp"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace ssx_native;

namespace {

std::string readText(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        throw std::runtime_error("cannot read " + path);
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::vector<std::string> positional;
        std::string screen = "loading";
        double frame = 120;
        int percent = 42;
        double black = 0;
        int hintCounter = 0;
        bool gamepad = false;
        std::string keyboard = "Classic";
        std::string canvasLog;
        double humanTicks = 12000;
        std::string rider = "zoe";
        std::string keyList;
        for (int k = 1; k < argc; ++k) {
            const std::string a = argv[k];
            auto next = [&] { return std::string(k + 1 < argc ? argv[++k] : ""); };
            if (a == "--screen") {
                screen = next();
            } else if (a == "--frame") {
                frame = std::stod(next());
            } else if (a == "--percent") {
                percent = std::stoi(next());
            } else if (a == "--black") {
                black = std::stod(next());
            } else if (a == "--hint-counter") {
                hintCounter = std::stoi(next());
            } else if (a == "--device") {
                gamepad = next() == "gamepad";
            } else if (a == "--keyboard") {
                keyboard = next();
            } else if (a == "--canvas-log") {
                canvasLog = next();
            } else if (a == "--ticks") {
                humanTicks = std::stod(next());
            } else if (a == "--rider") {
                rider = next();
            } else if (a == "--keys") {
                keyList = next();
            } else {
                positional.push_back(a);
            }
        }
        if (positional.size() != 2) {
            std::fprintf(stderr, "usage: ssx3_ui_shot ASSETS_DIR OUT.png --screen loading [--frame F] [--percent P] ...\n");
            return 2;
        }
        const std::string assets = positional[0];
        Gpu gpu;
        gpu.init();
        HudCanvas canvas;
        canvas.init(gpu);
        hud::HudImages ui;
        ui.canvas = &canvas;
        ui.dir = assets + "/UI";
        hud::HudImages pages;
        pages.canvas = &canvas;
        pages.dir = assets + "/LOADING";
        hud::UiText text;
        text.images = &ui;
        text.fonts["FEFONT"] = hud::loadGlyphs(nlohmann::json::parse(readText(assets + "/UI/FEFONT-glyphs.json")), false);
        text.fonts["HUDFONT"] = hud::loadGlyphs(nlohmann::json::parse(readText(assets + "/UI/HUDFONT-glyphs.json")), false);
        canvas.begin();
        canvas.logging = !canvasLog.empty();
        if (screen == "loading") {
            fe::LoadingScreen loading;
            loading.text = &text;
            loading.pages = &pages;
            loading.gamepad = gamepad;
            loading.keyboardMode = keyboard;
            if (!loading.load(nlohmann::json::parse(readText(assets + "/LOADING/loading.json")))) {
                throw std::runtime_error("no LOADING/loading.json");
            }
            loading.setHintCounter(hintCounter);
            loading.drawAt(canvas, frame, percent, black);
        } else if (screen == "results" || screen == "records" || screen == "racecard") {
            // the QA sample of native/tools/ui-shot.mjs (the same arguments)
            fe::ResultsLui rl;
            if (!rl.load(nlohmann::json::parse(readText(assets + "/UI/results-screens.json")), ui, text)) {
                throw std::runtime_error("no UI/results-screens.json");
            }
            rl.setKeyboard(!gamepad);
            if (screen == "results") {
                fe::ResultsLui::Results r;
                r.title = "Snow Jam";
                r.sub = "Single Event Results";
                r.heads = {"Rank", "Riders", "Time"};
                const char* names[6] = {"Zoe", "Mac", "Elise", "Psymon", "Kaori", "Moby"};
                const char* times[6] = {"03:54:12", "03:55:40", "03:57:02", "03:59:33", "04:01:18", "04:05:47"};
                for (int k = 0; k < 6; ++k) {
                    r.rows.push_back({k + 1, names[k], times[k], k == 0});
                }
                r.medal = fe::ResultsLui::Medal{0, 1};
                r.message = "Congratulations! You've received a Gold medal.";
                r.items = {"Continue", "Restart", "Replay", "Records", "Quit"};
                r.index = percent % 5;
                r.disabled = [](int k) { return k == 2; };
                rl.results(canvas, frame, r);
            } else if (screen == "records") {
                fe::ResultsLui::Records r;
                r.title = "Snow Jam";
                r.timed = true;
                const char* names[5] = {"PLAYER 1", "EA", "EA", "EA", "EA"};
                const char* riders[5] = {"Zoe", "Mac", "Elise", "Psymon", "Kaori"};
                const char* times[5] = {"03:54:12", "04:10:00", "04:20:00", "04:30:00", "04:40:00"};
                for (int k = 0; k < 5; ++k) {
                    fe::ResultsLui::Record rec;
                    rec.name = names[k];
                    rec.rider = riders[k];
                    rec.value = times[k];
                    rec.player = k == 0;
                    r.rows.push_back(rec);
                }
                r.message = "Congratulations! You set a new record time.";
                r.items = {"Continue", "Save Records"};
                r.index = percent % 2;
                rl.records(canvas, frame, r);
            } else {
                fe::ResultsLui::RaceCard r;
                r.title = "Snow Jam";
                r.sub = "Single Event";
                r.objective = "Finish in first place to earn a Gold medal.";
                const char* names[6] = {"Zoe", "Mac", "Elise", "Psymon", "Kaori", "Moby"};
                for (int k = 0; k < 6; ++k) {
                    r.riders.push_back({names[k], k == 0, false});
                }
                r.recordLabel = "Record time:";
                r.record = "03:54:12";
                r.continueLabel = "Continue";
                rl.raceCard(canvas, frame, r);
            }
        } else if (screen == "single") {
            // native/tools/ui-shot.mjs's QA standings: the human (finishing at --ticks) and five computer riders, all finished
            fe::CareerData career;
            if (!career.load(nlohmann::json::parse(readText(assets + "/CAREER/career.json")))) {
                throw std::runtime_error("no CAREER/career.json");
            }
            fe::RecordsStore records;
            records.reset(career.rules["records"]);
            std::string humanName = "Sam";
            for (const auto& r : nlohmann::json::parse(readText(assets + "/riders.json"))) {
                if (r.value("id", "") == rider) {
                    humanName = r.value("name", "Sam");
                }
            }
            std::vector<fe::RaceStanding> standings;
            fe::RaceStanding h;
            h.human = true;
            h.name = humanName;
            h.finished = true;
            h.finishTicks = humanTicks;
            standings.push_back(h);
            const char* names[5] = {"Mac", "Psymon", "Elise", "Kaori", "Moby"};
            const double finishes[5] = {12500, 12800, 13100, 13300, 13600};
            for (int k = 0; k < 5; ++k) {
                fe::RaceStanding r;
                r.name = names[k];
                r.finished = true;
                r.finishTicks = finishes[k];
                r.place = k + 1;
                standings.push_back(r);
            }
            const fe::RaceOutcome outcome = fe::singleEventRace(career, records, career.courseOf("ARA1"), fe::riderCharacter(rider), standings, humanTicks);
            fe::SingleEventScreens screens;
            screens.data = &career;
            screens.records = &records;
            screens.open(outcome);
            // the intro's input lock, then the keys
            for (int k = 0; k < fe::SingleEventScreens::kIntroLock; ++k) {
                screens.frame();
            }
            std::stringstream keys(keyList);
            for (std::string code; std::getline(keys, code, ',');) {
                if (!code.empty()) {
                    screens.key(code);
                    for (int k = 0; k < fe::SingleEventScreens::kIntroLock; ++k) {
                        screens.frame();
                    }
                }
            }
            fe::ResultsLui rl;
            if (!rl.load(nlohmann::json::parse(readText(assets + "/UI/results-screens.json")), ui, text)) {
                throw std::runtime_error("no UI/results-screens.json");
            }
            rl.setKeyboard(!gamepad);
            screens.draw(canvas, rl, 0, frame);
            std::printf("{\"place\":%d,\"medal\":%d,\"record\":%d,\"screen\":%d,\"index\":%d}\n", outcome.place, outcome.medal, outcome.record,
                        int(screens.screen()), screens.index());
        } else {
            throw std::runtime_error("unknown screen " + screen);
        }
        // the black frame the canvas composites over
        WGPUTexture target = gpu.texture(640, 480, WGPUTextureFormat_RGBA8Unorm,
                                         WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding, 1,
                                         "ui shot");
        {
            const std::vector<uint8_t> blackFrame(size_t(640) * 480 * 4, 0);
            std::vector<uint8_t> opaque = blackFrame;
            for (size_t i = 3; i < opaque.size(); i += 4) {
                opaque[i] = 255;
            }
            gpu.writeTexture(target, 640, 480, opaque.data());
        }
        WGPUTextureViewDescriptor vd = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
        WGPUTextureView view = wgpuTextureCreateView(target, &vd);
        WGPUCommandEncoderDescriptor ed = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
        WGPUCommandEncoder enc = wgpuDeviceCreateCommandEncoder(gpu.device, &ed);
        canvas.encode(enc, view, 640, 480);
        WGPUCommandBufferDescriptor cd = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
        WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(enc, &cd);
        wgpuQueueSubmit(gpu.queue, 1, &cmd);
        wgpuCommandBufferRelease(cmd);
        wgpuCommandEncoderRelease(enc);
        const auto pixels = gpu.readback(target, 640, 480);
        writePng(positional[1], 640, 480, pixels.data());
        if (!canvasLog.empty()) {
            std::ofstream(canvasLog) << canvas.log.dump();
        }
        std::printf("{\"screen\":\"%s\",\"frame\":%g,\"percent\":%d,\"out\":%s}\n", screen.c_str(), frame, percent, nlohmann::json(positional[1]).dump().c_str());
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "ssx3_ui_shot: %s\n", e.what());
        return 1;
    }
}
