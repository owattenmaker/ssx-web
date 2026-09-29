#pragma once
#include <array>
#include <cstdint>

namespace ssx {
struct GroundCurvePoint {float x=0,y=0;};
using GroundCurve=std::array<GroundCurvePoint,4>;
struct OriginalHeadingProfile {
    float surface28=0,surface2C=0,surface30=0,surface34=0;
    GroundCurve crouchTurnCurve,crouchSpeedCurve,directSteerCurve;
};
struct GroundSurface {
    float gravity=0;          // Original surface+0, centimeters/second².
    float powderDamping=0;   // Surface+0x1C.
    float lateralDrag=0;     // Surface+8.
    GroundCurve slipFriction; // Surface+0x90..0xAC, speed in km/h.
    int id=0;                // Rider+0x438.
};
struct GroundForceState {
    float depth1=0,depth3=0;  // Smoothed controller+4/+8, centimeters.
    float distance=0;        // Rider+0x454, signed centimeters.
    float normalSpeed=0,forwardSpeed=0,lateralSpeed=0; // Relative to moving surface, cm/s.
    float normalZ=1;         // Original Z-up contact normal component.
    float relativeVerticalSpeed=0; // Rider+0x1E8 minus rider+0x3D8.
    float brake=0;           // Effective brake, rider+0x214 (not raw button/stick).
    float turn=0;            // Rider+0x1F0.
    float boost=0;           // Rider+0x2FC.
    bool state320Equals324=true; // Original state comparison; semantic name unverified.
};
struct GroundNormalResult {float acceleration=0,compression=0;};
// Exact arithmetic recovered from the named original ranges. Functions scope
// roundtowardzero, preserving source-float operation order and caller rounding.
GroundNormalResult groundNormalResponse(const GroundSurface&,const GroundForceState&); //0x13C878
float groundForwardFriction(const GroundSurface&,const GroundForceState&,float stat1493D8,float stat148D80); //0x13CCF0
float groundLateralResponse(const GroundSurface&,const GroundForceState&,const GroundCurve& lateralSpeedCurve); //0x13D028
}

namespace ssx {
struct GroundDriveState {
    std::array<float,3> velocity{};
    float forwardX=0,forwardY=1,headingOffset=0;
    float forwardZ=0,normalZ=1;
    float brake=0,turn=0,crouch=0,boost=0,boostWindow=0,modeTiming=-1;
    float autoBoostSpeed=0,autoBoostFactor=0;
    int riderType=0,animationIndex=0;
    bool forceHeadingBoost=false;
};
float groundForwardDrive(const GroundDriveState&); //0x13C948
float originalAtan(float); //0x31C228.
struct GroundControlValue {float current=0,rate=0,target=0;};
void groundCrouchBrakeTargets(GroundControlValue& crouch,GroundControlValue& brake,float crouchInput,float brakeInput,float forwardVelocity,float turn); //0x113F88
void groundControlApproach(GroundControlValue&); //0x1211F8.
}

namespace ssx {
// All vectors below are original Z-up float centimeters/cm-per-second. Profile
// values come from owned asset/snapshot data; zero-initialization is not a tuning preset.
struct OriginalGroundProfile {
    GroundSurface surface;
    OriginalHeadingProfile headingProfile;
    GroundCurve lateralSpeedCurve,turnMinCurve,turnMaxCurve,extraLeanCurve;
    float depthTarget1=0,depthTarget3=0,maxTurnAngle=0;
    float autoBoostSpeed=0,autoBoostFactor=0;
    float bodyScale=0,speedLimit=0,speedStat=0,edgeStat=0,alignmentRate=0;
    std::array<float,48> speedLimitTable{};
    float topSpeedStat=0,surfaceTerminalVelocity=0,airHeight=0;
};
struct OriginalGroundState {
    std::array<float,3> position{},velocity{},normal{},forward{},lateral{},surfaceVelocity{},physicalForward{},previousNormal{};
    std::array<float,4> quaternion{0,0,0,1};
    std::array<float,3> boardUp{0,0,1},boardNormal{0,0,1},presentationUp{0,0,1};
    GroundControlValue turn,brake,crouch,presentationLift,animationTurn,extraLean,boardAlignment,presentationRoll,balance280,adjustment28C,adjustment298;
    float depth1=0,depth3=0,distance=0,timeScale=1;
    float boost=0,boostWindow=0,modeTiming=-1,headingOffset=0,contactClearance=0,manualSpin=0;
    float boardBouncePhase=0,boardLift=0; // Controller+0 and rider+31C, respectively.
    int controlState=0;bool reverseStance=false;
    int boostTierCounter=0;float boostSpeedFloor=0;
    uint32_t flags308=0;
    int riderType=0,animationIndex=0,animationClass=0,prewindStyle=0;
    bool animationSelectionSupported=true;
    bool forceHeadingBoost=false,state320Equals324=true;
};
struct OriginalGroundDiagnostics {
    std::array<float,3> acceleration{},relativeVelocity{};
    float stepTime=0;
    float normalAcceleration=0,forwardFriction=0,forwardDrive=0,lateralAcceleration=0,compression=0;
    std::array<float,3> leaningNormal{}; // Original rider+390, before contact query.
    std::array<float,3> boardNormalForPose{};
    float poseReferenceHeight=0; // dot(corrected position,leaningNormal) before movement.
};
OriginalGroundDiagnostics originalGroundIntegrate(const OriginalGroundProfile&,OriginalGroundState&);
void originalGroundContact(OriginalGroundState&,const std::array<float,3>& point,const std::array<float,3>& normal,const std::array<float,3>& surfaceVelocity);
std::array<float,2> originalSinCos(float radians);
}
namespace ssx { void groundTurnTarget(GroundControlValue&,float input,const std::array<float,3>& velocity,int surfaceId); }

namespace ssx {void originalGroundVelocityContact(OriginalGroundState&,int surfaceId,const std::array<float,3>& previousSurfaceVelocity);}

namespace ssx {float originalGroundSpeedLimit(const OriginalGroundProfile&,const OriginalGroundState&,int motionMode=0);}
namespace ssx {void originalGroundClampSpeed(OriginalGroundState&,float maximumSpeed);}
