# Stage scripts

## Stage-script VM (LUN)

Course stage scripts are programs for EA's "Luno" register VM (allocation tags at 0x479780..0x479880:
`LunoVMRegister`, `LunoVMCallParam`, `cLunoTable`, `LunoTable`, `TableData`, `cLunoTableEntry`). The
interpreter is 0x2227D0. `engine/stage_script_vm.hpp` (namespace `ssx`) is an instruction-level port of it,
together with the frame setup, the value helpers and the hash table. It is checked against the recompiled
originals by `tools/test_stage_script_vm_native.py`. Nothing is wired into the browser yet.

### Program layout

A stage program (magic 0x004E554C, `tools/export_stage_scripts.py` / `import_stage_scripts.py`) is laid out as follows:

| offset | contents |
|---|---|
| +0 | `0x004E554C` ("LUN\0") |
| +4 | code end (bytes from the header, ≥ 0x10) |
| +8, +C | extent (twice) |
| +0x10 .. code end | code words; pc is a word index from +0x10 |
| code end .. extent | trailer: one 16-byte function record per function |

Function record (read at `header + codeEnd + 16*index`):

| field | meaning |
|---|---|
| +0 | byte offset back to the header (`-(codeEnd + 16*index)`); the VM does not use it |
| +4 | start pc |
| +8 | register count. Frame setup only reads record 0's count, even when it enters another record. |
| +C | argument-stack capacity. 0x2227D0 allocates `capacity*16+16` bytes when this is > 0. |

Every stage program has one record, except program 3 of each course stage (ARA1, BRA2 and BHP1), which has three. Its
function 0 stores two function values (op 0x1D, records 1 and 2) into the globals table under the symbols
0x0DFB527E and 0x0A3FBCE3. These are callbacks that the host later calls by name through 0x309E50.

### Entry points and frames

- **0x2224B8** `(frame, funcref*, globals*, ret*)` is the host entry. It allocates `record0.registerCount` 12-byte registers, zeroing word0 and the type. `reg0` is a table copy of `*globals` (0x225068), with the type forced to 4. `reg1` is nil. It then calls 0x2227D0. Host callers:
  - 0x309C88 runs function 0 of the stage program from 0x3A6D18(ctx+0x28C), with globals `*(ctx+0x3CC+4*slot)`.
  - 0x309E50 looks a symbol up in those globals (0x225248) and calls the function value it finds.
  - 0x30A060 and 0x30AC98 run with the instance's table (ctx+0x2A4, or ctx+0x2A8 → +0x3C, then +0x1C). When that table is missing they use a temporary 8-bucket table that is released afterwards.
  - 0x308DB8 runs each instance's setup program with a temporary 8-bucket table.
- **0x222648** is the op 0x1B frame. It is identical to 0x2224B8, except that `reg0` is copied from the caller's `reg0` and `reg1` is a copy of the argument register.
- **0x2227D0** `(frame, funcref*, ret*)` is the interpreter. Its stack holds `+0x70` header, `+0x74` code base, `+0x78` code word count, `+0x7C` argument stack, and `+0x80` argument count. `s6` = pc, `s7` = return slot. It loops `while (pc < codeWords)` using a signed compare. Each iteration fetches a word, increments pc, and dispatches `word & 0xFF` through the jump table at 0x4797B0. Opcodes ≥ 0x2B do nothing. On exit it releases every argument-stack entry (all `capacity` entries, top first), then every register (0x225B90), and frees both blocks.

An instruction word has the form `op | b1<<8 | b2<<16 | b3<<24`, and jump targets are `word>>16`. Opcodes 0x14–0x17, 0x1D and 0x24–0x27 read one inline word. An inline word at the code end is read from the trailer.

### Values

Values are 12 bytes: `{word0, word1, type}`. Only type-5 copies copy `word1`. Every other copy writes `word0` and the type and keeps the destination's old `word1`. Each register write first releases the destination (when it is a table), then sets type = nil and word0 = 0, and only then reads the source. As a result `mov rX, rX` (0x0B/0x13) leaves nil.

