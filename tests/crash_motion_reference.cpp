#include "ps2_runtime_macros.h"
#include "../engine/crash_ground.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <random>
void sub_00136C40_0x136c40(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00136D40_0x136d40(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00136F30_0x136f30(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00137750_0x137750(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00137D18_0x137d18(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x340000;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=(0x340000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x340000-0x100000)/4]={};
static ssx::OriginalCrashGroundHit groundHit;static bool hitImpact,requestedReset,entityCall;static float impactSpeed;static ssx::terrain_original::Vector entityForce;
static bool began,aligned,stopGround=false;static int animationClass2;static ssx::terrain_original::Vector alignmentNormal,alignmentHeading;static float limit,gain,rate,seconds;
static void external(uint8_t* m,R5900Context* c,PS2Runtime*){if(c->pc==0x1135b8){began=true;limit=c->f[12];}if(c->pc==0x113648)seconds=c->f[12];if(c->pc==0x138960){auto p=GPR_U32(c,5);std::memset(m+p,0,0x80);std::memcpy(m+p,&groundHit.point,12);std::memcpy(m+p+16,&groundHit.normal,12);std::memcpy(m+p+32,&groundHit.surfaceVelocity,12);std::memcpy(m+p+0x4c,&groundHit.surface,4);uint32_t patch=groundHit.hasPatch?0x64000:0,instance=groundHit.hasLiveEntity?0x65000:0;std::memcpy(m+p+0x54,&patch,4);std::memcpy(m+p+0x50,&instance,4);c->f[0]=groundHit.fraction;}
if(c->pc==0x111aa0){hitImpact=true;impactSpeed=c->f[12];}if(c->pc==0x116120)requestedReset=true;if(c->pc==0x33ffe8){entityCall=true;std::memcpy(&entityForce,m+GPR_U32(c,6),12);}
if(c->pc==0x311ae8){SET_GPR_U32(c,2,animationClass2);}if(c->pc==0x121aa0){aligned=true;gain=c->f[12];rate=c->f[13];if(stopGround){std::memcpy(&alignmentNormal,m+GPR_U32(c,5),12);std::memcpy(&alignmentHeading,m+GPR_U32(c,6),12);throw 1;}}c->pc=GPR_U32(c,31);}
int main(int argc,char**argv){
    using namespace ssx;using namespace ssx::terrain_original;PS2Runtime runtime;std::vector<uint8_t> memory(32*1024*1024);if(argc!=2)return 1;std::ifstream ee(argv[1],std::ios::binary);ee.read((char*)memory.data(),memory.size());if(!ee)return 2;
    auto write=[&](uint32_t p,const auto& value){std::memcpy(memory.data()+p,&value,sizeof(value));};auto vec=[&](uint32_t p,Vector value){write(p,value);write(p+12,0.f);};auto readVec=[&](uint32_t p){Vector v;std::memcpy(&v,memory.data()+p,12);return v;};auto readInt=[&](uint32_t p){int v;std::memcpy(&v,memory.data()+p,4);return v;};
    for(uint32_t pc:{0x1135b8,0x113648,0x121aa0,0x11e098,0x311ae8,0x138960,0x111aa0,0x116120,0x33ffe8})runtime.registerFunction(pc,external);
    constexpr uint32_t actor=0x10000,state=0x20000,stack=0x30000,inputs=0x40000,predictor=0x50000,done=0x12345678;
    write(state+0x40,actor);write(state+0x44,1u);write(actor+0x788,predictor);
    std::mt19937 random(0x43524153);std::uniform_real_distribution<float> coordinate(-100000,100000),velocity(-5000,5000),unit(-1,1),time(.1f,2.f);
    auto vector=[&](auto& distribution){return Vector{distribution(random),distribution(random),distribution(random)};};auto quaternion=[&](){std::array<float,4> q{unit(random),unit(random),unit(random),unit(random)};float l=std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);for(auto& x:q)x/=l;return q;};
    auto context=[&](uint32_t pc){R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,state);SET_GPR_U32(&c,29,stack);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,done);return c;};
    for(unsigned n=0;n<20000;++n){OriginalCrashMotionState s;s.flag4=9;OriginalCrashActorState a;a.position=vector(coordinate);a.velocity=vector(velocity);a.groundNormal=vector(unit);a.spinRate=a.flipRate=8;int old=random()%6;
        vec(actor+0x110,a.position);vec(actor+0x1e0,a.velocity);vec(actor+0x370,a.groundNormal);write(state+4,s.flag4);write(actor+0x2dc,a.spinRate);write(actor+0x2e0,a.flipRate);auto c=context(0x136c40);SET_GPR_U32(&c,5,old);began=false;Rounding rounding;sub_00136C40_0x136c40(memory.data(),&c,&runtime);auto result=originalCrashMotionBegin(s,a,old);
        if(c.pc!=done||began!=result.beginPredictor||(began&&limit!=result.predictorSpeedLimit)||readInt(state)!=s.submode||readInt(state+4)!=0||readVec(actor+0x1e0)!=a.velocity||readVec(state+0x50)!=s.low||readVec(state+0x60)!=s.high||readInt(actor+0x2dc)||readInt(actor+0x2e0))throw std::runtime_error("Crash motion entry mismatch");
    }
    std::cout<<"20000 original crash motion2 entry cases match exactly\n";
    for(unsigned n=0;n<20000;++n){OriginalCrashMotionState s;s.submode=random()%2;OriginalCrashActorState a;a.groundNormal=vector(unit);Vector p=vector(coordinate),v=vector(velocity),w=vector(unit);auto q=quaternion();write(state,s.submode);vec(actor+0x370,a.groundNormal);vec(inputs,v);vec(inputs+16,w);vec(inputs+32,p);write(inputs+48,q);auto c=context(0x136d40);SET_GPR_U32(&c,5,inputs);SET_GPR_U32(&c,6,inputs+16);SET_GPR_U32(&c,7,inputs+32);Rounding rounding;sub_00136D40_0x136d40(memory.data(),&c,&runtime);originalCrashDetach(s,a,p,q,v,w);
        std::array<float,4> actual;std::memcpy(&actual,memory.data()+actor+0x140,16);if(c.pc!=done||readVec(actor+0x130)!=a.detachedPosition||actual!=a.detachedQuaternion||readInt(actor+0x150)!=1||readVec(state+16)!=s.detachedVelocity||readVec(state+32)!=s.detachedAngularVelocity)throw std::runtime_error("Crash detach mismatch");
    }
    std::cout<<"20000 original detached crash root entry cases match exactly\n";
    for(unsigned n=0;n<20000;++n){OriginalCrashMotionState s;s.detachedVelocity=vector(velocity);s.detachedAngularVelocity=vector(unit);OriginalCrashActorState a;a.detached=n%5!=0;a.timeScale=time(random);a.detachedPosition=vector(coordinate);a.detachedQuaternion=quaternion();vec(state+16,s.detachedVelocity);vec(state+32,s.detachedAngularVelocity);vec(actor+0x130,a.detachedPosition);write(actor+0x140,a.detachedQuaternion);write(actor+0x150,int(a.detached));write(actor+0x300,a.timeScale);auto c=context(0x136f30);Rounding rounding;sub_00136F30_0x136f30(memory.data(),&c,&runtime);originalCrashDetachedStep(s,a);
        std::array<float,4> actual;std::memcpy(&actual,memory.data()+actor+0x140,16);if(c.pc!=done||readVec(actor+0x130)!=a.detachedPosition||actual!=a.detachedQuaternion||readVec(state+16)!=s.detachedVelocity||readVec(state+32)!=s.detachedAngularVelocity){std::cerr<<"Crash secondary physics mismatch "<<n<<" quaternion "<<(actual==a.detachedQuaternion)<<'\n';return 3;}
    }
    std::cout<<"20000 original detached-root physics cases match position, velocity, angular damping and quaternion exactly\n";
    for(unsigned n=0;n<20000;++n){OriginalCrashMotionState s;s.angularVelocity=vector(unit);float scale=time(random),pred=time(random),elapsed=time(random);int status=random()%5;vec(state+48,s.angularVelocity);write(actor+0x300,scale);write(predictor+0xac,status);write(predictor+0x98,pred);write(predictor+0xa0,elapsed);auto c=context(0x137750);aligned=false;Rounding rounding;sub_00137750_0x137750(memory.data(),&c,&runtime);auto native=originalCrashAirAlignmentRequest(s,scale,status,pred,elapsed);
        if(c.pc!=done||aligned!=native.align||(aligned&&(gain!=native.gain||rate!=native.maximumRate))||seconds!=mul(scale,.01666666753590107f)||readVec(state+48)!=s.angularVelocity)throw std::runtime_error("Crash air predictor/alignment caller mismatch");
    }
    std::cout<<"20000 original airborne crash predictor/alignment callers and angular damping match exactly\n";
    write(0x4a30f0-0x848,0x60000u);write(0x60084,0x61000u);write(0x61044,0x62000u);write(actor+0x438,0u);stopGround=true;
    for(unsigned n=0;n<20000;++n){OriginalCrashMotionState s;s.angularVelocity=vector(unit);OriginalCrashActorState a;a.position=vector(coordinate);a.velocity=vector(velocity);a.groundNormal=vector(unit);float length=std::sqrt(dot(a.groundNormal,a.groundNormal));for(auto& x:a.groundNormal)x/=length;a.timeScale=time(random);a.detached=random()%2;Vector surfaceVelocity=vector(unit),forward=vector(unit);animationClass2=random()%3==0?29:(random()%2==0?25:27);
        GroundSurface surface;surface.slipFriction={{{0,unit(random)},{30,unit(random)},{80,unit(random)},{120,unit(random)}}};for(unsigned j=0;j<4;++j){write(0x62090+j*8,surface.slipFriction[j].x);write(0x62094+j*8,surface.slipFriction[j].y);}
        vec(state+48,s.angularVelocity);vec(actor+0x110,a.position);vec(actor+0x1e0,a.velocity);vec(actor+0x370,a.groundNormal);vec(actor+0x3d0,surfaceVelocity);vec(actor+0x1b0,forward);write(actor+0x300,a.timeScale);write(actor+0x150,int(a.detached));auto c=context(0x137d18);bool reached=false;Rounding rounding;try{sub_00137D18_0x137d18(memory.data(),&c,&runtime);}catch(int code){if(code!=1)throw;reached=true;}auto result=originalCrashSlidingForces(surface,s,a,surfaceVelocity,animationClass2,forward);
        if(!reached||readVec(state+48)!=s.angularVelocity||readVec(actor+0x110)!=a.position||readVec(actor+0x1e0)!=a.velocity||alignmentNormal!=result.normal||alignmentHeading!=result.heading||gain!=result.gain||rate!=result.maximumRate){std::cerr<<"Crash sliding force mismatch "<<n<<" angular "<<(readVec(state+48)==s.angularVelocity)<<" position "<<(readVec(actor+0x110)==a.position)<<" velocity "<<(readVec(actor+0x1e0)==a.velocity)<<" heading "<<(alignmentHeading==result.heading)<<'\n';return 4;}
    }
    std::cout<<"20000 original sliding crash force integrations match angular approach, friction curve, gravity, position/velocity, speed cap and alignment inputs exactly\n";

    stopGround=false;write(actor+0x780,0x63000u);write(0x6500c,0x65100u);write(0x6510c,0x65200u);write(0x65358,int16_t(0));write(0x6535c,0x33ffe8u);
    unsigned impacts=0,leaves=0,resets=0;
    for(unsigned n=0;n<20000;++n){OriginalCrashMotionState s;s.angularVelocity=vector(unit);OriginalCrashActorState a;a.position=vector(coordinate);a.velocity=vector(velocity);a.groundNormal={0,0,1};a.timeScale=time(random);a.detached=false;Vector surfaceVelocity=vector(unit),forward={0,1,0};animationClass2=27;float scale=time(random),depth=10+time(random)*30;
        GroundSurface surface;surface.slipFriction={{{0,0},{30,.05f},{80,.116666f},{120,.133333f}}};for(unsigned j=0;j<4;++j){write(0x62090+j*8,surface.slipFriction[j].x);write(0x62094+j*8,surface.slipFriction[j].y);}
        auto expectedActor=a;auto expectedState=s;auto alignment=originalCrashSlidingForces(surface,expectedState,expectedActor,surfaceVelocity,animationClass2,forward);
        groundHit={};groundHit.fraction=n%7?0.5f:-1;groundHit.normal={0,0,1};groundHit.point=expectedActor.position;groundHit.point[2]+=float(int(n%9)-4)*20;groundHit.surfaceVelocity=vector(unit);groundHit.surface=1;groundHit.surfaceProperty44=n%17==0;groundHit.hasPatch=n%3==0;groundHit.patchFlags=n%5==0?2:0;groundHit.hasLiveEntity=n%4==0;
        write(0x62018,depth);write(0x620f4,groundHit.surfaceProperty44);write(0x6400a,int16_t(groundHit.patchFlags));write(0x63140,scale);write(actor+0x438,0u);write(state+4,0);vec(state+48,s.angularVelocity);vec(actor+0x110,a.position);vec(actor+0x1e0,a.velocity);vec(actor+0x370,a.groundNormal);vec(actor+0x3d0,surfaceVelocity);vec(actor+0x1b0,forward);write(actor+0x300,a.timeScale);write(actor+0x150,0);auto c=context(0x137d18);began=hitImpact=requestedReset=entityCall=false;Rounding rounding;sub_00137D18_0x137d18(memory.data(),&c,&runtime);
        auto result=originalCrashSlidingContact(expectedState,expectedActor,groundHit,scale,depth,alignment.relativeVelocityBeforeForces);
        if(c.pc!=done||readVec(actor+0x110)!=expectedActor.position||readVec(actor+0x1e0)!=expectedActor.velocity||readInt(state+4)!=expectedState.flag4||began!=result.beginPredictor||hitImpact!=result.impact||requestedReset!=result.requestReset||entityCall!=result.dynamicCallbackRequired||(hitImpact&&impactSpeed!=result.impactSpeed)||(entityCall&&entityForce!=result.entityForce)||(groundHit.fraction>=0&&(readVec(actor+0x370)!=expectedActor.groundNormal||readVec(actor+0x3d0)!=expectedActor.surfaceVelocity||readVec(actor+0x460)!=expectedActor.contactPoint||readInt(actor+0x438)!=expectedActor.surface))){std::cerr<<"Crash sliding contact mismatch "<<n<<" position "<<(readVec(actor+0x110)==expectedActor.position)<<" velocity "<<(readVec(actor+0x1e0)==expectedActor.velocity)<<" impact "<<hitImpact<<'/'<<result.impact<<" begin "<<began<<'/'<<result.beginPredictor<<" reset "<<requestedReset<<'/'<<result.requestReset<<" entity "<<entityCall<<'/'<<result.dynamicCallbackRequired<<'\n';return 5;}
        impacts+=hitImpact;leaves+=began;resets+=requestedReset;
    }
    std::cout<<"20000 complete original sliding crash contact responses match; "<<impacts<<" impacts, "<<leaves<<" predictor starts, "<<resets<<" reset requests; moving-entity force callbacks exact\n";

}
