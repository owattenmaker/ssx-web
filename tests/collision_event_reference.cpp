#include "ps2_runtime_macros.h"
#include "../engine/collision_event.hpp"
#include "../engine/original_random.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <random>
void sub_001210B0_0x1210b0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00317A08_0x317a08(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00105D98_0x105d98(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00108388_0x108388(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x340000,g_ps2RecompiledFunctionTableSlotCount=(0x340000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x340000-0x100000)/4]={};
static constexpr uint32_t actor=0x30000,owner=0x40000;
static ssx::OriginalCollisionContext context;
static bool hazard,hard,predictor,cancelOne,strongSoft;
static int animation,newControl;
static unsigned draws;static uint32_t randomValue;
static float peak;
static void dependency(uint8_t*,R5900Context*c,PS2Runtime*) {
    switch(c->pc){
    case 0x11fee8:SET_GPR_U32(c,2,context.controlState);break;
    case 0x11fe98:SET_GPR_U32(c,2,context.motionMode);break;
    case 0x311ae8:SET_GPR_U32(c,2,context.animationClass);break;
    case 0x116120:hazard=true;break;
    case 0x150000:peak=std::max(c->f[12],peak);break;
    case 0x150010:SET_GPR_U32(c,2,actor+0x110);break;
    case 0x150020:SET_GPR_U32(c,2,actor+0x1e0);break;
    case 0x1135b8:predictor=true;break;
    case 0x10eb30:hard=true;animation=int(GPR_U32(c,5));break;
    case 0x317810:SET_GPR_U32(c,2,randomValue+draws++*1234567u);break;
    case 0x131348:cancelOne=true;break;
    case 0x3128e8:animation=int(GPR_U32(c,5));break;
    case 0x28b180:SET_GPR_U32(c,2,0x90000);break;
    case 0x2a0e70:strongSoft=GPR_U32(c,6)!=0;break;
    case 0x11fec8:newControl=int(GPR_U32(c,5));break;
    case 0x1200d0:case 0x120d90:break;
    default:throw std::runtime_error("Unexpected collision dependency");
    }
    c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){
    using namespace ssx;using namespace terrain_original;
    if(argc!=2)return 2;std::vector<uint8_t> memory(32*1024*1024);std::ifstream input(argv[1],std::ios::binary);input.read((char*)memory.data(),memory.size());if(!input)return 3;
    PS2Runtime runtime;runtime.setMissingFunctionPolicy(PS2Runtime::MissingFunctionPolicy::Stop);
    for(uint32_t pc:{0x11fee8,0x11fe98,0x311ae8,0x116120,0x150000,0x150010,0x150020,0x1135b8,0x10eb30,0x317810,0x131348,0x3128e8,0x28b180,0x2a0e70,0x11fec8,0x1200d0,0x120d90})runtime.registerFunction(pc,dependency);
    runtime.registerFunction(0x108388,sub_00108388_0x108388);
    auto put=[&](uint32_t at,const auto&value){std::memcpy(memory.data()+at,&value,sizeof(value));};
    auto get=[&](uint32_t at){uint32_t value;std::memcpy(&value,memory.data()+at,4);return value;};
    auto vector=[&](uint32_t at,Vector value,bool position=false){put(at,value);put(at+12,position?1.f:0.f);};
    uint32_t surfaceBase=get(get(get(0x4a30f0-0x848)+0x84)+0x44);
    constexpr uint32_t eventAt=0x60000,vtable=0x50000,done=0x12345678;
    put(actor+0x77c,owner);put(actor+0x6c0,vtable);put(vtable+0x88,uint16_t(0));put(vtable+0x8c,uint32_t(0x150000));put(vtable+0x28,uint16_t(0));put(vtable+0x2c,uint32_t(0x150010));put(vtable+0x10,uint16_t(0));put(vtable+0x14,uint32_t(0x150020));
    std::mt19937 random(0x45564e54);std::uniform_real_distribution<float> unit(-1,1),position(-200,200),speed(-3500,3500),impact(0,4000);
    for(unsigned i=0;i<30000;++i){OriginalRandomState state;for(auto&x:state.words)x=random();if(i%31==0)state.words.fill(0xffffffff);if(i%31==1)state.words.fill(0);if(i%31==2){state.words.fill(0xffffffff);state.words[i%6]=0;}put(0x80000,state.words);R5900Context c{};c.pc=0x317a08;SET_GPR_U32(&c,4,0x80000);SET_GPR_U32(&c,31,done);sub_00317A08_0x317a08(memory.data(),&c,&runtime);auto value=state.next();std::array<uint32_t,6> expected;std::memcpy(&expected,memory.data()+0x80000,24);if(value!=GPR_U32((&c),2)||state.words!=expected)throw std::runtime_error("Original shared RNG mismatch");}
    std::cout<<"30000 original shared RNG transitions match, including carry-wrap boundaries\n";
    for(unsigned i=0;i<10000;++i){Rounding rounding;context.motionMode=0;OriginalCollisionHistory history;history.previousNormal={unit(random),unit(random),unit(random)};history.directionChanges=std::abs(unit(random))*8;history.secondaryCounter=std::abs(unit(random))*10;
        vector(actor+0x3e0,history.previousNormal);put(actor+0x3f0,history.directionChanges);put(actor+0x3f4,history.secondaryCounter);hazard=false;
        R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);c.pc=0x1210b0;SET_GPR_U32(&c,4,actor);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,done);SET_GPR_U32(&c,28,0x4a30f0);sub_001210B0_0x1210b0(memory.data(),&c,&runtime);
        bool reset=originalCollisionHistoryDecay(OriginalCollisionProfile{},history);Vector normal;float changes,secondary;std::memcpy(&normal,memory.data()+actor+0x3e0,12);std::memcpy(&changes,memory.data()+actor+0x3f0,4);std::memcpy(&secondary,memory.data()+actor+0x3f4,4);
        if(c.pc!=done||reset!=hazard||normal!=history.previousNormal||changes!=history.directionChanges||secondary!=history.secondaryCounter)throw std::runtime_error("Original collision history decay mismatch");
    }
    std::cout<<"10000 original collision-history decay/recovery-threshold cases match exactly\n";
    random.seed(0x45564e54);unsigned counts[5]={};OriginalCollisionProfile profile;
    for(unsigned i=0;i<30000;++i){
        Rounding rounding;context={};context.motionMode=i%5;constexpr int states[]={0,1,2,3,7,9,11,13};context.controlState=states[(i/5)%8];context.animationClass=i%25;context.ragdollSubmode=i%2;context.reverseStance=i%2;
        auto frame=[&](){OriginalCollisionFrame f;float angle=unit(random)*3,tilt=unit(random);float ca=std::cos(angle),sa=std::sin(angle),ct=std::cos(tilt),st=std::sin(tilt);f.right={ca,sa,0};f.forward={-sa*ct,ca*ct,st};f.up=cross(f.right,f.forward);f.origin={position(random),position(random),position(random)};return f;};
        context.presentation=frame();context.physical=frame();context.velocityCmps={speed(random),speed(random),speed(random)};context.manualSpin=i%3?unit(random)*9:0;
        OriginalCollisionEvent event;event.pointCm={position(random),position(random),position(random)};event.normal={unit(random),unit(random),unit(random)};float inverse=terrain_original::div(1,std::sqrt(dot(event.normal,event.normal)));for(auto&x:event.normal)x=mul(x,inverse);event.closingSpeedCmps=impact(random);event.surface=i%7?-1:14;event.surfaceProperty44=i%29==0;
        OriginalCollisionHistory history;history.previousNormal={unit(random),unit(random),unit(random)};history.directionChanges=std::abs(unit(random)*3);history.peakImpactCmps=impact(random);peak=history.peakImpactCmps;
        vector(actor+0x3e0,history.previousNormal);put(actor+0x3f0,history.directionChanges);put(actor+0x320,uint32_t(context.reverseStance));put(actor+0x2dc,context.manualSpin);put(owner+0x30,context.ragdollSubmode);put(owner+0x314,0u);
        vector(actor+0x160,context.presentation.right);vector(actor+0x170,context.presentation.forward);vector(actor+0x180,context.presentation.up);vector(actor+0x190,context.presentation.origin,true);
        vector(actor+0x1a0,context.physical.right);vector(actor+0x1b0,context.physical.forward);vector(actor+0x1c0,context.physical.up);vector(actor+0x1e0,context.velocityCmps);
        vector(eventAt,event.pointCm,true);vector(eventAt+16,event.incomingDirection);vector(eventAt+32,event.normal);put(eventAt+48,event.closingSpeedCmps);
        if(event.surface!=-1)put(surfaceBase+event.surface*176+0x44,event.surfaceProperty44);
        hazard=hard=predictor=cancelOne=strongSoft=false;animation=newControl=-1;draws=0;randomValue=random();
        R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);c.pc=0x105d98;SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,done);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,4,actor);SET_GPR_U32(&c,5,eventAt);SET_GPR_U32(&c,6,uint32_t(event.surface));
        sub_00105D98_0x105d98(memory.data(),&c,&runtime);if(c.pc!=done){std::cerr<<"Incomplete collision reaction PC "<<std::hex<<c.pc<<'\n';return 4;}
        unsigned nativeDraws=0;auto native=originalCollisionReaction(profile,history,context,event,[&](){return randomValue+nativeDraws++*1234567u;});
        auto expectedKind=hazard?OriginalCollisionReactionKind::SurfaceReset:hard?OriginalCollisionReactionKind::Crash:get(owner+0x314)?OriginalCollisionReactionKind::RagdollImpact:animation>=0?OriginalCollisionReactionKind::Soft:OriginalCollisionReactionKind::Ignored;
        float changes,manual;Vector previous;std::memcpy(&changes,memory.data()+actor+0x3f0,4);std::memcpy(&manual,memory.data()+actor+0x2dc,4);std::memcpy(&previous,memory.data()+actor+0x3e0,12);
        if(native.kind!=expectedKind||native.animation!=animation||native.resetPredictor!=predictor||native.cancelControlOne!=cancelOne||native.strongSoftImpact!=strongSoft||native.randomDraws!=draws||native.manualSpin!=manual||history.peakImpactCmps!=peak||history.directionChanges!=changes||history.previousNormal!=previous||(newControl>=0&&native.nextControlState!=newControl)){
            std::cerr<<"Collision reaction mismatch case "<<i<<" mode/state "<<context.motionMode<<'/'<<context.controlState<<" kind "<<int(native.kind)<<'/'<<int(expectedKind)<<" anim "<<native.animation<<'/'<<animation<<" RNG "<<native.randomDraws<<'/'<<draws<<" spin "<<native.manualSpin<<'/'<<manual<<" history "<<history.directionChanges<<'/'<<changes<<" reset "<<native.resetPredictor<<'/'<<predictor<<'\n';return 1;
        }
        ++counts[int(native.kind)];
    }
    std::cout<<"30000 complete original collision reaction decisions match: ignored "<<counts[0]<<", hazard "<<counts[1]<<", ragdoll "<<counts[2]<<", soft "<<counts[3]<<", crash "<<counts[4]<<"; history/spin/random draw order exact\n";
}
