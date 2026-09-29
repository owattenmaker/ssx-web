#include "rail_motion.hpp"
#include <bit>
#include <cassert>
#include <cfenv>
#include <cmath>
#include <cstdio>
#include <vector>
namespace {
using namespace ssx;
float f(uint32_t b){return std::bit_cast<float>(b);}
RailVector v(uint32_t x,uint32_t y,uint32_t z){return {f(x),f(y),f(z)};}
bool near(float a,float b,float tolerance){return std::abs(a-b)<=tolerance;}
bool nearVector(RailVector a,RailVector b,float tolerance){for(int k=0;k<3;++k)if(!near(a[k],b[k],tolerance))return false;return true;}

// ARA1 spline_ARA1_EventRail_0 (SSB kind8, track8, rid0): segments0 and1 exactly as exported by
// tools/import_rails.py (raw little-endian floats from the owned USA PS2 BAM.SSB).
OriginalRailRecord eventRail0(){
    OriginalRailRecord r;r.packedId=(0u<<8)|8u;r.flags=1;r.surface=-1;
    OriginalRailSegment a;
    a.coefficients={v(0xc21a7000,0xc22f7000,0xc2cd7000),v(0x3d400000,0xbc400000,0x41cf0000),v(0xc4411700,0xc3eda680,0xc3319000),v(0xc827a859,0x4715c8b1,0xc886e9a4)};
    a.boundsMin=v(0xc8287314,0x4713c185,0xc8870971);a.boundsMax=v(0xc827a859,0x4715c8b1,0xc886e9a4);
    a.length=f(0x4479780c);a.distance=0;a.index=0;a.previous=-1;a.next=1;
    OriginalRailSegment b;
    b.coefficients={v(0x42480000,0x4350a400,0x428a1000),v(0xc2e76000,0xc3039a00,0xc38d3000),v(0xc45e0300,0xc417b900,0xc3d8f000),v(0xc8287314,0x4713c185,0xc8870971)};
    b.boundsMin=v(0xc8296183,0x4711afab,0xc8875a58);b.boundsMax=v(0xc8287314,0x4713c185,0xc8870971);
    b.length=f(0x449fb427);b.distance=f(0x4479780c);b.index=1;b.previous=0;b.next=2;
    r.segments={a,b};return r;
}
// Straight rail from start along a unit direction, length cm, single segment.
OriginalRailRecord straightRail(RailVector start,RailVector direction,float length,uint32_t flags=1){
    OriginalRailRecord r;r.packedId=(7u<<8)|8u;r.flags=flags;r.surface=-1;OriginalRailSegment s;
    s.coefficients={RailVector{0,0,0},RailVector{0,0,0},RailVector{direction[0]*length,direction[1]*length,direction[2]*length},start};
    for(int k=0;k<3;++k){float e=start[k]+direction[k]*length;s.boundsMin[k]=std::min(start[k],e);s.boundsMax[k]=std::max(start[k],e);}
    s.length=length;r.segments={s};return r;
}
struct Recorder {
    std::vector<int> controls,motions,semantics,switches;std::vector<float> rotations,roots,scores;
    int cls=15;bool flag0=false,flag2=false;int semantic=18;float stat=f(0x3dba2e8c);bool recovered=false,upper=false;
    int surface=-99;int leaveCalls=0,restoreCalls=0,reactionCalls=0;
    OriginalRailAccess access(std::span<const OriginalRailRecord> world){
        OriginalRailAccess a;
        a.query=[world](RailVector p){return originalRailWorldQuery(world,p);};
        a.channel2Class=[this]{return cls;};
        a.channel2SequenceFlag=[this](unsigned bit){return bit==0?flag0:bit==2?flag2:false;};
        a.currentSemantic=[this]{return semantic;};
        a.playAnimation=[this](int s,float,int){semantics.push_back(s);};
        a.rotateAnimation=[this](float r){rotations.push_back(r);};
        a.setAnimationRoot=[this](float h){roots.push_back(h);};
        a.setAnimationSwitch=[this](int s){switches.push_back(s);};
        a.requestControl=[this](int c){controls.push_back(c);};
        a.requestMotion=[this](int m){motions.push_back(m);};
        a.railEntryScore=[](bool,int style,int){return float(style)*100.f;};
        a.awardScore=[this](float s){scores.push_back(s);};
        a.airborneRailEvent=[this](int style,int,float,const OriginalRailQueryResult&){scores.push_back(-float(style));};
        a.recordRailSurface=[this](int s){surface=s;};
        a.balanceStat=[this]{return stat;};
        a.railSpinScore=[](int,float spin){return spin;};
        a.recovery=[this](bool){return recovered;};
        a.upperAction=[this](bool,bool){return upper;};
        a.boost=[](bool,bool){return false;};
        a.upperReactions=[this]{++reactionCalls;};
        a.restoreStance=[this]{++restoreCalls;};
        a.leaveEffects=[this]{++leaveCalls;};
        return a;
    }
};
OriginalRailRider onRail(RailVector position,RailVector velocity){
    OriginalRailRider r;r.position=position;r.velocity=velocity;r.bonePosition=position;r.speedLimit=5000;r.timeScale=1;
    r.right={1,0,0};r.forward={0,1,0};r.up={0,0,1};r.surfaceForward={0,1,0};r.quaternion={0,0,0,1};r.boneQuaternion={0,0,0,1};return r;
}
}
int main(){
    std::fesetround(FE_TONEAREST);
    // 1. Authored segments chain: evaluating segment0 at t=1 reproduces segment1's start row.
    {
        auto rail=eventRail0();
        auto end=originalRailPoint(rail.segments[0],1.f);
        assert(nearVector(end,rail.segments[1].coefficients[3],0.1f));
        //Exit contacts can change motion/control/velocity before the source's
    //conditional air transition and final speed clamp.
    {
        std::vector<OriginalRailRecord> world;Recorder rec;auto access=rec.access(world);
        OriginalRailMotionState motion;motion.lostRail=1;
        auto rider=onRail({0,0,0},{0,100,0});rider.motionMode=4;rider.controlState=12;rider.speedLimit=1000;
        bool threw=false;try{originalRailMotionLeave(motion,rider,access);}catch(const std::runtime_error&){threw=true;}assert(threw);
        access.exitContacts=[&](auto&r){assert(rec.leaveCalls==1);r.motionMode=2;r.controlState=8;r.position={10,20,30};r.velocity={2000,0,0};};
        auto result=originalRailMotionLeave(motion,rider,access);
        assert(!result.requestAirMotion&&rec.motions.empty());assert(rider.motionMode==2&&rider.controlState==8);
        assert((rider.position==RailVector{10,20,30})&&(rider.velocity==RailVector{1000,0,0}));
    }
    assert(std::fegetround()==FE_TONEAREST);
    }
    // 2. Query a point on the curve: found, sub-centimeter distance, unit tangent, parameter inside [0,1].
    {
        auto rail=eventRail0();std::vector<OriginalRailRecord> world{rail};
        auto probe=originalRailPoint(rail.segments[0],.6f);
        auto hit=originalRailWorldQuery(world,probe);
        assert(hit.found&&hit.record==&world[0]&&hit.segment==&world[0].segments[0]);
        assert(hit.distance<1.0f&&hit.t>0.f&&hit.t<1.f&&near(hit.t,.6f,.01f));
        float tl=std::sqrt(hit.tangent[0]*hit.tangent[0]+hit.tangent[1]*hit.tangent[1]+hit.tangent[2]*hit.tangent[2]);
        assert(near(tl,1.f,1e-3f)&&hit.surface==-1);
        // A point on segment1 selects segment1 (strictly closer replaces the accumulated result).
        auto probe1=originalRailPoint(rail.segments[1],.4f);
        auto hit1=originalRailWorldQuery(world,probe1);
        assert(hit1.found&&hit1.segment==&world[0].segments[1]&&hit1.distance<1.0f);
        // Outside the 300 cm box nothing is found; a record without bit0 is skipped.
        RailVector far=probe;far[2]+=1000.f;assert(!originalRailWorldQuery(world,far).found);
        world[0].flags=0;assert(!originalRailWorldQuery(world,probe).found);
        //Exit contacts can change motion/control/velocity before the source's
    //conditional air transition and final speed clamp.
    {
        std::vector<OriginalRailRecord> world;Recorder rec;auto access=rec.access(world);
        OriginalRailMotionState motion;motion.lostRail=1;
        auto rider=onRail({0,0,0},{0,100,0});rider.motionMode=4;rider.controlState=12;rider.speedLimit=1000;
        bool threw=false;try{originalRailMotionLeave(motion,rider,access);}catch(const std::runtime_error&){threw=true;}assert(threw);
        access.exitContacts=[&](auto&r){assert(rec.leaveCalls==1);r.motionMode=2;r.controlState=8;r.position={10,20,30};r.velocity={2000,0,0};};
        auto result=originalRailMotionLeave(motion,rider,access);
        assert(!result.requestAirMotion&&rec.motions.empty());assert(rider.motionMode==2&&rider.controlState==8);
        assert((rider.position==RailVector{10,20,30})&&(rider.velocity==RailVector{1000,0,0}));
    }
    assert(std::fegetround()==FE_TONEAREST);
    }
    // 3. Board axes: identity quaternion gives the unit basis; +90 degree yaw maps x to +y.
    {
        auto a=originalRailBoardAxes({0,0,0,1});
        assert(nearVector(a.x,{1,0,0},0)&&nearVector(a.y,{0,1,0},0)&&nearVector(a.z,{0,0,1},0));
        float h=f(0x3f3504f3);auto b=originalRailBoardAxes({0,0,h,h});
        assert(nearVector(b.x,{0,1,0},2e-7f)&&nearVector(b.y,{-1,0,0},2e-7f)&&nearVector(b.z,{0,0,1},0));
    }
    // 4. Attach: velocity projection, speed floor, z damping, style classification and control requests.
    {
        for(auto [theta,expectedStyle,velocitySign]:std::vector<std::tuple<float,int,float>>{{0.f,1,1.f},{0.f,2,-1.f},{f(0x3f860a92),3,1.f},{-f(0x3f860a92),4,1.f},{f(0x3e860a92),1,1.f}}){
            RailVector direction{std::cos(theta),0,std::sin(theta)};
            RailVector start{0,0,0};start[0]=-500.f*direction[0];start[2]=-500.f*direction[2];
            std::vector<OriginalRailRecord> world{straightRail(start,direction,1000.f)};
            Recorder rec;auto access=rec.access(world);
            auto r=onRail({0,0,0},{velocitySign*200.f*direction[0],0,velocitySign*200.f*direction[2]});
            r.controlState=0;r.motionMode=0;OriginalRailMotionState m;
            auto result=originalRailAttach(r,m,access);
            assert(result.attached&&result.style==expectedStyle&&r.style==expectedStyle);
            assert((rec.motions==std::vector<int>{4})&&(rec.controls==std::vector<int>{13,7})&&rec.surface==-1);
            assert(result.entrySemantic==originalRailEntrySemantic(expectedStyle,false)&&rec.semantics.back()==result.entrySemantic);
            assert(rec.scores.size()==1&&rec.scores[0]==float(expectedStyle)*100.f);
            float speed=std::sqrt(r.velocity[0]*r.velocity[0]+r.velocity[1]*r.velocity[1]+r.velocity[2]*r.velocity[2]);
            assert(near(speed,200.f*std::sqrt(direction[0]*direction[0]+direction[2]*direction[2]*.01f),1.f)); // z damped by 0.1
        }
        // Speed floor 555.5555 applies to the along-rail projection, capped by the actual speed.
        std::vector<OriginalRailRecord> world{straightRail({-500,0,0},{1,0,0},1000.f)};
        Recorder rec;auto access=rec.access(world);auto r=onRail({0,0,0},{1000,300,0});OriginalRailMotionState m;
        assert(originalRailAttach(r,m,access).attached);
        assert(nearVector(r.velocity,{1000,0,0},1e-3f));
        auto r2=onRail({0,0,0},{1000,-2000,0});Recorder rec2;auto access2=rec2.access(world);
        assert(originalRailAttach(r2,m,access2).attached);
        assert(near(r2.velocity[0],1000.f,1e-3f)&&r2.velocity[1]==0);
        // Airborne (motion1, control5) entry reports through the airborne event and semantic 68.
        auto r3=onRail({0,0,0},{600,0,-100});r3.motionMode=1;r3.controlState=5;Recorder rec3;auto access3=rec3.access(world);
        auto res3=originalRailAttach(r3,m,access3);
        assert(res3.attached&&res3.fromAir&&res3.entrySemantic==68&&(rec3.scores==std::vector<float>{-1.f}));
        assert(near(r3.velocity[2],0.f,1e-6f)&&near(r3.velocity[0],600.f,1e-3f));
        assert(m.headingOffset==f(0x3f490fdc)); // forward +Y across a +X rail: atan2 clamps to 45 degrees
    }
    // 5. Attach test rejections: behind the rider, grab class gating, foreign motion mode.
    {
        std::vector<OriginalRailRecord> world{straightRail({-500,0,0},{1,0,0},1000.f)};
        Recorder rec;auto access=rec.access(world);OriginalRailQueryResult out;
        auto r=onRail({0,0,0},{300,0,0});r.motionMode=2;assert(!originalRailAttachTest(r,access,out));
        r.motionMode=0;r.bonePosition={0,20,0};r.position=r.bonePosition;r.velocity={0,-300,0};
        assert(originalRailAttachTest(r,access,out));
        r.velocity={0,300,0};assert(!originalRailAttachTest(r,access,out)); // rail is behind the rider
        r.velocity={0,-300,0};rec.cls=18;rec.flag0=true;rec.flag2=false;assert(!originalRailAttachTest(r,access,out));
        rec.flag2=true;assert(originalRailAttachTest(r,access,out));
        rec.cls=19;rec.flag2=false;assert(!originalRailAttachTest(r,access,out));
        rec.cls=20;rec.flag2=true;assert(originalRailAttachTest(r,access,out));
    }
    // 6. Motion step on a horizontal rail: rides along it, no lateral drift, direction retained.
    {
        std::vector<OriginalRailRecord> world{straightRail({0,-500,0},{0,1,0},1000.f)};
        Recorder rec;auto access=rec.access(world);
        auto r=onRail({0,0,0},{0,1200,0});OriginalRailMotionState m;originalRailMotionBegin(m,r);
        assert(r.tolerance25C.rate==f(0x3d4cccce)&&r.tolerance25C.target==0);
        auto step=originalRailMotionStep(m,r,access);
        assert(step.outcome==OriginalRailMotionStepResult::Outcome::Riding);
        assert(r.tolerance25C.current==2.f&&m.timeOnRail==f(0x3c888889)&&m.lostRail==0);
        assert(nearVector(m.direction,{0,1,0},1e-6f));
        assert(near(r.position[1],1200.f*f(0x3c888889),1e-3f)&&r.position[0]==0&&near(r.position[2],0.f,1e-4f));
        assert(nearVector(r.velocity,{0,1200,0},1e-3f));
        assert(step.slide==0&&step.imbalance==-5.f&&m.railId==int32_t((7u<<8)|8u)&&r.surfaceId==-1);
        // Boost accelerates along the rail by 2450 cm/s^2 per unit.
        auto r2=onRail({0,0,0},{0,1200,0});OriginalRailMotionState m2;originalRailMotionBegin(m2,r2);r2.boost=1.f;
        originalRailMotionStep(m2,r2,access);
        assert(near(r2.velocity[1],1200.f+2450.f*f(0x3c888889),1e-2f));
        // Reverse travel flips the retained direction.
        auto r3=onRail({0,0,0},{0,-900,0});OriginalRailMotionState m3;originalRailMotionBegin(m3,r3);
        originalRailMotionStep(m3,r3,access);assert(nearVector(m3.direction,{0,-1,0},1e-6f));
        // Steering slides sideways at 180 * clamp(speed factor) cm/s once the imbalance gate is closed.
        auto r4=onRail({0,0,0},{0,1200,0});OriginalRailMotionState m4;originalRailMotionBegin(m4,r4);r4.steer22C.current=1.f;
        auto step4=originalRailMotionStep(m4,r4,access);
        assert(near(step4.slide,1.08f*180.f,1e-2f)&&near(r4.position[0],step4.slide*f(0x3c888889),1e-3f));
        // After 0.6 s on the rail an offset produces imbalance scaled by the stat.
        auto r5=onRail({0,0,0},{0,1200,0});OriginalRailMotionState m5;originalRailMotionBegin(m5,r5);m5.timeOnRail=1.f;r5.bonePosition={-10,0,0};r5.position={-10,0,0};
        auto step5=originalRailMotionStep(m5,r5,access);
        assert(step5.outcome==OriginalRailMotionStepResult::Outcome::Riding&&near(step5.lateralOffset,10.f,1e-3f));
        float expectedImbalance=f(0x3e99999a)/(1.f+rec.stat*f(0x3fd335b2));
        assert(near(step5.imbalance,expectedImbalance,1e-5f)&&step5.slide<0);
        assert(near(m5.balance,10.f*f(0x3d088889),1e-6f)&&r5.style!=3);
        // Lost rail: query outside the box leaves the flag set and only rebuilds.
        auto r6=onRail({0,2000,0},{0,1200,0});OriginalRailMotionState m6;originalRailMotionBegin(m6,r6);
        assert(originalRailMotionStep(m6,r6,access).outcome==OriginalRailMotionStepResult::Outcome::RailLost&&m6.lostRail==1);
        auto leave=originalRailMotionLeave(m6,r6,access);
        assert(leave.lostRail&&!leave.requestAirMotion&&rec.leaveCalls==1&&r6.steer22C.rate==f(0x3d088889));
        r6.motionMode=4;originalRailMotionLeave(m6,r6,access);assert(rec.motions.back()==1);
        // Detach: too far sideways for the board line pushes 277.777 cm/s away from the rail
        // (0x13BB14 direction = bone - hit), or along the remainder dir - vel*(dot/|vel|) when moving against it.
        auto hit7=originalRailWorldQuery(world,{200,0,0});assert(hit7.found);
        double ox=200.-hit7.point[0],oy=-hit7.point[1],ol=std::sqrt(ox*ox+oy*oy);
        auto expectPush=[&](double vx,double vy,double& ex,double& ey,bool& remainder){
            double dx=ox/ol,dy=oy/ol,dotv=vx*dx+vy*dy;remainder=dotv<0;
            if(remainder){double sp=std::sqrt(vx*vx+vy*vy),rx=dx-vx*(dotv/sp),ry=dy-vy*(dotv/sp),rl=std::sqrt(rx*rx+ry*ry);dx=rx/rl;dy=ry/rl;}
            ex=vx+dx*f(0x438ae38e);ey=vy+dy*f(0x438ae38e);};
        bool remainders[2];
        for(int k=0;k<2;++k){
            const double vy=k?1200.:-1200.;auto r7=onRail({200,0,0},{0,float(vy),0});OriginalRailMotionState m7;originalRailMotionBegin(m7,r7);m7.timeOnRail=1.f;
            assert(originalRailMotionStep(m7,r7,access).outcome==OriginalRailMotionStepResult::Outcome::Detached);
            double ex,ey;expectPush(0,vy,ex,ey,remainders[k]);
            assert(near(r7.velocity[0],float(ex),5e-2f)&&near(r7.velocity[1],float(ey),5e-2f));
        }
        assert(remainders[0]!=remainders[1]);
    }
    // 7. Heading offset clamps to +-45 degrees.
    {
        auto r=onRail({0,0,0},{0,0,0});OriginalRailMotionState m;
        originalRailHeadingOffset(m,r,{1,0,0});assert(m.headingOffset==f(0x3f490fdc));
        originalRailHeadingOffset(m,r,{-1,0,0});assert(m.headingOffset==-f(0x3f490fdc));
        originalRailHeadingOffset(m,r,{0,1,0});assert(m.headingOffset==0);
        float c=f(0x3f3504f3);originalRailHeadingOffset(m,r,{c,c,0});assert(near(m.headingOffset,f(0x3f490fdb),5e-5f)); // original 0x31C228 polynomial error
    }
    // 8. Rotation table 0x132060.
    {
        struct Case{int style;bool left;int semantic,next;};
        for(auto c:std::vector<Case>{{1,true,49,3},{2,true,53,4},{3,true,51,2},{4,true,54,1},{1,false,50,4},{2,false,52,3},{3,false,51,1},{4,false,54,2}}){
            std::vector<OriginalRailRecord> world;Recorder rec;auto access=rec.access(world);
            auto r=onRail({0,0,0},{0,0,0});r.style=c.style;r.balance.current=.25f;r.balance.target=.5f;OriginalRailControlState control;
            auto e=originalRailRotation(control,r,c.left,access);
            assert(e.semantic==c.semantic&&e.newStyle==c.next&&r.style==c.next&&rec.semantics.back()==c.semantic);
            assert(control.spin==(c.left?-f(0x3fc90fdc):f(0x3fc90fdc))&&rec.scores.back()==control.spin);
            bool expectNegate=(c.style==2)||(c.left&&c.style==3)||(!c.left&&c.style==4);
            assert(e.balanceNegated==expectNegate&&(expectNegate?r.balance.current==-.25f:r.balance.current==.25f));
            assert(r.reverseStance==(c.next==2?1:0));
        }
    }
    // 9. Command decoding.
    {
        uint32_t word0=(1u<<12)|(1u<<14)|(1u<<16)|(uint32_t(0xFFu)<<17)|(uint32_t(62u)<<25); // turn -2
        uint32_t word1=uint32_t(3u)|(uint32_t(61u)<<6);                                          // rotate +3, transfer -3
        auto c=originalRailDecodeCommand(word0,word1);
        assert(c.recovery&&!c.upper13&&c.upper14&&!c.boostPressed15&&c.boostHeld16&&c.identity==-1);
        assert(c.turn==-2.f*f(0x3d042108)&&near(c.rotate,3.f*f(0x3d042108),1e-8f)&&near(c.transfer,-3.f*f(0x3d042108),1e-8f));
    }
    // 10. Control7 step: cycle semantic, balance target, rotation dispatch, airborne exit.
    {
        std::vector<OriginalRailRecord> world;Recorder rec;auto access=rec.access(world);
        auto r=onRail({0,0,0},{0,1000,0});r.style=3;r.motionMode=4;OriginalRailControlState control;OriginalRailMotionState m;m.balance=.5f;
        originalRailControlBegin(control,r);assert(r.animationTurn.rate==f(0x3d888889)&&control.identity==-1);
        rec.semantic=18;auto res=originalRailControlStep(control,r,m,{},access);
        assert(res.stop==OriginalRailControlResult::Stop::None&&res.cycleRequested&&res.cycleSemantic==19&&rec.semantics.back()==19);
        assert(r.balance.target==.5f*f(0x3f99999a)&&r.balance.rate==f(0x3d888889)&&rec.reactionCalls==1);
        m.balance=1.f;rec.semantic=19;res=originalRailControlStep(control,r,m,{},access);
        assert(!res.cycleRequested&&r.balance.target==1.f);
        rec.cls=10;res=originalRailControlStep(control,r,m,{},access);assert(r.balance.target==0);
        rec.cls=15;OriginalRailCommand turnLeft;turnLeft.rotate=-f(0x3d042108);
        res=originalRailControlStep(control,r,m,turnLeft,access);
        assert(res.stop==OriginalRailControlResult::Stop::Rotated&&r.style==2&&rec.semantics.back()==51);
        rec.cls=14;rec.flag0=false;res=originalRailControlStep(control,r,m,turnLeft,access);
        assert(res.stop==OriginalRailControlResult::Stop::RotationPending&&r.style==2);
        rec.flag0=true;res=originalRailControlStep(control,r,m,turnLeft,access);assert(r.style==4);
        // Airborne: control 5 with stick, 4 without, after the rotation clip completes.
        r.motionMode=1;rec.cls=14;rec.flag0=false;res=originalRailControlStep(control,r,m,turnLeft,access);
        assert(res.stop!=OriginalRailControlResult::Stop::Airborne);
        rec.flag0=true;res=originalRailControlStep(control,r,m,turnLeft,access);
        assert(res.stop==OriginalRailControlResult::Stop::Airborne&&rec.controls.back()==5&&rec.restoreCalls==1);
        res=originalRailControlStep(control,r,m,{},access);assert(rec.controls.back()==4);
        r.motionMode=4;originalRailControlLeave(r);assert(r.balance.target==0&&r.balance.rate==f(0x3d888889));
        // Steer target rate follows 0x113F38.
        r.steer22C.current=0;originalRailSteerTarget(r,1.f);assert(r.steer22C.target==1.f&&near(r.steer22C.rate,7.f*f(0x3c888889),1e-7f));
        originalRailSteerTarget(r,0.01f);assert(near(r.steer22C.rate,f(0x3dcccccd)*f(0x3c888889),1e-8f));
        // Gaps throw instead of guessing.
        bool threw=false;OriginalRailCommand identity;identity.identity=3;
        try{originalRailControlStep(control,r,m,identity,access);}catch(const std::runtime_error&){threw=true;}assert(threw);
        threw=false;OriginalRailCommand transfer;transfer.transfer=f(0x3d042108);r.style=1;r.motionMode=0;
        try{originalRailControlStep(control,r,m,transfer,access);}catch(const std::runtime_error&){threw=true;}assert(threw);
        r.velocity={0,-1000,0};rec.cls=15;res=originalRailControlStep(control,r,m,transfer,access); // backward: 0x1161D0 declines, input becomes rotation
        assert(res.stop==OriginalRailControlResult::Stop::Rotated);
    }
    //132620 runs after upper/recovery gates and successful entry ends control7.
    {
        std::vector<OriginalRailRecord> world;Recorder rec;auto access=rec.access(world);
        auto rider=onRail({0,0,0},{0,900,0});rider.motionMode=4;OriginalRailControlState control;OriginalRailMotionState motion;
        std::vector<int> calls;access.upperAction=[&](bool,bool){calls.push_back(1);return false;};
        access.uberEntry=[&](auto&r,int identity){calls.push_back(2);assert(identity==3);r.controlState=12;return true;};
        access.boost=[&](bool,bool){calls.push_back(3);return false;};
        OriginalRailCommand command;command.identity=3;
        const auto result=originalRailControlStep(control,rider,motion,command,access);
        assert(result.stop==OriginalRailControlResult::Stop::Uber&&rider.controlState==12);
        assert((calls==std::vector<int>{1,2}));
    }
    // 11. Stance alignment: sideways styles rotate +-90 degrees about up without renormalizing.
    {
        std::vector<OriginalRailRecord> world;Recorder rec;auto access=rec.access(world);
        auto r=onRail({0,0,0},{0,0,0});r.style=3;originalRailStanceAlignment(r,access);
        auto expected=originalRotateOrientation({0,0,0,1},{0,0,1},f(0xbfc90fdb));
        assert(r.quaternion==expected&&(rec.rotations==std::vector<float>{f(0xbfc90fdb)})&&(rec.roots==std::vector<float>{f(0x3f490fdb)}));
        auto s=onRail({0,0,0},{0,0,0});s.style=2;Recorder rec2;auto access2=rec2.access(world);originalRailStanceAlignment(s,access2);
        assert(s.reverseStance==1&&(rec2.rotations==std::vector<float>{f(0x40490fdb)})&&(rec2.roots==std::vector<float>{f(0xbfc90fdb)}));
        assert(near(std::abs(s.quaternion[2]),1.f,1e-6f)&&near(s.forward[1],-1.f,1e-6f));
        auto t=onRail({0,0,0},{0,0,0});t.style=1;t.reverseStance=0;Recorder rec3;auto access3=rec3.access(world);originalRailStanceAlignment(t,access3);
        assert(rec3.rotations.empty()&&t.quaternion==(std::array<float,4>{0,0,0,1}));
    }
    //Exit contacts can change motion/control/velocity before the source's
    //conditional air transition and final speed clamp.
    {
        std::vector<OriginalRailRecord> world;Recorder rec;auto access=rec.access(world);
        OriginalRailMotionState motion;motion.lostRail=1;
        auto rider=onRail({0,0,0},{0,100,0});rider.motionMode=4;rider.controlState=12;rider.speedLimit=1000;
        bool threw=false;try{originalRailMotionLeave(motion,rider,access);}catch(const std::runtime_error&){threw=true;}assert(threw);
        access.exitContacts=[&](auto&r){assert(rec.leaveCalls==1);r.motionMode=2;r.controlState=8;r.position={10,20,30};r.velocity={2000,0,0};};
        auto result=originalRailMotionLeave(motion,rider,access);
        assert(!result.requestAirMotion&&rec.motions.empty());assert(rider.motionMode==2&&rider.controlState==8);
        assert((rider.position==RailVector{10,20,30})&&(rider.velocity==RailVector{1000,0,0}));
    }
    assert(std::fegetround()==FE_TONEAREST);
    std::puts("Original rail helpers: spline query, attach, motion4, control7, rotation and stance checks pass");
}
