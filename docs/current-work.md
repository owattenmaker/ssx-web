# Current objective — full native Metal engine

> **Route change — 2026-09-09:** The user has now authorized resuming the GameCube engine route, preserving original game code compiled ahead of time with platform/graphics adapters. `gamecube/` is the active delivery path; `engine/` remains a preserved native research/playable prototype. First milestone: original menus and one playable run, not another full-engine rebuild. See [GameCube continuation](gamecube-continuation.md).


> Current integration checkpoint: [Agent handoff — 2026-09-09](HANDOFF.md). The measurements and research below describe their recorded stages; older missing-feature notes and test counts are not the latest app status.

The user explicitly wants a definitive native macOS port: direct Metal rendering and native game systems, with no console CPU/graphics compatibility runtime in the final app. Both owned discs may supply assets and behavior. Preserve PS2 four-shoulder-button trick inputs; choose asset sources by measured quality. Keep implementing toward fully playable SSX3. The current build is a native riding prototype with original assets/animation and recovered airborne integration; grounded movement, launch/landing and full game systems remain unfinished.

The active target is `engine/`, built separately with no Dolphin, GXRuntime, DolRecomp, Dawn, PS2Recomp or guest memory dependency. See `docs/native-engine.md` for current validation and next work. The `gamecube/` and PS2 targets below are development reference tools, not the final architecture.

The user also requested cousin Sam as a playable character **once the game works correctly**, styled to fit the original SSX3 roster. All14 photos in `sam_character/` have been inspected. A selected model sheet, full biography, wardrobe/board/trick concepts,18 voice lines and native integration requirements are saved under `sam_character/design/`, with prepared content in `config/characters/sam.json`. This is concept/content work, not a rigged model or live roster entry. Preserve this requirement while continuing core gameplay recovery; integrate Sam through the completed shared character systems afterward. User facts:5′11″,160lb,regular stance,Wisconsin/Midwest upbringing,modest ability,fear of inversions,chopped-unc era,prefers uphill. See `sam_character/README.md`.

# Reference runtime research (superseded as the product architecture)

The user's continuing objective is **fully playable SSX 3 on macOS**, favoring a native engine with minimal console translation. Work is ongoing; do not describe the current development app as fully playable.

## Selected route

GameCube GXBE69 is the main route after testing both builds. The user supplied `Downloads/SSX 3 (USA)/SSX 3 (USA).rvz`. It was extracted and losslessly converted with a checksum-verified nodtool. DOL SHA-256: `b92162d6c616be3ce46b4eb61d5ddbb49891bc387ea7ddb2fea5792842fa29ce`. Converted ISO SHA-1: `9f047c3c3389b5f2ad464afc472dd15cb0083474`.

The app compiles the original game CPU code ahead of time with DolRecomp, uses standalone GXRuntime, and renders via Aurora/Dawn's native Metal backend. It does not link the Dolphin emulator or use a general CPU/DSP interpreter. Compatibility for the original memory, SDK, and device contracts remains. This is not recovered, original high-level engine source.

Relevant files: `gamecube/{main.c,platform.c,sdk.c,CMakeLists.txt}`, `config/gamecube-sdk.json`, `tools/{bootstrap_gamecube.py,prepare_gamecube.py,gamecube_generate.py,dol.py}`, `patches/gamecube/`.

`local/vendor/GXRuntime` is pinned to `f83d25877c7b701468c978c09559d513c4971ddd`. Its next commit introduced an incompatible CPU header; this revision is the tested baseline. `local/vendor/DolRecomp` is pinned to `40637c4683bd2820ac5b23607ee344720beb26df`. Other checkouts in `local/vendor` were examined as alternatives/reference material and are not runtime dependencies of this app.

## Verified progress

- Compiled all GameCube text into 176 C chunks and linked an ARM64 program.
- Added native platform SPR state, cache-barrier handling, and a guarded C implementation of the game's SDK exception vector. The original native-compiled SDK handlers manage interrupts and lazy FPU switching.
- Implemented the decrementer interrupt, allowing the game's task scheduler to advance.
- Implemented FST/apploader state and host-backed asynchronous disc reads. Immediate disc completion caused recursive callbacks and a guest-stack overflow; queued completions fixed it.
- Implemented native ARAM transfers and interrupts, AID sample-buffer output, and the AX task mailbox protocol using GXRuntime's native mixer. Mixer dispatches run, though audible nonzero output has not yet been verified.
- Implemented CPU FIFO capture for display lists. Captured commands were previously sent to the renderer too early.
- Added native graphics completion signals at verified SDK boundaries.
- Fixed Metal render target format selection, EFB uniform layout (64 bytes with clamp at 40 and pixel height at 48), viewport endpoint rounding, and a MapAsync reentrant-lock deadlock.
- Fixed first-use CI palette binding: SDK SETTLUT follows IMAGE3, so the frontend must refresh the texture resource when the palette arrives. Palette reloads also refresh bound textures.
- Visually verified the game's loading snowflake and then **“Checking for Memory Card in Slot A.”** with game fonts rendered by Metal. It has not reached validated menus or gameplay.

