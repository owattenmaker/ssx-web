# GameCube engine continuation — 2026-09-09

The user explicitly chose to resume the GameCube route after discussing the cost of rebuilding the whole engine. This supersedes earlier requirements prohibiting a graphics/platform compatibility layer. Preserve original engine behavior using ahead-of-time compiled PowerPC code on ARM64 plus native platform services and Metal graphics adapters. Future iOS portability is desirable, not yet proven. Keep `engine/` and its installed Desktop preview intact as a research/testbed and usable prototype.

## Active build

`gamecube/`, existing `build/gamecube-metal`, DolRecomp-generated C under `local/gamecube/dolrecomp/generated`, standalone GXRuntime/Aurora/Dawn Metal backend. No generic CPU interpreter. Dependencies/asset paths and historical recovery evidence are in `current-work.md`.

```sh
.venv/bin/python tools/gamecube_generate.py
.venv/bin/cmake --build build/gamecube-metal --target ssx3_gc_probe ssx3_platform_tests -j8
.venv/bin/ctest --test-dir build/gamecube-metal --output-on-failure
SSX_PROBE_SECONDS=180 build/gamecube-metal/ssx3_gc_probe.app/Contents/MacOS/ssx3_gc_probe
```

The original DOL hash is checked before generation. Do not overwrite extracted discs. Build now defaults to `SSX3_GAME_OPT=-O2`; performance and fidelity validation remain. `SSX_PROBE_SECONDS` bounds the outer dispatch loop, not every nested operation. App bundle ID is `local.ssx3.native-development`. Avoid duplicate instances: asking UI tools for a stopped app can relaunch it without the bounded environment.

## This continuation

- Rebuilt and ran existing Metal app, verified original loading snowflake.
- Added native empty-slot boundaries for original `CARDProbeEx`0x80298E04 and `CARDMountAsync`0x802994C8. Valid channels return SDK NOCARD(-3), invalid channels FATAL(-128), outputs untouched. Mount fails synchronously; no callback for an unaccepted operation. This is an honest no-card implementation, **not persistent save support**. No save container was modified.
- Visually reached original “No Memory Card found in Slot A” with “Continue without Saving”/“Retry”, beyond former checking-card stall.
- Found input had no actual host route: `DolSiDevice` only models register/interrupt status, and did not populate pad state. Added verified `PADRead`0x80290D44..80291040 boundary to native backend: four12-byte records, big-endian button16, signed sticks, triggers/analog bytes, signed error at+10, padding+11 untouched, return motor mask. Native pad backend initializes lazily.
- Hook metadata is in `config/gamecube-sdk.json`; regeneration inserts hooks reproducibly. SDK implementation is `gamecube/sdk.c`. Platform tests now link sdk.c and cover no-card results/output preservation and synthetic PADRead layout/motor-mask/errors.
- Platform CTest passes with the new tests. Logs in `local/gamecube/resume-*.log`.

Keyboard mapping for this GameCube build differs from the custom preview: J=A, K=B, U=X, I=Y, Return=Start, arrows=D-pad, WASD=left stick, Q=Z, E=L, R=R. The four-shoulder PS2 scheme is future targeted game modification, not implemented here.

## Next milestone and remaining risks

Reach original menus and one playable run before broad optimization or replacing renderer/engine components. Verify no-save selection with the corrected PADRead path, then pursue each concrete startup failure. Persistent cards require the existing GXRuntime memory-card container API plus verified SSX SDK bindings, deferred callback/context semantics, and explicit preservation of existing saves. Do not pretend saves succeed or format existing data implicitly.

Rendering is visibly vertically compressed and occasionally black between captured frames. Menus/gameplay/audio correctness, GPU projection/copy scaling, optimized execution and platform completeness remain unproven. Earlier helper math fidelity from `engine/` is useful reference, not proof that this route already plays correctly. No new GameCube app has replaced the installed custom native preview.

Latest UI checkpoint: the corrected-input build rendered the no-card prompt, but the next Up-key test was blocked because the Mac locked and automatic unlock paused after physical input. Actual menu selection with the new PADRead boundary is still unverified. Resume with fresh UI state after unlock. Diagnostic run uses SSX_PROBE_SECONDS=180 and `resume-pad-menu.log`.

## Optimized startup checkpoint

Next goal turn rebuilt all176 generated C chunks with `SSX3_GAME_OPT=-O2` (now the CMake default; use -O0 only for instruction debugging). Original arithmetic contract still uses `-ffp-contract=off`. Platform regression tests passed; bounded20-second graphics startup finished with exception0 and reached empty-slot handling. This establishes startup stability, not optimized whole-game equivalence. Logs: `optimize-build.log`, `optimized-graphics.log`, `transform-check.log`.

Added `SSX_TRACE_GRAPHICS=1` to enable existing backend statistics via AuroraBackendConfig. Beware its `parsed-state` projection reports the legacy Aurora GX state, which gxcore bypasses; zeros there do not prove missing projection. A run with DOL_AURORA_RECOMP_DRAW_TRANSFORM_LOG and minframe200 produced no transform observer rows despite gxcore submitted1492/rejected0/failed0; inspect the gxcore route directly or wire its draw observer before relying on that diagnostic. Vertically compressed UI remains unresolved. No optimized build was installed over the user's custom native preview.

## XFB aspect fix (next goal turn)

Actual gxcore matrices captured via new `SSX_TRACE_CORE=1` observer (`gxruntime-core-diagnostics.patch`): viewport320,-224,16777216,662,566,16777216; ortho projection0.003125/-0.0041666669 with identity model. This ruled out a large matrix-scale error.

Fixed `gxcore_draw.cpp` XFB copy width: previously width used mapped/high-DPI source width while height was overwritten by native console XFB height. Presentation then letterboxed the incorrectly wide texture. XFB destination width now uses `cmd.width`, in the same units as XFB height; texture-copy behavior is unchanged. Reproducible patch: `patches/gamecube/gxruntime-xfb-dimensions.patch`. Built successfully and visually verified original card text and snowflake now have normal proportions. Logs `xfb-fix-build.log`, `xfb-fix-run.log`; UI screenshot was viewed through Computer Use. Full rendering parity not established.

The60-second diagnostic ended while UI inspection was active; the tool reopened the app when targeting the exited bundle. Latest visible screen was the loading snowflake from that new instance. Do not mistake this restart for menu navigation success. Actual new PADRead menu interaction still needs proof. Inspect live processes and fresh UI before launching again.

## Controller sampling interrupt correction

Live `SSX_TRACE_PAD=1` showed the game called PADRead only once. Inspected original caller801D0B5C and sampling callback registration80291A9C: the engine gates further reads on the SDK sampling interrupt. `ssx3_platform_tick` incorrectly passed SDK-style0x80000000 to `dol_si_latch_poll`, whose API expects low bits1<<channel. Corrected to0x0F for four channels. Live trace now proves repeated PADRead calls; added platform regression requiring RDST status0x20202020 after a retrace. Tests pass.

