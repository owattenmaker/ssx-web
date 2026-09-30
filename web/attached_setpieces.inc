// Attached set pieces (engine/parent_modifier.hpp; seeds web/generated/attached_seed_<LOC>.hpp from
// tools/export_attached_setpieces.py). Included by web/set_piece_gameplay.inc AFTER the
// BrowserSplinePiece / BrowserLoopSpline declarations (it reads browserSplinePieces / browserLoopSplines).
// * Spline LiveComps: the LiveComp player (builtin3) of an entity whose primary modifier is a Spline:
//   the raven flyby (Snow Jam program 54: default player, 0.6 s wing-flap loop at 1/60 s per tick,
//   built with the spline piece) and the Junction blimp (program 39, resident: key5 10 -> 2 s loop at
//   1/180 s per tick, seeded from the race-tick-0 savestate). Node world = local * ... * spline matrix
//   (0x361098 -> 0x356078 -> modifier vt+0x94).
// * ParentModifier children of those (the Junction blimpad_1000/_1001, blimplights_1000 on blimpa node 0):
//   child matrix = parent node world + offset * M (0x357108), re-evaluated on read (update 0x3619B8 dirties).
// ParentModifier children of plain LiveComps (searchlight glows) are evaluated in web/attached-setpieces.js.
// Hooks (web/set_piece_gameplay.inc): attached_reset() at the end of browser_attach_set_pieces(),
// attached_launch(seed.resource) when a spline piece is launched (before its Spline ctor's shared-RNG
// draw), attached_tick() in advance_set_pieces() right after the browserSplinePieces update loop.
// Draw: set_piece_attached() (EMSCRIPTEN export; web/attached-setpieces.js).
#include "../engine/parent_modifier.hpp"
#include <memory>
#if __has_include("generated/attached_seed_ARA1.hpp")
#include "generated/attached_seed_ARA1.hpp"
#define BROWSER_ATTACHED_ARA1 1
#endif
#if __has_include("generated/attached_seed_BRA2.hpp")
#include "generated/attached_seed_BRA2.hpp"
#define BROWSER_ATTACHED_BRA2 1
#endif
#if __has_include("generated/attached_seed_BHP1.hpp")
#include "generated/attached_seed_BHP1.hpp"
#define BROWSER_ATTACHED_BHP1 1
#endif
#if __has_include("generated/attached_seed_ASS1.hpp")
#include "generated/attached_seed_ASS1.hpp"
#define BROWSER_ATTACHED_ASS1 1
#endif
#if __has_include("generated/attached_seed_ABA1.hpp")
#include "generated/attached_seed_ABA1.hpp"
#define BROWSER_ATTACHED_ABA1 1
#endif
#if __has_include("generated/attached_seed_ABC1.hpp")
#include "generated/attached_seed_ABC1.hpp"
#define BROWSER_ATTACHED_ABC1 1
#endif
// Peak 2 (docs/peak2.md)
#if __has_include("generated/attached_seed_CRA3.hpp")
#include "generated/attached_seed_CRA3.hpp"
#define BROWSER_ATTACHED_CRA3 1
#endif
#if __has_include("generated/attached_seed_DRA4.hpp")
#include "generated/attached_seed_DRA4.hpp"
#define BROWSER_ATTACHED_DRA4 1
#endif
#if __has_include("generated/attached_seed_DSS2.hpp")
#include "generated/attached_seed_DSS2.hpp"
#define BROWSER_ATTACHED_DSS2 1
#endif
#if __has_include("generated/attached_seed_CBA2.hpp")
#include "generated/attached_seed_CBA2.hpp"
#define BROWSER_ATTACHED_CBA2 1
#endif
#if __has_include("generated/attached_seed_CHP2.hpp")
#include "generated/attached_seed_CHP2.hpp"
#define BROWSER_ATTACHED_CHP2 1
#endif
#if __has_include("generated/attached_seed_DBC2.hpp")
#include "generated/attached_seed_DBC2.hpp"
#define BROWSER_ATTACHED_DBC2 1
#endif
// Peak 3 (docs/peak3.md)
#if __has_include("generated/attached_seed_ERA5.hpp")
#include "generated/attached_seed_ERA5.hpp"
#define BROWSER_ATTACHED_ERA5 1
#endif
#if __has_include("generated/attached_seed_ESS3.hpp")
#include "generated/attached_seed_ESS3.hpp"
#define BROWSER_ATTACHED_ESS3 1
#endif
#if __has_include("generated/attached_seed_EBA3.hpp")
#include "generated/attached_seed_EBA3.hpp"
#define BROWSER_ATTACHED_EBA3 1
#endif
#if __has_include("generated/attached_seed_EHP3.hpp")
#include "generated/attached_seed_EHP3.hpp"
#define BROWSER_ATTACHED_EHP3 1
#endif
#if __has_include("generated/attached_seed_EBC3.hpp")
#include "generated/attached_seed_EBC3.hpp"
#define BROWSER_ATTACHED_EBC3 1
#endif
struct BrowserAttachedLiveComp {
 uint32_t resource=0;bool resident=false,drawn=false,active=false;uint32_t launchTick=0;
 ssx::OriginalAnimModel model;ssx::RollerMatrix authored{};float scale=1;std::vector<ssx::RollerMatrix> rest;
 ssx::OriginalLiveCompArguments args;std::array<uint32_t,12> state{};uint32_t dirty=0;std::vector<uint32_t> caches;
 ssx::OriginalLiveComp live;
};
struct BrowserAttachedChild {uint32_t child=0,parent=0;int32_t node=0;ssx::RollerQuad offset{};ssx::RollerMatrix authored{};ssx::OriginalParentModifier modifier;bool active=false;};
RIDER_LOCAL static std::vector<std::unique_ptr<BrowserAttachedLiveComp>> attachedLiveComps; // stable addresses (the LiveComp keeps a model pointer)
RIDER_LOCAL static std::vector<BrowserAttachedChild> attachedChildren;
static ssx::RollerMatrix attached_rows(const std::array<uint32_t,16>& w){ssx::RollerMatrix r;for(unsigned i=0;i<16;i++)r[i/4][i%4]=std::bit_cast<float>(w[i]);return r;}
template<class NS> static void attached_load(){
 for(const auto& s:NS::splineLiveComps){
  auto x=std::make_unique<BrowserAttachedLiveComp>();x->resource=s.resource;x->resident=s.resident;x->drawn=s.drawn;
  x->model.length=std::bit_cast<float>(s.length);
  for(uint32_t i=0;i<s.nodeCount;i++){
   const auto& n=NS::nodes[s.firstNode+i];ssx::OriginalAnimNode node;node.parent=n.parent;node.bind=attached_rows(n.bind);
   if(n.animated){ssx::OriginalAnimTrack t;for(unsigned k=0;k<6;k++)t.base[k]=std::bit_cast<float>(n.base[k]);t.mask=n.mask;
    for(uint32_t c=0;c<n.curveCount;c++){const auto& cv=NS::curves[n.firstCurve+c];std::vector<ssx::OriginalAnimSegment> segs;
     for(uint32_t k=0;k<cv.count;k++){const auto& g=NS::segments[cv.first+k];auto F=[](uint32_t b){return std::bit_cast<float>(b);};segs.push_back({F(g.a),F(g.b),F(g.c),F(g.d),F(g.t0),F(g.t1)});}
     t.curves.push_back(std::move(segs));}
    node.track=std::move(t);}
   x->model.nodes.push_back(std::move(node));
  }
  x->authored=attached_rows(s.matrix);x->scale=std::bit_cast<float>(s.scale);
  x->rest=ssx::originalAnimRestMatrices(x->model,x->authored,x->scale);
  for(unsigned k=0;k<11;k++)x->args.words[k]=s.words[k];
  x->state=s.state;x->dirty=s.dirty;x->caches.assign(s.caches.begin(),s.caches.begin()+s.cacheCount);
  attachedLiveComps.push_back(std::move(x));
 }
 for(const auto& p:NS::parents){
  BrowserAttachedChild c;c.child=p.child;c.parent=p.parent;c.node=p.node;
  for(unsigned k=0;k<4;k++)c.offset[k]=std::bit_cast<float>(p.offset[k]);c.authored=attached_rows(p.childMatrix);
  attachedChildren.push_back(c);
 }
}
static BrowserAttachedLiveComp* attached_livecomp(uint32_t resource){for(auto& x:attachedLiveComps)if(x->resource==resource)return x.get();return nullptr;}
// Primary-modifier matrix of a spline LiveComp's entity (0x356078): a launched piece (Spline, or the
// PositionModifier after a stop-mode path end) or a resident looping Spline.
static const ssx::RollerMatrix* attached_root(uint32_t resource){
 for(auto& p:browserSplinePieces)if(p.seed->resource==resource)return p.phase==2?&ssx::originalPositionMatrix(p.position):&ssx::originalSplineMatrix(p.spline);
 for(auto& l:browserLoopSplines)if(l.ready&&l.seed->resource==resource)return &ssx::originalSplineMatrix(l.spline);
 return nullptr;
}
static void attached_activate_children(uint32_t parent){
 for(auto& c:attachedChildren)if(c.parent==parent&&!c.active){c.modifier=ssx::originalParentConstruct(c.parent,c.node,c.offset);c.active=true;}
}
// 0x341AA0 at the spline piece's launch (program 54: builtin3 then builtin19 in the same program run).
// draw=false (pv peakSplines, a streamed world's section run): the section pass already took the program's draws (builtin 3 key 8, builtin
// 19) in program order; the LiveComp's random start word is not carried over (0: unconfirmed).
static void attached_launch(uint32_t resource,bool draw=true){
 auto* x=attached_livecomp(resource);if(!x||x->active)return;
 x->live=draw?ssx::originalLiveCompConstruct(x->model,x->authored,x->scale,0u,x->args,[]{return browser_shared_random_next();}):ssx::originalLiveCompConstruct(x->model,x->authored,x->scale,0u,x->args,[]{return 0u;});
 x->active=true;x->launchTick=chairliftTicks?chairliftTicks-1:0;attached_activate_children(resource);
}
// A section leave destroyed the entity (0x34FD90 -> dtor): the LiveComp and the ParentModifier children on it go with it,
// and the next launch of the piece builds them again (R&B ravensplineanimb: resident at the load, relaunched by program 66).
static void attached_destroy(uint32_t resource){
 if(auto* x=attached_livecomp(resource))x->active=false;
 for(auto& c:attachedChildren)if(c.parent==resource)c.active=false;
}
// The seeds of one location's attached set pieces (the event package's resource ids).
static void attached_load_location(const std::string& code){
#ifdef BROWSER_ATTACHED_ARA1
 if(code=="ARA1")attached_load<browser_attached_ara1_ns>();
#endif
#ifdef BROWSER_ATTACHED_BRA2
 if(code=="BRA2")attached_load<browser_attached_bra2_ns>();
#endif
#ifdef BROWSER_ATTACHED_BHP1
 if(code=="BHP1")attached_load<browser_attached_bhp1_ns>();
#endif
#ifdef BROWSER_ATTACHED_ASS1
 if(code=="ASS1")attached_load<browser_attached_ass1_ns>();
#endif
#ifdef BROWSER_ATTACHED_ABA1
 if(code=="ABA1")attached_load<browser_attached_aba1_ns>();
#endif
#ifdef BROWSER_ATTACHED_ABC1
 if(code=="ABC1")attached_load<browser_attached_abc1_ns>();
#endif
#ifdef BROWSER_ATTACHED_CRA3
 if(code=="CRA3")attached_load<browser_attached_cra3_ns>();
#endif
#ifdef BROWSER_ATTACHED_DRA4
 if(code=="DRA4")attached_load<browser_attached_dra4_ns>();
#endif
#ifdef BROWSER_ATTACHED_DSS2
 if(code=="DSS2")attached_load<browser_attached_dss2_ns>();
#endif
#ifdef BROWSER_ATTACHED_CBA2
 if(code=="CBA2")attached_load<browser_attached_cba2_ns>();
#endif
#ifdef BROWSER_ATTACHED_CHP2
 if(code=="CHP2")attached_load<browser_attached_chp2_ns>();
#endif
#ifdef BROWSER_ATTACHED_DBC2
 if(code=="DBC2")attached_load<browser_attached_dbc2_ns>();
#endif
#ifdef BROWSER_ATTACHED_ERA5
 if(code=="ERA5")attached_load<browser_attached_era5_ns>();
#endif
#ifdef BROWSER_ATTACHED_ESS3
 if(code=="ESS3")attached_load<browser_attached_ess3_ns>();
#endif
#ifdef BROWSER_ATTACHED_EBA3
 if(code=="EBA3")attached_load<browser_attached_eba3_ns>();
#endif
#ifdef BROWSER_ATTACHED_EHP3
 if(code=="EHP3")attached_load<browser_attached_ehp3_ns>();
#endif
#ifdef BROWSER_ATTACHED_EBC3
 if(code=="EBC3")attached_load<browser_attached_ebc3_ns>();
#endif
}
static std::vector<std::string> streamed_set_piece_locations(const std::string& world); // web/set_piece_gameplay.inc
static void attached_reset(){
 attachedLiveComps.clear();attachedChildren.clear();
 // pv peakSplines: a streamed world's attached pieces are every event location's, and none is resident (the free-ride locations load
 // without entities: a spline LiveComp is built with its piece, attached_launch)
 const bool streamed=setPieceStreamed&&browser_streamed_world(setPieceLocationName);
 if(streamed){for(const auto& code:streamed_set_piece_locations(setPieceLocationName))attached_load_location(code);return;}
 attached_load_location(setPieceLocationName);
 for(auto& x:attachedLiveComps){
  if(!x->resident)continue;
  // Resident (built at the load): the race-tick-0 LiveComp words and channel caches.
  x->live=ssx::originalLiveCompConstruct(x->model,x->authored,x->scale,0u,x->args,[]{return 0u;});
  auto F=[](uint32_t b){return std::bit_cast<float>(b);};const auto& w=x->state;
  x->live.mode=int16_t(w[0]&0xFFFF);x->live.enabled=int32_t(w[1]);x->live.done=int32_t(w[2]);x->live.delay=int32_t(w[3]);x->live.rate=F(w[4]);x->live.low=F(w[5]);x->live.high=F(w[6]);
  x->live.anim.time=F(w[7]);x->live.anim.sampleTime=F(w[8]);x->live.anim.previous=F(w[9]);x->live.anim.unclamped=F(w[10]);x->live.anim.evaluated=int32_t(w[11]);
  for(size_t c=0;c<x->live.anim.channels.size()&&16*c+15<x->caches.size();c++)for(unsigned k=0;k<16;k++)x->live.anim.channels[c].segment[k]=int32_t(x->caches[16*c+k]);
  x->live.anim.dirty=true; /* node matrices are recomputed on the first read (root = the seeded spline) */
  x->active=true;attached_activate_children(x->resource);
 }
}
// Entity pass: 0x341D48 per active LiveComp, 0x3619B8 per active child.
static void attached_tick(){
 for(auto& x:attachedLiveComps)if(x->active)ssx::originalLiveCompTick(x->live);
 for(auto& c:attachedChildren)if(c.active)ssx::originalParentUpdate(c.modifier);
}
static const std::vector<ssx::RollerMatrix>* attached_nodes(BrowserAttachedLiveComp& x){
 const auto* root=attached_root(x.resource);if(!root)return nullptr;
 return &ssx::originalLiveCompMatricesOn(x.live,*root);
}
// Native (three.js column-major) delta of T = rest^-1 * world (row vectors), double precision (draw only).
static void attached_delta(std::vector<float>& out,uint32_t resource,int node,const ssx::RollerMatrix& rest,const ssx::RollerMatrix& world){
 double a[3][3],t[3];for(int i=0;i<3;i++){for(int k=0;k<3;k++)a[i][k]=rest[i][k];t[i]=rest[3][i];}
 const double det=a[0][0]*(a[1][1]*a[2][2]-a[1][2]*a[2][1])-a[0][1]*(a[1][0]*a[2][2]-a[1][2]*a[2][0])+a[0][2]*(a[1][0]*a[2][1]-a[1][1]*a[2][0]);
 double inv[4][4]={{(a[1][1]*a[2][2]-a[1][2]*a[2][1])/det,(a[0][2]*a[2][1]-a[0][1]*a[2][2])/det,(a[0][1]*a[1][2]-a[0][2]*a[1][1])/det,0},
  {(a[1][2]*a[2][0]-a[1][0]*a[2][2])/det,(a[0][0]*a[2][2]-a[0][2]*a[2][0])/det,(a[0][2]*a[1][0]-a[0][0]*a[1][2])/det,0},
  {(a[1][0]*a[2][1]-a[1][1]*a[2][0])/det,(a[0][1]*a[2][0]-a[0][0]*a[2][1])/det,(a[0][0]*a[1][1]-a[0][1]*a[1][0])/det,0},{0,0,0,1}};
 for(int k=0;k<3;k++)inv[3][k]=-(t[0]*inv[0][k]+t[1]*inv[1][k]+t[2]*inv[2][k]);
 double T[4][4];for(int i=0;i<4;i++)for(int k=0;k<4;k++){T[i][k]=0;for(int j=0;j<4;j++)T[i][k]+=inv[i][j]*double(world[j][k]);}
 const int axis[3]={0,2,1};const double sign[3]={1,1,-1};float e[16]={};
 for(int p=0;p<3;p++)for(int q=0;q<3;q++)e[q*4+p]=float(sign[p]*sign[q]*T[axis[q]][axis[p]]);
 for(int p=0;p<3;p++)e[12+p]=float(sign[p]*T[3][axis[p]]/100);
 e[15]=1;out.push_back(float(resource));out.push_back(float(node));out.insert(out.end(),e,e+16);
}
// Draw deltas: count, then per entry resource, node (-1 = the whole instance), 16 floats.
extern "C" EMSCRIPTEN_KEEPALIVE float* set_piece_attached(){
 RIDER_LOCAL static std::vector<float> out;std::vector<float> body;unsigned count=0;
 for(auto& x:attachedLiveComps){
  if(!x->active||!x->drawn)continue;
  const auto* nodes=attached_nodes(*x);if(!nodes)continue;
  for(size_t i=0;i<nodes->size();i++){attached_delta(body,x->resource,int(i),x->rest[i],(*nodes)[i]);++count;}
 }
 for(auto& c:attachedChildren){
  if(!c.active)continue;
  auto* parent=attached_livecomp(c.parent);if(!parent||!parent->active)continue;
  const auto* nodes=attached_nodes(*parent);if(!nodes||size_t(c.node)>=nodes->size())continue;
  const auto& m=ssx::originalParentMatrix(c.modifier,[&]{return &(*nodes)[size_t(c.node)];});
  attached_delta(body,c.child,-1,c.authored,m);++count;
 }
 out.assign(1,float(count));out.insert(out.end(),body.begin(),body.end());return out.data();
}
// Raw bits (capture comparisons): count, then per spline LiveComp: resource, active, time bits, and the node
// matrices (16 words per node); then per child: resource, active, 16 matrix words.
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* set_piece_attached_bits(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,uint32_t(attachedLiveComps.size()));
 for(auto& x:attachedLiveComps){out.push_back(x->resource);out.push_back(x->active);out.push_back(std::bit_cast<uint32_t>(x->live.anim.time));
  const auto* nodes=x->active?attached_nodes(*x):nullptr;out.push_back(nodes?uint32_t(nodes->size()):0u);
  if(nodes)for(const auto& m:*nodes)for(auto& r:m)for(float v:r)out.push_back(std::bit_cast<uint32_t>(v));}
 out.push_back(uint32_t(attachedChildren.size()));
 for(auto& c:attachedChildren){out.push_back(c.child);out.push_back(c.active);
  auto* parent=attached_livecomp(c.parent);const auto* nodes=parent&&parent->active?attached_nodes(*parent):nullptr;
  if(c.active&&nodes&&size_t(c.node)<nodes->size()){const auto& m=ssx::originalParentMatrix(c.modifier,[&]{return &(*nodes)[size_t(c.node)];});for(auto& r:m)for(float v:r)out.push_back(std::bit_cast<uint32_t>(v));}
  else for(unsigned k=0;k<16;k++)out.push_back(0);}
 return out.data();
}
