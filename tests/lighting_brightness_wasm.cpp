#include "../engine/lighting_brightness.hpp"
#include <emscripten/emscripten.h>
extern "C" EMSCRIPTEN_KEEPALIVE void lighting_brightness_batch(const float* input,float* output,unsigned count){
 for(unsigned i=0;i<count;++i)output[i]=ssx::originalLightingBrightness({input[i*4],input[i*4+1],input[i*4+2],input[i*4+3]});
}
