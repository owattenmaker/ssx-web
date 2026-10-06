// EE oracle: runs one PS2 function from a savestate's EE memory on the console's arithmetic (docs/ps2-float.md "EE oracle").
//
// A small R5900 interpreter: the integer ISA with the MMI ops the game uses, COP1 (the EE FPU) and COP2 macro mode (VU0),
// all float arithmetic through engine/ps2_fpu.hpp. It loads EE RAM and the scratchpad written by state.py, calls one
// function with chosen arguments (gp = 0x4A30F0, a private stack, a sentinel return address) and runs it to its return.
// It logs what was asked for (calls, every FPU / VU0 op, writes to watched ranges) and stops with the pc on anything it does
// not model: syscalls, hardware registers, unknown opcodes. VU0 micro programs (vcallms) run sequentially; a flag or Q
// read the hardware's pipeline would answer differently is reported as a hazard.
//
// usage: oracle --state DIR --call ADDR [--a0 X ...--a3 X] [--f12 X] [--f13 X] [--trace-calls] [--trace-fpu]
//               [--watch ADDR:LEN ...] [--dump ADDR:LEN ...] [--max-steps N] [--save DIR] [--arith exact|mode1] [--sp ADDR]
//   --sp runs on the game's own stack (state.py prints the frozen sp) instead of the private one: needed when the code
//   reads a stack word it never wrote (stale locals left by earlier calls).
//   numbers are hex with 0x or decimal; --f12 takes a float or 0x bits.
#include "arith.hpp"
#include "ps2_fpu.hpp"

#include <algorithm>
#include <cfenv>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace ssx::ps2fpu;
using namespace oracle_arith;

constexpr uint32_t kRamSize = 32u << 20;
constexpr uint32_t kScratchBase = 0x70000000u;
constexpr uint32_t kScratchSize = 16u << 10;
// The oracle's own stack, outside the PS2 map so it can never overwrite game data.
constexpr uint32_t kStackBase = 0x7F000000u;
constexpr uint32_t kStackSize = 1u << 20;
constexpr uint32_t kReturnSentinel = 0x7FFFFFF0u;
constexpr uint32_t kGp = 0x004A30F0u;

struct Stop : std::runtime_error {
    using std::runtime_error::runtime_error;
};

std::string hex(uint64_t value) {
    char buffer[32];
    std::snprintf(buffer, sizeof buffer, "0x%llX", (unsigned long long)value);
    return buffer;
}

struct Watch {
    uint32_t begin;
    uint32_t end;
};

struct Machine {
    std::vector<uint8_t> ram = std::vector<uint8_t>(kRamSize);
    std::vector<uint8_t> scratch = std::vector<uint8_t>(kScratchSize);
    std::vector<uint8_t> stack = std::vector<uint8_t>(kStackSize);
    std::vector<uint8_t> vu0Data = std::vector<uint8_t>(4096);

    // 128-bit GPRs as two 64-bit halves.
    uint64_t gpr[32][2] = {};
    uint64_t lo[2] = {};
    uint64_t hi[2] = {};
    uint32_t sa = 0;
    uint32_t pc = 0;
    uint32_t fpr[32] = {};
    uint32_t facc = 0;
    bool fcc = false;
    uint32_t vf[32][4] = {};
    uint16_t vi[16] = {};
    uint32_t vacc[4] = {};
    uint32_t q = 0;
    uint32_t i = 0;
    uint32_t r = 0;
    uint32_t status = 0;
    uint32_t mac = 0;

    bool traceCalls = false;
    bool traceFpu = false;
    std::vector<Watch> watches;
    int depth = 0;
    uint64_t steps = 0;
    uint64_t fpuOps = 0;
    uint64_t syscalls = 0;

    uint8_t* locate(uint32_t address, uint32_t size, bool write) {
        if (address >= kStackBase && address + size <= kStackBase + kStackSize) {
            return &stack[address - kStackBase];
        }
        if (address >= kScratchBase && address + size <= kScratchBase + kScratchSize) {
            return &scratch[address - kScratchBase];
        }
        // VU0 data memory as the EE sees it (0x11004000).
        if (address >= 0x11004000u && address + size <= 0x11005000u) {
            return &vu0Data[address - 0x11004000u];
        }
        const uint32_t physical = address & 0x1FFFFFFFu;
        const uint32_t segment = address & 0xE0000000u;
        const bool ramSegment = segment == 0x00000000u || segment == 0x20000000u || segment == 0x80000000u || segment == 0xA0000000u;
        if (ramSegment && (address & 0x1E000000u) == 0 && physical + size <= kRamSize) {
            return &ram[physical];
        }
        throw Stop(std::string(write ? "write" : "read") + " outside RAM / scratchpad at " + hex(address) + " (pc " + hex(pc) + ")");
    }

    template <typename T>
    T load(uint32_t address) {
        if (address % sizeof(T)) {
            throw Stop("unaligned load at " + hex(address) + " (pc " + hex(pc) + ")");
        }
        T value;
        std::memcpy(&value, locate(address, sizeof(T), false), sizeof(T));
        return value;
    }

    template <typename T>
    void store(uint32_t address, T value) {
        if (address % sizeof(T)) {
            throw Stop("unaligned store at " + hex(address) + " (pc " + hex(pc) + ")");
        }
        for (const Watch& watch : watches) {
            if (address < watch.end && address + sizeof(T) > watch.begin) {
                std::printf("write pc %s addr %s size %zu value %s\n", hex(pc).c_str(), hex(address).c_str(), sizeof(T), hex(uint64_t(value)).c_str());
            }
        }
        std::memcpy(locate(address, sizeof(T), true), &value, sizeof(T));
    }

    void load128(uint32_t address, uint64_t out[2]) {
        address &= ~15u;
        std::memcpy(out, locate(address, 16, false), 16);
    }

    void store128(uint32_t address, const uint64_t in[2]) {
        address &= ~15u;
        for (const Watch& watch : watches) {
            if (address < watch.end && address + 16 > watch.begin) {
                std::printf("write pc %s addr %s size 16 value %s %s\n", hex(pc).c_str(), hex(address).c_str(), hex(in[0]).c_str(), hex(in[1]).c_str());
            }
        }
        std::memcpy(locate(address, 16, true), in, 16);
    }

    int64_t s64(unsigned reg) const {
        return int64_t(gpr[reg][0]);
    }

    uint64_t u64(unsigned reg) const {
        return gpr[reg][0];
    }

    uint32_t u32(unsigned reg) const {
        return uint32_t(gpr[reg][0]);
    }

    int32_t s32(unsigned reg) const {
        return int32_t(gpr[reg][0]);
    }

    void set64(unsigned reg, uint64_t value) {
        if (reg) {
            gpr[reg][0] = value;
        }
    }

    // A 32-bit result, sign-extended into the low doubleword (the upper 64 bits are kept).
    void set32(unsigned reg, uint32_t value) {
        set64(reg, uint64_t(int64_t(int32_t(value))));
    }

    void logFpu(const char* op, uint32_t a, uint32_t b, uint32_t c, uint32_t result) {
        fpuOps++;
        if (traceFpu) {
            std::printf("fpu %s pc %s %08X %08X %08X -> %08X\n", op, hex(pc).c_str(), a, b, c, result);
        }
    }

    void run(uint64_t maxSteps);
    void step();
    void special(uint32_t code);
    void regimm(uint32_t code, uint32_t next);
    void mmi(uint32_t code);
    void cop1(uint32_t code, uint32_t next);
    void cop2(uint32_t code, uint32_t next);
    uint32_t readControl(unsigned index);
    void writeControl(unsigned index, uint32_t value);
    void fmacFlags(unsigned dest, const Result lanes[4]);
    void fmacUpper(uint32_t code);
    void fmacSpecial2(uint32_t code, unsigned index);
    void integerOp(uint32_t code);
    void lowerSpecial2(uint32_t code, unsigned index);
    void runMicro(uint32_t start);
    void executeMicroUpper(uint32_t upper);
    void executeMicroLower(uint32_t lower, bool& branchNow, uint32_t& branchTo);
    void hazard(const std::string& what);
    Result macLogged(uint32_t acc, uint32_t a, uint32_t b, bool subtract);
    uint32_t eeMacLogged(uint32_t acc, uint32_t a, uint32_t b, bool subtract);
    uint32_t qValue() const;
    uint32_t visibleMac() const;
    uint32_t visibleStatus() const;
    uint32_t visibleClip() const;
    void issueMicroPair(uint32_t upper, uint32_t lower);
    void retireMicroPair(uint32_t upper, uint32_t lower);

    // VU0 micro mode state, and the pair counters the hazard checks use.
    std::vector<uint8_t> microMem = std::vector<uint8_t>(4096);
    uint32_t clip = 0;
    uint32_t cmsar0 = 0;
    bool inMicro = false;
    uint32_t microPc = 0;
    uint64_t pairIndex = 0;
    // The micro pipeline (docs/ps2-float.md "EE oracle"): a pair issues a cycle after the previous one, later if a source
    // register is still in flight (FMAC and the lower VF writers deliver 4 cycles after issue). Flags reach the flag
    // registers 4 cycles after their op; Q arrives 7 (DIV, SQRT) or 13 (RSQRT) cycles after issue, and a read before then
    // sees the old Q. A WAITQ or a second divide stalls until Q is ready.
    uint64_t cycle = 0;
    uint64_t vfReady[32] = {};
    uint64_t accReady = 0;
    uint64_t qReadyCycle = 0;
    uint32_t qOld = 0;
    struct FlagEvent {
        uint64_t time;
        uint32_t mac;
        uint32_t status;
        uint32_t clip;
    };
    std::vector<FlagEvent> flagEvents;
    uint32_t entryMac = 0;
    uint32_t entryStatus = 0;
    uint32_t entryClip = 0;
    uint64_t hazards = 0;

    // Branch handling: the delay slot runs before the target is taken.
    uint32_t branchTarget = 0;
    bool branchPending = false;
    bool skipDelaySlot = false;

    void branch(bool taken, uint32_t target, bool likely) {
        if (taken) {
            branchTarget = target;
            branchPending = true;
        } else if (likely) {
            skipDelaySlot = true;
        }
    }
};

void Machine::run(uint64_t maxSteps) {
    while (pc != kReturnSentinel) {
        if (steps++ >= maxSteps) {
            throw Stop("step limit reached at pc " + hex(pc));
        }
        step();
    }
}

