// Live MagnetModifier parity (development oracle; tools/test_magnet_modifier_live.py).
// engine/magnet_modifier.hpp against the full recompiled original on The Junction (BHP1)
// pipe-run savestates, for every live pickup magnet (vtable 0x48F420 at entity+0x1C):
//  1. Lockstep with the real contact chain: per tick the human rider's +0x110 is set from
//     a trajectory (the PS2 pipe-finish capture, rider+0x110 per record; shifted past each
//     pickup, with and without per-tick jitter), machine A runs the pickup entity's update
//     0x356198 (container 0x352C70 -> 0x3573F8) and 0x3568B0, the port originalMagnetTick.
//     While the rider position is inside the magnet bounds (the contact emulation shared by
//     both sides) A runs the entity contact 0x355770 (rider+0xA60, +0x9E0 with +0x50 =
//     instance, +0x6C0), i.e. gate 0x357660 and, when it passes, the real slot-2 program
//     (0x34FE00 -> 0x30A060: points, halo removal, Debounce); the port runs
//     originalMagnetGate and the entity+0x20 timer rule. Compared per tick: all 0xB0 bytes,
//     entity+0x20, the gate result and the award tick. After the award: the Debounce
//     conversion (0x355F10) PositionModifier vs originalMagnetFreeze, then the new entity
//     is ticked (its vt+0x14/+0x194) and its later class / instance flags are reported.
//  2. Port-only timeline of the real pointa_1001 case (resource 125711) on the capture:
//     acquisition tick -> reached tick.
// Args: FOLDER TRAJECTORY TICKS STATE...   (STATE = prefix of .ee/.vuc/.vud in FOLDER)
#include "live_registrations.inc"
#include "original_live_runtime.hpp"
#include "../engine/magnet_modifier.hpp"
#include <map>
#include <random>
using namespace ssx;
static constexpr uint32_t GP=0x4A30F0;
static uint32_t bits(float x){return std::bit_cast<uint32_t>(x);}
struct Live {uint32_t modifier,entity,instance,resource;};
static std::vector<Live> findMagnets(const live::Machine& R){
    std::vector<Live> out;
    for(uint32_t a=0x100000;a<0x2000000;a+=16)if(R.get<uint32_t>(a)==0x48F420u){
        uint32_t inst=R.get<uint32_t>(a+0xA0),e=inst?R.get<uint32_t>(inst+0xC):0,c=e?R.get<uint32_t>(e+0x1C):0;
        if(c&&R.get<uint32_t>(c)==a)out.push_back({a,e,inst,R.get<uint32_t>(inst+0x78)});
    }
    return out;
}
static bool inside(const MagnetQuad& p,const MagnetQuad& lo,const MagnetQuad& hi){
    for(unsigned k=0;k<3;k++)if(!(lo[k]<p[k]&&p[k]<hi[k]))return false;
    return true;
}
int main(int argc,char** argv){
    if(argc<5){fprintf(stderr,"usage: live FOLDER TRAJECTORY TICKS STATE...\n");return 2;}
    const std::string folder=argv[1];const int ticks=atoi(argv[3]);FILE* log=stdout;
    std::map<uint32_t,MagnetQuad> track;
    {auto raw=live::readFile(argv[2]);for(size_t o=0;o+20<=raw.size();o+=20){uint32_t t;MagnetQuad q;std::memcpy(&t,raw.data()+o,4);std::memcpy(q.data(),raw.data()+o+4,16);track[t]=q;}}
    fprintf(log,"trajectory: %zu capture ticks %u..%u\n",track.size(),track.begin()->first,track.rbegin()->first);
    unsigned failures=0,runs=0,exactRuns=0,awards=0,awardsMatched=0,tickTotal=0,freezeOk=0,freezes=0;
    std::map<uint32_t,unsigned> laterClasses;std::map<uint32_t,unsigned> laterFlags;
    std::mt19937 rng(0x357660);
    for(int s=4;s<argc;s++){
        const std::string state=folder+"/"+argv[s];
        live::Machine A(state+".ee",state+".vuc",state+".vud");
        const std::vector<uint8_t> pristine=A.ee;
        const uint32_t pm=A.get<uint32_t>(GP-0x848),list=A.get<uint32_t>(A.get<uint32_t>(pm+0x84)+0xC),rider=A.get<uint32_t>(list+0x28);
        const uint32_t tick0=A.get<uint32_t>(list+8);
        const bool human=A.get<uint32_t>(rider+0x874)!=0;const int32_t index=A.get<int32_t>(rider+0x86C);
        auto magnets=findMagnets(A);
        fprintf(log,"%s (race tick %u): %zu magnets, rider %x index %d human %d\n",argv[s],tick0,magnets.size(),rider,index,human);
        for(const Live& L:magnets){
            // Closest capture tick to this pickup (translation of the trajectory).
            const MagnetQuad centre=A.get<MagnetQuad>(L.modifier+0x10);
            uint32_t closest=track.begin()->first;float best=1e30f;
            for(auto& [t,q]:track){float d=0;for(unsigned k=0;k<3;k++)d+=(q[k]-centre[k])*(q[k]-centre[k]);if(d<best){best=d;closest=t;}}
            for(unsigned kind=0;kind<3;kind++){
                A.ee=pristine;runs++;
                // kind 0: the raw capture window around capture tick 2800 (pointa_1001 is on it);
                // kind 1: the capture shifted so that its closest approach passes the pickup at an
                //         offset; kind 2: kind 1 with per-tick jitter.
                uint32_t start=kind==0?2780u:(closest>track.begin()->first+90?closest-90:track.begin()->first);
                MagnetQuad shift{0,0,0,0};
                if(kind){const MagnetQuad& q=track[closest];for(unsigned k=0;k<3;k++)shift[k]=centre[k]-q[k]+std::uniform_real_distribution<float>(-250,250)(rng);}
                auto m=originalMagnetModifierFromBytes(A.ee.data()+L.modifier);
                int32_t timer=A.get<int32_t>(L.entity+0x20);
                unsigned exact=0,firstBad=0;bool bad=false;int acquiredAt=-1,portAward=-1,originalAward=-1;
                const int32_t score0=A.get<int32_t>(A.get<uint32_t>(rider+0x790)+0x198);
                OriginalPositionModifier frozen;
                for(int k=0;k<ticks;k++){
                    auto it=track.find(start+uint32_t(k));if(it==track.end())break;
                    MagnetQuad p=it->second;
                    if(kind){OriginalRounding rounding;for(unsigned c=0;c<3;c++){float d=shift[c];if(kind==2)d=terrain_original::add(d,std::uniform_real_distribution<float>(-40,40)(rng));p[c]=terrain_original::add(p[c],d);}}
                    A.put(rider+0x110,p);
                    if(originalAward<0){
                        A.call(0x356198,{L.entity});A.call(0x3568b0,{L.entity});
                        if(timer>0)timer--;
                        originalMagnetTick(m,p);
                        auto bytes=originalMagnetModifierBytes(m);
                        bool same=!std::memcmp(A.ee.data()+L.modifier,bytes.data(),bytes.size())&&A.get<int32_t>(L.entity+0x20)==timer;
                        // Contact emulation: rider+0x110 inside the (fresh) bounds.
                        if(inside(p,m.boundsMin,m.boundsMax)){
                            A.put(rider+0x9E0+0x50,L.instance);
                            A.call(0x355770,{L.entity,rider+0xA60,rider+0x9E0,rider+0x6C0});
                            const bool wasTarget=m.target>=0;
                            const uint32_t gate=originalMagnetGate(m,index,human);
                            if(!wasTarget&&m.target>=0)acquiredAt=k;
                            bool portRuns=gate&&timer<=0;
                            if(portRuns){portAward=k;timer=30;frozen=originalMagnetFreeze(m);}
                            const bool converted=A.get<uint32_t>(L.instance+0xC)!=L.entity||A.get<uint32_t>(L.entity+0xC)!=0x490E80u;
                            if(converted)originalAward=k;
                            same&=converted==portRuns;
                            if(!converted){bytes=originalMagnetModifierBytes(m);same&=!std::memcmp(A.ee.data()+L.modifier,bytes.data(),bytes.size());}
                        }
                        if(same)exact++;else if(!bad){bad=true;firstBad=unsigned(k);
                            fprintf(log,"  %x kind %u tick %d mismatch (timer %d/%d)\n",L.resource,kind,k,A.get<int32_t>(L.entity+0x20),timer);
                            auto b=originalMagnetModifierBytes(m);
                            for(size_t o=0;o<0xB0;o+=4){uint32_t x,y;std::memcpy(&x,A.ee.data()+L.modifier+o,4);std::memcpy(&y,b.data()+o,4);if(x!=y)fprintf(log,"    +%03zx original %08x port %08x\n",o,x,y);}
                        }
                        tickTotal++;
                        if(originalAward>=0){
                            // 0x355F10: the new entity carries a PositionModifier at the magnet matrix.
                            freezes++;const uint32_t e2=A.get<uint32_t>(L.instance+0xC),c2=e2?A.get<uint32_t>(e2+0x1C):0,p2=c2?A.get<uint32_t>(c2):0;
                            bool fok=p2&&A.get<uint32_t>(p2)==0x48F5F0u;
                            if(fok){auto want=originalPositionModifierBytes(frozen);for(uint32_t o:{0x30u,0x50u,0x60u,0x70u,0x80u})fok&=!std::memcmp(A.ee.data()+p2+o,want.data()+o,16);}
                            freezeOk+=fok;
                            if(!fok)fprintf(log,"  %x kind %u: conversion entity %x (vt %x) modifier %x\n",L.resource,kind,e2,e2?A.get<uint32_t>(e2+0xC):0,p2);
                        }
                    }else{
                        // After the award: tick whatever entity the instance now has (vt+0x14, +0x194).
                        const uint32_t e2=A.get<uint32_t>(L.instance+0xC);
                        if(e2){const uint32_t vt=A.get<uint32_t>(e2+0xC);
                            A.call(A.get<uint32_t>(vt+0x14),{e2+uint32_t(int16_t(A.get<uint16_t>(vt+0x10)))});
                            if(A.get<uint32_t>(L.instance+0xC)==e2)A.call(A.get<uint32_t>(vt+0x194),{e2+uint32_t(int16_t(A.get<uint16_t>(vt+0x190)))});}
                    }
                }
                const uint32_t e3=A.get<uint32_t>(L.instance+0xC);
                if(originalAward>=0){awards++;laterClasses[e3?A.get<uint32_t>(e3+0xC):0]++;laterFlags[A.get<uint32_t>(L.instance+8)]++;}
                awardsMatched+=originalAward==portAward;
                const int32_t score1=A.get<int32_t>(A.get<uint32_t>(rider+0x790)+0x198);
                if(!bad&&originalAward==portAward)exactRuns++;else failures++;
                if(kind==0&&L.resource!=125711u&&acquiredAt<0)continue;
                fprintf(log,"  %6u %-5s ticks exact %u%s acquired %d award %d/%d (score %+d)\n",L.resource,kind==0?"raw":kind==1?"shift":"jitter",
                    exact,bad?" MISMATCH":"",acquiredAt,originalAward,portAward,score1-score0);
                (void)firstBad;
            }
        }
    }
    fprintf(log,"lockstep: %u/%u runs exact (%u ticks), awards %u (original == port award tick in %u runs), 0x355F10 PositionModifier == originalMagnetFreeze %u/%u\n",
        exactRuns,runs,tickTotal,awards,awardsMatched,freezeOk,freezes);
    fprintf(log,"after the award (+%d ticks): entity classes",ticks);for(auto& [vt,n]:laterClasses)fprintf(log," %x x%u",vt,n);
    fprintf(log,", instance flags");for(auto& [f,n]:laterFlags)fprintf(log," %x x%u",f,n);fprintf(log,"\n");
    // ---- 2. port-only timeline of pointa_1001 on the capture ----------------------------------
    {
        live::Machine A(folder+"/"+argv[4]+".ee",folder+"/"+argv[4]+".vuc",folder+"/"+argv[4]+".vud");
        for(const Live& L:findMagnets(A))if(L.resource==125711u){
            const auto base=originalMagnetModifierFromBytes(A.ee.data()+L.modifier);
            int entered=-1;
            for(uint32_t t=2800;t<2900&&entered<0;t++){auto m=base;originalMagnetEntityBounds(m);if(inside(track[t],m.boundsMin,m.boundsMax))entered=int(t);}
            fprintf(log,"pointa_1001 timeline on pipe-finish: rider+0x110 enters the static 800 cm box at capture tick %d; acquisition tick -> reached tick:",entered);
            for(uint32_t a=2836;a<=2866;a+=2){
                auto m=base;originalMagnetGate(m,0,true);int reached=-1;
                for(uint32_t t=a+1;t<a+60&&reached<0;t++){originalMagnetTick(m,track[t]);if(m.reached)reached=int(t);}
                fprintf(log," %u->%d",a,reached);
            }
            fprintf(log,"\n");
        }
    }
    fprintf(log,"original entries called:");for(auto& [pc,n]:callCounts)if(n)fprintf(log," %x:%llu",pc,(unsigned long long)n);fprintf(log,"\n");
    fprintf(log,failures?"FAILED (%u runs)\n":"all live runs match\n",failures);
    return failures?1:0;
}
