// Live parity of the falling billboard (web/falling_billboard.inc: engine/livecomp_animation.hpp +
// engine/rail_modifier.hpp; tools/test_falling_billboard_live.py) on the PS2 capture that fires it
// (local/ps2-capture/runs/setpieces-bb/fall.*: bcvolume_1001 contact at tick 1913):
//   construction: the LiveComp object (program 144 builtin3) and both RailModifiers (builtin48 0x5B08 /
//     0x5A08 on node 0) in the fall.tick1913 savestate vs originalLiveCompConstruct /
//     originalRailModifierConstruct (every modeled word);
//   lockstep: original 0x356198 (0x341D48 + 0x352D20 -> 0x35C4E0) + 0x361098 node matrices + 0x35C5A0 rail
//     transforms + 0x35C698 queries of random points around both rails, per tick, vs the port;
//   later savestates of the run: LiveComp words, clean node matrices, RailModifier words.
// config.txt: "entity E", "rail MOD PACKED", "ticks N", "target OFFSET PATH".
#pragma clang optimize off
#include "live_registrations.inc"
#pragma clang optimize on
#include "original_live_runtime.hpp"
#include "../engine/livecomp_animation.hpp"
#include "../web/generated/falling_billboard_seed.hpp"
#include <fstream>
#include <map>
#include <random>
#include <sstream>
using namespace ssx;
static constexpr uint32_t ARENA=0xE0000,POINT=ARENA+0x600,FOUND=ARENA+0x610,BEST=ARENA+0x614,OUT=ARENA+0x680,XFORM=ARENA+0x700;
static FILE* logf_=stdout;
static float F(uint32_t b){return std::bit_cast<float>(b);}
static RollerMatrix rowsOf(const std::array<uint32_t,16>& w){RollerMatrix r;for(unsigned i=0;i<16;i++)r[i/4][i%4]=F(w[i]);return r;}
static unsigned words(const uint8_t* a,const void* b,size_t n){unsigned bad=0;for(size_t o=0;o<n;o+=4)bad+=std::memcmp(a+o,static_cast<const uint8_t*>(b)+o,4)!=0;return bad;}
static std::array<uint32_t,12> headWords(const OriginalLiveComp& s){
    auto b=[](float x){return std::bit_cast<uint32_t>(x);};
    return {uint32_t(uint16_t(s.mode)),uint32_t(s.enabled),uint32_t(s.done),uint32_t(s.delay),b(s.rate),b(s.low),b(s.high),b(s.anim.time),b(s.anim.sampleTime),b(s.anim.previous),b(s.anim.unclamped),uint32_t(s.anim.evaluated)};
}
static unsigned headDiff(const uint8_t* ee,uint32_t obj,const std::array<uint32_t,12>& h,bool print){unsigned bad=0;for(unsigned k=0;k<12;k++){uint32_t v;std::memcpy(&v,ee+((obj+4*k)&0x1ffffff),4);if(k==0)v&=0xFFFF;if(v!=h[k]){bad++;if(print)fprintf(logf_,"    head+%x original %08x port %08x\n",4*k,v,h[k]);}}return bad;}
struct Rail {uint32_t modifier=0,packed=0;OriginalRailModifier port;OriginalRailRecord record;uint32_t head=0;};
static std::vector<uint32_t> railWords(const OriginalRailModifier& m){
    std::vector<uint32_t> w;for(auto& q:{m.boundsMin,m.boundsMax})for(float x:q)w.push_back(std::bit_cast<uint32_t>(x));
    w.push_back(m.packedId);w.push_back(uint32_t(m.node));for(auto& r:m.restInverse)for(float x:r)w.push_back(std::bit_cast<uint32_t>(x));return w;
}
static unsigned railDiff(const uint8_t* ee,const Rail& r,bool print){
    auto w=railWords(r.port);unsigned bad=0;auto at=[&](uint32_t o){uint32_t v;std::memcpy(&v,ee+((r.modifier+o)&0x1ffffff),4);return v;};
    for(unsigned k=0;k<8;k++)if(at(0x10+4*k)!=w[k]){bad++;if(print)fprintf(logf_,"    rail %x +%02x original %08x port %08x\n",r.packed,0x10+4*k,at(0x10+4*k),w[k]);}
    if(at(0x30)!=w[8]||at(0x34)!=w[9])bad++;
    for(unsigned k=0;k<16;k++)if(at(0x50+4*k)!=w[10+k]){bad++;if(print)fprintf(logf_,"    rail %x inverse[%u] original %08x port %08x\n",r.packed,k,at(0x50+4*k),w[10+k]);}
    return bad;
}
int main(int argc,char** argv){
    if(argc<2){fprintf(stderr,"usage: live FOLDER\n");return 2;}
    const std::string folder=argv[1];unsigned failures=0;
    live::Machine A(folder+"/start.ee",folder+"/start.vuc",folder+"/start.vud");
    uint32_t entity=0;int ticks=0;std::vector<Rail> rails;std::map<int,std::string> targets;
    std::ifstream config(folder+"/config.txt");std::string line;
    while(std::getline(config,line)){std::istringstream in(line);std::string k;in>>k;
        if(k=="entity")in>>std::hex>>entity;
        else if(k=="rail"){Rail r;in>>std::hex>>r.modifier>>r.packed;rails.push_back(r);}
        else if(k=="ticks")in>>std::dec>>ticks;
        else if(k=="target"){int t;std::string p;in>>std::dec>>t>>p;targets[t]=p;}}
    const auto& s=browserRailLiveCompSeeds[0];
    OriginalAnimModel model;model.length=F(s.length);
    for(uint32_t i=0;i<s.nodeCount;i++){const auto& n=browserRailLiveCompNodes[s.firstNode+i];OriginalAnimNode node;node.parent=n.parent;node.bind=rowsOf(n.bind);
        if(n.animated){OriginalAnimTrack t;for(unsigned k=0;k<6;k++)t.base[k]=F(n.base[k]);t.mask=n.mask;
            for(uint32_t c=0;c<n.curveCount;c++){const auto& cv=browserRailLiveCompCurves[n.firstCurve+c];std::vector<OriginalAnimSegment> segs;
                for(uint32_t k=0;k<cv.count;k++){const auto& g=browserRailLiveCompSegments[cv.first+k];segs.push_back({F(g.a),F(g.b),F(g.c),F(g.d),F(g.t0),F(g.t1)});}
                t.curves.push_back(std::move(segs));}
            node.track=std::move(t);}
        model.nodes.push_back(std::move(node));}
    const RollerMatrix matrix=rowsOf(s.matrix);const float scale=F(s.scale);
    const uint32_t obj=entity-0x30,instance=A.get<uint32_t>(entity+0x18);
    if(A.get<uint32_t>(entity+0xC)!=0x490B10u||A.get<uint32_t>(instance+0x78)!=s.resource)throw std::runtime_error("billboard entity");
    // ---- construction ----
    OriginalLiveCompArguments args;for(unsigned k=0;k<11;k++)args.words[k]=s.words[k];
    OriginalLiveComp live=originalLiveCompConstruct(model,matrix,scale,s.flags,args,[]{return 0u;});
    unsigned constructBad=headDiff(A.ee.data(),obj,headWords(live),true);
    if(bool(A.get<uint32_t>(instance+8)&4)!=live.suppressStaticDraw){constructBad++;fprintf(logf_,"  instance flags %08x vs suppressStaticDraw %d\n",A.get<uint32_t>(instance+8),live.suppressStaticDraw);}
    const std::array<float,3> low{F(s.boundsMin[0]),F(s.boundsMin[1]),F(s.boundsMin[2])},high{F(s.boundsMax[0]),F(s.boundsMax[1]),F(s.boundsMax[2])};
    for(auto& r:rails){
        r.port=originalRailModifierConstruct(r.packed,0,s.resource,model,matrix,scale,low,high);
        if(A.get<uint32_t>(r.modifier+0x40)!=instance)throw std::runtime_error("rail instance");
        constructBad+=railDiff(A.ee.data(),r,true);
        // rail record from memory (0x3451C0 binding; flags/surface as the stage descriptor)
        A.call(0x3451c0,{ARENA+0x400,r.packed});r.head=A.get<uint32_t>(ARENA+0x408);r.record.packedId=r.packed;r.record.flags=0x30003;int32_t index=0;
        for(uint32_t seg=r.head;seg;seg=A.get<uint32_t>(seg+0x64)){OriginalRailSegment q;q.index=index++;q.length=A.get<float>(seg+0xC);q.distance=A.get<float>(seg+0x84);
            for(unsigned row=0;row<4;row++)for(unsigned k=0;k<3;k++)q.coefficients[row][k]=A.get<float>(seg+0x10+16*row+4*k);r.record.segments.push_back(q);}
    }
    fprintf(logf_,"[construction at the fall.tick1913 savestate] LiveComp words + %zu RailModifiers: %s (%u words differ)\n",rails.size(),constructBad?"DIFFERS":"byte-identical",constructBad);
    failures+=constructBad;
    // ---- lockstep ----
    std::mt19937 rng(0x5A08);auto uni=[&](float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);};
    unsigned exact=0,queries=0,queryBad=0,hits=0,snapshotChecks=0,snapshotBad=0;
    for(int t=1;t<=ticks;t++){
        A.call(0x356198,{entity});A.call(0x3568b0,{entity});
        originalLiveCompTick(live);for(auto& r:rails)originalRailModifierBounds(r.port,std::nullopt);
        auto c=A.call(0x361098,{obj+0x1C});const uint32_t mats=GPR_U32((&c),2);
        const auto& m=originalLiveCompMatrices(live);
        unsigned d=words(A.ee.data()+mats,m.data(),64*m.size())+headDiff(A.ee.data(),obj,headWords(live),false);
        for(auto& r:rails){
            d+=railDiff(A.ee.data(),r,false);
            A.call(0x35c5a0,{r.modifier,XFORM});const RollerMatrix T=originalRailModifierTransform(r.port,m.at(0));
            d+=words(A.ee.data()+XFORM,T.data(),64);
            for(int q=0;q<8;q++){
                const auto& segment=r.record.segments[size_t(rng()%r.record.segments.size())];
                RollerQuad base=roller_math::transform(rail_modifier_math::product(originalRailSegmentRows(segment),T),{0,0,0,1});
                RollerQuad p{base[0]+uni(-300,300),base[1]+uni(-300,300),base[2]+uni(-300,300),1.f};
                A.put(POINT,p);A.put(FOUND,uint32_t(0));A.put(BEST,0.f);for(uint32_t o=0;o<0x74;o+=4)A.put(OUT+o,uint32_t(0xdeadbeef));
                A.call(0x35c698,{r.modifier,0,0,POINT,1,FOUND,BEST,OUT});
                bool found=false;float best=0;OriginalRailModifierHit hit;originalRailModifierQuery(r.port,r.record,T,p,1,found,best,hit);
                unsigned qd=(A.get<uint32_t>(FOUND)!=uint32_t(found))+(A.get<uint32_t>(BEST)!=std::bit_cast<uint32_t>(best));
                if(found){hits++;qd+=words(A.ee.data()+OUT,hit.point.data(),16)+words(A.ee.data()+OUT+16,hit.tangent.data(),16)+(A.get<uint32_t>(OUT+0x68)!=std::bit_cast<uint32_t>(hit.t));}
                queries++;queryBad+=qd!=0;
            }
        }
        if(!d)exact++;else if(t-int(exact)<=3)fprintf(logf_,"  tick +%d: %u words differ\n",t,d);
        if(auto it=targets.find(t);it!=targets.end()){
            auto ee=live::readFile(it->second);unsigned bad=0,checks=1;bad+=headDiff(ee.data(),obj,headWords(live),false)!=0;
            uint32_t dirty;std::memcpy(&dirty,ee.data()+obj+0x40,4);
            if(!((dirty>>16)&1)){uint32_t at;std::memcpy(&at,ee.data()+obj+0x60,4);checks++;bad+=words(ee.data()+at,m.data(),64*m.size())!=0;}
            for(auto& r:rails){checks++;bad+=railDiff(ee.data(),r,false)!=0;}
            fprintf(logf_,"  PS2 savestate +%d (%s): %u/%u checks equal (time %g%s)\n",t,it->second.substr(it->second.rfind('/')+1).c_str(),checks-bad,checks,live.anim.time,live.done?", done":"");
            snapshotChecks+=checks;snapshotBad+=bad;
        }
    }
    fprintf(logf_,"[lockstep] %u/%d ticks byte-identical (LiveComp words, node matrices, RailModifier words, 0x35C5A0 transforms); %u/%u 0x35C698 queries identical (%u hits)\n",exact,ticks,queries-queryBad,queries,hits);
    fprintf(logf_,"[PS2 savestates] %u/%u checks equal\n",snapshotChecks-snapshotBad,snapshotChecks);
    failures+=unsigned(ticks)-exact+queryBad+snapshotBad;
    fprintf(logf_,failures?"FAILED (%u)\n":"all live checks match\n",failures);
    return failures?1:0;
}
