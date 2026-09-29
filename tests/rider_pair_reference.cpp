#include "ps2_runtime_macros.h"
#include "../engine/rider_pair_collision.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <random>
void sub_00329F98_0x329f98(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00107E70_0x107e70(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011FF98_0x11ff98(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00107888_0x107888(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0010F560_0x10f560(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C228_0x31c228(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x340000;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=(0x340000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x340000-0x100000)/4]={};
static ssx::OriginalPairImpulseState state;
static float magnitude,clamped,stat;
static int weight;
static bool reset;
bool stopPairBeforeReaction=true;
static ssx::OriginalPairReactionRequest actualReaction;
static std::mt19937 reactionRng;
static unsigned randomDraws;
static void reaction(uint8_t* m,R5900Context* c,PS2Runtime*) {
    uint32_t pc=c->pc;
    if(pc==0x317810){SET_GPR_U32(c,2,reactionRng());++randomDraws;}
    else if(pc==0x10eb30||pc==0x108388) {
        actualReaction.kind=pc==0x10eb30?ssx::OriginalPairReactionRequest::Kind::Crash:ssx::OriginalPairReactionRequest::Kind::Soft;
        actualReaction.animation=pc==0x10eb30?int(GPR_U32(c,5)):-1;
        unsigned reg=pc==0x10eb30?8:5;uint32_t p=GPR_U32(c,reg);
        std::memcpy(&actualReaction.event.pointCm,m+p,12);std::memcpy(&actualReaction.event.incomingDirection,m+p+16,12);
        std::memcpy(&actualReaction.event.normal,m+p+32,12);std::memcpy(&actualReaction.event.closingSpeedCmps,m+p+48,4);
    }
    c->pc=GPR_U32(c,31);
}
void capturePairGate(R5900Context* c){magnitude=c->f[21];clamped=c->f[22];}
static void getter(uint8_t*,R5900Context* c,PS2Runtime*) {
    uint32_t result=0;
    switch(c->pc){case 0x11fee8:result=state.controlState;break;case 0x11fe98:result=state.motionMode;break;
    case 0x1231a8:result=ssx::originalPairGrounded(state.motionMode,state.ragdollSubmode);break;
    case 0x1135b8:reset=true;break;case 0x14ef30:result=weight;break;case 0x148f50:c->f[0]=stat;break;}
    SET_GPR_U32(c,2,result);c->pc=GPR_U32(c,31);
}
static int pairTick;
static float pairWeights[2];
static ssx::OriginalPairImpulseState pairStates[2];
static ssx::terrain_original::Vector pairTranslations[2],pairDirection[2];
static float pairImpulses[2];
static unsigned pairMoves,pairCalls;
static bool testingAttack=false;
static ssx::OriginalPairAttackState attackStates[2];
static void pairDispatch(uint8_t* m,R5900Context* c,PS2Runtime*) {
    auto actor=GPR_U32(c,4);unsigned index=actor==0x10000?0:1;
    switch(c->pc) {
    case 0x11ff98:c->f[0]=pairWeights[index];break;
    case 0x33ffe0:SET_GPR_U32(c,2,actor-0x6c0+0x110);break;
    case 0x155a50:case 0x155b50:SET_GPR_U32(c,2,0);break;
    case 0x1231a8:SET_GPR_U32(c,2,ssx::originalPairGrounded(pairStates[index].motionMode,pairStates[index].ragdollSubmode));break;
    case 0x1298c8:SET_GPR_U32(c,2,pairTick);break;
    case 0x311ae8:SET_GPR_U32(c,2,testingAttack?attackStates[actor==0x70000?0:1].animationClass1:0);break;
    case 0x311b20:SET_GPR_U32(c,2,actor+0x100);break;
    case 0x1446a0:{auto& a=attackStates[actor==0x701b0?0:1];SET_GPR_U32(c,2,GPR_U32(c,5)?a.marker1:a.marker0);break;}
    case 0x149038:c->f[0]=attackStates[0].resolvedAttackStat;break;
    case 0x106538:std::memcpy(&pairTranslations[index],m+GPR_U32(c,5),12);++pairMoves;break;
    case 0x107e70:std::memcpy(&pairDirection[index],m+GPR_U32(c,6),12);pairImpulses[index]=c->f[12];if(!testingAttack&&index!=pairCalls)throw std::runtime_error("Pair response order changed");++pairCalls;break;
    }
    c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv) {
    using namespace ssx::terrain_original;PS2Runtime runtime;std::vector<uint8_t> m(32*1024*1024);
    if(argc!=2)return 1;std::ifstream ee(argv[1],std::ios::binary);ee.read((char*)m.data(),m.size());if(!ee)return 2;
    auto write=[&](uint32_t p,const auto& x){std::memcpy(m.data()+p,&x,sizeof(x));};
    auto vec=[&](uint32_t p,Vector x){write(p,x);write(p+12,0.f);};
    auto body=[&](uint32_t p,const ssx::BodyCollisionVolume& b){vec(p+16,b.broadCenterCm);write(p+32,b.broadRadiusCm);write(p+40,b.activeMask);write(p+44,b.count);for(unsigned j=0;j<b.count;++j){vec(p+48+j*32,b.spheres[j].centerCm);write(p+64+j*32,b.spheres[j].radiusCm);}};
    std::mt19937 rng(0x50414952);std::uniform_real_distribution<float> coordinate(-150,150),radius(1,50),speed(-5000,5000),unit(-1,1);
    unsigned hits=0;constexpr uint32_t a=0x10000,b=0x11000,out=0x12000,stack=0x20000,done=0x12345678;
    for(unsigned i=0;i<20000;++i) {
        ssx::BodyCollisionVolume x,y;for(auto* v:{&x,&y}){v->broadRadiusCm=200;v->broadCenterCm={coordinate(rng),coordinate(rng),coordinate(rng)};v->count=i%101?10:0;v->activeMask=rng();for(unsigned j=0;j<v->count;++j)v->spheres[j]={{coordinate(rng),coordinate(rng),coordinate(rng)},radius(rng),j};}
        if(i%50==0){x.broadCenterCm=y.broadCenterCm={};if(x.count)y.spheres[0]=x.spheres[0];}
        body(a,x);body(b,y);R5900Context c{};c.pc=0x329f98;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,a);SET_GPR_U32(&c,5,b);SET_GPR_U32(&c,6,out);SET_GPR_U32(&c,7,out+16);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,31,done);
        Rounding rounding;sub_00329F98_0x329f98(m.data(),&c,&runtime);auto native=ssx::originalBodyPairContact(x,y);
        Vector push,point;std::memcpy(&push,m.data()+out,12);std::memcpy(&point,m.data()+out+16,12);
        if(c.pc!=done||bool(GPR_U32((&c),2))!=native.hit||(native.hit&&(push!=native.penetrationCm||point!=native.pointCm))){std::cerr<<"Pair contact mismatch "<<i<<" hit "<<GPR_U32((&c),2)<<'/'<<native.hit<<'\n';return 3;}hits+=native.hit;
    }
    std::cout<<"20000 original first-sphere pair queries match exactly; "<<hits<<" contacts\n";
    for(uint32_t pc:{0x11fee8,0x11fe98,0x1231a8,0x1135b8,0x33fff0,0x14dc80,0x14dd58,0x14ef30,0x148f50})runtime.registerFunction(pc,getter);
    write(a+0x6c0,uint32_t(0x13000));write(0x13000+0x28,int16_t(0));write(0x13000+0x2c,uint32_t(0x33fff0));write(0x13000+0x10,int16_t(0));write(0x13000+0x14,uint32_t(0x33fff0));write(a+0x77c,uint32_t(0x14000));
    unsigned eligible=0,resets=0;
    for(unsigned i=0;i<20000;++i) {
        state={};state.motionMode=rng()%5;state.controlState=rng()%14;state.ragdollSubmode=rng()%2;state.velocityCmps={speed(rng),speed(rng),speed(rng)};
        auto normalized=[&](){Vector v{unit(rng),unit(rng),unit(rng)};float scale=1/std::sqrt(dot(v,v));for(auto&k:v)k*=scale;return v;};
        state.groundNormal=normalized();state.physicalUp=normalized();auto direction=normalized();float impulse=speed(rng);
        vec(a+0x1e0,state.velocityCmps);vec(a+0x370,state.groundNormal);vec(a+0x1c0,state.physicalUp);vec(out,direction);write(0x14000+0x30,state.ragdollSubmode);
        R5900Context c{};c.pc=0x107e70;c.f[12]=impulse;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,a);SET_GPR_U32(&c,5,b);SET_GPR_U32(&c,6,out);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,done);
        bool actualEligible=false;reset=false;magnitude=clamped=0;Rounding rounding;
        try{sub_00107E70_0x107e70(m.data(),&c,&runtime);}catch(int code){if(code!=84)throw;actualEligible=true;}
        auto native=ssx::originalPairImpulse(state,direction,impulse);Vector velocity;std::memcpy(&velocity,m.data()+a+0x1e0,12);
        if(native.velocityCmps!=velocity||native.resetPredictor!=reset||native.reactionEligible!=actualEligible||native.clampedImpulseCmps!=clamped||native.reactionMagnitudeCmps!=magnitude){std::cerr<<"Pair impulse mismatch "<<i<<" mode "<<state.motionMode<<" control "<<state.controlState<<" sub "<<state.ragdollSubmode<<" gates "<<actualEligible<<'/'<<native.reactionEligible<<" magnitude "<<magnitude<<'/'<<native.reactionMagnitudeCmps<<'\n';return 4;}eligible+=actualEligible;resets+=reset;
    }
    std::cout<<"20000 original pair impulse prefixes/gates match exactly; "<<eligible<<" reactions, "<<resets<<" predictor resets\n";
    for(unsigned i=0;i<20000;++i){weight=int(rng()%200);stat=unit(rng);float boost=unit(rng);write(a+0x2fc,boost);R5900Context c{};c.pc=0x11ff98;SET_GPR_U32(&c,4,a);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,done);Rounding rounding;sub_0011FF98_0x11ff98(m.data(),&c,&runtime);if(c.f[0]!=ssx::originalRiderCollisionWeight(weight,stat,boost)){std::cerr<<"Weight mismatch "<<i<<'\n';return 5;}}
    std::cout<<"20000 original resolved collision weights match exactly\n";
    for(uint32_t pc:{0x317810,0x10eb30,0x108388,0x10e468,0x10e2e8,0x10e3a8,0x10e228})runtime.registerFunction(pc,reaction);
    stopPairBeforeReaction=false;unsigned soft=0,crash=0;
    for(unsigned i=0;i<20000;++i) {
        state={};state.motionMode=rng()%5;state.controlState=rng()%14;state.ragdollSubmode=rng()%2;state.velocityCmps={speed(rng),speed(rng),speed(rng)};
        Vector direction{unit(rng),unit(rng),unit(rng)},position{coordinate(rng),coordinate(rng),coordinate(rng)};
        float inverse=1/std::sqrt(dot(direction,direction));for(auto& k:direction)k*=inverse;
        float impulse=speed(rng),distance=radius(rng)*10;bool attack=rng()%2,cheat=rng()%7==0;
        ssx::OriginalCollisionContext context;context.motionMode=state.motionMode;context.controlState=state.controlState;
        vec(a+0x160,context.presentation.right);vec(a+0x170,context.presentation.forward);vec(a+0x180,context.presentation.up);
        vec(a+0x1e0,state.velocityCmps);vec(a+0x370,state.groundNormal);vec(a+0x1c0,state.physicalUp);vec(a+0x110,position);vec(out,direction);
        write(0x14000+0x30,state.ragdollSubmode);write(b+0x86c,0u);write(a+8,distance);write(0x5308d0,cheat?32u:0u);
        auto seed=rng();reactionRng.seed(seed);randomDraws=0;actualReaction={};R5900Context c{};c.pc=0x107e70;c.f[12]=impulse;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);
        SET_GPR_U32(&c,4,a);SET_GPR_U32(&c,5,b);SET_GPR_U32(&c,6,out);SET_GPR_U32(&c,7,attack);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,done);
        Rounding rounding;sub_00107E70_0x107e70(m.data(),&c,&runtime);std::mt19937 nativeRng(seed);
        auto nativeImpulse=ssx::originalPairImpulse(state,direction,impulse);
        auto native=ssx::originalPairReactionRequest(nativeImpulse,context,direction,position,distance,attack,cheat,[&](){return nativeRng();});
        const auto& x=actualReaction.event;const auto& y=native.event;
        if(c.pc!=done||actualReaction.kind!=native.kind||actualReaction.animation!=native.animation||randomDraws!=native.randomDraws||x.pointCm!=y.pointCm||x.incomingDirection!=y.incomingDirection||x.normal!=y.normal||x.closingSpeedCmps!=y.closingSpeedCmps){std::cerr<<"Pair reaction mismatch "<<i<<" kind "<<int(actualReaction.kind)<<'/'<<int(native.kind)<<" animation "<<actualReaction.animation<<'/'<<native.animation<<" draws "<<randomDraws<<'/'<<native.randomDraws<<" point "<<(x.pointCm==y.pointCm)<<" incoming "<<(x.incomingDirection==y.incomingDirection)<<" normal "<<(x.normal==y.normal)<<" closing "<<x.closingSpeedCmps<<'/'<<y.closingSpeedCmps<<'\n';return 6;}
        soft+=native.kind==ssx::OriginalPairReactionRequest::Kind::Soft;crash+=native.kind==ssx::OriginalPairReactionRequest::Kind::Crash;
    }
    std::cout<<"20000 complete original pair reaction requests match exactly; "<<soft<<" soft, "<<crash<<" crash dispatches\n";

    for(uint32_t pc:{0x11ff98,0x1231a8,0x1298c8,0x311ae8,0x106538,0x107e70})runtime.registerFunction(pc,pairDispatch);
    runtime.registerFunction(0x329f98,sub_00329F98_0x329f98);
    write(0x4a30f0-0x848,0x50000u);write(0x50084,0x51000u);write(0x5100c,0x52000u);write(0x52028,a);write(0x5202c,b);
    unsigned separations=0,impulsePairs=0;
    for(unsigned i=0;i<20000;++i) {
        std::memset(m.data()+a,0,0x1000);std::memset(m.data()+b,0,0x1000);
        ssx::BodyCollisionVolume x,y;x.broadRadiusCm=y.broadRadiusCm=250;x.count=y.count=10;
        for(auto* v:{&x,&y})for(unsigned j=0;j<10;++j)v->spheres[j]={{coordinate(rng),coordinate(rng),coordinate(rng)},radius(rng),j};
        body(0x60000,x);body(0x61000,y);write(a+0xaa0,0x60000u);write(b+0xaa0,0x61000u);write(a+0x86c,0u);write(b+0x86c,1u);
        for(unsigned n=0;n<2;++n){pairStates[n]={};pairStates[n].motionMode=rng()%5;pairStates[n].ragdollSubmode=rng()%2;pairStates[n].velocityCmps={speed(rng),speed(rng),speed(rng)};pairWeights[n]=50+radius(rng);vec((n?b:a)+0x1e0,pairStates[n].velocityCmps);vec((n?b:a)+0x370,pairStates[n].groundNormal);}
        pairTick=500+int(rng()%500);ssx::OriginalPairRecord own,other;own.enabled=rng()%10!=0;own.lastContactTick=pairTick-int(rng()%8);other.lastCheckedTick=pairTick-int(rng()%3);bool disabled=rng()%10==0;
        write(a+36,uint32_t(own.enabled));write(a+36+16,own.lastContactTick);write(b+20,other.lastCheckedTick);write(b+0x878,uint32_t(disabled));
        pairMoves=pairCalls=0;pairTranslations[0]=pairTranslations[1]={};pairDirection[0]=pairDirection[1]={};pairImpulses[0]=pairImpulses[1]=0;
        R5900Context c{};c.pc=0x107888;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,a);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,done);
        Rounding rounding;sub_00107888_0x107888(m.data(),&c,&runtime);
        bool checked=ssx::originalPairShouldCheck(own,other,disabled,pairTick);auto contact=ssx::originalBodyPairContact(x,y);bool moved=checked&&contact.hit,impulsed=moved&&ssx::originalPairImpulseDue(own,pairTick);
        auto native=ssx::originalPairSeparation(contact,pairStates[0],pairStates[1],pairWeights[0],pairWeights[1]);
        int checkedA,checkedB,contactA,contactB;std::memcpy(&checkedA,m.data()+a+36+20,4);std::memcpy(&checkedB,m.data()+b+20,4);std::memcpy(&contactA,m.data()+a+36+16,4);std::memcpy(&contactB,m.data()+b+16,4);
        if(c.pc!=done||pairMoves!=unsigned(moved)*2||pairCalls!=unsigned(impulsed)*2||checkedA!=(checked?pairTick:0)||checkedB!=(checked?pairTick:other.lastCheckedTick)||contactA!=(impulsed?pairTick:own.lastContactTick)||contactB!=(impulsed?pairTick:0)||(moved&&(pairTranslations[0]!=native.translationA||pairTranslations[1]!=native.translationB))||(impulsed&&(pairDirection[0]!=native.direction||pairDirection[1]!=native.direction||pairImpulses[0]!=native.impulseA||pairImpulses[1]!=native.impulseB))){std::cerr<<"Full ordinary pair dispatcher mismatch "<<i<<" moves "<<pairMoves<<'/'<<moved<<" calls "<<pairCalls<<'/'<<impulsed<<'\n';return 7;}
        separations+=moved;impulsePairs+=impulsed;
    }
    std::cout<<"20000 original ordinary pair dispatcher calls match exactly; "<<separations<<" separations, "<<impulsePairs<<" ordered impulse pairs; timestamps exact\n";

    for(uint32_t pc:{0x10f998,0x10f878,0x155a50,0x155b50,0x33ffe0})runtime.registerFunction(pc,pairDispatch);
    runtime.registerFunction(0x31c228,sub_0031C228_0x31c228);
    write(0x52008,600u);write(0x52078,2u);write(0x52084,0u);write(a+36,1u);
    for(uint32_t actor:{a,b})write(actor+0x6c0,0x53000u);
    write(0x53028,int16_t(0));write(0x5302c,0x33ffe0u);
    for(unsigned i=0;i<20000;++i) {
        Vector pa{speed(rng),speed(rng),speed(rng)},pb{speed(rng),speed(rng),speed(rng)};
        if(i%17==0)pa[0]=pb[0];if(i%31==0)pa[1]=pb[1];vec(a+0x110,pa);vec(b+0x110,pb);
        R5900Context c{};c.pc=0x10f560;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x52000);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,done);
        Rounding rounding;sub_0010F560_0x10f560(m.data(),&c,&runtime);auto native=ssx::originalPairProximity(pa,pb);
        float da,db,ba,bb;std::memcpy(&da,m.data()+a+36+8,4);std::memcpy(&db,m.data()+b+8,4);std::memcpy(&ba,m.data()+a+36+12,4);std::memcpy(&bb,m.data()+b+12,4);
        if(c.pc!=done||da!=native.distanceCm||db!=native.distanceCm||ba!=native.bearingAToB||bb!=native.bearingBToA){std::cerr<<"Pair proximity mismatch "<<i<<" distance "<<da<<'/'<<native.distanceCm<<" bearings "<<ba<<'/'<<native.bearingAToB<<' '<<bb<<'/'<<native.bearingBToA<<'\n';return 8;}
    }
    std::cout<<"20000 complete original proximity refresh calls match exact planar distances and reciprocal bearings\n";

    for(uint32_t pc:{0x311b20,0x1446a0,0x149038})runtime.registerFunction(pc,pairDispatch);
    testingAttack=true;unsigned attacks=0;
    for(unsigned i=0;i<20000;++i) {
        for(unsigned n=0;n<2;++n){auto& x=attackStates[n];x={};x.positionCm={coordinate(rng),coordinate(rng),coordinate(rng)};x.velocityCmps={speed(rng),speed(rng),speed(rng)};x.facing340={unit(rng),unit(rng),unit(rng)};x.animationClass1=n?(rng()%5==0?0:3):(rng()%4==0?13:0);x.marker0=rng()%5!=0;x.marker1=rng()%5!=0;x.resolvedAttackStat=unit(rng);x.strength350=unit(rng);}
        if(i%2==0){auto& x=attackStates[0];auto& y=attackStates[1];x.animationClass1=13;x.marker0=true;x.marker1=false;y.animationClass1=0;y.positionCm=x.positionCm;y.positionCm[0]+=radius(rng);x.facing340={1,0,0};}
        if(i%77==0)attackStates[0].velocityCmps={};
        for(unsigned n=0;n<2;++n){uint32_t actor=n?b:a;vec(actor+0x110,attackStates[n].positionCm);vec(actor+0x1e0,attackStates[n].velocityCmps);vec(actor+0x340,attackStates[n].facing340);write(actor+0x350,attackStates[n].strength350);write(actor+0x784,n?0x71000u:0x70000u);}
        pairTick=600;ssx::OriginalPairRecord reciprocal;reciprocal.lastAttackTick=pairTick-int(rng()%8);
        write(a+36,1u);write(b+20,pairTick);write(b+24,reciprocal.lastAttackTick);write(b+0x878,0u);pairCalls=pairMoves=0;pairDirection[1]={};pairImpulses[1]=0;
        R5900Context c{};c.pc=0x107888;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,a);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,done);
        Rounding rounding;sub_00107888_0x107888(m.data(),&c,&runtime);auto native=ssx::originalPairAttack(attackStates[0],attackStates[1],reciprocal,pairTick);
        int attackTick;std::memcpy(&attackTick,m.data()+b+24,4);
        if(c.pc!=done||pairMoves||pairCalls!=unsigned(native.hit)||attackTick!=(native.hit?pairTick:reciprocal.lastAttackTick)||(native.hit&&(pairDirection[1]!=native.direction||pairImpulses[1]!=native.impulseCmps))){std::cerr<<"Pair attack mismatch "<<i<<" calls "<<pairCalls<<'/'<<native.hit<<" owner "<<attackStates[0].animationClass1<<'/'<<attackStates[0].marker0<<'/'<<attackStates[0].marker1<<" other "<<attackStates[1].animationClass1<<'/'<<attackStates[1].marker0<<'/'<<attackStates[1].marker1<<" cooldown "<<reciprocal.lastAttackTick<<'\n';return 9;}attacks+=native.hit;
    }
    std::cout<<"20000 complete original pair attack branches match exactly; "<<attacks<<" attacks and cooldown updates\n";

}
