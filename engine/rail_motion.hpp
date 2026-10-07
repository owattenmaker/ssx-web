#pragma once
// Original SSX3 (PS2 USA, SLUS_207.72) grind-rail system, recovered from the
// executable and the SSB course container. See RAIL_RECOVERY.md for addresses,
// evidence, confidence and the integration plan. Source units are Z-up
// centimeters and cm/s with XYZW quaternions. EE scalar ADD/SUB keep the
// alignment guard bit, EE DIV/SQRT round to nearest, VU vector arithmetic
// rounds toward zero (terrain_original helpers under Rounding).
//
// Recovered: spline query 0x335128/0x334680 (authored spline layers only),
// attach test 0x108A48+0x1086B8, attach 0x106848, rail motion4
// 0x13AD20/0x13ADC0/0x13BD80/0x13AF28, motion exit 0x13BFA8 (decision part),
// rail control7 0x131D08/0x131D30/0x132048, rotation 0x132060, airborne exit
// 0x132770, entry animation 0x1326C8, stance alignment 0x115358/0x115168 and
// steer target 0x113F38. Unrecovered branches throw std::runtime_error.
#include "terrain_contact_math.hpp"
#include "world_residency.hpp"
#include "collision_scalar.hpp"
#include "ground_motion.hpp"
#include "orientation_motion.hpp"
#include "air_alignment.hpp"
#include "original_spatial.hpp"
#include <algorithm>
#include <bit>
#include <cstdint>
#include <functional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace ssx {
using RailVector=terrain_original::Vector;

// ---------------------------------------------------------------------------
// Authored data: SSB record kind8 = one rail; 48-byte header then 144-byte
// segments. Each segment is the runtime type-1 collision object that
// 0x334680 walks (rows +0x10..+0x40, bounds +0x6C..+0x80, descriptor +0x68).
struct OriginalRailSegment {
    std::array<RailVector,4> coefficients{}; // rows for t^3,t^2,t,1 (file w components 0,0,0,1)
    RailVector boundsMin{},boundsMax{};      // +0x6C / +0x78
    float length=0;                           // +0x0C arc length, cm
    float distance=0;                         // +0x84 cumulative distance at t=0, cm
    int32_t index=0,previous=-1,next=-1;     // +0x60/+0x64 global segment indices
    uint32_t flags=15;                        // +0x8C (15 in every shipped course)
    std::array<float,4> arcToParameter{};     // +0x50 arc length -> parameter cubic (0x345048 set-piece path follower)
};
struct OriginalRailRecord {
    uint32_t packedId=0;    // header+0: (rid<<8)|track; 0x13B054 stores it to motion+0x24
    uint32_t flags=1;       // header+0x1C at runtime: bit0 = queryable by mask1; bit2 doubles imbalance (0x13B66C)
    int32_t surface=-1;     // header+0x28 -> query+0x4C -> rider+0x438
    std::vector<OriginalRailSegment> segments;
};
struct OriginalRailQueryResult {
    bool found=false;                 // 0x334680 return (stack+0x30)
    RailVector point{},tangent{};     // out+0 curve point, out+0x10 unit derivative
    float distance=0;                 // stack+0x34 best distance, cm
    float t=0;                        // out+0x68
    int32_t surface=-1;               // out+0x4C
    const OriginalRailRecord* record=nullptr;   // out+0x58 descriptor
    const OriginalRailSegment* segment=nullptr; // winning type-1 object
};

namespace rail_original {
using namespace terrain_original;
inline float bits(uint32_t b){return std::bit_cast<float>(b);}
inline float vuSqrt(float x){return terrain_original::sqrt(x);}
inline float vuReciprocalSqrt(float x){if(x==0)return bits(0x7F7FFFFFu);return div(1,vuSqrt(x));}
inline float vuReciprocal(float x){if(x==0)return bits(0x7F7FFFFFu);return div(1,x);}
inline RailVector scale(RailVector v,float s){for(auto& x:v)x=mul(x,s);return v;}
inline RailVector vadd(RailVector a,RailVector b){for(unsigned k=0;k<3;++k)a[k]=add(a[k],b[k]);return a;}
inline RailVector vsub(RailVector a,RailVector b){for(unsigned k=0;k<3;++k)a[k]=sub(a[k],b[k]);return a;}
inline float length(RailVector v){return vuSqrt(dot(v,v));}
inline RailVector normalizeRsqrt(RailVector v){return scale(v,vuReciprocalSqrt(dot(v,v)));}
inline float sadd(float a,float b){return originalScalarAdd(a,b);}
inline float ssub(float a,float b){return originalScalarSubtract(a,b);}
inline float sdiv(float a,float b){return collision_scalar::divide(a,b);}
inline float minS(float a,float b){return a<b?a:b;}
inline float maxS(float a,float b){return a>b?a:b;}
inline float clampUnit(float v){return -1.f<=v?minS(v,1.f):-1.f;} // c.le.s/min.s pattern
// VU Hamilton product used by 0x115358 (opmula/opmsub, mulaw/maddaw, subax/msubay/msubz, maddw).
inline std::array<float,4> quaternionMultiply(const std::array<float,4>& a,const std::array<float,4>& b){
    RailVector ax{a[0],a[1],a[2]},bx{b[0],b[1],b[2]};
    RailVector crossed{sub(mul(a[1],b[2]),mul(b[1],a[2])),sub(mul(a[2],b[0]),mul(b[2],a[0])),sub(mul(a[0],b[1]),mul(b[0],a[1]))};
    RailVector xyz=vadd(vadd(scale(ax,b[3]),scale(bx,a[3])),crossed);
    // msubay / msubz take vf0 (1.0) as fs: the y / z terms go through 1.0 x (docs/ps2-float.md "The VU0 forms").
    float w=sub(sub(sub(mul(a[3],b[3]),mul(a[0],b[0])),mul(1.f,mul(a[1],b[1]))),mul(1.f,mul(a[2],b[2])));
    return {xyz[0],xyz[1],xyz[2],w};
}
template<class F> const F& need(const F& f,const char* what){if(!f)throw std::runtime_error(std::string("Rail callback missing: ")+what);return f;}
inline void rebuild(std::array<float,4>& q,RailVector& right,RailVector& forward,RailVector& up){
    auto basis=originalRebuildOrientation(q);q=basis.quaternion;right=basis.right;forward=basis.forward;up=basis.up;
}
}

// 0x335128 evaluation: ACC=row0*p0; ACC+=row1*p1; ACC+=row2*p2; row3*p3+ACC.
inline RailVector originalRailCurvePoint(const OriginalRailSegment& s,std::array<float,4> powers) {
    using namespace rail_original;RailVector p;
    for(unsigned k=0;k<3;++k)p[k]=add(add(add(mul(s.coefficients[0][k],powers[0]),mul(s.coefficients[1][k],powers[1])),mul(s.coefficients[2][k],powers[2])),mul(s.coefficients[3][k],powers[3]));
    return p;
}
inline RailVector originalRailPoint(const OriginalRailSegment& s,float t) {
    using namespace rail_original;Rounding rounding;float t2=mul(t,t);return originalRailCurvePoint(s,{mul(t,t2),t2,t,1});
}

