#pragma once
#include <algorithm>
#include <array>
#include "terrain_contact_math.hpp"
#include "original_spatial.hpp"
#include "world_residency.hpp"
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace ssx {
struct Vec3 {
    double x=0,y=0,z=0;
    double operator[](unsigned i) const { return i==0?x:i==1?y:z; }
    Vec3 operator+(Vec3 b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec3 operator-(Vec3 b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec3 operator*(double s) const { return {x*s,y*s,z*s}; }
};
inline double dot(Vec3 a,Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec3 cross(Vec3 a,Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline Vec3 unit(Vec3 v) { double length=std::sqrt(dot(v,v));return length>0?v*(1/length):Vec3{}; }
struct Triangle { Vec3 a,b,c; uint32_t source=0; };
struct RayHit { bool hit=false; double distance=0; Vec3 position,normal; uint32_t source=0; bool analytic=false; uint32_t resource=0; double u=0,v=0; uint8_t terrainQuery=0; float contactSignedDistanceCm=0,contactLateralDistanceCm=0; int surface=0; uint16_t patchFlags=0; /* authored patch flags (runtime patch+0xA without the loader 0x40) */ };
struct SourceTerrainSegmentHit {
    bool hit=false;float fraction=-1,u=0,v=0;
    terrain_original::Vector pointCm{},normal{};uint32_t resource=0;uint16_t flags=0;
    unsigned candidateCount=0;
    int surface=-1;
};

// Native mesh geometry query. Player capsule/contact response and the original
// surface classifications are separate work; this is not a console collision API.
struct TerrainPatch {
    // Native Y-up meters; original float coefficients converted once at import.
    // Row-major power basis: coefficient[u power + 4*v power].
    std::array<Vec3,16> coefficients;
    uint32_t resource=0,source=0;
    uint16_t authoredFlags=1;
    int16_t authoredSurface=0;
    bool hasAuthoredBounds=false; Vec3 authoredMinimum{},authoredMaximum{};
    void evaluate(double u,double v,Vec3& point,Vec3& du,Vec3& dv) const {
        point=du=dv={};
        double up[4]={1,u,u*u,u*u*u},vp[4]={1,v,v*v,v*v*v};
        for(unsigned j=0;j<4;++j) for(unsigned i=0;i<4;++i) {
            auto c=coefficients[j*4+i];point=point+c*(up[i]*vp[j]);
            if(i) du=du+c*(i*up[i-1]*vp[j]);
            if(j) dv=dv+c*(up[i]*j*vp[j-1]);
        }
    }
};

class CollisionWorld {
    struct Box {
        Vec3 low={INFINITY,INFINITY,INFINITY},high={-INFINITY,-INFINITY,-INFINITY};
        void add(Vec3 p) {
            low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};
            high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};
        }
        bool overlapsSegmentBox(Vec3 a,Vec3 b) const {
            for(unsigned i=0;i<3;++i)
                if(!(low[i]<std::max(a[i],b[i]) && std::min(a[i],b[i])<high[i]))return false;
            return true;
        }
        bool intersects(Vec3 origin,Vec3 direction,double limit) const {
            double near=0,far=limit;
            for (unsigned i=0;i<3;++i) {
                if (std::abs(direction[i])<1e-12f) {
                    if (origin[i]<low[i] || origin[i]>high[i]) return false;
                } else {
                    double a=(low[i]-origin[i])/direction[i],b=(high[i]-origin[i])/direction[i];
                    if (a>b) std::swap(a,b);
                    near=std::max(near,a);far=std::min(far,b);
                    if (near>far) return false;
                }
            }
            return true;
        }
    };
    struct Node { Box box; uint32_t first=0,count=0,left=0,right=0; };
    struct BoundedPatch { TerrainPatch patch; Box bounds,queryBounds; terrain_original::Coefficients sourceCoefficients; std::array<terrain_original::Vector,100> coarseGrid; };
    std::vector<BoundedPatch> patches;
    std::vector<size_t> sourcePatchOrder;
    // sourceTerrainSegment rejects almost every patch on its flag, resource and query box (in source floats), but a
    // BoundedPatch is ~1.6 KB and the order visits them out of memory order: the scan reads those fields from this
    // compact copy, in sourcePatchOrder order, built with the order (orderPatches, the only place it changes).
    struct SourceScanEntry {terrain_original::Vector minimum,maximum;uint32_t resource=0;bool usable=false;};
    std::vector<SourceScanEntry> sourceScan;
    std::vector<OriginalSpatialCell> patchCells; // spatial cell per patch (kept for streamed appends)
    std::vector<Triangle> triangles;
    std::vector<uint32_t> order;
    std::vector<Node> nodes;
    struct BuildTask { uint32_t first,count,parent;uint8_t side; }; // side 1: the parent's left child, 2: its right
    std::vector<BuildTask> pendingBuild;
    uint32_t build(uint32_t first,uint32_t count) {
        uint32_t id=uint32_t(nodes.size());nodes.emplace_back();
        Box bounds,centers;
        for (uint32_t i=first;i<first+count;++i) {
            const auto& t=triangles[order[i]];
            bounds.add(t.a);bounds.add(t.b);bounds.add(t.c);centers.add((t.a+t.b+t.c)*(1.0/3));
        }
        nodes[id].box=bounds;
        if (count<=8) { nodes[id].first=first;nodes[id].count=count;return id; }
        Vec3 extent=centers.high-centers.low;
        unsigned axis=extent.y>extent.x?1:0;if (extent.z>extent[axis]) axis=2;
        auto key=[&](uint32_t index){const auto& t=triangles[index];return t.a[axis]+t.b[axis]+t.c[axis];};
        uint32_t mid=first+count/2;
        std::nth_element(order.begin()+first,order.begin()+mid,order.begin()+first+count,
                         [&](uint32_t a,uint32_t b){return key(a)<key(b);});
        uint32_t left=build(first,mid-first),right=build(mid,first+count-mid);
        nodes[id].left=left;nodes[id].right=right;
        return id;
    }
    void trace(uint32_t id,Vec3 origin,Vec3 direction,RayHit& result) const {
        const Node& node=nodes[id];
        if (!node.box.intersects(origin,direction,result.distance)) return;
        if (!node.count) { trace(node.left,origin,direction,result);trace(node.right,origin,direction,result);return; }
        for (uint32_t i=node.first;i<node.first+node.count;++i) {
            const auto& t=triangles[order[i]];
            Vec3 e1=t.b-t.a,e2=t.c-t.a,p=cross(direction,e2);
            double determinant=dot(e1,p);
            if (std::abs(determinant)<1e-14) continue;
            double inverse=1/determinant;
            Vec3 s=origin-t.a;
            double u=dot(s,p)*inverse;
            if (u<0 || u>1) continue;
            Vec3 q=cross(s,e1);
            double v=dot(direction,q)*inverse;
            if (v<0 || u+v>1) continue;
            double distance=dot(e2,q)*inverse;
            if (distance<0 || distance>result.distance) continue;
            Vec3 normal=unit(cross(e1,e2));
            if (dot(normal,direction)>0) normal=normal*-1;
            result={true,distance,origin+direction*distance,normal,t.source};
        }
    }
    void tracePatches(Vec3 origin,Vec3 direction,RayHit& result,double preferred=-1) const {
        const double maximum=result.distance;
        double best=INFINITY;
        for(const auto& entry:patches) {
            if(!worldResident(entry.patch.resource))continue; // streamed location not loaded (world_residency.hpp)
            if(!entry.bounds.intersects(origin,direction,maximum)) continue;
            // Several seeds retain distinct roots on folded patches. Each accepted
            // root must satisfy the actual polynomial and bounded ray segment.
            for(double seedV:{.1,.5,.9}) for(double seedU:{.1,.5,.9}) {
                double u=seedU,v=seedV,t=0;Vec3 point,du,dv;
                entry.patch.evaluate(u,v,point,du,dv);t=dot(point-origin,direction);
                bool converged=false;
                for(unsigned step=0;step<20;++step) {
                    entry.patch.evaluate(u,v,point,du,dv);
                    Vec3 residual=point-origin-direction*t,third=direction*-1;
                    if(dot(residual,residual)<1e-16) {converged=true;break;}
                    double determinant=dot(du,cross(dv,third));
                    if(std::abs(determinant)<1e-14) break;
                    double deltaU=dot(residual,cross(dv,third))/determinant;
                    double deltaV=dot(du,cross(residual,third))/determinant;
                    double deltaT=dot(du,cross(dv,residual))/determinant;
                    u-=deltaU;v-=deltaV;t-=deltaT;
                    if(!std::isfinite(u)||!std::isfinite(v)||!std::isfinite(t)||std::abs(u)>4||std::abs(v)>4) break;
                }
                if(!converged||u< -1e-8||u>1+1e-8||v< -1e-8||v>1+1e-8||t<0||t>maximum) continue;
                double metric=preferred>=0?std::abs(t-preferred):t;
                if(metric>=best) continue;
                auto normal=unit(cross(du,dv));if(dot(normal,direction)>0) normal=normal*-1;
                best=metric;result={true,t,point,normal,entry.patch.source,true,entry.patch.resource,u,v};result.surface=entry.patch.authoredSurface;
            }
        }
    }
public:
    // append: a streamed location's patches join the loaded ones (web/peak_world.inc); later loads are inserted at the
    // octree list heads after the earlier ones, which the tie order below (higher index first) reproduces.
    void setTerrainPatches(std::vector<TerrainPatch> input,bool append=false) {
        if(!append){patches.clear();patchCells.clear();}
        // A streamed world appends a location in slices of three patches: grow the list geometrically there (an exact reserve
        // per slice moved the whole list every slice and fragmented the heap; the order and contents are the same either way).
        if(append&&patches.capacity()<patches.size()+input.size())patches.reserve(std::max(patches.size()+input.size(),patches.capacity()+patches.capacity()/2));
        else patches.reserve(patches.size()+input.size());
        // Convert power coefficients to Bezier controls. The convex hull gives
        // conservative bounds even where the patch bends beyond its four corners.
        static constexpr double choose[4][4]={{1,0,0,0},{1,1,0,0},{1,2,1,0},{1,3,3,1}};
        for(auto& patch:input) {
            BoundedPatch entry;entry.patch=patch;
            for(auto c:patch.coefficients)
                if(!std::isfinite(c.x)||!std::isfinite(c.y)||!std::isfinite(c.z)) throw std::runtime_error("Nonfinite terrain coefficient");
            for(unsigned j=0;j<4;++j) for(unsigned i=0;i<4;++i) {
                Vec3 control;
                for(unsigned b=0;b<=j;++b) for(unsigned a=0;a<=i;++a)
                    control=control+patch.coefficients[b*4+a]*(choose[i][a]/choose[3][a]*choose[j][b]/choose[3][b]);
                entry.bounds.add(control);
            }
            // Include roundoff at an exact boundary without changing the surface.
            entry.bounds.low=entry.bounds.low-Vec3{1e-7,1e-7,1e-7};entry.bounds.high=entry.bounds.high+Vec3{1e-7,1e-7,1e-7};
            for(unsigned i=0;i<16;++i) {
                auto c=patch.coefficients[i];entry.sourceCoefficients[i]={float(c.x*100),float(-c.z*100),float(c.y*100)};
            }
            entry.coarseGrid=terrain_original::coarseGrid(entry.sourceCoefficients);
            entry.queryBounds=entry.bounds;
            if(patch.hasAuthoredBounds){entry.queryBounds.low=patch.authoredMinimum;entry.queryBounds.high=patch.authoredMaximum;}
            patches.push_back(entry);
        }
        if(append){orderPending=true;return;} // streamed slices: one ordering pass when the location is complete (finishAppend)
        orderPatches();
    }
    // The appended slices of a streamed location join the query order (web/peak_world.inc peak_world_commit).
    void finishAppend(){if(orderPending)orderPatches();}
    // pv peakRelease (web/peak_world.inc release at T+7): a streamed location's patches leave (3284B8); the others keep their order
    // (the tie order is the index, which an erase keeps relative), a re-read appends them as the newest. Returns the patches removed.
    size_t releaseTrack(uint32_t track){
        if(orderPending)orderPatches();
        size_t kept=0;const size_t before=patches.size();
        for(size_t i=0;i<patches.size();++i)if((uint32_t(patches[i].patch.resource)&255u)!=track){if(kept!=i){patches[kept]=std::move(patches[i]);patchCells[kept]=patchCells[i];}++kept;}
        patches.resize(kept);patchCells.resize(kept);
        if(kept!=before)orderPatches();
        return before-kept;
    }
private:
    bool orderPending=false;
    void orderPatches(){
        orderPending=false;
        sourcePatchOrder.resize(patches.size());std::iota(sourcePatchOrder.begin(),sourcePatchOrder.end(),0);
        auto& cells=patchCells;cells.reserve(patches.size());
        for(size_t i=cells.size();i<patches.size();++i) {
            auto low=patches[i].queryBounds.low,high=patches[i].queryBounds.high;
            cells.push_back(originalSpatialCell({float(low.x*100),float(-high.z*100),float(low.y*100)},
                                               {float(high.x*100),float(-low.z*100),float(high.y*100)}));
        }
        std::sort(sourcePatchOrder.begin(),sourcePatchOrder.end(),[&](size_t a,size_t b) {
            if(originalSpatialBefore(cells[a],cells[b]))return true;
            if(originalSpatialBefore(cells[b],cells[a]))return false;
            return a>b; // Original linked-list insertion is at the head.
        });
        sourceScan.resize(sourcePatchOrder.size());
        for(size_t i=0;i<sourcePatchOrder.size();++i) {
            const auto& entry=patches[sourcePatchOrder[i]];auto low=entry.queryBounds.low,high=entry.queryBounds.high;
            sourceScan[i]={{float(low.x*100),float(-high.z*100),float(low.y*100)},{float(high.x*100),float(-low.z*100),float(high.y*100)},uint32_t(entry.patch.resource),(entry.patch.authoredFlags&1)!=0};
        }
    }
public:
    size_t patchCount() const { return patches.size(); }
    // 336850 / 335960 / 32B6E0: kind1 keeps coarse9x9 triangles;
    // kind2 refines point/normal/UV while retaining the coarse hit fraction.
    // Preferred fraction is query+A0 (F12 at 32E100), not a sphere radius.
    SourceTerrainSegmentHit sourceTerrainSegment(terrain_original::Vector origin,
            terrain_original::Vector end,float preferredFraction=1,bool refinedTerrain=false,terrain_original::ContactCache* cache=nullptr,
            const terrain_original::RiderScope* scope=nullptr) const {
        using namespace terrain_original;Rounding rounding;
        GroundProbe probe;probe.origin=origin;probe.direction=difference(end,origin);
        probe.preferredFraction=preferredFraction;probe.explicitEnd=end;probe.hasExplicitEnd=true;
        SourceTerrainSegmentHit result;float best=INFINITY;unsigned candidates=0;
        for(size_t position=0;position<sourcePatchOrder.size();++position) {
            const auto& scan=sourceScan[position];
            if(!scan.usable||!worldResident(scan.resource))continue;
            const Vector& minimum=scan.minimum;const Vector& maximum=scan.maximum;
            if(scope&&!scope->admits(minimum,maximum))continue; // not in the rider's 332DB8 scope list
            bool overlaps=true;
            for(unsigned k=0;k<3;++k)overlaps&=minimum[k]<std::max(origin[k],end[k])&&std::min(origin[k],end[k])<maximum[k];
            if(!overlaps)continue;
            const auto& entry=patches[sourcePatchOrder[position]];
            bool cached=cache&&cache->valid&&cache->resource==entry.patch.resource;
            auto coarse=coarseContact(entry.coarseGrid,probe,cached?cache:nullptr);
            if(!coarse.hit){if(cached)cache->valid=false;continue;}
            if(cache)*cache={true,entry.patch.resource,coarse.cellU,coarse.cellV,coarse.half};
            ++candidates;
            float metric=std::abs(originalScalarSubtract(coarse.fraction,preferredFraction));
            if(result.hit&&(metric>best||(metric==best&&entry.patch.resource>=result.resource)))continue;
            best=metric;
            float u=originalScalarAdd(mul(float(coarse.cellU),0.1111111119389534f),0.0555555559694767f);
            float v=originalScalarAdd(mul(float(coarse.cellV),0.1111111119389534f),0.0555555559694767f);
            auto point=coarse.point,normal=coarse.normal;
            if(refinedTerrain) {
                auto refined=refine(entry.sourceCoefficients,probe.origin,probe.direction,u,v);u=refined.u;v=refined.v;
                if(refined.valid){point=refined.point;normal=refined.normal;}
            }
            result={true,coarse.fraction,u,v,point,normal,entry.patch.resource,entry.patch.authoredFlags};
            result.surface=entry.patch.authoredSurface;
        }
        result.candidateCount=candidates;return result;
    }
    // Original grounded probe uses the previous normal, and chooses the root
    // closest to fraction 0.5 of the -100cm..+200cm segment (PS2 0x1242B0,
    // 0x3342D0, and query accessor 0x340A10).
    // Steering's lateral center offset belongs to the caller.
    RayHit surfaceContact(Vec3 center,Vec3 previousNormal) const {
        auto direction=unit(previousNormal);
        if(dot(direction,direction)<.5) return {};
        if(patches.empty()) return raycast(center+direction*2,direction*-1,3);
        RayHit result;result.distance=3;
        tracePatches(center-direction,direction,result,1.5);
        if(result.hit&&dot(result.normal,direction)<0) result.normal=result.normal*-1;
        return result;
    }
    // Original source-float ground query: nine-by-nine coarse cells, authored
    // collision flag, original triangle order, and four Newton refinements.
    // terrainQuery=3 records the original coarse-cell path; the coarse ray
    // fraction is retained for nearest-preferred-fraction candidate selection.
    RayHit sourceGroundContact(Vec3 position,Vec3 previousNormal,Vec3 previousLateral,
                               float filteredTurn,float geometryScale,terrain_original::ContactCache* cache=nullptr,const terrain_original::RiderScope* scope=nullptr) const {
        auto sourcePosition=[](Vec3 p){return terrain_original::Vector{float(p.x*100),float(-p.z*100),float(p.y*100)};};
        auto sourceDirection=[](Vec3 p){return terrain_original::Vector{float(p.x),float(-p.z),float(p.y)};};
        auto probe=terrain_original::groundProbe(sourcePosition(position),sourceDirection(previousNormal),
                                                sourceDirection(previousLateral),filteredTurn,geometryScale);
        Vec3 origin{probe.origin[0]/100.,probe.origin[2]/100.,-probe.origin[1]/100.};
        Vec3 direction{probe.direction[0]/100.,probe.direction[2]/100.,-probe.direction[1]/100.};
        double length=std::sqrt(dot(direction,direction));
        if(length<=0)return {};
        if(patches.empty())return surfaceContact(position,previousNormal);
        RayHit hit;hit.distance=length;float best=INFINITY;bool bestAligned=false;
        for(size_t position=0;position<sourcePatchOrder.size();++position) {
            const auto& entry=patches[sourcePatchOrder[position]];
            if(!(entry.patch.authoredFlags&1)||!worldResident(entry.patch.resource)||!entry.queryBounds.overlapsSegmentBox(origin,origin+direction))continue;
            // The rider's world queries walk its 332DB8 scope list (rider+0x860 -> +0xB50 +0x10C/+0x110): a patch the last refresh
            // did not list is not ground yet (PS2 metro Luther 505: patch 75280 joins the list at 508, so the rider leaves the ground).
            if(scope&&!scope->admits(sourceScan[position].minimum,sourceScan[position].maximum))continue;
            bool cached=cache&&cache->valid&&cache->resource==entry.patch.resource;
            auto coarse=terrain_original::coarseContact(entry.coarseGrid,probe,cached?cache:nullptr);
            if(!coarse.hit){if(cached)cache->valid=false;continue;}
            if(cache)*cache={true,entry.patch.resource,coarse.cellU,coarse.cellV,coarse.half};
            float metric=std::abs(originalScalarSubtract(coarse.fraction,probe.preferredFraction));
            float initialU,initialV;
            {
                terrain_original::Rounding rounding;
                initialU=originalScalarAdd(terrain_original::mul(float(coarse.cellU),0.1111111119389534f),0.0555555559694767f);
                initialV=originalScalarAdd(terrain_original::mul(float(coarse.cellV),0.1111111119389534f),0.0555555559694767f);
            }
            auto refined=terrain_original::refine(entry.sourceCoefficients,probe.origin,probe.direction,initialU,initialV);
            auto point=refined.valid?refined.point:coarse.point;
            auto normal=refined.valid?refined.normal:coarse.normal;
            bool aligned;
            {terrain_original::Rounding rounding;aligned=terrain_original::dot(normal,sourceDirection(previousNormal))>=.30000001192092896f;}
            if(!terrain_original::preferGroundCandidate(hit.hit,bestAligned,best,hit.resource,aligned,metric,entry.patch.resource))continue;
            best=metric;bestAligned=aligned;
            hit={true,coarse.fraction*length,{point[0]/100.,point[2]/100.,-point[1]/100.},
                 {normal[0],normal[2],-normal[1]},entry.patch.source,true,entry.patch.resource,refined.u,refined.v,3};
            hit.surface=entry.patch.authoredSurface;hit.patchFlags=entry.patch.authoredFlags;
            {
                terrain_original::Rounding rounding;
                auto offset=terrain_original::difference(probe.center,point);
                hit.contactSignedDistanceCm=terrain_original::dot(offset,normal);
                for(unsigned k=0;k<3;++k)offset[k]=terrain_original::sub(offset[k],terrain_original::mul(normal[k],hit.contactSignedDistanceCm));
                hit.contactLateralDistanceCm=std::sqrt(terrain_original::dot(offset,offset));
            }
        }
        return hit;
    }
    explicit CollisionWorld(std::vector<Triangle> input):triangles(std::move(input)) {
        if (triangles.size()>std::numeric_limits<uint32_t>::max()/2) throw std::runtime_error("Collision mesh too large");
        for (const auto& t:triangles)
            for (Vec3 v:{t.a,t.b,t.c})
                if (!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)) throw std::runtime_error("Nonfinite collision geometry");
        order.resize(triangles.size());std::iota(order.begin(),order.end(),0);
        nodes.reserve(triangles.size()/2+1);
        if (!triangles.empty()) build(0,uint32_t(triangles.size()));
    }
    // The same tree built a few nodes at a time (web/core.cpp init_world_begin / init_world_step, pv eventSlices): the nodes are
    // made in build()'s order (pre-order, the left subtree first; a range's nth_element runs when its node is made, after every
    // node before it, as in the recursion), so nodes and order come out identical (web/test-event-slices.mjs world_load_hash).
    struct DeferredBuild {};
    CollisionWorld(std::vector<Triangle> input,DeferredBuild):triangles(std::move(input)) {
        if (triangles.size()>std::numeric_limits<uint32_t>::max()/2) throw std::runtime_error("Collision mesh too large");
        for (const auto& t:triangles)
            for (Vec3 v:{t.a,t.b,t.c})
                if (!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)) throw std::runtime_error("Nonfinite collision geometry");
        order.resize(triangles.size());std::iota(order.begin(),order.end(),0);
        nodes.reserve(triangles.size()/2+1);
        if (!triangles.empty()) pendingBuild.push_back({0,uint32_t(triangles.size()),std::numeric_limits<uint32_t>::max(),0});
    }
    // Makes nodes until about `budget` triangles were scanned (at least one node); true when the tree is complete.
    bool buildStep(size_t budget) {
        size_t scanned=0;
        while (!pendingBuild.empty()&&(scanned==0||scanned<budget)) {
            const BuildTask task=pendingBuild.back();pendingBuild.pop_back();
            const uint32_t first=task.first,count=task.count,id=uint32_t(nodes.size());nodes.emplace_back();
            if (task.parent!=std::numeric_limits<uint32_t>::max()) { if (task.side==1) nodes[task.parent].left=id; else nodes[task.parent].right=id; }
            Box bounds,centers;
            for (uint32_t i=first;i<first+count;++i) {
                const auto& t=triangles[order[i]];
                bounds.add(t.a);bounds.add(t.b);bounds.add(t.c);centers.add((t.a+t.b+t.c)*(1.0/3));
            }
            scanned+=count;
            nodes[id].box=bounds;
            if (count<=8) { nodes[id].first=first;nodes[id].count=count;continue; }
            Vec3 extent=centers.high-centers.low;
            unsigned axis=extent.y>extent.x?1:0;if (extent.z>extent[axis]) axis=2;
            auto key=[&](uint32_t index){const auto& t=triangles[index];return t.a[axis]+t.b[axis]+t.c[axis];};
            uint32_t mid=first+count/2;
            std::nth_element(order.begin()+first,order.begin()+mid,order.begin()+first+count,
                             [&](uint32_t a,uint32_t b){return key(a)<key(b);});
            pendingBuild.push_back({mid,first+count-mid,id,2});
            pendingBuild.push_back({first,mid-first,id,1});
        }
        return pendingBuild.empty();
    }
    bool building() const { return !pendingBuild.empty(); }
    // Tests (web/test-event-slices.mjs): every field a load puts in, through `mix(pointer, bytes)` (no padding bytes).
    template<class Mix> void hashState(Mix&& mix) const {
        auto u32=[&](uint32_t v){mix(&v,4);};auto f64=[&](double v){mix(&v,8);};auto vec=[&](const Vec3& v){f64(v.x);f64(v.y);f64(v.z);};
        auto f32=[&](float v){mix(&v,4);};
        u32(uint32_t(triangles.size()));for (const auto& t:triangles){vec(t.a);vec(t.b);vec(t.c);u32(t.source);}
        u32(uint32_t(order.size()));for (uint32_t o:order)u32(o);
        u32(uint32_t(nodes.size()));for (const auto& n:nodes){vec(n.box.low);vec(n.box.high);u32(n.first);u32(n.count);u32(n.left);u32(n.right);}
        u32(uint32_t(patches.size()));
        for (const auto& e:patches){
            for (const auto& c:e.patch.coefficients)vec(c);u32(e.patch.resource);u32(e.patch.source);u32(e.patch.authoredFlags);u32(uint32_t(int32_t(e.patch.authoredSurface)));
            u32(e.patch.hasAuthoredBounds);vec(e.patch.authoredMinimum);vec(e.patch.authoredMaximum);
            vec(e.bounds.low);vec(e.bounds.high);vec(e.queryBounds.low);vec(e.queryBounds.high);
            for (const auto& c:e.sourceCoefficients)for (float x:c)f32(x);for (const auto& c:e.coarseGrid)for (float x:c)f32(x);
        }
        u32(uint32_t(sourcePatchOrder.size()));for (size_t o:sourcePatchOrder)u32(uint32_t(o));
        u32(uint32_t(sourceScan.size()));for (const auto& e:sourceScan){for (float x:e.minimum)f32(x);for (float x:e.maximum)f32(x);u32(e.resource);u32(e.usable);}
        u32(uint32_t(patchCells.size()));for (const auto& c:patchCells){u32(uint32_t(c.level));for (int32_t x:c.coordinate)u32(uint32_t(x));}
        u32(orderPending);u32(uint32_t(pendingBuild.size()));
    }
    RayHit raycast(Vec3 origin,Vec3 direction,double maximum) const {
        RayHit result;result.distance=maximum;
        if (maximum<=0||dot(direction,direction)<1e-12f) return result;
        if(!patches.empty()) tracePatches(origin,unit(direction),result);
        else if(!nodes.empty()) trace(0,origin,unit(direction),result);
        return result;
    }
    size_t triangleCount() const { return triangles.size(); }
};
}
