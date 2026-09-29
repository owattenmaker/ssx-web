#include "../web/animation_graph.hpp"
#include <cassert>
#include <iostream>
int main(){
 ssx::BrowserAnimationGraph graph;graph.rig=std::make_shared<ssx::BrowserRig>();
 graph.rig->stateDefinitions[100].animationClass=18;graph.requestedSemantics[2]=100;
 graph.requestedSemantics[1]=101;
 for(unsigned channel:{1,2,2}){ssx::OriginalAnimationSequence s;s.channel=channel;s.flags=(uint64_t(1)<<63)|5;s.weight=.75f;s.targetWeight=1;s.completionEnabled=true;graph.sequences.push_back(s);}
 // A held grab freezes the primary clip; the outgoing pose keeps advancing.
 graph.sequences[0].rate=.5f;graph.sequences[1].rate=1;graph.sequences[2].rate=1.25f;
 for(auto&s:graph.sequences){ssx::OriginalAnimationSlot slot;slot.time=.25f;slot.duration=2;slot.rate=1;slot.enabled=true;s.slots.push_back(slot);}
 graph.setRate(2,0);
 assert(graph.sequences[0].rate==.5f&&graph.sequences[1].rate==0&&graph.sequences[2].rate==1.25f);
 for(auto&s:graph.sequences)ssx::originalAnimationSlotStep(s.slots[0],s.rate,1.f/60);
 assert(graph.sequences[1].slots[0].time==.25f&&graph.sequences[2].slots[0].time>.25f);
 graph.setRate(2,-1);assert(graph.sequences[1].rate==-1&&graph.sequences[2].rate==1.25f);
 graph.setRate(2,1.1f);assert(graph.sequences[1].rate==1.1f&&graph.sequences[2].rate==1.25f);
 graph.setRate(5,0);assert(graph.sequences[0].rate==.5f);
 graph.fade(2,.1f);
 assert(graph.currentClass(2)==0&&graph.requestedSemantics[2]==438);
 assert(graph.requestedSemantics[1]==101&&graph.sequences[0].completionEnabled);
 assert(graph.sequences.size()==3); // Fading poses must not disappear immediately.
 for(unsigned i:{1,2}){auto&s=graph.sequences[i];assert(!s.completionEnabled&&s.flags==5&&s.targetWeight==0&&s.fadeRemaining==.1f&&s.weight==.75f&&s.stopWhenFaded);}
 assert(graph.flags(2)==5); // Ordinary grab markers belong to the fading playback.
 graph.fade(2,.3f);assert(graph.sequences[1].fadeRemaining==.1f);
 graph.fade(2,.05f);assert(graph.sequences[1].fadeRemaining==.05f);
 graph.fade(5,.1f);assert(graph.requestedSemantics[5]==438);
 // Completion callbacks use a fixed cohort, even if flags and vector storage change.
 std::vector<ssx::OriginalAnimationSequence> cohort(3);
 cohort[0].channel=2;cohort[0].semantic=10;cohort[0].flags=uint64_t(1)<<63;
 cohort[1].channel=1;cohort[1].semantic=20;cohort[1].flags=uint64_t(1)<<63;
 cohort[2].channel=2;cohort[2].semantic=30;cohort[2].flags=uint64_t(1)<<63;
 std::vector<int> order;
 ssx::originalAnimationCompletionBatch(cohort,[&](size_t index){
  order.push_back(cohort[index].semantic);
  if(order.size()==1){cohort[2].flags=0;ssx::OriginalAnimationSequence incoming;incoming.channel=0;incoming.semantic=40;incoming.flags=uint64_t(1)<<63;cohort.insert(cohort.begin(),incoming);}
 });
 assert((order==std::vector<int>{20,10,30}));
 std::cout<<"Browser channel rate targets only the primary sequence; fades clear identity/completion while retaining playback markers and crossfade weights.\n";
}
