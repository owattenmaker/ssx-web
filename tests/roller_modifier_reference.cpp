// Instruction oracle for engine/roller_modifier.hpp: the recompiled original
// RollerModifier functions (PCSX2 scalar-FP oracle copies) run on synthetic
// memory (ELF image loaded, so vtables and gp constants are the real ones)
// against the port. Only the world query (0x3303F0/0x2D1BE0/0x336850/0x3304E8)
// and the spatial relocation 0x3291E0 are recording stubs.
#include "ps2_runtime_macros.h"
#include "../engine/roller_modifier.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <vector>
#define ORIGINAL(name) void name(uint8_t*,R5900Context*,PS2Runtime*);
ORIGINAL(sub_0035D340_0x35d340) ORIGINAL(sub_0035E248_0x35e248) ORIGINAL(sub_0035D288_0x35d288) ORIGINAL(sub_0035E770_0x35e770)
ORIGINAL(sub_0032C648_0x32c648) ORIGINAL(sub_0035CFF0_0x35cff0) ORIGINAL(sub_0035DDE8_0x35dde8) ORIGINAL(sub_0035D4A0_0x35d4a0)
ORIGINAL(sub_0035D908_0x35d908) ORIGINAL(sub_0035EDC8_0x35edc8) ORIGINAL(sub_0035ED90_0x35ed90) ORIGINAL(sub_0032C630_0x32c630)
ORIGINAL(sub_0032C5A8_0x32c5a8) ORIGINAL(sub_0035DA70_0x35da70) ORIGINAL(sub_0035CFE8_0x35cfe8) ORIGINAL(sub_0032C508_0x32c508)
ORIGINAL(sub_0032C540_0x32c540) ORIGINAL(sub_00327CC8_0x327cc8) ORIGINAL(sub_0031B748_0x31b748) ORIGINAL(sub_0031B7A8_0x31b7a8)
ORIGINAL(sub_003612C0_0x3612c0) ORIGINAL(sub_003568B0_0x3568b0) ORIGINAL(sub_00352B88_0x352b88) ORIGINAL(sub_00352BF8_0x352bf8)
ORIGINAL(sub_00352BC0_0x352bc0) ORIGINAL(sub_00356020_0x356020) ORIGINAL(sub_0035FE10_0x35fe10)
ORIGINAL(sub_003554B0_0x3554b0) ORIGINAL(sub_00355468_0x355468) ORIGINAL(sub_00352AA8_0x352aa8) ORIGINAL(sub_003567E0_0x3567e0)
ORIGINAL(sub_00356780_0x356780) ORIGINAL(sub_00350570_0x350570) ORIGINAL(sub_00352C38_0x352c38) ORIGINAL(sub_00355DB8_0x355db8)
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x440000,g_ps2RecompiledFunctionTableSlotCount=(0x440000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x440000-0x100000)/4]={};
using namespace ssx;
static uint8_t* mem;
static constexpr uint32_t gp=0x4a30f0,done=0x12345678,stackTop=0x90000;
static constexpr uint32_t MOD=0x1000000,SRC=0x1001000,INST=0x1002000,REC=0x1003000,PKT=0x1003100,ARGS=0x1003200,LEVELS=0x1004000,
    OUT=0x1005000,ENTITY=0x1006000,CONTAINER=0x1006100,WORLD=0x1007000,FORCE=0x1008000,BODY=MOD+0x30;
static std::mt19937 rng(0x35E850);
static float uni(float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);}
static uint32_t bitsOf(float f){return std::bit_cast<uint32_t>(f);}
static void put(uint32_t a,const auto& v){std::memcpy(mem+a,&v,sizeof(v));}
static uint32_t word(uint32_t a){uint32_t v;std::memcpy(&v,mem+a,4);return v;}
static float real(uint32_t a){float v;std::memcpy(&v,mem+a,4);return v;}
static RollerQuad quad(uint32_t a){RollerQuad q;std::memcpy(q.data(),mem+a,16);return q;}
static RollerQuad rq(float lo,float hi,float w){return {uni(lo,hi),uni(lo,hi),uni(lo,hi),w};}
static RollerQuad unitQuat(){RollerQuad q{uni(-1,1),uni(-1,1),uni(-1,1),uni(-1,1)};float l=std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);if(l<1e-3f)return {0,0,0,1};for(auto&x:q)x/=l;return q;}
static RollerQuad unitVec(float w=0){RollerQuad q{uni(-1,1),uni(-1,1),uni(-1,1),w};float l=std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]);if(l<1e-3f)return {0,0,1,w};for(unsigned k=0;k<3;k++)q[k]/=l;return q;}
static float scale10(float lo,float hi){return std::pow(10.f,uni(lo,hi));}
static std::array<float,9> symmetric(float magnitude){std::array<float,9> m;float d[3]={uni(.5f,1.5f)*magnitude,uni(.5f,1.5f)*magnitude,uni(.5f,1.5f)*magnitude};
    for(unsigned r=0;r<3;r++)for(unsigned c=0;c<3;c++)m[3*r+c]=r==c?d[r]:0;for(unsigned r=0;r<3;r++)for(unsigned c=r+1;c<3;c++){float o=uni(-.05f,.05f)*magnitude;m[3*r+c]=o;m[3*c+r]=rng()%4?o:o*1.0001f;}return m;}