// Complete 0x335128: box rejection, five-sample chord search, golden-section
// refinement (0.381966/0.618034, tolerance 0.0005, at most 24 iterations),
// then strictly-closer replacement of the accumulated result.
inline void originalRailSegmentQuery(const OriginalRailRecord& record,const OriginalRailSegment& s,
        RailVector boxMin,RailVector boxMax,RailVector q,OriginalRailQueryResult& best) {
    using namespace rail_original;Rounding rounding;
    if(boxMax[0]<s.boundsMin[0]||s.boundsMax[0]<boxMin[0])return;
    if(boxMax[1]<s.boundsMin[1]||s.boundsMax[1]<boxMin[1])return;
    if(boxMax[2]<s.boundsMin[2]||s.boundsMax[2]<boxMin[2])return;
    RailVector previous=originalRailCurvePoint(s,{0,0,0,1});
    float bestDistance=length(vsub(q,previous));int bestIndex=0;
    for(int i=1;i<5;++i){
        float t=mul(float(i),.25f),t2=mul(t,t),t3=mul(t,t2);
        RailVector current=originalRailCurvePoint(s,{t3,t2,t,1});
        RailVector segment=vsub(current,previous),d=vsub(q,previous);
        float fraction=sdiv(dot(d,segment),dot(segment,segment));
        float clamped=0;if(0<=fraction)clamped=minS(fraction,1);
        RailVector closest=vadd(previous,scale(segment,clamped));
        float distance=length(vsub(q,closest));
        if(distance<bestDistance){bestDistance=distance;bestIndex=i;}
        previous=current;
    }
    static constexpr float lowTable[5]={0,0,0,.25f,.25f},highTable[5]={.75f,.75f,1,1,1}; // EE 0x48E560 / 0x48E578
    const float c1=bits(0x3ec3910cu),c2=bits(0x3f1e377au),tolerance=bits(0x3a03126fu);
    float low=lowTable[bestIndex],high=highTable[bestIndex];
    float span=ssub(high,low);
    float x1=sadd(low,mul(span,c1)),x2=sadd(low,mul(span,c2));
    auto evaluateAt=[&](float t){float t2=mul(t,t);return originalRailCurvePoint(s,{mul(t,t2),t2,t,1});};
    RailVector last=evaluateAt(low);
    last=evaluateAt(x1);float d1=length(vsub(last,q));
    last=evaluateAt(x2);float d2=length(vsub(last,q));
    float chosen=x2,chosenDistance=d2;
    last=evaluateAt(high); // original leaves P(high) as the output point when no iteration runs
    for(int iteration=0;iteration<24;++iteration){
        if(std::abs(ssub(high,low))<=tolerance)break;
        if(d2<d1){low=x1;x1=x2;d1=d2;x2=sadd(mul(x1,c2),mul(high,c1));chosen=x2;last=evaluateAt(x2);d2=length(vsub(last,q));chosenDistance=d2;}
        else {high=x2;x2=x1;d2=d1;x1=sadd(mul(x2,c2),mul(low,c1));chosen=x1;last=evaluateAt(x1);d1=length(vsub(last,q));chosenDistance=d1;}
    }
    if(best.found&&!(chosenDistance<best.distance))return;
    best.found=true;best.distance=chosenDistance;best.point=last;best.t=chosen;
    float three=mul(chosen,mul(chosen,3.f)),two=sadd(chosen,chosen);
    best.tangent=normalizeRsqrt(originalRailCurvePoint(s,{three,two,1,0}));
    best.surface=record.surface;best.record=&record;best.segment=&s;
}

// 0x334680 restricted to type-1 spline layers. The box is point +- radius
// (every rider caller passes 300 cm) and the descriptor flags must intersect
// mask. Object-bound descriptors (type2) and object-owned curve arrays
// (type3) are not represented, so those rails are never returned.
inline OriginalRailQueryResult originalRailWorldQuery(std::span<const OriginalRailRecord> records,
        RailVector point,float radius=300.f,uint32_t mask=1) {
    using namespace rail_original;Rounding rounding;
    RailVector extent{radius,radius,radius};RailVector boxMin=vsub(point,extent),boxMax=vadd(point,extent);
    OriginalRailQueryResult best;
    for(const auto& record:records){
        if(!(record.flags&mask)||!worldResident(record.packedId))continue; // streamed location (world_residency.hpp)
        for(const auto& segment:record.segments)originalRailSegmentQuery(record,segment,boxMin,boxMax,point,best);
    }
    return best;
}
// The order 0x334680 visits the segments in: its list (+0x210 count, +0x214 objects) is the rider's query scope, which
// 332DB8 collects from the octree (node list before children 0..7; each segment was inserted by the loader into the cell of
// its bounds +0x6C/+0x78 at the head of the cell's list, so later segments come first within a cell). The winner is the
// strictly closer one, so the order decides exact ties: two rails that share an end point (EBC3's treerailhevbb_1055 /
// _1056 start at the same point: peak3/the-throne-tuck 4057, the PS2 takes _1056).
struct OriginalRailWalkEntry {uint32_t record=0,segment=0;};
inline std::vector<OriginalRailWalkEntry> originalRailWalkOrder(std::span<const OriginalRailRecord> records) {
    std::vector<OriginalRailWalkEntry> order;std::vector<OriginalSpatialCell> cells;
    for(uint32_t r=0;r<records.size();++r)for(uint32_t k=0;k<records[r].segments.size();++k){
        const auto& s=records[r].segments[k];order.push_back({r,k});cells.push_back(originalSpatialCell(s.boundsMin,s.boundsMax));}
    std::vector<uint32_t> index(order.size());for(uint32_t i=0;i<index.size();++i)index[i]=i;
    std::stable_sort(index.begin(),index.end(),[&](uint32_t a,uint32_t b){
        if(originalSpatialBefore(cells[a],cells[b]))return true;if(originalSpatialBefore(cells[b],cells[a]))return false;return a>b;});
    std::vector<OriginalRailWalkEntry> out;out.reserve(index.size());for(uint32_t i:index)out.push_back(order[i]);return out;
}
inline OriginalRailQueryResult originalRailWorldQuery(std::span<const OriginalRailRecord> records,std::span<const OriginalRailWalkEntry> order,
        RailVector point,float radius=300.f,uint32_t mask=1) {
    using namespace rail_original;Rounding rounding;
    RailVector extent{radius,radius,radius};RailVector boxMin=vsub(point,extent),boxMax=vadd(point,extent);
    OriginalRailQueryResult best;
    for(const auto& e:order){const auto& record=records[e.record];
        if(!(record.flags&mask)||!worldResident(record.packedId))continue; // streamed location (world_residency.hpp)
        originalRailSegmentQuery(record,record.segments[e.segment],boxMin,boxMax,point,best);
    }
    return best;
}
// The same walk over a compact copy of the walked segments' bounds (docs/sim-performance.md "Rail query"): boxes[i] holds
// order[i]'s boundsMin/boundsMax (web/rail_bridge.cpp builds both with its walk cache). 0x335128's box test rejects almost
// every segment, and a segment is 112 B read in octree order; the record tests (flags, residency) and the box test are
// pure, so testing the copied box first visits exactly the same segments, in the same order, with the same arguments.
struct OriginalRailWalkBox {RailVector boundsMin{},boundsMax{};};
inline OriginalRailQueryResult originalRailWorldQuery(std::span<const OriginalRailRecord> records,std::span<const OriginalRailWalkEntry> order,
        std::span<const OriginalRailWalkBox> boxes,RailVector point,float radius=300.f,uint32_t mask=1,
        const terrain_original::RiderScope* scope=nullptr,std::span<const OriginalRailWalkBox> cells={}) {
    using namespace rail_original;Rounding rounding;
    RailVector extent{radius,radius,radius};RailVector boxMin=vsub(point,extent),boxMax=vadd(point,extent);
    OriginalRailQueryResult best;
    for(size_t i=0;i<order.size();++i){const auto& s=boxes[i];
        // 334680 visits only the rider's scope list (rider+0x860), which 332DB8 fills with the segments of the octree cells it
        // reaches: a segment is in it when its loose cell (originalSpatialCell, padded 0.2 cells) overlaps the scope box, not its own
        // bounds (PS2 hl2/plant-pipe-a savestates 898..901: 4 then 8 segments, exactly the cell rule at the 3-tick refresh; the
        // 300 cm handplant probe found a coping the PS2 did not list yet)
        if(scope&&!cells.empty()&&!scope->admits(cells[i].boundsMin,cells[i].boundsMax))continue;
        if(boxMax[0]<s.boundsMin[0]||s.boundsMax[0]<boxMin[0])continue; // 0x335128's box rejection, on the copy
        if(boxMax[1]<s.boundsMin[1]||s.boundsMax[1]<boxMin[1])continue;
        if(boxMax[2]<s.boundsMin[2]||s.boundsMax[2]<boxMin[2])continue;
        const auto& e=order[i];const auto& record=records[e.record];
        if(!(record.flags&mask)||!worldResident(record.packedId))continue; // streamed location (world_residency.hpp)
        originalRailSegmentQuery(record,record.segments[e.segment],boxMin,boxMax,point,best);
    }
    return best;
}

