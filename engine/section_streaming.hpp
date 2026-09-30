#pragma once
// Original SSX3 (PS2 USA, SLUS_207.72) race-event "section" activation and texture-chunk streaming.
// Data: tools/export_sections.py --location L -> web/public/assets/<L>/SECTIONS/sections.json.
//
// 1. Section activation = the proximity activation manager A = *(game+0xA4)
//    (game = *(*(*(gp-0x848)+0x84)+0x0C); ctor 0x1286A0, 0x8E0 bytes; reset 0x103358).
//      +0x00 point count (1 in a race: the human's rider+0x78C vector, 0x11BB40 from the human
//            create 0x129C00; 0x120E30 copies rider+0x110 into it at the end of the rider pass
//            0x121818), +0x10+0x20i point pointer, +0x20+0x20i position at the last scan,
//      +0xD0 tick of the last scan (-1 after 0x103358), +0xD4 parity bit, +0xD8 list count,
//      +0xDC.. listed instances (instance flag 0x100 = listed, 0x200 = parity of the last collect).
//    0x101B60(A) runs once per game tick at the END of the rider manager 0x128AF0 (after every
//    rider's passes 120F20..121950, the RFX updates and 120E88, before game+8++), i.e. after all
//    rider draws of the gameplay RNG in that tick and before the next tick's entity pass:
//      scan if A+0xD0 < 0 || tick - A+0xD0 >= 20 || |pos - last| >= 2000 (VU0, chop).
//      box = pos -/+ 15000 (0x4A61E0).  Octree walk from the 8 roots: a node whose loose cell
//      [(c-0.2)*2^L, (c+1+0.2)*2^L] overlaps the box contributes its whole subtree when L < 14
//      (0x101A28 recurse 1) and otherwise its own instances plus the children (0x144368).
//      Equivalently: an instance in cell C is collected iff collectCell(C) overlaps the box.
//      Collected instances must pass 0x30A310: an entity (+0xC), or the track stage row kind 3
//      and a slot-1 or slot-3 handler (instance +0x88 -> resource08 >> 8 row, 0x3AD120).
//      D0 = tick, last = pos, parity ^= 1, the list is replaced, then
//      leave = old - new, merge-sorted by resource (+0x78, 0x13F8F8), each 0x30A460:
//        entity & modifier vt+0xD0 == 2 (MultiSpline): 0x35AAE0 refcount-- (entity kept)
//        entity type 6 (DeadNode) or no entity: slot-3 program if any
//        other entity: slot-3 program of the entity's instance if any, else vt+0x118
//          (0x34FD90: flags = high half | 2, destroy the entity; type-16 nodes: empty)
//      enter = new - old, sorted by resource, each 0x30A3A0:
//        entity: MultiSpline 0x35AAD0 refcount++, anything else nothing
//        no entity: slot-1 program (0x30A298 -> VM 0x309C88) when the stage row kind is 3
//    Programs run inside 0x101B60: their gameplay-RNG draws (0x317810; builtin3 key 8
//    0x341BB4 / key 6 0x341BE8, builtin19 0x359554, builtin77 0x30366C) happen after all rider
//    draws of the tick, in resource order; entities they create first update in tick+1.
//    Every entity constructor except types 6/16/22 (0x34FB00) inserts an unlisted instance into
//    the list (0x1032C0), e.g. GO doors, trigger pieces; it leaves at the next scan unless it is
//    inside the box.  Cells: 0x328F28 (ssx::originalSpatialCell) of the instance bounds; entities
//    that move are relocated with their entity bounds by 0x3291E0 (MultiSpline: car-0 record
//    +0x30/+0x40; Spline/Position: translation -/+ radius) -> relocate().
//
// 2. Texture chunks (draw eligibility; collision ignores them): W = **(gp+0x16C8),
//    W+0x3F0+24c {state, lock, handle, priority, touch clock, free frame} with states
//    0 free, 1 requested, 2 loading, 3 resident, 4 releasing.  Per frame 0x3A8290:
//      0x3A9258: every resident location requests its locked chunk (0x3A8448 + 0x3A84F8 lock);
//        0x3A9D60 per enabled viewer walks each location's chunk tree (0x3AA028): node box vs
//        viewer box (eye -/+ range), leaf: d = 0x3A9E50(eye, leaf box) (-1 inside), skipped when
//        range < d, request with priority 0.8 (inside) or (1 - d/range)*0.8; child A when
//        planeA.(boxMin,1) < 0, child B when planeB.(boxMax,1) < 0.
//        0x3A8448 request: touch = clock, state 0 -> 1, 4 -> 3.
//      0x3A8668: expired = !lock && clock - touch >= W+0x1BF0 (6); 1: expired -> 0 else candidate
//        (highest priority, first wins); 3: expired -> 4 (free frame = render frame);
//        4: render frame - free frame >= 5 -> 0 (0x3A8528).  Idle loader (W+0x390 == 0) starts the
//        best candidate: state 2.  0x3A7098 completes it: state 3 (latency measured per chunk).
//    Viewer 0 = the OUTER camera: eye = outer+0x20, range = 1.5 * outer+0x08 (far =
//    min(30000, far cap 0x2EE3B8 painter value)), set by 0x15EC98 each frame; W update and viewer
//    of frame T are both visible at capture record T.
//    Draw eligibility: static instances 0x22A5A0: (flags&3)==3 and (+0x7D == 255 or (location
//    state 6 and chunk(+0x7E) resident)); patches 0x22A698: chunk(+0x156) >= 0 and resident.
#include "original_spatial.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <tuple>
#include <vector>

