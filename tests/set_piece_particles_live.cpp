// Live parity of engine/set_piece_particles.hpp against the full recompiled original (development
// oracle; driver tools/test_set_piece_particles_live.py).
//  1. Unit oracles on a race savestate: 0x370018+0x3705E0 static emitter construction (random and
//     every exported parameter block, random matrices, random 0x4FF018 state), 0x370788 updates,
//     0x36D500 bounds on random kernels, 0x370B60+0x370DC8 dynamic construction (+ ring alloc) and
//     0x3710D0 births. Every emitter word, every ring record and the 0x4FF018 words are compared.
//  2. Scenarios: real stage programs launched through the original dispatchers (slot 2 contact
//     0x30A060, slot 1 section 0x30A298) on race/full-course savestates, then the original world
//     object pass 0x354F98(group 1) per tick. Hooks on the effect constructors (0x3458C0/0x345C90),
//     updates (0x345B40/0x345F90), stop (0x346060) and destructors run each original operation to
//     completion and then the port operation on the port's own state and 0x4FF018 copy (inputs:
//     the exported parameter block for the target instance, the owner matrix / carrier node matrix
//     0x34FED8 of the original), and compare the whole effect (Particle 0x1F0 / DynamicParticle
//     0x270 + rings) and the 0x4FF018 words after every operation.
#pragma clang optimize off
#include "live_registrations.inc"
#pragma clang optimize on
#include "original_live_runtime.hpp"
#include "../engine/set_piece_particles.hpp"
#include <deque>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
using namespace ssx;
using namespace ssx::set_piece_particles;
static uint32_t RIDER=0x14701A0;
static constexpr uint32_t GP=0x4A30F0,PACKET=0xE0500,SCRATCH=0xE1000,NESTED_SP=0xF6000;
static FILE* out=stdout;
static live::Machine* A=nullptr;
static unsigned failures=0;

static uint32_t rd(uint32_t a){return A->get<uint32_t>(a);}
static float rf(uint32_t a){return A->get<float>(a);}
static void wr(uint32_t a,uint32_t v){A->put(a,v);}
static void wf(uint32_t a,float v){A->put(a,v);}
static OriginalRandomState memoryRandom(){OriginalRandomState r;for(unsigned k=0;k<6;k++)r.words[k]=rd(0x4FF018+4*k);return r;}
static void setMemoryRandom(const OriginalRandomState& r){for(unsigned k=0;k<6;k++)wr(0x4FF018+4*k,r.words[k]);}
static Mat4 readMatrix(uint32_t a){Mat4 m;for(int r=0;r<4;r++)for(int c=0;c<4;c++)m[r][c]=rf(a+16*r+4*c);return m;}

struct Diff {unsigned count=0;std::string first;};
static void cmpWord(Diff& d,const char* what,unsigned off,uint32_t original,uint32_t port){
    if(original==port)return;
    if(original==0x7F800000u&&port==0x7F7FFFFFu)return; // oracle DIV.S by zero (see hookLfsr)
    if(d.count<6){char b[160];snprintf(b,sizeof b,"%s+%03x orig %08x (%.9g) port %08x (%.9g); ",what,off,original,std::bit_cast<float>(original),port,std::bit_cast<float>(port));d.first+=b;}
    d.count++;
}

// ---- exported parameter blocks (resource, kind 16|25|26 -> words) --------------------------------
struct ExportBlock {uint32_t resource;int builtin;std::array<uint32_t,60> words;};
static std::vector<ExportBlock> exported;
static std::map<std::pair<uint32_t,int>,unsigned> used;
static const ExportBlock* findBlock(uint32_t resource,bool dynamic,const uint8_t* originalBlock){
    // Among the exported blocks for this target, the one whose words equal the original block
    // (DynamicParticle: before the constructor zeroes Vel), else the first in program order.
    const ExportBlock* first=nullptr;
    for(const auto& b:exported){
        if(b.resource!=resource||(b.builtin==26)!=dynamic)continue;
        if(!first)first=&b;
        const unsigned n=dynamic?60:56;
        if(!std::memcmp(b.words.data(),originalBlock,4*n))return &b;
        if(!dynamic){ // builtin16 on a fresh instance clamps LifeR (the export keeps the unclamped block)
            auto w=b.words;if(std::bit_cast<float>(w[5])<std::bit_cast<float>(w[7]))w[7]=w[5];
            if(!std::memcmp(w.data(),originalBlock,4*n))return &b;
        }
    }
    return first;
}

// ---- port mirror ---------------------------------------------------------------------------------
struct Stats {unsigned ops=0,exact=0,adopted=0;};
static std::map<std::string,Stats> stats;
static std::map<uint32_t,OriginalParticleEffect> particles;
static std::map<uint32_t,OriginalDynamicParticleEffect> dynamics;
static OriginalRandomState portRandom;
static bool rngOk=true;
static std::string scenarioName;
// PS2 replay: every particle draw is set to its index in the traced PS2 0x4FF018 stream.
static bool replayMode=false;static std::deque<uint64_t> replayQueue;static OriginalRandomState replayState;static uint64_t replayIndex=0;
static unsigned replayMissing=0;
static void replaySeek(unsigned draws){
    if(!replayMode)return;
    if(replayQueue.empty()){replayMissing++;return;}
    const uint64_t index=replayQueue.front();for(unsigned k=0;k<draws&&!replayQueue.empty();k++)replayQueue.pop_front();
    if(index<replayIndex)throw std::runtime_error("replay index goes backwards");
    while(replayIndex<index){replayState.next();replayIndex++;}
    setMemoryRandom(replayState);portRandom=replayState;
}
static void replayAdvance(unsigned draws){if(!replayMode)return;for(unsigned k=0;k<draws;k++){replayState.next();replayIndex++;}}

