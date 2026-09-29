// Development-only direct-original conformance. Never linked into the app.
#include "ps2_runtime_macros.h"
#include "ground_motion.hpp"
#include "ground_motion_golden.hpp"
#include <bit>
#include <cstdio>
#include <fstream>
#include <random>
#include <cfenv>

void sub_0011B3F8_0x11b3f8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00113E80_0x113e80(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0013D818_0x13d818(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BE50_0x31be50(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0013C878_0x13c878(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0013CCF0_0x13ccf0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0013D028_0x13d028(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0013C948_0x13c948(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C228_0x31c228(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00113F88_0x113f88(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001211F8_0x1211f8(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x110000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x320000;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=0x210000/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x210000/4]={};
static float speedStat,edgeStat,topSpeedStat;
static int motionMode;
static int animationIndex;
static void fixtureDependency(uint8_t*,R5900Context* ctx,PS2Runtime*) {
    // The native API takes these resolved external statistics as explicit inputs.
    // Only dependencies are fixtures; the original force routine itself is unmodified.
    if(ctx->pc==0x11fe98) SET_GPR_U32(ctx,2,motionMode);
    else if(ctx->pc==0x1494c0)ctx->f[0]=topSpeedStat;
    else if (ctx->pc==0x312aa0) SET_GPR_U32(ctx,2,animationIndex);
    else if (ctx->pc==0x1493d8) ctx->f[0]=speedStat;
    else if (ctx->pc==0x148d80||ctx->pc==0x148e68) ctx->f[0]=edgeStat;
    else if (ctx->pc==0x1494c0||ctx->pc==0x148e68) ctx->f[0]=.5f; // Return values unused by these source helpers.
    else SET_GPR_U32(ctx,2,0x90000);
    ctx->pc=GPR_U32(ctx,31);
}
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    std::ofstream goldens(argv[2],std::ios::binary);
    uint32_t goldenHeader[]={ssx::GroundGoldenMagic,ssx::GroundGoldenVersion,uint32_t(sizeof(ssx::GroundGoldenCase)),ssx::GroundGoldenCount};
    goldens.write(reinterpret_cast<const char*>(goldenHeader),sizeof(goldenHeader));
    std::ifstream file(argv[1],std::ios::binary);
    std::vector<uint8_t> ram((std::istreambuf_iterator<char>(file)),{});
    if(ram.size()!=32*1024*1024)throw std::runtime_error("Expected32MB development EE fixture");
    PS2Runtime runtime;
    for(uint32_t address:{0x14dc80,0x14dd58,0x1493d8,0x148d80,0x1494c0,0x148e68,0x312aa0,0x120378,0x125970,0x11fe98})
        if(!runtime.registerFunction(address,fixtureDependency))throw std::runtime_error("Register dependency");
    if(!runtime.registerFunction(0x31c228,sub_0031C228_0x31c228))throw std::runtime_error("Register atan");
    for (auto pair:std::array<std::pair<uint32_t,PS2Runtime::RecompiledFunction>,5>{{
        {0x13c878,sub_0013C878_0x13c878},{0x13c948,sub_0013C948_0x13c948},{0x13ccf0,sub_0013CCF0_0x13ccf0},
        {0x13d028,sub_0013D028_0x13d028},{0x31be50,sub_0031BE50_0x31be50}}})
        if(!runtime.registerFunction(pair.first,pair.second))throw std::runtime_error("Register original helper");
    auto write=[&](uint32_t at,const auto& value){std::memcpy(ram.data()+at,&value,sizeof(value));};
    constexpr uint32_t ctrl=0x60000,rider=0x61000,surface=0x63000,curve=0x64000,stack=0x80000,done=0x12345678,gp=0x4a30f0;
    write(ctrl+0x18,rider);write(gp-0x1fb0,curve);
    std::mt19937 random(0x47524f55);std::uniform_real_distribution<float> unit(0,1),speed(-4000,4000);
    ssx::GroundSurface s;s.gravity=1300.850341796875f;s.powderDamping=5.005756855010986f;s.lateralDrag=3.4996039867401123f;
    s.slipFriction={{{0,0},{30,.05f},{80,.11666599661111832f},{120,.1333329975605011f}}};
    ssx::GroundCurve lateral={{{0,.8f},{30,1.1f},{80,.9f},{120,.2f}}};
    write(surface,s.gravity);write(surface+8,s.lateralDrag);write(surface+0x1c,s.powderDamping);
    std::memcpy(ram.data()+surface+0x90,s.slipFriction.data(),32);std::memcpy(ram.data()+curve,lateral.data(),32);
    auto context=[&](uint32_t entry){R5900Context ctx{};ctx.vu0_vf[0]=_mm_set_ps(1,0,0,0);
        SET_GPR_U32(&ctx,4,ctrl);SET_GPR_U32(&ctx,5,surface);SET_GPR_U32(&ctx,29,stack);SET_GPR_U32(&ctx,31,done);SET_GPR_U32(&ctx,28,gp);ctx.pc=entry;return ctx;};
    auto check=[&](const char* label,int test,float expected,float actual){
        if(std::bit_cast<uint32_t>(expected)!=std::bit_cast<uint32_t>(actual)) {
            std::fprintf(stderr,"%s case%d original%.9g (%08x) native%.9g (%08x)\n",label,test,expected,std::bit_cast<uint32_t>(expected),actual,std::bit_cast<uint32_t>(actual));
            throw std::runtime_error("Original ground mismatch");
        }
    };
    std::fesetround(FE_TOWARDZERO);
    for(int i=0;i<12000;++i) {
        ssx::GroundForceState r;r.depth1=.1f+unit(random)*2;r.depth3=r.depth1+.1f+unit(random)*5;
        r.distance=(unit(random)*3-2)*r.depth3;r.normalSpeed=speed(random)*.1f;
        if(i%7==0)r.distance=0;if(i%7==1)r.distance=-r.depth1;if(i%7==2)r.distance=-r.depth3;
        r.forwardSpeed=speed(random);r.lateralSpeed=speed(random)*.2f;r.brake=unit(random);r.turn=unit(random)*2-1;
        r.normalZ=unit(random)*1.2f-.1f;r.relativeVerticalSpeed=speed(random)*.2f;r.boost=i%2?unit(random):0;
        r.state320Equals324=i%3==0;s.id=(i%5==0)?2:(i%5==1)?3:(i%5==2)?13:0;
        speedStat=unit(random);edgeStat=unit(random);
        write(ctrl+4,r.depth1);write(ctrl+8,r.depth3);write(rider+0x454,r.distance);write(rider+0x214,r.brake);
        write(rider+0x378,r.normalZ);write(rider+0x1e8,r.relativeVerticalSpeed);write(rider+0x3d8,0.f);
        write(rider+0x2fc,r.boost);write(rider+0x438,s.id);write(rider+0x320,uint32_t(0));write(rider+0x324,uint32_t(r.state320Equals324?0:1));
        auto ctx=context(0x13c878);ctx.f[12]=r.distance;ctx.f[13]=r.normalSpeed;
        sub_0013C878_0x13c878(ram.data(),&ctx,&runtime);if(ctx.pc!=done)throw std::runtime_error("Normal return");
        auto result=ssx::groundNormalResponse(s,r);check("normal",i,ctx.f[0],result.acceleration);
        float compression;std::memcpy(&compression,ram.data()+rider+0x758,4);check("compression",i,compression,result.compression);
        ctx=context(0x13ccf0);ctx.f[12]=r.forwardSpeed;ctx.f[13]=1;
        sub_0013CCF0_0x13ccf0(ram.data(),&ctx,&runtime);if(ctx.pc!=done)throw std::runtime_error("Forward return");
        check("forward",i,ctx.f[0],ssx::groundForwardFriction(s,r,speedStat,edgeStat));
        ctx=context(0x13d028);ctx.f[12]=r.lateralSpeed;ctx.f[13]=r.forwardSpeed;ctx.f[14]=-r.turn;
        sub_0013D028_0x13d028(ram.data(),&ctx,&runtime);if(ctx.pc!=done)throw std::runtime_error("Lateral return");
        check("lateral",i,ctx.f[0],ssx::groundLateralResponse(s,r,lateral));
        ssx::GroundDriveState drive;
        drive.velocity={speed(random)*.4f,speed(random)*.4f,speed(random)*.4f};
        drive.forwardX=i%7?unit(random)*2-1:0;drive.forwardY=i%9?unit(random)*2-1:0;
        drive.headingOffset=unit(random)*12-6;drive.forwardZ=unit(random)*2-1;drive.normalZ=r.normalZ;
        drive.brake=i%3?0:r.brake;drive.turn=i%4?0:r.turn;drive.crouch=unit(random);drive.boost=unit(random);
        drive.boostWindow=i%3?0:1;drive.modeTiming=i%5?-1:0;drive.forceHeadingBoost=i%4==0;
        drive.autoBoostSpeed=unit(random)*100;drive.autoBoostFactor=unit(random)*3;
        drive.riderType=i%28;drive.animationIndex=i%2?22:4;animationIndex=drive.animationIndex;
        write(rider+0x1b0,drive.forwardX);write(rider+0x1b4,drive.forwardY);write(rider+0x4cc,drive.headingOffset);
        write(rider+0x3a8,drive.forwardZ);write(rider+0x214,drive.brake);write(rider+0x1f0,drive.turn);
        write(rider+0x220,drive.crouch);write(rider+0x2fc,drive.boost);write(rider+0x2e8,drive.boostWindow);
        write(rider+0x470,drive.modeTiming);write(rider+0x434,drive.riderType);write(rider+0x2d4,uint16_t(drive.forceHeadingBoost?0x10:0));
        write(surface+0x20,drive.autoBoostSpeed);write(surface+0x24,drive.autoBoostFactor);
        std::memcpy(ram.data()+rider+0x1e0,drive.velocity.data(),12);write(rider+0x1ec,0.f);
        ctx=context(0x13c948);sub_0013C948_0x13c948(ram.data(),&ctx,&runtime);if(ctx.pc!=done)throw std::runtime_error("Drive return");
        check("drive",i,ctx.f[0],ssx::groundForwardDrive(drive));
        ssx::GroundControlValue turnControl{r.turn,0,0};write(rider+0x1f0,turnControl.current);
        float turnInput=unit(random)*2-1;ctx=context(0x113e80);SET_GPR_U32(&ctx,4,rider);ctx.f[12]=turnInput;
        sub_00113E80_0x113e80(ram.data(),&ctx,&runtime);
        ssx::groundTurnTarget(turnControl,turnInput,drive.velocity,s.id);
        float expectedTurn;std::memcpy(&expectedTurn,ram.data()+rider+0x1f8,4);check("turn target",i,expectedTurn,turnControl.target);
        std::memcpy(&expectedTurn,ram.data()+rider+0x1f4,4);check("turn rate",i,expectedTurn,turnControl.rate);
        float tangent=speed(random)/1000;ctx=context(0x31c228);ctx.f[12]=tangent;
        sub_0031C228_0x31c228(ram.data(),&ctx,&runtime);check("atan",i,ctx.f[0],ssx::originalAtan(tangent));
        ssx::GroundControlValue crouch{i%3?0:unit(random),unit(random),unit(random)},brake{i%4?0:unit(random)*2-1,unit(random),unit(random)};
        auto expectedC=crouch,expectedB=brake;
        std::memcpy(ram.data()+rider+0x220,&crouch,12);std::memcpy(ram.data()+rider+0x214,&brake,12);
        float ci=i%2?0:unit(random),bi=i%3?unit(random):0;
        write(rider+0x1f0,r.turn);write(rider+0x1e0,r.forwardSpeed);write(rider+0x1e4,0.f);write(rider+0x1e8,0.f);
        write(rider+0x3a0,1.f);write(rider+0x3a4,0.f);write(rider+0x3a8,0.f);write(rider+0x3ac,0.f);
        ctx=context(0x113f88);SET_GPR_U32(&ctx,4,rider);ctx.f[12]=ci;ctx.f[13]=bi;
        sub_00113F88_0x113f88(ram.data(),&ctx,&runtime);if(ctx.pc!=done)throw std::runtime_error("Control return");
        ssx::groundCrouchBrakeTargets(crouch,brake,ci,bi,r.forwardSpeed,r.turn);
        std::memcpy(&expectedC,ram.data()+rider+0x220,12);std::memcpy(&expectedB,ram.data()+rider+0x214,12);
        check("crouch target",i,expectedC.target,crouch.target);check("crouch rate",i,expectedC.rate,crouch.rate);
        check("brake target",i,expectedB.target,brake.target);check("brake rate",i,expectedB.rate,brake.rate);
        ctx=context(0x1211f8);SET_GPR_U32(&ctx,4,rider);
        sub_001211F8_0x1211f8(ram.data(),&ctx,&runtime);if(ctx.pc!=done)throw std::runtime_error("Approach return");
        ssx::groundControlApproach(crouch);ssx::groundControlApproach(brake);
        std::memcpy(&expectedC,ram.data()+rider+0x220,12);std::memcpy(&expectedB,ram.data()+rider+0x214,12);
        check("crouch current",i,expectedC.current,crouch.current);check("brake current",i,expectedB.current,brake.current);
    }
    // The complete original cruise translation prefix executes its original
    // helper calls and vector arithmetic; only unrelated getters/wind are fixtures.
    auto read32=[&](uint32_t at){uint32_t x;std::memcpy(&x,ram.data()+at,4);return x;};
    uint32_t manager=read32(read32(gp-0x848)+0x84);write(manager+0x44,surface);
    write(gp-0x1fa8,curve+64);write(gp-0x1fa0,curve+96);
    ssx::OriginalGroundProfile profile;profile.surface=s;profile.surface.id=0;
    profile.lateralSpeedCurve=lateral;profile.turnMinCurve={{{0,.8f},{30,.9f},{80,1},{120,.7f}}};
    profile.turnMaxCurve={{{0,1.1f},{30,1},{80,.9f},{120,.6f}}};
    profile.bodyScale=.85f;profile.depthTarget1=.5f;profile.depthTarget3=2.5041720867156982f;
    profile.maxTurnAngle=45.00255584716797f;profile.speedLimit=2800;profile.autoBoostSpeed=51.00103f;profile.autoBoostFactor=2.0011878f;
    std::memcpy(ram.data()+curve+64,profile.turnMinCurve.data(),32);std::memcpy(ram.data()+curve+96,profile.turnMaxCurve.data(),32);
    write(surface+12,profile.maxTurnAngle);write(surface+20,profile.depthTarget1);write(surface+24,profile.depthTarget3);
    write(rider+0x780,uint32_t(0x65000));write(0x65140,profile.bodyScale);write(rider+0x438,uint32_t(0));
    for(int i=0;i<12000;++i) {
        ssx::OriginalGroundState state;
        state.position={speed(random)*100,speed(random)*100,speed(random)*100};state.velocity={speed(random),speed(random),speed(random)};
        state.normal={0,0,1};state.forward={0,1,0};state.lateral={-1,0,0};state.physicalForward=state.forward;
        state.surfaceVelocity={speed(random)*.1f,speed(random)*.1f,0};state.depth1=.4f;state.depth3=2.1f;
        state.distance=(unit(random)*6-4);state.timeScale=unit(random)+.5f;state.turn.current=unit(random)*2-1;
        state.brake.current=i%3?unit(random):0;state.crouch.current=unit(random);state.boost=i%2?unit(random):0;
        state.flags308=i%2;state.riderType=i%28;state.animationIndex=i%2?22:5;state.modeTiming=-1;
        state.boostWindow=i%3?0:1;state.headingOffset=unit(random)*6;state.forceHeadingBoost=i%4==0;
        state.state320Equals324=i%5==0;profile.speedStat=speedStat=unit(random);profile.edgeStat=edgeStat=unit(random);animationIndex=state.animationIndex;
        write(ctrl+4,state.depth1);write(ctrl+8,state.depth3);write(rider+0x454,state.distance);write(rider+0x300,state.timeScale);
        write(rider+0x214,state.brake.current);write(rider+0x1f0,state.turn.current);write(rider+0x220,state.crouch.current);
        write(rider+0x2fc,state.boost);write(rider+0x308,state.flags308);write(rider+0x434,state.riderType);write(rider+0x470,state.modeTiming);
        write(rider+0x2e8,state.boostWindow);write(rider+0x4cc,state.headingOffset);write(rider+0x2d4,uint16_t(state.forceHeadingBoost?0x10:0));
        write(rider+0x320,uint32_t(0));write(rider+0x324,uint32_t(state.state320Equals324?0:1));write(rider+0x2e4,profile.speedLimit);
        write(surface+32,profile.autoBoostSpeed);write(surface+36,profile.autoBoostFactor);
        for(auto pair:std::array<std::pair<uint32_t,std::array<float,3>>,7>{{
            {0x110,state.position},{0x1e0,state.velocity},{0x370,state.normal},{0x3a0,state.forward},{0x3b0,state.lateral},{0x3d0,state.surfaceVelocity},{0x1b0,state.physicalForward}}}) {
            std::memcpy(ram.data()+rider+pair.first,pair.second.data(),12);write(rider+pair.first+12,0.f);
        }
        write(rider+0x11c,1.f);
        auto ctx=context(0x13d818);sub_0013D818_0x13d818(ram.data(),&ctx,&runtime);
        if(ctx.pc!=0x13e120)throw std::runtime_error("Original translation prefix boundary");
        if(i<int(ssx::GroundGoldenCount)) {
            ssx::GroundGoldenCase golden;golden.profile=profile;golden.initial=state;
            std::memcpy(golden.position.data(),ram.data()+rider+0x110,12);std::memcpy(golden.velocity.data(),ram.data()+rider+0x1e0,12);
            std::memcpy(&golden.depth1,ram.data()+ctrl+4,4);std::memcpy(&golden.depth3,ram.data()+ctrl+8,4);std::memcpy(&golden.distance,ram.data()+rider+0x454,4);
            goldens.write(reinterpret_cast<const char*>(&golden),sizeof(golden));
        }
        auto actual=state;ssx::originalGroundIntegrate(profile,actual);
        for(unsigned j=0;j<3;++j) {float expected;std::memcpy(&expected,ram.data()+rider+0x110+j*4,4);check("position",i,expected,actual.position[j]);
            std::memcpy(&expected,ram.data()+rider+0x1e0+j*4,4);check("velocity",i,expected,actual.velocity[j]);}
        auto contacted=actual;
        contacted.normal={0.f,unit(random)*.1f,1.f};
        auto previousSurface=contacted.surfaceVelocity;
        contacted.surfaceVelocity={0.f,0.f,0.f};
        std::array<float,3> relative;
        for(unsigned j=0;j<3;++j)relative[j]=actual.velocity[j]-previousSurface[j];
        std::memcpy(ram.data()+stack,relative.data(),12);write(stack+12,0.f);
        std::memcpy(ram.data()+rider+0x370,contacted.normal.data(),12);write(rider+0x37c,0.f);
        std::memcpy(ram.data()+rider+0x3d0,contacted.surfaceVelocity.data(),12);write(rider+0x3dc,0.f);
        ctx=context(0x13ebf4);SET_GPR_U32(&ctx,17,ctrl);SET_GPR_U32(&ctx,18,surface);
        sub_0013D818_0x13d818(ram.data(),&ctx,&runtime);
        ssx::originalGroundVelocityContact(contacted,0,previousSurface);
        for(unsigned j=0;j<3;++j) {float expected;std::memcpy(&expected,ram.data()+rider+0x1e0+j*4,4);check("post-contact velocity",i,expected,contacted.velocity[j]);}
        float angle=state.turn.current*2;ctx=context(0x31be50);ctx.f[12]=angle;SET_GPR_U32(&ctx,4,0x66000);SET_GPR_U32(&ctx,5,0x66004);
        sub_0031BE50_0x31be50(ram.data(),&ctx,&runtime);auto sc=ssx::originalSinCos(angle);
        for(unsigned j=0;j<2;++j){float expected;std::memcpy(&expected,ram.data()+0x66000+j*4,4);check("sincos",i,expected,sc[j]);}
    }
    for(int i=0;i<12000;++i) {
        ssx::OriginalGroundProfile p;ssx::OriginalGroundState s;
        for(unsigned j=0;j<48;++j)p.speedLimitTable[j]=40+unit(random)*80;
        p.topSpeedStat=topSpeedStat=unit(random);p.surfaceTerminalVelocity=unit(random)*2-1;p.speedLimit=unit(random)*4000;
        s.boost=unit(random)*1.5f-.1f;s.crouch.current=unit(random);s.state320Equals324=i%2;
        s.boostTierCounter=i%20;s.boostSpeedFloor=unit(random)*5000;motionMode=i%6;
        std::memcpy(ram.data()+0x4a6310,p.speedLimitTable.data(),192);write(surface+4,p.surfaceTerminalVelocity);
        write(rider+0x438,uint32_t(0));write(rider+0x2e4,p.speedLimit);write(rider+0x2fc,s.boost);write(rider+0x220,s.crouch.current);
        write(rider+0x320,uint32_t(0));write(rider+0x324,uint32_t(s.state320Equals324?0:1));write(rider+0x2f4,s.boostTierCounter);write(rider+0x30c,s.boostSpeedFloor);
        auto ctx=context(0x11b3f8);SET_GPR_U32(&ctx,4,rider);sub_0011B3F8_0x11b3f8(ram.data(),&ctx,&runtime);
        float expected;std::memcpy(&expected,ram.data()+rider+0x2e4,4);check("speed limit",i,expected,ssx::originalGroundSpeedLimit(p,s,motionMode));
    }
    std::fesetround(FE_TONEAREST);
    std::puts("12,000 direct-original cases each: normal/compression, forward friction/braking, lateral, forward drive, atan, crouch/brake filters and full cruise translation prefix/postcontact/speed-limit match float bits");
}
