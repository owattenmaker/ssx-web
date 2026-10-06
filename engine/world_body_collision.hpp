#pragma once
#include "collision_transform.hpp"
#include "sphere_tree_collision.hpp"
#include "original_spatial.hpp"
#include "instance_state.hpp"
#include "world_residency.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstring>
#include <algorithm>

namespace ssx {
struct CollisionTriangleMesh {std::vector<terrain_original::Vector> vertices,normals;std::vector<unsigned> indices;};
struct WorldCollisionNode {
    unsigned index=0,type=0;int surface=-1;
    uint16_t collisionFlags=0;uint32_t collisionValue=0,collisionAuxiliary=0; //original descriptor node+8/+0/+4
    collision_transform::Matrix world=collision_transform::identity,inverse=collision_transform::identity;
    terrain_original::Vector low{},high{};bool doubleSided=false;
    std::shared_ptr<const CollisionTriangleMesh> triangles;
    std::shared_ptr<const CollisionSphereTree> sphereTree;
    uint32_t sphereTreeResource=0;
};
// Original entity route (runtime flag 0x40 with an entity at instance+0xC and no
// vtable+0x134 override): 334458 tests the entity's bounds (vtable+0x164/+0x16C)
// and 334888 composes the model hierarchy on the entity matrix (vtable+0xCC)
// instead of the authored instance matrix (instance+0x10).
struct WorldCollisionEntity {
    terrain_original::Vector low{},high{};
    std::vector<collision_transform::Matrix> world,inverse; // parallel to WorldCollisionInstance::nodes
};
struct WorldCollisionInstance {
    uint32_t resource=0;unsigned type=0,flags=0;float scale=1;
    terrain_original::Vector low{},high{};
    std::vector<WorldCollisionNode> nodes;
    std::string unsupported;
    std::optional<uint32_t> eventRuntimeFlags; //Verified event snapshot override; absent retains existing world capability.
    bool rayAlwaysEmpty=false; // Verified scripted type3 no-override capability; body support is separate.
    std::shared_ptr<const WorldCollisionEntity> entity; // instance+0xC entity route geometry (nullptr: none)
    bool runtimeClone=false;  // created at run time (0x351170 clone of an authored instance), not a package instance
    bool entityPointer=false; // instance+0xC != 0 on a static-route instance (set-piece clones): entity callbacks still run
    // Model hierarchy (local matrices, parent or 0xffffffff) retained for entity re-composition.
    std::vector<collision_transform::Matrix> hierarchyLocal;std::vector<uint32_t> hierarchyParent;
    collision_transform::Matrix authoredMatrix=collision_transform::identity; // instance+0x10 (retained with the hierarchy)
    // Entity vt+0x134 override (334888 hands the query to vt+0x13C): the MagnetModifier (0x357538, engine/magnet_modifier.hpp)
    // answers filter 1 only, with the body box test 0x32FAC0 -> 0x32B2B8 against its bounds (world axes, node 0).
    std::optional<std::array<terrain_original::Vector,2>> answerBox;
    // A streamed location's eviction (WorldBodyCollision::releaseTrack, pv peakRelease): the slot stays (set pieces hold pointers,
    // the hierarchy stays for entity composition) without its collision nodes; a re-read fills it again, inserted at the head of its
    // octree cell (328C20) like a later load: reinsertion orders it before the instances of its cell (collidableInstances).
    bool released=false;uint32_t reinsertion=0;
};
// 334888 node composition on an entity matrix (same scaledNode chain as the static load).
inline WorldCollisionEntity composeWorldCollisionEntity(const WorldCollisionInstance& instance,const collision_transform::Matrix& base,
        terrain_original::Vector low,terrain_original::Vector high) {
    WorldCollisionEntity entity;entity.low=low;entity.high=high;
    const size_t count=instance.hierarchyLocal.size();
    if(count==0||instance.hierarchyParent.size()!=count)throw std::runtime_error("Entity collision hierarchy unavailable");
    std::vector<collision_transform::Matrix> worlds(count);std::vector<uint8_t> state(count);
    // The same memoised recursion as a plain recursive lambda (a std::function per call allocated: set pieces place their
    // entities every tick in every rider context; docs/sim-performance.md "Chair entities").
    auto compose=[&](auto& self,uint32_t i)->collision_transform::Matrix {
        if(i>=count||state[i]==1)throw std::runtime_error("Invalid entity collision hierarchy");if(state[i]==2)return worlds[i];state[i]=1;
        uint32_t parent=instance.hierarchyParent[i];auto parentWorld=parent==0xffffffffu?base:self(self,parent);
        worlds[i]=collision_transform::scaledNode(instance.hierarchyLocal[i],parentWorld,instance.scale);state[i]=2;return worlds[i];
    };
    entity.world.reserve(instance.nodes.size());entity.inverse.reserve(instance.nodes.size());
    for(const auto& node:instance.nodes){auto w=compose(compose,node.index);entity.world.push_back(w);entity.inverse.push_back(collision_transform::inverseRigid(w));}
    return entity;
}
struct WorldCollisionTerrain {
    uint32_t resource=0;unsigned flags=0;int surface=0;terrain_original::Vector low{},high{};
    std::array<terrain_original::Vector,100> grid;
    OriginalSpatialCell spatial;uint32_t insertionOrder=0;
};
struct WorldBodyHit {
    bool hit=false;float penetrationCm=0;
    terrain_original::Vector pointCm{},normal{},surfaceVelocityCmps{};
    uint32_t instance=0;unsigned node=0,triangle=0;int surface=0;bool terrain=false;bool priority=false;
    uint16_t nodeFlags=0;uint32_t nodeValue=0,nodeAuxiliary=0,instanceFlags=0; //descriptor node/instance+8 for 105398
    bool entity=false; //instance+0xC != 0 (entity route)
};
struct WorldBodyQuery {
    WorldBodyHit best;
    std::vector<WorldBodyHit> instanceContacts; //filter1 raw narrow-phase contacts
    std::vector<uint32_t> unsupportedInstances;
    size_t candidates=0,contacts=0;
    bool candidateLimitExceeded=false;
    bool complete()const{return unsupportedInstances.empty()&&!candidateLimitExceeded;}
};
class WorldBodyCollision {
    struct TreeCacheEntry {uint32_t resource=0;bool valid=false;std::vector<uint8_t> masks;};
    mutable std::array<TreeCacheEntry,10> shallowTrees;
    mutable std::array<TreeCacheEntry,2> deepTrees;
    mutable unsigned nextShallowTree=0,nextDeepTree=0;
    const std::vector<uint8_t>& treeMasks(const WorldCollisionNode& node) const {
        if(!node.sphereTree->compressed)return node.sphereTree->masks;
        // 32DF28 keys the decompression cache by resource alone. Distinct model
        // nodes deliberately alias the first decoded mask stream for that RID.
        auto select=[&](auto& pool,unsigned& next)->const std::vector<uint8_t>& {
            for(auto& entry:pool)if(entry.valid&&entry.resource==node.sphereTreeResource)return entry.masks;
            auto& entry=pool[next];next=(next+1)%pool.size();entry={node.sphereTreeResource,true,node.sphereTree->masks};return entry.masks;
        };
        return node.sphereTree->levels.size()-1<5?select(shallowTrees,nextShallowTree):select(deepTrees,nextDeepTree);
    }
public:
    void resetSphereTreeCache() const {shallowTrees={};deepTrees={};nextShallowTree=nextDeepTree=0;}
    // 32DF28 for a tree outside the instance list (roller collider, roller_world_query.hpp):
    // the same shared pools, keyed by resource.
    const std::vector<uint8_t>& sphereTreeMasks(const CollisionSphereTree& tree,uint32_t resource) const {
        WorldCollisionNode node;node.sphereTree=std::shared_ptr<const CollisionSphereTree>(std::shared_ptr<const CollisionSphereTree>{},&tree);node.sphereTreeResource=resource;
        return treeMasks(node);
    }
    std::vector<WorldCollisionInstance> instances;
    std::vector<WorldCollisionTerrain> terrain;
    // The terrain loop of query() rejects almost every patch on its flags, resource and bounds, but a patch is ~1.3 KB
    // (its 100-point grid): scanning the list touches a new cache line per patch, in every rider context's copy. A
    // compact copy of those fields is scanned instead; the patches that pass are then handled on the list, in list
    // order, exactly as before. Patches are never changed in place (the list is rebuilt, replaced or appended to), so
    // the copy is rebuilt whenever the list's buffer or size, or one of eight sampled entries, differs.
    struct TerrainScanEntry {terrain_original::Vector low{},high{};uint32_t resource=0;unsigned flags=0;};
    mutable std::vector<TerrainScanEntry> terrainScan;mutable const WorldCollisionTerrain* terrainScanData=nullptr;
    static bool terrainScanSame(const TerrainScanEntry& e,const WorldCollisionTerrain& p){return e.resource==p.resource&&e.flags==p.flags&&std::memcmp(e.low.data(),p.low.data(),sizeof e.low)==0&&std::memcmp(e.high.data(),p.high.data(),sizeof e.high)==0;}
    // Instances with collision (type != 0): the instance loops (query(), queryOriginalWorldSegment) skip a type-0 instance
    // (no collision, kept for drawing; about a third of the list) whatever its other fields, so they walk this index
    // instead. Instance types never change after the load and the list only grows (clones are appended); the index is
    // rebuilt whenever the list's buffer or size, or one of eight sampled entries, differs.
    // The index is in the original's octree order, as the terrain traversal: 328660 / 328C20 insert every instance at the
    // head of its cell's list +0x20 (the cell of its box +0x60/+0x6C), and the collectors (332DB8's scope lists via 3309D8,
    // 334458) walk a node's own list before children 0..7. The order decides 104E70's normal sum over several contacts
    // (PS2 allpeak/apr-start 785: rock_1010 in the parent cell comes before icechunk_00019's five triangles; the list
    // order summed them the other way and the normal left the original by 1 ulp) and which of two equal contacts wins.
    mutable std::vector<uint32_t> collidable;mutable const WorldCollisionInstance* collidableData=nullptr;mutable size_t collidableSize=0;
    uint32_t collidableGeneration=0;mutable uint32_t collidableBuilt=~0u; // bumped when instances are released / re-read in place
    mutable std::array<std::pair<uint32_t,unsigned>,8> collidableSample{};
    const std::vector<uint32_t>& collidableInstances() const {
        bool fresh=collidableData==instances.data()&&collidableSize==instances.size()&&collidableBuilt==collidableGeneration;
        for(size_t s=0;fresh&&s<8&&!instances.empty();++s){const auto& i=instances[s*(instances.size()-1)/7];fresh=collidableSample[s]==std::pair<uint32_t,unsigned>{i.resource,i.type};}
        if(!fresh){
            collidable.clear();for(size_t i=0;i<instances.size();++i)if(instances[i].type!=0)collidable.push_back(uint32_t(i));
            {std::vector<OriginalSpatialCell> cells(instances.size());for(uint32_t i:collidable)cells[i]=originalSpatialCell(instances[i].low,instances[i].high);
             std::stable_sort(collidable.begin(),collidable.end(),[&](uint32_t a,uint32_t b){ // later insertions first within a cell
                 if(originalSpatialBefore(cells[a],cells[b]))return true;if(originalSpatialBefore(cells[b],cells[a]))return false;
                 const uint64_t ka=instances[a].reinsertion?(uint64_t(1)<<32)+instances[a].reinsertion:a,kb=instances[b].reinsertion?(uint64_t(1)<<32)+instances[b].reinsertion:b;return ka>kb;});}
            collidableData=instances.data();collidableSize=instances.size();collidableBuilt=collidableGeneration;
            for(size_t s=0;s<8&&!instances.empty();++s){const auto& i=instances[s*(instances.size()-1)/7];collidableSample[s]={i.resource,i.type};}
        }
        return collidable;
    }
    const std::vector<TerrainScanEntry>& terrainScanList() const {
        bool fresh=terrainScanData==terrain.data()&&terrainScan.size()==terrain.size();
        for(size_t s=0;fresh&&s<8&&!terrain.empty();++s){const size_t i=s*(terrain.size()-1)/7;fresh=terrainScanSame(terrainScan[i],terrain[i]);}
        if(!fresh){terrainScan.resize(terrain.size());for(size_t i=0;i<terrain.size();++i){const auto& p=terrain[i];terrainScan[i]={p.low,p.high,p.resource,p.flags};}terrainScanData=terrain.data();}
        return terrainScan;
    }
    void prepareTerrainTraversal() {
        for(uint32_t i=0;i<terrain.size();++i){terrain[i].spatial=originalSpatialCell(terrain[i].low,terrain[i].high);terrain[i].insertionOrder=i;}
        std::sort(terrain.begin(),terrain.end(),[](const auto& a,const auto& b) {
            if(originalSpatialBefore(a.spatial,b.spatial))return true;
            if(originalSpatialBefore(b.spatial,a.spatial))return false;
            return a.insertionOrder>b.insertionOrder; //328660 inserts at list head.
        });
    }
    // A streamed location's patches join the loaded ones (web/peak_world.inc): they keep their load order after the
    // earlier locations', so the 328660 head insertion tie order holds across locations.
    // Staged in slices while the location loads (its track is not resident yet, so no query needs it), then merged
    // into the traversal order once (commitTerrainTraversal) with an index sort and a linear merge.
    std::vector<WorldCollisionTerrain> pendingTerrain;uint32_t nextInsertion=0;uint32_t nextReinsertion=0;
    // pv peakRelease (web/peak_world.inc release at T+7): a location's patches leave the traversal (3284B8; the others keep their
    // order, a re-read appends them as the newest), its instances keep their slots without nodes. Returns the patches removed.
    size_t releaseTrack(uint32_t track){
        auto of=[track](const WorldCollisionTerrain& p){return (p.resource&255u)==track;};
        const size_t before=terrain.size();terrain.erase(std::remove_if(terrain.begin(),terrain.end(),of),terrain.end());
        pendingTerrain.erase(std::remove_if(pendingTerrain.begin(),pendingTerrain.end(),of),pendingTerrain.end());
        for(auto& i:instances)if((i.resource&255u)==track&&!i.runtimeClone){std::vector<WorldCollisionNode>().swap(i.nodes);i.released=true;}
        ++collidableGeneration;return before-terrain.size();
    }
    // A re-read instance of a released slot (web/world_bridge.cpp): the newest insertion of its cell.
    void reinsertInstance(WorldCollisionInstance& slot,WorldCollisionInstance&& fresh){fresh.reinsertion=++nextReinsertion;fresh.released=false;slot=std::move(fresh);++collidableGeneration;}
    void appendTerrainTraversal(std::vector<WorldCollisionTerrain> more) {
        if(nextInsertion<terrain.size()+pendingTerrain.size())nextInsertion=uint32_t(terrain.size()+pendingTerrain.size());
        for(auto& patch:more){patch.spatial=originalSpatialCell(patch.low,patch.high);patch.insertionOrder=nextInsertion++;pendingTerrain.push_back(std::move(patch));}
    }
    void commitTerrainTraversal() {
        if(pendingTerrain.empty())return;
        auto before=[](const WorldCollisionTerrain& a,const WorldCollisionTerrain& b) {
            if(originalSpatialBefore(a.spatial,b.spatial))return true;
            if(originalSpatialBefore(b.spatial,a.spatial))return false;
            return a.insertionOrder>b.insertionOrder; //328660 inserts at list head.
        };
        std::vector<uint32_t> order(pendingTerrain.size());for(uint32_t i=0;i<order.size();++i)order[i]=i;
        std::sort(order.begin(),order.end(),[&](uint32_t a,uint32_t b){return before(pendingTerrain[a],pendingTerrain[b]);});
        // Merged in place from the back (the same order as a forward merge that takes the loaded patch unless the new one sorts
        // before it), in a list that grows geometrically: a new list per location fragmented the heap of a streamed world.
        const size_t n=terrain.size(),m=order.size();
        if(terrain.capacity()<n+m)terrain.reserve(std::max(n+m,terrain.capacity()+terrain.capacity()/2));
        terrain.resize(n+m);
        for(size_t k=n+m,i=n,j=m;j>0;) {
            if(i==0||!before(pendingTerrain[order[j-1]],terrain[i-1]))terrain[--k]=std::move(pendingTerrain[order[--j]]);
            else terrain[--k]=std::move(terrain[--i]);
        }
        pendingTerrain.clear();pendingTerrain.shrink_to_fit();
    }
    WorldBodyQuery query(const BodyCollisionVolume& body,terrain_original::Vector groundNormal,
                        const std::vector<uint32_t>* onlyInstances=nullptr,bool humanDetailed=true,unsigned surfaceFilter=2,terrain_original::ContactCache* terrainCache=nullptr,
                        const terrain_original::RiderScope* scope=nullptr) const {
        using namespace terrain_original;Rounding rounding;WorldBodyQuery result;
        Vector low=body.broadCenterCm,high=body.broadCenterCm;
        if(body.activeMask) {
            low={INFINITY,INFINITY,INFINITY};high={-INFINITY,-INFINITY,-INFINITY};
            for(unsigned i=0;i<body.count;++i)if(body.activeMask&(1u<<i))for(unsigned k=0;k<3;++k) {
                low[k]=std::min(low[k],sub(body.spheres[i].centerCm[k],body.spheres[i].radiusCm));
                high[k]=std::max(high[k],add(body.spheres[i].centerCm[k],body.spheres[i].radiusCm));
            }
        } else for(unsigned k=0;k<3;++k){low[k]=sub(low[k],body.broadRadiusCm);high[k]=add(high[k],body.broadRadiusCm);}
        float preferred=humanDetailed?originalScalarAdd(body.broadRadiusCm,body.broadRadiusCm):-1.f,best=INFINITY;
        auto offer=[&](WorldBodyHit hit) {
            if(surfaceFilter==1&&!hit.terrain)result.instanceContacts.push_back(hit);
            float metric=std::abs(originalScalarSubtract(hit.penetrationCm,preferred));++result.contacts;
            bool wins=!result.best.hit||metric<best;
            if(result.best.hit&&metric==best) {
                if(hit.terrain!=result.best.terrain)wins=!hit.terrain;
                else wins=hit.instance<result.best.instance;
            }
            if(wins){best=metric;result.best=hit;}
        };
        const auto& scan=surfaceFilter==1?terrainScan:terrainScanList(); //334458 (filter 1) scans instances, not terrain patches.
        for(size_t index=0;surfaceFilter!=1&&index<scan.size();++index) {
            const auto& entry=scan[index];
            if(!(entry.flags&1)||!worldResident(entry.resource))continue;
            bool overlaps=true;for(unsigned k=0;k<3;++k)overlaps&=entry.low[k]<high[k]&&low[k]<entry.high[k];
            if(!overlaps)continue;
            // 3342D0 walks the rider's scope list (rider+0x860, 332DB8) for terrain too: an unlisted patch is not touched.
            if(scope&&!scope->admits(entry.low,entry.high))continue;
            const auto& patch=terrain[index];
            ++result.candidates;bool found=false;
            unsigned cells=humanDetailed?9:3,stride=humanDetailed?1:3;
            auto test=[&](unsigned u,unsigned v,unsigned half) {
                unsigned row=u*stride,col=v*stride;
                auto a=patch.grid[row*10+col],b=patch.grid[(row+stride)*10+col],c=patch.grid[row*10+col+stride],d=patch.grid[(row+stride)*10+col+stride];
                bool cell=true;
                for(unsigned k=0;k<3;++k)cell&=std::min({a[k],b[k],c[k],d[k]})<high[k]&&low[k]<std::max({a[k],b[k],c[k],d[k]});
                if(!cell)return false;
                auto p=half?c:d,q=b,r=half?a:c;
                auto normal=cross(difference(p,q),difference(q,r));float length=terrain_original::sqrt(dot(normal,normal));
                float reciprocal=length>0?div(1,length):INFINITY;if(reciprocal>10)return false;
                for(auto& x:normal)x=mul(x,reciprocal);
                if(dot(normal,groundNormal)>.800000011920929f)return false;
                auto contact=originalBodyTriangleContact(body,p,q,r,normal);if(!contact.hit)return false;
                float depth=terrain_original::sqrt(dot(contact.translationCm,contact.translationCm));reciprocal=depth>0?div(1,depth):std::numeric_limits<float>::max();
                normal=contact.translationCm;for(auto& x:normal)x=mul(x,reciprocal);
                offer({true,depth,contact.pointCm,normal,{},patch.resource,u*9+v,half,patch.surface,true});
                if(terrainCache)*terrainCache={true,patch.resource,u,v,half?0u:1u};
                return true;
            };
            if(terrainCache&&terrainCache->valid&&terrainCache->resource==patch.resource) {
                if(terrainCache->cellU<cells&&terrainCache->cellV<cells)found=test(terrainCache->cellU,terrainCache->cellV,terrainCache->half?0:1);
                if(!found)terrainCache->valid=false;
            }
            for(unsigned u=0;u<cells&&!found;++u)for(unsigned v=0;v<cells&&!found;++v)
                for(unsigned half=0;half<2&&!found;++half)found=test(u,v,half);
        }

        for(const uint32_t at:collidableInstances()) {
            const auto& instance=instances[at];
            const auto route=instance.eventRuntimeFlags?originalInstanceBodyRoute(*instance.eventRuntimeFlags,true):OriginalInstanceBodyRoute::Static;
            if(route==OriginalInstanceBodyRoute::Skip||!worldResident(instance.resource))continue;
            if(onlyInstances&&std::find(onlyInstances->begin(),onlyInstances->end(),instance.resource)==onlyInstances->end())continue;
            if(instance.type==0)continue;
            // 334458 walks the rider's query scope list (rider+0x860, 332DB8 every third tick), not the whole world: a
            // static (flag 0x20) instance is in it when its inline box +0x60/+0x6C meets the scope bounds. Entity-route
            // instances are kept by their virtual +0x168 box, which is not modelled (as queryOriginalWorldSegment).
            if(scope&&route!=OriginalInstanceBodyRoute::Entity&&!scope->admits(instance.low,instance.high))continue;
            if(route==OriginalInstanceBodyRoute::Entity&&instance.answerBox) { // 334458 entity bounds, then the override
                const auto& box=*instance.answerBox;bool overlaps=true;for(unsigned k=0;k<3;++k)overlaps&=box[0][k]<high[k]&&low[k]<box[1][k];
                if(!overlaps)continue;
                ++result.candidates;if(!(surfaceFilter&1u))continue;
                const auto contact=originalBodyBoxContact(body,box[0],box[1]);if(!contact.hit)continue;
                const float depth=terrain_original::sqrt(dot(contact.translationCm,contact.translationCm));const float reciprocal=depth!=0?div(1,depth):0.f;
                Vector normal=contact.translationCm;for(auto& x:normal)x=mul(x,reciprocal);
                WorldBodyHit hit{true,depth,contact.pointCm,normal,{},instance.resource,0,0,-1,false};
                if(!instance.nodes.empty()){const auto& n=instance.nodes[0];hit.priority=(n.collisionFlags&1)&&std::bit_cast<float>(n.collisionValue)!=0;hit.nodeFlags=n.collisionFlags;hit.nodeValue=n.collisionValue;hit.nodeAuxiliary=n.collisionAuxiliary;}
                hit.instanceFlags=*instance.eventRuntimeFlags;hit.entity=true;offer(hit);continue;
            }
            const WorldCollisionEntity* entity=route==OriginalInstanceBodyRoute::Entity?instance.entity.get():nullptr;
            if(route==OriginalInstanceBodyRoute::Entity&&!entity){++result.candidates;result.unsupportedInstances.push_back(instance.resource);continue;}
            const auto& boundLow=entity?entity->low:instance.low;const auto& boundHigh=entity?entity->high:instance.high;
            bool overlaps=true;for(unsigned k=0;k<3;++k)overlaps&=boundLow[k]<high[k]&&low[k]<boundHigh[k];
            if(!overlaps)continue;
            ++result.candidates;
            if(!instance.unsupported.empty()){result.unsupportedInstances.push_back(instance.resource);continue;}
            for(size_t nodeIndex=0;nodeIndex<instance.nodes.size();++nodeIndex) {
                const auto& node=instance.nodes[nodeIndex];
                const auto& nodeWorld=entity?entity->world.at(nodeIndex):node.world;const auto& nodeInverse=entity?entity->inverse.at(nodeIndex):node.inverse;
                // 334888 0x334D00..0x334D6C: filter 2 skips every surface -1 node
                // (regardless of flags), filter 1 keeps only surface -1 nodes and
                // filter 0 keeps all. Solid surface -1 scenery (node flags 3) is
                // answered by the separate 105398 instance-contact phase.
                if(surfaceFilter==2&&node.surface==-1)continue;
                if(surfaceFilter==1&&node.surface!=-1)continue;
                auto local=collision_transform::toLocal(body,nodeInverse,instance.scale);
                auto accept=[&](const BodyTriangleContact& contact,unsigned triangle) {
                    if(!contact.hit)return;
                    float depth=terrain_original::sqrt(dot(contact.translationCm,contact.translationCm));
                    float reciprocal=depth>0?div(1,depth):std::numeric_limits<float>::max();Vector normal=contact.translationCm;
                    for(auto& x:normal)x=mul(x,reciprocal);
                    normal=collision_transform::apply(nodeWorld,normal,0);
                    WorldBodyHit hit{true,depth,collision_transform::toWorldPoint(contact.pointCm,nodeWorld,instance.scale),normal,{},instance.resource,node.index,triangle,node.surface,false};
                    hit.priority=(node.collisionFlags&1)&&std::bit_cast<float>(node.collisionValue)!=0;hit.nodeFlags=node.collisionFlags;hit.nodeValue=node.collisionValue;hit.nodeAuxiliary=node.collisionAuxiliary;hit.instanceFlags=instance.eventRuntimeFlags?*instance.eventRuntimeFlags:instance.flags;hit.entity=entity!=nullptr||instance.entityPointer;offer(hit);
                };
                if(node.type==2)accept(originalBodyBoxContact(local,node.low,node.high),0);
                else if(node.sphereTree) {
                    bool intersects=true;
                    for(unsigned k=0;k<3;++k)intersects&=sub(local.broadCenterCm[k],local.broadRadiusCm)<=node.high[k]&&node.low[k]<=add(local.broadCenterCm[k],local.broadRadiusCm);
                    if(intersects)accept(originalBodySphereTreeContact(local,*node.sphereTree,&treeMasks(node)),0);
                }
                else if(node.triangles) {
                    bool intersects=true;
                    for(unsigned k=0;k<3;++k)intersects&=sub(local.broadCenterCm[k],local.broadRadiusCm)<=node.high[k]&&node.low[k]<=add(local.broadCenterCm[k],local.broadRadiusCm);
                    if(!intersects)continue;
                    const auto& mesh=*node.triangles;
                    for(unsigned i=0;i<mesh.indices.size()/3;++i) {
                        auto normal=mesh.normals[i];
                        // Original 0x32C17C compares against the retained query
                        // direction before the optional double-sided normal flip.
                        if(surfaceFilter!=1&&dot(normal,groundNormal)>.800000011920929f)continue; //32F650(mode1) disables directional filtering.
                        auto a=mesh.vertices[mesh.indices[i*3]],b=mesh.vertices[mesh.indices[i*3+1]],c=mesh.vertices[mesh.indices[i*3+2]];
                        accept(originalBodyTriangleContact(local,a,b,c,normal,node.doubleSided),i);
                    }
                }
            }
        }
        result.candidateLimitExceeded=result.contacts>=64;
        return result;
    }
};
}