static void report(const char* op,uint32_t obj,const Diff& d){
    auto& s=stats[op];s.ops++;
    if(!d.count){s.exact++;return;}
    if(failures<40)fprintf(out,"  [%s] %s %06x: %u words differ: %s\n",scenarioName.c_str(),op,obj,d.count,d.first.c_str());
    failures++;
}
static Diff compareParticle(uint32_t obj,const OriginalParticleEffect& x){
    Diff d;
    for(unsigned k=0;k<16;k++)cmpWord(d,"matrix",4*k,rd(obj+0x10+4*k),bitsf(x.matrix[k/4][k%4]));
    for(unsigned o=0;o<0x190;o+=4)cmpWord(d,"emitter",o,rd(obj+0x50+o),x.emitter.u(o));
    cmpWord(d,"dead",0x1E4,rd(obj+0x1E4),x.dead);
    return d;
}
static Diff compareDynamic(uint32_t obj,const OriginalDynamicParticleEffect& x){
    Diff d;const uint32_t E=obj+0x60;
    cmpWord(d,"node",0x10,rd(obj+0x10),uint32_t(x.node));cmpWord(d,"stopped",0x14,rd(obj+0x14),x.stopped);cmpWord(d,"dead",0x18,rd(obj+0x18),x.dead);
    for(unsigned k=0;k<4;k++){cmpWord(d,"offset",0x20+4*k,rd(obj+0x20+4*k),bitsf(x.offset[k]));cmpWord(d,"worldPos",0x30+4*k,rd(obj+0x30+4*k),bitsf(x.worldPosition[k]));
        cmpWord(d,"worldVel",0x40+4*k,rd(obj+0x40+4*k),bitsf(x.worldVelocity[k]));cmpWord(d,"vel50",0x50+4*k,rd(obj+0x50+4*k),bitsf(x.velocity[k]));}
    for(unsigned o=0;o<0x1F8+4;o+=4){if(o==0x1A0||o==0x1A4)continue;cmpWord(d,"emitter",o,rd(E+o),x.emitter.u(o));}
    const int32_t cap=int32_t(rd(E+0x178));
    if(cap>0){
        if(x.emitter.ringPosition.size()!=unsigned(cap)){cmpWord(d,"ringsize",0,cap,uint32_t(x.emitter.ringPosition.size()));}
        else{const uint32_t ra=rd(E+0x1A0),rb=rd(E+0x1A4);
            for(int i=0;i<cap;i++)for(int k=0;k<4;k++){cmpWord(d,"ringA",16*i+4*k,rd(ra+16*i+4*k),bitsf(x.emitter.ringPosition[i][k]));cmpWord(d,"ringB",16*i+4*k,rd(rb+16*i+4*k),bitsf(x.emitter.ringVelocity[i][k]));}}
    }
    return d;
}
static void checkRandom(const char* op){
    const auto m=memoryRandom();
    if(m.words!=portRandom.words){
        if(rngOk&&failures<40)fprintf(out,"  [%s] 0x4FF018 differs after %s (port %08x.. original %08x..)\n",scenarioName.c_str(),op,portRandom.words[5],m.words[5]);
        if(rngOk)failures++;rngOk=false;portRandom=m;
    }else rngOk=true;
}
static OriginalParticleEffect adoptParticle(uint32_t obj){
    OriginalParticleEffect x;x.matrix=readMatrix(obj+0x10);for(unsigned o=0;o<0x190;o+=4)x.emitter.setU(o,rd(obj+0x50+o));x.dead=rd(obj+0x1E4);return x;
}
static OriginalDynamicParticleEffect adoptDynamic(uint32_t obj){
    OriginalDynamicParticleEffect x;const uint32_t E=obj+0x60;
    x.node=int32_t(rd(obj+0x10));x.stopped=rd(obj+0x14);x.dead=rd(obj+0x18);
    for(int k=0;k<4;k++){x.offset[k]=rf(obj+0x20+4*k);x.worldPosition[k]=rf(obj+0x30+4*k);x.worldVelocity[k]=rf(obj+0x40+4*k);x.velocity[k]=rf(obj+0x50+4*k);}
    for(unsigned o=0;o<0x200;o+=4)x.emitter.setU(o,rd(E+o));
    const int32_t cap=int32_t(rd(E+0x178));
    for(int i=0;i<std::max(cap,0);i++){Vec4 a,b;for(int k=0;k<4;k++){a[k]=rf(rd(E+0x1A0)+16*i+4*k);b[k]=rf(rd(E+0x1A4)+16*i+4*k);}x.emitter.ringPosition.push_back(a);x.emitter.ringVelocity.push_back(b);}
    return x;
}
static Mat4 nodeMatrix(uint32_t instance,int32_t node){
    A->call(0x34fed8,{instance,uint32_t(node),SCRATCH+0xF00},{},NESTED_SP);return readMatrix(SCRATCH+0xF00);
}
// Static path of 0x34FED8 for entities without animated nodes: compare the port helper.
static unsigned nodeChecks=0,nodeExact=0;
static void checkNodeMatrix(uint32_t instance,int32_t node,const Mat4& original){
    const uint32_t entity=rd(instance+0xC);if(!entity||node<0)return;
    const uint32_t vt=rd(entity+0xC);if(vt==0x490B10u)return; // LiveComp: animated nodes (vt+0xEC)
    const uint32_t model=rd(instance+0x80),table=rd(model+8);std::vector<Mat4> chain;
    for(int32_t n=node;n!=-1;n=int32_t(rd(table+16*n)))chain.push_back(readMatrix(rd(table+16*n+0xC)));
    // entity matrix: vt+0xC4 (modifier matrix or instance+0x10)
    R5900Context c=A->call(0x356078,{entity},{},NESTED_SP);const Mat4 E=readMatrix(GPR_U32((&c),2));
    const Mat4 port=originalParticleNodeMatrix(chain,rf(instance+0x84),E);
    nodeChecks++;bool same=true;for(int r=0;r<4;r++)for(int k=0;k<4;k++)same&=bitsf(port[r][k])==bitsf(original[r][k]);
    if(same)nodeExact++;else if(failures<40){fprintf(out,"  node matrix differs for instance %06x node %d\n",instance,node);failures++;}
}

