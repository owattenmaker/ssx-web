# Original PS2 input and accepted command recovery

The native input primitives in `original_input.cpp` recover the original PS2
pad conversion `0x327210..0x3276BC`, consumed-sample button history `0x321298`,
and signed six-bit command axis quantization in `0x127998`. They are independent
of the four-shoulder-button binding configuration.

## Raw values and button history

The original device reader produces 24 float channels:

| Indices | Meaning |
|---|---|
| 0..3 | Select, Start, L3, R3 |
| 4..7 | D-pad right, left, up, down |
| 8..11 | Triangle, Circle, Cross, Square |
| 12..15 | L1, R1, L2, R2 |
| 16..19 | Right stick left, right, up, down |
| 20..23 | Left stick left, right, up, down |

Digital values use active-low packet bits. In pressure mode, indices 4..15
instead use the twelve unsigned pressure bytes times float bits `0x3B808081`
(approximately 1/255). Analog stick byte `b` is split into negative/positive
float channels as follows, with integer division truncating toward zero:

```
negative = max(((79-b)*255)/79, 0) * float(0x3B808081)
positive = max(((b-176)*255)/79, 0) * float(0x3B808081)
```

Thus the driver has an axial deadzone, unlike the provisional `/127` and `/128`
normalization. No host radial deadzone should be silently substituted when
reproducing the original PS2 byte stream.

The input provider then packs float axes into signed six-bit fields: multiply
by 31 and truncate. Controllers decode these with float bits `0x3D042108`
(approximately 1/31), so full-scale decoded input is `0.9999999403953552`.
This quantization occurs after the mapping expressions.

Each consumed button record has stride 0x1C and fields `value` (float),
`pressed`, `released`, `held`, `repeat`, `repeatTimer`, `edgeAge` (six uint32s).
Value always updates. After a recognized edge, the next three consumed samples
suppress new edges while advancing age from zero to three. This also leaves the
held flag unchanged during those samples. Repetition starts immediately, waits
24 samples, and then repeats at 12-sample intervals. Newly allocated original button records begin with age zero; seeded replays
should preserve their captured history. The raw value remains
available independently of debounced held/edge flags; the mapping's bare button
expression reads that raw value.

## Frozen input state

`tools/reference_input.py` follows these verified source pointers:

```
owner = *(rider+0x77C)
providerInterface = *(owner+0xDE8)
provider = *(providerInterface+0x0C)       // human0x127998
context = *(owner+0xDF0)
pad = *(context)
mapping = *(context+4)
button[i] = pad+4+i*0x1C
```

In the captured Snow Jam human, context is 0x53FCF8, pad is 0x9CF3E0, and mapping
is 0x542560. The reader evaluates the first 27 main-action mapping expressions
from the actual compiled input map, with bounded operands/instructions. It
supports the operations these expressions use and rejects unimplemented ones.

The device producer and game consumer are separated by a 30-entry ring at
`*(gp-0x850)`: read cursor +0x2EE0, write cursor +0x2EE4. A ring entry is 0x190
bytes, with four 0x64-byte device records. Consequently, emulator frame names,
game global ticks, and consumed pad samples must not be assumed identical.

## Use the game's accepted command recording

The original input provider calls `0x26D178` after producing its eight-byte
command. In ordinary gameplay that dispatches to `0x26D2B0`, which records
run-length-encoded commands:

```
recorder = *(owner+0xDF8)
recordCount = *(recorder)
recordBuffer = *(recorder+0x0C)
record[i] = two little-endian uint32s
runLength = word0 & 0xFFF
commandWord0 = word0 & 0xFFFFF000
```

`reference_input.py OUTCOME.p2s --baseline BASELINE.p2s --output REPORT.json`
verifies the entire baseline accepted history is a prefix of the outcome,
including when the outcome extends the baseline's last run. It extracts the
suffix into `REPORT.accepted_input`, the `native.accepted_input` replay schema.
Each segment retains its masked words, duration, decoded controls, and controller
state. SHA256 hashes cover the expanded little-endian eight-byte commands with
the run-length bits removed. Baseline/outcome EE hashes preserve provenance.
Declared P2M2 event frames are a separate requested-input record.

The supported decoder covers cruise state 0, crouch state 2, the verified basic
jump transition 0→2→5, state5 spin/flip/air-adjust/board-press/tweak/grab fields,
and neutral state4. Handplant, late-spin, unknown bits and inconsistent
transitions fail closed. State 4/5 neutral input contains
`word0=0x00FF0000`: original `0x1276F0` returns -1 when no grab is selected and
the provider stores that byte at command byte 2. Zero would select grab 0 and
must not be mislabeled as neutral in those states.

