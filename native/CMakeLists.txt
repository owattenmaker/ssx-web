# The native build of the course core (docs/native.md): the same sources and arithmetic flags as the wasm build
# (web/build-core.sh), compiled by clang for the host, with native/core's rider contexts, host hooks and tools.
#
# Targets
#   ssx3_core          static library: the core's units plus the native rider contexts and host hooks
#   ssx3_race_driver   a headless Snow Jam race, one ?simtrace hash per tick (native/core/race_driver.cpp)
#   ssx3_replay_calls  replays a logged wasm export-call stream and checks its probes (native/core/replay_calls.cpp)
# Tests (ctest): the native results against the wasm build's (native/tests/parity.mjs).

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # The rider contexts need clang's emulated TLS (-femulated-tls and native/core/rider_context_native.cpp's
    # __emutls_get_address); MSVC's cl has no equivalent. On Windows use clang-cl.
    message(FATAL_ERROR "The native core needs clang (Apple clang, LLVM clang or clang-cl), not ${CMAKE_CXX_COMPILER_ID}")
endif()

set(SSX_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/..")
cmake_path(NORMAL_PATH SSX_ROOT)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/core_sources.cmake")
ssx_core_sources("${SSX_ROOT}/web/build-core.sh" SSX_CORE_UNITS SSX_CORE_DEFINES)
# The rider-context snapshot's registry needs web/rider_context.cpp's snapshot functions (wasm only so far): not yet native.
list(REMOVE_ITEM SSX_CORE_DEFINES SSX_SNAPSHOT_REGISTRY)

# The generated core sources come from your own disc (tools/setup_from_iso.py); they are never in the repository.
set(SSX_MISSING "")
foreach(unit IN LISTS SSX_CORE_UNITS)
    if(NOT EXISTS "${SSX_ROOT}/${unit}")
        list(APPEND SSX_MISSING "${unit}")
    endif()
endforeach()
foreach(required web/generated/snapshot engine/generated/lighting_math.hpp engine/generated/irradiance_transform.hpp
                 engine/generated/set_piece_particle_bounds.inc)
    if(NOT EXISTS "${SSX_ROOT}/${required}")
        list(APPEND SSX_MISSING "${required}")
    endif()
endforeach()
if(SSX_MISSING)
    list(JOIN SSX_MISSING "\n  " SSX_MISSING_TEXT)
    message(FATAL_ERROR "The core's generated sources are missing (make them from your own disc with "
                        "python3 tools/setup_from_iso.py, docs/native.md):\n  ${SSX_MISSING_TEXT}")
endif()

option(SSX_EXACT_FPU "Build the console-arithmetic core (-DSSX_PS2_EXACT_FPU=1, docs/ps2-float.md)" OFF)
set(SSX_CORE_CFLAGS "" CACHE STRING "Extra compiler flags for the core's units (as web/build-core.sh's SSX_CORE_CFLAGS)")
option(SSX_NATIVE_UBSAN "Trap on float-to-int conversions out of range (they differ between wasm, arm64 and x86-64)" OFF)

# The build type does not change the core's code: the wasm build has no NDEBUG (nlohmann's JSON_ASSERT stays on) and its own
# optimisation level, so the configuration flags here carry debug info at most; the core's flags below set the rest.
set(SSX_MSVC_DRIVER OFF)
set(SSX_DEBUG_INFO -g)
if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    set(SSX_MSVC_DRIVER ON)
    set(SSX_DEBUG_INFO /Z7)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "")
set(CMAKE_CXX_FLAGS_MINSIZEREL "")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${SSX_DEBUG_INFO}")
set(CMAKE_CXX_FLAGS_DEBUG "${SSX_DEBUG_INFO}")

# The arithmetic flags of web/build-core.sh: no FMA contraction, no fast-math. Signed char as wasm and Apple arm64
# (Linux aarch64 defaults to unsigned).
if(SSX_MSVC_DRIVER)
    set(SSX_CORE_FLAGS /clang:-ffp-contract=off /clang:-fno-fast-math /fp:precise /clang:-fsigned-char /EHsc /O2
        "/FI${CMAKE_CURRENT_SOURCE_DIR}/core/shim/native_shim.h")
    set(SSX_EMULATED_TLS /clang:-femulated-tls)
    set(SSX_QUIET /w)
else()
    set(SSX_CORE_FLAGS -ffp-contract=off -fno-fast-math -fsigned-char -O2
        -include "${CMAKE_CURRENT_SOURCE_DIR}/core/shim/native_shim.h")
    set(SSX_EMULATED_TLS -femulated-tls)
    set(SSX_QUIET -w)
