#include "rider_local.hpp"
#include "../engine/world_body_collision.hpp"
#include "json.hpp"
#include "generated/event_instance_seed.hpp"
#include "generated/physics_seed.hpp"
#include <emscripten/emscripten.h>
#include <map>
using namespace ssx;
using nlohmann::json;
RIDER_LOCAL std::unique_ptr<WorldBodyCollision> browserBodies;
bool set_piece_instance(const std::string& location,uint32_t resource,uint32_t authoredFlags,uint32_t& runtimeFlags); // web/set_piece_gameplay.inc
extern unsigned setPieceInstanceCalls;void browser_set_piece_location(const std::string&);
void browser_attach_set_pieces(); // clones + race-tick-0 state after every world load
RIDER_LOCAL static std::string bodySourceHash;RIDER_LOCAL static bool bodyTerrainReady=false;RIDER_LOCAL static terrain_original::ContactCache bodyContactCache;
void apply_snow_jam_dead_nodes(){
 if(!browserBodies||bodySourceHash!=browserEventWorldHash)throw std::runtime_error("Event collision world mismatch");
 std::vector<std::pair<WorldCollisionInstance*,uint32_t>> changes;
 for(const auto& seed:browserEventDeadNodes){
  auto it=std::find_if(browserBodies->instances.begin(),browserBodies->instances.end(),[&](const auto& i){return i.resource==seed.resource;});
  if(it==browserBodies->instances.end()||it->flags!=seed.authoredFlags)throw std::runtime_error("Event instance identity mismatch");
  changes.emplace_back(&*it,seed.runtimeFlags);
 }
 for(auto [instance,flags]:changes)instance->eventRuntimeFlags=flags;
}
void reset_body_queries(){bodyContactCache={};if(browserBodies)browserBodies->resetSphereTreeCache();}
// Actor +0x868 body-query cache kind: 1 (detailed 9x9 cells) for the human, 0 (coarse 3x3) for computer
// riders (web/npc_gameplay.inc clears this in a computer rider's core instance).
RIDER_LOCAL bool browserHumanRider=true;
// pv finishFences (web/free-ride.js stage_object_route): the ten collision instances authored with flags 0 (ARA1
// endmode_collide, ERA5 fencecollision_end: the barriers before those courses' finishes in free ride and the peak runs) load on
// the PS2 with instance+8 = 2, no collision route, until their slot-1 program's Object entity (builtin 0 key1: 356DB0) sets 0x20.
// The port marked them unsupported, so a query meeting their box was incomplete (no contact at all) and the rider went through.
RIDER_LOCAL bool browserObjectRoutes=false;
static void object_route_instances(WorldBodyCollision& w){if(!browserObjectRoutes)return;
 for(auto& i:w.instances)if(i.flags==0&&i.type>=1&&i.type<=3&&!i.runtimeClone&&!i.nodes.empty()&&i.unsupported=="Dynamic entity callbacks unavailable"){i.unsupported.clear();if(!i.eventRuntimeFlags)i.eventRuntimeFlags=0x2u;}}
