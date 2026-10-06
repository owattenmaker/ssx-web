#include "ground_motion.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <cfenv>
#include <cmath>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {
namespace {
using Rounding=OriginalRounding;
constexpr float f(uint32_t bits){return std::bit_cast<float>(bits);}
float curve(float x,const GroundCurve& p) {
    unsigned at;
    if (p[1].x<x) {
        if (p[2].x<x) {if (p[3].x<x)return p[3].y;at=2;}
        else at=1;
    } else {if (x<p[0].x)return p[0].y;at=0;}
    float offset=originalScalarSubtract(x,p[at].x),span=originalScalarSubtract(p[at+1].x,p[at].x);
    float delta=originalScalarSubtract(p[at+1].y,p[at].y);
    delta=terrain_original::mul(delta,offset);delta=originalScalarDivide(delta,span);
    return originalScalarAdd(p[at].y,delta);
}
}
GroundNormalResult groundNormalResponse(const GroundSurface& s,const GroundForceState& r) {
    Rounding rounding;
    float distance=r.distance,acceleration;
    if (0<distance) {
        float value=terrain_original::mul(distance,f(0xbd08882fu));acceleration=terrain_original::mul(s.gravity,value);
        if (0<r.normalSpeed) {value=terrain_original::mul(s.powderDamping,r.normalSpeed);acceleration=originalScalarSubtract(acceleration,value);}
    } else if (-r.depth1<distance) {
        float value=-s.gravity;value=terrain_original::mul(value,distance);acceleration=originalScalarDivide(value,r.depth1);
        value=terrain_original::mul(s.powderDamping,r.normalSpeed);acceleration=originalScalarSubtract(acceleration,value);
    } else {
        float span=originalScalarSubtract(r.depth3,r.depth1);
        distance=std::max(-r.depth3,distance);
        float damping=terrain_original::mul(s.powderDamping,r.normalSpeed);
        float value=originalScalarAdd(distance,r.depth1);value=originalScalarAdd(value,value);value=originalScalarDivide(value,span);
        value=originalScalarSubtract(1.f,value);acceleration=terrain_original::mul(s.gravity,value);acceleration=originalScalarSubtract(acceleration,damping);
    }
    float compression=-distance;compression=originalScalarDivide(compression,r.depth3);compression=std::max(compression,0.f);
    return {acceleration,compression};
}
float groundForwardFriction(const GroundSurface& s,const GroundForceState& r,float speedStat,float edgeStat) {
    Rounding rounding;
    float brake=std::abs(r.brake);
    float inverse=terrain_original::mul(speedStat,f(0x3ea38a3cu));inverse=originalScalarAdd(inverse,1.f);inverse=originalScalarDivide(1.f,inverse);
    float curveSpeed=terrain_original::mul(std::abs(r.forwardSpeed),f(0x3d1374bcu));
    float edgeFactor=terrain_original::mul(edgeStat,f(0x3eeca3c4u));edgeFactor=originalScalarAdd(edgeFactor,1.f);
    float slip=curve(curveSpeed,s.slipFriction);
    float speedBlend=originalScalarDivide(std::abs(r.forwardSpeed),f(0x44d05556u));speedBlend=std::clamp(speedBlend,0.f,1.f);
    float value=originalScalarSubtract(1.f,speedBlend);value=terrain_original::mul(inverse,value);
    // 0x13CE90..0x13CEBC: f22 = 1.0 (0x13CD20) multiplies the blend before the add and the sum after it (mul.s f1,f22,f1; mul.s f0,f22,f0)
    value=originalScalarAdd(value,terrain_original::mul(1.f,speedBlend));value=terrain_original::mul(1.f,value);
    slip=terrain_original::mul(slip,value);
    float slope=0;
    if (f(0x3dcccccdu)<=r.normalZ) {
        if (r.normalZ<=f(0x3e4ccccdu)) {slope=originalScalarSubtract(r.normalZ,f(0x3dcccccdu));slope=originalScalarDivide(slope,f(0x3dcccccdu));}
        else slope=1;
    }
    float braking=terrain_original::mul(brake,f(0x404006e5u));braking=terrain_original::mul(slope,braking);braking=terrain_original::mul(braking,brake);braking=terrain_original::mul(braking,edgeFactor);
    float brakeOffset=terrain_original::mul(brake,f(0x4479fd71u));brakeOffset=terrain_original::mul(slope,brakeOffset);
    float depthFactor=1;
    if (s.id==2||s.id==3||s.id==13) {depthFactor=originalScalarDivide(r.distance,r.depth3);depthFactor=originalScalarSubtract(.5f,depthFactor);}
    float rising=r.relativeVerticalSpeed>0?f(0x3f1996deu):1.f;
    float stateFactor=1;
    if (!r.state320Equals324 && r.boost==0) stateFactor=f(0x3f8eaf70u);
    float special=(r.forwardSpeed>0&&r.relativeVerticalSpeed>0&&s.id==13)?f(0x420b5da4u):1.f;
    value=-r.forwardSpeed;float friction=originalScalarAdd(slip,braking);
    value=terrain_original::mul(value,depthFactor);value=terrain_original::mul(value,friction);value=terrain_original::mul(value,rising);value=terrain_original::mul(value,stateFactor);
    value=originalScalarSubtract(value,brakeOffset);return terrain_original::mul(value,special);
}
float groundLateralResponse(const GroundSurface& s,const GroundForceState& r,const GroundCurve& speedCurve) {
    Rounding rounding;
    float opposite=-r.turn,lateral=r.lateralSpeed;
    if (lateral<0&&opposite>0) lateral=terrain_original::mul(lateral,opposite);
    else if (lateral>0&&opposite<0) {float factor=-opposite;lateral=terrain_original::mul(lateral,factor);}
    else return 0;
    float speed=terrain_original::mul(std::abs(r.forwardSpeed),f(0x3d1374bcu));
    lateral=terrain_original::mul(lateral,curve(speed,speedCurve));
    float drag=-s.lateralDrag;lateral=terrain_original::mul(lateral,drag);
    if (r.boost>0) {float value=terrain_original::mul(r.boost,f(0x408046c2u));value=originalScalarAdd(value,1.f);value=originalScalarDivide(1.f,value);lateral=terrain_original::mul(lateral,value);}
    return lateral;
}
}