endif()
if(SSX_NATIVE_UBSAN)
    # reported, not fatal: native/tests/parity.mjs fails a run that reports one and lists every site
    list(APPEND SSX_CORE_FLAGS -fsanitize=float-cast-overflow)
endif()
separate_arguments(SSX_EXTRA_CORE_FLAGS NATIVE_COMMAND "${SSX_CORE_CFLAGS}")

set(SSX_CORE_INCLUDES
    "${CMAKE_CURRENT_SOURCE_DIR}/core/shim"
    "${SSX_ROOT}/engine"
    "${SSX_ROOT}/web/third_party/nlohmann")

set(SSX_CORE_SOURCES "")
foreach(unit IN LISTS SSX_CORE_UNITS)
    list(APPEND SSX_CORE_SOURCES "${SSX_ROOT}/${unit}")
endforeach()
list(APPEND SSX_CORE_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/core/host_hooks.cpp")

# The core's units: every RIDER_LOCAL (thread_local) access through __emutls_get_address, i.e. the current rider context.
add_library(ssx3_core_units OBJECT ${SSX_CORE_SOURCES})
target_compile_features(ssx3_core_units PUBLIC cxx_std_20)
set_target_properties(ssx3_core_units PROPERTIES CXX_EXTENSIONS OFF)
target_include_directories(ssx3_core_units PRIVATE ${SSX_CORE_INCLUDES})
target_compile_definitions(ssx3_core_units PRIVATE ${SSX_CORE_DEFINES} $<$<BOOL:${SSX_EXACT_FPU}>:SSX_PS2_EXACT_FPU=1>)
target_compile_options(ssx3_core_units PRIVATE ${SSX_CORE_FLAGS} ${SSX_EMULATED_TLS} ${SSX_QUIET} ${SSX_EXTRA_CORE_FLAGS})

# The contexts themselves (this unit defines __emutls_get_address, so it is compiled without emulated TLS).
add_library(ssx3_core STATIC
    $<TARGET_OBJECTS:ssx3_core_units>
    "${CMAKE_CURRENT_SOURCE_DIR}/core/rider_context_native.cpp")
target_compile_features(ssx3_core PUBLIC cxx_std_20)
target_include_directories(ssx3_core PRIVATE ${SSX_CORE_INCLUDES})
target_compile_options(ssx3_core PRIVATE ${SSX_CORE_FLAGS})
if(SSX_NATIVE_UBSAN)
    target_link_options(ssx3_core INTERFACE -fsanitize=float-cast-overflow)
endif()

# The host tools: plain C++ over the core's C exports.
function(ssx_core_tool name)
    add_executable(${name} ${ARGN})
    target_compile_features(${name} PRIVATE cxx_std_20)
    target_include_directories(${name} PRIVATE ${SSX_CORE_INCLUDES} "${CMAKE_CURRENT_BINARY_DIR}")
    target_compile_options(${name} PRIVATE ${SSX_CORE_FLAGS} ${SSX_QUIET})
    target_link_libraries(${name} PRIVATE ssx3_core)
endfunction()

ssx_core_tool(ssx3_race_driver "${CMAKE_CURRENT_SOURCE_DIR}/core/race_driver.cpp")

# The replayer's export table: every export of the wasm build (web/build-core.sh), so one binary replays any stream.
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(SSX_DISPATCH "${CMAKE_CURRENT_BINARY_DIR}/dispatch.inc")
file(GLOB SSX_EXPORT_SOURCES CONFIGURE_DEPENDS "${SSX_ROOT}/web/*.cpp" "${SSX_ROOT}/web/*.inc")
add_custom_command(OUTPUT "${SSX_DISPATCH}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/gen_dispatch.py" --exports "${SSX_ROOT}/web/build-core.sh" "${SSX_DISPATCH}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/gen_dispatch.py" "${SSX_ROOT}/web/build-core.sh" ${SSX_EXPORT_SOURCES}
    COMMENT "Generating the replayer's export table"
    VERBATIM)
ssx_core_tool(ssx3_replay_calls "${CMAKE_CURRENT_SOURCE_DIR}/core/replay_calls.cpp" "${SSX_DISPATCH}")

set(SSX_WASM_CORE "${SSX_ROOT}/web/runtime/core.js" CACHE FILEPATH "The wasm core the parity tests compare against")
# The native replay verifier (docs/native.md "N2"): a replay file re-simulated as the page's ?verify path does it.
# The vendored stb implementations (PNG, raw deflate), compiled once
add_library(ssx3_stb STATIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb_impl.cpp")
target_include_directories(ssx3_stb PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party")
target_compile_options(ssx3_stb PRIVATE ${SSX_QUIET})
ssx_core_tool(ssx3_verify_replay "${CMAKE_CURRENT_SOURCE_DIR}/verify/verify_replay.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/verify/inflate.cpp")
target_include_directories(ssx3_verify_replay PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/verify")
target_link_libraries(ssx3_verify_replay PRIVATE ssx3_stb)
# The wasm core id this build corresponds to (the page's id: the first 8 bytes of SHA-256 of core.wasm, hex). Default: the id of
# SSX_WASM_CORE's core.wasm, read again whenever that file changes; deploy passes the id of the core it ships (-DSSX_CORE_ID=).
# It is correct only when that wasm core was built from the same sources as this binary.
set(SSX_CORE_ID "" CACHE STRING "The wasm core id the native verifier reports (empty: from SSX_WASM_CORE's core.wasm)")
set(SSX_CORE_ID_USED "${SSX_CORE_ID}")
string(REGEX REPLACE "\\.js$" ".wasm" SSX_WASM_FILE "${SSX_WASM_CORE}")
if(NOT SSX_CORE_ID_USED AND EXISTS "${SSX_WASM_FILE}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SSX_WASM_FILE}")
    file(SHA256 "${SSX_WASM_FILE}" SSX_WASM_SHA256)
    string(SUBSTRING "${SSX_WASM_SHA256}" 0 16 SSX_CORE_ID_USED)
endif()
if(NOT SSX_CORE_ID_USED)
    set(SSX_CORE_ID_USED "unknown")
endif()
target_compile_definitions(ssx3_verify_replay PRIVATE "SSX_CORE_ID=\"${SSX_CORE_ID_USED}\"")
message(STATUS "ssx3_verify_replay reports core ${SSX_CORE_ID_USED}")

# ---- The native front end (docs/native.md "N3"): webgpu.h (wgpu-native) rendering, SDL3 for the window and input ----
option(SSX_BUILD_FRONTEND "Build the native front end (fetches the pinned wgpu-native release; the window needs SDL3)" OFF)
if(SSX_BUILD_FRONTEND)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/wgpu_native.cmake")
    # the renderer: course, sky, rider and the composites from the exported assets, offscreen or into a window surface
    add_library(ssx3_render STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/frontend/gpu.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/frontend/image_io.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/frontend/texture_archive.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/frontend/world_renderer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/frontend/post_effects.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/frontend/rider_renderer.cpp")
    target_link_libraries(ssx3_render PUBLIC ssx3_stb)
    target_compile_features(ssx3_render PUBLIC cxx_std_20)
    target_include_directories(ssx3_render PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/frontend" "${CMAKE_CURRENT_SOURCE_DIR}/third_party"
        "${SSX_ROOT}/web/third_party/nlohmann")
    target_link_libraries(ssx3_render PUBLIC wgpu::native)
    # headless: render a frame offscreen and write a PNG (the visual-parity checks; no window)
    ssx_core_tool(ssx3_render_shot "${CMAKE_CURRENT_SOURCE_DIR}/frontend/render_shot.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/verify/inflate.cpp")
    target_include_directories(ssx3_render_shot PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/verify")
    target_link_libraries(ssx3_render_shot PRIVATE ssx3_render)
    # the window (SDL3: zlib licence, a system install; docs/native.md "N3")
    find_package(SDL3 CONFIG)
    if(SDL3_FOUND)
        ssx_core_tool(ssx3 "${CMAKE_CURRENT_SOURCE_DIR}/frontend/app.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/verify/inflate.cpp")
        target_include_directories(ssx3 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/verify")
        target_link_libraries(ssx3 PRIVATE ssx3_render SDL3::SDL3)
    else()
        message(STATUS "SDL3 not found: the windowed front end (ssx3) is not built; ssx3_render_shot is")
    endif()
    add_executable(ssx3_image_compare "${CMAKE_CURRENT_SOURCE_DIR}/frontend/image_compare.cpp")
    target_link_libraries(ssx3_image_compare PRIVATE ssx3_render)
endif()

# ---- Parity tests: the native results against the wasm build's (native/tests/parity.mjs) ----
# SSX_WASM_CORE: the wasm reference (web/runtime/core.js by default). The comparison is meaningful only when that core was
# built from the same sources as this build (CORE_OUT=<dir> sh web/build-core.sh, then -DSSX_WASM_CORE=<dir>/core.js).

set(SSX_ASSETS "${SSX_ROOT}/web/public/assets" CACHE PATH "The exported game data (tools/setup_from_iso.py)")
set(SSX_PS2_RUNS "${SSX_ROOT}/local/ps2-capture/runs" CACHE PATH "PS2 capture runs for the gate-replay tests (optional)")
find_program(SSX_NODE node)
if(CMAKE_CROSSCOMPILING)
    # a cross build (Windows with llvm-mingw): its programs cannot run here; the test checks they link standalone
    if(SSX_NODE)
        find_program(SSX_OBJDUMP llvm-objdump HINTS "${LLVM_MINGW}/bin" NO_CMAKE_FIND_ROOT_PATH)
        add_test(NAME windows_link COMMAND "${SSX_NODE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/check-pe.mjs" "${SSX_OBJDUMP}"
            $<TARGET_FILE:ssx3_race_driver> $<TARGET_FILE:ssx3_replay_calls> $<TARGET_FILE:ssx3_verify_replay>)
    endif()
elseif(SSX_NODE)
    set(SSX_PARITY "${CMAKE_CURRENT_SOURCE_DIR}/tests/parity.mjs")
    set(SSX_PARITY_ARGS
        --wasm-core "${SSX_WASM_CORE}"
        --assets "${SSX_ASSETS}"
        --runs "${SSX_PS2_RUNS}"
        --cache "${CMAKE_CURRENT_BINARY_DIR}/parity-cache"
        --race-driver $<TARGET_FILE:ssx3_race_driver>
        --replay-calls $<TARGET_FILE:ssx3_replay_calls>)
    # Snow Jam, Zoe alone: 3000 ticks with the scripted pad
    add_test(NAME core_race_solo COMMAND "${SSX_NODE}" "${SSX_PARITY}" race ${SSX_PARITY_ARGS})
    # Snow Jam with the course's five computer riders (the core's rider manager, six rider contexts)
    add_test(NAME core_race_ai COMMAND "${SSX_NODE}" "${SSX_PARITY}" race --ai ${SSX_PARITY_ARGS})
    # a pause Give Up through the results
    add_test(NAME core_race_give_up COMMAND "${SSX_NODE}" "${SSX_PARITY}" race --ai --ticks 1600 --give-up 900 ${SSX_PARITY_ARGS})
    # the event-race gate's export-call stream (web/compare-ps2-capture.mjs on the wasm core), replayed natively
    add_test(NAME core_gate_event_race COMMAND "${SSX_NODE}" "${SSX_PARITY}" gate event-race ${SSX_PARITY_ARGS})
    # the native replay verifier against the browser verifier's verdicts and per-tick traces (fixtures: docs/native.md "N2")
    set(SSX_VERIFY_FIXTURES "${SSX_ROOT}/local/native-port/fixtures" CACHE PATH "Replay files with the browser verifier's verdicts and traces")
    add_test(NAME core_verify_replays COMMAND "${SSX_NODE}" "${SSX_PARITY}" verify --fixtures "${SSX_VERIFY_FIXTURES}"
        --verify-replay $<TARGET_FILE:ssx3_verify_replay> ${SSX_PARITY_ARGS})
    set_tests_properties(core_race_solo core_race_ai core_race_give_up core_gate_event_race core_verify_replays PROPERTIES SKIP_RETURN_CODE 77)
    set_tests_properties(core_verify_replays PROPERTIES TIMEOUT 1800)
    set_tests_properties(core_gate_event_race PROPERTIES TIMEOUT 1800)
    # the native front end's pad input (native/frontend/pad_input.hpp) against web/pad-input.js; needs no game data
    add_executable(ssx3_pad_input_test "${CMAKE_CURRENT_SOURCE_DIR}/tests/pad_input_test.cpp")
    target_compile_features(ssx3_pad_input_test PRIVATE cxx_std_20)
    target_include_directories(ssx3_pad_input_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/frontend" "${SSX_ROOT}/web/third_party/nlohmann")
    target_compile_options(ssx3_pad_input_test PRIVATE ${SSX_CORE_FLAGS} ${SSX_QUIET})
    add_test(NAME frontend_pad_input COMMAND "${SSX_NODE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/pad-cases.mjs" --run $<TARGET_FILE:ssx3_pad_input_test>)
else()
    message(WARNING "node not found: the parity tests (they run the wasm reference) are not added")
endif()
