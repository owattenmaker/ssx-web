#include "../engine/rail_score.hpp"
extern "C" void rail_score_rows(const uint32_t* input,uint32_t* output,unsigned count){
 const ssx::OriginalRailScoreProfile profile;
 for(unsigned n=0;n<count;n++,input+=15,output+=12){
  auto f=[&](unsigned i){return std::bit_cast<float>(input[i]);};auto integer=[&](unsigned i){return std::bit_cast<int32_t>(input[i]);};
  ssx::OriginalRailScoreState state{f(0),f(1),f(2),f(3),f(4),integer(5),integer(6),integer(7)};
  ssx::originalRailRotationScore(state,profile,integer(13),f(14));auto bonus=ssx::originalRailScoreTick(state,profile,{f(8),f(9),f(10)},f(11),f(12));
  const float values[]={state.accumulated14,state.inverted1C,state.distance24,state.spin34,state.multiplier1C4};for(unsigned i=0;i<5;i++)output[i]=std::bit_cast<uint32_t>(values[i]);
  output[5]=uint32_t(state.style20);output[6]=uint32_t(state.bonusPoints84);output[7]=uint32_t(state.threshold98);output[8]=bool(bonus);output[9]=bonus?uint32_t(bonus->points):0;output[10]=bonus?uint32_t(bonus->distanceCm):0;output[11]=bonus?std::bit_cast<uint32_t>(bonus->displaySeconds):0;
 }
}
