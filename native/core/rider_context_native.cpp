// Rider contexts in a native build through emulated TLS (docs/core-migration.md "Native core: the context pointer"): the core's
// units are compiled with -femulated-tls, so every RIDER_LOCAL access is a call of __emutls_get_address(control); this file defines
// it. Each variable gets an offset in a context block (assigned at its first access, the same for every context) and the access is
// the current block + that offset: switching the rider context is one pointer store, as the wasm build's __tls_base. A new context
// is a copy of the pristine image (every variable's initial value) with the units' static-initialisation work run in it, as
// web/rider_context.cpp does. One thread; contexts are switched, not run concurrently.
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#if defined(_WIN32)
#include <malloc.h>
#endif
#include <stdexcept>

void rider_statics_core();
void rider_statics_rails();
void rider_statics_world();
void rider_statics_animation();
void rider_statics_environment();

namespace {
// compiler-rt's / libgcc's control block of one emulated thread_local variable
struct EmutlsControl {
    size_t size;
    size_t align;
    uintptr_t index;  // its offset + 1 (0: not assigned yet)
    const void* value;  // its initial bytes (null: zero)
};
constexpr size_t kBlockBytes = size_t(4) << 20;
constexpr int kMaxContexts = 64;
// plain zero-initialised storage: other units' static initialisers reach __emutls_get_address before this unit's run
uint8_t* pristine = nullptr;
uint8_t* blocks[kMaxContexts];
int blockCount = 0;
uint8_t* current = nullptr;
size_t used = 0;
uint8_t* newBlock() {
#if defined(_WIN32)
    // the Windows CRT has no aligned_alloc; the blocks are never freed, so no _aligned_free is needed
    void* p = _aligned_malloc(kBlockBytes, 4096);
#else
    void* p = std::aligned_alloc(4096, kBlockBytes);
#endif
    if (!p) {
        std::abort();
    }
    std::memset(p, 0, kBlockBytes);
    return static_cast<uint8_t*>(p);
}
void boot() {
    pristine = newBlock();
    current = newBlock();
    blocks[blockCount++] = current;
}
// A variable's first access in any context: its offset, its initial bytes in the pristine image and in every context made so far.
__attribute__((noinline)) uintptr_t assign(EmutlsControl* c) {
    const size_t align = c->align ? c->align : 1;
    const size_t at = (used + align - 1) / align * align;
    if (at + c->size > kBlockBytes) {
        std::abort();
    }
    used = at + c->size;
    if (c->value) {
        std::memcpy(pristine + at, c->value, c->size);
        for (int k = 0; k < blockCount; ++k) {
            std::memcpy(blocks[k] + at, c->value, c->size);
        }
    }
    c->index = at + 1;
    return c->index;
}
}  // namespace

extern "C" {
void* __emutls_get_address(EmutlsControl* c) {
    uintptr_t index = c->index;
    if (__builtin_expect(index == 0, 0)) {
        if (!current) {
            boot();
        }
        index = assign(c);
    }
    return current + index - 1;
}
void* native_tls_base(void) {
    if (!current) {
        boot();
    }
    return current;
}
void rider_tls_set(void* block) {
    current = static_cast<uint8_t*>(block);
}
void* rider_context_create() {
    if (!current) {
        boot();
    }
    if (blockCount == kMaxContexts) {
        throw std::runtime_error("Rider context allocation");
    }
    uint8_t* block = newBlock();
    std::memcpy(block, pristine, used);
    blocks[blockCount++] = block;
    uint8_t* previous = current;
    current = block;
    try {
        rider_statics_core();
        rider_statics_rails();
        rider_statics_world();
        rider_statics_animation();
        rider_statics_environment();
    } catch (...) {
        current = previous;
        throw;
    }
    current = previous;
    return block;
}
void* rider_context_current() {
    return native_tls_base();
}
void rider_context_enter(void* block) {
    rider_tls_set(block);
}
unsigned rider_context_bytes() {
    return unsigned(used);
}
}

// The native host's helper (native/core/race_driver.cpp): fn in a context, the previous one back after it (also on a throw).
void native_run_in_context(void* block, const std::function<void()>& fn) {
    uint8_t* previous = current;
    current = static_cast<uint8_t*>(block);
    try {
        fn();
    } catch (...) {
        current = previous;
        throw;
    }
    current = previous;
}
