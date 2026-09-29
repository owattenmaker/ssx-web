// Development-only instruction oracle: original handplant entry 0x107578 with
// motion-5 setup 0x138BA0, control-11 setup 0x1329B0 and motion-5 enter 0x138B48
// against engine/handplant.hpp. The spline query and request routines are
// controlled stubs; asin/sincos/sine are the recompiled originals.
#include "handplant_reference_common.hpp"
void sub_00107578_0x107578(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00138BA0_0x138ba0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00138B48_0x138b48(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001329B0_0x1329b0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C128_0x31c128(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BE50_0x31be50(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BF60_0x31bf60(uint8_t*,R5900Context*,PS2Runtime*);
using namespace hp;
static std::vector<Call> originalCalls;
static int mode=0,animationClass=0;static bool found=false;static Quad suppliedPoint{},suppliedTangent{};static uint32_t descriptor=0;static Quad probeSeen{};
static void fail(const char* what){throw std::runtime_error(what);}
static void flag(uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=OWNER+0xd20||GPR_U32(c,5)!=2)fail("0x2F6AC8 arguments");originalCalls.push_back({0x2f6ac8,2,0});SET_GPR_U32(c,2,0);ret(c);}
static void motionGetter(uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=RIDER)fail("0x11FE98 rider");SET_GPR_U32(c,2,mode);ret(c);}
static void classGetter(uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=ANIM||GPR_U32(c,5)!=2)fail("0x311AE8 arguments");SET_GPR_U32(c,2,animationClass);ret(c);}
static void query(uint8_t*,R5900Context*c,PS2Runtime*){
    if(GPR_U32(c,4)!=WORLD||GPR_U32(c,7)!=2||fb(c->f[12])!=fb(300.f))fail("0x334680 contract (world, mask 2, reach 300)");
    probeSeen=rq(GPR_U32(c,5));originalCalls.push_back({0x334680,0,0});
    if(found){uint32_t out=GPR_U32(c,6);wr(out,suppliedPoint);wr(out+0x10,suppliedTangent);wr(out+0x58,descriptor);}
    SET_GPR_U32(c,2,found);ret(c);
}
static void requestControl(uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=RIDER)fail("0x11FEC8 rider");originalCalls.push_back({0x11fec8,int32_t(GPR_U32(c,5)),0});ret(c);}
static void requestMotion(uint8_t*,R5900Context*c,PS2Runtime*){if(GPR_U32(c,4)!=RIDER)fail("0x11FE78 rider");originalCalls.push_back({0x11fe78,int32_t(GPR_U32(c,5)),0});ret(c);}

