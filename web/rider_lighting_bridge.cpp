#include "rider_local.hpp"
#include "../engine/rider_light_asset.hpp"
#include "json.hpp"
#include <emscripten/emscripten.h>
#include <memory>
#include <cstring>
using namespace ssx;
namespace {
RIDER_LOCAL std::unique_ptr<OriginalRiderLightWorld> lightWorld;
RIDER_LOCAL OriginalRiderLightWorld::Selection selection;
RIDER_LOCAL OriginalIrradianceCoefficients shaded{},gpuCoefficients{};
RIDER_LOCAL unsigned refreshes=0,draws=0;
template<size_t N> std::array<float,N> copy(const float* input){
 if(!input)throw std::runtime_error("Missing rider lighting input");std::array<float,N> values;std::memcpy(values.data(),input,N*sizeof(float));
 for(auto x:values)if(!std::isfinite(x))throw std::runtime_error("Nonfinite rider lighting input");return values;
}
}
extern "C" {
EMSCRIPTEN_KEEPALIVE void init_rider_lighting(const char* catalog,const char* tree){
 auto loaded=std::make_unique<OriginalRiderLightWorld>(originalRiderLightAsset(nlohmann::json::parse(catalog),nlohmann::json::parse(tree)));
 lightWorld=std::move(loaded);selection={};shaded={};gpuCoefficients={};refreshes=draws=0;
}
EMSCRIPTEN_KEEPALIVE void reset_rider_lighting(){selection={};shaded={};gpuCoefficients={};refreshes=draws=0;}
// Explicit source world-query phase. Bounds are min4/max4, followed by optional
// sphere data; rankPoint is the source primary bone XYZ, not physics position.
EMSCRIPTEN_KEEPALIVE uint32_t* refresh_rider_lighting(const float* bounds,const float* rankPoint){
 if(!lightWorld)throw std::runtime_error("Rider light world not initialized");
 auto low=copy<3>(bounds),high=copy<3>(bounds+4),point=copy<3>(rankPoint);
 selection=lightWorld->refresh(low,high,point);++refreshes;return selection.ids.data();
}
// Explicit draw phase. Does not update selection or environment history.
// Optional extra input: ambient RGB followed by count directional records,
// each direction XYZ then color RGB. Controller source supports up to5.
EMSCRIPTEN_KEEPALIVE float* shade_rider_lighting(const float* environment,const float* view,const float* point,float rimScale,const float* constants,const float* extra,int count){
 if(!lightWorld)throw std::runtime_error("Rider light world not initialized");
 if(!std::isfinite(rimScale)||count<0||count>5||(!extra&&count))throw std::runtime_error("Invalid rider extra lighting");
 auto rows=copy<40>(environment);OriginalIrradianceCoefficients bank;std::memcpy(bank.data(),rows.data(),sizeof(bank));
 std::array<OriginalRiderDirectionalLight,5> directional;OriginalRiderExtraLighting effects;
 if(extra){effects.ambient=copy<3>(extra);for(int i=0;i<count;++i)directional[i]={copy<3>(extra+3+i*6),copy<3>(extra+6+i*6)};effects.directional={directional.data(),size_t(count)};}
 shaded=lightWorld->shade(selection,bank,copy<16>(view),copy<4>(point),rimScale,copy<5>(constants),extra?&effects:nullptr);gpuCoefficients=originalIrradianceScale(shaded,255.f);++draws;return shaded[0].data();
}
EMSCRIPTEN_KEEPALIVE float* rider_lighting_gpu_coefficients(){return gpuCoefficients[0].data();}
// Online races (web/net/remote-riders.js): another rider's original lighting in this core's light world,
// without touching this rider's selection/shade state (the query and shade keep no history). Inputs are
// the remote rider's own streamed values: query bounds (min4, max4), rank point (primary bone XYZ),
// its environment irradiance bank (40) and rim scalar, with this client's camera view. Returns the
// GPU coefficients (x255, 40 floats) for rider-material.js capture().
EMSCRIPTEN_KEEPALIVE float* shade_external_rider_lighting(const float* bounds,const float* rankPoint,const float* environment,const float* view,const float* point,float rimScale,const float* constants){
 if(!lightWorld)throw std::runtime_error("Rider light world not initialized");
 if(!std::isfinite(rimScale))throw std::runtime_error("Invalid rider rim scale");
 RIDER_LOCAL static OriginalIrradianceCoefficients external{};
 const auto chosen=lightWorld->refresh(copy<3>(bounds),copy<3>(bounds+4),copy<3>(rankPoint));
 auto rows=copy<40>(environment);OriginalIrradianceCoefficients bank;std::memcpy(bank.data(),rows.data(),sizeof(bank));
 external=originalIrradianceScale(lightWorld->shade(chosen,bank,copy<16>(view),copy<4>(point),rimScale,copy<5>(constants),nullptr),255.f);
 return external[0].data();
}
EMSCRIPTEN_KEEPALIVE uint32_t* rider_lighting_selection(){return selection.ids.data();}
EMSCRIPTEN_KEEPALIVE float* rider_lighting_info(){RIDER_LOCAL static float out[5];out[0]=bool(lightWorld);out[1]=refreshes;out[2]=draws;out[3]=selection.candidates.size();out[4]=0;for(auto id:selection.ids)out[4]+=bool(id);return out;}
}
#ifdef SSX_SNAPSHOT_REGISTRY // the rider-context snapshot's registry (web/generate-snapshot-registry.mjs, docs/replay.md §2a)
#include "generated/snapshot/rider_lighting_bridge.inc"
#endif
