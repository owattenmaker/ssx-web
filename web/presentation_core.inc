// Presentation leftovers in the core (docs/presentation.md; included by web/animation_bridge.cpp after
// web/stage_script_gameplay.inc, whose stage VM / stage world it uses):
//   - cutscene stage calls (pv plane): a script's kind-7 channel-0 point event (0x2808E8) looks its hash up in the course
//     location's stage globals (0x309E50) and runs that function (0x2224B8 with the track's globals). web/cutscenes.js sends
//     the calls no cutscene set owns here: the new-career plane's snow spray, ABC1 global program 2 = 0x0D9F751E (builtin 25:
//     a Particle on ospreySpray_1001, 60 particles, continuous) / its cleanup 0x0D905E74 (builtin 69 mode 0: a Particle's
//     stop is a no-op, so the spray keeps emitting, as in PS2 new-career s3200..3330 after the jump). The world's entity pass
//     runs under the NIS on the PS2; the page's world is paused behind a cutscene, so the effects those calls built advance
//     with the script's ticks (stage_cutscene_tick);
//   - the painter region (pv regionTick): gp+0x770 for the located Fog / Lighting records (web/environment_bridge.cpp).

// gp+0x770 (web/weather.inc): the track of the human's last contacted patch, set by 2ED490 after block 0's wrappers; -1
// until the first contact of a world.
int browser_painter_region_track(){return weatherRegionTrack;}