New opt-in SDK trace logs initial PAD reads and button transitions. Files: gamecube/sdk.c, platform.c, platform_tests.c. Logs `pad-trace-run.log` (one read), `si-mask-run.log` (repeated reads), `si-mask-build.log`.

Live Up-key test after correction still produced no button transition in the SDK trace; screenshot during focused interaction was black. Keyboard-event delivery remains unresolved, distinct from the now-fixed sampling interrupt. Aurora PADRead only samples SDL_GetKeyboardState, so investigate whether short down/up events drain between slow original reads; do not assume that hypothesis is proven or that gameplay is reached. Current diagnostic run is bounded120 seconds; inspect process/session state before restarting.

## Keyboard input verified; original intro reached

Implemented event-to-PAD tap retention in Aurora (`gxruntime-keyboard-taps.patch`): nonrepeat SDL key-down persists until one PADRead, ORed with held state; focus loss clears pending taps. This fixes short down/up pairs disappearing between slow game samples. No synthetic game commands or menu bypasses. Added `ssx3_keyboard_taps_tests`: transient tap once, held state across reads, repeat suppression, focus clear, bounds. Both native platform and tap tests pass.

**LIVE PROOF:** `keyboard-taps-run.log` records Up at read356/buttons0008 and release357; screenshot shows selection changed from Retry to Continue Without Saving. J/A at read603/buttons0100 and release604 confirmed the selection. Same process progressed to the original animated EA Sports BIG intro. This supersedes earlier notes saying input/menu confirmation were unverified.

The intro shows severe magenta/green blocky color corruption while recognizable logo geometry animates. Next inspect movie texture/YUV conversion, TEV inputs and decoded frame data; cause is not yet established. Return/Start was pressed during intro, but a transition beyond intro was not observed at that check. Original main menus and gameplay still unproven. Run was bounded180 seconds, session72802; verify live/terminal state before further UI calls to avoid accidental restart. Evidence stored in private log; screenshots viewed through Computer Use.

## Movie texture evidence

Added opt-in first multi-texture draw capture `SSX_CAPTURE_MULTITEX=<local-prefix>` via `gxruntime-multitex-capture.patch`. Captures native texture bytes/metadata; updated source also captures TLUT bytes (requires rebuilding/rerun to obtain those). Actual files `local/gamecube/movie-first.*` are from first implementation, without palette dumps. Live startup Up/J again reached intro under this diagnostic.

First frame has mask0x7F,4vertices: tex0/1 CI8(9),320x240, all128 (neutral chroma); tex2 CI8,640x480, all16 (black luminance). Auxiliary tex3/4 IA8(3),320x4; tex5/6 IA8,640x4 contain lookup patterns. This is EA MAD (`data/movies/eabig.mad`), not THP. Initial planes are sensible; later-frame decode correctness is not proved. Next inspect actual TLUTs and seven-texture/TEV/indirect conversion before blaming the CPU movie decoder. No decoder or conversion behavior changed this turn.

`movie-capture-run.log` belongs to a300-second diagnostic, tool session12857. New source palette capture was edited while that older binary was running; rebuild only after treating that run's results consistently. Tests from prior input fixes remain the latest runtime tests.

## Full palette/shader capture and next concrete lead

`local/gamecube/movie-palettes.*` now contains first movie draw's seven textures, seven palettes, generated WGSL, pixel uniform block and metadata. All CI palettes resolve as IA8/256entries. Neutral first-frame U/V index128: palette0 bytes F689, palette1 8055; Y index16 palette2 FF10. Palette1/2 are smooth ramps, palette0 around indices120..136 is irregular (459c,80a9,2bcb,1fff,...), unlike a conversion LUT. First movie neutral planes remain all128/128/16. WGSL has five TEV stages and four indirect texture lookups. This materially narrows the failure but is not a proven full explanation of later-frame corruption.

Critical source finding: `graphics/frontend/src/render_sink.cpp` case RenderResourceKind::Tlut only increments a counter; it does not snapshot the loaded bytes. Texture case later resolves `tlut_address` directly to guest RAM and retains a raw pointer. Hardware loads palettes into texture memory, so source-memory reuse must not change an already-loaded palette. Investigate/fix load-time palette lifetime, with source-overwrite regression; also preserve TMEM slot identity, reload and subrange semantics rather than merely caching forever by RAM address. Relevant render_sink.hpp fields currently expose tlut RAM address/entries/format but no load snapshot ownership. Packet TLUT event resource.index=event.a,address=event.b,size=event.c,format=event.d.

Capture helper patch now also writes `.wgsl` and `.pixel.bin` plus `.tlutN.bin`. Built successfully (`palette-capture-build.log`); run `palette-capture-run.log`, live tool session34138,300-second bound. Original card UI Up/J was exercised again. No palette-lifetime correction has been made yet; no claim the intro is fixed. User expressed concern this approach recreates an inferior emulator; acknowledged substantial compatibility work and did not claim native compilation avoids those costs. Active automated goal still requests perfect GameCube route.

## Palette-memory implementation and falsifying live check

Added `gxruntime-tlut-memory.patch`: consuming render sink snapshots palette-load bytes into512KiB TMEM storage (512-byte slot addressing), supports range overwrites, and gives bound/queued draws owned palette snapshots. Texture trace event.g carries TMEM offset+1 (zero absent), passed through RenderResource. Non-CI textures no longer spuriously resolve an unrelated palette. Reload refreshes bound palettes while previously queued snapshots retain old data.

Tests `gamecube/tlut_memory_tests.cpp` cover source overwrite, partial destination overwrite, independent slots, queued snapshot lifetime, unknown ranges, reset and bounds. Also tests the actual ConsumingAuroraRenderSink: palette load → source RAM overwrite → texture bind → reload, checking bytes at each stage. All3 GameCube CTests pass. Build `tlut-memory-build.log`, sink test `tlut-sink-test-build.log`.

**Live result: intro STILL corrupted.** `movie-tmem.*` capture shows palette0 at index128 remainsF689 even after load snapshots; palettes1/2 stay8055/FF80. Thus later RAM reuse alone did not explain this run. Next inspect exact palette0 source address and bytes when LOADTLUT is processed, plus FIFO processing timing and original table generation. Do not claim this corrected the visible intro. Source metadata for interior/non-exact palette bindings in gx_recomp remains limited; native TMEM storage supports overlapping ranges but full original event metadata coverage still needs audit.

Live run session79626,300-second bound, `tlut-memory-run.log`. Fresh UI confirmed Up/J and then visibly corrupted EA BIG intro. Active goal unfinished.

