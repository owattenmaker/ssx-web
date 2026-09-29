//Authored Snow Jam boost pickups. Geometry and ranking use original collision rules.
#include "../engine/pickup_contact_selection.hpp"
#include "../engine/pickup_timers.hpp"
#include "../engine/pickup_instance_flags.hpp"
extern "C" void award_boost_pickup(int,float);
struct BrowserPickup {uint32_t resource=0,flags=0,initialFlags=0;int effect=0,phase=0,cooldown=0,debounce=0,duration=60;float amount=0;};
RIDER_LOCAL static std::vector<BrowserPickup> pickupItems;RIDER_LOCAL static uint32_t pickupTick=0xffffffffu,pickupCount=0,pickupLast=0;
void collect_pickup_contact(const WorldBodyQuery&,uint32_t resource);
RIDER_LOCAL static bool pickupQueryComplete=true;RIDER_LOCAL static unsigned pickupUnsupported=0,pickupContacts=0;
void browser_reset_rollers();void browser_reset_stage_scripts();
void browser_reset_pickups(){browser_reset_rollers();browser_reset_stage_scripts();for(auto&p:pickupItems){p.phase=p.cooldown=p.debounce=0;p.flags=p.initialFlags;}pickupTick=0xffffffffu;pickupCount=pickupLast=0;pickupQueryComplete=true;pickupUnsupported=pickupContacts=0;}
static void setup_pickups(const json&config){
 pickupItems.clear();const auto&data=config.at("original_pickups");if(data.at("version")!=1)throw std::runtime_error("Pickup package version");
 for(const auto&x:data.at("items")){BrowserPickup p;p.resource=x.at("resource");p.effect=x.at("effect_type");p.amount=x.at("amount");p.duration=x.at("debounce_ticks");p.initialFlags=x.at("initial_flags");if((p.effect!=1&&p.effect!=2)||p.amount!=5||p.duration!=60||x.at("completion_mode")!=3)throw std::runtime_error("Unsupported authored pickup program");pickupItems.push_back(p);}
 browser_reset_pickups();
}
void advance_pickup_timers(uint32_t tick){
 if(pickupTick==tick)return;pickupTick=tick;
 for(auto&p:pickupItems)if(p.phase==1&&ssx::originalPickupTimersTick(p.cooldown,p.debounce)){p.flags=ssx::originalPickupRestorePendingFlags(ssx::originalPickupRestoredFlags(p.flags));p.phase=2;}
}
void dispatch_pickup_contacts(const BodyCollisionVolume& body){
 if(!browserBodies||browserResetActive||physicsState.controlState==9)return;
 auto query=browserBodies->query(body,physicsState.normal,nullptr,false,1);
 pickupQueryComplete=query.complete();pickupUnsupported=query.unsupportedInstances.size();pickupContacts=query.instanceContacts.size();
 if(!pickupQueryComplete)return; //Never award from an incomplete contact list.
 std::vector<ssx::OriginalPickupContactCandidate> candidates;for(const auto&hit:query.instanceContacts)candidates.push_back({hit.instance,hit.pointCm,hit.normal,hit.penetrationCm,hit.priority});
 auto selection=ssx::originalPickupContactSelection(candidates,body.broadCenterCm,-1);
 if(selection.selected<0)return;
 collect_pickup_contact(query,candidates[selection.selected].instance);
}
//121818 for the contact 105398 copied to rider+9E0/A30 (node flags bit1).
void shared_world_log(std::initializer_list<uint32_t>); // web/shared_world.inc
// The pickup instance leaves play for every rider (debounce); a replay from another rider's core takes it without the award.
void browser_pickup_taken(uint32_t resource){
 for(auto&p:pickupItems)if(p.resource==resource&&p.phase==0){
  p.phase=1;p.debounce=p.duration;p.cooldown=0;
  p.flags=ssx::originalPickupDebounceFlags(p.flags);++pickupCount;pickupLast=resource;break;
 }
}
//121818 for the contact 105398 copied to rider+9E0/A30 (node flags bit1).
void collect_pickup_contact(const WorldBodyQuery&,uint32_t resource){
 for(auto&p:pickupItems)if(p.resource==resource&&p.phase==0){
  //Verified start instances have no entity, so121818 dispatches handler2
  //directly. Both Debounce and type19 have empty contact callbacks afterward.
  //The award itself (program 28/30 builtin27 -> 10F1C0) runs in 121818 (web/stage_script_gameplay.inc).
  shared_world_log({2u,resource});browser_pickup_taken(resource);break;
 }
}
RIDER_LOCAL extern uint32_t instanceContactTickShared;
//Pickups are collected only through the 105398 store step (-> 121818, web/stage_script_gameplay.inc):
//frames without that phase (reset control 9, rail riding) select no contact in the original.
static void update_pickups(){advance_pickup_timers(motionTick);}
//Explicit extra contact pass for exit-phase integration/QA; it never advances time.
extern "C" EMSCRIPTEN_KEEPALIVE void pickup_contact_pass(){if(browserBodyVolume)dispatch_pickup_contacts(*browserBodyVolume);}
extern "C" EMSCRIPTEN_KEEPALIVE int pickup_visible(uint32_t resource){for(const auto&p:pickupItems)if(p.resource==resource)return p.phase==0;return 1;}
extern "C" EMSCRIPTEN_KEEPALIVE float* pickup_info(){RIDER_LOCAL static std::vector<float> out;out={float(pickupItems.size()),float(pickupCount),float(pickupLast),float(pickupQueryComplete),float(pickupUnsupported),float(pickupContacts)};for(const auto&p:pickupItems){out.push_back(float(p.resource));out.push_back(float(p.phase));out.push_back(float(p.debounce));out.push_back(float(p.effect));}return out.data();}
// Online races (web/net/mp-game.js pickup arbitration): in the original one world the first rider to touch a boost
// pickup takes it (slot order within a tick) and it leaves play for the others. Across the network two racers can
// both take it inside the link latency; the one with the later take (tick, then slot) gives it back: the 10F1C0
// award its take made (a counter increment, core.cpp award_boost_pickup) is taken back, clamped at zero.
extern "C" EMSCRIPTEN_KEEPALIVE int pickup_revoke(uint32_t resource){
 for(const auto&p:pickupItems)if(p.resource==resource){
  OriginalRounding round;
  if(p.effect==1){boostState.window=std::max(0.f,originalScalarAdd(boostState.window,-p.amount));physicsState.boostWindow=boostState.window;}
  else if(p.effect==2)boostState.modifier=std::max(0.f,originalScalarAdd(boostState.modifier,-p.amount));
  return 1;
 }
 return 0;
}