void Machine::step() {
    const uint32_t code = load<uint32_t>(pc);
    const uint32_t next = pc + 4;
    const bool inDelay = branchPending;
    const uint32_t target = branchTarget;
    branchPending = false;
    const unsigned op = code >> 26;
    const unsigned rs = (code >> 21) & 31;
    const unsigned rt = (code >> 16) & 31;
    const int32_t immediate = int16_t(code & 0xFFFF);
    const uint32_t uimmediate = code & 0xFFFF;
    const uint32_t branchTo = next + (uint32_t(immediate) << 2);
    const uint32_t address = u32(rs) + uint32_t(immediate);
    switch (op) {
    case 0x00:
        special(code);
        break;
    case 0x01:
        regimm(code, next);
        break;
    case 0x02:
        branch(true, (next & 0xF0000000u) | ((code & 0x3FFFFFF) << 2), false);
        break;
    case 0x03: {
        const uint32_t callee = (next & 0xF0000000u) | ((code & 0x3FFFFFF) << 2);
        if (traceCalls) {
            std::printf("call %*s%s -> %s\n", depth * 2, "", hex(pc).c_str(), hex(callee).c_str());
        }
        depth++;
        set64(31, uint64_t(int64_t(int32_t(pc + 8))));
        branch(true, callee, false);
        break;
    }
    case 0x04:
        branch(u64(rs) == u64(rt), branchTo, false);
        break;
    case 0x05:
        branch(u64(rs) != u64(rt), branchTo, false);
        break;
    case 0x06:
        branch(s64(rs) <= 0, branchTo, false);
        break;
    case 0x07:
        branch(s64(rs) > 0, branchTo, false);
        break;
    case 0x08:
    case 0x09:
        set32(rt, u32(rs) + uint32_t(immediate));
        break;
    case 0x0A:
        set64(rt, s64(rs) < int64_t(immediate) ? 1 : 0);
        break;
    case 0x0B:
        set64(rt, u64(rs) < uint64_t(int64_t(immediate)) ? 1 : 0);
        break;
    case 0x0C:
        set64(rt, u64(rs) & uimmediate);
        break;
    case 0x0D:
        set64(rt, u64(rs) | uimmediate);
        break;
    case 0x0E:
        set64(rt, u64(rs) ^ uimmediate);
        break;
    case 0x0F:
        set32(rt, uimmediate << 16);
        break;
    case 0x10: {
        // COP0: Count and Status reads come back 0; EI / DI / writes are ignored.
        const unsigned format = rs;
        if (format == 0x00) {
            set32(rt, 0);
        } else if (format != 0x04 && format != 0x10) {
            throw Stop("COP0 op " + hex(code) + " at " + hex(pc));
        }
        break;
    }
    case 0x11:
        cop1(code, next);
        break;
    case 0x12:
        cop2(code, next);
        break;
    case 0x14:
        branch(u64(rs) == u64(rt), branchTo, true);
        break;
    case 0x15:
        branch(u64(rs) != u64(rt), branchTo, true);
        break;
    case 0x16:
        branch(s64(rs) <= 0, branchTo, true);
        break;
    case 0x17:
        branch(s64(rs) > 0, branchTo, true);
        break;
    case 0x18:
    case 0x19:
        set64(rt, u64(rs) + uint64_t(int64_t(immediate)));
        break;
    case 0x1A: {
        // LDL: the high bytes of the doubleword containing the address.
        const uint32_t aligned = address & ~7u;
        const unsigned shift = (address & 7) * 8;
        const uint64_t memory = load<uint64_t>(aligned);
        const uint64_t mask = shift == 56 ? 0 : (~0ull >> (shift + 8));
        set64(rt, (u64(rt) & mask) | (memory << (56 - shift)));
        break;
    }
    case 0x1B: {
        const uint32_t aligned = address & ~7u;
        const unsigned shift = (address & 7) * 8;
        const uint64_t memory = load<uint64_t>(aligned);
        const uint64_t mask = shift == 0 ? 0 : (~0ull << (64 - shift));
        set64(rt, (u64(rt) & mask) | (memory >> shift));
        break;
    }
    case 0x1C:
        mmi(code);
        break;
    case 0x1E: {
        uint64_t value[2];
        load128(address, value);
        if (rt) {
            gpr[rt][0] = value[0];
            gpr[rt][1] = value[1];
        }
        break;
    }
    case 0x1F:
        store128(address, gpr[rt]);
        break;
    case 0x20:
        set64(rt, uint64_t(int64_t(load<int8_t>(address))));
        break;
    case 0x21:
        set64(rt, uint64_t(int64_t(load<int16_t>(address))));
        break;
    case 0x22: {
        // LWL
        const uint32_t aligned = address & ~3u;
        const unsigned shift = (address & 3) * 8;
        const uint32_t memory = load<uint32_t>(aligned);
        const uint32_t mask = shift == 24 ? 0 : (0xFFFFFFFFu >> (shift + 8));
        set32(rt, (u32(rt) & mask) | (memory << (24 - shift)));
        break;
    }
    case 0x23:
        set32(rt, load<uint32_t>(address));
        break;
    case 0x24:
        set64(rt, load<uint8_t>(address));
        break;
    case 0x25:
        set64(rt, load<uint16_t>(address));
        break;
    case 0x26: {
        // LWR
        const uint32_t aligned = address & ~3u;
        const unsigned shift = (address & 3) * 8;
        const uint32_t memory = load<uint32_t>(aligned);
        const uint32_t mask = shift == 0 ? 0 : (0xFFFFFFFFu << (32 - shift));
        const uint32_t value = (u32(rt) & mask) | (memory >> shift);
        if (shift == 0) {
            set32(rt, value);
        } else {
            set64(rt, (u64(rt) & ~0xFFFFFFFFull) | value);
        }
        break;
    }
    case 0x27:
        set64(rt, load<uint32_t>(address));
        break;
    case 0x28:
        store<uint8_t>(address, uint8_t(u64(rt)));
        break;
    case 0x29:
        store<uint16_t>(address, uint16_t(u64(rt)));
        break;
    case 0x2A: {
        // SWL
        const uint32_t aligned = address & ~3u;
        const unsigned shift = (address & 3) * 8;
        const uint32_t memory = load<uint32_t>(aligned);
        const uint32_t mask = shift == 24 ? 0 : (0xFFFFFFFFu << (shift + 8));
        store<uint32_t>(aligned, (memory & mask) | (u32(rt) >> (24 - shift)));
        break;
    }
    case 0x2B:
        store<uint32_t>(address, u32(rt));
        break;
    case 0x2C: {
        // SDL
        const uint32_t aligned = address & ~7u;
        const unsigned shift = (address & 7) * 8;
        const uint64_t memory = load<uint64_t>(aligned);
        const uint64_t mask = shift == 56 ? 0 : (~0ull << (shift + 8));
        store<uint64_t>(aligned, (memory & mask) | (u64(rt) >> (56 - shift)));
        break;
    }
    case 0x2D: {
        // SDR
        const uint32_t aligned = address & ~7u;
        const unsigned shift = (address & 7) * 8;
        const uint64_t memory = load<uint64_t>(aligned);
        const uint64_t mask = shift == 0 ? 0 : (~0ull >> (64 - shift));
        store<uint64_t>(aligned, (memory & mask) | (u64(rt) << shift));
        break;
    }
    case 0x2E: {
        // SWR
        const uint32_t aligned = address & ~3u;
        const unsigned shift = (address & 3) * 8;
        const uint32_t memory = load<uint32_t>(aligned);
        const uint32_t mask = shift == 0 ? 0 : (0xFFFFFFFFu >> (32 - shift));
        store<uint32_t>(aligned, (memory & mask) | (u32(rt) << shift));
        break;
    }
    case 0x2F:
        // CACHE
        break;
    case 0x31:
        fpr[rt] = load<uint32_t>(address);
        break;
    case 0x33:
        // PREF
        break;
    case 0x36: {
        uint64_t value[2];
        load128(address, value);
        std::memcpy(vf[rt], value, 16);
        if (rt == 0) {
            const uint32_t zero[4] = {0, 0, 0, 0x3F800000u};
            std::memcpy(vf[0], zero, 16);
        }
        break;
    }
    case 0x37:
        set64(rt, load<uint64_t>(address));
        break;
    case 0x39:
        store<uint32_t>(address, fpr[rt]);
        break;
    case 0x3E: {
        uint64_t value[2];
        std::memcpy(value, vf[rt], 16);
        store128(address, value);
        break;
    }
    case 0x3F:
        store<uint64_t>(address, u64(rt));
        break;
    default:
        throw Stop("unknown opcode " + hex(code) + " at " + hex(pc));
    }
    // Advance: a taken branch's delay slot is the next instruction; a likely branch not taken skips it.
    if (inDelay) {
        pc = target;
    } else if (skipDelaySlot) {
        skipDelaySlot = false;
        pc = next + 4;
    } else {
        pc = next;
    }
}

