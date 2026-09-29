// Instruction oracle for engine/flag_cloth.hpp: the recompiled original flag
// functions (PCSX2 scalar-FP oracle copies) run on synthetic memory with the
// ELF image loaded (gp constants, sine generator) against the port.
//   0x392DF0 (+0x31BF60) sine table, 0x34AC88 parameters, 0x34BCA0 vertices,
//   0x34B818 per-cloth tick, 0x34C668 manager tick (wind + slots), 0x34B228 build.
// Stubs: 0x2D1CB0 (manager), 0x2D1C98 (frame), 0x2D1BA0 (wind mode), 0x3177F0
// (RNG word), 0x317E30 (alloc), renderer vtable +0x308/+0x260 and mesh
// vtable +0x18/+0x30 (recording).
#include "ps2_runtime_macros.h"
#include "../engine/flag_cloth.hpp"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <vector>
#define ORIGINAL(name) void name(uint8_t*,R5900Context*,PS2Runtime*);
ORIGINAL(sub_00392DF0_0x392df0) ORIGINAL(sub_0031BF60_0x31bf60) ORIGINAL(sub_0034AC88_0x34ac88) ORIGINAL(sub_0034BCA0_0x34bca0)
ORIGINAL(sub_0034B818_0x34b818) ORIGINAL(sub_0034C600_0x34c600) ORIGINAL(sub_0034B228_0x34b228)
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x440000,g_ps2RecompiledFunctionTableSlotCount=(0x440000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x440000-0x100000)/4]={};
using namespace ssx;
static uint8_t* mem;
static constexpr uint32_t gp=0x4a30f0,done=0x12345678,stackTop=0x90000,TABLE=0x504FB8;
static constexpr uint32_t MGR=0x1000000,ARGS=0x1010000,OUT=0x1011000,VERTS=0x1012000,FPS=0x1013000,RENDERER=0x1014000,RVT=0x1016000,
    MESH=0x1017000,MVT=0x1017100,INSTANCE=0x1018000,CAPTURE=0x1019000;
static constexpr uint32_t STUB_CORNERS=0x43f000,STUB_CREATE=0x43f010,STUB_UPDATE=0x43f020,STUB_SET=0x43f030;
static std::mt19937 rng(0x34BCA0);
static float uni(float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);}
template<class T> static void put(uint32_t a,const T& v){std::memcpy(mem+a,&v,sizeof(v));}
static uint32_t word(uint32_t a){uint32_t v;std::memcpy(&v,mem+a,4);return v;}
static float real(uint32_t a){float v;std::memcpy(&v,mem+a,4);return v;}