RIDER_LOCAL extern std::optional<terrain_original::RiderScope> riderScope; // web/core.cpp (rider+0x860): 13F488 / 13AA48 query the scope list
std::optional<WorldBodyQuery> inspect_body_contacts(const BodyCollisionVolume& volume,std::array<float,3> normal){if(!browserBodies||!bodyTerrainReady)return {};return browserBodies->query(volume,normal,nullptr,browserHumanRider,2,&bodyContactCache,riderScope?&*riderScope:nullptr);}
// Authored dynamic instances with a verified countdown runtime state (event_instance_seed.hpp).
// The streamed peak worlds seed no event course (browserEventCourseSeeded false, no list): a dynamic instance there takes its
// course's event seed (resource ids are unique per SDB track). Without one it stayed unsupported, and a crash flight, crash
// body query or reset placement meeting it stopped the core (Gravitude's mdl_ERA5_CRbillboard_1000 in the All Peak Race,
// docs/peak3.md section 6). 251 of the whole mountain's 256 dynamic instances have a course seed; the stations' five os609
// models take the free-ride seed browser_streamed::scripted.
namespace {constexpr std::array<std::span<const BrowserEventScriptedInstance>,17> courseScriptedLists{{browser_event_ARA1::scripted,browser_event_BRA2::scripted,browser_event_BHP1::scripted,browser_event_ABC1::scripted,browser_event_ASS1::scripted,browser_event_ABA1::scripted,browser_event_CRA3::scripted,browser_event_DRA4::scripted,browser_event_DSS2::scripted,browser_event_CBA2::scripted,browser_event_CHP2::scripted,browser_event_DBC2::scripted,browser_event_ERA5::scripted,browser_event_ESS3::scripted,browser_event_EBA3::scripted,browser_event_EHP3::scripted,browser_event_EBC3::scripted}};}
const BrowserEventScriptedInstance* scripted_instance(uint32_t resource){
 for(const auto& x:browserEventScriptedInstances)if(x.resource==resource)return &x;
 if(!browserEventCourseSeeded)for(const auto& list:courseScriptedLists)for(const auto& x:list)if(x.resource==resource)return &x;
 // The five stations' os609 departure models belong to no course event: their free-ride seed (PS2 free-ride audits: no
 // entity, static route 0x50215023, no slot-2 contact program; tools/generate_event_seed.py streamed_scripted).
 if(!browserEventCourseSeeded)for(const auto& x:browser_streamed::scripted)if(x.resource==resource)return &x;
 return nullptr;
}
namespace {
using V=terrain_original::Vector;using M=collision_transform::Matrix;
std::string resourceKey(uint32_t r){return std::to_string(r&255)+":"+std::to_string(r>>8);}
V vector(const json& j,unsigned offset=0){if(!j.is_array()||j.size()<offset+3)throw std::runtime_error("Invalid world collision vector");V v;for(unsigned k=0;k<3;k++){v[k]=j.at(offset+k).get<float>();if(!std::isfinite(v[k]))throw std::runtime_error("Nonfinite world collision vector");}return v;}
M matrix(const json& j){auto m=j.get<M>();for(float f:m)if(!std::isfinite(f))throw std::runtime_error("Nonfinite collision matrix");return m;}
}
void browser_set_track_location(int,const std::string&);void browser_clear_track_locations();
// Streamed locations (web/peak_world.inc): the next init_world_collision / init_body_terrain / init_terrain / init_rails
// append a location to the loaded world instead of replacing it (false for every race event package).
RIDER_LOCAL bool browserWorldAppend=false;
// Course package parse cache, shared by the rider contexts (web/rider_local.hpp): every rider loads the same course, so a
// computer rider's init_world_collision / init_body_terrain copy the parse of the same text (the human's, done first)
// instead of parsing it again (~0.3 s each). The parse is a pure function of the text (the course seeds it reads follow
// the location in that text), so the copy is what the parse would build. Streamed (append) loads never use it.
namespace {
struct BodyParse {std::string key,location,sourceHash;std::vector<std::pair<int,std::string>> tracks;std::unique_ptr<const WorldBodyCollision> bodies;bool setPieces=false;};
struct TerrainParse {std::string key;std::vector<WorldCollisionTerrain> terrain;};
BodyParse bodyParseCache;TerrainParse terrainParseCache;
std::string parse_key(const char* text,const std::string& salt){uint32_t h=0x811c9dc5u;size_t n=0;for(const char* c=text;*c;++c,++n){h^=uint8_t(*c);h*=0x01000193u;}return salt+":"+std::to_string(n)+":"+std::to_string(h);}
}
// The parse cache's world (a computer rider's context: init_world_collision of the text the human parsed, or its key).
static void world_collision_from_cache(){
 const auto& c=bodyParseCache;browser_select_event_course(c.location);browser_select_glide_course(c.location);browser_clear_track_locations();
 for(const auto& [track,name]:c.tracks)browser_set_track_location(track,name);
 if(c.setPieces)browser_set_piece_location(c.location); // the parse's set_piece_instance calls name it
 auto result=std::make_unique<WorldBodyCollision>(*c.bodies);
 bodyTerrainReady=false;bodyContactCache={};bodySourceHash=c.sourceHash;browserBodies=std::move(result);object_route_instances(*browserBodies);browser_attach_set_pieces();
}
// init_body_terrain's patches of a parsed document (its source must be the world's).
static std::vector<WorldCollisionTerrain> body_terrain_patches(const json& data){
 if(data.at("source_sha256")!=bodySourceHash)throw std::runtime_error("Body terrain source mismatch");
 std::vector<WorldCollisionTerrain> out;
 auto meters=[](const json& value){double n=value.get<double>();if(!std::isfinite(n))throw std::runtime_error("Nonfinite terrain coordinate");float result=float(n*100);if(!std::isfinite(result))throw std::runtime_error("Terrain coordinate overflow");return result;};
 for(const auto& source:data.at("patches")){
  WorldCollisionTerrain patch;patch.resource=source.at("resource_id");patch.flags=source.at("authored_flags");patch.surface=source.at("authored_surface_id");
  const auto& lo=source.at("authored_bounds_min");const auto& hi=source.at("authored_bounds_max");vector(lo);vector(hi);
  patch.low={meters(lo.at(0)),-meters(hi.at(2)),meters(lo.at(1))};patch.high={meters(hi.at(0)),-meters(lo.at(2)),meters(hi.at(1))};
  for(unsigned i=0;i<3;i++)if(patch.high[i]<patch.low[i])throw std::runtime_error("Reversed body terrain bounds");
  const auto& values=source.at("coefficients");if(values.size()!=16)throw std::runtime_error("Invalid body terrain coefficients");
  terrain_original::Coefficients coefficients;
  for(unsigned i=0;i<16;i++){const auto& v=values.at(i);if(v.size()!=3)throw std::runtime_error("Invalid terrain coefficient dimensions");coefficients[i]={meters(v.at(0)),-meters(v.at(2)),meters(v.at(1))};}
  patch.grid=terrain_original::coarseGrid(coefficients);out.push_back(std::move(patch));
 }
 return out;
}
void browser_camera_terrain_json(json& data); // web/race_bridge.cpp (init_terrain's work)
void browser_rails_parse_cache_clear(); // web/rail_bridge.cpp
#include "../engine/collision.hpp"
extern "C" {extern std::unique_ptr<ssx::CollisionWorld> cameraTerrain;} // (as web/peak_world.inc declares it)
// pv eventSlices (web/load-slices.js initEventWorldSliced): an event course's terrain, collision and body terrain in parts that end in
// the state of the whole-document calls (web/test-event-slices.mjs body_load_hash / world_load_hash):
//   event_world_begin(); the header documents through the ordinary calls (init_terrain, init_world_collision, init_body_terrain with
//   no patches or instances: the course selection, the track locations); peak_world_append(1); the world parts (init_world_collision:
//   instances with their descriptors, meshes and hierarchies, cut from the document's text), then the terrain parts (terrain_part: one
//   parse for both terrain systems); event_world_seal(worldKey, terrainKey): the terrain orders committed, then the tails of the whole
//   calls in their order (the collision parse cache (no terrain in it yet), object routes, the set pieces; then the body terrain and its
//   parse cache). The keys are the whole documents' parse keys, so a computer rider's context copies this load as it copies a parse.
RIDER_LOCAL static bool eventSlicing=false;RIDER_LOCAL static unsigned eventSliceSetPieceCalls=0;
extern "C" {
// Frees the course parse cache (web/ai-racers.js, once every rider context has its world).
EMSCRIPTEN_KEEPALIVE void rider_parse_cache_clear(){bodyParseCache={};terrainParseCache={};browser_rails_parse_cache_clear();}
EMSCRIPTEN_KEEPALIVE void init_world_collision(const char* text,const char* expectedHash){
 const bool cacheable=!(browserWorldAppend&&browserBodies);const std::string key=cacheable?parse_key(text,expectedHash?expectedHash:""):std::string();
 if(cacheable&&bodyParseCache.bodies&&bodyParseCache.key==key){world_collision_from_cache();return;}
 const unsigned setPieceCallsBefore=setPieceInstanceCalls;
 const auto p=json::parse(text);if(p.at("version")!=1||p.at("source_sha256")!=expectedHash)throw std::runtime_error("World collision source mismatch");
 // Course-specific event seeds (dead/skip nodes, scripted instances, grid start) follow the loaded location.
 const std::string location=p.value("location",std::string()); // synthetic test worlds carry no location
 const bool append=browserWorldAppend&&browserBodies; // streamed location: the streaming state owns the track slots
 if(!append){
 browser_select_event_course(location);
 // reset_rider re-reads the glide seed (generated/physics_seed.hpp) of the selected course.
 browser_select_glide_course(location);
 browser_clear_track_locations(); // track slot -> location id for rider+0x434 (web/core.cpp, 1218D0)
 if(p.contains("event_locations"))for(const auto& e:p.at("event_locations"))browser_set_track_location(e.at("track").get<int>(),e.at("name").get<std::string>());
 }
 auto result=std::make_unique<WorldBodyCollision>();
 if(append){result->instances=std::move(browserBodies->instances);result->terrain=std::move(browserBodies->terrain);
  result->pendingTerrain=std::move(browserBodies->pendingTerrain);result->nextInsertion=browserBodies->nextInsertion;result->nextReinsertion=browserBodies->nextReinsertion;result->collidableGeneration=browserBodies->collidableGeneration;}
 // pv peakRelease: a re-read location's instances go back into their released slots (set pieces hold pointers to them).
 std::unordered_map<uint32_t,size_t> releasedSlots;if(append)for(size_t k=0;k<result->instances.size();++k)if(result->instances[k].released)releasedSlots.emplace(result->instances[k].resource,k);
 auto place=[&](WorldCollisionInstance&& instance){
  if(!releasedSlots.empty())if(auto it=releasedSlots.find(instance.resource);it!=releasedSlots.end()){result->reinsertInstance(result->instances[it->second],std::move(instance));releasedSlots.erase(it);return;}
  result->instances.push_back(std::move(instance));};
 std::map<std::pair<uint32_t,unsigned>,std::shared_ptr<const CollisionTriangleMesh>> meshes;
 std::map<std::pair<uint32_t,unsigned>,std::shared_ptr<const CollisionSphereTree>> trees;
 for(const auto& source:p.at("instances")){
  WorldCollisionInstance instance;unsigned track=source.at("track"),rid=source.at("rid");instance.resource=(rid<<8)|track;
  const auto& descriptor=p.at("bindings").at(std::to_string(track)).at("descriptors").at(source.at("collision_descriptor").get<unsigned>());
  instance.type=descriptor.at("type");instance.flags=descriptor.at("flags");instance.scale=source.at("scale");instance.low=vector(source.at("bounds_min_cm"));instance.high=vector(source.at("bounds_max_cm"));
  if(source.contains("ray_policy")){if(source.at("ray_policy")!="sphere-tree-no-override"||instance.type!=3)throw std::runtime_error("Invalid authored ray capability");instance.rayAlwaysEmpty=true;}
  if(!std::isfinite(instance.scale))throw std::runtime_error("Nonfinite collision scale");
  for(unsigned k=0;k<3;k++)if(instance.high[k]<instance.low[k])throw std::runtime_error("Reversed collision bounds");
  if(instance.type==0){ /* no collision; set pieces still need the authored matrix for drawing */
   // instance+0x10 exists for every instance: the stage programs read it (stage_instance_matrix), e.g. the Big Challenge
   // teleport markers mdl_<LOC>_bcteleport_* (builtin 64 -> 1234D0 places the rider at their matrix; with the identity the
   // rider was put at the world origin, in the air: PS2 ctm-parity/runs/dizzy s200 at the marker, 2756 / -41460 / 94812).
   instance.authoredMatrix=matrix(source.at("matrix"));
   if(uint32_t flags=0;set_piece_instance(p.value("location",std::string()),instance.resource,instance.flags,flags))instance.eventRuntimeFlags=flags;
   place(std::move(instance));continue;}
  if(instance.scale==0)instance.unsupported="Zero-scale collision";
  else if(instance.type<1||instance.type>3)instance.unsupported="Unknown descriptor type";
  else if(!(instance.flags&0x200000)||((instance.flags&0x40000000)&&!scripted_instance(instance.resource)))instance.unsupported="Dynamic entity callbacks unavailable";
  const auto key=resourceKey(source.at("model_resource"));
  if(!p.at("render_model_nodes").contains(key)){instance.unsupported="Missing model hierarchy";place(std::move(instance));continue;}
  const auto& nodes=p.at("render_model_nodes").at(key).at("nodes");
  if(nodes.empty()){instance.unsupported="Empty model hierarchy";place(std::move(instance));continue;}
  M transform=matrix(source.at("matrix"));std::vector<M> worlds(nodes.size());std::vector<uint8_t> state(nodes.size());
  instance.authoredMatrix=transform; // instance+0x10 for every instance (stage-script modifiers built at run time, web/stage_script_gameplay.inc)
  if(const auto* scripted=scripted_instance(instance.resource)){
   // Authored dynamic instance: countdown runtime flags route it static until its contact
   // handler attaches an entity (the hierarchy is kept for entity re-composition).
   if(scripted->authoredFlags!=instance.flags)throw std::runtime_error("Scripted instance identity mismatch");
   instance.eventRuntimeFlags=scripted->runtimeFlags;instance.authoredMatrix=transform;
   for(const auto& node:nodes){instance.hierarchyLocal.push_back(matrix(node.at("matrix")));instance.hierarchyParent.push_back(node.at("parent").get<uint32_t>());}
  }
  // Set-piece instances with a race-load entity (chairlift LiveComp + MultiSplineModifier,
  // web/set_piece_gameplay.inc): countdown runtime flags and the hierarchy for entity re-composition.
  else if(uint32_t flags=0;set_piece_instance(p.value("location",std::string()),instance.resource,instance.flags,flags)){
   instance.eventRuntimeFlags=flags;instance.authoredMatrix=transform;instance.entityPointer=true;
   for(const auto& node:nodes){instance.hierarchyLocal.push_back(matrix(node.at("matrix")));instance.hierarchyParent.push_back(node.at("parent").get<uint32_t>());}
  }
  // Every other instance keeps its hierarchy too: a stage program can give it an entity at run time (a collected pickup's
  // Debounce holds the frozen MagnetModifier matrix, web/stage_world.inc stage_magnet_freeze; R&B multi pickups).
  else for(const auto& node:nodes){instance.hierarchyLocal.push_back(matrix(node.at("matrix")));instance.hierarchyParent.push_back(node.at("parent").get<uint32_t>());}
  // Type-16 node entities keep course-script flags without 0x20/0x40 from race load: no collector
  // (334458/334888 bodies, 335B90/336D40 rays) ever tests them (event_instance_seed.hpp).
  if(p.at("source_sha256")==browserEventWorldHash)for(const auto& node:browserCourseSkipNodes)if(node.resource==instance.resource){
   if(node.authoredFlags!=instance.flags)throw std::runtime_error("Course skip node identity mismatch");instance.eventRuntimeFlags=node.runtimeFlags;}
  std::function<M(unsigned)> compose=[&](unsigned i)->M{
   if(i>=nodes.size()||state[i]==1)throw std::runtime_error("Invalid collision hierarchy");if(state[i]==2)return worlds[i];state[i]=1;
   unsigned parent=nodes.at(i).at("parent");auto base=parent==0xffffffffu?transform:compose(parent);
   worlds[i]=collision_transform::scaledNode(matrix(nodes.at(i).at("matrix")),base,instance.scale);state[i]=2;return worlds[i];
  };
  unsigned ordinal=0;
  for(unsigned i=0;i<nodes.size();i++){
   const auto& node=nodes.at(i);auto world=compose(i);if(!node.contains("draw_bounds_cm")||node.at("draw_bounds_cm").is_null())continue;
   WorldCollisionNode target;target.index=i;target.type=instance.type;target.world=world;target.inverse=collision_transform::inverseRigid(world);target.low=vector(node.at("draw_bounds_cm"));target.high=vector(node.at("draw_bounds_cm"),3);target.doubleSided=(node.at("draw_flags").get<unsigned>()&1)!=0;target.surface=descriptor.at("nodes").at(i).at("surface_id");target.collisionFlags=descriptor.at("nodes").at(i).at("flags");target.collisionValue=descriptor.at("nodes").at(i).at("value");target.collisionAuxiliary=descriptor.at("nodes").at(i).value("auxiliary",0u);
   if(instance.type==3){
    uint32_t resource=descriptor.at("collision_resource");target.sphereTreeResource=resource;auto treeKey=std::make_pair(resource,ordinal);auto found=trees.find(treeKey);
    if(found!=trees.end())target.sphereTree=found->second;
    else {auto key=resourceKey(resource);
     if(!p.at("collision_meshes").contains(key)||p.at("collision_meshes").at(key).at("format")!=3||ordinal>=p.at("collision_meshes").at(key).at("models").size())instance.unsupported="Missing sphere-tree collision model";
     else {const auto& geometry=p.at("collision_meshes").at(key).at("models").at(ordinal);auto tree=std::make_shared<CollisionSphereTree>();tree->compressed=geometry.at("compressed");tree->centerCm=vector(geometry.at("center_cm"));
      const auto& levels=geometry.at("levels");if(levels.empty()||levels.size()>8)throw std::runtime_error("Invalid sphere-tree depth");
      size_t total=0,stride=1;for(const auto& level:levels){float radius=level.at("radius_cm"),offset=level.at("child_offset_cm");if(!std::isfinite(radius)||!std::isfinite(offset)||radius<0||level.at("stride").get<size_t>()!=stride)throw std::runtime_error("Invalid sphere-tree level");tree->levels.push_back({radius,offset,uint32_t(stride)});total+=stride;stride*=8;}
      const auto& masks=geometry.at("masks");if(masks.size()!=total)throw std::runtime_error("Sphere-tree mask count mismatch");for(const auto& mask:masks){unsigned value=mask.get<unsigned>();if(value>255)throw std::runtime_error("Invalid sphere-tree mask");tree->masks.push_back(uint8_t(value));}
      target.sphereTree=tree;trees.emplace(treeKey,tree);
     }
    }
    ++ordinal;
   }else if(instance.type==1){
    uint32_t resource=descriptor.at("collision_resource");auto meshKey=std::make_pair(resource,ordinal);auto found=meshes.find(meshKey);
    if(found!=meshes.end())target.triangles=found->second;
    else {
     auto key=resourceKey(resource);
     if(!p.at("collision_meshes").contains(key)||ordinal>=p.at("collision_meshes").at(key).at("models").size())instance.unsupported="Missing triangle collision model";
     else {const auto& geometry=p.at("collision_meshes").at(key).at("models").at(ordinal);auto data=std::make_shared<CollisionTriangleMesh>();
      for(const auto& x:geometry.at("vertices_cm"))data->vertices.push_back(vector(x));for(const auto& x:geometry.at("normals"))data->normals.push_back(vector(x));
      data->indices=geometry.at("indices").get<std::vector<unsigned>>();for(auto index:data->indices)if(index>=data->vertices.size())throw std::runtime_error("Collision index out of bounds");
      if(data->indices.size()%3||data->normals.size()!=data->indices.size()/3)throw std::runtime_error("Collision normal count mismatch");target.triangles=data;meshes.emplace(meshKey,data);
     }
    }
    ++ordinal;
   }
   instance.nodes.push_back(std::move(target));
  }
  place(std::move(instance));
 }
 if(append){if(!eventSlicing)object_route_instances(*result);browserBodies=std::move(result);return;} // (an event in parts: routed at its seal) // set pieces stay attached (instance storage is reserved, web/peak_world.inc)
 {BodyParse c{key,location,p.at("source_sha256").get<std::string>(),{},std::make_unique<const WorldBodyCollision>(*result),setPieceInstanceCalls!=setPieceCallsBefore};
  if(p.contains("event_locations"))for(const auto& e:p.at("event_locations"))c.tracks.emplace_back(e.at("track").get<int>(),e.at("name").get<std::string>());
  bodyParseCache=std::move(c);}
 bodyTerrainReady=false;bodyContactCache={};bodySourceHash=p.at("source_sha256").get<std::string>();browserBodies=std::move(result);object_route_instances(*browserBodies);browser_attach_set_pieces();
}
// pv peakRelease (web/peak_world.inc release at T+7): a streamed location's terrain patches leave both terrain systems and its
// instances keep their slots without collision nodes (engine/world_body_collision.hpp releaseTrack). Returns the items released.
size_t browser_world_release_track(uint32_t track){
 size_t n=0;if(browserBodies)n+=browserBodies->releaseTrack(track);if(cameraTerrain)n+=cameraTerrain->releaseTrack(track);
 return n; // (contact caches match by resource: a released patch is simply not visited again)
}
// pv finishFences (above): on for this world; the loaded instances too (a streamed world's start row is in before free-ride.js starts).
EMSCRIPTEN_KEEPALIVE void stage_object_route(int on){browserObjectRoutes=on!=0;if(browserBodies)object_route_instances(*browserBodies);}
// Diagnostics (docs/peak-mountain.md "Course limits"): the collision state of every package instance, for comparison with the
// PS2's instance+8 / +0xC: [count, then per instance resource, runtime flags (-1: none), bits: route (0 skip, 1 static, 2 entity),
// 4 unsupported, 8 track resident, 16 released, 32 entity geometry].
EMSCRIPTEN_KEEPALIVE uint32_t* world_instance_states(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,0u);if(!browserBodies)return out.data();
 for(const auto& i:browserBodies->instances){if(i.runtimeClone)continue;
  const auto route=i.eventRuntimeFlags?originalInstanceBodyRoute(*i.eventRuntimeFlags,true):OriginalInstanceBodyRoute::Static;
  const uint32_t r=route==OriginalInstanceBodyRoute::Static?1u:route==OriginalInstanceBodyRoute::Entity?2u:0u;
  out.insert(out.end(),{i.resource,i.eventRuntimeFlags.value_or(0xffffffffu),r|(i.unsupported.empty()?0u:4u)|(ssx::worldResident(i.resource)?8u:0u)|(i.released?16u:0u)|(i.entity?32u:0u)});++out[0];}
 return out.data();
}
EMSCRIPTEN_KEEPALIVE void init_body_terrain(const char* text){
 if(!browserBodies)throw std::runtime_error("Body terrain requires its world package");
 const std::string key=browserWorldAppend?std::string():parse_key(text,bodySourceHash);
 if(!browserWorldAppend&&terrainParseCache.key==key&&!terrainParseCache.terrain.empty()){browserBodies->terrain=terrainParseCache.terrain;bodyTerrainReady=true;bodyContactCache={};return;}
 const auto data=json::parse(text);
 WorldBodyCollision candidate;candidate.terrain=body_terrain_patches(data);
 if(browserWorldAppend){browserBodies->appendTerrainTraversal(std::move(candidate.terrain));bodyTerrainReady=true;return;} // streamed location: staged until peak_world_commit (web/peak_world.inc)
 candidate.prepareTerrainTraversal();terrainParseCache={key,candidate.terrain};browserBodies->terrain=std::move(candidate.terrain);bodyTerrainReady=true;bodyContactCache={};
}
// pv eventSlices: see event_world_seal above init_world_collision.
// pv eventInWorldAi (docs/ctm-events-in-world.md stage 4): the event package loaded in parts into a computer rider's context of the
// streamed world, whose camera terrain (shared by every context, web/core.cpp) is the streamed world's own: the parts then build the
// context's body collision only (terrain_part leaves the camera terrain alone, the seal commits no camera-terrain append). The event
// package's collision is exactly its resident locations' (web/test-ctm-event-world.mjs), so this is the streamed world's resident set.
RIDER_LOCAL static bool eventSliceBodiesOnly=false;
EMSCRIPTEN_KEEPALIVE void event_world_begin(){eventSlicing=true;eventSliceBodiesOnly=false;eventSliceSetPieceCalls=setPieceInstanceCalls;}
EMSCRIPTEN_KEEPALIVE void event_world_bodies_only(){if(!eventSlicing)throw std::runtime_error("event_world_bodies_only without event_world_begin");eventSliceBodiesOnly=true;}
EMSCRIPTEN_KEEPALIVE void terrain_part(const char* text){
 if(!eventSlicing||!browserWorldAppend||!browserBodies||(!cameraTerrain&&!eventSliceBodiesOnly))throw std::runtime_error("terrain_part outside an event load in parts");
 auto data=json::parse(text);if(!eventSliceBodiesOnly)browser_camera_terrain_json(data);browserBodies->appendTerrainTraversal(body_terrain_patches(data));bodyTerrainReady=true;
}
EMSCRIPTEN_KEEPALIVE void event_world_seal(const char* worldKey,const char* terrainKey){
 if(!eventSlicing||!browserBodies||(!cameraTerrain&&!eventSliceBodiesOnly))throw std::runtime_error("event_world_seal without event_world_begin");
 browserWorldAppend=false;eventSlicing=false;
 if(!eventSliceBodiesOnly)cameraTerrain->finishAppend();eventSliceBodiesOnly=false;browserBodies->commitTerrainTraversal();browserBodies->nextInsertion=0;
 auto terrain=std::move(browserBodies->terrain);browserBodies->terrain.clear();
 {BodyParse c{worldKey,bodyParseCache.location,bodySourceHash,bodyParseCache.tracks,std::make_unique<const WorldBodyCollision>(*browserBodies),setPieceInstanceCalls!=eventSliceSetPieceCalls};bodyParseCache=std::move(c);}
 bodyTerrainReady=false;bodyContactCache={};object_route_instances(*browserBodies);browser_attach_set_pieces();
 terrainParseCache={terrainKey,terrain};browserBodies->terrain=std::move(terrain);bodyTerrainReady=true;bodyContactCache={};
}
// A computer rider's context (web/ai-racers.js, pv eventSlices): the parse caches by the documents' keys, without their text; 0 when the
// cache holds another document (the caller then passes the text).
EMSCRIPTEN_KEEPALIVE int init_world_collision_cached(const char* key){if(!bodyParseCache.bodies||bodyParseCache.key!=key)return 0;world_collision_from_cache();return 1;}
EMSCRIPTEN_KEEPALIVE int init_body_terrain_cached(const char* key){
 if(!browserBodies||terrainParseCache.key!=key||terrainParseCache.terrain.empty())return 0;
 browserBodies->terrain=terrainParseCache.terrain;bodyTerrainReady=true;bodyContactCache={};return 1;
}
// Tests: the parse key the core gives a text (web/peak-world-batches.js parseKey computes the same off the main thread).
EMSCRIPTEN_KEEPALIVE const char* parse_key_of(const char* text,const char* salt){RIDER_LOCAL static std::string out;out=parse_key(text,salt?salt:"");return out.c_str();}
// Tests (web/test-event-slices.mjs): FNV-1a over the body collision world, its terrain and the parse caches.
EMSCRIPTEN_KEEPALIVE uint32_t body_load_hash(){
 uint32_t h=0x811c9dc5u;auto mix=[&](const void* p,size_t n){const auto* b=static_cast<const uint8_t*>(p);for(size_t i=0;i<n;++i){h^=b[i];h*=0x01000193u;}};
 auto u32=[&](uint32_t v){mix(&v,4);};auto str=[&](const std::string& t){u32(uint32_t(t.size()));mix(t.data(),t.size());};
 auto terrainList=[&](const std::vector<WorldCollisionTerrain>& list){u32(uint32_t(list.size()));for(const auto& t:list){u32(t.resource);u32(t.flags);u32(uint32_t(t.surface));mix(t.low.data(),12);mix(t.high.data(),12);mix(t.grid.data(),sizeof(t.grid));u32(uint32_t(t.spatial.level));mix(t.spatial.coordinate.data(),12);u32(t.insertionOrder);}};
 auto hashWorld=[&](const WorldBodyCollision& w){
  u32(uint32_t(w.instances.size()));
  for(const auto& i:w.instances){
   u32(i.resource);u32(i.type);u32(i.flags);mix(&i.scale,4);mix(i.low.data(),12);mix(i.high.data(),12);str(i.unsupported);
   u32(i.eventRuntimeFlags.has_value());u32(i.eventRuntimeFlags.value_or(0));u32(i.rayAlwaysEmpty);u32(i.runtimeClone);u32(i.entityPointer);
   u32(i.entity!=nullptr);if(i.entity){mix(i.entity->low.data(),12);mix(i.entity->high.data(),12);u32(uint32_t(i.entity->world.size()));for(const auto& m:i.entity->world)mix(m.data(),64);for(const auto& m:i.entity->inverse)mix(m.data(),64);}
   u32(uint32_t(i.hierarchyLocal.size()));for(const auto& m:i.hierarchyLocal)mix(m.data(),64);for(uint32_t x:i.hierarchyParent)u32(x);mix(i.authoredMatrix.data(),64);
   u32(i.answerBox.has_value());if(i.answerBox)for(const auto& v:*i.answerBox)mix(v.data(),12);
   u32(uint32_t(i.nodes.size()));
   for(const auto& n:i.nodes){
    u32(n.index);u32(n.type);u32(uint32_t(n.surface));u32(n.collisionFlags);u32(n.collisionValue);u32(n.collisionAuxiliary);mix(n.world.data(),64);mix(n.inverse.data(),64);
    mix(n.low.data(),12);mix(n.high.data(),12);u32(n.doubleSided);u32(n.sphereTreeResource);
    u32(n.triangles!=nullptr);if(n.triangles){u32(uint32_t(n.triangles->vertices.size()));for(const auto& v:n.triangles->vertices)mix(v.data(),12);for(const auto& v:n.triangles->normals)mix(v.data(),12);u32(uint32_t(n.triangles->indices.size()));for(unsigned x:n.triangles->indices)u32(x);}
    u32(n.sphereTree!=nullptr);if(n.sphereTree){const auto& t=*n.sphereTree;mix(t.centerCm.data(),12);mix(&t.radiusScale,4);u32(t.compressed);u32(uint32_t(t.levels.size()));for(const auto& l:t.levels){mix(&l.radiusCm,4);mix(&l.childOffsetCm,4);u32(l.stride);}u32(uint32_t(t.masks.size()));mix(t.masks.data(),t.masks.size());}
   }
  }
  terrainList(w.terrain);u32(uint32_t(w.pendingTerrain.size()));u32(w.nextInsertion);
 };
 u32(browserBodies!=nullptr);if(browserBodies)hashWorld(*browserBodies);str(bodySourceHash);u32(bodyTerrainReady);
 str(bodyParseCache.key);str(bodyParseCache.location);str(bodyParseCache.sourceHash);u32(bodyParseCache.setPieces);u32(uint32_t(bodyParseCache.tracks.size()));for(const auto& [t,n]:bodyParseCache.tracks){u32(uint32_t(t));str(n);}
 u32(bodyParseCache.bodies!=nullptr);if(bodyParseCache.bodies)hashWorld(*bodyParseCache.bodies);
 str(terrainParseCache.key);terrainList(terrainParseCache.terrain);
 return h;
}
// QA: the unsupported collision instances ([count, (resource, authored flags)...], at most 256), e.g. entity instances that
// only a stage program routes (flags 0: the peak-run fences) and dynamic instances without a seed.
EMSCRIPTEN_KEEPALIVE uint32_t* world_collision_unsupported(){
 RIDER_LOCAL static std::vector<uint32_t> out;out.assign(1,0u);if(!browserBodies)return out.data();
 for(const auto& i:browserBodies->instances)if(!i.runtimeClone&&!i.unsupported.empty()&&out[0]<256){out.push_back(i.resource);out.push_back(i.flags);++out[0];}
 return out.data();
}
EMSCRIPTEN_KEEPALIVE int* world_collision_info(){
 RIDER_LOCAL static int counts[11];std::fill(std::begin(counts),std::end(counts),0);if(!browserBodies)return counts;
 for(const auto& i:browserBodies->instances){if(i.runtimeClone)continue; /*set-piece clones (set_piece_gameplay.inc) are not package instances*/counts[0]++;if(i.type<4)counts[1+i.type]++;if(!i.unsupported.empty())counts[5]++;counts[6]+=i.nodes.size();if(i.eventRuntimeFlags){counts[8]+=originalInstanceBodyRoute(*i.eventRuntimeFlags,true)==OriginalInstanceBodyRoute::Skip;counts[9]+=originalInstanceRayRoute(*i.eventRuntimeFlags,true,0)==OriginalInstanceBodyRoute::Skip;counts[10]+=originalInstanceRayRoute(*i.eventRuntimeFlags,true,2)==OriginalInstanceBodyRoute::Skip;}}counts[7]=browserBodies->terrain.size();return counts;
}
}

