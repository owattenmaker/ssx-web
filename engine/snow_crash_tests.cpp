// Ad hoc: clang++ -std=c++20 -frounding-math -ffp-contract=off -UNDEBUG -Iengine engine/snow_crash_tests.cpp \
//   engine/snow_emission.cpp engine/snow_context.cpp engine/rider_pose_motion.cpp engine/animation_motion.cpp \
//   engine/orientation_motion.cpp engine/ground_motion.cpp -o <scratch>/snowcrash/snow_crash_tests
#include "snow_crash.hpp"
#include "snow_context.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <map>
using namespace ssx;
static OriginalSnowBodyContext context(){
    OriginalSnowBodyContext c;for(unsigned i=0;i<30;++i)c.boneOriginsCm[i]={float(i)*10,float(i)*-3,float(i)+100};
    c.primaryBoneOriginCm={1,2,3};c.velocityCmps={1000,-500,250};c.speedCmps=1145.6438f;c.colour={1.2f,1.4f,1.6f,1};c.motionMode=2;return c;
}
int main(){
    OriginalSnowTrailProfile profile;profile.velocityScale=.800000011920929f;
    {   // Inactive while no impact buildup: request carries the primary bone origin and no velocity/colour.
        OriginalSnowBodyState state;OriginalSnowImpactState impact;auto c=context();auto out=originalSnowBodyEmission(profile,state,impact,c);
        assert(!out.active&&out.emitter==9&&out.positionCm==c.primaryBoneOriginCm&&!out.velocityCmps&&!out.colour&&out.stepSeconds==.01666666753590107f&&state.cursor==0);
    }
    {   // Speed gate is strict at 83.33333587646484 cm/s and applies even with buildup.
        OriginalSnowBodyState state;OriginalSnowImpactState impact;impact.buildup=2;auto c=context();c.speedCmps=83.33333587646484f;
        auto out=originalSnowBodyEmission(profile,state,impact,c);assert(!out.active&&impact.buildup==2&&state.cursor==0);
        c.speedCmps=std::nextafter(83.33333587646484f,1e9f);out=originalSnowBodyEmission(profile,state,impact,c);assert(out.active&&state.cursor==1);
    }
    {   // Active request: cycling bone origin, velocity*VelScale, environment colour with alpha=min(1.5*buildup,1).
        OriginalSnowBodyState state;state.cursor=7;OriginalSnowImpactState impact;impact.buildup=.5f;auto c=context();
        auto out=originalSnowBodyEmission(profile,state,impact,c);
        assert(out.active&&out.positionCm==c.boneOriginsCm[7]&&state.cursor==8);
        assert(out.velocityCmps&&(*out.velocityCmps)[0]==1000*.800000011920929f&&(*out.velocityCmps)[1]==-500*.800000011920929f&&(*out.velocityCmps)[2]==250*.800000011920929f);
        assert(out.colour&&(*out.colour)[0]==1.2f&&(*out.colour)[1]==1.4f&&(*out.colour)[2]==1.6f&&(*out.colour)[3]==.75f);
        impact.buildup=2;out=originalSnowBodyEmission(profile,state,impact,c);assert((*out.colour)[3]==1&&state.cursor==9); // saturates
    }
    {   // Cursor wraps after the thirtieth bone.
        OriginalSnowBodyState state;state.cursor=29;OriginalSnowImpactState impact;impact.buildup=1;auto c=context();
        auto out=originalSnowBodyEmission(profile,state,impact,c);assert(out.positionCm==c.boneOriginsCm[29]&&state.cursor==0);
        state.cursor=30;bool threw=false;try{originalSnowBodyEmission(profile,state,impact,c);}catch(const std::runtime_error&){threw=true;}assert(threw);
    }
    {   // Buildup holds during motion 2 and decays by 1/60 per active tick otherwise, floored at 0.
        OriginalSnowBodyState state;OriginalSnowImpactState impact;impact.buildup=1;auto c=context();c.motionMode=2;
        originalSnowBodyEmission(profile,state,impact,c);assert(impact.buildup==1);
        c.motionMode=0;originalSnowBodyEmission(profile,state,impact,c);assert(impact.buildup==originalScalarSubtract(1.f,.01666666753590107f));
        impact.buildup=.01f;originalSnowBodyEmission(profile,state,impact,c);assert(impact.buildup==0);
        auto out=originalSnowBodyEmission(profile,state,impact,c);assert(!out.active); // and no decay while inactive
        impact.buildup=1;c.speedCmps=10;originalSnowBodyEmission(profile,state,impact,c);assert(impact.buildup==1);
    }
    {   // Bone table: thirty 2DF4D0 names resolve in the shipped rig order; a missing name throws.
        std::map<std::string_view,int> rig{{"hips",0},{"lowerspine",1},{"middlespine",2},{"upperspine",3},{"neck",4},{"head",5},{"clavicleleft",6},{"bicepleft",7},{"biceptwistleft",8},{"forearmleft",9},{"handleft",10},{"clavicleright",11},{"bicepright",12},{"biceptwistright",13},{"forearmright",14},{"handright",15},{"thighleft",16},{"shinleft",17},{"footleft",18},{"thighright",19},{"shinright",20},{"footright",21}};
        auto lookup=[&](std::string_view n){auto it=rig.find(n);return it==rig.end()?-1:it->second;};
        auto table=originalSnowBodyBoneTable(lookup);
        OriginalSnowBodyBoneTable expected{17,18,20,21,16,19,0,1,2,3,4,6,15,7,8,17,18,20,21,16,19,0,1,2,9,10,11,12,13,14};assert(table==expected);
        rig.erase("neck");bool threw=false;try{originalSnowBodyBoneTable(lookup);}catch(const std::runtime_error&){threw=true;}assert(threw);
    }
    {   // Crash impact trigger is 2E23E0 with kind=false: retained strength doubles, position/normal/surface stored.
        OriginalSnowContextState state;OriginalSnowRiderInput rider;rider.motionMode=2;
        bool taken=originalSnowCrashImpactTrigger(state,{100,200,300},{0,0,1},-900,4,rider);
        assert(taken&&state.impact.strength==1800&&!state.impact.kind&&state.impactSurface==4&&(state.impact.positionCm==SnowVector{100,200,300}));
        assert(!originalSnowCrashImpactTrigger(state,{100,200,300},{0,0,1},500,4,rider)); // weaker and within 180 cm
        assert(originalSnowCrashImpactTrigger(state,{100,200,300},{0,0,1},2000,4,rider)==false); // stronger but within 180 cm, not the special tracking case
        assert(originalSnowCrashImpactTrigger(state,{100,200,1000},{0,0,1},2000,4,rider)&&state.impact.strength==4000);
    }
    puts("snow_crash_tests: all assertions passed");return 0;
}
