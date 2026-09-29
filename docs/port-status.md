# SSX 3 macOS port status — 2026-09-07

> Current integration checkpoint: [Agent handoff — 2026-09-09](HANDOFF.md). The measurements and research below describe their recorded stages; older missing-feature notes and test counts are not the latest app status.

## Outcome

The generated main-CPU code compiles and links into an Apple Silicon executable. Two isolated original routines pass native tests. The diagnostic host initializes a macOS window and Core Audio, loads the original ELF, and executes startup code. **The game does not reach a validated title screen or gameplay.**

No ready-to-play app is delivered. App packaging, a native graphics backend, and playable game behavior remain future work. The upstream runtime currently uses raylib/OpenGL for presentation and models PS2 hardware, including interpreted VU microprograms.

## Disc evidence

| Property | Observed value |
| --- | --- |
| Volume | `SSX3` |
| Image size | 3,005,415,424 bytes |
| Boot executable | `SLUS_207.72` |
| Executable SHA-1 | `77114dfd1205eaccf1ccc18c5f9650097fa78bd8` |
| Executable SHA-256 | `1b49d05ca2793922180851b9e1ce9ae2291d61a7863565ac4e71f12e967af7bc` |
| Entry point | `0x00100008` |
| ELF format | 32-bit little-endian MIPS, no symbol table |
| Main `.text` | 3,334,176 bytes |
| `.vutext` | 59,360 bytes |
| Load segment | VA `0x00100000`, 3,820,532 file bytes, 4,451,036 memory bytes |
| Disc files | 163 |
| BIG archives / entries | 28 / 9,163, including zero-length wildcard entries |

`DATA/CONFIG/SLUSOVF.BIG` contains `overlay.dat` and `config.dat`. Their loading/relocation semantics have not been analyzed. `NETGUI/NTGUI.ELF` is a separate executable. Translating only the main ELF does not establish coverage of either of these paths.

## Translation and validation

PS2Recomp's heuristic analyzer found 8,143 candidates, 180 potential library bindings, and 359 suspected library functions without runtime handlers. The recompiler emitted 8,017 translated routines and 126 stub wrappers, with zero reported decode failures but 3,594 unresolved indirect-control-flow warnings. These are tool diagnostics, not independent verification of coverage.

The preferred next analysis path is a Ghidra function map: upstream explicitly cautions that its native heuristic scanner is incomplete for stripped retail executables. No Ghidra project or verified whole-program function map exists here yet.

The function at `0x00317618`, identified by the decompilation project as `tHashName32_getHashValue`, runs natively with the upstream runtime. Tests cover empty strings, ASCII names, long strings, embedded terminators, signed-byte behavior, and 10,000 deterministic randomized inputs. Expected values come from an independent C++ formulation of the hash. This is not differential testing against a running PS2.

The routine at `0x003FE818` transfers its argument into CMSAR0 and invokes `VCALLMSR` in a return delay slot. Six native tests check control-register contents, derived VU byte address, return PC, and delay-slot completion. The test deliberately does not initialize VU code memory; it verifies call address generation, not microprogram execution.

Nine Python tests cover the ISO/archive reader and dispatch-table compaction. A mismatched boot executable is rejected before the diagnostic host initializes graphics or executes game code.

## Translator correction

The original `translateVU_VCALLMSR` emitted `ctx->vi[27]`. The runtime's integer-register array has only 16 entries; control register 27 is represented separately as `vu0_cmsar0`. The local patch reads that field, retains the existing nine-bit micro-memory index mask, and preserves the runtime invocation. Six SSX 3 routines exercised this emission path.

Reference: [PCSX2's VCALLMSR implementation](https://github.com/PCSX2/pcsx2/blob/master/pcsx2/COP2.cpp) also selects CMSAR0. No game instruction bytes are patched for this correction.

The generated dispatch initializer originally had 397,540 lines. The build coalesces adjacent assignments with the same target, producing 36,892 lines. Before writing the compact form, the tool compares the complete final slot-to-function mapping, preserving gaps and overwrite order.

## First reproduced startup blocker

`local/boot.log` records:

```text
[probe] ELF loaded; starting at 0x100008
[probe] host frame=120 guest cycles=353901536 thread=1 pc=0x42c278
[probe] host frame=240 guest cycles=712718360 thread=1 pc=0x42c278
[probe] host frame=360 guest cycles=1081365792 thread=1 pc=0x42c278
[probe] host frame=480 guest cycles=1445097912 thread=1 pc=0x42c278
[probe] host frame=600 guest cycles=1808830040 thread=1 pc=0x42c278
[probe] Stopped at guest PC 0x42c28c
```

The cycle counts are runtime accounting, not measured PS2 performance. Host frame counts are presentation iterations, not rendered game frames.

The generated routine at `0x0042C1F0` repeatedly calls `0x0042C1A8`, which issues syscall `0x83` (`FindAddress`). The initial search range is `0x80000000..0x80080000`, with targets `0x0042C168` and `0x0042C130`. It subtracts `0x20C` and `0x168` from the returned locations and loops until the derived bases agree. The caller is `0x0042C300`.

The runtime implements a word scan for `FindAddress`, but reproducing the title's expected PS2 kernel layout/patch behavior has not been established. A missing or incompatible kernel table is the leading explanation, **not a proven complete diagnosis**. Next: trace syscall arguments/results and writes installed by the preceding bootstrap routines, then implement the required behavior. Blindly returning success from this loop would conceal missing initialization.

## Remaining milestones

1. Verify startup function boundaries and syscall dispatch; implement the kernel initialization behavior needed to exit the reproduced loop.
2. Reach and validate the title screen, with real disc access and IOP module/RPC behavior. Investigate overlays and the separate network executable.
3. Validate VIF/DMA/VU/GS behavior against reference frames, then implement or adapt the macOS rendering path.
4. Validate controllers, audio, streaming, physics, tricks, AI, saving/loading, and scene transitions against the original game.
5. Test complete runs across the mountain and package a playable macOS app only after these behaviors work.

The [SSX 3 decompilation project](https://github.com/ssxdecomp/ssx3) is useful reference material, but its current state cannot supply the missing implementation: 307 of its 313 C++ files contain only a placeholder comment. [PS2Recomp](https://github.com/ran-j/PS2Recomp) describes its runtime as experimental and its hardware support as partial. A full port remains a substantial reverse-engineering project.
