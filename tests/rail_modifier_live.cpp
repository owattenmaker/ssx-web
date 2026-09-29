// Live AnimTeeter / RailModifier parity (development oracle) on the race-capture savestates
// (local/ps2-capture/runs/setpieces/race.tick*.p2s), all originals linked by
// tools/original_live_build.py, the port fed only from web/generated/rail_teeter_seed.hpp:
//  1. Construction: on the savestate before a teeter's section activation, the ORIGINAL
//     builtin6 (0x2FB498, argument entries as the LUN interpreter builds them from programs
//     65..68: key, value, type) and builtin48's factory 0x355E38 (block {-1, rail, node})
//     run for the instance as the current script instance; the resulting AnimTeeter / RailModifier objects are compared
//     with originalAnimTeeterConstruct / originalRailModifierConstruct and with the objects
//     the game itself built (next savestate).
//  2. Lockstep: on the savestate holding the live objects, every tick random rider forces
//     (0x342538), the full original entity update 0x356198 (teeter 0x342358, rail list
//     0x352D20 -> 0x35C4E0 incl. the real spatial relocation) and random 0x35C698 rail queries
//     against the port; node matrices through 0x3610E0.
#include "live_registrations.inc"
#include "original_live_runtime.hpp"
#include "../engine/rail_modifier.hpp"
#include "../web/generated/rail_teeter_seed.hpp"
#include <random>
using namespace ssx;
static constexpr uint32_t arena=0xE0000,ARGS=arena,RESULT=arena+0x100,CONTACT=arena+0x200,FORCE=arena+0x300,POINT=arena+0x400,OUT=arena+0x500,
    FOUND=arena+0x600,BEST=arena+0x604,gp=0x4a30f0;
static std::mt19937 rng(0x342538);
static float uni(float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);}
static float F(uint32_t b){return std::bit_cast<float>(b);}
static uint32_t B(float f){return std::bit_cast<uint32_t>(f);}
static RollerMatrix matrixAt(const live::Machine& m,uint32_t a){RollerMatrix r;for(unsigned i=0;i<4;i++)r[i]=m.get<RollerQuad>(a+16*i);return r;}
static RollerMatrix fromBits(const std::array<uint32_t,16>& w){RollerMatrix r;for(unsigned i=0;i<16;i++)r[i/4][i%4]=F(w[i]);return r;}
struct Seed {const BrowserRailTeeterSeed* seed;OriginalAnimModel model;OriginalAnimTeeterArgs args;RollerMatrix matrix;float scale;std::array<float,3> low,high;};
static Seed seedFor(const char* name){
    for(const auto& s:browserRailTeeterSeeds)if(!strcmp(s.name,name)){
        Seed out;out.seed=&s;out.model.length=F(s.length);
        for(uint32_t i=0;i<s.nodeCount;i++){
            const auto& n=browserRailTeeterNodes[s.firstNode+i];OriginalAnimNode node;node.parent=n.parent;node.bind=fromBits(n.bind);
            if(n.animated){OriginalAnimTrack t;for(unsigned k=0;k<6;k++)t.base[k]=F(n.base[k]);t.mask=n.mask;
                for(uint32_t c=0;c<n.curveCount;c++){const auto& cv=browserRailTeeterCurves[n.firstCurve+c];std::vector<OriginalAnimSegment> segs;
                    for(uint32_t k=0;k<cv.count;k++){const auto& g=browserRailTeeterSegments[cv.first+k];segs.push_back({F(g.a),F(g.b),F(g.c),F(g.d),F(g.t0),F(g.t1)});}
                    t.curves.push_back(segs);}
                node.track=t;}
            out.model.nodes.push_back(node);
        }
        const auto& a=s.teeterArgs;out.args={F(a[1]),F(a[2]),F(a[3]),F(a[4]),F(a[5]),F(a[6]),a[7],F(a[8]),F(a[9])};
        out.matrix=fromBits(s.matrix);out.scale=F(s.scale);for(unsigned k=0;k<3;k++){out.low[k]=F(s.boundsMin[k]);out.high[k]=F(s.boundsMax[k]);}
        return out;
    }
    throw std::runtime_error("seed");
}
static std::vector<uint32_t> teeterWords(const OriginalAnimTeeter& e){
    const float f[]={e.torque,e.step,e.gain,e.restitution,e.minimum,e.maximum,e.maxStep,e.damping,e.stiffness,e.rest,e.velocity};
    std::vector<uint32_t> w;for(float x:f)w.push_back(B(x));w.push_back(e.flipMask);
    for(float x:{e.time,e.sampleTime,e.previous,e.unclamped})w.push_back(B(x));w.push_back(uint32_t(e.evaluated));return w;
}
static unsigned diff(const live::Machine& m,uint32_t at,const std::vector<uint32_t>& port,const char* what,unsigned& printed,size_t from=0,size_t to=~size_t(0)){
    unsigned bad=0;
    for(size_t k=from;k<std::min(port.size(),to);k++)if(m.get<uint32_t>(at+4*uint32_t(k))!=port[k]){
        if(printed++<40)printf("    %s +%03zx original %08x (%g) port %08x (%g)\n",what,4*k,m.get<uint32_t>(at+4*uint32_t(k)),m.get<float>(at+4*uint32_t(k)),port[k],F(port[k]));
        bad++;
    }
    return bad;
}
static std::vector<uint32_t> railWords(const OriginalRailModifier& r){
    std::vector<uint32_t> w;for(auto& q:{r.boundsMin,r.boundsMax})for(float x:q)w.push_back(B(x));w.push_back(r.packedId);w.push_back(uint32_t(r.node));
    return w;
}
static std::vector<uint32_t> matrixWords(const RollerMatrix& m){std::vector<uint32_t> w;for(auto& r:m)for(float x:r)w.push_back(B(x));return w;}
// 35B8xx descriptor lookup through the stage table (0x2D1BD8).
static uint32_t descriptor(live::Machine& m,uint32_t packed){
    auto c=m.call(0x2d1bd8);uint32_t table=m.get<uint32_t>(m.get<uint32_t>(GPR_U32((&c),2))+8);
    uint32_t kind=m.get<uint32_t>(table+4*(packed&0xff));if(!kind)return 0;
    uint32_t w=m.get<uint32_t>(m.get<uint32_t>(kind+0x44)+4*(packed>>8));return (w>>8)<<2;
}
static std::vector<RollerMatrix> segmentRows(live::Machine& m,uint32_t desc,OriginalRailRecord& record){
    record.flags=m.get<uint32_t>(desc+0x1C);record.surface=m.get<int32_t>(desc+0x28);
    std::vector<RollerMatrix> rows;uint32_t seg=m.get<uint32_t>(desc+0x24);
    for(uint32_t i=0;i<m.get<uint32_t>(desc+0x20);i++){rows.push_back(matrixAt(m,seg+0x10));seg=m.get<uint32_t>(seg+0x64);}
    return rows;
}
static void entries(live::Machine& m,std::initializer_list<std::array<uint32_t,3>> list){
    uint32_t at=ARGS;for(auto& e:list){m.put(at,e[0]);m.put(at+4,e[1]);m.put(at+8,uint32_t(0));m.put(at+12,e[2]);at+=16;}
}

