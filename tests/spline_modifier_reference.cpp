// Instruction oracle for engine/spline_modifier.hpp (development only): the
// recompiled original SplineModifier / PositionModifier functions (PCSX2 scalar-FP
// oracle copies, linked by tools/original_live_build.py) run on the race tick-4319
// savestate image (local/ps2-capture/runs/setpieces), whose world resource table and
// runtime kind-8 spline records are the real ones, against the port. Every case
// compares all 0xF0 (0x90) bytes of the object; the constructor also compares the
// shared RNG words (0x4FF030). Nothing is stubbed: 0x3451C0/0x3454E8 bind the real
// records, 0x345248/0x345048 evaluate them, 0x31C228/0x31BE50/0x317830 run as-is.
#include "live_registrations.inc"
#include "original_live_runtime.hpp"
#include "../engine/spline_modifier.hpp"
#include <map>
#include <random>
using namespace ssx;
static constexpr uint32_t arena=0xE0000,MOD=arena,PKT=arena+0x200,ARGS=arena+0x300,BIND=arena+0x400,POS=arena+0x500,MAT=arena+0x600;
static uint32_t bits(float x){return std::bit_cast<uint32_t>(x);}
struct Path {uint32_t resource,head;OriginalRailRecord record;};
// Runtime record from the original bind (0x3451C0 on scratch): cursor 0 = first segment.
static Path bindPath(live::Machine& m,uint32_t resource){
    m.call(0x3451c0,{BIND,resource});
    Path p;p.resource=resource;p.head=m.get<uint32_t>(BIND+8);p.record.packedId=resource;int32_t index=0;
    for(uint32_t seg=p.head;seg;seg=m.get<uint32_t>(seg+0x64)){
        if(seg!=p.head+0x90u*uint32_t(index))throw std::runtime_error("segments not contiguous");
        OriginalRailSegment s;s.index=index++;s.length=m.get<float>(seg+0xC);s.distance=m.get<float>(seg+0x84);
        for(unsigned row=0;row<4;row++)for(unsigned k=0;k<3;k++)s.coefficients[row][k]=m.get<float>(seg+0x10+16*row+4*k);
        for(unsigned k=0;k<4;k++)s.arcToParameter[k]=m.get<float>(seg+0x50+4*k);
        p.record.segments.push_back(s);
    }
    if(bits(originalSplineLength(p.record))!=bits(m.get<float>(BIND+0xC)))throw std::runtime_error("0x3454E8 length");
    return p;
}
static unsigned diff(const uint8_t* a,const uint8_t* b,size_t n,FILE* log,const char* what,unsigned limit){
    unsigned bad=0;
    for(size_t o=0;o<n;o+=4)if(std::memcmp(a+o,b+o,4)){
        if(bad<limit){uint32_t x,y;std::memcpy(&x,a+o,4);std::memcpy(&y,b+o,4);fprintf(log,"    %s +%03zx original %08x (%.9g) port %08x (%.9g)\n",what,o,x,std::bit_cast<float>(x),y,std::bit_cast<float>(y));}
        bad++;
    }
    return bad;
}
int main(int argc,char** argv){
    if(argc<2){fprintf(stderr,"usage: reference FOLDER [CASES]\n");return 2;}
    const std::string folder=argv[1];const unsigned cases=argc>2?unsigned(atoi(argv[2])):20000;FILE* log=stdout;
    live::Machine R(folder+"/4319.ee",folder+"/4319.vuc",folder+"/4319.vud");
    const float dt=R.get<float>(R.get<uint32_t>(0x4A30F0-0x848)+0x14);
    if(bits(dt)!=0x3c888889u)throw std::runtime_error("clock step");
    // Paths: every Snow Jam builtin19 spline plus the two gondola rails.
    std::vector<Path> paths;
    for(uint32_t id:{0x5408u,0xa908u,0xaa08u,0xa208u,0xa108u,0xa608u,0xa508u,0xa408u,0xa308u,0xa708u,0xa808u,0x608u,0x708u})paths.push_back(bindPath(R,id));
    for(auto& p:paths)fprintf(log,"path %05x: %zu segments at %08x, length %.4f\n",p.resource,p.record.segments.size(),p.head,originalSplineLength(p.record));
    {   // runtime segment words for the catalog check (tools/test_spline_modifier_native.py vs rails.json)
        std::ofstream out(folder+"/runtime-paths.bin",std::ios::binary);
        auto w=[&](uint32_t x){out.write(reinterpret_cast<const char*>(&x),4);};
        for(auto& p:paths){w(p.resource);w(uint32_t(p.record.segments.size()));
            for(uint32_t k=0;k<p.record.segments.size();k++){const uint32_t seg=p.head+0x90*k;w(R.get<uint32_t>(seg+0xC));
                for(unsigned row=0;row<4;row++)for(unsigned c=0;c<3;c++)w(R.get<uint32_t>(seg+0x10+16*row+4*c));
                for(unsigned c=0;c<4;c++)w(R.get<uint32_t>(seg+0x50+4*c));w(R.get<uint32_t>(seg+0x84));}}
    }
    std::mt19937 rng(0x359830);auto uni=[&](float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);};
    auto word=[&]{return uint32_t(rng());};
    // Random modifier state bound to path p (cursor anywhere on the record).
    auto randomModifier=[&](const Path& p){
        OriginalSplineModifier m;const float L=originalSplineLength(p.record);
        for(auto& w:m.word04)w=word();m.word5C=word();for(auto& w:m.wordE8)w=word();
        for(unsigned k=0;k<4;k++){m.boundsMin[k]=uni(-3e5,3e5);m.boundsMax[k]=uni(-3e5,3e5);}
        m.endMode=rng()%8==0?int32_t(word()):int32_t(rng()%6);m.orientation=rng()%6==0?int32_t(word()):int32_t(rng()%5);
        m.roll=rng()%3==0?0.f:rng()%3==0?-1.57079637f:uni(-7,7);
        switch(rng()%6){case 0:m.distance=uni(-0.2f*L,0);break;case 1:m.distance=uni(L-60,L+60);break;case 2:m.distance=uni(-60,60);break;case 3:m.distance=uni(-2*L,3*L);break;default:m.distance=uni(0,L);}
        m.acceleration=rng()%3?0.f:uni(-2000,2000);m.accelerationTime=rng()%3?0.f:rng()%2?uni(-1,3):std::bit_cast<float>(0x3c888889u);
        m.speed=rng()%10==0?0.f:rng()%2?uni(-5000,5000):(rng()%2?1250.f:-4166.66699f);m.radius=uni(0,500);
        m.finished=rng()%4==0?(rng()%3?1u:word()):0u;m.running=rng()%5==0?0u:(rng()%4?1u:word());m.dirty=rng()%2?1u:(rng()%3?0u:word());
        for(auto& r:m.matrix)for(auto& x:r)x=uni(-3e5,3e5);
        for(auto* q:{&m.position,&m.tangent,&m.curvature})for(auto& x:*q)x=uni(-3e5,3e5);
        m.tangentXX=uni(0,1e6);m.tangentYY=uni(0,1e6);
        m.path.resource=p.resource;m.path.record=&p.record;m.path.length=L;m.path.cursor=rng()%p.record.segments.size();m.path.index=int32_t(m.path.cursor);
        return m;
    };
    auto store=[&](const OriginalSplineModifier& m,const Path& p){auto b=originalSplineModifierBytes(m,p.head);std::memcpy(R.ee.data()+MOD,b.data(),b.size());};
    auto compare=[&](const OriginalSplineModifier& m,const Path& p,const char* what,unsigned& bad){
        auto b=originalSplineModifierBytes(m,p.head);unsigned d=diff(R.ee.data()+MOD,b.data(),b.size(),log,what,bad<4?8:0);
        if(d){if(bad<4)fprintf(log,"  %s mismatch (%u words)\n",what,d);bad++;return false;}return true;
    };
    unsigned failures=0;
    // ---- 0x359698 update -----------------------------------------------------------
    {
        unsigned ok=0,bad=0,idle=0,stopped=0,active=0;
        for(unsigned n=0;n<2*cases;n++){
            const Path& p=paths[rng()%paths.size()];auto m=randomModifier(p);store(m,p);
            R.call(0x359698,{MOD});
            const uint32_t fin=m.finished,run=m.running;originalSplineUpdate(m,dt);
            if(compare(m,p,"359698",bad))ok++;
            if(!run||fin)idle++;else if(m.finished)stopped++;else active++;
        }
        fprintf(log,"0x359698 update: %u/%u byte-identical (idle %u, stopped at an end %u, advanced/wrapped/reflected %u)\n",ok,2*cases,idle,stopped,active);
        failures+=bad;
    }
    // ---- 0x359830 evaluate, 0x361B90 matrix getter -----------------------------------
    {
        unsigned ok=0,bad=0,getterOk=0;unsigned modes[6]={};
        for(unsigned n=0;n<2*cases;n++){
            const Path& p=paths[n%paths.size()];auto m=randomModifier(p);
            if(n%4==0){m.orientation=3;m.roll=n%8?0.f:-1.57079637f;}
            store(m,p);
            if(n%2){R.call(0x359830,{MOD});originalSplineEvaluate(m);}
            else{auto c=R.call(0x361b90,{MOD});originalSplineMatrix(m);if(GPR_U32((&c),2)==MOD+0x60)getterOk++;else fprintf(log,"  361B90 returned %x\n",GPR_U32((&c),2));}
            if(compare(m,p,n%2?"359830":"361B90",bad))ok++;
            modes[std::min<uint32_t>(uint32_t(m.orientation),5)]++;
        }
        fprintf(log,"0x359830 evaluate / 0x361B90 getter: %u/%u byte-identical (getter returned +0x60 %u/%u; orientation modes 0..4 %u %u %u %u %u, other %u)\n",
            ok,2*cases,getterOk,cases,modes[0],modes[1],modes[2],modes[3],modes[4],modes[5]);
        failures+=bad;
    }
    // ---- 0x359CF8 contact velocity (after a real evaluation) ---------------------------
    {
        unsigned ok=0,rate=0,applied=0;
        for(unsigned n=0;n<cases;n++){
            const Path& p=paths[n%paths.size()];auto m=randomModifier(p);m.dirty=1;originalSplineEvaluate(m);
            if(n%9==0){m.tangent[0]=uni(-2e-5f,2e-5f);}
            store(m,p);
            SplineQuad point{uni(-500,500),uni(-500,500),uni(-500,500),rng()%4?1.f:uni(-2,2)};for(unsigned k=0;k<3;k++)point[k]=terrain_original::add(m.position[k],point[k]);
            SplineQuad v0{uni(-900,900),uni(-900,900),uni(-900,900),uni(-1,1)},w0{uni(-2,2),uni(-2,2),uni(-2,2),uni(-1,1)};
            R.put(PKT,point);R.put(PKT+0x20,v0);R.put(PKT+0x30,w0);
            R.call(0x359cf8,{MOD,PKT});
            auto v=originalSplineContactVelocity(m,point);originalSplineApplyContactVelocity(v,v0,w0);
            bool same=true;for(unsigned k=0;k<4;k++)same&=bits(R.get<float>(PKT+0x20+4*k))==bits(v0[k])&&bits(R.get<float>(PKT+0x30+4*k))==bits(w0[k]);
            unsigned dummy=0;same&=compare(m,p,"359CF8 (object)",dummy);
            if(same)ok++;else if(n-ok<5)fprintf(log,"  359CF8 case %u: v %.9g/%.9g w %.9g/%.9g\n",n,R.get<float>(PKT+0x20),v0[0],R.get<float>(PKT+0x30),w0[0]);
            applied+=v.applied;rate+=v.applied&&std::fabs(m.tangent[0])>spline_modifier_constants::kVelocityEpsilon;
        }
        fprintf(log,"0x359CF8 contact velocity: %u/%u identical packets (%u applied, %u with the rate branch)\n",ok,cases,applied,rate);
        failures+=cases-ok;
    }
    // ---- 0x359EB8 messages -----------------------------------------------------------
    {
        unsigned ok=0,bad=0;
        for(unsigned n=0;n<cases/4;n++){
            const Path& p=paths[n%paths.size()];auto m=randomModifier(p);store(m,p);
            const uint32_t id=0x18F+rng()%5;const float value=uni(-300,300);
            R.call(0x359eb8,{MOD,id},{value});originalSplineMessage(m,id,value);
            if(compare(m,p,"359EB8",bad))ok++;
        }
        fprintf(log,"0x359EB8 messages 0x18F..0x193: %u/%u byte-identical\n",ok,cases/4);failures+=bad;
    }
    // ---- 0x359460 constructor (real 0x3451C0 bind, 0x317830 shared RNG) --------------
    {
        unsigned ok=0,bad=0,rngOk=0,clamped=0,reversed=0;
        for(unsigned n=0;n<cases;n++){
            const Path& p=paths[n%paths.size()];
            std::array<uint8_t,0xF0> stale;for(auto& x:stale)x=n%10==0?0:uint8_t(rng());std::memcpy(R.ee.data()+MOD,stale.data(),0xF0);
            OriginalSplineScriptArgs a;a.spline=p.resource;a.instance=int32_t(word());
            a.endMode=int32_t(rng()%5);a.orientation=int32_t(rng()%4);
            a.speedKmh=rng()%4==0?-uni(0,200):rng()%10==0?0.f:rng()%10==0?-0.f:uni(0,200);
            a.rollDegrees=rng()%2?0.f:uni(-360,360);a.startDistance=rng()%3?0.f:uni(-500,originalSplineLength(p.record)+500);
            a.startJitter=rng()%2?0.f:uni(0,3000);a.running=rng()%3?1:int32_t(rng()%3);
            a.acceleration=rng()%3?0.f:uni(-50,50);a.accelerationTime=rng()%3?0.f:uni(0,5);
            R.put(ARGS,a.instance);R.put(ARGS+4,a.spline);R.put(ARGS+8,a.endMode);R.put(ARGS+0xC,a.orientation);R.put(ARGS+0x10,a.speedKmh);
            R.put(ARGS+0x14,a.rollDegrees);R.put(ARGS+0x18,a.startDistance);R.put(ARGS+0x1C,a.startJitter);R.put(ARGS+0x20,a.running);
            R.put(ARGS+0x24,a.acceleration);R.put(ARGS+0x28,a.accelerationTime);
            OriginalRandomState random;for(auto& w:random.words)w=word();for(unsigned k=0;k<6;k++)R.put(0x4FF030+4*k,random.words[k]);
            auto c=R.call(0x359460,{MOD,ARGS});
            auto m=originalSplineModifierFromBytes(stale.data(),nullptr,0);
            originalSplineConstruct(m,a,p.record,random);
            if(GPR_U32((&c),2)!=MOD)throw std::runtime_error("359460 return");
            if(compare(m,p,"359460",bad))ok++;
            bool rngSame=true;for(unsigned k=0;k<6;k++)rngSame&=R.get<uint32_t>(0x4FF030+4*k)==random.words[k];rngOk+=rngSame;
            clamped+=m.distance==0.f&&(a.startDistance!=0.f||a.startJitter!=0.f);reversed+=!(0.f<=a.speedKmh);
        }
        fprintf(log,"0x359460 constructor: %u/%u byte-identical, shared RNG %u/%u identical (one 0x317830 draw each; %u clamped starts, %u reverse starts)\n",ok,cases,rngOk,cases,clamped,reversed);
        failures+=bad+(cases-rngOk);
    }
    // ---- PositionModifier 0x356F10 / 0x356FF0 / 0x361940 ---------------------------------
    {
        unsigned ok=0;
        for(unsigned n=0;n<cases/4;n++){
            std::array<uint8_t,0x90> stale;for(auto& x:stale)x=uint8_t(rng());std::memcpy(R.ee.data()+POS,stale.data(),0x90);
            RollerMatrix M;for(auto& r:M)for(auto& x:r)x=uni(-3e5,3e5);R.put(MAT,M);
            OriginalPositionModifier p;std::memcpy(p.word04.data(),stale.data()+4,12);std::memcpy(&p.boundsMin,stale.data()+0x10,16);std::memcpy(&p.boundsMax,stale.data()+0x20,16);
            std::memcpy(&p.radius,stale.data()+0x40,4);std::memcpy(p.word48.data(),stale.data()+0x48,8);
            R.call(0x356f10,{POS,MAT});originalPositionFromMatrix(p,M);
            bool same=!diff(R.ee.data()+POS,originalPositionModifierBytes(p).data(),0x90,log,"356F10",2);
            if(n%2){R.put(POS+0x30,SplineQuad{uni(-3e5,3e5),uni(-3e5,3e5),uni(-3e5,3e5),uni(-2,2)});p.translation=R.get<SplineQuad>(POS+0x30);}
            auto c=R.call(0x361940,{POS});originalPositionMatrix(p);
            same&=GPR_U32((&c),2)==POS+0x50&&!diff(R.ee.data()+POS,originalPositionModifierBytes(p).data(),0x90,log,"361940",2);
            ok+=same;
        }
        fprintf(log,"PositionModifier 0x356F10 + 0x361940 (-> 0x356FF0): %u/%u byte-identical\n",ok,cases/4);failures+=cases/4-ok;
    }
    fprintf(log,"original entries called:");for(auto& [pc,n]:callCounts)if(n)fprintf(log," %x:%llu",pc,(unsigned long long)n);fprintf(log,"\n");
    fprintf(log,failures?"FAILED (%u)\n":"all randomized original/port cases match\n",failures);
    return failures?1:0;
}