// ---------------------------------------------------------------------------
// Rider fields the rail routines read or write (source offsets in comments).
struct OriginalRailRider {
    RailVector position{},velocity{};                  // +0x110 / +0x1E0
    std::array<float,4> quaternion{0,0,0,1};           // +0x120
    RailVector right{1,0,0},forward{0,1,0},up{0,0,1};  // +0x1A0 / +0x1B0 / +0x1C0 (0x11E098 columns)
    RailVector contactNormal{0,0,1};                   // +0x370
    RailVector surfaceForward{0,1,0};                  // +0x3A0 (0x1161D0 transfer test; negated by 0x115168)
    RailVector lateral{1,0,0};                         // +0x3B0 (0x11FA10 turn shift; negated by 0x115168)
    RailVector surfaceVelocity{};                      // +0x3D0
    RailVector contactPoint{};                         // +0x460
    RailVector offset9D0{};                            // +0x9D0 board query offset
    RailVector bonePosition{};                         // posed board-root bone (+0x780->+0x2C, index +0x8A0)
    std::array<float,4> boneQuaternion{0,0,0,1};       // bone+0x10
    float speedLimit=0;                                // +0x2E4
    float timeScale=1;                                 // +0x300
    float boost=0;                                     // +0x2FC: 2450 cm/s^2 per unit along the rail (0x13B490)
    GroundControlValue turn,animationTurn,extraLean,brake,crouch,steer22C,balance,presentationRoll,tolerance25C,balance280;
    // +0x1F0, +0x1FC, +0x208, +0x214, +0x220, +0x22C, +0x238, +0x250, +0x25C, +0x280 triplets (current,rate,target)
    float manualSpin2DC=0;                             // +0x2DC
    int32_t style=0;                                   // +0x328: 1 forward 50-50, 2 backward 50-50, 3/4 sideways
    int32_t reverseStance=0,state324=0,flag330=0;      // +0x320 / +0x324 / +0x330
    int32_t surfaceId=0;                               // +0x438
    int32_t controlState=0,motionMode=0;               // owner+0xDE4 / owner+0xDE0
};
struct OriginalRailMotionState { // owner+0xB0
    RailVector direction{};       // +0x00 rail tangent oriented along travel
    float lean=0;                 // +0x10 radians
    float timeOnRail=0;           // +0x14 seconds
    float balance=0;              // +0x18 -1..1; control7 scales it by 1.2 into rider+0x240
    float headingOffset=0;        // +0x1C radians, +-45 degree clamp at entry
    int32_t lostRail=0;           // +0x20 set when the tick's query found no rail
    int32_t railId=-1;            // +0x24 record packed id
    RailVector entryPosition{};   // +0x30
    int32_t word40=0;             // +0x40
};
struct OriginalRailControlState { // owner+0x2B0
    float spin=0;                 // +0 accumulated rotation, +-pi/2 per 0x132060
    int32_t identity=-1;          // +4 last trick identity byte
};

// External subsystems driven by the rail routines. Missing callbacks throw.
struct OriginalRailAccess {
    std::function<OriginalRailQueryResult(RailVector point)> query;      // 0x334680(world,point,out,1,300)
    std::function<int()> channel2Class;                                  // 0x311AE8(anim,2)
    std::function<bool(unsigned bit)> channel2SequenceFlag;             // 0x1446A0(0x311B20(anim,2)+0xB0,bit)
    std::function<int()> currentSemantic;                                // 0x312AA0(anim)
    std::function<void(int semantic,float blend,int flags)> playAnimation; // 0x3128E8(anim,semantic,blend,flags)
    std::function<void(float radians)> rotateAnimation;                  // 0x311B48(anim,radians)
    std::function<void(float halfRadians)> setAnimationRoot;             // anim+0x30 = 0x4FF130, anim+0x40 = (0,0,sin,cos)
    std::function<void(int)> setAnimationSwitch;                         // anim+0x18
    std::function<void(int)> requestControl;                             // 0x11FEC8
    std::function<void(int)> requestMotion;                              // 0x11FE78
    std::function<float(bool stanceChanged,int style,int flag330)> railEntryScore; // 0x119D40(rider+0x790,...)
    std::function<void(float)> awardScore;                               // 0x10E098(rider,1,value)
    std::function<void(int style,int flag330,float speed,const OriginalRailQueryResult&)> airborneRailEvent; // 0x10E910
    std::function<void(int surface)> recordRailSurface;                  // global(gp+0x410)+0x598C
    std::function<void(const OriginalRailQueryResult&,const RailVector& before,const RailVector& after)> attachForce; // optional: 0x106848 -> hit entity vtable+0x15C (AnimTeeter 0x342538) with v_before - v_after
    std::function<void(const OriginalRailQueryResult&,const RailVector& force)> motionForce; // optional: 0x13AF28 (0x13B4D0..0x13B534) -> hit entity vtable+0x15C every rail motion tick
    // optional: 0x13B07C..0x13B098, an entity-owned rail (out+0x50 instance, its entity +0xC): vtable+0x154 writes the contact
    // velocity at the rail point to out+0x20, which 0x13B0C8 stores as +0x3D0 (a log teeter swinging under the rider)
    std::function<bool(const OriginalRailQueryResult&,RailVector&)> entityVelocity;
    std::function<float()> balanceStat;                                  // 0x149208 via 0x14DC80/0x14DD58(.,3)
    std::function<float(int style,float spin)> railSpinScore;            // 0x119918(rider+0x790,style,spin)
    std::function<bool(bool requested)> recovery;                        // 0x116120
    std::function<bool(bool bit14,bool bit13)> upperAction;              // Legacy name: 0x1162C8 crouch/jump request (held, pressed), enters control2
    std::function<bool(bool held,bool pressed)> boost;                   // 0x114130
    std::function<bool(OriginalRailRider&,int)> uberEntry; //132620, after upper action and before boost
    std::function<void()> upperReactions;                                // 0x115B58 then 0x115D48
    // 0x1161D0 accepted on the rail (131E80/131ED8): plays 24 (press>0) / 32, sets +0x330 and
    // requests control 1 (engine/board_press.hpp originalBoardPressEntry).
    std::function<void(float press)> boardPressEntry;
    std::function<void()> restoreStance;                                 // 0x115640
    std::function<void(OriginalRailRider&)> exitContacts; //ordered13BFF0..13C09C; return live actor changes
    std::function<void()> leaveEffects;                                  // 0x13BFA8 block: 0x11E150,0x114298,0x105398,0x13C140,0x107888,0x294170
};