// ---- hooks --------------------------------------------------------------------------------------
static std::map<uint32_t,PS2Runtime::RecompiledFunction> originals;
static void runOriginal(uint32_t pc,uint8_t* m,R5900Context* c,PS2Runtime* rt){
    const uint32_t ra=GPR_U32(c,31);originals.at(pc)(m,c,rt);
    while(c->pc!=ra){if(!rt->hasFunction(c->pc))throw std::runtime_error("hooked original continuation missing");rt->lookupFunction(c->pc)(m,c,rt);}
}
static void hookParticleCtor(uint8_t* m,R5900Context* c,PS2Runtime* rt){ // 0x3458C0(obj, block, instance)
    const uint32_t obj=GPR_U32(c,4),blk=GPR_U32(c,5),inst=GPR_U32(c,6);
    OriginalStaticEmitter initial;for(unsigned o=0;o<0x190;o+=4)initial.setU(o,rd(obj+0x50+o));
    std::array<uint32_t,60> original{};for(unsigned k=0;k<56;k++)original[k]=rd(blk+4*k);
    replaySeek(9);runOriginal(0x3458c0,m,c,rt);replayAdvance(9);
    const uint32_t resource=rd(inst+0x78);const ExportBlock* b=findBlock(resource,false,reinterpret_cast<const uint8_t*>(original.data()));
    OriginalParticleParams p=originalParticleDefaults(16);
    if(b){for(unsigned k=0;k<56;k++)p.setU(4*k,b->words[k]);}else{fprintf(out,"  [%s] no exported block for Particle on %06x\n",scenarioName.c_str(),resource);failures++;for(unsigned k=0;k<56;k++)p.setU(4*k,original[k]);}
    // builtin16 on an instance without entity clamps LifeR (the export stores the unclamped block).
    if(std::memcmp(p.w.data(),original.data(),4*56)){auto q=p;originalParticleClampLifeRange(q);if(!std::memcmp(q.w.data(),original.data(),4*56))p=q;}
    Diff db;for(unsigned k=0;k<56;k++)cmpWord(db,"block",4*k,original[k],p.u(4*k));report("Particle block",obj,db);
    auto x=originalParticleEffectConstruct(p,readMatrix(obj+0x10),portRandom,initial);
    particles[obj]=x;report("Particle construct",obj,compareParticle(obj,x));checkRandom("Particle construct");
}
static void hookDynamicCtor(uint8_t* m,R5900Context* c,PS2Runtime* rt){ // 0x345C90(obj, block, instance, node, &offset)
    const uint32_t obj=GPR_U32(c,4),blk=GPR_U32(c,5),inst=GPR_U32(c,6);
    OriginalDynamicEmitter initial;for(unsigned o=0;o<0x200;o+=4)initial.setU(o,rd(obj+0x60+o));
    std::array<uint32_t,60> original{};for(unsigned k=0;k<60;k++)original[k]=rd(blk+4*k);
    const unsigned draws=9+(int32_t(original[52])>=2?1:0);
    replaySeek(draws);runOriginal(0x345c90,m,c,rt);replayAdvance(draws);
    const uint32_t resource=rd(inst+0x78);const ExportBlock* b=findBlock(resource,true,reinterpret_cast<const uint8_t*>(original.data()));
    OriginalParticleParams p=originalParticleDefaults(26);
    if(b){for(unsigned k=0;k<60;k++)p.setU(4*k,b->words[k]);}else{fprintf(out,"  [%s] no exported block for DynamicParticle on %06x\n",scenarioName.c_str(),resource);failures++;for(unsigned k=0;k<60;k++)p.setU(4*k,original[k]);}
    Diff db;for(unsigned k=0;k<60;k++)cmpWord(db,"block",4*k,original[k],p.u(4*k));report("DynamicParticle block",obj,db);
    const Mat4 node=nodeMatrix(inst,int32_t(rd(obj+0x10)));checkNodeMatrix(inst,int32_t(rd(obj+0x10)),node);
    auto x=originalDynamicParticleConstruct(p,node,portRandom,initial);
    dynamics[obj]=x;report("DynamicParticle construct",obj,compareDynamic(obj,x));checkRandom("DynamicParticle construct");
}
static void hookParticleUpdate(uint8_t* m,R5900Context* c,PS2Runtime* rt){ // 0x345B40(obj)
    const uint32_t obj=GPR_U32(c,4);
    if(!particles.count(obj)){particles[obj]=adoptParticle(obj);stats["Particle adopted"].adopted++;}
    runOriginal(0x345b40,m,c,rt);
    auto& x=particles[obj];originalParticleEffectUpdate(x);
    Diff d=compareParticle(obj,x);report("Particle update",obj,d);if(d.count)x=adoptParticle(obj);checkRandom("Particle update");
}
static void hookDynamicUpdate(uint8_t* m,R5900Context* c,PS2Runtime* rt){ // 0x345F90(obj)
    const uint32_t obj=GPR_U32(c,4);
    if(!dynamics.count(obj)){dynamics[obj]=adoptDynamic(obj);stats["DynamicParticle adopted"].adopted++;}
    const bool draws=!rd(obj+0x18)&&!rd(obj+0x14)&&rd(obj+0x60+0x174);
    if(draws)replaySeek(1);
    runOriginal(0x345f90,m,c,rt);
    if(draws)replayAdvance(1);
    auto& x=dynamics[obj];
    if(!x.dead){const uint32_t inst=rd(obj+0xC);const Mat4 node=nodeMatrix(inst,x.node);checkNodeMatrix(inst,x.node,node);originalDynamicParticleUpdate(x,node,portRandom);}
    Diff d=compareDynamic(obj,x);report("DynamicParticle update",obj,d);if(d.count)x=adoptDynamic(obj);checkRandom("DynamicParticle update");
}
static void hookDynamicStop(uint8_t* m,R5900Context* c,PS2Runtime* rt){ // 0x346060(obj)
    const uint32_t obj=GPR_U32(c,4);runOriginal(0x346060,m,c,rt);
    if(dynamics.count(obj)){originalDynamicParticleStop(dynamics[obj]);report("DynamicParticle stop",obj,compareDynamic(obj,dynamics[obj]));}
}
static void hookParticleDtor(uint8_t* m,R5900Context* c,PS2Runtime* rt){const uint32_t obj=GPR_U32(c,4);runOriginal(0x345ad0,m,c,rt);particles.erase(obj);stats["Particle destroyed"].ops++;}
static void hookDynamicDtor(uint8_t* m,R5900Context* c,PS2Runtime* rt){const uint32_t obj=GPR_U32(c,4);runOriginal(0x345e88,m,c,rt);dynamics.erase(obj);stats["DynamicParticle destroyed"].ops++;}
// Oracle corrections (the recompiled VU0 macro RINIT/RNEXT and the EE DIV.S by zero differ from
// the PS2): 0x36D400 is replaced by the PCSX2 RINIT+RNEXT semantics (checked against PS2 savestates:
// continuous emitters' seeds after 6 window wraps), and K+0x30 = 1/NumBlur with NumBlur 0 is 0x7F7FFFFF
// on the PS2 (fulldense savestates) where the oracle stores +inf.
static void hookLfsr(uint8_t* m,R5900Context* c,PS2Runtime*){
    const uint32_t a=GPR_U32(c,4);uint32_t v;std::memcpy(&v,m+(a&0x1FFFFFF),4);v=lfsrStep(v);std::memcpy(m+(a&0x1FFFFFF),&v,4);c->pc=GPR_U32(c,31);}
