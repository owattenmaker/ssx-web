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
- Gates outside the plain `ps2_capture.py run` flow:
  - allpeak/{apr,p2r,apj}-start come from closed-loop autopilot runs. `tools/ps2_autopilot.py run STATE OUT --frames N --replay GATE.script.json` feeds the gate's consumed pads open loop instead of steering. The mode-1 replay of p2r-start equals the gate byte for byte, so the exact replays use the gate's own inputs.
  - peak3/fr-throne-unload is records 5602..7000 of a 7000-frame run. The whole script runs (the mode-1 slice equals the gate), and the exact gate is the same slice of the exact run (`fr-throne-unload.json` `slice`).
  - ctm-events/c0a-ws13-splines is an alias of c0a-ws13 (links, as in `runs/`).
- rng-order sidecars where the exact runs differ in length:
  - An exact gate one record longer than its companion: the sidecar covers the shared records. A missing record falls back in the comparer.
  - peak1/rnb-event-tuck: in exact mode the tick restarts at record 10181 in both the gate and its companion, so the sidecar covers the records before the restart.
- Still to re-capture: ctm-events/c0a-ret3 and c0a-ret2-coast (ring overrun near record 995 on a loaded machine, at both speeds) and c0a-full / c0a-full-ai (timed out).
- `PS2_RUNS=<tree>` points `web/test-ps2-captures.mjs` at another capture tree.
- `tools/ps2-float/score_gates.mjs --runs DIR --core CORE.js [--arith exact]` scores every gate one by one, with the human's first inexact tick and a per-area tally. `--arith exact` sets `PS2_ARITH=exact` for the comparers.

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
- `--sp ADDR` runs on the game's own stack instead. state.py prints the frozen sp of a snap_at.py state (from the savestate's cpuRegs). Use it when the code reads stack words it never wrote.
  - Example: event-race-ai rider+0xA9C. 0x105398 copies its sp+0x80..0xBF record to rider+0xA60 (0x10570C..0x105718) but never writes sp+0xB4..0xBF.
  - At tick 1599 the stale sp+0xBC is the last overlap flag of the 0x33B748 spatial-tree cell test in the same pass (its sp+0x1C, 0x33BAD8 / 0x33BB0C): 1 in both profiles, as in the interpreter capture.
  - It is a float-dependent stale word, so it stays out of comparisons (engine/instance_contact.hpp).

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

### The switch core

- In the live tree, behind the compile-time `SSX_PS2_EXACT_FPU` (default off; the default build is byte-identical).
  - Build: `SSX_CORE_CFLAGS=-DSSX_PS2_EXACT_FPU=1 CORE_OUT=… sh web/build-core.sh`.
  - Every helper returns early on the runtime switch `software_float::exactArithmetic` (default on in such a core).
  - Covered: software_float add / mul / div / sqrt, the EE add / sub / DIV.S / SQRT.S, terrain_original in both rounding policies, collision_scalar, the LUN VM.
  - `web/check-rider-globals.mjs` lists the switch as shared.
- Exported as `ps2_arith_exact(on)`. It also re-derives the seeded stats in the calling rider context, so the comparers call it in every context.
- `PS2_ARITH=exact` (default off) in `web/compare-ps2-capture.mjs` / `web/compare-ai-capture.mjs` runs the setup in mode 1 and turns the switch on at the capture's first tick.

**Mode-1 history.** Every baseline savestate was made in mode 1, so an exact capture carries mode-1 history up to its first tick. With `--event` the comparer simulates the grid start and countdown up to that tick itself, so those ticks must stay in mode 1 too. Only an exact-mode baseline made from a state with no float history (the title or a menu) would remove this.

### The matcher

Per tick:

