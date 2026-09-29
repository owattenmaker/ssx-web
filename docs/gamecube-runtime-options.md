# GameCube runtime decision checkpoint

The user prioritizes faithful original SSX3 gameplay, smooth frame delivery and low power, and questioned whether the custom route can beat Dolphin. No matched Dolphin performance/energy comparison has been run. Do not promise a custom-runtime advantage.

## Verified local state

- Game disc ID GXBE69, read directly from local/gamecube/ssx3.iso.
- Current implementation: generated native CPU code plus gamecube/sdk.c/platform.c and GXRuntime/Aurora/Metal compatibility. Latest built CPU ThinLTO experiment passes11tests but has not been live-validated. PID97243 was left running an older binary.
- Searches of /Applications, ~/Applications and Downloads found no Dolphin-named executable/file. This is a scoped search, not proof Dolphin exists nowhere on the machine.
- ModernGekko source is already vendored at revision5417826c31187d4dadf8588c7aa25bf107782936 with Dolphin revision55c7b023fa0f4eba1cf3fdbbb25b1c5ec468d5ac. No runner binary found under build or local/bin.

## Third option: compiled game plus Dolphin runtime

Source evidence: ModernGekko/src/runtime/dolphin_runtime.cpp validates native module discID/CPUABI/structsize, then supplies the module to Dolphin JitInterface::SetStaticRecompModuleSource. SelectCPUCore defaults StaticRecomp; MODERNGEKKO_STATICRECOMP=0 selects the platform JIT. The production chassis is enabled by default in ModernGekko/CMakeLists.txt. This is substantially different from replacing the original game engine with a newly written native engine.

Important limitation: a validated native module does NOT guarantee zero fallback. Dolphin StaticRecompCore_Run.cpp contains interpreter and JIT fallback paths, and StaticRecompCore_SMC.cpp handles forced fallback for modified code. Any JIT-free/phone claim requires actual coverage counters, fallback auditing and target-platform testing. No SSX3 integration has been verified on this runtime.

The current generator inserts ssx3_sdk_dispatch at original SDK entry labels. A hybrid experiment must generate into a separate directory without blindly carrying over current runtime hooks; Dolphin should own its devices/services. Validate the ModernGekko CPU ABI (currently4) against generated module descriptors. Do not overwrite the current generated tree or assume the existing binary module is compatible.

## Decision and comparison gates

Pending user preference: stock Dolphin with targeted mods versus continued custom runtime. Hybrid is a newly identified candidate, not an accepted migration. Preserve current source/builds and running session.

Before claiming improvement, compare the same original disc/event/character, equal internal resolution and enhancements, power mode, warmup and input route. Run implementations sequentially to avoid CPU/GPU contention. Record frame-time distribution and long stalls, wall time versus game time, CPU time and measured energy where available. Validate graphics, original effects/animations, controls, audio, race completion, saving/loading and other events separately. Menu FPS, generated assembly or unit tests do not prove these end-to-end requirements.