static void installHooks(){
    A->runtime.registerFunction(0x36d400,hookLfsr);
    const std::pair<uint32_t,PS2Runtime::RecompiledFunction> hooks[]={{0x3458c0,hookParticleCtor},{0x345c90,hookDynamicCtor},{0x345b40,hookParticleUpdate},
        {0x345f90,hookDynamicUpdate},{0x346060,hookDynamicStop},{0x345ad0,hookParticleDtor},{0x345e88,hookDynamicDtor}};
    for(auto& [pc,fn]:hooks){if(!originals.count(pc))originals[pc]=A->runtime.lookupFunction(pc);A->runtime.registerFunction(pc,fn);}
}

// ---- unit oracles -------------------------------------------------------------------------------
static std::mt19937 rng(20260922);
static float uni(float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);}
static int irange(int a,int b){return std::uniform_int_distribution<int>(a,b)(rng);}
static OriginalParticleParams randomParams(int builtin){
    using namespace particle_param;OriginalParticleParams p=originalParticleDefaults(builtin);
    p.setI(NumParticles,builtin==26?irange(1,6):irange(1,200));p.setI(NumBlur,irange(0,3)?0:irange(1,8));
    p.setF(Duration,irange(0,2)?-1.f:uni(0.f,2.f));if(irange(0,9)==0)p.setF(Duration,0.f);
    p.setF(Damp,uni(.25f,3.f));p.setF(Size,uni(0,400));p.setF(Life,uni(.05f,6.f));p.setF(SizeR,uni(0,60));
    p.setF(LifeR,irange(0,2)?0.f:uni(0,p.f(Life)*1.2f));p.setF(BlurStep,uni(0,.05f));
    for(unsigned o=Off;o<StartCol;o+=4)p.setF(o,irange(0,3)?uni(-2000,2000):0.f);
    for(unsigned o=StartCol;o<TextureId;o+=4)p.setF(o,uni(0,1.2f));
    p.setI(TextureId,irange(0,60));p.setI(BlendMode,irange(0,3));p.setF(SizeFinal,uni(0,800));
    p.setI(NumFlipTextures,irange(0,3)?1:irange(0,5));p.setF(FlipTextureRate,uni(0,30));
    return p;
}
static Mat4 randomMatrix(){
    Mat4 m{};float a=uni(-3.14f,3.14f),b=uni(-1.5f,1.5f);
    m[0]={std::cos(a),std::sin(a),0,0};m[1]={-std::sin(a)*std::cos(b),std::cos(a)*std::cos(b),std::sin(b),0};m[2]={std::sin(a)*std::sin(b),-std::cos(a)*std::sin(b),std::cos(b),0};
    m[3]={uni(-300000,300000),uni(-300000,300000),uni(-50000,50000),1};if(irange(0,4)==0)m=kIdentity;return m;
}
static void writeParams(uint32_t at,const OriginalParticleParams& p){for(unsigned o=0;o<0xF0;o+=4)wr(at+o,p.u(o));}
static void writeMatrix(uint32_t at,const Mat4& m){for(int r=0;r<4;r++)for(int k=0;k<4;k++)wf(at+16*r+4*k,m[r][k]);}
static OriginalRandomState randomState(){OriginalRandomState r;for(auto& w:r.words)w=uint32_t(rng());return r;}