void Machine::special(uint32_t code) {
    const unsigned rs = (code >> 21) & 31;
    const unsigned rt = (code >> 16) & 31;
    const unsigned rd = (code >> 11) & 31;
    const unsigned shamt = (code >> 6) & 31;
    const unsigned funct = code & 63;
    switch (funct) {
    case 0x00:
        set32(rd, u32(rt) << shamt);
        break;
    case 0x02:
        set32(rd, u32(rt) >> shamt);
        break;
    case 0x03:
        set32(rd, uint32_t(s32(rt) >> shamt));
        break;
    case 0x04:
        set32(rd, u32(rt) << (u32(rs) & 31));
        break;
    case 0x06:
        set32(rd, u32(rt) >> (u32(rs) & 31));
        break;
    case 0x07:
        set32(rd, uint32_t(s32(rt) >> (u32(rs) & 31)));
        break;
    case 0x08: {
        const uint32_t target = u32(rs);
        if (rs == 31 && depth > 0) {
            depth--;
        }
        branch(true, target, false);
        break;
    }
    case 0x09: {
        const uint32_t target = u32(rs);
        if (traceCalls) {
            std::printf("call %*s%s -> %s (jalr)\n", depth * 2, "", hex(pc).c_str(), hex(target).c_str());
        }
        depth++;
        set64(rd, uint64_t(int64_t(int32_t(pc + 8))));
        branch(true, target, false);
        break;
    }
    case 0x0A:
        if (u64(rt) == 0) {
            set64(rd, u64(rs));
        }
        break;
    case 0x0B:
        if (u64(rt) != 0) {
            set64(rd, u64(rs));
        }
        break;
    case 0x0C: {
        // The kernel calls the gameplay passes reach (the sound library's thread / semaphore bookkeeping) answer
        // as an idle kernel would; any other syscall stops the run.
        const uint32_t number = u32(3);
        if (traceCalls) {
            std::printf("syscall %s at %s\n", hex(number).c_str(), hex(pc).c_str());
        }
        syscalls++;
        if (number == 0x2F) {
            set64(2, 1);
        } else if ((number >= 0x40 && number <= 0x47) || number == 0x64 || number == 0x68) {
            set64(2, 0);
        } else {
            throw Stop("syscall " + hex(number) + " at " + hex(pc));
        }
        break;
    }
    case 0x0D:
        throw Stop("break at " + hex(pc));
    case 0x0F:
        break;
    case 0x10:
        set64(rd, hi[0]);
        break;
    case 0x11:
        hi[0] = u64(rs);
        break;
    case 0x12:
        set64(rd, lo[0]);
        break;
    case 0x13:
        lo[0] = u64(rs);
        break;
    case 0x14:
        set64(rd, u64(rt) << (u32(rs) & 63));
        break;
    case 0x16:
        set64(rd, u64(rt) >> (u32(rs) & 63));
        break;
    case 0x17:
        set64(rd, uint64_t(s64(rt) >> (u32(rs) & 63)));
        break;
    case 0x18: {
        const int64_t product = int64_t(s32(rs)) * int64_t(s32(rt));
        lo[0] = uint64_t(int64_t(int32_t(product)));
        hi[0] = uint64_t(int64_t(int32_t(product >> 32)));
        set64(rd, lo[0]);
        break;
    }
    case 0x19: {
        const uint64_t product = uint64_t(u32(rs)) * uint64_t(u32(rt));
        lo[0] = uint64_t(int64_t(int32_t(uint32_t(product))));
        hi[0] = uint64_t(int64_t(int32_t(uint32_t(product >> 32))));
        set64(rd, lo[0]);
        break;
    }
    case 0x1A: {
        const int32_t n = s32(rs);
        const int32_t d = s32(rt);
        if (d != 0 && !(n == INT32_MIN && d == -1)) {
            lo[0] = uint64_t(int64_t(n / d));
            hi[0] = uint64_t(int64_t(n % d));
        } else if (d == 0) {
            lo[0] = uint64_t(int64_t(n < 0 ? 1 : -1));
            hi[0] = uint64_t(int64_t(n));
        } else {
            lo[0] = uint64_t(int64_t(INT32_MIN));
            hi[0] = 0;
        }
        break;
    }
    case 0x1B: {
        const uint32_t n = u32(rs);
        const uint32_t d = u32(rt);
        if (d != 0) {
            lo[0] = uint64_t(int64_t(int32_t(n / d)));
            hi[0] = uint64_t(int64_t(int32_t(n % d)));
        } else {
            lo[0] = ~0ull;
            hi[0] = uint64_t(int64_t(int32_t(n)));
        }
        break;
    }
    case 0x20:
    case 0x21:
        set32(rd, u32(rs) + u32(rt));
        break;
    case 0x22:
    case 0x23:
        set32(rd, u32(rs) - u32(rt));
        break;
    case 0x24:
        set64(rd, u64(rs) & u64(rt));
        break;
    case 0x25:
        set64(rd, u64(rs) | u64(rt));
        break;
    case 0x26:
        set64(rd, u64(rs) ^ u64(rt));
        break;
    case 0x27:
        set64(rd, ~(u64(rs) | u64(rt)));
        break;
    case 0x28:
        set64(rd, sa);
        break;
    case 0x29:
        sa = u32(rs);
        break;
    case 0x2A:
        set64(rd, s64(rs) < s64(rt) ? 1 : 0);
        break;
    case 0x2B:
        set64(rd, u64(rs) < u64(rt) ? 1 : 0);
        break;
    case 0x2C:
    case 0x2D:
        set64(rd, u64(rs) + u64(rt));
        break;
    case 0x2E:
    case 0x2F:
        set64(rd, u64(rs) - u64(rt));
        break;
    case 0x30:
    case 0x31:
    case 0x32:
    case 0x33:
    case 0x34:
    case 0x36:
        // Traps: the game's would end the run; they are reported if they fire.
        if (funct == 0x30 && s64(rs) >= s64(rt)) {
            throw Stop("tge trap at " + hex(pc));
        }
        break;
    case 0x38:
        set64(rd, u64(rt) << shamt);
        break;
    case 0x3A:
        set64(rd, u64(rt) >> shamt);
        break;
    case 0x3B:
        set64(rd, uint64_t(s64(rt) >> shamt));
        break;
    case 0x3C:
        set64(rd, u64(rt) << (shamt + 32));
        break;
    case 0x3E:
        set64(rd, u64(rt) >> (shamt + 32));
        break;
    case 0x3F:
        set64(rd, uint64_t(s64(rt) >> (shamt + 32)));
        break;
    default:
        throw Stop("unknown SPECIAL " + hex(code) + " at " + hex(pc));
    }
}

void Machine::regimm(uint32_t code, uint32_t next) {
    const unsigned rs = (code >> 21) & 31;
    const unsigned rt = (code >> 16) & 31;
    const uint32_t target = next + (uint32_t(int32_t(int16_t(code & 0xFFFF))) << 2);
    switch (rt) {
    case 0x00:
        branch(s64(rs) < 0, target, false);
        break;
    case 0x01:
        branch(s64(rs) >= 0, target, false);
        break;
    case 0x02:
        branch(s64(rs) < 0, target, true);
        break;
    case 0x03:
        branch(s64(rs) >= 0, target, true);
        break;
    case 0x18:
        // MTSAB
        sa = ((u32(rs) & 15) ^ (code & 15)) * 8;
        break;
    case 0x19:
        // MTSAH
        sa = ((u32(rs) & 7) ^ (code & 7)) * 16;
        break;
    default:
        throw Stop("unknown REGIMM " + hex(code) + " at " + hex(pc));
    }
}

// The MMI ops are lane-wise over the 128-bit registers.
struct Lanes {
    uint64_t v[2];

    uint32_t w(int k) const {
        return uint32_t(v[k / 2] >> ((k % 2) * 32));
    }

    uint16_t h(int k) const {
        return uint16_t(v[k / 4] >> ((k % 4) * 16));
    }

    uint8_t b(int k) const {
        return uint8_t(v[k / 8] >> ((k % 8) * 8));
    }

    void setW(int k, uint32_t x) {
        const int shift = (k % 2) * 32;
        v[k / 2] = (v[k / 2] & ~(0xFFFFFFFFull << shift)) | (uint64_t(x) << shift);
    }

    void setH(int k, uint16_t x) {
        const int shift = (k % 4) * 16;
        v[k / 4] = (v[k / 4] & ~(0xFFFFull << shift)) | (uint64_t(x) << shift);
    }

    void setB(int k, uint8_t x) {
        const int shift = (k % 8) * 8;
        v[k / 8] = (v[k / 8] & ~(0xFFull << shift)) | (uint64_t(x) << shift);
    }
};

