// Rider contexts: one core runs all six riders, as the original's rider manager does (docs/ai-racers.md "One core, six
// riders"). Every mutable global of the core is RIDER_LOCAL (thread-local, web/rider_local.hpp); a rider context is a
// TLS block. The human's is the static block; a computer rider's is a heap copy of the pristine block with this
// module's static-initialisation work run in it, so a new context starts exactly like a freshly instantiated core.
// Only the course geometry (web/core.cpp world, cameraTerrain) and constant tables are shared.
#include "rider_local.hpp"
#include <emscripten/emscripten.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
extern "C" uint32_t rider_tls_size_value(); // web/rider_tls.S
extern "C" void rider_tls_set(void*);
// Static-initialisation work per translation unit (link order, web/build-core.sh).
void rider_statics_core();void rider_statics_rails();void rider_statics_world();void rider_statics_animation();void rider_statics_environment();
namespace {
uint8_t* tlsTemplate=nullptr;uint32_t tlsSize=0;
constexpr uint32_t kAlign=64;
}
// Before any other constructor touches thread-local state: keep the pristine static block (the .tdata image).
__attribute__((constructor(101))) static void rider_tls_capture(){
 tlsSize=rider_tls_size_value();if(!tlsSize)abort(); // web/patch-rider-tls.mjs did not run
 if(reinterpret_cast<uintptr_t>(__builtin_wasm_tls_base())%16)abort();
 tlsTemplate=static_cast<uint8_t*>(malloc(tlsSize));std::memcpy(tlsTemplate,__builtin_wasm_tls_base(),tlsSize);
}
extern "C" {
// A new rider context (a fresh core's rider state); returns its TLS block. The current context is unchanged.
EMSCRIPTEN_KEEPALIVE void* rider_context_create(){
 void* block=aligned_alloc(kAlign,(tlsSize+kAlign-1)/kAlign*kAlign);if(!block)throw std::runtime_error("Rider context allocation");
 std::memcpy(block,tlsTemplate,tlsSize);
 void* previous=__builtin_wasm_tls_base();rider_tls_set(block);
 try{rider_statics_core();rider_statics_rails();rider_statics_world();rider_statics_animation();rider_statics_environment();}
 catch(...){rider_tls_set(previous);throw;}
 rider_tls_set(previous);return block;
}
EMSCRIPTEN_KEEPALIVE void* rider_context_current(){return __builtin_wasm_tls_base();}
EMSCRIPTEN_KEEPALIVE void rider_context_enter(void* block){rider_tls_set(block);}
EMSCRIPTEN_KEEPALIVE uint32_t rider_context_bytes(){return tlsSize;}
}