## Verified intro color fix: GameCube TLUT address masking

Rejected-load trace proved slot512 was requested with LOADTLUT0=00C63A9A, yielding18C75340 and failing guest resolution. GameCube ignores upper address bits for palette loads: local Dolphin `Source/Core/VideoCommon/BPStructs.cpp` BPMEM_LOADTLUT1 explicitly masks with01FFFFFF. Correct source is00C75340 (beside movie palette1 at00C75140 and Y palette at00C74F40). Old failed load left a prior font palette bound; this explains the irregular palette0 bytes. Palette retention was a separate correctness fix, not the cause of the observed intro failure.

Applied25-bit mask only to TLUT load source in gx_recomp.c. Added actual resolve_tmem_tlut regression with high address bits set; all3 GameCube tests pass. New trace shows slot512 source00C75340/middle2A80. **LIVE VISUAL VERIFIED: EA Sports BIG intro now has orange background, white/black logo and readable copyright text; magenta/green corruption is gone.** No frame-perfect pixel claim or full movie suite verification yet.

Palette memory, load diagnostics and address correction are consolidated into `patches/gamecube/gxruntime-tlut-memory.patch` (supersedes temporary tlut-trace/address patches, removed after consolidation). Reverse-apply check succeeds against current vendor tree. Build `tlut-address-build.log`, runtime `tlut-address-run.log`, current run session29071 bounded300seconds. Return/Start was pressed after corrected logo appeared; next-stage transition still needs observation. Main menus, races and full game remain unproven.

## Intro end stall identified as graphics queue wait

The corrected intro stays on its final EA BIG frame while native CPU continues executing. Sampled live process64557 (`intro-stall.sample.txt`) rather than treating it as a crash. Added opt-in observation boundary8022C274 (`SSX_TRACE_MOVIE=1`) with source-hook metadata; no control-flow bypass. Captured object804C6150/vtable802F7C30/ready8021A4EC. Disassembly: ready calls801CBB64, requiring r13-504C(word)==0 and r13-506F(byte)==0.

These are GRAPHICS queue fields, not movie audio clocks:801CB9E4 emits GXSetDrawSync tokenFE00 and increments queue count;801CB8EC receives tokenFE00 and sets flag;801CB964 decrements queue count and enables/disables FIFO breakpoints through8029D650/8029D6DC. The post-retrace path must retire queued buffers.

Concrete missing platform mechanism: `gamecube/platform.c` never asserts `DOL_PI_CAUSE_CP`; `DolCp` is documented as an MMIO-only surface, not a FIFO consumer. Current linked gather writes immediately forward bytes, copy write pointer to read pointer and zero distance, ignoring GP_READ_ENABLE and breakpoint pauses. Implementing correct FIFO ingestion/breakpoint interrupts (including original unlinked capture/ring switching, pauses, wrap and retirement order) is the next task. Do not fake completion by clearing game fields or always firing a breakpoint: that would bypass queue ownership and corrupt later gameplay.

Current diagnostic session85843 is bounded300sec, log `movie-wait-run.log`; original Up/J confirmed. Built observation hook via `movie-wait-generate.log`/`movie-wait-build.log`. Main menus/gameplay still not reached. This turn changed diagnostics and obtained a concrete wait-condition call graph; no graphics-queue fix yet.

## FIFO consumer and breakpoint interrupt integration

Added `gamecube/fifo.c/.h`: reads complete32-byte bursts from CP ring in RAM, honors GP_READ_ENABLE, stops at enabled breakpoint equality, wraps end→base, updates read pointer/distance, and computes breakpoint/watermark interrupt conditions. Platform ticks assert CP cause; CPU gather only publishes completed bursts when linked. Unlinked capture still writes RAM only. Consumer submits bytes in queue order instead of producer bypassing the CP.

First integration failed with malformed FIFO opcode. CPU/GPU byte captures (`SSX_TRACE_FIFO`, now also gpu-fifo.bin) exposed missed bursts from unaligned writes crossing32-byte boundaries without ending aligned. Corrected publication gate to `(write_pointer&31)<size` and added platform regression. `ssx3_fifo_tests` covers breakpoint pause/resume, ring wrap/order and disabled reading. All4 CTests pass.

Bounded10sec `fifo-crossing-run.log` completed exception0, gxcore submitted8200/rejected0/failed0, reached card prompt. Current longer run session12224 (`fifo-gameplay-run.log`,300sec bound) was navigated Up/J into movie wait; end-of-intro progression still needs observation. Do not yet claim queue fix proves full startup. CP MMIO idle bits still come from existing surface; PE token/finish delivery remains previously timed SDK adapters and needs ordering/fidelity audit. Full GPU timing/backpressure/presentation equivalence is not established.

## Fence-state evidence after FIFO integration

Expanded SSX_TRACE_MOVIE to report queue count/token flag periodically and platform CP pointers. `fence-state-run.log`: stuck queue1/token0; CP read0059B2C0==breakpoint0059B2C0, write0059B360,distance160,ctrl0017,status001E. BP_INT bit20 is disabled after original CP handler, so the breakpoint interrupt is delivered; this is not simply a missing CP cause anymore. PI has no pending enabled PE event at observation.

Original post-retrace801CB7C8 checks token flag-506F; with it set, it retires a queued buffer via801CB964 and changes/disables the next breakpoint. CPU SDK `GXSetDrawSync` currently calls ssx3_platform_draw_sync at function entry, scheduling a single overwriteable token latch after64000devicecycles. This is not tied to FIFO consumption or GPU command order. Next replace that provisional timer-source with actual parsed GPU token/fence events (including display lists), preserving PE enable/ack behavior and event order. Do not clear queue fields manually or bypass breakpoints. No fix to this ordering was made yet; logging only this turn.

Current bounded300sec diagnostic session58949, binary has expanded sdk/platform diagnostics. Main menus/gameplay still unverified.

## Parsed GPU fence events replace SDK timers

Added `gxruntime-fence-events.patch`: GxCoreSink observes BP47 token,48 interrupt-token,45 lowbyte2 finish at FIFO stream position, flushes prior draw assembly, and calls optional backend host fence callback. Covers parsed display-list commands too. GameCube platform callback updates PE token/flags; original PE enable/ack register semantics retained. Removed SDK-entry64000-cycle draw-sync timer and OSSleepThread-based fake finish scheduling. Observation hooks remain read-only.

Tests: actual GxCoreSink callback order including repeated identical tokens and ignored nonfinish BP45; platform token value, interrupt/noninterrupt distinction, independent write-one-to-clear acknowledgment. All5 GameCube CTests pass. Build logs `fence-events-build.log`, `fence-observer-build.log`. Patch reverse-check passed.

