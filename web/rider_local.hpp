#pragma once
// One core, six riders (docs/ai-racers.md "One core, six riders"). Every mutable global and function-local static of the
// browser core is RIDER_LOCAL: thread-local storage of the rider context that is current, like the original's per-actor
// state (rider +0x000.., motion owner, controllers). The core runs one context per rider and switches between them by
// setting __tls_base (web/rider_tls.S, web/rider_context.cpp): the human's context is the static TLS block, a computer
// rider's is a heap copy of the pristine block. Only the course geometry (web/core.cpp world/cameraTerrain) and
// constant tables are plain globals, shared by all riders.
//   constinit: no lazy per-thread initialiser (web/rider_context.cpp runs the few computed initial values explicitly,
//              in the original static-initialisation order, for every new context);
//   no_destroy: contexts live as long as the page, so no per-thread destructor registration (and no TLS wrapper calls).
// Built with -matomics -mbulk-memory -ftls-model=local-exec (web/build-core.sh); a native build gets ordinary TLS.
#define RIDER_LOCAL __attribute__((no_destroy)) constinit thread_local
// Containers whose default constructor is not constexpr (std::map, std::unordered_map; a TLS address is not a constant,
// so a node-based container cannot be constant-initialised): constructed empty on the context's first use. Only for
// default-constructed state whose construction reads nothing else.
#define RIDER_LOCAL_LAZY __attribute__((no_destroy)) thread_local
// Odr-use a RIDER_LOCAL_LAZY variable: runs its translation unit's lazy constructors for the current context.
inline void rider_touch(const void* p){__asm__ volatile(""::"r"(p):"memory");}
