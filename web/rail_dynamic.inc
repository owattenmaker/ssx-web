// Dynamic rails of the other locations (included at the END of web/rail_bridge.cpp, after falling_billboard.inc;
// declarations in web/rail_bridge.hpp). engine/rail_modifier.hpp, docs/set-pieces.md "R&B (ASS1) and Crow's Nest (ABA1)".
// * RailModifiers that stage programs bind at run time (builtin48 0x2FF1C8 -> 0x355E38 -> ctor 0x35B708, from the core's
//   stage VM, web/stage_script_gameplay.inc): R&B program 55 binds the 27 spline_ASS1_boxcar_* rails to
//   mdl_ASS1_boxcar_1000 with node -1 (rest = instance+0x10, transform = rest^-1 * entity matrix = the Spline matrix:
//   0x35C0E8 / 0x35C5A0 -> 0x34FED8(inst, -1) -> vt+0xC4), bounds = the Spline entity bounds -/+ 100 (0x35C040 at the ctor
//   and 0x35C4E0 in every entity pass, after the modifier update); programs 63/64 bind spline_ASS1_animloga_1000 to node 0
//   of the mdl_ASS1_animloga_1000 LiveComp (no primary modifier: instance bounds -/+ 100). The ctor takes the rail's static
//   segments out of the world tree (0x3284B8, here: the staticRecords copy answers nothing for it) and the entity's
//   destruction (section leave, builtin2, MeshAnim) reinserts them (0x35BD70). 0x334680 answers the bound rails through
//   0x35C698 after the static layers, like the Snow Jam log teeters.
// * The resident extra location's log teeter (R&B mdl_A_ASS1_logbreakteeter_1000, program 13 = builtin6 + builtin48(0x404,
//   node 1); generated/rail_teeter_seed_<LOC>.hpp): built at the load (the countdown savestates hold it), destroyed by its
//   section leave and rebuilt by the slot-1 program at the section enter (web/section_gameplay.inc), unlike the Snow Jam logs
//   (bound for the whole race).
// Entity hooks of the rail snap 0x106F78: Object 0x356AE0 -> Spline vt+0xB4 0x359CF8 (contact velocity), LiveComp 0x34E698;
// vt+0x15C 0x356AB8 forwards to the primary modifier's vt+0xCC (Spline 0x360BF0: none): no force on either.
#if __has_include("generated/rail_teeter_seed_ASS1.hpp")
#include "generated/rail_teeter_seed_ASS1.hpp"
#define BROWSER_RAIL_TEETERS_ASS1 1
#endif
bool browser_dynamic_rail_entity(uint32_t owner,int32_t node,ssx::RollerMatrix* matrix,ssx::RollerQuad* boundsMin,ssx::RollerQuad* boundsMax); // web/set_piece_gameplay.inc
bool browser_dynamic_rail_contact_velocity(uint32_t owner,int32_t node,const ssx::RollerQuad& point,ssx::RollerQuad& packet20,ssx::RollerQuad& packet30);
namespace {
struct DynamicRail {uint32_t owner=0;ssx::OriginalRailModifier modifier;const ssx::OriginalRailRecord* record=nullptr;};
RIDER_LOCAL std::vector<DynamicRail> dynamicRails;
RIDER_LOCAL const std::vector<BrowserRailTeeterSeed>* locationTeeterSeeds=nullptr; // the location's teeter seeds (by pointer: the teeters keep them)
struct LocationTeeterData {std::vector<BrowserRailTeeterSeed> seeds;std::vector<BrowserRailTeeterNode> nodes;std::vector<BrowserRailTeeterCurve> curves;std::vector<BrowserRailTeeterSegment> segments;std::vector<BrowserRailTeeterRail> rails;};
RIDER_LOCAL LocationTeeterData locationTeeters;
void static_records_active(){ // the static layers without the bound rails: a copy of every record, bound ones answer nothing (flags 0)
 if(staticActive||!teeters.empty())return;
 staticRecords=records;staticActive=true;
}
void static_record_bound(uint32_t packedId,bool bound){
 static_records_active();
 for(auto& e:staticRecords)if(e.packedId==packedId){e.flags=0;if(!bound)for(const auto& r:records)if(r.packedId==packedId)e.flags=r.flags;}
}
ssx::OriginalAnimModel teeter_model(const LocationTeeterData& d,const BrowserRailTeeterSeed& s){
 ssx::OriginalAnimModel model;model.length=bitsF(s.length);
 for(uint32_t i=0;i<s.nodeCount;i++){
  const auto& n=d.nodes[s.firstNode+i];ssx::OriginalAnimNode node;node.parent=n.parent;node.bind=matrixBits(n.bind);
  if(n.animated){ssx::OriginalAnimTrack track;for(unsigned k=0;k<6;k++)track.base[k]=bitsF(n.base[k]);track.mask=n.mask;
   for(uint32_t c=0;c<n.curveCount;c++){const auto& cv=d.curves[n.firstCurve+c];std::vector<ssx::OriginalAnimSegment> segs;
    for(uint32_t k=0;k<cv.count;k++){const auto& g=d.segments[cv.first+k];segs.push_back({bitsF(g.a),bitsF(g.b),bitsF(g.c),bitsF(g.d),bitsF(g.t0),bitsF(g.t1)});}
    track.curves.push_back(std::move(segs));}
   node.track=std::move(track);}
  model.nodes.push_back(std::move(node));
 }
 return model;
}
// builtin6 (0x3421A0) + the program's builtin48 (0x35B708) of one location teeter; the rails leave the static layers.
void location_teeter_build(BrowserTeeter& t){
 const auto& s=*t.seed;const auto& a=s.teeterArgs;
 ssx::OriginalAnimTeeterArgs args{bitsF(a[1]),bitsF(a[2]),bitsF(a[3]),bitsF(a[4]),bitsF(a[5]),bitsF(a[6]),a[7],bitsF(a[8]),bitsF(a[9])};
 const auto matrix=matrixBits(s.matrix);const float scale=bitsF(s.scale);
 t.entity=ssx::originalAnimTeeterConstruct(t.model,matrix,scale,args);t.rails.clear();
 std::array<float,3> low{bitsF(s.boundsMin[0]),bitsF(s.boundsMin[1]),bitsF(s.boundsMin[2])},high{bitsF(s.boundsMax[0]),bitsF(s.boundsMax[1]),bitsF(s.boundsMax[2])};
 for(uint32_t r=0;r<s.railCount;r++){
  const auto& rail=locationTeeters.rails[s.firstRail+r];const ssx::OriginalRailRecord* record=nullptr;
  for(const auto& x:records)if(x.packedId==rail.packedId)record=&x;
  if(!record)continue;
  t.rails.push_back({ssx::originalRailModifierConstruct(rail.packedId,rail.node,s.resource,t.model,matrix,scale,low,high),record});static_record_bound(rail.packedId,true);
 }
}
void bind_location_teeters(const std::string& location){
 teeters.clear();staticRecords.clear();staticActive=false;teeterForces=teeterTicks=0;locationTeeters={};
#ifdef BROWSER_RAIL_TEETERS_ASS1
 if(location=="ASS1"){namespace N=browser_rail_teeter_ass1;
  locationTeeters.seeds.assign(N::Seeds.begin(),N::Seeds.end());locationTeeters.nodes.assign(N::Nodes.begin(),N::Nodes.end());
  locationTeeters.curves.assign(N::Curves.begin(),N::Curves.end());locationTeeters.segments.assign(N::Segments.begin(),N::Segments.end());
  locationTeeters.rails.assign(N::Rails.begin(),N::Rails.end());}
#endif
 if(locationTeeters.seeds.empty())return;
 staticRecords=records;staticActive=true;
 for(const auto& s:locationTeeters.seeds){BrowserTeeter t;t.seed=&s;t.model=teeter_model(locationTeeters,s);teeters.push_back(std::move(t));}
 for(auto& t:teeters)location_teeter_build(t); // after the vector is final: the entity keeps a pointer to its model
}
}
// builtin35 (0x300B20 -> 0x358B28 on the RailMan gp+0xF50, 128 group slots at +0x18): flag 0 takes a rail's static segments out
// of the world tree (0x358780) and keeps its id in a free slot; flag 1 puts a slotted rail back (0x358998: 328C20 per segment, the
// slot freed). An id already slotted is not removed again, an unslotted one is not re-inserted. ABC1 fallingtreea trunk rails
// (0x4706..0x4E06: off in slot 1, on at the fall, then builtin48 binds them to the tree), DRA4 treefallrail g/i (0x4020..0x4520).
namespace {RIDER_LOCAL std::vector<uint32_t> railGroupSlots;}
void browser_rail_group(uint32_t id,bool on){
 if(!ready||teeterCourse)return;bool known=false;for(const auto& r:records)known|=r.packedId==id;if(!known)return;
 const auto it=std::find(railGroupSlots.begin(),railGroupSlots.end(),id);
 if(it!=railGroupSlots.end()){if(on){railGroupSlots.erase(it);static_record_bound(id,false);}return;}
 if(on||railGroupSlots.size()>=128)return;
 railGroupSlots.push_back(id);static_record_bound(id,true);
}
// A streamed location's rails were appended (web/rail_bridge.cpp init_rails): the static layers are the whole catalog again, less the
// rails still out of the world tree (bound to a run-time entity, or a RailMan group off). Before, the copy was dropped and every bound
// or grouped-off rail answered statically again after the next append.
void browser_static_records_rebuild(){
 if(!staticActive){staticRecords.clear();return;}
 staticRecords=records;
 for(auto& e:staticRecords){bool out=std::find(railGroupSlots.begin(),railGroupSlots.end(),e.packedId)!=railGroupSlots.end();
  for(const auto& d:dynamicRails)out|=d.modifier.packedId==e.packedId;if(out)e.flags=0;}
}
// QA: [count, ids...] of the RailMan's filled group slots (+0x18), in slot order.
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* rail_group_slots(){RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,uint32_t(railGroupSlots.size()));out.insert(out.end(),railGroupSlots.begin(),railGroupSlots.end());return out.data();}
// New race / rail load (web/rail_bridge.cpp): the location teeters from their seeds, no run-time rails.
void browser_dynamic_rails_reset(const std::string& location){dynamicRails.clear();railGroupSlots.clear();bind_location_teeters(location);}
// builtin48 on an entity another browser system owns: a Spline piece (node -1) or a stage-world LiveComp (node >= 0).
bool browser_dynamic_rail_bind(uint32_t owner,uint32_t packedId,int32_t node,const ssx::OriginalAnimModel* model,const ssx::RollerMatrix& instanceMatrix,float scale,
  const std::array<float,3>& low,const std::array<float,3>& high){
 if(!ready||teeterCourse)return false; // Snow Jam keeps its seeded teeters / billboard
 const ssx::OriginalRailRecord* record=nullptr;for(const auto& x:records)if(x.packedId==packedId)record=&x;
 if(!record)return false;
 ssx::RollerQuad bmin,bmax;std::optional<ssx::OriginalRailEntityBounds> entity;
 if(browser_dynamic_rail_entity(owner,-1,nullptr,&bmin,&bmax))entity=ssx::OriginalRailEntityBounds{bmin,bmax}; // 0x3569D0: a primary modifier reports bounds
 DynamicRail d;d.owner=owner;d.record=record;
 if(node<0)d.modifier=ssx::originalRailModifierConstructInstance(packedId,owner,instanceMatrix,low,high,entity);
 else{if(!model)return false;d.modifier=ssx::originalRailModifierConstruct(packedId,node,owner,*model,instanceMatrix,scale,low,high,entity);}
 dynamicRails.push_back(d);static_record_bound(packedId,true);
 return true;
}
// The entity is gone (0x35BD70 for each of its rails): the static segments answer again.
void browser_dynamic_rails_unbind(uint32_t owner){
 for(size_t k=0;k<dynamicRails.size();)if(dynamicRails[k].owner==owner){const uint32_t id=dynamicRails[k].modifier.packedId;dynamicRails.erase(dynamicRails.begin()+long(k));
  bool still=false;for(const auto& d:dynamicRails)if(d.modifier.packedId==id)still=true;if(!still)static_record_bound(id,false);}else ++k;
}
// 0x35C4E0 in the entity pass (after the primary modifier's update): bounds from the entity bounds.
void browser_dynamic_rails_tick(){
 for(auto& d:dynamicRails){ssx::RollerQuad bmin,bmax;
  if(browser_dynamic_rail_entity(d.owner,-1,nullptr,&bmin,&bmax))ssx::originalRailModifierBounds(d.modifier,ssx::OriginalRailEntityBounds{bmin,bmax});}
}
// Section leave / enter of a location teeter (0x34FD90 destroys the entity; slot-1 program 13 rebuilds it).
void browser_location_teeter_section(uint32_t resource,bool alive){
 if(teeterCourse)return;
 for(auto& t:teeters)if(t.seed->resource==resource){
  if(alive)location_teeter_build(t);
  else{for(auto& [m,record]:t.rails)static_record_bound(m.packedId,false);t.rails.clear();}
 }
}
// 0x334680 type-2 layers of the run-time rails (after the static layers, strictly closer replaces).
void browser_dynamic_rails_query(ssx::RailVector point,uint32_t mask,bool& found,float& best,ssx::OriginalRailModifierHit& hit,bool& modifierHit){
 const ssx::RailVector extent{300,300,300};
 for(auto& d:dynamicRails){
  const auto& m=d.modifier;
  bool overlaps=true;for(unsigned k=0;k<3;k++)overlaps&=m.boundsMin[k]<point[k]+extent[k]&&point[k]-extent[k]<m.boundsMax[k];
  if(!overlaps)continue;
  ssx::RollerMatrix world;if(!browser_dynamic_rail_entity(d.owner,m.node,&world,nullptr,nullptr))continue;
  bool before=found;float previous=best;
  ssx::originalRailModifierQuery(m,*d.record,ssx::originalRailModifierTransform(m,world),{point[0],point[1],point[2],1.f},mask,found,best,hit);
  if(found&&(!before||best!=previous))modifierHit=true;
 }
}
namespace {
const DynamicRail* dynamic_rail_of(const ssx::OriginalRailQueryResult& hit){if(!hit.record)return nullptr;for(const auto& d:dynamicRails)if(d.record==hit.record)return &d;return nullptr;}
}
bool browser_dynamic_rail_snap_entity(const ssx::OriginalRailQueryResult& hit){return dynamic_rail_of(hit)!=nullptr;}
void browser_dynamic_rail_snap_velocity(const ssx::OriginalRailQueryResult& hit,ssx::RollerQuad& packet20,ssx::RollerQuad& packet30){
 const auto* d=dynamic_rail_of(hit);if(!d)return;
 browser_dynamic_rail_contact_velocity(d->owner,d->modifier.node,{hit.point[0],hit.point[1],hit.point[2],1.f},packet20,packet30);
}
// QA: [count, then per rail 1 + 1 + 1 + 8 + 16 words: owner, packed id, node, bounds min/max, rest inverse] (raw bits).
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* rail_dynamic_bits(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,0u);auto B=[](float x){return std::bit_cast<uint32_t>(x);};
 auto add=[&](uint32_t owner,const ssx::OriginalRailModifier& m){++out[0];out.insert(out.end(),{owner,m.packedId,uint32_t(m.node)});
  for(unsigned k=0;k<4;k++)out.push_back(B(m.boundsMin[k]));for(unsigned k=0;k<4;k++)out.push_back(B(m.boundsMax[k]));
  for(unsigned r=0;r<4;r++)for(unsigned c=0;c<4;c++)out.push_back(B(m.restInverse[r][c]));};
 for(const auto& d:dynamicRails)add(d.owner,d.modifier);
 if(!teeterCourse)for(auto& t:teeters)for(auto& [m,record]:t.rails)add(t.seed->resource,m);
 return out.data();
}
// QA: location teeters [count, then per teeter: resource, 17 words of the AnimTeeter +0x00..+0x40 (raw bits)].
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* rail_location_teeters(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,0u);auto B=[](float x){return std::bit_cast<uint32_t>(x);};
 if(!teeterCourse)for(auto& t:teeters){const auto& e=t.entity;++out[0];
  out.insert(out.end(),{t.seed->resource,B(e.torque),B(e.step),B(e.gain),B(e.restitution),B(e.minimum),B(e.maximum),B(e.maxStep),B(e.damping),B(e.stiffness),B(e.rest),B(e.velocity),e.flipMask,B(e.time),B(e.sampleTime),B(e.previous),B(e.unclamped),uint32_t(e.evaluated),uint32_t(t.rails.size())});}
 return out.data();
}