Live `fence-events-run.log` now reports movie wait queued1/token1, where prior run stayed token0. CPread==breakpoint00591400,write005914C0,distance192,ctrl0017. Screenshot after navigation still black; transition to main menus NOT verified. Need inspect remaining execution/wait after token receipt. Notification currently means parsed/submitted draw ordering, not a Metal command-buffer-completed fence; full GPU completion/readback synchronization remains explicit work. Do not claim exact hardware timing.

Current bounded300sec diagnostic session49678; check authoritative state before re-running. No installed preview replaced.

## Live snapshots and scheduler evidence

Added optional SSX_DEBUG_SNAPSHOTS=1 support in main.c: SIGUSR1 sets a sig_atomic_t flag only; outer dispatch safely writes matched `live-ram.bin`/`live-state.json` and diagnostics. Never send SIGUSR1 to an instance not started with this flag (default OS action terminates). `tools/inspect_gc_threads.py` reads original SDK thread queue/state/context offsets (Dolphin debugger OSThread layout), bounded traversal. Build `live-snapshot-build.log`.

Bounded600sec snapshot run10575 ended during analysis (terminal confirmed), and UI reopened app without diagnostic env. Closed that instance and started persistent controlled diagnostic with SSX_PROBE_SECONDS=0, SSX_DEBUG_SNAPSHOTS=1, session50708, PID71634 at capture, `scheduler-live.log`. Verify PID/session before signaling; leave this instance for continued snapshots instead of replaying startup on every question.

Snapshot after original Up/J: CPU80288ED4 scheduler idle, currentThread0, all6threads state4(waiting), suspend0. Main803B7DB0 waits8049C01C; call stack80289968→801CAD38→801CD724→801CD874→801CA90C→800032AC. Queue count1/token1; CPread005B6360==breakpoint,write005B6400,distance160,ctrl0017. Other waits:803A2BC8→803A2B30;803985E0→8039A900;804A0030→804A0348;804B0350→804B0668;803A7288→803A75A0. Main wait is frame-synchronization object+0xC through801CAD24, not directly GXDrawDone queue(r13-4784=803DB2DC).

Next inspect VI/frame wake path:801CB7C8 post-retrace should process token-506F, retire queue through801CB964, then call chained callback-5058.801CABAC wakes manager+4338 unless manager+1466A set. Original VI register model currently asserts onlyDI0 at periodic retrace; beam/field behavior may matter but cause not proved. No scheduler wake bypass or arbitrary queue clear has been applied. Main menus/gameplay remain unverified.

## Corrected diagnosis: game progresses; presentation remains black

Repeated live snapshots DISPROVE a permanent scheduler deadlock: SDK retrace counter(r13-4924) advances; CP pointers changed005B6360→005B2B20→005A2080, and at last snapshot CPread==write,distance0,ctrl0015. VI callback pointers are intact: pre801CB888,post801CB7C8,chained801CABAC. Audio mixes/nonzero output also advances. A single all-sleeping snapshot was normal between frames, not proof of a missing wakeup.

Decoded actual YUYV framebuffer at original r13-5078 pointer804EC660 from live-ram.bin (640x448) using BT.601. Result `local/gamecube/live-xfb.png` visibly shows a moving snowboarding intro scene despite black app window. Thus game has progressed past EA BIG into further intro content. Main menus still not verified. No queue/wake bypass should be added based on earlier mistaken interpretation.

Found a concrete presentation-address bug in `interrupts.c`: TFBL side effects only ran on HI writes under assumption LO precedesHI. SSX3 VI shadow-register flush can write HI thenLO, so host address must update on either half. Added `gxruntime-vi-address-halves.patch`, tests high-first then low-only address changes against native headless current_xfb. All5 CTests pass; build `vi-address-build.log`. **Latest running PID71634/session50708 is still the OLD binary without this VI correction.** Do not claim live black-screen fix yet. Next intentionally replace that diagnostic instance with rebuilt binary and verify presentation; snapshot-enabled setup is in previous section. Retain data before restart as needed.

## Live presentation comparison after VI-half fix

Rebuilt and tested VI-half fix live: window remains black, so it is a valid address-update fix but NOT sufficient for observed blank output. Added SSX_TRACE_PRESENT diagnostics (`gxruntime-present-diagnostics.patch`): presenter selects physical004EC660,black=false,texture found640x448, matching original framebuffer pointer and dims. This rules out a completely missing/wrong framebuffer lookup in captured cases.

Added diagnostic-only `SSX_PRESENT_RAM_XFB=1` (`gxruntime-ram-xfb-diagnostic.patch`) to skip Virtual-XFB GPU cache hit and exercise existing RAM YUYV decoding path. Default behavior unchanged. Live comparison still black in app, including after Up/J. This points further downstream to final presentation/overlay/surface or shared upload/blit behavior, not proof that either framebuffer-copy path alone is culprit. Need inspect canPresent/surfaceStatus, Rml overlay choice, selected texture content at final pass and copy bind group. Current RAM diagnostic run has envflag enabled; do not mistake it for production configuration.

Current session35064, unbounded diagnostic, `ram-xfb-live.log`, snapshots enabled. Previous sessions58475 and30900 intentionally stopped for rebuilt diagnostics, terminal confirmed. Build `ram-xfb-build.log`. The earlier RAM snapshot live-xfb.png proves real movie image generation but not current presenter texture data. Main menu/gameplay unverified.

## Exact presenter texture captured black

Extended SSX_TRACE_PRESENT with final pass diagnostics: ready1,statusSuccessOptimal(1),rml0,overlay0,xfb1,viewport251,0 2057x1440. This rules out missing surface, replacement Rml overlay and zero viewport in observed frames. Fresh RAM snapshot during same investigation had nonzero YUYV data (most common29/129), but it was not synchronized to the GPU texture capture.

Added opt-in `SSX_CAPTURE_PRESENT=<directory>` one-frame GPU readback at presentation frame180 using existing plane_capture machinery. New render textures gain CopySrc usage; default behavior otherwise unchanged. Patch `gxruntime-zz-present-capture.patch` applies after `gxruntime-present-diagnostics.patch`. Exact bound XFB texture captured to `local/gamecube/present-capture/planes/f0180_n0000_efb-color.bin`:640x448, every pixelRGB0/alpha255. SHA inplanes.json. Thus that presenter input is actually black, not merely a failed final blit. Still need synchronized EFB-source/copy/filter and RAM comparison to establish why; do not infer all frames from one capture.

Current live run session51897 (unbounded), `present-capture-live.log`; started with SSX_CAPTURE_PRESENT andSSX_TRACE_PRESENT, NOT SSX_DEBUG_SNAPSHOTS. Do not SIGUSR1 this process. Previous session61926 intentionally stopped before rebuild. New capture build `present-capture-build.log` passed. No visible black-screen fix yet; main menus/gameplay remain unverified.