// Rotated basis columns of an XYZW quaternion in the EE scalar order of
// 0x1086F4 (x), 0x106A48 (z) and 0x106BC4 (y).
struct OriginalRailBoardAxes {RailVector x,y,z;};
inline OriginalRailBoardAxes originalRailBoardAxes(const std::array<float,4>& q) {
    using namespace rail_original;Rounding rounding;float x=q[0],y=q[1],z=q[2],w=q[3];OriginalRailBoardAxes a;
    {float yy=mul(y,y),zz=mul(z,z),xz=mul(x,z),wy=mul(w,y),s=sadd(yy,zz),xy=mul(x,y),wz=mul(w,z);
     float c=ssub(xz,wy);s=sadd(s,s);float b=sadd(xy,wz);c=sadd(c,c);s=ssub(1,s);b=sadd(b,b);a.x={s,b,c};}
    {float xx=mul(x,x),yy=mul(y,y),zy=mul(z,y),wx=mul(w,x),s=sadd(xx,yy),zx=mul(z,x),wy=mul(w,y);
     float b=ssub(zy,wx);s=sadd(s,s);float c=sadd(zx,wy);b=sadd(b,b);s=ssub(1,s);c=sadd(c,c);a.z={c,b,s};}
    {float zz=mul(z,z),xx=mul(x,x),yz=mul(y,z),wx=mul(w,x),s=sadd(zz,xx),xy=mul(y,x),wz=mul(w,z);
     float c=sadd(yz,wx);s=sadd(s,s);float b=ssub(xy,wz);c=sadd(c,c);s=ssub(1,s);b=sadd(b,b);a.y={b,s,c};}
    return a;
}

// atan2 construction shared by 0x106B00 and 0x13AE74: 0x31C228 with quadrant fix.
inline float originalRailAngle(float f21,float f20) {
    using namespace rail_original;
    if(f20==0){if(f21==0)return 0;return 0<=f21?bits(0x3fc90fdbu):bits(0xbfc90fdbu);}
    float angle=collision_scalar::atan(sdiv(f21,f20));
    if(f20<0)angle=0<f21?sadd(angle,bits(0x40490fdbu)):ssub(angle,bits(0x40490fdbu));
    return angle;
}

// 0x1086B8: is the board line close enough to the rail hit for attachment.
inline bool originalRailBoardProximity(const OriginalRailRider& r,const OriginalRailAccess& access,
        const OriginalRailQueryResult& hit) {
    using namespace rail_original;Rounding rounding;
    RailVector axisX=originalRailBoardAxes(r.boneQuaternion).x;
    RailVector q=vadd(r.bonePosition,r.offset9D0);
    float f2=r.tolerance25C.current,f9=ssub(1,f2);
    float halfWidth=sadd(mul(f2,50.f),mul(f9,30.f));
    float reach=r.flag330!=0?sadd(mul(f2,170.f),mul(f9,50.f)):sadd(mul(f2,170.f),mul(f9,100.f));
    RailVector d=vsub(hit.point,q);
    float low=ssub(halfWidth,reach),high=ssub(reach,halfWidth);
    float along=dot(axisX,d),clamped=low;if(low<=along)clamped=minS(along,high);
    RailVector closest=vadd(q,scale(axisX,clamped));
    if(length(vsub(closest,hit.point))<=halfWidth)return true;
    float f3=dot(hit.tangent,axisX),f2b=ssub(1,mul(f3,f3));
    if(f2b<bits(0x3a83126fu))return false;
    RailVector qMinusHit=scale(vsub(q,hit.point),vuReciprocal(f2b));
    RailVector projector=vsub(scale(hit.tangent,f3),axisX);
    float along2=dot(qMinusHit,projector),clamped2=low;if(low<=along2)clamped2=minS(along2,high);
    RailVector probe=vadd(q,scale(axisX,clamped2));
    auto second=need(access.query,"query")(probe);
    if(!second.found)return false;
    return length(vsub(probe,second.point))<=halfWidth;
}

// 0x108A48: motion 0/1 only, board query, not-behind test, grab-class gating, then 0x1086B8.
inline bool originalRailAttachTest(const OriginalRailRider& r,const OriginalRailAccess& access,OriginalRailQueryResult& out) {
    using namespace rail_original;Rounding rounding;
    if(r.motionMode!=0&&r.motionMode!=1)return false;
    RailVector q=vadd(r.bonePosition,r.offset9D0);
    out=need(access.query,"query")(q);
    if(!out.found)return false;
    float speed=length(r.velocity);
    if(bits(0x3a83126fu)<speed){
        RailVector d=vsub(out.point,q);
        if(dot(d,r.velocity)<mul(speed,bits(0xbe4ccccdu)))return false;
    }
    int cls=need(access.channel2Class,"channel2Class")();
    auto flag=[&](unsigned bit){return need(access.channel2SequenceFlag,"channel2SequenceFlag")(bit);};
    if(cls==18){if(!flag(2)&&flag(0))return false;}
    else if(cls==19||cls==20){if(!flag(2))return false;}
    return originalRailBoardProximity(r,access,out);
}

// 0x1326C8: rail entry/cycle semantic by style (grounded 18/19/20, airborne 68/70/69).
inline int originalRailEntrySemantic(int style,bool fromAir) {
    if(style==4)return fromAir?69:20;if(style==3)return fromAir?70:19;return fromAir?68:18;
}