// ---- stubs ---------------------------------------------------------------------
static int writtenFor(const OriginalFlagCloth& c,int h){return c.parameters.pinFirstRow?8*(h-1):8*h;}
static int32_t stubFrame=0,stubMode=1;static std::vector<uint32_t> randomWords;static size_t randomAt=0;
struct Event {uint32_t pc;std::vector<uint8_t> data;bool operator==(const Event&)const=default;};
static std::vector<Event> events;
static OriginalFlagCorners stubCorners;static uint32_t meshHandle=MESH;static int gridSize=0;
// 0x34BCA0 leaves the last row unwritten with pinFirstRow (copy branch skips the
// increments): positions beyond this many vertices are stack garbage, zeroed in both.
static int writtenVertices=0;
static void clip(std::vector<uint8_t>& d){for(size_t b=size_t(writtenVertices)*12;b<size_t(gridSize)*12&&b<d.size();b++)d[b]=0;}
static void stub(uint8_t*,R5900Context*c,PS2Runtime*){
    const uint32_t pc=c->pc,a0=GPR_U32(c,4),a1=GPR_U32(c,5),a2=GPR_U32(c,6),a3=GPR_U32(c,7),t0=GPR_U32(c,8),t1=GPR_U32(c,9);
    switch(pc){
    case 0x2d1cb0:SET_GPR_U32(c,2,MGR);break;
    case 0x2d1c98:SET_GPR_U32(c,2,uint32_t(stubFrame));break;
    case 0x2d1ba0:SET_GPR_U32(c,2,uint32_t(stubMode));break;
    case 0x3177f0:if(randomAt>=randomWords.size())throw std::runtime_error("RNG exhausted");SET_GPR_U32(c,2,randomWords[randomAt++]);break;
    case 0x317e30:events.push_back({pc,{uint8_t(a0),uint8_t(a0>>8)}});SET_GPR_U32(c,2,VERTS);break;
    case STUB_CORNERS:{
        if(a0!=RENDERER||a1!=INSTANCE||a2!=0)throw std::runtime_error("corner arguments");
        for(unsigned i=0;i<4;i++){put(a3+12*i,stubCorners.position[i]);put(t0+8*i,stubCorners.uv[i]);put(t1+16*i,stubCorners.colour[i]);}
        break;}
    case STUB_CREATE:if(a0!=RENDERER)throw std::runtime_error("create arguments");events.push_back({pc,{}});SET_GPR_U32(c,2,meshHandle);break;
    case STUB_UPDATE:{if(a0!=MESH)throw std::runtime_error("update arguments");Event e{pc,std::vector<uint8_t>(mem+a1,mem+a1+gridSize*12)};clip(e.data);events.push_back(e);break;}
    case STUB_SET:{if(a0!=MESH)throw std::runtime_error("set arguments");Event e{pc,std::vector<uint8_t>(mem+a1,mem+a1+gridSize*12)};
        clip(e.data);e.data.insert(e.data.end(),mem+a2,mem+a2+gridSize*16);e.data.insert(e.data.end(),mem+a3,mem+a3+gridSize*8);events.push_back(e);break;}
    default:throw std::runtime_error("Unexpected callee");
    }
    c->pc=GPR_U32(c,31);
}
static PS2Runtime* runtime;
static R5900Context context(uint32_t pc){
    R5900Context c{};c.pc=pc;
    for(unsigned i=1;i<32;i++)c.vu0_vf[i]=_mm_set_ps(uni(-1e3,1e3),uni(-1e3,1e3),uni(-1e3,1e3),uni(-1e3,1e3));c.vu0_vf[0]=_mm_set_ps(1,0,0,0);
    for(unsigned i=0;i<32;i++)c.f[i]=uni(-1e3,1e3);
    for(unsigned r:{1u,2u,3u,4u,5u,6u,7u,8u,9u,10u,11u,12u,13u,14u,15u,16u,17u,18u,19u,20u,21u,22u,23u,24u,25u,30u})SET_GPR_U64(&c,r,(uint64_t(rng())<<32)|rng());
    SET_GPR_U32(&c,28,gp);SET_GPR_U32(&c,29,stackTop);SET_GPR_U32(&c,31,done);return c;
}
using Original=void(*)(uint8_t*,R5900Context*,PS2Runtime*);
static void run(Original f,R5900Context& c){
    OriginalRounding rounding;
    for(unsigned guard=0;c.pc!=done;guard++){
        if(guard>256||runtime->isStopRequested()){printf("incomplete pc %x\n",c.pc);throw std::runtime_error("original incomplete");}
        f(mem,&c,runtime);
    }
}

