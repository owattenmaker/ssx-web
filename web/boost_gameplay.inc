#include "../engine/boost_draw.hpp"
RIDER_LOCAL static OriginalBoostEffectState boostFx;
RIDER_LOCAL static OriginalBoostSideHistory boostSide;
static int32_t board_press_style();
RIDER_LOCAL static OriginalBoostRibbonHistory boostRibbon;
RIDER_LOCAL static std::array<std::vector<float>,3> boostVertices;
RIDER_LOCAL static uint32_t boostFxSerial=0;RIDER_LOCAL static unsigned boostFxUnavailable=0;
RIDER_LOCAL static bool boostFxVisible=false;
static void reset_rider_fx();
static void update_rider_fx(const OriginalBoostEffectParameters&);
static void reset_boost_fx(){reset_rider_fx();boostFx={};boostSide={};boostSide.capacity=originalBoostHistoryCapacity(resetInputs.deviceIndex);boostRibbon={};for(auto&v:boostVertices)v.clear();boostFxVisible=false;boostFxUnavailable=0;++boostFxSerial;}
// gp+0x1630: the original's trick-boost look switch, read by 2EADD0 (aura + rider light: f20 = 10 instead of rider+0x2EC)
// and 2EF950 (prbn streamers); 0 in the shipped game. One word for every rider, like gp (check-rider-globals SHARED):
// a QA switch here (set_trick_fx_force) to compare the computer riders' boost FX with PS2 frames made with it poked.
static bool trickFxForce=false;
extern "C" EMSCRIPTEN_KEEPALIVE void set_trick_fx_force(int on){trickFxForce=on!=0;}
// pv streamers (web/boost-renderer.js): the air streamers take their nose / tail offsets (2EF6D0, 90 x row 0) and half width (2EF950,
// 7.5 / 18.75 x row 2) from the board bone's render matrices (rider+0x780 -> +0x34: the world rows x the rider geometry scale,
// original_animation.scale 0.85; PS2 setpieces-bra2 tick 418: a ring pair 153 cm apart = 180 x 0.85), where the port's board rows
// are the unit world rows (+0x30). Off = the port's unit rows (the shipped path until the switch is on); on = each rider's own
// geometry scale (graph.scale). One word for every rider of the core.
static bool riderFxRenderScale=false;
extern "C" EMSCRIPTEN_KEEPALIVE void set_rider_fx_render_scale(int on){riderFxRenderScale=on!=0;}
// The control owner's light list (rider+0x77C -> +0xD30): cleared every tick by 1218D0 (392D18), one light added by 2EADD0
// while the trick boost holds (392D90: direction (0,0,1), colour (2,2,2)); 1220D8 adds it to the rider irradiance.
// [count, ambient RGB, direction XYZ, colour RGB] = shade_rider_lighting's extra block from +1 (web/rider-controller-lights.js).
RIDER_LOCAL static float controllerLights[10];
extern "C" EMSCRIPTEN_KEEPALIVE float* rider_controller_lights(){return controllerLights;}
static void update_boost_fx(){
 for(auto&v:controllerLights)v=0.f; // 1218D0 -> 392D18, before the FX pass
 if(!physicsAttached||cachedCrashWorld.size()<=23)return;
 auto quad=[](std::array<float,3> v,float w){return BoostHistoryQuad{v[0],v[1],v[2],w};};
 const auto physical=originalOrientationBasis(physicsState.quaternion);const auto board=originalRiderCollisionFrame(cachedCrashWorld.at(trailBones[0]));
 const auto head=quad(cachedCrashWorld.at(0).position,1),right=quad(physical.right,0);
 OriginalBoostEffectInput input;input.velocity=physicsState.velocity;input.boost=boostState.amount;input.pickup=boostState.window;input.tier=boostState.tier;
 input.motionMode=rider_motion_mode();
 const auto parameters=originalBoostEffectParameters(boostFx,input);
 originalBoostSideHistory(boostFx,boostSide,parameters,right,head,quad(cachedCrashWorld.at(trailBones[1]).position,1),quad(cachedCrashWorld.at(trailBones[2]).position,1),true);
 OriginalBoostRibbonInput ribbon;ribbon.position=quad(physicsState.position,1);ribbon.velocity=quad(physicsState.velocity,0);ribbon.boardPosition=quad(board.origin,1);ribbon.boardX=quad(board.right,0);ribbon.boardZ=quad(board.up,0);ribbon.groundNormal=quad(physicsState.normal,0);ribbon.contactPoint=quad(browserTrailContact,1);ribbon.switchStance=physicsState.reverseStance;ribbon.semantic=graph.requestedSemantics[2];
 ribbon.flag330=board_press_style()!=0; //rider+0x330 (web/boardpress_gameplay.inc)
 originalBoostRibbonStep(boostFx,boostRibbon,ribbon);
 for(auto&v:boostVertices)v.clear();boostFxVisible=originalBoostDrawEnabled(boostFx,true);
 if(boostFxVisible){const auto draw=originalBoostDrawVertices(boostFx,boostSide,boostRibbon,head,right);unsigned strip=0;
  for(const auto* source:{&draw.left,&draw.right,&draw.main}){
   auto&out=boostVertices[strip++];
   for(unsigned i=2;i<source->size();i++){
    const std::array<unsigned,3> indices=i&1?std::array<unsigned,3>{i-1,i-2,i}:std::array<unsigned,3>{i-2,i-1,i};
    bool transparent=true,finite=true;for(auto index:indices){const auto&v=source->at(index);transparent&=v.colour[3]==0;finite&=std::isfinite(v.u)&&std::isfinite(v.v);for(float x:v.position)finite&=std::isfinite(x);}
    if(transparent)continue;if(!finite){++boostFxUnavailable;continue;}
    for(auto index:indices){const auto&v=source->at(index);out.insert(out.end(),{v.position[0],v.position[1],v.position[2],v.u,v.v});for(auto colour:v.colour)out.push_back(float(colour)/128.f);}
   }
  }
 }
 update_rider_fx(parameters);
 ++boostFxSerial;
}
extern "C" EMSCRIPTEN_KEEPALIVE float* boost_fx_info(){RIDER_LOCAL static float out[12];out[0]=boostFx.texture;out[1]=boostFx.trailCount;out[2]=boostFx.primaryCount;out[3]=boostFx.trailLength;out[4]=boostFx.alpha;out[5]=boostFx.width;for(unsigned i=0;i<3;i++)out[6+i]=boostVertices[i].size()/9;out[9]=boostFxSerial;out[10]=boostFxVisible;out[11]=boostFxUnavailable;return out;}
extern "C" EMSCRIPTEN_KEEPALIVE float* boost_fx_vertices(unsigned strip){return boostVertices.at(strip).data();}