For the existing glide→jump-31 capture, the accepted suffix is one neutral tick,
one press-transition tick, 28 held ticks, and one release tick. Its SHA256 is
`f55f2172c0c542fc6d6d973b8e22076758b7c3a776a7769f6f726e2a5fa783c0`.
The charge-1/2/3 movies do not share a fixed added delay: their accepted histories
prove that some runs consumed one extra initial neutral sample. Do not change
native gameplay to imitate that recording-boundary artifact.

## Validation and remaining integration

`tools/test_original_input_native.py` compares 8,192 original driver conversions,
all 24 channels and their full sample histories, bit-for-bit. It covers every
stick byte and both analog/pressure modes. Twenty thousand axis values compare
against the original complete input provider's packing. The snapshot reader's
27 main-action expressions also match the original input-map VM exactly.

`tests/test_reference_input.py` checks baseline prefix alignment, independent
expanded-stream hashing, signed axis values, unknown-action rejection and the
no-grab sentinel. These checks establish the extracted stimulus, not physics
parity. The parent replay integration consumes accepted controls separately from
requested pads. Modern macOS controller calibration and full rail/handplant/late-spin
command decoding remain separate work. See AIR_CONTROL_RECOVERY.md for
the verified state5 nonneutral layout and grab-index mapping.

## Native adapters now in use

`InputMapper::update` now runs in the strict-float `input_adapter.cpp` target.
Ordinary `InputSample` values are tagged `DeviceNormalized`: macOS stick values
are clamped to [-1,1], mapped to the nearest byte using
`floor((value+1)*127.5+0.5)`, and passed through the original PS2 axial response.
Y is inverted when constructing the original packet because Apple's positive Y
points upward. D-pad magnitudes are converted to pressure bytes. This device
conversion is an explicit native adapter policy; it does not claim that every
modern controller's physical calibration equals a DualShock 2.

`originalReplayInput(movieBytes)` consumes the exact 18 PS2 movie bytes, inserts
the pad payload header, and calls `originalDecodePad`. Its returned sample is
tagged `OriginalResponse`, so the mapper does not apply the hardware conversion
a second time. The INPUT.MAP relationships are evaluated after response handling
and before signed six-bit axis packing. Canonical accepted-command replays bypass
this mapper because their semantic axes are already decoded from original words.

Keyboard digital axis extrema and equivalent PS2 stick/D-pad inputs produce the
same decoded axis values. L1/L2/R1/R2 remain independently bindable, all 16 chords
survive both adapters, and existing pause/button edge behavior is preserved.
The original sample-history helper remains separate: applying its three-sample
edge filter correctly requires a consumed-sample clock, whereas the current UI
mapper is called at render cadence. It must not be advanced blindly per render.

`native_input_adapter` checks every byte on all four axes through both replay and
hardware adapters, keyboard/D-pad equivalence, pressure-based button values, all
shoulder chords, pause edges, and floating-point mode restoration. The native
input, adapter and replay CTests passed after integration.

## Runtime-state raw command replay

`tools/canonical_replay.py --raw-commands SCENARIO OUTCOME OUTPUT` verifies the original RLE prefix and emits original words without prescribing per-frame control states. `original_command.cpp` decodes those words using `PrototypeRider.currentControlState()` at execution. The initializer checks the original starting state; telemetry records the actual dispatch state and exact words. Unsupported controller/action branches report the exact replay frame.

This enables long traces containing unobserved transitions without guessing that a zero word means a landing. It means cruise-neutral in state0 but grab0 in state5. The raw verifier deliberately does not claim independently verified original semantic controls. State/physics comparison remains separate; original endpoints can disagree while the recorded word stream still matches.

30,000 roundtrips through the original127998 provider verify native states0/2/5 decoding. This found and corrected an earlier semantic-axis swap: original action10 `AirAdjRotFB` is word1 bits6..11 (writer128500/128528), and action11 `AirAdjRotLR` is bits12..17. Both Python/C++ decoders and the angular-controller parameter mapping were corrected together. Old raw-byte tests had compensated names, so they missed incorrect live-stick behavior. The corrected full angular/presentation oracle passes60,000 cases. Existing saved nonzero-adjustment semantic scenarios should be regenerated; original recordings remain unchanged.

