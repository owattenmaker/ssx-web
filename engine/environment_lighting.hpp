#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>
#include <stdexcept>
namespace ssx {
struct OriginalEnvironmentUnavailable:std::runtime_error {using std::runtime_error::runtime_error;};
using EnvironmentColour=std::array<float,4>; //Original A,R,G,B, all channels /255 at texture sampling.
struct OriginalEnvironmentTexture {
    unsigned width=0,height=0;
    // Inclusive guard row/column reproduce3889F0's source index clamp to width
    // and height. Exporter supplies actual CPU-fetch texels, not invented edges.
    std::vector<std::array<uint8_t,4>> rgba;
    std::vector<uint8_t> valid; //Optional authored-fetch availability, including guard cells.
};
EnvironmentColour originalEnvironmentTextureSample(const OriginalEnvironmentTexture&,float u,float v); //3889F0
struct OriginalEnvironmentPatch {
    uint32_t resource=0,flags=0;
    bool eligible=true;
    std::array<float,4> lightUV{}; //minU,minV,scaleU,scaleV
    std::array<std::array<float,2>,4> baseUV{};
    std::array<int,3> textures{-1,-1,-1};
};
struct OriginalEnvironmentState {EnvironmentColour ambient{.5f,1,1,1},ratio{.5f,1,1,1};};
struct OriginalEnvironmentGlobals {
    EnvironmentColour multiplier{1,.550000011920929f,.5350000262260437f,.550000011920929f}; //4FAB00
    EnvironmentColour airAmbient{1,1,1,1},airRatio{1,1,1,1}; //4FAB20/40
    bool forceNext=false; //gp+A68 is shared across rider updates, not per rider.
};
using EnvironmentTextureSampler=std::function<EnvironmentColour(int,float,float)>;
OriginalEnvironmentState originalEnvironmentFromSamples(EnvironmentColour light,EnvironmentColour base,EnvironmentColour multiplier);
std::optional<OriginalEnvironmentState> originalEnvironmentPatchSample(const OriginalEnvironmentPatch&,float u,float v,EnvironmentColour multiplier,const EnvironmentTextureSampler&); //2EDB20
void originalEnvironmentBlend(OriginalEnvironmentState&,const OriginalEnvironmentState&,float incomingWeight); //2ED338
void originalEnvironmentGroundBlend(OriginalEnvironmentState&,const OriginalEnvironmentState&,bool force); //2ED1D0
struct OriginalEnvironmentFrame {
    int motionMode=0,predictionStatus=0;
    const OriginalEnvironmentPatch* groundPatch=nullptr;
    const OriginalEnvironmentPatch* predictedPatch=nullptr;
    float u=0,v=0,predictedU=0,predictedV=0,predictedTime=0,elapsed=0;
};
void originalEnvironmentUpdate(OriginalEnvironmentState&,OriginalEnvironmentGlobals&,const OriginalEnvironmentFrame&,const EnvironmentTextureSampler&); //2ED490 colour branches
}