// ---- cloth memory image (slot layout 0x188 bytes) -------------------------------
static void storeCloth(uint32_t a,const OriginalFlagCloth& c){
    std::memcpy(mem+a,&c.parameters,0x58);put(a+0x58,c.model);put(a+0x5C,c.count);put(a+0x60,c.mesh?meshHandle:0u);
    put(a+0x64,c.width);put(a+0x68,c.height);put(a+0x6C,c.widthSpan);put(a+0x70,c.heightSpan);put(a+0x74,c.base.empty()?0u:VERTS);
    for(size_t i=0;i<c.base.size();i++)put(VERTS+12*i,c.base[i]);
    put(a+0x78,c.phase);put(a+0x88,c.uvOffset);put(a+0x90,c.parity);
    for(unsigned i=0;i<60;i++)put(a+0x94+4*i,i<c.instances.size()?c.instances[i]:0u);
}
static OriginalFlagCloth loadCloth(uint32_t a){
    OriginalFlagCloth c;std::memcpy(&c.parameters,mem+a,0x58);c.model=word(a+0x58);c.count=int(word(a+0x5C));c.mesh=word(a+0x60)!=0;
    c.width=int(word(a+0x64));c.height=int(word(a+0x68));c.widthSpan=real(a+0x6C);c.heightSpan=real(a+0x70);
    if(word(a+0x74)&&c.width>0&&c.height>0&&c.width*c.height<=256){c.base.resize(c.width*c.height);for(size_t i=0;i<c.base.size();i++)std::memcpy(&c.base[i],mem+word(a+0x74)+12*i,12);}
    std::memcpy(&c.phase,mem+a+0x78,16);std::memcpy(&c.uvOffset,mem+a+0x88,8);c.parity=int(word(a+0x90));
    for(int i=0;i<c.count&&i<60;i++)c.instances.push_back(word(a+0x94+4*i));
    return c;
}
static bool sameCloth(const OriginalFlagCloth& x,const OriginalFlagCloth& y){
    return !std::memcmp(&x.parameters,&y.parameters,0x58)&&x.model==y.model&&x.count==y.count&&x.mesh==y.mesh&&x.width==y.width&&x.height==y.height&&
        std::bit_cast<uint32_t>(x.widthSpan)==std::bit_cast<uint32_t>(y.widthSpan)&&std::bit_cast<uint32_t>(x.heightSpan)==std::bit_cast<uint32_t>(y.heightSpan)&&
        !std::memcmp(x.phase.data(),y.phase.data(),16)&&!std::memcmp(x.uvOffset.data(),y.uvOffset.data(),8)&&x.parity==y.parity&&
        x.base.size()==y.base.size()&&(x.base.empty()||!std::memcmp(x.base.data(),y.base.data(),x.base.size()*12));
}

// ---- random inputs ---------------------------------------------------------------
static float pick(std::initializer_list<float> xs){auto it=xs.begin();std::advance(it,rng()%xs.size());return *it;}
static OriginalFlagParameters randomParameters(){
    OriginalFlagParameters p;
    auto flag=[&]{return uint8_t(rng()%3?rng()%2:1);};
    p.attachStart=flag();p.attachEnd=flag();p.pinFirstRow=uint8_t(rng()%6==0);p.drawMode=uint8_t(rng()%2);
    if(rng()%4==0){p.attachStart=1;p.attachEnd=0;}
    for(unsigned i=0;i<4;i++){
        p.speed[i]=rng()%4?uni(0,.05f):pick({0.f,1.f/60,.02f});
        p.amplitude[i]=rng()%5?uni(0,120):pick({0.f,.005f,.01f,.0100001f,70.f,30.f});
        p.frequency[i]=rng()%3?1.f:uni(-3,3);
    }
    for(unsigned k=0;k<3;k++){p.calm[k]=rng()%2?0.f:uni(-900,900);p.gust[k]=rng()%2?0.f:uni(-400,400);}
    p.minimumStrength=rng()%3?uni(0,1):pick({0.f,.5f,.6f,1.f});
    for(auto& u:p.uvSpeed)u=rng()%3?0.f:pick({.001f,.0010001f,uni(0,.2f),uni(-.1f,.1f)});
    return p;
}
static OriginalFlagCloth randomCloth(bool grid=true){
    OriginalFlagCloth c;c.parameters=randomParameters();c.model=rng();c.count=rng()%4?1+rng()%8:0;c.mesh=rng()%8!=0;
    c.width=8;c.height=rng()%4?int(pick({2.f,5.f})):int(2+rng()%5);c.widthSpan=float(c.width-1);c.heightSpan=float(c.height-1);
    if(rng()%20==0)c.widthSpan=uni(1,10);
    if(grid){c.base.resize(c.width*c.height);for(auto& v:c.base)v={uni(-400,400),uni(-400,400),uni(-400,400)};}
    for(auto& x:c.phase)x=rng()%10?uni(0,1):pick({0.f,.99999994f,.5f,uni(.98f,1.f)});
    c.uvOffset={rng()%3?0.f:uni(0,1),rng()%3?0.f:uni(0,1)};c.parity=rng()%2;
    for(int i=0;i<c.count;i++)c.instances.push_back(rng());
    return c;
}