## Black presentation fixed: preserve EFB across host frames

Paired GPU capture (`paired-capture`): source EFB contains nonblack pixels, selected XFB is entirelyRGB0/A255. Copy diagnostics show valid source rectangle2560x1344/target2560x1440, UV0,0,1,.9333, filter16/32/16. Root issue: common.cpp begin_frame unconditionally cleared the EFB at each host presentation boundary. Original GPU FIFO pauses mean a display copy can occur AFTER that clear, losing the previous frame's image.

Added `gxruntime-zzz-efb-preservation.patch`: raw gxcore backend enables EFB persistence across host frames; freshly allocated/resized targets initialize, subsequent frames load existing color/depth. Game-authored display-copy clear is restored (`cmd.clear`) instead of relying on host clears. Retained texture reference is reset on backend shutdown. Patch reverse-check passed; final build `efb-preserve-final-build.log` passed.

**LIVE VERIFIED:** card menu visible again. `preserved-capture` display texture now contains nonblack image pixels. After original Up/J, THX screen appeared; Return then showed original SSX3 title/loading artwork. This is progression beyond EA BIG, not yet full menus or gameplay. All5 existing CTests pass; GPU captures/visual comparison provide the direct evidence for framebuffer persistence, not those unit tests. Full partial-copy clear rectangle behavior, resize/fidelity and broader GPU completion timing remain to audit.

Current live session83351, unbounded, `efb-preserve-live.log`, capture enabled but snapshots NOT enabled. Running binary predates only the shutdown-reference cleanup/comment edits; rendering fix is present. Do not signalUSR1. Keep this progressing instance for menu testing rather than restarting. Neither SSX_PRESENT_RAM_XFB nor presentation override is enabled; this is normal GPU presentation.

## First running race + optimized runtime performance (latest)

User flagged ~1FPS as fundamental and asked about hardware acceleration. Prior game code used-O2, but supporting GXRuntime/Aurora/frontend/gxcore libraries still used Debug flags. Reconfigured `build/gamecube-metal` with `-DCMAKE_BUILD_TYPE=RelWithDebInfo -DSSX3_GAME_OPT=-O2` and rebuilt all targets (748steps). Use this build type going forward; historical Debug command in current-work.md is for instruction debugging only. All5 CTests passed after rebuild.

**LIVE VERIFIED on original game engine:** title PressStart→MainMenu→SingleEvent→Zoe→Continue→Peak1→Race→SnowJam→default MyRules→race briefing→A/Continue→countdown→active downhill race. Screenshot at00:21 showed5th/6,50MPH,9%progress,score280,other riders,snow spray/tracks/course. No game code skip or fake physics was used. Actual steering/jump combinations and race finish still need testing. This supersedes earlier statements that main menus/gameplay were unverified; only narrow running-race behavior is established.

Performance evidence `release-profile-live.log`: card/intro/main menus/3Dcharacter selection reported60FPS. SnowJam briefing ~24–28FPS (4062–4661draws); downhill samples frames13800/13860/13920 reported37.6/39.1/38.0FPS (1701–1722draws). These are existing renderer counters, not proof of frame pacing/game-speed fidelity or a guaranteed course average. Target60FPS remains unmet. User's old1FPS was not measured with an equivalent recorded baseline; do not claim an exact speedup ratio.

Old diagnostic profile `snowjam-load.sample.txt`; new runtime profile `release-snowjam.sample.txt` captured briefing workload. CPU graphics command parsing/assembly and repeated texture hashing remain costs. Frequent EFB readback/YUYV materialization is still active. Optimize from fresh same-scene profiles; do not trade away required frame/memory behavior to inflate FPS.

Visible defects: Zoe head/upper-body shading is largely black at character selection; race has dark holes/triangles and some questionable colors/shadows. Original animations/movement/NPCs/HUD appear live, but visual fidelity is unfinished. Next scope is performance and these rendering defects, plus real input/race completion checks—not more menu reconstruction.

Current run session81014, unbounded, `release-profile-live.log`, PID77617 at profiling. FlagsSSX_TRACE_GRAPHICS=1,SSX_DEBUG_SNAPSHOTS=1; SIGUSR1 snapshots supported after verifying samePID. Return/Start was sent to pause the active race for further profiling. Latest UI state must be refreshed. Old custom native Desktop app remains untouched. New build not separately packaged/installed as a user release.

## Power/performance steering and idle polling experiment

User explicitly wants buttery smooth output and low power. Target sustained60FPS/frame pacing and lower CPU/GPU cost; do not equate averageFPS or CPU reduction with measured watts. Last downhill evidence remained38–39FPS, so performance goal unmet.

Added opt-in `SSX_IDLE_SKIP=1` in main.c with exact opcode guard for original scheduler loop80288ED4: lwz r0,-4A10(r13),cmplwi0,beq self. After one real empty-loop iteration, only skip repeated identical iterations up to the nearest device event, preserving the iteration-cycle lattice. Guard requires unchangedPC,interrupts enabled,no CPU exception,no pending decrementer. Next-event calculation includes VI,audio DMA,disc,ARAM,DSP mailbox and decrementer; active runnable GPU FIFO returns1 so it is not skipped. `gamecube/idle.h` helper and exhaustive deadline/iteration boundary test; all6 CTests pass. Enabled only when envvariable present; leave unset for baseline. Not default pending race verification.

Short same-binary12-second startup/card-prompt A/B, no GUI interaction: baseline (`idle-baseline.log`) user6.53s+sys1.45s=7.98s,real13.09s,127,088,304,433retiredinstructions,719VI. Opt-in (`idle-optimized.log`) user3.20s+sys1.69s=4.89s,real12.76s,35,405,678,487instructions,720VI,5,624,355,108idlecycles skipped. Both ended exception0 and reported60FPS. ~39%less CPUtime,~72%fewer retiredinstructions. Not a controlled energy/wattage measurement, whole-course test, or exact replay equality; wall-clock bounds account for slightly differing frame counts.

Old paused race session81014 was intentionally terminated for sequential A/B runs. Both benchmark sessions72794/97473 completed successfully. No active race process is promised after this checkpoint. Latest build remainsRelWithDebInfo. Next test opt-in through racing and compare matched gameplay workload before enabling by default; then profile remaining hashing/assembly/readback costs without removing required behavior.

## Avoid duplicate live draw audit

Added `gxruntime-zzzz-assembly-audit.patch`: ConsumingAuroraRenderSink retains diagnostic vertex/topology/storage audit by default, but live gxcore backend opts out unlessSSX_ASSEMBLY_AUDIT is present. It still dispatches the same span-complete draw observer, builds/validates the real DrawPlan and tracks draw count. Detailed audit counters are intentionally uncollected in this mode, not proof of zero errors. Tests/offline consumers keep default audit behavior.

