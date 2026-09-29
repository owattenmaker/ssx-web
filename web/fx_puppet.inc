// Online races (web/net/remote-fx.js): another racer's rider effects -- board track (RFX+0x520), snow spray, wake,
// breath, kicker, body snow (+0xB40/+0x3B0), boost (+0x610), board sparks and fist sparkle (+0x470/+0xC70) -- drawn
// on this client. The original updates every rider's RFX container in the rider-manager passes (128F20..1290F0);
// a remote racer has no rider simulation here, so its own client records, once per FX pass, exactly the rider
// fields those updates read (fx_record, called at the start of the FX pass in animation_post_phase), streams them,
// and a "puppet" core on this client (one per remote racer, never simulated) writes them back and runs the same
// update_board_sparks / update_trail / update_snow / update_fist_sparkle / update_boost_fx (fx_puppet_step). The
// renderers then read the puppet core's usual exports. Geometry, timing, colours and random draws follow the
// sender's rider; the sender's particle and visual random words travel with each pass, so the puppet's particles,
// track jitter and sparks are the sender's bit for bit. Included at the end of web/animation_bridge.cpp.
//
// Record (FX_RECORD floats; integers stored exactly):
//  0 motion (0 ground, 1 air, 2 crash, 3 reset, 4 rail)  1 crash submode  2 crash detached  3 crash ticks
//  4 crash surface  5..7 crash contact point  8 ground surface id  9 rail surface
// 10..12 velocity  13..16 quaternion  17..19 position  20..22 normal  23..25 lateral  26 distance  27 reverse stance
// 28 presentation roll (+274)  29 extra lean (+208)  30 turn (+1F0)  31 brake (+214)  32 control state
// 33..35 trail contact  36 landed  37 landing speed
// 38 snow impact serial (low 24 bits)  39..41 impact point  42..44 impact normal  45 impact strength  46 impact surface
// 47..49 requested semantics channels 0..2  50 channel-1 sequence flags (low 16)  51 channel-2 sequence flags (low 16)
// 52 attack strength (+350)  53 boost amount  54 boost window  55 boost tier  56 super time  57 board press style (+330)
// 58 ground patch  59 ground u  60 ground v  61 prediction status  62 predicted patch  63 predicted u  64 predicted v
// 65 predicted time  66 prediction elapsed
// 67..78 particle random state (0x4FF... six words, as 16-bit halves: the sender's camera shake draws from it too)
// 79..80 visual LCG word GP+0xA0C (16-bit halves)
RIDER_LOCAL extern int browserGroundPatch;RIDER_LOCAL extern float browserGroundU,browserGroundV; // web/core.cpp (ground patch of the environment colour)
static constexpr unsigned FX_RECORD=81;
RIDER_LOCAL static bool fxRecording=false;RIDER_LOCAL static std::vector<float> fxRecords;RIDER_LOCAL static uint32_t fxRecordCount=0;
static void fx_record(){
 if(!fxRecording||fxRecords.size()>64*(FX_RECORD+1))return;
 const int motion=rider_motion_mode(); // the puppet maps 3 back to browserResetActive (every FX consumer reads only the mode)
 float r[FX_RECORD]{};auto put3=[&](unsigned at,const std::array<float,3>& v){for(unsigned k=0;k<3;k++)r[at+k]=v[k];};
 r[0]=float(motion);r[1]=float(crash.motion.submode);r[2]=crash.actor.detached;r[3]=float(std::min<uint32_t>(crash.ticks,1u<<20));r[4]=float(crash.actor.surface);put3(5,crash.actor.contactPoint);
 r[8]=float(physicsProfile.surface.id);r[9]=float(railSurface);
 put3(10,physicsState.velocity);for(unsigned k=0;k<4;k++)r[13+k]=physicsState.quaternion[k];put3(17,physicsState.position);put3(20,physicsState.normal);put3(23,physicsState.lateral);
 r[26]=physicsState.distance;r[27]=physicsState.reverseStance;r[28]=physicsState.presentationRoll.current;r[29]=physicsState.extraLean.current;
 r[30]=gs.turn.current;r[31]=gs.brake.current;r[32]=float(gs.controlState);put3(33,browserTrailContact);r[36]=float(landed);r[37]=browserLandingSpeed;
 r[38]=float(snowImpactSerial&0xffffffu);put3(39,snowImpactPoint);put3(42,snowImpactNormal);r[45]=snowImpactStrength;r[46]=float(snowImpactSurface);
 for(unsigned k=0;k<3;k++)r[47+k]=float(graph.requestedSemantics[k]);r[50]=float(graph.flags(1)&0xffff);r[51]=float(graph.flags(2)&0xffff);
 r[52]=attackRider.strength;r[53]=boostState.amount;r[54]=boostState.window;r[55]=float(boostState.tier);r[56]=boostState.superTime;r[57]=float(board_press_style());
 r[58]=float(browserGroundPatch);r[59]=browserGroundU;r[60]=browserGroundV;r[61]=float(browserTrajectory.status);r[62]=float(browserTrajectory.patchId);
 r[63]=browserTrajectory.patchU;r[64]=browserTrajectory.patchV;r[65]=browserTrajectory.predictedTime;r[66]=browserTrajectory.elapsed;
 for(unsigned k=0;k<6;k++){r[67+2*k]=float(snowParticleRandom.words[k]&0xffffu);r[68+2*k]=float(snowParticleRandom.words[k]>>16);}
 r[79]=float(trailVisualRandom&0xffffu);r[80]=float(trailVisualRandom>>16);
 fxRecords.push_back(float(motionTick));fxRecords.insert(fxRecords.end(),r,r+FX_RECORD);++fxRecordCount;
}
RIDER_LOCAL static uint32_t fxPuppetImpactSerial=0;RIDER_LOCAL static unsigned fxPuppetSteps=0;
extern "C" {
// Sender: 1 = record every FX pass (online race), 0 = off.
EMSCRIPTEN_KEEPALIVE void fx_recording(int on){fxRecording=on!=0;fxRecords.clear();}
// Sender: drains the records since the last call: [count, (motion tick, FX_RECORD floats) x count].
EMSCRIPTEN_KEEPALIVE float* fx_records(){RIDER_LOCAL static std::vector<float> out;out.assign(1,float(fxRecords.size()/(FX_RECORD+1)));out.insert(out.end(),fxRecords.begin(),fxRecords.end());fxRecords.clear();return out.data();}
EMSCRIPTEN_KEEPALIVE int fx_record_size(){return int(FX_RECORD);}
// Puppet: the remote rider's placement (full=0: 111890's reset placement) or a new run / rescue (full=1).
EMSCRIPTEN_KEEPALIVE void fx_puppet_reset(int full){reset_trail();reset_snow(full!=0);reset_impact_fx(full!=0);reset_boost_fx();cachedCrashWorld.clear();}
// Puppet: one FX pass of the remote rider. record = FX_RECORD floats, pose = world pose (bones x 7: position xyz,
// quaternion xyzw, as world_pose_bones).
EMSCRIPTEN_KEEPALIVE int fx_puppet_step(const float* r,const float* pose,int bones){
 if(bones<24||bones>64)throw std::runtime_error("FX puppet pose");
 physicsAttached=true;cachedCrashWorld.resize(size_t(bones));
 for(int b=0;b<bones;b++){auto& t=cachedCrashWorld[b];for(unsigned k=0;k<3;k++)t.position[k]=pose[b*7+k];for(unsigned k=0;k<4;k++)t.rotation[k]=pose[b*7+3+k];}
 const int motion=int(r[0]);auto get3=[&](unsigned at){return std::array<float,3>{r[at],r[at+1],r[at+2]};};
 browserResetActive=motion==3;crash.active=motion==2;browserRailActive=motion==4; ::grounded=motion==0;
 crash.motion.submode=int(r[1]);crash.actor.detached=r[2]!=0;crash.ticks=uint32_t(r[3]);crash.actor.surface=int(r[4]);crash.actor.contactPoint=get3(5);
 physicsProfile.surface.id=int(r[8]);railSurface=int(r[9]);
 physicsState.velocity=get3(10);for(unsigned k=0;k<4;k++)physicsState.quaternion[k]=r[13+k];physicsState.position=get3(17);physicsState.normal=get3(20);physicsState.lateral=get3(23);
 physicsState.distance=r[26];physicsState.reverseStance=r[27]!=0;physicsState.presentationRoll.current=r[28];physicsState.extraLean.current=r[29];
 gs.turn.current=r[30];gs.brake.current=r[31];gs.controlState=int(r[32]);browserTrailContact=get3(33);landed=int(r[36]);browserLandingSpeed=r[37];
 const uint32_t impact=uint32_t(r[38]);if(fxPuppetSteps&&impact!=fxPuppetImpactSerial)record_snow_crash(get3(39),get3(42),r[45],int(r[46]));fxPuppetImpactSerial=impact;
 for(unsigned k=0;k<3;k++)graph.requestedSemantics[k]=int(r[47+k]);
 graph.sequences.clear();for(unsigned channel:{1u,2u}){OriginalAnimationSequence s;s.channel=int(channel);s.semantic=graph.requestedSemantics[channel];s.flags=uint64_t(r[49+channel]);graph.sequences.push_back(std::move(s));}
 attackRider.strength=r[52];boostState.amount=r[53];boostState.window=r[54];boostState.tier=int32_t(r[55]);boostState.superTime=r[56];bp330=int32_t(r[57]);
 browserGroundPatch=int(r[58]);browserGroundU=r[59];browserGroundV=r[60];browserTrajectory.status=decltype(browserTrajectory.status)(int(r[61]));browserTrajectory.patchId=decltype(browserTrajectory.patchId)(r[62]);
 browserTrajectory.patchU=r[63];browserTrajectory.patchV=r[64];browserTrajectory.predictedTime=r[65];browserTrajectory.elapsed=r[66];
 for(unsigned k=0;k<6;k++)snowParticleRandom.words[k]=uint32_t(r[67+2*k])|(uint32_t(r[68+2*k])<<16);trailVisualRandom=uint32_t(r[79])|(uint32_t(r[80])<<16);
 // The rider-manager FX pass order of animation_post_phase (sparks, track, snow, fist sparkle, boost).
 update_board_sparks();update_trail();update_snow();update_fist_sparkle();update_boost_fx();
 return int(++fxPuppetSteps);
}
}
