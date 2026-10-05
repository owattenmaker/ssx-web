// Stage builtin 34 "teleport current player", rider side: rider vt+0x54 = 0x123210(rider, matrix) for the human and the
// computer riders (both vtables 0x4583A8 / 0x458660 have it). The Metro-City phone booths and water towers (BRA2 track 16,
// programs 417/419/420: builtin 77 draw, builtin 34 to one of the destination matrices), in the race and in the streamed
// Peak 1 world. Spec with every callee: docs/stage-teleport.md; PS2 captures: docs/stage-teleport.md "PS2 ground truth".
// Included at the end of web/animation_bridge.cpp (the reset placement 11D660, the pose state; case 34 in web/stage_script_gameplay.inc).
// Off until the host turns it on (pv boothTeleport, web/pv-flags.js): off, builtin 34 stays counted as unsupported.
// stageTeleportEnabled (web/stage_script_gameplay.inc, before builtin 34): pv boothTeleport.
RIDER_LOCAL static uint32_t stageTeleportCount=0;
RIDER_LOCAL static std::array<float,10> stageTeleportLast{}; // QA: count, P xyz, D xy, speed, slot, motion and control before
extern "C" void browser_camera_teleport_cut(float x,float y,float z); // web/core.cpp: director set-target cut with the rider as it is
extern "C" void start_ground_motion();                                 // web/core.cpp: 13C7A8 (motion 0 entry)
static void stage_teleport_rider(const std::array<float,16>& m){
 if(!cameraTerrain||!browserBodies)return;
 const bool human=browserHumanRider;
 const int motion=npc_motion_mode(),control=browser_control_state();
 // 1. 270970(race event, rider+0x86C): releases a pending human gate link only (not modelled; a no-op in these events).
 // 2. Destination 0x123230..0x1233C4 (VU0, chop): o = (a, b, 0, 0) by rider+0x86C (roster slot: the human 0, computer riders
 //    1..5), t = M o (vmulax/vmadday/vmaddaz/vmaddw), P = r3 + t * 100; D = M (1, 0, 0, 0) (0x4FF140), only D.xy is used.
 static constexpr float kSlot[6][2]={{2,0},{1,0},{2,-2},{2,2},{1,-2},{2,1}};
 const unsigned slot=human?0u:npc.slot;const float a=slot<6?kSlot[slot][0]:0.f,b=slot<6?kSlot[slot][1]:0.f;
 terrain_original::Vector P{},D{};
 {terrain_original::Rounding rounding;using terrain_original::add;using terrain_original::mul;
  for(unsigned k=0;k<3;++k){
   const float t=add(add(add(mul(m[k],a),mul(m[4+k],b)),mul(m[8+k],0.f)),mul(m[12+k],0.f));
   P[k]=add(m[12+k],mul(t,100.f));
   D[k]=add(add(add(mul(m[k],1.f),mul(m[4+k],0.f)),mul(m[8+k],0.f)),mul(m[12+k],0.f));}}
 // 3. 119368(score, 1): combo / pending points lost, reset count +1; the returned boost change is discarded.
 grab={};score_reset(true);
 // 4. 11FE78(rider, 0) -> 1112B8: the current motion's exit (13F410 ground: decay targets, the leave tick; 139A18 air: none;
 //    136F28 crash; 13C5A0 rail; 139178 handplant), then the motion-0 entry 13C7A8 (velocity x 0.7 when the rider left the
 //    ground at most 40 ticks ago, less loss the longer it flew; the ground focus tick).
 if(browserResetActive)clear_reset(); // 125038 below: the reset control 9 ends with the teleport (its fade stops)
 if(motion==0&&::grounded&&!browserRailActive)originalLandingGroundLeave(physicsState,motionTick,lastGroundLeave);
 if(motion==2){crash.active=false;browserCrashActive=false;}
 if(motion==5&&browserHandplantReset)browserHandplantReset();
 // 5. 11FEC8(rider, 0) -> 111538: the current control's exit (control 5 134CB0 the air control exit, control 7 132048 the rail
 //    exit), then control 0 (131608: idle clock and board-press latch 0).
 if(control==5&&!::grounded&&!landingAirExitBaked&&!crash.active)landing_air_exit();
 rail_reset_leave();detach_rail_for_crash();
 start_ground_motion();
 heldAirMode=passiveMode=false;prewind={};air={};airAnimation={};passive={};landingAirExitBaked=false;browserSoftActive=browserSoftFrame=false;
 physicsState.controlState=gs.controlState=0;idleSeconds=0;idleControl=0; // 131608 already ran: the next post keeps the clock running
 // 6. Camera cut 1 (the human, director vt+0x1C 0x15CC70 -> set-target 0x176FE0 of its nodes): the rider as it is now (this
 //    tick's head, the scaled velocity, motion 0). The outer camera's own DEFAULT_3 (Y+0xC0) is not modelled.
 if(human)browser_camera_teleport_cut(posedPhysical[9],posedPhysical[10],posedPhysical[11]);
 // 7. The speed after 13C7A8: vmul, vadday.x, vmaddaz.x, vmaddw.x (w 0), vsqrt.
 float speed;
 {terrain_original::Rounding rounding;using terrain_original::add;using terrain_original::mul;const auto& v=physicsState.velocity;
  speed=terrain_original::sqrt(add(add(add(mul(v[0],v[0]),mul(v[1],v[1])),mul(1.f,mul(v[2],v[2]))),mul(1.f,mul(0.f,0.f))));}
 // 8. 11D660(rider, P, D, semantic 5 RNORM_FWD_CYC, clearance 0): the reset placement (probe, heading from D.xy, velocity 0,
 //    filters, boost amount 0, animation 5 with one full-rate step, the placed pose; FX reset 111890). It keeps the 13C7A8
 //    depths and writes the surface +0x438 = 0 (as the location entry 11DE60, web/animation_bridge.cpp place_rider_region), and
 //    it does not rebuild the camera (15CCF0 set-targets instead).
 const float depth1=physicsState.depth1,depth3=physicsState.depth3;const bool cameraBuilt=chaseReady;
 reset_place_at(P,D,0.f,5,true);
 chaseReady=cameraBuilt;physicsState.depth1=depth1;physicsState.depth3=depth3;
 // 11E150 cached the placed pose (rider+0x2C) and its board frame (+0x170 / +0x180): the next tick's contacts and this tick's skin
 // palette read them (the reset control places before this tick's pose; the teleport comes after it, inside 121818).
 if(!resetStaleWorld.empty()){cachedCrashWorld=resetStaleWorld;boardMorphDrawn=boardMorphReset;clear_skin_matrices();
  const auto frame=originalRiderCollisionFrame(resetPlacedBoardRoot);physicsState.presentationUp=frame.up;presentationForward170=frame.forward;}
 {const float limit=physicsProfile.speedLimit;physicsProfile=physicsMaterials[0];physicsProfile.speedLimit=limit;}
 physicsState.controlState=gs.controlState=0;::grounded=true;previousGround=true;previousHeld=false;
 const auto& head=resetStaleWorld.empty()?physicsState.position:resetStaleWorld.at(0).position;
 for(unsigned k=0;k<3;++k){posedPhysical[k]=physicsState.position[k];posedPhysical[9+k]=head[k];}
 for(unsigned k=0;k<4;++k)posedPhysical[3+k]=physicsState.quaternion[k];posedPhysical[7]=1;posedPhysical[8]=1;
 // 9. 11D660's tail for devices 0 / 1: director vt+0x24 0x15CCF0 (set-target on every node) with the placed rider, velocity 0.
 if(human)browser_camera_teleport_cut(head[0],head[1],head[2]);
 // 10. +0x1E0 = +0x1B0 (the placed physical forward) x speed, all lanes (z kept: no 11DF18 here, no animation freeze).
 {terrain_original::Rounding rounding;for(unsigned k=0;k<3;++k)physicsState.velocity[k]=terrain_original::mul(physicsState.physicalForward[k],speed);}
 gs=physicsState;commit_rider_physics();
 // 11. 111890 (FX reset: reset_place_at above) and 2F1A00 (rider+0x88C bone emitter +0xA0 = 0: not modelled).
 ++stageTeleportCount;stageTeleportLast={float(stageTeleportCount),P[0],P[1],P[2],D[0],D[1],speed,float(slot),float(motion),float(control)};
}
extern "C" {
// pv boothTeleport (web/pv-flags.js): web/main.js applyPresentationFast sets it on the human's and every computer rider's core;
// the capture comparer by pv / STAGE_TELEPORT / the capture's booth_injections.
EMSCRIPTEN_KEEPALIVE void stage_teleport_enable(int on){stageTeleportEnabled=on!=0;}
// QA: [count, P xyz, D xy, speed, slot, motion, control] of the last teleport of this rider.
EMSCRIPTEN_KEEPALIVE float* stage_teleport_info(){return stageTeleportLast.data();}
// QA / PS2 capture injection (docs/stage-teleport.md "PS2 ground truth"): this tick's 121818 runs the slot-2 program of the
// instance as for a contact (the PS2 capture writes rider+0xA30 on the same tick). Call between animation_tick and race_end.
EMSCRIPTEN_KEEPALIVE void stage_contact_inject(uint32_t resource){stagePending=StagePendingContact{resource,{},{}};}
}
