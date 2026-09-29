// The replay cameras (docs/replay.md): the view a replay (web/replay.js) draws through, stepped after the gameplay camera of
// each replayed tick from the same camera input (camera_for_head's). The gameplay camera keeps running unchanged under it:
// its eye drives the fog / weather painters and the section activation, so the re-simulated run stays exact.
//
// PS2 (SLUS_207.72): during a replay the view's director is locked (0x161FA0: flags bit 1, +0x2C = the cycled camera) and
// every change is a cut (0x15D078 forces rate 1). Web-cam (+0x2C = 0x5D) follows the course's replay camera triggers
// (manager 0x4C5830, world-stream chunk kind 17; tools/export_camera_triggers.py -> <course>/camera-triggers.json):
// 0x16D320 tests the human's rider+0x110 against every volume from the rider mover (not in the countdown / a reset / a
// handplant) and 0x16E1D8 requests the entered volume's camera (Bounded 0x5B on a point or a line, or a switch camera). A
// Web-cam that has held another camera than the preferred one for 10 s cuts back to the preferred one (0x161D80). The full
// replay cycles 9 cameras (0x445438): Web-cam, Manual 0x0B, Near/Mid/Far 0x3C..0x3E, Relative 0x5E..0x61 (2:00 ...).
// Every algorithm runs through the same compositor (0x15E668: terrain lift 0x15EE00, lens clamps); no shake in a replay.
#include "json.hpp"
namespace replay_camera {
using namespace original_camera;using Quad=OriginalCameraQuad;
struct Action {int kind=3;float blend=0;int type=0;float distance=400,fov=0x1.921fb6p-1f,lookHeight=0,pitchDegrees=0,lead=0;int lookMode=0;Quad lookPoint{0,0,0,1};
 int bound=3;Quad boundPos{0,0,0,1},lineA{0,0,0,1},lineB{0,0,0,1};};
struct Trigger {uint32_t id=0,mask=0;int volume=0;Quad pos{0,0,0,1},scale{1,1,1,0};float rz=0,rx=0,ry=0;Action enter,exit;};
struct Active {uint32_t trigger=0;bool inside=false;};
// One camera algorithm of the replay view (the chase family in `chase`, the rest in the fields below).
struct Algorithm {
 int type=0x3D;int action=-1;OriginalChaseAlgorithmState chase;
 Quad eye{0,0,0,1},look{0,0,0,1};float fov=0x1.921fb6p-1f,near=10,far=30000;
 bool relativeSet=false;Quad relativeLook{0,0,0,1},relativeOffset{0,0,0,0};   // Relative +0x394, +0x3B0, +0x3A0
 Quad spokeDirection{1,0,0,0};                                                  // SPOKE +0x390
};
}
RIDER_LOCAL_LAZY static std::vector<replay_camera::Trigger> replayTriggers;RIDER_LOCAL_LAZY static std::vector<replay_camera::Active> replayActive;
RIDER_LOCAL_LAZY static std::vector<uint32_t> replayStack;
RIDER_LOCAL static replay_camera::Algorithm replayAlgorithm;RIDER_LOCAL static OriginalCameraCompositorState replayCompositor;
RIDER_LOCAL static int replayMode=0x5D,replayPreferred=0x3D,replayOverrideType=0x3D,replayOverrideAction=-1;RIDER_LOCAL static bool replayOverride=false,replayCutPending=true;
RIDER_LOCAL static float replayRevertTimer=0;RIDER_LOCAL static uint32_t replayFired=0,replayLastTrigger=0xFFFFFFFFu,replayCuts=0;RIDER_LOCAL static float replayOut[10];
RIDER_LOCAL static float manualYaw=0,manualPitch=0,manualDistance=220; // 0x4A127C / 0x4A1280 / 0x4A1284 (session globals, 0 / 0 / 220 at boot)
namespace replay_camera {
inline Quad Q3(const nlohmann::json& a,float w=1){return {a.at(0).get<float>(),a.at(1).get<float>(),a.at(2).get<float>(),w};}
inline Action parseAction(const nlohmann::json& j){
 Action a;const std::string kind=j.value("kind",std::string("none"));a.blend=j.value("blendSeconds",0.f);
 if(kind=="switch"){a.kind=0;a.type=j.value("directorType",0x3D);}
 else if(kind=="bounded"){a.kind=1;a.type=0x5B;a.distance=j.value("distance",400.f);a.fov=j.value("fov",0x1.921fb6p-1f);a.lookHeight=j.value("lookHeight",0.f);
  a.pitchDegrees=j.value("pitchDegrees",0.f);a.lead=j.value("lookAhead",0.f);a.lookMode=j.value("lookMode",0);if(j.contains("lookPoint"))a.lookPoint=Q3(j["lookPoint"]);
  const auto& b=j.at("bound");const std::string bt=b.value("type",std::string("point"));a.bound=bt=="line"?2:bt=="point"?3:bt=="box"?1:0;
  if(b.contains("pos"))a.boundPos=Q3(b["pos"]);if(a.bound==2){a.lineA=Q3(b.at("a"));a.lineB=Q3(b.at("b"));}}
 else if(kind=="spline"){a.kind=2;a.type=0x5C;}
 return a;
}
// Rotation of v about a world axis by angle (right-handed, column form).
inline Quad rotX(const Quad& v,float a){const float c=std::cos(a),s=std::sin(a);return {v[0],c*v[1]-s*v[2],s*v[1]+c*v[2],v[3]};}
inline Quad rotY(const Quad& v,float a){const float c=std::cos(a),s=std::sin(a);return {c*v[0]+s*v[2],v[1],-s*v[0]+c*v[2],v[3]};}
inline Quad rotZ(const Quad& v,float a){const float c=std::cos(a),s=std::sin(a);return {c*v[0]-s*v[1],s*v[0]+c*v[1],v[2],v[3]};}
// 0x172278 ellipsoid: |S^-1 Rz Ry Rx (p - pos)|^2 <= 1; 0x1728F8 box: |(Rx Rz (p - pos))_i| <= extent_i (no Y rotation).
inline bool contains(const Trigger& t,const Quad& p){
 Quad d{p[0]-t.pos[0],p[1]-t.pos[1],p[2]-t.pos[2],0};
 if(t.volume==0){d=rotZ(rotY(rotX(d,t.rx),t.ry),t.rz);const float x=d[0]/t.scale[0],y=d[1]/t.scale[1],z=d[2]/t.scale[2];return (x*x+y*y)+z*z<=1.f;}
 d=rotX(rotZ(d,t.rz),t.rx);return std::fabs(d[0])<=t.scale[0]&&std::fabs(d[1])<=t.scale[1]&&std::fabs(d[2])<=t.scale[2];
}
inline Quad unit3(Quad v){v[3]=0;const float l=std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]);return l>0?Quad{v[0]/l,v[1]/l,v[2]/l,0}:Quad{1,0,0,0};}
inline Quad add3(const Quad& a,const Quad& b,float w=1){return {a[0]+b[0],a[1]+b[1],a[2]+b[2],w};}
inline Quad sub3(const Quad& a,const Quad& b,float w=0){return {a[0]-b[0],a[1]-b[1],a[2]-b[2],w};}
inline Quad mul3(const Quad& a,float s,float w=0){return {a[0]*s,a[1]*s,a[2]*s,w};}
inline float len3(const Quad& a){return std::sqrt(a[0]*a[0]+a[1]*a[1]+a[2]*a[2]);}
inline float dot3(const Quad& a,const Quad& b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
constexpr float quarterPi=0x1.921fb6p-1f;
// The chase family's filtered travel velocity (step 1, 0x162568) for the non-chase types, on a scratch state.
inline Context contextOf(OriginalChaseAlgorithmState& s,const OriginalCameraInput& in){Context c;stepVelocity(s,c,in);return c;}
inline bool chaseType(int t){return original_camera_director::algorithmForRequest(t)!=0&&t!=0x44;}
inline int chaseVariant(int t){return original_camera_director::algorithmForRequest(t);}
// Bounded 0x5B (0x174200): eye on its bound (a point, or the closest point of a line to the previous eye), look-at the head
// (+ lead along the forward, + look height) or the action's point; the action's fov. 0x1732E8 (set-target) starts a line
// bound's eye where a chase camera would stand.
inline void boundedUpdate(Algorithm& g,const Action& a,const OriginalCameraInput& in){
 const Quad look=a.lookMode==1?a.lookPoint:add3(add3(in.headPosition,mul3(in.riderForward,a.lead)),Quad{0,0,a.lookHeight,0});
 if(a.bound==3)g.eye=a.boundPos;
 else if(a.bound==2){const Quad ab=sub3(a.lineB,a.lineA);const float l2=dot3(ab,ab);const float t=l2>0?dot3(sub3(g.eye,a.lineA),ab)/l2:0;g.eye=add3(a.lineA,mul3(ab,t));}
 g.eye[3]=1;g.look=look;g.look[3]=1;g.fov=a.fov;g.near=10;g.far=30000;
}
inline void boundedSetTarget(Algorithm& g,const Action& a,const OriginalCameraInput& in){
 const Quad p=add3(add3(in.headPosition,Quad{0,0,a.lookHeight,0}),mul3(in.riderForward,a.lead));
 const Quad f=in.riderForward;const float yaw=std::atan2(f[1],f[0]),slope=std::atan2(f[2],std::sqrt(f[0]*f[0]+f[1]*f[1]));
 const float pitch=-0.8f*slope-a.pitchDegrees*float(M_PI/180);
 g.eye=sub3(p,mul3(Quad{std::cos(pitch)*std::cos(yaw),std::cos(pitch)*std::sin(yaw),std::sin(pitch),0},a.distance),1);
 boundedUpdate(g,a,in);
}
// Relative 0x5E..0x61 (0x160BF8): 385.9 cm from the head at a fixed angle to the travel direction, 53.2 cm up, lagged.
inline float relativeAngle(int t){switch(t){case 0x5E:return -float(M_PI/8);case 0x5F:return float(M_PI/8);case 0x60:return -float(3*M_PI/4);case 0x61:return float(3*M_PI/4);default:return float(t-0x21)*quarterPi;}}
inline void relativeUpdate(Algorithm& g,const OriginalCameraInput& in){
 Context c=contextOf(g.chase,in);Quad dir=c.velocity;
 if(std::fabs(dir[0])<=0.001f&&std::fabs(dir[1])<=0.001f)dir=in.riderForward;
 dir[2]=0;dir=unit3(dir);const Quad side=unit3(Quad{-dir[1],dir[0],0,0});   // Z x dir: the rider's left
 Quad look=in.headPosition;if(g.relativeSet)look=add3(mul3(g.relativeLook,0.64f),mul3(look,0.36f));
 g.relativeLook=look;const float a=relativeAngle(g.type);
 Quad eye=add3(add3(look,mul3(dir,std::cos(a)*385.918f)),add3(mul3(side,std::sin(a)*385.918f),Quad{0,0,53.2416f,0}));
 if(g.relativeSet){const Quad off=add3(mul3(g.relativeOffset,0.97f),mul3(sub3(look,eye),0.03f),0);eye=sub3(look,off,1);}
 g.relativeOffset=sub3(look,eye);g.relativeSet=true;g.eye=eye;g.eye[3]=1;g.look=look;g.look[3]=1;g.fov=quarterPi;
}
// Manual 0x0B (0x1604B8): an orbit of the head by the pad (left stick yaw / pitch, right stick zoom), kept for the session.
inline void manualUpdate(Algorithm& g,const OriginalCameraInput& in,float x,float y,float z){
 manualYaw+=x*std::fabs(x)*0.05f;const float twoPiF=float(2*M_PI);while(manualYaw>=twoPiF)manualYaw-=twoPiF;while(manualYaw<0)manualYaw+=twoPiF;
 manualPitch=std::clamp(manualPitch+y*std::fabs(y)*0.05f,0.f,1.50796f);manualDistance=std::clamp(manualDistance*(1+(-z*std::fabs(z))*0.05f),150.f,850.f);
 const Quad f=directionFromAngles(manualYaw,manualPitch);
 g.look=in.headPosition;g.look[3]=1;g.eye=sub3(g.look,mul3(unit3(f),manualDistance),1);g.fov=quarterPi;
}
// Idle 7 (0x161728): a fixed eye ~1 m over where the rider was when it cut in, zooming in beyond 5.3 m.
inline void idleSetTarget(Algorithm& g,const OriginalCameraInput& in){g.eye=add3(in.headPosition,Quad{1,0,100.05f,0});}
inline void idleUpdate(Algorithm& g,const OriginalCameraInput& in){
 g.look=add3(in.headPosition,Quad{0,0,13.49558f,0});const float L=len3(sub3(g.look,g.eye));if(L>0)g.fov=std::min(531.66046f/L,1.f)*quarterPi;
}
// Lazy 0xD (0x168338): the eye trails 180 cm behind the look-at with a distance-dependent lag.
inline void lazySetTarget(Algorithm& g,const OriginalCameraInput& in){g.eye=sub3(add3(in.headPosition,Quad{0,0,100,0}),mul3(in.riderForward,200),1);}
inline void lazyUpdate(Algorithm& g,const OriginalCameraInput& in){
 (void)contextOf(g.chase,in);g.look=add3(in.headPosition,Quad{0,0,2.3602667f,0});const Quad d=sub3(g.look,g.eye);const float L=len3(d);
 if(L>0){const Quad p=sub3(g.look,mul3(d,180.19183f/L),1);const float u=L<188.07303f?0:(L<=775.458f?(L-188.07303f)/587.38501f:1);const float k=(1-u)*0.9916904f+u*0.8601375f;
  g.eye=add3(mul3(g.eye,k),mul3(p,1-k));}
 g.fov=quarterPi;
}
// SPOKE 0x42 (0x178208): pulled toward 250 cm behind the smoothed travel direction, 30 cm up, from a rider / head anchor.
inline Quad spokeAnchor(const OriginalCameraInput& in,const Quad& position){return add3(mul3(position,0.2f),mul3(in.headPosition,0.8f));}
inline Quad spokeGoal(Algorithm& g,const OriginalCameraInput& in,const Quad& position,Quad v){
 if(len3(v)<0.01f)v=in.riderForward;else v=unit3(v);
 Quad dir=add3(mul3(g.spokeDirection,0.96f),mul3(v,0.04f),0);dir=len3(dir)<0.02f?v:unit3(dir);g.spokeDirection=dir;
 return sub3(add3(spokeAnchor(in,position),Quad{0,0,30,0}),mul3(dir,250),1);
}
inline void spokeUpdate(Algorithm& g,const OriginalCameraInput& in,const Quad& position){
 Context c=contextOf(g.chase,in);const float L=len3(sub3(g.look,g.eye));const float u=L<150?0:(L<=1500?(L-150)/1350:1);float k=(1-u)*0.988f;if(in.motionMode==5)k=std::sqrt(k);
 g.eye=add3(mul3(g.eye,k),mul3(spokeGoal(g,in,position,c.velocity),1-k));g.look=add3(spokeAnchor(in,position),Quad{0,0,37,0});g.fov=quarterPi;
}
}
extern "C" {
// A course's replay camera triggers (tools/export_camera_triggers.py <course>/camera-triggers.json); only the replay-registered
// ones (mask bit 0, 0x16D1D8). Returns the count.
EMSCRIPTEN_KEEPALIVE int replay_camera_triggers(const char* text){
 replayTriggers.clear();replayActive.clear();replayStack.clear();if(!text)return 0;
 const auto d=nlohmann::json::parse(text);
 for(const auto& j:d.at("triggers")){if(!(j.value("mask",0u)&1u))continue;replay_camera::Trigger t;t.id=j.at("id");t.mask=j.at("mask");const auto& v=j.at("volume");
  t.volume=v.value("typeCode",0);t.pos=replay_camera::Q3(v.at("pos"));t.scale=replay_camera::Q3(v.at("scale"),0);t.rz=v.at("rot").value("z",0.f);t.rx=v.at("rot").value("x",0.f);t.ry=v.at("rot").value("y",0.f);
  t.enter=replay_camera::parseAction(j.at("enter"));t.exit=replay_camera::parseAction(j.at("exit"));replayTriggers.push_back(t);}
 return int(replayTriggers.size());
}
}
namespace replay_camera {
// The factory (0x15D078) with the lock's rate 1: a fresh algorithm of the type, set-target twice (push + factory tail).
inline void activate(int type,int action,const OriginalCameraInput& in){
 if(type==0x4C)type=replayPreferred;
 if(chaseType(type))type=chaseVariant(type); // +0x18 = the constructed algorithm's own type (0x161E58): switch cams 0 / 3 / 0x3D are DEFAULT_3
 Algorithm g;g.type=type;g.action=action;
 if(chaseType(type)||type==0x20||(type>=0x12&&type<=0x19)){g.chase.variantType=chaseVariant(type==0x20||(type>=0x12&&type<=0x19)?0x3D:type);setTarget(g.chase,in);setTarget(g.chase,in);}
 else {OriginalChaseAlgorithmState s;s.variantType=0x3D;setTarget(s,in);g.chase=s;} // the step-1 filters of the non-chase types
 for(int k=0;k<2;k++){
  if(type==0x5B&&action>=0&&action<int(replayTriggers.size()*2)){const auto& t=replayTriggers[size_t(action/2)];boundedSetTarget(g,action&1?t.exit:t.enter,in);}
  else if(type==7)idleSetTarget(g,in);else if(type==0xD)lazySetTarget(g,in);
  else if(type==0x42){g.spokeDirection=unit3(in.riderForward);g.eye=spokeGoal(g,in,in.headPosition,in.velocity);g.look=add3(in.headPosition,Quad{0,0,37,0});}
  else if(type>=0x5E&&type<=0x61)g.relativeSet=false;
 }
 replayAlgorithm=g;++replayCuts;
}
// 0x162060 under the lock: a trigger's request records the override; it cuts only while the view is on Web-cam.
inline void request(int type,int action,const OriginalCameraInput& in){
 if(type==0x4C){replayOverride=false;type=replayPreferred;action=-1;}else {replayOverrideType=type;replayOverrideAction=action;replayOverride=true;}
 if(replayMode==0x5D)activate(type,action,in);
}
// 0x16E1D8: a trigger's enter / exit action (index*2 + exit).
inline void fire(uint32_t index,bool exit,const OriginalCameraInput& in){
 const auto& t=replayTriggers[index];const Action& a=exit?t.exit:t.enter;++replayFired;replayLastTrigger=t.id;
 if(a.kind==1)request(0x5B,int(index*2+(exit?1:0)),in);else if(a.kind==0)request(a.type,-1,in);else if(a.kind==2)request(0x3D,-1,in); // no shipped Spline action
}
// 0x16D320 for the human rider: volumes entered / left this tick, the id stack's top camera.
inline void triggers(const Quad& p,const OriginalCameraInput& in){
 for(auto& e:replayActive)e.inside=false;
 std::vector<uint32_t> entered;
 for(uint32_t i=0;i<replayTriggers.size();i++){if(!contains(replayTriggers[i],p))continue;bool known=false;for(auto& e:replayActive)if(e.trigger==i){e.inside=true;known=true;}if(!known)entered.push_back(i);}
 for(size_t k=0;k<replayActive.size();){
  if(replayActive[k].inside){++k;continue;}
  const uint32_t i=replayActive[k].trigger,id=replayTriggers[i].id;const uint32_t top=replayStack.empty()?0xFFFFFFFFu:replayStack.back();
  std::erase(replayStack,id);
  if(replayStack.empty())fire(i,true,in);
  else if(replayStack.back()!=top){for(uint32_t j=0;j<replayTriggers.size();j++)if(replayTriggers[j].id==replayStack.back()){fire(j,false,in);break;}}
  replayActive.erase(replayActive.begin()+std::ptrdiff_t(k));
 }
 for(uint32_t i:entered){replayActive.push_back({i,true});if(replayStack.size()<6)replayStack.push_back(replayTriggers[i].id);fire(i,false,in);}
}
// The cut of 0x161FA0 / 0x1620D0 to the view's camera: the cycled one, or on Web-cam the trigger override, else the preferred.
inline void cut(const OriginalCameraInput& in){
 int type=replayMode,action=-1;
 if(replayMode==0x5D){if(replayOverride){type=replayOverrideType;action=replayOverrideAction;}else type=replayPreferred;}
 activate(type,action,in);replayRevertTimer=0;
}
inline void step(const OriginalCameraInput& in,const Quad& position,float mx,float my,float mz){
 Algorithm& g=replayAlgorithm;
 if(chaseType(g.type)){stepAlgorithm(g.chase,in);g.eye=g.chase.outputEye;g.look=g.chase.lookAt;g.fov=g.chase.fov;g.near=g.chase.near;g.far=g.chase.far;}
 else if(g.type==0x20||(g.type>=0x12&&g.type<=0x19)){
  stepAlgorithm(g.chase,in);Quad eye=g.chase.outputEye,look=g.chase.lookAt;Quad d=sub3(look,eye);const float L=len3(d);
  if(g.type==0x20&&L>0){d=mul3(d,(L+294.93588f)/L);eye=add3(eye,d);const float dd=dot3(d,d);const float t=dd>0?dot3(sub3(in.headPosition,eye),d)/dd:0;look=add3(eye,mul3(d,t));} // Look Back 0x168940 flip
  else if(g.type!=0x20){const float dz=d[2];const float h=std::sqrt(d[0]*d[0]+d[1]*d[1]);const float a=float(g.type-0x12)*quarterPi;if(h>0)eye=sub3(look,Quad{h*std::cos(a),h*std::sin(a),dz,0},1);} // Direction 0x165540
  g.eye=eye;g.eye[3]=1;g.look=look;g.look[3]=1;g.fov=g.chase.fov;
 }
 else if(g.type==0x5B&&g.action>=0&&g.action<int(replayTriggers.size()*2)){const auto& t=replayTriggers[size_t(g.action/2)];boundedUpdate(g,g.action&1?t.exit:t.enter,in);}
 else if(g.type>=0x5E&&g.type<=0x61)relativeUpdate(g,in);
 else if(g.type==0x0B)manualUpdate(g,in,mx,my,mz);
 else if(g.type==7)idleUpdate(g,in);
 else if(g.type==0xD)lazyUpdate(g,in);
 else if(g.type==0x42)spokeUpdate(g,in,position);
}
inline float* output(const OriginalCameraInput& in){
 OriginalCameraInput calm=in;calm.visualRandom=nullptr;calm.raceStateSuppressesShake=true; // the replay view never draws the visual stream
 replayCompositor.eye=replayAlgorithm.eye;replayCompositor.lookAt=replayAlgorithm.look;replayCompositor.eye[3]=1;replayCompositor.lookAt[3]=1;
 const auto out=compositeCamera(replayCompositor,calm,replayAlgorithm.fov,replayAlgorithm.near,replayAlgorithm.far);const auto view=nativeCameraView(out);
 for(int i=0;i<3;i++){replayOut[i]=view.eye[i];replayOut[i+3]=view.lookAt[i];}replayOut[6]=view.fovRadians;replayOut[7]=view.nearMeters;replayOut[8]=view.farMeters;replayOut[9]=float(replayCuts);return replayOut;
}
}
extern "C" {
// A replay starts (0x26F8A0 -> 0x26EE68): the view on `mode` (0x5D Web-cam), the preferred camera the Pause Options one,
// the trigger lists empty; the cut comes with the first step. The view's outer camera is the one the game drew with: after a
// live run its terrain lift decays on from the run's last frame (PS2 bhp1-neutral replay ticks 1..40), a replay started from
// another replay keeps the replay's (fromLive 0).
EMSCRIPTEN_KEEPALIVE void replay_camera_begin(int mode,int fromLive){replayMode=mode;replayPreferred=cameraViewType;replayOverride=false;replayOverrideAction=-1;replayActive.clear();replayStack.clear();replayCutPending=true;replayRevertTimer=0;
 if(fromLive)replayCompositor=chase.compositor;}
// The loop's rewind / a seek (0x1620D0): the override is dropped and the view cuts to its camera; the trigger lists stay.
EMSCRIPTEN_KEEPALIVE void replay_camera_rewind(){replayOverride=false;replayCutPending=true;replayRevertTimer=0;}
// ReplayCycleCamera (0x26ED98 -> 0x26EDD8 -> 0x161FA0): the cycled camera, cut at once.
EMSCRIPTEN_KEEPALIVE void replay_camera_mode(int mode){replayMode=mode;replayCutPending=true;}
// After this tick's step_camera_head (the same input): the triggers (a moving human outside a countdown / reset / handplant),
// the 10 s return to the preferred camera, the view's algorithm and the compositor. paused: the view only (the Manual orbit,
// a camera change). mx / my / mz: the Manual camera's sticks (left x / y, right x) in [-1, 1]. Returns [eye 3, look-at 3, fov,
// near, far (native metres, as step_camera_head), cuts (a new count: the view cut, no interpolation from the last pose)].
EMSCRIPTEN_KEEPALIVE float* replay_camera_step(int paused,float mx,float my,float mz){
 if(!cameraLastInputSet)return replayOut;
 const OriginalCameraInput& in=cameraLastInput;original_camera::Rounding rounding;
 const OriginalCameraQuad position{physicsState.position[0],physicsState.position[1],physicsState.position[2],1};
 // a cut (start, loop, seek, camera cycle) steps the new camera once, paused or not (0x26FA78 runs cGameViewMan_updateAll on a
 // ReplayCycleCamera while paused)
 if(replayCutPending){replayCutPending=false;replay_camera::cut(in);replay_camera::step(in,position,mx,my,mz);return replay_camera::output(in);}
 if(!paused){
  if(in.motionMode!=3&&in.motionMode!=5&&!replayTriggers.empty())replay_camera::triggers(position,in);
  // 0x161D80: on Web-cam, 10 s on other cameras than the preferred one (triggers or not) cut back to it; the timer is cleared
  // on the preferred camera, off Web-cam and on the first frame
  if(replayMode==0x5D&&replayAlgorithm.type!=replayPreferred){replayRevertTimer=original_camera::eadd(replayRevertTimer,std::bit_cast<float>(0x3C888889u)); /*gp-0x6B24, EE add.s (chop): the 601st tick (PS2 bhp1-neutral replay)*/if(10.f<replayRevertTimer){replayRevertTimer=0;replay_camera::request(replayPreferred,-1,in);}}
  else replayRevertTimer=0;
  replay_camera::step(in,position,mx,my,mz);
 } else if(replayAlgorithm.type==0x0B)replay_camera::step(in,position,mx,my,mz);
 else return replayOut; // paused: the view holds
 return replay_camera::output(in);
}
// QA: [algorithm type, view mode, override, override type, triggers, active, stack size, revert timer, fired, last trigger id].
EMSCRIPTEN_KEEPALIVE float* replay_camera_info(){RIDER_LOCAL static float v[10];v[0]=float(replayAlgorithm.type);v[1]=float(replayMode);v[2]=replayOverride;v[3]=float(replayOverrideType);
 v[4]=float(replayTriggers.size());v[5]=float(replayActive.size());v[6]=float(replayStack.size());v[7]=replayRevertTimer;v[8]=float(replayFired);v[9]=float(int32_t(replayLastTrigger));return v;}
}