namespace ssx::sections {
using Vec3=std::array<float,3>;
constexpr float kBoxHalf=15000.f,kScanDistance=2000.f,kPriority=0.800000011920929f; // gp-0x23C0 0x3F4CCCCD
constexpr int32_t kScanPeriod=20,kSubtreeLevel=14,kChunkTimeout=6,kChunkFreeFrames=5;
constexpr float kLoose=.20000000298023224f;                                          // gp-0x7F90

struct Cell {int32_t level=11,x=0,y=0,z=0;bool operator==(const Cell&) const=default;bool operator<(const Cell& o) const{return std::tie(level,x,y,z)<std::tie(o.level,o.x,o.y,o.z);}};
inline Cell cellFromBounds(const Vec3& low,const Vec3& high){auto c=originalSpatialCell(low,high);return {c.level,c.coordinate[0],c.coordinate[1],c.coordinate[2]};}
// The node whose overlap decides collection: the cell itself at level >= 14, else its level-13 ancestor.
inline Cell collectCell(Cell c){if(c.level>=kSubtreeLevel)return c;const int s=kSubtreeLevel-1-c.level;return {kSubtreeLevel-1,c.x>>s,c.y>>s,c.z>>s};}
// 0x101B60 / 0x144368 loose-cell vs box test (EE sub.s/add.s with the guard bit, mul.s chop).
inline bool looseOverlaps(const Cell& c,const Vec3& boxMin,const Vec3& boxMax){
    using namespace terrain_original;Rounding rounding;const float size=std::bit_cast<float>(uint32_t(c.level+127)<<23);const int32_t k[3]={c.x,c.y,c.z};
    for(unsigned a=0;a<3;++a){
        const float lo=mul(originalScalarSubtract(float(k[a]),kLoose),size),hi=mul(originalScalarAdd(float(k[a]+1),kLoose),size);
        if(!(lo<=boxMax[a]&&boxMin[a]<=hi))return false;
    }
    return true;
}
inline void scanBox(const Vec3& p,Vec3& lo,Vec3& hi){using namespace terrain_original;Rounding rounding;for(unsigned a=0;a<3;++a){lo[a]=sub(p[a],kBoxHalf);hi[a]=add(p[a],kBoxHalf);}}

enum class EntityKind:uint8_t {None=0,Plain=1,Dead=2,Type16=3,MultiSpline=4};
enum class Action:uint8_t {None=0,Slot1=1,Slot3=2,Destroy=3,MultiSplineAcquire=4,MultiSplineRelease=5};
inline EntityKind entityKindFromName(const std::string& s){
    if(s=="plain")return EntityKind::Plain;if(s=="dead")return EntityKind::Dead;if(s=="type16")return EntityKind::Type16;
    if(s=="multispline")return EntityKind::MultiSpline;return EntityKind::None;
}
struct Instance {
    uint32_t resource=0;Cell cell{};int32_t slot1=-1,slot3=-1; // stage programs (-1 none)
    bool stage=true;                                            // track stage row kind 3 (the slots count)
    EntityKind entity=EntityKind::None;                          // live entity class (initial: ready savestate)
    bool listed=false,parity=false;                              // instance flags 0x100 / 0x200
};
// A stage program's effect as far as activation and the gameplay RNG are concerned.
struct Program {
    EntityKind creates=EntityKind::None;           // entity left on the running instance (observed in the snapshots)
    uint32_t guard=0;bool guardSelf=false;         // `if builtin52(guard) == 1 return` (target has an entity)
    struct Draw {bool liveComp=false;uint32_t target=0;};   // target 0 = the running instance
    std::vector<Draw> draws;                        // builtin3 key8/key6 (only when it constructs), builtin19, builtin77
};
// guarded: the slot-1 program returned at its `if builtin52(guard) == 1` head (0x303130: the target's +0xC entity is set), so
// nothing after it runs (BHP1 fencecollision_1001 / CHP2 96279: the resident blimp's rebuild programs 39 / 82).
struct Event {uint32_t resource=0;bool enter=false;Action action=Action::None;int32_t program=-1;uint8_t rngDraws=0;bool guarded=false;};

// 0x101B60 state.  Instances are kept sorted by resource.
struct Activation {
    std::vector<Instance> instances;std::vector<uint32_t> list;   // list = indices in list order
    std::map<int32_t,Program> programs;
    int32_t lastScan=-1;Vec3 last{};bool parity=true;             // +0xD0, +0x20, +0xD4 (ctor 1)
    bool rescan=true;                                             // +0xD0 == -1 (a seeded lastScan may be negative)
    // Points after the human's (A+0 count > 1): a NIS director's camera point (0x281370 -> 0x1033B0 adds director+0x100; 0x281100
    // copies the outer camera's +0x20, the rendered eye, into it each tick; 0x281400 -> 0x1033F8 removes it). 101B60 builds a box per
    // point, scans when any point moved >= 2000 cm since the last scan, and collects an instance whose cell meets any box
    // (PS2 c0a-snap: A+0 2 under the Snow Jam fly-over, 42 more pieces listed at 2854).
    struct Point {Vec3 pos{},last{};};
    std::vector<Point> points;
    // Streamed worlds (Peak 1, web/peak_world.inc): per track (resource & 0xFF) whether its instances are in the octree
    // (read completion 328C20 .. eviction 3284B8); null = every instance (one race location).
    const uint8_t* present=nullptr;

