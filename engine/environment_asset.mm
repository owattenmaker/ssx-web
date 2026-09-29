#include "environment_asset.h"
#include <stdexcept>
namespace ssx {namespace {
template<size_t N>std::array<float,N> vec(id value){if(![value isKindOfClass:NSArray.class]||[value count]!=N)throw std::runtime_error("Invalid environment lighting vector");std::array<float,N> out;for(unsigned k=0;k<N;++k)out[k]=[value[k] floatValue];return out;}
}
std::unique_ptr<OriginalEnvironmentAsset> loadOriginalEnvironmentAsset(NSString* folder){
 NSData* data=[NSData dataWithContentsOfFile:[folder stringByAppendingPathComponent:@"environment-lighting.json"]];if(!data)return nullptr;
 NSDictionary* json=[NSJSONSerialization JSONObjectWithData:data options:0 error:nil];if(![json isKindOfClass:NSDictionary.class]||[json[@"version"]intValue]!=1)throw std::runtime_error("Invalid environment lighting asset");
 auto out=std::make_unique<OriginalEnvironmentAsset>();out->globals.multiplier=vec<4>(json[@"multiplier"]);out->globals.airAmbient=vec<4>(json[@"air_ambient"]);out->globals.airRatio=vec<4>(json[@"air_ratio"]);
 for(NSDictionary* d in json[@"textures"]){unsigned index=[d[@"id"]unsignedIntValue];if(index!=out->textures.size())throw std::runtime_error("Noncontiguous environment textures");OriginalEnvironmentTexture t;t.width=[d[@"width"]unsignedIntValue];t.height=[d[@"height"]unsignedIntValue];size_t cells=(size_t(t.width)+1)*(size_t(t.height)+1);NSData* pixels=[NSData dataWithContentsOfFile:[folder stringByAppendingPathComponent:d[@"rgba"]]];NSData* valid=[NSData dataWithContentsOfFile:[folder stringByAppendingPathComponent:d[@"valid"]]];if(!t.width||!t.height||pixels.length!=cells*4||valid.length!=cells)throw std::runtime_error("Invalid environment texture lattice");t.rgba.resize(cells);memcpy(t.rgba.data(),pixels.bytes,cells*4);t.valid.resize(cells);memcpy(t.valid.data(),valid.bytes,cells);out->textures.push_back(std::move(t));}
 for(NSDictionary* d in json[@"patches"]){OriginalEnvironmentPatch p;p.resource=[d[@"resource"]unsignedIntValue];p.flags=[d[@"flags"]unsignedIntValue];p.eligible=[d[@"eligible"]boolValue];p.lightUV=vec<4>(d[@"light_uv"]);if([d[@"base_uv"]count]!=4||[d[@"textures"]count]!=3)throw std::runtime_error("Invalid environment patch");for(unsigned k=0;k<4;++k)p.baseUV[k]=vec<2>(d[@"base_uv"][k]);for(unsigned k=0;k<3;++k){p.textures[k]=[d[@"textures"][k]intValue];if(p.textures[k]>=int(out->textures.size()))throw std::runtime_error("Invalid environment texture reference");}if(!out->patches.emplace(p.resource,p).second)throw std::runtime_error("Duplicate environment patch");}
 return out;
}
}
