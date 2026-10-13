#pragma once
// Native build of the course core (docs/core-migration.md "Native core"), included before every source: the wasm TLS base (the
// current rider context) is native/core/rider_context_native.cpp's current context.
#ifdef __cplusplus
extern "C" {
#endif
void* native_tls_base(void);
#ifdef __cplusplus
}
#endif
#define __builtin_wasm_tls_base() native_tls_base()
#define SSX_NATIVE_CONTEXTS 1
