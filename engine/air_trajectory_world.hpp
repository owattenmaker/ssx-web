#pragma once
#include "air_trajectory.hpp"
#include "collision.hpp"
#include "world_body_collision.hpp"
#include "ray_instance_collision.hpp"

namespace ssx {
struct OriginalWorldSegmentHit : OriginalAirTrajectoryHit {
    uint32_t instance=0;unsigned node=0;
    bool hasInstance=false;
    terrain_original::Vector surfaceVelocityCmps{};
};
// Recovered mode0 terrain and authored static-instance ray paths. Dynamic
// callbacks remain explicit incomplete coverage when their bounds intersect.
// Mode2 (3378C0) intentionally omits terrain in the original game.
inline OriginalWorldSegmentHit queryOriginalWorldSegment(const CollisionWorld& ground,
        const WorldBodyCollision* world,terrain_original::Vector end,terrain_original::Vector start,int mode,
        float preferredFraction=1,bool refinedTerrain=false,terrain_original::ContactCache* cache=nullptr,
        const terrain_original::RiderScope* scope=nullptr) {
    terrain_original::Rounding rounding;OriginalWorldSegmentHit result;unsigned terrainCandidates=0;
    if(mode!=0&&mode!=2)throw std::runtime_error("Unknown original trajectory query mode");
    if(mode==0) {
        auto hit=ground.sourceTerrainSegment(end,start,preferredFraction,refinedTerrain,cache,scope);
        terrainCandidates=hit.candidateCount;
        if(hit.hit) {
            result.fraction=hit.fraction;result.position=hit.pointCm;result.normal=hit.normal;
            result.hasPatch=true;result.patchFlags=hit.flags|0x40;result.patchId=int(hit.resource);
            result.patchU=hit.u;result.patchV=hit.v;
            result.surface=hit.surface;
        }
    }
    if(!world){result.complete=false;result.unavailableCause=1;return result;}
    float best=result.fraction>=0?std::abs(originalScalarSubtract(result.fraction,preferredFraction)):INFINITY;
    uint32_t bestResource=result.hasPatch?uint32_t(result.patchId):0;
    bool have=result.fraction>=0;unsigned contacts=terrainCandidates;
    ray_instance::Ray original{end,terrain_original::difference(start,end)};
    for(const uint32_t at:world->collidableInstances()) { // type-0 instances are skipped below whatever their other fields
        const auto& instance=world->instances[at];
        if(!worldResident(instance.resource))continue; // streamed location not loaded (world_residency.hpp)
        if(instance.eventRuntimeFlags&&originalInstanceRayRoute(*instance.eventRuntimeFlags,true,mode)==OriginalInstanceBodyRoute::Skip)continue;
        if(instance.type==0)continue;
        // A rider scope keeps static (flag 0x20) instances by their inline box; entity-route
        // instances (0x40) are kept by their virtual +0x168 box, which is not modelled here.
        if(scope&&!(instance.eventRuntimeFlags&&originalInstanceRayRoute(*instance.eventRuntimeFlags,true,mode)==OriginalInstanceBodyRoute::Entity)&&!scope->admits(instance.low,instance.high))continue;
        bool overlaps=true;
        for(unsigned k=0;k<3;++k)overlaps&=instance.low[k]<std::max(end[k],start[k])&&std::min(end[k],start[k])<instance.high[k];
        if(!overlaps)continue;
        // 32E688 supplies zero ray contacts even for decoded sphere trees.
        if(instance.type==3&&instance.scale!=0&&(instance.rayAlwaysEmpty||((instance.flags&0x200000)&&!(instance.flags&0x40000000))))continue;
        if(!instance.unsupported.empty()){result.complete=false;if(!result.unavailableCause){result.unavailableCause=2;result.unavailableResource=instance.resource;}continue;}
        for(const auto& node:instance.nodes) {
            // 335B90 passes filter2 (surface != -1); 336D40 passes filter1
            // (surface == -1). This separates rideable and obstacle prediction.
            if((mode==0&&node.surface==-1)||(mode==2&&node.surface!=-1))continue;
            auto ray=ray_instance::transform(original,node.inverse,originalScalarDivide(1,instance.scale));
            auto offer=[&](const ray_instance::Hit& hit) {
                ++contacts;float metric=std::abs(originalScalarSubtract(hit.fraction,preferredFraction));
                if(have&&(metric>best||(metric==best&&!result.hasPatch&&instance.resource>=bestResource)))return;
                have=true;best=metric;bestResource=instance.resource;
                result.fraction=hit.fraction;result.position=collision_transform::toWorldPoint(hit.point,node.world,instance.scale);
                result.normal=collision_transform::apply(node.world,hit.normal,0);result.surface=node.surface;
                result.hasPatch=false;result.patchFlags=-1;result.patchId=-1;result.patchU=result.patchV=0;
                result.instance=instance.resource;result.node=node.index;
                result.hasInstance=true;
            };
            if(node.type==2) {
                auto hits=ray_instance::box(ray,node.low,node.high);
                for(unsigned i=0;i<hits.count;++i)offer(hits.hits[i]);
            } else if(node.triangles&&ray_instance::overlaps(ray,node.low,node.high)) {
                const auto& mesh=*node.triangles;
                for(unsigned i=0;i<mesh.indices.size()/3;++i) {
                    auto hit=ray_instance::triangle(ray,mesh.vertices[mesh.indices[i*3]],mesh.vertices[mesh.indices[i*3+1]],mesh.vertices[mesh.indices[i*3+2]],mesh.normals[i],node.doubleSided);
                    if(hit)offer(*hit);
                }
            }
        }
    }
    // Original traversal caps candidate arrays; streaming visitation order has
    // not yet been reconstructed for overloaded queries.
    if(contacts>=unsigned(mode==0?64:128)){result.complete=false;if(!result.unavailableCause)result.unavailableCause=3;}
    return result;
}
inline OriginalAirTrajectoryHit queryOriginalAirTrajectoryWorld(const CollisionWorld& ground,
        const WorldBodyCollision* world,terrain_original::Vector end,terrain_original::Vector start,int mode) {
    return queryOriginalWorldSegment(ground,world,end,start,mode);
}
}