namespace ssx {
namespace {
float atanRaw(float x) {
    float square=terrain_original::mul(x,x);
    float t,root;
    if (x< -1 || x>1) {t=originalScalarAdd(square,1.f);t=originalScalarDivide(1.f,t);root=originalScalarSqrt(t);}
    else {float denominator=originalScalarAdd(square,1.f);t=originalScalarDivide(square,denominator);root=originalScalarSqrt(t);}
    float value=terrain_original::mul(t,f(0x3d6137abu));value=originalScalarAdd(f(0x3d55033au),value);value=terrain_original::mul(t,value);
    value=originalScalarAdd(f(0x3d8a908bu),value);value=terrain_original::mul(t,value);value=originalScalarAdd(f(0x3e2bba25u),value);
    value=terrain_original::mul(t,value);value=originalScalarAdd(value,1.f);value=terrain_original::mul(root,value);
    if (x< -1)return originalScalarAdd(value,f(0xbfc90fdbu));
    if (x>1)return originalScalarSubtract(f(0x3fc90fdbu),value);
    return x<0?-value:value;
}
}
float originalAtan(float x) {Rounding rounding;return atanRaw(x);}
float groundForwardDrive(const GroundDriveState& r) {
    Rounding rounding;
    if (r.brake!=0||r.modeTiming>=0)return 0;
    bool highType=r.riderType>=17;
    float typeScale=highType?f(0x3effff5cu):1.f;
    float headingWeight=0;
    if (r.forceHeadingBoost)headingWeight=1;
    else {
        float angle;
        if (r.forwardX==0) {
            if (r.forwardY==0)angle=0;
            else angle=r.forwardY>=0?f(0x3fc90fdbu):f(0xbfc90fdbu);
        } else {
            float ratio=originalScalarDivide(r.forwardY,r.forwardX);angle=atanRaw(ratio);
            if (r.forwardX<0)angle=r.forwardY>0?originalScalarAdd(angle,f(0x40490fdbu)):originalScalarSubtract(angle,f(0x40490fdbu));
        }
        float difference=originalScalarSubtract(angle,r.headingOffset);
        float revolutions=terrain_original::mul(difference,f(0x3e22f983u));revolutions=originalScalarAdd(revolutions,.5f);
        float rounded=float(int(revolutions));if (revolutions<rounded)rounded=originalScalarSubtract(rounded,1.f);
        float wrapped=terrain_original::mul(rounded,f(0x40c90fdbu));wrapped=originalScalarSubtract(difference,wrapped);wrapped=std::abs(wrapped);
        if (wrapped<f(0x3fc90fdcu)) {float scale=originalScalarDivide(1.f,f(0x3fc90fdcu));scale=terrain_original::mul(wrapped,scale);headingWeight=originalScalarSubtract(1.f,scale);}
    }
    float enabled=headingWeight<=0&&highType?0.f:1.f;
    float boost=terrain_original::mul(r.boost,f(0x3e2e0cf5u));
    if (r.boostWindow>0) {float value=terrain_original::mul(headingWeight,5.5f);boost=std::max(value,boost);}
    float drive=0;
    if (boost>0) {
        float turnMargin=originalScalarSubtract(f(0x3da3d708u),std::abs(r.turn));
        if (turnMargin>0) {
            boost=terrain_original::mul(boost,turnMargin);
            float scale=f(0x4512e000u);
            if (r.forwardZ>=0) {scale=terrain_original::mul(r.forwardZ,f(0x448398cbu));scale=originalScalarAdd(scale,f(0x4512e000u));}
            float value=terrain_original::mul(boost,scale);drive=originalScalarAdd(0.f,value);
        }
    }
    float x2=terrain_original::mul(r.velocity[0],r.velocity[0]),y2=terrain_original::mul(r.velocity[1],r.velocity[1]),z2=terrain_original::mul(r.velocity[2],r.velocity[2]);
    // VU0 horizontal dot (0x113E8C..0x113E9C): z2 goes through vmaddaz.x with the 1.0 vector as fs.
    float squared=terrain_original::add(x2,y2);squared=terrain_original::add(squared,terrain_original::mul(1.f,z2));float speed=terrain_original::sqrt(squared);
    float missing=terrain_original::mul(r.autoBoostSpeed,typeScale);missing=terrain_original::mul(missing,f(0x41de38e4u));missing=originalScalarSubtract(missing,speed);
    missing=std::min(missing,f(0x448ae38eu));
    if (missing>0) {
        float floor=r.animationIndex==22?f(0x3e4ccccdu):0;
        if (r.riderType>=11&&r.riderType<=13)missing=terrain_original::mul(missing,std::abs(r.normalZ));
        floor=terrain_original::mul(floor,f(0x3f800e96u));floor=std::max(headingWeight,floor);floor=terrain_original::mul(floor,missing);drive=originalScalarAdd(drive,floor);
    }
    float result=terrain_original::mul(r.autoBoostFactor,drive);
    float crouch=terrain_original::mul(r.crouch,f(0x4261a6e4u));
    result=terrain_original::mul(result,typeScale);result=originalScalarAdd(result,crouch);return terrain_original::mul(enabled,result); // 0x13CCB4 mul.s f0,f25,f0
}
void groundControlApproach(GroundControlValue& v) {
    Rounding rounding;
    float upper=originalScalarAdd(v.target,v.rate);
    if (upper<v.current)v.current=originalScalarSubtract(v.current,v.rate);
    else {float lower=originalScalarSubtract(v.target,v.rate);if(v.current<lower)v.current=originalScalarAdd(v.current,v.rate);else v.current=v.target;}
}
void groundCrouchBrakeTargets(GroundControlValue& crouch,GroundControlValue& brake,float c,float b,float forwardVelocity,float turn) {
    Rounding rounding;
    if (c>0&&brake.current!=0) {brake.target=0;brake.rate=f(0x3d2aa635u);return;}
    if (b>0&&crouch.current!=0) {crouch.target=0;crouch.rate=f(0x3dcccdc2u);return;}
    if (c>0&&brake.current!=0)c=0;
    float delta=originalScalarSubtract(c,crouch.current);delta=std::abs(delta);
    crouch.rate=delta>=f(0x3dcccccdu)?terrain_original::mul(delta,f(0x3dcccdc2u)):f(0x3c23d7cfu);
    crouch.target=c;
    if (b>0&&(crouch.current>0||forwardVelocity<0))b=0;
    float old=brake.current;
    if (std::abs(old)<=f(0x3dcccccdu)&&turn<0)b=-b;
    else if (old<f(0xbdcccccdu)&&turn<f(0x3dcccccdu))b=-b;
    delta=originalScalarSubtract(b,old);delta=std::abs(delta);
    float rate=f(0x3dcccccdu);if(rate<=delta)rate=std::min(delta,1.f);
    brake.target=b;brake.rate=terrain_original::mul(rate,f(0x3d2aa635u));
}
}

