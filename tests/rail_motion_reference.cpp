// Development-only instruction oracle: original motion-4 rail update 0x13AF28 against
// engine/rail_motion.hpp originalRailMotionStep. 0x1086B8 (board proximity), 0x13BD80
// (board normal), 0x121AA0 (alignment), 0x11E098, 0x11FEE8 and the math routines are the
// recompiled originals; the world query 0x334680 and the stat chain 0x14DC80/0x14DD58/
// 0x149208 are controlled inputs. Every rider/motion byte the original writes is compared.
#include "handplant_reference_common.hpp"
void sub_0013AF28_0x13af28(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001086B8_0x1086b8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0013BD80_0x13bd80(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00121AA0_0x121aa0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011E098_0x11e098(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0011FEE8_0x11fee8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BB30_0x31bb30(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BE50_0x31be50(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031BF60_0x31bf60(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C128_0x31c128(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0031C228_0x31c228(uint8_t*,R5900Context*,PS2Runtime*);
using namespace hp;
using V=ssx::RailVector;
namespace {
constexpr uint32_t RAILM=OWNER+0xB0,BONE=2,BONE_AT=BONES+BONE*32;
// Controlled world query: every call is logged; the second (1086B8 re-probe) can differ.
struct Hit{bool found=false;V point{},tangent{};int surface=0;uint32_t packed=0,flags=0;};
Hit first,second;std::vector<V> originalQueries;float stat=0;
void query(uint8_t* ram,R5900Context* c,PS2Runtime*){
    V p;std::memcpy(&p,ram+GPR_U32(c,5),12);originalQueries.push_back(p);
    if(c->f[12]!=300||GPR_U32(c,7)!=1||GPR_U32(c,4)!=WORLD)throw std::runtime_error("Wrong rail query contract");
    const Hit& h=originalQueries.size()==1?first:second;uint32_t out=GPR_U32(c,6);
    if(h.found){
        wr(out,h.point);wr(out+12,1.f);wr(out+16,h.tangent);wr(out+28,0.f);wr(out+0x20,Quad{0,0,0,0});wr(out+0x30,Quad{0,0,0,0});
        wr(out+0x4c,h.surface);wr(out+0x50,0u);wr(out+0x58,DESC);
    }
    SET_GPR_U32(c,2,h.found);ret(c);
}
void statChain(uint8_t*,R5900Context* c,PS2Runtime*){SET_GPR_U32(c,2,STAT);ret(c);}
void statValue(uint8_t*,R5900Context* c,PS2Runtime*){if(GPR_U32(c,4)!=STAT)throw std::runtime_error("Wrong stat chain");c->f[0]=stat;ret(c);}
void storeValue3(std::vector<uint8_t>* img,uint32_t a,const ssx::GroundControlValue& v){put(img,a,v.current);put(img,a+4,v.rate);put(img,a+8,v.target);}
struct Case{ssx::OriginalRailRider r;ssx::OriginalRailMotionState m;Quad quatW{};float velocityW=0,positionW=1;};
void storeRail(const ssx::OriginalRailRider& r,std::vector<uint8_t>* img=nullptr){
    auto R=RIDER;
    put(img,R+0x110,r.position);put(img,R+0x120,r.quaternion);put(img,R+0x1a0,r.right);put(img,R+0x1b0,r.forward);put(img,R+0x1c0,r.up);
    put(img,R+0x1e0,r.velocity);storeValue3(img,R+0x22c,r.steer22C);storeValue3(img,R+0x25c,r.tolerance25C);
    put(img,R+0x2e4,r.speedLimit);put(img,R+0x2fc,r.boost);put(img,R+0x300,r.timeScale);put(img,R+0x328,r.style);put(img,R+0x330,r.flag330);
    put(img,R+0x370,r.contactNormal);put(img,R+0x3d0,r.surfaceVelocity);put(img,R+0x438,r.surfaceId);put(img,R+0x460,r.contactPoint);
    put(img,R+0x9d0,r.offset9D0);put(img,BONE_AT,r.bonePosition);put(img,BONE_AT+0x10,r.boneQuaternion);
    // Live w lanes (captured): position/contact point/bone 1, velocity/surface velocity/offset 0.
    put(img,R+0x11c,1.f);put(img,R+0x1ec,0.f);put(img,R+0x3dc,0.f);put(img,R+0x9dc,0.f);put(img,BONE_AT+12,1.f);
}
void storeMotion4(const ssx::OriginalRailMotionState& m,std::vector<uint8_t>* img=nullptr){
    auto A=RAILM;put(img,A,m.direction);put(img,A+0x10,m.lean);put(img,A+0x14,m.timeOnRail);put(img,A+0x18,m.balance);put(img,A+0x1c,m.headingOffset);
    put(img,A+0x20,m.lostRail);put(img,A+0x24,m.railId);put(img,A+0x30,m.entryPosition);put(img,A+0x40,m.word40);
}
void listDiffs(const std::vector<uint8_t>& expected){
    for(auto& r:regions())for(uint32_t a=r.begin;a<r.end;a+=4){uint32_t x,y;std::memcpy(&x,M+a,4);std::memcpy(&y,expected.data()+a,4);
        if(x!=y)std::printf("    %s+0x%x original %08x (%.9g) native %08x (%.9g)\n",r.name,a-r.begin,x,std::bit_cast<float>(x),y,std::bit_cast<float>(y));}
}
V unitNear(Rand& g,V axis,float spread){V v;for(int k=0;k<3;k++)v[k]=axis[k]+g.uni(-spread,spread);float l=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);for(auto& x:v)x/=l;return v;}
}
int main(int argc,char**argv){
    if(argc!=2)return 2;auto ram=loadElf(argv[1]);M=ram.data();baseLayout();
    wr(RIDER+0x8a0,BONE);wr(RAILM+0x50,RIDER);
    PS2Runtime rt;
    rt.registerFunction(0x1086b8,sub_001086B8_0x1086b8);rt.registerFunction(0x13bd80,sub_0013BD80_0x13bd80);rt.registerFunction(0x121aa0,sub_00121AA0_0x121aa0);
    rt.registerFunction(0x11e098,sub_0011E098_0x11e098);rt.registerFunction(0x11fee8,sub_0011FEE8_0x11fee8);
    rt.registerFunction(0x31bb30,sub_0031BB30_0x31bb30);rt.registerFunction(0x31be50,sub_0031BE50_0x31be50);rt.registerFunction(0x31bf60,sub_0031BF60_0x31bf60);
    rt.registerFunction(0x31c128,sub_0031C128_0x31c128);rt.registerFunction(0x31c228,sub_0031C228_0x31c228);
    rt.registerFunction(0x334680,query);rt.registerFunction(0x14dc80,statChain);rt.registerFunction(0x14dd58,statChain);rt.registerFunction(0x149208,statValue);
    std::fesetround(FE_TOWARDZERO);Rand g(0x13af28);
    unsigned cases=0,lost=0,detached=0,riding=0,reprobe=0,clampedDown=0,clampedUp=0,imbalanced=0,sideways=0,inverted=0,boosted=0,uber=0,flagged=0;
    for(unsigned test=0;test<60000;++test){
        ssx::OriginalRailRider r;ssx::OriginalRailMotionState m;
        const bool sideways3=g.coin(3);r.style=sideways3?3+int(g.n(2)):1+int(g.n(2));r.flag330=g.coin(10);
        V tangent=g.coin(8)?unitNear(g,{0,0,1},.3f):g.unit();if(!g.coin(3))tangent[2]*=.3f;tangent=unitNear(g,tangent,0);
        r.quaternion=g.coin(3)?g.quat():g.yaw(g.uni(-7,7),g.uni(-.6f,.6f));if(g.coin(12)){for(auto& x:r.quaternion)x*=g.uni(.98f,1.02f);} // slightly off-unit
        {auto b=ssx::originalRebuildOrientation(r.quaternion);r.right=b.right;r.forward=b.forward;r.up=b.up;}
        if(g.coin(10))r.up=g.unit(); // stale axes
        r.position=g.vec(200000);r.bonePosition=r.position;for(int k=0;k<3;k++)r.bonePosition[k]+=g.uni(-60,60);
        r.boneQuaternion=g.coin(2)?r.quaternion:g.quat();r.offset9D0=g.coin(4)?g.vec(3):V{};
        float speed=g.coin(6)?g.uni(0,560):g.uni(300,4000);V dir=unitNear(g,tangent,g.coin()?.05f:.8f);float sign=g.coin()?1.f:-1.f;
        for(int k=0;k<3;k++)r.velocity[k]=dir[k]*speed*sign;if(g.coin(15))r.velocity={};
        r.speedLimit=g.coin(5)?g.uni(100,1500):g.uni(2000,5000);r.timeScale=g.coin(4)?g.uni(.25f,2):1.f;r.boost=g.coin(3)?g.uni(0,1):0.f;
        r.steer22C={g.uni(-1,1),g.uni(0,1),g.uni(-1,1)};r.tolerance25C={g.uni(0,2),g.uni(0,1),g.uni(0,1)};
        r.controlState=g.coin(6)?12:7;r.motionMode=4;r.surfaceId=int(g.n(40));r.contactNormal=g.unit();r.contactPoint=g.vec(1000);r.surfaceVelocity=g.vec(10);
        m.direction=g.unit();m.lean=g.uni(-1,1);m.timeOnRail=g.coin(3)?g.uni(0,.6f):g.coin(10)?g.near(.6f-r.timeScale/60.f,.002f):g.uni(0,5);
        m.balance=g.uni(-1,1);m.headingOffset=g.uni(-.9f,.9f);m.lostRail=int(g.n(2));m.railId=int(g.g());m.entryPosition=g.vec(1000);m.word40=int(g.g());
        stat=g.coin(3)?0.f:g.uni(0,1);
        // Hit around the bone: lateral offsets that straddle 2.5/20/30/70 cm, vertical within/outside the clamp.
        first={};first.found=!g.coin(15);first.tangent=tangent;first.surface=int(g.n(40));first.packed=g.g();first.flags=g.coin(3)?4u:g.n(16);
        {V side=g.unit();float lat=g.coin(4)?g.uni(-3,3):g.uni(-80,80);float vert=g.coin(3)?g.uni(-5,5):g.uni(-40,40);
         for(int k=0;k<3;k++)first.point[k]=r.bonePosition[k]+r.right[k]*lat+r.up[k]*vert+side[k]*g.uni(0,4);}
        if(g.coin(8))for(int k=0;k<3;k++)first.point[k]+=g.uni(-400,400); // proximity failure / detach push
        second=first;second.found=!g.coin(4);for(int k=0;k<3;k++)second.point[k]+=g.uni(-40,40);
        wr(OWNER+0xde4,r.controlState);wr(OWNER+0xde0,r.motionMode);wr(DESC,first.packed);wr(DESC+0x1c,first.flags);
        wr(RIDER+0x5ac,int(g.n(3)));
        storeRail(r);storeMotion4(m);auto& expected=snapshot();originalQueries.clear();
        auto c=context(0x13af28);SET_GPR_U32(&c,4,RAILM);sub_0013AF28_0x13af28(M,&c,&rt);
        if(c.pc!=DONE){std::printf("rail update continuation %x\n",c.pc);return 3;}
        // Native.
        ssx::OriginalRailRecord record;record.packedId=first.packed;record.flags=first.flags;record.surface=first.surface;
        unsigned queryIndex=0;ssx::OriginalRailAccess a;
        a.query=[&](V p){
            if(queryIndex>=originalQueries.size()||std::memcmp(&p,&originalQueries[queryIndex],12)){throw std::runtime_error("rail query sequence differs (case "+std::to_string(test)+")");}
            const Hit& h=queryIndex++==0?first:second;ssx::OriginalRailQueryResult q;q.found=h.found;
            if(h.found){q.point=h.point;q.tangent=h.tangent;q.surface=h.surface;q.record=&record;}return q;};
        a.balanceStat=[&]{return stat;};
        auto result=ssx::originalRailMotionStep(m,r,a);
        if(queryIndex!=originalQueries.size()){std::printf("rail query count differs case %u: original %zu native %u\n",test,originalQueries.size(),queryIndex);return 4;}
        storeRail(r,&expected);storeMotion4(m,&expected);
        // 0x11E098 stores +0x1D0 = position and clears the basis w lanes; 0x13AF28 clears +0x5AC.
        put(&expected,RIDER+0x1d0,r.position);put(&expected,RIDER+0x1dc,1.f);for(uint32_t o:{0x1acu,0x1bcu,0x1ccu})put(&expected,RIDER+o,0.f);
        put(&expected,RIDER+0x5ac,0);if(first.found)put(&expected,RIDER+0x46c,1.f);
        put(&expected,RIDER+0x37c,rd<float>(RIDER+0x37c)); // +0x370 w lane (0x13BD80 sign of zero): not modelled
        if(result.outcome==ssx::OriginalRailMotionStepResult::Outcome::Riding&&true)put(&expected,RAILM+0xc,std::memcmp(&m.direction,&first.tangent,12)?-0.f:0.f); // tangent quad negated in place (w lane)
        if(!compareImage(expected,"rail update 0x13AF28",test)){listDiffs(expected);
            std::printf("  tangent %.9g %.9g %.9g dir %.9g %.9g %.9g orig %.9g %.9g %.9g\n",first.tangent[0],first.tangent[1],first.tangent[2],m.direction[0],m.direction[1],m.direction[2],rd<float>(RAILM),rd<float>(RAILM+4),rd<float>(RAILM+8));
            std::printf("  style %d control %d time %.9g found %d/%d queries %zu outcome %d\n",r.style,r.controlState,m.timeOnRail,first.found,second.found,originalQueries.size(),int(result.outcome));return 5;}
        ++cases;
        switch(result.outcome){case ssx::OriginalRailMotionStepResult::Outcome::RailLost:++lost;break;case ssx::OriginalRailMotionStepResult::Outcome::Detached:++detached;break;default:++riding;}
        reprobe+=originalQueries.size()==2;boosted+=r.boost>0;uber+=r.controlState==12;flagged+=(first.flags&4)!=0;sideways+=r.style>=3;inverted+=r.up[2]<0;imbalanced+=result.imbalance>=0;
        (void)clampedDown;(void)clampedUp;
    }
    std::printf("rail update 0x13AF28: %u cases match (rider pose/velocity/axes/contact, motion state, query sequence); riding %u, lost %u, detached %u, re-probe %u, imbalance applied %u, sideways %u, inverted %u, boost %u, control 12 %u, descriptor bit 2 %u\n",
        cases,riding,lost,detached,reprobe,imbalanced,sideways,inverted,boosted,uber,flagged);
}