static void unitOracles(unsigned cases){
    scenarioName="unit";
    const uint32_t E=SCRATCH,P=SCRATCH+0x200,M=SCRATCH+0x300,K=SCRATCH+0x400,B=SCRATCH+0x600,D=SCRATCH+0x800,POS=SCRATCH+0xA00,VEL=SCRATCH+0xA10;
    unsigned staticCases=0,staticExact=0,updates=0,updatesExact=0,boundsCases=0,boundsExact=0,dynCases=0,dynExact=0,births=0,birthsExact=0;
    std::vector<OriginalParticleParams> fixed;
    for(const auto& b:exported){OriginalParticleParams p=originalParticleDefaults(b.builtin==26?26:16);for(unsigned k=0;k<(b.builtin==26?60u:56u);k++)p.setU(4*k,b.words[k]);fixed.push_back(p);}
    for(unsigned n=0;n<cases;n++){
        // static construction + updates
        OriginalParticleParams p=n<fixed.size()&&fixed[n].builtin!=26?fixed[n]:randomParams(16);p.builtin=16;
        const Mat4 m=randomMatrix();writeParams(P,p);writeMatrix(M,m);
        OriginalStaticEmitter initial;for(unsigned o=0;o<0x190;o+=4){initial.setU(o,uint32_t(rng()));wr(E+o,initial.u(o));}
        portRandom=randomState();setMemoryRandom(portRandom);
        A->call(0x370018,{E});A->call(0x3705e0,{E,M,P});
        auto e=originalStaticEmitterConstruct(p,m,portRandom,initial);
        Diff d;for(unsigned o=0;o<0x190;o+=4)cmpWord(d,"static",o,rd(E+o),e.u(o));
        if(memoryRandom().words!=portRandom.words)cmpWord(d,"rng",0,memoryRandom().words[5],portRandom.words[5]);
        staticCases++;if(!d.count)staticExact++;else if(failures<40){fprintf(out,"  unit static construct %u: %s\n",n,d.first.c_str());failures++;}
        const int ticks=irange(1,400);
        for(int t=0;t<ticks;t++){
            const float dt=irange(0,9)?fbits(0x3C888889u):uni(0,.1f);
            A->call(0x370788,{E},{dt});originalStaticEmitterUpdate(e,dt);
            Diff du;for(unsigned o=0;o<0x190;o+=4)cmpWord(du,"update",o,rd(E+o),e.u(o));
            updates++;if(!du.count)updatesExact++;else{if(failures<40){fprintf(out,"  unit static update %u tick %d: %s\n",n,t,du.first.c_str());failures++;}break;}
        }
        // bounds on a random kernel
        {
            OriginalStaticEmitter k;for(unsigned o=0;o<0x190;o+=4)k.setF(o,uni(-3000,3000));
            k.setI(0x10,irange(1,300));k.setF(0x10+0x14,uni(0,5));k.setF(0x10+0x1C,uni(-2,5));
            for(unsigned o=0;o<0x150;o+=4)wr(K+o,k.u(0x10+o));
            std::array<float,8> b;for(auto& x:b)x=uni(-10,10);for(int j=0;j<8;j++)wf(B+4*j,b[j]);
            A->call(0x36d500,{K,B});auto kr=k.kernel();originalParticleKernelBounds(kr,b);
            Diff db;for(int j=0;j<8;j++)cmpWord(db,"bounds",4*j,rd(B+4*j),bitsf(b[j]));
            boundsCases++;if(!db.count)boundsExact++;else if(failures<40){fprintf(out,"  unit bounds %u: %s\n",n,db.first.c_str());failures++;}
        }
        // dynamic construction + births
        {
            OriginalParticleParams q=n<fixed.size()&&fixed[n].builtin==26?fixed[n]:randomParams(26);q.builtin=26;
            {using namespace particle_param;q.setF(Duration,-1.f);if(q.f(Life)<q.f(LifeR))q.setU(LifeR,q.u(Life));q.setF(Vel,0);q.setF(Vel+4,0);q.setF(Vel+8,0);}
            writeParams(P,q);
            OriginalDynamicEmitter initial;for(unsigned o=0;o<0x200;o+=4){initial.setU(o,uint32_t(rng()));wr(D+o,initial.u(o));}
            A->call(0x370b60,{D});wr(D+0x1F8,0x491370u);
            portRandom=randomState();setMemoryRandom(portRandom);
            A->call(0x370dc8,{D,P},{-1.f});
            auto e2=originalDynamicEmitterConstruct(q,portRandom,initial);e2.setU(dynamic_emitter::Vtable,0x491370u);
            auto cmpDyn=[&](Diff& dd){for(unsigned o=0;o<0x1FC;o+=4){if(o==0x1A0||o==0x1A4)continue;cmpWord(dd,"dynamic",o,rd(D+o),e2.u(o));}
                const int32_t cap=int32_t(rd(D+0x178));for(int i=0;i<cap;i++)for(int k=0;k<4;k++){cmpWord(dd,"ringA",16*i+4*k,rd(rd(D+0x1A0)+16*i+4*k),bitsf(e2.ringPosition.at(i)[k]));cmpWord(dd,"ringB",16*i+4*k,rd(rd(D+0x1A4)+16*i+4*k),bitsf(e2.ringVelocity.at(i)[k]));}
                if(memoryRandom().words!=portRandom.words)cmpWord(dd,"rng",0,memoryRandom().words[5],portRandom.words[5]);};
            Diff dd;cmpDyn(dd);dynCases++;if(!dd.count)dynExact++;else if(failures<40){fprintf(out,"  unit dynamic construct %u: %s\n",n,dd.first.c_str());failures++;}
            const int count=irange(1,300);
            for(int t=0;t<count;t++){
                Vec4 pos{uni(-3e5f,3e5f),uni(-3e5f,3e5f),uni(-5e4f,5e4f),1},vel{uni(-50,50),uni(-50,50),uni(-50,50),0};
                const bool hasPos=irange(0,7)!=0,hasVel=irange(0,7)!=0,active=irange(0,5)!=0;const float dt=irange(0,9)?fbits(0x3C888889u):uni(0,.1f);
                for(int k=0;k<4;k++){wf(POS+4*k,pos[k]);wf(VEL+4*k,vel[k]);}
                A->call(0x3710d0,{D,hasPos?POS:0u,hasVel?VEL:0u,uint32_t(active)},{dt});
                originalDynamicEmitterBirth(e2,hasPos?&pos:nullptr,hasVel?&vel:nullptr,active,dt,portRandom);
                Diff db;cmpDyn(db);births++;if(!db.count)birthsExact++;else{if(failures<40){fprintf(out,"  unit birth %u/%d: %s\n",n,t,db.first.c_str());failures++;}break;}
            }
            A->call(0x370cf8,{D,1u}); // free the rings
        }
    }
    fprintf(out,"[unit] static constructions %u/%u exact, 0x370788 updates %u/%u, 0x36D500 bounds %u/%u, dynamic constructions %u/%u, 0x3710D0 births %u/%u\n",
        staticExact,staticCases,updatesExact,updates,boundsExact,boundsCases,dynExact,dynCases,birthsExact,births);
}

