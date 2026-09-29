#pragma once
#include "pickup_instance_flags.hpp"
#include <cstdint>
#include <functional>
#include <optional>
namespace ssx {
//2FC2C0: fallback after an instance's event6 handler declines the request.
//Mode labels deliberately retain the source integers; these are not map layers.
struct OriginalInstanceStateAccess {
    std::function<void(int)> destroyEntity;
    std::function<void(int)> constructEntity;
};
inline void originalInstanceStateFallback(uint32_t& flags,std::optional<int16_t> entityType,
                                         int32_t mode,const OriginalInstanceStateAccess& access){
    if(mode!=0&&mode!=1&&mode!=3)return;
    if(entityType&&(*entityType==6||(mode==3&&*entityType==19)))return;
    if(entityType)access.destroyEntity(3);
    //Destruction can mutate the instance. Read flags after the callback, as2FC338 does.
    if(mode==1)flags=originalPickupRestoredFlags(flags);
    else access.constructEntity(mode==0?6:19);
}
}
namespace ssx {
enum class OriginalInstanceBodyRoute { Skip,Static,Entity };
//333EF8 collector,3340F4..3341A0: static eligibility takes precedence.
//Entity is a request for virtual160/168, not permission to use static geometry.
inline OriginalInstanceBodyRoute originalInstanceBodyRoute(uint32_t runtimeFlags,bool hasEntity){
    if(runtimeFlags&0x20u)return OriginalInstanceBodyRoute::Static;
    if((runtimeFlags&0x40u)&&hasEntity)return OriginalInstanceBodyRoute::Entity;
    return OriginalInstanceBodyRoute::Skip;
}
}
namespace ssx {
//335B90 (mode0) guards a missing entity;336D40 (mode2) assumes one exists.
//Entity with no owner in mode2 must be reported unavailable by the host.
inline OriginalInstanceBodyRoute originalInstanceRayRoute(uint32_t flags,bool hasEntity,int mode){
    if(flags&0x20u)return OriginalInstanceBodyRoute::Static;
    if((flags&0x40u)&&(mode==2||hasEntity))return OriginalInstanceBodyRoute::Entity;
    return OriginalInstanceBodyRoute::Skip;
}
}