// ---- runtime stubs ------------------------------------------------------------
struct Event {uint32_t id;std::vector<uint32_t> w;bool operator==(const Event&)const=default;};
static std::vector<Event> events;
struct Pregen {int mode=0;float extra=0;RollerQuad offset{},noise{};float pointW=1,normalW=0;OriginalRollerTerrainCache cache;bool scratch=false;float scratchDepth=0;std::array<int32_t,8> order{};};
static Pregen pregen;
struct StubContact {float depth;RollerQuad point,normal;};
// Deterministic contact generation shared by the stub and the port callback.
static StubContact makeContact(const RollerQuad& pos,const RollerQuad& vel,float leaf){
    OriginalRounding rounding;StubContact s;
    s.depth=pregen.mode==0?-1.f:terrain_original::add(leaf,pregen.extra);
    for(unsigned k=0;k<3;k++)s.point[k]=terrain_original::add(pos[k],pregen.offset[k]);s.point[3]=pregen.pointW;
    RollerQuad n=pregen.noise;
    if(pregen.mode==2){float l=std::sqrt(vel[0]*vel[0]+vel[1]*vel[1]+vel[2]*vel[2]);if(l>1e-3f)for(unsigned k=0;k<3;k++)n[k]=terrain_original::sub(n[k],vel[k]/l);}
    float l=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);if(!(l>1e-4f)){n={0,0,1,0};l=1;}
    for(unsigned k=0;k<3;k++)s.normal[k]=n[k]/l;s.normal[3]=pregen.normalW;
    return s;
}
static float memoryLeaf(){uint32_t header=word(MOD+0xE0+0x98);uint32_t levels=word(header+0x20);OriginalRounding r;return terrain_original::mul(terrain_original::mul(real(levels+12*word(header+0xc)),real(MOD+0xE0+0x90)),.5f);}
static uint32_t queryObject=0;
static void stub(uint8_t*,R5900Context*c,PS2Runtime*){
    const uint32_t pc=c->pc,a0=GPR_U32(c,4),a1=GPR_U32(c,5),a2=GPR_U32(c,6),a3=GPR_U32(c,7);
    switch(pc){
    case 0x3303f0:if(a1!=MOD+0xE0||a2!=1)throw std::runtime_error("3303F0 arguments");queryObject=a0;events.push_back({0x3303f0,{}});SET_GPR_U32(c,2,a0);break;
    case 0x2d1be0:SET_GPR_U32(c,2,WORLD);break;
    case 0x336850:{
        if(a0!=WORLD||a1!=queryObject||a3!=MOD+0x2C0)throw std::runtime_error("336850 arguments");
        auto s=makeContact(quad(BODY),quad(BODY+0x80),memoryLeaf());
        put(a2,s.point);put(a2+16,s.normal);
        put(a3,pregen.cache.patch);put(a3+4,pregen.cache.cellU);put(a3+6,pregen.cache.cellV);put(a3+8,pregen.cache.half);put(a3+12,pregen.cache.detailed);
        if(pregen.scratch){put(MOD+0xE0+0x94,pregen.scratchDepth);put(MOD+0xE0+0x9C,pregen.order);}
        c->f[0]=s.depth;events.push_back({0x336850,{}});break;}
    case 0x3304e8:if(a0!=queryObject||a1!=2)throw std::runtime_error("3304E8 arguments");events.push_back({0x3304e8,{}});break;
    case 0x317d70:if(a0==0x2D0){SET_GPR_U32(c,2,MOD);}else if(a0==0x28){SET_GPR_U32(c,2,CONTAINER);}else throw std::runtime_error("317D70 size");events.push_back({0x317d70,{a0}});break;
    case 0x3291e0:{Event e{0x3291e0,{a0,a1,a2}};uint32_t t0=GPR_U32(c,8);for(uint32_t o=0;o<32;o+=4)e.w.push_back(word(a3+o));for(uint32_t o=0;o<32;o+=4)e.w.push_back(word(t0+o));events.push_back(e);break;}
    default:throw std::runtime_error("Unexpected callee");
    }
    c->pc=GPR_U32(c,31);
}
static OriginalRollerWorldContact portQuery(OriginalRollerModifier& m,OriginalSphereTreeCollider& collider,OriginalRollerTerrainCache& cache){
    events.push_back({0x3303f0,{}});
    float leaf;{OriginalRounding r;leaf=terrain_original::mul(terrain_original::mul(collider.tree->levels.at(m.treeHeader[3]).radiusCm,collider.scale),.5f);}
    auto s=makeContact(m.body.position,m.body.velocity,leaf);
    cache=pregen.cache;if(pregen.scratch){collider.depth=pregen.scratchDepth;collider.order=pregen.order;}
    events.push_back({0x336850,{}});events.push_back({0x3304e8,{}});
    return {s.depth>=0,s.point,s.normal,s.depth};
}

