// Development-only instruction oracle: original board press (control 1) against
// engine/board_press.hpp. The routines under test run as recompiled originals and
// call each other through the runtime table (0x31BE50 sine/cosine and 0x31C228 atan
// are the recompiled originals too). Animation, scoring, control/motion requests,
// reverse turn, takeoff, effects and the shared cruise helpers are recording stubs;
// the port runs against the same world through OriginalBoardPressAccess.
//   0x1161D0 entry, 0x12FC60 enter, 0x12FE98 exit, 0x12FEC8 stance flip,
//   0x1313A8 spring, 0x131428 finalise, 0x131348 jump-out, 0x1162C8 jump request,
//   0x12FFF8 airborne, 0x130228 phase 0, 0x1306B0 reverse, 0x1307B8 R3 ollie,
//   0x1308D8 pivot, 0x130DD0 depth/effects, 0x131200 release, 0x1303E0 phase 1,
//   0x1304D0 phase 2, 0x1304E0 phase 3, 0x12FC80 update.
#include "handplant_reference_common.hpp"
#include "../engine/board_press.hpp"
#include <map>
#define DECL(a,b) void sub_##a##_##b(uint8_t*,R5900Context*,PS2Runtime*);
DECL(001161D0,0x1161d0) DECL(0012FC60,0x12fc60) DECL(0012FC80,0x12fc80) DECL(0012FE98,0x12fe98) DECL(0012FEC8,0x12fec8)
DECL(0012FFF8,0x12fff8) DECL(00130228,0x130228) DECL(001303E0,0x1303e0) DECL(001304D0,0x1304d0) DECL(001304E0,0x1304e0)
DECL(001306B0,0x1306b0) DECL(001307B8,0x1307b8) DECL(001308D8,0x1308d8) DECL(00130DD0,0x130dd0) DECL(00131200,0x131200)
DECL(00131428,0x131428) DECL(001313A8,0x1313a8) DECL(00131348,0x131348) DECL(001162C8,0x1162c8) DECL(0031BE50,0x31be50) DECL(0031C228,0x31c228)
using namespace hp;
using Rider=ssx::OriginalBoardPressRider;using State=ssx::OriginalBoardPressState;
constexpr uint32_t BPC=OWNER+0x1d0;
static void fail(const char* what){throw std::runtime_error(what);}
struct World{int semantic=438,motion=0,control=1;bool reverse=false,status=false,reset=false,rail=false;float press=0,pivot=0,end=0;};
static World world;static std::vector<Call> originalCalls;
struct Fx{int32_t id=0,surface=0;std::array<uint32_t,17> words{};bool operator==(const Fx&)const=default;};
static std::vector<Fx> originalFx;
static std::map<uint32_t,unsigned> callCounts;static std::map<int,unsigned> playCounts;
static void note(uint32_t fn,int32_t arg=0,uint32_t value=0){originalCalls.push_back({fn,arg,value});++callCounts[fn];if(fn==0x3128e8)++playCounts[arg];}
static void stub(uint8_t*,R5900Context*c,PS2Runtime*){
    const uint32_t a0=GPR_U32(c,4),a1=GPR_U32(c,5),a2=GPR_U32(c,6);
    switch(c->pc){
    case 0x312aa0:if(a0!=ANIM||a1!=2)fail("0x312AA0 arguments");note(0x312aa0);SET_GPR_U32(c,2,uint32_t(world.semantic));break;
    case 0x3128e8:if(a0!=ANIM||a2!=0||fb(c->f[12])!=fb(-1.f))fail("0x3128E8 arguments");note(0x3128e8,int32_t(a1));world.semantic=int32_t(a1);break;
    case 0x11fec8:if(a0!=RIDER)fail("0x11FEC8 rider");note(0x11fec8,int32_t(a1));break;
    case 0x11fe78:if(a0!=RIDER)fail("0x11FE78 rider");note(0x11fe78,int32_t(a1));world.motion=int32_t(a1);break;
    case 0x11fe98:if(a0!=RIDER)fail("0x11FE98 rider");note(0x11fe98);SET_GPR_U32(c,2,uint32_t(world.motion));break;
    case 0x11fee8:if(a0!=RIDER)fail("0x11FEE8 rider");note(0x11fee8);SET_GPR_U32(c,2,uint32_t(world.control));break;
    case 0x1199f8:if(a0!=SCORER)fail("0x1199F8 scorer");note(0x1199f8,int32_t(a1));c->f[0]=world.press;break;
    case 0x119ad8:if(a0!=SCORER)fail("0x119AD8 scorer");note(0x119ad8,int32_t(a1));c->f[0]=world.pivot;break;
    case 0x119a38:if(a0!=SCORER)fail("0x119A38 scorer");note(0x119a38);c->f[0]=world.end;break;
    case 0x10e098:if(a0!=RIDER||a1!=1)fail("0x10E098 arguments");note(0x10e098,1,fb(c->f[12]));break;
    case 0x114cc0:if(a0!=RIDER)fail("0x114CC0 rider");note(0x114cc0);SET_GPR_U32(c,2,world.reverse);break;
    case 0x114298:if(a0!=RIDER||fb(c->f[12])!=fb(1.f))fail("0x114298 arguments");note(0x114298);break;
    case 0x294170:if(a0!=rd<uint32_t>(0x4a30f0+0x410)||a1!=RIDER)fail("0x294170 arguments");note(0x294170);break;
    case 0x28b180:note(0x28b180);SET_GPR_U32(c,2,STAT);break;
    case 0x2a1560:if(a0!=STAT||a1!=RIDER)fail("0x2A1560 arguments");note(0x2a1560);break;
    case 0x10eb30:{if(a0!=RIDER||a2!=0)fail("0x10EB30 arguments");note(0x10eb30,int32_t(a1));const uint32_t t0=GPR_U32(c,8);Fx fx;fx.id=int32_t(a1);fx.surface=int32_t(GPR_U32(c,7));
        for(unsigned k=0;k<12;k++)fx.words[k]=rd<uint32_t>(t0+4*k);fx.words[12]=rd<uint32_t>(t0+0x30);for(unsigned k=0;k<4;k++)fx.words[13+k]=rd<uint32_t>(t0+0x40+4*k);originalFx.push_back(fx);break;}
    case 0x115640:if(a0!=RIDER)fail("0x115640 rider");note(0x115640);break;
    case 0x1326c8:if(a0!=OWNER+0x2b0||a1!=0)fail("0x1326C8 arguments");note(0x1326c8);break;
    case 0x2f6ac8:if(a0!=OWNER+0xd20||a1!=1)fail("0x2F6AC8 arguments");note(0x2f6ac8);break;
    case 0x116930:if(a0!=RIDER)fail("0x116930 rider");break; // empty routine
    case 0x116378:if(a0!=RIDER)fail("0x116378 rider");note(0x116378);SET_GPR_U32(c,2,world.status);break;
    case 0x116120:if(a0!=RIDER||a2!=0)fail("0x116120 arguments");note(0x116120,int32_t(a1));SET_GPR_U32(c,2,world.reset);break;
    case 0x106848:if(a0!=RIDER)fail("0x106848 rider");note(0x106848);SET_GPR_U32(c,2,world.rail);break;
    case 0x114130:if(a0!=RIDER)fail("0x114130 rider");note(0x114130,int32_t(a1|(a2<<1)));break;
    case 0x113f88:if(a0!=RIDER)fail("0x113F88 rider");note(0x113f88,int32_t(fb(c->f[13])),fb(c->f[12]));break;
    case 0x113e80:if(a0!=RIDER)fail("0x113E80 rider");note(0x113e80,0,fb(c->f[12]));break;
    case 0x113f38:if(a0!=RIDER)fail("0x113F38 rider");note(0x113f38,0,fb(c->f[12]));break;
    case 0x115b58:if(a0!=RIDER)fail("0x115B58 rider");note(0x115b58);break;
    case 0x115d48:if(a0!=RIDER)fail("0x115D48 rider");note(0x115d48);break;
    default:std::printf("unexpected %x\n",c->pc);fail("unexpected stub");
    }
    ret(c);
}
struct Root{bool set=false;Quad rotation{};int32_t mirror=0;};
struct Host{
    std::vector<Call> calls;std::vector<Fx> fx;World w;Root root;
    ssx::OriginalBoardPressAccess access(){
        ssx::OriginalBoardPressAccess a;auto* h=this;
        a.requestedSemantic=[h]{h->calls.push_back({0x312aa0});return h->w.semantic;};
        a.play=[h](int s){h->calls.push_back({0x3128e8,s});h->w.semantic=s;};
        a.requestControl=[h](int n){h->calls.push_back({0x11fec8,n});};
        a.requestMotion=[h](int n){h->calls.push_back({0x11fe78,n});h->w.motion=n;};
        a.motionMode=[h]{h->calls.push_back({0x11fe98});return h->w.motion;};
        a.controlState=[h]{h->calls.push_back({0x11fee8});return h->w.control;};
        a.scorePress=[h](int s){h->calls.push_back({0x1199f8,s});return h->w.press;};
        a.scorePivot=[h](int s){h->calls.push_back({0x119ad8,s});return h->w.pivot;};
        a.scoreEnd=[h]{h->calls.push_back({0x119a38});return h->w.end;};
        a.award=[h](float v){h->calls.push_back({0x10e098,1,fb(v)});};
        a.reverseTurn=[h]{h->calls.push_back({0x114cc0});return h->w.reverse;};
        a.stanceRoot=[h](const Quad& q,int32_t m){h->root={true,q,m};};
        a.jumpTakeoff=[h]{h->calls.push_back({0x114298});};
        a.ollieEvent=[h]{h->calls.push_back({0x294170});};
        a.rumble=[h]{h->calls.push_back({0x28b180});h->calls.push_back({0x2a1560});};
        a.effect=[h](const ssx::OriginalBoardPressEffect& e){h->calls.push_back({0x10eb30,e.id});Fx f;f.id=e.id;f.surface=e.surface;
            for(unsigned k=0;k<4;k++){f.words[k]=fb(e.position[k]);f.words[4+k]=fb(e.direction[k]);f.words[8+k]=fb(e.down[k]);f.words[13+k]=fb(e.direction2[k]);}f.words[12]=fb(e.speed);h->fx.push_back(f);};
        a.restoreStance=[h]{h->calls.push_back({0x115640});};
        a.railCycle=[h]{h->calls.push_back({0x1326c8});};
        a.jumpStat=[h]{h->calls.push_back({0x2f6ac8});};
        a.statusCheck=[h]{h->calls.push_back({0x116378});return h->w.status;};
        a.resetPath=[h](bool r){h->calls.push_back({0x116120,int32_t(r)});return h->w.reset;};
        a.railAttach=[h]{h->calls.push_back({0x106848});return h->w.rail;};
        a.boost=[h](bool held,bool pressed){h->calls.push_back({0x114130,int32_t(held)|int32_t(pressed)<<1});};
        a.crouchBrake=[h](float c,float b){h->calls.push_back({0x113f88,int32_t(fb(b)),fb(c)});};
        a.turnTarget=[h](float t){h->calls.push_back({0x113e80,0,fb(t)});};
        a.railTurn=[h](float t){h->calls.push_back({0x113f38,0,fb(t)});};
        a.upperReactions=[h]{h->calls.push_back({0x115b58});h->calls.push_back({0x115d48});};
        return a;
    }
};
static void storeState(const Rider& r,const State& c,std::vector<uint8_t>* img=nullptr){
    put(img,RIDER+0x110,r.position);put(img,RIDER+0x1e0,r.velocity);put(img,RIDER+0x3a0,r.forward);
    put(img,RIDER+0x300,r.timeScale);put(img,RIDER+0x2dc,r.manualSpin);put(img,RIDER+0x360,r.jumpLatch);
    put(img,RIDER+0x320,r.reverseStance);put(img,RIDER+0x328,r.style);put(img,RIDER+0x330,r.press);put(img,RIDER+0x438,r.surface);
    storeValue(img,RIDER+0x268,r.depth268);storeValue(img,RIDER+0x274,r.depth274);storeValue(img,RIDER+0x280,r.pivot280);
    put(img,BPC,c.phase);put(img,BPC+4,c.time);put(img,BPC+8,c.fullTime);put(img,BPC+0xc,c.idleTime);put(img,BPC+0x10,c.ollieLatch);
}
static void storeRoot(const Root& root,std::vector<uint8_t>& img){
    if(!root.set)return;wrTo(img,ANIM+0x30,Quad{0,0,0,1});wrTo(img,ANIM+0x40,root.rotation);wrTo(img,ANIM+0x18,root.mirror);
}
static float pick(Rand& g,std::initializer_list<float> v){return *(v.begin()+g.n(unsigned(v.size())));}
static float nearOf(Rand& g,std::initializer_list<float> v,float spread){float t=pick(g,v);switch(g.n(4)){case 0:return t;case 1:return std::nextafter(t,INFINITY);case 2:return std::nextafter(t,-INFINITY);default:return t+g.uni(-spread,spread);}}
static int field(Rand& g){static const int vals[]={0,31,-31,-32,16,-16,15,-15,3,-3,4,-4,25,-25,26,-26,1,-1,0,0};return g.coin(3)?int(g.n(64))-32:vals[g.n(20)];}
static uint32_t setField(uint32_t w,unsigned shift,int v){return (w&~(63u<<shift))|(uint32_t(v&63)<<shift);}
static uint32_t word0(Rand& g){uint32_t w=g.g();w=setField(w,18,field(g));w=setField(w,24,field(g));return w;}
static uint32_t word1(Rand& g){uint32_t w=g.g();w=setField(w,0,field(g));w=setField(w,6,field(g));return w;}
static ssx::GroundControlValue tri(Rand& g,std::initializer_list<float> specials){
    ssx::GroundControlValue v;v.current=g.coin(3)?g.uni(-1.2f,1.2f):nearOf(g,specials,.02f);if(g.coin(8))v.current=-v.current;
    v.rate=g.coin(3)?0.f:g.uni(-.1f,.1f);v.target=g.coin(4)?v.current:g.uni(-1.2f,1.2f);return v;
}
static void randomize(Rand& g,Rider& r,State& c){
    r=Rider{};c=State{};
    r.position={g.uni(-2e5f,2e5f),g.uni(-2e5f,2e5f),g.uni(-2e4f,2e4f),g.coin(6)?g.uni(-2,2):1.f};
    auto f=g.unit();r.forward={f[0],f[1],f[2],g.coin(8)?g.uni(-1,1):0.f};
    switch(g.n(5)){case 0:r.velocity={0,0,0,0};break;case 1:{float s=g.uni(-3000,3000);r.velocity={f[0]*s,f[1]*s,f[2]*s,0};break;}
        case 2:{float s=g.uni(0,.0012f);auto u=g.unit();r.velocity={u[0]*s,u[1]*s,u[2]*s,0};break;}default:r.velocity={g.uni(-3000,3000),g.uni(-3000,3000),g.uni(-800,800),g.coin(8)?g.uni(-1,1):0.f};}
    r.timeScale=g.coin(3)?g.uni(.25f,2):1.f;r.manualSpin=g.coin(3)?g.uni(-5,5):0.f;
    r.jumpLatch=g.coin(3)?pick(g,{0.f,-0.f}):g.coin(2)?1.f:g.uni(-2,2);
    r.reverseStance=g.coin(10)?int32_t(g.n(4)):int32_t(g.n(2));r.style=g.coin(10)?int32_t(g.n(8)):int32_t(g.n(5));
    r.press=g.coin(10)?int32_t(g.n(5)):int32_t(g.n(3));r.surface=int32_t(g.n(19));
    r.depth268=tri(g,{0.f,1.f,.5f,.25f});if(g.coin(3))r.depth268.current=pick(g,{1.f,0.f,-0.f});
    r.depth274=tri(g,{0.f,1.f,.5f,.8f,.4f,.6f,.2f});if(g.coin(4))r.depth274.current=pick(g,{1.f,0.f,.5f,.8f});
    r.pivot280=tri(g,{0.f,.15f,.2f,.5f,.8f,.85f,1.f,std::bit_cast<float>(0x3ef1463bu)/std::bit_cast<float>(0x40490fdbu)});
    c.phase=g.coin(12)?int32_t(g.n(6)):int32_t(g.n(4));
    c.time=g.coin(3)?nearOf(g,{1.f,.5f,5.f},.02f):g.uni(0,3);c.fullTime=g.coin(3)?nearOf(g,{1.f},.02f):g.uni(0,3);
    c.idleTime=g.coin(3)?nearOf(g,{.5f,.49f,1.f},.02f):g.uni(0,1.5f);c.ollieLatch=g.coin(3)?int32_t(g.n(2)):0;
}
static void randomWorld(Rand& g,World& w){
    static const int sems[]={438,5,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,29,30,37,38,25,33};
    w.semantic=sems[g.n(24)];w.motion=(int[]){0,0,1,4,0,1}[g.n(6)];w.control=g.coin(6)?0:1;w.reverse=g.coin(3);
    w.status=g.coin(20);w.reset=g.coin(20);w.rail=g.coin(20);w.press=g.uni(-50,50);w.pivot=g.uni(-50,50);w.end=g.uni(-50,50);
}
using Fn=void(*)(uint8_t*,R5900Context*,PS2Runtime*);
static PS2Runtime* runtime=nullptr;
// Runs one original routine and its port on the same randomized state; compares memory, calls, effects.
template<class Native> static bool check(const char* name,Fn fn,uint32_t pc,Rider& r,State& c,unsigned test,Native&& native,
        std::function<void(R5900Context&)> args){
    storeState(r,c);auto& expected=snapshot();originalCalls.clear();originalFx.clear();const World before=world;
    auto ctx=context(pc);args(ctx);fn(M,&ctx,runtime);if(ctx.pc!=DONE){std::printf("%s continuation %x\n",name,ctx.pc);return false;}
    Host h;h.w=before;auto a=h.access();const uint32_t result=native(r,c,a);
    storeState(r,c,&expected);storeRoot(h.root,expected);
    if(!compareCalls(originalCalls,h.calls,name,test))return false;
    if(originalFx!=h.fx){std::printf("%s effect mismatch case %u (%zu/%zu)\n",name,test,originalFx.size(),h.fx.size());
        for(size_t i=0;i<std::min(originalFx.size(),h.fx.size());i++)for(unsigned k=0;k<17;k++)if(originalFx[i].words[k]!=h.fx[i].words[k])std::printf(" word %u %08x/%08x\n",k,originalFx[i].words[k],h.fx[i].words[k]);return false;}
    if(result!=0xffffffffu&&(GPR_U32((&ctx),2)&0xff)!=result){std::printf("%s return mismatch case %u: original %u native %u\n",name,test,GPR_U32((&ctx),2),result);return false;}
    if(!compareImage(expected,name,test))return false;
    world=before;return true;
}
int main(int argc,char**argv){
    if(argc!=2)return 2;auto ram=loadElf(argv[1]);M=ram.data();baseLayout();
    wr(0x4ff130,Quad{0,0,0,1});wr(0x4ff160,Quad{0,0,1,0});
    wr(0x4fe920,std::array<float,8>{0,.2f,.4f,.4f,.6f,.4f,1,.2f});wr(0x4fe940,std::array<float,8>{0,.3f,.4f,1.2f,.6f,1.2f,1,.3f});
    if(rd<uint32_t>(0x4a30f0-0x1fdc)!=0x4fe920u||rd<uint32_t>(0x4a30f0-0x1fd4)!=0x4fe940u)fail("depth curve pointers");
    wr(BPC+0x14,RIDER);
    PS2Runtime rt;runtime=&rt;
    const std::pair<uint32_t,Fn> originals[]={{0x1161d0,sub_001161D0_0x1161d0},{0x12fc60,sub_0012FC60_0x12fc60},{0x12fc80,sub_0012FC80_0x12fc80},{0x12fe98,sub_0012FE98_0x12fe98},
        {0x12fec8,sub_0012FEC8_0x12fec8},{0x12fff8,sub_0012FFF8_0x12fff8},{0x130228,sub_00130228_0x130228},{0x1303e0,sub_001303E0_0x1303e0},{0x1304d0,sub_001304D0_0x1304d0},
        {0x1304e0,sub_001304E0_0x1304e0},{0x1306b0,sub_001306B0_0x1306b0},{0x1307b8,sub_001307B8_0x1307b8},{0x1308d8,sub_001308D8_0x1308d8},{0x130dd0,sub_00130DD0_0x130dd0},
        {0x131200,sub_00131200_0x131200},{0x131428,sub_00131428_0x131428},{0x1313a8,sub_001313A8_0x1313a8},{0x131348,sub_00131348_0x131348},{0x1162c8,sub_001162C8_0x1162c8},
        {0x31be50,sub_0031BE50_0x31be50},{0x31c228,sub_0031C228_0x31c228}};
    for(auto [pc,fn]:originals)rt.registerFunction(pc,fn);
    for(uint32_t pc:{0x312aa0u,0x3128e8u,0x11fec8u,0x11fe78u,0x11fe98u,0x11fee8u,0x1199f8u,0x119ad8u,0x119a38u,0x10e098u,0x114cc0u,0x114298u,0x294170u,0x28b180u,0x2a1560u,
        0x10eb30u,0x115640u,0x1326c8u,0x2f6ac8u,0x116930u,0x116378u,0x116120u,0x106848u,0x114130u,0x113f88u,0x113e80u,0x113f38u,0x115b58u,0x115d48u})rt.registerFunction(pc,stub);
    std::fesetround(FE_TOWARDZERO);Rand g(0x12fc80);
    using A=const ssx::OriginalBoardPressAccess&;
    auto withControl=[](R5900Context& x){SET_GPR_U32(&x,4,BPC);};
    auto withWords=[](R5900Context& x){SET_GPR_U32(&x,4,BPC);SET_GPR_U32(&x,5,WORD);};
    unsigned total=0;
    std::array<unsigned,8> branch{};
    // Pivot/idle branch coverage computed from the pre-state (0x1308D8 / 0x131200 predicates).
    auto classify=[&](const Rider& r,const State& c){
        ssx::terrain_original::Rounding rr;using namespace ssx::board_press_original;
        const uint32_t w1=rd<uint32_t>(WORD+4);const float press=field6(w1,0),pivot=field6(w1,6);
        const float dt=mul(r.timeScale,tick);if(press==0&&std::abs(pivot)<.5f&&ssx::originalScalarAdd(c.idleTime,dt)==.5f)++branch[0];
        const float current=mul(r.pivot280.current,pi);const float wrapped=wrap(ssx::originalScalarSubtract(0.f,current));
        const float delta=mul(mul(wrapped,tick),ssx::originalScalarAdd(r.timeScale,r.timeScale));
        if(current<0&&0.f<delta)++branch[1];else if(0.f<current&&delta<0)++branch[2];else ++branch[3];
    };
    auto run=[&](const char* name,unsigned count,uint32_t pc,auto&& native,std::function<void(R5900Context&)> args,std::function<void()> prepare,std::array<unsigned,4>* counts=nullptr){
        Fn fn=nullptr;for(auto [p,f]:originals)if(p==pc)fn=f;
        for(unsigned test=0;test<count;++test){
            Rider r;State c;randomize(g,r,c);randomWorld(g,world);wr(WORD,word0(g));wr(WORD+4,word1(g));if(prepare)prepare();
            if(g.coin(8)){ // idle-time boundary: idle + ts/60 lands on 0.5 exactly or one ulp away
                ssx::terrain_original::Rounding rr;const float dt=ssx::terrain_original::mul(r.timeScale,std::bit_cast<float>(0x3c888889u));
                c.idleTime=ssx::originalScalarSubtract(.5f,dt);if(g.coin(3))c.idleTime=std::nextafter(c.idleTime,g.coin()?INFINITY:-INFINITY);
                wr(WORD+4,setField(setField(rd<uint32_t>(WORD+4),0,0),6,int(g.n(31))-15));}
            classify(r,c);
            if(!check(name,fn,pc,r,c,test,[&](Rider& x,State& y,A a){return native(x,y,a);},args))std::exit(3);
            ++total;
        }
        std::printf("%s: %u cases match\n",name,count);
    };
    // 0x1161D0 entry (press in f12).
    {
        unsigned accepted=0,cases=0;
        for(unsigned test=0;test<60000;++test){
            Rider r;State c;randomize(g,r,c);randomWorld(g,world);
            float press=g.coin(4)?pick(g,{0.f,-0.f,1.f,-1.f}):g.coin(2)?ssx::terrain_original::mul(float(field(g)),std::bit_cast<float>(0x3d042108u)):g.uni(-1,1);
            if(g.coin(6)){r.velocity={r.forward[1],-r.forward[0],0,0};} // dot exactly 0 or tiny
            bool ok=check("board press entry 0x1161D0",sub_001161D0_0x1161d0,0x1161d0,r,c,test,
                [&](Rider& x,State&,A a){return uint32_t(ssx::originalBoardPressEntry(x,press,a));},[&](R5900Context& x){SET_GPR_U32(&x,4,RIDER);x.f[12]=press;});
            if(!ok)return 3;accepted+=r.press!=0&&!originalCalls.empty()&&originalCalls.back().fn==0x11fec8;++cases;
        }
        total+=cases;std::printf("board press entry 0x1161D0: %u cases match (%u requested control 1)\n",cases,accepted);
    }
    run("board press enter 0x12FC60",20000,0x12fc60,[](Rider& r,State& c,A){ssx::originalBoardPressEnter(c,r);return 0xffffffffu;},withControl,nullptr);
    run("board press exit 0x12FE98",20000,0x12fe98,[](Rider& r,State&,A){ssx::originalBoardPressExit(r);return 0xffffffffu;},withControl,nullptr);
    run("stance flip 0x12FEC8",30000,0x12fec8,[](Rider& r,State&,A a){ssx::originalBoardPressFlipStance(r,a);return 0xffffffffu;},withControl,nullptr);
    run("pivot spring 0x1313A8",20000,0x1313a8,[](Rider& r,State&,A){ssx::originalBoardPressSpring(r);return 0xffffffffu;},withControl,nullptr);
    run("pivot finalise 0x131428",40000,0x131428,[](Rider& r,State&,A a){ssx::originalBoardPressFinalize(r,a);return 0xffffffffu;},withControl,nullptr);
    run("jump-out 0x131348",30000,0x131348,[](Rider& r,State&,A a){ssx::originalBoardPressJumpOut(r,a);return 0xffffffffu;},withControl,nullptr);
    {
        unsigned requests=0;
        for(unsigned test=0;test<40000;++test){
            Rider r;State c;randomize(g,r,c);randomWorld(g,world);const bool held=g.n(2),pressed=g.n(2);
            bool ok=check("jump request 0x1162C8",sub_001162C8_0x1162c8,0x1162c8,r,c,test,[&](Rider& x,State&,A a){return uint32_t(ssx::originalBoardPressJumpRequest(x,held,pressed,a));},
                [&](R5900Context& x){SET_GPR_U32(&x,4,RIDER);SET_GPR_U32(&x,5,held);SET_GPR_U32(&x,6,pressed);});
            if(!ok)return 3;
            for(auto& call:originalCalls)requests+=call.fn==0x11fec8;
        }
        total+=40000;std::printf("jump request 0x1162C8: 40000 cases match (%u requested control 2)\n",requests);
    }
    run("airborne 0x12FFF8",50000,0x12fff8,[](Rider& r,State& c,A a){return uint32_t(ssx::originalBoardPressAirborne(c,r,a));},withControl,nullptr);
    run("phase 0 0x130228",50000,0x130228,[](Rider& r,State& c,A a){ssx::originalBoardPressPhase0(c,r,rd<uint32_t>(WORD+4),a);return 0xffffffffu;},withWords,nullptr);
    run("reverse 0x1306B0",30000,0x1306b0,[](Rider& r,State&,A a){return uint32_t(ssx::originalBoardPressReverse(r,a));},withControl,nullptr);
    run("R3 ollie 0x1307B8",40000,0x1307b8,[](Rider& r,State& c,A a){return uint32_t(ssx::originalBoardPressOllie(c,r,rd<uint32_t>(WORD),a));},withWords,nullptr);
    run("pivot 0x1308D8",120000,0x1308d8,[](Rider& r,State& c,A a){return uint32_t(ssx::originalBoardPressPivot(c,r,rd<uint32_t>(WORD+4),a));},withWords,nullptr);
    run("depth/effects 0x130DD0",80000,0x130dd0,[](Rider& r,State& c,A a){return uint32_t(ssx::originalBoardPressDepth(c,r,rd<uint32_t>(WORD+4),a));},withWords,nullptr);
    run("release 0x131200",50000,0x131200,[](Rider& r,State& c,A){return uint32_t(ssx::originalBoardPressRelease(c,r,rd<uint32_t>(WORD+4)));},withWords,nullptr);
    run("phase 1 0x1303E0",80000,0x1303e0,[](Rider& r,State& c,A a){ssx::originalBoardPressPhase1(c,r,rd<uint32_t>(WORD),rd<uint32_t>(WORD+4),a);return 0xffffffffu;},withWords,nullptr);
    run("phase 2 0x1304D0",10000,0x1304d0,[](Rider&,State& c,A){c.phase=1;return 0xffffffffu;},withWords,nullptr);
    run("phase 3 0x1304E0",50000,0x1304e0,[](Rider& r,State& c,A a){ssx::originalBoardPressPhase3(c,r,rd<uint32_t>(WORD+4),a);return 0xffffffffu;},withWords,nullptr);
    run("update 0x12FC80",200000,0x12fc80,[](Rider& r,State& c,A a){ssx::originalBoardPressUpdate(c,r,rd<uint32_t>(WORD),rd<uint32_t>(WORD+4),a);return 0xffffffffu;},withWords,nullptr);
    std::printf("board press control oracle: %u original executions match bit-for-bit\n",total);
    std::printf("  coverage: idle exactly 0.5 %u; pivot rotation branches (neg->pos wrap %u, pos->neg wrap %u, plain %u)\n",branch[0],branch[1],branch[2],branch[3]);
    std::printf("  original callee counts:");for(auto [fn,n]:callCounts)std::printf(" %x:%u",fn,n);std::printf("\n  plays:");for(auto [sem,n]:playCounts)std::printf(" %d:%u",sem,n);std::printf("\n");
}
