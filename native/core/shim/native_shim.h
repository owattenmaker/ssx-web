#pragma once
// Windows: M_PI and friends from <cmath> (the core's sources use them; glibc and Apple libc define them by default)
#if defined(_WIN32) && !defined(_USE_MATH_DEFINES)
#define _USE_MATH_DEFINES
#endif
// Native build of the course core (docs/core-migration.md "Native core"), included before every source: the wasm TLS base (the
// current rider context) is native/core/rider_context_native.cpp's current context block.
#ifdef __cplusplus
extern "C" {
#endif
void* native_tls_base(void);
#ifdef __cplusplus
}
#endif
#define __builtin_wasm_tls_base() native_tls_base()
// Headers some of the core's sources use without including them (libc++ on Apple includes them transitively; llvm-mingw's
// newer libc++ does not)
#ifdef __cplusplus
#include <string>
#endif
// The WebAssembly floating-point environment (docs/native.md "How the native core stays bit-identical"): musl's wasm fenv accepts
// fesetround and changes nothing, and fegetround is always nearest. With SSX_NATIVE_CORE the core takes its WebAssembly
// branches (software toward-zero arithmetic, engine/original_rounding.hpp), and a direct fesetround (engine/reset_fade.hpp) is
// inert as in the browser, so the hardware rounding mode stays nearest.
#ifdef __cplusplus
#include <cfenv>
inline int ssx_wasm_fegetround() {
    return FE_TONEAREST;
}
inline int ssx_wasm_fesetround(int r) {
    return r == FE_TONEAREST || r == FE_DOWNWARD || r == FE_UPWARD || r == FE_TOWARDZERO ? 0 : -1;
}
namespace std {
using ::ssx_wasm_fegetround;
using ::ssx_wasm_fesetround;
}
#define fegetround ssx_wasm_fegetround
#define fesetround ssx_wasm_fesetround
#define SSX_NATIVE_CORE 1
#endif
