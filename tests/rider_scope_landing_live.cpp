// Live check of the rider query scope on the landing probe (The Junction super pipe, pipe-tricks capture).
// 3342D0 only visits the rider's scope lists (rider+0x860), which 332DB8 builds from the rider query bounds
// (rider+0x400/+0x410); the browser rebuilds the scope every third game tick (web/core.cpp riderScope), so the
// probe of an inverted rider (200cm past the board root on the head side) can leave it (pipe-tricks 1361).
// Part 1 (scope lists): random boxes around the captured rider; the original 332DB8 terrain list (scope+0x110,
// count +0x10C) and static-instance list (scope+0xC, count +8, instance flag 0x20) against
// terrain_original::RiderScope::admits over the browser world package.
// Part 2 (landing probe): captured board roots/presentation up of random ticks with the query bounds of an
// earlier tick (0..6 ticks back, the scope refresh lag) plus random offsets and orientations; the original
// scope refresh 0x332DB8 and complete landing probe 0x13A7B0 against originalLandingContact(..., &scope).
// Part 3: the captured inverted landing (ticks 1361..1363) with the scope the refresh cadence selects.
#include "live_registrations.inc"
#include "original_live_runtime.hpp"
#include "../engine/landing_contact.hpp"
#include <map>
#include <memory>
#include <set>
std::unique_ptr<ssx::WorldBodyCollision> loadRollerNativeWorld(const char* folder);
std::unique_ptr<ssx::CollisionWorld> loadScopeNativeTerrain(const char* folder);
using namespace ssx;using namespace terrain_original;
static const std::map<uint32_t,std::array<Vector,100>>* oracleGrids=nullptr;
static void exactGrid(uint8_t* rdram,R5900Context* ctx,PS2Runtime* runtime){
    // 0x327C00's recompiled VU0 program is not bit-exact; store the console-exact grid (see roller live test).
    const uint32_t patch=GPR_U32(ctx,5),entry=GPR_U32(ctx,6);sub_00327C00_0x327c00(rdram,ctx,runtime);
    uint32_t resource;std::memcpy(&resource,rdram+patch+0x150,4);auto it=oracleGrids->find(resource);if(it==oracleGrids->end())throw std::runtime_error("grid for an unknown patch");
    for(unsigned i=0;i<100;++i)std::memcpy(rdram+entry+0x10+16*i,it->second[i].data(),12);
}
int main(int argc,char** argv){
    if(argc!=6){fprintf(stderr,"usage: ASSETS CAPTURE.bin EE VUC VUD\n");return 2;}
    auto world=loadRollerNativeWorld(argv[1]);auto terrain=loadScopeNativeTerrain(argv[1]);
    std::map<uint32_t,std::array<Vector,100>> grids;for(const auto& t:world->terrain)grids[t.resource]=t.grid;oracleGrids=&grids;
    std::map<uint32_t,const WorldCollisionInstance*> instances;for(const auto& i:world->instances)instances[i.resource]=&i;
    const auto capture=live::readFile(argv[2]);constexpr size_t R=16384;uint32_t t0;std::memcpy(&t0,capture.data()+4,4);const int ticks=int(capture.size()/R);
    auto field=[&](int tick,uint32_t offset){Vector v;std::memcpy(v.data(),capture.data()+size_t(tick-int(t0))*R+32+offset-0x100,12);return v;};
    auto boardRoot=[&](int tick){Vector v;std::memcpy(v.data(),capture.data()+size_t(tick-int(t0))*R+3264+22*32,12);return v;};
    live::Machine m(argv[3],argv[4],argv[5]);m.runtime.registerFunction(0x327c00,exactGrid);
    const std::vector<uint8_t> original=m.ee;constexpr uint32_t rider=0x14542A0,output=0x90000;   // BHP1 human rider
    uint32_t seed=0x5eed1361u;auto random=[&](){seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
    auto refresh=[&](const Vector& low,const Vector& high){m.put(rider+0x400,low);m.put(rider+0x410,high);m.call(0x332db8,{m.get<uint32_t>(rider+0x860),rider+0x400});};
    // ---- part 1: scope lists ----
    unsigned boxes=0,patchEntries=0,instanceEntries=0;std::set<uint32_t> absent;
    for(unsigned n=0;n<4000;++n){
        m.ee=original;const int tick=int(t0)+1+int(random()*float(ticks-2));Vector low=field(tick,0x400),high;
        for(unsigned k=0;k<3;++k){low[k]=low[k]+(random()-.5f)*800;high[k]=low[k]+random()*700;}
        refresh(low,high);const uint32_t scope=m.get<uint32_t>(rider+0x860);const RiderScope native{low,high};
        std::set<uint32_t> originalPatches,nativePatches,originalInstances,nativeInstances;
        for(uint32_t i=0,count=m.get<uint32_t>(scope+0x10C);i<count;++i)originalPatches.insert(m.get<uint32_t>(m.get<uint32_t>(scope+0x110+4*i)+0x150));
        for(const auto& patch:world->terrain)if(native.admits(patch.low,patch.high))nativePatches.insert(patch.resource);
        for(uint32_t i=0,count=m.get<uint32_t>(scope+0x8);i<count;++i){const uint32_t item=m.get<uint32_t>(scope+0xC+4*i);
            if(m.get<uint32_t>(item+8)&0x40)continue; // entity route: virtual +0x168 box
            if(!instances.count(m.get<uint32_t>(item+0x78))){absent.insert(m.get<uint32_t>(item+0x78));continue;} // not in the browser package
            originalInstances.insert(m.get<uint32_t>(item+0x78));}
        for(const auto& instance:world->instances){
            if(instance.type==0||(instance.eventRuntimeFlags&&!(*instance.eventRuntimeFlags&0x20)))continue;
            if(native.admits(instance.low,instance.high))nativeInstances.insert(instance.resource);}
        if(originalPatches!=nativePatches){fprintf(stderr,"scope terrain list differs at box %u (original %zu, native %zu)\n",n,originalPatches.size(),nativePatches.size());return 1;}
        if(originalInstances!=nativeInstances){fprintf(stderr,"scope instance list differs at box %u (original %zu, native %zu)\n",n,originalInstances.size(),nativeInstances.size());return 1;}
        ++boxes;patchEntries+=originalPatches.size();instanceEntries+=originalInstances.size();
    }
    // ---- parts 2 and 3: landing probes through the scope ----
    unsigned probes=0,contacts=0,scopeMisses=0,cacheOrder=0,unsupported=0;
    auto probe=[&](const Vector& center,const Vector& up,const Vector& low,const Vector& high,bool keepCache,int label)->float{
        m.ee=original;refresh(low,high);
        const uint32_t geometry=m.get<uint32_t>(rider+0x780),bone=m.get<uint32_t>(geometry+0x2c)+m.get<uint32_t>(rider+0x8a0)*32,cacheAt=m.get<uint32_t>(rider+0x864);
        m.put(bone,center);m.put(rider+0x180,up);if(!keepCache&&cacheAt)m.put(cacheAt,0u);
        ContactCache cache;if(cacheAt&&m.get<uint32_t>(cacheAt)&&m.get<uint32_t>(cacheAt+12)==2){cache.valid=true;cache.resource=m.get<uint32_t>(m.get<uint32_t>(cacheAt)+0x150);cache.cellU=m.get<uint16_t>(cacheAt+4);cache.cellV=m.get<uint16_t>(cacheAt+6);cache.half=m.get<uint32_t>(cacheAt+8);}
        const RiderScope scope{low,high};OriginalLandingProbe query;query.centerCm=center;query.presentationUp=up;
        auto actual=originalLandingContact(*terrain,world.get(),query,&cache,&scope);if(!actual.complete){if(label<0){fprintf(stderr,"native landing incomplete (case %d)\n",label);exit(3);}++unsupported;return -2.f;} // an unported dynamic-entity callback on the segment
        auto unscoped=originalLandingContact(*terrain,world.get(),query);
        auto c=m.call(0x13a7b0,{m.get<uint32_t>(rider+0x77c)+0x20,output},{},0x70000);
        if(cacheAt){uint32_t pointer=m.get<uint32_t>(cacheAt);bool valid=pointer&&m.get<uint32_t>(cacheAt+12)==2;
            if(cache.valid!=valid||(valid&&(cache.resource!=m.get<uint32_t>(pointer+0x150)||cache.cellU!=m.get<uint16_t>(cacheAt+4)||cache.cellV!=m.get<uint16_t>(cacheAt+6)||cache.half!=m.get<uint32_t>(cacheAt+8))))++cacheOrder; // patch visit order (see rider_scope_query_live.cpp)
        }
        ++probes;scopeMisses+=unscoped.fraction!=c.f[0];
        if(c.f[0]<0){if(actual.fraction!=c.f[0]){fprintf(stderr,"landing case %d: native %a on original miss\n",label,actual.fraction);exit(4);}return c.f[0];}
        Vector point=m.get<Vector>(output),normal=m.get<Vector>(output+16),velocity=m.get<Vector>(output+32);float u=m.get<float>(output+0x6c),v=m.get<float>(output+0x70);
        int surface=int(m.get<uint32_t>(output+0x4c));uint32_t patch=m.get<uint32_t>(output+0x54),instance=m.get<uint32_t>(output+0x50);if(patch)patch=m.get<uint32_t>(patch+0x150);if(instance)instance=m.get<uint32_t>(instance+0x78);
        if(actual.fraction!=c.f[0]||actual.position!=point||actual.normal!=normal||actual.surfaceVelocityCmps!=velocity||actual.surface!=surface||actual.instance!=instance||actual.hasPatch!=bool(patch)||(patch&&(uint32_t(actual.patchId)!=patch||actual.patchU!=u||actual.patchV!=v))){
            fprintf(stderr,"landing mismatch case %d fraction %a/%a patch %x/%x\n",label,actual.fraction,c.f[0],actual.patchId,patch);exit(1);}
        ++contacts;return c.f[0];
    };
    for(int n=0;n<3000;++n){
        const int tick=int(t0)+8+int(random()*float(ticks-9)),lag=int(random()*7);
        Vector center=boardRoot(tick),up=field(tick,0x180);
        if(n%3){for(unsigned k=0;k<3;++k)center[k]=center[k]+(random()-.5f)*300;}
        if(n%4==1){Vector r{random()-.5f,random()-.5f,random()-.5f};float length=std::sqrt(r[0]*r[0]+r[1]*r[1]+r[2]*r[2]);if(length>1e-3f)for(unsigned k=0;k<3;++k)up[k]=r[k]/length;}
        probe(center,up,field(tick-lag,0x400),field(tick-lag,0x410),n%2==0,n);
    }
    // The captured inverted landing: records 1361/1362 read the scope refreshed after record 1359, record 1363 the one after 1362.
    const float f1361=probe(boardRoot(1361),field(1361,0x180),field(1359,0x400),field(1359,0x410),false,-1361);
    const float f1362=probe(boardRoot(1362),field(1362,0x180),field(1359,0x400),field(1359,0x410),false,-1362);
    const float f1363=probe(boardRoot(1363),field(1363,0x180),field(1362,0x400),field(1362,0x410),false,-1363);
    if(!(f1361<0&&f1362<0&&f1363>=.5f)){fprintf(stderr,"captured inverted landing: fractions %g %g %g (expected miss, miss, >= 0.5)\n",f1361,f1362,f1363);return 5;}
    printf("rider scope landing live: %u scope boxes (%u terrain, %u static-instance entries) match RiderScope::admits (%zu scope instances absent from the browser package); %u landing probes (%u contacts) through a lagged 332DB8 scope match exactly, %u of them differ from an unscoped query; captured inverted landing misses at 1361/1362 and lands at 1363 (fraction %.6f); %u probes leave another overlapping patch in the contact cache (patch visit order); %u probes skipped (explicit unsupported entity callbacks)\n",
           boxes,patchEntries,instanceEntries,absent.size(),probes,contacts,scopeMisses,f1363,cacheOrder,unsupported);
}
