#include "rider_local.hpp"
#include "rail_bridge.hpp"
#include "../engine/rail_modifier.hpp"
#include "generated/rail_teeter_seed.hpp"
#include <span>
#include "json.hpp"
#include <emscripten/emscripten.h>
#include <unordered_set>
#include <unordered_map>
using nlohmann::json;
namespace {
RIDER_LOCAL std::vector<ssx::OriginalRailRecord> records;RIDER_LOCAL unsigned segmentCount=0;RIDER_LOCAL bool ready=false;
void finite(ssx::RailVector value){for(float x:value)if(!std::isfinite(x))throw std::runtime_error("Nonfinite original rail geometry");}
}
// Snow Jam log teeters (engine/rail_modifier.hpp, generated/rail_teeter_seed.hpp): programs 65..68 (slot 1)
// build an AnimTeeter (builtin6 0x3421A0) and RailModifiers (builtin48 0x35B708) that bind rails to
// model node 1. While bound, the rail's static segments leave the world tree and 0x334680 answers
// it through the modifier's type-2 layer (0x35C698) with the node transform, after the static
// layers. The browser binds them for the whole race (the original binds on section activation;
// queries are local, so only the rest transform's rounding differs away from the log).
namespace {
struct BrowserTeeter {const BrowserRailTeeterSeed* seed=nullptr;ssx::OriginalAnimModel model;ssx::OriginalAnimTeeter entity;
 std::vector<std::pair<ssx::OriginalRailModifier,const ssx::OriginalRailRecord*>> rails;};
RIDER_LOCAL std::vector<BrowserTeeter> teeters;RIDER_LOCAL std::vector<ssx::OriginalRailRecord> staticRecords;RIDER_LOCAL unsigned teeterForces=0,teeterTicks=0;RIDER_LOCAL bool teeterCourse=false; /* ARA1 only */
RIDER_LOCAL bool staticActive=false;RIDER_LOCAL std::string railLocation; /* other locations: staticRecords answers the static layers while run-time rails are bound (web/rail_dynamic.inc) */
float bitsF(uint32_t b){return std::bit_cast<float>(b);}
ssx::RollerMatrix matrixBits(const std::array<uint32_t,16>& w){ssx::RollerMatrix r;for(unsigned i=0;i<16;i++)r[i/4][i%4]=bitsF(w[i]);return r;}
void bind_teeters(){
 teeters.clear();staticRecords.clear();teeterForces=teeterTicks=0;
 std::unordered_set<uint32_t> bound;
 for(const auto& s:browserRailTeeterSeeds){
  BrowserTeeter t;t.seed=&s;t.model.length=bitsF(s.length);
  for(uint32_t i=0;i<s.nodeCount;i++){
   const auto& n=browserRailTeeterNodes[s.firstNode+i];ssx::OriginalAnimNode node;node.parent=n.parent;node.bind=matrixBits(n.bind);
   if(n.animated){ssx::OriginalAnimTrack track;for(unsigned k=0;k<6;k++)track.base[k]=bitsF(n.base[k]);track.mask=n.mask;
    for(uint32_t c=0;c<n.curveCount;c++){const auto& cv=browserRailTeeterCurves[n.firstCurve+c];std::vector<ssx::OriginalAnimSegment> segs;
     for(uint32_t k=0;k<cv.count;k++){const auto& g=browserRailTeeterSegments[cv.first+k];segs.push_back({bitsF(g.a),bitsF(g.b),bitsF(g.c),bitsF(g.d),bitsF(g.t0),bitsF(g.t1)});}
     track.curves.push_back(std::move(segs));}
    node.track=std::move(track);}
   t.model.nodes.push_back(std::move(node));
  }
  teeters.push_back(std::move(t));
 }
 for(auto& t:teeters){ // after the vector is final: the entity keeps a pointer to its model
  const auto& s=*t.seed;const auto& a=s.teeterArgs;
  ssx::OriginalAnimTeeterArgs args{bitsF(a[1]),bitsF(a[2]),bitsF(a[3]),bitsF(a[4]),bitsF(a[5]),bitsF(a[6]),a[7],bitsF(a[8]),bitsF(a[9])};
  const auto matrix=matrixBits(s.matrix);const float scale=bitsF(s.scale);
  t.entity=ssx::originalAnimTeeterConstruct(t.model,matrix,scale,args);
  std::array<float,3> low{bitsF(s.boundsMin[0]),bitsF(s.boundsMin[1]),bitsF(s.boundsMin[2])},high{bitsF(s.boundsMax[0]),bitsF(s.boundsMax[1]),bitsF(s.boundsMax[2])};
  for(uint32_t r=0;r<s.railCount;r++){
   const auto& rail=browserRailTeeterRails[s.firstRail+r];const ssx::OriginalRailRecord* record=nullptr;
   for(const auto& x:records)if(x.packedId==rail.packedId)record=&x;
   if(!record)continue; /* not this location's rail */
   t.rails.push_back({ssx::originalRailModifierConstruct(rail.packedId,rail.node,s.resource,t.model,matrix,scale,low,high),record});bound.insert(rail.packedId);
  }
 }
 for(const auto& r:records)if(!bound.count(r.packedId))staticRecords.push_back(r);
 browser_rail_livecomps_reset();
}
}
void browser_dynamic_rails_query(ssx::RailVector point,uint32_t mask,bool& found,float& best,ssx::OriginalRailModifierHit& hit,bool& modifierHit); // web/rail_dynamic.inc
namespace {
// The static segments in 0x334680's visiting order (engine/rail_motion.hpp originalRailWalkOrder: the octree), one cache per
// record set (all records / the static ones while rails are bound), rebuilt when the set changes (streamed appends, binds).
// Each cache also keeps the walked segments' bounds in walk order (OriginalRailWalkBox, docs/sim-performance.md "Rail query"):
// the query scans that compact copy (32 B per segment, in order) instead of the 112 B segments in octree order. Segment bounds
// are never edited in place (a change replaces the record set: new segment buffers, whose addresses are in the key); eight
// sampled boxes are compared with their segments on every query as well.
struct RailWalkCache {std::vector<ssx::OriginalRailWalkEntry> order;std::vector<ssx::OriginalRailWalkBox> boxes;uint64_t key=0;};
RIDER_LOCAL RailWalkCache walkAll,walkStatic;
const RailWalkCache& walk_order(std::span<const ssx::OriginalRailRecord> set,RailWalkCache& cache){
 uint64_t k=1469598103934665603ull^uint64_t(set.size());
 for(const auto& r:set){k=(k^r.packedId)*1099511628211ull;k=(k^uint64_t(r.segments.size()))*1099511628211ull;k=(k^uint64_t(uintptr_t(r.segments.data())))*1099511628211ull;}
 bool fresh=k==cache.key&&!cache.order.empty()&&cache.boxes.size()==cache.order.size();
 for(size_t s=0;fresh&&s<8;++s){const size_t i=s*(cache.order.size()-1)/7;const auto& e=cache.order[i];const auto& g=set[e.record].segments[e.segment];
  fresh=cache.boxes[i].boundsMin==g.boundsMin&&cache.boxes[i].boundsMax==g.boundsMax;}
 if(!fresh){
  cache.order=ssx::originalRailWalkOrder(set);cache.key=k;cache.boxes.clear();cache.boxes.reserve(cache.order.size());
  for(const auto& e:cache.order){const auto& g=set[e.record].segments[e.segment];cache.boxes.push_back({g.boundsMin,g.boundsMax});}
 }
 return cache;
}
// 0x334680: static layers, then the bound rails' type-2 layers (strictly closer replaces).
ssx::OriginalRailQueryResult teeterQuery(ssx::RailVector point,uint32_t mask){
 const bool all=teeters.empty()&&!(staticActive&&!staticRecords.empty());
 const std::span<const ssx::OriginalRailRecord> set=all?std::span<const ssx::OriginalRailRecord>(records):std::span<const ssx::OriginalRailRecord>(staticRecords);
 const auto& walk=walk_order(set,all?walkAll:walkStatic);
 auto result=ssx::originalRailWorldQuery(set,walk.order,walk.boxes,point,300,mask);
 bool found=result.found;float best=result.distance;ssx::OriginalRailModifierHit hit;bool modifierHit=false;
 const ssx::RailVector extent{300,300,300};
 for(auto& t:teeters)for(auto& [m,record]:t.rails){
  bool overlaps=true;for(unsigned k=0;k<3;k++)overlaps&=m.boundsMin[k]<point[k]+extent[k]&&point[k]-extent[k]<m.boundsMax[k];
  if(!overlaps)continue; // the rider's nearby box (approximated by the 300 cm query box)
  bool before=found;float previous=best;
  ssx::originalRailModifierQuery(m,*record,ssx::originalRailModifierTransform(m,t.entity),{point[0],point[1],point[2],1.f},mask,found,best,hit);
  if(found&&(!before||best!=previous))modifierHit=true;
 }
 browser_rail_livecomps_query(point,mask,found,best,hit,modifierHit); /* falling billboard rails (web/falling_billboard.inc) */
 browser_dynamic_rails_query(point,mask,found,best,hit,modifierHit); /* run-time rails of other locations (web/rail_dynamic.inc) */
 if(modifierHit)ssx::originalRailModifierApply(hit,best,result);
 return result;
}
}
ssx::OriginalRailQueryResult browserRailQuery(ssx::RailVector point){
 if(!ready)return {};finite(point);return teeterQuery(point,1);
}
//0x107578 handplant probe: 0x334680(world,probe,out,mask2,300).
ssx::OriginalRailQueryResult browserHandplantQuery(ssx::RailVector point){
 if(!ready)return {};finite(point);return teeterQuery(point,2);
}
// Entity pass (before the rider): 0x342358 per teeter (dt 1/60).
void browser_advance_teeters(){for(auto& t:teeters)ssx::originalAnimTeeterUpdate(t.entity,std::bit_cast<float>(0x3C888889u));if(!teeters.empty())++teeterTicks;browser_rail_livecomps_advance();}
void browser_rails_reserve(size_t count){records.reserve(count);} // streamed locations (web/peak_world.inc)
// pv peakRelease (web/peak_world.inc release at T+7): a streamed location's grind / handplant rails (flags != 0) leave the world tree
// (3AB798 unlinks every segment); the record stays (held pointers) without segments until a re-read fills it (init_rails append).
// Set-piece spline paths (flags 0) stay: the location's MultiSplines / splines keep evaluating them. Returns the segments released.
namespace {RIDER_LOCAL std::vector<uint32_t> releasedRails;RIDER_LOCAL std::array<uint8_t,256> releasedRailTracks{};} // packed ids of released records (sorted); tracks released once
void browser_static_records_rebuild(); // web/rail_dynamic.inc
bool browserRailReleased(const ssx::OriginalRailRecord& r){return std::binary_search(releasedRails.begin(),releasedRails.end(),r.packedId);}
size_t browser_rails_release_track(uint32_t track){
 if(!ready)return 0;size_t n=0;
 for(auto& r:records)if((r.packedId&255u)==track&&r.flags!=0&&!r.segments.empty()){n+=r.segments.size();std::vector<ssx::OriginalRailSegment>().swap(r.segments);releasedRails.insert(std::lower_bound(releasedRails.begin(),releasedRails.end(),r.packedId),r.packedId);}
 for(auto& r:staticRecords)if((r.packedId&255u)==track&&browserRailReleased(r))std::vector<ssx::OriginalRailSegment>().swap(r.segments);
 releasedRailTracks[track&255u]=1;segmentCount-=unsigned(n);return n;
}
void browser_dynamic_rails_reset(const std::string& location); // web/rail_dynamic.inc
void browser_reset_teeters(){if(ready&&teeterCourse)bind_teeters();else{teeters.clear();staticRecords.clear();browser_rail_livecomps_reset();if(ready)browser_dynamic_rails_reset(railLocation);}}
// 0x106848 attach onto a bound rail: entity vtable+0x15C 0x342538 with F = v_before - v_after.
void shared_world_log(std::initializer_list<uint32_t>); // web/shared_world.inc (the log of the shared world)
void browser_teeter_attach_force(const ssx::OriginalRailQueryResult& hit,const ssx::RailVector& before,const ssx::RailVector& after){
 if(!hit.record)return;
 for(unsigned ti=0;ti<teeters.size();ti++){auto& t=teeters[ti];for(auto& [m,record]:t.rails)if(record==hit.record){
  ssx::RollerQuad force;{ssx::OriginalRounding rounding;for(unsigned k=0;k<3;k++)force[k]=ssx::terrain_original::sub(before[k],after[k]);force[3]=0;}
  auto b=[](float x){return std::bit_cast<uint32_t>(x);};
  shared_world_log({3u,ti,uint32_t(m.node),b(hit.point[0]),b(hit.point[1]),b(hit.point[2]),b(force[0]),b(force[1]),b(force[2]),b(force[3])});
  if(ssx::originalAnimTeeterApplyForce(t.entity,m.node,{hit.point[0],hit.point[1],hit.point[2],1.f},force))++teeterForces;
  return;
 }}
}
// The same log shoved by another rider (web/shared_world.inc replay).
void browser_teeter_apply_force(unsigned teeter,unsigned node,const std::array<float,3>& point,const ssx::RollerQuad& force){
 if(teeter>=teeters.size())return;
 if(ssx::originalAnimTeeterApplyForce(teeters[teeter].entity,node,{point[0],point[1],point[2],1.f},force))++teeterForces;
}
// Per teeter: resource and the node-1 transform T (static world -> current world, row vectors).
std::vector<std::pair<uint32_t,ssx::RollerMatrix>> browser_teeter_transforms(){
 std::vector<std::pair<uint32_t,ssx::RollerMatrix>> out;
 for(auto& t:teeters)if(!t.rails.empty())out.push_back({t.seed->resource,ssx::originalRailModifierTransform(t.rails.front().first,t.entity)});
 return out;
}
std::array<float,4> browser_teeter_info(){float angle=0;for(const auto& t:teeters)angle=std::max(angle,std::abs(t.entity.time));return {float(teeters.size()),float(teeterTicks),float(teeterForces),angle};}
// Authored spline record by packed id (set-piece path followers, web/set_piece_gameplay.inc).
const ssx::OriginalRailRecord* browserRailRecord(uint32_t packedId){if(!ready)return nullptr;for(const auto& r:records)if(r.packedId==packedId)return &r;return nullptr;}
// init_rails' tail: the location's teeters (Snow Jam), or its static records and run-time rails.
static void rails_loaded(const std::string& location){
 teeterCourse=location=="ARA1";railLocation=location;if(teeterCourse)bind_teeters();else{teeters.clear();staticRecords.clear();browser_rail_livecomps_reset();browser_dynamic_rails_reset(railLocation);}
}
// The catalog of an event loaded in parts (rails_seal), shared by the rider contexts like world_bridge.cpp's parse cache.
struct RailParse {std::string key,location;std::vector<ssx::OriginalRailRecord> records;unsigned segments=0;};
static RailParse railParseCache;
void browser_rails_parse_cache_clear(){railParseCache={};}
extern "C" {
EMSCRIPTEN_KEEPALIVE void init_rails(const char* text,const char* worldHash){
 const auto data=json::parse(text);
 if(data.at("version")!=1||!data.at("location").is_string()||data.at("source_record_kind")!=8||data.at("source_sha256")!=worldHash)throw std::runtime_error("Original rail/course provenance mismatch");
 std::vector<ssx::OriginalRailRecord> next;std::unordered_set<unsigned> ids;std::unordered_set<int64_t> segments;unsigned total=0;std::unordered_map<unsigned,int> trackSegments;
 // pv sliceLoad (web/peak-world-batches.js railBatches): a streamed location's catalog in parts. A part carries the location's segment
 // count per track before it (segment_base: the +0x60/+0x64 indices go on across the parts) and all but the last are "partial": the
 // teeter / static-record pass after an append runs once, after the last part, so the result is the whole catalog's (rail_load_hash).
 if(data.contains("segment_base"))for(const auto& [k,v]:data["segment_base"].items())trackSegments[unsigned(std::stoul(k))]=v.get<int>();
 const bool partial=data.value("partial",false);
 for(const auto& item:data.at("rails")){
  ssx::OriginalRailRecord record;record.packedId=item.at("packed_id");record.surface=item.contains("runtime_surface")?item.at("runtime_surface").get<int>():item.at("header_words").at("surface_id").get<int>(); //+0x28: -1 on disc, the loader patches it (10 metal, 9 wood; tools/export_rail_runtime_flags.py) -> rider+0x438
  //record+0x1C runtime query flags: bit0 rail, bit1 handplant; set-piece paths are 0 (tools/export_rail_runtime_flags.py).
  record.flags=item.at("runtime_flags").get<uint32_t>();/* 0x30003 rail+handplant, 0x20002 handplant only (BHP1 coping, tools/export_rail_runtime_flags.py), 0x10001 rail only (28 ASS1 splines; bit0 rail / bit1 handplant query masks) */if(record.flags!=0&&record.flags!=0x30003&&record.flags!=0x20002&&record.flags!=0x10001)throw std::runtime_error("Unverified original rail runtime flags");
  if(!ids.insert(record.packedId).second)throw std::runtime_error("Duplicate original rail id");
  for(const auto& part:item.at("segments")){
   ssx::OriginalRailSegment segment;if(part.at("index").get<size_t>()!=record.segments.size())throw std::runtime_error("Rail local index mismatch");segment.index=trackSegments[record.packedId&255]++; /*authored +0x60/+0x64 links index the segments of one location (resource track)*/segment.previous=part.at("previous_segment");segment.next=part.at("next_segment");segment.length=part.at("length_cm");segment.distance=part.at("distance_cm");segment.flags=part.at("flags");
   const auto& source=part.at("source");if(source.at("units")!="centimeters"||source.at("up_axis")!="Z")throw std::runtime_error("Original rail coordinate convention mismatch");
   segment.coefficients=source.at("coefficients").get<std::array<ssx::RailVector,4>>();segment.boundsMin=source.at("bounds_min").get<ssx::RailVector>();if(source.contains("row50"))segment.arcToParameter=source.at("row50").get<std::array<float,4>>();segment.boundsMax=source.at("bounds_max").get<ssx::RailVector>();
   for(auto row:segment.coefficients)finite(row);finite(segment.boundsMin);finite(segment.boundsMax);
   if(!(segment.length>0)||!std::isfinite(segment.length)||!std::isfinite(segment.distance)||segment.distance<0||!segments.insert(int64_t(record.packedId&255)<<32|uint32_t(segment.index)).second)throw std::runtime_error("Invalid original rail segment");
   for(unsigned k=0;k<3;k++)if(segment.boundsMin[k]>segment.boundsMax[k])throw std::runtime_error("Inverted rail bounds");
   record.segments.push_back(segment);++total;
  }
  if(record.segments.empty()||record.segments.size()!=item.at("segment_count").get<size_t>())throw std::runtime_error("Original rail segment count mismatch");
  for(size_t i=0;i<record.segments.size();i++)if(record.segments[i].previous!=(i?record.segments[i-1].index:-1)||record.segments[i].next!=(i+1<record.segments.size()?record.segments[i+1].index:-1))throw std::runtime_error("Original rail chain mismatch");
  next.push_back(std::move(record));
 }
 if(next.size()!=data.at("rail_count").get<size_t>()||total!=data.at("segment_count").get<unsigned>())throw std::runtime_error("Original rail catalog count mismatch");
 RIDER_LOCAL extern bool browserWorldAppend;
 if(partial&&!(browserWorldAppend&&ready))throw std::runtime_error("A partial rail catalog needs an appended world");
 if(browserWorldAppend&&ready){ // streamed location (web/peak_world.inc): capacity is reserved, so held record pointers stay valid
  if(records.capacity()<records.size()+next.size())throw std::runtime_error("Streamed rail capacity exceeded");
  for(auto& r:next){bool refilled=false; // pv peakRelease: a re-read location's rails go back into their released records (held pointers)
   for(auto& o:records)if(o.packedId==r.packedId){
    if(browserRailReleased(o)&&o.segments.empty()){releasedRails.erase(std::lower_bound(releasedRails.begin(),releasedRails.end(),o.packedId));o=std::move(r);}
    else if(o.flags==0&&r.flags==0&&releasedRailTracks[o.packedId&255u]){total-=unsigned(r.segments.size());} // a set-piece path kept through the release: the same record, left in place
    else throw std::runtime_error("Duplicate original rail id");
    refilled=true;break;}
   if(!refilled)records.push_back(std::move(r));}
  segmentCount+=total;if(partial)return;if(data.at("location")=="ARA1"||!teeters.empty()){teeterCourse=true;bind_teeters();}else browser_static_records_rebuild();return;
 }
 records=std::move(next);segmentCount=total;ready=true;releasedRails.clear();releasedRailTracks={};
 rails_loaded(data.at("location").get<std::string>());
}
// pv eventSlices (web/load-slices.js initEventWorldSliced): an event's catalog in parts. The header (no rails) through init_rails, the
// parts (all "partial": segment_base, append mode), then rails_seal: init_rails' tail over the whole catalog, and the parse cache the
// computer riders' contexts copy by key (init_rails_cached; the whole catalog's parse key, web/peak-world-batches.js parseKey).
EMSCRIPTEN_KEEPALIVE void rails_seal(const char* key){
 if(!ready)throw std::runtime_error("rails_seal without a catalog");
 rails_loaded(railLocation);railParseCache={key,railLocation,records,segmentCount};
}
EMSCRIPTEN_KEEPALIVE int init_rails_cached(const char* key){
 if(railParseCache.key.empty()||railParseCache.key!=key)return 0;
 records=railParseCache.records;segmentCount=railParseCache.segments;ready=true;releasedRails.clear();releasedRailTracks={};rails_loaded(railParseCache.location);return 1;
}
// Tests (web/test-ctm-stream.mjs): FNV-1a over what the catalog loads put in (a whole location == its parts, bit for bit): the records
// and their segments, the counts, the teeters' bound rails and the static records.
EMSCRIPTEN_KEEPALIVE uint32_t rail_load_hash(){
 uint32_t h=0x811c9dc5u;auto mix=[&](const void* p,size_t n){const auto* b=static_cast<const uint8_t*>(p);for(size_t i=0;i<n;++i){h^=b[i];h*=0x01000193u;}};
 auto records_=[&](const std::vector<ssx::OriginalRailRecord>& v){const uint32_t n=uint32_t(v.size());mix(&n,4);for(const auto& r:v){mix(&r.packedId,4);mix(&r.flags,4);mix(&r.surface,4);const uint32_t m=uint32_t(r.segments.size());mix(&m,4);
  for(const auto& g:r.segments){mix(g.coefficients.data(),sizeof(g.coefficients));mix(g.boundsMin.data(),sizeof(g.boundsMin));mix(g.boundsMax.data(),sizeof(g.boundsMax));mix(&g.length,4);mix(&g.distance,4);mix(&g.index,4);mix(&g.previous,4);mix(&g.next,4);mix(&g.flags,4);mix(g.arcToParameter.data(),sizeof(g.arcToParameter));}}};
 records_(records);records_(staticRecords);mix(&segmentCount,4);const uint8_t f[3]={uint8_t(ready),uint8_t(teeterCourse),uint8_t(staticActive)};mix(f,3);mix(railLocation.data(),railLocation.size());
 const uint32_t nt=uint32_t(teeters.size());mix(&nt,4);for(const auto& t:teeters){mix(&t.seed->resource,sizeof(t.seed->resource));const uint32_t nr=uint32_t(t.rails.size());mix(&nr,4);for(const auto& [m,record]:t.rails)mix(&record->packedId,4);}
 return h;
}
EMSCRIPTEN_KEEPALIVE float* rail_info(){RIDER_LOCAL static float result[3];result[0]=ready;result[1]=records.size();result[2]=segmentCount;return result;}
EMSCRIPTEN_KEEPALIVE float* rail_query(float x,float y,float z){
 RIDER_LOCAL static float result[14];std::fill(std::begin(result),std::end(result),0.f);result[0]=ready;auto hit=browserRailQuery({x,y,z});result[1]=hit.found;
 result[2]=hit.record?float(hit.record->packedId):-1;result[3]=hit.segment?float(hit.segment->index):-1;result[4]=hit.t;result[5]=hit.distance;
 for(unsigned i=0;i<3;i++){result[6+i]=hit.point[i];result[9+i]=hit.tangent[i];}result[12]=hit.surface;result[13]=hit.record?hit.record->flags:0;return result;
}
}
// 0x106F78 entity hooks of the log teeters, and the Snow Jam falling billboard (program 144).
#include "rail_snap_teeter.inc"
#include "falling_billboard.inc"
#include "rail_dynamic.inc"
// Per rider context (web/rider_context.cpp): construct this translation unit's RIDER_LOCAL_LAZY containers.
void rider_statics_rails(){rider_touch(&teeterVelocityTables);}
static const bool riderStaticsRailsReady=(rider_statics_rails(),true);
#ifdef SSX_SNAPSHOT_REGISTRY // the rider-context snapshot's registry (web/generate-snapshot-registry.mjs, docs/replay.md §2a)
#include "generated/snapshot/rail_bridge.inc"
#endif