static ssx::OriginalHandplantMotionState garbageMotion(Rand& g){
    ssx::OriginalHandplantMotionState m;auto q=[&]{return Quad{g.uni(-9,9),g.uni(-9,9),g.uni(-9,9),g.uni(-9,9)};};
    m.start=q();m.exitVelocity=q();m.tangent=q();m.lip=q();m.toLip=q();m.predictorPosition=q();m.predictorVelocity=q();m.axis=q();
    m.rate=g.uni(-9,9);m.duration=g.uni(-9,9);m.elapsed=g.uni(-9,9);m.entrySpeed=g.uni(-9,9);m.railFlag=int32_t(g.g());m.side=int32_t(g.g());return m;
}
static ssx::OriginalHandplantControlState garbageControl(Rand& g){
    ssx::OriginalHandplantControlState c;c.timer=g.uni(-9,9);c.smoothed=g.uni(-9,9);c.fast=int32_t(g.g());c.railFlag=int32_t(g.g());c.side=int32_t(g.g());return c;
}
static ssx::OriginalHandplantRider randomRider(Rand& g){
    ssx::OriginalHandplantRider r;
    r.position=g.vec(20000);r.positionW=g.coin(8)?g.uni(.5f,1.5f):1;
    switch(g.n(4)){case 0:r.quaternion=g.quat();break;case 1:r.quaternion=g.yaw(g.uni(-7,7));break;default:r.quaternion=g.yaw(g.uni(-7,7),g.coin(3)?g.near(.001f,.0008f):g.uni(-2.5f,2.5f));}
    setBasis(r);r.forwardW=g.coin(8)?g.uni(-1,1):0;r.upW=g.coin(8)?g.uni(-1,1):0;
    r.presentedUpZ=g.coin(6)?(g.coin()?-0.f:0.f):g.uni(-1,1);
    r.timeScale=g.uni(.5f,2);r.reverseStance=g.n(2);r.phase=g.n(7);
    r.handBone=r.position;for(auto& x:r.handBone)x+=g.uni(-150,150);r.handBoneW=g.coin(8)?g.uni(.5f,1.5f):1;
    for(auto* v:{&r.turn,&r.animationTurn,&r.extraLean,&r.lean244,&r.roll250}){v->current=g.uni(-2,2);v->rate=g.uni(-2,2);v->target=g.uni(-2,2);}
    return r;
}
static ssx::RailVector randomVelocity(Rand& g){
    ssx::RailVector d=g.unit();float speed;
    switch(g.n(8)){case 0:speed=g.coin(4)?0:g.uni(0,27.7f);break;case 1:speed=g.near(27.777779f,.5f);break;case 2:speed=g.near(277.77777f,40);break;
    case 3:speed=g.near(925.92596f,40);break;case 4:speed=g.near(2222.2222f,50);break;case 5:speed=g.near(2777.7778f,60);break;default:speed=g.uni(0,4000);}
    ssx::RailVector v{d[0]*speed,d[1]*speed,d[2]*speed};
    if(g.coin(5))v[2]=g.near(-555.55554f,60);
    if(g.coin(40)){float s=std::sqrt(2222.2222f*2222.2222f/(1+v[0]*v[0]+v[1]*v[1]+v[2]*v[2]));v={0,0,0};v[g.n(3)]=g.coin()?2222.2222f:std::nextafter(2222.2222f,INFINITY);(void)s;}
    return v;
}
// Float mirror of the 0x138BA0 gates, used only to steer generation and count branches.
struct SetupMirror{Quad right{};float relativeW=0;};
static SetupMirror mirrorSetup(const ssx::OriginalHandplantRider& r,Quad tangent){
    using namespace ssx::handplant_original;ssx::terrain_original::Rounding rr;SetupMirror out;Quad q=r.quaternion;
    auto axis=cross4(quad(r.up,r.upW),unitZ4);float length=vuSqrt(dot4(axis,axis));
    if(bits(0x3a83126fu)<length){axis=scale4(axis,vuReciprocal(length));q=quatMul(deltaFromSine(axis,length),q);}
    float x=q[0],y=q[1],z=q[2],w=q[3];float f1=mul(y,y),f6=mul(z,z),f2=mul(x,z),f7=mul(w,y);f1=A(f1,f6);float f0=mul(x,y),f5=mul(w,z);
    f2=S(f2,f7);f1=A(f1,f1);f0=A(f0,f5);f2=A(f2,f2);f1=S(1.f,f1);f0=A(f0,f0);out.right={f1,f0,f2,0};
    Quad flat{tangent[0],tangent[1],0,tangent[3]};flat=scale4(flat,vuRsqrt(dot4(flat,flat)));if(dot4(flat,out.right)<0)flat=scale4(flat,-1.f);
    auto turn=cross4(out.right,flat);float turnLength=vuSqrt(dot4(turn,turn));Quad target=q;
    if(bits(0x3a83126fu)<turnLength){turn=scale4(turn,vuReciprocal(turnLength));target=quatMul(deltaFromSine(turn,turnLength),q);}
    out.relativeW=quatMul(target,Quad{-r.quaternion[0],-r.quaternion[1],-r.quaternion[2],r.quaternion[3]})[3];return out;
}
int main(int argc,char**argv){
    if(argc!=2)return 2;auto ram=loadElf(argv[1]);M=ram.data();baseLayout();
    PS2Runtime rt;
    rt.registerFunction(0x138ba0,sub_00138BA0_0x138ba0);rt.registerFunction(0x1329b0,sub_001329B0_0x1329b0);
    rt.registerFunction(0x31c128,sub_0031C128_0x31c128);rt.registerFunction(0x31be50,sub_0031BE50_0x31be50);rt.registerFunction(0x31bf60,sub_0031BF60_0x31bf60);
    rt.registerFunction(0x2f6ac8,flag);rt.registerFunction(0x11fe98,motionGetter);rt.registerFunction(0x311ae8,classGetter);rt.registerFunction(0x334680,query);
    rt.registerFunction(0x11fec8,requestControl);rt.registerFunction(0x11fe78,requestMotion);
    std::fesetround(FE_TOWARDZERO);Rand g(0x107578);
    ssx::OriginalRailRecord record;
    // ---------------------------------------------------------------- 0x107578
    unsigned entryCases=0,accepted=0,flips=0,sides[2]{},stance[2]{},rails[2]{},fast[2]{},fallback=0,spanLow=0,spanHigh=0,spanMid=0,motionOne=0;unsigned rejects[8]{};
    for(unsigned test=0;test<60000;++test){
        auto r=randomRider(g);r.motionMode=mode=g.coin(2)?1:(int[]){0,2,5,13}[g.n(4)];
        r.channel2Class=animationClass=g.coin(5)?18+g.n(3):(int[]){15,16,17,21,22}[g.n(5)];
        r.velocity=randomVelocity(g);r.velocityW=g.coin(10)?g.uni(-1,1):0;
        // Probe as the original forms it (approximately) so hits land around the 300 cm gates.
        float speed=std::sqrt(r.velocity[0]*r.velocity[0]+r.velocity[1]*r.velocity[1]+r.velocity[2]*r.velocity[2]);
        ssx::RailVector dir=speed>27.777779f?ssx::RailVector{r.velocity[0]/speed,r.velocity[1]/speed,r.velocity[2]/speed}:r.forward;
        found=!g.coin(10);
        auto offset=g.unit();float reach=g.coin(4)?g.near(300,2):g.uni(0,340);
        for(int k=0;k<3;k++)suppliedPoint[k]=r.position[k]+dir[k]*300+offset[k]*reach;suppliedPoint[3]=1;
        auto t=g.unit();if(g.coin(3))t[2]=g.coin(3)?(g.coin()?1:-1)*std::bit_cast<float>(0x3f666666u):g.near(g.coin()?.9f:-.9f,.1f);
        if(g.coin(8)){for(int k=0;k<3;k++)t[k]=g.coin()?dir[k]:-dir[k];}
        suppliedTangent={t[0],t[1],t[2],0};
        bool hasDescriptor=!g.coin(3);record.flags=g.coin()?g.g():uint32_t(g.n(2));descriptor=hasDescriptor?DESC:0;wr(DESC+0x1c,record.flags);
        auto m=garbageMotion(g);auto c=garbageControl(g);
        storeRider(r);storeMotion(m);storeControl(c);wr(OWNER+0x1a4,m.side);
        auto& expected=snapshot();originalCalls.clear();
        auto ctx=context(0x107578);SET_GPR_U32(&ctx,4,RIDER);sub_00107578_0x107578(M,&ctx,&rt);
        if(ctx.pc!=DONE){std::printf("entry continuation %x\n",ctx.pc);return 3;}
        std::vector<Call> nativeCalls;ssx::RailVector nativeProbe{};ssx::OriginalHandplantAccess a;
        a.markAttempt=[&]{nativeCalls.push_back({0x2f6ac8,2,0});};
        a.query=[&](ssx::RailVector p){nativeProbe=p;nativeCalls.push_back({0x334680,0,0});ssx::OriginalRailQueryResult q;q.found=found;
            if(found){q.point={suppliedPoint[0],suppliedPoint[1],suppliedPoint[2]};q.tangent={suppliedTangent[0],suppliedTangent[1],suppliedTangent[2]};q.record=hasDescriptor?&record:nullptr;}return q;};
        a.requestControl=[&](int id){nativeCalls.push_back({0x11fec8,id,0});};a.requestMotion=[&](int id){nativeCalls.push_back({0x11fe78,id,0});};
        auto e=ssx::originalHandplantEnter(r,m,c,a);
        storeRider(r,&expected);storeMotion(m,&expected);storeControl(c,&expected);
        if(bool(GPR_U32((&ctx),2))!=e.accepted){std::printf("entry acceptance mismatch case %u original %u native %d reject %d\n",test,GPR_U32((&ctx),2),e.accepted,int(e.reject));return 4;}
        if(!compareCalls(originalCalls,nativeCalls,"entry",test))return 5;
        if(!(e.reject==ssx::OriginalHandplantEntryTest::Reject::Falling||e.reject==ssx::OriginalHandplantEntryTest::Reject::Inverted||e.reject==ssx::OriginalHandplantEntryTest::Reject::RailCycle))
            for(int k=0;k<3;k++)if(!same(probeSeen[k],nativeProbe[k])){std::printf("entry probe mismatch case %u lane %d\n",test,k);return 6;}
        if(!compareImage(expected,"entry",test))return 7;
        ++entryCases;rejects[int(e.reject)]++;motionOne+=r.motionMode==1;
        if(e.reject!=ssx::OriginalHandplantEntryTest::Reject::Falling&&e.reject!=ssx::OriginalHandplantEntryTest::Reject::Inverted&&e.reject!=ssx::OriginalHandplantEntryTest::Reject::RailCycle&&!(std::bit_cast<float>(0x41de38e4u)<e.speed))fallback++;
        if(e.accepted){
            ++accepted;flips+=!same(e.hit.tangent[0],suppliedTangent[0])||!same(e.hit.tangent[1],suppliedTangent[1])||!same(e.hit.tangent[2],suppliedTangent[2]);
            sides[m.side&1]++;stance[r.reverseStance&1]++;rails[m.railFlag&1]++;fast[c.fast&1]++;
            if(e.duration==.25f)spanLow++;else if(e.duration==.75f)spanHigh++;else spanMid++;
            // 0x138B48 motion-5 enter on the accepted state.
            auto enter=context(0x138b48);SET_GPR_U32(&enter,4,MOTION);auto& before=snapshot();sub_00138B48_0x138b48(M,&enter,&rt);
            if(enter.pc!=DONE)return 8;ssx::originalHandplantMotionEnter(m,r);storeRider(r,&before);
            if(!compareImage(before,"motion enter after entry",test))return 9;
        }
    }
    std::printf("handplant entry 0x107578: %u cases match (acceptance, call order, probe, motion/control setup memory, motion enter); %u accepted; rejects falling %u inverted %u rail-cycle %u no-spline %u steep %u distance %u receding %u; motion1 %u; forward fallback %u; tangent flips %u; side %u/%u; reverseStance %u/%u; rail %u/%u; fast %u/%u; span 0.5 %u, 1.5 %u, between %u\n",
        entryCases,accepted,rejects[1],rejects[2],rejects[3],rejects[4],rejects[5],rejects[6],rejects[7],motionOne,fallback,flips,sides[0],sides[1],stance[0],stance[1],rails[0],rails[1],fast[0],fast[1],spanLow,spanHigh,spanMid);
    // ---------------------------------------------------------------- 0x138BA0 direct
    unsigned setupCases=0,uprighted=0,turned=0,unitAxis=0,negativeW=0,setupSides[2]{},setupRail[3]{};
    for(unsigned test=0;test<60000;++test){
        auto r=randomRider(g);r.velocity=randomVelocity(g);r.velocityW=g.coin(10)?g.uni(-1,1):0;
        if(g.coin(6)){auto b=ssx::originalRebuildOrientation(g.quat());r.up=b.up;} // rows independent of +0x120
        if(g.coin(20)){for(auto& x:r.quaternion)x*=g.uni(.5f,1.5f);}             // unnormalized +0x120
        ssx::OriginalHandplantHit hit;
        auto t=g.unit();t[2]*=.9f;
        if(g.coin(4)){ // tangent along the (uprighted) right axis: tiny or zero turn
            auto right=r.right;float a=g.coin()?0:g.near(0,.002f);float c=std::cos(a),s=std::sin(a);
            t={right[0]*c-right[1]*s,right[0]*s+right[1]*c,g.coin()?0:g.uni(-.5f,.5f)};if(g.coin())for(auto& x:t)x=-x;
        }
        if(g.coin(6)){ // rider on its side (up horizontal) with the tangent across the uprighted right axis: relative.w ~ 0
            r.quaternion=g.yaw(g.uni(-7,7),g.coin()?1.5707964f:-1.5707964f);setBasis(r);r.upW=0;r.forwardW=0;
            auto right=mirrorSetup(r,Quad{1,0,0,0}).right;float sign=g.coin()?1.f:-1.f,e=g.coin()?0:g.uni(-1e-3f,1e-3f);
            t={-right[1]*sign+e,right[0]*sign,g.coin()?0:g.uni(-.01f,.01f)};
        }
        // relative.w <= 0 (0x1390AC..0x1390E4) is unreachable for finite riders: the upright and
        // turn deltas have orthogonal axes or half angles <= 45 degrees, so w >= cos(a+b) >= 0.
        hit.tangent={t[0],t[1],t[2],g.coin(4)?-0.f:0.f};if(g.coin(12))hit.tangent[3]=g.uni(-1,1);
        hit.point={r.position[0]+g.uni(-300,300),r.position[1]+g.uni(-300,300),r.position[2]+g.uni(-300,300),g.coin(8)?g.uni(.5f,1.5f):1};
        hit.descriptor=!g.coin(3);hit.descriptorFlags=g.coin()?g.g():uint32_t(g.n(2));
        float duration=g.coin(4)?(g.coin()?.25f:.75f):g.uni(.25f,.75f);
        wr(OUT,hit.point);wr(OUT+0x10,hit.tangent);wr(OUT+0x58,hit.descriptor?DESC:0u);wr(DESC+0x1c,hit.descriptorFlags);
        auto m=garbageMotion(g);storeRider(r);storeMotion(m);auto& expected=snapshot();
        auto ctx=context(0x138ba0);SET_GPR_U32(&ctx,4,MOTION);SET_GPR_U32(&ctx,5,OUT);ctx.f[12]=duration;sub_00138BA0_0x138ba0(M,&ctx,&rt);
        if(ctx.pc!=DONE)return 10;
        ssx::originalHandplantMotionSetup(m,r,hit,duration);storeMotion(m,&expected);
        if(!compareImage(expected,"motion setup 0x138BA0",test))return 11;
        ++setupCases;setupSides[m.side&1]++;setupRail[hit.descriptor?(m.railFlag&1):2]++;
        // Branch accounting (float-level mirror of the gates).
        {using namespace ssx::handplant_original;ssx::terrain_original::Rounding rr;
         auto axis=cross4(quad(r.up,r.upW),unitZ4);uprighted+=bits(0x3a83126fu)<vuSqrt(dot4(axis,axis));
         unitAxis+=sameQ(m.axis,unitX4);negativeW+=!(0<mirrorSetup(r,hit.tangent).relativeW);}
    }
    std::printf("handplant motion setup 0x138BA0: %u direct cases match all +0x00..+0x97 fields; uprighted %u; unit-x axis %u; relative.w<=0 %u; side %u/%u; rail flag 0/1/no-descriptor %u/%u/%u\n",
        setupCases,uprighted,unitAxis,negativeW,setupSides[0],setupSides[1],setupRail[0],setupRail[1],setupRail[2]);
    (void)turned;(void)negativeW;
    // ---------------------------------------------------------------- 0x1329B0 / 0x138B48 direct
    unsigned controlCases=0,fastCount=0,sideValues[3]{};
    for(unsigned test=0;test<30000;++test){
        auto r=randomRider(g);r.velocity=randomVelocity(g);r.velocityW=g.coin(10)?g.uni(-1,1):0;
        ssx::OriginalHandplantHit hit;hit.descriptor=!g.coin(3);hit.descriptorFlags=g.coin()?g.g():uint32_t(g.n(2));
        int side=g.coin(3)?int(g.g()):int(g.n(2));float duration=g.uni(.25f,.75f);
        wr(OUT+0x58,hit.descriptor?DESC:0u);wr(DESC+0x1c,hit.descriptorFlags);
        auto c=garbageControl(g);storeRider(r);storeControl(c);auto& expected=snapshot();
        auto ctx=context(0x1329b0);SET_GPR_U32(&ctx,4,CONTROL);SET_GPR_U32(&ctx,5,OUT);SET_GPR_U32(&ctx,6,uint32_t(side));ctx.f[12]=duration;sub_001329B0_0x1329b0(M,&ctx,&rt);
        if(ctx.pc!=DONE)return 12;
        ssx::originalHandplantControlSetup(c,r,hit,side,duration);storeControl(c,&expected);
        if(!compareImage(expected,"control setup 0x1329B0",test))return 13;
        ++controlCases;fastCount+=c.fast;sideValues[side==0?0:side==1?1:2]++;
    }
    unsigned enterCases=0;
    for(unsigned test=0;test<30000;++test){
        auto r=randomRider(g);auto m=garbageMotion(g);m.lip={g.uni(-20000,20000),g.uni(-20000,20000),g.uni(-20000,20000),g.coin(8)?g.uni(0,2):1};
        m.duration=g.coin(20)?.25f:g.uni(.001f,1); // 0x107578 bounds T to [0.25,0.75]; the recompiler's VDIV-by-zero (Q=0) differs from VU hardware (Q=+-FLT_MAX), so T=0 is not comparedr.velocityW=g.uni(-1,1);
        storeRider(r);storeMotion(m);auto& expected=snapshot();
        auto ctx=context(0x138b48);SET_GPR_U32(&ctx,4,MOTION);sub_00138B48_0x138b48(M,&ctx,&rt);if(ctx.pc!=DONE)return 14;
        ssx::originalHandplantMotionEnter(m,r);storeRider(r,&expected);
        if(!compareImage(expected,"motion enter 0x138B48",test))return 15;++enterCases;
    }
    std::printf("handplant control setup 0x1329B0: %u cases match (fast %u, side arg 0/1/other %u/%u/%u); motion enter 0x138B48: %u cases match\n",
        controlCases,fastCount,sideValues[0],sideValues[1],sideValues[2],enterCases);
}