// ---- scenarios ----------------------------------------------------------------------------------
struct Launch {int slot;uint32_t instance;};
static void scenario(const std::string& folder,const std::string& label,const std::string& state,const std::vector<Launch>& launches,int ticks){
    live::Machine M(folder+"/"+state+".ee",folder+"/"+state+".vuc",folder+"/"+state+".vud");A=&M;installHooks();
    scenarioName=label;particles.clear();dynamics.clear();stats.clear();portRandom=memoryRandom();rngOk=true;
    const unsigned before=failures;const uint32_t context=rd(GP+0xCE8);
    try{
        for(const auto& l:launches){
            if(l.slot==2){for(uint32_t o=0;o<0x80;o+=4)wr(PACKET+o,rd(RIDER+0x9E0+o));wr(PACKET+0x50,l.instance);M.call(0x30a060,{context,RIDER+0xA60,PACKET,RIDER+0x6C0});}
            else M.call(0x30a298,{context,l.instance});
            checkRandom("launch");
        }
        for(int t=0;t<ticks;t++){M.call(0x354f98,{GP+0x2898,1u});checkRandom("tick");}
    }catch(const std::exception& e){fprintf(out,"  [%s] original run stopped: %s\n",label.c_str(),e.what());failures++;}
    fprintf(out,"[%s] from %s, %d entity passes:",label.c_str(),state.c_str(),ticks);
    for(auto& [k,s]:stats){if(s.adopted)fprintf(out," %s %u;",k.c_str(),s.adopted);else if(k.find("destroyed")!=std::string::npos)fprintf(out," %s %u;",k.c_str(),s.ops);else fprintf(out," %s %u/%u;",k.c_str(),s.exact,s.ops);}
    fprintf(out," node matrices %u/%u; %s\n",nodeExact,nodeChecks,failures==before?"exact":"DIFFERS");
    nodeChecks=nodeExact=0;A=nullptr;
}

