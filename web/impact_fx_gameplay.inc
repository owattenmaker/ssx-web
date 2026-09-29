// Rider contact/impact FX beyond the snow emitters (docs/terrain-render-fidelity.md "Impact and
// contact effects"). Included from animation_bridge.cpp after the snow code.
//  - board sparks, glints and grind chunks: RFX+0x470 (2DABC8, engine/board_sparks.hpp). The
//    rider-manager pass order puts it after the wake (2DD0B8) and before the board track
//    (2E8938): it runs before update_trail() so its visual-LCG and 3177F0 draws precede theirs.
//  - attack fist sparkle: RFX+0xC70 (2F1150, engine/fist_sparkle.hpp), after the snow pass.
#include "../engine/board_sparks.hpp"
#include "../engine/fist_sparkle.hpp"

RIDER_LOCAL_LAZY static OriginalBoardSparksState boardSparks;RIDER_LOCAL static OriginalFistSparkleState fistSparkle{};
RIDER_LOCAL static std::unique_ptr<OriginalSnowParticles> grindChunks;RIDER_LOCAL static OriginalGrindChunkProfile grindProfile;
RIDER_LOCAL static float grindFlipPhase=0;RIDER_LOCAL static bool impactFxReady=false;
RIDER_LOCAL static std::array<bool,19> surfaceSparks88{},surfaceSparks8C{};
RIDER_LOCAL static std::vector<float> sparkSprites,glintSprites,grindBuffer,fistSprites;RIDER_LOCAL static uint32_t impactFxSerial=0,sparkUpdates=0,fistUpdates=0;
// Glide savestate (tools: local/reference/pcsx2/snow-jam-glide.p2s) grind emitter +0x10 flip phase.
static constexpr uint32_t grindInitialFlipBits=0x40f4ccc7u;
static void reset_impact_fx(bool clearParticles){
 if(!impactFxReady)return;
 // 2DAA78 (sparks reset) and 2F1148 (fist reset) from 111890; the kernel and the emitter ring
 // survive a rider reset, a new run rebuilds them.
 boardSparks.sparks=boardSparks.glints=boardSparks.chunks=false;boardSparks.pointCm={};originalFistSparkleReset(fistSparkle);
 if(clearParticles){boardSparks=OriginalBoardSparksState{};grindChunks=std::make_unique<OriginalSnowParticles>(grindProfile.particles);grindFlipPhase=std::bit_cast<float>(grindInitialFlipBits);sparkSurface438=0;}
 sparkSprites.clear();glintSprites.clear();grindBuffer.clear();fistSprites.clear();++impactFxSerial;
}
static void setup_impact_fx(const json& config){
 for(const auto& d:config.at("original_snow").at("surfaces")){int id=d.at("id");if(id<0||id>=19)throw std::runtime_error("Spark surface index");const auto words=d.at("raw_words_48_to_8c").get<std::array<uint32_t,18>>();surfaceSparks88[id]=words[16]!=0;surfaceSparks8C[id]=words[17]!=0;}
 grindProfile=originalGrindChunkProfile();impactFxReady=true;reset_impact_fx(true);
}
static int impact_motion(){return rider_motion_mode();}
static void update_board_sparks(){
 if(!impactFxReady||cachedCrashWorld.size()<24)return;
 const int motion=impact_motion();
 // rider+0x438: ground/landing contacts write it, the rail attach writes the rail surface; the
 // air keeps it (the PS2 keeps spraying after a grind until the next landing).
 if(motion==4)sparkSurface438=railSurface;else if(motion==2)sparkSurface438=crash.actor.surface;else if(motion==0)sparkSurface438=physicsProfile.surface.id;
 OriginalBoardSparksInput in;in.velocityCmps=physicsState.velocity;in.rail=motion==4;in.surface=sparkSurface438;
 in.contactCm=crash.active&&crash.motion.submode==0&&crash.ticks?crash.actor.contactPoint:browserTrailContact;
 in.rightAxis=originalOrientationBasis(physicsState.quaternion).right;
 const auto board=originalRiderCollisionFrame(cachedCrashWorld.at(trailBones[0]));
 { terrain_original::Rounding rounding; //geometry+0x34 rows carry the rider scale
   for(unsigned k=0;k<3;k++){in.boardRow0[k]=terrain_original::mul(board.right[k],graph.scale[0]);in.boardRow2[k]=terrain_original::mul(board.up[k],graph.scale[2]);} }
 // Authored rails record surface -1 on some courses: 2DABC8 then reads the record before the
 // table, whose live words are +0x88 = 0 and +0x8C != 0 on every captured course.
 const bool known=in.surface>=0&&in.surface<19;in.surfaceSparks=known?surfaceSparks8C[in.surface]:true;in.surfaceGrind=known?surfaceSparks88[in.surface]:false;
 in.tricky=boostState.superTime>0;
 OriginalSnowRandom visual{trailVisualRandom};auto step=originalBoardSparksUpdate(boardSparks,in,visual);trailVisualRandom=visual.word;++sparkUpdates;
 // 3717C0 with the emitter always enabled (+0x174 = 1); inactive requests still advance the ring.
 grindChunks->emit(step.chunk,snowParticleRandom,true);grindFlipPhase=originalSnowFlipbookStep(grindFlipPhase,grindProfile.flipFrames,grindProfile.flipRate,step.chunk.stepSeconds,true);
 sparkSprites.clear();glintSprites.clear();grindBuffer.clear();
 if(boardSparks.sparks)for(const auto& s:originalSparkSprites(boardSparks.kernel)){for(float v:s.positionCm)sparkSprites.push_back(v);sparkSprites.push_back(s.halfExtentCm);for(float c:s.rgb)sparkSprites.push_back(c);sparkSprites.push_back(s.alpha0);sparkSprites.push_back(float(s.slot));}
 if(boardSparks.glints)for(unsigned i=0;i<3;i++){for(float v:boardSparks.glintCm[i])glintSprites.push_back(v);glintSprites.push_back(boardSparks.glintHalfCm[i]);}
 for(const auto& p:grindChunks->particles()){if(!p.colourGs[3])continue;for(float v:p.positionCm)grindBuffer.push_back(v);grindBuffer.push_back(p.halfExtentCm);for(auto v:p.colourGs)grindBuffer.push_back(float(v)/128.f);}
 ++impactFxSerial;
}
static void update_fist_sparkle(){
 if(!impactFxReady||cachedCrashWorld.size()<24)return;
 OriginalFistSparkleInput in;in.strength=attackRider.strength;in.clip0=graph.requestedSemantics[0];in.clip1=graph.requestedSemantics[1];in.class1=graph.currentClass(1);
 if(auto* seq=attack_first_sequence(1)){in.marker0=seq->flags&1;in.marker1=(seq->flags>>1)&1;}
 in.switchStance=physicsState.reverseStance;
 const auto a=originalSnowUnitBoardFrame(cachedCrashWorld.at(trailBones[1])),b=originalSnowUnitBoardFrame(cachedCrashWorld.at(trailBones[2]));
 in.hand8B0Axis0=a.right;in.hand8B0Origin=a.originCm;in.hand8B8Axis0=b.right;in.hand8B8Origin=b.originCm;
 OriginalSnowRandom visual{trailVisualRandom};originalFistSparkleUpdate(fistSparkle,in,visual);trailVisualRandom=visual.word;++fistUpdates;
 fistSprites.clear();
 if(fistSparkle.active)for(unsigned i=0;i<4;i++){for(float v:fistSparkle.positionCm[i])fistSprites.push_back(v);fistSprites.push_back(fistSparkle.halfExtentCm[i]);
  const auto& c=fistSparkle.colour[i];for(unsigned k=1;k<4;k++)fistSprites.push_back(c[k]);fistSprites.push_back(c[0]);} //lanes (A,R,G,B) -> RGBA, x128 on the GS
 ++impactFxSerial;
}
// [serial, spark sprites, glints, grind particles, fist sprites, grind texture id, sparks, glints, chunks,
//  fist active, surface438, spark updates, unsupported chunks, kernel accumulator, point x,y,z]
extern "C" EMSCRIPTEN_KEEPALIVE float* impact_fx_info(){RIDER_LOCAL static float v[17];v[0]=float(impactFxSerial);v[1]=sparkSprites.size()/9;v[2]=glintSprites.size()/4;v[3]=grindBuffer.size()/8;v[4]=fistSprites.size()/8;
 v[5]=grindProfile.textureId+int(grindFlipPhase);v[6]=boardSparks.sparks;v[7]=boardSparks.glints;v[8]=boardSparks.chunks;v[9]=fistSparkle.active;v[10]=sparkSurface438;v[11]=sparkUpdates;v[12]=boardSparks.unsupportedChunks;v[13]=boardSparks.kernel.accumulator;for(unsigned k=0;k<3;k++)v[14+k]=boardSparks.pointCm[k];return v;}
