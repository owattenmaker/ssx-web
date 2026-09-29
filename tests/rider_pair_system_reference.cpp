#include "ps2_runtime_macros.h"
#include "../engine/rider_pair_system.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <random>
void sub_00107888_0x107888(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00107E70_0x107e70(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00329F98_0x329f98(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00106538_0x106538(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00329B40_0x329b40(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010F560_0x10f560(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C228_0x31c228(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x340000;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=(0x340000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x340000-0x100000)/4]={};
using namespace ssx;using namespace ssx::terrain_original;
struct Actor {OriginalPairActorView view;BodyCollisionVolume body;Vector offset{},low{},high{};};
struct Event {unsigned target,other;OriginalPairReactionRequest request;};
static std::array<Actor,6> seed,native;
static std::vector<Event> actualEvents,nativeEvents;
static std::mt19937 originalRng,nativeRng;
static unsigned originalResets,nativeResets;
static int tick;
static uint32_t address(unsigned i){return 0x10000+i*0x1000;}
static unsigned index(uint32_t p){if(p<0x10000||p>=0x16000)throw std::runtime_error("Unexpected original actor");return (p-0x10000)/0x1000;}
template<class T> static T read(uint8_t* m,uint32_t p){T value;std::memcpy(&value,m+p,sizeof(T));return value;}
template<class T> static void write(uint8_t* m,uint32_t p,T value){std::memcpy(m+p,&value,sizeof(T));}
static void vector(uint8_t* m,uint32_t p,Vector value){write(m,p,value);write(m,p+12,0.f);}
static void external(uint8_t* m,R5900Context* c,PS2Runtime*) {
    uint32_t pc=c->pc,a=GPR_U32(c,4);unsigned n=0;
    switch(pc) {
    case 0x11ff98:n=index(a);c->f[0]=originalRiderCollisionWeight(seed[n].view.weightAttribute,seed[n].view.resolvedCollisionStat,read<float>(m,a+0x2fc));break;
    case 0x11fee8:n=index(a);SET_GPR_U32(c,2,read<int>(m,0x90000+n*0x1000+0xde4));break;
    case 0x11fe98:n=index(a);SET_GPR_U32(c,2,seed[n].view.impulse.motionMode);break;
    case 0x1231a8:n=index(a);SET_GPR_U32(c,2,originalPairGrounded(seed[n].view.impulse.motionMode,seed[n].view.impulse.ragdollSubmode));break;
    case 0x1298c8:SET_GPR_U32(c,2,tick);break;
    case 0x311ae8:n=(a-0x80000)/0x1000;SET_GPR_U32(c,2,seed[n].view.attack.animationClass1);break;
    case 0x311b20:SET_GPR_U32(c,2,a+0x100);break;
    case 0x1446a0:n=(a-0x801b0)/0x1000;{bool marker=GPR_U32(c,5)?seed[n].view.attack.marker1:seed[n].view.attack.marker0;SET_GPR_U32(c,2,marker);}break;
    case 0x149038:n=GPR_U32(c,5);c->f[0]=seed[n].view.attack.resolvedAttackStat;break;
    case 0x1135b8:++originalResets;break;
    case 0x33fff0:SET_GPR_U32(c,2,a-0x6c0+0x110);break;
    case 0x33ffe0:SET_GPR_U32(c,2,a-0x6c0+0x1e0);break;
    case 0x317810:SET_GPR_U32(c,2,originalRng());break;
    case 0x10eb30:case 0x108388:{
        Event event;event.target=index(a);event.other=index(GPR_U32(c,20));auto& request=event.request;
        request.kind=pc==0x10eb30?OriginalPairReactionRequest::Kind::Crash:OriginalPairReactionRequest::Kind::Soft;
        request.animation=pc==0x10eb30?int(GPR_U32(c,5)):-1;request.attack=bool(GPR_U32(c,21));
        unsigned reg=pc==0x10eb30?8:5;uint32_t p=GPR_U32(c,reg);
        request.event.pointCm=read<Vector>(m,p);request.event.incomingDirection=read<Vector>(m,p+16);request.event.normal=read<Vector>(m,p+32);request.event.closingSpeedCmps=read<float>(m,p+48);
        actualEvents.push_back(event);
        // Controlled lifecycle boundary mutation checks fresh reads and cached
        // owner weight; this is not a substitute for native crash simulation.
        write(m,a+0x2fc,0.f);
        break;}
    default:SET_GPR_U32(c,2,0);break;
    }
    c->pc=GPR_U32(c,31);
}
int main(int argc,char** argv) {
    if(argc!=2)return 1;PS2Runtime runtime;std::vector<uint8_t> memory(32*1024*1024);auto* m=memory.data();std::ifstream ee(argv[1],std::ios::binary);ee.read((char*)m,memory.size());if(!ee)return 2;
    for(uint32_t pc:{0x11ff98,0x11fee8,0x11fe98,0x1231a8,0x1298c8,0x311ae8,0x311b20,0x1446a0,0x149038,0x1135b8,0x33fff0,0x33ffe0,0x317810,0x10eb30,0x108388,0x10e468,0x10e2e8,0x10e3a8,0x10e228,0x14dc80,0x14dd58,0x10f998,0x10f878,0x155a50,0x155b50})runtime.registerFunction(pc,external);
    runtime.registerFunction(0x107e70,sub_00107E70_0x107e70);runtime.registerFunction(0x329f98,sub_00329F98_0x329f98);runtime.registerFunction(0x106538,sub_00106538_0x106538);runtime.registerFunction(0x329b40,sub_00329B40_0x329b40);runtime.registerFunction(0x31c228,sub_0031C228_0x31c228);
    write(m,0x4a30f0-0x848,0x50000u);write(m,0x50084,0x51000u);write(m,0x5100c,0x52000u);
    std::mt19937 random(0x53595354);std::uniform_real_distribution<float> coord(-40,40),speed(-2500,2500),unit(0,1);
    unsigned checks=0,separations=0,impulses=0,attacks=0,reactions=0,frames=0;
    for(unsigned scenario=0;scenario<2000;++scenario) {
        unsigned count=2+random()%5,excludedTail=random()%3==0?1:0;seed={};native={};actualEvents.clear();nativeEvents.clear();originalResets=nativeResets=0;uint32_t rngSeed=random();originalRng.seed(rngSeed);nativeRng.seed(rngSeed);
        write(m,0x52078,count);write(m,0x52084,excludedTail);write(m,0x5308d0,0u);
        for(unsigned n=0;n<count;++n) {
            auto& x=seed[n];auto& v=x.view;v.slot=n;v.kind880=(n==0||random()%3==0)?7:8;v.disabled=random()%30==0;v.weightAttribute=50+random()%50;v.resolvedCollisionStat=.25f;v.boost=unit(random);
            v.attack.positionCm={float(n*55)+coord(random),coord(random),coord(random)};v.impulse.velocityCmps={speed(random),speed(random),speed(random)};v.impulse.motionMode=random()%3;v.impulse.ragdollSubmode=random()%2;v.impulse.controlState=random()%10;
            v.reaction.motionMode=v.impulse.motionMode;v.reaction.controlState=v.impulse.controlState;v.reaction.presentation.origin=v.attack.positionCm;
            v.attack.animationClass1=random()%4==0?13:0;v.attack.marker0=true;v.attack.marker1=false;v.attack.facing340={1,0,0};v.attack.strength350=.5f;v.attack.resolvedAttackStat=.25f;
            x.body.broadCenterCm=v.attack.positionCm;x.body.broadRadiusCm=100;x.body.count=10;x.body.activeMask=random();x.body.reactionFrame=v.reaction.presentation;x.body.landingCenterCm=v.attack.positionCm;
            for(unsigned j=0;j<10;++j)x.body.spheres[j]={{add(v.attack.positionCm[0],coord(random)),add(v.attack.positionCm[1],coord(random)),add(v.attack.positionCm[2],coord(random))},25,j};
            uint32_t a=address(n),b=0x60000+n*0x1000,owner=0x90000+n*0x1000;std::memset(m+a,0,0x1000);std::memset(m+b,0,0x1000);
            vector(m,a+0x110,v.attack.positionCm);vector(m,a+0x1e0,v.impulse.velocityCmps);vector(m,a+0x370,v.impulse.groundNormal);vector(m,a+0x1c0,v.impulse.physicalUp);vector(m,a+0x160,v.reaction.presentation.right);vector(m,a+0x170,v.reaction.presentation.forward);vector(m,a+0x180,v.reaction.presentation.up);vector(m,a+0x190,v.reaction.presentation.origin);vector(m,a+0x340,v.attack.facing340);write(m,a+0x350,v.attack.strength350);
            write(m,a+0x2fc,v.boost);write(m,a+0x86c,n);write(m,a+0x878,uint32_t(v.disabled));write(m,a+0xaa0,b);write(m,a+0x77c,owner);write(m,owner+0x30,v.impulse.ragdollSubmode);write(m,owner+0xde4,v.impulse.controlState);write(m,a+0x784,0x80000+n*0x1000);write(m,a+0x788,0xb0000+n*0x1000);
            write(m,a+0x6c0,0x53000u);write(m,0x53028,int16_t(0));write(m,0x5302c,0x33fff0u);write(m,0x53010,int16_t(0));write(m,0x53014,0x33ffe0u);write(m,0x52028+n*4,a);
            vector(m,b+16,x.body.broadCenterCm);write(m,b+32,x.body.broadRadiusCm);write(m,b+40,x.body.activeMask);write(m,b+44,x.body.count);for(unsigned j=0;j<10;++j){vector(m,b+48+j*32,x.body.spheres[j].centerCm);write(m,b+64+j*32,25.f);}
            for(unsigned j=0;j<6;++j){write(m,a+j*36,uint32_t(v.kind880==7&&j<count&&j!=n));write(m,a+j*36+8,10000000000.f);}
            native[n]=x;
        }
        OriginalPairCallbacks cb;
        cb.liveView=[&](unsigned n){auto v=native[n].view;v.body=&native[n].body;v.attack.velocityCmps=v.impulse.velocityCmps;v.reaction.velocityCmps=v.impulse.velocityCmps;return v;};
        cb.translate=[&](unsigned n,Vector delta){auto& x=native[n];for(unsigned k=0;k<3;++k){x.view.attack.positionCm[k]=add(x.view.attack.positionCm[k],delta[k]);x.offset[k]=add(x.offset[k],delta[k]);x.low[k]=add(x.low[k],delta[k]);x.high[k]=add(x.high[k],delta[k]);}translateOriginalPairBody(x.body,delta);};
        cb.setVelocity=[&](unsigned n,Vector velocity,bool reset){native[n].view.impulse.velocityCmps=velocity;nativeResets+=reset;};
        cb.randomWord=[&](){return nativeRng();};
        cb.react=[&](unsigned a,unsigned b,const OriginalPairReactionRequest& request,const CollisionRandom&){nativeEvents.push_back({a,b,request});native[a].view.boost=0;};
        OriginalRiderPairSystem system(count,cb,excludedTail);
        if(scenario%3==0){auto records=system.records();for(unsigned n=0;n<count;++n)for(unsigned j=0;j<count;++j){auto& r=records[n][j];r.lastContactTick=598;r.lastCheckedTick=scenario%2?600:599;r.lastAttackTick=598;uint32_t p=address(n)+j*36;write(m,p+16,r.lastContactTick);write(m,p+20,r.lastCheckedTick);write(m,p+24,r.lastAttackTick);}system.seedRecords(records);}
        auto execute=[&](uint32_t pc,uint32_t a){R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,a);SET_GPR_U32(&c,29,0x30000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,0x12345678);if(pc==0x107888)sub_00107888_0x107888(m,&c,&runtime);else sub_0010F560_0x10f560(m,&c,&runtime);if(c.pc!=0x12345678)throw std::runtime_error("Original dispatcher did not return");};
        for(tick=600;tick<612;++tick) {
            Rounding rounding;write(m,0x52008,tick);execute(0x10f560,0x52000);system.refreshProximity(tick);
            for(unsigned n=0;n<count;++n){execute(0x107888,address(n));auto c=system.resolveActor(n,tick);checks+=c.checks;separations+=c.separations;impulses+=c.impulses;attacks+=c.attacks;reactions+=c.reactions;}
            ++frames;
            for(unsigned n=0;n<count;++n){auto& x=native[n];uint32_t a=address(n),b=0x60000+n*0x1000;
                if(read<Vector>(m,a+0x110)!=x.view.attack.positionCm||read<Vector>(m,a+0x1e0)!=x.view.impulse.velocityCmps||read<Vector>(m,b+16)!=x.body.broadCenterCm||read<Vector>(m,a+0x9d0)!=x.offset||read<Vector>(m,a+0x400)!=x.low||read<Vector>(m,a+0x410)!=x.high)throw std::runtime_error("Shared pair state/AA0 mismatch scenario "+std::to_string(scenario)+" tick "+std::to_string(tick));
                if(x.body.reactionFrame->origin!=seed[n].body.reactionFrame->origin||x.body.landingCenterCm!=seed[n].body.landingCenterCm)throw std::runtime_error("Pair translation incorrectly changed stale geometry metadata");
                for(unsigned j=0;j<10;++j)if(read<Vector>(m,b+48+j*32)!=x.body.spheres[j].centerCm)throw std::runtime_error("Shared pair child sphere mismatch");
                for(unsigned j=0;j<6;++j){auto& r=system.records()[n][j];uint32_t p=a+j*36;if(read<float>(m,p+8)!=r.planarDistanceCm||read<float>(m,p+12)!=r.bearing||read<int>(m,p+16)!=r.lastContactTick||read<int>(m,p+20)!=r.lastCheckedTick||read<int>(m,p+24)!=r.lastAttackTick)throw std::runtime_error("Shared pair record mismatch");}
            }
            if(actualEvents.size()!=nativeEvents.size()||originalResets!=nativeResets||originalRng!=nativeRng)throw std::runtime_error("Shared pair event/reset count mismatch");
            for(unsigned j=0;j<actualEvents.size();++j){auto& a=actualEvents[j];auto& b=nativeEvents[j];auto& x=a.request;auto& y=b.request;if(a.target!=b.target||a.other!=b.other||x.kind!=y.kind||x.animation!=y.animation||x.attack!=y.attack||x.event.pointCm!=y.event.pointCm||x.event.normal!=y.event.normal||x.event.incomingDirection!=y.event.incomingDirection||x.event.closingSpeedCmps!=y.event.closingSpeedCmps)throw std::runtime_error("Shared pair ordered reaction mismatch");}
        }
    }
    std::cout<<frames<<" shared original/native frames match exactly across 2000 rosters: "<<checks<<" queries, "<<separations<<" separations, "<<impulses<<" impulses, "<<attacks<<" attacks, "<<reactions<<" ordered reactions; AA0, stale metadata, reciprocal records and shared RNG order match\n";
}
