// One shared world for the six riders (web/ai-racers.js). The original updates every rider inside one
// world: a crashbag one rider kicks, a boost pickup one rider takes, a log teeter one rider lands on
// and a section MultiSpline the lead activates are the same objects for everyone. The browser keeps
// one world copy per rider core; every change a rider makes to a shared entity is logged here and
// replayed at once, in the same pass order, in every other core (world_event_apply), so all copies
// stay identical. Entity updates themselves (race_begin: 35E850 rollers, 342358 teeters, splines,
// MultiSplines) are deterministic and run in every core. Replays never draw the shared RNG and never
// touch the replaying core's rider (no boost award, no score, no reaction).
// Events (uint32 words; floats bit-cast): [kind, word count, payload...]
//   1 scripted instance contact (crashbag builtin0/15 create, RollerModifier re-kick 361CD8):
//     instance, node, record point4 direction4 normal4 closing speed
//   2 boost pickup taken (121818 handler 2 -> debounce): resource
//   3 log-teeter attach force (106848 -> 342538): teeter index, rail node, point xyz, force xyzw
//   4 rider trigger contact (105398 selected contact -> set-piece trigger log, renderer): resource
//   5 section MultiSpline activation (human progress): authored resource
//   7 section MultiSpline release (leave scan of the section activation, web/section_gameplay.inc): authored resource
//   6 stage trigger contact (121818 -> 355770 guard -> 30A060 slot 2, web/stage_script_gameplay.inc): resource, human (builtin 39: the player object is human)
//   8 painter reset 0x2C03E8 (a rider's FX reset 0x111890 at its reset placement / teleport: every world painter wrapper, web/weather.inc)
//  10 Spline piece released (pv peakSplines; logged only by a core with shared_world_nodes on, the streamed world's human): its section
//     leave destroyed the entity (web/set_piece_gameplay.inc section_stop_piece): resource, owner key (owner * 8 + slot, as world_triggers)
//   9 world node state (pv eventInWorldAi, docs/ctm-events-in-world.md stage 4; logged only by a core with shared_world_nodes on, the
//     streamed world's human): resource, node type (0 none, 6 DeadNode, 16 Hide, 19 RestoreNode), collision-world runtime flags
//     known + flags, stage flags known + flags. In the streamed world the stage and section programs of the resident locations run in the
//     human's context only (the computer riders' contexts hold the event package, whose own seeds are Single Event's); every change the
//     programs make to an instance's state (0x2FC420 DeadNode / RestoreNode, 0x2FFB50 Hide / Unhide, the flag builtins: stage_set_flags)
//     reaches the other contexts, as in the PS2's one world. Logged also while the human replays another rider's contact (its programs'
//     answer to that contact). world_node_states / world_node_states_apply copy the whole state of a list of instances the same way.
RIDER_LOCAL bool sharedWorldReplay=false;
RIDER_LOCAL static std::vector<uint32_t> sharedWorldLog;
void shared_world_log(std::initializer_list<uint32_t> words){
 if(sharedWorldReplay||sharedWorldLog.size()>65536)return;
 sharedWorldLog.push_back(*words.begin());sharedWorldLog.push_back(uint32_t(words.size()-1));
 for(auto it=words.begin()+1;it!=words.end();++it)sharedWorldLog.push_back(*it);
}
static uint32_t shared_float(float x){return std::bit_cast<uint32_t>(x);}
RIDER_LOCAL static bool sharedWorldNodes=false;RIDER_LOCAL static uint32_t sharedWorldNodesKept=0; // kind 9 records this context kept its own state for (below)
static void shared_world_node_words(uint32_t resource,uint32_t* w){ // kind 9's payload (6 words) from this context's state
 w[0]=resource;w[1]=uint32_t(stage_node_state(resource));
 const auto* i=set_piece_instance_at(resource);w[2]=i&&i->eventRuntimeFlags?1u:0u;w[3]=w[2]?*i->eventRuntimeFlags:0u;
 const auto it=stageFlags.find(resource);w[4]=it!=stageFlags.end()?1u:0u;w[5]=w[4]?it->second:0u;
}
void shared_world_spline_released(uint32_t resource,uint32_t key){if(sharedWorldNodes&&setPieceStreamed)shared_world_log({10u,resource,key});}
void shared_world_node_changed(uint32_t resource){ // web/stage_world.inc stage_set_flags (every node and flag change ends there)
 if(!sharedWorldNodes||sharedWorldLog.size()>65536)return;
 uint32_t w[6];shared_world_node_words(resource,w);sharedWorldLog.insert(sharedWorldLog.end(),{9u,6u,w[0],w[1],w[2],w[3],w[4],w[5]});
}
static void shared_world_node_apply(const uint32_t* w){
 const uint32_t resource=w[0];const int type=int(w[1]);auto* instance=set_piece_instance_at(resource);if(!instance)return; // an instance of another location
 // A live entity of the human's world that this context does not hold (the CTM collectibles' MagnetModifier pickups, built by the
 // section programs, which run in the human's context only): the flags route it to an entity this context cannot answer. The context
 // keeps its own state (the event load's: a DeadNode for those pickups). On the PS2 the magnet answers only the instance-contact query
 // (0x357538, filter 1) and its gate 0x357660 refuses a computer rider (only a human becomes its target), so a computer rider's contact
 // has no effect; unconfirmed: whether such a contact can win 105398's contact selection over another one in the same tick.
 if(w[2]&&originalInstanceBodyRoute(w[3],true)==OriginalInstanceBodyRoute::Entity&&!instance->entity&&!instance->answerBox&&!stageMagnets.count(resource)){++sharedWorldNodesKept;return;}
 if(type){stageNodeState[resource]=type;if(auto* st=stage_instance(resource)){st->entity=StageNoopEntity;st->dead=type==6;st->restored=type==19;}}
 else if(stageNodeState.erase(resource)){if(auto* st=stage_instance(resource);st&&st->entity==StageNoopEntity){st->entity=StageNoEntity;st->dead=false;st->restored=false;}}
 if(auto* i=set_piece_instance_at(resource))i->eventRuntimeFlags=w[2]?std::optional<uint32_t>(w[3]):std::nullopt;
 if(w[4]){stage_note_countdown_flags(resource);stageFlags[resource]=w[5];if(auto* st=stage_instance(resource)){st->flags=w[5];st->flagsKnown=true;}}
 else{stageFlags.erase(resource);if(auto* st=stage_instance(resource))st->flagsKnown=false;}
}
void shared_world_log_contact(const OriginalInstanceContactPacket& p,const OriginalInstanceContactRecord& r){
 shared_world_log({1u,p.instance,uint32_t(p.node),shared_float(r.point[0]),shared_float(r.point[1]),shared_float(r.point[2]),shared_float(r.point[3]),
  shared_float(r.direction[0]),shared_float(r.direction[1]),shared_float(r.direction[2]),shared_float(r.direction[3]),
  shared_float(r.normal[0]),shared_float(r.normal[1]),shared_float(r.normal[2]),shared_float(r.normal[3]),shared_float(r.closingSpeed)});
}
void browser_teeter_apply_force(unsigned teeter,unsigned node,const std::array<float,3>& point,const ssx::RollerQuad& force); // web/rail_bridge.cpp
extern "C" {
// Drains this core's log: [word count, events...].
EMSCRIPTEN_KEEPALIVE uint32_t* world_events(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,uint32_t(sharedWorldLog.size()));out.insert(out.end(),sharedWorldLog.begin(),sharedWorldLog.end());sharedWorldLog.clear();return out.data();
}
// Replays one event logged by another rider's core.
EMSCRIPTEN_KEEPALIVE void world_event_apply(const uint32_t* e){
 const BrowserRandom savedRandom=rng;sharedWorldReplay=true;
 auto f=[&](unsigned k){return std::bit_cast<float>(e[2+k]);};
 try{
  switch(e[0]){
   case 1:{OriginalInstanceContactPacket p;p.instance=e[2];p.node=e[3];OriginalInstanceContactRecord r;
    for(unsigned k=0;k<4;k++){r.point[k]=f(2+k);r.direction[k]=f(6+k);r.normal[k]=f(10+k);}r.closingSpeed=f(14);
    browser_scripted_contact(p,r);break;}
   case 2:browser_pickup_taken(e[2]);break;
   case 3:browser_teeter_apply_force(e[2],e[3],{f(2),f(3),f(4)},{f(5),f(6),f(7),f(8)});break;
   case 4:browser_set_piece_contact_log(e[2]);break;
   case 5:browser_activate_section_lift(e[2]);break;
   case 6:browser_stage_replay(e[2],e[3]!=0);break;
   case 7:browser_release_section_lift(e[2]);break;
   case 8:weather_painters_reset();break;
   case 9:shared_world_node_apply(e+2);break;
   case 10:browser_spline_piece_released(e[2]);break;
   default:throw std::runtime_error("Unknown shared world event");
  }
 }catch(...){sharedWorldReplay=false;rng=savedRandom;throw;}
 sharedWorldReplay=false;rng=savedRandom;
}
// pv eventInWorldAi: this core logs its node states (kind 9); the streamed world's human only (web/ai-race.js).
EMSCRIPTEN_KEEPALIVE void shared_world_nodes(int on){sharedWorldNodes=on!=0;}
// The node states of a list of instances in this context: [count, then kind 9's 6 words per resource].
EMSCRIPTEN_KEEPALIVE uint32_t* world_node_states(const uint32_t* resources,int count){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,0u);
 for(int k=0;k<count;++k){uint32_t w[6];shared_world_node_words(resources[k],w);out.insert(out.end(),w,w+6);++out[0];}
 return out.data();
}
// Another context's world_node_states list into this one (the instances this context has; no draw, no rider effect). Returns the
// records applied (not those of another location, nor the live entities kept above).
EMSCRIPTEN_KEEPALIVE int world_node_states_apply(const uint32_t* list){
 int n=0;const uint32_t kept=sharedWorldNodesKept;for(uint32_t k=0;k<list[0];++k){const uint32_t* w=list+1+6*k;if(set_piece_instance_at(w[0])){shared_world_node_apply(w);++n;}}
 return n-int(sharedWorldNodesKept-kept);
}
}
