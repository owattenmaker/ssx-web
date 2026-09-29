// Audio observers: event queue + per-tick telemetry for web/sfx.js (docs/audio-logic.md section 5).
// Included once, at the end of web/animation_bridge.cpp. Observers only (see web/audio_events.hpp).
RIDER_LOCAL static float audioQueue[1+64*5];RIDER_LOCAL static int audioQueueCount=0;RIDER_LOCAL static uint32_t audioQueueDropped=0;
RIDER_LOCAL float audioSpeed75C=0,audioTurn754=0,audioBrake750=0,audioCompression758=0,audioImpact770=0;
void audio_event(int type,float a,float b,float c,float d){
 if(audioQueueCount>=64){++audioQueueDropped;return;}
 float* e=audioQueue+1+5*audioQueueCount++;e[0]=float(type);e[1]=a;e[2]=b;e[3]=c;e[4]=d;
}
void audio_ground_speed(const std::array<float,3>& v){
 const float speed=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);
 audioSpeed75C=speed<=3333.33349609375f?speed/3333.33349609375f:1.f;
 audioTurn754=std::abs(physicsState.turn.current);audioBrake750=std::abs(physicsState.brake.current);
}
// 114130 -> 298D90 (start, a2 = pressed) / 299368 (denied) / 2992D8 (release), in the original's order.
OriginalBoostEffects audio_boost_observe(OriginalBoostEffects e){
 if(e.started)audio_event(AE_BOOST,1,e.pressed?1.f:0.f);
 if(e.denied)audio_event(AE_BOOST,2);
 if(e.stopped)audio_event(AE_BOOST,3);
 return e;
}
// 103AA0 observer: every playback sequence's event bits latched this tick (the original fires the 0x8000-flagged
// ids through 104CC8 for the human rider). Read-only: the flags are not consumed here.
static void audio_animation_events(const BrowserAnimationGraph& g){
 for(const auto& s:g.sequences){const uint64_t fired=s.flags&s.raisedFlags;if(!fired||s.slots.empty())continue;
  for(unsigned bit=0;bit<64;bit++)if(fired>>bit&1)audio_event(AE_ANIM_EVENT,float(s.slots.front().clip),float(bit),float(s.channel));}
}
// Stage-script sound builtins 30/31/73 (keys: 0 instance resource, -1 = the running program's instance; 1 sound id; 2 word).
void audio_stage_sound(int kind,const std::array<uint32_t,3>& w,uint32_t current){
 const uint32_t resource=w[0]==0xffffffffu?current:w[0];
 audio_event(AE_SCRIPT_SOUND,float(kind),float(int32_t(w[1])),float(resource),float(int32_t(w[2])));
}
extern "C" {
// Instance+0x10 translation of a stage/world instance (source cm, Z up), for positional script sounds.
EMSCRIPTEN_KEEPALIVE float* audio_instance_position(uint32_t resource){
 RIDER_LOCAL static float p[3];const auto m=stage_instance_matrix(resource);for(unsigned k=0;k<3;k++)p[k]=m[3][k];return p;}
// [count, then count x {type, a, b, c, d}]; the JS side calls audio_events_clear() after reading.
EMSCRIPTEN_KEEPALIVE float* audio_events(){audioQueue[0]=float(audioQueueCount);return audioQueue;}
EMSCRIPTEN_KEEPALIVE void audio_events_clear(){audioQueueCount=0;}
// Per-tick inputs of the board loops, air duck and gates (read after the tick):
//  0 speed 0x75C (13F178: min(|v| / 3333.33, 1), ground ticks), 1 turn 0x754 = |+0x1F0|, 2 brake 0x750 = |+0x214|,
//  3 compression 0x758 (13C878), 4 motion mode (0 ground, 1 air, 2 crash, 3 reset, 4 rail, 5 handplant),
//  5 surface 0x438, 6 rail surface audio+0x598C (106848), 7 predictor status +0xAC, 8 predicted time +0x98,
//  9 apex time +0x9C, 10 elapsed +0xA0, 11 impact 0x770, 12..14 position rider+0x110 (cm, Z up),
//  15 human (+0x874), 16 control state, 17 crash active, 18 dropped events, 19..21 velocity rider+0x1E0 (cm/s),
//  22 Tricky time +0x2F0 (superTime), 23 boost tier +0x2F4.
EMSCRIPTEN_KEEPALIVE float* audio_telemetry(){
 RIDER_LOCAL static float v[24];
 v[0]=audioSpeed75C;v[1]=audioTurn754;v[2]=audioBrake750;v[3]=audioCompression758;v[4]=float(npc_motion_mode());
 v[5]=float(physicsProfile.surface.id);v[6]=float(railSurface);
 v[7]=browserPredictionAvailable?float(browserTrajectory.status):0.f;v[8]=browserTrajectory.predictedTime;v[9]=browserTrajectory.apexTime;v[10]=browserTrajectory.elapsed;
 v[11]=audioImpact770;for(unsigned k=0;k<3;k++){v[12+k]=physicsState.position[k];v[19+k]=physicsState.velocity[k];}
 v[15]=browserHumanRider?1.f:0.f;v[16]=float(physicsState.controlState);v[17]=browserCrashActive?1.f:0.f;v[18]=float(audioQueueDropped);
 v[22]=boostState.superTime;v[23]=float(boostState.tier);
 return v;
}
}