// 0x115358 with 0x115168: make the switch flag match the style (180 degree
// flip about up) and rotate sideways styles +-90 degrees physically while
// compensating the animation. The flip is the VU product (up,0)*q, 0x11E098, then
// 0x115168: toggle +0x320, 0x311B48(pi), root sincos(-pi/2 | -0), anim+0x18 = +0x320,
// and negate +0x3A0/+0x3B0 and the turn/animation-turn/extra-lean/brake/+0x280
// current and target values. The sideways turn is 0x11DFE0, which ends in 0x11E098.
inline void originalRailStanceAlignment(OriginalRailRider& r,const OriginalRailAccess& access) {
    using namespace rail_original;Rounding rounding;
    auto flip=[&](){
        std::array<float,4> axis{r.up[0],r.up[1],r.up[2],0};
        r.quaternion=quaternionMultiply(axis,r.quaternion);
        rebuild(r.quaternion,r.right,r.forward,r.up);
        r.reverseStance^=1;need(access.rotateAnimation,"rotateAnimation")(bits(0x40490fdbu));
        need(access.setAnimationRoot,"setAnimationRoot")(r.reverseStance?bits(0xbfc90fdbu):-0.f);
        need(access.setAnimationSwitch,"setAnimationSwitch")(r.reverseStance);
        r.surfaceForward=scale(r.surfaceForward,-1.f);r.lateral=scale(r.lateral,-1.f);
        for(auto* v:{&r.turn,&r.brake,&r.extraLean,&r.animationTurn,&r.balance280}){v->current=-v->current;v->target=-v->target;}
    };
    if(r.style==2){if(r.reverseStance==0)flip();return;}
    if(r.style==1){if(r.reverseStance!=0)flip();return;}
    if(r.reverseStance!=0)flip();
    float physical=r.style==4?bits(0x3fc90fdbu):bits(0xbfc90fdbu),root=r.style==4?bits(0xbf490fdbu):bits(0x3f490fdbu);
    r.quaternion=originalRotateOrientation(r.quaternion,r.up,physical);rebuild(r.quaternion,r.right,r.forward,r.up);
    need(access.rotateAnimation,"rotateAnimation")(physical);
    need(access.setAnimationRoot,"setAnimationRoot")(root);
}

// 0x13BD80: board normal from the rail tangent. Lean applies only when the
// rail is near-vertical (|tangent.z| > 0.92); otherwise the perpendicular-up
// direction is used with its sign matched to the rider's up.
inline RailVector originalRailBoardNormal(const OriginalRailMotionState& m,const OriginalRailRider& r,RailVector tangent) {
    using namespace rail_original;Rounding rounding;
    auto sc=collision_scalar::sincos(m.lean);
    float tz=tangent[2];
    if(!(bits(0x3f6b851fu)<std::abs(tz))){
        RailVector perpendicular=normalizeRsqrt(vsub(RailVector{0,0,1},scale(tangent,tz)));
        if(dot(perpendicular,r.up)<0)perpendicular=scale(perpendicular,-1.f);
        return perpendicular;
    }
    RailVector lateral=normalizeRsqrt(cross(r.up,tangent));
    RailVector perpendicular=cross(tangent,lateral);
    return vsub(scale(perpendicular,sc[1]),scale(lateral,sc[0]));
}

// 0x13ADC0: heading offset of the physical forward from the rail direction, +-45 degrees.
inline void originalRailHeadingOffset(OriginalRailMotionState& m,const OriginalRailRider& r,RailVector direction) {
    using namespace rail_original;Rounding rounding;
    RailVector normal=originalRailBoardNormal(m,r,direction);
    RailVector lateral=cross(normal,direction);
    float angle=originalRailAngle(dot(r.forward,lateral),dot(r.forward,direction));
    float limit=bits(0x3f490fdcu);
    m.headingOffset=-limit<=angle?minS(angle,limit):-limit;
}

// 0x13AD20: motion4 focus gain (after 0x11FA10, whose recovery is separate).
inline void originalRailMotionBegin(OriginalRailMotionState& m,OriginalRailRider& r) {
    using namespace rail_original;Rounding rounding;
    rebuild(r.quaternion,r.right,r.forward,r.up);
    r.turn={};r.extraLean={};r.presentationRoll={};m.lean=0;r.manualSpin2DC=0;m.timeOnRail=0;m.balance=0;
    r.tolerance25C.rate=bits(0x3d4cccceu);r.tolerance25C.target=0;m.lostRail=0;m.headingOffset=0;m.entryPosition=r.position;m.word40=0;
}

