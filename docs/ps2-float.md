# PS2 float arithmetic: hardware-exact EE FPU / VU0

Status 2026-10-05 (PS2 arithmetic agent). The target is the PS2 hardware, not the emulator. Contents:

1. What the hardware does, op by op, and the evidence for it.
2. What ARMSX2 computes for our captures.
3. The library, `engine/ps2_fpu.hpp`.
4. The gap between today's captures and the hardware.
5. The capture profile and the bulk re-capture.
6. The EE oracle.
7. The core swap and its open work.

## 1. The hardware, op by op

Sources:

- **ARMSX2** (the arm64 PCSX2 fork we capture with), build `2ecae98a2c` (2026-09-22).
  - `pcsx2/FPU.cpp`, `EeFpuModel.h`, `VuEfuModel.h` and `VUops.cpp` hold a console model.
  - It was measured on an SCPH-90000:
    - 100,663,296 MUL rows (all 2^23 ft significands × twelve fs values, both orders);
    - 150,994,944 DIV rows (eighteen divisors × every numerator significand);
    - 16,777,216 SQRT rows;
    - a 1147-case FP matrix;
    - a 3000-operand holdout;
    - the VU0 tables (guard mask, saturation, underflow, zero sign, MADDA, div unit).
  - Its console rows are in `tests/ctest/core/recompilers/*console*` and `autocases_*.h` (GPL-3.0).
- **unknownbrackets/ps2autotests:** `tests/cpu/ee_fpu/*.expected`, recorded on a PS2.
- **PCSX2 master** (2026-09): `FPU.cpp`, `x86/iFPU.cpp`, microVU. It has no soft-float. GitHubProUser67's PS2Float series ("Core/EE: Implements Soft-Floats for the interpreters", 2025) was never merged. ARMSX2 took its multiplier array and divide unit.
- **Play! / DobieStation:** both use host floats with clamping, so neither is a hardware reference.

| Op | Hardware rule | Evidence |
|---|---|---|
| operands | Exponent field 0 reads as a signed zero (no denormals). Exponent 255 is an ordinary binade: 0x7FFFFFFF = (2 − 2^-23)·2^128 is the maximum, and there is no Inf / NaN. | ps2autotests (`div 7FFFFFFF,7FFFFFFF = 3F800000`), ARMSX2 top-binade rows |
| ADD / SUB (EE and VU) | The smaller-exponent operand loses its low (\|Δe\| − 1) mantissa bits (one guard bit; past 24 apart only its sign survives). Then an exact add and a chop. Past 2^129 the result saturates to ±0x7FFFFFFF. A result below 2^-126 keeps its normalised mantissa with exponent field 0. Two zeros give −0 only from −0 + −0. | ARMSX2 guarded / underflow / O-U rows, 579 VU guard-mask rows |
| MUL (EE and VU) | Chopped product, then the Booth array's one-ULP deficit: one ULP low when the 48-bit product's tail below the ULP is under 2^15 and the truncated low columns change it. **Not commutative**: only ft is Booth-recoded. mul.s(1.0, x) is low for 98.4% of x; mul.s(x, 1.0) is always exact. | 100.6M MUL rows, bit-exact |
| MADD / MSUB (and A forms, VU MADD, OPMSUB) | Two roundings: the product (with its deficit), then the guarded add. An overflowing product ends the op saturated. EE: above 0x7FFFFFFF; VU: 2^129 or more. | vumadd rows (fused vs two roundings), ps2autotests madd / msub |
| DIV / SQRT | A radix-2 SRT digit recurrence, no rounding step. It returns T or T+1 (T = truncation). Round-to-nearest is wrong on ~17% of random DIVs and ~26% of SQRTs. A zero divisor gives sign(a^b)\|0x7FFFFFFF. SQRT drops the sign, and SQRT(−0) is +0. | 151M DIV + 16.7M SQRT rows, 502 VU0 div-unit rows |
| RSQRT | div(fs, sqrt(\|ft\|)) through an ordinary single. A zero ft gives sign(fs)\|0x7FFFFFFF. | 2231-pair capture, ps2autotests rsqrt |
| MIN / MAX | Integer order on raw words; denormals are not flushed. | ARMSX2 min / max rows, ps2autotests |
| C.EQ / LT / LE | Ordered compare of the EE values (zeros and denormals equal, exponent 255 ordinary). | ARMSX2 compare rows |
| CVT.W.S / VFTOI0 | Truncate; from 2^31 up, saturate by sign. | ps2autotests convert |
| CVT.S.W / VITOF0 | Chop. | ps2autotests convert (the cases there are exact or chopped). Larger ints are not separately measured. |

