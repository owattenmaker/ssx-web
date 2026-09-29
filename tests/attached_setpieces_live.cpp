// Live parity of engine/parent_modifier.hpp (development oracle; tools/test_attached_setpieces_live.py):
// the full recompiled original entity updates on PS2 savestates in lockstep with the port.
//   spline LiveComps (the Snow Jam raven from race.tick4319, the Junction blimp from the-junction-ready):
//     original 0x356198 (modifier 0x359698 + LiveComp 0x341D48) + 0x3568B0, node matrices 0x361098
//     vs originalSplineUpdate + originalLiveCompTick + originalLiveCompMatricesOn(spline matrix);
//   ParentModifier children (blimpad x2, blimplights on the blimp; searchlightglowa on searchlightbasea
//     node 3): original 0x356198 + 0x3568B0 of the child (0x3619B8, 0x357108) and 0x361940, vs
//     originalParentUpdate + originalParentMatrix on the port parent (spline LiveComps) or on the
//     original parent node matrix (plain LiveComps, whose player is web/livecomp-animation.js);
//   every later kept PS2 savestate of the same run: port state vs memory (LiveComp words, clean node
//     matrices, clean child matrices).
// config.txt: "splinelc RES ENTITY MODIFIER", "livecomp RES ENTITY", "parent CHILDRES CHILDENTITY MODIFIER PARENTRES NODE",
// "ticks N", "target OFFSET PATH.ee".
#pragma clang optimize off
#include "live_registrations.inc"
#pragma clang optimize on
#include "original_live_runtime.hpp"
#include "../engine/spline_modifier.hpp"
#include "../engine/parent_modifier.hpp"
#include "../web/generated/attached_seed_ARA1.hpp"
#include "../web/generated/attached_seed_BHP1.hpp"
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
using namespace ssx;
static constexpr uint32_t GP=0x4A30F0;
static FILE* logf_=stdout;
static RollerMatrix rowsOf(const std::array<uint32_t,16>& w){RollerMatrix r;for(unsigned i=0;i<16;i++)r[i/4][i%4]=std::bit_cast<float>(w[i]);return r;}
template<class NS> static bool modelOf(uint32_t resource,OriginalAnimModel& model,RollerMatrix& authored,float& scale){
    for(const auto& s:NS::splineLiveComps){
        if(s.resource!=resource)continue;
        model.length=std::bit_cast<float>(s.length);
        for(uint32_t i=0;i<s.nodeCount;i++){
            const auto& n=NS::nodes[s.firstNode+i];OriginalAnimNode node;node.parent=n.parent;node.bind=rowsOf(n.bind);
            if(n.animated){OriginalAnimTrack t;for(unsigned k=0;k<6;k++)t.base[k]=std::bit_cast<float>(n.base[k]);t.mask=n.mask;
                for(uint32_t c=0;c<n.curveCount;c++){const auto& cv=NS::curves[n.firstCurve+c];std::vector<OriginalAnimSegment> segs;
                    for(uint32_t k=0;k<cv.count;k++){const auto& g=NS::segments[cv.first+k];auto F=[](uint32_t b){return std::bit_cast<float>(b);};segs.push_back({F(g.a),F(g.b),F(g.c),F(g.d),F(g.t0),F(g.t1)});}
                    t.curves.push_back(std::move(segs));}
                node.track=std::move(t);}
            model.nodes.push_back(std::move(node));
        }
        authored=rowsOf(s.matrix);scale=std::bit_cast<float>(s.scale);return true;
    }
    return false;
}
struct Path {std::shared_ptr<OriginalRailRecord> record;uint32_t head=0;};
static Path bindPath(live::Machine& m,uint32_t resource){
    constexpr uint32_t BIND=0xE0400;m.call(0x3451c0,{BIND,resource});
    Path p;p.head=m.get<uint32_t>(BIND+8);p.record=std::make_shared<OriginalRailRecord>();p.record->packedId=resource;int32_t index=0;
    for(uint32_t seg=p.head;seg;seg=m.get<uint32_t>(seg+0x64)){
        OriginalRailSegment s;s.index=index++;s.length=m.get<float>(seg+0xC);s.distance=m.get<float>(seg+0x84);
        for(unsigned row=0;row<4;row++)for(unsigned k=0;k<3;k++)s.coefficients[row][k]=m.get<float>(seg+0x10+16*row+4*k);
        for(unsigned k=0;k<4;k++)s.arcToParameter[k]=m.get<float>(seg+0x50+4*k);
        p.record->segments.push_back(s);
    }
    return p;
}
static std::array<uint32_t,12> headWords(const OriginalLiveComp& s){
    auto b=[](float x){return std::bit_cast<uint32_t>(x);};
    return {uint32_t(uint16_t(s.mode)),uint32_t(s.enabled),uint32_t(s.done),uint32_t(s.delay),b(s.rate),b(s.low),b(s.high),b(s.anim.time),b(s.anim.sampleTime),b(s.anim.previous),b(s.anim.unclamped),uint32_t(s.anim.evaluated)};
}
static void seedLiveComp(OriginalLiveComp& s,const OriginalAnimModel& model,const RollerMatrix& authored,float scale,const uint8_t* ee,uint32_t obj){
    s=originalLiveCompConstruct(model,authored,scale,0u,OriginalLiveCompArguments::defaults(),[]{return 0u;});
    auto W=[&](uint32_t o){uint32_t v;std::memcpy(&v,ee+((obj+o)&0x1ffffff),4);return v;};auto F=[&](uint32_t o){return std::bit_cast<float>(W(o));};
    s.mode=int16_t(W(0)&0xFFFF);s.enabled=int32_t(W(4));s.done=int32_t(W(8));s.delay=int32_t(W(0xC));s.rate=F(0x10);s.low=F(0x14);s.high=F(0x18);
    s.anim.time=F(0x1C);s.anim.sampleTime=F(0x20);s.anim.previous=F(0x24);s.anim.unclamped=F(0x28);s.anim.evaluated=int32_t(W(0x2C));
    const uint32_t channels=W(0x64);
    for(size_t c=0;c<s.anim.channels.size();c++)for(unsigned k=0;k<16;k++){uint32_t v;std::memcpy(&v,ee+((channels+0xD0*uint32_t(c)+4*k)&0x1ffffff),4);s.anim.channels[c].segment[k]=int32_t(v);}
    s.anim.dirty=true;
}
struct SplineLC {uint32_t resource=0,entity=0,modifier=0;OriginalAnimModel model;RollerMatrix authored{};float scale=1;OriginalLiveComp live;OriginalSplineModifier spline;Path path;unsigned exact=0,compared=0,bad=0;};
struct PlainLC {uint32_t resource=0,entity=0;};
struct Child {uint32_t resource=0,entity=0,modifier=0,parent=0;int32_t node=0;OriginalParentModifier port;unsigned exact=0,compared=0,bad=0;};
static unsigned headDiff(const uint8_t* ee,uint32_t obj,const std::array<uint32_t,12>& h){unsigned bad=0;for(unsigned k=0;k<12;k++){uint32_t v;std::memcpy(&v,ee+((obj+4*k)&0x1ffffff),4);if(k==0)v&=0xFFFF;bad+=v!=h[k];}return bad;} // +0x00 mode is an int16 (+0x02 unrelated)
static unsigned words(const uint8_t* a,const void* b,size_t n){unsigned bad=0;for(size_t o=0;o<n;o+=4)bad+=std::memcmp(a+o,static_cast<const uint8_t*>(b)+o,4)!=0;return bad;}
int main(int argc,char** argv){
    if(argc<3){fprintf(stderr,"usage: live FOLDER SCENARIO\n");return 2;}
    const std::string folder=argv[1],scenario=argv[2];unsigned failures=0;
    live::Machine A(folder+"/"+scenario+"/start.ee",folder+"/"+scenario+"/start.vuc",folder+"/"+scenario+"/start.vud");
    const float dt=A.get<float>(A.get<uint32_t>(GP-0x848)+0x14);
    std::vector<std::unique_ptr<SplineLC>> splines;std::vector<PlainLC> plains;std::vector<Child> children;int ticks=0;std::vector<std::pair<int,std::string>> targets;
    std::ifstream config(folder+"/"+scenario+"/config.txt");std::string line;
    while(std::getline(config,line)){
        std::istringstream in(line);std::string kind;in>>kind;
        if(kind=="splinelc"){auto s=std::make_unique<SplineLC>();in>>std::hex>>s->resource>>s->entity>>s->modifier;
            if(!modelOf<browser_attached_ara1_ns>(s->resource,s->model,s->authored,s->scale)&&!modelOf<browser_attached_bhp1_ns>(s->resource,s->model,s->authored,s->scale))throw std::runtime_error("model");
            s->path=bindPath(A,A.get<uint32_t>(s->modifier+0xD8));s->spline=originalSplineModifierFromBytes(A.ee.data()+s->modifier,s->path.record.get(),s->path.head);
            seedLiveComp(s->live,s->model,s->authored,s->scale,A.ee.data(),s->entity-0x30);splines.push_back(std::move(s));}
        else if(kind=="livecomp"){PlainLC p;in>>std::hex>>p.resource>>p.entity;plains.push_back(p);}
        else if(kind=="parent"){Child c;in>>std::hex>>c.resource>>c.entity>>c.modifier>>c.parent>>std::dec>>c.node;
            c.port=originalParentConstruct(c.parent,c.node,A.get<RollerQuad>(c.modifier+0x30));c.port.matrix=A.get<RollerMatrix>(c.modifier+0x50);c.port.dirty=A.get<uint32_t>(c.modifier+0x44);
            if(A.get<int32_t>(c.modifier+0x90)!=c.node)throw std::runtime_error("parent node");children.push_back(c);}
        else if(kind=="ticks")in>>std::dec>>ticks;
        else if(kind=="target"){int t;std::string p;in>>std::dec>>t>>p;targets.push_back({t,p});}
    }
    auto splineOf=[&](uint32_t r)->SplineLC*{for(auto& s:splines)if(s->resource==r)return s.get();return nullptr;};
    auto plainOf=[&](uint32_t r)->PlainLC*{for(auto& p:plains)if(p.resource==r)return &p;return nullptr;};
    std::map<int,std::string> targetAt;for(auto& [t,p]:targets)targetAt[t]=p;
    unsigned snapshotChecks=0,snapshotBad=0;
    for(int t=1;t<=ticks;t++){
        for(auto& s:splines){A.call(0x356198,{s->entity});A.call(0x3568b0,{s->entity});originalSplineUpdate(s->spline,dt);if(originalSplineTakeFinished(s->spline))throw std::runtime_error("spline finished");originalSplineEntityBounds(s->spline);originalLiveCompTick(s->live);}
        for(auto& p:plains){A.call(0x356198,{p.entity});A.call(0x3568b0,{p.entity});}
        for(auto& c:children){A.call(0x356198,{c.entity});A.call(0x3568b0,{c.entity});originalParentUpdate(c.port);}
        // node matrices of the spline LiveComps (original 0x361098 at the draw point) and the children (0x361940)
        for(auto& s:splines){
            auto c=A.call(0x361098,{s->entity-0x30+0x1C});const uint32_t mats=GPR_U32((&c),2);
            const auto& m=originalLiveCompMatricesOn(s->live,originalSplineMatrix(s->spline));
            unsigned d=words(A.ee.data()+mats,m.data(),64*m.size());auto h=headWords(s->live);d+=headDiff(A.ee.data(),s->entity-0x30,h);
            s->compared++;if(d){if(s->bad++<3){fprintf(logf_,"  tick %d %06x: %u words differ:",t,s->resource,d);
                for(unsigned k=0;k<12;k++)if(std::memcmp(A.ee.data()+s->entity-0x30+4*k,&h[k],4))fprintf(logf_," head+%x orig %08x port %08x",4*k,A.get<uint32_t>(s->entity-0x30+4*k),h[k]);
                for(size_t k=0;k<16*m.size();k++){uint32_t o=A.get<uint32_t>(mats+4*uint32_t(k)),q;std::memcpy(&q,reinterpret_cast<const uint8_t*>(m.data())+4*k,4);if(o!=q){fprintf(logf_," node%zu[%zu] orig %08x port %08x",k/16,k%16,o,q);break;}}
                fprintf(logf_,"\n");}}else s->exact++;
        }
        for(auto& c:children){
            auto r=A.call(0x361940,{c.modifier});const uint32_t at=GPR_U32((&r),2);
            const RollerMatrix* parentNode=nullptr;RollerMatrix fromOriginal;
            if(auto* s=splineOf(c.parent))parentNode=&originalLiveCompMatricesOn(s->live,originalSplineMatrix(s->spline)).at(size_t(c.node));
            else if(auto* p=plainOf(c.parent)){auto k=A.call(0x361098,{p->entity-0x30+0x1C});fromOriginal=A.get<RollerMatrix>(GPR_U32((&k),2)+64*uint32_t(c.node));parentNode=&fromOriginal;}
            const auto& m=originalParentMatrix(c.port,[&]{return parentNode;});
            unsigned d=words(A.ee.data()+at,m.data(),64);c.compared++;if(d){if(c.bad++<3)fprintf(logf_,"  tick %d child %06x: %u words differ\n",t,c.resource,d);}else c.exact++;
        }
        if(auto it=targetAt.find(t);it!=targetAt.end()){
            auto ee=live::readFile(it->second);unsigned bad=0,checks=0;
            auto W=[&](uint32_t a){uint32_t v;std::memcpy(&v,ee.data()+(a&0x1ffffff),4);return v;};
            for(auto& s:splines){
                const uint32_t obj=s->entity-0x30;auto h=headWords(s->live);checks++;bad+=headDiff(ee.data(),obj,h)!=0;
                if(!((W(obj+0x40)>>16)&1)){const auto& m=originalLiveCompMatricesOn(s->live,originalSplineMatrix(s->spline));checks++;bad+=words(ee.data()+W(obj+0x60),m.data(),64*m.size())!=0;}
            }
            for(auto& c:children)if(!W(c.modifier+0x44)){checks++;const bool differs=words(ee.data()+c.modifier+0x50,c.port.matrix.data(),64)!=0;bad+=differs;
                if(differs)fprintf(logf_,"    child %06x differs (modifier vtable %08x, child entity %08x, instance->entity %08x)\n",c.resource,W(c.modifier),c.entity,W(W(c.entity+0x18)+0xC));}
            fprintf(logf_,"  PS2 savestate +%d (%s): %u/%u checks equal\n",t,it->second.substr(it->second.rfind('/')+1).c_str(),checks-bad,checks);
            snapshotChecks+=checks;snapshotBad+=bad;
        }
    }
    for(auto& s:splines){fprintf(logf_,"[%s] spline LiveComp %06x: %u/%u ticks node matrices + LiveComp words byte-identical\n",scenario.c_str(),s->resource,s->exact,s->compared);failures+=s->compared-s->exact;}
    for(auto& c:children){fprintf(logf_,"[%s] ParentModifier child %06x <- %06x node %d: %u/%u ticks matrix byte-identical\n",scenario.c_str(),c.resource,c.parent,c.node,c.exact,c.compared);failures+=c.compared-c.exact;}
    fprintf(logf_,"[%s] PS2 savestates: %u/%u checks equal\n",scenario.c_str(),snapshotChecks-snapshotBad,snapshotChecks);failures+=snapshotBad;
    fprintf(logf_,failures?"FAILED (%u)\n":"all live checks match\n",failures);
    return failures?1:0;
}