namespace ssx {
namespace {
using V=std::array<float,3>;
V add(V a,V b){for(unsigned i=0;i<3;++i)a[i]=terrain_original::add(a[i],b[i]);return a;}
V sub(V a,V b){for(unsigned i=0;i<3;++i)a[i]=terrain_original::sub(a[i],b[i]);return a;}
V mul(V a,float b){for(float& x:a)x=terrain_original::mul(x,b);return a;}
// VU0 horizontal dot: vadda x + y, vmadda ACC + vf0w (1.0) x z, vmadd ACC + 1.0 x w (w = +0)
float dot3(V a,V b){float x=terrain_original::mul(a[0],b[0]),y=terrain_original::mul(a[1],b[1]),z=terrain_original::mul(a[2],b[2]);float xy=terrain_original::add(x,y);float xyz=terrain_original::add(xy,terrain_original::mul(1.f,z));return terrain_original::add(xyz,0.f);}
V normalize(V a){float length=terrain_original::sqrt(dot3(a,a));float inverse=terrain_original::div(1.f,length);return mul(a,inverse);}
// vopmula a, b then vopmsub b, a (0x13D738): the subtracted products are b x a
V cross3(V a,V b){return {terrain_original::sub(terrain_original::mul(a[1],b[2]),terrain_original::mul(b[1],a[2])),terrain_original::sub(terrain_original::mul(a[2],b[0]),terrain_original::mul(b[2],a[0])),terrain_original::sub(terrain_original::mul(a[0],b[1]),terrain_original::mul(b[0],a[1]))};}
std::array<float,2> sincosRaw(float x) {
    float scaled=terrain_original::mul(x,f(0x3f22f983u));scaled=x<0?originalScalarSubtract(scaled,.5f):originalScalarAdd(scaled,.5f);
    int quadrant=int(scaled);float offset=terrain_original::mul(float(quadrant),f(0x3fc90fdbu));x=originalScalarSubtract(x,offset);
    float square=terrain_original::mul(x,x);float value=terrain_original::mul(square,f(0x3638ef1fu));value=originalScalarAdd(value,f(0xb9500d03u));
    value=terrain_original::mul(value,square);value=originalScalarAdd(value,f(0x3c088889u));value=terrain_original::mul(value,square);value=originalScalarAdd(value,f(0xbe2aaaabu));
    value=terrain_original::mul(value,square);value=originalScalarAdd(value,1.f);float sine=terrain_original::mul(value,x);
    value=terrain_original::mul(sine,sine);value=originalScalarSubtract(1.f,value);float cosine=originalScalarSqrt(value);
    switch(quadrant&3){case 0:return {sine,cosine};case 1:return {cosine,-sine};case 2:return {-sine,-cosine};default:return {-cosine,sine};}
}
}
std::array<float,2> originalSinCos(float x){Rounding rounding;return sincosRaw(x);}
OriginalGroundDiagnostics originalGroundIntegrate(const OriginalGroundProfile& p,OriginalGroundState& s) {
    Rounding rounding;
    float speed=terrain_original::sqrt(dot3(s.velocity,s.velocity));
    if(p.speedLimit<speed)s.velocity=mul(s.velocity,originalScalarDivide(p.speedLimit,speed));
    float dt=terrain_original::mul(s.timeScale,f(0x3c888889u));
    float rate=terrain_original::mul(dt,100.f);
    GroundControlValue d1{s.depth1,rate,terrain_original::mul(p.bodyScale,p.depthTarget1)},d3{s.depth3,rate,terrain_original::mul(p.bodyScale,p.depthTarget3)};
    groundControlApproach(d1);groundControlApproach(d3);s.depth1=d1.current;s.depth3=d3.current;
    V relative=sub(s.velocity,s.surfaceVelocity);
    GroundForceState force;
    force.depth1=s.depth1;force.depth3=s.depth3;force.distance=s.distance;
    force.normalSpeed=dot3(relative,s.normal);force.forwardSpeed=dot3(relative,s.forward);force.lateralSpeed=dot3(relative,s.lateral);
    force.normalZ=s.normal[2];force.relativeVerticalSpeed=relative[2];force.brake=s.brake.current;force.turn=s.turn.current;
    force.boost=s.boost;force.state320Equals324=s.state320Equals324;
    auto response=groundNormalResponse(p.surface,force);
    float normalAcceleration=response.acceleration;
    if(s.normal[2]<0&&normalAcceleration<0)normalAcceleration=0;
    GroundDriveState drive;
    drive.velocity=s.velocity;drive.forwardX=s.physicalForward[0];drive.forwardY=s.physicalForward[1];drive.headingOffset=s.headingOffset;
    drive.forwardZ=s.forward[2];drive.normalZ=s.normal[2];drive.brake=s.brake.current;drive.turn=s.turn.current;
    drive.crouch=s.crouch.current;drive.boost=s.boost;drive.boostWindow=s.boostWindow;drive.modeTiming=s.modeTiming;
    drive.autoBoostSpeed=p.autoBoostSpeed;drive.autoBoostFactor=p.autoBoostFactor;drive.riderType=s.riderType;
    drive.animationIndex=s.animationIndex;drive.forceHeadingBoost=s.forceHeadingBoost;
    float friction=groundForwardFriction(p.surface,force,p.speedStat,p.edgeStat);
    float forwardDrive=groundForwardDrive(drive);
    float lateral=groundLateralResponse(p.surface,force,p.lateralSpeedCurve);
    float speedKmh=terrain_original::mul(speed,f(0x3d1374bcu));
    float low=curve(speedKmh,p.turnMinCurve),high=curve(speedKmh,p.turnMaxCurve);
    float difference=originalScalarSubtract(high,low);float blended=terrain_original::mul(p.edgeStat,difference);blended=originalScalarAdd(blended,difference);blended=originalScalarAdd(low,blended);
    float angle=terrain_original::mul(p.maxTurnAngle,blended);angle=terrain_original::mul(angle,f(0x3c8efa36u));angle=terrain_original::mul(angle,(-s.turn.current));
    auto sc=sincosRaw(angle);
    V leaning=add(mul(s.normal,sc[1]),mul(s.lateral,sc[0]));
    float twiceGravity=originalScalarAdd(p.surface.gravity,p.surface.gravity);
    float rotated=std::min(normalAcceleration,twiceGravity);rotated=originalScalarDivide(rotated,sc[1]);
    float remaining=originalScalarSubtract(normalAcceleration,rotated);
    V acceleration=add(mul(s.normal,remaining),mul(leaning,rotated));
    acceleration=add(acceleration,mul(s.forward,originalScalarAdd(friction,forwardDrive)));
    acceleration=add(acceleration,mul(s.lateral,lateral));
    acceleration=add(acceleration,{0,0,-p.surface.gravity});
    if(!s.flags308)acceleration=add(acceleration,mul(s.normal,-1000.f));
    float correction=originalScalarAdd(s.depth3,s.distance);correction=std::max(correction,f(0xc1200001u));
    if(correction<0) {
        s.position=sub(s.position,mul(s.normal,correction));s.distance=originalScalarSubtract(s.distance,correction);
        if(force.normalSpeed<0) {relative=sub(relative,mul(s.normal,force.normalSpeed));s.velocity=add(relative,s.surfaceVelocity);}
        float normalPart=dot3(acceleration,s.normal);
        if(normalPart<0)acceleration=sub(acceleration,mul(s.normal,normalPart));
    }
    if(!s.flags308&&s.distance>=0) {s.position=sub(s.position,mul(s.normal,s.distance));s.distance=0;}
    float relativeSquared=dot3(relative,relative);
    if(std::abs(s.brake.current)>.5f&&relativeSquared<f(0x48742aa9u)&&s.normal[2]>f(0x3f5db8bbu)) {
        relative=mul(relative,relativeSquared<2.f?0.f:f(0x3f666666u));
        s.velocity=add(relative,s.surfaceVelocity);acceleration={0,0,0};
    }
    float poseReferenceHeight=dot3(s.position,s.boardNormal);
    s.position=add(s.position,mul(s.velocity,dt));
    s.velocity=add(s.velocity,mul(acceleration,dt));
    return {acceleration,sub(s.velocity,s.surfaceVelocity),dt,normalAcceleration,friction,forwardDrive,lateral,response.compression,leaning,s.boardNormal,poseReferenceHeight};
}
void originalGroundContact(OriginalGroundState& s,const V& point,const V& normal,const V& surfaceVelocity) {
    Rounding rounding;
    s.normal=normal;s.surfaceVelocity=surfaceVelocity;
    float projection=dot3(s.physicalForward,normal);
    s.forward=normalize(sub(s.physicalForward,mul(normal,projection)));
    s.lateral=cross3(normal,s.forward);
    s.distance=dot3(sub(s.position,point),normal);
}
}
namespace ssx {
void groundTurnTarget(GroundControlValue& turn,float input,const std::array<float,3>& velocity,int surfaceId) {
    Rounding rounding;
    float speed=terrain_original::sqrt(dot3(velocity,velocity));speed=originalScalarDivide(speed,f(0x442da27cu));
    if(speed<1.f)input=terrain_original::mul(input,speed);
    float delta=originalScalarSubtract(input,turn.current);delta=std::abs(delta);delta=terrain_original::mul(delta,f(0x414009d8u));
    float rate=std::clamp(delta,f(0x3dcccccdu),f(0x416004f4u));
    if(surfaceId==2||surfaceId==3||surfaceId==13)rate=terrain_original::mul(rate,f(0x3f4191ffu));
    turn.target=input;turn.rate=terrain_original::mul(rate,f(0x3c888889u));
}
}
namespace ssx {
void originalGroundVelocityContact(OriginalGroundState& s,int surfaceId,const std::array<float,3>& previousSurfaceVelocity) {
    Rounding rounding;
    if(surfaceId==2||surfaceId==3||surfaceId==13)return;
    auto relative=sub(s.velocity,previousSurfaceVelocity);
    float normalSpeed=dot3(relative,s.normal),speed=terrain_original::sqrt(dot3(relative,relative));
    normalSpeed=terrain_original::mul(normalSpeed,f(0xbecccccdu));
    auto correction=mul(s.normal,normalSpeed);correction[2]=std::max(correction[2],-40.f);
    relative=add(relative,correction);
    float correctedSpeed=terrain_original::sqrt(dot3(relative,relative));
    if(correctedSpeed!=0)relative=mul(relative,originalScalarDivide(speed,correctedSpeed));
    s.velocity=add(s.surfaceVelocity,relative);
}
}
namespace ssx {
float originalGroundSpeedLimit(const OriginalGroundProfile& p,const OriginalGroundState& s,int motionMode) {
    Rounding rounding;
    float stat=originalScalarAdd(p.topSpeedStat,1.f);stat=originalScalarSubtract(stat,1.f);
    float terminal=motionMode?1.f:p.surfaceTerminalVelocity;
    unsigned band=terminal>0?1:0;
    if(!band)terminal=originalScalarAdd(terminal,1.f);
    unsigned boost=0;
    if(s.boost>=f(0x3dcccccdu)) {boost=1;if(s.boost>=f(0x3ecccccdu))boost=s.boost<f(0x3f4ccccdu)?2:3;}
    float crouch=motionMode?1.f:s.crouch.current;
    float standing=originalScalarSubtract(1.f,crouch),lowSurface=originalScalarSubtract(1.f,terminal);
    float wHighCrouch=terrain_original::mul(terminal,crouch),wHighStand=terrain_original::mul(terminal,standing);
    float wLowCrouch=terrain_original::mul(lowSurface,crouch),wLowStand=terrain_original::mul(lowSurface,standing);
    float limits[2];
    for(unsigned statEndpoint=0;statEndpoint<2;++statEndpoint) {
        unsigned low=statEndpoint*24+band*8+boost*2,high=low+8;
        float a=terrain_original::mul(wLowStand,p.speedLimitTable[low]),b=terrain_original::mul(wHighStand,p.speedLimitTable[high]);
        float c=terrain_original::mul(wLowCrouch,p.speedLimitTable[low+1]),d=terrain_original::mul(wHighCrouch,p.speedLimitTable[high+1]);
        float result=originalScalarAdd(a,b);result=originalScalarAdd(result,c);limits[statEndpoint]=originalScalarAdd(result,d);
    }
    float result;
    if(motionMode==1)result=f(0x45505556u);
    else {
        float lo=originalScalarSubtract(1.f,stat);lo=terrain_original::mul(lo,limits[0]);float hi=terrain_original::mul(stat,limits[1]);result=originalScalarAdd(hi,lo);result=terrain_original::mul(result,f(0x41de38e4u));
        if(!s.state320Equals324&&s.boost<=0)result=terrain_original::mul(result,f(0x3f7ae148u));
        float retain=result<p.speedLimit?f(0x3f7851ecu):f(0x3f666666u);
        float previous=terrain_original::mul(retain,p.speedLimit),next=originalScalarSubtract(1.f,retain);next=terrain_original::mul(next,result);result=originalScalarAdd(next,previous);
    }
    if(s.boostTierCounter>=11&&s.boost>0)result=std::max(s.boostSpeedFloor,result);
    return result;
}
}
namespace ssx {
void originalGroundClampSpeed(OriginalGroundState& s,float maximumSpeed) {
    Rounding rounding;
    float speed=terrain_original::sqrt(dot3(s.velocity,s.velocity));
    if(maximumSpeed<speed)s.velocity=mul(s.velocity,originalScalarDivide(maximumSpeed,speed));
}
}