struct OriginalRailMotionStepResult {
    enum class Outcome {Riding,RailLost,Detached};
    Outcome outcome=Outcome::Riding;
    float lateralOffset=0,alongSpeed=0,slide=0,imbalance=0;
};
// 0x13AF28: complete rail motion tick for authored spline rails.
inline OriginalRailMotionStepResult originalRailMotionStep(OriginalRailMotionState& m,OriginalRailRider& r,const OriginalRailAccess& access) {
    using namespace rail_original;Rounding rounding;OriginalRailMotionStepResult result;
    if(r.controlState==12)m.headingOffset=0;
    {float speed=length(r.velocity);if(r.speedLimit<speed)r.velocity=scale(r.velocity,sdiv(r.speedLimit,speed));}
    float dt=mul(r.timeScale,bits(0x3c888889u));m.lostRail=1;
    auto hit=need(access.query,"query")(r.bonePosition);
    if(!hit.found){rebuild(r.quaternion,r.right,r.forward,r.up);result.outcome=OriginalRailMotionStepResult::Outcome::RailLost;return result;}
    m.railId=hit.record?int32_t(hit.record->packedId):-1;
    r.surfaceId=hit.surface;r.contactPoint=hit.point;r.surfaceVelocity={};
    if(access.entityVelocity){
        RailVector moving{};
        if(access.entityVelocity(hit,moving))r.surfaceVelocity=moving;
    }
    // 0x13B0A4..0x13B10C: f20 starts at 0.5 and becomes |v|*0.0009 only from 555.5555 cm/s up.
    float f20=.5f;{float speed=length(r.velocity);if(!(speed<bits(0x440ae38eu)))f20=mul(speed,bits(0x3a6bedfbu));}
    float f24=mul(f20,f20);
    if(m.timeOnRail<bits(0x3f19999au))r.tolerance25C.current=2.f;
    if(!originalRailBoardProximity(r,access,hit)){
        // 0x13BB14: push 277.777 cm/s away from the rail (bone - hit), or along it, and rebuild.
        RailVector offset=vsub(r.bonePosition,hit.point);float len=length(offset);
        RailVector direction=len<bits(0x3a83126fu)?hit.tangent:scale(offset,vuReciprocal(len));
        if(bits(0x3f000000u)<std::abs(dot(direction,hit.tangent)))direction=hit.tangent;
        float f2=dot(r.velocity,direction);
        if(f2<0){
            float speed=length(r.velocity);
            if(speed<bits(0x3a83126fu))direction=hit.tangent;
            else {RailVector remainder=vsub(direction,scale(r.velocity,sdiv(f2,speed)));float rl=length(remainder);
                  direction=bits(0x3a83126fu)<rl?scale(remainder,vuReciprocal(rl)):scale(r.velocity,vuReciprocal(speed));}
        }
        r.velocity=vadd(r.velocity,scale(direction,bits(0x438ae38eu)));
        rebuild(r.quaternion,r.right,r.forward,r.up);
        result.outcome=OriginalRailMotionStepResult::Outcome::Detached;return result;
    }
    m.lostRail=0;m.timeOnRail=sadd(m.timeOnRail,dt);
    RailVector relative=vsub(r.velocity,r.surfaceVelocity);RailVector tangent=hit.tangent;
    float along=dot(relative,tangent);if(along<0){along=-along;tangent=scale(tangent,-1.f);}
    m.direction=tangent;
    RailVector offset=vsub(hit.point,r.bonePosition);
    float step=mul(dt,1000.f),lateralOffset=dot(r.right,offset),vertical=dot(offset,r.up);
    float correction=-step;if(-step<=vertical)correction=minS(vertical,step);
    r.position=vadd(r.position,scale(r.up,correction));
    float blend=mul(dt,30.f),keep=ssub(1,blend),speed=length(relative);
    RailVector alongVelocity=scale(tangent,along);
    RailVector blended=vadd(scale(alongVelocity,blend),scale(relative,keep));
    float blendedLength=length(blended);
    if(bits(0x3a83126fu)<blendedLength)blended=scale(blended,sdiv(speed,blendedLength));
    r.velocity=vadd(blended,r.surfaceVelocity);
    RailVector gravity{0,0,-980.f}; // 0x4A5E50, initialized once at 0x13B3DC
    RailVector acceleration=vsub(scale(tangent,dot(gravity,tangent)),scale(alongVelocity,0.f));
    if(0<r.boost){float f0=0<along?mul(r.boost,2450.f):mul(r.boost,-2450.f);acceleration=vadd(acceleration,scale(tangent,f0));}
    // 0x13B4D0..0x13B534: a rail on an instance with an entity (hit +0x50 -> +0xC) gets entity vt+0x15C(hit point, gravity - along x 0)
    // every tick: the rider's weight on a log teeter (AnimTeeter 0x342538; fuzz r6-0121 1222, Snow Jam).
    if(access.motionForce)access.motionForce(hit,vsub(gravity,scale(alongVelocity,0.f)));
    r.velocity=vadd(r.velocity,scale(acceleration,dt));
    r.position=vadd(r.position,scale(r.velocity,dt));
    float speedFactor=bits(0x3f000000u)<=f20?minS(f20,2.f):.5f;
    float slideScale=mul(speedFactor,180.f);
    float steer=r.up[2]<0?-r.steer22C.current:r.steer22C.current;
    float slide=mul(steer,slideScale);
    bool nearVertical=bits(0x3f6b851fu)<std::abs(hit.tangent[2]);
    float imbalance=-5.f;
    if(!(m.timeOnRail<bits(0x3f19999au))&&!nearVertical){
        imbalance=(r.style==3||r.style==4)?bits(0x3fe66666u):bits(0x3e99999au);
        // 0x13B65C..0x13B6A0: descriptor bit 2 doubles it; only otherwise does |offset| < 2.5 cap it at -1.
        if(hit.record&&(hit.record->flags&4))imbalance=sadd(imbalance,imbalance);
        else if(std::abs(lateralOffset)<2.5f)imbalance=minS(imbalance,-1.f);
        float stat=need(access.balanceStat,"balanceStat")();
        imbalance=mul(imbalance,sdiv(1.f,sadd(mul(stat,bits(0x3fd335b2u)),1.f)));
    }
    // 0x13B6EC..0x13B7C4: the offset term is always applied. A negative imbalance (-5 on
    // entry or on near-vertical rails, or the -1 cap) uses the unclamped offset and so pulls
    // the board onto the rail; the inverted +-360 push is likewise unconditional.
    {
        float offsetTerm=lateralOffset;
        if(0<=imbalance){offsetTerm=-20.f;if(-20.f<=lateralOffset)offsetTerm=minS(lateralOffset,20.f);}
        float inverse=sdiv(1.f,f24),f3=1.f;if(1.f<=inverse)f3=minS(inverse,4.f);
        slide=ssub(slide,mul(offsetTerm,mul(imbalance,f3)));
        if(r.up[2]<0&&std::abs(lateralOffset)<20.f){if(0<lateralOffset)slide=minS(slide,-360.f);else slide=maxS(slide,360.f);}
    }
    r.position=vadd(r.position,scale(r.right,mul(slide,dt)));
    float target,leanLimit;
    if(r.style==3||r.style==4){m.balance=clampUnit(mul(lateralOffset,bits(0x3c6a0ea1u)));target=mul(m.balance,bits(0x3f060a93u));leanLimit=bits(0x3f32b8c4u);}
    else {m.balance=clampUnit(mul(lateralOffset,bits(0x3d088889u)));target=mul(m.balance,bits(0x3eb2b8c4u));leanLimit=bits(0x3eb2b8c4u);}
    float heading=-leanLimit;if(-leanLimit<=m.headingOffset)heading=minS(m.headingOffset,leanLimit);
    if(r.up[2]<0)target=-target;
    {float rate=mul(dt,300.f),current=m.lean,next;
     if(sadd(target,rate)<current)next=ssub(current,rate);else if(current<ssub(target,rate))next=sadd(current,rate);else next=target;m.lean=next;}
    auto leanSC=collision_scalar::sincos(m.lean);
    RailVector normal=originalRailBoardNormal(m,r,tangent);
    r.contactNormal=normal;
    RailVector lateral=cross(normal,tangent);
    RailVector targetUp=vadd(scale(normal,leanSC[1]),scale(lateral,leanSC[0]));
    auto headingSC=collision_scalar::sincos(heading);
    RailVector turned=vsub(scale(lateral,headingSC[1]),scale(tangent,headingSC[0]));
    RailVector targetForward=normalizeRsqrt(cross(turned,targetUp));
    float f12=dot(targetForward,r.forward);f12=mul(f12,.5f);f12=sadd(f12,.5f);float f0=ssub(1.f,f12);f12=mul(f12,15.f);f12=sadd(f12,f0);f12=mul(f12,r.timeScale);
    auto aligned=originalAirAlignment(r.quaternion,targetUp,targetForward,f12,bits(0x501502f9u));
    r.quaternion=aligned.quaternion;rebuild(r.quaternion,r.right,r.forward,r.up);
    result.lateralOffset=lateralOffset;result.alongSpeed=along;result.slide=slide;result.imbalance=imbalance;return result;
}

// 0x13BFA8 motion4 focus loss. The recovered decisions are the steer reset,
// the motion1 request after a lost rail and the final speed clamp; the effect
// block runs through the mandatory leaveEffects callback.
// predictionVelocity: +0x1E0 when 11FE78(1) ran: motion 1's enter 1399E0 seeds the predictor (1135B8) there, before the clamp below.
struct OriginalRailMotionLeaveEffects {bool lostRail=false,requestAirMotion=false;int scoreEventKind=22;RailVector predictionVelocity{};};
inline OriginalRailMotionLeaveEffects originalRailMotionLeave(const OriginalRailMotionState& m,OriginalRailRider& r,const OriginalRailAccess& access) {
    using namespace rail_original;Rounding rounding;OriginalRailMotionLeaveEffects e;
    if(r.controlState==12&&!access.exitContacts)throw std::runtime_error("Rail Uber exit requires original contact-mask/query callbacks (13BFA8)");
    if(m.lostRail){r.steer22C.target=0;r.steer22C.rate=bits(0x3d088889u);e.lostRail=true;}
    need(access.leaveEffects,"leaveEffects")();
    if(access.exitContacts)access.exitContacts(r);
    if(m.lostRail&&r.motionMode==4){e.requestAirMotion=true;need(access.requestMotion,"requestMotion")(1);e.predictionVelocity=r.velocity;}
    float speed=length(r.velocity);if(r.speedLimit<speed)r.velocity=scale(r.velocity,sdiv(r.speedLimit,speed));
    return e;
}