// ---- Power-up aura (RFX+0x9C0: ctor 2EADC0, update 2EADD0, draw 2EB198) and air streamers (RFX+0xAD0: ctor 2EF6A0,
// update 2EF6D0, draw 2EF950), updated right after the boost ribbon (2E66B8) in the FX pass (docs/visual-rng-order.md:
// no random draws). PS2 runs/peak2/cra3-full 8280..8401: after a trick boost (rider+0x2EC > 0) the streamers turn into
// the purple `prbn` beams and the board is wrapped in the `psmr` smear while airborne; without it the streamers are the
// faint white `strm` sheets under every jump (runs/peak2/cra3-full.tick6819).
struct BrowserRiderAura {int count=0,cursor=0;std::array<std::array<std::array<float,3>,4>,3> rows{};std::array<float,3> e0{},f0{};};
struct BrowserAirStreamer {int cursor=0,count=0,capacity=25;float scroll=0;std::array<std::array<std::array<float,3>,2>,25> ring{};};
RIDER_LOCAL static BrowserRiderAura riderAura;
RIDER_LOCAL static BrowserAirStreamer airStreamer;
RIDER_LOCAL static std::array<std::vector<float>,6> riderFxVertices; // triangle lists: 0..3 aura faces (psmr), 4..5 streamers
RIDER_LOCAL static bool riderFxTrick=false;                             // streamer texture: prbn (repeat) instead of strm (clamp)
RIDER_LOCAL static uint32_t riderFxSerial=0;
static void reset_rider_fx(){ // 2EADC0 / 2EF6A0 (capacity 25, 18 for a rider without a device: rider+0x870 < 0)
 riderAura={};airStreamer={};airStreamer.capacity=resetInputs.deviceIndex<0?18:25;for(auto&v:riderFxVertices)v.clear();riderFxTrick=false;++riderFxSerial;
}
// The air streamers' state (RFX+0xAD0: +8 cursor, +0xC count, +0x10 scroll, ring *(+4) of 25 x {nose, tail}) taken from a PS2 tick:
// w = cursor, count, scroll, then 25 x 2 x (x, y, z) source cm (board - 0.7 x root). The CTM plane drop (web/plane-drop.js, pv
// dropCamera) starts 10 ticks after its placement's FX reset, when the scroll is 0.81 and the ring holds 4 rows.
extern "C" EMSCRIPTEN_KEEPALIVE void rider_fx_streamer_seed(const float* w){auto& st=airStreamer;st.cursor=int(w[0])%st.capacity;st.count=std::min(int(w[1]),st.capacity);st.scroll=w[2];
 for(unsigned k=0;k<25;k++)for(unsigned e=0;e<2;e++)for(unsigned i=0;i<3;i++)st.ring[k][e][i]=w[3+(k*2+e)*3+i];++riderFxSerial;}