1. A frozen snapshot: `snap_at.py CAP.p2s TICK OUT.p2s`.
2. The PS2 trace: `oracle --trace-fpu`, which also logs a MAC's product and RSQRT's inner SQRT.
3. The port trace: a trace core, run with `PS2_ARITH=exact PS2_MATCH_TICK=T+1 PS2_MATCH_OUT=port.json TICK_HOOK=../tools/ps2-float/trace_hook.mjs`. The comparer's row T+1 is the PS2's pass T.
   - `tools/ps2-float/make_trace_tree.py OUT` mirrors the live tree as symlinks and patches copies of the helpers, so every call records its site through `std::source_location` (`tools/ps2-float/ps2_trace.hpp`).
   - It never writes through a symlink.
   - Build the tree with `SSX_CORE_CFLAGS=-DSSX_PS2_EXACT_FPU=1 CORE_OUT=… sh OUT/web/build-core.sh`. A trace core is an instrument, never a deploy candidate.
   - Candidate fixes are tried on the tree's copies (unlink the symlink, write the edited text) before they go to the core owner.
4. `tools/ps2-float/match.py port.json oracle.fpu`.

`tools/ps2-float/match_tick.sh GATE TICK TRACE_CORE_DIR -- COMPARER_ARGS` runs steps 1-4 against `local/ps2-capture/runs-exact/GATE.bin`. It caches the snapshot and the PS2 trace under `local/ps2-float/match/`. Rider gates need `STAGE_WORLD=1`.

What match.py does:

- It classes every port call against the PS2's: match, operand **swap**, **form** (same operands, another result), **drift** (an operand a few ULPs off a PS2 operand), value, or unmatched.
- It skips world objects and presentation code by default, since they run outside 0x128AF0.
- It votes per multiply site on operand order (`--swaps OUT.json`).
- It follows the first drift's provenance back through the port calls that produced its operands (negations included) to the first call that left the PS2.

### First findings (riders/zoe-race, exact)

- **13D8F0 dt** = mul.s rider+0x300 (fs) × gp-0x7064 1/60 (ft). With fs = 1.0 the multiplier is one ULP low (0x3C888888). `web/core.cpp` computes `timeScale/60.f` (0x3C888889), so the +0x20C / +0x2C0 rates are off. This is not mode-1-neutral for timeScale ≠ 1, so it goes behind the switch.
- **Motion 3 (136958) calls 11E098 every tick**, including the countdown hold, where the port skips it. 11E098's sum is ACC + 1.0·z² + 1.0·w² (VMADDA / VMADD with 1.0 as fs), so on the console the rebuild is not an identity: Q = 3F800001 moves q by one ULP from the first exact tick. Port form: `add(add(add(sq0,sq1), mul(1.f,sq2)), mul(1.f,sq3))`. Both changes are mode-1-neutral.
- **The general rule:** wherever the port dropped a ×1.0, a 1.0×, or a normalisation because it was the identity in mode 1, the console disagrees when 1.0 is the fs operand.
- Operand-order fixes (`mul(a,b)` → `mul(b,a)`) are mode-1-neutral (the mode-1 product is commutative), so they can land without the switch.

### The VU0 forms (matcher batches 2-5, riders/zoe-race tick 186)

The console forms a vector op's products in a fixed operand order and runs sums through the MAC with vf0 (1.0) as fs, so the port must spell them out. All of these are mode-1-neutral.