// 0 sparks (x,y,z cm, half cm, r,g,b 0..255, alpha0 0..255, slot), 1 glints (x,y,z, half),
// 2 grind chunks (x,y,z, half, rgba/128), 3 fist sparkle (x,y,z, half, rgba/128).
extern "C" EMSCRIPTEN_KEEPALIVE float* impact_fx_sprites(int kind){return kind==0?sparkSprites.data():kind==1?glintSprites.data():kind==2?grindBuffer.data():kind==3?fistSprites.data():nullptr;}
// Raw spark kernel words for capture comparison (the 0x150-byte object, fields in offset order).
extern "C" EMSCRIPTEN_KEEPALIVE float* impact_fx_kernel(){
 RIDER_LOCAL static float v[84];const auto& k=boardSparks.kernel;std::fill(std::begin(v),std::end(v),0.f);
 auto put=[&](unsigned off,float x){v[off/4]=x;};auto put4=[&](unsigned off,const SparkVector4& q){for(unsigned i=0;i<4;i++)v[off/4+i]=q[i];};
 v[0]=std::bit_cast<float>(uint32_t(k.slots));v[1]=std::bit_cast<float>(uint32_t(k.perSlot));put(8,k.accumulator);put(0xC,k.period);put(0x10,k.sizeRange);put(0x14,k.lifeRange);put(0x18,k.sizeBase);put(0x1C,k.lifeBase);put(0x20,k.sizeDelta);put(0x30,k.alphaStep);put(0x34,k.trailStep);
 const unsigned seedOffsets[9]={0x38,0x3C,0x40,0x44,0x48,0x50,0x54,0x58,0x5C};for(unsigned i=0;i<9;i++)v[seedOffsets[i]/4]=std::bit_cast<float>(k.seeds[i]);put(0x4C,k.damping);
 put4(0x60,k.force);put4(0x70,k.velocityBase);put4(0x80,k.velocityRange0);put4(0x90,k.velocityRange1);put4(0xA0,k.velocityRange2);put4(0xB0,k.base);put4(0xC0,k.axisA);put4(0xD0,k.axisB);put4(0xE0,k.drift);
 put4(0xF0,k.colourBase);put4(0x100,k.colourRange0);put4(0x110,k.colourRange1);put4(0x120,k.colourSlope);return v;
}
