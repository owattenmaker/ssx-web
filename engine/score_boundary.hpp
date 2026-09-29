#pragma once
#include "grab_score.hpp"
#include "trick_identity.hpp"
#include "terrain_contact_math.hpp"
namespace ssx {
// Additional per-trick fields reset by117838. Persistent statistics and
// history outside that reset range remain caller-owned.
struct OriginalScoreBoundaryState {
 OriginalGrabScoreState score;
 OriginalTrickIdentityState identity;
 float multiplier18=1,inverted1C=0,airSeconds30=-1,activeSeconds6C=-1,timer78=-1;
 int32_t field74=0,field80=0,threshold88=0,threshold90=0,threshold94=0,threshold98=0;
};
inline void originalResetScoreBoundary(OriginalScoreBoundaryState& state){
 const float timeout=state.score.comboTimeoutA4,multiplier=state.score.multiplier1C4;
 state={};state.identity.time24=state.identity.time2C=-1;
 state.score.comboTimeoutA4=timeout;state.score.multiplier1C4=multiplier;
}
//119D40: commit the old state before reset, then seed the new mode. The
// callback implements11A228 and returns its award/result without being lost
// by117838. It receives the original five argument words, including t1=0.
template<class Commit>
auto originalScoreBoundary(OriginalScoreBoundaryState& state,int32_t stance,
 int32_t field04,int32_t style,int32_t flag,Commit&& commit){
 terrain_original::Rounding rounding;
 if(style!=0&&state.score.accumulated14>0)
  state.score.accumulated14=originalScalarAdd(state.score.accumulated14,std::bit_cast<float>(0x3e051eb8u));
 const int32_t active=state.identity.active70;
 auto result=commit(state,stance,field04,style,flag,0);
 originalResetScoreBoundary(state);
 state.identity.stance00=stance;state.identity.field04=field04;state.identity.style0C=style;state.identity.flag10=flag;
 if(style){state.identity.style20=style;state.identity.time24=0;}
 if(flag){state.identity.flag28=flag;state.identity.time2C=0;}
 if(active){state.identity.active70=active;state.score.activeUber5C=1;state.activeSeconds6C=0;}
 return result;
}
//119E38, called by114298 on charged and passive takeoff. Active rail Ubers
// retain their pending trick; ordinary rails commit with t1=1 before reset.
template<class Commit>
auto originalScoreTakeoffBoundary(OriginalScoreBoundaryState& state,int32_t stance,
 int32_t field04,Commit&& commit){
 terrain_original::Rounding rounding;
 const int32_t style=state.identity.style20,flag=state.identity.flag28;
 auto result=decltype(commit(state,0,0,0,0,1)){};
 if(state.identity.active70){state.identity.spin34=0;state.identity.time24=-1;state.identity.style20=0;}
 else {result=commit(state,0,0,0,0,1);originalResetScoreBoundary(state);}
 state.identity.style0C=style;state.identity.flag10=flag;
 state.identity.field04=field04;state.identity.stance00=stance;state.airSeconds30=0;
 return result;
}
}

namespace ssx {
//119938: begin active rail-Uber scoring without committing/resetting the old state.
inline float originalRailUberScoreBegin(OriginalScoreBoundaryState&s,int tier,int style){
 s.activeSeconds6C=0;s.identity.style20=style;s.identity.active70=tier;s.score.activeUber5C=1;s.identity.style0C=style;return 0;
}
//119958: increment Uber counters before commit, then preserve distance/airtime
//across117838 while seeding the new rail style.
template<class Commit> auto originalRailUberScoreEnd(OriginalScoreBoundaryState&s,int style,Commit&&commit){
 OriginalRounding rounding;float air=s.airSeconds30,distance=s.identity.time24;
 s.score.uberCount54=std::bit_cast<int32_t>(uint32_t(s.score.uberCount54)+1u);
 s.field74=std::bit_cast<int32_t>(uint32_t(s.field74)+1u);
 auto result=commit(s,0,0,0,0,0);originalResetScoreBoundary(s);
 s.airSeconds30=air;s.identity.style20=style;s.identity.time24=distance;s.identity.style0C=style;return result;
}
}

namespace ssx {
//117E58..117E84. The caller supplies117C28's already-scaled simulation dt.
inline void originalRailUberScoreTick(OriginalScoreBoundaryState&s,float dt){
 OriginalRounding rounding;
 if(!(s.activeSeconds6C>=0))return;
 s.activeSeconds6C=originalScalarAdd(s.activeSeconds6C,dt);
 s.score.accumulated14=originalScalarAdd(s.score.accumulated14,terrain_original::mul(dt,.04999999701976776f));
}
}