The normal jump request also uses the original rider360 latch: pressed always requests control2; held alone requests it only when the latch iszero. Control2 releases on absent held input, including after a pressed-only pulse. This is independent of host keyboard edge timing.

## Complete human provider port (`original_input_provider.cpp`)

`original_input_provider.{hpp,cpp}` ports the whole path from the 24 consumed
button records to both command words for every controller state.

**Actions.** 0x226B60 registers action names by index (0x320E18): 0..26 as in
`reference_input.py`, 27..30 UberGrind1..4, 31..45 Trick1..15, 46 ResetPath
(47..204 are camera/menu/editor actions the provider never reads). The compiled
map uses only VM opcodes 0 literal, 1 move, 5 `&&`, 13 SUB.S, 16 MAX, 31 `!`, 32
return. MAX is `right<left ? left : right` (so +0/-0 ties return the right
operand); `&&`/`!` produce 1.0/0.0 from zero tests. Operand property 5 (bare name)
redirects to the context default property, which 0x325250 zeroes, so it reads
`value`; 1/2/3/4 read held/pressed/released/repeat as uint32 converted by
CVT.S.W with the unsigned fixup (toward zero: truncation to 24 significant bits).
Float getter 0x320BF0 returns the VM result; bool getter 0x320C48/0x321108 is
`value != 0`. LateSpin compiles to literal 0, so the late-spin bit is never set by
this map. Grab selection 0x1276F0 is the first active Trick1..15 (0..14, else -1);
0x127848 is the first active UberGrind1..4 (0..3, else -1). Both read only actions.

**Provider fields.** 0x127998 zeroes 8 bytes, reads the control state through
0x11FEE8 (`*(*(owner+0x18)+0x77C)+0xDE4`, i.e. motion owner +0xDE4) and switches
through the 14-entry table at 0x457EA0. Axes are `trunc(value*31) & 63` (MUL.S
toward zero, CVT.W.S), decoded as signed six bits times float(0x3D042108). Flags
are bool-getter results. The only rider field read is the control state; the pad
context (owner+0xDF0: pad, mapping) is input, and owner+0xDF8 (recorder 0x26D178)
is an output side effect.

| Control (update) | word0 | word1 |
|---|---|---|
| 0 cruise (131620) | 12 ResetPath, 13 Handplant, 14 JumpPressed, 15 JumpHeld, 16 BoostPressed, 17 BoostHeld, 18 AttackLeft, 19 AttackRight, 20..25 CruiseTurn, 26..31 CruiseCrouch | 0..5 CruiseBrake, 6..11 BoardPress |
| 1 (12FC80) | 12 ResetPath, 13 BoostPressed, 14 BoostHeld, 15 JumpHeld, 16 JumpPressed, 17 OllieHeld, 18..23 CruiseTurn, 24..29 RailBalance | 0..5 BoardPress, 6..11 BoardPivot |
| 2 prewind (12E9B8) | 12 ResetPath, 13 JumpHeld, 14 BoostHeld, 15..20 PrewindSpin, 21..26 PrewindFlip | 0..5 PrewindTurn, 6..11 RailBalance |
| 3 soft collision (12E778) | 12 ResetPath, 13 BoostPressed, 14 BoostHeld, 15..20 CruiseTurn, 21..26 RailBalance | - |
| 4 passive air (12F730) | 12 ResetPath, 13 Handplant, 14 AttackLeft, 15 AttackRight, 16..23 grab (int8, SB of 0x1276F0), 24..29 CruiseTurn | 0..5 CruiseCrouch |
| 5 air (133308) | 12 ResetPath, 13 Handplant, 14 Tweak, 15 LateSpin, 16..23 grab, 24..29 Spin | 0..5 Flip, 6..11 AirAdjRotFB, 12..17 AirAdjRotLR, 18..19 BoardPress step (raw value <-0.5: 3, >0.5: 1, else 0) |
| 6 race start (12BF68) | 12..17 GateAnticipate (no ResetPath bit) | - |
| 7 rail (131D30) | 12 ResetPath, 13 JumpPressed, 14 JumpHeld, 15 BoostPressed, 16 BoostHeld, 17..24 Uber identity (int8, 0x127848), 25..30 RailBalance | 0..5 RailSpin, 6..11 BoardPress |
| 8 crash (12CB68) | 12 ResetPath, 13 WipeoutRecover | - |
| 9 reset (12F398) | - | - |
| 10 (12C678) | 12..17 CruiseTurn | - |
| 11 handplant (132A30) | 12 ResetPath, 13 Handplant, 14..19 HandplantBalance | - |
| 12 rail Uber (136508) | 12 ResetPath, 13 BoostPressed, 14 BoostHeld, 15..22 Uber identity, 23..28 RailBalance | - |
| 13, and >=14 unsigned | - | - |