// ---- golden cases for web/test-flag-animation.mjs (port outputs, already
// proven equal to the originals above; floats as uint32 bit patterns) ----------
struct Json {std::string s;void raw(const std::string& x){s+=x;}void num(uint64_t v){s+=std::to_string(v);s+=',';}
    void f(float v){num(std::bit_cast<uint32_t>(v));}template<class C> void fs(const C& c){s+='[';for(float v:c)f(v);if(s.back()==',')s.pop_back();s+="],";}
    void key(const char* k){s+='"';s+=k;s+="\":";}void open(char c){s+=c;}void close(char c){if(s.back()==',')s.pop_back();s+=c;s+=',';}};
static void params(Json& j,const OriginalFlagParameters& p){
    j.key("parameters");j.open('{');j.key("attachStart");j.num(p.attachStart);j.key("attachEnd");j.num(p.attachEnd);j.key("pinFirstRow");j.num(p.pinFirstRow);j.key("drawMode");j.num(p.drawMode);
    j.key("speed");j.fs(p.speed);j.key("amplitude");j.fs(p.amplitude);j.key("frequency");j.fs(p.frequency);j.key("calm");j.fs(p.calm);j.key("gust");j.fs(p.gust);
    j.key("minimumStrength");j.f(p.minimumStrength);j.key("uvSpeed");j.fs(p.uvSpeed);j.close('}');
}
static void cloth(Json& j,const OriginalFlagCloth& c){
    params(j,c.parameters);j.key("width");j.num(c.width);j.key("height");j.num(c.height);j.key("widthSpan");j.f(c.widthSpan);j.key("heightSpan");j.f(c.heightSpan);
    std::vector<float> b;for(auto& v:c.base)b.insert(b.end(),v.begin(),v.end());j.key("base");j.fs(b);j.key("phase");j.fs(c.phase);j.key("uvOffset");j.fs(c.uvOffset);j.key("parity");j.num(c.parity);
}
static std::vector<float> flat(const std::vector<OriginalFlagVec3>& v){std::vector<float> o;for(auto& x:v)o.insert(o.end(),x.begin(),x.end());return o;}
static void writeGolden(const char* path){
    Json j;j.open('{');
    j.key("arithmetic");j.open('[');
    for(unsigned n=0;n<4000;n++){
        float a=rng()%4?uni(-1000,1000):std::ldexp(uni(-1,1),int(rng()%60)-30),b=rng()%4?uni(-1000,1000):std::ldexp(uni(-1,1),int(rng()%60)-30);
        if(n%5==0){a=float(rng()%64);b=float(1+rng()%16);}
        if(b==0)b=1;OriginalRounding r;j.open('[');j.f(a);j.f(b);j.f(terrain_original::mul(a,b));j.f(originalScalarAdd(a,b));j.f(originalScalarSubtract(a,b));j.f(originalScalarDivide(a,b));j.close(']');
    }
    j.close(']');
    j.key("vertices");j.open('[');
    for(unsigned n=0;n<600;n++){auto c=randomCloth();float wind=uni(0,1);auto v=originalFlagVertices(c,wind);
        if(c.parameters.pinFirstRow)for(size_t i=size_t(c.width)*(c.height-1);i<v.size();i++)v[i]={0,0,0};
        j.open('{');cloth(j,c);j.key("wind");j.f(wind);j.key("vertices");j.fs(flat(v));j.close('}');}
    j.close(']');
    j.key("wind");j.open('[');
    for(unsigned n=0;n<1500;n++){OriginalFlagWind w;w.base=rng()%4?uni(0,1):pick({0.f,1.f});w.delta=uni(-.45f,.45f);w.timer=rng()%3?uni(0,1):uni(.97f,1.f);w.wind=uni(0,1);
        int mode=int(rng()%5),fps=rng()%4?60:int(1+rng()%120);uint32_t word=rng();auto out=w;size_t used=0;originalFlagWindTick(out,mode,fps,[&]{used++;return word;});
        j.open('{');j.key("in");j.fs(std::array<float,4>{w.wind,w.base,w.delta,w.timer});j.key("mode");j.num(mode);j.key("fps");j.num(fps);j.key("word");j.num(word);j.key("used");j.num(used);
        j.key("out");j.fs(std::array<float,4>{out.wind,out.base,out.delta,out.timer});j.close('}');}
    j.close(']');
    j.key("tick");j.open('[');
    for(unsigned n=0;n<600;n++){auto c=randomCloth();c.count=1;c.mesh=true;float wind=uni(0,1);int32_t frame=int32_t(rng()%100000);auto out=c;std::vector<OriginalFlagVec3> v;bool updated=originalFlagTick(out,wind,frame,true,&v);
        if(updated&&c.parameters.pinFirstRow)for(size_t i=size_t(c.width)*(c.height-1);i<v.size();i++)v[i]={0,0,0};
        j.open('{');cloth(j,c);j.key("wind");j.f(wind);j.key("frame");j.num(frame);j.key("updated");j.num(updated);j.key("phaseOut");j.fs(out.phase);j.key("uvOffsetOut");j.fs(out.uvOffset);j.key("vertices");j.fs(flat(v));j.close('}');}
    j.close(']');
    j.key("build");j.open('[');
    for(unsigned n=0;n<300;n++){OriginalFlagCloth c;c.parameters=randomParameters();OriginalFlagCorners k;
        for(unsigned i=0;i<4;i++){for(auto& x:k.position[i])x=uni(-600,600);for(auto& x:k.uv[i])x=float(int(rng()%8192)-4096)/4096.f;k.colour[i]={float(rng()%2),float(rng()%32)/31.f,float(rng()%32)/31.f,float(rng()%32)/31.f};}
        std::array<uint32_t,4> words{rng(),rng(),rng(),rng()};size_t at=0;float wind=uni(0,1);auto v=originalFlagBuild(c,k,[&]{return words[at++];},wind);
        if(c.parameters.pinFirstRow)for(size_t i=size_t(c.width)*(c.height-1);i<v.size();i++)v[i]={0,0,0};
        j.open('{');params(j,c.parameters);j.key("corners");j.open('{');
        std::vector<float> pp,uu,cc;for(unsigned i=0;i<4;i++){pp.insert(pp.end(),k.position[i].begin(),k.position[i].end());uu.insert(uu.end(),k.uv[i].begin(),k.uv[i].end());cc.insert(cc.end(),k.colour[i].begin(),k.colour[i].end());}
        j.key("position");j.fs(pp);j.key("uv");j.fs(uu);j.key("colour");j.fs(cc);j.close('}');
        j.key("words");j.open('[');for(auto w:words)j.num(w);j.close(']');j.key("wind");j.f(wind);j.key("height");j.num(c.height);j.key("phase");j.fs(c.phase);
        std::vector<float> b,u2,c2;for(auto& x:c.base)b.insert(b.end(),x.begin(),x.end());for(auto& x:c.uv)u2.insert(u2.end(),x.begin(),x.end());for(auto& x:c.colour)c2.insert(c2.end(),x.begin(),x.end());
        j.key("base");j.fs(b);j.key("uv");j.fs(u2);j.key("colour");j.fs(c2);j.key("vertices");j.fs(flat(v));j.close('}');}
    j.close(']');
    j.close('}');j.s.pop_back();
    std::ofstream(path)<<j.s;printf("golden cases written to %s\n",path);
}

