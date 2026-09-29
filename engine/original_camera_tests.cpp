#include "original_camera.hpp"
#include "../local/reference/original_camera_oracle.hpp" // PCSX2 savestate words (local-only, never published)
#include "original_camera_words.hpp"
#include <bit>
#include <cassert>
#include <cfenv>
#include <cmath>
#include <cstdio>
#include <string_view>
#include <vector>
namespace {
using namespace ssx;using namespace ssx::original_camera_words;
using Quad=OriginalCameraQuad;
Quad q(const std::array<uint32_t,4>& w){return {f(w[0]),f(w[1]),f(w[2]),f(w[3])};}
bool finite(const Quad& v){for(float x:v)if(!std::isfinite(x))return false;return true;}
float dist3(const Quad& a,const Quad& b){double s=0;for(int i=0;i<3;++i){double d=double(a[i])-b[i];s+=d*d;}return float(std::sqrt(s));}

OriginalCameraInput inputFrom(const original_camera_oracle::Snapshot& s){
    OriginalCameraInput in;
    in.headPosition=q(s.head);in.velocity=q(s.velocity);in.riderForward=q(s.forward);in.previousContactNormal=q(s.previousNormal);
    in.wallNormal=q(s.wallNormal);in.motionMode=s.mode;in.boostLevel=f(s.boost);in.jumpCharge=f(s.charge);in.surfaceId=s.surfaceId;
    in.riderType=s.riderType;in.launchValue=f(s.launchValue);in.proximityFlag=s.proximity!=0;
    in.trajectoryStatusActive=s.trajectoryStatus==1||s.trajectoryStatus==3;in.predictedAirTime=f(s.trajectoryTimes[0]);
    in.trajectoryHeading=q(s.trajectoryHeading);in.trajectoryNormal=q(s.trajectoryNormal);in.tick=s.tick;in.farCap=f(s.fovNearFar[2]);
    return in;
}
// Offsets the decode names as written by the per-frame path; everything else
// in the object is either untouched (compared implicitly by seeding) or heap.
const std::vector<unsigned> comparedOffsets=[]{
    std::vector<unsigned> v;
    auto quad=[&](unsigned o){for(unsigned i=0;i<4;++i)v.push_back(o+4*i);};
    v.insert(v.end(),{0x00,0x50,0x54});quad(0x20);quad(0x40);quad(0x60);quad(0x80);quad(0xA0);quad(0xC0);quad(0x100);quad(0x1B0);quad(0x1C0);
    for(unsigned o=0x1D0;o<=0x228;o+=4)v.push_back(o);
    v.insert(v.end(),{0x248,0x2B8,0x2C0,0x2C4,0x2CC,0x2D0,0x2D4,0x2D8,0x2E0,0x2E4,0x2E8,0x2F0,0x2F4,0x2F8,0x2FC,0x300});
    for(unsigned o=0x340;o<0x380;o+=4)v.push_back(o);
    return v;
}();
unsigned compareOracle(const original_camera_oracle::Snapshot& before,const original_camera_oracle::Snapshot& after,bool checkCompositor,float tolerance){
    OriginalCameraState state;
    state.algorithm=fromWords(before.algorithm);
    state.compositor.lookAt=q({before.compositorLookEye[0],before.compositorLookEye[1],before.compositorLookEye[2],before.compositorLookEye[3]});
    state.compositor.eye=q({before.compositorLookEye[8],before.compositorLookEye[9],before.compositorLookEye[10],before.compositorLookEye[11]});
    state.compositor.lift=f(before.lift);state.compositor.lastProbeNormal=q(before.lastProbeNormal);state.compositor.firstFrameAfterReset=0;
    auto in=inputFrom(after);
    auto out=originalChaseCameraStep(state,in);
    auto words=toWords(state.algorithm);
    unsigned mismatches=0;
    for(unsigned off:comparedOffsets){
        uint32_t expected=after.algorithm[off/4],actual=words[off/4];
        if(off>=0x304&&off<0x340&&(off-0x304)%12==0){expected&=0x7FFFFFF;actual&=0x7FFFFFF;}
        if(expected!=actual){
            bool tolerated=tolerance>0&&std::fabs(f(expected)-f(actual))<=tolerance*std::max(1.f,std::fabs(f(expected))*1e-3f);
            if(!tolerated)++mismatches;
            std::printf("  %s +0x%03X expected %08x (%g) got %08x (%g)%s\n",after.name,off,expected,f(expected),actual,f(actual),tolerated?" [tolerated]":"");
        }
    }
    for(unsigned i=0;i<5;++i){
        unsigned off=0x304+12*i;uint32_t expected=after.algorithm[off/4]&0x7FFFFFFu,actual=words[off/4]&0x7FFFFFFu;
        if(expected!=actual){++mismatches;std::printf("  %s spline +0x%03X flags expected %08x got %08x\n",after.name,off,expected,actual);}
    }
    if(checkCompositor){
        Quad expected=q(after.outerEye);
        for(unsigned i=0;i<3;++i)if(u(out.eye[i])!=u(expected[i])&&!(tolerance>0&&std::fabs(out.eye[i]-expected[i])<1.f)){++mismatches;std::printf("  %s compositor eye[%u] expected %08x (%g) got %08x (%g)\n",after.name,i,u(expected[i]),expected[i],u(out.eye[i]),out.eye[i]);}
        if(u(state.compositor.lift)!=after.lift){++mismatches;std::printf("  %s lift expected %08x got %08x\n",after.name,after.lift,u(state.compositor.lift));}
        if(u(out.fov)!=after.fovNearFar[0]||u(out.near)!=after.fovNearFar[1]||u(out.far)!=after.fovNearFar[2]){++mismatches;std::printf("  %s fov/near/far mismatch %08x %08x %08x\n",after.name,u(out.fov),u(out.near),u(out.far));}
    }
    return mismatches;
}

// Reference natural cubic spline in double precision (same knot layout).
struct ReferenceSpline {
    std::vector<double> x,y,b,c,d;
    ReferenceSpline(std::vector<double> X,std::vector<double> Y):x(X),y(Y){
        size_t n=x.size()-1;std::vector<double> h(n),al(n+1),l(n+1),mu(n+1),z(n+1);b.assign(n,0);c.assign(n+1,0);d.assign(n,0);
        for(size_t i=0;i<n;++i)h[i]=x[i+1]-x[i];
        for(size_t i=1;i<n;++i)al[i]=3/h[i]*(y[i+1]-y[i])-3/h[i-1]*(y[i]-y[i-1]);
        l[0]=1;mu[0]=0;z[0]=0;
        for(size_t i=1;i<n;++i){l[i]=2*(x[i+1]-x[i-1])-h[i-1]*mu[i-1];mu[i]=h[i]/l[i];z[i]=(al[i]-h[i-1]*z[i-1])/l[i];}
        l[n]=1;z[n]=0;c[n]=0;
        for(int j=int(n)-1;j>=0;--j){c[j]=z[j]-mu[j]*c[j+1];b[j]=(y[j+1]-y[j])/h[j]-h[j]*(c[j+1]+2*c[j])/3;d[j]=(c[j+1]-c[j])/(3*h[j]);}
    }
    double operator()(double v)const{size_t j=0;while(j+1<x.size()-1&&v>x[j+1])++j;double t=v-x[j];return y[j]+b[j]*t+c[j]*t*t+d[j]*t*t*t;}
};

// Synthetic cruise -> jump -> landing rider on flat ground at z=0, moving +X.
struct SyntheticRider {
    double x=0,y=0,z=0,vx=2000,vz=0;bool air=false;int airFrames=0;uint32_t tick=1000;
    OriginalCameraInput frame(int i){
        if(i==200){air=true;vz=650;}
        if(air){z+=vz/60;vz-=981.0/60;++airFrames;if(z<=0){z=0;vz=0;air=false;airFrames=0;}}
        x+=vx/60;++tick;
        OriginalCameraInput in;
        in.headPosition={float(x),float(y),float(z+150),1};
        in.velocity={float(vx),0,float(air?vz:0),0};
        in.riderForward={1,0,0,0};in.previousContactNormal={0,0,1,0};
        in.motionMode=air?1:0;in.jumpCharge=air?0.f:0.3f;in.launchValue=air?450.f:0.f;
        in.trajectoryStatusActive=false;in.predictedAirTime=air?1.3f:0.f;
        in.trajectoryHeading={float(vx),0,float(vz),0};in.trajectoryNormal={0,0,1,0};
        in.tick=tick;return in;
    }
};
}
int main(){
    std::fesetround(FE_TONEAREST);
    // 1. Constructor defaults (0x1622B0/0x162318/0x176D68).
    {
        OriginalChaseAlgorithmState d;
        assert(u(d.fov)==0x3F490FDBu&&d.near==10&&d.far==30000);
        assert(d.eye==(Quad{0,0,0,1})&&d.lookAt==(Quad{0,0,0,0})&&d.yaw==0&&d.pitch==0);
        assert(d.distance==200&&d.eyeVerticalOffset==0&&d.filteredPitch==0&&d.fovScale==1&&d.mode5Timer==10&&d.lookAtHeight==0);
        assert(d.mode5Frames==5000&&d.wallLaunch==0&&d.swingInitialised==0&&d.ringIndex==0);
        assert(u(d.gainP)==0x3C3AB019u&&d.gainI==0&&u(d.gainD)==0x3AA793A0u);
        assert(d.pullBehind==(Quad{0,0,0,0})&&d.fieldD0==(Quad{0,0,0,0})&&d.fieldE0==(Quad{0,0,0,0}));
        OriginalCameraCompositorState o;
        assert(o.lift==0&&o.lastProbeNormal==(Quad{0,0,1,0})&&o.lastUnoccludedEye==(Quad{0,0,0,1})&&o.firstFrameAfterReset==1);
        assert(u(o.fovMaximum)==0x3F490FDBu&&o.nearMinimum==30&&o.farMaximum==30000);
    }
    // 2. Set-target seeds (0x166C60 + 0x166550) after begin().
    {
        OriginalCameraInput in;in.headPosition={1000,2000,300,1};in.velocity={800,100,0,0};in.riderForward={0.99f,0.1f,0,0};in.tick=5;
        OriginalCameraState s;s.begin(in);
        assert(s.algorithm.mode5Frames==5000&&s.algorithm.landed==1&&s.algorithm.takeoffCountdown==-1&&s.algorithm.lockState==1);
        assert(u(s.algorithm.landingDecay)==0x3F6B851Fu&&s.algorithm.takeoffRamp==1&&s.algorithm.resetPending==0);
        assert(std::fabs(s.algorithm.lockView[0]-247.5f)<1e-3f&&std::fabs(s.algorithm.lockView[1]-25)<1e-3f&&s.algorithm.lockView[2]==0&&s.algorithm.lastVelocity==in.velocity);
        assert(finite(s.algorithm.eye)&&finite(s.algorithm.lookAt)&&s.algorithm.eye[3]==1);
        // +0x80 is normalize(fwd*559.744 - Z*300): pointing forward and down.
        assert(s.algorithm.direction[0]>0&&s.algorithm.direction[2]<0&&std::fabs(std::hypot(s.algorithm.direction[0],s.algorithm.direction[1],s.algorithm.direction[2])-1)<0.05f); // second update blends +0x80 toward the velocity direction without renormalising
        // Two updates + one finish ran: eye sits behind the rider along +0x80.
        assert(s.algorithm.eye[0]<in.headPosition[0]);
    }
    // 3. cCSICubicSpline: natural build vs a double-precision reference.
    {
        std::vector<double> X{0,1,2,3,4},Y{0,1,0,-1,0.5};
        ReferenceSpline ref(X,Y);OriginalCubicSpline sp;
        {original_camera::Rounding rounding;sp.reset(5);for(size_t i=0;i<5;++i)sp.add(float(X[i]),float(Y[i]));}
        assert(sp.built&&sp.segments==4&&sp.count==4&&sp.endValue==4);
        for(double v=0;v<=4;v+=0.125){
            float got;{original_camera::Rounding rounding;got=sp.evaluate(float(v));}
            assert(std::fabs(got-ref(v))<2e-5);
        }
        // Coefficients bit-compare against the reference rounded to float.
        for(unsigned i=0;i<4;++i){assert(std::fabs(sp.records[i].a0-float(Y[i]))==0);assert(std::fabs(sp.records[i].a1-ref.b[i])<1e-5);assert(std::fabs(sp.records[i].a2-ref.c[i])<1e-5);assert(std::fabs(sp.records[i].a3-ref.d[i])<1e-5);}
        // Evaluation cache path: descending lookups and out-of-order accesses agree with fresh objects.
        OriginalCubicSpline fresh=sp;
        for(double v:{3.9,0.2,2.5,1.1,3.97,0.05,2.99}){ // off-knot: at a knot the cached lower segment is used (source tie rule)
            float a,b;{original_camera::Rounding rounding;a=sp.evaluate(float(v));OriginalCubicSpline copy=fresh;b=copy.evaluate(float(v));}
            assert(u(a)==u(b));
        }
        // Clamped (zero end slopes) swing ease curve: ends flat, monotone.
        OriginalCubicSpline ease;{original_camera::Rounding rounding;original_camera::buildSwingSpline(ease);}
        assert(ease.clamped&&ease.built&&ease.segments==8);
        float previous=-1;
        for(int i=0;i<=64;++i){float v=i/64.f,got;{original_camera::Rounding rounding;got=ease.evaluate(v);}assert(got>=previous-1e-6f);previous=got;}
        float e0,e1,e2,e3;{original_camera::Rounding rounding;e0=ease.evaluate(0);e1=ease.evaluate(0.01f);e2=ease.evaluate(1);e3=ease.evaluate(0.99f);}
        assert(e0==0&&e2==1&&std::fabs(e1-e0)<2e-3f&&std::fabs(e2-e3)<2e-3f);
    }
    // 4. PCSX2 savestate oracle: one tick from frame N with frame N+1 rider state.
    {
        unsigned total=0;
        for(auto [before,after]:original_camera_oracle::pairs){
            bool compositor=u(original_camera::emul(f(before->lift),std::bit_cast<float>(0x3F7851ECu)))==after->lift; // no lift push happened this frame
            // charge23 is the crouch-start frame: the velocity/charge the camera read
            // mid-frame differ from the savestate's end-of-frame rider (+0x1E0/+0x220);
            // its words agree only to ~1e-2 and are checked with a tolerance below.
            bool crouchStart=std::string_view(after->name)=="charge23N1";
            unsigned m=compareOracle(*before,*after,compositor,crouchStart?0.09f:0.f);
            std::printf("oracle %s: %u mismatching words%s\n",after->name,m,compositor?" (compositor checked)":"");
            total+=m;
        }
        assert(total==0);
    }
    // 5. Determinism + 600-frame synthetic cruise/jump/landing run.
    {
        std::vector<OriginalCameraOutput> first,second;
        for(int run=0;run<2;++run){
            SyntheticRider rider;OriginalCameraState state;auto seed=rider.frame(0);state.begin(seed);
            auto& outs=run?second:first;
            for(int i=1;i<600;++i){auto in=rider.frame(i);outs.push_back(originalChaseCameraStep(state,in));
                const auto& o=outs.back();
                assert(finite(o.eye)&&finite(o.lookAt)&&std::isfinite(o.fov)&&std::isfinite(o.yaw)&&std::isfinite(o.pitch));
                float d=dist3(o.eye,o.lookAt);
                assert(d>=100&&d<=1600);
                // Eye stays behind the rider (travel is +X) and above the head.
                assert(o.eye[0]<in.headPosition[0]&&o.eye[2]>in.headPosition[2]);
                assert(!o.resetFired);
                if(i>150&&i<200){ // settled cruise: follow distance near the DEFAULT_3 target for 20 m/s.
                    assert(std::fabs(state.algorithm.distance-380)<40);
                    assert(std::fabs(o.fov-0.95990783f*0.78539819f)<1e-5f);
                }
            }
            assert(state.algorithm.airborneLatch==0&&state.algorithm.landed==1&&state.algorithm.phaseAActive==0);
        }
        assert(first.size()==second.size());
        for(size_t i=0;i<first.size();++i){for(int k=0;k<4;++k)assert(u(first[i].eye[k])==u(second[i].eye[k])&&u(first[i].lookAt[k])==u(second[i].lookAt[k]));assert(u(first[i].fov)==u(second[i].fov));}
        // The jump camera raised the eye relative to the head during the air phase.
        float maxRise=0;for(int i=200;i<270;++i)maxRise=std::max(maxRise,first[i-1].eye[2]-first[199].eye[2]);
        assert(maxRise>0);
    }
    // A wall departure requires the horizontal takeoff direction from rider+3C0.
    // Exercise the swing branch that ordinary ground/air fixtures never enter.
    for(Quad wall:{Quad{1,0,0,0},Quad{-1,0,0,0},Quad{0,1,0,0},Quad{0,-1,0,0}}){
        OriginalCameraInput in;in.headPosition={1000,2000,150,1};
        in.riderForward={0,1,0,0};in.velocity={0,900,0,0};in.tick=1000;
        OriginalCameraState state;state.begin(in);
        for(int tick=0;tick<60;++tick){++in.tick;originalChaseCameraStep(state,in);}
        in.motionMode=1;in.velocity={150,200,2000,0};in.previousContactNormal=wall;
        in.wallNormal=wall;in.launchValue=450;in.predictedAirTime=3;
        bool swung=false;auto missingDirection=state;float maximumDirectionEffect=0;
        for(int tick=0;tick<180;++tick){
            ++in.tick;in.trajectoryStatusActive=tick>=1;
            if(tick==120)in.motionMode=0;
            const auto out=originalChaseCameraStep(state,in);
            auto incomplete=in;incomplete.wallNormal={};
            const auto old=originalChaseCameraStep(missingDirection,incomplete);
            maximumDirectionEffect=std::max(maximumDirectionEffect,dist3(out.eye,old.eye));
            assert(finite(out.eye)&&finite(out.lookAt)&&std::isfinite(out.fov));
            swung|=state.algorithm.swingInitialised!=0;
        }
        assert(swung&&state.algorithm.wallLaunch==0&&maximumDirectionEffect>1.f);
    }
    // 6. Collision probe lift (0x15EE00): a floor just below the eye pushes it up.
    {
        SyntheticRider rider;OriginalCameraState state;state.begin(rider.frame(0));
        for(int i=1;i<120;++i)originalChaseCameraStep(state,rider.frame(i));
        OriginalCameraState noProbe=state,withProbe=state;SyntheticRider r2=rider;
        auto in=rider.frame(120);
        auto baseline=originalChaseCameraStep(noProbe,in);
        auto in2=in;in2.terrainProbe=[](const Quad& start,const Quad& end)->std::optional<OriginalCameraProbeHit>{
            assert(start[2]>end[2]); // probe runs from above (Pup) to below (Pdown)
            return OriginalCameraProbeHit{0.55f,{0,0,1,0}};
        };
        auto lifted=originalChaseCameraStep(withProbe,in2);
        assert(withProbe.compositor.lift>0&&noProbe.compositor.lift==0);
        assert(lifted.eye[2]>baseline.eye[2]);
        assert(dist3(lifted.eye,lifted.lookAt)-dist3(baseline.eye,baseline.lookAt)<1e-2f); // distance to the look-at is preserved
        assert(u(lifted.lookAt[0])==u(baseline.lookAt[0])&&u(lifted.lookAt[2])==u(baseline.lookAt[2])); // look-at untouched
        // Next frame with no hit: lift decays by 0.97.
        float before=withProbe.compositor.lift;originalChaseCameraStep(withProbe,rider.frame(121));
        assert(u(withProbe.compositor.lift)==u(original_camera::emul(before,std::bit_cast<float>(0x3F7851ECu))));
        (void)r2;
    }
    // 7. Shake: boosting above 60 km/h requests index 1..3 and displaces the look-at only.
    {
        SyntheticRider rider;OriginalCameraState state;state.begin(rider.frame(0));
        for(int i=1;i<60;++i)originalChaseCameraStep(state,rider.frame(i));
        OriginalRandomState rng{{1,2,3,4,5,6}};
        auto in=rider.frame(60);in.boostLevel=1.0f;in.visualRandom=&rng;
        auto out=originalChaseCameraStep(state,in);
        assert(state.compositor.shakeWasPending==1&&state.compositor.shake.active==1&&state.compositor.shakeIndex==3);
        assert(u(out.lookAt[0])!=u(out.algorithmLookAt[0])||u(out.lookAt[1])!=u(out.algorithmLookAt[1])||u(out.lookAt[2])!=u(out.algorithmLookAt[2]));
        assert(dist3(out.lookAt,out.algorithmLookAt)<20);
        for(int i=61;i<200;++i){auto next=rider.frame(i);next.boostLevel=0;originalChaseCameraStep(state,next);}
        assert(state.compositor.shake.active==0&&state.compositor.shake.fadeTimer==0);
    }
    // 8. Native conversion: source cm/Z-up -> metres/Y-up via (x,z,-y)*0.01.
    {
        OriginalCameraOutput o;o.eye={100,200,300,1};o.lookAt={-100,50,25,1};o.fov=0.75f;o.near=30;o.far=30000;
        auto n=nativeCameraView(o);
        auto near3=[](std::array<float,3> a,std::array<float,3> b){for(int i=0;i<3;++i)if(std::fabs(a[i]-b[i])>1e-5f)return false;return true;};
        assert(near3(n.eye,{1,3,-2})&&near3(n.lookAt,{-1,0.25f,-0.5f})&&n.fovRadians==0.75f&&std::fabs(n.nearMeters-0.3f)<1e-6f&&std::fabs(n.farMeters-300)<1e-3f);
    }
    assert(std::fegetround()==FE_TONEAREST);
    std::puts("Original DEFAULT_3 chase camera: constructor/set-target seeds, cubic spline, 5 PCSX2 savestate single-step oracles, 600-frame synthetic cruise/jump/landing, collision lift, shake and native conversion pass");
    return 0;
}
