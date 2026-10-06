// engine/ps2_fpu.hpp against ARMSX2's console-validated EE FPU model (its pcsx2/FPU.cpp interpreter
// and EeFpuModel), by fuzzed and exhaustive operand sweeps (docs/ps2-float.md "Validation").
// Built by tools/ps2-float/test_ps2_fpu.sh, which compiles ARMSX2's FPU.cpp from a local copy
// (GPL-3.0) beside this file. The reference rounds through host float casts, so it runs under
// FE_TOWARDZERO, the EE's FPCR; ps2_fpu.hpp is integer-only and ignores the host mode.
//
// usage: ps2_fpu_fuzz [random_per_generator] [exhaustive_scale]
#include "ps2_fpu.hpp"
#include "Common.h"
#include "EeFpuModel.h"

#include <cfenv>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace R5900::Interpreter::OpcodeImpl::COP1 {
void ADD_S();
void SUB_S();
void MUL_S();
void DIV_S();
void SQRT_S();
void RSQRT_S();
void MADD_S();
void MSUB_S();
void MAX_S();
void MIN_S();
void C_EQ();
void C_LT();
void C_LE();
void CVT_S();
void CVT_W();
}

namespace {

namespace cop1 = R5900::Interpreter::OpcodeImpl::COP1;
using namespace ssx::ps2fpu;

// fd = 0, fs = 1, ft = 2.
constexpr uint32_t kCode = (2u << 16) | (1u << 11) | (0u << 6);

uint32_t reference(void (*op)(), uint32_t fs, uint32_t ft, uint32_t acc = 0) {
    cpuRegs.code = kCode;
    fpuRegs.fpr[1].SetWord(fs);
    fpuRegs.fpr[2].SetWord(ft);
    fpuRegs.ACC.SetWord(acc);
    fpuRegs.fprc[31] = 0;
    op();
    return fpuRegs.fpr[0].Word();
}

bool referenceCondition(void (*op)(), uint32_t fs, uint32_t ft) {
    reference(op, fs, ft);
    return (fpuRegs.fprc[31] & 0x00800000u) != 0;
}

uint64_t state = 0x243F6A8885A308D3ull;

uint64_t next64() {
    state += 0x9E3779B97F4A7C15ull;
    uint64_t z = state;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

uint32_t next32() {
    return uint32_t(next64());
}

struct Counter {
    std::string name;
    uint64_t rows = 0;
    uint64_t bad = 0;
};

Counter counters[64];
int counterCount = 0;

Counter& counter(const char* name) {
    for (int i = 0; i < counterCount; ++i) {
        if (counters[i].name == name) {
            return counters[i];
        }
    }
    counters[counterCount].name = name;
    return counters[counterCount++];
}

void expect(const char* name, uint32_t a, uint32_t b, uint32_t c, uint32_t want, uint32_t got) {
    Counter& tally = counter(name);
    tally.rows++;
    if (want != got) {
        if (tally.bad < 8) {
            std::printf("MISMATCH %s [%08X %08X %08X]: armsx2 %08X, ps2_fpu %08X\n", name, a, b, c, want, got);
        }
        tally.bad++;
    }
}

// Operands of the shapes game code produces and the edge shapes the rules turn on.
uint32_t shapedOperand(int shape) {
    const uint32_t sign = next32() & signBit;
    switch (shape) {
    case 0:
        // Any word at all.
        return next32();
    case 1: {
        // Ordinary magnitudes (2^-20 .. 2^20).
        const uint32_t exponent = 107u + next32() % 41u;
        return sign | (exponent << 23) | (next32() & mantissaMask);
    }
    case 2: {
        // Short significands: constants such as 0.5, 1.5, 60, 0.75.
        const uint32_t exponent = 107u + next32() % 41u;
        const uint32_t bits = next32() % 6u;
        const uint32_t mantissa = (next32() & mantissaMask) & ~((1u << (23 - bits * 3)) - 1u);
        return sign | (exponent << 23) | mantissa;
    }
    case 3: {
        // The range ends: exponents 0, 1, 2, 253..255.
        static constexpr uint32_t exponents[] = {0, 1, 2, 253, 254, 255};
        const uint32_t exponent = exponents[next32() % 6u];
        return sign | (exponent << 23) | (next32() & mantissaMask);
    }
    default: {
        // Long runs of ones / zeros in the mantissa (Booth digit patterns).
        const uint32_t exponent = 100u + next32() % 55u;
        const uint32_t run = next32() % 24u;
        const uint32_t mantissa = next32() & 1u ? (mantissaMask >> run) : (mantissaMask & ~(mantissaMask >> run));
        return sign | (exponent << 23) | (mantissa ^ (next32() & 3u));
    }
    }
}

// The second operand near the first's exponent, where the guard mask and cancellation matter.
uint32_t nearOperand(uint32_t first) {
    const int exponent = int(exponentOf(first)) + int(next32() % 7u) - 3;
    const uint32_t clamped = exponent < 0 ? 0u : (exponent > 255 ? 255u : uint32_t(exponent));
    return (next32() & signBit) | (clamped << 23) | (next32() & mantissaMask);
}

void compareAll(uint32_t a, uint32_t b, uint32_t acc) {
    expect("add", a, b, 0, reference(cop1::ADD_S, a, b), addBits(a, b));
    expect("add fast path", a, b, 0, addBits(a, b), addSubFast(a, b, false));
    expect("sub fast path", a, b, 0, subBits(a, b), addSubFast(a, b, true));
    expect("mul fast path", a, b, 0, mulBits(a, b), mulFast(a, b));
    expect("sub", a, b, 0, reference(cop1::SUB_S, a, b), subBits(a, b));
    expect("mul", a, b, 0, reference(cop1::MUL_S, a, b), mulBits(a, b));
    expect("div", a, b, 0, reference(cop1::DIV_S, a, b), divBits(a, b));
    expect("sqrt", b, 0, 0, reference(cop1::SQRT_S, a, b), sqrtBits(b));
    expect("rsqrt", a, b, 0, reference(cop1::RSQRT_S, a, b), rsqrtBits(a, b));
    expect("madd (EE)", a, b, acc, reference(cop1::MADD_S, a, b, acc), eeMulAccumulateBits(acc, a, b, false));
    expect("msub (EE)", a, b, acc, reference(cop1::MSUB_S, a, b, acc), eeMulAccumulateBits(acc, a, b, true));
    const EeFpuModel::Accumulate vuAdd = EeFpuModel::MulAccumulate(acc, a, b, false);
    const EeFpuModel::Accumulate vuSub = EeFpuModel::MulAccumulate(acc, a, b, true);
    expect("madd (VU)", a, b, acc, vuAdd.result.bits, vuMulAccumulateBits(acc, a, b, false));
    expect("msub (VU)", a, b, acc, vuSub.result.bits, vuMulAccumulateBits(acc, a, b, true));
    const EeFpuModel::Result vuAddSub = EeFpuModel::AddSub(a, b, false);
    const Result mine = addSubResult(a, b, false);
    expect("add flags", a, b, 0, (vuAddSub.overflow ? 2u : 0u) | (vuAddSub.underflow ? 1u : 0u),
           (mine.overflow ? 2u : 0u) | (mine.underflow ? 1u : 0u));
    const EeFpuModel::Result vuMul = EeFpuModel::Mul(a, b);
    const Result mineMul = mulResult(a, b);
    expect("mul flags", a, b, 0, (vuMul.overflow ? 2u : 0u) | (vuMul.underflow ? 1u : 0u),
           (mineMul.overflow ? 2u : 0u) | (mineMul.underflow ? 1u : 0u));
    expect("max", a, b, 0, reference(cop1::MAX_S, a, b), maxBits(a, b));
    expect("min", a, b, 0, reference(cop1::MIN_S, a, b), minBits(a, b));
    expect("c.eq", a, b, 0, referenceCondition(cop1::C_EQ, a, b), equal(a, b));
    expect("c.lt", a, b, 0, referenceCondition(cop1::C_LT, a, b), less(a, b));
    expect("c.le", a, b, 0, referenceCondition(cop1::C_LE, a, b), lessEqual(a, b));
    // CVT.S.W / CVT.W.S read fs.
    expect("cvt.s.w", a, 0, 0, reference(cop1::CVT_S, a, b), intToFloatBits(int32_t(a)));
    expect("cvt.w.s", a, 0, 0, reference(cop1::CVT_W, a, b), uint32_t(floatToIntBits(a)));
}

void randomSweep(uint64_t perGenerator) {
    for (int shapeA = 0; shapeA < 5; ++shapeA) {
        for (int shapeB = 0; shapeB < 5; ++shapeB) {
            for (uint64_t i = 0; i < perGenerator; ++i) {
                const uint32_t a = shapedOperand(shapeA);
                const uint32_t b = (i & 1) ? nearOperand(a) : shapedOperand(shapeB);
                const uint32_t acc = (i & 2) ? nearOperand(mulBits(a, b)) : shapedOperand(shapeA);
                compareAll(a, b, acc);
            }
        }
    }
}

// Every ft significand against a set of fs significands, both orders: the multiplier deficit.
void multiplySweep(uint32_t fixedCount) {
    for (uint32_t k = 0; k < fixedCount; ++k) {
        const uint32_t fixed = k == 0 ? 0x3F800000u : (k == 1 ? 0x3F000000u : (0x3F800000u | (next32() & mantissaMask)));
        for (uint32_t m = 0; m <= mantissaMask; ++m) {
            const uint32_t variable = 0x3F800000u | m;
            expect("mul sweep (fixed fs)", fixed, variable, 0, reference(cop1::MUL_S, fixed, variable), mulBits(fixed, variable));
            expect("mul sweep (fixed ft)", variable, fixed, 0, reference(cop1::MUL_S, variable, fixed), mulBits(variable, fixed));
            expect("mul fast sweep", fixed, variable, 0, mulBits(fixed, variable), mulFast(fixed, variable));
        }
    }
}

// Every numerator significand against a set of divisors (the shortcut and the recurrence).
void divideSweep(uint32_t divisorCount) {
    for (uint32_t k = 0; k < divisorCount; ++k) {
        const uint32_t divisor = k == 0 ? 0x3F800000u : (k == 1 ? 0x3FFFFFFFu : (0x3F800000u | (next32() & mantissaMask)));
        for (uint32_t m = 0; m <= mantissaMask; ++m) {
            const uint32_t numerator = 0x3F800000u | m;
            expect("div sweep", numerator, divisor, 0, reference(cop1::DIV_S, numerator, divisor), divBits(numerator, divisor));
        }
    }
}

// Every significand at both exponent parities, plus every exponent of a few significands.
void sqrtSweep() {
    for (uint32_t parity = 0; parity < 2; ++parity) {
        for (uint32_t m = 0; m <= mantissaMask; ++m) {
            const uint32_t word = ((126u + parity) << 23) | m;
            expect("sqrt sweep", word, 0, 0, reference(cop1::SQRT_S, 0, word), sqrtBits(word));
            expect("rsqrt sweep", 0x3F800000u, word, 0, reference(cop1::RSQRT_S, 0x3F800000u, word), rsqrtBits(0x3F800000u, word));
        }
    }
    for (uint32_t exponent = 0; exponent < 256; ++exponent) {
        for (uint32_t k = 0; k < 4096; ++k) {
            const uint32_t word = (next32() & signBit) | (exponent << 23) | (next32() & mantissaMask);
            expect("sqrt exponents", word, 0, 0, reference(cop1::SQRT_S, 0, word), sqrtBits(word));
        }
    }
}

// Every int32, sampled with a stride, through CVT.S.W.
void convertSweep(uint32_t stride) {
    for (uint64_t i = 0; i <= 0xFFFFFFFFull; i += stride) {
        const uint32_t word = uint32_t(i) ^ (next32() & (stride - 1));
        expect("cvt.s.w sweep", word, 0, 0, reference(cop1::CVT_S, word, 0), intToFloatBits(int32_t(word)));
        expect("cvt.w.s sweep", word, 0, 0, reference(cop1::CVT_W, word, 0), uint32_t(floatToIntBits(word)));
    }
}

}

int main(int argc, char** argv) {
    const uint64_t perGenerator = argc > 1 ? std::strtoull(argv[1], nullptr, 0) : 200000;
    const uint32_t scale = argc > 2 ? uint32_t(std::strtoul(argv[2], nullptr, 0)) : 4;
    if (std::fesetround(FE_TOWARDZERO) != 0) {
        std::printf("cannot set FE_TOWARDZERO\n");
        return 2;
    }
    randomSweep(perGenerator);
    multiplySweep(scale * 4);
    divideSweep(scale * 2);
    sqrtSweep();
    convertSweep(scale >= 4 ? 64 : 4096);
    uint64_t bad = 0;
    uint64_t rows = 0;
    for (int i = 0; i < counterCount; ++i) {
        std::printf("%-24s %12llu rows %10llu mismatches\n", counters[i].name.c_str(), (unsigned long long)counters[i].rows,
                    (unsigned long long)counters[i].bad);
        bad += counters[i].bad;
        rows += counters[i].rows;
    }
    std::printf("TOTAL %llu rows, %llu mismatches\n", (unsigned long long)rows, (unsigned long long)bad);
    return bad ? 1 : 0;
}
