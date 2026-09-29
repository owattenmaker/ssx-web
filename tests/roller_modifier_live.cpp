// Live RollerModifier parity (development oracle): engine/roller_modifier.hpp
// against the full recompiled original on the carve-bag savestates.
//  1. Lockstep from tick 607: machine A runs the original 0x35E850 + 0x3568B0 for
//     both crashbags; machine B hosts the port, whose world query callback runs
//     the ORIGINAL 0x3303F0/0x2D1BE0/0x336850/0x3304E8 on B after writing the
//     port's modifier bytes there (the result, the +0x2C0 cache and the collider
//     scratch are read back), and whose relocation is the original 0x3291E0 with
//     the port's bounds. Every modifier byte is compared per tick with A and with
//     the capture records (local/ps2-capture/runs/bag/carve-bag.bin).
//  2. Construction from tick 600 (bags static): original 0x35DA70 (+0x350570
//     attach bounds/radius) vs originalRollerConstruct/originalRollerAttach with
//     synthetic records, and the exact tick-606 construction of bag 0003 (the
//     tick-607 savestate still holds its rider packet/record) vs the 607 image.
#include "live_registrations.inc"
#include "original_live_runtime.hpp"
#include "../engine/roller_modifier.hpp"
#include <random>
using namespace ssx;
struct Bag {uint32_t entity,modifier,instance;const char* name;};
static constexpr uint32_t arena=0xE0000; // free capture arena (query object, packet, bounds)
static CollisionSphereTree readTree(const live::Machine& m,uint32_t header){
    CollisionSphereTree tree;uint32_t depth=m.get<uint32_t>(header+0xC),levels=m.get<uint32_t>(header+0x20);
    tree.compressed=m.get<uint32_t>(header+8)!=0;tree.centerCm={m.get<float>(header+0x14),m.get<float>(header+0x18),m.get<float>(header+0x1C)};
    for(uint32_t i=0;i<=depth;i++)tree.levels.push_back({m.get<float>(levels+12*i),m.get<float>(levels+12*i+4),m.get<uint32_t>(levels+12*i+8)});
    return tree;
}
static std::array<uint8_t,0x2D0> image(const live::Machine& m,uint32_t at){std::array<uint8_t,0x2D0> b;std::memcpy(b.data(),m.ee.data()+at,b.size());return b;}
static unsigned diff(const uint8_t* a,const uint8_t* b,size_t n,FILE* log=nullptr,const char* what=""){
    unsigned count=0;
    for(size_t o=0;o<n;o+=4)if(std::memcmp(a+o,b+o,4)){
        if(log&&count<6){uint32_t x,y;std::memcpy(&x,a+o,4);std::memcpy(&y,b+o,4);fprintf(log,"    %s +%03zx %08x %08x\n",what,o,x,y);}
        count++;
    }
    return count;
}
int main(int argc,char** argv){
    if(argc<4){fprintf(stderr,"usage: live FOLDER CAPTURE TICKS\n");return 2;}
    std::string folder=argv[1];int ticks=atoi(argv[3]);FILE* log=stdout;
    auto capture=live::readFile(argv[2]);
    std::map<uint32_t,size_t> records;for(size_t k=0;k+16384<=capture.size();k+=16384){uint32_t t;std::memcpy(&t,capture.data()+k+4,4);records[t]=k;}
    const Bag bags[2]={{0x56FE40,0x1CEF550,0xF46E80,"0002"},{0x56FB40,0x1CEF830,0xFCF9C0,"0003"}};
    // ---- 1. lockstep ----------------------------------------------------------
    {
        live::Machine A(folder+"/607.ee",folder+"/607.vuc",folder+"/607.vud"),B(folder+"/607.ee",folder+"/607.vuc",folder+"/607.vud");
        CollisionSphereTree trees[2];OriginalRollerModifier port[2];
        for(unsigned i=0;i<2;i++){
            uint32_t container=A.get<uint32_t>(bags[i].entity+0x1C);
            if(!container||A.get<uint32_t>(container)!=bags[i].modifier||A.get<uint32_t>(bags[i].entity+0x18)!=bags[i].instance)throw std::runtime_error("entity layout");
            trees[i]=readTree(B,bags[i].modifier+0x1A0);
            port[i]=originalRollerModifierFromBytes(B.ee.data()+bags[i].modifier,&trees[i]);
        }
        unsigned tickMatch[2]={},byteMismatch[2]={},queries=0,hits=0,headerWrites=0,paths[4]={};
        unsigned captureMatch[2][2]={},captureCompared[2][2]={},worldMismatch=0;int firstCaptureMiss[2][2]={{-1,-1},{-1,-1}};
        for(int t=0;t<ticks;t++){
            for(unsigned i=0;i<2;i++){
                const Bag& bag=bags[i];auto& m=port[i];
                A.call(0x35e850,{bag.modifier});A.call(0x3568b0,{bag.entity});
                auto query=[&](OriginalSphereTreeCollider& collider,OriginalRollerTerrainCache& cache)->OriginalRollerWorldContact{
                    auto bytes=originalRollerModifierBytes(m,bag.modifier);std::memcpy(B.ee.data()+bag.modifier,bytes.data(),bytes.size());
                    const uint32_t object=arena,packet=arena+0x100;
                    B.call(0x3303f0,{object,bag.modifier+0xE0,1});
                    auto w=B.call(0x2d1be0);uint32_t world=GPR_U32((&w),2);
                    auto c=B.call(0x336850,{world,object,packet,bag.modifier+0x2C0});
                    B.call(0x3304e8,{object,2});
                    auto back=originalRollerModifierFromBytes(B.ee.data()+bag.modifier,&trees[i]);
                    const CollisionSphereTree* tree=collider.tree;uint32_t resource=collider.treeResource;
                    collider=back.collider;collider.tree=tree;collider.treeResource=resource;cache=back.cache;
                    if(back.treeHeader!=m.treeHeader){headerWrites++;m.treeHeader=back.treeHeader;}
                    OriginalRollerWorldContact out;out.depth=c.f[0];out.hit=out.depth>=0;
                    std::memcpy(out.point.data(),B.ee.data()+packet,16);std::memcpy(out.normal.data(),B.ee.data()+packet+16,16);
                    queries++;hits+=out.hit;return out;
                };
                auto result=originalRollerUpdate(m,query);paths[int(result.path)]++;
                auto bounds=originalRollerEntityBounds(m);
                {   // 0x3568B0 relocation part on B with the port's bounds (original 0x3291E0)
                    auto bytes=originalRollerModifierBytes(m,bag.modifier);std::memcpy(B.ee.data()+bag.modifier,bytes.data(),bytes.size());
                    B.put(arena+0x200,bounds.min);B.put(arena+0x210,bounds.max);B.put(arena+0x220,bounds.oldMin);B.put(arena+0x230,bounds.oldMax);
                    auto w=B.call(0x2d1be0);uint32_t world=GPR_U32((&w),2);
                    B.call(0x3291e0,{world,0,bag.instance,arena+0x200,arena+0x220});
                }
                auto bytes=originalRollerModifierBytes(m,bag.modifier);auto original=image(A,bag.modifier);
                unsigned d=diff(original.data(),bytes.data(),bytes.size(),log,bag.name);
                if(d){fprintf(log,"  tick %d bag %s: %u modifier words differ from the original\n",607+t,bag.name,d);byteMismatch[i]+=d;}else tickMatch[i]++;
                // capture: record T holds the rollers after tick T's update; try T=607+t and 608+t.
                for(unsigned shift=0;shift<2;shift++){
                    auto r=records.find(uint32_t(607+t+int(shift)));if(r==records.end())continue;
                    const uint8_t* window=capture.data()+r->second+10240;
                    const uint8_t* captured=window+(bag.modifier-0x1CEF540);
                    captureCompared[i][shift]++;
                    if(!diff(captured,bytes.data(),bytes.size()))captureMatch[i][shift]++;else if(firstCaptureMiss[i][shift]<0)firstCaptureMiss[i][shift]=607+t;
                }
            }
            // Whole-machine sync outside the arena: the port-hosted world stays identical.
            if(diff(A.ee.data()+0x100000,B.ee.data()+0x100000,A.ee.size()-0x100000)||diff(A.ee.data(),B.ee.data(),arena))worldMismatch++;
        }
        fprintf(log,"lockstep %d ticks from 607: bag 0002 %u/%d ticks byte-identical, bag 0003 %u/%d (%u/%u differing words); %u original 336850 queries (%u hits), %u header writes by the query\n",
            ticks,tickMatch[0],ticks,tickMatch[1],ticks,byteMismatch[0],byteMismatch[1],queries,hits,headerWrites);
        fprintf(log,"  paths: dynamics %u, rest %u, frozen %u, sinking %u; EE memory outside the scratch arena differs on %u ticks\n",paths[0],paths[1],paths[2],paths[3],worldMismatch);
        for(unsigned shift=0;shift<2;shift++)
            fprintf(log,"  capture (tick %d+k): bag 0002 %u/%u records match (first miss %d), bag 0003 %u/%u (first miss %d)\n",607+int(shift),
                captureMatch[0][shift],captureCompared[0][shift],firstCaptureMiss[0][shift],captureMatch[1][shift],captureCompared[1][shift],firstCaptureMiss[1][shift]);
    }
    // ---- 2. construction from 600 ----------------------------------------------
    {
        live::Machine M(folder+"/600.ee",folder+"/600.vuc",folder+"/600.vud"),S(folder+"/607.ee",folder+"/607.vuc",folder+"/607.vud");
        std::mt19937 rng(0x35DA70);auto uni=[&](float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);};
        constexpr uint32_t args=arena+0x300,packet=arena+0x400,record=arena+0x480,fakeEntity=arena+0x500,out=arena+0x600;
        unsigned matched=0,attachMatched=0,cases=0;
        for(unsigned i=0;i<2;i++){
            const Bag& bag=bags[i];const uint32_t source=0x11F53C0;
            auto stale=image(M,bag.modifier);uint32_t flags=M.get<uint32_t>(bag.instance+8);
            CollisionSphereTree tree=readTree(M,source+0xC0);
            OriginalRollerSource src;std::memcpy(src.corners.data(),M.ee.data()+source,0x80);src.center=M.get<RollerQuad>(source+0x80);
            std::memcpy(src.treeHeader.data(),M.ee.data()+source+0xC0,0x80);src.tree=&tree;src.instance=bag.instance;
            std::memcpy(src.instanceMatrix.data(),M.ee.data()+bag.instance+0x10,0x40);
            for(unsigned n=0;n<2000;n++,cases++){
                std::memcpy(M.ee.data()+bag.modifier,stale.data(),stale.size());M.put(bag.instance+8,flags);
                OriginalRollerScriptArgs a{n%3?1.f:uni(.3f,4),.699999988079071f};
                M.put(args,uint32_t(0xffffffffu));M.put(args+4,a.mass);M.put(args+8,a.parameter);M.put(args+12,.5f);
                for(uint32_t o=0;o<128;o+=4)M.put(packet+o,uint32_t(rng()));M.put(packet+0x48,source);M.put(packet+0x50,bag.instance);
                OriginalRollerContactRecord r;r.point=src.instanceMatrix[3];
                RollerQuad d{uni(-1,1),uni(-1,1),uni(-1,1),0};float l=std::sqrt(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]);for(unsigned k=0;k<3;k++)d[k]/=l;r.direction=d;
                RollerQuad nn{uni(-1,1),uni(-1,1),uni(-1,1),0};l=std::sqrt(nn[0]*nn[0]+nn[1]*nn[1]+nn[2]*nn[2]);for(unsigned k=0;k<3;k++)nn[k]/=l;r.normal=nn;
                r.closingSpeed=uni(0,2500);
                M.put(record,r.point);M.put(record+16,r.direction);M.put(record+32,r.normal);M.put(record+48,r.closingSpeed);
                M.call(0x35da70,{bag.modifier,args,packet,record});
                OriginalRollerModifier port=originalRollerModifierFromBytes(stale.data(),&tree);
                uint32_t edited=originalRollerConstruct(port,a,src,flags,r);
                auto bytes=originalRollerModifierBytes(port,bag.modifier);
                if(!diff(M.ee.data()+bag.modifier,bytes.data(),bytes.size(),n<2?log:nullptr,bag.name)&&M.get<uint32_t>(bag.instance+8)==edited)matched++;
                // attach: original 0x350570 on a stand-in entity (+0x18 = instance)
                M.put(fakeEntity+0x18,bag.instance);M.call(0x350570,{fakeEntity,out,out+0x20});
                RollerQuad low=M.get<RollerQuad>(bag.instance+0x60),high=M.get<RollerQuad>(bag.instance+0x6C);
                originalRollerAttach(port,M.get<RollerQuad>(bag.instance+0x40),{low[0],low[1],low[2]},{high[0],high[1],high[2]},edited);
                if(!std::memcmp(M.ee.data()+out,port.boundsMin.data(),16)&&!std::memcmp(M.ee.data()+out+16,port.boundsMax.data(),16)&&M.get<float>(out+0x20)==port.radius&&
                   std::bit_cast<uint32_t>(M.get<float>(out+0x20))==std::bit_cast<uint32_t>(port.radius))attachMatched++;
            }
        }
        fprintf(log,"construction from 600: %u/%u original 35DA70 runs byte-identical (both crashbag instances, synthetic records/masses), %u/%u 350570 attach bounds/radius identical\n",
            matched,cases,attachMatched,cases);
        // Exact tick-606 construction of bag 0003: stale allocation from 600, source collider,
        // rider packet (0x1470B80 = rider+0x9E0) and record (+0x80) from the 607 savestate.
        const Bag& bag=bags[1];const uint32_t riderPacket=0x1470B80;uint32_t source=S.get<uint32_t>(riderPacket+0x48);
        if(S.get<uint32_t>(riderPacket+0x50)!=bag.instance)throw std::runtime_error("rider packet");
        CollisionSphereTree tree=readTree(S,source+0xC0);
        OriginalRollerSource src;std::memcpy(src.corners.data(),S.ee.data()+source,0x80);src.center=S.get<RollerQuad>(source+0x80);
        std::memcpy(src.treeHeader.data(),S.ee.data()+source+0xC0,0x80);src.tree=&tree;src.instance=bag.instance;
        std::memcpy(src.instanceMatrix.data(),M.ee.data()+bag.instance+0x10,0x40);
        OriginalRollerContactRecord r;r.point=S.get<RollerQuad>(riderPacket+0x80);r.direction=S.get<RollerQuad>(riderPacket+0x90);
        r.normal=S.get<RollerQuad>(riderPacket+0xA0);r.closingSpeed=S.get<float>(riderPacket+0xB0);
        auto stale=image(M,bag.modifier);
        OriginalRollerModifier port=originalRollerModifierFromBytes(stale.data(),&tree);
        uint32_t flags=originalRollerConstruct(port,{1.f,.699999988079071f},src,M.get<uint32_t>(bag.instance+8),r);
        RollerQuad low=M.get<RollerQuad>(bag.instance+0x60),high=M.get<RollerQuad>(bag.instance+0x6C);
        flags=originalRollerAttach(port,M.get<RollerQuad>(bag.instance+0x40),{low[0],low[1],low[2]},{high[0],high[1],high[2]},flags);
        auto bytes=originalRollerModifierBytes(port,bag.modifier);
        unsigned d=diff(S.ee.data()+bag.modifier,bytes.data(),bytes.size(),log,"606");
        uint32_t flags607=S.get<uint32_t>(bag.instance+8);
        fprintf(log,"tick-606 construction of bag 0003 (stale 600 allocation + 607 rider packet/record, construct + attach) vs the 607 image: %u differing words of 0x2D0; instance flags port %08x | other 607 bits %08x = %08x (607 %08x)\n",
            d,flags,flags607&~flags,flags|(flags607&~flags),flags607);
    }
    if(getenv("COUNTS"))for(auto [pc,n]:callCounts)fprintf(stderr,"%06x %llu\n",pc,(unsigned long long)n);
}
