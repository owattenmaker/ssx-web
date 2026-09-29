#include "environment_lighting.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <algorithm>
#include <cfenv>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {namespace {
using Round=OriginalRounding;
float M(float a,float b){return terrain_original::mul(a,b);}
float VAdd(float a,float b){return terrain_original::add(a,b);}
float A(float a,float b){return originalScalarAdd(a,b);}float S(float a,float b){return originalScalarSubtract(a,b);}
}
EnvironmentColour originalEnvironmentTextureSample(const OriginalEnvironmentTexture&t,float u,float v){
    Round round;if(!t.width||!t.height)return {1,1,0,1};if(t.rgba.size()!=(t.width+1)*(t.height+1))throw std::runtime_error("Incomplete original environment sample lattice");
    u=S(u,float(int(u)));v=S(v,float(int(v)));u=M(u,float(t.width));v=M(v,float(t.height));
    float x0=std::clamp(S(u,.5f),0.f,float(t.width)),x1=std::clamp(A(u,.5f),0.f,float(t.width));
    float y0=std::clamp(S(v,.5f),0.f,float(t.height)),y1=std::clamp(A(v,.5f),0.f,float(t.height));
    float x=S(x0,float(int(x0))),y=S(y0,float(int(y0))),ix=S(1.f,x),iy=S(1.f,y);
    std::array<unsigned,4> xs{unsigned(x0),unsigned(x0),unsigned(x1),unsigned(x1)},ys{unsigned(y0),unsigned(y1),unsigned(y0),unsigned(y1)};
    std::array<float,4> weights{M(ix,iy),M(ix,y),M(x,iy),M(x,y)};EnvironmentColour sum{};
    for(unsigned n=0;n<4;++n){auto at=ys[n]*(t.width+1)+xs[n];if(!t.valid.empty()&&!t.valid.at(at)&&weights[n]!=0)throw OriginalEnvironmentUnavailable("Original environment sample reads outside authored texture bytes");const auto&pixel=t.rgba[at];EnvironmentColour c{float(pixel[3]),float(pixel[0]),float(pixel[1]),float(pixel[2])};for(unsigned k=0;k<4;++k){float term=M(c[k],weights[n]);sum[k]=n?VAdd(sum[k],term):term;}}
    for(auto&value:sum)value=M(value,.003921568859368563f);return sum;
}
OriginalEnvironmentState originalEnvironmentFromSamples(EnvironmentColour light,EnvironmentColour base,EnvironmentColour multiplier){
    Round round;OriginalEnvironmentState result;float strength=A(light[0],light[0]);
    for(unsigned i=0;i<4;++i){float difference=S(base[i],light[i]);float scaled=M(difference,multiplier[i]);result.ambient[i]=M(scaled,strength);result.ratio[i]=originalScalarDivide(result.ambient[i],base[i]);}
    result.ambient[0]=result.ratio[0]=.5f;return result;
}
std::optional<OriginalEnvironmentState> originalEnvironmentPatchSample(const OriginalEnvironmentPatch&p,float u,float v,EnvironmentColour multiplier,const EnvironmentTextureSampler&sample){
    Round round;if(!p.eligible)return std::nullopt;EnvironmentColour light{},base{};
    for(unsigned slot=0;slot<3;++slot){unsigned kind=(p.flags>>(slot*3))&7;
        if(kind==5){float x=M(p.lightUV[2],std::clamp(u,0.f,1.f));float y=M(p.lightUV[3],std::clamp(v,0.f,1.f));light=sample(p.textures[slot],A(p.lightUV[0],x),A(p.lightUV[1],y));}
        else if(kind==1){
            // Original deliberately uses different intermediate interpolation
            // orders for U and V, then reduces each around positive1000.
            float a=A(p.baseUV[0][0],M(S(p.baseUV[2][0],p.baseUV[0][0]),u));
            float b=A(p.baseUV[1][0],M(S(p.baseUV[3][0],p.baseUV[1][0]),u));
            float c=A(p.baseUV[0][1],M(S(p.baseUV[1][1],p.baseUV[0][1]),v));
            float d=A(p.baseUV[2][1],M(S(p.baseUV[3][1],p.baseUV[2][1]),v));
            float x=A(A(a,M(S(b,a),v)),1000.f),y=A(A(c,M(S(d,c),u)),1000.f);x=S(x,float(int(x)));y=S(y,float(int(y)));base=sample(p.textures[slot],x,y);
        }
    }return originalEnvironmentFromSamples(light,base,multiplier);
}
void originalEnvironmentBlend(OriginalEnvironmentState&state,const OriginalEnvironmentState&target,float weight){
    Round round;float previous=S(1.f,weight);for(unsigned i=0;i<4;++i){float old=M(state.ambient[i],previous),incoming=M(target.ambient[i],weight);state.ambient[i]=A(old,incoming);old=M(state.ratio[i],previous);incoming=M(target.ratio[i],weight);state.ratio[i]=A(old,incoming);}
}
void originalEnvironmentGroundBlend(OriginalEnvironmentState&state,const OriginalEnvironmentState&target,bool force){originalEnvironmentBlend(state,target,force?1.f:.8999999761581421f);}
void originalEnvironmentUpdate(OriginalEnvironmentState&state,OriginalEnvironmentGlobals&globals,const OriginalEnvironmentFrame&frame,const EnvironmentTextureSampler&sample){
    Round round;
    if(frame.motionMode==1){OriginalEnvironmentState target{globals.airAmbient,globals.airRatio};float weight=.019999999552965164f;
        if((frame.predictionStatus==1||frame.predictionStatus==3)&&frame.predictedPatch){float remaining=S(frame.predictedTime,frame.elapsed);float window=std::min(std::max(M(frame.predictedTime,.5f),2.f),frame.predictedTime);
            if(window<remaining){float rest=S(frame.predictedTime,window);float value=S(rest,S(remaining,window));weight=std::clamp(originalScalarDivide(value,rest),0.f,.10000000149011612f);}
            else{if(auto sampled=originalEnvironmentPatchSample(*frame.predictedPatch,frame.predictedU,frame.predictedV,globals.multiplier,sample))target=*sampled;weight=window>0?std::clamp(originalScalarDivide(S(window,remaining),window),0.f,.10000000149011612f):1.f;}
        }originalEnvironmentBlend(state,target,weight);return;
    }
    if(!frame.groundPatch||!frame.groundPatch->eligible){globals.forceNext=true;originalEnvironmentGroundBlend(state,{{0,1,1,1},globals.airRatio},false);return;}
    if(frame.motionMode==4){globals.forceNext=true;return;}
    auto target=originalEnvironmentPatchSample(*frame.groundPatch,frame.u,frame.v,globals.multiplier,sample);originalEnvironmentGroundBlend(state,*target,globals.forceNext);globals.forceNext=false;
}
}