void Machine::mmi(uint32_t code) {
    const unsigned rs = (code >> 21) & 31;
    const unsigned rt = (code >> 16) & 31;
    const unsigned rd = (code >> 11) & 31;
    const unsigned sub = (code >> 6) & 31;
    const unsigned funct = code & 63;
    Lanes s{{gpr[rs][0], gpr[rs][1]}};
    Lanes t{{gpr[rt][0], gpr[rt][1]}};
    Lanes d{{0, 0}};
    auto write = [&](const Lanes& lanes) {
        if (rd) {
            gpr[rd][0] = lanes.v[0];
            gpr[rd][1] = lanes.v[1];
        }
    };
    auto unknown = [&]() {
        throw Stop("unimplemented MMI " + hex(code) + " (funct " + hex(funct) + " sub " + hex(sub) + ") at " + hex(pc));
    };
    switch (funct) {
    case 0x04: {
        // PLZCW: leading sign bits minus one, per word of rs's low doubleword.
        for (int k = 0; k < 2; ++k) {
            const uint32_t x = s.w(k);
            const uint32_t y = (x & 0x80000000u) ? ~x : x;
            const int count = y == 0 ? 32 : __builtin_clz(y);
            d.setW(k, uint32_t(count - 1));
        }
        set64(rd, d.v[0]);
        return;
    }
    case 0x00:
    case 0x01:
    case 0x20:
    case 0x21: {
        // MADD / MADDU (pipeline 0) and MADD1 / MADDU1 (pipeline 1): {HI, LO} += rs * rt.
        const int pipe = funct >= 0x20 ? 1 : 0;
        const bool isSigned = (funct & 1) == 0;
        const uint64_t accumulator = (uint64_t(uint32_t(hi[pipe])) << 32) | uint32_t(lo[pipe]);
        const uint64_t product = isSigned ? uint64_t(int64_t(s32(rs)) * int64_t(s32(rt))) : uint64_t(u32(rs)) * uint64_t(u32(rt));
        const uint64_t sum = accumulator + product;
        lo[pipe] = uint64_t(int64_t(int32_t(uint32_t(sum))));
        hi[pipe] = uint64_t(int64_t(int32_t(uint32_t(sum >> 32))));
        set64(rd, lo[pipe]);
        return;
    }
    case 0x19: {
        const uint64_t product = uint64_t(u32(rs)) * uint64_t(u32(rt));
        lo[1] = uint64_t(int64_t(int32_t(uint32_t(product))));
        hi[1] = uint64_t(int64_t(int32_t(uint32_t(product >> 32))));
        set64(rd, lo[1]);
        return;
    }
    case 0x1A:
    case 0x1B: {
        // DIV1 / DIVU1
        if (funct == 0x1A) {
            const int32_t n = s32(rs);
            const int32_t d = s32(rt);
            if (d != 0 && !(n == INT32_MIN && d == -1)) {
                lo[1] = uint64_t(int64_t(n / d));
                hi[1] = uint64_t(int64_t(n % d));
            }
        } else if (u32(rt) != 0) {
            lo[1] = uint64_t(int64_t(int32_t(u32(rs) / u32(rt))));
            hi[1] = uint64_t(int64_t(int32_t(u32(rs) % u32(rt))));
        }
        return;
    }
    case 0x10:
        set64(rd, hi[1]);
        return;
    case 0x11:
        hi[1] = u64(rs);
        return;
    case 0x12:
        set64(rd, lo[1]);
        return;
    case 0x13:
        lo[1] = u64(rs);
        return;
    case 0x18: {
        const int64_t product = int64_t(s32(rs)) * int64_t(s32(rt));
        lo[1] = uint64_t(int64_t(int32_t(product)));
        hi[1] = uint64_t(int64_t(int32_t(product >> 32)));
        set64(rd, lo[1]);
        return;
    }
    case 0x08:
        // MMI0
        switch (sub) {
        case 0x00:
            for (int k = 0; k < 4; ++k) {
                d.setW(k, s.w(k) + t.w(k));
            }
            break;
        case 0x01:
            for (int k = 0; k < 4; ++k) {
                d.setW(k, s.w(k) - t.w(k));
            }
            break;
        case 0x02:
            for (int k = 0; k < 4; ++k) {
                d.setW(k, int32_t(s.w(k)) > int32_t(t.w(k)) ? 0xFFFFFFFFu : 0u);
            }
            break;
        case 0x03:
            for (int k = 0; k < 4; ++k) {
                d.setW(k, int32_t(s.w(k)) > int32_t(t.w(k)) ? s.w(k) : t.w(k));
            }
            break;
        case 0x04:
            for (int k = 0; k < 8; ++k) {
                d.setH(k, uint16_t(s.h(k) + t.h(k)));
            }
            break;
        case 0x05:
            for (int k = 0; k < 8; ++k) {
                d.setH(k, uint16_t(s.h(k) - t.h(k)));
            }
            break;
        case 0x06:
            for (int k = 0; k < 8; ++k) {
                d.setH(k, int16_t(s.h(k)) > int16_t(t.h(k)) ? 0xFFFF : 0);
            }
            break;
        case 0x07:
            for (int k = 0; k < 8; ++k) {
                d.setH(k, int16_t(s.h(k)) > int16_t(t.h(k)) ? s.h(k) : t.h(k));
            }
            break;
        case 0x08:
            for (int k = 0; k < 16; ++k) {
                d.setB(k, uint8_t(s.b(k) + t.b(k)));
            }
            break;
        case 0x09:
            for (int k = 0; k < 16; ++k) {
                d.setB(k, uint8_t(s.b(k) - t.b(k)));
            }
            break;
        case 0x12:
            // PEXTLW
            d.setW(0, t.w(0));
            d.setW(1, s.w(0));
            d.setW(2, t.w(1));
            d.setW(3, s.w(1));
            break;
        case 0x16:
            // PEXTLH
            for (int k = 0; k < 4; ++k) {
                d.setH(2 * k, t.h(k));
                d.setH(2 * k + 1, s.h(k));
            }
            break;
        case 0x17:
            // PPACH
            for (int k = 0; k < 4; ++k) {
                d.setH(k, t.h(2 * k));
                d.setH(k + 4, s.h(2 * k));
            }
            break;
        case 0x1A:
            // PEXTLB
            for (int k = 0; k < 8; ++k) {
                d.setB(2 * k, t.b(k));
                d.setB(2 * k + 1, s.b(k));
            }
            break;
        case 0x1B:
            // PPACB
            for (int k = 0; k < 8; ++k) {
                d.setB(k, t.b(2 * k));
                d.setB(k + 8, s.b(2 * k));
            }
            break;
        default:
            unknown();
        }
        write(d);
        return;
    case 0x28:
        // MMI1
        switch (sub) {
        case 0x02:
            for (int k = 0; k < 4; ++k) {
                d.setW(k, s.w(k) == t.w(k) ? 0xFFFFFFFFu : 0u);
            }
            break;
        case 0x03:
            for (int k = 0; k < 4; ++k) {
                d.setW(k, int32_t(s.w(k)) < int32_t(t.w(k)) ? s.w(k) : t.w(k));
            }
            break;
        case 0x06:
            for (int k = 0; k < 8; ++k) {
                d.setH(k, s.h(k) == t.h(k) ? 0xFFFF : 0);
            }
            break;
        case 0x07:
            for (int k = 0; k < 8; ++k) {
                d.setH(k, int16_t(s.h(k)) < int16_t(t.h(k)) ? s.h(k) : t.h(k));
            }
            break;
        case 0x10:
            for (int k = 0; k < 4; ++k) {
                const uint64_t sum = uint64_t(s.w(k)) + t.w(k);
                d.setW(k, sum > 0xFFFFFFFFull ? 0xFFFFFFFFu : uint32_t(sum));
            }
            break;
        case 0x12:
            // PEXTUW
            d.setW(0, t.w(2));
            d.setW(1, s.w(2));
            d.setW(2, t.w(3));
            d.setW(3, s.w(3));
            break;
        case 0x1A:
            // PEXTUB
            for (int k = 0; k < 8; ++k) {
                d.setB(2 * k, t.b(k + 8));
                d.setB(2 * k + 1, s.b(k + 8));
            }
            break;
        case 0x1B: {
            // QFSRV: (rs:rt) >> sa
            const unsigned shift = sa & 127;
            unsigned __int128 value = (unsigned __int128)t.v[0] | ((unsigned __int128)t.v[1] << 64);
            unsigned __int128 upper = (unsigned __int128)s.v[0] | ((unsigned __int128)s.v[1] << 64);
            unsigned __int128 result = shift == 0 ? value : ((value >> shift) | (upper << (128 - shift)));
            d.v[0] = uint64_t(result);
            d.v[1] = uint64_t(result >> 64);
            break;
        }
        default:
            unknown();
        }
        write(d);
        return;
    case 0x09:
        // MMI2
        switch (sub) {
        case 0x0E:
            // PCPYLD
            d.v[0] = t.v[0];
            d.v[1] = s.v[0];
            break;
        case 0x12:
            d.v[0] = s.v[0] & t.v[0];
            d.v[1] = s.v[1] & t.v[1];
            break;
        case 0x13:
            d.v[0] = s.v[0] ^ t.v[0];
            d.v[1] = s.v[1] ^ t.v[1];
            break;
        default:
            unknown();
        }
        write(d);
        return;
    case 0x29:
        // MMI3
        switch (sub) {
        case 0x0A:
            // PINTEH
            for (int k = 0; k < 4; ++k) {
                d.setH(2 * k, t.h(2 * k));
                d.setH(2 * k + 1, s.h(2 * k));
            }
            break;
        case 0x0E:
            // PCPYUD
            d.v[0] = s.v[1];
            d.v[1] = t.v[1];
            break;
        case 0x12:
            d.v[0] = s.v[0] | t.v[0];
            d.v[1] = s.v[1] | t.v[1];
            break;
        case 0x13:
            d.v[0] = ~(s.v[0] | t.v[0]);
            d.v[1] = ~(s.v[1] | t.v[1]);
            break;
        case 0x1B:
            // PCPYH
            for (int k = 0; k < 4; ++k) {
                d.setH(k, t.h(0));
                d.setH(k + 4, t.h(4));
            }
            break;
        default:
            unknown();
        }
        write(d);
        return;
    case 0x34:
    case 0x36:
    case 0x37:
    case 0x3C:
    case 0x3E:
    case 0x3F: {
        // PSLLH / PSRLH / PSRAH / PSLLW / PSRLW / PSRAW
        const unsigned shift = sub;
        for (int k = 0; k < 8 && funct < 0x38; ++k) {
            const uint16_t x = t.h(k);
            const unsigned amount = shift & 15;
            d.setH(k, funct == 0x34 ? uint16_t(x << amount) : funct == 0x36 ? uint16_t(x >> amount) : uint16_t(int16_t(x) >> amount));
        }
        for (int k = 0; k < 4 && funct >= 0x3C; ++k) {
            const uint32_t x = t.w(k);
            d.setW(k, funct == 0x3C ? (x << shift) : funct == 0x3E ? (x >> shift) : uint32_t(int32_t(x) >> shift));
        }
        write(d);
        return;
    }
    default:
        unknown();
    }
}

void Machine::cop1(uint32_t code, uint32_t next) {
    const unsigned format = (code >> 21) & 31;
    const unsigned ft = (code >> 16) & 31;
    const unsigned fs = (code >> 11) & 31;
    const unsigned fd = (code >> 6) & 31;
    const unsigned funct = code & 63;
    if (format == 0x00) {
        set32(ft, fpr[fs]);
        return;
    }
    if (format == 0x02) {
        set32(ft, fs >= 16 ? (fcc ? 0x01800001u : 0x01000001u) : 0x2E30u);
        return;
    }
    if (format == 0x04) {
        fpr[fs] = u32(ft);
        return;
    }
    if (format == 0x06) {
        if (fs == 31) {
            fcc = (u32(ft) & 0x00800000u) != 0;
        }
        return;
    }
    if (format == 0x08) {
        const uint32_t target = next + (uint32_t(int32_t(int16_t(code & 0xFFFF))) << 2);
        const bool wantTrue = (ft & 1) != 0;
        const bool likely = (ft & 2) != 0;
        branch(fcc == wantTrue, target, likely);
        return;
    }
    if (format == 0x14 && funct == 0x20) {
        fpr[fd] = intToFloatBits(int32_t(fpr[fs]));
        logFpu("cvt.s.w", fpr[fs], 0, 0, fpr[fd]);
        return;
    }
    if (format != 0x10) {
        throw Stop("unknown COP1 " + hex(code) + " at " + hex(pc));
    }
    const uint32_t a = fpr[fs];
    const uint32_t b = fpr[ft];
    uint32_t result = 0;
    switch (funct) {
    case 0x00:
        result = eeAdd(a, b, false);
        logFpu("add.s", a, b, 0, result);
        fpr[fd] = result;
        break;
    case 0x01:
        result = eeAdd(a, b, true);
        logFpu("sub.s", a, b, 0, result);
        fpr[fd] = result;
        break;
    case 0x02:
        result = eeMul(a, b);
        logFpu("mul.s", a, b, 0, result);
        fpr[fd] = result;
        break;
    case 0x03:
        result = eeDiv(a, b);
        logFpu("div.s", a, b, 0, result);
        fpr[fd] = result;
        break;
    case 0x04:
        result = eeSqrt(b);
        logFpu("sqrt.s", b, 0, 0, result);
        fpr[fd] = result;
        break;
    case 0x05:
        fpr[fd] = absBits(a);
        break;
    case 0x06:
        fpr[fd] = a;
        break;
    case 0x07:
        fpr[fd] = negBits(a);
        break;
    case 0x16:
        result = eeRsqrt(a, b);
        logFpu("sqrt(rsqrt)", b, 0, 0, eeSqrt(b));
        logFpu("rsqrt.s", a, b, 0, result);
        fpr[fd] = result;
        break;
    case 0x18:
        facc = eeAdd(a, b, false);
        logFpu("adda.s", a, b, 0, facc);
        break;
    case 0x19:
        facc = eeAdd(a, b, true);
        logFpu("suba.s", a, b, 0, facc);
        break;
    case 0x1A:
        facc = eeMul(a, b);
        logFpu("mula.s", a, b, 0, facc);
        break;
    case 0x1C:
        result = eeMacLogged(facc, a, b, false);
        logFpu("madd.s", a, b, facc, result);
        fpr[fd] = result;
        break;
    case 0x1D:
        result = eeMacLogged(facc, a, b, true);
        logFpu("msub.s", a, b, facc, result);
        fpr[fd] = result;
        break;
    case 0x1E: {
        const uint32_t before = facc;
        facc = eeMacLogged(facc, a, b, false);
        logFpu("madda.s", a, b, before, facc);
        break;
    }
    case 0x1F: {
        const uint32_t before = facc;
        facc = eeMacLogged(facc, a, b, true);
        logFpu("msuba.s", a, b, before, facc);
        break;
    }
    case 0x24:
        fpr[fd] = uint32_t(floatToIntBits(a));
        logFpu("cvt.w.s", a, 0, 0, fpr[fd]);
        break;
    case 0x28:
        fpr[fd] = maxBits(a, b);
        break;
    case 0x29:
        fpr[fd] = minBits(a, b);
        break;
    case 0x30:
        fcc = false;
        break;
    case 0x32:
        fcc = equal(a, b);
        break;
    case 0x34:
        fcc = less(a, b);
        break;
    case 0x36:
        fcc = lessEqual(a, b);
        break;
    default:
        throw Stop("unknown COP1.S " + hex(code) + " at " + hex(pc));
    }
}