**Still uncertain from hardware data:**

- The flag details of chained VU micro code beyond the measured rows.
- ITOF / FTOI4/12/15 rounding at large magnitudes.
- MADD whose ACC is already saturated (the ARMSX2 recompiler and interpreter differ there).

None of these showed up in SSX 3 runs so far (§6: 0 hazards).

## 2. What ARMSX2 computes for our captures

- The capture ini (`local/browser-pickups/pcsx2/inis/PCSX2.ini`) sets `fpuOverflow` / `vu0Overflow` only.
- The GameDB has no clamp entry for SLUS-20772.
- So every gated capture ran **clamp mode 1**, the recompilers' fast path:

| Op | Mode 1 | Hardware-exact? |
|---|---|---|
| EE ADD / SUB | Guard mask + host add under chop + FTZ, results clamped to FLT_MAX | yes in the normal range |
| EE MUL, MADD products | Host mul under chop, no deficit | **no** |
| EE DIV.S / SQRT.S / RSQRT.S | Host op under round-to-nearest (`FPUDiv.Roundmode = 0`) | **no** |
| VU0 ADD / SUB | Host add under chop, **no guard mask** | **no** |
| VU0 MUL / MADD | Chop, no deficit | **no** |
| VDIV / VSQRT / VRSQRT | Host op under chop | **no** |

- The port (`software_float`, `original_float`, `terrain_original`, `collision_scalar`) reproduces exactly that mode-1 path. That is why it is bit-exact to the gates.
- ARMSX2's **clamp mode 4** (`fpuOverflow` + `fpuExtraOverflow` + `fpuFullMode` + `fpuExactMode`, and the vu0 equivalents up to `vu0ExactMode`) runs the console model in the recompilers.
- With `EnableEE = false` / `EnableVU0 = false`, its interpreters run the same model with no recompiler.
- Mode-4 recompiler and interpreter rings are byte-identical on neutral-3000, hl/hl-sj-1 and score-uber.
  - event-race-ai is identical in all physics, score and RNG. It differs only in a crash-loop sound voice handle (rider +0x774, audio timing) and in one computer rider's +0xA9C 0/1 word from tick 1600 (open, low priority).
- The interpreter really ran: interpreter + mode-1 keys gives the mode-4 ring, since the interpreters have no mode-1 path.

## 3. The library: `engine/ps2_fpu.hpp`

- Integer-only and constexpr, so no host rounding mode, FTZ or FMA contraction can change a result. It gives the same bits natively and in WebAssembly.
- API: `addBits subBits mulBits divBits sqrtBits rsqrtBits eeMulAccumulateBits vuMulAccumulateBits maxBits minBits equal less lessEqual intToFloatBits floatToIntBits`, the `Result` forms with O / U, and float wrappers.

Validation:

