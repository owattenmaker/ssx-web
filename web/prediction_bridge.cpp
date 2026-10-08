#include "rider_local.hpp"
#include "../engine/air_trajectory_world.hpp"
#include <emscripten/emscripten.h>
#include <bit>
using namespace ssx;
extern std::unique_ptr<CollisionWorld> cameraTerrain;
RIDER_LOCAL extern std::unique_ptr<WorldBodyCollision> browserBodies;
RIDER_LOCAL OriginalAirTrajectory browserTrajectory;
RIDER_LOCAL bool browserPredictionAvailable=false;
RIDER_LOCAL static bool predictionFailed=false,trajectorySeeded=false;RIDER_LOCAL static OriginalAirState lastIntegrated;
void reset_prediction(){trajectorySeeded=false;browserTrajectory={};browserPredictionAvailable=false;predictionFailed=false;}
void begin_prediction(OriginalAirState current){trajectorySeeded=true;browserTrajectory.begin(current);browserPredictionAvailable=false;predictionFailed=false;}
void reseed_prediction(OriginalAirState current){if(trajectorySeeded)browserTrajectory.reseed(current);else{browserTrajectory.begin(current);trajectorySeeded=true;}browserPredictionAvailable=false;}
// scaled: 0x139A20's step, seconds = mul.s(rider+0x300 time scale 1.0 as fs, 1/60) at 0x139A58.
// Not scaled: the handplant motion-5 steps pass the constant 1/60 straight in (0x13940C gp-0x71FC, 0x1394F4 gp-0x71F4).
// The two differ only on the console model, where 1.0 as fs gives 3C888888 (docs/ps2-float.md); mode 1 gives 3C888889 for both.
static bool advance_prediction_by(OriginalAirState& current,bool scaled,float timeScale=1.f){
 if(predictionFailed)return false;
 if(!trajectorySeeded)begin_prediction(current);
 if(!cameraTerrain||!browserBodies){predictionFailed=true;browserPredictionAvailable=false;return false;}
 auto candidate=browserTrajectory;
 try{
  //Original113648/113200 prediction, with original mode0/2 world queries.
  //Publish the same integrated state used by original139A20, exactly once.
  auto query=[](auto end,auto start,int mode){return queryOriginalAirTrajectoryWorld(*cameraTerrain,browserBodies.get(),end,start,mode);};
  auto next=scaled?candidate.stepLogic(timeScale,current,query):candidate.step(std::bit_cast<float>(0x3c888889u),current,query);
  browserTrajectory=std::move(candidate);browserPredictionAvailable=true;current=lastIntegrated=next;return true;
 }catch(const OriginalAirTrajectoryUnavailable&){predictionFailed=true;browserPredictionAvailable=false;return false;}
}
bool advance_prediction(OriginalAirState& current){
 return advance_prediction_by(current,true);
}
// 139A20 with the rider's +0x300 time scale (0x139A58 mul.s(+0x300, 1/60)): a computer rider's NPC provider (120090) writes it below 1
// (PS2 TAS best-8964 5944: computer rider 4 at 0.9675 leaves the ground; 113648 elapsed 0.016125, the port stepped 1/60).
bool advance_prediction_scaled(OriginalAirState& current,float timeScale){
 return advance_prediction_by(current,true,timeScale);
}
bool advance_prediction_unscaled(OriginalAirState& current){
 return advance_prediction_by(current,false);
}
extern "C" EMSCRIPTEN_KEEPALIVE float* prediction_info(){
 RIDER_LOCAL static float result[19];result[0]=browserPredictionAvailable;result[1]=browserTrajectory.status;
 result[2]=browserTrajectory.predictedTime;result[3]=browserTrajectory.elapsed;
 result[4]=browserTrajectory.predictedTime-browserTrajectory.elapsed;result[5]=browserTrajectory.surface;result[6]=predictionFailed;for(unsigned i=0;i<3;i++){result[7+i]=browserTrajectory.heading[i];result[10+i]=browserTrajectory.normal[i];result[13+i]=lastIntegrated.position[i];result[16+i]=lastIntegrated.velocity[i];}return result;
}

void sync_crash_prediction(const ssx::OriginalAirTrajectory& trajectory,ssx::OriginalAirState current,bool airborne){browserTrajectory=trajectory;lastIntegrated=current;trajectorySeeded=true;browserPredictionAvailable=airborne;predictionFailed=false;}

// 0x114DB8 (in-flight stance switch) runs in the control-5 update before air motion, but the browser
// runs that controller after this tick's translation: keep the predictor as it was before motion so a
// switch can restart the flight (0x1135B8) from the pre-motion state and translate again.
struct AirMotionStartPrediction {OriginalAirTrajectory trajectory;OriginalAirState last;bool available=false,failed=false,seeded=false;};
RIDER_LOCAL static AirMotionStartPrediction airMotionStartPrediction;
void save_air_motion_prediction(){airMotionStartPrediction={browserTrajectory,lastIntegrated,browserPredictionAvailable,predictionFailed,trajectorySeeded};}
void restore_air_motion_prediction(){const auto& s=airMotionStartPrediction;browserTrajectory=s.trajectory;lastIntegrated=s.last;browserPredictionAvailable=s.available;predictionFailed=s.failed;trajectorySeeded=s.seeded;}
const OriginalAirTrajectory* air_motion_start_trajectory(){return airMotionStartPrediction.available?&airMotionStartPrediction.trajectory:nullptr;}
// QA: the rider's air predictor (rider+0x788) in the crash_trajectory_info layout: hit xyz, heading xyz,
// normal xyz, patch, prediction pos/vel, integrated pos/vel, predicted/apex/elapsed/integrated time, limit, status.
extern "C" EMSCRIPTEN_KEEPALIVE float* rider_trajectory_info(){RIDER_LOCAL static float v[32];const auto& t=browserTrajectory;unsigned n=0;auto put=[&](const std::array<float,3>& a){for(float x:a)v[n++]=x;};
 put(t.hitPosition);put(t.heading);put(t.normal);v[n++]=float(t.patchId);put(t.prediction.position);put(t.prediction.velocity);put(t.integrated.position);put(t.integrated.velocity);
 v[n++]=t.predictedTime;v[n++]=t.apexTime;v[n++]=t.elapsed;v[n++]=t.integratedTime;v[n++]=t.speedLimit;v[n++]=float(t.status);return v;}
#ifdef SSX_SNAPSHOT_REGISTRY // the rider-context snapshot's registry (web/generate-snapshot-registry.mjs, docs/replay.md §2a)
#include "generated/snapshot/prediction_bridge.inc"
#endif