// ---- VU0: macro mode (COP2) and micro mode (vcallms) ----
//
// The upper (FMAC) encoding is the same in both modes; the macro COP2 word carries it in its low 26 bits. The lower
// specials (MOVE, DIV, LQI, ...) share COP2's special2 index table. Micro programs run sequentially, one pair at a time,
// which is the hardware's answer wherever its stalls cover the latency; the places they do not (a flag or Q read too soon
// after the op that produced it) are counted as hazards and reported, not modelled.

const char* laneName(int k) {
    static const char* names[] = {"x", "y", "z", "w"};
    return names[k];
}

bool laneWritten(unsigned dest, int k) {
    return (dest & (8u >> k)) != 0;
}

// The MAC flag (O U S Z nibbles, x highest in each) and the status ZSUO cause bits with their sticky copies,
// as ARMSX2's VUflags.cpp derives them from each written lane's result.
void Machine::fmacFlags(unsigned dest, const Result lanes[4]) {
    uint32_t newMac = 0;
    for (int k = 0; k < 4; ++k) {
        if (!laneWritten(dest, k)) {
            continue;
        }
        const unsigned shift = 3 - k;
        const Result& lane = lanes[k];
        if (lane.bits & signBit) {
            newMac |= 0x0010u << shift;
        }
        if (lane.overflow) {
            newMac |= 0x1000u << shift;
        } else if (lane.underflow) {
            newMac |= 0x0101u << shift;
        } else if ((lane.bits & magnitudeMask) == 0) {
            newMac |= 0x0001u << shift;
        }
    }
    mac = newMac;
    uint32_t cause = 0;
    if (mac & 0x000F) {
        cause |= 1;
    }
    if (mac & 0x00F0) {
        cause |= 2;
    }
    if (mac & 0x0F00) {
        cause |= 4;
    }
    if (mac & 0xF000) {
        cause |= 8;
    }
    status = (status & 0xFC0u) | cause | (cause << 6);
    if (inMicro) {
        flagEvents.push_back({cycle + 4, mac, status, clip});
    }
}

void Machine::cop2(uint32_t code, uint32_t next) {
    const unsigned format = (code >> 21) & 31;
    const unsigned rt = (code >> 16) & 31;
    const unsigned rd = (code >> 11) & 31;
    (void)next;
    if (format == 0x01) {
        // QMFC2
        uint64_t value[2];
        std::memcpy(value, vf[rd], 16);
        if (rt) {
            gpr[rt][0] = value[0];
            gpr[rt][1] = value[1];
        }
        return;
    }
    if (format == 0x05) {
        // QMTC2
        if (rd) {
            std::memcpy(vf[rd], gpr[rt], 16);
        }
        return;
    }
    if (format == 0x02) {
        set32(rt, readControl(rd));
        return;
    }
    if (format == 0x06) {
        writeControl(rd, u32(rt));
        return;
    }
    if (format == 0x08) {
        throw Stop("bc2 at " + hex(pc));
    }
    if (format < 0x10) {
        throw Stop("unknown COP2 " + hex(code) + " at " + hex(pc));
    }
    const unsigned funct = code & 63;
    if (funct < 0x30) {
        fmacUpper(code);
    } else if (funct <= 0x35) {
        integerOp(code);
    } else if (funct == 0x38) {
        runMicro((code >> 6) & 0x7FFF);
    } else if (funct == 0x39) {
        runMicro(cmsar0);
    } else if (funct >= 0x3C) {
        const unsigned index = (code & 3) | ((code >> 4) & 0x7C);
        if (index < 0x30) {
            fmacSpecial2(code, index);
        } else {
            lowerSpecial2(code, index);
        }
    } else {
        throw Stop("unknown COP2 macro " + hex(code) + " at " + hex(pc));
    }
}

// CFC2 / CTC2 registers: VI0-15, status 16, MAC 17, clip 18, R 20, I 21, Q 22, CMSAR0 27.
uint32_t Machine::readControl(unsigned index) {
    if (index < 16) {
        return vi[index];
    }
    switch (index) {
    case 16:
        return status;
    case 17:
        return mac;
    case 18:
        return clip;
    case 20:
        return r;
    case 21:
        return i;
    case 22:
        return q;
    case 27:
        return cmsar0;
    default:
        throw Stop("cfc2 of control register " + std::to_string(index) + " at " + hex(pc));
    }
}

void Machine::writeControl(unsigned index, uint32_t value) {
    if (index == 0) {
        return;
    }
    if (index < 16) {
        vi[index] = uint16_t(value);
        return;
    }
    switch (index) {
    case 16:
        // Only the sticky bits are writable.
        status = (status & 0x3Fu) | (value & 0xFC0u);
        return;
    case 17:
        return;
    case 18:
        clip = value & 0xFFFFFFu;
        return;
    case 20:
        r = value;
        return;
    case 21:
        i = value;
        return;
    case 22:
        q = value;
        return;
    case 27:
        cmsar0 = value & 0xFFFF;
        return;
    default:
        throw Stop("ctc2 to control register " + std::to_string(index) + " at " + hex(pc));
    }
}

// Upper ops through funct 0x2F (special1): the broadcast, Q / I and full-vector forms of ADD SUB MADD MSUB MAX MINI MUL,
// and OPMSUB.
void Machine::fmacUpper(uint32_t code) {
    const unsigned dest = (code >> 21) & 15;
    const unsigned ft = (code >> 16) & 31;
    const unsigned fs = (code >> 11) & 31;
    const unsigned fd = (code >> 6) & 31;
    const unsigned funct = code & 63;
    uint32_t out[4];
    std::memcpy(out, vf[fd], 16);
    Result lanes[4] = {};
    bool setsFlags = true;
    for (int k = 0; k < 4; ++k) {
        if (!laneWritten(dest, k)) {
            continue;
        }
        uint32_t a = vf[fs][k];
        uint32_t b = vf[ft][k];
        if (funct < 0x1C) {
            b = vf[ft][funct & 3];
        } else if (funct == 0x1C || funct == 0x20 || funct == 0x21 || funct == 0x24 || funct == 0x25) {
            b = qValue();
        } else if (funct == 0x1D || funct == 0x1E || funct == 0x1F || funct == 0x22 || funct == 0x23 || funct == 0x26 || funct == 0x27) {
            b = i;
        }
        const unsigned group = funct < 0x1C ? (funct >> 2) : funct;
        Result result = {0, false, false};
        const char* name = "?";
        uint32_t accumulator = 0;
        switch (group) {
        case 0x00:
        case 0x20:
        case 0x22:
        case 0x28:
            result = vuAddSub(a, b, false);
            name = "vadd";
            break;
        case 0x01:
        case 0x24:
        case 0x26:
        case 0x2C:
            result = vuAddSub(a, b, true);
            name = "vsub";
            break;
        case 0x02:
        case 0x21:
        case 0x23:
        case 0x29:
            accumulator = vacc[k];
            result = macLogged(vacc[k], a, b, false);
            name = "vmadd";
            break;
        case 0x03:
        case 0x25:
        case 0x27:
        case 0x2D:
            accumulator = vacc[k];
            result = macLogged(vacc[k], a, b, true);
            name = "vmsub";
            break;
        case 0x04:
        case 0x1D:
        case 0x2B:
            result = {maxBits(a, b), false, false};
            name = "vmax";
            setsFlags = false;
            break;
        case 0x05:
        case 0x1F:
        case 0x2F:
            result = {minBits(a, b), false, false};
            name = "vmini";
            setsFlags = false;
            break;
        case 0x06:
        case 0x1C:
        case 0x1E:
        case 0x2A:
            result = vuMul(a, b);
            name = "vmul";
            break;
        case 0x2E: {
            // OPMSUB: fd = ACC - fs.yzx * ft.zxy; the w lane's multiplicand is +0.
            static const int rotateS[4] = {1, 2, 0, 3};
            static const int rotateT[4] = {2, 0, 1, 3};
            a = vf[fs][rotateS[k]];
            b = k == 3 ? 0u : vf[ft][rotateT[k]];
            accumulator = vacc[k];
            result = macLogged(vacc[k], a, b, true);
            name = "vopmsub";
            break;
        }
        default:
            throw Stop("unimplemented VU0 upper " + hex(code) + " at " + hex(pc));
        }
        char label[24];
        std::snprintf(label, sizeof label, "%s.%s", name, laneName(k));
        logFpu(label, a, b, accumulator, result.bits);
        lanes[k] = result;
        out[k] = result.bits;
    }
    if (setsFlags) {
        fmacFlags(dest, lanes);
    }
    if (fd) {
        std::memcpy(vf[fd], out, 16);
    }
}

