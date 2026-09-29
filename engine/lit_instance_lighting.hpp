#pragma once
// Lit static-model instances (runtime instance flag 0x4000 = authored descriptor flag 0x40000000): the light cache of 2F5400.
// 37E238 calls 2F5148 -> 2F5400(cache, instance, flag 0x1000) for such an instance. The cache (32 entries of 0x180 bytes) keeps per
// resource: +0x10 the ten rows the VU gets, +0xB0 the position they were made at, +0xC0 an RTLOverlapList, +0xD0 the environment bank.
// - A new entry, or an instance with runtime flag 0x1000 that moved more than 10 cm (2F5584: the instance's entity matrix via 34FED8,
//   else the instance matrix row 3), is relit: after 5 m (or new) the bank is re-read from the cache's painter wrapper stepped at the
//   position (vt+0x10 at x, y; vt+0x140 = Lighting reference 3, or vt+0x128 = reference 0 when that name is empty, or the course
//   default gp+0x12D4 when both are), and when the position leaves the overlap list's box, the box becomes the position +-1000 and
//   331450 queries the light tree for it (332DB8 / 33B748 / 340DC0: engine/spatial_light_query.hpp).
// - The rows are the bank plus up to 4 local lights: 2F5AF0 -> 2F5B68 with capacity 4 ranks the list's kind-6 nodes by 2F5D30 at
//   the position (engine/local_light_selection.hpp originalLocalLightRank) and keeps the 4 best, then 38A6A8 adds each (kind 1
//   spotlight, kind 2 point: engine/spotlight_irradiance.hpp, point_light_irradiance.hpp; other kinds add nothing).
// VU1 program 3 scales the rows by 128 (0x2170) and at 0x8B8 evaluates them per vertex on the node-rotated normal over the baked
// colours (web/world-material.js litWorldMaterial).
#include "local_light_selection.hpp"
#include "spatial_light_query.hpp"
#include "spotlight_irradiance.hpp"
#include "point_light_irradiance.hpp"
#include <span>
#include <unordered_map>
#include <vector>
namespace ssx {
// 2F5B68 with capacity N: kind-6 nodes only; a candidate enters when it outranks the last slot; new equal scores precede existing
// equals; ties at the cutoff are rejected (the rider's 8 slots: originalSelectLocalLights).
template<unsigned N> inline std::array<uint32_t,N> originalSelectLocalLightsN(std::span<const OriginalLocalLightCandidate> candidates){
 std::array<uint32_t,N> ids{};std::array<float,N> ranks{};unsigned count=0;
 for(const auto& c:candidates){
  if(c.nodeKind!=6||!(ranks[N-1]<c.rank))continue;
  unsigned at=0;while(at<count&&c.rank<ranks[at])++at;
  if(at>=N)continue;
  count=std::min(count+1,N);
  for(unsigned i=count-1;i>at;--i){ids[i]=ids[i-1];ranks[i]=ranks[i-1];}
  ids[at]=c.id;ranks[at]=c.rank;
 }
 return ids;
}
struct OriginalLitInstanceLight {uint32_t resource=0;OriginalLightRankingInput ranking;};
class OriginalLitInstanceLightWorld {
 std::vector<OriginalSpatialLightNode> nodes_;std::array<OriginalSpatialLightRoot,8> roots_{};
 std::unordered_map<uint32_t,OriginalLightRankingInput> lights_;
public:
 OriginalLitInstanceLightWorld(std::vector<OriginalSpatialLightNode> nodes,std::array<OriginalSpatialLightRoot,8> roots,std::span<const OriginalLitInstanceLight> lights)
  :nodes_(std::move(nodes)),roots_(roots){
  for(const auto& light:lights)if(!light.resource||!lights_.emplace(light.resource,light.ranking).second)throw std::runtime_error("Invalid or duplicate light resource");
  for(const auto& node:nodes_)for(auto id:node.lights)if(!lights_.contains(id))throw std::runtime_error("Spatial index references unknown light");
 }
 // 2F5AF0 over the box position +-1000: the rows and the chosen lights (0 = empty slot)
 OriginalIrradianceCoefficients rows(const OriginalIrradianceCoefficients& bank,const std::array<float,3>& position,std::array<uint32_t,4>* chosen=nullptr)const{
  terrain_original::Rounding rounding;
  std::array<float,3> low,high;for(unsigned i=0;i<3;++i){low[i]=originalScalarSubtract(position[i],1000.f);high[i]=originalScalarAdd(position[i],1000.f);}
  const auto candidates=originalSpatialLightQuery(nodes_,roots_,low,high);
  std::vector<OriginalLocalLightCandidate> ranked;ranked.reserve(candidates.size());
  for(auto id:candidates)ranked.push_back({id,6,originalLocalLightRank(position,lights_.at(id))});
  const auto ids=originalSelectLocalLightsN<4>(ranked);if(chosen)*chosen=ids;
  auto out=bank;
  for(auto id:ids){
   if(!id)continue;const auto& in=lights_.at(id);const auto& light=in.light;
   if(in.kind==1)originalSpotlightIrradiance(out,position,light);
   else if(in.kind==2)originalPointLightIrradiance(out,position,{light.geometry.radius,light.intensity,light.geometry.position,light.color,light.distanceMode});
  }
  return out;
 }
};
// The same catalog / tree assets as the rider light world (engine/rider_light_asset.hpp).
template<class Json> OriginalLitInstanceLightWorld originalLitInstanceLightAsset(const Json& catalog,const Json& tree){
 if(catalog.at("version")!=1||tree.at("version")!=1||catalog.at("source_sha256")!=tree.at("source_sha256"))throw std::runtime_error("Mismatched light assets");
 std::vector<OriginalLitInstanceLight> lights;
 for(const auto& row:catalog.at("lights")){
  OriginalLitInstanceLight entry;entry.resource=row.at("resource").template get<uint32_t>();auto& r=entry.ranking;auto& l=r.light;
  r.kind=row.at("kind").template get<int>();r.brightness=row.at("brightness").template get<float>();l.intensity=row.at("intensity").template get<float>();
  l.geometry.radius=row.at("radius").template get<float>();l.geometry.position=row.at("position").template get<std::array<float,3>>();l.geometry.axis=row.at("axis").template get<std::array<float,3>>();l.color=row.at("color").template get<std::array<float,3>>();
  l.innerCosine=row.at("inner_cosine").template get<float>();l.outerCosine=row.at("outer_cosine").template get<float>();l.distanceMode=row.at("distance_mode").template get<int8_t>();l.angularMode=row.at("angular_mode").template get<int8_t>();lights.push_back(entry);
 }
 std::vector<OriginalSpatialLightNode> nodes;
 for(const auto& row:tree.at("nodes"))nodes.push_back({row.at("children").template get<std::array<int32_t,8>>(),row.at("lights").template get<std::vector<uint32_t>>()});
 if(tree.at("roots").size()!=8)throw std::runtime_error("Light root count");std::array<OriginalSpatialLightRoot,8> roots;
 for(unsigned i=0;i<8;++i){const auto& row=tree.at("roots")[i];roots[i].node=row.at("node").template get<int32_t>();roots[i].region.exponent=row.at("exponent").template get<int>();roots[i].region.cell=row.at("cell").template get<std::array<int32_t,3>>();}
 return OriginalLitInstanceLightWorld(std::move(nodes),roots,lights);
}
}
