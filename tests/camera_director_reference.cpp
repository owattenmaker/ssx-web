// Instruction-level conformance of the camera director port (tools/test_camera_director_native.py).
#include "ps2_runtime_macros.h"
#define main camera_unit_test_main
#include "../engine/original_camera_tests.cpp"
#undef main
#include "../engine/original_camera_director.hpp"
#include <cstring>
#include <fstream>
#include <map>
#include <random>
#include <vector>

extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
#define SUB(name) void name(uint8_t*,R5900Context*,PS2Runtime*);
SUB(sub_001789E8_0x1789e8) SUB(sub_00162458_0x162458) SUB(sub_00166F28_0x166f28) SUB(sub_00166C60_0x166c60)
SUB(sub_00166550_0x166550) SUB(sub_00165938_0x165938) SUB(sub_00166640_0x166640) SUB(sub_00166530_0x166530)
SUB(sub_00168150_0x168150) SUB(sub_00166F90_0x166f90) SUB(sub_0031B748_0x31b748) SUB(sub_0031B7A8_0x31b7a8)
SUB(sub_0031BE50_0x31be50) SUB(sub_0031C040_0x31c040) SUB(sub_0031C128_0x31c128) SUB(sub_0031C228_0x31c228)
SUB(sub_00162568_0x162568) SUB(sub_00161AB0_0x161ab0) SUB(sub_0015CA50_0x15ca50) SUB(sub_00161E58_0x161e58)
SUB(sub_0015C988_0x15c988) SUB(sub_0015CB08_0x15cb08) SUB(sub_0015E668_0x15e668) SUB(sub_001673A0_0x1673a0)
SUB(sub_00167E30_0x167e30)

