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
RIDER_LOCAL bool sharedWorldReplay=false;
RIDER_LOCAL static std::vector<uint32_t> sharedWorldLog;
void shared_world_log(std::initializer_list<uint32_t> words){
 if(sharedWorldReplay||sharedWorldLog.size()>65536)return;
 sharedWorldLog.push_back(*words.begin());sharedWorldLog.push_back(uint32_t(words.size()-1));
 for(auto it=words.begin()+1;it!=words.end();++it)sharedWorldLog.push_back(*it);
}
static uint32_t shared_float(float x){return std::bit_cast<uint32_t>(x);}
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
   default:throw std::runtime_error("Unknown shared world event");
  }
 }catch(...){sharedWorldReplay=false;rng=savedRandom;throw;}
 sharedWorldReplay=false;rng=savedRandom;
}
}