`ssx3_assembly_audit_tests` compares1000 synthetic3000-vertex draw notifications/payloads with audit on/off; callback count and payloads identical. One measured bookkeeping run:2.823ms audit vs1.613ms without. This is a small synthetic stage benchmark, NOT a measured course FPS/power gain. Main output/visual fidelity not reverified on newbinary. All7 GameCube CTests pass after dependent test binaries rebuilt. Patch reverse-check passed. Logs assembly-audit-build.log,audit-regression-build.log.

Idle skipping remains opt-in and still needs race validation. No runtime process was started in this checkpoint. User targets steady60FPS and low power; prior on-course38–39FPS remains latest actual gameplay performance evidence. Next use matched course workload/profile to measure combined changes; retain complete graphics validation in real DrawPlan path.

## Opt-in synchronous texture-hash reuse

Added `gxruntime-zzzzz-hash-batch.patch`, flagSSX_HASH_BATCH=1. Fixed32-entry allocation-free memo keyed by source pointer,length,seed; active only during synchronous aurora_backend_gx_write/present call, nested scopes supported. Entries clear at outer return (guest CPU resumes) and before copy_efb_to_texture (may write guest RAM). Outside active scope original hashing runs every time. No once-per-frame assumption, persistent texture-cache shortcut, or ignored RAM invalidation introduced. Headless/direct DrawPlan calls remain uncached unless a scope explicitly begins.

`ssx3_hash_batch_tests` covers same-span reuse, distinct lengths/seeds,nesting,invalidation with data change,changes between scopes and normal nonbatched behavior. All8 CTests pass, buildhash-batch-build.log, patchreverse-check passed. This only establishes helper/integration build coverage; no real-race speed/power gain measured. Keep opt-in until profiled; cache lookup overhead may outweigh hits. Env flag read once at backend init to avoid getenv on every FIFO write. Thread-local memo prevents accidental cross-thread reuse; existing rendering ownership rules still apply.

Idle skip remains opt-in; renderer assembly audit is disabled only on live gxcore (can reenable SSX_ASSEMBLY_AUDIT). Latest race measurement is still38–39FPS before these experiments. No active run launched this turn. Next measure same-course hit rates/time and verify graphics before promoting experimental flags. User goal steady60FPS,lowpower,fullcorrectness still far from completed; visible shading/geometry defects and save support remain.

## Hash experiment measured and withdrawn

Added counters and ran12-second startup with SSX_HASH_BATCH=1 andSSX_IDLE_SKIP=1. `hash-metrics-run.log`:0hits,5894misses,0bytes saved,146,946,688bytes hashed;user3.73s/sys2.00s. This workload provides no demonstrated hash-reuse benefit; timing is not a matched course comparison. Do not advertise a hashing speedup.

Removed the experiment from active vendor code and normal CMake tests via reverse patch. Archived complete instrumented patch under `patches/gamecube/experiments/hash-batch.patch` (bootstrap glob is nonrecursive, so it is NOT applied), test source under `tools/experiments/hash_batch_tests.cpp`. Active runtime no longer includes hash-scope/cache overhead. Rebuilt app; all7 active CTests pass (`hash-removal-build.log`). Idle skipping remains opt-in; draw-audit optimization remains active. No runtime running at this checkpoint.

Next priority: verify idle skipping and draw-audit removal on matched racing workload, then profile remaining real costs. ExistingDOL_FRAME_PACING_LOG=1 logs compact counters every60frames without the much noisierSSX_TRACE_GRAPHICS output. Current best actual downhill baseline remains38–39FPS; smooth60/lowpower/visualfidelity not yet achieved.

## Idle optimization exercised in racing

Started current build withSSX_IDLE_SKIP=1,DOL_FRAME_PACING_LOG=1,SSX_DEBUG_SNAPSHOTS=1,unbounded. Original menu routeZoe→Peak1→Race→SnowJam/default rules succeeded; briefing,countdown,downhill race observed. Initial racing samples34.2/37.2/40.5FPS; later at00:01:37,47%progress,2nd/6,48MPH,score19594,logs16980/17040/17100 reported49.6/47.2/52.9FPS. These are different scene samples, NOT matched A/B gains attributable solely to idle skip or audit removal.

Attempted J jump test returned external UI change after delayed tool call. Fresh state shows continued racing, but cannot claim that requestedjump was delivered/verified. Avoid interfering if user is actively controlling app. Rendering defects remain (dark rider/terrain regions,questionable colors). CPU ps sample~120.8% at this scene; no watts/energy measure. Smooth60/lowpower target remains unmet.

Live session47717, PID84809 at last authoritative check, `idle-race-live.log`; snapshots supported after verifyingPID. Idle flag remains opt-in pending stronger deterministic timing/input comparison, race finish and broader scenarios. No new code changes this checkpoint; concrete new evidence is successful racing through47% with opt-in enabled. Preserve running instance for further read-only profiling rather than replaying startup.

## Full Snow Jam finish observed; swizzle experiment rejected

Read-only inspection of current run84809/session47717 showed93% at03:33, then original SnowJam SingleEvent Results: Zoe6th,04:10; opponents listed and restart/replay/records/quit menu visible. This verifies a full event's countdown→downhill→finish→results flow with SSX_IDLE_SKIP=1, not control-input fidelity, exact original timing, or whole-game completeness. No input sent this turn. Current game left on results screen. Snapshot saved under `local/gamecube/race-finish-checkpoint/` after SIGUSR1; RAM/register diagnostic only, not a restorable full state.

Fresh profile `current-race.sample.txt` from93% scene still includes graphics parsing/assembly, texture hashing, memory helpers and EFB readback conversion. Attempted explicit ARM NEON BGRA→RGBA swizzle. Byte tests widths0..1024/unaligned/tails passed; guarded60x2560x1440 benchmark scalar22.32ms vsSIMD21.87ms (essentially parity; compiler already optimizes scalar loop). Removed from active renderer; archived patch `patches/gamecube/experiments/pixel-swizzle.patch` and test `tools/experiments/pixel_swizzle_tests.cpp`. No performance benefit claimed. Existing readback transfer volume and per-frame allocations remain targets; don't bypass guest RAM/EFB coherence to inflate FPS.

Active app rebuilt after reversal; all7 CTests pass (`swizzle-removal-build.log`). Live race instance never loaded experimental swizzle; still on prior validated build with idle opt-in. Latest on-course samples ~42–45FPS near93%, scene-dependent. Smooth60FPS,lowpower,visual defects,save support and broader game fidelity remain incomplete.

## Reduce BP register packet initialization