Unlisted bits are always zero. Naming notes: the existing state3 view calls bits
21..26 `boardPress`, but the provider writes action 23 RailBalance (LStickR-LStickL)
there; state5 bit14 is Tweak (the existing view's `boostHeld`, same Square value);
bit12 is ResetPath (`Select.pressed`), which native code carries as
`recoverPressed`. `rail_motion.hpp` `originalRailDecodeCommand` agrees with the
state7 row (its upper13/14 are JumpPressed/JumpHeld, turn/rotate/transfer are
RailBalance/RailSpin/BoardPress).

**API.** Per consumed input sample (not per render frame), with a persistent
zero-initialized `OriginalPadState history` and the control state that will run:
`OriginalCommandWords w=originalHumanInputTick(history,originalDecodePad(packet),controlState);`
then `originalCommandRiderInput(controlState,w.word0,w.word1)` or the lossless
`originalDecodeCommandFields`. `originalEvaluateActions`/`originalPackCommand` expose
the two stages. `originalDecodeCommand` keeps its strict checks for states 0/2/3/4/5
(only adding railBalance/attackLeft/attackRight/tweak copies) and now routes states
1 and 6..13 to the full decoder. Nonfinite pad/action values throw.

**Validation.** `tools/test_original_input_provider_native.py` runs the recompiled
original code with the snapshot's compiled map: 360,000 stubbed-getter packings
(arbitrary action values incl. k/31 and ±0.5 neighbours, ±0, subnormals and
saturating CVT.W.S) for control states -1..15 and 100; 32,768 pad states (half
real consumed-sample histories, half arbitrary records with ±0, values in [-2,2],
subnormals and large flag words) where all 47 actions match 0x320BF0 bit-for-bit
and 0x320C48, 0x1276F0 and 0x127848 match; and 589,824 full 0x127998 commands
bit-for-bit. Every decoded field is checked against the actions, and the lenient
RiderInput view equals strict `originalDecodeCommand` whenever it accepts. The
port is built twice, hardware toward-zero and the WebAssembly software_float path;
both pass, and a real emscripten build hashes identically to native over 200,000
states x 14 controllers. Mutating bit positions, MAX tie order, board-press
thresholds, grab priority or toward-zero SUB/MUL is caught.

The oracle is the recompiled code's IEEE toward-zero SUB.S. PCSX2's EE guard-bit
add/sub (`original_float.hpp`) differs in the raw difference for 21,718 of the
66,049 pairs of realistic pad values, but never after six-bit quantization, and no
unquantized provider field uses a subtraction with two nonzero operands, so command
words are identical under both models for driver-produced values.

## Settled pad history and keyboard Simple mode (2026-09-23)

**Settled pad.** The pad-history debounce in 0x321298 suppresses edges until `edgeAge` reaches 3. The PS2 pad has been read for many frames before any race, so its history is always settled.
- `web/input_bridge.inc` now starts with, and `reset_pad_history` resets to, a settled neutral pad (`edgeAge` = 3).
- A button held from the first sample therefore reports its press edge, as it does on the PS2. tech-speedcap-groomed 338 shows the boost press bit 0x10000.

**Keyboard Simple mode** (`web/pad-input.js`). Each movement key keeps the role it had when it was pressed, until it is released:
- Pressed on the ground: it is the stick.
- Pressed in the air: it is the D-pad.
- Pressed in the air while the opposite direction is held as the D-pad: it is the stick.
- When several keys act as the stick, the newest one wins the axis.

A Simple-mode player can therefore do a D-pad spin plus the opposite stick (spinboost, 540), buffer a prewind through a landing, and keep a steering key held from the ground. `web/test-pad-input.mjs` checks that these combinations produce the same channels as a DualShock. Classic mode (the default) keeps the arrows and WASD on the stick.

**Unchanged and oracle-verified:**
- INPUT.MAP: CruiseTurn = max(DPad, stick); Spin = DPadR - DPadL; AirAdjRotFB = LStickU - LStickD.
- The axial dead zone: bytes 79..176 read as zero.
- The analog quantisation, `originalQuantizeAxis`.
