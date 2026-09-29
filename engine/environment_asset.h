#pragma once
#import <Foundation/Foundation.h>
#include "environment_lighting.hpp"
#include <memory>
#include <unordered_map>
namespace ssx {
struct OriginalEnvironmentAsset {
    OriginalEnvironmentGlobals globals;
    std::unordered_map<uint32_t,OriginalEnvironmentPatch> patches;
    std::vector<OriginalEnvironmentTexture> textures;
    const OriginalEnvironmentPatch* patch(int64_t resource)const {
        auto it=patches.find(uint32_t(resource));return it==patches.end()?nullptr:&it->second;
    }
    void update(OriginalEnvironmentState& state,const OriginalEnvironmentFrame& frame) {
        originalEnvironmentUpdate(state,globals,frame,[this](int id,float u,float v){return originalEnvironmentTextureSample(textures.at(id),u,v);});
    }
};
// Returns nullptr when this course has no CPU lighting package. Malformed
// packages fail explicitly. No guest memory or original executable is loaded.
std::unique_ptr<OriginalEnvironmentAsset> loadOriginalEnvironmentAsset(NSString* courseFolder);
}