// Special2 indices below 0x30: the ACC-writing FMAC forms, conversions, ABS, CLIP and NOP.
void Machine::fmacSpecial2(uint32_t code, unsigned index) {
    const unsigned dest = (code >> 21) & 15;
    const unsigned ft = (code >> 16) & 31;
    const unsigned fs = (code >> 11) & 31;
    Result lanes[4] = {};
    auto accumulate = [&](const char* name, auto&& op) {
        for (int k = 0; k < 4; ++k) {
            if (!laneWritten(dest, k)) {
                continue;
            }
            uint32_t a = 0;
            uint32_t b = 0;
            const uint32_t before = vacc[k];
            const Result result = op(k, a, b);
            char label[24];
            std::snprintf(label, sizeof label, "%s.%s", name, laneName(k));
            logFpu(label, a, b, before, result.bits);
            lanes[k] = result;
            vacc[k] = result.bits;
        }
        fmacFlags(dest, lanes);
    };
    const unsigned bc = index & 3;
    switch (index) {
    case 0x00:
    case 0x01:
    case 0x02:
    case 0x03:
    case 0x28:
        accumulate("vadda", [&](int k, uint32_t& a, uint32_t& b) {
            a = vf[fs][k];
            b = index == 0x28 ? vf[ft][k] : vf[ft][bc];
            return vuAddSub(a, b, false);
        });
        return;
    case 0x04:
    case 0x05:
    case 0x06:
    case 0x07:
    case 0x2C:
        accumulate("vsuba", [&](int k, uint32_t& a, uint32_t& b) {
            a = vf[fs][k];
            b = index == 0x2C ? vf[ft][k] : vf[ft][bc];
            return vuAddSub(a, b, true);
        });
        return;
    case 0x08:
    case 0x09:
    case 0x0A:
    case 0x0B:
    case 0x29:
        accumulate("vmadda", [&](int k, uint32_t& a, uint32_t& b) {
            a = vf[fs][k];
            b = index == 0x29 ? vf[ft][k] : vf[ft][bc];
            return macLogged(vacc[k], a, b, false);
        });
        return;
    case 0x0C:
    case 0x0D:
    case 0x0E:
    case 0x0F:
    case 0x2D:
        accumulate("vmsuba", [&](int k, uint32_t& a, uint32_t& b) {
            a = vf[fs][k];
            b = index == 0x2D ? vf[ft][k] : vf[ft][bc];
            return macLogged(vacc[k], a, b, true);
        });
        return;
    case 0x18:
    case 0x19:
    case 0x1A:
    case 0x1B:
    case 0x2A:
        accumulate("vmula", [&](int k, uint32_t& a, uint32_t& b) {
            a = vf[fs][k];
            b = index == 0x2A ? vf[ft][k] : vf[ft][bc];
            return vuMul(a, b);
        });
        return;
    case 0x1C:
        accumulate("vmulaq", [&](int k, uint32_t& a, uint32_t& b) {
            a = vf[fs][k];
            b = qValue();
            return vuMul(a, b);
        });
        return;
    case 0x1E:
        accumulate("vmulai", [&](int k, uint32_t& a, uint32_t& b) {
            a = vf[fs][k];
            b = i;
            return vuMul(a, b);
        });
        return;
    case 0x20:
    case 0x22:
        accumulate("vadda", [&](int k, uint32_t& a, uint32_t& b) {
            a = vf[fs][k];
            b = index == 0x20 ? qValue() : i;
            return vuAddSub(a, b, false);
        });
        return;
    case 0x24:
    case 0x26:
        accumulate("vsuba", [&](int k, uint32_t& a, uint32_t& b) {
            a = vf[fs][k];
            b = index == 0x24 ? qValue() : i;
            return vuAddSub(a, b, true);
        });
        return;
    case 0x21:
    case 0x23:
        accumulate("vmadda", [&](int k, uint32_t& a, uint32_t& b) {
            a = vf[fs][k];
            b = index == 0x21 ? qValue() : i;
            return macLogged(vacc[k], a, b, false);
        });
        return;
    case 0x25:
    case 0x27:
        accumulate("vmsuba", [&](int k, uint32_t& a, uint32_t& b) {
            a = vf[fs][k];
            b = index == 0x25 ? qValue() : i;
            return macLogged(vacc[k], a, b, true);
        });
        return;
    case 0x2E: {
        // OPMULA: ACC = fs.yzx * ft.zxy (the w lane multiplies by +0).
        static const int rotateS[4] = {1, 2, 0, 3};
        static const int rotateT[4] = {2, 0, 1, 3};
        accumulate("vopmula", [&](int k, uint32_t& a, uint32_t& b) {
            a = vf[fs][rotateS[k]];
            b = k == 3 ? 0u : vf[ft][rotateT[k]];
            return vuMul(a, b);
        });
        return;
    }
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17: {
        // ITOF0/4/12/15 and FTOI0/4/12/15: the fixed-point forms scale by 2^-4 / 2^-12 / 2^-15 (an exact power of two).
        static const unsigned fraction[4] = {0, 4, 12, 15};
        const unsigned bits = fraction[index & 3];
        for (int k = 0; k < 4; ++k) {
            if (!laneWritten(dest, k) || !ft) {
                continue;
            }
            const uint32_t source = vf[fs][k];
            if (index < 0x14) {
                uint32_t value = intToFloatBits(int32_t(source));
                if (bits && (value & 0x7F800000u)) {
                    value -= bits << 23;
                }
                vf[ft][k] = value;
            } else {
                uint32_t scaled = source;
                if (bits && (scaled & 0x7F800000u)) {
                    scaled = mulBits(source, (127u + bits) << 23);
                }
                vf[ft][k] = uint32_t(floatToIntBits(scaled));
            }
        }
        return;
    }
    case 0x1D:
        for (int k = 0; k < 4; ++k) {
            if (laneWritten(dest, k) && ft) {
                vf[ft][k] = absBits(vf[fs][k]);
            }
        }
        return;
    case 0x1F: {
        // CLIP: fs.xyz against |ft.w|, six judgement bits shifted into the 24-bit clip flag.
        uint32_t limit = vf[ft][3];
        limit = (limit & 0x7F800000u) ? (limit & magnitudeMask) : 0x007FFFFFu;
        uint32_t flags = 0;
        for (int k = 0; k < 3; ++k) {
            if (int32_t(vf[fs][k] & magnitudeMask) > int32_t(limit) && !(vf[fs][k] & signBit)) {
                flags |= 1u << (2 * k);
            }
            if (int32_t(vf[fs][k] & magnitudeMask) > int32_t(limit) && (vf[fs][k] & signBit)) {
                flags |= 2u << (2 * k);
            }
        }
        clip = ((clip << 6) | flags) & 0xFFFFFFu;
        if (inMicro) {
            flagEvents.push_back({cycle + 4, mac, status, clip});
        }
        return;
    }
    case 0x2F:
        return;
    default:
        throw Stop("unimplemented VU0 special2 index " + hex(index) + " (" + hex(code) + ") at " + hex(pc));
    }
}

// IADD ISUB IADDI IAND IOR (macro funct 0x30-0x35, micro lower special funct 0x30-0x35).
void Machine::integerOp(uint32_t code) {
    const unsigned funct = code & 63;
    const unsigned it = (code >> 16) & 15;
    const unsigned is = (code >> 11) & 15;
    const unsigned id = (code >> 6) & 15;
    if (funct == 0x32) {
        const unsigned raw = (code >> 6) & 31;
        const int16_t immediate = int16_t((raw & 0x10) ? (0xFFF0 | raw) : raw);
        if (it) {
            vi[it] = uint16_t(vi[is] + immediate);
        }
        return;
    }
    uint16_t value = 0;
    switch (funct) {
    case 0x30:
        value = uint16_t(vi[is] + vi[it]);
        break;
    case 0x31:
        value = uint16_t(vi[is] - vi[it]);
        break;
    case 0x34:
        value = uint16_t(vi[is] & vi[it]);
        break;
    case 0x35:
        value = uint16_t(vi[is] | vi[it]);
        break;
    default:
        throw Stop("unknown VU integer op " + hex(code) + " at " + hex(pc));
    }
    if (id) {
        vi[id] = value;
    }
}

uint32_t vu0DataAddress(uint32_t quadword) {
    return 0x11004000u + ((quadword * 16u) & 0xFFFu);
}

// Special2 indices from 0x30: MOVE MR32 LQI SQI LQD SQD DIV SQRT RSQRT WAITQ MTIR MFIR ILWR ISWR RNEXT RGET RINIT RXOR.
void Machine::lowerSpecial2(uint32_t code, unsigned index) {
    const unsigned dest = (code >> 21) & 15;
    const unsigned ft = (code >> 16) & 31;
    const unsigned fs = (code >> 11) & 31;
    const unsigned fsf = (code >> 21) & 3;
    const unsigned ftf = (code >> 23) & 3;
    const unsigned it = ft & 15;
    const unsigned is = fs & 15;
    auto lanes = [&](auto&& body) {
        for (int k = 0; k < 4; ++k) {
            if (laneWritten(dest, k)) {
                body(k);
            }
        }
    };
    switch (index) {
    case 0x30:
        lanes([&](int k) {
            if (ft) {
                vf[ft][k] = vf[fs][k];
            }
        });
        return;
    case 0x31: {
        const uint32_t rotated[4] = {vf[fs][1], vf[fs][2], vf[fs][3], vf[fs][0]};
        lanes([&](int k) {
            if (ft) {
                vf[ft][k] = rotated[k];
            }
        });
        return;
    }
    case 0x34:
    case 0x36: {
        // LQI / LQD: ft = data[vi[is]] (LQD decrements first).
        if (index == 0x36 && is) {
            vi[is]--;
        }
        uint32_t current[4];
        std::memcpy(current, locate(vu0DataAddress(vi[is]), 16, false), 16);
        lanes([&](int k) {
            if (ft) {
                vf[ft][k] = current[k];
            }
        });
        if (index == 0x34 && is) {
            vi[is]++;
        }
        return;
    }
    case 0x35:
    case 0x37: {
        // SQI / SQD: data[vi[it]] = fs.
        if (index == 0x37 && it) {
            vi[it]--;
        }
        const uint32_t address = vu0DataAddress(vi[it]);
        uint32_t current[4];
        std::memcpy(current, locate(address, 16, false), 16);
        lanes([&](int k) {
            current[k] = vf[fs][k];
        });
        std::memcpy(locate(address, 16, true), current, 16);
        if (index == 0x35 && it) {
            vi[it]++;
        }
        return;
    }
    case 0x38: {
        const uint32_t a = vf[fs][fsf];
        const uint32_t b = vf[ft][ftf];
        status &= ~0x30u;
        if ((b & 0x7F800000u) == 0) {
            status |= ((a & 0x7F800000u) == 0) ? 0x10u : 0x20u;
        }
        status |= (status & 0x30u) << 6;
        qOld = qValue();
        q = vuDiv(a, b);
        qReadyCycle = inMicro ? cycle + 7 : 0;
        logFpu("vdiv", a, b, 0, q);
        return;
    }
    case 0x39: {
        const uint32_t b = vf[ft][ftf];
        status &= ~0x30u;
        if (b & signBit) {
            status |= 0x10u;
        }
        status |= (status & 0x30u) << 6;
        qOld = qValue();
        q = vuSqrt(b);
        qReadyCycle = inMicro ? cycle + 7 : 0;
        logFpu("vsqrt", b, 0, 0, q);
        return;
    }
    case 0x3A: {
        const uint32_t a = vf[fs][fsf];
        const uint32_t b = vf[ft][ftf];
        status &= ~0x30u;
        if (b & signBit) {
            status |= 0x10u;
        }
        if ((b & 0x7F800000u) == 0) {
            status |= ((a & 0x7F800000u) == 0) ? 0x10u : 0x20u;
        }
        status |= (status & 0x30u) << 6;
        qOld = qValue();
        q = vuRsqrt(a, b);
        logFpu("sqrt(rsqrt)", b, 0, 0, vuSqrt(b));
        qReadyCycle = inMicro ? cycle + 13 : 0;
        logFpu("vrsqrt", a, b, 0, q);
        return;
    }
    case 0x3B:
        // WAITQ: the stall is taken when the pair issues.
        return;
    case 0x3C:
        if (it) {
            vi[it] = uint16_t(vf[fs][fsf]);
        }
        return;
    case 0x3D:
        lanes([&](int k) {
            if (ft) {
                vf[ft][k] = uint32_t(int32_t(int16_t(vi[is])));
            }
        });
        return;
    case 0x3E: {
        // ILWR: vi[it] = the low 16 bits of the dest lane at data[vi[is]].
        uint32_t current[4];
        std::memcpy(current, locate(vu0DataAddress(vi[is]), 16, false), 16);
        for (int k = 0; k < 4; ++k) {
            if (laneWritten(dest, k) && it) {
                vi[it] = uint16_t(current[k]);
            }
        }
        return;
    }
    case 0x3F: {
        const uint32_t address = vu0DataAddress(vi[is]);
        uint32_t current[4];
        std::memcpy(current, locate(address, 16, false), 16);
        lanes([&](int k) {
            current[k] = vi[it];
        });
        std::memcpy(locate(address, 16, true), current, 16);
        return;
    }
    case 0x40:
    case 0x41: {
        // RNEXT advances the VU's 23-bit generator (PCSX2's model of it); RGET reads it.
        if (index == 0x40) {
            const uint32_t x = (r >> 4) & 1;
            const uint32_t y = (r >> 22) & 1;
            r = ((r << 1) ^ (x ^ y)) & 0x7FFFFFu;
            r |= 0x3F800000u;
        }
        lanes([&](int k) {
            if (ft) {
                vf[ft][k] = r;
            }
        });
        return;
    }
    case 0x42:
        r = 0x3F800000u | (vf[fs][fsf] & 0x007FFFFFu);
        return;
    case 0x43:
        r = 0x3F800000u | ((r ^ vf[fs][fsf]) & 0x007FFFFFu);
        return;
    default:
        throw Stop("unimplemented VU0 lower special2 index " + hex(index) + " (" + hex(code) + ") at " + hex(pc));
    }
}

