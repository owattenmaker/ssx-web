// M7 step 1 (docs/core-migration.md "M7"): the online contact random stream (web/net/pair-net.js contactStream, pv coreContactStream).
// Port-only netcode, not a PS2 rule: an online race's contact reaction draws its randoms from six words of the shared generator's
// layout (0x317810) keyed by the race seed, the contact tick, the pair and the reacting rider (a 32-bit murmur finaliser chain), so
// the attacker's client predicts the victim's reaction with the victim's own draws. One key tick per dispatch (contact_stream_begin:
// the local rider's 0x107888 pass, or one received attack's response); each (pair, target) stream is made once in it and advances.
RIDER_LOCAL static uint32_t contactSeed=0;
RIDER_LOCAL static uint32_t contactTick=0;
RIDER_LOCAL static std::array<std::array<uint32_t,6>,216> contactStreams{};
RIDER_LOCAL static std::array<bool,216> contactStreamMade{};
static std::array<uint32_t,6> contact_stream_words(uint32_t seed,uint32_t tick,int32_t a,int32_t b,int32_t target){
 uint32_t h=seed^0x9e3779b9u;
 auto mix=[&h](uint32_t x){
  h=(h^x)*0x85ebca6bu;
  h^=h>>13;
  h=h*0xc2b2ae35u;
  h^=h>>16;
  return h;
 };
 mix(tick);
 mix(uint32_t(std::min(a,b)));
 mix(uint32_t(std::max(a,b)));
 mix(uint32_t(target));
 std::array<uint32_t,6> w{};
 for(uint32_t k=0;k<6;k++)w[k]=mix(0x632be5abu+k);
 return w;
}
static std::array<uint32_t,6>& contact_stream_of(int32_t a,int32_t b,int32_t target){
 const int32_t lo=std::clamp(std::min(a,b),0,5);
 const int32_t hi=std::clamp(std::max(a,b),0,5);
 const size_t key=size_t((lo*6+hi)*6+std::clamp(target,0,5));
 if(!contactStreamMade[key]){
  contactStreams[key]=contact_stream_words(contactSeed,contactTick,a,b,target);
  contactStreamMade[key]=true;
 }
 return contactStreams[key];
}
extern "C" {
// pv coreContactStream: a new dispatch's key tick (web/net/pair-net.js: the local pass at the tick, or a received attack's tick); the
// streams of the last one are dropped
EMSCRIPTEN_KEEPALIVE void contact_stream_begin(uint32_t seed,uint32_t tick){
 contactSeed=seed;
 contactTick=tick;
 contactStreamMade.fill(false);
}
// pv coreContactStream: the local rider's reaction (pair_react) with the game RNG's words swapped for the (slot, other, slot) stream,
// which keeps the draws it took; the game RNG is restored after
EMSCRIPTEN_KEEPALIVE void contact_stream_react(int slot,int other,int kind,int animation,int attack,const float* e){
 auto& stream=contact_stream_of(slot,other,slot);
 const auto saved=rng.words;
 rng.words=stream;
 try{
  pair_react(kind,animation,attack,e);
 }catch(...){
  stream=rng.words;
  rng.words=saved;
  throw;
 }
 stream=rng.words;
 rng.words=saved;
}
// pv coreContactStream: one draw of the (slot, other, target) stream (the pair host's random(): 107E70's draws for a viewed rider)
EMSCRIPTEN_KEEPALIVE uint32_t contact_stream_random(int slot,int other,int target){
 auto& stream=contact_stream_of(slot,other,target);
 ssx::OriginalRandomState state;
 state.words=stream;
 const uint32_t value=state.next();
 stream=state.words;
 return value;
}
// Tests (web/test-mp-pairs.mjs): the six words of one key, as web/net/pair-net.js contactStream makes them
EMSCRIPTEN_KEEPALIVE uint32_t* contact_stream_test(uint32_t seed,uint32_t tick,int a,int b,int target){
 RIDER_LOCAL static std::array<uint32_t,6> out{};
 out=contact_stream_words(seed,tick,a,b,target);
 return out.data();
}
}
