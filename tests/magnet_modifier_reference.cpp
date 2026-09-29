// Instruction oracle for engine/magnet_modifier.hpp (development only): the
// recompiled original MagnetModifier functions (PCSX2 scalar-FP oracle copies,
// linked by tools/original_live_build.py) run on a The Junction (BHP1) pipe-run
// savestate image, whose player manager, human rider and 14 live pickup magnets are
// real, against the port. Every case compares all 0xB0 bytes of the modifier.
//   0x3572E0 ctor, 0x3573F8 update (real 0x2D1C70 clock / 0x2D1B58 rider position),
//   0x357528 / 0x361A60 evaluate + matrix getter, 0x357660 gate (real 0x2D1B08 and
//   rider interface vt+0x44 0x140BC0), 0x3568B0 entity bounds on a live pickup entity
//   (with the real 0x3291E0 relocation), and builtin90 0x305478 end to end (keyed
//   argument decode, 0x3559F8 factory, ctor, 0x3554B0 attach) on a live pickup instance.
#include "live_registrations.inc"
#include "original_live_runtime.hpp"
#include "../engine/magnet_modifier.hpp"
#include <random>
using namespace ssx;
static constexpr uint32_t GP=0x4A30F0,arena=0xE0000,MOD=arena,MAT=arena+0x100,ARGS=arena+0x200,RET=arena+0x400;
static uint32_t bits(float x){return std::bit_cast<uint32_t>(x);}
static unsigned diff(const uint8_t* a,const uint8_t* b,size_t n,FILE* log,const char* what,unsigned limit){
    unsigned bad=0;
    for(size_t o=0;o<n;o+=4)if(std::memcmp(a+o,b+o,4)){
        if(bad<limit){uint32_t x,y;std::memcpy(&x,a+o,4);std::memcpy(&y,b+o,4);fprintf(log,"    %s +%03zx original %08x (%.9g) port %08x (%.9g)\n",what,o,x,std::bit_cast<float>(x),y,std::bit_cast<float>(y));}
        bad++;
    }
    return bad;
}
struct Live {uint32_t modifier,entity,instance;};
int main(int argc,char** argv){
    if(argc<2){fprintf(stderr,"usage: reference FOLDER [CASES]\n");return 2;}
    const std::string folder=argv[1];const unsigned cases=argc>2?unsigned(atoi(argv[2])):20000;FILE* log=stdout;
    live::Machine R(folder+"/state.ee",folder+"/state.vuc",folder+"/state.vud");
    const std::vector<uint8_t> pristine=R.ee;
    const uint32_t pm=R.get<uint32_t>(GP-0x848),clock=pm+0x14;
    if(bits(R.get<float>(clock))!=0x3c888889u)throw std::runtime_error("clock step");
    const uint32_t list=R.get<uint32_t>(R.get<uint32_t>(pm+0x84)+0xC),rider=R.get<uint32_t>(list+0x28);
    if(!rider||R.get<uint32_t>(rider+0x86C)!=0||R.get<uint32_t>(rider+0x874)==0)throw std::runtime_error("rider 0 is not the human");
    const uint32_t riderHuman=R.get<uint32_t>(rider+0x874);
    // Live magnets: vtable 0x48F420 at an entity's container +0.
    std::vector<Live> magnets;
    for(uint32_t a=0x100000;a<0x2000000;a+=16)if(R.get<uint32_t>(a)==0x48F420u){
        uint32_t inst=R.get<uint32_t>(a+0xA0),e=inst?R.get<uint32_t>(inst+0xC):0,c=e?R.get<uint32_t>(e+0x1C):0;
        if(c&&R.get<uint32_t>(c)==a)magnets.push_back({a,e,inst});
    }
    fprintf(log,"state: rider 0 at %x (+0x874 %u), %zu live magnets\n",rider,riderHuman,magnets.size());
    if(magnets.empty())throw std::runtime_error("no magnets");
    std::mt19937 rng(0x3573F8);auto uni=[&](float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);};
    auto word=[&]{return uint32_t(rng());};
    auto randomQuad=[&](float span,float w){return MagnetQuad{uni(-span,span),uni(-span,span),uni(-span,span),w};};
    // Random magnet state around a centre.
    auto randomMagnet=[&](const MagnetQuad& centre,float span){
        OriginalMagnetModifier m;
        for(auto& w:m.word04)w=word();for(auto& w:m.word58)w=word();for(auto& w:m.wordA4)w=word();
        m.position={terrain_original::add(centre[0],uni(-span,span)),terrain_original::add(centre[1],uni(-span,span)),terrain_original::add(centre[2],uni(-span,span)),rng()%4?1.f:uni(-2,2)};
        m.boundsMin=randomQuad(3e5,uni(-2,2));m.boundsMax=randomQuad(3e5,uni(-2,2));
        m.radius=rng()%3?400.f:uni(0,2000);
        m.target=rng()%3==0?-1:rng()%9==0?int32_t(-1-(rng()%5)):0;
        m.speed=rng()%3?277.777771f:rng()%5==0?0.f:uni(0,20000);
        m.time=rng()%4==0?0.f:uni(0,rng()%2?3.f:40.f);
        m.dirty=rng()%2?1u:(rng()%3?0u:word());m.reached=rng()%3?0u:(rng()%2?1u:word());
        for(auto& r:m.matrix)r=randomQuad(3e5,uni(-2,2));
        m.instance=word();
        return m;
    };
    auto store=[&](uint32_t at,const OriginalMagnetModifier& m){auto b=originalMagnetModifierBytes(m);std::memcpy(R.ee.data()+at,b.data(),b.size());};
    auto compare=[&](uint32_t at,const OriginalMagnetModifier& m,const char* what,unsigned& bad){
        auto b=originalMagnetModifierBytes(m);unsigned d=diff(R.ee.data()+at,b.data(),b.size(),log,what,bad<4?8:0);
        if(d){if(bad<4)fprintf(log,"  %s mismatch (%u words)\n",what,d);bad++;return false;}return true;
    };
    unsigned failures=0;
    // ---- 0x3572E0 constructor ----------------------------------------------------
    {
        unsigned ok=0,bad=0;
        for(unsigned n=0;n<cases;n++){
            std::array<uint8_t,0xB0> stale;for(auto& x:stale)x=uint8_t(rng());std::memcpy(R.ee.data()+MOD,stale.data(),0xB0);
            RollerMatrix M;for(auto& r:M)r=randomQuad(n%2?3e5f:1e3f,rng()%3?(&r==&M[3]?1.f:0.f):uni(-2,2));R.put(MAT,M);
            const float radius=rng()%2?400.f:uni(-100,5000);
            const float kmh=rng()%3==0?10.f:rng()%3==0?30.f:rng()%8==0?-uni(0,500):uni(0,500);
            const uint32_t instance=word();
            auto c=R.call(0x3572e0,{MOD,instance,MAT},{radius,kmh});
            auto m=originalMagnetModifierFromBytes(stale.data());originalMagnetConstruct(m,M,radius,kmh,instance);
            if(GPR_U32((&c),2)!=MOD)throw std::runtime_error("3572E0 return");
            if(compare(MOD,m,"3572E0",bad))ok++;
        }
        fprintf(log,"0x3572E0 constructor: %u/%u byte-identical\n",ok,cases);failures+=bad;
    }
    // ---- 0x3573F8 update ---------------------------------------------------------
    {
        unsigned ok=0,bad=0,idle=0,near=0,overshoot=0,moved=0;
        const MagnetQuad savedRider=R.get<MagnetQuad>(rider+0x110);
        for(unsigned n=0;n<5*cases;n++){
            const MagnetQuad centre=R.get<MagnetQuad>(magnets[n%magnets.size()].modifier+0x10);
            auto m=randomMagnet(centre,n%3?500.f:3e4f);store(MOD,m);
            // Rider position: near (< 50), around the pickup, or far; w usually 1.
            MagnetQuad p=m.position;const float r=n%5==0?uni(0,60):n%5==1?uni(0,500):n%5==2?uni(0,5000):uni(0,1e5);
            for(unsigned k=0;k<3;k++)p[k]=terrain_original::add(p[k],uni(-r,r));
            p[3]=rng()%5?1.f:uni(-3,3);
            if(n%97==0)p=m.position;
            R.put(rider+0x110,p);
            const float dt=n%4?std::bit_cast<float>(0x3c888889u):uni(0,0.2f);R.put(clock,dt);
            const uint32_t reachedBefore=m.reached;const MagnetQuad before=m.position;
            R.call(0x3573f8,{MOD});
            originalMagnetUpdate(m,p,dt);
            if(compare(MOD,m,"3573F8",bad))ok++;
            if(m.target<0)idle++;else if(m.position==before&&m.reached)near++;else if(m.reached&&!reachedBefore)overshoot++;else moved++;
        }
        R.put(rider+0x110,savedRider);R.put(clock,std::bit_cast<float>(0x3c888889u));
        fprintf(log,"0x3573F8 update: %u/%u byte-identical (no target %u, within 50 cm %u, overshoot snapped %u, moved %u)\n",ok,5*cases,idle,near,overshoot,moved);
        failures+=bad;
    }
    // ---- 0x357528 evaluate / 0x361A60 matrix getter -------------------------------------
    {
        unsigned ok=0,bad=0,getterOk=0;
        for(unsigned n=0;n<2*cases;n++){
            auto m=randomMagnet({0,0,0,1},3e5);store(MOD,m);
            if(n%2){R.call(0x357528,{MOD});originalMagnetEvaluate(m);}
            else{auto c=R.call(0x361a60,{MOD});originalMagnetMatrix(m);getterOk+=GPR_U32((&c),2)==MOD+0x60;}
            if(compare(MOD,m,n%2?"357528":"361A60",bad))ok++;
        }
        fprintf(log,"0x357528 evaluate / 0x361A60 getter: %u/%u byte-identical (getter returned +0x60 %u/%u)\n",ok,2*cases,getterOk,cases);
        failures+=bad+(cases-getterOk);
    }
    // ---- 0x357660 gate ---------------------------------------------------------------
    {
        unsigned ok=0,bad=0,acquired=0,returnedReached=0;
        for(unsigned n=0;n<2*cases;n++){
            auto m=randomMagnet({0,0,0,1},3e5);
            m.target=n%3==0?-1:n%3==1?0:int32_t(rng()%8)-2;
            store(MOD,m);
            // Only rider 0 exists in the pipe event: an unacquired magnet is asked by rider 0.
            const int32_t index=m.target<0?0:(rng()%2?m.target:int32_t(rng()%8));
            const uint32_t flag=rng()%3==0?0u:rng()%2?1u:word();R.put(rider+0x874,flag);
            auto c=R.call(0x357660,{MOD,uint32_t(index)});
            const uint32_t expect=originalMagnetGate(m,index,flag!=0);
            bool same=GPR_U32((&c),2)==expect;same&=compare(MOD,m,"357660",bad);
            if(same)ok++;else if(n-ok<5)fprintf(log,"  357660 case %u returned %u, port %u\n",n,GPR_U32((&c),2),expect);
            acquired+=m.target==index&&index==0&&flag;returnedReached+=expect!=0;
        }
        R.put(rider+0x874,riderHuman);
        fprintf(log,"0x357660 gate: %u/%u identical returns and bytes (%u targets held by rider 0, %u nonzero returns)\n",ok,2*cases,acquired,returnedReached);
        failures+=2*cases-ok;
    }
    // ---- 0x3568B0 entity bounds on the live pickup entities ------------------------------
    {
        unsigned ok=0,bad=0;
        for(unsigned n=0;n<cases;n++){
            if(n%500==0)R.ee=pristine;
            const Live& L=magnets[n%magnets.size()];
            const MagnetQuad centre=R.get<MagnetQuad>(L.instance+0x40);
            auto m=randomMagnet(centre,n%4?800.f:6000.f);m.instance=L.instance;
            m.radius=rng()%3?400.f:uni(0,3000);
            if(n%2==0)m.matrix[3]={centre[0],centre[1],centre[2],1.f};
            for(auto& w:m.matrix[3])if(rng()%4==0)w=terrain_original::add(w,uni(-300,300));
            if(rng()%3==0)m.position[3]=1.f;
            store(L.modifier,m);
            R.call(0x3568b0,{L.entity});
            originalMagnetEntityBounds(m);
            if(compare(L.modifier,m,"3568B0",bad))ok++;
        }
        R.ee=pristine;
        fprintf(log,"0x3568B0 entity bounds (live entities, real 0x3291E0): %u/%u byte-identical\n",ok,cases);failures+=bad;
    }
    // ---- builtin90 0x305478 end to end -------------------------------------------------
    {
        unsigned ok=0,bad=0,decodeOk=0;const uint32_t context=R.get<uint32_t>(GP+0xCE8);
        const unsigned total=cases/4;
        for(unsigned n=0;n<total;n++){
            if(n%200==0)R.ee=pristine;
            const Live& L=magnets[n%magnets.size()];
            const uint32_t oldModifier=R.get<uint32_t>(R.get<uint32_t>(L.entity+0x1C));
            // Perturb the old magnet (its matrix feeds the new one through 0x356078 -> 0x361A60),
            // the instance AABB (attach bounds) and the instance flags.
            auto old=originalMagnetModifierFromBytes(R.ee.data()+oldModifier);
            const MagnetQuad centre=R.get<MagnetQuad>(L.instance+0x40);
            old.position={terrain_original::add(centre[0],uni(-300,300)),terrain_original::add(centre[1],uni(-300,300)),terrain_original::add(centre[2],uni(-300,300)),1.f};
            old.dirty=rng()%2;if(rng()%2)old.matrix[0]=randomQuad(1.5f,0.f);
            store(oldModifier,old);
            std::array<float,3> low,high;
            for(unsigned k=0;k<3;k++){low[k]=terrain_original::sub(centre[k],uni(0,200));high[k]=terrain_original::add(centre[k],uni(0,200));
                R.put(L.instance+0x60+4*k,low[k]);R.put(L.instance+0x6C+4*k,high[k]);}
            uint32_t flags=(R.get<uint32_t>(L.instance+8)&~0x60u)|(word()&0x60u);R.put(L.instance+8,flags);
            // Keyed arguments: {key, value, pad, type}; type 1 int, 2 float, others raw.
            std::vector<OriginalScriptKeyedArg> args;
            if(rng()%4)args.push_back({1,rng()%2?uint32_t(rng()%3000):bits(uni(0,3000)),rng()%5?(rng()%2?1u:2u):uint32_t(rng()%4)});
            if(rng()%4)args.push_back({2,rng()%2?uint32_t(rng()%200):bits(uni(0,200)),rng()%5?(rng()%2?1u:2u):uint32_t(rng()%4)});
            if(rng()%4==0)args.push_back({0,0xFFFFFFFFu,1});
            if(rng()%3==0)std::swap(args.front(),args.back());
            for(size_t k=0;k<args.size();k++){R.put(ARGS+16*k,args[k].key);R.put(ARGS+16*k+4,args[k].value);R.put(ARGS+16*k+8,0u);R.put(ARGS+16*k+12,args[k].type);}
            R.put(context+0x290,L.instance);
            R.call(0x305478,{RET,uint32_t(args.size()),ARGS});
            const uint32_t fresh=R.get<uint32_t>(R.get<uint32_t>(L.entity+0x1C));
            if(fresh==oldModifier&&R.get<uint32_t>(fresh)!=0x48F420u)throw std::runtime_error("builtin90 did not attach");
            // Port: decode, entity matrix = the old magnet's (lazily committed) matrix, construct + attach.
            const auto a=originalMagnetDecodeArgs(args);
            const RollerMatrix M=originalMagnetMatrix(old);
            uint32_t portFlags=flags;auto m=originalMagnetCreate(a,M,L.instance,low,high,portFlags);
            // The fresh allocation's never-written words are heap contents: copy them over.
            auto got=originalMagnetModifierFromBytes(R.ee.data()+fresh);m.word04=got.word04;m.word58=got.word58;m.wordA4=got.wordA4;
            {OriginalRounding rounding;decodeOk+=bits(got.radius)==bits(a.radius)&&bits(got.speed)==bits(terrain_original::mul(a.speedKmh,magnet_modifier_constants::kKmhToCms));}
            bool same=compare(fresh,m,"305478",bad);
            if(R.get<uint32_t>(L.instance+8)!=portFlags){same=false;if(n-ok<5)fprintf(log,"  305478 flags %x port %x\n",R.get<uint32_t>(L.instance+8),portFlags);}
            if(same)ok++;
            // Keep the pickup usable for the next case (the old magnet was freed).
        }
        R.ee=pristine;
        fprintf(log,"builtin90 0x305478 (decode + 0x3559F8 + 0x3572E0 + 0x3554B0 attach): %u/%u byte-identical incl. instance flags (radius/speed decode %u/%u)\n",ok,total,decodeOk,total);
        failures+=total-ok;
    }
    fprintf(log,"original entries called:");for(auto& [pc,n]:callCounts)if(n)fprintf(log," %x:%llu",pc,(unsigned long long)n);fprintf(log,"\n");
    fprintf(log,failures?"FAILED (%u)\n":"all randomized original/port cases match\n",failures);
    return failures?1:0;
}