// ---- random state -------------------------------------------------------------
static CollisionSphereTree tree;
static void randomTree(){
    tree=CollisionSphereTree{};unsigned n=1+rng()%5;
    for(unsigned i=0;i<n;i++){CollisionSphereTreeLevel l{uni(5,400)/float(1u<<i),uni(1,100),1u<<(3*i)};tree.levels.push_back(l);put(LEVELS+12*i,l.radiusCm);put(LEVELS+12*i+4,l.childOffsetCm);put(LEVELS+12*i+8,l.stride);}
}
static std::array<uint32_t,32> randomHeader(){
    std::array<uint32_t,32> h;for(auto&x:h)x=rng();
    h[3]=uint32_t(tree.levels.size()-1);h[8]=LEVELS;
    for(unsigned k=0;k<3;k++){h[5+k]=bitsOf(uni(-50,50));h[11+k]=bitsOf(rng()%4?uni(-20,20):0.f);}
    auto inertia=symmetric(uni(1e3,6e3)),inverse=symmetric(uni(5e-5f,3e-4f));
    for(unsigned i=0;i<9;i++){h[14+i]=bitsOf(inertia[i]);h[23+i]=bitsOf(inverse[i]);}
    return h;
}
static float randomTimer(){
    switch(rng()%9){
    case 0:return uni(0,17);case 1:return uni(4.97f,5.03f);case 2:return uni(9.97f,10.f);case 3:return uni(10.97f,11.f);
    case 4:return uni(15.97f,16.f);case 5:return uni(0,5);case 6:return uni(5,10);case 7:return uni(16,20);default:return uni(10,11);
    }
}
static OriginalRollerModifier randomModifier(){
    OriginalRollerModifier m;randomTree();
    auto stale=[&](auto& a){for(auto& x:a)x=rng();};
    m.energy=rng()%3?uni(0,3e6):uni(9000,12000);m.mass=rng()%3?1.f:uni(.3f,4);
    m.scriptParameter=uni(0,1);m.word10=rng();m.restitution=rng()%4?.1f:uni(0,1);m.friction=rng()%6==0?0.f:rng()%3?.4f:uni(0,1);
    m.restitutionSpeed=rng()%3?277.77777099609375f:uni(0,800);m.timer=randomTimer();m.radius=uni(10,300);
    m.contactTimer=rng()%3==0?0.f:rng()%8==0?uni(-.2f,0):uni(0,.4f);m.word2C=rng();
    auto& b=m.body;float s=scale10(-1,3);
    b.position=rq(-2e5,2e5,rng()%4?1.f:uni(-2,2));b.orientation=unitQuat();if(rng()%10==0)for(auto&x:b.orientation)x*=uni(.9f,1.1f);
    b.linearMomentum=rq(-s,s,rng()%5?0.f:uni(-1,1));b.angularMomentum=rq(-50*s,50*s,rng()%5?0.f:uni(-1,1));
    b.inverseMass=rng()%20==0?0.f:rng()%2?1.f/m.mass:uni(.2f,3);stale(b.stale44);b.inverseInertia=symmetric(uni(5e-5f,3e-4f));
    if(rng()%10==0)for(auto&x:b.inverseInertia)x=uni(-1e-3f,1e-3f);
    stale(b.stale74);b.velocity=rq(-s,s,rng()%5?0.f:uni(-1,1));b.angularVelocity=rq(-10,10,rng()%5?0.f:uni(-1,1));
    m.instance=INST;stale(m.staleD4);
    for(auto& c:m.collider.corners)c=rq(-2e5,2e5,uni(-1,1));m.collider.center=rq(-2e5,2e5,1);m.collider.scale=rng()%3?1.f:uni(.5f,2);
    m.collider.depth=uni(-5,5);for(auto&x:m.collider.order)x=int32_t(rng());m.collider.tree=&tree;
    m.colliderWordBC=rng();m.treeHeader=randomHeader();
    m.boundsMin=rq(-2e5,2e5,1);m.boundsMax=rq(-2e5,2e5,1);for(auto& r:m.matrix)r=rq(-2,2,uni(-1,1));
    for(unsigned i=0;i<9;i++)m.bodyInertia[i]=std::bit_cast<float>(m.treeHeader[23+i])*.5f;stale(m.stale2A4);
    m.centerOfMass={std::bit_cast<float>(m.treeHeader[11]),std::bit_cast<float>(m.treeHeader[12]),std::bit_cast<float>(m.treeHeader[13]),0};
    if(rng()%4==0)m.centerOfMass=rq(-20,20,0);
    m.cache.patch=rng();m.cache.cellU=uint16_t(rng());m.cache.cellV=uint16_t(rng());m.cache.half=rng();m.cache.detailed=rng();
    return m;
}
static void randomPregen(bool nearGround=true){
    Pregen& g=pregen;unsigned k=rng()%10;g.mode=k<2?0:k<6?2:1;
    g.extra=rng()%10==0?0.f:uni(-5,nearGround?40.f:5.f);g.offset=rq(-60,60,0);g.noise=rq(-.6f,.6f,0);if(g.mode==1)g.noise=unitVec();
    g.pointW=rng()%4?1.f:uni(-1,1);g.normalW=rng()%5?0.f:uni(-.2f,.2f);
    g.cache.patch=rng();g.cache.cellU=uint16_t(rng()%9);g.cache.cellV=uint16_t(rng()%9);g.cache.half=rng()%2;g.cache.detailed=rng()%2;
    g.scratch=rng()%2;g.scratchDepth=uni(-10,10);for(auto&x:g.order)x=int32_t(rng()%8);
}
static void store(const OriginalRollerModifier& m){auto b=originalRollerModifierBytes(m,MOD);std::memcpy(mem+MOD,b.data(),b.size());}
static int compare(const OriginalRollerModifier& m,const char* what,unsigned n){
    auto b=originalRollerModifierBytes(m,MOD);
    if(!std::memcmp(mem+MOD,b.data(),b.size()))return 0;
    printf("%s mismatch case %u:\n",what,n);
    for(unsigned o=0;o<b.size();o+=4){uint32_t x=word(MOD+o),y;std::memcpy(&y,b.data()+o,4);if(x!=y)printf("  +%03x original %08x (%g) port %08x (%g)\n",o,x,std::bit_cast<float>(x),y,std::bit_cast<float>(y));}
    return 1;
}
static PS2Runtime* runtime;
static R5900Context context(uint32_t pc){
    R5900Context c{};c.pc=pc;
    for(unsigned i=1;i<32;i++)c.vu0_vf[i]=_mm_set_ps(uni(-1e3,1e3),uni(-1e3,1e3),uni(-1e3,1e3),uni(-1e3,1e3));
    c.vu0_acc=_mm_set_ps(uni(-1e3,1e3),uni(-1e3,1e3),uni(-1e3,1e3),uni(-1e3,1e3));c.vu0_vf[0]=_mm_set_ps(1,0,0,0);
    for(unsigned i=0;i<32;i++)c.f[i]=uni(-1e3,1e3);
    for(unsigned r:{1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u,13u,14u,15u,24u,25u})SET_GPR_U64(&c,r,(uint64_t(rng())<<32)|rng());
    SET_GPR_U32(&c,28,gp);SET_GPR_U32(&c,29,stackTop);SET_GPR_U32(&c,31,done);return c;
}
using Original=void(*)(uint8_t*,R5900Context*,PS2Runtime*);
// A call back into the same recompiled file returns to the dispatcher with the
// continuation pc (like the runtime loop): resume through the file's switch.
static void run(Original f,R5900Context& c){
    OriginalRounding rounding;
    for(unsigned guard=0;c.pc!=done;guard++){
        if(guard>64||runtime->isStopRequested()){printf("incomplete pc %x\n",c.pc);throw std::runtime_error("original incomplete");}
        f(mem,&c,runtime);
    }
}

