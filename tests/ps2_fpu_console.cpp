// engine/ps2_fpu.hpp against PS2 hardware rows (docs/ps2-float.md "Hardware data").
// Build and run through tools/ps2-float/test_ps2_fpu.sh, which generates console_rows.hpp from
// ARMSX2's SCPH-90000 captures and unknownbrackets/ps2autotests' .expected files.
// Prints one line per mismatching row and a per-table tally; exits 1 on any mismatch.
#include "ps2_fpu.hpp"
#include "console_rows.hpp"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>

using namespace ssx::ps2fpu;

namespace {

struct Tally {
    int rows = 0;
    int bad = 0;
};

std::map<std::string, Tally> tallies;

void check(const char* table, const char* what, uint32_t want, uint32_t got) {
    Tally& tally = tallies[table];
    tally.rows++;
    if (want != got) {
        tally.bad++;
        std::printf("MISMATCH %s: %s: console %08X, ps2_fpu %08X\n", table, what, want, got);
    }
}

std::string label(const char* text, uint32_t a, uint32_t b, uint32_t c) {
    char buffer[256];
    std::snprintf(buffer, sizeof buffer, "%s [%08X %08X %08X]", text ? text : "", a, b, c);
    return buffer;
}

// ---- ARMSX2 EE FPU console tables ----

void guardedAddSub() {
    using namespace f_ee_fpu_guarded_addsub_console_tests;
    for (const ConsoleCase& row : kConsole) {
        uint32_t got = 0;
        switch (row.form) {
        case FORM_ADD:
        case FORM_ADDA:
            got = addBits(row.fs, row.ft);
            break;
        case FORM_SUB:
        case FORM_SUBA:
            got = subBits(row.fs, row.ft);
            break;
        case FORM_MADD:
        case FORM_MADDA:
            got = eeMulAccumulateBits(row.acc, row.fs, row.ft, false);
            break;
        case FORM_MSUB:
        case FORM_MSUBA:
            got = eeMulAccumulateBits(row.acc, row.fs, row.ft, true);
            break;
        }
        check("ee guarded add/sub", label("ordinal", row.ordinal, row.fs, row.ft).c_str(), row.want, got);
    }
}

void underflow() {
    using namespace f_ee_fpu_underflow_console_tests;
    for (const ConsoleCase& row : kConsole) {
        uint32_t got = 0;
        switch (row.form) {
        case FORM_ADD:
            got = addBits(row.fs, row.ft);
            break;
        case FORM_SUB:
            got = subBits(row.fs, row.ft);
            break;
        case FORM_MADD:
            got = eeMulAccumulateBits(row.acc, row.fs, row.ft, false);
            break;
        case FORM_MSUB:
            got = eeMulAccumulateBits(row.acc, row.fs, row.ft, true);
            break;
        case FORM_MUL:
            got = mulBits(row.fs, row.ft);
            break;
        case FORM_DIV:
            got = divBits(row.fs, row.ft);
            break;
        }
        check("ee underflow", label("form", row.form, row.fs, row.ft).c_str(), row.want, got);
    }
}

uint32_t topBinadeForm(int form, uint32_t fs, uint32_t ft, uint32_t acc) {
    using namespace f_ee_fpu_top_binade_console_tests;
    switch (Form(form)) {
    case F_ADD:
    case F_ADDA:
        return addBits(fs, ft);
    case F_SUB:
    case F_SUBA:
        return subBits(fs, ft);
    case F_MUL:
    case F_MULA:
        return mulBits(fs, ft);
    case F_MADD:
    case F_MADDA:
        return eeMulAccumulateBits(acc, fs, ft, false);
    case F_MSUB:
    case F_MSUBA:
        return eeMulAccumulateBits(acc, fs, ft, true);
    }
    return 0;
}

void topBinade() {
    using namespace f_ee_fpu_top_binade_console_tests;
    for (const ConsoleCase& row : kConsole) {
        const uint32_t got = topBinadeForm(row.form, row.fs, row.ft, row.acc);
        check("ee top binade", label(row.what, row.fs, row.ft, row.acc).c_str(), row.want, got);
    }
}

void ouRounding() {
    using namespace f_ee_fpu_ou_rounding_console_tests;
    for (const ConsoleCase& row : kConsole) {
        uint32_t got = 0;
        switch (row.form) {
        case FORM_ADD:
        case FORM_ADDA:
            got = addBits(row.fs, row.ft);
            break;
        case FORM_SUB:
        case FORM_SUBA:
            got = subBits(row.fs, row.ft);
            break;
        case FORM_MADD:
        case FORM_MADDA:
            got = eeMulAccumulateBits(row.acc, row.fs, row.ft, false);
            break;
        case FORM_MSUB:
        case FORM_MSUBA:
            got = eeMulAccumulateBits(row.acc, row.fs, row.ft, true);
            break;
        }
        check("ee O/U rounding", label(row.note, row.fs, row.ft, row.acc).c_str(), row.want, got);
    }
}

void minMax() {
    using namespace f_ee_fpu_minmax_console_tests;
    for (const ConsoleCase& row : kConsole) {
        const uint32_t got = row.ismin ? minBits(row.fs, row.ft) : maxBits(row.fs, row.ft);
        check("ee min/max", label(row.what, row.fs, row.ft, 0).c_str(), row.want, got);
    }
}

void compare() {
    using namespace f_ee_fpu_compare_console_tests;
    for (const ConsoleCase& row : kConsole) {
        bool condition = false;
        switch (row.cond) {
        case C_F:
            condition = false;
            break;
        case C_EQ:
            condition = equal(row.fs, row.ft);
            break;
        case C_LT:
            condition = less(row.fs, row.ft);
            break;
        case C_LE:
            condition = lessEqual(row.fs, row.ft);
            break;
        }
        const uint32_t want = (row.want_fcr31 >> 23) & 1u;
        check("ee compare", label(row.what, row.fs, row.ft, 0).c_str(), want, condition ? 1u : 0u);
    }
}

void rsqrtSign() {
    using namespace f_ee_fpu_rsqrt_sign_console_tests;
    for (const Row& row : kRows) {
        uint32_t got = 0;
        switch (row.op) {
        case kRsqrt:
            got = rsqrtBits(row.fs, row.ft);
            break;
        case kDiv:
            got = divBits(row.fs, row.ft);
            break;
        case kSqrt:
            got = sqrtBits(row.ft);
            break;
        case kSqrtDiv:
            got = divBits(row.fs, sqrtBits(row.ft));
            break;
        }
        check("ee rsqrt/div/sqrt signs", label(row.what, row.fs, row.ft, 0).c_str(), row.console, got);
    }
}

void divideUnit() {
    using namespace f_ee_fpu_divunit_console_tests;
    for (const ConsoleRow& row : kRows) {
        check("ee divide unit: sqrt", label(row.what, row.fs, row.ft, 0).c_str(), row.con_sqrt, sqrtBits(row.ft));
        check("ee divide unit: div", label(row.what, row.fs, row.ft, 0).c_str(), row.con_div, divBits(row.fs, row.ft));
        check("ee divide unit: rsqrt", label(row.what, row.fs, row.ft, 0).c_str(), row.con_rsqrt, rsqrtBits(row.fs, row.ft));
    }
}

void fpuOverflow() {
    using namespace console_fpuovf;
    for (const FpuOvfCase& row : kCases) {
        uint32_t got = 0;
        switch (row.op) {
        case FO_ADD:
        case FO_ADDA:
            got = addBits(row.fs, row.ft);
            break;
        case FO_SUB:
        case FO_SUBA:
            got = subBits(row.fs, row.ft);
            break;
        case FO_MUL:
        case FO_MULA:
            got = mulBits(row.fs, row.ft);
            break;
        case FO_DIV:
            got = divBits(row.fs, row.ft);
            break;
        case FO_SQRT:
            got = sqrtBits(row.ft);
            break;
        case FO_RSQRT:
            got = rsqrtBits(row.fs, row.ft);
            break;
        case FO_MADD:
        case FO_MADDA:
            got = eeMulAccumulateBits(row.acc, row.fs, row.ft, false);
            break;
        case FO_MSUB:
        case FO_MSUBA:
            got = eeMulAccumulateBits(row.acc, row.fs, row.ft, true);
            break;
        }
        check("ee overflow/underflow", label(row.what, row.fs, row.ft, row.acc).c_str(), row.result, got);
    }
    for (const VuOvfCase& row : kVuCases) {
        for (int lane = 0; lane < 4; ++lane) {
            uint32_t got = 0;
            switch (row.op) {
            case VO_VADD:
                got = addBits(row.fs[lane], row.ft[lane]);
                break;
            case VO_VSUB:
                got = subBits(row.fs[lane], row.ft[lane]);
                break;
            case VO_VMUL:
                got = mulBits(row.fs[lane], row.ft[lane]);
                break;
            }
            check("vu0 overflow/underflow", label(row.what, row.fs[lane], row.ft[lane], lane).c_str(), row.result[lane], got);
        }
    }
}

// ---- ARMSX2 VU0 macro console tables ----

void vuGuardMask() {
    using namespace console_vgmask;
    for (const VgMaskCase& row : kVgMaskCases) {
        const uint32_t got = row.op == VG_ADD ? addBits(row.fs, row.ft) : subBits(row.fs, row.ft);
        check("vu0 guard mask", label(row.what, row.fs, row.ft, 0).c_str(), row.out, got);
    }
}

void vuMadd() {
    using namespace console_vumadd;
    for (const VuMaddCase& row : kVuMaddCases) {
        const uint32_t got = vuMulAccumulateBits(row.acc, row.fs, row.ft, row.sub);
        check("vu0 madda/msuba", label(row.what, row.acc, row.fs, row.ft).c_str(), row.acc_out, got);
    }
}

// The vusat / vuflow captures seed ACC with vadda ACC, acc, +0.
uint32_t vuPairOp(int op, uint32_t fs, uint32_t ft, uint32_t acc, int add, int sub, int mul, int madd, int msub, int max, int mini) {
    const uint32_t seeded = addBits(acc, 0);
    if (op == add) {
        return addBits(fs, ft);
    }
    if (op == sub) {
        return subBits(fs, ft);
    }
    if (op == mul) {
        return mulBits(fs, ft);
    }
    if (op == madd) {
        return vuMulAccumulateBits(seeded, fs, ft, false);
    }
    if (op == msub) {
        return vuMulAccumulateBits(seeded, fs, ft, true);
    }
    if (op == max) {
        return maxBits(fs, ft);
    }
    if (op == mini) {
        return minBits(fs, ft);
    }
    return 0xDEADDEADu;
}

void vuSaturation() {
    using namespace console_vusat;
    for (const VuSatCase& row : kVuSatCases) {
        const uint32_t got = vuPairOp(row.op, row.fs, row.ft, row.acc, VS_ADD, VS_SUB, VS_MUL, VS_MADD, VS_MSUB, VS_MAX, VS_MINI);
        check("vu0 saturation", label(row.what, row.fs, row.ft, row.acc).c_str(), row.out, got);
    }
}

void vuUnderflow() {
    using namespace console_vuflow;
    for (const VuFlowCase& row : kVuFlowCases) {
        const uint32_t got = vuPairOp(row.op, row.fs, row.ft, row.acc, VF_ADD, VF_SUB, VF_MUL, VF_MADD, VF_MSUB, -1, -1);
        check("vu0 underflow", label(row.what, row.fs, row.ft, row.acc).c_str(), row.out, got);
    }
}

// vuzsign seeds ACC with vmulax ACC, acc, 1.0.
void vuZeroSign() {
    using namespace console_vuzsign;
    for (const VuZCase& row : kVuZCases) {
        const uint32_t seeded = mulBits(row.acc, 0x3F800000u);
        uint32_t got = 0;
        switch (row.op) {
        case VZ_NONE:
            got = seeded;
            break;
        case VZ_ADD:
            got = addBits(row.fs, row.ft);
            break;
        case VZ_SUB:
            got = subBits(row.fs, row.ft);
            break;
        case VZ_MUL:
        case VZ_MULA:
            got = mulBits(row.fs, row.ft);
            break;
        case VZ_MADD:
        case VZ_MADDA:
            got = vuMulAccumulateBits(seeded, row.fs, row.ft, false);
            break;
        case VZ_MSUB:
            got = vuMulAccumulateBits(seeded, row.fs, row.ft, true);
            break;
        }
        check("vu0 zero sign", label(row.what, row.fs, row.ft, row.acc).c_str(), row.out, got);
    }
}

void vuDivideUnit() {
    using namespace console_vurs;
    for (const VursCase& row : kVursCases) {
        uint32_t got = 0;
        switch (row.op) {
        case VURS_DIV:
            got = divBits(row.fs, row.ft);
            break;
        case VURS_SQRT:
            got = sqrtBits(row.ft);
            break;
        case VURS_RSQRT:
            got = rsqrtBits(row.fs, row.ft);
            break;
        }
        check("vu0 div/sqrt/rsqrt", label(row.tag, row.fs, row.ft, 0).c_str(), row.q, got);
    }
}

// ---- ps2autotests tests/cpu/ee_fpu ----

// The tests set ACC with adda.s acc, -0 and read it back with madd.s ACC + -0 * +0.
uint32_t accumulatorSeed(uint32_t acc) {
    return addBits(acc, 0x80000000u);
}

uint32_t accumulatorReadback(uint32_t acc) {
    return eeMulAccumulateBits(acc, 0x80000000u, 0x00000000u, false);
}

void autotests() {
    for (const AutotestRow& row : kAutotestRows) {
        const std::string op = row.op;
        const uint32_t a = row.operands[0];
        const uint32_t b = row.operands[1];
        const std::string table = std::string("ps2autotests ") + row.file + " " + op;
        const std::string what = label(op.c_str(), a, b, row.acc);
        const uint32_t acc = accumulatorSeed(row.acc);
        uint32_t got = 0;
        bool known = true;
        if (op == "abs") {
            got = absBits(a);
        } else if (op == "neg") {
            got = negBits(a);
        } else if (op == "mov") {
            got = a;
        } else if (op == "add") {
            got = addBits(a, b);
        } else if (op == "sub") {
            got = subBits(a, b);
        } else if (op == "mul") {
            got = mulBits(a, b);
        } else if (op == "div") {
            got = divBits(a, b);
        } else if (op == "sqrt") {
            got = sqrtBits(a);
        } else if (op == "rsqrt") {
            got = rsqrtBits(a, b);
        } else if (op == "max") {
            got = maxBits(a, b);
        } else if (op == "min") {
            got = minBits(a, b);
        } else if (op == "adda") {
            got = accumulatorReadback(addBits(a, b));
        } else if (op == "suba") {
            got = accumulatorReadback(subBits(a, b));
        } else if (op == "mula") {
            got = accumulatorReadback(mulBits(a, b));
        } else if (op == "madd") {
            got = eeMulAccumulateBits(acc, a, b, false);
        } else if (op == "msub") {
            got = eeMulAccumulateBits(acc, a, b, true);
        } else if (op == "madda") {
            got = accumulatorReadback(eeMulAccumulateBits(acc, a, b, false));
        } else if (op == "msuba") {
            got = accumulatorReadback(eeMulAccumulateBits(acc, a, b, true));
        } else if (op == "cvt.w.s") {
            got = uint32_t(floatToIntBits(a));
        } else if (op == "cvt.s.w") {
            got = intToFloatBits(int32_t(a));
        } else if (op == "eq") {
            got = equal(a, b) ? 1u : 0u;
        } else if (op == "lt") {
            got = less(a, b) ? 1u : 0u;
        } else if (op == "le") {
            got = lessEqual(a, b) ? 1u : 0u;
        } else if (op == "f") {
            got = 0;
        } else {
            known = false;
        }
        if (!known) {
            std::printf("UNKNOWN autotest op %s\n", row.op);
            continue;
        }
        check(table.c_str(), what.c_str(), row.result, got);
        if (row.hasResultAcc && (op == "madd" || op == "msub")) {
            check((table + " (acc)").c_str(), what.c_str(), row.resultAcc, accumulatorReadback(acc));
        }
    }
}

}

int main() {
    guardedAddSub();
    underflow();
    topBinade();
    ouRounding();
    minMax();
    compare();
    rsqrtSign();
    divideUnit();
    fpuOverflow();
    vuGuardMask();
    vuMadd();
    vuSaturation();
    vuUnderflow();
    vuZeroSign();
    vuDivideUnit();
    autotests();
    int bad = 0;
    int rows = 0;
    for (const auto& [table, tally] : tallies) {
        std::printf("%-44s %5d rows %5d mismatches\n", table.c_str(), tally.rows, tally.bad);
        bad += tally.bad;
        rows += tally.rows;
    }
    std::printf("TOTAL %d rows, %d mismatches\n", rows, bad);
    return bad ? 1 : 0;
}
