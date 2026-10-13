// Diagnostic host for the experimental translation. Not a playable game app.
#include "ps2_runtime.h"
#include "runtime/ee_scheduler.h"
#include <CommonCrypto/CommonDigest.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include "ps2_stubs.h"
#include "Stubs/Audio.h"
#include "Stubs/MPEG.h"

int main(int argc, char** argv) {
    if (argc != 3) {
        std::cerr << "Usage: ssx3_boot_probe /path/to/SLUS_207.72 /path/to/game.iso\n"
                  << "Experimental boot probe; gameplay is not validated.\n";
        return 2;
    }
    try {
        std::ifstream stream(argv[1], std::ios::binary);
        if (!stream) throw std::runtime_error("Cannot read SSX 3 executable");
        std::vector<unsigned char> elf((std::istreambuf_iterator<char>(stream)), {});
        unsigned char digest[CC_SHA256_DIGEST_LENGTH];
        if (elf.size() > 16 * 1024 * 1024) throw std::runtime_error("Unexpected executable size");
        CC_SHA256(elf.data(), static_cast<CC_LONG>(elf.size()), digest);
        std::ostringstream hex;
        for (unsigned char byte : digest) hex << std::hex << std::setw(2) << std::setfill('0') << unsigned(byte);
        if (hex.str() != "1b49d05ca2793922180851b9e1ce9ae2291d61a7863565ac4e71f12e967af7bc")
            throw std::runtime_error("Executable does not match SSX 3 NTSC-U; refusing to execute");
        PS2Runtime runtime;
        runtime.setMissingFunctionPolicy(PS2Runtime::MissingFunctionPolicy::Stop);
        const bool headless = std::getenv("SSX_HEADLESS") != nullptr;
        unsigned frames = 0;
        runtime.setDebugUiCallbacks(
            [](PS2Runtime&, void*) {},
            [](PS2Runtime& rt, void* data) {
                auto& frames = *static_cast<unsigned*>(data);
                if (++frames % 120 == 0) {
                    auto snapshot = rt.eeScheduler().snapshot();
                    std::cerr << "[probe] host frame=" << frames << " guest cycles=" << snapshot.eeCycle;
                    for (const auto& thread : snapshot.threads)
                        std::cerr << " thread=" << thread.id << " pc=0x" << std::hex << thread.pc << std::dec;
                    std::cerr << '\n';
                }
                if (frames >= 600) rt.requestStop();
            },
            [](PS2Runtime&, void*) {}, &frames);
        if (headless) {
            if (!runtime.memory().initialize() || !runtime.syncCoreSubsystems()) return 1;
        } else if (!runtime.initialize("SSX 3 — experimental boot probe")) return 1;
        std::cerr << "[probe] Host initialized; loading SSX 3 ELF\n";
        if (!runtime.loadELF(argv[1])) return 1;
        auto paths = PS2Runtime::getIoPaths();
        paths.cdImage = argv[2];
        PS2Runtime::setIoPaths(paths);
        std::cerr << "[probe] ELF loaded; starting at 0x" << std::hex << runtime.cpu().pc << std::dec << '\n';
        if (headless) {
            ps2_stubs::resetSifState();
            ps2_stubs::resetAudioStubState();
            ps2_stubs::resetMpegStubState();
            runtime.initializeEeKernelState(runtime.memory().getRDRAM());
            runtime.cpu().r[4] = _mm_setzero_si128();
            runtime.cpu().r[5] = _mm_setzero_si128();
            runtime.cpu().r[29] = _mm_set_epi64x(0, 32 * 1024 * 1024 - 16);
            runtime.eeScheduler().reset(runtime.memory().getRDRAM(), runtime.cpu());
            std::jthread watchdog([&](std::stop_token stop) {
                for (int i = 0; i < 20 && !stop.stop_requested(); ++i)
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                runtime.requestStop();
            });
            runtime.eeScheduler().run();
            watchdog.request_stop();
        } else runtime.run();
        std::cerr << "[probe] Stopped at guest PC 0x" << std::hex << runtime.cpu().pc << std::dec << '\n';
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