int main(int argc,char** argv){
    if(argc<2){puts("usage: reference SLUS_207.72");return 2;}
    std::vector<uint8_t> memory(32*1024*1024);mem=memory.data();
    {std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(f)),{});if(elf.size()<0x3b0000)return 3;std::memcpy(mem+0xFF000,elf.data(),elf.size());}
    for(unsigned i=0;i<8;i++){RollerQuad row{i&4?1.f:-1.f,i&2?1.f:-1.f,i&1?1.f:-1.f,0};put(0x4FF640+16*i,row);}   // BSS 0x4FF640 (savestates)
    put(0x4FF130,RollerQuad{0,0,0,1});
    PS2Runtime rt;runtime=&rt;rt.setMissingFunctionPolicy(PS2Runtime::MissingFunctionPolicy::Stop);
    auto reg=[&](uint32_t pc,Original f){if(!rt.registerFunction(pc,f))throw std::runtime_error("registration");};
    for(uint32_t pc:{0x3303f0u,0x2d1be0u,0x336850u,0x3304e8u,0x3291e0u,0x317d70u})reg(pc,stub);
    reg(0x35d340,sub_0035D340_0x35d340);reg(0x35e248,sub_0035E248_0x35e248);reg(0x35d288,sub_0035D288_0x35d288);reg(0x35e770,sub_0035E770_0x35e770);
    reg(0x35e850,sub_0035E770_0x35e770);reg(0x32c648,sub_0032C648_0x32c648);reg(0x35cff0,sub_0035CFF0_0x35cff0);reg(0x35dde8,sub_0035DDE8_0x35dde8);
    reg(0x35d4a0,sub_0035D4A0_0x35d4a0);reg(0x35d908,sub_0035D908_0x35d908);reg(0x35edc8,sub_0035EDC8_0x35edc8);reg(0x35ed90,sub_0035ED90_0x35ed90);
    reg(0x32c630,sub_0032C630_0x32c630);reg(0x32c5a8,sub_0032C5A8_0x32c5a8);reg(0x35da70,sub_0035DA70_0x35da70);reg(0x35cfe8,sub_0035CFE8_0x35cfe8);
    reg(0x32c508,sub_0032C508_0x32c508);reg(0x32c540,sub_0032C540_0x32c540);reg(0x327cc8,sub_00327CC8_0x327cc8);reg(0x31b748,sub_0031B748_0x31b748);
    reg(0x31b7a8,sub_0031B7A8_0x31b7a8);reg(0x3568b0,sub_003568B0_0x3568b0);reg(0x352b88,sub_00352B88_0x352b88);reg(0x352bf8,sub_00352BF8_0x352bf8);
    reg(0x352bc0,sub_00352BC0_0x352bc0);for(uint32_t pc:{0x361cd8u,0x361d18u,0x361d20u,0x361d00u,0x361d30u,0x361d38u})reg(pc,sub_003612C0_0x3612c0);
    reg(0x356078,sub_00356020_0x356020);reg(0x360be8,sub_0035FE10_0x35fe10);
    reg(0x3554b0,sub_003554B0_0x3554b0);reg(0x355468,sub_00355468_0x355468);reg(0x352aa8,sub_00352AA8_0x352aa8);reg(0x3567e0,sub_003567E0_0x3567e0);
    reg(0x356780,sub_00356780_0x356780);reg(0x350570,sub_00350570_0x350570);reg(0x352c38,sub_00352C38_0x352c38);reg(0x355db8,sub_00355DB8_0x355db8);
    reg(0x361d28,sub_003612C0_0x3612c0);
    unsigned total=0;
    // ---- 0x35D340 integrate ----
    {
        unsigned zeroQ=0;
        for(unsigned n=0;n<20000;n++){
            auto m=randomModifier();if(n%50==0){m.body.orientation={0,0,0,0};zeroQ++;}
            RollerQuad force=rq(-1e4,1e4,uni(-1,1)),torque=rq(-1e4,1e4,uni(-1,1));float dt=rng()%3?1.f/60:uni(0,.1f);
            if(n%3==0)dt=std::bit_cast<float>(0x3c888889u);
            store(m);put(FORCE,force);put(FORCE+16,torque);
            auto c=context(0x35d340);SET_GPR_U32(&c,4,BODY);SET_GPR_U32(&c,5,FORCE);SET_GPR_U32(&c,6,FORCE+16);c.f[12]=dt;run(sub_0035D340_0x35d340,c);
            originalRollerIntegrate(m.body,force,torque,dt);if(compare(m,"35D340",n))return 1;
        }
        printf("20,000 original 35D340 integrations match (%u zero quaternions)\n",zeroQ);total+=20000;
    }
    // ---- 0x35E248 world inverse inertia, 0x35D288 velocities, 0x35E770 pose ----
    for(unsigned n=0;n<10000;n++){
        auto m=randomModifier();store(m);auto c=context(0x35e248);SET_GPR_U32(&c,4,MOD);run(sub_0035E248_0x35e248,c);
        originalRollerWorldInverseInertia(m);if(compare(m,"35E248",n))return 1;
        m=randomModifier();store(m);c=context(0x35d288);SET_GPR_U32(&c,4,BODY);run(sub_0035D288_0x35d288,c);
        originalRollerVelocities(m.body);if(compare(m,"35D288",n))return 1;
        m=randomModifier();store(m);std::array<uint8_t,32> sentinel;for(auto&x:sentinel)x=uint8_t(rng());std::memcpy(mem+OUT,sentinel.data(),32);
        c=context(0x35e770);SET_GPR_U32(&c,4,OUT);SET_GPR_U32(&c,5,MOD);run(sub_0035E770_0x35e770,c);
        auto pose=originalRollerPose(m);
        if(std::memcmp(mem+OUT,pose.position.data(),16)){printf("35E770 position mismatch %u\n",n);return 1;}
        if(std::memcmp(mem+OUT+16,pose.orientation.data(),16)){printf("35E770 orientation mismatch %u\n",n);return 1;}
        if(compare(m,"35E770 (object)",n))return 1;
    }
    printf("10,000 each original 35E248 world inverse inertia / 35D288 velocities / 35E770 pose match\n");total+=30000;
    // ---- 0x32C648 collider transform ----
    for(unsigned n=0;n<10000;n++){
        auto m=randomModifier();store(m);RollerMatrix matrix;for(auto&r:matrix)r=rq(-3,3,uni(-1,1));matrix[3]=rq(-2e5,2e5,rng()%2?1.f:uni(-2,2));
        float s=rng()%2?1.f:uni(.1f,3);put(OUT,matrix);
        auto c=context(0x32c648);SET_GPR_U32(&c,4,MOD+0xE0);SET_GPR_U32(&c,5,OUT);c.f[12]=s;run(sub_0032C648_0x32c648,c);
        originalRollerColliderTransform(m.collider,m.treeHeader,matrix,s);if(compare(m,"32C648",n))return 1;
    }
    printf("10,000 original 32C648 collider corner/centre transforms match\n");total+=10000;
    // ---- 0x35CFF0 angular velocity, 0x35D908 impulse ----
    for(unsigned n=0;n<10000;n++){
        auto m=randomModifier();store(m);RollerQuad w=rq(-20,20,uni(-1,1));put(FORCE,w);
        auto c=context(0x35cff0);SET_GPR_U32(&c,4,BODY);SET_GPR_U32(&c,5,FORCE);run(sub_0035CFF0_0x35cff0,c);
        originalRollerSetAngularVelocity(m.body,w);if(compare(m,"35CFF0",n))return 1;
        m=randomModifier();store(m);RollerQuad lever=rq(-100,100,uni(-1,1)),impulse=rq(-1e3,1e3,uni(-1,1));put(FORCE,lever);put(FORCE+16,impulse);
        c=context(0x35d908);SET_GPR_U32(&c,4,BODY);SET_GPR_U32(&c,5,FORCE);SET_GPR_U32(&c,6,FORCE+16);run(sub_0035D908_0x35d908,c);
        originalRollerApplyImpulse(m.body,lever,impulse);if(compare(m,"35D908",n))return 1;
    }
    printf("10,000 each original 35CFF0 angular-velocity set / 35D908 impulse match\n");total+=20000;
    // ---- 0x35DDE8 kick, 0x361CD8 re-contact ----
    auto randomRecord=[&]{OriginalRollerContactRecord r;r.point=rq(-2e5,2e5,1);r.direction=unitVec(rng()%5?0.f:uni(-1,1));r.normal=unitVec(rng()%5?0.f:uni(-1,1));
        r.closingSpeed=rng()%5?uni(0,2000):0.f;put(REC,r.point);put(REC+16,r.direction);put(REC+32,r.normal);put(REC+48,r.closingSpeed);
        for(unsigned o=52;o<64;o+=4)put(REC+o,uint32_t(rng()));return r;};
    {
        unsigned capped=0;
        for(unsigned n=0;n<10000;n++){
            auto m=randomModifier();if(n%4==0)m.mass=uni(1,100);if(n%7==0)m.mass=uni(.1f,.5f);store(m);auto r=randomRecord();
            auto c=context(0x35dde8);SET_GPR_U32(&c,4,MOD);SET_GPR_U32(&c,5,REC);SET_GPR_U32(&c,6,PKT);run(sub_0035DDE8_0x35dde8,c);
            originalRollerKick(m,r);if(compare(m,"35DDE8",n))return 1;capped+=m.body.velocity[2]!=0&&5000.f/m.mass+100>600;
            m=randomModifier();store(m);r=randomRecord();
            c=context(0x361cd8);SET_GPR_U32(&c,4,MOD);SET_GPR_U32(&c,5,REC);SET_GPR_U32(&c,6,PKT);run(sub_003612C0_0x3612c0,c);
            originalRollerRecontact(m,r);if(compare(m,"361CD8",n))return 1;
        }
        printf("10,000 each original 35DDE8 kick / 361CD8 re-contact match (%u capped lifts)\n",capped);total+=20000;
    }
    // ---- 0x35D4A0 contact response ----
    {
        unsigned paths[2][2]={},frictionOff=0,slow=0;
        for(unsigned n=0;n<30000;n++){
            auto m=randomModifier();store(m);
            RollerQuad point=m.body.position;for(unsigned k=0;k<3;k++)point[k]+=uni(-60,60);point[3]=rng()%4?1.f:uni(-1,1);
            RollerQuad normal=rng()%2?unitVec(rng()%5?0.f:uni(-.2f,.2f)):RollerQuad{0,0,0,0};
            if(normal[0]==0&&normal[1]==0){float l=std::sqrt(m.body.velocity[0]*m.body.velocity[0]+m.body.velocity[1]*m.body.velocity[1]+m.body.velocity[2]*m.body.velocity[2]);
                normal=unitVec();if(l>0)for(unsigned k=0;k<3;k++)normal[k]=-m.body.velocity[k]/l+uni(-.3f,.3f);}
            if(n%13==0){normal={0,0,0,0};normal[rng()%3]=rng()%2?1.f:-1.f;float k=-uni(1,100);m.body.angularVelocity={0,0,0,0};
                m.body.velocity=normal;for(auto&x:m.body.velocity)x*=rng()%3?k:-k;store(m);}   // zero slip (0x35D5D0 branch)
            float e=rng()%3?0.f:uni(0,1),mu=rng()%4==0?0.f:uni(0,1);frictionOff+=mu==0;
            put(FORCE,point);put(FORCE+16,normal);
            auto c=context(0x35d4a0);SET_GPR_U32(&c,4,BODY);SET_GPR_U32(&c,5,FORCE);SET_GPR_U32(&c,6,FORCE+16);c.f[12]=e;c.f[13]=mu;run(sub_0035D4A0_0x35d4a0,c);
            auto r=originalRollerContactResponse(m.body,point,normal,e,mu);if(compare(m,"35D4A0",n)){printf("e %g mu %g fr %d no %d n %g %g %g %g im %g\n",e,mu,r.friction,r.normal,normal[0],normal[1],normal[2],normal[3],m.body.inverseMass);return 1;}
            paths[r.friction][r.normal]++;slow+=mu!=0&&!r.friction;
        }
        printf("30,000 original 35D4A0 contact responses match (friction+normal %u, friction only %u, normal only %u, none %u; friction off %u, slip <= 0.001 or skipped %u)\n",
            paths[1][1],paths[1][0],paths[0][1],paths[0][0],frictionOff,slow);total+=30000;
    }
    // ---- 0x35EDC8 collision step (query stubbed) ----
    {
        unsigned none=0,separating=0,approaching=0,elastic=0,timerSet=0;
        for(unsigned n=0;n<30000;n++){
            auto m=randomModifier();if(n%3==0){m.body.position=rq(-2e5,2e5,1);}store(m);randomPregen();events.clear();
            auto c=context(0x35edc8);SET_GPR_U32(&c,4,MOD);run(sub_0035EDC8_0x35edc8,c);
            auto originalEvents=events;events.clear();float before=m.timer;
            auto r=originalRollerCollision(m,[&](OriginalSphereTreeCollider& k,OriginalRollerTerrainCache& cache){return portQuery(m,k,cache);});
            if(compare(m,"35EDC8",n))return 1;if(originalEvents!=events){printf("35EDC8 query calls %u\n",n);return 1;}
            if(!r.contact)none++;else if(!r.approaching)separating++;else{approaching++;elastic+=!(r.speed<m.restitutionSpeed);timerSet+=5.f<before;}
        }
        printf("30,000 original 35EDC8 collision steps match (no contact %u, separating %u, approaching %u: restitution %u, timer->10 %u)\n",none,separating,approaching,elastic,timerSet);total+=30000;
    }
    // ---- 0x35E850 whole update (query stubbed) ----
    {
        unsigned paths[4]={},restEnergy=0,zeroMass=0,contacts=0,clamp=0;
        for(unsigned n=0;n<40000;n++){
            auto m=randomModifier();if(n%5==0)m.body.position[2]=uni(-100,100);store(m);randomPregen();events.clear();
            auto c=context(0x35e850);SET_GPR_U32(&c,4,MOD);run(sub_0035E770_0x35e770,c);
            auto originalEvents=events;events.clear();float energy=m.energy,timer=m.timer,t2=m.contactTimer;
            auto r=originalRollerUpdate(m,[&](OriginalSphereTreeCollider& k,OriginalRollerTerrainCache& cache){return portQuery(m,k,cache);});
            if(compare(m,"35E850",n))return 1;if(originalEvents!=events){printf("35E850 query calls %u\n",n);return 1;}
            paths[int(r.path)]++;zeroMass+=m.body.inverseMass==0;contacts+=r.collision.approaching;
            if(r.path==OriginalRollerPath::Rest&&m.energy<10000&&timer+1.f/60<10)restEnergy++;
            clamp+=r.path==OriginalRollerPath::Dynamics&&t2*.8333333f*10>2;(void)energy;
        }
        printf("40,000 original 35E850 updates match (dynamics %u, rest %u [energy %u], frozen %u, sinking %u; zero inverse mass %u, approaching contacts %u, grip clamp %u)\n",
            paths[0],paths[1],restEnergy,paths[2],paths[3],zeroMass,contacts,clamp);total+=40000;
    }
    // ---- 0x31B748/0x31B7A8 matrix -> quaternion ----
    auto randomMatrix=[&]{RollerMatrix M;auto q=unitQuat();
        RollerMatrix r=roller_math::quaternionMatrix(q,{0,0,0,1});
        float s[3]={1,1,1};if(rng()%4==0)for(auto&x:s)x=uni(.5f,2);if(rng()%8==0)s[1]=s[2]=s[0];
        for(unsigned i=0;i<3;i++)for(unsigned k=0;k<4;k++)M[i][k]=k<3?r[i][k]*s[i]:(rng()%5?0.f:uni(-1,1));
        M[3]=rq(-2e5,2e5,rng()%5?1.f:uni(-2,2));if(rng()%40==0)for(unsigned i=0;i<3;i++)for(unsigned k=0;k<3;k++)M[i][k]=0;
        if(rng()%30==0)for(unsigned i=0;i<3;i++)for(unsigned k=0;k<3;k++)M[i][k]=uni(-1,1);return M;};
    {
        unsigned branch[4]={};
        for(unsigned n=0;n<20000;n++){
            auto M=randomMatrix();put(INST+0x10,M);
            auto c=context(0x31b748);SET_GPR_U32(&c,4,OUT);SET_GPR_U32(&c,5,INST+0x10);run(sub_0031B748_0x31b748,c);
            auto q=originalRollerMatrixQuaternion(M);
            if(std::memcmp(mem+OUT,M[3].data(),16)||std::memcmp(mem+OUT+16,q.data(),16)){printf("31B748 mismatch %u\n",n);return 1;}
            float trace=M[0][0]+M[1][1]+M[2][2];unsigned i=0;if(M[0][0]<M[1][1])i=1;if(M[i][i]<M[2][2])i=2;branch[trace>0?3:i]++;
        }
        printf("20,000 original 31B748/31B7A8 matrix->quaternion conversions match (positive trace %u, x %u, y %u, z %u)\n",branch[3],branch[0],branch[1],branch[2]);total+=20000;
    }
    // ---- 0x35DA70 constructor / builtin15 0x355DB8 (+0x3554B0 attach) ----
    {
        unsigned attached=0;
        for(unsigned n=0;n<20000;n++){
            randomTree();std::array<uint8_t,0x2D0> staleBytes;for(auto&x:staleBytes)x=n%10==0?0:uint8_t(rng());std::memcpy(mem+MOD,staleBytes.data(),0x2D0);
            OriginalRollerSource source;for(auto&q:source.corners)q=rq(-2,2,uni(-1,1));source.center=rq(-100,100,1);source.treeHeader=randomHeader();
            source.tree=&tree;source.instanceMatrix=randomMatrix();source.instance=INST;
            for(uint32_t o=0;o<0x140;o+=4)put(SRC+o,uint32_t(rng()));put(SRC,source.corners);put(SRC+0x80,source.center);put(SRC+0x98,SRC+0xC0);put(SRC+0xC0,source.treeHeader);
            uint32_t flags=rng();put(INST+8,flags);put(INST+0x10,source.instanceMatrix);
            for(uint32_t o=0;o<128;o+=4)put(PKT+o,uint32_t(rng()));put(PKT+0x48,SRC);put(PKT+0x50,INST);
            OriginalRollerScriptArgs args{rng()%3?1.f:uni(.2f,5),n%2?.699999988079071f:uni(0,1)};
            put(ARGS,uint32_t(rng()));put(ARGS+4,args.mass);put(ARGS+8,args.parameter);put(ARGS+12,uni(0,1));
            auto record=randomRecord();
            bool builtin=n%2;std::array<float,3> low{uni(-2e5,2e5),uni(-2e5,2e5),uni(-2e5,2e5)},high=low;for(auto&x:high)x+=uni(0,500);
            if(n%17==0)std::swap(low,high);put(INST+0x60,low);put(INST+0x6C,high);
            for(uint32_t o=0;o<0x28;o+=4)put(CONTAINER+o,uint32_t(rng()));events.clear();
            if(builtin){   // builtin15 0x355DB8: 317D70 (stub) -> 35DA70 -> 3554B0 attach (fresh entity: 355468 container)
                put(ENTITY+0xC,0x490E80u);put(ENTITY+0x18,INST);put(ENTITY+0x1C,0u);
                auto c=context(0x355db8);SET_GPR_U32(&c,4,ENTITY);SET_GPR_U32(&c,5,ARGS);SET_GPR_U32(&c,6,PKT);SET_GPR_U32(&c,7,REC);run(sub_00355DB8_0x355db8,c);
                if(word(ENTITY+0x1C)!=CONTAINER||word(CONTAINER)!=MOD||events.size()!=2)return 8;
            }else{
                auto c=context(0x35da70);SET_GPR_U32(&c,4,MOD);SET_GPR_U32(&c,5,ARGS);SET_GPR_U32(&c,6,PKT);SET_GPR_U32(&c,7,REC);run(sub_0035DA70_0x35da70,c);
                if(GPR_U32((&c),2)!=MOD)return 7;
            }
            auto m=originalRollerModifierFromBytes(staleBytes.data(),&tree);
            uint32_t edited=originalRollerConstruct(m,args,source,flags,record);
            if(builtin){edited=originalRollerAttach(m,source.instanceMatrix[3],low,high,edited);attached++;}
            if(compare(m,"35DA70",n))return 1;
            if(word(INST+8)!=edited){printf("35DA70 flags %u: %08x/%08x\n",n,word(INST+8),edited);return 1;}
        }
        printf("20,000 original constructions match: %u 35DA70, %u builtin15 355DB8 (35DA70 + 3554B0 attach: bounds/radius) (every byte of the 0x2D0 object incl. stale words and pointers; instance flag edits)\n",20000-attached,attached);total+=20000;
    }
    // ---- 0x3568B0 entity bounds (0x3291E0 stubbed) ----
    for(unsigned n=0;n<5000;n++){
        auto m=randomModifier();store(m);put(ENTITY+0xC,0x490E80u);put(ENTITY+0x18,INST);put(ENTITY+0x1C,CONTAINER);put(CONTAINER,MOD);events.clear();
        auto c=context(0x3568b0);SET_GPR_U32(&c,4,ENTITY);run(sub_003568B0_0x3568b0,c);
        auto originalEvents=events;events.clear();
        auto b=originalRollerEntityBounds(m);
        Event e{0x3291e0,{WORLD,0,INST}};for(auto*q:{&b.min,&b.max,&b.oldMin,&b.oldMax})for(float x:*q)e.w.push_back(bitsOf(x));
        if(compare(m,"3568B0",n))return 1;if(originalEvents!=std::vector<Event>{e}){printf("3568B0 relocation arguments %u\n",n);return 1;}
    }
    printf("5,000 original 3568B0 entity bounds match (+0x220/+0x230 and 3291E0 new/old bounds)\n");total+=5000;
    printf("%u randomized original/port cases match\n",total);
}
