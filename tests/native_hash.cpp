// Execute one unmodified generated SSX 3 routine using the upstream runtime.
// This is a CPU conformance test, not a game boot or a replacement runtime.
#include "ps2_runtime_macros.h"
#include <bit>
#include <iostream>
#include <random>
#include <string>
#include <vector>

void sub_00317618_0x317618(uint8_t*, R5900Context*, PS2Runtime*);
void sub_003FE818_0x3fe818(uint8_t*, R5900Context*, PS2Runtime*);

// The tested leaf routine makes no dispatched calls. A deliberately empty
// dispatch table ensures this test cannot silently run another guest function.
extern const uint32_t g_ps2RecompiledFunctionTableBase = 0;
extern const uint32_t g_ps2RecompiledFunctionTableEnd = 0;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount = 0;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[1] = {nullptr};

static uint32_t reference(const std::string& input) {
    uint32_t hash = 0;
    for (unsigned char byte : input) {
        if (byte == 0) break;
        const int32_t signedByte = byte < 128 ? byte : int32_t(byte) - 256;
        hash = hash * 16u + uint32_t(signedByte);
        const uint32_t high = hash & 0xf0000000u;
        hash ^= high >> 23;
        hash ^= high;
    }
    return hash;
}

int main() {
    PS2Runtime runtime;
    std::vector<uint8_t> ram(32 * 1024 * 1024);
    std::mt19937 random(0x53535833);
    std::vector<std::string> cases = {"", "SSX3", "data/worlds/bam.sdb", "Kaori", std::string(4096, 'z'), std::string("a\0b", 3)};
    for (int i = 0; i < 10000; ++i) {
        std::string value(random() % 257, '\0');
        for (char& c : value) c = char(1 + random() % 255);
        cases.push_back(value);
    }
    for (const auto& input : cases) {
        constexpr uint32_t source = 0x10000, dest = 0x20000, returnPC = 0x12345678;
        std::memcpy(ram.data() + source, input.c_str(), input.size() + 1);
        std::fill(ram.begin() + dest - 4, ram.begin() + dest + 8, 0xa5);
        R5900Context ctx{};
        SET_GPR_U32((&ctx), 4, dest);
        SET_GPR_U32((&ctx), 5, source);
        SET_GPR_U32((&ctx), 31, returnPC);
        SET_GPR_U32((&ctx), 29, 0x1ff0000);
        SET_GPR_U32((&ctx), 16, 0xdeadbeef);
        ctx.pc = 0x317618;
        unsigned resumes = 0;
        do {
            sub_00317618_0x317618(ram.data(), &ctx, &runtime);
            if (++resumes > 100000) throw std::runtime_error("Routine failed to return");
        } while (ctx.pc != returnPC);
        uint32_t stored;
        std::memcpy(&stored, ram.data() + dest, 4);
        if (stored != reference(input) || GPR_U32((&ctx), 2) != stored ||
            GPR_U32((&ctx), 29) != 0x1ff0000 || GPR_U32((&ctx), 16) != 0xdeadbeef ||
            GPR_U64((&ctx), 0) != 0 || ctx.in_delay_slot ||
            ram[dest - 1] != 0xa5 || ram[dest + 4] != 0xa5) {
            std::cerr << "Hash/ABI mismatch for input length " << input.size() << '\n';
            return 1;
        }
    }
    std::cout << cases.size() << " native SSX 3 hash cases passed (return value, memory, ABI).\n";
    for (uint32_t address : {0u, 1u, 511u, 512u, 65535u, 0x12345678u}) {
        R5900Context ctx{};
        SET_GPR_U32(&ctx, 4, address);
        SET_GPR_U32(&ctx, 31, 0x12345678);
        sub_003FE818_0x3fe818(ram.data(), &ctx, &runtime);
        if (ctx.vu0_cmsar0 != (address & 0xffffu) ||
            ctx.vu0_pc != ((address & 0x1ffu) * 8u) ||
            ctx.pc != 0x12345678 || ctx.in_delay_slot) {
            std::cerr << "VCALLMSR CMSAR0 mapping/return mismatch\n";
            return 1;
        }
    }
    std::cout << "6 native VCALLMSR address/return cases passed (microprogram execution not tested).\n";
}