| type | meaning |
|---|---|
| 0 | nil (the constant 0x4C9098 = {0,0,0}; builtins may return nil with any word0) |
| 1 | int |
| 2 | float |
| 3 | symbol (32-bit name hash loaded by 0x14; all course table keys) |
| 4 | table: word0 → `{+8 refcount, +0xC modulus, +0x10 node array}`. A copy increments the refcount (0x225068). A release (0x224DF0) decrements it; at zero the table is destroyed (0x224D00) and word0 is cleared. |
| 5 | function `{word0 = program header, word1 = record index}` |
| other | copied as word0; truthy; comparable only with the same type |

Builtins never create tables. Op 0x19 creates a table that holds two references (the leaked `cLunoTable` handle plus the register), so VM-created tables are never freed.

### Opcodes (jump table 0x4797B0)

x = `reg[b2]`, y = `reg[b3]`, and the destination is `reg[b1]` unless noted.

| op | handler | semantics |
|---|---|---|
| 00 | 222B44 | pc = word>>16 |
| 01 | 222A60 | if truthy(reg[b1]) pc = word>>16 |
| 02 | 222AD0 | if !truthy(reg[b1]) pc = word>>16 |
| 03 | 222DD4 | int(x == y) |
| 04 | 222EE4 | int(x != y); pairings that cannot be compared give 0 |
| 05 | 222FF4 | int(x >= y) |
| 06 | 223118 | int(x > y) |
| 07 | 223238 | int(x <= y) |
| 08 | 22335C | int(x < y) |
| 09 | 22347C | x.type==nil ? 0 : x.w0 ? 1 : (y.type!=nil && y.w0!=0) |
| 0A | 22353C | int(y.type && x.type && x.w0 && y.w0) |
| 0B | 2235E0 | reg[b1] = reg[b2] (shares 0x13's code) |
| 0C | 223644 | x + y |
| 0D | 2237FC | x − y |
| 0E | 22398C | x × y |
| 0F | 223B1C | x ÷ y (an int divisor of 0 hits BREAK 7) |
| 10 | 223E08 | reg[b1] = nil |
| 11 | 223CD0 | int x % int y when both are ints, else nil (BREAK 7 on 0) |
| 12 | 224030 | table set: reg[b1][reg[b2]] = reg[b3] (0x225338) |
| 13 | 2240C0 | move |
| 14 | 224154 | reg[b1] = symbol(inline) |
| 15 | 2241B0 | reg[b1] = int(inline) |
| 16 | 222A04 | reg[b1] = int(inline) |
| 17 | 2229A0 | reg[b1] = float bits(inline) |
| 18 | 224660 | reg[b1] = reg[b2][reg[b3]] (0x225248) |
| 19 | 2246EC | reg[b1] = new 32-bucket table |
| 1A | 224B2C | no-op |
| 1B | 223EE0 | if reg[b2] is not nil: call function reg[b2] with argument reg[b3], returning into reg[b1] (0x222648) |
| 1C | 224070 | table set as in 0x12, then reg[b2].word0 += 1 |
| 1D | 224214 | reg[b1] = function {this program, inline record} |
| 1E | 224764 | reg[b1] = nil |
| 1F | 2247F8 | *ret = reg[b1]; pc = end |
| 20 | 2228F0 | push {key b1, reg[b2]} |
| 21 | 223F58 | builtin call: `table[b2](&temp, b3, &args[count-b3])`. reg[b1] = temp, count −= b3, temp released |
| 22 | 222B4C | not: int → int(w0==0), float → int(f==0), otherwise nil |
| 23 | 222C98 | negate: int → −w0, float → NEG.S, otherwise nil |
| 24 | 22428C | for loop (below) |
| 25 | 224878 | push {key b1, int(inline)} |
| 26 | 2248E0 | push {key b1, float bits(inline)} |
| 27 | 224958 | push {key b1, int(inline)} (course programs use it for `rid<<8\|track`) |
| 28 | 2249D0 | push {key b1, int(b2)} |
| 29 | 224A3C | push {key b1, float(b2)} |
| 2A | 224AB0 | *ret = nil; pc = end |

Truthiness (0x01/0x02) works as follows:

- nil is false.
- An int or symbol is true when `word0 != 0`.
- A float is true when `f != 0.0`, so −0.0 is false.
- Tables, functions and every other type are true.

Comparisons (0x03–0x08, the table key match, and the loop tests):

- **Same type:** compares word0. Ordering uses signed ints, including for floats, so two negative floats compare in reverse.
- **x float, y int:** compares `x` with `CVT.S.W(y)`.
- **x int, y float:** compares `x` with `CVT.W.S(y)` (truncated and saturated).
- **x symbol, y int:** unsigned compare.
- **Any other pairing:** false for every operator, including `!=`.

Arithmetic (0x0C–0x0F and 0x226628):

- **Same type:** float uses the EE scalar operation. ADD/SUB keep the guard bit, MUL rounds by chop, and DIV rounds to nearest; a zero-exponent divisor gives `sign|0x7F7FFFFF`. Int uses 32-bit MIPS arithmetic. Symbol+symbol gives a symbol, and only addition works on symbols.
- **float ∘ int:** `x ∘ CVT.S.W(y)`.
- **int ∘ float:** `x ∘ CVT.W.S(y)`, giving an int.
- **Anything else:** nil.

For loop 0x24: b1 = i, b2 = limit, b3 = step, and the inline word = target.

- `negative = step < 0.0f`, using the comparison rules above, so a float step is tested on its sign bit.
- If `(negative ? i >= limit : i <= limit)`, then `i = i + step` (0x226628) and pc = target. Otherwise execution falls through.
- `nil <= nil` holds, so a loop over nil registers never ends.

### Tables

A table has `n` embedded head nodes of 32 bytes each: key at +0, value at +0xC, next at +0x18, vtable at +0x1C. The bucket is `key.word0 % (n − 1)` (unsigned), so the last head is never used. Host tables have 8 buckets and op 0x19 creates 32.

- **get (0x225248):** returns nil when the head's value is nil, without walking the chain. Otherwise it walks from the head and returns the first key that compares equal.
- **set (0x225338):**
  - A nil key does nothing.
  - An empty head (value nil) is overwritten with the key and value, even when the value is nil.
  - A head key that matches gets the new value; a nil value empties the bucket and hides its chain.
  - A chained key that matches gets the new value. A nil value unlinks the node, then runs its destructor 0x226768, which first destroys the rest of the chain (the previous node still points there). The port keeps those nodes as tombstones: key and value set to nil with stale words, links intact. This matches what the original reads from freed memory that has not been reused.
  - When no key matches, a non-nil value is appended as a new chain node at the tail.
- Hashing a table or function value uses its address, so the bucket depends on the heap. Course programs only use symbol keys.

### Builtins

`fn(OriginalScriptValue* result, int count, OriginalScriptArg* args)` receives the top `count` argument-stack entries in push order. Each entry is 16 bytes, `{int key, 12-byte value}`. Entries are not released after the call; they are released when a later push overwrites them or when the frame exits.

Builtins parse their arguments by key. For example, builtin 27 at 0x2FF850 (defaults 0x4FBAE8, expected types 0x446530) and builtin 0x41 at 0x301D78 (types at gp+0xBC0) both work like this:

1. Start from a default word array.
2. For each argument, if `type == expected[key]`, store word0.
3. Otherwise, if the expected type is float, store `CVT.S.W(word0)`, whatever the argument's type.
4. Otherwise, store word0.

A key of −1 in a player slot means the current player, `*(gp+0xCE8)`. `ssx::originalScriptKeyedArguments` mirrors this parsing.

### Port API (`engine/stage_script_vm.hpp`)

- `OriginalScriptValue {word0, word1, type}`, `OriginalScriptArg {key, value}`, `OriginalScriptFunctionRecord`, and `OriginalScriptProgram {code, trailer}`.
- `originalScriptProgram(code, trailer)` takes the scripts.json layout. `originalScriptProgramFromWords(words, count)` takes the raw header+code+trailer words from `web/generated/stage_scripts_seed.hpp`.
- `OriginalScriptVM` provides:
  - `addProgram`, `newTable(buckets)`, `tableValue`, `get`, `set`, `retain`, `release`, and `table`/`tableCount` for inspection.
  - `run(program, function, globals, builtin, finalRegisters*, returnSlot)`, which is 0x2224B8.
  - `call(functionValue, globals, builtin, …)`.
  - `maxCallDepth` and an `onInstruction` debug hook.
- `Builtin = std::function<OriginalScriptValue(int index, const OriginalScriptArg* args, int count)>`. A builtin that returns a table value must hold one reference for it, like the original temporary.
- Where the original would crash or corrupt memory, the port throws `OriginalScriptError`: a register out of range, stack underflow or overflow, a table operation on a non-table, calling a non-function, BREAK 7, or depth over the limit.

### Oracle

`python3 tools/test_stage_script_vm_native.py [synthetic-count=100000]` (2026-09-22):

- **Recompiled code:** 0x2227D0 and 0x2224A0/A8/B8, 0x222648, 0x224DA0/0x224E50/0x224DF0/0x224F30/0x224D00, 0x225068, 0x225248, 0x225338, 0x225B90, 0x226600..0x226628 and 0x226768. These are built with the PCSX2 EE FP correction, and the interpreter's DIV.S also gets the EE zero-divisor rule.
- **Environment:** the ELF is mapped at +0xFF000. Allocators are a zero-filled bump heap that never reuses memory. The 111 builtin table entries go to a recording stub that returns seeded random values: nil, int, float, symbol, live tables with a reference taken, functions, and unknown types.
- **Stage programs:** every stage program of the three course worlds (ARA1 3/8/9, BRA2 14/16, BHP1 13/15), 858 programs and 864 function records, 160 randomized runs each. Each run gets random globals, including the program's own symbols, extra tables, function values and nil values. The course-track programs match `local/browser-pickups/*scripts.json` exactly.
- **Synthetic programs:** 100000 random programs with 1–4 functions, 4 runs each. They cover all 43 opcodes plus 0x2B–0xFF, nested 0x1B calls (depth limit 4), bounded for/while loops, and table collisions and tombstones.
- **Compared:** the builtin call trace (index, count, keys, full values), the return slot, the top-level register file just before release (all three words), and every table's refcount, buckets and head+chain nodes. Table and function words are compared as creation index or program handle.

Result: stage programs 138240 runs and 443311 builtin calls; synthetic 400000 runs (72517 depth-limit aborts, compared by trace) and 1061492 builtin calls. Everything matches.

Mutation checks were run by introducing faults into the port. The oracle catches every semantic change tried: float ordering, −0.0 truthiness, chain tombstoning, word1 copying, the empty-head get, the 0x09 asymmetry, the record-0 register count, symbol arithmetic and compare, self-move, refcounts, nil insertion, the bucket modulus, the for-loop test, and push word1. The course programs alone catch most of them. They never execute 0x01, 0x0B, 0x0F, 0x10, 0x11, 0x13, 0x19, 0x1A, 0x1B, 0x1C, 0x22 or 0x24; the synthetic programs exercise those.

Not modelled:

- Denormal, Inf and NaN float operands. The EE flushes them, the recompiled host does not, and the random values avoid them.
- The real allocator reusing memory after the chain-destructor use-after-free.
- Address-dependent hashing and ordering of table and function values. The port uses monotonic handles.

## Builtin audit: ARA1 / BRA2 / BHP1 race programs (2026-09-23)

Every builtin that a Snow Jam, Metro-City or Junction stage program calls (every track of the event world: course,
hub connectors), with what the browser does. Core: `web/stage_script_gameplay.inc` (dispatch, contacts, Debounce,
collectibles) and `web/stage_world.inc` (entities built by programs: LiveComps, particles, MeshAnims, magnets, halos,
MultiParticle groups, one-way volumes, crowd). Decoded calls per course: `tools/export_stage_world.py`
(`web/public/assets/<LOC>/STAGE/stage-world.json`: calls, MeshAnim models, teleports, collections).

| # | address | name | browser |
|---|---|---|---|
| 0 | 2FC0D0 | Object entity (type 17) | contact gate entity (30-tick guard, 355770); key1/key2 flag variants only on the inert ARA1 endmode colliders |
| 1 | 2FC7D0 | Debounce | 342C08/342D88/342E98: flags by mode, restore 0 DeadNode (permanent, section Dead), 3 RestoreNode, slot 4 replaces the completion; replaces any entity (0x355F10: effects move, magnet frozen) |
| 2 | 2FC420 | SetNodeState | DeadNode 6 / authored flags / RestoreNode 19 (runtime flags to the collision world and the renderer) |
| 3 | 2FBCB8 | LiveComp player | core players for trigger/timer starts (key8/key6 gameplay-RNG draws), JS players for section starts |
| 6, 48 | 2FB498, 2FF1C8 | AnimTeeter / Rail | `web/rail_bridge.cpp` (log teeters), falling billboard |
| 7 | 2FBEC8 | one-way volume (Boost, type 8) | `engine/one_way_volume.hpp` + stage world; entity words bit-exact vs `setpieces/full.tick11758/11918`; flags \| 0x100 over the load's runtime flags (pv loadFlags, free ride: push exact vs course-limits/p3b-zig3000) |
| 12 | 2FC9C8 | flag cloth | `web/flag-animation.js` |
| 13 | 2FCFF0 | MeshAnim break pieces | 0x351B40/0x352230/0x352500; deterministic words exact vs PS2 (BRA2 chinaroof 6466); pose from a LiveComp node / the magnet |
| 15 | 2FD250 | RollerModifier (crashbag) | `web/roller_gameplay.inc` |
| 16, 25, 26, 69 | 2FD420, 2FE840, 2FEE98, 302490 | particles (burst / add / trail / stop, mode 1 deletes halos) | `engine/set_piece_particles.hpp` + `web/set-piece-particles.js`: 2000+ effects word-exact vs 90 savestates |
| 18 | 2FDC60 | ParentModifier | `web/attached-setpieces.js` |
| 19, 20 | 2FDED0, 2FE0C0 | Spline / MultiSpline | `web/set_piece_gameplay.inc` (slot-5 spline launches too) |
| 21 | 2FE2C0 | UVScroll | `web/uv-scroll.js` |
| 27 | 2FF850 | player effect | boost types 1/2, reset 5 ("Wrong Way!"), points 6 |
| 29, 58 | 2FFB50, 2FFD58 | Hide / Unhide | type-16 nodes |
| 30, 31, 73 | 2FFF00, 3000A8, 300260 | sounds | queued for the audio agent (`audio_stage_sound`, `stage_world_sounds`) |
| 34 | 300770 | teleport current player | done: destination = instance+0x10, contacting rider only; rider side 0x123210 in web/stage_teleport.inc (pv `boothTeleport`), human and computer riders, PS2-verified (stage-teleport.md 4) |
| 37, 38, 39 | 300E28, 300F50, 301120 | collections | 30C4A8 with 0x535C11: single events DeadNode, career races the uncollected ones; collect -> career save row + cash |
| 43 | 302778 | event-kind test | int(table 0x4465F8[n] == 0x535C10): Race on ARA1/BRA2, Half Pipe on BHP1 -> mode fences / challenge reset planes kill themselves |
| 44, 61 | 302968, 303430 | current instance (set / read, -1 without) | done |
| 52, 55, 77 | 303130, 3019C8, 303598 | has entity / LiveComp timer gate / random float (gameplay RNG) | done |
| 87 | 304E38 | contact guard preset | entity+0x20 = int(60 * s) |
| 88 | 303BA0 | CrowdMan2d | crowd texture animation (CRWD.SSH an00..15, 3 ticks/frame) and camera-flash areas (`web/crowd-2d.js`) |
| 90 | 305478 | MagnetModifier | `engine/magnet_modifier.hpp` in the stage world: box answer, gate, flight; BHP1 pointa award exact (pipe-finish 2862) |
| 97 | 3057C0 | HaloModifier | `web/set-piece-halos.js` (FX whha/gcha/rdha, spin, double copy for keys 1/4) |
| 99 | 3061B0 | game option enabled: selector 0 / 1 / 2 = bit 6 Multipliers / 8 Power-ups / 7 Point icons of *0x5308D0 clear (other selectors 1). Setter 192088 (option 1..14, options screen 192380), getter 192240, Yes / No rows 1AF098 / 1AFE08 / 1B0850 (TextMultipliersResult / TextPowerUpsResult / TextPointiconsResult) | nil. Every one of the 162 calls is `if 99(sel)==0: builtin29, return`; 0x5308D0 is 0 in all 794 PS2 free-ride savestates, and nil==0 is false, so the build branch runs as on the PS2 (docs/peak-mountain.md "Course limits") |
| 101 | 306438 | attention point: key0 instance (-1 ctx+0x290), key1 radius (1000), key2 seconds -> 101728 slot in W+0x84->+0xC->+0xA8 (64 x 0x1C) | nil (the PS2 returns nil too). The only consumer is 1441B8 -> 101A10 -> 122CF0 -> rider+0x5B0, read only by 122CF0 itself, whose change notification 11A0C0 is an empty stub: no gameplay, collision, draw or sound effect |
| 105, 106 | 306A90, 306CB0 | MultiParticle (roadflare flames) | groups from the ready savestate, member add/first-match remove; exact vs PS2 |
| 40, 47, 50, 63..65, 67, 68, 75, 76, 78, 79, 83, 98, 107 | | Big Challenge / free ride / hub | inert in a race (their programs never run there, or return 0) |

**Teleport (builtin 34), the Metro-City phone booths and water towers (2026-09-27).** 0x123210(rider, M) does the following:
- P = r3 + 100 * (a*r0 + b*r1), with (a, b) by the roster slot rider+0x86C (0:(2,0) 1:(1,0) 2:(2,-2) 3:(2,2) 4:(1,-2)
  5:(2,1)), and D = r0.
- The score reset 119368 (its return is discarded).
- The current motion's exit, then the ground entry 13C7A8 (velocity x 0.7, or min(0.7 + (airticks-40)*0.01, 1)).
- The control exit and control 0; the reset fade stops (125038).
- The camera set-target on the director nodes (the old state).
- speed = |v|.
- The placement 11D660(P, D, semantic 5 RNORM_FWD_CYC, clearance 0), with the second camera cut.
- v = physical forward * speed (z kept), then the FX reset 111890.

Computer riders teleport too. Ported in web/stage_teleport.inc behind pv `boothTeleport` (`core._stage_teleport_enable`
per rider core). It matches PS2 captures that inject the beam contact at 0x121820 (`tools/ps2_booth_inject.py`,
`local/ps2-capture/runs/booth/`, gates `booth/*` in test-ps2-captures.mjs). Full spec and results:
[stage-teleport.md](stage-teleport.md).

## R&B (ASS1) and Crow's Nest (ABA1) (2026-09-23)

`tools/export_stage_scripts.py` exports `browser_stage_ass1` (A_ASS1 track 4, 20 programs; ASS1 track 11, 186 programs;
700 handler rows, 91 countdown entities) and `browser_stage_aba1` (A_ABA1 track 2, 18; ABA1 track 5, 138; 218 rows, 27
entities); `stage_tables_for` selects them. New contact classes: the AnimTeeter entity 0x4908F8 (vt+0x144 = 355770, gate)
and the one-way volume 0x4914E0 (0x3609F0, no-op). builtin43 sees 0x535C10 = 1 on ASS1 and 2 on ABA1.
Builtins these programs call that the core dispatches as no-ops (as on BRA2/BHP1): 12 (flag cloths: the JS flags own
them), 40, 47, 64, 65, 67, 75, 76, 78, 79, 98, 99 and the ones new to these courses, 50, 81, 83, 86 (not analysed). 48
(RailModifier: the train and crane rails) is not ported for these courses (see set-pieces.md gaps). Crow's Nest program 42 (ospreytrigger) is the first random-gated
Spline launch: builtin77 draws, `< 60` returns, else LiveComp + Spline (launched from the VM's builtin19, seed guard 2).
