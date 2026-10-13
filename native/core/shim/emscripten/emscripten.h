#pragma once
// Native build of the course core (docs/core-migration.md "Native core"): the emscripten names the core's sources use.
// EMSCRIPTEN_KEEPALIVE keeps the export; an EM_JS hook (a call into the page) becomes a C function the native host defines
// (native/core/host_hooks.cpp).
#define EMSCRIPTEN_KEEPALIVE __attribute__((used))
#define EM_JS(ret, name, params, ...) extern "C" ret name params;