- `tests/ps2_fpu_console.cpp`: 3785 hardware rows (ARMSX2's console tables + ps2autotests), 0 mismatches. `tools/ps2-float/extract_console_rows.py` builds the row header from local copies under `local/ps2-float/` (GPL data, not committed).
- `tests/ps2_fpu_fuzz.cpp`: against ARMSX2's own model (`FPU.cpp` compiled standalone through `local/ps2-float/shim`, run under FE_TOWARDZERO), 121.5M rows, 0 mismatches. It covers:
  - every op on shaped random operands;
  - MUL with every ft significand against fixed fs values, both orders;
  - DIV with every numerator against a set of divisors (the shortcut and the recurrence);
  - SQRT / RSQRT with every significand at both parities;
  - CVT sweeps.

Where the hardware differs from mode 1, on full-mantissa operands:

- MUL: 0.14% of pairs.
- DIV: 17% against nearest.
- SQRT: 26%.

The MUL rate depends on the operand shape:

| fs | Low (deficit) rate |
|---|---|
| 1.0, 0.5, 2, 0.25 | 98.4% |
| 1.5, 3 | 32.6% |
| 60 | 6.3% |
| any of these as ft | 0% |

So **a mul site whose ft is a short constant is provably native-RTZ**: a per-site certification with an exhaustive proof, by sweeping the 2^23 significands of the other operand.

## 4. The gap: mode 4 against the gates

`local/ps2-float/capture_mode.py` re-ran gated savestates in mode 4, and `tools/ps2-float/capture_diff.py` compared the rings (human position gap):

| Scenario | First difference | Gap |
|---|---|---|
| neutral-3000 | tick 339 (first tick) | 1.4 cm at 663, 7.3 m at 1638 |
| hl/hl-sj-1 | tick 19 (first tick); score 187 | 1.1 m at 1318, 12.9 m at the end |
| event-race-ai | tick 18–19; computer riders 182 | 16 m at 1218 |
| score-uber | tick 339; score 339 | 1.8 cm |

## 5. Capture profile and bulk re-capture

- `tools/ps2_capture.py`: `PS2_CAPTURE_FPU=mode1|exact` (default `mode1`).
  - `build` records it as the manifest's `fpu_mode`.
  - `run` follows the manifest unless the variable overrides it, and writes `fpu_mode` into the run summary.
  - The default stays `mode1` until the core swap is ready.
- Turbo speed gives the same ring as normal speed (checked byte for byte in exact mode).
- `tools/ps2-float/recapture_exact.py` re-runs every gate in `web/test-ps2-captures.mjs` in exact mode into `local/ps2-capture/runs-exact/`:
  - order: riders first, then the human gates, then the computer-rider races;
  - turbo, with a normal-speed fallback; at most 4 emulators machine-wide (`pgrep -x ARMSX2`); resumable;
  - a gate whose savestate is gone is rebuilt from its capture.json and checked: same patch list, or a mode-1 rerun byte-equal to the gate;
  - rng-order sidecars are regenerated;
  - other capture flows (booth/*, peak1-green-start, peak1-lodge-attrs) are listed as manual.
- `PS2_RUNS=<tree>` points `web/test-ps2-captures.mjs` at another capture tree.
- `tools/ps2-float/score_gates.mjs --runs DIR --core CORE.js` scores every gate one by one, with the human's first inexact tick and a per-area tally.

## 6. The EE oracle (`tools/ps2-float/ee_oracle`)

An R5900 interpreter that runs the PS2's own code from a savestate's EE memory on hardware arithmetic. It gives:

- an independent reproduction of a game tick;
- every call, and every FPU / VU0 op with operands and result;
- writes to watched ranges.

**What it models:**

- the integer ISA and the MMI ops the game uses;
- COP1 and COP2 macro;
- VU0 micro programs (vcallms / vcallmsr) with a pipeline model:
  - a pair issues a cycle after the last, later when a VF source is in flight (FMAC and lower VF writes deliver 4 cycles after issue);
  - the status / MAC / clip flags arrive 4 cycles after their op;
  - Q arrives 7 cycles after DIV / SQRT and 13 after RSQRT (an earlier read sees the old Q, WAITQ stalls);
  - ACC is forwarded with no stall;
- the sound library's thread / semaphore syscalls, answered as an idle kernel would.

**It stops with the pc on:** any other syscall, hardware registers, an unknown opcode, or a VU0 hazard it can't model (reported as HAZARD).

**Arithmetic profiles (`--arith exact|mode1`):**

- `exact` is `ps2_fpu.hpp`.
- `mode1` is the port's mode-1 helpers, ARMSX2's clamp mode 1 (§2). Use it to study today's gates.

**Validation.** Exact-mode neutral-3000, a snapshot at tick 840, two calls of the rider pass 0x128AF0:

- The capture hook inside the pass wrote the tick-841 ring record into RAM. Every word equals ARMSX2's record except what runs outside 0x128AF0: pad bookkeeping (2 words), the camera blocks, and rider +0x7C0..+0x8xx (38 words, rewritten elsewhere in the frame).
- Bit-exact: physics +0x100..+0x7BF (the +0x2E4 limit included), all 32 world bones, local poses, sequences, owner +0x200..+0x300, the computer-rider block and the shared RNG.
- Per pass: ~490k instructions, ~90k float ops and every VU0 micro call (the terrain triangle test at micro 0x6E0 reads status flags inside the FMAC latency). 0 hazards.
- The same check with `--arith mode1` on a mode-1 snapshot gives the same picture against the mode-1 gate. Exact arithmetic on that snapshot differs in 407 words.

**Use:**

```sh
tools/ps2-float/ee_oracle/build.sh                                   # -> local/ps2-float/bin/oracle
PS2_CAPTURE_FPU=exact python3 tools/ps2_capture.py run CAP.p2s OUT.bin --frames N --snap K --keep-states
python3 tools/ps2-float/ee_oracle/state.py OUT.tickT.p2s DIR          # prints: tick T rider_manager 0x…
local/ps2-float/bin/oracle --state DIR --call 0x128AF0 --a0 <rider_manager> \
    [--arith mode1] [--trace-calls] [--trace-fpu] [--watch ADDR:LEN] [--dump ADDR:LEN] [--save DIR2]
python3 tools/ps2-float/ee_oracle/vudis.py DIR/vu0MicroMem.bin 0x6E0  # read a micro program
```

- `--snap K` saves at record count K. The state lands a tick or two later; its file name and state.py print the tick T.
- The state sits at the start of tick T's rider pass (after the pad sampling), so one call of 0x128AF0 is exactly tick T for every rider:
  - 10F560 manager;
  - 120F20 / 121068 providers and controllers;
  - motion, pose and 121750 contacts;
  - 121818 progress and triggers;
  - the world pass.
- The rider manager is `*(*(*(gp-0x848)+0x84)+0xC)`; its +8 is the tick.
- Any other function can be called the same way with its own arguments (`--a0..--a3`, `--f12`, `--f13`). The oracle sets gp = 0x4A30F0, a private stack outside the PS2 map, and a sentinel ra.

**Not modelled:**

- whatever runs outside the call (pad sampling, the camera, the render side, VU1, DMA / GS);
- chaining several calls is only right where that outside work does not matter (a neutral pad, no camera reads).

## 7. The core swap

**Audit:** the core's float arithmetic goes through a small helper layer:

| Helper | Uses | Files |
|---|---|---|
| `terrain_original::*` | 2917 | 202 |
| `originalScalarAdd/Subtract` | 1586 | 178 |
| `originalScalarDivide/Sqrt` | 310 | 101 |
| `collision_scalar` | 57 | 22 |
| `software_float` directly (incl. `original_input_provider` and the LUN VM) | 51 | 5 |

- A named build of core.wasm (`local/ps2-float/build-core-names.sh`) shows native f32 add / sub / mul / div / sqrt in 279 functions. These are mostly the helpers' nearest-mode branches and the port-side presentation code (camera replay, snow, skin palette); the sim functions call the helpers.

`tools/ps2-float/make_swap_tree.py OUT` builds a scratch tree that routes the helpers onto `ps2_fpu`. A scratch core has been built from it (`local/ps2-float/core-ps2fpu`, wasm 6.27 MB vs 6.81 MB). Changing the helpers is not enough. The port has expressions that equal the PS2's under mode-1 arithmetic but not on the console:

- **Example 1:** 13D8F0 computes dt = mul.s rider+0x300 (fs) × gp-0x7064 1/60 (ft). With +0x300 = 1.0 as fs the multiplier is one ULP low (0x3C888888). `web/core.cpp` passed `timeScale/60.f` (0x3C888889), so the extra-lean / board-alignment rates (+0x20C / +0x2C0) were off. The scratch tree carries the fix; it is the physics agent's file.
- **Example 2:** every `mul(a, b)` in the port has an operand order that mode 1 never checked. Where the PS2's fs is a short constant (1.0, 0.5, 2, 3, ...) the deficit fires on most operands.
- **Next: the matcher.** A logging build records every helper call with its source location. For each tick, port results whose bits are missing from the oracle's trace for that tick (or present only with the operands swapped) name the site and what to change.
- Site fixes land in the live tree behind one compile-time switch (`SSX_PS2_EXACT_FPU`, default off), so today's build stays bit-exact to the mode-1 gates until the coordinator switches.
