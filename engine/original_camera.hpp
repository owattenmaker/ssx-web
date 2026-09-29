#pragma once
// Native translation of the SSX3 PS2 (SLUS_207.72) automatic chase camera
// "DEFAULT_3" (type 0x3D; the Mid camera option and the in-game default) plus
// the outer compositor stages that run on it every frame: terrain clearance
// (0x15EE00), speed/boost shake (0x15E460/0x1656B0/0x165938) and fov/near/far
// clamping (0x15E668). Everything is in source units: centimetres, Z up.
//
// Numerical policy (see docs/HANDOFF.md): every step runs inside an
// FE_TOWARDZERO scope. VU0 vector ops use plain chop arithmetic (mul/add/sub of
// terrain_contact_math.hpp, vsqrt = sqrt under chop, VU DIV/RSQRT saturate on
// zero). EE scalar ADD/SUB use the one-guard-bit helpers, EE MUL is a chop
// multiply, EE DIV/SQRT round to nearest (collision_scalar). Operation order
// follows the decoded listings; constants are given as bit patterns.
//
// Addresses, offsets, constants and uncertainties: engine/CAMERA_RECOVERY.md.
#include "terrain_contact_math.hpp"
#include "collision_scalar.hpp"
#include "original_random.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>

namespace ssx {
using OriginalCameraQuad=std::array<float,4>; // (x,y,z,w); points carry w=1, directions w=0

// Result of the 0x336850 segment query used by 0x15EE00: the parameter along
// start->end (closest hit to start) and the hit normal (only .z and the quad
// itself are consumed).
struct OriginalCameraProbeHit {
    float fraction=0;                       // t in [0,1] along start->end
    OriginalCameraQuad normal{0,0,1,0};     // hit record +0x10
};
using OriginalCameraProbe=std::function<std::optional<OriginalCameraProbeHit>(const OriginalCameraQuad& start,const OriginalCameraQuad& end)>;

// Per-tick inputs, all read through the RiderCameraTarget vtable 0x45B7A8
// unless noted. Field comments give the source offset and the decoded meaning;
// UNCERTAIN marks semantics inferred from usage only.
struct OriginalCameraInput {
    OriginalCameraQuad headPosition{0,0,0,1};          // slot 0x08: head bone (rider+0x89C) world position, cm, w=1
    OriginalCameraQuad riderForward{0,1,0,0};          // rider+0x1B0 physical forward column (also read directly by 0x168150), w=0
    OriginalCameraQuad velocity{0,0,0,0};              // slot 0x18: rider+0x1E0 velocity cm/s, w=0 (zero quad when the physics object is inactive)
    OriginalCameraQuad previousContactNormal{0,0,1,0}; // slot 0x48: rider+0x380
    OriginalCameraQuad wallNormal{0,0,0,0};            // slot 0x80: rider+0x3C0 horizontal takeoff normal saved by114998 on every jump (fallback +X)
    int motionMode=0;                                  // 0x11FE98: 0 cruise, 1 air, 2 wipeout, 3, 4, 5 (4/5 UNCERTAIN: 5 triggers SPOKE/hold, 4 the pull-behind)
    float boostLevel=0;                                // slot 0x88: rider+0x2FC, 0 / 0.25 / 0.625 / 1.0 boost tier (must be >= 0, it is square-rooted)
    float jumpCharge=0;                                // slot 0x90: rider+0x220 crouch/jump charge 0..1 (engine groundState.crouch.current / jumpCharge)
    int surfaceId=0;                                   // slot 0xA0: rider+0x438 surface parameter index; {2,3,13} select the heavy velocity filter
    int riderType=0;                                   // rider+0x434 (jump_motion riderState); {11,12,13} take the 4th root of the direction coefficient (UNCERTAIN meaning)
    float launchValue=0;                               // slot 0xA8: rider+0x5A4, ramped over 300..500 into the jump-camera amplitude (UNCERTAIN: jump launch magnitude)
    bool proximityFlag=false;                          // slot 0xC0: rider+0x5AC int flag (UNCERTAIN: object within 300 cm probe)
    bool trajectoryStatusActive=false;                 // slot 0x98: *(rider+0x788)+0xAC (air trajectory status) == 1 || == 3
    float predictedAirTime=0;                          // slot 0x60: *(rider+0x788)+0x98 seconds (OriginalAirTrajectory::predictedTime)
    OriginalCameraQuad trajectoryHeading{0,0,0,0};     // *(rider+0x788)+0x10 (slot 0xB8 vector b; UNCERTAIN: landing approach direction)
    OriginalCameraQuad trajectoryNormal{0,0,1,0};      // *(rider+0x788)+0x20 (slot 0xB8 vector a; UNCERTAIN: predicted landing normal)
    uint32_t tick=0;                                   // 0x1298C8: monotonically increasing 60 Hz frame counter
    bool raceStateSuppressesShake=false;               // 0x15E360: *(levelmgr+0x28)+0 in [1,9] suppresses shake indices 1..4 (UNCERTAIN state meaning)
    float farCap=30000;                                // 0x2EE3B8 render-listener far cap (oracle shows ~29999.34)
    OriginalCameraProbe terrainProbe;                  // 0x336850 segment query; empty => no hit
    OriginalRandomState* visualRandom=nullptr;         // 0x3177F0 shared six-word RNG at 0x4FF018 (same state as snow particles); null => private fallback generator
    int32_t raceRiderCount=1;                          // game-info+0x78 roster count (*(*(*(gp-0x848)+0x84)+0xC)+0x78); POST_RACE_1 (0x178BB0) pulls back when >= 2 (6 in a Snow Jam race)
};

// cCSICubicSpline (12-byte object, ctor 0x31D5E8): 1-D cubic spline y(x),
// natural (0x31D700) or clamped (0x31D738). Records mirror the 20-byte heap
// entries: raw knots before build, {d,c,b,a,x} coefficients after.
struct OriginalCubicSpline {
    struct Record {float a3=0,a2=0,a1=0,a0=0,x=0;}; // +0 t^3, +4 t^2, +8 t, +0xC y, +0x10 knot x
    bool clamped=false,allocated=false,built=false;  // flags bits 0,1,2
    unsigned count=0,segments=0,current=0;           // flags bits 3..10, 11..18, 19..26
    float endValue=0;                                // +0x4: end slope before a clamped build, x of the last knot after build
    std::array<Record,12> records{};                 // up to 13 knots
    void allocate(unsigned knots){ // 0x31D660
        segments=(knots-1)&0xFF;records={};allocated=true;count=0;built=false;
    }
    void reset(unsigned knots){allocate(knots);clamped=false;}                    // 0x31D700
    void resetClamped(unsigned knots,float slope0,float slopeN){                  // 0x31D738
        allocate(knots);clamped=true;records[0].x=slope0;endValue=slopeN;
    }
    void add(float x,float y){ // 0x31D7E0
        if(count!=segments){records[count].a3=x;records[count].a2=y;count=(count+1)&0xFF;}
        else {records[segments-1].a1=x;records[segments-1].a0=y;build();built=true;}
    }
    void build();
    float evaluateSegment(const Record& r,float x)const{ // 0x31D5A8
        using namespace terrain_original;
        float t=originalScalarSubtract(x,r.x);
        float f0=r.a1,f4=r.a0,f3=r.a3,f1=r.a2;
        float f2=mul(t,t);f0=mul(f0,t);float f12=mul(t,f3);f1=mul(f1,f2);
        f0=originalScalarAdd(f0,f4);f12=mul(f12,f2);f0=originalScalarAdd(f0,f1);
        return originalScalarAdd(f12,f0);
    }
    float evaluate(float x); // 0x31DEE0 (updates the cached segment index)
};

// The 0x390-byte chase algorithm object (base ctor 0x162318, DEFAULT_3 ctor
// 0x176D68). Names follow the decoded meaning; the source offset is given.
struct OriginalChaseAlgorithmState {
    float fov=0x1.921fb6p-1f;                 // +0x00 (pi/4 ctor; 0x1633B0 writes F*pi/4*scale)
    float near=10,far=30000;                  // +0x04, +0x08 (clamped later by the compositor)
    OriginalCameraQuad lookAt{0,0,0,0};       // +0x20 (ctor leaves it unwritten; first written by step 4 / snap)
    OriginalCameraQuad eye{0,0,0,1};          // +0x40
    float yaw=0,pitch=0;                      // +0x50, +0x54 (0x166640)
    OriginalCameraQuad outputEye{0,0,0,1};    // +0x60 (0x166F90: eye with w forced to 1)
    OriginalCameraQuad direction{0,0,0,0};    // +0x80 filtered travel direction (not unit)
    OriginalCameraQuad headCopy{0,0,0,0};     // +0xA0
    OriginalCameraQuad savedOffset{0,0,0,0};  // +0xB0 lookat-eye remembered outside mode 5
    OriginalCameraQuad pullBehind{0,0,0,0};   // +0xC0 filtered mode-4 displacement
    OriginalCameraQuad fieldD0{0,0,0,0},fieldE0{0,0,0,0},fieldF0{0,0,0,0}; // +0xD0/+0xE0 zero, +0xF0 = Z x forward (init only)
    OriginalCameraQuad velocityFilter{0,0,0,0};   // +0x100 lag-filtered horizontal velocity
    OriginalCameraQuad swingStartView{0,0,0,0};   // +0x110 V0
    OriginalCameraQuad swingTargetView{0,0,0,0};  // +0x120 T
    OriginalCameraQuad swingAxis{0,0,0,0};        // +0x130 normalize(V0 x T)
    OriginalCameraQuad swingRotatedView{0,0,0,0}; // +0x140
    OriginalCameraQuad swingWallProjection{0,0,0,0}; // +0x150 projection of T on the wall normal
    OriginalCameraQuad swingCurveProjection{0,0,0,0}; // +0x160 curve(t) * +0x150
    OriginalCameraQuad returnView{0,0,0,0};       // +0x170
    OriginalCameraQuad returnLookDelta{0,0,0,0};  // +0x180
    OriginalCameraQuad field190{0,0,0,0};         // +0x190 velocity copy (init only)
    OriginalCameraQuad lockView{0,0,0,0};         // +0x1A0 view vector captured by the slow lock
    OriginalCameraQuad lastVelocity{0,0,0,0};     // +0x1B0
    OriginalCameraQuad lastRawVelocity{0,0,0,0};  // +0x1C0
    float distance=200;                  // +0x1D0 follow distance
    float eyeVerticalOffset=0;           // +0x1D4 (subtracted from eye.z)
    float filteredPitch=0;               // +0x1D8
    float fovScale=1;                    // +0x1DC
    float mode5Timer=10;                 // +0x1E0 seconds
    float lookAtHeight=0;                // +0x1E4 cm added to lookat.z
    float boostBump=0,lastBoost=0;       // +0x1E8, +0x1EC
    float field1F0=0;                    // +0x1F0
    float phaseBClock=0;                 // +0x1F4
    float amplitudeGain=0;               // +0x1F8
    float timeGain=0;                    // +0x1FC
    float phaseBDuration=0;              // +0x200
    float heldEyeTerm=0,heldLookTerm=0;  // +0x204, +0x208 (ground-only terms frozen in the air)
    float lookOffsetB=0,eyeOffsetB=0;    // +0x20C, +0x210
    float pushFilter=0;                  // +0x214
    float phaseAClock=0;                 // +0x218
    float lookOffsetA=0,eyeOffsetA=0;    // +0x21C, +0x220
    float landingDecay=0;                // +0x224
    float takeoffRamp=0;                 // +0x228
    float swingElapsed=0;                // +0x22C
    float swingDuration=0;               // +0x230
    float swingClock=0;                  // +0x234
    float swingAngle=0;                  // +0x238 radians
    float returnTimer=0;                 // +0x23C
    float residualTimer=0,residualStart=0; // +0x240, +0x244
    float lockBlend=0;                   // +0x248
    float field24C=0,field250=0;         // +0x24C, +0x250
    float extraEyeHeight=0;              // +0x2B8
    float field2BC=0;                    // +0x2BC
    int32_t mode5Frames=5000;            // +0x2C0
    int32_t takeoffCountdown=0;          // +0x2C4
    int32_t field2C8=0;                  // +0x2C8
    int32_t lastAirTick=0;               // +0x2CC
    int32_t wallLaunch=0;                // +0x2D0
    int32_t airborneLatch=0;             // +0x2D4
    int32_t swingInitialised=0;          // +0x2D8
    int32_t field2DC=0;                  // +0x2DC
    int32_t phaseBActive=0;              // +0x2E0
    int32_t landed=0;                    // +0x2E4
    int32_t phaseAActive=0;              // +0x2E8
    int32_t field2EC=0;                  // +0x2EC (shake active flag on the compositor instance only)
    int32_t resetPending=0;              // +0x2F0
    float field2F4=0,field2F8=0;         // +0x2F4, +0x2F8 copies of ctx boost values
    int32_t field2FC=0;                  // +0x2FC copy of (mode == 5)
    int32_t lockState=0;                 // +0x300: 0 locking-in, 2 locked, 1 releasing, 3 idle
    OriginalCubicSpline lookSplineB,eyeSplineB,lookSplineA,eyeSplineA,swingSpline; // +0x304, +0x310, +0x31C, +0x328, +0x334
    std::array<float,5> ringX{},ringY{},ringE{}; // +0x340, +0x354, +0x368 PID tracker history
    int32_t ringIndex=0;                 // +0x37C
    float gainP=0x1.7560320000000p-7f;   // +0x380 0x3C3AB019 0.011394524
    float gainI=0;                       // +0x384
    float gainD=0x1.4f27400000000p-10f;  // +0x388 0x3AA793A0 0.0012785085
    int variantType=0x3D;                     // +0x0C algorithm type: 0x3C Near, 0x3D Mid (default), 0x3E Far (originalChaseVariant constants), 0x44 POST_RACE_1
    OriginalCameraQuad postRaceDirection{0,0,0,0}; // +0x390 POST_RACE_1 horizontal unit direction captured by the ctor 0x1789E8
    int32_t postRacePhase=0;                  // +0x3A0 POST_RACE_1 sweep frame counter, modulo 1100
};

// Compositor-side shake state (comp+0x254..+0x2B4, +0x2EC; 0x1656B0/0x165938).
struct OriginalCameraShakeState {
    std::array<float,3> period1{},target1{},previous1{},timer1{}; // +0x254, +0x260, +0x26C, +0x278
    std::array<float,3> period2{},target2{},previous2{},timer2{}; // +0x284, +0x290, +0x29C, +0x2A8
    float fadeTimer=0;   // +0x2B4
    int32_t active=0;    // +0x2EC
};

// Outer game camera fields (object at 0x22E418, ctor 0x15DC70) that the chase
// path touches, plus the embedded DEFAULT_3 compositor's look-at/eye/angles.
struct OriginalCameraCompositorState {
    OriginalCameraQuad lookAt{0,0,0,0},eye{0,0,0,1}; // comp+0x20 (= outer+0xE0), comp+0x40 (= outer+0x100)
    float yaw=0,pitch=0;                             // comp+0x50/+0x54
    float fov=0,near=0,far=0;                        // outer+0x00/+0x04/+0x08
    float fovMaximum=0x1.921fb6p-1f;                 // outer+0x0C (pi/4)
    float nearMinimum=30,farMaximum=30000;           // outer+0x10, outer+0x14 (savestate evidence: 30 / 30000)
    int32_t shakeIndex=0;float shakeAmplitude=0;     // outer+0x450, +0x454
    int32_t shakePending=0,shakeWasPending=0;        // outer+0x458, +0x45C
    float lift=0;                                    // outer+0x460
    OriginalCameraQuad lastProbeNormal{0,0,1,0};     // outer+0x470
    OriginalCameraQuad lastUnoccludedEye{0,0,0,1};   // outer+0x490 (override-only occlusion path; kept for completeness)
    int32_t occludedLastFrame=0,firstFrameAfterReset=1; // outer+0x4A0, +0x4A4
    OriginalCameraShakeState shake;
    OriginalRandomState fallbackRandom{{0x2545F491u,0x9D6C0A5Bu,0x7E1A3C77u,0x13579BDFu,0x2468ACE0u,0x0F1E2D3Cu}}; // used only when the input supplies no shared RNG
};

struct OriginalCameraState {
    OriginalChaseAlgorithmState algorithm;
    OriginalCameraCompositorState compositor;
    // Mirrors `new DEFAULT_3` (0x162318/0x176D68 ctor) followed by the
    // set-target sequence 0x176FE0 (0x166C60 reset + update + finish,
    // 0x166550 offset direction, update) and the outer camera ctor state.
    void begin(const OriginalCameraInput&,int variantType=0x3D);
};

struct OriginalCameraOutput {
    OriginalCameraQuad eye{},lookAt{};             // final compositor eye (outer+0x20) and shaken look-at (comp+0x20), cm, Z-up
    OriginalCameraQuad algorithmEye{},algorithmLookAt{}; // algorithm output before collision lift / shake
    float yaw=0,pitch=0;                           // compositor angles (heading from +X about Z in [0,2pi); positive pitch looks down)
    OriginalCameraQuad forward{};                  // unit view direction from the angles (row 0 of RotY(-pitch)*RotZ(-yaw)); roll is always zero
    float fov=0;                                   // radians (full vertical/horizontal aperture as the source uses it; DEFAULT_3 settles at 0.95990783*pi/4)
    float near=0,far=0;                            // cm
    bool resetFired=false;                         // 0x168150 snapped this frame (set-target re-runs next tick)
};

// One 60 Hz gameplay tick of the chase camera. Not time-scaled: every constant
// is per frame at 60 Hz, so call exactly once per fixed gameplay tick.
OriginalCameraOutput originalChaseCameraStep(OriginalCameraState&,const OriginalCameraInput&);

// Source cm / Z-up -> native metres / Y-up via (x,z,-y)*0.01 (engine convention).
struct NativeCameraView {std::array<float,3> eye{},lookAt{};float fovRadians=0,nearMeters=0,farMeters=0;};
inline NativeCameraView nativeCameraView(const OriginalCameraOutput& o){
    auto convert=[](const OriginalCameraQuad& q){return std::array<float,3>{q[0]*.01f,q[2]*.01f,-q[1]*.01f};};
    return {convert(o.eye),convert(o.lookAt),o.fov,o.near*.01f,o.far*.01f};
}

// ---------------------------------------------------------------------------
// Implementation
namespace original_camera {
using Quad=OriginalCameraQuad;
using terrain_original::Rounding;
inline float bits(uint32_t b){return std::bit_cast<float>(b);}
// VU0 macro-mode ops (chop; caller holds a Rounding scope)
inline float vmul(float a,float b){return terrain_original::mul(a,b);}
inline float vadd(float a,float b){return terrain_original::add(a,b);}
inline float vsub(float a,float b){return terrain_original::sub(a,b);}
inline Quad qsub(const Quad& a,const Quad& b){return {vsub(a[0],b[0]),vsub(a[1],b[1]),vsub(a[2],b[2]),vsub(a[3],b[3])};}
inline Quad qadd(const Quad& a,const Quad& b){return {vadd(a[0],b[0]),vadd(a[1],b[1]),vadd(a[2],b[2]),vadd(a[3],b[3])};}
inline Quad qscale(const Quad& a,float s){return {vmul(a[0],s),vmul(a[1],s),vmul(a[2],s),vmul(a[3],s)};}
// vmul.xyzw; vadday.x; vmaddaz.x (x1.0); vmaddw.x (x1.0): ((xx+yy)+zz)+ww, w included.
inline float dot4(const Quad& a,const Quad& b){return vadd(vadd(vadd(vmul(a[0],b[0]),vmul(a[1],b[1])),vmul(a[2],b[2])),vmul(a[3],b[3]));}
inline float vsqrt(float x){return terrain_original::sqrt(x);}
inline float len4(const Quad& a){return vsqrt(dot4(a,a));}
// VU DIV Q = n/d and RSQRT Q = n/sqrt(d); PCSX2 saturates a zero divisor to 0x7F7FFFFF.
inline float vdivQ(float n,float d){if(d==0)return bits(0x7F7FFFFFu);return terrain_original::div(n,d);}
inline float vrsqrtQ(float n,float d){if(d==0)return bits(0x7F7FFFFFu);return terrain_original::div(n,vsqrt(d));}
// vopmula/vopmsub then vsub.w: (a.y*b.z - b.y*a.z, a.z*b.x - b.z*a.x, a.x*b.y - b.x*a.y, 0)
inline Quad cross3(const Quad& a,const Quad& b){return {vsub(vmul(a[1],b[2]),vmul(b[1],a[2])),vsub(vmul(a[2],b[0]),vmul(b[2],a[0])),vsub(vmul(a[0],b[1]),vmul(b[0],a[1])),0};}
inline constexpr Quad zeroQuad{0,0,0,0};   // 0x4FF120
inline constexpr Quad zAxis{0,0,1,0};      // 0x4FF160
// EE scalar FPU
inline float eadd(float a,float b){return originalScalarAdd(a,b);}
inline float esub(float a,float b){return originalScalarSubtract(a,b);}
inline float emul(float a,float b){return terrain_original::mul(a,b);}
inline float ediv(float a,float b){return collision_scalar::divide(a,b);}
inline float esqrt(float x){return collision_scalar::squareRoot(x);}
inline float emin(float a,float b){return a<b?a:b;}
inline float emax(float a,float b){return a>b?a:b;}
inline float clampUnit(float v){return -1.f<=v?emin(v,1.f):-1.f;} // c.le.s -1,v ; min.s
inline constexpr float halfPi=0x1.921fb6p+0f;   // 0x3FC90FDB
inline constexpr float twoPi=0x1.921fb6p+2f;    // 0x40C90FDB
inline constexpr float frameSeconds=0x1.1111120000000p-6f; // 0x3C888889 1/60
inline constexpr float oneKmh=0x1.bc71c80000000p+4f;       // 0x41DE38E4 27.777779 cm/s
// 0x31C128 asin (EE polynomial; coefficients gp+0xDF0..0xDFC).
inline float asinEE(float x){
    Rounding rounding;
    const float c0=bits(0x3E2BBA25),c1=bits(0x3D8A908B),c2=bits(0x3D55033A),c3=bits(0x3D6137AB);
    auto poly=[&](float q){float t=emul(q,c3);t=eadd(c2,t);t=emul(q,t);t=eadd(c1,t);t=emul(q,t);t=eadd(c0,t);return emul(q,t);};
    if(x<bits(0xBF3504F3)){float u2=emul(x,x);u2=esub(1,u2);float u=esqrt(u2);float t=eadd(poly(u2),1);return eadd(emul(u,t),bits(0xBFC90FDB));}
    if(!(bits(0x3F3504F3)<x)){float x2=emul(x,x);float t=poly(x2);t=emul(t,x);return eadd(t,x);}
    float u2=emul(x,x);u2=esub(1,u2);float u=esqrt(u2);float t=eadd(poly(u2),1);return esub(halfPi,emul(u,t));
}
inline std::array<float,2> sincosEE(float x){return collision_scalar::sincos(x);} // 0x31BE50
inline float atanEE(float x){return collision_scalar::atan(x);}                    // 0x31C228
// 3u^2 - 2u^3 with the EE op order used throughout the camera; clamped at 1.
inline float smoothstepEE(float u){float sq=emul(u,u);float f0=eadd(sq,sq);float f1=emul(sq,3.f);f0=emul(f0,u);float s=esub(f1,f0);if(1.f<s)s=1.f;return s;}
// Random unit float of 0x1656B0/0x165938: (bits & 0x7FFFFF | 0x3F800000) - 1.0
inline float unitRandom(OriginalRandomState& r){uint32_t v=r.next();return esub(bits((v&0x7FFFFFu)|0x3F800000u),1.f);}

// Row 0 of RotY(-pitch)*RotZ(-yaw) built by 0x1668B8 (VU products, exact zero terms).
inline Quad directionFromAngles(float yaw,float pitch){
    auto [sp,cp]=sincosEE(-pitch);auto [sy,cy]=sincosEE(-yaw);
    float m22=eadd(esub(1.f,cy),cy);
    return {vmul(cp,cy),vmul(cp,-sy),vmul(sp,m22),0};
}

// Per-frame scratch (the 0x70-byte stack context filled by 0x162568 etc.)
struct Context {
    Quad eyeAtStart{},head{},velocity{},direction{};   // +0x00, +0x10, +0x20, +0x30
    int mode=0;float speed=0,fade=0,boost=0,boostHalf=0,charge=0; // +0x40, +0x44, +0x48, +0x4C, +0x50, +0x54
    bool wipeout=false,mode5=false,airborne=false,mode4=false;   // +0x58, +0x5C, +0x60, +0x64
};

// DEFAULT_3 tuning constants (gp-0x6134..gp-0x607C and 0x176DE0), bit exact.
namespace default3 {
inline constexpr float lo0=0x1.15dfc00000000p-2f /*0x3E8AEFE0*/,hi0=0x1.ccccccp-1f /*0x3F666666*/,zLo0=0x1.2d44a80000000p-2f /*0x3E96A254*/,zHi0=0x1.b3e9c80000000p-1f /*0x3F59F4E4*/;
inline constexpr float lo1=0x1.b333340000000p-1f /*0x3F59999A*/,hi1=0x1.f0a3d80000000p-1f /*0x3F7851EC*/,zLo1=0x1.99999a0000000p-4f /*0x3DCCCCCD*/,zHi1=0x1.3333340000000p-1f /*0x3F19999A*/;
inline constexpr float distanceA=0x1.2cecc80000000p+8f /*0x43967664 300.92493*/,distanceB=0x1.7198440000000p+3f /*0x4138CC22*/,distanceC=0x1.32bf1c0000000p+6f /*0x42995F8E*/,distanceD=0x1.582aaa0000000p+3f /*0x412C1555*/,distanceE=0x1.c44f9c0000000p+0f /*0x3FE227CE*/,distanceF=0x1.d913a60000000p-1f /*0x3F6C89D3*/,distanceG=0x1.d098b20000000p-1f /*0x3F684C59*/;
inline constexpr float verticalA=0x1.75b89a0000000p+3f /*0x413ADC4D*/,verticalB=0x1.f39c120000000p+4f /*0x41F9CE09*/,verticalC=0x1.6c8e700000000p+3f /*0x41364738*/,verticalD=0x1.2ab5c80000000p+3f /*0x41155AE4*/,verticalK=0x1.f4efc40000000p-1f /*0x3F7A77E2*/;
inline constexpr float fovA=0.f,fovF=0x1.eb790a0000000p-1f /*0x3F75BC85 0.95990783*/,fovC=1.f,fovR=0x1.e666660000000p-1f /*0x3F733333*/;
inline constexpr float holdT=0x1.00789c0000000p+1f /*0x40003C4E*/,holdH=0.f,holdKh=0x1.f5c2900000000p-1f /*0x3F7AE148*/,holdKd=0x1.eb851e0000000p-1f /*0x3F75C28F*/;
inline constexpr float lookBase=0x1.f1a4240000000p+4f /*0x41F8D212*/,lookSpeedGain=0x1.7faf120000000p-2f /*0x3EBFD789*/,lookG208=0x1.8aa5200000000p+3f /*0x41455290*/,lookG4C=0x1.52d9900000000p+3f /*0x41296CC8*/,lookK=0x1.d92f8a0000000p-1f /*0x3F6C97C5*/;
inline constexpr float jumpAmpA=0x1.cf9ed40000000p+0f /*0x3FE7CF6A*/,jumpBlendT=0x1.3f53040000000p+1f /*0x401FA982*/,jumpAmpScale=0x1.17607c0000000p-3f /*0x3E0BB03E*/,jumpLandDecay=0x1.b514020000000p-1f /*0x3F5A8A01*/;
inline constexpr float swingReturn=0x1.86e3ae0000000p+0f /*0x3FC371D7 1.5269116*/;
inline constexpr float pullHeight=0x1.24c72c0000000p+7f /*0x43126396 146.389*/,pullK=0x1.f0ff5a0000000p-1f /*0x3F787FAD*/;
inline constexpr float pitchLimitA=0x1.02408a0000000p+4f /*0x41812045 16.140757 deg*/,pitchLimitB=0x1.a9e9ae0000000p+2f /*0x40D4F4D7 6.6548877 deg*/,pitchKeepC=0x1.da548c0000000p-1f /*0x3F6D2A46*/,pitchKeepD=0x1.af652c0000000p-1f /*0x3F57B296*/;
inline constexpr float initialVerticalOffset=0x1.ed80f20000000p+5f /*0x4276C079 61.687962*/,initialOffsetDistance=0x1.17df3c0000000p+9f /*0x440BEF9E 559.74402*/;
// Pause Options "camera select" (0x1F8DF8): Near DEFAULT_2 (0x3C) / Mid DEFAULT_3 (0x3D) / Far
// DEFAULT_4 (0x3E). Drivers 0x176B10/0x176E10/0x177110 call the same helpers with these
// per-variant gp blocks (Near's 5.25 is a lui immediate); every other constant is shared.
struct OriginalChaseVariant {
    float distanceA,distanceB,distanceD,verticalA,verticalB,verticalC,verticalD,fovF;
    float lookBase,lookSpeedGain,lookG208,lookG4C,jumpAmpA,jumpAmpScale,jumpLandDecay,pullHeight;
};
inline const OriginalChaseVariant& originalChaseVariant(int type){
    static const OriginalChaseVariant near{bits(0x435ED885),bits(0x40BD531C),bits(0x40A80000),bits(0x40B7F5B2),bits(0x41D847A0),bits(0x410D2027),bits(0x40FFF388),bits(0x3F7715FF),
        bits(0x426429FD),bits(0x3FC6AC3C),bits(0x41260CDA),bits(0x41211050),bits(0x3FEBA4CF),bits(0x3DC26DB6),bits(0x3F5ACAEF),bits(0x42F1B834)};
    static const OriginalChaseVariant mid{distanceA,distanceB,distanceD,verticalA,verticalB,verticalC,verticalD,fovF,
        lookBase,lookSpeedGain,lookG208,lookG4C,jumpAmpA,jumpAmpScale,jumpLandDecay,pullHeight};
    static const OriginalChaseVariant far{bits(0x43FCE62A),bits(0x4138CC22),bits(0x41361C72),bits(0x415056D3),bits(0x42C5B20E),bits(0x4144B2AA),bits(0x4150EECD),bits(0x3F77210C),
        bits(0x42481BAE),bits(0x4024FFAD),bits(0x415FA39C),bits(0x412BFAF4),bits(0x3FF076DA),bits(0x3DD12B93),bits(0x3F5C2861),bits(0x4327F253)};
    if(type==0x3C)return near;if(type==0x3E)return far;if(type!=0x3D)throw std::runtime_error("Unsupported chase camera variant");return mid;
}
}

// 0x168150 snap guard (also 0x166530).
inline void snapGuard(OriginalChaseAlgorithmState& s,const OriginalCameraInput& in){
    Quad d=qsub(s.lookAt,s.eye);float len=len4(d);
    if(!(len<20.f)&&!(bits(0x60AD78EC)<len))return;
    Quad t1=qscale(in.riderForward,200.f);Quad t2=qsub(in.headPosition,t1);Quad t3=qscale(zAxis,50.f);
    s.resetPending=1;s.eye=qadd(t2,t3);s.lookAt=in.headPosition;
}

// 0x162568 step 1: velocity acquisition, vertical-dependent lag filter, context fill.
inline void stepVelocity(OriginalChaseAlgorithmState& s,Context& c,const OriginalCameraInput& in){
    using namespace default3;
    Quad v=in.velocity;int mode=in.motionMode;
    if(len4(v)<oneKmh)v=s.lastVelocity;
    Quad flat=v;flat[2]=0;s.lastVelocity=v;
    Quad vertical=qsub(v,flat);
    float d=dot4(v,v);Quad dir=zeroQuad;
    if(0.f<d)dir=qscale(v,vrsqrtQ(1.f,d));
    bool heavy=in.surfaceId==2||in.surfaceId==3||in.surfaceId==13;
    float lo=heavy?lo1:lo0,hi=heavy?hi1:hi0,zLo=heavy?zLo1:zLo0,zHi=heavy?zHi1:zHi0;
    float az=std::fabs(dir[2]),t,range;
    if(!(zLo<=az)){range=esub(hi,lo);t=0;}
    else if(!(az<=zHi)){t=1;range=esub(hi,lo);}
    else {float f1=esub(zHi,zLo);float f0=esub(az,zLo);t=ediv(f0,f1);range=esub(hi,lo);}
    float coefficient=eadd(lo,emul(range,t));
    if(mode==1){if(len4(flat)<oneKmh)flat=s.velocityFilter;}
    Quad l7=qscale(s.velocityFilter,coefficient);
    float complement=esub(1.f,coefficient);
    Quad l8=qadd(l7,qscale(flat,complement));
    s.velocityFilter=l8;
    Quad filtered=qadd(l8,vertical);
    c.mode4=mode==4;c.wipeout=mode==2;c.mode5=mode==5;c.airborne=mode==1;c.mode=mode;
    c.boost=in.boostLevel;c.charge=in.jumpCharge;
    c.eyeAtStart=s.eye;c.head=in.headPosition;
    bool positive=0.f<c.boost;
    c.velocity=filtered;c.speed=len4(filtered);
    c.boostHalf=positive?emul(eadd(c.boost,1.f),.5f):0.f;
    s.field2F4=c.boost;s.field2F8=c.boostHalf;s.field2FC=c.mode5?1:0;
}

// 0x162998 step 2: mode-5 fade timer.
inline void stepModeFiveFade(OriginalChaseAlgorithmState& s,Context& c){
    if(c.mode5){s.mode5Frames=0;c.fade=1;return;}
    int32_t n=s.mode5Frames;if(n<820)s.mode5Frames=n+1;n=s.mode5Frames;
    float f3=1;
    if(!(n<120)){float f0=float(n);f0=esub(f0,120.f);f0=emul(f0,bits(0x3ABB3EE7));f3=esub(f3,f0);}
    if(f3<0.f)f3=0;
    c.fade=f3;
}

// 0x162A20 step 3: take-off detection / airborne latch / jump timestamps.
inline void stepTakeoff(OriginalChaseAlgorithmState& s,Context& c,const OriginalCameraInput& in){
    if(c.airborne){
        if(s.airborneLatch!=0){s.lastAirTick=int32_t(in.tick);return;}
        if(0.f<c.speed){
            Quad l0=c.velocity;float f1=len4(l0);l0=qscale(l0,vdivQ(1.f,f1));
            if(bits(0x3F59999A)<l0[2]){if(in.previousContactNormal[2]<bits(0x3DCCCCCD)){s.swingElapsed=0;s.wallLaunch=1;}}
        }
        s.airborneLatch=1;
        bool stale=!((int32_t(in.tick)-s.lastAirTick)<31);
        if(s.phaseBActive==0&&stale)s.takeoffCountdown=15;
    } else if(s.airborneLatch!=0){s.airborneLatch=0;s.wallLaunch=0;s.swingInitialised=0;}
    if(c.airborne)s.lastAirTick=int32_t(in.tick);
}

// 0x162C78 step 5: travel-direction filter with the PID-adapted coefficient.
inline void stepDirection(OriginalChaseAlgorithmState& s,Context& c,const OriginalCameraInput& in){
    Quad result;
    if(oneKmh<c.speed){
        Quad raw=in.velocity;
        if(dot4(raw,raw)<1.f)raw=s.lastRawVelocity;
        s.lastRawVelocity=raw;
        float a=dot4(s.direction,s.direction),b=dot4(s.lastRawVelocity,s.lastRawVelocity);
        float f3=esqrt(emul(a,b));float alignment;
        if(!(f3<bits(0x3A83126F))){
            float cosine=ediv(dot4(s.direction,s.lastRawVelocity),f3);
            alignment=esub(halfPi,asinEE(clampUnit(cosine)));
        } else alignment=0;
        int idx=s.ringIndex,prev=(idx+4)%5;
        float x=esub(alignment,bits(0x3E436792));x=emax(x,0.f);
        float yPrev=s.ringY[prev];float f3b=esub(x,yPrev);
        float sumE=0;for(int i=0;i<5;++i)sumE=eadd(sumE,s.ringE[i]);
        float p=emul(f3b,s.gainP),integral=emul(sumE,s.gainI);
        float y=eadd(yPrev,p);y=eadd(y,integral);
        float xPrev=s.ringX[prev];s.ringX[idx]=x;
        float dterm=emul(esub(xPrev,x),s.gainD);y=eadd(y,dterm);
        s.ringY[idx]=y;
        float error=esub(x,y);
        float g=emul(y,bits(0x40215ED5));g=emin(g,1.f);g=emax(g,0.f);
        s.ringE[idx]=error;
        g=esub(1.f,g);
        s.ringIndex=(idx+1)%5;
        g=emul(g,bits(0x3D21A510));
        float coefficient=eadd(g,bits(0x3F749D64));
        if(uint32_t(in.riderType-11)<3u){coefficient=esqrt(coefficient);coefficient=esqrt(coefficient);}
        float complement=esub(1.f,coefficient);float inverse=ediv(1.f,c.speed);
        Quad l2=qscale(c.velocity,inverse);Quad l5=qscale(s.direction,coefficient);Quad l6=qscale(l2,complement);
        s.direction=qadd(l5,l6);result=s.direction;
    } else result=s.direction;
    float l=len4(result);c.direction=qscale(result,vdivQ(1.f,l));
}

// 0x163010 step 6: follow distance (speed + boost) and eye placement.
inline void stepDistance(OriginalChaseAlgorithmState& s,Context& c,const OriginalCameraInput& in){
    using namespace default3;const auto& V=originalChaseVariant(s.variantType);const float distanceA=V.distanceA,distanceB=V.distanceB,distanceD=V.distanceD;
    const float A=distanceA,B=distanceB,C=distanceC,D=distanceD,E=distanceE,F=distanceF,G=distanceG;
    float f0=esub(c.speed,C);if(f0<0.f)f0=0;
    float f3=esqrt(f0);float f1=emul(f0,bits(0x38D1B717));float boost=c.boost;float last=s.lastBoost;
    float f2=emul(A,5.f);f1=emul(B,f1);bool rose=last<boost;f1=emul(f1,f3);
    float target=eadd(A,f1);float m=emax(target,A);target=emin(f2,m);
    if(rose)s.boostBump=emul(D,esqrt(boost));
    float bump=s.boostBump;float current=s.distance;bump=esub(bump,E);s.lastBoost=c.boost;bump=emax(bump,0.f);
    target=eadd(target,bump);bool shrinking=target<current;s.boostBump=bump;
    float k=shrinking?G:F;
    float length=len4(c.direction);
    float f1b=emul(k,current);float f0b=esub(1.f,k);f0b=emul(f0b,target);f1b=eadd(f1b,f0b);
    s.distance=f1b;
    float scale=ediv(f1b,length);
    s.eye=qsub(s.lookAt,qscale(c.direction,scale));
    snapGuard(s,in);
}

// 0x163270: eye vertical offset from speed.
inline void stepVerticalOffset(OriginalChaseAlgorithmState& s,Context& c,const OriginalCameraInput& in){
    using namespace default3;const auto& V=originalChaseVariant(s.variantType);const float verticalA=V.verticalA,verticalB=V.verticalB,verticalC=V.verticalC,verticalD=V.verticalD;
    const float A=verticalA,B=verticalB,C=verticalC,D=verticalD,K=verticalK;
    float f0=esub(c.speed,bits(0x44AD9C72));float excess=(0.f<=f0)?f0:0.f;
    float t;
    if(bits(0x44505556)<=c.speed){if(c.speed<=bits(0x44D05556))t=ediv(esub(c.speed,bits(0x44505556)),bits(0x44505556));else t=1;}
    else t=0;
    float t2=emul(t,t);float held;
    if(!c.airborne){float v=c.charge;float m=emul(t2,D);v=esub(v,.5f);held=emul(m,v);}
    else held=s.heldEyeTerm;
    float f2=bits(0x38D1B717);float sqrtExcess=esqrt(excess);s.heldEyeTerm=held;
    f2=emul(excess,f2);float f3=1;float f0b=esqrt(c.boost);f3=esub(f3,K);f2=emul(A,f2);
    float f1=s.eyeVerticalOffset;f0b=emul(C,f0b);f1=emul(K,f1);f2=emul(f2,sqrtExcess);f0b=emax(held,f0b);
    f2=esub(f2,B);f2=eadd(f2,f0b);f3=emul(f3,f2);f1=eadd(f1,f3);
    s.eyeVerticalOffset=f1;
    s.eye=qsub(s.eye,qscale(zAxis,f1));
    snapGuard(s,in);
}

// 0x1633B0: FOV from speed, filtered.
inline void stepFov(OriginalChaseAlgorithmState& s,Context& c,const OriginalCameraInput& in){
    using namespace default3;const float fovF=originalChaseVariant(s.variantType).fovF;
    float A=fovA,F=fovF,C=fovC,R=fovR;
    float f0=bits(0x33D6BF95);float f3=1;float speed=c.speed;
    C=esub(C,f3);float q=c.boost;f0=emul(speed,f0);int mode=c.mode;C=emul(C,q);A=emul(A,f0);A=emul(A,speed);A=eadd(A,f3);A=eadd(A,C);
    float rate=mode==1?1.f:mode==0?R:bits(0x3F7FBE77);
    float f0b=esub(1.f,rate);float f1=s.fovScale;float f2=bits(0x3F490FDB);
    f1=emul(rate,f1);f2=emul(F,f2);f0b=emul(f0b,A);f1=eadd(f1,f0b);f2=emul(f2,f1);
    s.fovScale=f1;s.fov=f2;
    snapGuard(s,in);
}

// 0x163450: mode-5 eye hold/blend + extra eye height.
inline void stepModeFiveHold(OriginalChaseAlgorithmState& s,Context& c,const OriginalCameraInput& in){
    using namespace default3;
    const float T=holdT,H=holdH,Kh=holdKh,Kd=holdKd;
    // Listing 0x163458: `beqz ctx+0x5C` skips to the timer path, i.e. the offset
    // is recorded WHILE in mode 5 and the blend back runs after leaving it.
    if(c.mode5){s.savedOffset=qsub(s.lookAt,s.eye);s.mode5Timer=0;}
    else {
        float timer=s.mode5Timer;
        if(timer<5.f){
            timer=eadd(timer,frameSeconds);bool cc=timer<T;s.mode5Timer=timer;
            if(cc){
                float u=ediv(timer,T);float f0=emul(u,u);float f1=eadd(f0,f0);f0=emul(f0,3.f);f1=emul(f1,u);float sm=esub(f0,f1);if(1.f<sm)sm=1;
                Quad a=qsub(s.lookAt,s.savedOffset);float rest=esub(1.f,sm);a=qscale(a,rest);Quad b=qscale(s.eye,sm);s.eye=qadd(a,b);
            }
        }
    }
    s.headCopy=c.head;
    float h;
    if(!c.mode5)h=emul(Kd,s.extraEyeHeight);
    else {float f0=esub(1.f,Kh);h=emul(Kh,s.extraEyeHeight);f0=emul(f0,H);h=eadd(h,f0);}
    float ez=s.eye[2];s.extraEyeHeight=h;s.eye[2]=eadd(ez,h);
    snapGuard(s,in);
}

// 0x162B90: look-at vertical offset.
inline void stepLookAtHeight(OriginalChaseAlgorithmState& s,Context& c,const OriginalCameraInput& in){
    using namespace default3;const auto& V=originalChaseVariant(s.variantType);const float lookBase=V.lookBase,lookSpeedGain=V.lookSpeedGain,lookG208=V.lookG208,lookG4C=V.lookG4C;
    const float Base=lookBase,SpeedGain=lookSpeedGain,G208=lookG208,G4C=lookG4C,K=lookK;
    float f1=esub(c.speed,bits(0x44AD9C72));float excess=(0.f<=f1)?f1:0.f;
    float st=emul(excess,bits(0x3A06D121));st=emin(st,1.f);
    float v208;
    if(!c.airborne){v208=c.charge;s.heldLookTerm=v208;}else v208=s.heldLookTerm;
    float f1b=emul(st,SpeedGain);float f4=emul(G208,v208);float f3=1;float f2=c.boost;
    f1b=eadd(Base,f1b);float f0=s.lookAtHeight;f2=emul(f2,G4C);f3=esub(f3,K);f0=emul(K,f0);
    f1b=eadd(f1b,f2);f1b=eadd(f1b,f4);f3=emul(f3,f1b);f0=eadd(f0,f3);
    s.lookAtHeight=f0;
    s.lookAt=qadd(s.lookAt,qscale(zAxis,f0));
    snapGuard(s,in);
}

// Target slot 0xB8 = 0x15F780: angle between the trajectory normal and the reversed heading.
inline float landingAngle(const OriginalCameraInput& in){
    const Quad& a=in.trajectoryNormal;const Quad& b=in.trajectoryHeading;
    if(!(0.f<dot4(a,a))||!(0.f<dot4(b,b)))return 0;
    Quad cq=qscale(b,-1.f);
    float f1=dot4(cq,cq);float f0=dot4(a,a);f0=emul(f0,f1);f1=esqrt(f0);
    if(f1<bits(0x3A83126F))return 0;
    float cosine=ediv(dot4(a,cq),f1);
    return esub(halfPi,asinEE(clampUnit(cosine)));
}

// Knot tables of the jump camera splines (gp-0x6A98.. / gp-0x69F0..).
struct JumpKnots {std::array<float,13> lookX,lookY,eyeX,eyeY;};
inline const JumpKnots& jumpKnots(){
    static const JumpKnots k{
        {0.f,bits(0x3EA8F5C3),bits(0x3F28F5C3),bits(0x3F7FB6F5),bits(0x3FAA3D71),bits(0x3FD47AE1),bits(0x40000000),bits(0x40151EB8),bits(0x402A3D71),bits(0x40400000),bits(0x40551EB8),bits(0x406A3D71),bits(0x40800000)},
        {0.f,bits(0xC2F244F3),bits(0xC359BD78),bits(0xC37EF984),bits(0xC37A979B),bits(0xC34161AE),bits(0xC27C1E39),bits(0x42DD9BBF),bits(0x4399BBDE),bits(0x43F8398A),bits(0x442C699B),bits(0x444D2223),bits(0x4454591A)},
        {0.f,bits(0x3EA8F5C3),bits(0x3F28F5C3),bits(0x3F800000),bits(0x3FAA1CAD),bits(0x3FD46A7F),bits(0x4000011A),bits(0x40155B88),bits(0x402A43C2),bits(0x40400000),bits(0x40551EB8),bits(0x406A381F),bits(0x40800000)},
        {0.f,bits(0xC1B08260),bits(0xC20A8581),bits(0x415D2C5D),bits(0x42B841CC),bits(0x435530F5),bits(0x43BFBB22),bits(0x441C121D),bits(0x444B4541),bits(0x44746A28),bits(0x4485DE4B),bits(0x448A18A3),bits(0x448D6DC9)}};
    return k;
}

// 0x163E8C..0x163FD0: touchdown latch and decay of retained jump offsets.
// The angle getter is evaluated only on the first grounded tick.
template<class Angle> inline void stepJumpLanding(OriginalChaseAlgorithmState& s,bool airborne,Angle&& angle){
    // ground / landing
    if(!airborne){
        if(s.landed!=0){s.phaseBActive=0;s.landed=1;s.phaseAActive=0;}
        else {
            float ca=s.phaseAClock,w;
            if(bits(0x3F99999A)<=ca){if(ca<=bits(0x400CCCCD))w=esub(ca,bits(0x3F99999A));else w=1;}else w=0;
            float f1=emul(w,default3::originalChaseVariant(s.variantType).jumpLandDecay);float f0=esub(1.f,w);f0=emul(f0,bits(0x3F6E147B));f1=eadd(f1,f0);
            s.landingDecay=f1;
            float ang=angle();float dcy=s.landingDecay;
            f0=esub(1.f,dcy);bool cc=bits(0x3F866666)<=ang;f0=emul(f0,.5f);float mid=eadd(dcy,f0);
            float kk;
            if(cc){if(ang<=bits(0x3FB33333))kk=ediv(esub(ang,bits(0x3F866666)),bits(0x3EB33334));else kk=1;}else kk=0;
            f1=emul(kk,mid);dcy=s.landingDecay;f0=esub(1.f,kk);f0=emul(f0,dcy);f1=eadd(f1,f0);
            s.landingDecay=f1;
            s.phaseBActive=0;s.landed=1;s.phaseAActive=0;
        }
    }
    if(s.landed!=0){
        float d=s.landingDecay;
        s.lookOffsetB=emul(s.lookOffsetB,d);s.eyeOffsetB=emul(s.eyeOffsetB,d);s.lookOffsetA=emul(s.lookOffsetA,d);s.eyeOffsetA=emul(s.eyeOffsetA,d);
    }
}

// 0x1635F8: jump ("air") camera: spline-driven vertical offsets for eye and look-at.
inline void stepJumpCamera(OriginalChaseAlgorithmState& s,Context& c,const OriginalCameraInput& in){
    using namespace default3;const auto& V=originalChaseVariant(s.variantType);const float jumpAmpA=V.jumpAmpA,jumpAmpScale=V.jumpAmpScale;
    const float AmpA=jumpAmpA,BlendT=jumpBlendT,AmpScale=jumpAmpScale;
    const JumpKnots& k=jumpKnots();
    Quad d0=qsub(s.lookAt,s.eye);float distPrev=len4(d0);
    if(c.airborne&&s.wallLaunch==0){
        float elapsed=0;
        int32_t n=s.takeoffCountdown-1;s.takeoffCountdown=n;
        if(n>0&&in.trajectoryStatusActive){int32_t c2=s.takeoffCountdown;s.takeoffCountdown=0;elapsed=emul(float(16-c2),frameSeconds);}
        if(s.takeoffCountdown==14){
            s.phaseAClock=frameSeconds;
            float j=in.launchValue,ramp;
            if(300.f<=j){if(j<=500.f)ramp=ediv(esub(j,300.f),200.f);else ramp=1;}else ramp=0;
            float f0=emul(AmpScale,AmpA);float ampA=emul(f0,ramp);s.takeoffRamp=ramp;
            if(0.f<ampA){
                s.lookSplineA.reset(13);for(int i=0;i<13;++i)s.lookSplineA.add(k.lookX[i],i?emul(ampA,k.lookY[i]):0.f);
                s.eyeSplineA.reset(13);for(int i=0;i<13;++i)s.eyeSplineA.add(k.eyeX[i],i?emul(ampA,k.eyeY[i]):0.f);
                s.phaseBDuration=4;s.phaseAActive=1;s.timeGain=1;s.amplitudeGain=1;s.landed=0;
            }
        } else if(s.takeoffCountdown==0){
            s.phaseBClock=elapsed;
            float T=in.predictedAirTime;float q=emul(T,.25f);float r;
            if(bits(0x3FD9999A)<=T){
                if(T<=bits(0x40333333)){float u=ediv(esub(T,bits(0x3FD9999A)),bits(0x3F8CCCCC));float f1=eadd(u,u);float f0=emul(u,u);f1=emul(f1,f0);f0=emul(f0,3.f);r=esub(f0,f1);if(1.f<r)r=1;}
                else r=1;
            } else r=0;
            float g=emul(r,AmpScale);float rampA=s.takeoffRamp;float ampB=emul(g,rampA);
            if(0.f<ampB){
                std::array<float,13> lx,ex;
                lx[0]=0;for(int i=1;i<13;++i)lx[i]=i==6?eadd(q,q):emul(q,k.lookX[i]);
                ex[0]=0;ex[1]=lx[1];ex[2]=lx[2];ex[3]=q;for(int i=4;i<13;++i)ex[i]=emul(q,k.eyeX[i]);ex[9]=lx[9];ex[10]=lx[10];ex[12]=lx[12];
                s.lookSplineB.reset(13);for(int i=0;i<13;++i)s.lookSplineB.add(lx[i],i?emul(ampB,k.lookY[i]):0.f);
                s.eyeSplineB.reset(13);for(int i=0;i<13;++i)s.eyeSplineB.add(ex[i],i?emul(ampB,k.eyeY[i]):0.f);
                s.phaseBActive=1;s.timeGain=1;s.phaseBDuration=T;s.amplitudeGain=1;s.landed=0;
            }
        }
    }
    // per-frame evaluation
    if(s.phaseAActive!=0){
        float ca=s.phaseAClock;
        if(bits(0x407EEEEF)<=ca)s.phaseAClock=4;
        else {ca=eadd(ca,frameSeconds);ca=emin(ca,4.f);s.phaseAClock=ca;}
        float xa=emul(s.timeGain,s.phaseAClock);float eA=s.eyeSplineA.evaluate(xa);
        float xb=emul(s.timeGain,s.phaseAClock);float g=emul(s.amplitudeGain,eA);float lA=s.lookSplineA.evaluate(xb);
        s.eyeOffsetA=g;s.lookOffsetA=emul(s.amplitudeGain,lA);
    }
    if(s.phaseBActive!=0){
        float dur=s.phaseBDuration,cb=s.phaseBClock;float lim=esub(dur,frameSeconds);
        if(lim<=cb)s.phaseBClock=dur;else {cb=eadd(cb,frameSeconds);cb=emin(cb,dur);s.phaseBClock=cb;}
        float xa=emul(s.timeGain,s.phaseBClock);float eB=s.eyeSplineB.evaluate(xa);
        float xb=emul(s.timeGain,s.phaseBClock);float g=emul(s.amplitudeGain,eB);float lB=s.lookSplineB.evaluate(xb);
        s.eyeOffsetB=g;s.lookOffsetB=emul(s.amplitudeGain,lB);
    }
    stepJumpLanding(s,c.airborne,[&](){return landingAngle(in);});
    // apply offsets, restore distance
    float wB=0;
    if(s.phaseBActive!=0){wB=ediv(s.phaseBClock,BlendT);wB=emin(wB,1.f);}
    Quad d1=qsub(s.lookAt,s.eye);float dist=len4(d1);
    float f4=s.lookOffsetA;float f3=1;dist=emul(dist,bits(0x3B5A740E));float f0=s.lookOffsetB;f3=esub(f3,wB);float f1=s.eyeOffsetB;float f5=s.eyeOffsetA;
    f4=emul(dist,f4);f0=emul(dist,f0);f1=emul(dist,f1);f4=emul(f3,f4);f0=emul(wB,f0);float f2=emul(dist,f5);f1=emul(wB,f1);f0=eadd(f0,f4);
    s.lookAt=qadd(s.lookAt,qscale(zAxis,f0));
    f3=emul(f3,f2);f1=eadd(f1,f3);
    s.eye=qadd(s.eye,qscale(zAxis,f1));
    snapGuard(s,in);
    Quad d2=qsub(s.lookAt,s.eye);float want=emax(distPrev,50.f);float now=len4(d2);float sc=ediv(want,now);
    s.eye=qsub(s.lookAt,qscale(d2,sc));
}

// Ease curve of the swing camera (9 clamped knots, gp-0x6904..).
inline void buildSwingSpline(OriginalCubicSpline& sp){
    static const float y[9]={0.f,bits(0x3D830410),bits(0x3E9D9ED8),bits(0x3F0D86D3),bits(0x3F2861D0),bits(0x3F3A9776),bits(0x3F53BBF7),bits(0x3F71021C),1.f};
    static const float x[9]={0.f,bits(0x3E000000),bits(0x3E800000),bits(0x3EC00000),bits(0x3F000000),bits(0x3F200000),bits(0x3F400000),bits(0x3F600000),1.f};
    sp.resetClamped(9,0.f,0.f);for(int i=0;i<9;++i)sp.add(x[i],y[i]);
}
// VU quaternion rotation used by 0x164878: v + 2w(q x v) + 2(q x (q x v)), accumulated in that order.
inline Quad rotateByQuaternion(const Quad& v,const Quad& q){
    Quad c1=cross3(q,v);Quad c2=cross3(q,c1);
    Quad acc=v;acc=qadd(acc,qscale(c1,q[3]));acc=qadd(acc,qscale(c1,q[3]));acc=qadd(acc,c2);acc=qadd(acc,c2);
    acc[3]=0;return acc;
}

// 0x164878: wall-launch swing-around camera, return blend, 60 % minimum-distance guard.
inline void stepSwing(OriginalChaseAlgorithmState& s,Context& c,const OriginalCameraInput& in){
    const float returnTime=default3::swingReturn;
    Quad d0=qsub(s.lookAt,s.eye);Quad e0=s.eye;
    int32_t armed;
    if(s.wallLaunch!=0){
        bool stepping=s.swingInitialised!=0;
        if(!stepping){
            s.swingElapsed=eadd(s.swingElapsed,frameSeconds);
            if(in.trajectoryStatusActive){
                s.swingStartView=qsub(s.lookAt,s.eye);
                float f21=len4(s.swingStartView);
                float f0=esub(in.predictedAirTime,s.swingElapsed);
                if(bits(0x40133333)<=f0)s.swingDuration=esub(in.predictedAirTime,s.swingElapsed);
                else s.swingDuration=esub(bits(0x40133333),s.swingElapsed);
                s.returnTimer=returnTime;s.swingClock=0;
                Quad R=in.wallNormal;
                Quad H=cross3(zAxis,R);
                float l=len4(H);H=qscale(H,vdivQ(1.f,l));
                if(dot4(H,c.velocity)<0.f)H=qscale(H,-1.f);
                l=len4(H);float f0b=ediv(f21,l);Quad Hs=qscale(H,f0b);
                float rr=dot4(R,R);float vr=dot4(s.swingStartView,R);float f1=ediv(vr,rr);
                Quad pr=qscale(R,f1);Quad p2=qscale(pr,2.f);
                Quad T=qsub(s.swingStartView,p2);
                T[2]=-T[2];
                if(dot4(T,T)<=0.f)T=Hs;
                T=qscale(T,bits(0x3F4CCCCD));Quad hp=qscale(Hs,bits(0x3E4CCCCD));T=qadd(T,hp);
                l=len4(T);f0b=ediv(f21,l);T=qscale(T,f0b);
                rr=dot4(R,R);float tr=dot4(T,R);f1=ediv(tr,rr);
                s.swingWallProjection=qscale(R,f1);
                Quad tp=qsub(T,s.swingWallProjection);
                f0b=dot4(tp,tp);
                if(f0b<bits(0x471C4000)){
                    tp[2]=0;float f2=len4(tp);
                    if(1.f<f2){f0b=ediv(200.f,f2);tp=qscale(tp,f0b);T=qadd(T,tp);T=qsub(T,qscale(zAxis,100.f));}
                }
                s.swingTargetView=T;
                s.swingAxis=cross3(s.swingStartView,s.swingTargetView);
                l=len4(s.swingAxis);s.swingAxis=qscale(s.swingAxis,vdivQ(1.f,l));
                float f1c=dot4(s.swingStartView,s.swingStartView);float f0c=dot4(s.swingTargetView,s.swingTargetView);
                f1c=emul(f1c,f0c);f1c=esqrt(f1c);
                if(f1c<bits(0x3A83126F))s.swingAngle=0;
                else {float cosine=ediv(dot4(s.swingStartView,s.swingTargetView),f1c);s.swingAngle=esub(halfPi,asinEE(clampUnit(cosine)));}
                buildSwingSpline(s.swingSpline);
                s.swingInitialised=1;
            }
            stepping=s.swingInitialised!=0;
        }
        if(stepping){
            float f1=eadd(s.swingClock,frameSeconds);s.swingClock=f1;float f2=s.swingDuration;
            if(f1<f2){
                float t=ediv(f1,f2);float curve=s.swingSpline.evaluate(t);
                float half=emul(curve,s.swingAngle);half=emul(half,.5f);
                auto [sn,cs]=sincosEE(half);
                Quad q{emul(sn,s.swingAxis[0]),emul(sn,s.swingAxis[1]),emul(sn,s.swingAxis[2]),cs};
                s.swingRotatedView=rotateByQuaternion(s.swingStartView,q);
                s.swingCurveProjection=qscale(s.swingWallProjection,curve);
            }
            float f0=emul(s.swingDuration,bits(0x3E99999A));float u=ediv(s.swingClock,f0);u=emin(u,1.f);u=emax(u,0.f);
            float sm=smoothstepEE(u);float rest=esub(1.f,sm);
            s.lookAt=qadd(qscale(s.lookAt,rest),qscale(c.head,sm));
            s.eye=qadd(qsub(s.lookAt,s.swingRotatedView),s.swingCurveProjection);
        }
        armed=s.wallLaunch;
    } else {
        float f3=s.returnTimer;
        if(0.f<f3){
            float f0=esub(f3,frameSeconds);f0=emax(f0,0.f);float u=ediv(f0,returnTime);s.returnTimer=f0;
            float sm=smoothstepEE(u);
            Quad oldLook=s.lookAt;float rest=esub(1.f,sm);
            s.lookAt=qadd(qscale(s.lookAt,rest),qscale(c.head,sm));
            Quad e1=qscale(s.eye,rest);Quad t1=qsub(c.head,s.swingRotatedView);Quad t2=qadd(t1,s.swingCurveProjection);Quad t3=qscale(t2,sm);
            s.eye=qadd(e1,t3);
            s.returnView=qsub(s.lookAt,s.eye);s.returnLookDelta=qsub(s.lookAt,oldLook);
            s.residualTimer=s.returnTimer;s.residualStart=s.returnTimer;
        }
        armed=s.wallLaunch;
    }
    if(armed!=0){
        float f4=s.residualTimer;
        if(0.f<f4){
            float f1=s.residualStart;
            if(0.f<f1){
                float f0=esub(f4,frameSeconds);f0=emax(f0,0.f);float u=ediv(f0,f1);s.residualTimer=f0;
                float sm=smoothstepEE(u);
                Quad d=qsub(s.lookAt,s.eye);float rest=esub(1.f,sm);
                Quad a1=qscale(s.returnView,sm);Quad a2=qscale(d,rest);Quad vb=qadd(a1,a2);
                Quad a3=qscale(s.returnLookDelta,sm);
                s.lookAt=qadd(s.lookAt,a3);s.eye=qsub(s.lookAt,vb);
            }
        }
    }
    Quad d=qsub(s.lookAt,s.eye);
    float f0=len4(d0),f2=len4(d);float f1=emul(f0,bits(0x3F19999A));
    if(f2<f1){
        if(1.f<f2){float sc=ediv(f1,f2);d=qscale(d,sc);s.eye=qsub(s.lookAt,d);}
        else s.eye=e0;
    }
    snapGuard(s,in);
}

// 0x1646A0: velocity-aligned pull-behind, filtered (mode 4 only).
inline void stepPullBehind(OriginalChaseAlgorithmState& s,Context& c,const OriginalCameraInput& in){
    using namespace default3;const float pullHeight=originalChaseVariant(s.variantType).pullHeight;
    Quad e1;
    if(0.f<c.speed){
        Quad d=qsub(s.lookAt,s.eye);Quad v=c.velocity;
        float f0=len4(d);float f1=len4(v);f0=ediv(f0,f1);
        Quad scaled=qscale(v,f0);Quad zs=qscale(zAxis,pullHeight);Quad w=qsub(scaled,zs);
        e1=qsub(s.lookAt,w);
    } else e1=s.eye;
    Quad delta=qsub(e1,s.eye);Quad effective=c.mode4?delta:zeroQuad;
    Quad p=qscale(s.pullBehind,pullK);float f0=esub(1.f,pullK);Quad q=qscale(effective,f0);
    s.pullBehind=qadd(p,q);
    s.eye=qadd(s.eye,s.pullBehind);
    snapGuard(s,in);
}

// 0x1641C0: filtered side/up/forward eye push while rider+0x5AC is set (600, 300, 100, 0.98).
inline void stepProximityPush(OriginalChaseAlgorithmState& s,Context&,const OriginalCameraInput& in){
    const float side=600.f,forwardDist=300.f,up=100.f,k=bits(0x3F7AE148);
    Quad d=qsub(s.lookAt,s.eye);
    float f3=in.proximityFlag?1.f:0.f;
    float f0=esub(1.f,k);float f1=emul(k,s.pushFilter);f0=emul(f0,f3);float f15=eadd(f1,f0);
    s.pushFilter=f15;
    f0=emul(f15,forwardDist);
    Quad n=qscale(d,vrsqrtQ(1.f,dot4(d,d)));
    f1=emul(f15,up);
    Quad a=qscale(n,f0);a[2]=eadd(a[2],f1);
    Quad b=cross3(zAxis,a);
    f0=dot4(b,b);
    if(0.f<f0){f0=emul(f15,side);float l=len4(b);f0=ediv(f0,l);b=qscale(b,f0);}
    s.eye=qadd(s.eye,qadd(a,b));
    snapGuard(s,in);
}

// 0x1643A8: slow/stopped camera lock state machine (15 km/h, 1.5 s, 1.5 s).
inline void stepSlowLock(OriginalChaseAlgorithmState& s,Context& c,const OriginalCameraInput& in){
    const float threshold=15.f,lockIn=1.5f,release=1.5f;
    float kmh=emul(c.speed,bits(0x3D1374BC));
    int slow=0;
    if(kmh<threshold){if(!c.airborne)slow=c.mode5?0:1;}
    int32_t st=s.lockState;
    if(st==3){if(slow){s.lockState=0;s.lockBlend=0;s.lockView=qsub(s.lookAt,s.eye);}}
    else if(st==2){if(!slow){s.lockState=1;s.lockBlend=1;}}
    else if(st==0){if(!slow)s.lockState=1;}
    else if(st==1){if(slow)s.lockState=0;}
    st=s.lockState;
    if(st==0){float f0=ediv(frameSeconds,lockIn);float f1=eadd(s.lockBlend,f0);f1=emin(f1,1.f);s.lockBlend=f1;if(f1==1.f)s.lockState=2;}
    else if(st==1){float f0=ediv(frameSeconds,release);float f1=esub(s.lockBlend,f0);f1=emax(f1,0.f);s.lockBlend=f1;if(f1==0.f)s.lockState=3;}
    st=s.lockState;
    if(st==3){snapGuard(s,in);return;}
    float sm=smoothstepEE(s.lockBlend);
    Quad d=qsub(s.lookAt,s.eye);float rest=esub(1.f,sm);
    Quad p=qscale(d,rest);Quad qv=qscale(s.lockView,sm);Quad bv=qadd(p,qv);
    float f0=len4(d);float f2=len4(bv);f0=emul(f0,.5f);float f1=emul(f2,.5f);f0=eadd(f0,f1);f0=ediv(f0,f2);
    bv=qscale(bv,f0);
    Quad l1=qscale(s.lookAt,rest);Quad l2=qscale(c.head,sm);
    s.lookAt=qadd(l1,l2);
    s.eye=qsub(s.lookAt,bv);
    snapGuard(s,in);
}

// 0x176E10: DEFAULT_3 per-frame update driver (vtable slot 0x28).
inline void chaseUpdate(OriginalChaseAlgorithmState& s,const OriginalCameraInput& in){
    Context c;
    stepVelocity(s,c,in);
    stepModeFiveFade(s,c);
    stepTakeoff(s,c,in);
    s.lookAt=c.head;                    // 0x162B80
    stepDirection(s,c,in);
    stepDistance(s,c,in);
    stepVerticalOffset(s,c,in);
    stepFov(s,c,in);
    stepModeFiveHold(s,c,in);
    stepLookAtHeight(s,c,in);
    stepJumpCamera(s,c,in);
    stepSwing(s,c,in);
    stepPullBehind(s,c,in);
    stepProximityPush(s,c,in);
    stepSlowLock(s,c,in);
}

// 0x31C040 cosine (independent polynomial; add.s with the EE guard bit, mul.s chop).
inline float cosEE(float x){
    Rounding rounding;
    float scaled=emul(x,bits(0x3F22F983));scaled=x<0?esub(scaled,.5f):eadd(scaled,.5f);int quadrant=int(scaled);
    x=esub(x,emul(float(quadrant),halfPi));float square=emul(x,x),value;
    if(quadrant&1){value=emul(square,bits(0x3638EF1F));value=eadd(value,bits(0xB9500D03));value=emul(value,square);value=eadd(value,bits(0x3C088889));
        value=emul(value,square);value=eadd(value,bits(0xBE2AAAAB));value=emul(value,square);value=emul(value,x);value=eadd(value,x);}
    else {value=emul(square,bits(0x37D00D03));value=eadd(value,bits(0xBAB60B62));value=emul(value,square);value=eadd(value,bits(0x3D2AAAAB));
        value=emul(value,square);value=eadd(value,-.5f);value=emul(value,square);value=eadd(value,1.f);}
    return ((quadrant+1)&2)?-value:value;
}

// POST_RACE_1 (type 0x44, vtable 0x45CBD8): the finish camera. Constants from gp-0x5D80..-0x5D4C.
namespace post_race {
inline constexpr int32_t period=1100;                       // 0x44C frames per sweep
inline constexpr float inversePeriod=std::bit_cast<float>(0x3A6E500Fu);    // gp-0x5D58 1/1100
inline constexpr float inverseTwoPi=std::bit_cast<float>(0x3E22F983u);     // gp-0x5D54 1/(2 pi)
inline constexpr float yawAmplitude=std::bit_cast<float>(0x3F3F8F34u);     // gp-0x5D50 0.748278856 rad (gp-0x5D4C is its negation)
inline constexpr float directionMinimum=std::bit_cast<float>(0x3C23D70Au); // gp-0x5D80 0.01
}
// 0x178BB0: POST_RACE_1 per-frame driver (vtable slot 0x28). Velocity stage 0x162568 with the shared
// filter constants, then a slow cosine yaw sweep of the captured direction about Z around the head.
inline void postRaceUpdate(OriginalChaseAlgorithmState& s,const OriginalCameraInput& in){
    using namespace post_race;
    Context c;stepVelocity(s,c,in);
    int32_t phase=(s.postRacePhase+1)%period;s.postRacePhase=phase;
    float f1=float(phase);f1=emul(f1,inversePeriod);f1=emul(f1,twoPi);
    float f0=emul(f1,inverseTwoPi);f0=eadd(f0,.5f);
    float whole=float(int32_t(f0));
    if(f0<whole)whole=emul(esub(whole,1.f),twoPi);else whole=emul(whole,twoPi);
    float cosine=cosEE(esub(f1,whole));
    float yaw=esub(1.f,cosine);yaw=emul(yaw,yawAmplitude);yaw=eadd(yaw,-yawAmplitude);
    f0=emul(yaw,inverseTwoPi);f0=eadd(f0,.5f);
    float turns=float(int32_t(f0));if(f0<turns)turns=esub(turns,1.f);
    float half=esub(yaw,emul(turns,twoPi));half=emul(half,.5f);
    auto [sine,cosineHalf]=sincosEE(half);
    Quad q{emul(sine,zAxis[0]),emul(sine,zAxis[1]),emul(sine,zAxis[2]),cosineHalf};
    Quad rotated=rotateByQuaternion(s.postRaceDirection,q);
    s.lookAt=qadd(in.headPosition,qscale(zAxis,-3.f));
    const bool group=2<=in.raceRiderCount;
    float distance=eadd(group?200.f:0.f,126.f),height=eadd(group?150.f:0.f,-27.f);
    Quad eye=qsub(s.lookAt,qscale(rotated,distance));
    s.eye=qadd(eye,qscale(zAxis,height));
}
// 0x1789E8: POST_RACE_1 ctor (after the base ctor 0x162318): horizontal travel direction, or the
// rider forward when |v| <= 1 cm/s, normalised when longer than 0.01, else +X (0x4FF140).
inline void postRaceConstruct(OriginalChaseAlgorithmState& s,const OriginalCameraInput& in){
    Rounding rounding;
    s=OriginalChaseAlgorithmState{};s.variantType=0x44;s.postRacePhase=0;
    Quad d;
    if(1.f<len4(in.velocity))d=qscale(in.velocity,vrsqrtQ(1.f,dot4(in.velocity,in.velocity)));
    else d=in.riderForward;
    d[2]=0;
    float length=len4(d);
    s.postRaceDirection=(post_race::directionMinimum<length)?qscale(d,vdivQ(1.f,length)):Quad{1,0,0,0};
}

// Vtable slot 0x28 dispatch.
inline void update(OriginalChaseAlgorithmState& s,const OriginalCameraInput& in){
    if(s.variantType==0x44)postRaceUpdate(s,in);else chaseUpdate(s,in);
}

// 0x166640: yaw/pitch from lookat - eye.
inline void computeAngles(const Quad& lookAt,const Quad& eye,float& yaw,float& pitch){
    Quad d=qsub(lookAt,eye);
    float dz=d[2];Quad h=d;h[2]=0;
    float hl=len4(h);
    float ratio=ediv(dz,hl);
    pitch=-atanEE(ratio);
    if(!(0.f<hl)){yaw=0;return;}
    float dy=dot4(h,Quad{0,1,0,0});
    float f0=dot4(h,h);float f1=dot4(Quad{1,0,0,0},Quad{1,0,0,0});f0=emul(f0,f1);f1=esqrt(f0);
    if(0.f<dy){
        if(f1<bits(0x3A83126F)){yaw=0;return;}
        float cx=ediv(dot4(h,Quad{1,0,0,0}),f1);
        yaw=esub(halfPi,asinEE(clampUnit(cx)));
    } else {
        if(f1<bits(0x3A83126F)){yaw=esub(twoPi,0.f);return;}
        float cx=ediv(dot4(h,Quad{1,0,0,0}),f1);
        float f1b=esub(halfPi,asinEE(clampUnit(cx)));
        yaw=esub(twoPi,f1b);
    }
}

// 0x1662A0: pitch lag filter with angle-difference limiting; re-projects the look-at.
inline void pitchLagFilter(OriginalChaseAlgorithmState& s,const OriginalCameraInput& in){
    using namespace default3;
    const float A=pitchLimitA,B=pitchLimitB,C=pitchKeepC,D=pitchKeepD;
    float k,lim;
    if(s.filteredPitch<s.pitch){k=D;lim=B;}else{k=C;lim=A;}
    float raw=s.pitch;
    float f1=esub(1.f,k);float f0=emul(k,s.filteredPitch);f1=emul(f1,raw);f0=eadd(f0,f1);
    s.pitch=f0;
    Quad d=qsub(s.lookAt,s.eye);
    Quad dir=directionFromAngles(s.yaw,s.pitch);
    f0=dot4(d,d);f1=dot4(dir,dir);f0=emul(f0,f1);float f3=esqrt(f0);
    float angle;
    if(f3<bits(0x3A83126F))angle=0;
    else {float cosine=ediv(dot4(d,dir),f3);angle=esub(halfPi,asinEE(clampUnit(cosine)));}
    f0=emul(lim,bits(0x3C8EFA36));float f14=k;
    if(f0<angle){f0=ediv(angle,f0);f14=ediv(k,f0);}
    f1=esub(1.f,f14);f0=emul(f14,s.filteredPitch);f1=emul(f1,raw);f0=eadd(f0,f1);
    s.filteredPitch=f0;s.pitch=f0;
    Quad dir2=directionFromAngles(s.yaw,s.pitch);
    Quad v=qsub(in.headPosition,s.eye);
    f1=dot4(dir2,dir2);f0=dot4(v,dir2);f0=ediv(f0,f1);
    s.lookAt=qadd(s.eye,qscale(dir2,f0));
    snapGuard(s,in);
}

// 0x166228: finish (vtable slot 0x40) = angles, pitch lag filter, snap guard, output.
// The pitch filter parameters come from vtable slot 0x38: the chase variants return the
// DEFAULT_3 set (0x176DE0), POST_RACE_1 (0x178BA8) returns "no filter".
inline void finish(OriginalChaseAlgorithmState& s,const OriginalCameraInput& in){
    computeAngles(s.lookAt,s.eye,s.yaw,s.pitch);
    if(s.variantType!=0x44)pitchLagFilter(s,in);
    snapGuard(s,in);
    s.eye[3]=1;s.outputEye=s.eye;   // 0x166F90 (quaternion output not reproduced; the view is eye + angles, zero roll)
}

// 0x166C60 base set-target followed by the DEFAULT_3 override 0x176FE0 (POST_RACE_1: 0x178E90,
// eye seed 0 from 0x178B98 and offset direction (100, 100)).
inline void setTarget(OriginalChaseAlgorithmState& s,const OriginalCameraInput& in){
    using namespace default3;
    const bool postRace=s.variantType==0x44;
    const float initialVerticalOffset=postRace?0.f:default3::initialVerticalOffset;
    const float initialOffsetDistance=postRace?100.f:default3::initialOffsetDistance,initialOffsetHeight=postRace?100.f:300.f;
    Quad fwd=in.riderForward,vel=in.velocity;
    s.mode5Frames=5000;s.mode5Timer=10;s.fovScale=1;s.lookAtHeight=0;s.wallLaunch=0;s.swingInitialised=0;
    s.pullBehind=zeroQuad;s.fieldD0=zeroQuad;s.direction=fwd;s.fieldE0=zeroQuad;
    s.eyeVerticalOffset=initialVerticalOffset;s.boostBump=0;s.lastBoost=0;
    Quad crossed=cross3(zAxis,fwd);
    s.landingDecay=bits(0x3F6B851F);s.fieldF0=crossed;s.landed=1;s.takeoffCountdown=-1;s.takeoffRamp=1;
    s.field1F0=0;s.field2C8=0;s.field2DC=0;s.amplitudeGain=0;s.phaseBActive=0;s.heldEyeTerm=0;s.heldLookTerm=0;s.eyeOffsetB=0;s.lookOffsetB=0;
    s.eyeOffsetA=0;s.lookOffsetA=0;s.pushFilter=0;s.phaseAActive=0;s.lastAirTick=0;s.returnTimer=0;
    s.returnView=zeroQuad;s.residualStart=0;s.residualTimer=0;
    s.field190=vel;s.lockView=qscale(fwd,250.f);s.lockState=1;s.lockBlend=.5f;s.field24C=0;s.field250=0;
    s.lastVelocity=fwd;   // 0x166E30: target slot 0x20 re-read into the fwd*250 scratch (sp+0x20) just before +0x1B0 is stored
    s.field2EC=0;s.field2BC=0;s.extraEyeHeight=0;s.resetPending=0;s.ringIndex=0;
    s.ringX={};s.ringY={};s.ringE={};
    s.lastRawVelocity=qscale(fwd,2.f);
    s.velocityFilter=vel;s.velocityFilter[2]=0;
    update(s,in);
    finish(s,in);
    // 0x166550(559.74402, 300) / (100, 100): offset direction
    Quad v=qscale(fwd,initialOffsetDistance);s.direction=v;
    Quad zz=qscale(zAxis,initialOffsetHeight);s.direction=qsub(s.direction,zz);
    float len=len4(s.direction);s.direction=qscale(s.direction,vdivQ(1.f,len));
    update(s,in);
}

// 15F0C8 passes F12=0.5 to32E100: select the hit nearest the camera/probe midpoint.
inline constexpr float originalCameraTerrainPreferredFraction=.5f;

// 0x15EE00 terrain clearance (chase controller active; override occlusion path out of scope).
inline void cameraCollision(OriginalCameraCompositorState& o,const OriginalCameraInput& in){
    Quad eye=o.eye,lookAt=o.lookAt;float lift=o.lift;
    Quad zLift=qscale(zAxis,lift);
    Quad liftedEye=qadd(eye,zLift);
    Quad d=qsub(lookAt,liftedEye);
    float dl=len4(d);Quad flat=d;flat[2]=0;
    float dl2=emul(dl,dl);float fl=len4(flat);float fl2=emul(fl,fl);
    Quad d0=qsub(lookAt,eye);
    float r=eadd(fl2,dl2);r=esqrt(r);
    float distLE=len4(d0);
    float ratio=ediv(fl,r);
    zLift=qscale(zAxis,lift);liftedEye=qadd(eye,zLift);
    ratio=emax(ratio,bits(0x3D4CCCCD));
    float inverse=ediv(1.f,ratio);
    float halfLen=emul(inverse,300.f);
    Quad zHalf=qscale(zAxis,halfLen);
    float clear=emul(inverse,50.f);
    Quad down=qsub(liftedEye,zHalf);Quad up=qadd(liftedEye,zHalf);
    Quad side=cross3(d,zAxis);
    float sign=(0.f<dot4(o.lastProbeNormal,side))?-1.f:1.f;
    float f0=emul(sign,50.f);
    float f1=len4(side);
    f0=ediv(f0,f1);
    Quad off=qscale(side,f0);
    Quad a=qadd(up,off);Quad b=qadd(down,off);
    std::optional<OriginalCameraProbeHit> hit;
    if(in.terrainProbe)hit=in.terrainProbe(a,b);
    if(hit&&0.f<hit->fraction){
        float t=hit->fraction;float nz=hit->normal[2];
        if(.5f<t){
            float f=eadd(t,t);bool positive=0.f<nz;f=esub(f,1.f);f=emul(halfLen,f);
            if(positive){float push=(f<clear)?esub(clear,f):0.f;lift=eadd(lift,push);}
            else {float push=eadd(f,clear);if(150.f<push)push=0;lift=esub(lift,push);}
        } else {
            float f=eadd(t,t);bool positive=0.f<nz;f=esub(1.f,f);f=emul(halfLen,f);
            if(positive){float push=eadd(f,clear);if(150.f<push)push=0;lift=eadd(lift,push);}
            else {float push=(f<clear)?esub(clear,f):0.f;lift=esub(lift,push);}
        }
        o.lastProbeNormal=hit->normal;
    }
    eye=qadd(eye,qscale(zAxis,lift));
    Quad v=qsub(lookAt,eye);
    float vl=len4(v);float sc=ediv(distLE,vl);v=qscale(v,sc);
    eye=qsub(lookAt,v);
    lift=emul(lift,bits(0x3F7851EC));
    o.eye=eye;o.lift=lift;
    o.occludedLastFrame=0;o.firstFrameAfterReset=0;
}

struct ShakeTableEntry {float period,random,amplitude,fadeReference;int distanceScaled;};
inline const ShakeTableEntry& shakeTable(int index){
    static const ShakeTableEntry table[5]={
        {bits(0x3D4CCCCD),bits(0x3C23D70A),bits(0x3ECCCCCD),bits(0x469C4000),0},
        {bits(0x3CA3D70A),bits(0x3C5FCE31),bits(0x3D8F5C29),-1.f,1},
        {bits(0x3CA3D70A),bits(0x3C5FCE31),bits(0x3DA3D70A),-1.f,1},
        {bits(0x3CA3D70A),bits(0x3C5FCE31),bits(0x3DB851EC),-1.f,1},
        {bits(0x3DCCCCCD),bits(0x3CF5C28F),bits(0x3FB33333),-1.f,1}};
    return table[index];
}
// 0x15E360 requestShake(outer, idx, scale, fadeParam).
inline void requestShake(OriginalCameraCompositorState& o,const OriginalCameraInput& in,int index,float scale,float fadeParameter){
    if(in.raceStateSuppressesShake&&uint32_t(index-1)<4u)return;
    const ShakeTableEntry& e=shakeTable(index);
    float k=1;
    if(0.f<fadeParameter&&0.f<e.fadeReference){float f=eadd(fadeParameter,fadeParameter);f=ediv(f,e.fadeReference);f=esub(2.f,f);k=(f<=1.f)?((0.f<=f)?f:0.f):1.f;}
    float amplitude=emul(scale,k);amplitude=emul(amplitude,e.amplitude);
    if(o.shakeAmplitude<amplitude){o.shakeIndex=index;o.shakeAmplitude=amplitude;}
    o.shakePending=1; // 0x15E444: bc1f with the delay slot v0 = 1 lands on the +0x458 store: every call that is not suppressed
}
// 0x1656B0 shakeStart: seeds the two-octave random walk (12 RNG draws, in order).
inline void shakeStart(OriginalCameraShakeState& s,OriginalRandomState& rng,float per,float rnd,float amp){
    float negAmp=-amp;
    float u=unitRandom(rng);float twoAmp=esub(amp,negAmp);s.period1[0]=eadd(per,emul(rnd,u));
    u=unitRandom(rng);s.period1[1]=eadd(per,emul(rnd,u));
    u=unitRandom(rng);float ampZ=emul(amp,bits(0x3FCCCCCD));float negAmpZ=-ampZ;s.period1[2]=eadd(per,emul(rnd,u));
    u=unitRandom(rng);s.target1[0]=eadd(negAmp,emul(twoAmp,u));
    u=unitRandom(rng);twoAmp=emul(twoAmp,u);negAmp=eadd(negAmp,twoAmp);s.target1[1]=negAmp;
    u=unitRandom(rng);float f1=esub(ampZ,negAmpZ);s.previous1={};s.timer1={};
    float halfPer=emul(per,.5f);float halfRnd=emul(rnd,.5f);float halfAmpZ=emul(ampZ,.5f);f1=emul(f1,u);
    float halfAmp=emul(amp,.5f);float negHalfAmpZ=-halfAmpZ;negAmpZ=eadd(negAmpZ,f1);float negHalfAmp=-halfAmp;
    s.target1[2]=negAmpZ;
    u=unitRandom(rng);float twoHalf=esub(halfAmp,negHalfAmp);s.period2[0]=eadd(halfPer,emul(halfRnd,u));
    u=unitRandom(rng);s.period2[1]=eadd(halfPer,emul(halfRnd,u));
    u=unitRandom(rng);halfRnd=emul(halfRnd,u);halfPer=eadd(halfPer,halfRnd);s.period2[2]=halfPer;
    u=unitRandom(rng);s.target2[0]=eadd(negHalfAmp,emul(twoHalf,u));
    u=unitRandom(rng);twoHalf=emul(twoHalf,u);negHalfAmp=eadd(negHalfAmp,twoHalf);s.target2[1]=negHalfAmp;
    u=unitRandom(rng);float f21=esub(halfAmpZ,negHalfAmpZ);s.fadeTimer=0;s.previous2={};s.timer2={};s.active=0;
    f21=emul(f21,u);negHalfAmpZ=eadd(negHalfAmpZ,f21);s.target2[2]=negHalfAmpZ;
}
// 0x165938 shakeApply: advances the random walk and displaces the compositor look-at.
inline void shakeApply(OriginalCameraCompositorState& o,OriginalRandomState& rng,int requested,int distanceScaled,float per,float rnd,float amp){
    OriginalCameraShakeState& s=o.shake;
    if(requested)s.fadeTimer=0;else if(s.active!=0)s.fadeTimer=2.f;
    float tm=s.fadeTimer;
    s.active=requested;
    if(0.f<tm){float f0=esub(tm,frameSeconds);f0=emax(f0,0.f);s.fadeTimer=f0;}
    float after=s.fadeTimer;float fade=(0.f<after)?emul(after,.5f):1.f;
    if(!requested&&!(0.f<after))return;
    // Only the set-1 timers (+0x278..+0x280) advance by 1/60; the listing never
    // increments +0x2A8..+0x2B0, so the second octave stays at its zero seed.
    auto advance=[&](std::array<float,3>& period,std::array<float,3>& target,std::array<float,3>& previous,std::array<float,3>& timer,float basePeriod,float baseRandom,float amplitudeXY,float amplitudeZ,bool tickTimers){
        if(tickTimers){
            float t0=eadd(timer[0],frameSeconds),t1=eadd(timer[1],frameSeconds),t2=eadd(timer[2],frameSeconds);
            timer[0]=t0;timer[1]=t1;timer[2]=t2;
        }
        for(int axis=0;axis<3;++axis){
            float amplitude=axis==2?amplitudeZ:amplitudeXY;
            if(period[axis]<timer[axis]){
                float u=unitRandom(rng);previous[axis]=target[axis];float neg=-amplitude;
                period[axis]=eadd(basePeriod,emul(baseRandom,u));
                float u2=unitRandom(rng);float span=esub(amplitude,neg);span=emul(span,u2);
                timer[axis]=esub(timer[axis],period[axis]);target[axis]=eadd(neg,span);
            }
        }
    };
    float ampZ=emul(amp,bits(0x3FCCCCCD));
    advance(s.period1,s.target1,s.previous1,s.timer1,per,rnd,amp,ampZ,true);
    float halfPer=emul(per,.5f),halfAmp=emul(amp,.5f),halfRnd=emul(rnd,.5f),halfAmpZ=emul(ampZ,.5f);
    advance(s.period2,s.target2,s.previous2,s.timer2,halfPer,halfRnd,halfAmp,halfAmpZ,false);
    auto weight=[&](float timer,float period){
        if(!(0.f<=timer))return 0.f;
        if(!(timer<=period))return 1.f;
        return smoothstepEE(ediv(timer,period));
    };
    float s1x=weight(s.timer1[0],s.period1[0]),s1y=weight(s.timer1[1],s.period1[1]),s1z=weight(s.timer1[2],s.period1[2]);
    float s2x=weight(s.timer2[0],s.period2[0]),s2y=weight(s.timer2[1],s.period2[1]),s2z=weight(s.timer2[2],s.period2[2]);
    float x=eadd(eadd(emul(s1x,s.target1[0]),emul(esub(1.f,s1x),s.previous1[0])),eadd(emul(s2x,s.target2[0]),emul(esub(1.f,s2x),s.previous2[0])));
    float y=eadd(eadd(emul(s1y,s.target1[1]),emul(esub(1.f,s1y),s.previous1[1])),eadd(emul(s2y,s.target2[1]),emul(esub(1.f,s2y),s.previous2[1])));
    float z=eadd(eadd(emul(s1z,s.target1[2]),emul(esub(1.f,s1z),s.previous1[2])),eadd(emul(s2z,s.target2[2]),emul(esub(1.f,s2z),s.previous2[2])));
    float scale=15.f;
    if(distanceScaled!=0){
        float l=len4(qsub(o.lookAt,o.eye));
        if(l<=150.f){l=len4(qsub(o.lookAt,o.eye));scale=emul(l,bits(0x3DCCCCCD));}
    }
    scale=emul(fade,scale);
    o.lookAt=qadd(o.lookAt,qscale(Quad{x,y,z,0},scale));
}
// 0x15E460 shakeTrigger: boost/speed-scaled request then start/apply.
inline void shakeTrigger(OriginalCameraCompositorState& o,const OriginalCameraInput& in){
    float impact=in.boostLevel;
    if(bits(0x3DCCCCCD)<impact){
        float kmh=emul(len4(in.velocity),bits(0x3D1374BC));
        float scale;
        if(60.f<=kmh)scale=(kmh<=120.f)?ediv(esub(kmh,60.f),60.f):1.f;else scale=0;
        if(bits(0x3F333333)<impact)requestShake(o,in,3,scale,0.f);
        else if(bits(0x3ECCCCCD)<impact)requestShake(o,in,2,scale,0.f);
        else requestShake(o,in,1,scale,0.f);
    }
    int index=o.shakeIndex;const ShakeTableEntry& e=shakeTable(index);
    int pending=o.shakePending;float amp=o.shakeAmplitude;
    OriginalRandomState& rng=in.visualRandom?*in.visualRandom:o.fallbackRandom;
    if(pending&&o.shakeWasPending==0)shakeStart(o.shake,rng,e.period,e.random,amp);
    shakeApply(o,rng,pending,e.distanceScaled,e.period,e.random,amp);
    o.shakeAmplitude=0;o.shakeWasPending=pending;o.shakePending=0;
}
}

inline void OriginalCameraState::begin(const OriginalCameraInput& in,int variantType){
    original_camera::Rounding rounding;
    algorithm=OriginalChaseAlgorithmState{};algorithm.variantType=variantType;
    compositor=OriginalCameraCompositorState{};
    original_camera::setTarget(algorithm,in);
}

// 0x161EF0/0x15D050 at rate 1.0: an instant cut to a new chase variant. The algorithm is rebuilt;
// 0x15D078 runs its set-target twice (list push 0x15C988 and the factory tail, both vtable slot
// 0x20) with the current target; compositor lift/shake state persists. The full director
// (fades, POST_RACE_1, restart) is original_camera_director.hpp.
inline void originalChaseCameraSelect(OriginalCameraState& state,const OriginalCameraInput& in,int variantType){
    original_camera::Rounding rounding;
    (void)original_camera::default3::originalChaseVariant(variantType);
    state.algorithm=OriginalChaseAlgorithmState{};state.algorithm.variantType=variantType;
    original_camera::setTarget(state.algorithm,in);
    original_camera::setTarget(state.algorithm,in);
}
namespace original_camera {
// 0x15E668 after the eye/look-at gather (compositor+0x20/+0x40 hold them with w = 1):
// angles, terrain clearance 0x15EE00, shake 0x15E460, angles, then the fov/near/far
// clamps of the blended raw values (outer+0x0C/+0x10/+0x14 limits, far cap from 0x2EE3B8).
inline OriginalCameraOutput compositeCamera(OriginalCameraCompositorState& o,const OriginalCameraInput& in,float fov,float near,float far){
    computeAngles(o.lookAt,o.eye,o.yaw,o.pitch);
    cameraCollision(o,in);
    shakeTrigger(o,in);
    computeAngles(o.lookAt,o.eye,o.yaw,o.pitch);
    far=(o.nearMinimum<=far)?emin(far,in.farCap):o.nearMinimum;
    fov=(0.f<=fov)?emin(fov,o.fovMaximum):0.f;
    near=(o.nearMinimum<=near)?emin(near,o.farMaximum):o.nearMinimum;
    far=(o.nearMinimum<=far)?emin(far,o.farMaximum):o.nearMinimum;
    o.fov=fov;o.near=near;o.far=far;
    OriginalCameraOutput out;
    out.eye=o.eye;out.lookAt=o.lookAt;
    out.yaw=o.yaw;out.pitch=o.pitch;out.forward=directionFromAngles(o.yaw,o.pitch);
    out.fov=fov;out.near=near;out.far=far;
    return out;
}
// 0x1624E8 (vtable slot 0x48): pending reset -> re-set-target, update, finish.
inline void stepAlgorithm(OriginalChaseAlgorithmState& a,const OriginalCameraInput& in){
    if(a.resetPending!=0)setTarget(a,in);
    update(a,in);
    finish(a,in);
}
}
inline OriginalCameraOutput originalChaseCameraStep(OriginalCameraState& state,const OriginalCameraInput& in){
    using namespace original_camera;
    Rounding rounding;
    OriginalChaseAlgorithmState& a=state.algorithm;
    OriginalCameraCompositorState& o=state.compositor;
    stepAlgorithm(a,in);
    // 0x15E668: single algorithm at full weight -> compositor eye/look-at are exact copies.
    o.eye=a.outputEye;o.lookAt=a.lookAt;o.lookAt[3]=1;o.eye[3]=1;
    // fov / near / far blend with a single raw weight of 1.0.
    float fov=eadd(emul(1.f,a.fov),emul(0.f,0.f)),near=eadd(emul(1.f,a.near),emul(0.f,0.f)),far=eadd(emul(1.f,a.far),emul(0.f,0.f));
    OriginalCameraOutput out=compositeCamera(o,in,fov,near,far);
    out.algorithmEye=a.outputEye;out.algorithmLookAt=a.lookAt;
    out.resetFired=a.resetPending!=0;
    return out;
}

// cCSICubicSpline build (0x31D8B0): Burden-Faires natural / clamped tridiagonal solve, EE scalar order.
inline void OriginalCubicSpline::build(){
    using namespace original_camera;
    const unsigned n=segments;
    std::array<float,13> X{},Y{},AL{},L{},MU{},Z{},C{};std::array<float,12> H{},B{},D{};
    for(unsigned i=0;i<n;++i){X[i]=records[i].a3;Y[i]=records[i].a2;}
    X[n]=records[n-1].a1;Y[n]=records[n-1].a0;
    for(unsigned i=0;i<n;++i)H[i]=esub(X[i+1],X[i]);
    if(!clamped){
        for(unsigned i=1;i<n;++i){
            float f1=ediv(3.f,H[i]);float f3=ediv(3.f,H[i-1]);
            float f0=esub(Y[i+1],Y[i]);float f2=esub(Y[i],Y[i-1]);
            f1=emul(f1,f0);f3=emul(f3,f2);AL[i]=esub(f1,f3);
        }
        L[0]=1;MU[0]=0;Z[0]=0;
    } else {
        float slope0=records[0].x,slopeN=endValue;
        float f1=esub(Y[1],Y[0]);f1=emul(f1,3.f);float f0=emul(slope0,3.f);float f3=emul(slopeN,3.f);f1=ediv(f1,H[0]);AL[0]=esub(f1,f0);
        f0=esub(Y[n],Y[n-1]);f0=emul(f0,3.f);f0=ediv(f0,H[n-1]);AL[n]=esub(f3,f0);
        for(unsigned i=1;i<n;++i){
            float t0=ediv(3.f,H[i]);float t1=ediv(3.f,H[i-1]);
            float d1=esub(Y[i+1],Y[i]);float d0=esub(Y[i],Y[i-1]);
            AL[i]=esub(emul(t0,d1),emul(t1,d0));
        }
        L[0]=eadd(H[0],H[0]);MU[0]=.5f;Z[0]=ediv(AL[0],L[0]);
    }
    for(unsigned i=1;i<n;++i){
        float f1=esub(X[i+1],X[i-1]);float f2=emul(H[i-1],MU[i-1]);f1=eadd(f1,f1);L[i]=esub(f1,f2);
        MU[i]=ediv(H[i],L[i]);
        float f3=emul(H[i-1],Z[i-1]);f2=esub(AL[i],f3);Z[i]=ediv(f2,L[i]);
    }
    if(!clamped){L[n]=1;Z[n]=0;C[n]=0;}
    else {L[n]=emul(H[n-1],esub(2.f,MU[n-1]));Z[n]=ediv(esub(AL[n],emul(H[n-1],Z[n-1])),L[n]);C[n]=Z[n];}
    for(int j=int(n)-1;j>=0;--j){
        float f1=emul(MU[j],C[j+1]);C[j]=esub(Z[j],f1);
        float f0=eadd(C[j],C[j]);float f3=eadd(C[j+1],f0);
        f1=esub(Y[j+1],Y[j]);
        float f4=emul(H[j],3.f);f3=emul(H[j],f3);
        f1=ediv(f1,H[j]);
        f3=emul(f3,bits(0x3EAAAAAB));
        B[j]=esub(f1,f3);
        D[j]=ediv(esub(C[j+1],C[j]),f4);
    }
    for(unsigned i=0;i<n;++i)records[i]={D[i],C[i],B[i],Y[i],X[i]};
    endValue=X[n];current=0;
}

inline float OriginalCubicSpline::evaluate(float x){
    const unsigned last=segments-1;
    auto upper=[&](unsigned seg){return seg==last?endValue:records[seg+1].x;};
    unsigned cur=current;
    if(records[cur].x<=x){
        if(x<=upper(cur))return evaluateSegment(records[cur],x);
        if(cur!=last-1){
            if(x<records[cur+2].x){current=cur+1;return evaluateSegment(records[current],x);}
        } else {current=last;return evaluateSegment(records[last],x);}
    } else if(x<records[1].x){current=0;return evaluateSegment(records[0],x);}
    int lo=0,hi=int(last);
    for(;;){
        int mid=(lo+hi)>>1;float up=(mid<int(last))?records[mid+1].x:endValue;
        if(!(records[mid].x<=x)){hi=mid;continue;}
        if(!(x<=up)){lo=mid+1;continue;}
        current=unsigned(mid);return evaluateSegment(records[mid],x);
    }
}
}