// 0x106848: automatic attachment from controllers 0/1/2/3/4/5/11/12.
struct OriginalRailAttachResult {
    bool attached=false,fromAir=false,restyled=false;
    int style=0,entrySemantic=-1;
    std::vector<int> controlRequests;
};
inline OriginalRailAttachResult originalRailAttach(OriginalRailRider& r,OriginalRailMotionState& m,const OriginalRailAccess& access) {
    using namespace rail_original;Rounding rounding;OriginalRailAttachResult result;OriginalRailQueryResult hit;
    if(!originalRailAttachTest(r,access,hit))return result;
    RailVector direction=hit.tangent;
    float f12=dot(r.velocity,direction);if(f12<0){f12=-f12;direction=scale(direction,-1.f);}
    float floor=bits(0x440ae38eu);f12=f12<=floor?floor:f12;
    float speed=length(r.velocity);float f23=speed<=f12?speed:f12;
    const RailVector velocityBefore=r.velocity;
    r.velocity=scale(direction,f23);r.velocity[2]=mul(r.velocity[2],bits(0x3dcccccdu));
    if(access.attachForce)access.attachForce(hit,velocityBefore,r.velocity);
    auto axes=originalRailBoardAxes(r.boneQuaternion);
    float f21=dot(axes.z,direction);
    float angle=originalRailAngle(f21,dot(axes.x,direction));
    int previousStyle=r.style;
    if(r.controlState==1)r.style=r.reverseStance==0?1:2;
    else if(r.flag330!=0)r.style=bits(0x3fc90fdcu)<std::abs(angle)?2:1;
    else if(r.controlState==12)r.style=3;
    else if(axes.y[2]<0)r.style=f21<0?4:3;
    else if(bits(0x40278d37u)<std::abs(angle))r.style=2;
    else if(std::abs(angle)<bits(0x3f060a93u))r.style=1;
    else r.style=angle<bits(0xbf060a93u)?4:3;
    r.surfaceId=hit.surface;result.style=r.style;
    if(r.motionMode==1)need(access.airborneRailEvent,"airborneRailEvent")(r.style,r.flag330,f23,hit);
    else {float score=need(access.railEntryScore,"railEntryScore")(r.reverseStance!=r.state324,r.style,r.flag330);need(access.awardScore,"awardScore")(score);}
    need(access.requestMotion,"requestMotion")(4);need(access.recordRailSurface,"recordRailSurface")(hit.surface);
    int c=r.controlState;
    if(c==0||c==4||c==5||c==11){
        result.fromAir=c==5||c==4;
        need(access.requestControl,"requestControl")(13);result.controlRequests.push_back(13);
        originalRailStanceAlignment(r,access);
        if(r.flag330!=0){ //0x106D9C: a pending board press lands on the rail in control 1 (26 nose / 34 tail)
            result.entrySemantic=r.flag330==1?26:34;
            need(access.playAnimation,"playAnimation")(result.entrySemantic,-1.f,0);
            need(access.requestControl,"requestControl")(1);result.controlRequests.push_back(1);
        }else{
            result.entrySemantic=originalRailEntrySemantic(r.style,result.fromAir);
            need(access.playAnimation,"playAnimation")(result.entrySemantic,-1.f,0);
            need(access.requestControl,"requestControl")(7);result.controlRequests.push_back(7);
        }
    } else if(previousStyle!=r.style){
        result.restyled=true;
        if(previousStyle==3||previousStyle==4){
            float physical=previousStyle==3?bits(0x3fc90fdbu):bits(0xbfc90fdbu);
            r.quaternion=originalRotateOrientation(r.quaternion,r.up,physical);rebuild(r.quaternion,r.right,r.forward,r.up); //0x11DFE0 ends in 0x11E098
            need(access.rotateAnimation,"rotateAnimation")(physical);need(access.setAnimationRoot,"setAnimationRoot")(-0.f);
        }
        originalRailStanceAlignment(r,access);
    }
    originalRailHeadingOffset(m,r,direction);
    result.attached=true;return result;
}

// ---------------------------------------------------------------------------
// Control7. Command layout from 0x131D30 (run-length bits already removed).
struct OriginalRailCommand {
    bool recovery=false,upper13=false,upper14=false,boostPressed15=false,boostHeld16=false;
    int8_t identity=-1;  // word0 bits 17..24
    float turn=0;        // word0 bits 25..30 signed6 * 1/31 -> 0x113F38
    float rotate=0;      // word1 bits 0..5 signed6 * 1/31 -> 0x132060 (negative = left)
    float transfer=0;    // word1 bits 6..11 signed6 * 1/31 -> 0x1161D0
};
inline OriginalRailCommand originalRailDecodeCommand(uint32_t word0,uint32_t word1) {
    using namespace rail_original;Rounding rounding;OriginalRailCommand c;
    auto axis=[&](uint32_t word,unsigned shift){int v=int((word>>shift)&63);if(v>=32)v-=64;return mul(float(v),bits(0x3d042108u));};
    c.recovery=word0&0x1000;c.upper13=word0&0x2000;c.upper14=word0&0x4000;c.boostPressed15=word0&0x8000;c.boostHeld16=word0&0x10000;
    c.identity=int8_t((word0>>17)&255);c.turn=axis(word0,25);c.rotate=axis(word1,0);c.transfer=axis(word1,6);return c;
}
// 0x113F38: rail steer target (also the soft controller's requestBalance).
inline void originalRailSteerTarget(OriginalRailRider& r,float input) {
    using namespace rail_original;Rounding rounding;
    float delta=mul(std::abs(ssub(input,r.steer22C.current)),7.f);
    float rate=bits(0x3dcccccdu);if(rate<=delta)rate=minS(delta,8.f);
    r.steer22C.target=input;r.steer22C.rate=mul(rate,bits(0x3c888889u));
}
// 0x131D08.
inline void originalRailControlBegin(OriginalRailControlState& c,OriginalRailRider& r) {
    using namespace rail_original;c.spin=0;c.identity=-1;r.animationTurn.rate=bits(0x3d888889u);r.animationTurn.target=0;
}
// 0x132048.
inline void originalRailControlLeave(OriginalRailRider& r){using namespace rail_original;r.balance.target=0;r.balance.rate=bits(0x3d888889u);}