| PS2 pattern | Port form | Sites (examples) |
|---|---|---|
| horizontal dot: `vmul`, `vadda x+y`, `vmadda vf0w·z`, `vmadd vf0w·w` | `add(add(x,y),mul(1.f,z))` (+ `mul(1.f,w)` for four lanes) | ground_motion dot3, orientation dotV / length, terrain dot, rider_pose dot, animation blend dot / norm, patch normal length |
| cross product `vopmula fs=a,ft=b` / `vopmsub fs=b,ft=a` | `sub(mul(a[j],b[k]),mul(b[j],a[k]))` | terrain cross, orientation crossV / axis, quaternion products, patch normal (a = dv, b = du) |
| quaternion w lane: `vsuba.w`, `vmsuba.w vf0`, `vmsub.w vf0` | `sub(sub(sub(ww,xx),mul(1.f,yy)),mul(1.f,zz))` | originalAnimationCompose (0x310310), originalRotateOrientation (0x11E060) |
| basis from a quaternion (11E098 style): every element is an ACC stage plus an FMAC with fs = 1.0 | second term `mul(1.f,x)`, cross terms start at `0 + M` | orientation rebuild, originalRiderCollisionFrame (0x11E114, 0x31018C) |
| matrix × vector `vmulax / vmadday / vmaddaz / vmaddw` (ft = the vector's lane) | the plain chain `add(add(add(c0·p0,c1·p1),c2·p2),c3·p3)` | terrain Bezier rows and derivatives (0x32EA24) |
| Bezier horizontal outputs (0x32EB28 / 0x32EDA8 / 0x32EC28) | `point = dot4h(row,vp)`, `du = dot4h(vp,derivative)`, `dv = dot4h(dvp,row)` | terrain evaluate |
| scalar `mul.s fd, sin, axis` | `mul(sin, axis[i])` | axis quaternions (0x11E024, 0x11F644) |
| scalar 1.0 factors (f22 = 1.0 in 0x13CCF0) | `mul(1.f, x)` where the code multiplies by the register | groundForwardFriction |

In this tick the drifts went 953 → 16 with batches 4 and 5. The originalAnimationCompose w lane alone took them from 621 to 147: through 0x149E730 the skeleton feeds the next pass's secondary motion.

**Mode-1 history seen in the matcher:**

- Triangle normals cached before the baseline (0x32E4EC reads B895D946, the mode-1 normal of vertices whose exact normal is B895D947).
- Records that run past the baseline's own history (peak1/rnb-event-tuck restarts its tick at 10181 in exact mode).
- Only exact-mode baselines remove these.

**Not a form:**

- start_gameplay re-encodes the decoded command axis (31 × 0x3D042108 = 3F7FFFFF → 30) where 0x128610 multiplies the raw channel (1.0 × 31).
- That loss happens in both modes, so it is a separate port fix.

**Camera forms (2026-10-07, `tools/ps2-float/match_camera.sh`):**

- The camera code uses the same horizontal dot: 1.0 is fs for the z and w products (vf6 = vaddw.x vf0, vf0w; e.g. 0x160610, 0x162604). `original_camera.hpp dot4`.
- The output eye (+0x60, 0x166F90) goes through the identity matrix at 0x4FF1A0 first (0x167004..0x167010, identity columns as fs, eye lanes broadcast as ft), so each lane is 1.0 × eye. The later rotation products carry it with 1.0 as ft (exact).
- match_camera.sh: the oracle runs pass TICK−1's rider pass 0x128AF0, saves, then traces the DEFAULT_3 update 0x176E10 on the gate's camera block. Only `original_camera*` port sites are matched.
- cam-mix-glide went from camera words at 339 to 502 with these two forms. Open: the 0x1630E0 direction (port angles vs the PS2 vector) at 502, and the swap at 0x163434 (1.0 × 3F41003C).

### Performance

Measured on a loaded machine.

Native ns per op:

| Op | software_float (mode 1) | ps2_fpu |
|---|---|---|
| add | 4.0 | 2.8 (exact double fast path) |
| mul | 1.2 | 2.3 (fast path; array only on short tails; a power-of-two fs reads an 8 KiB table) |
| div | 3.1 | 110 (SRT recurrence; a power-of-two divisor is exact) |
| sqrt | 1.4 | 79 |

Proofs behind the fast paths:

- A mul whose ft has its low 16 bits clear is never low (exhaustive).
- Division by a power of two is exact.
- With a power-of-two fs, the array's outcome depends only on ft's low 16 bits. The table equals mulResult on 1.34G pairs (8 fs × 10 exponents × all significands × both signs).
- mul(1.0, x) is low for 98.44% of x.

Six-rider race, `web/bench-sim.mjs` (`CORE_DIR:exact` runs a switch core in exact mode) and `web/bench-sim-browser.mjs --browser webkit`, cores built from the same tree:

| Core | node | WebKit |
|---|---|---|
| default build | 1.00x | 1.31 ms/tick |
| SSX_PS2_EXACT_FPU=1 (runtime switch), exact | 1.26-1.29x | 1.67 ms (1.27x) |
| the same with the switch a compile-time constant | 1.17-1.18x | 1.55 ms (1.18x) |
| that, with the power-of-two table | 1.14x | |

- The runtime switch itself costs ~8% (a load and branch in every helper). A shipping exact core would make it a constant, once exact baselines remove the mode-1 prefix.
- What is left: the multiplier's array (mulSlow, ~5.6% of a tick before the table) and the SRT divide (~3%). The rest is the helpers' fast paths, inlined into the animation, cross-product and terrain code.
- Nothing here adds input latency. The wasm is 6.59 MB against 6.85 MB for the live core.

### Exact baselines

Every gate's baseline was made in mode 1, so even an exact capture starts from mode-1 history: cached triangle normals, patch tessellation and the grid placement. Exact baselines are derived from frontend (menu) states, with the event load and everything after it run in exact mode.

**Why a menu root is enough:**

- `oracle --stale-ref FRONTEND/eeMemory.bin` lists every float-looking RAM word a call reads that it never wrote and that still holds the frontend value.
- Rider pass checks, each a race with 5 computer riders:
  - Snow Jam tick 345, from character-selection.p2s;
  - Ruthless Ridge (CRA3) tick 900, from characters/zoe/select.p2s.
- Every such word is one of:
  - an ELF data constant, or a copy of one (the tuning table at *(gp-0x1FB0) read by 0x13D0A0..0x13EAF0);
  - an integer-valued float;
  - not a float at all (INPUT.MAP bytecode at 0xA18BD0, read by 0x32549C as integers).
- Nothing arithmetic-derived crosses from the menu into the race. The load rebuilds the caches.

**Tools:**

- `tools/ps2_navigate.py` follows `PS2_CAPTURE_FPU` and records `fpu_mode` in navigate.json.
- `tools/ps2-float/derive_exact_baselines.py` handles the location states:
  - It finds each reference state's navigation run by EE hash (`local/reference-exact/provenance-map.json`) and follows the chain back to a menu root.
  - A root must have no rider manager and no resident locations; a mid-load root is refused.
  - It re-runs each step in exact mode, reusing steps shared between targets.
  - A tick-bound save is kept only at the reference's own tick and race phase. It is requested at sample−1, sample and sample+1, with retries.
  - The race phase is the race clock's C+0, the same object whose C+8 is the game tick: [[gp−0x848]+0x84]+0x0C.
    - 0x113B10(C, phase) keeps the old phase at C+4. Then 0x113B48 sets C+0 = phase (1..7), C+0x98 = C+0xAC + 4·(phase−1) and C+0xA0 = the phase handler (table 0x456CA0).
    - Phases: 3 PreRace (the course flythrough and the event brief), 4 Countdown, 5 Race, 6 EndRace.
    - The game tick also counts during PreRace, so a tick alone cannot tell the flythrough's tick 18 from the countdown's.
    - All 64 exact baselines match their references' phase (26 PreRace, 14 Countdown, 24 Race).
  - Output: `local/reference-exact/<name>.p2s` with `<name>.provenance.json`.
- Snow Jam (the hand-made references): `snow-jam-countdown-anchor` / `-glide` / `-ready` come from Zoe's recorded menu path from character-selection.p2s (local/ps2-float/baselines/nav/zoe.p2s).
  - The anchor is saved at sample 3560: Countdown, tick 18. The same run's glide is tick 339 at sample 3881.
  - Since 2026-10-07 the anchor is a byte copy of the canonical exact Zoe countdown, `characters/zoe/countdown.p2s` (same lineup and grid). The exact ARA1 event seed and its camera seed come from that file, so the anchor gates' baseline and the comparer's seeds agree by construction. The sample-3560 derivation is kept as `snow-jam/countdown-s3560.p2s`, and its 20 gates are in `runs-exactbase/.anchor-s3560/`. With it, cam-event-start / cam-event-race left at tick 19 on camera words, because the event camera seed came from the other derivation.
  - The first anchor (2026-10-05) was PreRace tick 18, in the flythrough. It is kept as `snow-jam/flythrough-tick18.p2s`.
  - Its gates stopped at record 401: the script's Cross skips the flythrough and the game waits on the event brief. Twenty exact-base gates were built on it; they were moved to `runs-exactbase/.flythrough-anchor/` and recaptured on 2026-10-06.
- Per-rider and per-course countdowns are the rider-parity agent's `make_course_states.py` with PS2_CAPTURE_FPU=exact, into `local/reference-exact/characters/`.
- `tools/ps2-float/derive_exact_chain.py STATE...` handles states that do not start from a menu: free ride, CTM, world states. It walks a state's own records back to a root:
  - patches.json sources (a deleted source is named by its recorded EE hash);
  - ps2_navigate records (navigate.json or NAME.navigate.json);
  - ps2_capture kept states (RUN.tickN.p2s with RUN.json);
  - copies found by identical EE memory.

  The root is a menu state or a CTM session state. Those come from `ctm.session.json` (title-outcome-1), replayed in exact mode by `ps2_menu_capture.py` into `local/reference-exact/ctm/session-exact.fNNNNN.p2s`. Session states are matched raw or hook-cleaned (0x321298 restored, arena zeroed from 0x90000 or 0x96000).

  The tool replays each step in exact mode:
  - patches go onto the exact source: hook and arena patches as written, game data checked;
  - navigation and captures re-run, keeping the state at the original tick.

  Output: `local/reference-exact/chains/<path under local/>`.
- `tools/ps2-float/recapture_exact.py --exact-baselines` captures the gates from exact baselines:
  - It rebuilds each gate from its baseline's exact copy (reference-exact, characters, chains), with the same script and options, into `local/ps2-capture/runs-exactbase/`.
  - A --watch window inside a heap object the manifest names (the camera block, the outer camera) is moved to that object's address in the exact state: the build runs once, reads the new addresses, and runs again with the moved windows. Before 2026-10-07 the 8 cam-* gates watched the mode-1 camera address (0x1A58650; exact 0x1A584D0) and lost their camera seed beyond +0x15C. The stale copies are in `runs-exactbase/.stale-camera-watch/`.
  - Gates run most-gated baseline first, and riders/* is skipped.
  - The riders/* gates come from the rider-parity sweep (`runs/riders-exact`). `tools/ps2-float/link_riders_exact.py` links the ones captured from exact baselines under the gate names.
- Captures from an exact baseline compare with `PS2_ARITH=exact-base`: the setup runs on the console model too. `score_gates.mjs --runs local/ps2-capture/runs-exactbase --arith exact-base` scores them.
- `tools/ps2-float/match_tick.sh` takes `MATCH_STATES` / `MATCH_RUNS` / `MATCH_ARITH` / `MATCH_TAG` for such gates, and `MATCH_ACTOR=<rider>` for a human-only PS2 trace (`oracle --actor`). The full pass mixes in the computer riders, whose values at the grid are often bit-identical to the human's.

**First result** (riders/zoe-race from the exact Zoe countdown): the exact race load places the human 1 ULP away from the mode-1 grid start.

| | x | z | velocity z |
|---|---|---|---|
| exact | C800C650 | C85F68B3 | BF4365B2 |
| mode 1 | C800C64F | C85F68B4 | BF4365B3 |

The port's `--event` setup reproduces the mode-1 bits, so its grid placement is the next form to fix.

### Mode-1-base gates

These gates keep their mode-1 baseline, because the state they start from can't be re-derived. When the swap lands, each one either gets a new scenario captured from an exact root or is retired with a note.

| Gate | Baseline | Why |
|---|---|---|
| peak1-lodge-attrs | `…/7fde9fd7…/scratchpad/ba/ps2/bought-t0.p2s` | the baseline lived in another session's scratchpad; file and recipe are gone |
| peak1-green-start | `…/7fde9fd7…/scratchpad/lodgewall/ps2/navpre/s682.p2s` | the same |
| weather/eba3-lightning | `…/7fde9fd7…/scratchpad/m2m-1620-clean.p2s` | the same |
| peak1-race-abc1a | `local/ps2-capture/peak1/peak1-race-abc1a-entry.p2s` | no patches, navigation or capture record, and no other file with its EE memory |
| peak1-race-abc1a-glide | `local/ps2-capture/peak1/peak1-race-abc1a-glide.p2s` | the same |
| peak1-race-start | `local/ps2-capture/peak1/peak1-race-objectives.p2s` | the same |
| peak1-fr-aara1 | `local/ps2-capture/peak1/fr-aara1-entry.p2s` | the same |
| peak1-fr-aara1-glide, course-limits/p1-{neutral3000, right3000, left, right, left3000, zig3000} | `local/ps2-capture/peak1/fr-aara1-glide.p2s` | the same |
| course-limits/gs-{zig3000, right3000, tuckleft3000, left3000, halfleft3000, halfright3000} | `local/ps2-capture/peak1/green-start-t0.p2s` | its patch source (`…/7fde9fd7…/lodgewall/ps2/navpre/.raw/00001.p2s`) is gone and matches no known root |
| peak2/fr-c, vp-stations/c-tuck | `local/ps2-capture/peak2/fr-c-arrive.p2s` | its chain re-runs, but in exact mode the menu path stops at Transport > Select Peak (the pad script no longer lines up) |
| ctm-events/c0a-race, c0a-race-riders | `local/ctm-events/caps/c0a-cd/countdown.p2s` | built from `ctm-parity/states/sj-card-q.p2s` ("from an earlier ride-in"); that state's only records loop back to itself (race-q / race-q-clean) |

So far that is 24 gates on 11 baselines. The other 57 gates on the tail's 16 baselines resolve to the CTM session root through `tools/ps2-float/derive_exact_chain.py`.

**At the swap** each of these needs a replacement scenario from an exact root (the exact CTM session replay `local/reference-exact/ctm/session-exact.fNNNNN.p2s`, or a derived exact state), covering the same behaviour. If no such scenario can be made, it is retired with a note:

| Gates | Behaviour covered | Replacement from an exact root |
|---|---|---|
| peak1-lodge-attrs | buying attributes in a lodge (score / stats words) | session frame 37044 (state-lodge-peak1) → the r3-attrs-buy menu path (`menus/ctm/r3-attrs-buy*.json`) → capture with the lodge-attrs watches |
| peak1-green-start; course-limits/gs-* (6) | CTM last-lodge start, world load, then neutral / steered free ride against the course limits | session frame 33959 / 35576 (green cutscene, lodge prompt) → the same start → a new green-start-t0 → the same six scripts |
| peak1-fr-aara1, peak1-fr-aara1-glide; course-limits/p1-* (6) | Peak 1 free-ride crossing A → A_ARA1 → ARA1 (unload / eviction / read), course limits | session frame 8992 (state-freeride-peak1) → ride to the ARA1 entry (autopilot or the recorded pads) → new entry and glide states → the same scripts |
| peak1-race-abc1a, -abc1a-glide, peak1-race-start | Peak 1 race start, objectives and the ABC1 crossing | session frame 13976 (state-race-event-list) → the race → new entry / glide / objectives states |
| ctm-events/c0a-race, c0a-race-riders | CTM first heat from its card: countdown, race with riders | the exact ara1-screen10 chain → the c0a card path (`ctm-events/caps/c0a-card.hooked.nav.json`) → a new countdown |
| weather/eba3-lightning | Much 2 Much lightning around tick 1620 | the exact much-2-much anchor (local/reference-exact) → the original script to 1620 → the same watches; retire if the exact run never reaches lightning there |

### Status (2026-10-05)

No-go for the swap. Per-gate scores against `local/ps2-capture/runs-exact`. The exact run also has the gates added later: allpeak/*-start, fr-throne-unload, c0a-ws13-splines.

| Core | pass | fail |
|---|---|---|
| live mode-1 core (wasm a8894cb8) | 3 | 441 |
| SSX_PS2_EXACT_FPU core from the 16:44 tree, `--arith exact` | 3 | 445 |

The exact core reaches a later first-inexact tick than the live core on 123 gates and an earlier one on none, but it is still about one tick: most rider gates stop at 186-187.

Remaining:

- batch 5 into the tree, then matcher rounds past tick 186 on more gates;
- exact-mode baselines, made from states with no float history (removes the mode-1 history drifts);
- the four ctm-events re-captures above;
- divide speed.