// ---- PS2 savestate replay -----------------------------------------------------------------------
struct Found {uint32_t object,resource;bool dynamic;};
static std::vector<Found> findEffects(const std::vector<uint8_t>& ee){
    auto u=[&](uint32_t a){uint32_t v;std::memcpy(&v,ee.data()+(a&0x1FFFFFF),4);return v;};
    std::vector<Found> out;
    for(uint32_t a=0x100000;a+16<ee.size();a+=4){
        const uint32_t v=u(a);if(v!=0x4912B0u&&v!=0x491268u)continue;
        const uint32_t obj=a-8,inst=u(obj+0xC);if(inst<0x100000||inst>=0x2000000)continue;
        const uint32_t entity=u(inst+0xC);if(entity<0x100000||entity>=0x2000000)continue;
        const uint32_t cont=u(entity+0x1C);if(cont<0x100000||cont>=0x2000000)continue;
        bool listed=false;for(uint32_t e=u(cont+0x10);e>=0x100000&&e<0x2000000;e=u(e))if(e==obj){listed=true;break;}
        if(listed)out.push_back({obj,u(inst+0x78),v==0x491268u});
    }
    return out;
}
static void replay(const std::string& folder,const std::string& label,const std::string& stateA,const std::string& stateB,int first,int last,uint64_t baseIndex,
                   const std::map<int,std::vector<uint64_t>>& queue,const std::map<int,std::vector<Launch>>& launches){
    live::Machine M(folder+"/"+stateA+".ee",folder+"/"+stateA+".vuc",folder+"/"+stateA+".vud");A=&M;installHooks();
    scenarioName=label;particles.clear();dynamics.clear();stats.clear();rngOk=true;
    replayMode=true;replayState=memoryRandom();replayIndex=baseIndex;replayMissing=0;portRandom=replayState;
    const unsigned before=failures;const uint32_t context=rd(GP+0xCE8);unsigned leftover=0;
    try{
        for(int t=first;t<=last;t++){
            replayQueue.clear();if(auto it=queue.find(t);it!=queue.end())replayQueue.assign(it->second.begin(),it->second.end());
            M.call(0x354f98,{GP+0x2898,1u});
            if(auto it=launches.find(t);it!=launches.end())for(const auto& l:it->second){
                if(l.slot==2){for(uint32_t o=0;o<0x80;o+=4)wr(PACKET+o,rd(RIDER+0x9E0+o));wr(PACKET+0x50,l.instance);M.call(0x30a060,{context,RIDER+0xA60,PACKET,RIDER+0x6C0});}
                else M.call(0x30a298,{context,l.instance});
            }
            leftover+=unsigned(replayQueue.size());
        }
    }catch(const std::exception& e){fprintf(out,"  [%s] original run stopped: %s\n",label.c_str(),e.what());failures++;}
    replayMode=false;
    // Compare every live effect of the PS2 savestate B with the harness (original) and the port.
    const auto B=live::readFile(folder+"/"+stateB+".ee");const auto here=findEffects(M.ee),there=findEffects(B);
    unsigned compared=0,exactOriginal=0,exactPort=0;
    auto uB=[&](uint32_t a){uint32_t v;std::memcpy(&v,B.data()+(a&0x1FFFFFF),4);return v;};
    for(const auto& f:there){
        std::vector<Found> same;for(const auto& h:here)if(h.resource==f.resource&&h.dynamic==f.dynamic)same.push_back(h);
        std::vector<Found> sameB;for(const auto& g:there)if(g.resource==f.resource&&g.dynamic==f.dynamic)sameB.push_back(g);
        size_t rank=0;while(rank<sameB.size()&&sameB[rank].object!=f.object)rank++;
        if(same.size()!=sameB.size()){if(failures<40)fprintf(out,"  [%s] %s on %06x: PS2 has %zu, harness %zu\n",label.c_str(),f.dynamic?"DynamicParticle":"Particle",f.resource,sameB.size(),same.size());failures++;continue;}
        const uint32_t h=same[rank].object;Diff d,dp;compared++;
        if(!f.dynamic){
            for(uint32_t o=0x10;o<0x1E0;o+=4)cmpWord(d,"ps2",o,uB(f.object+o),rd(h+o));cmpWord(d,"ps2",0x1E4,uB(f.object+0x1E4),rd(h+0x1E4));
            if(particles.count(h))dp=compareParticle(h,particles[h]);else cmpWord(dp,"port-missing",0,0,1);
        }else{
            for(uint32_t o=0x10;o<0x258+4;o+=4){if(o==0x60+0x1A0||o==0x60+0x1A4||(o>=0x60+0x1E4&&o<0x60+0x1F8))continue;cmpWord(d,"ps2",o,uB(f.object+o),rd(h+o));}
            const int32_t cap=int32_t(uB(f.object+0x60+0x178));
            for(int i=0;i<cap;i++)for(uint32_t k=0;k<16;k+=4){cmpWord(d,"ps2ringA",16*i+k,uB(uB(f.object+0x60+0x1A0)+16*i+k),rd(rd(h+0x60+0x1A0)+16*i+k));cmpWord(d,"ps2ringB",16*i+k,uB(uB(f.object+0x60+0x1A4)+16*i+k),rd(rd(h+0x60+0x1A4)+16*i+k));}
            if(dynamics.count(h))dp=compareDynamic(h,dynamics[h]);else cmpWord(dp,"port-missing",0,0,1);
        }
        if(!d.count)exactOriginal++;else if(failures<40){fprintf(out,"  [%s] PS2 %s %06x (res %06x): %u words differ from the original harness: %s\n",label.c_str(),f.dynamic?"DynP":"Particle",f.object,f.resource,d.count,d.first.c_str());failures++;}
        if(!dp.count)exactPort++;else if(failures<40){fprintf(out,"  [%s] port differs from the harness on %06x: %s\n",label.c_str(),h,dp.first.c_str());failures++;}
    }
    if(here.size()!=there.size()){if(failures<40)fprintf(out,"  [%s] harness has %zu effects, PS2 %zu\n",label.c_str(),here.size(),there.size());failures++;}
    if(leftover||replayMissing){if(failures<40)fprintf(out,"  [%s] replay draws: %u traced draws not consumed, %u particle ops without a traced draw\n",label.c_str(),leftover,replayMissing);failures++;}
    fprintf(out,"[%s] PS2 %s -> %s (passes %d..%d): %u/%u PS2 effects equal the original harness, %u/%u equal the port;",label.c_str(),stateA.c_str(),stateB.c_str(),first,last,exactOriginal,compared,exactPort,compared);
    for(auto& [k,s2]:stats){if(s2.adopted)fprintf(out," %s %u;",k.c_str(),s2.adopted);else fprintf(out," %s %u/%u;",k.c_str(),s2.exact,s2.ops);}
    fprintf(out," %s\n",failures==before?"exact":"DIFFERS");A=nullptr;
}

