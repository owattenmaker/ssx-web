// Development-only instruction oracle: original rail attach 0x106848 with the
// immediate motion-4 enter 0x13AD20 (via 0x11FE78 -> 0x1112B8), stance alignment
// 0x115358/0x115168/0x11DFE0, entry animation 0x1326C8, control-7 enter 0x131D08 and
// heading offset 0x13ADC0/0x13BD80, against engine/rail_motion.hpp originalRailAttach
// plus originalRailMotionBegin/originalRailControlBegin called from the request
// callbacks in the same places (web/rail_gameplay.inc step_rails does the same).
// Recompiled originals: 0x106848, 0x108A48, 0x1086B8, 0x13AD20, 0x11E098, 0x115358,
// 0x115168, 0x116930, 0x11DFE0, 0x311B48, 0x1326C8, 0x131D08, 0x13ADC0, 0x13BD80,
// 0x11FEE8, 0x11FE98 and the math routines. Controlled inputs: the world query, the
// animation getters, scoring (0x10E910/0x119D40/0x10E098), the 0x11FA10 bake result
// (its own oracle is tools/test_rider_pose_native.py), the old controller's exit
// (0x134CB0 etc., stubbed as a scripted bake) and the animation sinks 0x3128E8/0x311BF0.
#include "handplant_reference_common.hpp"
void sub_00106848_0x106848(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00108A48_0x108a48(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001086B8_0x1086b8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0013AD20_0x13ad20(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011E098_0x11e098(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00115358_0x115358(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00115168_0x115168(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00116930_0x116930(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011DFE0_0x11dfe0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00311B48_0x311b48(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001326C8_0x1326c8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00131D08_0x131d08(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0013ADC0_0x13adc0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0013BD80_0x13bd80(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011FEE8_0x11fee8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011FE98_0x11fe98(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BB30_0x31bb30(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BE50_0x31be50(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BF60_0x31bf60(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C128_0x31c128(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C228_0x31c228(uint8_t*,R5900Context*,PS2Runtime*);
using namespace hp;
using V=ssx::RailVector;
namespace {
constexpr uint32_t RAILM=OWNER+0xB0,C7=OWNER+0x2B0,BONE=2,BONE_AT=BONES+BONE*32,GLOBAL=0xE0000;
struct Hit{bool found=false;V point{},tangent{};int surface=0;};
Hit first,second;std::vector<V> originalQueries;
int animationClass=0;unsigned sequenceFlags=0;float scoreValue=0;
// Scripted 0x11FA10 bake and old-controller exit bake (controlled, applied identically on both sides).
struct Bake{V position{};Quad quaternion{};};Bake presentBake,exitBake;int oldControl=0;
std::vector<Call> originalCalls;
void log(uint32_t fn,int32_t arg,uint32_t value=0){originalCalls.push_back({fn,arg,value});}
void query(uint8_t* ram,R5900Context* c,PS2Runtime*){
    V p;std::memcpy(&p,ram+GPR_U32(c,5),12);originalQueries.push_back(p);
    if(c->f[12]!=300||GPR_U32(c,7)!=1||GPR_U32(c,4)!=WORLD)throw std::runtime_error("Wrong rail query contract");
    const Hit& h=originalQueries.size()==1?first:second;uint32_t out=GPR_U32(c,6);
    if(h.found){wr(out,h.point);wr(out+12,1.f);wr(out+16,h.tangent);wr(out+28,0.f);wr(out+0x20,Quad{0,0,0,0});wr(out+0x30,Quad{0,0,0,0});wr(out+0x4c,h.surface);wr(out+0x50,0u);wr(out+0x58,DESC);}
    SET_GPR_U32(c,2,h.found);ret(c);
}
void stub(uint8_t*,R5900Context* c,PS2Runtime* rt){
    const uint32_t pc=c->pc,a0=GPR_U32(c,4),a1=GPR_U32(c,5);
    switch(pc){
    case 0x311ae8:if(a1!=2)throw std::runtime_error("channel");SET_GPR_U32(c,2,animationClass);break;
    case 0x311b20:SET_GPR_U32(c,2,SEQ);break;
    case 0x1446a0:SET_GPR_U32(c,2,!!(sequenceFlags&(1u<<a1)));break;
    case 0x10e910:if(a0!=RIDER||a1!=0)throw std::runtime_error("0x10E910 arguments");log(0x10e910,int32_t(GPR_U32(c,6))*16+int32_t(GPR_U32(c,7)),fb(c->f[12]));break;
    case 0x119d40:if(a0!=SCORER)throw std::runtime_error("0x119D40 arguments");log(0x119d40,int32_t(a1)*256+int32_t(GPR_U32(c,7))*16+int32_t(GPR_U32(c,8)));c->f[0]=scoreValue;break;
    case 0x10e098:if(a0!=RIDER||a1!=1)throw std::runtime_error("0x10E098 arguments");log(0x10e098,0,fb(c->f[12]));break;
    case 0x11fe78:{ // 0x1112B8: exit (motion 1 is empty; motion 0 is 0x13F410, logged), mode, enter 0x13AD20
        if(a0!=RIDER)throw std::runtime_error("0x11FE78 arguments");const int previous=rd<int>(OWNER+0xde0);log(0x11fe78,int32_t(a1),uint32_t(previous));wr(OWNER+0xde0,int(a1));
        if(a1==4){R5900Context e=context(0x13ad20);SET_GPR_U32(&e,4,RAILM);SET_GPR_U32(&e,29,GPR_U32(c,29)-0x400);sub_0013AD20_0x13ad20(M,&e,rt);if(e.pc!=DONE)throw std::runtime_error("0x13AD20 continuation");}
        break;}
    case 0x11fa10:if(a0!=RIDER||a1!=RIDER+0x110)throw std::runtime_error("0x11FA10 arguments");log(0x11fa10,0);wr(RIDER+0x110,presentBake.position);wr(RIDER+0x120,presentBake.quaternion);break;
    case 0x11fec8:{ // 0x111538: old controller exit (scripted bake), control, enter (7 = 0x131D08)
        if(a0!=RIDER)throw std::runtime_error("0x11FEC8 arguments");const int previous=rd<int>(OWNER+0xde4);log(0x11fec8,int32_t(a1),uint32_t(previous));
        if(a1==13&&previous==5){wr(RIDER+0x110,exitBake.position);wr(RIDER+0x120,exitBake.quaternion);R5900Context e=context(0x11e098);SET_GPR_U32(&e,4,RIDER);SET_GPR_U32(&e,29,GPR_U32(c,29)-0x400);sub_0011E098_0x11e098(M,&e,rt);}
        wr(OWNER+0xde4,int(a1));
        if(a1==7){R5900Context e=context(0x131d08);SET_GPR_U32(&e,4,C7);SET_GPR_U32(&e,29,GPR_U32(c,29)-0x400);sub_00131D08_0x131d08(M,&e,rt);}
        break;}
    case 0x3128e8:if(a0!=ANIM)throw std::runtime_error("0x3128E8 arguments");log(0x3128e8,int32_t(a1)*16+int32_t(GPR_U32(c,6)),fb(c->f[12]));break;
    case 0x311bf0:{if(a0!=ANIM)throw std::runtime_error("0x311BF0 arguments");Quad q=rd<Quad>(a1+16);Quad t=rd<Quad>(a1);
        log(0x311bf0,0,fb(q[0]));log(0x311bf0,1,fb(q[1]));log(0x311bf0,2,fb(q[2]));log(0x311bf0,3,fb(q[3]));
        for(int k=0;k<4;k++)log(0x311bf1,k,fb(t[k]));break;}
    default:throw std::runtime_error("unexpected stub");
    }
    ret(c);
}
void storeValue3(std::vector<uint8_t>* img,uint32_t a,const ssx::GroundControlValue& v){put(img,a,v.current);put(img,a+4,v.rate);put(img,a+8,v.target);}
void storeRider(const ssx::OriginalRailRider& r,std::vector<uint8_t>* img=nullptr){
    auto R=RIDER;
    put(img,R+0x110,r.position);put(img,R+0x11c,1.f);put(img,R+0x120,r.quaternion);put(img,R+0x1a0,r.right);put(img,R+0x1b0,r.forward);put(img,R+0x1c0,r.up);
    put(img,R+0x1e0,r.velocity);put(img,R+0x1ec,0.f);
    storeValue3(img,R+0x1f0,r.turn);storeValue3(img,R+0x1fc,r.animationTurn);storeValue3(img,R+0x208,r.extraLean);storeValue3(img,R+0x214,r.brake);
    storeValue3(img,R+0x250,r.presentationRoll);storeValue3(img,R+0x25c,r.tolerance25C);storeValue3(img,R+0x280,r.balance280);
    put(img,R+0x2dc,r.manualSpin2DC);put(img,R+0x300,r.timeScale);put(img,R+0x320,r.reverseStance);put(img,R+0x324,r.state324);put(img,R+0x328,r.style);put(img,R+0x330,r.flag330);
    put(img,R+0x3a0,r.surfaceForward);put(img,R+0x3b0,r.lateral);put(img,R+0x438,r.surfaceId);
    put(img,R+0x9d0,r.offset9D0);put(img,R+0x9dc,0.f);put(img,BONE_AT,r.bonePosition);put(img,BONE_AT+12,1.f);put(img,BONE_AT+0x10,r.boneQuaternion);
}
void storeMotion4(const ssx::OriginalRailMotionState& m,std::vector<uint8_t>* img=nullptr){
    auto A=RAILM;put(img,A+0x10,m.lean);put(img,A+0x14,m.timeOnRail);put(img,A+0x18,m.balance);put(img,A+0x1c,m.headingOffset);
    put(img,A+0x20,m.lostRail);put(img,A+0x30,m.entryPosition);put(img,A+0x40,m.word40);
}
void listDiffs(const std::vector<uint8_t>& expected){
    for(auto& r:regions())for(uint32_t a=r.begin;a<r.end;a+=4){uint32_t x,y;std::memcpy(&x,M+a,4);std::memcpy(&y,expected.data()+a,4);
        if(x!=y)std::printf("    %s+0x%x original %08x (%.9g) native %08x (%.9g)\n",r.name,a-r.begin,x,std::bit_cast<float>(x),y,std::bit_cast<float>(y));}
}
V unitNear(Rand& g,V axis,float spread){V v;for(int k=0;k<3;k++)v[k]=axis[k]+g.uni(-spread,spread);float l=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);for(auto& x:v)x/=l;return v;}
}
int main(int argc,char**argv){
    if(argc!=2)return 2;auto ram=loadElf(argv[1]);M=ram.data();baseLayout();
    wr(RIDER+0x8a0,BONE);wr(RAILM+0x50,RIDER);wr(C7+0x8,RIDER);wr(0x4ff130,Quad{0,0,0,1});wr(0x4a30f0+0x410,GLOBAL);
    PS2Runtime rt;
    for(auto [pc,fn]:std::initializer_list<std::pair<uint32_t,PS2Runtime::RecompiledFunction>>{{0x108a48,sub_00108A48_0x108a48},{0x1086b8,sub_001086B8_0x1086b8},{0x13ad20,sub_0013AD20_0x13ad20},
        {0x11e098,sub_0011E098_0x11e098},{0x115358,sub_00115358_0x115358},{0x115168,sub_00115168_0x115168},{0x116930,sub_00116930_0x116930},{0x11dfe0,sub_0011DFE0_0x11dfe0},
        {0x311b48,sub_00311B48_0x311b48},{0x1326c8,sub_001326C8_0x1326c8},{0x131d08,sub_00131D08_0x131d08},{0x13adc0,sub_0013ADC0_0x13adc0},{0x13bd80,sub_0013BD80_0x13bd80},
        {0x11fee8,sub_0011FEE8_0x11fee8},{0x11fe98,sub_0011FE98_0x11fe98},{0x31bb30,sub_0031BB30_0x31bb30},{0x31be50,sub_0031BE50_0x31be50},{0x31bf60,sub_0031BF60_0x31bf60},
        {0x31c128,sub_0031C128_0x31c128},{0x31c228,sub_0031C228_0x31c228}})rt.registerFunction(pc,fn);
    rt.registerFunction(0x334680,query);
    for(uint32_t pc:{0x311ae8u,0x311b20u,0x1446a0u,0x10e910u,0x119d40u,0x10e098u,0x11fe78u,0x11fa10u,0x11fec8u,0x3128e8u,0x311bf0u})rt.registerFunction(pc,stub);
    std::fesetround(FE_TOWARDZERO);Rand g(0x106848);
    const int controls[8]={0,1,2,3,4,5,11,12};
    unsigned cases=0,attached=0,rejected=0,flipped=0,sideways=0,restyled=0,airEntries=0,boardPress=0,fromGround=0,styleSeen[5]{};
    for(unsigned test=0;test<60000;++test){
        ssx::OriginalRailRider r;ssx::OriginalRailMotionState m;
        const int control=controls[g.n(8)];r.controlState=control;r.motionMode=int(g.n(2));oldControl=control;
        r.style=int(g.n(5));r.reverseStance=int(g.n(2));r.state324=int(g.n(2));r.flag330=g.coin(4)?int(1+g.n(2)):0;
        V tangent=g.coin(8)?unitNear(g,{0,0,1},.3f):g.unit();if(!g.coin(3))tangent[2]*=.3f;tangent=unitNear(g,tangent,0);
        r.quaternion=g.coin(3)?g.quat():g.yaw(g.uni(-7,7),g.uni(-.6f,.6f));
        {auto b=ssx::originalRebuildOrientation(r.quaternion);r.right=b.right;r.forward=b.forward;r.up=b.up;}
        r.position=g.vec(200000);r.bonePosition=r.position;for(int k=0;k<3;k++)r.bonePosition[k]+=g.uni(-60,60);
        r.boneQuaternion=g.coin(3)?r.quaternion:g.quat();r.offset9D0=g.coin(4)?g.vec(3):V{};
        float speed=g.coin(6)?g.uni(0,600):g.uni(300,4000);V dir=unitNear(g,tangent,g.coin()?.05f:.9f);float sign=g.coin()?1.f:-1.f;
        for(int k=0;k<3;k++)r.velocity[k]=dir[k]*speed*sign;if(g.coin(20))r.velocity={};
        r.timeScale=g.coin(4)?g.uni(.25f,2):1.f;r.manualSpin2DC=g.uni(-5,5);r.surfaceId=int(g.n(40));
        for(auto* v:{&r.turn,&r.animationTurn,&r.extraLean,&r.brake,&r.presentationRoll,&r.tolerance25C,&r.balance280}){*v={g.uni(-1,1),g.uni(0,1),g.uni(-1,1)};if(g.coin(6))*v={};}
        r.tolerance25C.current=g.uni(0,1);r.surfaceForward=g.unit();r.lateral=g.unit();
        m.direction=g.unit();m.lean=g.uni(-1,1);m.timeOnRail=g.uni(0,5);m.balance=g.uni(-1,1);m.headingOffset=g.uni(-.9f,.9f);m.lostRail=int(g.n(2));m.railId=int(g.g());m.entryPosition=g.vec(1000);m.word40=int(g.g());
        animationClass=17+int(g.n(5));sequenceFlags=g.n(8);scoreValue=g.uni(-2,2);
        first={};first.found=!g.coin(12);first.tangent=tangent;first.surface=int(g.n(40));
        {V q=r.bonePosition;for(int k=0;k<3;k++)q[k]+=r.offset9D0[k];V side=g.unit();for(int k=0;k<3;k++)first.point[k]=q[k]+side[k]*g.uni(0,g.coin(4)?80.f:25.f);}
        if(g.coin(10))for(int k=0;k<3;k++)first.point[k]+=g.uni(-300,300);
        second=first;second.found=!g.coin(4);for(int k=0;k<3;k++)second.point[k]+=g.uni(-30,30);
        presentBake.position=r.position;for(auto& x:presentBake.position)x+=g.uni(-20,20);presentBake.quaternion=g.coin(4)?Quad{r.quaternion[0],r.quaternion[1],r.quaternion[2],r.quaternion[3]}:g.quat();
        exitBake.position=presentBake.position;for(auto& x:exitBake.position)x+=g.uni(-20,20);exitBake.quaternion=g.quat();
        wr(OWNER+0xde4,control);wr(OWNER+0xde0,r.motionMode);
        storeRider(r);storeMotion4(m);wr(C7,g.uni(-3,3));wr(C7+4,int(g.g()));wr(RIDER+0x360,int(g.n(3)));wr(GLOBAL+0x598c,-7);
        auto& expected=snapshot();originalQueries.clear();originalCalls.clear();
        auto c=context(0x106848);SET_GPR_U32(&c,4,RIDER);sub_00106848_0x106848(M,&c,&rt);
        if(c.pc!=DONE){std::printf("attach continuation %x\n",c.pc);return 3;}
        const bool originalAttached=GPR_U32((&c),2)!=0;
        // Native, with the same request-time effects as web/rail_gameplay.inc.
        std::vector<Call> nativeCalls;unsigned queryIndex=0;ssx::OriginalRailControlState c7;c7.spin=rd<float>(C7);c7.identity=rd<int>(C7+4);
        int motionMode=r.motionMode,controlNow=control;
        ssx::OriginalRailAccess a;
        a.query=[&](V p){if(queryIndex>=originalQueries.size()||std::memcmp(&p,&originalQueries[queryIndex],12))throw std::runtime_error("rail query sequence differs (case "+std::to_string(test)+")");
            const Hit& h=queryIndex++==0?first:second;ssx::OriginalRailQueryResult q;q.found=h.found;if(h.found){q.point=h.point;q.tangent=h.tangent;q.surface=h.surface;}return q;};
        a.channel2Class=[&]{return animationClass;};a.channel2SequenceFlag=[&](unsigned bit){return bool(sequenceFlags&(1u<<bit));};
        a.airborneRailEvent=[&](int style,int flag,float speed,const ssx::OriginalRailQueryResult&){nativeCalls.push_back({0x10e910,style*16+flag,fb(speed)});};
        a.railEntryScore=[&](bool stance,int style,int flag){nativeCalls.push_back({0x119d40,int32_t(stance)*256+style*16+flag,0});return scoreValue;};
        a.awardScore=[&](float v){nativeCalls.push_back({0x10e098,0,fb(v)});};
        a.recordRailSurface=[&](int surface){put(&expected,GLOBAL+0x598c,surface);};
        a.requestMotion=[&](int mode){nativeCalls.push_back({0x11fe78,mode,uint32_t(motionMode)});motionMode=mode;r.motionMode=mode;
            if(mode==4){nativeCalls.push_back({0x11fa10,0,0});r.position=presentBake.position;r.quaternion=presentBake.quaternion;ssx::originalRailMotionBegin(m,r);}};
        a.requestControl=[&](int next){nativeCalls.push_back({0x11fec8,next,uint32_t(controlNow)});
            if(next==13&&controlNow==5){r.position=exitBake.position;auto b=ssx::originalRebuildOrientation(exitBake.quaternion);r.quaternion=b.quaternion;r.right=b.right;r.forward=b.forward;r.up=b.up;}
            controlNow=next;r.controlState=next;if(next==7){ssx::originalRailControlBegin(c7,r);put(&expected,RIDER+0x360,0);}};
        a.playAnimation=[&](int semantic,float blend,int flags){nativeCalls.push_back({0x3128e8,semantic*16+flags,fb(blend)});};
        a.rotateAnimation=[&](float angle){ssx::rail_original::Rounding rounding; // web/rail_gameplay.inc: 0x311B48 = sincos(-angle/2) about (0,0,1)
            auto sc=ssx::originalSinCos(ssx::rail_original::mul(-angle,.5f));Quad q{ssx::rail_original::mul(sc[0],0.f),ssx::rail_original::mul(sc[0],0.f),ssx::rail_original::mul(sc[0],1.f),sc[1]};
            for(int k=0;k<4;k++)nativeCalls.push_back({0x311bf0,k,fb(q[k])});Quad t{0,0,0,1};for(int k=0;k<4;k++)nativeCalls.push_back({0x311bf1,k,fb(t[k])});};
        a.setAnimationRoot=[&](float half){ssx::rail_original::Rounding rounding;auto sc=ssx::originalSinCos(half);
            put(&expected,ANIM+0x30,Quad{0,0,0,1});put(&expected,ANIM+0x40,Quad{ssx::rail_original::mul(sc[0],0.f),ssx::rail_original::mul(sc[0],0.f),ssx::rail_original::mul(sc[0],1.f),sc[1]});};
        a.setAnimationSwitch=[&](int value){put(&expected,ANIM+0x18,value);};
        ssx::OriginalRailAttachResult result;
        try{result=ssx::originalRailAttach(r,m,a);}catch(const std::exception& e){std::printf("native threw case %u: %s\n",test,e.what());return 4;}
        if(result.attached!=originalAttached){std::printf("attach result mismatch case %u: original %d native %d\n",test,originalAttached,result.attached);return 5;}
        if(queryIndex!=originalQueries.size()){std::printf("query count mismatch case %u\n",test);return 6;}
        if(!compareCalls(originalCalls,nativeCalls,"rail attach",test))return 7;
        storeRider(r,&expected);storeMotion4(m,&expected);put(&expected,C7,c7.spin);put(&expected,C7+4,c7.identity);
        put(&expected,OWNER+0xde0,motionMode);put(&expected,OWNER+0xde4,controlNow);
        if(originalAttached){
            // 0x11E098 stores +0x1D0 = position quad and clears the basis w lanes; 0x13AD20 copies the position quad to motion+0x30.
            put(&expected,RIDER+0x1d0,r.position);put(&expected,RIDER+0x1dc,1.f);for(uint32_t o:{0x1acu,0x1bcu,0x1ccu})put(&expected,RIDER+o,0.f);put(&expected,RAILM+0x3c,1.f);
            // 0x115168 negates the +0x3A0/+0x3B0 quads: their w lanes (0) become -0 on each flip.
            if(rd<float>(RIDER+0x3ac)!=0||std::signbit(rd<float>(RIDER+0x3ac))!=std::signbit(rd<float>(RIDER+0x3bc)))throw std::runtime_error("tangent w lanes");
            put(&expected,RIDER+0x3ac,rd<float>(RIDER+0x3ac));put(&expected,RIDER+0x3bc,rd<float>(RIDER+0x3bc));
            // Velocity w lane: sign of zero from the along-travel tangent negation (not modelled; the browser keeps xyz).
            put(&expected,RIDER+0x1ec,rd<float>(RIDER+0x1ec));if(rd<float>(RIDER+0x1ec)!=0)throw std::runtime_error("velocity w lane");
        }
        if(rd<int>(GLOBAL+0x598c)!=(originalAttached?first.surface:-7)){std::printf("global +0x598C mismatch case %u\n",test);return 9;}
        if(!compareImage(expected,"rail attach 0x106848",test)){listDiffs(expected);
            std::printf("  control %d motion %d style %d->%d reverse %d flag330 %d attached %d\n",control,rd<int>(OWNER+0xde0),rd<int>(RIDER+0x328),r.style,r.reverseStance,r.flag330,originalAttached);return 8;}
        ++cases;attached+=originalAttached;rejected+=!originalAttached;
        if(originalAttached){styleSeen[r.style]++;restyled+=result.restyled;airEntries+=result.fromAir;boardPress+=result.entrySemantic==26||result.entrySemantic==34;fromGround+=motionMode==4&&rd<int>(0)==0;
            for(auto& call:nativeCalls)if(call.fn==0x311bf0&&call.arg==2)sideways++;}
        flipped+=rd<int>(RIDER+0x320)!=0;
    }
    std::printf("rail attach 0x106848 + 0x13AD20/0x115358/0x115168/0x11DFE0/0x1326C8/0x131D08/0x13ADC0: %u cases match (result, query and call order, rider/motion/control-7/animator/global bytes); attached %u (styles 1..4 %u/%u/%u/%u, restyled %u, air entries %u, board-press entries %u), rejected %u, animation-root rotations %u\n",
        cases,attached,styleSeen[1],styleSeen[2],styleSeen[3],styleSeen[4],restyled,airEntries,boardPress,rejected,sideways);
    (void)flipped;(void)fromGround;
}