namespace {
std::vector<uint8_t> M(32*1024*1024);
template<class T> void put(uint32_t a,const T& v){std::memcpy(M.data()+a,&v,sizeof v);}
template<class T> T get(uint32_t a){T v;std::memcpy(&v,M.data()+a,sizeof v);return v;}
void ret(R5900Context* c){c->pc=GPR_U32(c,31);}
constexpr uint32_t GP=0x4a30f0,STACK=0x90000,SENTINEL=0x12345678,TARGET=0x30000,TARGET_VT=0x31000,RIDER=0x40000,CAMERA_TARGET_VT=0x32000;
constexpr uint32_t ALG=0x20000,DIRECTOR=0x20000,LIST=0x21000,NODES=0x22000,ALGS=0x23000,FAKE_ALG_VT=0x24000,NEW_ALG=0x26000,OUTER=0x60000;

// Rider camera target (vtable 0x45B7A8 stand-in) and rider fields read directly.
struct Target {Quad head{0,0,0,1},velocity{},forward{1,0,0,0};float boost=0,charge=0;int32_t surface=0,mode=0;} T;
void targetHead(uint8_t* m,R5900Context* c,PS2Runtime*){std::memcpy(m+GPR_U32(c,4),T.head.data(),16);ret(c);}
void targetVelocity(uint8_t* m,R5900Context* c,PS2Runtime*){std::memcpy(m+GPR_U32(c,4),T.velocity.data(),16);ret(c);}
void targetForward(uint8_t* m,R5900Context* c,PS2Runtime*){std::memcpy(m+GPR_U32(c,4),T.forward.data(),16);ret(c);}
void targetBoost(uint8_t*,R5900Context* c,PS2Runtime*){c->f[0]=T.boost;ret(c);}
void targetCharge(uint8_t*,R5900Context* c,PS2Runtime*){c->f[0]=T.charge;ret(c);}
void targetSurface(uint8_t*,R5900Context* c,PS2Runtime*){SET_GPR_U32(c,2,uint32_t(T.surface));ret(c);}
void targetTrap(uint8_t*,R5900Context* c,PS2Runtime*){std::printf("unexpected target slot at %08x\n",c->pc);throw std::runtime_error("Unexpected camera target call");}
void motionMode(uint8_t*,R5900Context* c,PS2Runtime*){if(GPR_U32(c,4)!=RIDER)throw std::runtime_error("Motion mode of another rider");SET_GPR_U32(c,2,uint32_t(T.mode));ret(c);}
void cameraTarget(uint8_t*,R5900Context* c,PS2Runtime*){SET_GPR_U32(c,2,TARGET);ret(c);}
unsigned baseCtorCalls=0;
void baseCtor(uint8_t*,R5900Context* c,PS2Runtime*){++baseCtorCalls;SET_GPR_U32(c,2,GPR_U32(c,4));ret(c);}
// Director boundaries.
std::vector<std::pair<uint32_t,uint32_t>> steps,dtors;std::vector<uint32_t> frees,setTargets,allocations;
unsigned spokeBegins=0,spokeEnds=0,defaults=0;struct RequestCall{uint32_t type,argument;float rate;};std::vector<RequestCall> requests;
uint32_t bump=0x70000;
void algStep(uint8_t*,R5900Context* c,PS2Runtime*){steps.push_back({GPR_U32(c,4),GPR_U32(c,5)});ret(c);}
void algDtor(uint8_t*,R5900Context* c,PS2Runtime*){dtors.push_back({GPR_U32(c,4),GPR_U32(c,5)});ret(c);}
void algSetTarget(uint8_t*,R5900Context* c,PS2Runtime*){setTargets.push_back(GPR_U32(c,4));ret(c);}
void freeStub(uint8_t*,R5900Context* c,PS2Runtime*){frees.push_back(GPR_U32(c,4));ret(c);}
void allocStub(uint8_t*,R5900Context* c,PS2Runtime*){allocations.push_back(GPR_U32(c,4));uint32_t at=bump;bump+=0x40;std::memset(M.data()+at,0,0x40);SET_GPR_U32(c,2,at);ret(c);}
void spokeBeginStub(uint8_t*,R5900Context* c,PS2Runtime*){if(GPR_U32(c,4)!=DIRECTOR)throw std::runtime_error("SPOKE director");++spokeBegins;ret(c);}
void spokeEndStub(uint8_t*,R5900Context* c,PS2Runtime*){if(GPR_U32(c,4)!=DIRECTOR)throw std::runtime_error("SPOKE director");++spokeEnds;ret(c);}
void setDefaultStub(uint8_t*,R5900Context* c,PS2Runtime*){if(GPR_U32(c,5)!=0x3D)throw std::runtime_error("Guard default differs");++defaults;ret(c);}
void requestStub(uint8_t*,R5900Context* c,PS2Runtime*){requests.push_back({GPR_U32(c,5),GPR_U32(c,6),c->f[12]});ret(c);}

R5900Context context(uint32_t pc,uint32_t a0,uint32_t a1=0){
    R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);
    SET_GPR_U32(&c,4,a0);SET_GPR_U32(&c,5,a1);SET_GPR_U32(&c,28,GP);SET_GPR_U32(&c,29,STACK);SET_GPR_U32(&c,31,SENTINEL);
    return c;
}
void setVtableSlot(uint32_t vtable,unsigned slot,uint32_t fn){put(vtable+slot,uint16_t(0));put(vtable+slot+2,uint16_t(0));put(vtable+slot+4,fn);}
std::mt19937 rng(0x178bb0);
float uniform(float lo,float hi){return lo+(hi-lo)*float(rng()%1000001)/1000000.f;}
Quad randomQuad(float scale,float w){return {uniform(-scale,scale),uniform(-scale,scale),uniform(-scale,scale),w};}
OriginalCameraInput inputFromTarget(int32_t riders){
    OriginalCameraInput in;in.headPosition=T.head;in.velocity=T.velocity;in.riderForward=T.forward;in.boostLevel=T.boost;in.jumpCharge=T.charge;
    in.surfaceId=T.surface;in.motionMode=T.mode;in.raceRiderCount=riders;return in;
}
void randomTarget(){
    T.head={uniform(-150000,150000),uniform(-150000,150000),uniform(0,100000),1};
    float speed=rng()%8==0?0.f:rng()%8==1?uniform(0,30):uniform(0,3500);float heading=uniform(-3.2f,3.2f),climb=uniform(-.6f,.6f);
    T.velocity={speed*std::cos(heading)*std::cos(climb),speed*std::sin(heading)*std::cos(climb),speed*std::sin(climb),0};
    if(rng()%16==0)T.velocity={0,0,uniform(-500,500),0};
    float yaw=uniform(-3.2f,3.2f);T.forward={std::cos(yaw),std::sin(yaw),rng()%4==0?uniform(-.3f,.3f):0.f,0};
    if(rng()%32==0)T.forward={0,0,1,0};
    static const float boosts[]={0,.25f,.625f,1};T.boost=boosts[rng()%4];T.charge=rng()%3==0?0.f:uniform(0,1);
    T.surface=int32_t(rng()%16);T.mode=int32_t(rng()%6);
}
unsigned compareAlgorithm(const OriginalChaseAlgorithmState& s,uint32_t base,unsigned n,const char* what){
    unsigned bad=0;auto words=toWords(s);
    visitFields(s,[&](unsigned off,const auto&){uint32_t expected=get<uint32_t>(base+off);if(expected!=words[off/4]){if(bad++<4)std::printf("%s case %u +0x%03X original %08x (%g) native %08x (%g)\n",what,n,off,expected,f(expected),words[off/4],f(words[off/4]));}});
    for(unsigned i=0;i<4;++i)if(get<uint32_t>(base+0x390+4*i)!=u(s.postRaceDirection[i])){if(bad++<4)std::printf("%s case %u direction[%u] differs\n",what,n,i);}
    if(get<int32_t>(base+0x3A0)!=s.postRacePhase){if(bad++<4)std::printf("%s case %u phase %d native %d\n",what,n,get<int32_t>(base+0x3A0),s.postRacePhase);}
    return bad;
}
}

