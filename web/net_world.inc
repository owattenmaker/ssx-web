// M7-2 (docs/core-migration.md "M7"): an online race's shared-world replication in the core (pv coreNetWorld; port-only netcode, not
// a PS2 rule), as web/net/mp-game.js beginTick / endTick and web/net/pickup-arbiter.js did it. The host moves the WORLD packets'
// bytes (web/net/rider-packet.js encodeWorld: word 0 = kind 4 | tick << 8, the trigger and event-word counts, the trigger keys, the
// event words); the core keeps the triggers already shared (sent or applied), arbitrates the boost pickups two racers took inside
// the 60-tick debounce (the earlier tick keeps it, on the same tick the lower slot; the loser gives it back: pickup_revoke) and
// applies the other racers' events (world_event_apply).
RIDER_LOCAL_LAZY static std::set<uint32_t> netSharedTriggers;
RIDER_LOCAL_LAZY static std::map<uint32_t,int32_t> netPickupTakes; // resource -> the tick of this racer's take
RIDER_LOCAL static int32_t netSlot=0;
// [takes, conflicts, revoked, kept, events applied, events failed, packets made]
RIDER_LOCAL static std::array<uint32_t,7> netWorldStats{};
static constexpr int32_t kNetPickupDebounce=60;
extern "C" {
// pv coreNetWorld: a race starts (web/net/mp-game.js at its start): this racer's slot; the triggers the core already holds count as shared
EMSCRIPTEN_KEEPALIVE void net_world_begin(int slot){
 netSlot=slot;
 netSharedTriggers.clear();
 netPickupTakes.clear();
 netWorldStats.fill(0);
 const uint32_t* t=world_triggers();
 for(uint32_t k=0;k<t[0];k++)netSharedTriggers.insert(t[1+k]);
}
// pv coreNetWorld: one WORLD packet of racer `from` (bytes without the relay's slot byte), at this tick's start
EMSCRIPTEN_KEEPALIVE int net_world_apply(const uint8_t* bytes,int length,int from){
 if(length<8||length%4)return 0;
 uint32_t head=0;
 uint16_t triggers=0;
 uint16_t words=0;
 std::memcpy(&head,bytes,4);
 std::memcpy(&triggers,bytes+4,2);
 std::memcpy(&words,bytes+6,2);
 if((head&0xffu)!=4u||length!=8+(int(triggers)+int(words))*4)return 0;
 const int32_t tick=int32_t(head>>8);
 std::vector<uint32_t> all(size_t(triggers)+words);
 if(!all.empty())std::memcpy(all.data(),bytes+8,all.size()*4);
 for(uint32_t k=0;k<triggers;k++){
  if(netSharedTriggers.insert(all[k]).second)world_trigger_apply(all[k]);
 }
 const uint32_t* events=all.data()+triggers;
 for(uint32_t at=0;at+1<words;){
  const uint32_t length=2+events[at+1];
  if(at+length>words)break;
  // web/net/pickup-arbiter.js remote(): a take of the same pickup inside the debounce of this racer's
  if(events[at]==2){
   const uint32_t resource=events[at+2];
   auto mine=netPickupTakes.find(resource);
   if(mine!=netPickupTakes.end()&&std::abs(mine->second-tick)<kNetPickupDebounce){
    netWorldStats[1]++;
    const bool theirs=tick<mine->second||(tick==mine->second&&from<netSlot);
    if(theirs){
     netPickupTakes.erase(mine);
     netWorldStats[2]++;
     pickup_revoke(resource);
    }else netWorldStats[3]++;
   }
  }
  try{
   world_event_apply(events+at);
   netWorldStats[4]++;
  }catch(...){
   netWorldStats[5]++;
  }
  at+=length;
 }
 return 1;
}
// pv coreNetWorld: this racer's WORLD packet for the tick just run (its new triggers, its logged events; the takes recorded and the
// old ones forgotten), [byte length, bytes...] (length 0: nothing to send)
EMSCRIPTEN_KEEPALIVE uint32_t* net_world_packet(int tick){
 RIDER_LOCAL static std::vector<uint32_t> out;
 std::vector<uint32_t> triggers;
 const uint32_t* t=world_triggers();
 for(uint32_t k=0;k<t[0];k++){
  if(netSharedTriggers.insert(t[1+k]).second)triggers.push_back(t[1+k]);
 }
 const uint32_t* e=world_events();
 const std::vector<uint32_t> events(e+1,e+1+e[0]);
 for(size_t at=0;at+1<events.size();at+=2+events[at+1]){
  if(events[at]==2){
   netPickupTakes[events[at+2]]=tick;
   netWorldStats[0]++;
  }
 }
 for(auto it=netPickupTakes.begin();it!=netPickupTakes.end();){
  if(tick-it->second>=kNetPickupDebounce)it=netPickupTakes.erase(it);
  else ++it;
 }
 out.assign(1,0u);
 if(triggers.empty()&&events.empty())return out.data();
 const uint32_t head=4u|((uint32_t(tick)&0xffffffu)<<8);
 out.push_back(head);
 out.push_back(uint32_t(triggers.size())|(uint32_t(events.size())<<16));
 out.insert(out.end(),triggers.begin(),triggers.end());
 out.insert(out.end(),events.begin(),events.end());
 out[0]=uint32_t(out.size()-1)*4;
 netWorldStats[6]++;
 return out.data();
}
// pv coreNetWorld: [takes, conflicts, revoked, kept, events applied, events failed, packets made]
EMSCRIPTEN_KEEPALIVE uint32_t* net_world_stats(){return netWorldStats.data();}
}
