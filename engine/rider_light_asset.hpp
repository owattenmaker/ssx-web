#pragma once
#include "rider_light_world.hpp"
namespace ssx {
// Json is caller-supplied so native and browser asset loaders share field mapping
// without introducing a JSON dependency into the lighting arithmetic.
template<class Json> OriginalRiderLightWorld originalRiderLightAsset(const Json& catalog,const Json& tree){
 if(catalog.at("version")!=1||tree.at("version")!=1||catalog.at("source_sha256")!=tree.at("source_sha256"))throw std::runtime_error("Mismatched light assets");
 std::vector<OriginalWorldLight> lights;
 for(const auto& row:catalog.at("lights")){
  OriginalWorldLight entry;entry.resource=row.at("resource").template get<uint32_t>();auto& r=entry.ranking;auto& l=r.light;
  r.kind=row.at("kind").template get<int>();r.brightness=row.at("brightness").template get<float>();l.intensity=row.at("intensity").template get<float>();
  l.geometry.radius=row.at("radius").template get<float>();l.geometry.position=row.at("position").template get<std::array<float,3>>();l.geometry.axis=row.at("axis").template get<std::array<float,3>>();l.color=row.at("color").template get<std::array<float,3>>();
  l.innerCosine=row.at("inner_cosine").template get<float>();l.outerCosine=row.at("outer_cosine").template get<float>();l.distanceMode=row.at("distance_mode").template get<int8_t>();l.angularMode=row.at("angular_mode").template get<int8_t>();lights.push_back(entry);
 }
 std::vector<OriginalSpatialLightNode> nodes;
 for(const auto& row:tree.at("nodes"))nodes.push_back({row.at("children").template get<std::array<int32_t,8>>(),row.at("lights").template get<std::vector<uint32_t>>()});
 if(tree.at("roots").size()!=8)throw std::runtime_error("Light root count");std::array<OriginalSpatialLightRoot,8> roots;
 for(unsigned i=0;i<8;++i){const auto& row=tree.at("roots")[i];roots[i].node=row.at("node").template get<int32_t>();roots[i].region.exponent=row.at("exponent").template get<int>();roots[i].region.cell=row.at("cell").template get<std::array<int32_t,3>>();}
 return OriginalRiderLightWorld(std::move(nodes),roots,lights);
}
}