The early disc reads were music/UI archives, not movies; preliminary diagnostic names containing “movie” were guesses and are being replaced with neutral frame-callback labels.

## Parked reference-runtime next work

Implement/verify native memory-card services to get through startup and support real saves. Standalone GXRuntime has a persistent `dol_card_*` container API. The reference client `local/vendor/StrikersRecomp/runtime/host/hle.c` shows SDK adapters and deferred callback handling; addresses in that client are for a different game and must not be reused.

Verified SSX 3 SDK addresses from original call sites and disassembly:

| Address | Function |
| --- | --- |
| `0x80295DD4` | CARDInit |
| `0x80295FDC` | CARDGetResultCode |
| `0x8029600C` | CARDFreeBlocks |
| `0x8029615C` | CARDGetMemSize (u16 output) |
| `0x802961E0` | CARDGetSectorSize (u32 output) |
| `0x80298D10` | CARDCheckAsync (wraps CheckExAsync at 0x80298780) |
| `0x80298E04` | CARDProbeEx |
| `0x802994C8` | CARDMountAsync |
| `0x80299704` | CARDUnmount |

Additional likely public APIs from the game's memory-card driver call graph, still verify signatures before binding: FormatAsync `0x80299F4C`, Open `0x8029A2B8`, Close `0x8029A3BC`, CreateAsync `0x8029A548`, ReadAsync `0x8029AA50`, WriteAsync `0x8029ADB4`, DeleteAsync `0x8029AF6C`, GetStatus `0x8029B290`, SetStatusAsync `0x8029B3A4`.

Original card control blocks: `0x803B8720`, stride `0x110`; attached at +0, result +4, size-Mbits u16 +8, sector bytes +0xC. Native card callbacks must be deferred, preserving the immediate return and CPU context. Existing card files must not be overwritten or formatted implicitly.

## Reproduction and diagnostics

```sh
python3 tools/bootstrap_gamecube.py
python3 tools/prepare_gamecube.py "$HOME/Downloads/SSX 3 (USA)/SSX 3 (USA).rvz"
python3 tools/gamecube_generate.py
.venv/bin/cmake -S gamecube -B build/gamecube -G Ninja -DCMAKE_MAKE_PROGRAM="$PWD/.venv/bin/ninja" -DCMAKE_BUILD_TYPE=Debug
.venv/bin/cmake --build build/gamecube --target ssx3_gc_probe ssx3_platform_tests -j 8
SSX_PROBE_SECONDS=8 build/gamecube/ssx3_gc_probe
```

Metal build: same CMake command with `-B build/gamecube-metal -DSSX3_GRAPHICS=ON`. App: `build/gamecube-metal/ssx3_gc_probe.app`. It defaults to the workspace's prepared data and runs until closed; `SSX_PROBE_SECONDS` bounds diagnostics. GUI runtime bundle ID is `local.ssx3.native-development`; use that with CUA. After prior crashes, macOS may show a window-restoration prompt; dismiss “Don’t Reopen” through CUA.

Logs and memory snapshots stay under `local/gamecube/`. `SSX_TRACE_CALLBACKS`, `SSX_TRACE_DSP`, `SSX_TRACE_FIFO`, and `SSX_WATCH=0x...` enable focused diagnostics. All generated code, game assets, dependencies, binaries, and logs are Git-ignored.

Native platform tests cover FST, FIFO capture/wrap, deferred disc IO, and ARAM DMA/interrupts and pass. The original GXRuntime suite passes after enabling assertions under Release. DolRecomp's 19 tests passed after supplying the venv Ninja path to its compile test. The new palette-ordering regression test needs its first run. Preserve further upstream changes as patches; `bootstrap_gamecube.py` applies all `gxruntime-*.patch` under `patches/gamecube`.

## PS2 work retained

The PS2 experiment also compiles ARM64. Follow-up fixed missing syscall handler entries (`0x42C130`, `0x42C168`, `0x42CB78`) and progresses to dynamically copied kernel code at `0x80075000`. It is parked in favor of GameCube. PS2 scripts/tests remain in the root `native/`, `tools/`, and `tests/` directories.

The unrelated `sam_character/` directory appeared during this work and has not been modified by this task.