// 0x132060: rotate the board on the rail. Table verified per branch.
struct OriginalRailRotationEffects {int semantic=0,newStyle=0;float animationRootHalfAngle=0;bool balanceNegated=false;float spinScore=0;};
inline OriginalRailRotationEffects originalRailRotation(OriginalRailControlState& c,OriginalRailRider& r,bool left,const OriginalRailAccess& access) {
    using namespace rail_original;Rounding rounding;OriginalRailRotationEffects e;
    float quarter=bits(0x3fc90fdcu);c.spin=left?ssub(c.spin,quarter):sadd(c.spin,quarter);
    auto negateBalance=[&](){r.balance.current=-r.balance.current;r.balance.target=-r.balance.target;e.balanceNegated=true;};
    int style=r.style;
    if(left){
        if(style==1){e.animationRootHalfAngle=bits(0x3f490fdbu);e.semantic=49;e.newStyle=3;}
        else if(style==2){r.reverseStance=0;e.animationRootHalfAngle=bits(0xbf490fdbu);need(access.setAnimationSwitch,"setAnimationSwitch")(0);negateBalance();e.semantic=53;e.newStyle=4;}
        else if(style==3){r.reverseStance=1;e.animationRootHalfAngle=bits(0xbfc90fdbu);need(access.setAnimationSwitch,"setAnimationSwitch")(1);negateBalance();e.semantic=51;e.newStyle=2;}
        else if(style==4){e.animationRootHalfAngle=-0.f;e.semantic=54;e.newStyle=1;}
        else throw std::runtime_error("Rail rotation from an unknown style");
    } else {
        if(style==1){e.animationRootHalfAngle=bits(0xbf490fdbu);e.semantic=50;e.newStyle=4;}
        else if(style==2){r.reverseStance=0;e.animationRootHalfAngle=bits(0x3f490fdbu);need(access.setAnimationSwitch,"setAnimationSwitch")(0);negateBalance();e.semantic=52;e.newStyle=3;}
        else if(style==3){e.animationRootHalfAngle=-0.f;e.semantic=51;e.newStyle=1;}
        else if(style==4){r.reverseStance=1;e.animationRootHalfAngle=bits(0xbfc90fdbu);need(access.setAnimationSwitch,"setAnimationSwitch")(1);negateBalance();e.semantic=54;e.newStyle=2;}
        else throw std::runtime_error("Rail rotation from an unknown style");
    }
    need(access.setAnimationRoot,"setAnimationRoot")(e.animationRootHalfAngle);
    need(access.playAnimation,"playAnimation")(e.semantic,-1.f,0);
    r.style=e.newStyle;
    e.spinScore=need(access.railSpinScore,"railSpinScore")(r.style,c.spin);need(access.awardScore,"awardScore")(e.spinScore);
    return e;
}

// 0x132770: once the rail motion has become airborne (motion1), wait for a
// rotation animation (class14) to complete, restore the stance and hand over
// to control4 (no stick) or control5 (stick held).
struct OriginalRailAirborneExit {bool exited=false;int nextControl=0;};
inline OriginalRailAirborneExit originalRailAirborneExit(const OriginalRailRider& r,const OriginalRailCommand& command,const OriginalRailAccess& access) {
    using namespace rail_original;OriginalRailAirborneExit e;
    if(r.motionMode!=1)return e;
    if(need(access.channel2Class,"channel2Class")()==14&&!need(access.channel2SequenceFlag,"channel2SequenceFlag")(0))return e;
    need(access.restoreStance,"restoreStance")();
    e.exited=true;e.nextControl=command.rotate==0?4:5;need(access.requestControl,"requestControl")(e.nextControl);return e;
}

// 0x1161D0 from control 7 (BoardPress, word1 bits 6..11): zero input, sideways styles 3/4
// and (motion 0 only) travelling against +0x3A0 decline; otherwise the board press
// begins (24/32, +0x330, control 1) through access.boardPressEntry.
inline bool originalRailTransferHandled(const OriginalRailRider& r,float transfer,const OriginalRailAccess& access) {
    using namespace rail_original;Rounding rounding;
    if(transfer==0)return false;
    if(r.style==3||r.style==4)return false;
    if(r.motionMode==0&&dot(r.velocity,r.surfaceForward)<0)return false;
    need(access.boardPressEntry,"boardPressEntry")(transfer);return true;
}

struct OriginalRailControlResult {
    enum class Stop {None,Recovery,Airborne,Upper,Uber,RotationPending,Rotated};
    Stop stop=Stop::None;bool cycleRequested=false;int cycleSemantic=-1;
};
// 0x131D30 per-tick control. The caller owns132620 and control12 entry.
inline OriginalRailControlResult originalRailControlStep(OriginalRailControlState& c,OriginalRailRider& r,
        const OriginalRailMotionState& m,const OriginalRailCommand& command,const OriginalRailAccess& access) {
    using namespace rail_original;Rounding rounding;OriginalRailControlResult result;
    if(need(access.recovery,"recovery")(command.recovery)){result.stop=OriginalRailControlResult::Stop::Recovery;return result;}
    if(originalRailAirborneExit(r,command,access).exited){result.stop=OriginalRailControlResult::Stop::Airborne;return result;}
    if(need(access.upperAction,"upperAction")(command.upper14,command.upper13)){result.stop=OriginalRailControlResult::Stop::Upper;return result;}
    if(access.uberEntry){if(access.uberEntry(r,command.identity)){result.stop=OriginalRailControlResult::Stop::Uber;return result;}}
    else if(command.identity!=-1)throw std::runtime_error("Rail Uber entry callback missing");
    need(access.boost,"boost")(command.boostHeld16,command.boostPressed15);
    originalRailSteerTarget(r,command.turn);
    groundCrouchBrakeTargets(r.crouch,r.brake,0,0,dot(r.velocity,r.surfaceForward),r.turn.current);
    need(access.upperReactions,"upperReactions")();
    int cls=need(access.channel2Class,"channel2Class")();
    auto rotate=[&](float f0){r.balance.target=0;r.balance.rate=bits(0x3d888889u);originalRailRotation(c,r,f0<0,access);result.stop=OriginalRailControlResult::Stop::Rotated;};
    if(cls==14){
        if(!need(access.channel2SequenceFlag,"channel2SequenceFlag")(0)){result.stop=OriginalRailControlResult::Stop::RotationPending;return result;}
        if(originalRailTransferHandled(r,command.transfer,access))return result;
        if(command.rotate==0)return result;
        rotate(command.rotate);return result;
    }
    if(originalRailTransferHandled(r,command.transfer,access))return result;
    float f0=command.rotate;
    if(command.transfer!=0&&command.rotate==0)f0=command.transfer;
    if(f0!=0){rotate(f0);return result;}
    if(cls==10){r.balance.target=0;r.balance.rate=bits(0x3d888889u);return result;} //0x131F94 branches to the epilogue: no cycle request while a class-10 entry plays
    else {r.balance.target=clampUnit(mul(m.balance,bits(0x3f99999au)));r.balance.rate=bits(0x3d888889u);}
    int expected=r.style==4?20:(r.style==3?19:18);
    if(need(access.currentSemantic,"currentSemantic")()!=expected){need(access.playAnimation,"playAnimation")(expected,-1.f,0);result.cycleRequested=true;result.cycleSemantic=expected;}
    return result;
}
}