    Instance* find(uint32_t resource){auto it=std::lower_bound(instances.begin(),instances.end(),resource,[](const Instance& a,uint32_t r){return a.resource<r;});return it!=instances.end()&&it->resource==resource?&*it:nullptr;}
    void sort(){std::sort(instances.begin(),instances.end(),[](const Instance& a,const Instance& b){return a.resource<b.resource;});}
    bool eligible(const Instance& i) const{return i.entity!=EntityKind::None||(i.stage&&(i.slot1>=0||i.slot3>=0));}
    // 0x103358 (race start / restart): unflag the list, rescan on the next update; entities stay.
    void reset(){for(uint32_t k:list)instances[k].listed=false;list.clear();lastScan=-1;rescan=true;}
    // Scan condition of 0x101B60 (pos = the human's rider+0x110 after this tick's rider physics).
    bool due(int32_t tick,const Vec3& pos) const{
        if(rescan||tick-lastScan>=kScanPeriod)return true;
        using namespace terrain_original;Rounding rounding;float s=0;
        for(unsigned a=0;a<3;++a){const float d=sub(last[a],pos[a]);s=add(s,mul(d,d));}
        if(kScanDistance<=sqrt(s))return true;
        for(const auto& p:points){float t=0;for(unsigned a=0;a<3;++a){const float d=sub(p.last[a],p.pos[a]);t=add(t,mul(d,d));}if(kScanDistance<=sqrt(t))return true;}
        return false;
    }
    // 0x1033B0 / 0x1033F8: a point added (its last = where it is) or removed; either forces a scan (+0xD0 = -1).
    void addPoint(const Vec3& pos){points.push_back({pos,pos});rescan=true;}
    void removePoint(){if(!points.empty()){points.pop_back();rescan=true;}}
    // 0x101B60: returns false when no scan ran.  Events: leave first, then enter, each sorted by
    // resource; the caller runs them in this order (slot programs, destroys) at the end of the tick.
    bool update(int32_t tick,const Vec3& pos,std::vector<Event>& events){
        events.clear();if(!due(tick,pos))return false;
        Vec3 lo,hi;scanBox(pos,lo,hi);
        std::vector<std::pair<Vec3,Vec3>> more;for(const auto& p:points){Vec3 a,b;scanBox(p.pos,a,b);more.push_back({a,b});}
        const auto meets=[&](const Cell& c){if(looseOverlaps(c,lo,hi))return true;for(const auto& [a,b]:more)if(looseOverlaps(c,a,b))return true;return false;};
        std::map<Cell,bool> tested;std::vector<uint32_t> collected;
        for(uint32_t k=0;k<instances.size();++k){
            const Instance& i=instances[k];if(!eligible(i)||(present&&!present[i.resource&255u]))continue;
            const Cell c=collectCell(i.cell);auto t=tested.find(c);
            const bool in=t!=tested.end()?t->second:tested.emplace(c,meets(c)).first->second;
            if(in)collected.push_back(k);
        }
        lastScan=tick;last=pos;parity=!parity;rescan=false;for(auto& p:points)p.last=p.pos;
        std::vector<uint32_t> enter,leave;
        for(uint32_t k:collected){Instance& i=instances[k];if(!i.listed){enter.push_back(k);i.listed=true;}i.parity=!parity;}
        for(uint32_t k:list){Instance& i=instances[k];if(i.parity==parity){leave.push_back(k);i.listed=false;}}
        list=collected;  // index order = resource order already (instances are sorted)
        for(uint32_t k:leave){  // 0x30A460
            Instance& i=instances[k];Event e;e.resource=i.resource;e.enter=false;
            if(i.entity==EntityKind::MultiSpline)e.action=Action::MultiSplineRelease;
            else if(i.entity==EntityKind::None||i.entity==EntityKind::Dead||i.entity==EntityKind::Type16||(i.stage&&i.slot3>=0)){
                if(i.stage&&i.slot3>=0){e.action=Action::Slot3;e.program=i.slot3;}
            }else{e.action=Action::Destroy;i.entity=EntityKind::None;}
            events.push_back(e);
        }
        for(uint32_t k:enter){  // 0x30A3A0
            Instance& i=instances[k];Event e;e.resource=i.resource;e.enter=true;
            if(i.entity!=EntityKind::None){if(i.entity==EntityKind::MultiSpline)e.action=Action::MultiSplineAcquire;}
            else if(i.stage&&i.slot1>=0){e.action=Action::Slot1;e.program=i.slot1;e.guarded=guarded(i,i.slot1);e.rngDraws=runProgram(i,i.slot1);}
            events.push_back(e);
        }
        return true;
    }
    // Slot-1 program bookkeeping: gameplay-RNG draws (in program order) and the entity left on the
    // running instance.  A builtin3 LiveComp on another instance also inserts it (0x34FB00).
    bool guarded(Instance& self,int32_t id){
        auto it=programs.find(id);if(it==programs.end())return false;const Program& p=it->second;
        if(!p.guardSelf&&!p.guard)return false;
        Instance* g=p.guardSelf?&self:find(p.guard);return g&&g->entity!=EntityKind::None;
    }
    uint8_t runProgram(Instance& self,int32_t id){
        auto it=programs.find(id);if(it==programs.end())return 0;const Program& p=it->second;
        const uint32_t selfResource=self.resource;
        if(guarded(self,id))return 0;
        uint8_t n=0;
        for(const auto& d:p.draws){
            if(!d.liveComp){++n;continue;}
            Instance* t=d.target?find(d.target):find(selfResource);
            if(t&&t->entity!=EntityKind::None&&t->resource!=selfResource)continue;   // 0x2FAE38 reuses the existing entity
            ++n;if(t&&t->resource!=selfResource)entityCreated(t->resource,EntityKind::Plain);
        }
        if(Instance* me=find(selfResource))me->entity=p.creates;
        return n;
    }
    // 0x34FB00 -> 0x1032C0: an entity was constructed on `resource` outside the enter handling of this
    // instance (GO doors, trigger programs, programs acting on other instances).  Returns true when it
    // was inserted into the list (types 6/16/22 are not).
    // cell: for an instance outside the activation octree (triggers, timers, emitter points, spline carriers): the
    // entity construction places it in the octree by its bounds (0x328F28), so later scans collect it (or it leaves).
    bool entityCreated(uint32_t resource,EntityKind kind,const Cell* cell=nullptr){
        Instance* i=find(resource);
        if(!i&&cell){ // sorted insert; list entries hold instance indices
            auto it=std::lower_bound(instances.begin(),instances.end(),resource,[](const Instance& a,uint32_t r){return a.resource<r;});
            const uint32_t at=uint32_t(it-instances.begin());for(uint32_t& k:list)if(k>=at)++k;
            Instance fresh;fresh.resource=resource;fresh.cell=*cell;fresh.slot1=fresh.slot3=-1;fresh.stage=false;
            i=&*instances.insert(it,fresh);
        }
        if(!i)return false;i->entity=kind;
        if(i->listed||kind==EntityKind::Dead||kind==EntityKind::Type16)return false;
        i->listed=true;i->parity=!parity;list.push_back(uint32_t(i-instances.data()));return true;
    }
    // A program removed the entity (timers finishing, builtin16 conversions to other nodes, ...).
    void entityDestroyed(uint32_t resource){if(Instance* i=find(resource))i->entity=EntityKind::None;}
    // 0x3291E0: an entity-routed instance moved (pass the entity bounds of this tick's entity pass).
    void relocate(uint32_t resource,const Vec3& low,const Vec3& high){if(Instance* i=find(resource))i->cell=cellFromBounds(low,high);}
    bool isListed(uint32_t resource){Instance* i=find(resource);return i&&i->listed;}
    // 0x103308 (230360, streamer row 5 -> 7): a location's instances leave the list without leave handlers, and the
    // location's entities go with its teardown (a later load starts the instances without entities).
    void dropTrack(uint32_t track){
        std::vector<uint32_t> kept;for(uint32_t k:list){if((instances[k].resource&255u)==track)instances[k].listed=false;else kept.push_back(k);}
        list=std::move(kept);for(auto& i:instances)if((i.resource&255u)==track)i.entity=EntityKind::None;
    }
};

// ---- texture chunks ---------------------------------------------------------------------------------
struct ChunkNode {Vec3 min{},max{};std::array<float,4> planeA{},planeB{};int32_t childA=-1,childB=-1,chunk=-1;};
struct ChunkTree {int32_t track=0,lockedChunk=0;std::vector<ChunkNode> nodes;};
struct ChunkState {uint8_t state=0;bool locked=false;float priority=0;int32_t touch=0,freeFrame=0,latency=135;};
// 0x3A9E50: -1 inside, else the Euclidean distance to the box (EE sub.s/add.s guard bit, mul.s chop, sqrt.s nearest).
inline float chunkDistance(const Vec3& p,const Vec3& lo,const Vec3& hi){
    using namespace terrain_original;Rounding rounding;bool inside=true;float s=0;
    for(unsigned a=0;a<3;++a){
        if(lo[a]<=p[a]&&p[a]<=hi[a])continue;inside=false;
        float d=originalScalarSubtract(p[a],hi[a]);if(d<0)d=originalScalarSubtract(lo[a],p[a]);
        s=originalScalarAdd(s,mul(d,d));
    }
    return inside?-1.f:originalScalarSqrt(s);
}
struct ChunkStreaming {
    std::vector<ChunkTree> trees;std::map<int32_t,ChunkState> chunks;int32_t loading=-1,loadDone=0;
    int32_t timeout=kChunkTimeout,freeFrames=kChunkFreeFrames;
    void request(int32_t chunk,float priority,int32_t clock){   // 0x3A8448
        auto it=chunks.find(chunk);if(it==chunks.end())return;ChunkState& c=it->second;c.priority=priority;c.touch=clock;
        if(c.state==0)c.state=1;else if(c.state==4)c.state=3;
    }
    void traverse(const ChunkTree& t,int32_t k,const Vec3& eye,float range,const Vec3& lo,const Vec3& hi,int32_t clock){   // 0x3AA028
        const ChunkNode& n=t.nodes[size_t(k)];
        for(unsigned a=0;a<3;++a)if(!(lo[a]<=n.max[a]&&n.min[a]<=hi[a]))return;
        if(n.chunk>=0){
            using namespace terrain_original;Rounding rounding;const float d=chunkDistance(eye,n.min,n.max);float priority=kPriority;
            if(!(d<0)){if(range<d)return;priority=mul(originalScalarSubtract(1.f,originalScalarDivide(d,range)),kPriority);}
            request(n.chunk,priority,clock);
        }
        auto dot=[](const std::array<float,4>& p,const Vec3& v){using namespace terrain_original;Rounding rounding;return add(add(add(mul(p[0],v[0]),mul(p[1],v[1])),mul(p[2],v[2])),p[3]);};
        if(n.childA>=0&&dot(n.planeA,lo)<0)traverse(t,n.childA,eye,range,lo,hi,clock);
        if(n.childB>=0&&dot(n.planeB,hi)<0)traverse(t,n.childB,eye,range,lo,hi,clock);
    }
    // One 0x3A8290 update: clock = game clock (1 per tick in the race), frame = render frame counter
    // (equal to the clock unless the console drops frames), eye/range = outer camera +0x20 / 1.5*+0x08.
    void frame(int32_t clock,int32_t renderFrame,const Vec3& eye,float range){
        for(auto& [id,c]:chunks)if(c.locked){c.touch=clock;if(c.state==0)c.state=1;else if(c.state==4)c.state=3;}
        Vec3 lo,hi;{using namespace terrain_original;Rounding rounding;for(unsigned a=0;a<3;++a){lo[a]=sub(eye[a],range);hi[a]=add(eye[a],range);}}
        for(const auto& t:trees)if(!t.nodes.empty())traverse(t,0,eye,range,lo,hi,clock);
        int32_t best=-1;float bestPriority=-1.f;   // 0x3A8668
        for(auto& [id,c]:chunks){
            if(!c.state)continue;const bool expired=!c.locked&&clock-c.touch>=timeout;
            if(c.state==1){if(expired)c.state=0;else if(bestPriority<c.priority){bestPriority=c.priority;best=id;}}
            else if(c.state==3){if(expired){c.state=4;c.freeFrame=renderFrame;}}
            else if(c.state==4){if(renderFrame-c.freeFrame>=freeFrames)c.state=0;}
        }
        if(best>=0&&loading<0){chunks[best].state=2;loading=best;loadDone=clock+chunks[best].latency;}
        if(loading>=0&&clock>=loadDone){chunks[loading].state=3;loading=-1;}   // 0x3A7098 (latency model)
    }
    bool resident(int32_t chunk) const{auto it=chunks.find(chunk);return it!=chunks.end()&&it->second.state==3;}
    // 0x22A5A0 static instances (track +0x7D, chunk +0x7E); the (flags & 3) == 3 test is the caller's.
    bool instanceDrawable(uint32_t track,int32_t chunk) const{return track==255||resident(chunk);}
    // 0x22A698 patches (+0x155 track, +0x156 chunk).
    bool patchDrawable(int32_t chunk) const{return chunk>=0&&resident(chunk);}
};

// Loader for nlohmann::json (web/json.hpp) documents written by tools/export_sections.py.
template<class Json> inline void loadSections(const Json& doc,Activation& activation,ChunkStreaming* chunks=nullptr){
    activation=Activation{};
    for(auto it=doc["programs"].begin();it!=doc["programs"].end();++it){
        const auto& j=it.value();Program p;
        if(j.contains("creates")&&!j["creates"].is_null())p.creates=entityKindFromName(j["creates"].template get<std::string>());
        if(j.contains("guard")&&!j["guard"].is_null()){if(j["guard"].is_string())p.guardSelf=true;else p.guard=j["guard"].template get<uint32_t>();}
        const auto& kinds=j["rng_draw_sources"];const auto& targets=j["rng_draw_targets"];
        for(size_t k=0;k<kinds.size();++k){Program::Draw d;d.liveComp=kinds[k].template get<std::string>().rfind("builtin3",0)==0;d.target=targets[k].is_string()?0u:targets[k].template get<uint32_t>();p.draws.push_back(d);}
        activation.programs[std::stoi(it.key())]=p;
    }
    for(const auto& j:doc["instances"]){
        Instance i;i.resource=j["resource"].template get<uint32_t>();const auto& c=j["cell"];
        i.cell={c[0].template get<int32_t>(),c[1].template get<int32_t>(),c[2].template get<int32_t>(),c[3].template get<int32_t>()};
        i.slot1=j["slot1"].template get<int32_t>();i.slot3=j["slot3"].template get<int32_t>();i.stage=true;
        i.entity=j["entity_at_start"].is_null()?EntityKind::None:entityKindFromName(j["entity_at_start"].template get<std::string>());
        activation.instances.push_back(i);
    }
    activation.sort();
    if(!chunks)return;
    *chunks=ChunkStreaming{};
    for(const auto& c:doc["chunks"]){
        ChunkState s;s.locked=c["locked"].template get<bool>();s.state=c["resident_at_start"].template get<bool>()?3:0;
        if(!c["load_latency_ticks"].empty())s.latency=c["load_latency_ticks"][0].template get<int32_t>();
        chunks->chunks[c["id"].template get<int32_t>()]=s;
    }
    for(const auto& t:doc["chunk_trees"]){
        ChunkTree tree;tree.track=t["track"].template get<int32_t>();tree.lockedChunk=t["locked_chunk"].template get<int32_t>();
        for(const auto& n:t["nodes"]){
            ChunkNode node;for(unsigned a=0;a<3;++a){node.min[a]=n["min"][a].template get<float>();node.max[a]=n["max"][a].template get<float>();}
            for(unsigned a=0;a<4;++a){node.planeA[a]=n["plane_a"][a].template get<float>();node.planeB[a]=n["plane_b"][a].template get<float>();}
            node.childA=n["child_a"].template get<int32_t>();node.childB=n["child_b"].template get<int32_t>();node.chunk=n["chunk"].template get<int32_t>();
            tree.nodes.push_back(node);
        }
        chunks->trees.push_back(std::move(tree));
    }
}
}
