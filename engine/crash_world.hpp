#pragma once
#include "crash_contact.hpp"
#include "crash_ground.hpp"
namespace ssx {
struct OriginalCrashWorldUnavailable:std::runtime_error {using std::runtime_error::runtime_error;};
// Source query policies are part of crash behavior. Main sliding, main air,
// and detached-board queries deliberately use different detail/cache settings.
class OriginalCrashWorldQueries {
    const CollisionWorld& terrain;
    const WorldBodyCollision& world;
    std::function<int(int)> surfaceProperty44;
    // partialEntities: dynamic entities without callbacks (unsupported instances, air-trajectory cause 2) are left out of the
    // query instead of failing it (the browser's streamed worlds, which seed no scripted instances: web/crash_runtime.hpp).
    bool partialEntities=false;
    WorldBodyQuery requireComplete(WorldBodyQuery result)const {
        if(!result.complete()&&!(partialEntities&&!result.candidateLimitExceeded))throw OriginalCrashWorldUnavailable("Crash body query intersects unsupported native collision resources/callbacks");
        return result;
    }
    bool segmentUsable(const OriginalWorldSegmentHit& hit)const{return hit.complete||(partialEntities&&hit.unavailableCause==2);}
public:
    OriginalCrashWorldQueries(const CollisionWorld& terrainWorld,const WorldBodyCollision& bodyWorld,
                             std::function<int(int)> propertyLookup,bool partial=false)
        :terrain(terrainWorld),world(bodyWorld),surfaceProperty44(std::move(propertyLookup)),partialEntities(partial) {
        if(!surfaceProperty44)throw std::runtime_error("Crash surface property catalog is unavailable");
    }
    OriginalWorldSegmentHit terrainContact(const OriginalCrashContactProbe& probe,terrain_original::ContactCache* cache864)const {
        auto hit=originalCrashContact(terrain,&world,probe,cache864);
        if(!segmentUsable(hit))throw OriginalCrashWorldUnavailable("Crash terrain probe intersects unsupported native collision resources/callbacks");
        return hit;
    }
    OriginalCrashGroundHit motionHit(const OriginalWorldSegmentHit& hit)const {
        if(!segmentUsable(hit))throw OriginalCrashWorldUnavailable("Incomplete crash terrain hit");
        OriginalCrashGroundHit result;result.fraction=hit.fraction;if(hit.fraction<0)return result;
        result.point=hit.position;result.normal=hit.normal;result.surfaceVelocity=hit.surfaceVelocityCmps;result.surface=hit.surface;
        result.surfaceProperty44=surfaceProperty44(hit.surface);result.hasPatch=hit.hasPatch;result.patchFlags=hit.patchFlags;
        // Complete native world hits currently exclude unresolved entity hooks.
        return result;
    }
    WorldBodyQuery slidingBody(const BodyCollisionVolume& body,terrain_original::Vector groundNormal,
                               terrain_original::ContactCache* cache868)const {
        return requireComplete(world.query(body,groundNormal,nullptr,false,2,cache868)); //138640: always coarse.
    }
    WorldBodyQuery airborneBody(const BodyCollisionVolume& body,bool human,
                                terrain_original::ContactCache* cache868)const {
        return requireComplete(world.query(body,{},nullptr,human,2,cache868)); //137860: no normal filter.
    }
    WorldBodyQuery detachedBody(const BodyCollisionVolume& body)const {
        return requireComplete(world.query(body,{},nullptr,false,2,nullptr)); //137138:336850, no rider cache.
    }
};
}