int main(int argc,char** argv){
    if(argc!=2)return 2;
    std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(file)),{});
    std::memcpy(M.data()+0xff000,elf.data(),elf.size());
    PS2Runtime rt;
    for(uint32_t a:{0x1789e8u,0x178bb0u,0x178e90u,0x178ba8u,0x178b98u})rt.registerFunction(a,sub_001789E8_0x1789e8);
    rt.registerFunction(0x1624e8,sub_00162458_0x162458);rt.registerFunction(0x166f28,sub_00166F28_0x166f28);
    rt.registerFunction(0x166c60,sub_00166C60_0x166c60);rt.registerFunction(0x166550,sub_00166550_0x166550);
    rt.registerFunction(0x166228,sub_00165938_0x165938);rt.registerFunction(0x166640,sub_00166640_0x166640);
    rt.registerFunction(0x166530,sub_00166530_0x166530);rt.registerFunction(0x168150,sub_00168150_0x168150);
    rt.registerFunction(0x166f90,sub_00166F90_0x166f90);rt.registerFunction(0x31b748,sub_0031B748_0x31b748);
    rt.registerFunction(0x31b7a8,sub_0031B7A8_0x31b7a8);rt.registerFunction(0x31be50,sub_0031BE50_0x31be50);
    rt.registerFunction(0x31c040,sub_0031C040_0x31c040);rt.registerFunction(0x31c128,sub_0031C128_0x31c128);
    rt.registerFunction(0x31c228,sub_0031C228_0x31c228);rt.registerFunction(0x162568,sub_00162568_0x162568);
    rt.registerFunction(0x161ab0,sub_00161AB0_0x161ab0);rt.registerFunction(0x161bb8,sub_00161AB0_0x161ab0);
    rt.registerFunction(0x15ca50,sub_0015CA50_0x15ca50);rt.registerFunction(0x161e58,sub_00161E58_0x161e58);
    rt.registerFunction(0x15c988,sub_0015C988_0x15c988);rt.registerFunction(0x15cb08,sub_0015CB08_0x15cb08);
    rt.registerFunction(0x15e668,sub_0015E668_0x15e668);rt.registerFunction(0x1673a0,sub_001673A0_0x1673a0);
    rt.registerFunction(0x167fa0,sub_00167E30_0x167e30);
    rt.registerFunction(0x11fe98,motionMode);rt.registerFunction(0x162318,baseCtor);
    rt.registerFunction(0x1621a8,spokeBeginStub);rt.registerFunction(0x162218,spokeEndStub);rt.registerFunction(0x161ef0,setDefaultStub);
    rt.registerFunction(0x162060,requestStub);rt.registerFunction(0x317e50,freeStub);rt.registerFunction(0x317d70,allocStub);
    rt.registerFunction(0x110100,targetHead);rt.registerFunction(0x110200,targetVelocity);rt.registerFunction(0x110300,targetForward);
    rt.registerFunction(0x110400,targetBoost);rt.registerFunction(0x110500,targetCharge);rt.registerFunction(0x110600,targetSurface);
    rt.registerFunction(0x110700,targetTrap);rt.registerFunction(0x110800,cameraTarget);
    rt.registerFunction(0x112000,algStep);rt.registerFunction(0x112100,algDtor);rt.registerFunction(0x112200,algSetTarget);
    // Runtime-initialised constant quads (0x4FF120..) and the identity used by 0x166F90.
    put(0x4ff120,Quad{0,0,0,0});put(0x4ff130,Quad{0,0,0,1});put(0x4ff140,Quad{1,0,0,0});put(0x4ff150,Quad{0,1,0,0});put(0x4ff160,Quad{0,0,1,0});
    put(0x4ff1a0,std::array<float,16>{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});
    put(GP-0x6a0,0u);
    // Target object, its vtable, rider (+4) and the rider's camera-target provider at +0x6C0.
    for(unsigned slot=0;slot<0xC8;slot+=8)setVtableSlot(TARGET_VT,slot,0x110700);
    setVtableSlot(TARGET_VT,0x08,0x110100);setVtableSlot(TARGET_VT,0x18,0x110200);setVtableSlot(TARGET_VT,0x20,0x110300);
    setVtableSlot(TARGET_VT,0x88,0x110400);setVtableSlot(TARGET_VT,0x90,0x110500);setVtableSlot(TARGET_VT,0xA0,0x110600);
    put(TARGET,TARGET_VT);put(TARGET+4,RIDER);put(RIDER+0x6c0,CAMERA_TARGET_VT);setVtableSlot(CAMERA_TARGET_VT,0x38,0x110800);
    // Game info roster count *(*(*(gp-0x848)+0x84)+0xC)+0x78 and level manager *(world+0x28).
    put(GP-0x848,0x50000u);put(0x50084,0x51000u);put(0x5100c,0x52000u);put(0x51028,0x53000u);
    setVtableSlot(FAKE_ALG_VT,0x48,0x112000);setVtableSlot(FAKE_ALG_VT,0x08,0x112100);setVtableSlot(FAKE_ALG_VT,0x20,0x112200);

    // ---- A. POST_RACE_1 per-frame step 0x1624E8 (with pending set-target) ----
    unsigned mismatchA=0,resets=0,groups=0;
    for(unsigned n=0;n<20000;++n){
        randomTarget();put(RIDER+0x1b0,T.forward);
        static const int32_t rosters[]={1,2,6,0};int32_t riders=rosters[rng()%4];put(0x52078,riders);groups+=riders>=2;
        OriginalChaseAlgorithmState s;
        visitFields(s,[&](unsigned,auto& v){using V=std::remove_reference_t<decltype(v)>;if constexpr(std::is_same_v<V,float>)v=uniform(-600,600);else v=int32_t(rng()%3);});
        s.ringIndex=int32_t(rng()%5);s.resetPending=rng()%4==0;resets+=s.resetPending;
        s.lookAt=T.head;s.eye=randomQuad(400,1);s.eye[0]+=T.head[0];s.eye[1]+=T.head[1];s.eye[2]+=T.head[2];
        s.variantType=0x44;float yaw=uniform(-3.2f,3.2f);s.postRaceDirection={std::cos(yaw),std::sin(yaw),0,0};
        s.postRacePhase=n%9==0?1099:n%9==1?0:n%9==2?274+int32_t(n%3):int32_t(rng()%1100);
        std::memset(M.data()+ALG,0,0x3b0);auto words=toWords(s);std::memcpy(M.data()+ALG,words.data(),words.size()*4);
        put(ALG+0xc,0x44u);put(ALG+0x10,0x45cbd8u);put(ALG+0x30,TARGET);put(ALG+0x390,s.postRaceDirection);put(ALG+0x3a0,s.postRacePhase);
        auto in=inputFromTarget(riders);
        R5900Context c=context(0x1624e8,ALG,0);
        {original_camera::Rounding rounding;sub_00162458_0x162458(M.data(),&c,&rt);original_camera::stepAlgorithm(s,in);}
        if(c.pc!=SENTINEL)throw std::runtime_error("POST_RACE_1 step control flow differs");
        mismatchA+=compareAlgorithm(s,ALG,n,"POST_RACE_1 step")!=0;
    }
    if(mismatchA)throw std::runtime_error("POST_RACE_1 step differs in "+std::to_string(mismatchA)+" cases");
    std::printf("A: 20000 original POST_RACE_1 steps (0x1624E8: %u pending set-targets 0x178E90/0x166C60/0x166550, driver 0x178BB0, finish 0x166228) match every decoded word, direction and phase; %u pulled-back roster cases.\n",resets,groups);

    // ---- B. POST_RACE_1 ctor 0x1789E8 (base ctor 0x162318 is a boundary) ----
    unsigned fallbackForward=0,fallbackAxis=0;
    for(unsigned n=0;n<20000;++n){
        randomTarget();
        if(n%10==0)T.velocity={uniform(-.7f,.7f),uniform(-.7f,.7f),0,0};
        if(n%10==1)T.forward={0,0,1,0};
        if(n%10==2){T.velocity={0,0,0,0};T.forward={uniform(-.005f,.005f),uniform(-.005f,.005f),1,0};}
        std::memset(M.data()+0x60000,0,0x3b0);baseCtorCalls=0;
        auto in=inputFromTarget(6);OriginalChaseAlgorithmState s;
        R5900Context c=context(0x1789e8,0x60000,TARGET);R5900Context* cp=&c;
        {original_camera::Rounding rounding;sub_001789E8_0x1789e8(M.data(),&c,&rt);original_camera::postRaceConstruct(s,in);}
        if(c.pc!=SENTINEL||baseCtorCalls!=1||GPR_U32(cp,2)!=0x60000)throw std::runtime_error("POST_RACE_1 ctor control flow differs");
        if(get<uint32_t>(0x6000c)!=0x44||get<uint32_t>(0x60010)!=0x45cbd8||get<int32_t>(0x603a0)!=0||std::memcmp(M.data()+0x60390,s.postRaceDirection.data(),16)){
            std::printf("ctor case %u original %g %g %g %g native %g %g %g %g\n",n,get<float>(0x60390),get<float>(0x60394),get<float>(0x60398),get<float>(0x6039c),s.postRaceDirection[0],s.postRaceDirection[1],s.postRaceDirection[2],s.postRaceDirection[3]);
            throw std::runtime_error("POST_RACE_1 ctor differs");
        }
        fallbackForward+=!(1.f<std::sqrt(T.velocity[0]*T.velocity[0]+T.velocity[1]*T.velocity[1]+T.velocity[2]*T.velocity[2]));fallbackAxis+=s.postRaceDirection==Quad{1,0,0,0};
    }
    std::printf("B: 20000 original POST_RACE_1 constructions match type, vtable, phase and direction; %u forward fallbacks, %u +X fallbacks.\n",fallbackForward,fallbackAxis);

    // ---- C. Director update 0x161BB8 (transitions 0x161AB0, node pass, 0x15CA50 unlink, auto-revert) ----
    unsigned removed=0,stepped=0,spokeCalls=0,reverts=0,guardResets=0;
    for(unsigned n=0;n<20000;++n){
        randomTarget();
        if(n%3==0){float kmh=uniform(33,37);float s=kmh/0.036f;T.velocity={s,0,0,0};}
        if(n%4==0)T.mode=5;
        OriginalCameraDirectorState d;
        d.currentType=rng()%8==0?0x4A:rng()%2?0x44:0x3D;d.guardType=rng()%6==0?0x3C:0x3D;d.preferredType=rng()%3?0x3D:0x3E;
        d.lastRequestedType=rng()%2?0x5D:0x3D;d.flags=rng()%8;d.revertTimer=rng()%3==0?uniform(9.95f,10.05f):uniform(0,10);d.spokeLatch=rng()%2;
        original_camera_director::Frame frame;int32_t state=int32_t(rng()%12)-1;frame.raceStateActive=state!=0&&state<10;frame.managerField484=int32_t(rng()%2);
        if(rng()%4==0)d.currentType=d.preferredType;
        unsigned count=1+rng()%4;
        static const float rates[]={1.f,original_camera_director::finishRate,original_camera_director::spokeReturnRate,std::bit_cast<float>(0x3C888889u),.25f};
        for(unsigned i=0;i<count;++i){
            OriginalCameraDirectorNode node;node.algorithm.variantType=i&&rng()%10==0?0x4A:(rng()%2?0x44:0x3D);
            node.rate=i==0?rates[rng()%5]:uniform(0,1);
            node.weight=rng()%6==0?0.f:rng()%6==1?1.f:uniform(-.3f,1.3f);
            if(i&&rng()%8==0)node.weight=d.nodes.empty()?0.f:(rng()%2?1.f:0.f);
            node.smoothed=uniform(-1,1);d.nodes.push_back(node);
        }
        if(count>1&&rng()%6==0)d.nodes[1].weight=d.nodes[0].rate;
        std::memset(M.data()+DIRECTOR,0,0x50);
        put(DIRECTOR+0x08,LIST);put(DIRECTOR+0x18,d.currentType);put(DIRECTOR+0x1c,d.guardType);put(DIRECTOR+0x20,TARGET);put(DIRECTOR+0x24,d.preferredType);
        put(DIRECTOR+0x2c,d.lastRequestedType);put(DIRECTOR+0x30,d.flags);put(DIRECTOR+0x38,d.revertTimer);put(DIRECTOR+0x3c,d.spokeLatch);
        put(0x53000,state);put(0x53484,frame.managerField484);
        put(LIST,NODES);put(LIST+4,count);
        for(unsigned i=0;i<count;++i){
            uint32_t node=NODES+i*0x40,alg=ALGS+i*0x100;
            put(node,alg);put(node+4,d.nodes[i].weight);put(node+8,d.nodes[i].rate);put(node+0xc,d.nodes[i].smoothed);
            put(node+0x10,i?node-0x40:0u);put(node+0x14,i+1<count?node+0x40:0u);put(node+0x18,1u);
            put(alg+0xc,uint32_t(d.nodes[i].algorithm.variantType));put(alg+0x10,FAKE_ALG_VT);
        }
        steps.clear();dtors.clear();frees.clear();requests.clear();spokeBegins=spokeEnds=defaults=0;
        R5900Context c=context(0x161bb8,DIRECTOR);
        std::vector<std::pair<unsigned,int>> nativeSteps;std::vector<unsigned> ids;for(unsigned i=0;i<count;++i)ids.push_back(i);
        std::vector<unsigned> order=ids;
        original_camera_director::TransitionDecision t;bool revert;
        {
            original_camera::Rounding rounding;for(unsigned guard=0;c.pc!=SENTINEL&&guard<8;++guard)sub_00161AB0_0x161ab0(M.data(),&c,&rt); // internal jal 0x161AB0 returns through the dispatcher
            t=original_camera_director::decideTransitions(d.guardType,d.currentType,d.spokeLatch,T.mode,T.velocity);
            // Track node identity through the native pass: the phase field is a free tag here.
            for(unsigned i=0;i<count;++i)d.nodes[i].algorithm.postRacePhase=int32_t(i);
            original_camera_director::advanceNodes(d,[&](OriginalCameraDirectorNode& node,int a1){nativeSteps.push_back({unsigned(node.algorithm.postRacePhase),a1});});
            revert=original_camera_director::advanceRevertTimer(d,frame);
        }
        if(c.pc!=SENTINEL){std::printf("director case %u pc %08x\n",n,c.pc);throw std::runtime_error("Director update control flow differs");}
        bool bad=false;
        if(get<int32_t>(DIRECTOR+0x3c)!=t.latch||get<int32_t>(DIRECTOR+0x1c)!=0x3D||spokeBegins!=unsigned(t.spokeBegin)||spokeEnds!=unsigned(t.spokeEnd)||defaults!=unsigned(t.resetDefault))bad=true;
        if(steps.size()!=nativeSteps.size())bad=true;
        else for(size_t k=0;k<steps.size();++k)if(steps[k].first!=ALGS+nativeSteps[k].first*0x100||steps[k].second!=uint32_t(nativeSteps[k].second))bad=true;
        uint32_t node=get<uint32_t>(LIST);unsigned remaining=0;
        for(;node&&remaining<8;++remaining,node=get<uint32_t>(node+0x14)){
            if(remaining>=d.nodes.size()){bad=true;break;}
            const auto& expected=d.nodes[remaining];
            if(get<uint32_t>(node)!=ALGS+uint32_t(expected.algorithm.postRacePhase)*0x100||get<uint32_t>(node+4)!=u(expected.weight)||get<uint32_t>(node+0xc)!=u(expected.smoothed))bad=true;
        }
        if(remaining!=d.nodes.size()||get<uint32_t>(LIST+4)!=d.nodes.size()||frees.size()!=count-d.nodes.size()||dtors.size()!=frees.size())bad=true;
        if(get<uint32_t>(DIRECTOR+0x38)!=u(d.revertTimer)||requests.size()!=unsigned(revert))bad=true;
        if(revert&&(requests[0].type!=uint32_t(d.preferredType)||requests[0].argument!=0||requests[0].rate!=1.f))bad=true;
        if(bad){
            std::printf("director case %u: count %u latch %d/%d spoke %u%u/%d%d default %u/%d steps %zu/%zu remaining %u/%zu timer %g/%g revert %zu/%d\n",n,count,get<int32_t>(DIRECTOR+0x3c),t.latch,spokeBegins,spokeEnds,t.spokeBegin,t.spokeEnd,defaults,t.resetDefault,
                steps.size(),nativeSteps.size(),remaining,d.nodes.size(),get<float>(DIRECTOR+0x38),d.revertTimer,requests.size(),revert);
            throw std::runtime_error("Director update differs");
        }
        removed+=count-d.nodes.size();stepped+=steps.size();spokeCalls+=spokeBegins+spokeEnds;reverts+=revert;guardResets+=defaults;
    }
    std::printf("C: 20000 original director updates (0x161BB8/0x161AB0/0x15CA50) match latch, transition calls, step order and a1, surviving node weights/smoothstep, unlinks and revert timer; %u steps, %u unlinks, %u SPOKE calls, %u guard resets, %u reverts.\n",stepped,removed,spokeCalls,guardResets,reverts);

    // ---- D. List insertion 0x161E58 (0x15C988 push, 0x15CB08 clear) ----
    unsigned clears=0,forced=0;
    for(unsigned n=0;n<20000;++n){
        OriginalCameraDirectorState d;unsigned count=rng()%4;
        for(unsigned i=0;i<count;++i){OriginalCameraDirectorNode node;node.algorithm.variantType=0x3D;node.weight=uniform(0,1);node.rate=uniform(0,1);node.smoothed=uniform(0,1);node.algorithm.postRacePhase=int32_t(i);d.nodes.push_back(node);}
        float rate=rng()%3==0?1.f:rng()%3==0?original_camera_director::finishRate:uniform(0,2);
        OriginalChaseAlgorithmState incoming;incoming.variantType=rng()%2?0x44:0x3C;incoming.postRacePhase=99;
        std::memset(M.data()+DIRECTOR,0,0x50);put(DIRECTOR+0x08,LIST);put(DIRECTOR+0x18,0x3Du);
        put(LIST,count?NODES:0u);put(LIST+4,count);
        for(unsigned i=0;i<count;++i){
            uint32_t node=NODES+i*0x40,alg=ALGS+i*0x100;
            put(node,alg);put(node+4,d.nodes[i].weight);put(node+8,d.nodes[i].rate);put(node+0xc,d.nodes[i].smoothed);
            put(node+0x10,i?node-0x40:0u);put(node+0x14,i+1<count?node+0x40:0u);put(node+0x18,1u);put(alg+0x10,FAKE_ALG_VT);
        }
        std::memset(M.data()+NEW_ALG,0,0x40);put(NEW_ALG+0xc,uint32_t(incoming.variantType));put(NEW_ALG+0x10,FAKE_ALG_VT);
        setTargets.clear();frees.clear();dtors.clear();allocations.clear();bump=0x70000;
        R5900Context c=context(0x161e58,DIRECTOR,NEW_ALG);c.f[12]=rate;
        unsigned nativeSetTargets=0;
        {original_camera::Rounding rounding;sub_00161E58_0x161e58(M.data(),&c,&rt);original_camera_director::insertAlgorithm(d,incoming,rate,[&](OriginalChaseAlgorithmState& a){if(a.postRacePhase!=99)throw std::runtime_error("set-target on old node");++nativeSetTargets;});}
        bool bad=c.pc!=SENTINEL||get<int32_t>(DIRECTOR+0x18)!=d.currentType||setTargets.size()!=nativeSetTargets||(nativeSetTargets&&setTargets[0]!=NEW_ALG)||allocations.size()!=1||allocations[0]!=0x1c;
        uint32_t node=get<uint32_t>(LIST);unsigned k=0;
        for(;node&&k<8;++k,node=get<uint32_t>(node+0x14)){
            if(k>=d.nodes.size()){bad=true;break;}
            const auto& e=d.nodes[k];uint32_t alg=e.algorithm.postRacePhase==99?NEW_ALG:ALGS+uint32_t(e.algorithm.postRacePhase)*0x100;
            if(get<uint32_t>(node)!=alg||get<uint32_t>(node+4)!=u(e.weight)||get<uint32_t>(node+8)!=u(e.rate)||get<uint32_t>(node+0x18)!=1)bad=true;
            if(k==0&&(get<uint32_t>(node+0x10)!=0))bad=true;
        }
        if(k!=d.nodes.size()||get<uint32_t>(LIST+4)!=d.nodes.size()||frees.size()!=count+1-d.nodes.size())bad=true;
        if(bad){std::printf("insert case %u count %u rate %g: nodes %u/%zu setTargets %zu/%u\n",n,count,rate,k,d.nodes.size(),setTargets.size(),nativeSetTargets);throw std::runtime_error("Director insertion differs");}
        clears+=count&&rate==1.f;forced+=!count&&rate!=1.f;
    }
    std::printf("D: 20000 original list insertions (0x161E58/0x15C988/0x15CB08) match type, order, weight, rate, links, frees and one set-target per push; %u clears, %u empty-list rate-1 pushes.\n",clears,forced);

    // ---- E. Compositor 0x15E668 gather and fov/near/far blend ----
    unsigned blended=0,single=0,skipped=0;
    for(unsigned n=0;n<20000;++n){
        OriginalCameraDirectorState d;unsigned count=1+rng()%4;if(n%5==0)count=1;
        for(unsigned i=0;i<count;++i){
            OriginalCameraDirectorNode node;node.algorithm.variantType=rng()%12==0?0x4A:0x3D;
            node.algorithm.outputEye=randomQuad(100000,1);node.algorithm.lookAt=randomQuad(100000,1);
            node.algorithm.fov=uniform(0,1.2f);node.algorithm.near=uniform(0,60);node.algorithm.far=uniform(0,40000);
            node.weight=rng()%5==0?0.f:uniform(-.2f,1.2f);node.smoothed=rng()%5==0?(rng()%2?original_camera_director::blendMinimum:0.f):uniform(-.1f,1.1f);
            d.nodes.push_back(node);skipped+=!(original_camera_director::blendMinimum<node.smoothed);
        }
        std::memset(M.data()+OUTER,0,0x500);put(OUTER+0xa0,DIRECTOR);put(OUTER+0xa8,DIRECTOR);
        std::memset(M.data()+DIRECTOR,0,0x50);put(DIRECTOR+0x08,LIST);put(LIST,NODES);put(LIST+4,count);
        for(unsigned i=0;i<count;++i){
            uint32_t node=NODES+i*0x40,alg=ALGS+i*0x100;const auto& a=d.nodes[i].algorithm;
            put(node,alg);put(node+4,d.nodes[i].weight);put(node+8,0.f);put(node+0xc,d.nodes[i].smoothed);
            put(node+0x10,i?node-0x40:0u);put(node+0x14,i+1<count?node+0x40:0u);put(node+0x18,1u);
            std::memset(M.data()+alg,0,0x100);put(alg,a.fov);put(alg+4,a.near);put(alg+8,a.far);put(alg+0xc,uint32_t(a.variantType));put(alg+0x10,0x45ca38u);
            put(alg+0x20,a.lookAt);put(alg+0x60,a.outputEye);put(alg+0x70,Quad{0,0,0,1});
        }
        R5900Context c=context(0x15e668,OUTER);
        Quad eye,lookAt;bool one=original_camera_director::singleAlgorithm(d);
        {original_camera::Rounding rounding;sub_0015E668_0x15e668(M.data(),&c,&rt);
         if(one){eye=d.nodes[0].algorithm.outputEye;lookAt=d.nodes[0].algorithm.lookAt;}else{original_camera_director::blendEyeLookAt(d.nodes,eye,lookAt);eye[3]=1;lookAt[3]=1;}}
        if(c.pc!=0x15e914||std::memcmp(M.data()+OUTER+0x100,eye.data(),16)||std::memcmp(M.data()+OUTER+0xe0,lookAt.data(),16)){
            std::printf("gather case %u count %zu single %d eye %g %g %g / %g %g %g\n",n,d.nodes.size(),one,get<float>(OUTER+0x100),get<float>(OUTER+0x104),get<float>(OUTER+0x108),eye[0],eye[1],eye[2]);
            throw std::runtime_error("Compositor gather differs");
        }
        c=context(0x15eb08,0);SET_GPR_U32(&c,17,OUTER);c.f[6]=1.f;put(STACK+0x6c,0.f);
        float fov,near,far;
        {original_camera::Rounding rounding;sub_0015E668_0x15e668(M.data(),&c,&rt);original_camera_director::blendLens(d.nodes,fov,near,far);}
        if(c.pc!=0x15ebbc||get<uint32_t>(OUTER)!=u(fov)||get<uint32_t>(OUTER+4)!=u(near)||get<uint32_t>(OUTER+8)!=u(far)){
            std::printf("lens case %u: %g %g %g / %g %g %g\n",n,get<float>(OUTER),get<float>(OUTER+4),get<float>(OUTER+8),fov,near,far);throw std::runtime_error("Compositor lens blend differs");
        }
        blended+=!one;single+=one;
    }
    std::printf("E: 20000 original compositor gathers (0x15E668 single copy and running-mean blend) and fov/near/far blends match; %u blended, %u single, %u nodes under the 1e-4 threshold.\n",blended,single,skipped);
}
