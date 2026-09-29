#pragma once
#include "rider_irradiance.hpp"
#include "local_light_selection.hpp"
#include "spatial_light_query.hpp"
#include <unordered_map>
namespace ssx {
struct OriginalWorldLight {
 uint32_t resource=0;
 OriginalLightRankingInput ranking;
};
// Authored static light index and per-rider retained selection. Refresh belongs
// to120E50's world-query phase; shading belongs to1220D8's draw phase. Keeping
// these separate is essential: drawing must not re-rank a retained light list.
class OriginalRiderLightWorld {
 std::vector<OriginalSpatialLightNode> nodes_;
 std::array<OriginalSpatialLightRoot,8> roots_{};
 std::unordered_map<uint32_t,OriginalLightRankingInput> lights_;
public:
 struct Selection {std::array<uint32_t,8> ids{};std::vector<uint32_t> candidates;};
 OriginalRiderLightWorld(std::vector<OriginalSpatialLightNode> nodes,
  std::array<OriginalSpatialLightRoot,8> roots,std::span<const OriginalWorldLight> lights)
  :nodes_(std::move(nodes)),roots_(roots){
  for(const auto& light:lights)if(!light.resource||!lights_.emplace(light.resource,light.ranking).second)throw std::runtime_error("Invalid or duplicate light resource");
  for(const auto& node:nodes_){
   for(auto id:node.lights)if(!lights_.contains(id))throw std::runtime_error("Spatial index references unknown light");
   for(auto child:node.children)if(child < -1 || (child>=0&&size_t(child)>=nodes_.size()))throw std::runtime_error("Invalid light child");
  }
  for(const auto& root:roots_)if(root.node < -1 || (root.node>=0&&size_t(root.node)>=nodes_.size()))throw std::runtime_error("Invalid light root");
 }
 Selection refresh(const std::array<float,3>& minimum,const std::array<float,3>& maximum,const std::array<float,3>& point)const{
  Selection selection;selection.candidates=originalSpatialLightQuery(nodes_,roots_,minimum,maximum);
  std::vector<OriginalLocalLightCandidate> ranked;ranked.reserve(selection.candidates.size());
  for(auto id:selection.candidates)ranked.push_back({id,6,originalLocalLightRank(point,lights_.at(id))});
  selection.ids=originalSelectLocalLights(ranked);return selection;
 }
 OriginalIrradianceCoefficients shade(const Selection& selection,const OriginalIrradianceCoefficients& environment,
  const std::array<float,16>& view,const std::array<float,4>& point,float rimScale,
  const std::array<float,5>& rimConstants,const OriginalRiderExtraLighting* extra=nullptr)const{
  std::array<OriginalRiderLocalLight,8> storage;std::array<const OriginalRiderLocalLight*,8> selected{};
  for(unsigned i=0;i<8;++i)if(auto id=selection.ids[i]){const auto& light=lights_.at(id);storage[i]={light.kind,light.light};selected[i]=&storage[i];}
  return originalRiderIrradiance(environment,view,point,rimScale,rimConstants,selected,extra);
 }
};
}