// A multiply-accumulate also logs its product, which the port computes as a separate multiply.
Result Machine::macLogged(uint32_t acc, uint32_t a, uint32_t b, bool subtract) {
    logFpu("mul(mac)", a, b, 0, vuMul(a, b).bits);
    return vuMac(acc, a, b, subtract);
}

uint32_t Machine::eeMacLogged(uint32_t acc, uint32_t a, uint32_t b, bool subtract) {
    logFpu("mul(mac)", a, b, 0, eeMul(a, b));
    return eeMac(acc, a, b, subtract);
}

uint32_t Machine::qValue() const {
    if (inMicro && cycle < qReadyCycle) {
        return qOld;
    }
    return q;
}

// The flag registers as a lower op reading them at this cycle sees them: the latest event delivered by now.
uint32_t Machine::visibleMac() const {
    for (auto event = flagEvents.rbegin(); event != flagEvents.rend(); ++event) {
        if (event->time <= cycle) {
            return event->mac;
        }
    }
    return entryMac;
}

uint32_t Machine::visibleStatus() const {
    for (auto event = flagEvents.rbegin(); event != flagEvents.rend(); ++event) {
        if (event->time <= cycle) {
            return event->status;
        }
    }
    return entryStatus;
}

uint32_t Machine::visibleClip() const {
    for (auto event = flagEvents.rbegin(); event != flagEvents.rend(); ++event) {
        if (event->time <= cycle) {
            return event->clip;
        }
    }
    return entryClip;
}

void Machine::hazard(const std::string& what) {
    hazards++;
    std::printf("HAZARD micro pc %s: %s (sequential value used)\n", hex(microPc * 8).c_str(), what.c_str());
}

// The VF registers an upper op reads (a bit mask), whether it reads / writes ACC, and the VF register it writes (0: none).
struct UpperUse {
    uint32_t reads = 0;
    bool readsAcc = false;
    bool writesAcc = false;
    unsigned writes = 0;
};

UpperUse upperUse(uint32_t upper) {
    UpperUse use;
    const uint32_t code = upper & 0x07FFFFFFu;
    const unsigned ft = (code >> 16) & 31;
    const unsigned fs = (code >> 11) & 31;
    const unsigned fd = (code >> 6) & 31;
    const unsigned funct = code & 63;
    if (funct < 0x30) {
        use.reads = (1u << fs) | (1u << ft);
        const bool accumulates = (funct >= 0x08 && funct <= 0x0F) || funct == 0x21 || funct == 0x23 || funct == 0x25 || funct == 0x27 ||
                                 funct == 0x29 || funct == 0x2D || funct == 0x2E;
        use.readsAcc = accumulates;
        use.writes = fd;
        return use;
    }
    const unsigned index = (code & 3) | ((code >> 4) & 0x7C);
    if (funct >= 0x3C && index < 0x30) {
        if (index == 0x2F) {
            return use;
        }
        use.reads = (1u << fs) | (1u << ft);
        const bool toRegister = (index >= 0x10 && index <= 0x17) || index == 0x1D;
        if (toRegister) {
            use.reads = 1u << fs;
            use.writes = ft;
            return use;
        }
        if (index == 0x1F) {
            return use;
        }
        use.writesAcc = true;
        use.readsAcc = (index >= 0x08 && index <= 0x0F) || index == 0x21 || index == 0x23 || index == 0x25 || index == 0x27 || index == 0x29 || index == 0x2D;
    }
    return use;
}

struct LowerUse {
    uint32_t reads = 0;
    unsigned writes = 0;
    bool divide = false;
    bool waitQ = false;
    unsigned divideLatency = 0;
};

LowerUse lowerUse(uint32_t lower) {
    LowerUse use;
    const unsigned opcode = lower >> 25;
    const unsigned ft = (lower >> 16) & 31;
    const unsigned fs = (lower >> 11) & 31;
    if (opcode == 0x00) {
        use.writes = ft;
    } else if (opcode == 0x01) {
        use.reads = 1u << fs;
    } else if (opcode == 0x40 && (lower & 0x3C) == 0x3C) {
        const unsigned index = (lower & 3) | ((lower >> 4) & 0x7C);
        switch (index) {
        case 0x30:
        case 0x31:
            use.reads = 1u << fs;
            use.writes = ft;
            break;
        case 0x34:
        case 0x36:
        case 0x3D:
        case 0x40:
        case 0x41:
            use.writes = ft;
            break;
        case 0x35:
        case 0x37:
        case 0x3C:
        case 0x42:
        case 0x43:
            use.reads = 1u << fs;
            break;
        case 0x38:
        case 0x3A:
            use.reads = (1u << fs) | (1u << ft);
            use.divide = true;
            use.divideLatency = index == 0x38 ? 7 : 13;
            break;
        case 0x39:
            use.reads = 1u << ft;
            use.divide = true;
            use.divideLatency = 7;
            break;
        case 0x3B:
            use.waitQ = true;
            break;
        default:
            break;
        }
    }
    use.reads &= ~1u;
    if (use.writes == 0) {
        use.writes = 0;
    }
    return use;
}

void Machine::issueMicroPair(uint32_t upper, uint32_t lower) {
    uint64_t issue = cycle + 1;
    const UpperUse up = upperUse(upper);
    uint32_t reads = up.reads & ~1u;
    LowerUse low;
    if (!(upper & 0x80000000u)) {
        low = lowerUse(lower);
        reads |= low.reads;
        if (low.divide || low.waitQ) {
            issue = std::max(issue, qReadyCycle);
        }
    }
    for (unsigned reg = 1; reg < 32; ++reg) {
        if (reads & (1u << reg)) {
            issue = std::max(issue, vfReady[reg]);
        }
    }
    // ACC is forwarded inside the FMAC pipeline: a MADDA / MADD chain issues back to back, so ACC never stalls a pair.
    cycle = issue;
}

void Machine::retireMicroPair(uint32_t upper, uint32_t lower) {
    const UpperUse up = upperUse(upper);
    if (up.writes) {
        vfReady[up.writes] = cycle + 4;
    }
    if (up.writesAcc) {
        accReady = cycle + 4;
    }
    if (!(upper & 0x80000000u)) {
        const LowerUse low = lowerUse(lower);
        if (low.writes) {
            vfReady[low.writes] = cycle + 4;
        }
    }
}

// A VU0 micro program from micro address start (in 8-byte pairs) to its E bit (plus the pair after it).
void Machine::runMicro(uint32_t start) {
    if (traceCalls) {
        std::printf("call %*s%s -> vu0 micro %s\n", depth * 2, "", hex(pc).c_str(), hex(start * 8).c_str());
    }
    inMicro = true;
    microPc = start & 511;
    entryMac = mac;
    entryStatus = status;
    entryClip = clip;
    flagEvents.clear();
    bool ending = false;
    bool branchPendingMicro = false;
    uint32_t branchTargetMicro = 0;
    for (uint64_t guard = 0; guard < 1000000; ++guard) {
        uint32_t lower;
        uint32_t upper;
        std::memcpy(&lower, &microMem[microPc * 8], 4);
        std::memcpy(&upper, &microMem[microPc * 8 + 4], 4);
        pairIndex++;
        const bool takeBranch = branchPendingMicro;
        const uint32_t target = branchTargetMicro;
        branchPendingMicro = false;
        if (upper & 0x80000000u) {
            i = lower;
        }
        issueMicroPair(upper, lower);
        // Both halves read the registers as the pair found them: run the upper on the live file, the lower on a copy
        // of it as it was, and merge (a lane both write is reported).
        uint32_t before[32][4];
        std::memcpy(before, vf, sizeof before);
        executeMicroUpper(upper);
        if (!(upper & 0x80000000u)) {
            uint32_t afterUpper[32][4];
            std::memcpy(afterUpper, vf, sizeof afterUpper);
            std::memcpy(vf, before, sizeof before);
            bool branchNow = false;
            uint32_t branchTo = 0;
            executeMicroLower(lower, branchNow, branchTo);
            for (int reg = 1; reg < 32; ++reg) {
                for (int k = 0; k < 4; ++k) {
                    const bool upperWrote = afterUpper[reg][k] != before[reg][k];
                    const bool lowerWrote = vf[reg][k] != before[reg][k];
                    if (upperWrote && lowerWrote) {
                        hazard("upper and lower write vf" + std::to_string(reg) + "." + laneName(k) + " in one pair");
                    }
                    if (upperWrote && !lowerWrote) {
                        vf[reg][k] = afterUpper[reg][k];
                    }
                }
            }
            if (branchNow) {
                branchPendingMicro = true;
                branchTargetMicro = branchTo;
            }
        }
        retireMicroPair(upper, lower);
        if (ending) {
            break;
        }
        if (upper & 0x40000000u) {
            ending = true;
        }
        microPc = takeBranch ? target : ((microPc + 1) & 511);
    }
    inMicro = false;
}