int main(int argc,char** argv){
    if(argc<3){fprintf(stderr,"usage: live FOLDER TICKS\n");return 2;}
    std::string folder=argv[1];int ticks=atoi(argv[2]);unsigned failures=0;
    struct Case {const char* name;const char* build;const char* live;uint32_t instance,teeter,rail;};
    const Case cases[]={{"mdl_ARA1_logbreakteeter_1200","319","718",0x101a6b0,0x590680,0x5a4700},
                        {"mdl_ARA1_logbreakteeter_1000","1518","1919",0x10adef0,0x58ed80,0x5acb00},
                        {"mdl_ARA1_logbreakteeter_2000","4719","5118",0xfb5270,0x58de80,0x5a4800},
                        {"mdl_ARA1_logteetera_3000","7919",nullptr,0xfccb20,0,0}};
    for(const auto& k:cases){
        Seed s=seedFor(k.name);unsigned printed=0,bad=0;
        const float dt60=F(0x3c888889u);
        // ---- 1. construction with the original builtins ----
        std::vector<OriginalRailModifier> portRails;OriginalAnimTeeter built;uint32_t builtObject=0,flagsAfter=0;
        {
            live::Machine m(folder+"/"+k.build+".ee",folder+"/"+k.build+".vuc",folder+"/"+k.build+".vud");
            if(m.get<uint32_t>(k.instance+0xC)!=0||m.get<uint32_t>(k.instance+0x78)!=s.seed->resource)throw std::runtime_error("instance not pristine");
            uint32_t scene=m.get<uint32_t>(gp+0xCE8);m.put(scene+0x290,k.instance);
            // program word order: 0x126 (1,1000f) 0x228 (2,0) 0x529 (5,0f) 0x426 (4,.5f) 0x780329 (3,120f)
            entries(m,{{1,0x447a0000u,2},{2,0,1},{5,0,2},{4,0x3f000000u,2},{3,0x42f00000u,2}});
            m.call(0x2fb498,{RESULT,5,ARGS});
            uint32_t entity=m.get<uint32_t>(k.instance+0xC);builtObject=entity-0x44;
            if(!entity||m.get<uint32_t>(builtObject+0x50)!=0x4908f8)throw std::runtime_error("builtin6 built no AnimTeeter");
            for(uint32_t r=0;r<s.seed->railCount;r++){
                const auto& rail=browserRailTeeterRails[s.seed->firstRail+r];
                // builtin48 (2FF1C8) with its block {instance -1 = current, rail, node} (all int keys, 446470):
                // entity vtable+0x84 (360DD0) != 0, then 355E38(entity, block).
                m.put(ARGS,uint32_t(0xffffffff));m.put(ARGS+4,rail.packedId);m.put(ARGS+8,uint32_t(rail.node));m.put(ARGS+12,uint32_t(0));
                m.call(0x355e38,{entity,ARGS});
            }
            built=originalAnimTeeterConstruct(s.model,s.matrix,s.scale,s.args,m.get<int32_t>(m.get<uint32_t>(gp+0x2A74)+0x10));
            bad+=diff(m,builtObject,teeterWords(built),"ctor",printed);
            if(!(m.get<uint16_t>(builtObject+0x56)&1))bad++;
            flagsAfter=m.get<uint32_t>(k.instance+8);
            // RailModifiers: container+0x1C list (35B670 inserts at the head)
            uint32_t list=m.get<uint32_t>(m.get<uint32_t>(entity+0x1C)+0x1C);unsigned found=0;
            for(uint32_t r=0;r<s.seed->railCount;r++){
                const auto& rail=browserRailTeeterRails[s.seed->firstRail+r];
                auto port=originalRailModifierConstruct(rail.packedId,rail.node,k.instance,s.model,s.matrix,s.scale,s.low,s.high);portRails.push_back(port);
                for(uint32_t o=list;o;o=m.get<uint32_t>(o))if(m.get<uint32_t>(o+0x30)==rail.packedId){
                    found++;bad+=diff(m,o+0x10,railWords(port),"rail-ctor",printed);bad+=diff(m,o+0x50,matrixWords(port.restInverse),"rail-ctor-inverse",printed);
                    if(m.get<uint32_t>(o+0x40)!=k.instance||m.get<uint32_t>(o+8)!=0x4911d0)bad++;
                }
            }
            if(found!=s.seed->railCount)bad++;
            printf("%s: original builtin6+builtin48 on the tick-%s savestate vs port construction: %u differing words (%u rails, instance flags %08x -> %08x)\n",
                k.name,k.build,bad,found,s.seed->authoredFlags|3u,flagsAfter);
        }
        failures+=bad;
        if(!k.live)continue;
        // ---- 2. the game's own objects and the lockstep ----
        live::Machine A(folder+"/"+k.live+".ee",folder+"/"+k.live+".vuc",folder+"/"+k.live+".vud");
        const uint32_t obj=k.teeter,rail=k.rail,entity=obj+0x44;
        unsigned base=0;
        if(A.get<uint32_t>(k.instance+0xC)!=entity||A.get<uint32_t>(rail+0x40)!=k.instance)throw std::runtime_error("live objects");
        {   // config words from the game's construction; state after N quiet updates
            OriginalAnimTeeter e=built;originalAnimTeeterUpdate(e,dt60);
            base+=diff(A,obj,teeterWords(e),"live-state",printed);
            base+=diff(A,rail+0x10,railWords(portRails[0]),"live-rail",printed);base+=diff(A,rail+0x50,matrixWords(portRails[0].restInverse),"live-rail-inverse",printed);
            base+=A.get<uint32_t>(k.instance+8)!=flagsAfter;
        }
        printf("  tick-%s live objects vs port construction + one quiet update: %u differing words (flags %08x)\n",k.live,base,A.get<uint32_t>(k.instance+8));
        failures+=base;
        OriginalAnimTeeter e=built;originalAnimTeeterUpdate(e,dt60);
        const OriginalRailModifier& port=portRails[0];
        OriginalRailRecord record;record.packedId=port.packedId;uint32_t desc=descriptor(A,port.packedId);auto rows=segmentRows(A,desc,record);
        const float dt=A.get<float>(A.get<uint32_t>(gp-0x848)+0x14);
        unsigned tickBad=0,forces=0,applied=0,queries=0,hits=0,queryBad=0,matrixBad=0,atMax=0;float peak=0;
        for(int t=0;t<ticks;t++){
            // a rider force (106848 attach impulse / 106F78 x fps) at a point near the bound rail
            if(rng()%6==0){
                RollerMatrix tm=originalRailModifierTransform(port,e);
                RollerQuad p=roller_math::transform(rail_modifier_math::product(rows[rng()%rows.size()],tm),{0,0,0,1});
                RollerQuad dir=roller_math::transform(rail_modifier_math::product(rows[0],tm),{0,0,1,0});
                float along=uni(0,1);RollerQuad point{p[0]+dir[0]*along+uni(-20,20),p[1]+dir[1]*along+uni(-20,20),p[2]+dir[2]*along+uni(-20,20),1};
                float magnitude=std::pow(10.f,uni(1.7f,5.3f));RollerQuad force{uni(-.3f,.3f)*magnitude,uni(-.3f,.3f)*magnitude,-uni(.2f,1)*magnitude,0};
                if(rng()%3==0)for(unsigned q=0;q<3;q++)force[q]=-force[q];
                int32_t node=rng()%8?port.node:int32_t(rng()%s.model.nodes.size());
                A.put(CONTACT,point);A.put(CONTACT+0x5C,node);A.put(FORCE,force);
                A.call(0x342538,{obj,CONTACT,FORCE});applied+=originalAnimTeeterApplyForce(e,node,point,force);forces++;
            }
            A.call(0x356198,{entity});originalAnimTeeterUpdate(e,dt);
            unsigned d=diff(A,obj,teeterWords(e),"tick",printed);
            if(bool(A.get<uint16_t>(obj+0x56)&1)!=e.dirty)d++;
            d+=diff(A,rail+0x10,railWords(port),"tick-rail",printed);d+=diff(A,rail+0x50,matrixWords(port.restInverse),"tick-rail",printed);
            if(d&&printed<40)printf("   tick %d\n",t);tickBad+=d!=0;
            peak=std::max(peak,e.time);atMax+=e.time==e.maximum;
            // rail query near the (moving) rail
            for(unsigned q=0;q<2;q++){
                RollerMatrix tm=originalRailModifierTransform(port,e);
                RollerQuad p=roller_math::transform(rail_modifier_math::product(rows[rng()%rows.size()],tm),{uni(0,1),uni(0,1),uni(0,1),1});
                RollerQuad point{p[0]+uni(-250,250),p[1]+uni(-250,250),p[2]+uni(-250,250),1};
                A.put(POINT,point);A.put(FOUND,uint32_t(0));A.put(BEST,0.f);
                for(uint32_t o=0;o<0x74;o+=4)A.put(OUT+o,uint32_t(0xdeadbeef));
                A.call(0x35c698,{rail,0,0,POINT,1,FOUND,BEST,OUT});
                bool found=false;float best=0;OriginalRailModifierHit hit;
                originalRailModifierQuery(port,record,rows,originalRailModifierTransform(port,e),point,1,found,best,hit);
                unsigned qd=0;queries++;
                if(A.get<uint32_t>(FOUND)!=uint32_t(found)||A.get<uint32_t>(BEST)!=B(best))qd++;
                if(found){hits++;std::vector<uint32_t> w;for(float x:hit.point)w.push_back(B(x));for(float x:hit.tangent)w.push_back(B(x));
                    qd+=diff(A,OUT,w,"query",printed);
                    if(A.get<uint32_t>(OUT+0x68)!=B(hit.t)||A.get<uint32_t>(OUT+0x50)!=k.instance||A.get<int32_t>(OUT+0x5C)!=port.node||A.get<uint32_t>(OUT+0x58)!=desc)qd++;}
                queryBad+=qd!=0;
            }
            if(t%7==0){
                for(uint32_t n=0;n<s.model.nodes.size();n++){
                    auto c=A.call(0x3610e0,{obj+0x30,n});const RollerMatrix& pm=originalAnimNodeMatrix(e,int32_t(n));
                    matrixBad+=diff(A,GPR_U32((&c),2),matrixWords(pm),"node-matrix",printed)!=0;
                }
            }
        }
        printf("  lockstep %d ticks from %s: %u/%d ticks byte-identical (teeter +0x00..+0x40, dirty, rail bounds/inverse), %u forces (%u above the 100 threshold on animated nodes), "
               "peak time %g (%u ticks at the maximum %g); %u/%u 35C698 queries identical (%u hits); node matrices %u mismatches\n",
               ticks,k.live,ticks-tickBad,ticks,forces,applied,peak,atMax,e.maximum,queries-queryBad,queries,hits,matrixBad);
        failures+=tickBad+queryBad+matrixBad;
    }
    printf("total failures %u\n",failures);
    if(getenv("COUNTS"))for(auto [pc,n]:callCounts)fprintf(stderr,"%06x %llu\n",pc,(unsigned long long)n);
    return failures?1:0;
}
