// Live check of the rider world queries on the connector terrain (A_ARA1 / ARA1_B, live in the
// Snow Jam race event) through the rider's own query scope: the rider query bounds
// (rider+0x400/+0x410), the captured posed board root and the body shape are moved onto a target,
// the original 0x120E50 scope refresh 0x332DB8(rider+0x860, rider+0x400) rebuilds the scope, then
// the complete original landing probe 0x13A7B0 and body query 0x32F650 -> 0x3342D0 run against the
// native originalLandingContact / WorldBodyCollision::query on the browser world package.
// Targets: tools/terrain_probe_targets.py (over and next to the connector patches).
#include "live_registrations.inc"
#include "original_live_runtime.hpp"
#include "../engine/landing_contact.hpp"
#include <map>
#include <memory>
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
    if(argc<4){fprintf(stderr,"usage: ASSETS TARGETS STATE...(ee,vuc,vud)\n");return 2;}
    auto world=loadRollerNativeWorld(argv[1]);auto terrain=loadScopeNativeTerrain(argv[1]);
    std::map<uint32_t,std::array<Vector,100>> grids;for(const auto& t:world->terrain)grids[t.resource]=t.grid;oracleGrids=&grids;
    std::vector<Vector> targets;{auto raw=live::readFile(argv[2]);for(size_t at=0;at+12<=raw.size();at+=12){Vector v;std::memcpy(v.data(),raw.data()+at,12);targets.push_back(v);}}
    unsigned landing=0,landingHits=0,body=0,bodyHits=0,connectorHits=0,cacheOrder=0;
    for(int s=3;s+2<argc;s+=3) {
        live::Machine base(argv[s],argv[s+1],argv[s+2]);base.runtime.registerFunction(0x327c00,exactGrid);
        const std::vector<uint8_t> original=base.ee;constexpr uint32_t rider=0x14701a0,output=0x90000,query=0x80000,done=0x12345678;
        auto& m=base;
        for(size_t n=0;n<targets.size();++n) for(unsigned which=0;which<2;++which) {
            m.ee=original;
            const uint32_t geometry=m.get<uint32_t>(rider+0x780),bone=m.get<uint32_t>(geometry+0x2c)+m.get<uint32_t>(rider+0x8a0)*32,shape=m.get<uint32_t>(rider+0xaa0);
            // Offset everything by target - reference (landing: board root; body: broad centre).
            Vector reference=which==0?m.get<Vector>(bone):m.get<Vector>(shape+16),offset;
            {Rounding r;for(unsigned k=0;k<3;++k)offset[k]=sub(targets[n][k],reference[k]);}
            auto shift=[&](uint32_t at){Vector v=m.get<Vector>(at);Rounding r;for(unsigned k=0;k<3;++k)v[k]=add(v[k],offset[k]);m.put(at,v);return v;};
            shift(rider+0x400);shift(rider+0x410);
            m.call(0x332db8,{m.get<uint32_t>(rider+0x860),rider+0x400});   // 0x120E50's scope refresh
            if(which==0) {
                const uint32_t cacheAt=m.get<uint32_t>(rider+0x864);
                OriginalLandingProbe probe;probe.centerCm=shift(bone);probe.presentationUp=m.get<Vector>(rider+0x180);
                ContactCache cache;if(cacheAt&&m.get<uint32_t>(cacheAt)&&m.get<uint32_t>(cacheAt+12)==2){cache.valid=true;cache.resource=m.get<uint32_t>(m.get<uint32_t>(cacheAt)+0x150);cache.cellU=m.get<uint16_t>(cacheAt+4);cache.cellV=m.get<uint16_t>(cacheAt+6);cache.half=m.get<uint32_t>(cacheAt+8);}
                auto actual=originalLandingContact(*terrain,world.get(),probe,&cache);if(!actual.complete){fprintf(stderr,"native landing incomplete\n");return 3;}
                auto c=m.call(0x13a7b0,{m.get<uint32_t>(rider+0x77c)+0x20,output},{},0x70000);
                if(cacheAt){uint32_t pointer=m.get<uint32_t>(cacheAt);bool valid=pointer&&m.get<uint32_t>(cacheAt+12)==2;
                    if(cache.valid!=valid||(valid&&(cache.resource!=m.get<uint32_t>(pointer+0x150)||cache.cellU!=m.get<uint16_t>(cacheAt+4)||cache.cellV!=m.get<uint16_t>(cacheAt+6)||cache.half!=m.get<uint32_t>(cacheAt+8)))){
                        // Known gap: CollisionWorld visits patches in the fixed 0..7 octree order, the scope list
                        // (332DB8: Gray order for partial cells) can differ, so the last-written cell cache can
                        // name another overlapping patch. The contact itself is compared below.
                        ++cacheOrder;if(getenv("CACHE_TRACE"))fprintf(stderr,"landing cache order target %zu native %x cell %u,%u original %x cell %u,%u\n",n,cache.resource,cache.cellU,cache.cellV,pointer?m.get<uint32_t>(pointer+0x150):0,m.get<uint16_t>(cacheAt+4),m.get<uint16_t>(cacheAt+6));}}
                if(c.f[0]<0){if(actual.fraction!=c.f[0]){fprintf(stderr,"landing target %zu: native %g on original miss\n",n,actual.fraction);return 4;}++landing;continue;}
                Vector point=m.get<Vector>(output),normal=m.get<Vector>(output+16),velocity=m.get<Vector>(output+32);float u=m.get<float>(output+0x6c),v=m.get<float>(output+0x70);
                int surface=int(m.get<uint32_t>(output+0x4c));uint32_t patch=m.get<uint32_t>(output+0x54),instance=m.get<uint32_t>(output+0x50);if(patch)patch=m.get<uint32_t>(patch+0x150);if(instance)instance=m.get<uint32_t>(instance+0x78);
                if(actual.fraction!=c.f[0]||actual.position!=point||actual.normal!=normal||actual.surfaceVelocityCmps!=velocity||actual.surface!=surface||actual.instance!=instance||actual.hasPatch!=bool(patch)||(patch&&(uint32_t(actual.patchId)!=patch||actual.patchU!=u||actual.patchV!=v))){
                    fprintf(stderr,"landing mismatch target %zu fraction %a/%a patch %x/%x\n",n,actual.fraction,c.f[0],actual.patchId,patch);return 1;}
                ++landing;++landingHits;connectorHits+=patch&&(patch&255)!=8;
            } else {
                const uint32_t cacheAt=m.get<uint32_t>(rider+0x868);
                BodyCollisionVolume volume;volume.broadCenterCm=shift(shape+16);volume.broadRadiusCm=m.get<float>(shape+32);volume.activeMask=m.get<uint32_t>(shape+40);volume.count=m.get<uint32_t>(shape+44);
                for(unsigned i=0;i<volume.count;++i){volume.spheres[i].centerCm=shift(shape+48+i*32);volume.spheres[i].radiusCm=m.get<float>(shape+64+i*32);}
                Vector up=m.get<Vector>(rider+0x180);
                ContactCache cache;if(cacheAt&&m.get<uint32_t>(cacheAt)&&m.get<uint32_t>(cacheAt+12)==1){cache.valid=true;cache.resource=m.get<uint32_t>(m.get<uint32_t>(cacheAt)+0x150);cache.cellU=m.get<uint16_t>(cacheAt+4);cache.cellV=m.get<uint16_t>(cacheAt+6);cache.half=m.get<uint32_t>(cacheAt+8);}
                world->resetSphereTreeCache();auto actual=world->query(volume,up,nullptr,true,2,&cache);if(!actual.complete()){fprintf(stderr,"native body incomplete\n");return 3;}
                m.call(0x32f650,{query,shape,0},{},0x70000);m.put(query+8,1u);m.put(query+16,up);m.put(query+28,0.f);
                auto c=m.call(0x3342d0,{m.get<uint32_t>(rider+0x860),query,output,cacheAt},{},0x70000);
                if(cacheAt){uint32_t pointer=m.get<uint32_t>(cacheAt);bool valid=pointer&&m.get<uint32_t>(cacheAt+12)==1;
                    if(cache.valid!=valid||(valid&&(cache.resource!=m.get<uint32_t>(pointer+0x150)||cache.cellU!=m.get<uint16_t>(cacheAt+4)||cache.cellV!=m.get<uint16_t>(cacheAt+6)||cache.half!=m.get<uint32_t>(cacheAt+8)))){
                        ++cacheOrder;if(getenv("CACHE_TRACE"))fprintf(stderr,"body cache order target %zu native %x original %x\n",n,cache.resource,pointer?m.get<uint32_t>(pointer+0x150):0);}}
                if(c.f[0]<0){if(actual.best.hit){fprintf(stderr,"body target %zu: native hit on original miss\n",n);return 4;}++body;continue;}
                Vector point=m.get<Vector>(output),normal=m.get<Vector>(output+16),velocity=m.get<Vector>(output+32);
                uint32_t patch=m.get<uint32_t>(output+0x54),instance=m.get<uint32_t>(output+0x50);if(patch)patch=m.get<uint32_t>(patch+0x150);if(instance)instance=m.get<uint32_t>(instance+0x78);int surface=int(m.get<uint32_t>(output+0x4c));
                const auto& a=actual.best;
                if(!a.hit||a.penetrationCm!=c.f[0]||a.pointCm!=point||a.normal!=normal||a.surfaceVelocityCmps!=velocity||a.surface!=surface||a.terrain!=bool(patch)||a.instance!=(patch?patch:instance)){
                    fprintf(stderr,"body mismatch target %zu depth %a/%a resource %x/%x\n",n,a.penetrationCm,c.f[0],a.instance,patch?patch:instance);return 1;}
                ++body;++bodyHits;connectorHits+=patch&&(patch&255)!=8;
            }
        }
    }
    printf("rider scope live: %u landing probes (%u contacts) and %u body queries (%u contacts) through a refreshed 0x332DB8 scope match exactly; %u contacts on connector patches; %u queries leave another overlapping patch in the contact cache (patch visit order)\n",landing,landingHits,body,bodyHits,connectorHits,cacheOrder);
}
