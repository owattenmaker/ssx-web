// Development-only instruction oracle: original motion-5 handplant launch
// 0x139548, exit 0x139178 and update 0x1391A8 against engine/handplant.hpp.
// 0x121AA0 (air alignment), 0x11E098 and the math routines are the recompiled
// originals; the trajectory predictor 0x1135B8/0x113648 is a scripted stub.
#include "handplant_reference_common.hpp"
void sub_00139548_0x139548(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00139178_0x139178(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001391A8_0x1391a8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00121AA0_0x121aa0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011E098_0x11e098(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BB30_0x31bb30(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BE50_0x31be50(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BF60_0x31bf60(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C128_0x31c128(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C228_0x31c228(uint8_t*,R5900Context*,PS2Runtime*);
using namespace hp;
static void fail(const char* what){throw std::runtime_error(what);}
// Scripted predictor: status(k) after k steps; deterministic kinematic step.
struct Script{int initial=0,before=0,landed=1;unsigned landAt=0;int at(unsigned k)const{return k>=landAt?landed:before;}};
static Script script;
static void predictorStep(Quad& p,Quad& v){for(int k=0;k<4;k++)p[k]=p[k]+v[k]*std::bit_cast<float>(0x3c888889u);v[2]=v[2]-16.f;v[0]=v[0]*.999f;}
static std::vector<Call> originalCalls;static std::vector<Quad> originalBegin;static unsigned originalSteps=0;
static void begin(uint8_t*,R5900Context*c,PS2Runtime*){
    if(GPR_U32(c,4)!=PRED||GPR_U32(c,5)!=MOTION+0x50||GPR_U32(c,6)!=MOTION+0x60)fail("0x1135B8 arguments");
    originalCalls.push_back({0x1135b8,0,fb(c->f[12])});originalBegin={rq(MOTION+0x50),rq(MOTION+0x60)};originalSteps=0;wr(PRED+0xac,script.at(0));ret(c);
}
static void step(uint8_t*,R5900Context*c,PS2Runtime*){
    if(GPR_U32(c,4)!=PRED||GPR_U32(c,5)!=MOTION+0x50||GPR_U32(c,6)!=MOTION+0x60)fail("0x113648 arguments");
    originalCalls.push_back({0x113648,0,fb(c->f[12])});auto p=rq(MOTION+0x50),v=rq(MOTION+0x60);predictorStep(p,v);wr(MOTION+0x50,p);wr(MOTION+0x60,v);
    wr(PRED+0xac,script.at(++originalSteps));ret(c);
}
struct Case{ssx::OriginalHandplantRider r;ssx::OriginalHandplantMotionState m;ssx::RailVector normal{},heading{};};
static Case randomCase(Rand& g){
    Case k;auto& r=k.r;auto& m=k.m;
    r.position=g.vec(20000);r.positionW=1;r.quaternion=g.coin(4)?g.yaw(g.uni(-7,7),g.uni(-.5f,.5f)):g.quat();setBasis(r);
    r.forwardW=g.coin(6)?g.uni(-1,1):0;r.upW=g.coin(6)?g.uni(-1,1):0;r.velocity=g.vec(2000);r.velocityW=g.coin(6)?g.uni(-1,1):0;
    r.timeScale=g.coin(3)?1.f:g.uni(.25f,2);r.phase=g.n(8);r.reverseStance=g.n(2);r.presentedUpZ=g.uni(-1,1);
    for(auto* v:{&r.turn,&r.animationTurn,&r.extraLean,&r.lean244,&r.roll250}){v->current=g.uni(-2,2);v->rate=g.uni(-2,2);v->target=g.uni(-2,2);}
    r.handBone=g.vec(20000);
    m.start={r.position[0],r.position[1],r.position[2],g.coin(8)?g.uni(.5f,1.5f):1};
    m.lip=m.start;for(int i=0;i<3;i++)m.lip[i]+=g.uni(-400,400);m.lip[3]=g.coin(8)?g.uni(.5f,1.5f):1;
    auto t=g.unit();t[2]*=g.coin(3)?0.f:.9f;float l=std::sqrt(t[0]*t[0]+t[1]*t[1]+t[2]*t[2]);if(l<.1f)t={1,0,0};else for(auto& x:t)x/=l;
    m.tangent={t[0],t[1],t[2],g.coin(4)?-0.f:0.f};
    auto u=g.unit();m.toLip={u[0],u[1],u[2],g.coin(8)?g.uni(-1,1):0};if(g.coin(10)){m.toLip={-t[1],t[0],0,0};} // lip exactly sideways
    if(g.coin(20)){m.toLip={t[0],t[1],t[2],0};} // d == 0 against the side axis
    m.entrySpeed=g.coin(10)?0.f:g.uni(-3000,3000);m.railFlag=g.coin(4)?int32_t(g.g()):int32_t(g.n(2));m.side=g.n(2);
    auto a=g.unit();m.axis={a[0],a[1],a[2],g.coin(6)?g.uni(-1,1):0};m.rate=g.coin(10)?0.f:g.uni(-8,8);
    m.duration=g.uni(.25f,.75f);
    float stepSize=r.timeScale*std::bit_cast<float>(0x3c888889u);
    switch(g.n(4)){case 0:m.elapsed=g.uni(0,m.duration);break;case 1:m.elapsed=g.near(m.duration-stepSize,.003f);break;case 2:m.elapsed=m.duration+g.uni(0,1);break;default:m.elapsed=g.near(m.duration,.01f);}
    switch(g.n(4)){ // exit velocity: +0, -0 lanes, one nonzero lane, general
    case 0:m.exitVelocity={0,0,0,0};break;case 1:m.exitVelocity={g.coin()?-0.f:0.f,g.coin()?-0.f:0.f,g.coin()?-0.f:0.f,g.coin()?-0.f:0.f};break;
    case 2:m.exitVelocity={0,0,0,0};m.exitVelocity[g.n(4)]=g.coin()?g.uni(-900,900):std::numeric_limits<float>::denorm_min();break;default:m.exitVelocity={g.uni(-900,900),g.uni(-900,900),g.uni(-500,500),0};}
    m.predictorPosition={g.uni(-9,9),g.uni(-9,9),g.uni(-9,9),g.uni(-9,9)};m.predictorVelocity={g.uni(-9,9),g.uni(-9,9),g.uni(-9,9),g.uni(-9,9)};
    k.normal=g.unit();if(g.coin(8))k.normal={0,0,1};
    if(g.coin(10)){ // normal orthogonal to the exit velocity: dot == 0
        auto e=m.exitVelocity;k.normal={-e[1],e[0],0};float n=std::sqrt(k.normal[0]*k.normal[0]+k.normal[1]*k.normal[1]);if(n>0){k.normal[0]/=n;k.normal[1]/=n;}else k.normal={0,0,1};
    }
    k.heading=g.coin(3)?ssx::RailVector{0,0,0}:g.unit();
    return k;
}
int main(int argc,char**argv){
    if(argc!=2)return 2;auto ram=loadElf(argv[1]);M=ram.data();baseLayout();
    PS2Runtime rt;
    rt.registerFunction(0x139548,sub_00139548_0x139548);rt.registerFunction(0x121aa0,sub_00121AA0_0x121aa0);rt.registerFunction(0x11e098,sub_0011E098_0x11e098);
    rt.registerFunction(0x31bb30,sub_0031BB30_0x31bb30);rt.registerFunction(0x31be50,sub_0031BE50_0x31be50);rt.registerFunction(0x31bf60,sub_0031BF60_0x31bf60);
    rt.registerFunction(0x31c128,sub_0031C128_0x31c128);rt.registerFunction(0x31c228,sub_0031C228_0x31c228);
    rt.registerFunction(0x1135b8,begin);rt.registerFunction(0x113648,step);
    std::fesetround(FE_TOWARDZERO);Rand g(0x1391a8);
    // ---------------------------------------------------------------- 0x139548 / 0x139178
    unsigned launchCases=0,launchPhase[8]{},kept=0,flipped=0;
    for(unsigned test=0;test<40000;++test){
        auto k=randomCase(g);auto& r=k.r;auto& m=k.m;bool exitCall=test&1;
        storeRider(r);storeMotion(m);auto& expected=snapshot();
        auto c=context(exitCall?0x139178:0x139548);SET_GPR_U32(&c,4,MOTION);
        if(exitCall)sub_00139178_0x139178(M,&c,&rt);else sub_00139548_0x139548(M,&c,&rt);
        if(c.pc!=DONE){std::printf("launch continuation %x\n",c.pc);return 3;}
        auto before=m.exitVelocity;bool launched;
        if(exitCall){ssx::originalHandplantMotionExit(m,r);launched=true;}
        else {launched=ssx::originalHandplantLaunch(m,r.phase);if(unsigned(GPR_U32((&c),2))!=unsigned(launched)){std::printf("launch result mismatch case %u phase %d\n",test,r.phase);return 4;}}
        storeRider(r,&expected);storeMotion(m,&expected);
        if(!compareImage(expected,exitCall?"motion exit 0x139178":"launch 0x139548",test))return 5;
        ++launchCases;launchPhase[r.phase&7]++;kept+=sameQ(before,m.exitVelocity);
        if((r.phase==4||r.phase==5)&&!sameQ(before,m.exitVelocity))flipped++;
    }
    std::printf("handplant launch 0x139548 / exit 0x139178: %u cases match (return, +0x10 exit velocity, rider velocity); phases 0..7 %u/%u/%u/%u/%u/%u/%u/%u; exit velocity kept %u\n",
        launchCases,launchPhase[0],launchPhase[1],launchPhase[2],launchPhase[3],launchPhase[4],launchPhase[5],launchPhase[6],launchPhase[7],kept);
    (void)flipped;
    // ---------------------------------------------------------------- 0x1391A8
    unsigned updateCases=0,interpolated=0,pinned=0,launchedCount=0,noLaunch=0,aligned=0,notAligned=0,stepped=0,maxSteps=0,statusSeen[5]{};
    for(unsigned test=0;test<60000;++test){
        auto k=randomCase(g);auto& r=k.r;auto& m=k.m;
        script.initial=g.coin(8)?int(g.g()%7)-1:int(g.n(4));script.before=g.coin()?0:2;script.landed=g.coin()?1:3;script.landAt=g.coin(4)?0:g.n(20);
        storeRider(r);storeMotion(m);wr(PRED+0xac,script.initial);
        wr(PRED+0x10,k.heading);wr(PRED+0x1c,0.f);wr(PRED+0x20,k.normal);wr(PRED+0x2c,0.f);
        auto& expected=snapshot();originalCalls.clear();originalBegin.clear();originalSteps=0;
        auto c=context(0x1391a8);SET_GPR_U32(&c,4,MOTION);sub_001391A8_0x1391a8(M,&c,&rt);
        if(c.pc!=DONE){std::printf("update continuation %x\n",c.pc);return 6;}
        std::vector<Call> nativeCalls;std::vector<Quad> nativeBegin;unsigned nativeSteps=0;int status=script.initial;
        ssx::OriginalHandplantAccess a;
        a.predictorBegin=[&](Quad p,Quad v){nativeCalls.push_back({0x1135b8,0,0x45505556u});nativeBegin={p,v};nativeSteps=0;status=script.at(0);};
        a.predictorStep=[&](Quad& p,Quad& v){nativeCalls.push_back({0x113648,0,0x3c888889u});predictorStep(p,v);status=script.at(++nativeSteps);};
        a.predictorStatus=[&]{return status;};a.predictorNormal=[&]{return k.normal;};a.predictorHeading=[&]{return k.heading;};
        auto result=ssx::originalHandplantMotionUpdate(m,r,a);
        storeRider(r,&expected,true);storeMotion(m,&expected);wrTo(expected,PRED+0xac,status);
        if(!compareCalls(originalCalls,nativeCalls,"motion update",test))return 7;
        if(originalBegin.size()!=nativeBegin.size()||(originalBegin.size()&&(!sameQ(originalBegin[0],nativeBegin[0])||!sameQ(originalBegin[1],nativeBegin[1])))){std::printf("predictor begin arguments mismatch case %u\n",test);return 8;}
        if(!compareImage(expected,"motion update 0x1391A8",test))return 9;
        ++updateCases;if(result.pinned)++pinned;else ++interpolated;launchedCount+=result.launched;aligned+=result.aligned;
        if(result.pinned&&!result.launched&&sameQ(m.exitVelocity,Quad{0,0,0,0}))++noLaunch;
        if(result.pinned&&!result.launched&&!result.aligned&&(script.initial==1||script.initial==3))++notAligned;
        stepped+=result.predictorSteps>0;maxSteps=std::max(maxSteps,result.predictorSteps);statusSeen[std::clamp(script.initial,-1,3)+1]++;
    }
    std::printf("handplant motion update 0x1391A8: %u cases match (rider pose/velocity/axes, motion state, predictor calls); interpolated %u, pinned %u, launched %u, launch refused (phase) %u, air-aligned %u, landed-without-alignment %u, predictor stepped %u (max %u steps); initial status -1..3 %u/%u/%u/%u/%u\n",
        updateCases,interpolated,pinned,launchedCount,noLaunch,aligned,notAligned,stepped,maxSteps,statusSeen[0],statusSeen[1],statusSeen[2],statusSeen[3],statusSeen[4]);
}