struct RiderFxVertex {std::array<float,3> p;float s,t;std::array<int,4> rgba;};
// A GS triangle strip -> triangle list (the boost strips' winding), dropping fully transparent triangles.
static void rider_fx_strip(std::vector<float>& out,const std::vector<RiderFxVertex>& v){
 for(size_t i=2;i<v.size();i++){
  const std::array<size_t,3> k=i&1?std::array<size_t,3>{i-1,i-2,i}:std::array<size_t,3>{i-2,i-1,i};
  if(v[k[0]].rgba[3]==0&&v[k[1]].rgba[3]==0&&v[k[2]].rgba[3]==0)continue;
  for(size_t j:k){const auto& x=v[j];out.insert(out.end(),{x.p[0],x.p[1],x.p[2],x.s,x.t});for(int c:x.rgba)out.push_back(float(c)/128.f);}
 }
}
static void update_rider_fx(const OriginalBoostEffectParameters&){
 for(auto&v:riderFxVertices)v.clear();
 if(!physicsAttached||cachedCrashWorld.size()<=size_t(trailBones[0])){++riderFxSerial;return;}
 terrain_original::Rounding rounding;using namespace terrain_original;
 const bool air=rider_motion_mode()==1;const float modifier=trickFxForce?10.f:boostState.modifier; // 11FE98 == 1, rider+0x2EC (gp+0x1630: 10)
 if(0.f<modifier){ // 2EAE14..2EAE5C: 392DE8 (empty), 392D90(D30, (0,0,1,0), (2,2,2))
  controllerLights[0]=1.f;controllerLights[6]=1.f;controllerLights[7]=controllerLights[8]=controllerLights[9]=2.f;
 }
 const auto board=originalRiderCollisionFrame(cachedCrashWorld.at(trailBones[0])); // bone rider+0x8A4: row0 right, row2 up, row3 origin
 const auto root=cachedCrashWorld.at(0).position;                                  // bone rider+0x89C (0)
 auto axpy=[](const std::array<float,3>& a,float k,const std::array<float,3>& x){return std::array<float,3>{add(a[0],mul(k,x[0])),add(a[1],mul(k,x[1])),add(a[2],mul(k,x[2]))};};
 // 2EADD0: the smear rows (last three board outlines) while the trick boost holds in the air (its light: above).
 if(0.f<modifier&&air){
  riderAura.count=std::min(riderAura.count+1,3);riderAura.cursor=(riderAura.cursor+2)%3;
  const auto& P=board.origin;riderAura.e0=axpy(P,-12.f,board.up);riderAura.f0=axpy(P,12.f,board.up);
  const auto nose=axpy(P,70.f,board.right),tail=axpy(P,-70.f,board.right);auto& row=riderAura.rows[size_t(riderAura.cursor)];
  row={axpy(nose,-18.f,board.up),axpy(nose,18.f,board.up),axpy(tail,18.f,board.up),axpy(tail,-18.f,board.up)};
 }else riderAura.count=0;
 // 2EF6D0: nose/tail points relative to 0.7 x the root, advanced on even frames (1298C8 & 1), shrinking by 2 on the ground.
 auto& st=airStreamer;
 if(air){
  const std::array<float,3> c{sub(board.origin[0],mul(.699999988f,root[0])),sub(board.origin[1],mul(.699999988f,root[1])),sub(board.origin[2],mul(.699999988f,root[2]))};
  std::array<float,3> right=board.right;if(riderFxRenderScale)for(auto& x:right)x=mul(x,graph.scale[0]); // +0x34 row 0
  st.ring[size_t(st.cursor)]={axpy(c,90.f,right),axpy(c,-90.f,right)};
  if(!(motionTick&1u)){const int old=st.cursor;st.cursor=(st.cursor+1)%25;if(st.count<st.capacity)++st.count;st.ring[size_t(st.cursor)]=st.ring[size_t(old)];}
 }else st.count=st.count>2?st.count-2:(st.count<=-3?st.count+2:0);
 const auto& v=physicsState.velocity;
 // 0x2EF8C4..0x2EF8D8: vaddw.x vf6 = 1.0, vadday x^2+y^2, vmaddaz 1.0 x z^2, vmaddw 1.0 x w (0), vsqrt
 st.scroll=sub(st.scroll,mul(terrain_original::sqrt(add(add(mul(v[0],v[0]),mul(v[1],v[1])),mul(1.f,mul(v[2],v[2])))),.000166666665f));
 if(st.scroll<0.f)st.scroll=add(st.scroll,1.f);
 if(1.f<st.scroll)st.scroll=sub(st.scroll,1.f);
 // Draw 2EB198 (count >= 2): four faces from the board centre +/-12 Z (top / bottom caps, colour 1, alpha 1) through the
 // rows (newest T = 1 .. oldest T = 0, colour 0.5, alpha 0.5); psmr, clamp, blend 7, priority 7.
 if(riderAura.count>=2){
  std::array<std::vector<RiderFxVertex>,4> face;const RiderFxVertex capTop{riderAura.f0,.5f,0.f,{255,255,255,128}},capBottom{riderAura.e0,.5f,0.f,{255,255,255,128}};
  face[1].push_back(capTop);face[3].push_back(capBottom);
  for(int k=0;k<riderAura.count;k++){
   const auto& r=riderAura.rows[size_t((riderAura.cursor+k)%3)];const float t=sub(1.f,originalScalarDivide(float(k),float(riderAura.count-1)));const std::array<int,4> c{127,127,127,64};
   face[0].push_back({r[0],0.f,t,c});face[0].push_back({r[1],1.f,t,c});
   face[1].push_back({r[1],1.f,t,c});face[1].push_back({r[2],0.f,t,c});
   face[2].push_back({r[2],0.f,t,c});face[2].push_back({r[3],1.f,t,c});
   face[3].push_back({r[3],1.f,t,c});face[3].push_back({r[0],0.f,t,c});
  }
  for(unsigned f=0;f<4;f++)rider_fx_strip(riderFxVertices[f],face[f]);
 }
 // Draw 2EF950 (count >= 2): two strips (nose, tail) of width 15 across board Z (37.5 with the trick boost), alpha
 // 0.3 (1.0) x (1 - k^2 / (capacity-1)^2), RGB x 255 (x 0.5 with the trick boost, gp+0xA84), T = scroll + k / count.
 riderFxTrick=0.f<modifier;
 if(st.count>=2){
  const float alphaScale=riderFxTrick?1.f:.300000012f,grey=riderFxTrick?.5f:1.f,half=mul(riderFxTrick?37.5f:15.f,.5f);
  const float fade=originalScalarDivide(1.f,float((st.capacity-1)*(st.capacity-1))),step=originalScalarDivide(1.f,float(st.count));
  const std::array<float,3> base{mul(root[0],.699999988f),mul(root[1],.699999988f),mul(root[2],.699999988f)};
  std::array<float,3> up=board.up;if(riderFxRenderScale)for(auto& x:up)x=mul(x,graph.scale[0]); // +0x34 row 2
  std::array<std::vector<RiderFxVertex>,2> strip;int idx=st.cursor;float t=st.scroll,kf=0.f;const int rgb=int(mul(grey,255.f));
  for(int k=0;k<st.count;k++){
   if(idx<0)idx=24;
   float alpha=mul(alphaScale,sub(1.f,mul(mul(kf,kf),fade)));if(alpha<0.f)alpha=0.f;const std::array<int,4> c{rgb,rgb,rgb,int(mul(alpha,128.f))};
   for(unsigned e=0;e<2;e++){const auto& q=st.ring[size_t(idx)][e];const std::array<float,3> p{add(q[0],base[0]),add(q[1],base[1]),add(q[2],base[2])};
    strip[e].push_back({axpy(p,-half,up),0.f,t,c});strip[e].push_back({axpy(p,half,up),1.f,t,c});}
   --idx;kf=add(kf,1.f);t=add(t,step);
  }
  rider_fx_strip(riderFxVertices[4],strip[0]);rider_fx_strip(riderFxVertices[5],strip[1]);
 }
 ++riderFxSerial;
}
// Renderer (web/boost-renderer.js): [serial, streamer texture (61 prbn / 63 strm), then 6 triangle-list vertex counts].
extern "C" EMSCRIPTEN_KEEPALIVE float* rider_fx_info(){RIDER_LOCAL static float out[8];out[0]=float(riderFxSerial);out[1]=riderFxTrick?61.f:63.f;for(unsigned i=0;i<6;i++)out[2+i]=float(riderFxVertices[i].size()/9);return out;}
extern "C" EMSCRIPTEN_KEEPALIVE float* rider_fx_vertices(unsigned strip){return riderFxVertices.at(strip).data();}