Added `gxruntime-zzzzzzz-bp-packet-reuse.patch`: RetailGxFrontend reuses a dedicated RenderPacket for BP register events instead of constructing/zeroing unused draw arrays for each. Packet is3648bytes; active BP state is small. Scratch packet never handles draws/resources, and each submission still carries identical kind,sequence,event andstate. Retaining sinks copy as before. Other packet paths unchanged. `set_reuse_bp_packet(false)` retains original path for tests/measurement.

`ssx3_bp_packet_tests` compares semantic members against originalmake_render_packet across1000 varying events, and benchmarks100k events through the real frontend: fresh11.20ms/reused6.69ms in one run. Initial memcmp test failed only at padding offsets994/995 and3477..3479 of RenderDrawPacket; replaced with member-wise byte comparisons to exclude unspecified C++ padding. Do not interpret padding difference as game-data mismatch. On-course frame/power gain not yet measured.

All8 active CTests pass after dependent builds, patchreverse-check passed. Logs bp-reuse-build.log,bp-test-build.log,bp-regressions-build.log. Live race84809/session47717 still uses previousbinary (no BP reuse) and was left on results in priorcheckpoint. Rebuilt binary contains optimization; normalboot/racevisual validation still needed. Smooth60/lowpower/fullvisualandsavefidelity notcomplete.

## Runtime patch reproducibility repaired

Audit found4 overlapping top-level patches could no longer reverse-check after subsequent edits. Because bootstrap uses reverse-check for idempotence, rerunning setup could fail or attempt duplicate application. Consolidated active vendor changes against pinned f83d25877c7b701468c978c09559d513c4971ddd into `patches/gamecube/gxruntime-ssx3.patch`; archived16 prior patches underhistory/pre-consolidation. Experiments remain outside active glob.

`active-patch-manifest.json` records patch hash and23 resulting-file hashes. New `tools/verify_gamecube_runtime_patch.py` checks baseline revision, no uncovered tracked modifications, current file hashes, clean temporary apply, exact reproduction, reverse restoration and current-checkout idempotence. Verified successfully. Actual vendor source bytes and game behavior were unchanged by consolidation. `patches/gamecube/README.md` documents workflow; older individual patch paths in this log now refer tohistory/pre-consolidation.

This is setup/reproducibility evidence, not new FPS/visual/gameplay proof. Latest rebuilt app has BP packet reuse; live race session47717 previously retained olderbinary atresults. Full60FPS/lowpower,renderingfidelity,save support and broad gameplay validation remain incomplete.

## Indexed normal-matrix correction

Found source-level mismatch: WGSL used per-vertex PNMTXIDX for position but always one fixed normal matrix for lighting. Dolphin VertexShaderGen::dolphin_normal_matrix uses posidx&31 then corresponding normal rows. Added32 normal rows at tail of VertexShaderConstants, packed from captured normal matrices with existing uncaptured-position fallback. Indexed lit shader selects those rows; non-indexed path stays fixed. Uniform declarations retain all preceding optional fields so tail offsets match. Pipeline cache version8→9.

New `ssx3_indexed_normal_tests`: two distinct captured normal matrices retained in actual DrawPlan; indexed shader referencesnormalrows/PNindex and full preceding layout; nonindexed shader retainsfixedpath. Initial fixture lackedchan_captured_mask, corrected to represent a validlitdraw. All9 CTests pass. Buildindexed-normal-build.log; normal-tests-build.log. Full live Metal/visual comparison NOT yet performed; do not claim black rider issue resolved. Emboss tangent/binormal indexed transforms and broader matrix coverage still need audit.

Consolidated active runtime patch+manifest regenerated with4newtrackedfiles; verifier reproduced27files exactly and passed clean/reverse/idempotence checks. Current built app contains correction. Existing process84809/session47717 is olderbinary and was previously on race results. No newrun started this checkpoint.

## Indexed-normal live check and wider model capture

Ran rebuilt version9 shader app through original character selection. No reported shader errors, but Zoe remains dark in same regions: indexed-normal correction is source-correct for indexed lit draws, NOT established as the cause/fix of this visual defect.

Added opt-in SSX_CAPTURE_LIT prefix capturing first4 matching draw shaders, vertex/pixel uniform blocks, vertex bytes and channel/material metadata. Initial filter required shader.lit_valid; after visibly reaching Zoe selection it produced no files. This shows no hardware-lit DrawPlans under that filter in this view, so inspect vertex-color/material/texture path rather than assuming hardware lights. It does not prove original game intended no hardware lighting; captured-state loss remains possible.

Widened capture predicate to lit_valid OR perspective projection[3][2]!=0, preserving same env flag for compatibility. Latest binary built with wider filter (`model-capture-build.log`), but currently running session32888 (`lit-capture-live.log`) is OLD predicate and has no useful capture yet. Stop/restart intentionally for wider capture; no snapshots enabled. Previous session64786 was stopped for diagnostics. Consolidatedpatch/manifest updated and verifier passed27files. No claim dark shading resolved.

## Model-capture targeting corrected

Perspective-filter run93058 produced no files, but character screen was not reverified before that run was replaced; do NOT infer projection behavior from that empty result. Next run73273 used varying-Z filter and was explicitly visually confirmed at Zoe selection. Captured `zoe-depth-*` are NOT useful rider data: first3 are untextured UI quads withX0,Y0..640,Z0..448; fourth is untextured32-vertex startup geometry. The filter matched earlier geometry and exhausted its4draw budget. No character shading conclusion follows from those captures.

Latest built diagnostic now requires marker `<SSX_CAPTURE_LIT prefix>.armed`, checked once per present until armed. Then captures first4 textured draws with all3coordinate axes varying. Start with fresh prefix, navigate VISUALLY to Zoe, only then create marker. This avoids consuming capture budget on startup clears/loading geometry. Buildarmed-capture-build.log passed; consolidatedpatchmanifest regenerated and verifier passed27files. Current live session73273 still OLD unarmed filter atcharacter view; restart deliberately to use latest tool. No new shading fix; original darkrider issue remains.

## Zoe dark silhouette fixed: normal-source texture coordinates

Armed capture after visually confirming Zoe (`zoe-armed.armed`) finally collected actual rider draws:207/209vertex strips using CMPR256x256 and40/38vertex strips usingRGB5A3 64x64, PNindices+normals present, rawvertexcolors white, hardware lightingenabled0. Captured `zoe-armed-*.wgsl` showed texgen1 kept constantcoord(0,0,1,1) despite suppliednormals, then sampled lighting/reflection textures with it. Shader generator explicitly omitted Normal/BinormalT/B texture-coordinate sources. This, not the earlier indexed-light normal mismatch, caused observed dark rider shading.

