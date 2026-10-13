#pragma once
#include "software_float.hpp"
#include <cfenv>
#include <stdexcept>
namespace ssx {
// SSX_NATIVE_CORE: the native build of the core (native/core/shim/native_shim.h, docs/native.md) takes the WebAssembly branches,
// with WebAssembly's inert floating-point environment, so both builds run the same arithmetic.
#if (defined(__EMSCRIPTEN__) || defined(SSX_NATIVE_CORE))
inline thread_local int originalRoundingMode=FE_TONEAREST;
#endif
struct OriginalRounding {
 int previous;
 explicit OriginalRounding(int mode=FE_TOWARDZERO){
#if (defined(__EMSCRIPTEN__) || defined(SSX_NATIVE_CORE))
  previous=originalRoundingMode;
  if(mode!=FE_TOWARDZERO&&mode!=FE_TONEAREST)throw std::runtime_error("Unsupported software rounding policy");
  originalRoundingMode=mode;
#else
  previous=std::fegetround();
  if(previous<0||std::fesetround(mode))throw std::runtime_error("Original rounding unavailable");
#endif
 }
 ~OriginalRounding(){
#if (defined(__EMSCRIPTEN__) || defined(SSX_NATIVE_CORE))
  originalRoundingMode=previous;
#else
  std::fesetround(previous);
#endif
 }
 OriginalRounding(const OriginalRounding&)=delete;
 OriginalRounding& operator=(const OriginalRounding&)=delete;
};
}
