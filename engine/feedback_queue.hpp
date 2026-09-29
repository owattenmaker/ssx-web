#pragma once
#include <array>
#include <bit>
#include <cstdint>
namespace ssx {
//2B1458: ten32-byte records at owner+18. Field names remain neutral where
//the caller-dependent meaning has not been recovered.
struct OriginalFeedbackEntry {
 uint32_t active=0;int32_t ticks=0;uint32_t keyGroup=0,keyId=0,arg0=0,arg1=0;float scalar=0;uint32_t arg2=0;
};
using OriginalFeedbackQueue=std::array<OriginalFeedbackEntry,10>;
inline bool originalFeedbackEnqueue(OriginalFeedbackQueue&q,uint32_t group,uint32_t id,uint32_t arg0,uint32_t arg1,float scalar,uint32_t arg2,bool refreshDuplicate){
 int free=-1;
 for(unsigned i=0;i<q.size();i++){
  auto&e=q[i];if(!e.active){if(free<0)free=int(i);continue;}
  if(e.keyGroup==group&&e.keyId==id){if(!refreshDuplicate)return false;e.ticks=180;return true;}
 }
 if(free<0)return false;
 q[free]={1,180,group,id,arg0,arg1,scalar,arg2};return true;
}
}

namespace ssx {
//2B1720: active entries decrement even at zero/negative values, with EE
//32-bit wrapping. Expiry clears only active, retaining the payload/timer.
inline void originalFeedbackTick(OriginalFeedbackQueue&q){
 for(auto&e:q)if(e.active){e.ticks=std::bit_cast<int32_t>(uint32_t(e.ticks)-1u);if(e.ticks<=0)e.active=0;}
}
//2B1428: clearing the queue does not overwrite cached entry payloads.
inline void originalFeedbackClear(OriginalFeedbackQueue&q){for(auto&e:q)e.active=0;}
}