Implemented those3source rows from raw normal/tangent/binormal when the vertex format contains them; absent attributes preserve original default coord, matching local Dolphin VertexShaderGen SourceRow switch. GPU vertex layout now binds these attributes even on unlit draws that need them for texgen. Cacheversion9→10. Extended source tests for all3present/absent cases; all9 CTests pass. Build `normal-texgen-build.log`; consolidatedpatchmanifest verifier passed27files.

**LIVE VISUAL VERIFIED:** Zoe character selection now shows face, coloredhair, jacket/pants details and redshoes instead of silhouette. No shader errors in `normal-texgen-live.log`. NormalGPUrendering, no texture override. This establishes the specific character defect corrected, not full pixel parity/allcharacters/course rendering.

Current run session3769, unbounded, flagsSSX_IDLE_SKIP=1,DOL_FRAME_PACING_LOG=1; atZoe selection. No snapshots flag, do notSIGUSR1. Earlier capture run8829 intentionally stopped. Capture artifacts underlocal/gamecube/zoe-armed-*; priorzoe-depth-* remain irrelevant startupgeometry. Performance and low-power targets, remaining scene defects, save support and broad game validation still unfinished.

## Projected UV sign/zero handling corrected

Source audit found all projected fragment sampling used xy/max(q,1e-6), incorrectly replacing negative and tiny nonzeroq. Local Dolphin PixelShaderGen fixed-point UV path uses q==0?xy:xy/q. Added generated gx_project_uv helper with that branch; direct TEV, indirect fixed-point and fallback texture paths call it. Cacheversion10→11. Normal-based environment coordinates on riders can have signedq; correction applies generally, not a fitted character adjustment.

Extended shader-generation regression across3sampling paths to require correctzero branch/signeddivision and no epsilonclamp. All9 CTests pass (`projected-uv-build.log`). This is codegen/source-semantic evidence, NOT actual GPU pixel or visual parity validation. Latest builtapp contains change; current live session3769 remainsoldversion10 atZoe selection. No currentrun restarted in this checkpoint. Consolidatedpatch includesnewtev.cpp; verifier passed28files.

Next livecheck broader rider/course shading andperformance, and continue remaining saves/controls/fidelity work. Goal notcomplete; do not claim terrainholes orallprojectedtextures visually fixed without comparison.

## Reuse completed Metal framebuffer readback buffers

Latest user priority: smooth gameplay and low power. Changed efb_readback.cpp to retain a successfully unmapped GPU buffer for subsequent copies of the same byte size/device. Previous code created and released a new buffer every copy. In-flight guard and mutex remain; resize/device replacement reallocates, failed/cancelled mapping discards storage. Pixel transfer, conversion and guest-visible readback remain unchanged. This removes allocation churn, not readback bandwidth or synchronization costs.

Added ssx3_readback_gpu_tests: a real headless Metal device copies 120 changing RGBA/BGRA textures through the production readback code, verifies every output byte, exercises padded rows, format changes and resize. Two allocations total (one per size), with no stale pixels or GPU validation errors. All10 CTests pass; app rebuilt (readback-reuse-build.log). Consolidated patch/manifest regenerated and verifier passed29files.

No measured on-course FPS or wattage improvement from this change yet. Existing PID97243 was observed running the previous version11 binary and deliberately left running; it does not load this newly rebuilt code. Its projected-uv-live.log had recent ~60FPS samples, but current screen/workload was NOT visually verified in this turn, so these are not evidence of steady60 racing. Prior full-course scene-dependent results still apply. Latest binary: build/gamecube-metal/ssx3_gc_probe.app. Old installed native preview remains separate and unchanged. Next: controlled racing measurement and frame-time distribution, plus investigate remaining CPU graphics processing and readback volume. Steady60FPS/lowpower goal remains incomplete.

## CPU ThinLTO experiment and broader runtime coverage

Read-only screenshot of live PID97243 showed Metro-City Race Top5 Records (PLAYER1/Nate03:13), not a controlled racing scene. Process preserved. Profile performance-current.sample.txt contains CPU helper costs alongside graphics and readback. Added optional SSX3_CPU_LTO (defaultOFF; current build cacheON), enabling CMake IPO/Apple ThinLTO for game+gxruntime. No fast-math added; generated code keeps ffp-contract=off. Built successfully cpu-lto-build.log. Pre-LTO rebuilt binary preserved at local/gamecube/ssx3_gc_probe-pre-lto. New build not live-validated or benchmarked; do not claim speed/power gains. Assembly exports cpu-{pre-lto,lto}-function.asm show changed code generation but not a runtime performance proof.

Added existing upstream runtime suite as ssx3_cpu_runtime_tests with assertions and same optionalIPO, so memory/device/paired-single tests now run locally. Suite initially failed stale VI expectation: prior product fix publishes on either TFBLhalf write. Updated test for LO→HI and added HI→LO. Intermediatezero does not publish (existingguard), hence latteronepublication vsformertwo. All11CTests nowpass. Consolidatedmanifestverifier passed30files. No product semantics altered for testfix.

User asked whether this can realistically beat Dolphin. Answered no reasonable expectation of beating it overall; possible game-specific wins unproven. AOT still retains console compatibility costs. Recommended Dolphin as practical choice for smooth reliable original gameplay, customroute chiefly for deeper engine modification. This is discussion, not authorization to discard project or migrate implementation. Goal remains active, fullfidelity/performance/power unproved. No new instance launched or existinggameinputs sent this turn.

## Runtime direction audit after Dolphin question

Previous turn classifiedprogress: optionalLTO build and expanded11testcoverage. This turn inspected localapps/builds and found a third source-supported candidate: alreadyvendoredModernGekko uses compiled modules with Dolphin's runtime. Native module validation doesnotguarantee nointerpreter/JITfallback; thosepaths explicitlyexist. Recorded exactrevisions, sourcepaths, integrationconstraints and faircomparison gates in docs/gamecube-runtime-options.md. No migration/download/buildstarted; no inputs sent to currentgame. Asked user asynchronously whether to prioritize Dolphin+mods or keepcustomruntime; answerpending atcheckpoint. Goalstillactive; implementation/power/fidelity notcomplete.

## Sam PS2 task supersedes immediate GameCube work

User explicitly requested installing Sam into PS2 for emulator gameplay, then full character-select integration. Current work and exact installed/staged/verified limitations: sam_character/PS2-INSTALL.md. Private test ISO created in Downloads; original untouched. Live PCSX2 PID4856 at lastcheck uses SamEdition, not the GameCubeapp. UI keys unreliable; user asked to reach SingleEvent→Sam with controller. No actualSam gameplayvisualverification yet. Do not call complete.

Interrupted priorGameCube turn: frame-interval logging implementation built successfully (session4722 completed), dedicated statisticaltest passes. It measures hostsubmissionintervals, notGPUdisplaycompletion. Source/manifest consolidated; verifier passed31files. No liveperformancegainclaimed. Pendingroutechoice remains unanswered.
