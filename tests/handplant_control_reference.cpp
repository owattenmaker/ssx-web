// Development-only instruction oracle: original control-11 handplant enter
// 0x1328B0, play 0x132FB8, update 0x132A30 and exit 0x132F98 against
// engine/handplant.hpp. 0x11E098 is the recompiled original; animation, score,
// request, rail-attach and root-bake routines are recording stubs.
#include "handplant_reference_common.hpp"
void sub_001328B0_0x1328b0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00132FB8_0x132fb8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00132A30_0x132a30(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00132F98_0x132f98(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011E098_0x11e098(uint8_t*,R5900Context*,PS2Runtime*);
using namespace hp;
static void fail(const char* what){throw std::runtime_error(what);}
// Per-case world the stubs expose (identical for the original and the port).
struct World{
    std::array<float,512> durations{};bool reset=false,complete=false,attach=false;uint32_t events=0;int mode=0;
    float beginAward=0,launchAward=0;Quad bakedPosition{},bakedQuaternion{};
};
static World world;static int originalMode=0;static std::vector<Call> originalCalls;
static void note(uint32_t fn,int32_t arg=0,uint32_t value=0){originalCalls.push_back({fn,arg,value});}
static void stub(uint8_t*,R5900Context*c,PS2Runtime*){
    const uint32_t a0=GPR_U32(c,4),a1=GPR_U32(c,5);
    switch(c->pc){
    case 0x28b180:note(0x28b180);SET_GPR_U32(c,2,STAT);break;
    case 0x29dc48:if(a0!=STAT||a1!=RIDER)fail("0x29DC48 arguments");note(0x29dc48);break;
    case 0x312790:if(a0!=ANIM)fail("0x312790 animator");note(0x312790,int32_t(a1));c->f[0]=world.durations.at(a1);break;
    case 0x3128e8:if(a0!=ANIM||GPR_U32(c,6)!=0||fb(c->f[12])!=fb(-1.f))fail("0x3128E8 arguments");note(0x3128e8,int32_t(a1),rd<uint32_t>(ANIM+0x1c));break;
    case 0x311e88:if(a0!=ANIM)fail("0x311E88 animator");note(0x311e88,int32_t(a1),fb(c->f[12]));break;
    case 0x311b48:if(a0!=ANIM)fail("0x311B48 animator");note(0x311b48,0,fb(c->f[12]));break;
    case 0x116120:if(a0!=RIDER)fail("0x116120 rider");note(0x116120,int32_t(a1));SET_GPR_U32(c,2,world.reset);break;
    case 0x312ae8:if(a0!=ANIM||a1!=2)fail("0x312AE8 arguments");note(0x312ae8,2);SET_GPR_U32(c,2,world.complete);break;
    case 0x119bf0:if(a0!=SCORER)fail("0x119BF0 scorer");note(0x119bf0,int32_t(a1));c->f[0]=world.beginAward;break;
    case 0x10e098:if(a0!=RIDER||a1!=1)fail("0x10E098 arguments");note(0x10e098,1,fb(c->f[12]));break;
    case 0x11fe98:if(a0!=RIDER)fail("0x11FE98 rider");note(0x11fe98);SET_GPR_U32(c,2,originalMode);break;
    case 0x119c38:if(a0!=SCORER)fail("0x119C38 scorer");note(0x119c38);c->f[0]=world.launchAward;break;
    case 0x11fe78:if(a0!=RIDER)fail("0x11FE78 rider");note(0x11fe78,int32_t(a1));originalMode=int32_t(a1);break;
    case 0x11fec8:if(a0!=RIDER)fail("0x11FEC8 rider");note(0x11fec8,int32_t(a1));break;
    case 0x311b20:if(a0!=ANIM||a1!=2)fail("0x311B20 arguments");note(0x311b20,2);SET_GPR_U32(c,2,SEQ);break;
    case 0x1446a0:if(a0!=SEQ+0xb0)fail("0x1446A0 flags");note(0x1446a0,int32_t(a1));SET_GPR_U32(c,2,(world.events>>a1)&1);break;
    case 0x106848:if(a0!=RIDER)fail("0x106848 rider");note(0x106848);SET_GPR_U32(c,2,world.attach);break;
    case 0x11fa10:if(a0!=RIDER||a1!=RIDER+0x110)fail("0x11FA10 arguments");note(0x11fa10);wr(RIDER+0x110,world.bakedPosition);wr(RIDER+0x120,world.bakedQuaternion);break;
    default:fail("unexpected stub");
    }
    ret(c);
}
// Native host bound to the same world; logs the same external calls.
struct Host{
    std::vector<Call> calls;int mode=0;bool rebuilt=false;float seqRate=0;bool seqRateSet=false;float animRate=0;
    ssx::OriginalHandplantAccess access(){
        ssx::OriginalHandplantAccess a;auto* h=this;
        a.stat=[h]{h->calls.push_back({0x28b180});h->calls.push_back({0x29dc48});};
        a.semanticDuration=[h](int s){h->calls.push_back({0x312790,s});return world.durations.at(s);};
        a.play=[h](int s,float rate){h->animRate=rate;h->calls.push_back({0x3128e8,s,fb(rate)});};
        a.fade=[h](unsigned ch,float s){h->calls.push_back({0x311e88,int32_t(ch),fb(s)});};
        a.rotateAnimation=[h](float r){h->rebuilt=true;h->calls.push_back({0x311b48,0,fb(r)});};
        a.resetPath=[h](bool requested){h->calls.push_back({0x116120,int32_t(requested)});return world.reset;};
        a.channel2Complete=[h]{h->calls.push_back({0x312ae8,2});return world.complete;};
        a.scoreBegin=[h](int kind){h->calls.push_back({0x119bf0,kind});return world.beginAward;};
        a.award=[h](float v){h->calls.push_back({0x10e098,1,fb(v)});};
        a.motionMode=[h]{h->calls.push_back({0x11fe98});return h->mode;};
        a.scoreLaunch=[h]{h->calls.push_back({0x119c38});return world.launchAward;};
        a.requestMotion=[h](int id){h->calls.push_back({0x11fe78,id});h->mode=id;};
        a.requestControl=[h](int id){h->calls.push_back({0x11fec8,id});};
        a.channel2Event=[h](unsigned bit){h->calls.push_back({0x311b20,2});h->calls.push_back({0x1446a0,int32_t(bit)});return bool((world.events>>bit)&1);};
        a.railAttach=[h]{h->calls.push_back({0x106848});return world.attach;};
        a.bakeRoot=[h](ssx::OriginalHandplantRider& r){h->calls.push_back({0x11fa10});h->rebuilt=true;
            r.position={world.bakedPosition[0],world.bakedPosition[1],world.bakedPosition[2]};r.positionW=world.bakedPosition[3];r.quaternion=world.bakedQuaternion;
            ssx::handplant_original::rebuild(r);};
        a.predictorStatus=[]{return rd<int32_t>(PRED+0xac);};
        // predictor+0x98 - predictor+0xA0 as the original's EE SUB.S (guard bit).
        a.predictorRemaining=[]{return ssx::originalScalarSubtract(rd<float>(PRED+0x98),rd<float>(PRED+0xa0));};
        a.channel2Clock=[h]{h->calls.push_back({0x311b20,2});return std::array<float,2>{rd<float>(SEQ+0x08),rd<float>(SEQ+0x10)};};
        a.setChannel2Rate=[h](float v){h->seqRate=v;h->seqRateSet=true;};
        return a;
    }
};
static float pick(Rand& g,std::initializer_list<float> values){return *(values.begin()+g.n(unsigned(values.size())));}
static ssx::OriginalHandplantRider randomRider(Rand& g){
    ssx::OriginalHandplantRider r;r.position=g.vec(20000);r.positionW=g.coin(8)?g.uni(.5f,1.5f):1;
    r.quaternion=g.coin(3)?g.yaw(g.uni(-7,7),g.uni(-.6f,.6f)):g.quat();setBasis(r);
    if(g.coin(8)){auto b=ssx::originalRebuildOrientation(g.quat());r.up=b.up;}
    r.forwardW=g.coin(8)?g.uni(-1,1):0;r.upW=g.coin(8)?g.uni(-1,1):0;
    r.velocity=g.vec(2000);r.velocityW=0;r.timeScale=g.coin(3)?1.f:g.uni(.25f,2);r.reverseStance=g.n(2);r.phase=g.n(7);r.presentedUpZ=g.uni(-1,1);
    for(auto* v:{&r.turn,&r.animationTurn,&r.extraLean,&r.lean244,&r.roll250}){v->current=g.uni(-2,2);v->rate=g.uni(-2,2);v->target=g.uni(-2,2);}
    r.handBone=g.vec(20000);return r;
}
int main(int argc,char**argv){
    if(argc!=2)return 2;auto ram=loadElf(argv[1]);M=ram.data();baseLayout();
    PS2Runtime rt;rt.registerFunction(0x132fb8,sub_00132FB8_0x132fb8);rt.registerFunction(0x11e098,sub_0011E098_0x11e098);
    for(uint32_t pc:{0x28b180u,0x29dc48u,0x312790u,0x3128e8u,0x311e88u,0x311b48u,0x116120u,0x312ae8u,0x119bf0u,0x10e098u,0x11fe98u,0x119c38u,0x11fe78u,0x11fec8u,0x311b20u,0x1446a0u,0x106848u,0x11fa10u})rt.registerFunction(pc,stub);
    std::fesetround(FE_TOWARDZERO);Rand g(0x132a30);
    auto garbageControl=[&]{ssx::OriginalHandplantControlState c;c.timer=g.uni(-9,9);c.smoothed=g.uni(-9,9);c.fast=int32_t(g.n(2));c.railFlag=int32_t(g.n(2));c.side=int32_t(g.n(2));return c;};
    // ---------------------------------------------------------------- 0x1328B0 enter
    unsigned enterCases=0,enterSides[2]{};
    for(unsigned test=0;test<20000;++test){
        auto r=randomRider(g);auto c=garbageControl();c.timer=g.coin(4)?pick(g,{.25f,.75f}):g.uni(.25f,.75f);
        for(int s:{39,44})world.durations[s]=g.uni(.05f,3);
        storeRider(r);storeControl(c);wr(ANIM+0x1c,g.uni(-9,9));auto& expected=snapshot();originalCalls.clear();
        auto ctx=context(0x1328b0);SET_GPR_U32(&ctx,4,CONTROL);sub_001328B0_0x1328b0(M,&ctx,&rt);if(ctx.pc!=DONE){std::printf("enter continuation %x\n",ctx.pc);return 3;}
        // The original writes anim+0x1C = T-scaled rate before 0x3128E8 and restores 1.0 afterwards.
        for(auto& call:originalCalls)if(call.fn==0x3128e8&&rd<uint32_t>(ANIM+0x1c)!=fb(1.f))fail("enter did not restore anim+0x1C");
        Host h;auto a=h.access();ssx::originalHandplantControlEnter(c,r,a);
        storeRider(r,&expected,h.rebuilt);storeControl(c,&expected);wrTo(expected,ANIM+0x1c,1.f);
        if(!compareCalls(originalCalls,h.calls,"control enter",test))return 4;
        if(!compareImage(expected,"control enter 0x1328B0",test))return 5;
        ++enterCases;enterSides[c.side&1]++;
    }
    std::printf("handplant control enter 0x1328B0: %u cases match (turn/animation-turn seeks, phase 1, stat, duration/T rate, INTO clip, fades 1/0 at 0.33 s); side %u/%u\n",enterCases,enterSides[0],enterSides[1]);
    // ---------------------------------------------------------------- 0x132FB8 play / 0x132F98 exit
    unsigned playCases=0,reflects=0;
    for(unsigned test=0;test<28000;++test){
        auto r=randomRider(g);int phase=int(test%7),side=int((test/7)%2);auto c=garbageControl();c.side=side;
        storeRider(r);storeControl(c);wr(ANIM+0x1c,1.f);auto& expected=snapshot();originalCalls.clear();
        auto ctx=context(0x132fb8);SET_GPR_U32(&ctx,4,CONTROL);SET_GPR_U32(&ctx,5,phase);SET_GPR_U32(&ctx,6,side);sub_00132FB8_0x132fb8(M,&ctx,&rt);if(ctx.pc!=DONE)return 6;
        Host h;auto a=h.access();ssx::originalHandplantPlay(r,phase,side,1.f,a);storeRider(r,&expected,h.rebuilt);
        if(!compareCalls(originalCalls,h.calls,"play",test))return 7;
        if(!compareImage(expected,"play 0x132FB8",test))return 8;
        ++playCases;reflects+=h.rebuilt;
    }
    unsigned exitCases=0;
    for(unsigned test=0;test<20000;++test){
        auto r=randomRider(g);storeRider(r);auto& expected=snapshot();auto ctx=context(0x132f98);SET_GPR_U32(&ctx,4,CONTROL);sub_00132F98_0x132f98(M,&ctx,&rt);if(ctx.pc!=DONE)return 9;
        ssx::originalHandplantControlExit(r);storeRider(r,&expected);if(!compareImage(expected,"control exit 0x132F98",test))return 10;++exitCases;
    }
    std::printf("handplant play 0x132FB8: %u cases (phases 0..6 x sides) match semantics, reflect turns (%u) and rebuilt axes; control exit 0x132F98: %u cases match\n",playCases,reflects,exitCases);
    // ---------------------------------------------------------------- 0x132A30 update
    unsigned cases=0;std::array<unsigned,12> stops{};unsigned phaseCount[8]{},kinds[3]{},exits[7]{},heldBalance=0,neutral=0,positive=0,negative=0,negativeZero=0,timerPast=0,rateSet=0,rateClampLow=0,rateClampHigh=0,launchedScore=0,statusCount[4]{},targetBranch[5]{};
    for(unsigned test=0;test<120000;++test){
        auto r=randomRider(g);auto c=garbageControl();
        unsigned pr=g.n(20);r.phase=pr<5?1:pr<13?2:(int[]){0,3,4,5,6,7}[g.n(6)];
        // Command word: bit12 reset request, bit13 held, bits14..19 signed balance.
        int balance=g.coin(3)?0:g.coin(3)?(g.coin()?31:-32):int(g.n(64))-32;
        uint32_t word=(g.g()&~0xfc000u&~0x3000u)|(uint32_t(balance&63)<<14)|(g.coin(4)?0x1000u:0)|(r.phase==2?(g.coin(4)?0:0x2000u):(g.coin()?0x2000u:0));
        world.reset=g.coin(12);world.complete=g.coin();world.attach=g.coin(3);world.events=g.coin()?0x2u|(g.g()&~2u):(g.g()&~2u);
        world.mode=originalMode=(int[]){0,1,5,2,1,0}[g.n(6)];world.beginAward=g.uni(-100,100);world.launchAward=g.uni(-100,100);
        world.bakedPosition={g.uni(-20000,20000),g.uni(-20000,20000),g.uni(-20000,20000),g.coin(8)?g.uni(.5f,1.5f):1};world.bakedQuaternion=g.quat();
        c.fast=g.coin(4)?int32_t(g.n(2)):0;c.railFlag=g.coin(4)?int32_t(g.g()|1):int32_t(g.n(2));c.side=g.n(2);
        float dt=r.timeScale*std::bit_cast<float>(0x3c888889u);
        c.timer=g.coin(2)?g.near(std::bit_cast<float>(0x3f0057b7u)-dt,.02f):g.uni(0,6);
        c.smoothed=g.coin(4)?pick(g,{0.f,-0.f,1.f,-1.f}):g.coin()?g.near(float(balance)*std::bit_cast<float>(0x3d042108u),dt*4):g.uni(-1.2f,1.2f);
        if(g.coin(5)){ // smoothing boundaries: last == input+step or last == input-step exactly
            ssx::terrain_original::Rounding rr;float input=ssx::terrain_original::mul(float(balance),std::bit_cast<float>(0x3d042108u));
            if(r.reverseStance)input=-input;if(c.side==1)input=-input;float stepSize=ssx::terrain_original::mul(dt,3.f);
            c.smoothed=g.coin()?ssx::originalScalarAdd(input,stepSize):ssx::originalScalarSubtract(input,stepSize);
        }
        r.roll250.current=g.coin(4)?pick(g,{0.f,-0.f,1.f,-1.f,.25f,-.25f,-.05f}):g.coin(3)?g.uni(-1.5f,1.5f):g.near(pick(g,{1.f,-1.f,.25f,-.25f,-.05f}),.03f);
        // Predictor / sequence clock for the air re-timing.
        int status=g.coin(8)?int(g.g()%6)-1:int(g.n(4));wr(PRED+0xac,status);
        float predicted=g.uni(0,3),elapsed=g.coin(6)?predicted:predicted-g.uni(-1,2);wr(PRED+0x98,predicted);wr(PRED+0xa0,elapsed);
        float seqTime=g.uni(0,2),seqDuration=seqTime+g.uni(-.5f,3);
        if(g.coin(4)){seqTime=.25f;seqDuration=1.25f;predicted=g.coin()?2.5f:1.f;elapsed=.5f;wr(PRED+0x98,predicted);wr(PRED+0xa0,elapsed);} // rate exactly 0.5 / 2
        wr(SEQ+0x08,seqTime);wr(SEQ+0x10,seqDuration);wr(SEQ+0x90,g.uni(-9,9));
        storeRider(r);storeControl(c);wr(WORD,word);wr(ANIM+0x1c,1.f);auto& expected=snapshot();originalCalls.clear();
        auto ctx=context(0x132a30);SET_GPR_U32(&ctx,4,CONTROL);SET_GPR_U32(&ctx,5,WORD);sub_00132A30_0x132a30(M,&ctx,&rt);if(ctx.pc!=DONE){std::printf("update continuation %x\n",ctx.pc);return 11;}
        const int phaseBefore=r.phase;
        Host h;h.mode=world.mode;auto a=h.access();auto result=ssx::originalHandplantControlUpdate(c,r,word,a);
        storeRider(r,&expected,h.rebuilt);storeControl(c,&expected);if(h.seqRateSet)wrTo(expected,SEQ+0x90,h.seqRate);
        if(!compareCalls(originalCalls,h.calls,"control update",test))return 12;
        if(!compareImage(expected,"control update 0x132A30",test)){std::printf(" phase %d word %08x timer %.9g smoothed %.9g b %.9g stop %d\n",phaseBefore,word,c.timer,c.smoothed,r.roll250.current,int(result.stop));return 13;}
        ++cases;stops[int(result.stop)]++;phaseCount[phaseBefore&7]++;kinds[result.scoreKind]++;
        if(result.stop==ssx::OriginalHandplantControlResult::Stop::Exit)exits[r.phase]++;
        if(result.stop==ssx::OriginalHandplantControlResult::Stop::Balance){++heldBalance;float in=r.lean244.target;if(in==0){++neutral;negativeZero+=std::signbit(in);}else if(in>0)++positive;else ++negative;timerPast+=std::bit_cast<float>(0x3f0057b7u)<c.timer;
            float sm=c.smoothed,b=r.roll250.current;targetBranch[sm<0?0:0<sm?1:std::bit_cast<float>(0x3f0057b7u)<c.timer?2:!(b==0)?3:4]++;}
        if(h.seqRateSet){++rateSet;rateClampLow+=h.seqRate==.5f;rateClampHigh+=h.seqRate==2.f;}
        launchedScore+=world.mode==5&&phaseBefore!=1&&phaseBefore!=2&&!world.reset;if(phaseBefore>2&&!world.reset&&status>=0&&status<4)statusCount[status]++;
    }
    std::printf("handplant control update 0x132A30: %u cases match (rider seeks/phase/bake, control state, seq rate, ordered calls); phases 0..7 %u/%u/%u/%u/%u/%u/%u/%u; stops reset %u waiting %u scored %u balance %u exit %u launched %u attached %u ground %u air %u fallthrough %u; score kinds 1/2 %u/%u; exits to 4/5/6 %u/%u/%u; balance input neutral %u (-0 %u) positive %u negative %u, timer past 0.501338 %u; balance target from smoothed<0 %u, >0 %u, zero+timer %u, zero+b %u, zero+b0 %u; seq rate set %u (clamped 0.5 %u, 2 %u); motion-5 launch scoring %u; status 0..3 %u/%u/%u/%u\n",
        cases,phaseCount[0],phaseCount[1],phaseCount[2],phaseCount[3],phaseCount[4],phaseCount[5],phaseCount[6],phaseCount[7],
        stops[1],stops[2],stops[3],stops[4],stops[5],stops[6],stops[7],stops[8],stops[9],stops[0],kinds[1],kinds[2],exits[4],exits[5],exits[6],
        neutral,negativeZero,positive,negative,timerPast,targetBranch[0],targetBranch[1],targetBranch[2],targetBranch[3],targetBranch[4],rateSet,rateClampLow,rateClampHigh,launchedScore,statusCount[0],statusCount[1],statusCount[2],statusCount[3]);
}
