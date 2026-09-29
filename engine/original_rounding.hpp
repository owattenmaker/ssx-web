#pragma once
#include "software_float.hpp"
#include <cfenv>
#include <stdexcept>
namespace ssx {
#if defined(__EMSCRIPTEN__)
inline thread_local int originalRoundingMode=FE_TONEAREST;
#endif
struct OriginalRounding {
 int previous;
 explicit OriginalRounding(int mode=FE_TOWARDZERO){
#if defined(__EMSCRIPTEN__)
  previous=originalRoundingMode;
  if(mode!=FE_TOWARDZERO&&mode!=FE_TONEAREST)throw std::runtime_error("Unsupported software rounding policy");
  originalRoundingMode=mode;
#else
  previous=std::fegetround();
  if(previous<0||std::fesetround(mode))throw std::runtime_error("Original rounding unavailable");
#endif
 }
 ~OriginalRounding(){
#if defined(__EMSCRIPTEN__)
  originalRoundingMode=previous;
#else
  std::fesetround(previous);
#endif
 }
 OriginalRounding(const OriginalRounding&)=delete;
 OriginalRounding& operator=(const OriginalRounding&)=delete;
};
}