// Rider contact caches shared with the crash runtime (web/crash_runtime.hpp): rider+0x868 body, rider+0x864 terrain (core.cpp).
RIDER_LOCAL extern ssx::terrain_original::ContactCache groundCache;
namespace ssx {terrain_original::ContactCache& browser_rider_body_cache(){return bodyContactCache;}terrain_original::ContactCache& browser_rider_terrain_cache(){return groundCache;}}
#include "peak_world.inc" // streamed Peak 1 locations (docs/peak-mountain.md)
// Per rider context (web/rider_context.cpp): construct this translation unit's RIDER_LOCAL_LAZY containers.
void rider_statics_world(){rider_touch(&peak_stream::residency);}
static const bool riderStaticsWorldReady=(rider_statics_world(),true);
#ifdef SSX_SNAPSHOT_REGISTRY
// The rider-context snapshot's hook (web/world_snapshot.hpp; docs/replay.md §2a): the collision world's run-time instance state
// (the PS2's entity buckets 26D988 / 26DDC0: flags, entity placement, answer boxes, matrices), its geometry kept
// (web/snapshot-policy.mjs). The streaming's eviction words (released, reinsertion) are the world cache's (3A6800): not restored.
// A restore needs the countdown's instance count (set-piece clones are made at their setup, not in a race).
#include "world_snapshot.hpp"
namespace {
// Every instance: its eventRuntimeFlags (value, engaged) and bits; the dynamic ones (an entity, a clone, an entity pointer or an
// answer box at the save) also their entity, answer box and matrix. A static instance's matrix is its authored one (QA: hashed).
struct BodyRuntime{size_t count=0;std::vector<uint32_t> flags;std::vector<uint8_t> bits;std::vector<uint32_t> dynamic;std::vector<std::shared_ptr<const WorldCollisionEntity>> entities;std::vector<std::optional<std::array<terrain_original::Vector,2>>> boxes;std::vector<collision_transform::Matrix> matrices;uint64_t staticMatrices=0;};
RIDER_LOCAL_LAZY std::array<BodyRuntime,2> bodySnapshot;
bool body_dynamic(const WorldCollisionInstance& i){return i.entity||i.runtimeClone||i.entityPointer||i.answerBox;}
uint64_t body_static_matrices(const BodyRuntime& s){uint64_t h=1469598103934665603ull;size_t d=0;for(size_t k=0;k<browserBodies->instances.size();++k){if(d<s.dynamic.size()&&s.dynamic[d]==k){++d;continue;}h=ssx_snapshot::fnv(&browserBodies->instances[k].authoredMatrix,sizeof(collision_transform::Matrix),h);}return h;}
void body_snapshot_save(unsigned slot){auto& s=bodySnapshot[slot];s.count=browserBodies?browserBodies->instances.size():0;s.flags.resize(s.count);s.bits.resize(s.count);s.dynamic.clear();s.entities.clear();s.boxes.clear();s.matrices.clear();
 for(size_t k=0;k<s.count;++k){const auto& i=browserBodies->instances[k];s.flags[k]=i.eventRuntimeFlags.value_or(0);s.bits[k]=uint8_t((i.eventRuntimeFlags?1:0)|(i.entityPointer?2:0)|(i.rayAlwaysEmpty?4:0)|(i.runtimeClone?8:0));
  if(body_dynamic(i)){s.dynamic.push_back(uint32_t(k));s.entities.push_back(i.entity);s.boxes.push_back(i.answerBox);s.matrices.push_back(i.authoredMatrix);}}
 s.staticMatrices=browserBodies&&ssx_snapshot::qa?body_static_matrices(s):0;}
bool body_snapshot_check(unsigned slot){const auto& s=bodySnapshot[slot];if((browserBodies?browserBodies->instances.size():0)!=s.count)return false;return !(browserBodies&&ssx_snapshot::qa&&body_static_matrices(s)!=s.staticMatrices);}
bool body_snapshot_restore(unsigned slot){auto& s=bodySnapshot[slot];if(!body_snapshot_check(slot))return false;
 size_t d=0;for(size_t k=0;k<s.count;++k){auto& i=browserBodies->instances[k];const uint8_t b=s.bits[k];i.eventRuntimeFlags=b&1?std::optional<uint32_t>(s.flags[k]):std::nullopt;i.entityPointer=b&2;i.rayAlwaysEmpty=b&4;i.runtimeClone=b&8;
  if(d<s.dynamic.size()&&s.dynamic[d]==k){i.entity=s.entities[d];i.answerBox=s.boxes[d];i.authoredMatrix=s.matrices[d];++d;}else{i.entity.reset();i.answerBox.reset();}}
 reset_body_queries();return true;} // (the sphere-tree / contact caches: re-derived)
uint64_t body_snapshot_hash(){uint64_t h=1469598103934665603ull;if(!browserBodies)return h;for(const auto& i:browserBodies->instances){const uint32_t f=i.eventRuntimeFlags.value_or(0xFFFFFFFFu);h=ssx_snapshot::fnv(&f,4,h);h=ssx_snapshot::mix(h,reinterpret_cast<uintptr_t>(i.entity.get()));h=ssx_snapshot::fnv(&i.authoredMatrix,sizeof i.authoredMatrix,h);h=ssx_snapshot::mix(h,(i.answerBox?1:0)|(i.entityPointer?2:0)|(i.rayAlwaysEmpty?4:0)|(i.runtimeClone?8:0));if(i.answerBox)h=ssx_snapshot::fnv(i.answerBox->data(),sizeof(*i.answerBox),h);}return h;}
size_t body_snapshot_bytes(){size_t n=0;for(const auto& s:bodySnapshot)n+=s.flags.capacity()*4+s.bits.capacity()+s.dynamic.capacity()*4+s.entities.capacity()*sizeof(s.entities[0])+s.boxes.capacity()*sizeof(s.boxes[0])+s.matrices.capacity()*sizeof(s.matrices[0]);return n;}
struct BodySnapshotHook{BodySnapshotHook(){ssx_snapshot::hooks().push_back({"browserBodies (run-time state)",&body_snapshot_save,&body_snapshot_restore,&body_snapshot_hash,&body_snapshot_bytes,&body_snapshot_check});}};
[[maybe_unused]] BodySnapshotHook bodySnapshotHook;
}
#endif
#ifdef SSX_SNAPSHOT_REGISTRY // the rider-context snapshot's registry (web/generate-snapshot-registry.mjs, docs/replay.md §2a)
#include "generated/snapshot/world_bridge.inc"
#endif