void Machine::executeMicroUpper(uint32_t upper) {
    const uint32_t code = upper & 0x07FFFFFFu;
    const unsigned funct = code & 63;
    if (funct < 0x30) {
        fmacUpper(code);
        return;
    }
    if (funct >= 0x3C) {
        const unsigned index = (code & 3) | ((code >> 4) & 0x7C);
        if (index < 0x30) {
            fmacSpecial2(code, index);
            return;
        }
    }
    throw Stop("unknown VU0 micro upper " + hex(upper) + " at micro " + hex(microPc * 8));
}

void Machine::executeMicroLower(uint32_t lower, bool& branchNow, uint32_t& branchTo) {
    const unsigned opcode = lower >> 25;
    const unsigned dest = (lower >> 21) & 15;
    const unsigned it = (lower >> 16) & 15;
    const unsigned is = (lower >> 11) & 15;
    const int32_t imm11 = int32_t(lower << 21) >> 21;
    const uint32_t imm12 = ((lower >> 10) & 0x800u) | (lower & 0x7FFu);
    const uint32_t imm15 = ((lower >> 10) & 0x7800u) | (lower & 0x7FFu);
    const uint32_t imm24 = lower & 0xFFFFFFu;
    const uint32_t branchTarget = (microPc + 1 + uint32_t(imm11)) & 511;
    auto setVi = [&](unsigned reg, uint32_t value) {
        if (reg) {
            vi[reg] = uint16_t(value);
        }
    };
    switch (opcode) {
    case 0x40: {
        const unsigned funct = lower & 63;
        if (funct >= 0x30 && funct <= 0x35) {
            integerOp(lower);
        } else if (funct >= 0x3C) {
            lowerSpecial2(lower, (lower & 3) | ((lower >> 4) & 0x7C));
        } else {
            throw Stop("unknown VU0 micro lower special " + hex(lower) + " at micro " + hex(microPc * 8));
        }
        return;
    }
    case 0x00: {
        // LQ ft, imm(vi[is])
        const unsigned ft = (lower >> 16) & 31;
        uint32_t current[4];
        std::memcpy(current, locate(vu0DataAddress(uint32_t(vi[is] + imm11)), 16, false), 16);
        for (int k = 0; k < 4; ++k) {
            if (laneWritten(dest, k) && ft) {
                vf[ft][k] = current[k];
            }
        }
        return;
    }
    case 0x01: {
        // SQ fs, imm(vi[it])
        const unsigned fs = (lower >> 11) & 31;
        const uint32_t address = vu0DataAddress(uint32_t(vi[it] + imm11));
        uint32_t current[4];
        std::memcpy(current, locate(address, 16, false), 16);
        for (int k = 0; k < 4; ++k) {
            if (laneWritten(dest, k)) {
                current[k] = vf[fs][k];
            }
        }
        std::memcpy(locate(address, 16, true), current, 16);
        return;
    }
    case 0x04:
    case 0x05: {
        const uint32_t address = vu0DataAddress(uint32_t(vi[is] + imm11));
        uint32_t current[4];
        std::memcpy(current, locate(address, 16, false), 16);
        for (int k = 0; k < 4; ++k) {
            if (!laneWritten(dest, k)) {
                continue;
            }
            if (opcode == 0x04) {
                setVi(it, current[k] & 0xFFFF);
            } else {
                current[k] = vi[it];
            }
        }
        if (opcode == 0x05) {
            std::memcpy(locate(address, 16, true), current, 16);
        }
        return;
    }
    case 0x08:
        setVi(it, vi[is] + imm15);
        return;
    case 0x09:
        setVi(it, vi[is] - imm15);
        return;
    case 0x10:
        setVi(1, (visibleClip() & 0xFFFFFFu) == imm24 ? 1 : 0);
        return;
    case 0x11:
        clip = imm24;
        return;
    case 0x12:
        setVi(1, (visibleClip() & imm24) ? 1 : 0);
        return;
    case 0x13:
        setVi(1, ((visibleClip() | imm24) & 0xFFFFFFu) == 0xFFFFFFu ? 1 : 0);
        return;
    case 0x14:
        setVi(it, (visibleStatus() & 0xFFFu) == imm12 ? 1 : 0);
        return;
    case 0x15:
        status = (status & 0x3Fu) | (imm12 & 0xFC0u);
        return;
    case 0x16:
        setVi(it, visibleStatus() & imm12);
        return;
    case 0x17:
        setVi(it, ((visibleStatus() | imm12) & 0xFFFu) == 0xFFFu ? 1 : 0);
        return;
    case 0x18:
        setVi(it, (visibleMac() & 0xFFFFu) == vi[is] ? 1 : 0);
        return;
    case 0x1A:
        setVi(it, visibleMac() & vi[is]);
        return;
    case 0x1B:
        setVi(it, ((visibleMac() | vi[is]) & 0xFFFFu) == 0xFFFFu ? 1 : 0);
        return;
    case 0x1C:
        setVi(it, visibleClip() & 0xFFFu);
        return;
    case 0x20:
        branchNow = true;
        branchTo = branchTarget;
        return;
    case 0x21:
        setVi(it, microPc + 2);
        branchNow = true;
        branchTo = branchTarget;
        return;
    case 0x24:
        branchNow = true;
        branchTo = vi[is] & 511;
        return;
    case 0x25:
        setVi(it, microPc + 2);
        branchNow = true;
        branchTo = vi[is] & 511;
        return;
    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
    case 0x2E:
    case 0x2F: {
        const int16_t a = int16_t(vi[is]);
        const int16_t b = int16_t(vi[it]);
        bool taken = false;
        if (opcode == 0x28) {
            taken = a == b;
        } else if (opcode == 0x29) {
            taken = a != b;
        } else if (opcode == 0x2C) {
            taken = a < 0;
        } else if (opcode == 0x2D) {
            taken = a > 0;
        } else if (opcode == 0x2E) {
            taken = a <= 0;
        } else {
            taken = a >= 0;
        }
        if (taken) {
            branchNow = true;
            branchTo = branchTarget;
        }
        return;
    }
    default:
        throw Stop("unknown VU0 micro lower " + hex(lower) + " at micro " + hex(microPc * 8));
    }
}

uint32_t parseNumber(const std::string& text) {
    return uint32_t(std::stoull(text, nullptr, 0));
}

uint32_t parseFloatBits(const std::string& text) {
    if (text.rfind("0x", 0) == 0) {
        return parseNumber(text);
    }
    const float value = std::stof(text);
    uint32_t bits;
    std::memcpy(&bits, &value, 4);
    return bits;
}

void readFile(const std::string& path, std::vector<uint8_t>& out, bool required) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        if (required) {
            throw std::runtime_error("cannot read " + path);
        }
        return;
    }
    file.read(reinterpret_cast<char*>(out.data()), std::streamsize(out.size()));
}

void writeFile(const std::string& path, const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(data.data()), std::streamsize(data.size()));
}

}

int main(int argc, char** argv) {
    Machine machine;
    std::string state;
    std::string save;
    uint32_t call = 0;
    uint64_t maxSteps = 50000000;
    uint32_t stackPointer = kStackBase + kStackSize - 0x100;
    std::vector<std::pair<uint32_t, uint32_t>> dumps;
    for (int k = 1; k < argc; ++k) {
        const std::string option = argv[k];
        auto value = [&]() -> std::string {
            if (k + 1 >= argc) {
                throw std::runtime_error("missing value for " + option);
            }
            return argv[++k];
        };
        if (option == "--state") {
            state = value();
        } else if (option == "--call") {
            call = parseNumber(value());
        } else if (option.size() == 4 && option.rfind("--a", 0) == 0) {
            machine.set64(4 + unsigned(option[3] - '0'), uint64_t(int64_t(int32_t(parseNumber(value())))));
        } else if (option == "--f12") {
            machine.fpr[12] = parseFloatBits(value());
        } else if (option == "--f13") {
            machine.fpr[13] = parseFloatBits(value());
        } else if (option == "--arith") {
            const std::string profile = value();
            if (profile != "exact" && profile != "mode1") {
                throw std::runtime_error("--arith exact|mode1");
            }
            oracle_arith::mode1 = profile == "mode1";
        } else if (option == "--trace-calls") {
            machine.traceCalls = true;
        } else if (option == "--trace-fpu") {
            machine.traceFpu = true;
        } else if (option == "--watch" || option == "--dump") {
            const std::string text = value();
            const size_t colon = text.find(':');
            const uint32_t begin = parseNumber(text.substr(0, colon));
            const uint32_t length = parseNumber(text.substr(colon + 1));
            if (option == "--watch") {
                machine.watches.push_back({begin, begin + length});
            } else {
                dumps.push_back({begin, length});
            }
        } else if (option == "--max-steps") {
            maxSteps = std::stoull(value());
        } else if (option == "--save") {
            save = value();
        } else if (option == "--sp") {
            stackPointer = parseNumber(value());
        } else {
            std::fprintf(stderr, "unknown option %s\n", option.c_str());
            return 2;
        }
    }
    if (state.empty() || !call) {
        std::fprintf(stderr, "usage: oracle --state DIR --call ADDR [options]\n");
        return 2;
    }
    if (oracle_arith::mode1 && std::fesetround(FE_TOWARDZERO) != 0) {
        throw std::runtime_error("cannot set FE_TOWARDZERO for --arith mode1");
    }
    readFile(state + "/eeMemory.bin", machine.ram, true);
    readFile(state + "/Scratchpad.bin", machine.scratch, false);
    readFile(state + "/vu0Memory.bin", machine.vu0Data, false);
    readFile(state + "/vu0MicroMem.bin", machine.microMem, false);
    machine.vf[0][3] = 0x3F800000u;
    machine.set64(28, kGp);
    machine.set64(29, stackPointer);
    machine.set64(31, kReturnSentinel);
    machine.pc = call;
    int code = 0;
    try {
        machine.run(maxSteps);
        std::printf("returned v0 %s f0 %08X steps %llu fpu-ops %llu hazards %llu\n", hex(machine.u64(2)).c_str(), machine.fpr[0],
                    (unsigned long long)machine.steps, (unsigned long long)machine.fpuOps, (unsigned long long)machine.hazards);
    } catch (const Stop& stop) {
        std::printf("STOP %s after %llu steps\n", stop.what(), (unsigned long long)machine.steps);
        code = 3;
    }
    for (const auto& [begin, length] : dumps) {
        for (uint32_t offset = 0; offset < length; offset += 4) {
            uint32_t word = 0;
            try {
                word = machine.load<uint32_t>(begin + offset);
            } catch (const Stop&) {
                break;
            }
            float asFloat;
            std::memcpy(&asFloat, &word, 4);
            std::printf("dump %s %08X %.9g\n", hex(begin + offset).c_str(), word, asFloat);
        }
    }
    if (!save.empty()) {
        writeFile(save + "/eeMemory.bin", machine.ram);
        writeFile(save + "/Scratchpad.bin", machine.scratch);
    }
    return code;
}