int main(int argc,char** argv){
    if(argc<3){fprintf(stderr,"usage: live FOLDER BLOCKS [SCENARIOS]\n");return 2;}
    const std::string folder=argv[1];
    {std::ifstream f(argv[2]);std::string line;while(std::getline(f,line)){std::istringstream s(line);ExportBlock b{};s>>b.resource>>b.builtin;for(auto& w:b.words)s>>std::hex>>w>>std::dec;exported.push_back(b);}}
    fprintf(out,"%zu exported parameter blocks\n",exported.size());
    {live::Machine M(folder+"/unit.ee",folder+"/unit.vuc",folder+"/unit.vud");A=&M;installHooks();unitOracles(argc>4?unsigned(atoi(argv[4])):3000);A=nullptr;}
    if(argc>3){std::ifstream f(argv[3]);std::string line;
        while(std::getline(f,line)){std::istringstream s(line);std::string label,state;int ticks;s>>label>>state>>ticks;std::vector<Launch> l;int slot;uint32_t inst;
            uint32_t rider;s>>std::hex>>rider>>std::dec;RIDER=rider;
            while(s>>slot>>std::hex>>inst>>std::dec)l.push_back({slot,inst});scenario(folder,label,state,l,ticks);RIDER=0x14701A0;}}
    if(argc>5){std::ifstream f(argv[5]);std::string line;std::string label,a,b;int first=0,last=0;uint64_t base=0;std::map<int,std::vector<uint64_t>> q;std::map<int,std::vector<Launch>> l;bool have=false;
        auto flush=[&]{if(have)replay(folder,label,a,b,first,last,base,q,l);q.clear();l.clear();have=false;};
        while(std::getline(f,line)){std::istringstream s(line);std::string tag;s>>tag;
            if(tag=="R"){flush();s>>label>>a>>b>>first>>last>>base;have=true;}
            else if(tag=="Q"){int t;uint64_t i;s>>t;while(s>>i)q[t].push_back(i);}
            else if(tag=="L"){int t,slot;uint32_t inst;s>>t>>slot>>std::hex>>inst;l[t].push_back({slot,inst});}}
        flush();}
    fprintf(out,failures?"FAILED (%u)\n":"all live checks match\n",failures);
    return failures?1:0;
}
