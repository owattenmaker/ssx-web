#pragma once
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <array>
#include <algorithm>
#include <cmath>
#include <span>
#include <vector>
#include <stdexcept>
namespace ssx {
struct OriginalPickupContactCandidate {
 uint32_t instance=0;
 std::array<float,3> point{},normal{};
 float distance=0;
 bool priority=false; //instance collision-node flags bit0 and scalar !=0
};
struct OriginalPickupContactSelection {int selected=-1;std::vector<uint32_t> instances;};
//104E70 instance-contact selection only; normal aggregation and selected entity
//surface-motion callback belong to the caller, not this function.
inline OriginalPickupContactSelection originalPickupContactSelection(std::span<const OriginalPickupContactCandidate> contacts,std::array<float,3> center,float queryRadius){
 OriginalRounding rounding;OriginalPickupContactSelection out;if(contacts.empty())return out;
 if(contacts.size()>64)throw std::runtime_error("Original pickup contact capacity exceeded");
 auto facing=[&](const auto&c){auto delta=terrain_original::difference(c.point,center);float dot=terrain_original::dot(delta,c.normal);dot=terrain_original::add(dot,0.f);return dot<0;};
 auto metric=[&](const auto&c){return std::abs(originalScalarSubtract(c.distance,queryRadius));};
 out.selected=0;bool priority=contacts[0].priority,front=facing(contacts[0]);float nearest=metric(contacts[0]);
 for(unsigned i=0;i<contacts.size();i++){
  const auto&c=contacts[i];if(std::find(out.instances.begin(),out.instances.end(),c.instance)==out.instances.end())out.instances.push_back(c.instance);
  if(!i)continue;float d=metric(c);
  if(c.priority&&!priority){out.selected=i;nearest=d;priority=true;continue;}
  if(priority&&!c.priority)continue;
  bool side=facing(c);
  if(side&&!front){out.selected=i;nearest=d;front=true;continue;}
  if(front&&!side)continue;
  if(d<nearest){out.selected=i;nearest=d;}
  else if(d==nearest&&c.instance<contacts[out.selected].instance)out.selected=i;
 }
 return out;
}
}