RIDER_LOCAL static std::vector<uint32_t> stageCutsceneResources; // instances whose effects cutscene calls built
RIDER_LOCAL static std::vector<uint32_t> stageCutsceneBuilt; // the same, kept for the world (the run start's stage reset keeps them)
// web/stage_world.inc stage_world_reset: a race start in the same world keeps the entities cutscene calls built (a Particle with
// Duration -1 emits until its location goes; the PS2 does not reset the free-ride world when the run starts).
static std::vector<stage_world::Entity> stage_cutscene_kept(){
 std::vector<stage_world::Entity> out;if(!stageWorldLoaded||stageWorldLocation!=setPieceLocationName){stageCutsceneBuilt.clear();return out;}
 for(const auto& e:stageEntities)if(e.alive&&std::find(stageCutsceneBuilt.begin(),stageCutsceneBuilt.end(),e.resource)!=stageCutsceneBuilt.end())out.push_back(e);
 return out;
}
static void stage_cutscene_restore(std::vector<stage_world::Entity>& kept){for(auto& e:kept){e.serial=++stageEntitySerial;stageEntities.push_back(std::move(e));}kept.clear();}
// The track's globals table: the race's stage VM once the world's first run built it (reset_race), else a private VM for the
// cutscene calls (the drop NIS plays before that run; the PS2 runs each location's global handler programs at its read
// completion, 3ACA10 + 230430): the track's programs, its global handler programs run with every builtin a no-op (they define
// the track's functions; the race's VM runs them again with their effects), so nothing else of the stage state changes.
RIDER_LOCAL static std::optional<ssx::OriginalScriptVM> stageCutsceneVm;RIDER_LOCAL static std::string stageCutsceneVmLocation;
RIDER_LOCAL static std::vector<std::pair<uint32_t,ssx::OriginalScriptValue>> stageCutsceneGlobals;
static const ssx::OriginalScriptValue* stage_cutscene_globals(int track,ssx::OriginalScriptVM*& vm){
 if(stageVm)for(const auto& [t,v]:stageTrackGlobals)if(int(t)==track){vm=&*stageVm;return &v;}
 if(stageCutsceneVmLocation!=setPieceLocationName){stageCutsceneVm.reset();stageCutsceneGlobals.clear();stageCutsceneVmLocation=setPieceLocationName;}
 for(const auto& [t,v]:stageCutsceneGlobals)if(int(t)==track){vm=&*stageCutsceneVm;return &v;}
 const auto tables=stage_tables_for(setPieceLocationName);if(!tables)return nullptr;
 for(const auto& st:tables->stages){
  if(int(st.track)!=track)continue;
  if(!stageCutsceneVm)stageCutsceneVm.emplace();
  std::vector<uint32_t> handles;for(uint32_t k=0;k<st.programCount;k++){const auto& p=tables->programs[st.firstProgram+k];handles.push_back(stageCutsceneVm->addProgram(ssx::originalScriptProgramFromWords(tables->words.data()+p.first,p.count)));}
  const auto globals=ssx::OriginalScriptVM::tableValue(stageCutsceneVm->newTable(64));
  for(uint32_t g=0;g<st.globalCount;g++){const int program=tables->globals[st.firstGlobal+g];if(program<0||uint32_t(program)>=handles.size())continue;
   try{auto r=stageCutsceneVm->run(handles[size_t(program)],0,globals,[](int,const ssx::OriginalScriptArg*,int){return ssx::OriginalScriptValue{};});stageCutsceneVm->release(r);}catch(const std::exception&){}}
  stageCutsceneGlobals.emplace_back(st.track,globals);vm=&*stageCutsceneVm;return &stageCutsceneGlobals.back().second;
 }
 return nullptr;
}
// Runs the global function `symbol` of stage track `track` (1: called, 0: no such function / no stage). (x, y, z): where the
// human rider is for the call (source cm; NaN: the core's own position). Builtin 25 creates its effect only with a rider
// within 30000 cm (0x2FEB24, (gp-0x84C)+0x550 = 2 in CTM free ride too: PS2 new-career fr); under the drop NIS the PS2's
// rider rides in the plane, while the page places its rider only when the run starts.
extern "C" EMSCRIPTEN_KEEPALIVE int stage_global_call(int track,uint32_t symbol,float x,float y,float z){
 if(track<0||track>255)return 0;
 ssx::OriginalScriptVM* vm=nullptr;const ssx::OriginalScriptValue* globals=stage_cutscene_globals(track,vm);
 if(!globals||!vm)return 0;
 const auto function=vm->get(*globals,ssx::OriginalScriptValue::symbol(symbol));
 if(function.type!=ssx::kOriginalScriptFunction)return 0;
 std::vector<std::pair<uint32_t,size_t>> before;for(const auto& e:stageEntities)if(e.alive)before.push_back({e.resource,e.effects.size()});
 const int32_t previousTrack=stageRunningTrack;const uint32_t previousInstance=stage_current_instance;const auto previousPosition=physicsState.position;
 stageRunningTrack=track;if(std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z))physicsState.position={x,y,z};
 auto restore=[&]{stageRunningTrack=previousTrack;stage_current_instance=previousInstance;physicsState.position=previousPosition;};
 try{auto result=vm->call(function,*globals,[](int index,const ssx::OriginalScriptArg* args,int count){return stage_builtin_value(index,args,count,false);});vm->release(result);}
 catch(...){restore();throw;}
 restore();
 for(const auto& e:stageEntities){ // an entity the call built, or one it added an effect to
  if(!e.alive)continue;size_t had=0;bool known=false;for(const auto& [r,n]:before)if(r==e.resource){had=n;known=true;break;}
  if(known&&e.effects.size()<=had)continue;
  if(std::find(stageCutsceneResources.begin(),stageCutsceneResources.end(),e.resource)==stageCutsceneResources.end())stageCutsceneResources.push_back(e.resource);
  if(std::find(stageCutsceneBuilt.begin(),stageCutsceneBuilt.end(),e.resource)==stageCutsceneBuilt.end())stageCutsceneBuilt.push_back(e.resource);
 }
 return 1;
}
// One NIS tick of those effects (the entity pass's effect-list update 0x355748). Returns the effects updated.
extern "C" EMSCRIPTEN_KEEPALIVE int stage_cutscene_tick(){
 if(!stageWorldLoaded)return 0;int n=0;
 for(uint32_t r:stageCutsceneResources)for(auto& e:stageEntities){
  if(!e.alive||e.resource!=r)continue;
  for(auto& x:e.effects){
   if(!x.dynamic){ssx::originalParticleEffectUpdate(x.burst);++n;continue;}
   if(x.trail.dead)continue;
   stage_world::PMat node;if(!stage_node_matrix(r,x.trail.node,node))node=stage_entity_matrix(r);
   ssx::originalDynamicParticleUpdate(x.trail,node,stage_visual_random());++n;
  }
 }
 return n;
}
// The list ended (web/cutscenes.js): the run's entity pass owns the effects from here on.
extern "C" EMSCRIPTEN_KEEPALIVE void stage_cutscene_end(){stageCutsceneResources.clear();}