int main(int argc,char** argv){
    if(argc<2){puts("usage: reference SLUS_207.72");return 2;}
    std::vector<uint8_t> memory(32*1024*1024);mem=memory.data();
    {std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(f)),{});if(elf.size()<0x3b0000)return 3;std::memcpy(mem+0xFF000,elf.data(),elf.size());}
    PS2Runtime rt;runtime=&rt;rt.setMissingFunctionPolicy(PS2Runtime::MissingFunctionPolicy::Stop);
    auto reg=[&](uint32_t pc,Original f){if(!rt.registerFunction(pc,f))throw std::runtime_error("registration");};
    for(uint32_t pc:{0x2d1cb0u,0x2d1c98u,0x2d1ba0u,0x3177f0u,0x317e30u,STUB_CORNERS,STUB_CREATE,STUB_UPDATE,STUB_SET})reg(pc,stub);
    reg(0x392df0,sub_00392DF0_0x392df0);reg(0x31bf60,sub_0031BF60_0x31bf60);reg(0x34ac88,sub_0034AC88_0x34ac88);reg(0x34bca0,sub_0034BCA0_0x34bca0);
    reg(0x34b818,sub_0034B818_0x34b818);reg(0x34c668,sub_0034C600_0x34c600);reg(0x34b228,sub_0034B228_0x34b228);
    // Globals: fps object [gp+2A74]+10, renderer [gp+2A90] (+10D8 vtable), enable [gp+1150].
    put(gp+0x2A74,FPS);put(gp+0x2A90,RENDERER);put(RENDERER+0x10D8,RVT);
    put(RVT+0x308,uint32_t(0));put(RVT+0x30C,STUB_CORNERS);put(RVT+0x260,uint32_t(0));put(RVT+0x264,STUB_CREATE);
    put(MESH+4,MVT);put(MVT+0x18,uint32_t(0));put(MVT+0x1C,STUB_UPDATE);put(MVT+0x30,uint32_t(0));put(MVT+0x34,STUB_SET);
    unsigned failures=0;
    // ---- 0x392DF0 sine table ----
    {R5900Context c=context(0x392df0);SET_GPR_U32(&c,4,TABLE);run(sub_00392DF0_0x392df0,c);
     for(unsigned i=0;i<640;i++)if(word(TABLE+4*i)!=originalFlagSineTableBits[i]){printf("sine %u original %08x port %08x\n",i,word(TABLE+4*i),originalFlagSineTableBits[i]);failures++;}
     if(!failures)puts("0x392DF0 sine table: 640 words match");}
    // ---- 0x34AC88 parameters ----
    {unsigned bad=0;
     for(unsigned n=0;n<20000;n++){
        auto a=OriginalFlagArguments::defaults();
        for(unsigned k=1;k<26;k++)if(rng()%2)a.words[k]=(k<=3||k==25)?(rng()%3?rng()%2:rng()):std::bit_cast<uint32_t>(uni(-1000,1000));
        int32_t fps=rng()%3?60:pick({30.f,50.f,float(1+rng()%240)});
        put(ARGS,a.words);put(FPS+0x10,fps);std::memset(mem+OUT,0xCD,0x58);
        R5900Context c=context(0x34ac88);SET_GPR_U32(&c,4,OUT);SET_GPR_U32(&c,5,ARGS);run(sub_0034AC88_0x34ac88,c);
        auto p=originalFlagParameters(a,fps);
        if(std::memcmp(mem+OUT,&p,0x58)){if(bad++<5)printf("34AC88 mismatch case %u\n",n);}
     }
     failures+=bad;printf("0x34AC88 parameters: 20000 cases, %u mismatches\n",bad);}
    // ---- 0x34BCA0 vertices ----
    {unsigned bad=0,pinned=0;
     for(unsigned n=0;n<30000;n++){
        auto c=randomCloth();float wind=rng()%5?uni(0,1):pick({0.f,1.f,uni(-.2f,1.2f)});
        storeCloth(MGR+0x20,c);put(MGR+0x10,wind);std::memset(mem+OUT,0xCD,c.width*c.height*12);
        R5900Context x=context(0x34bca0);SET_GPR_U32(&x,4,MGR+0x20);SET_GPR_U32(&x,5,OUT);run(sub_0034BCA0_0x34bca0,x);
        auto v=originalFlagVertices(c,wind);pinned+=c.parameters.pinFirstRow;
        // The pinned-row quirk leaves trailing vertices unwritten in the original.
        size_t written=v.size();if(c.parameters.pinFirstRow){written=size_t(c.width)*(c.height-1);v.resize(written);}
        if(std::memcmp(mem+OUT,v.data(),written*12)){if(bad++<5){printf("34BCA0 mismatch case %u b=%d%d%d\n",n,c.parameters.attachStart,c.parameters.attachEnd,c.parameters.pinFirstRow);
            for(size_t i=0;i<written;i++)for(unsigned k=0;k<3;k++){float o=real(OUT+12*i+4*k);if(std::bit_cast<uint32_t>(o)!=std::bit_cast<uint32_t>(v[i][k])){printf("  v%zu.%u original %g port %g\n",i,k,o,v[i][k]);i=written;break;}}}}
     }
     failures+=bad;printf("0x34BCA0 vertices: 30000 cases (%u pinned-row), %u mismatches\n",pinned,bad);}
    // ---- 0x34B818 cloth tick ----
    {unsigned bad=0,updates=0;
     for(unsigned n=0;n<20000;n++){
        auto c=randomCloth();float wind=uni(0,1);stubFrame=int32_t(rng()%100000);bool enabled=rng()%10!=0;gridSize=c.width*c.height;
        storeCloth(MGR+0x20,c);put(MGR+0x10,wind);put(gp+0x1150,uint32_t(enabled));events.clear();writtenVertices=c.parameters.pinFirstRow?c.width*(c.height-1):gridSize;
        R5900Context x=context(0x34b818);SET_GPR_U32(&x,4,MGR+0x20);run(sub_0034B818_0x34b818,x);
        auto port=c;std::vector<OriginalFlagVec3> verts;bool updated=originalFlagTick(port,wind,stubFrame,enabled,&verts);
        std::vector<Event> expected;if(updated){Event e{STUB_UPDATE,std::vector<uint8_t>(verts.size()*12)};std::memcpy(e.data.data(),verts.data(),e.data.size());clip(e.data);expected.push_back(e);updates++;}
        if(!sameCloth(loadCloth(MGR+0x20),port)||events!=expected){if(bad++<5)printf("34B818 mismatch case %u events %zu/%zu\n",n,events.size(),expected.size());}
     }
     failures+=bad;printf("0x34B818 cloth tick: 20000 cases (%u grid updates), %u mismatches\n",updates,bad);}
    // ---- 0x34C668 manager tick ----
    {unsigned bad=0,keyframes=0;
     for(unsigned n=0;n<8000;n++){
        OriginalFlagManager m(int(rng()%2));
        m.wind.base=rng()%4?uni(0,1):pick({0.f,1.f});m.wind.delta=uni(-.45f,.45f);m.wind.timer=rng()%3?uni(0,1):uni(.97f,1.f);m.wind.wind=uni(0,1);
        for(auto& s:m.slots){int parity=s.parity;if(rng()%3==0){s=randomCloth();s.parity=parity;}}
        stubMode=int(rng()%5);stubFrame=int32_t(rng()%100000);int32_t fps=rng()%4?60:int32_t(1+rng()%120);bool enabled=rng()%10!=0;
        randomWords.clear();randomAt=0;for(unsigned k=0;k<4;k++)randomWords.push_back(rng()%8?rng():pick({0.f,1.f})==0?0u:0x7FFFFFu);
        put(MGR+0xC,uint32_t(0x48FB80));put(MGR+0x10,m.wind.wind);put(MGR+0x14,m.wind.base);put(MGR+0x18,m.wind.delta);put(MGR+0x1C,m.wind.timer);
        for(int i=0;i<originalFlagSlots;i++){gridSize=0;storeCloth(MGR+0x20+0x188*i,m.slots[i]);}
        // Each slot's grid lives at VERTS in memory; keep only one populated slot's grid meaningful.
        int live=-1;for(int i=0;i<originalFlagSlots;i++)if(m.slots[i].count&&m.slots[i].mesh){if(live<0)live=i;else{m.slots[i].mesh=false;put(MGR+0x20+0x188*i+0x60,0u);}}
        if(live>=0)storeCloth(MGR+0x20+0x188*live,m.slots[live]);
        put(FPS+0x10,fps);put(gp+0x1150,uint32_t(enabled));events.clear();gridSize=live>=0?m.slots[live].width*m.slots[live].height:0;writtenVertices=live>=0&&m.slots[live].parameters.pinFirstRow?m.slots[live].width*(m.slots[live].height-1):gridSize;
        R5900Context x=context(0x34c668);SET_GPR_U32(&x,4,MGR);run(sub_0034C600_0x34c600,x);
        size_t used=randomAt;randomAt=0;
        std::vector<Event> expected;float before=m.wind.timer;
        originalFlagWindTick(m.wind,stubMode,fps,[&]{return randomWords.at(randomAt++);});
        keyframes+=randomAt>0;
        for(auto& s:m.slots)if(s.count){std::vector<OriginalFlagVec3> v;if(originalFlagTick(s,m.wind.wind,stubFrame,enabled,&v)){Event e{STUB_UPDATE,std::vector<uint8_t>(v.size()*12)};std::memcpy(e.data.data(),v.data(),e.data.size());clip(e.data);expected.push_back(e);}}
        bool okay=used==randomAt&&events==expected;(void)before;
        for(uint32_t o:{0x10u,0x14u,0x18u,0x1Cu}){float port=o==0x10?m.wind.wind:o==0x14?m.wind.base:o==0x18?m.wind.delta:m.wind.timer;okay&=word(MGR+o)==std::bit_cast<uint32_t>(port);}
        for(int i=0;i<originalFlagSlots;i++){auto got=loadCloth(MGR+0x20+0x188*i);if(i!=live){got.base.clear();}auto want=m.slots[i];if(i!=live)want.base.clear();okay&=sameCloth(got,want);}
        if(!okay){if(bad++<5)printf("34C668 mismatch case %u rng %zu/%zu events %zu/%zu wind %g/%g\n",n,used,randomAt,events.size(),expected.size(),real(MGR+0x10),m.wind.wind);}
     }
     failures+=bad;printf("0x34C668 manager tick: 8000 cases (%u wind keyframes), %u mismatches\n",keyframes,bad);}
    // ---- 0x34B228 build ----
    {unsigned bad=0;
     for(unsigned n=0;n<10000;n++){
        OriginalFlagCloth c=randomCloth(false);c.count=0;c.base.clear();c.width=c.height=0;c.widthSpan=c.heightSpan=0;c.mesh=false;
        for(unsigned i=0;i<4;i++){for(auto& x:stubCorners.position[i])x=rng()%4?uni(-600,600):0.f;for(auto& x:stubCorners.uv[i])x=float(int(rng()%8192)-4096)/4096.f;
            stubCorners.colour[i]={float(rng()%2),float(rng()%32)/31.f,float(rng()%32)/31.f,float(rng()%32)/31.f};}
        float wind=uni(0,1);randomWords.clear();randomAt=0;for(unsigned k=0;k<4;k++)randomWords.push_back(rng());
        const int height=c.parameters.amplitude[2]<.01f&&c.parameters.amplitude[3]<.01f?2:5;gridSize=8*height;writtenVertices=writtenFor(c,height);
        storeCloth(MGR+0x20,c);put(MGR+0x10,wind);events.clear();std::memset(mem+VERTS,0xCD,gridSize*12);
        R5900Context x=context(0x34b228);SET_GPR_U32(&x,4,MGR+0x20);SET_GPR_U32(&x,5,INSTANCE);run(sub_0034B228_0x34b228,x);
        randomAt=0;auto port=c;auto v=originalFlagBuild(port,stubCorners,[&]{return randomWords.at(randomAt++);},wind);
        std::vector<Event> expected{{STUB_CREATE,{}},{0x317e30,{uint8_t(gridSize*12),uint8_t((gridSize*12)>>8)}}};
        Event set{STUB_SET,std::vector<uint8_t>(gridSize*36)};std::memcpy(set.data.data(),v.data(),gridSize*12);std::memcpy(set.data.data()+gridSize*12,port.colour.data(),gridSize*16);std::memcpy(set.data.data()+gridSize*28,port.uv.data(),gridSize*8);{std::vector<uint8_t> head(set.data.begin(),set.data.begin()+gridSize*12);clip(head);std::copy(head.begin(),head.end(),set.data.begin());}
        expected.push_back(set);
        auto got=loadCloth(MGR+0x20);
        bool okay=events==expected&&sameCloth(got,port);
        if(!okay){if(bad++<5){printf("34B228 mismatch case %u events %zu/%zu cloth %d\n",n,events.size(),expected.size(),int(sameCloth(got,port)));
            if(events.size()==expected.size())for(size_t e=0;e<events.size();e++)if(!(events[e]==expected[e]))for(size_t b=0;b<events[e].data.size();b+=4)if(std::memcmp(&events[e].data[b],&expected[e].data[b],4)){float o,p;std::memcpy(&o,&events[e].data[b],4);std::memcpy(&p,&expected[e].data[b],4);printf("  event %zu byte %zu original %g port %g\n",e,b,o,p);break;}}}
     }
     failures+=bad;printf("0x34B228 build: 10000 cases, %u mismatches\n",bad);}
    if(failures){printf("FAILED %u\n",failures);return 1;}
    if(argc>2)writeGolden(argv[2]);
    puts("flag cloth oracle: all cases match");return 0;
}
