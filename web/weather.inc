// Weather (docs/weather.md): the camera's Weather painter (environment block 6, stepped by the camera update 0x15EBCC ->
// 0x2ED490), the snowfall object 0x2E5DA0 (group 2, first) with its layers 0x2E4F50, the camera splash 0x2F39E0 (group 2,
// after the flag manager), the lightning chance 0x2F00A0 reads for ScreenTint 0x390C60 (group 3). Included by
// web/animation_bridge.cpp before the stage world (web/stage_world.inc calls weather_group2_*).
// The rider's own Weather painter (block 0: breath + wind push) is breathEnvironment (weather_painter_step above).
#include "../engine/weather.hpp"
RIDER_LOCAL static bool weatherReady=false,weatherKeepState=false;
RIDER_LOCAL static OriginalEnvironmentRegions weatherRegions;RIDER_LOCAL static std::vector<OriginalEnvironmentPayload> weatherPayloads;
RIDER_LOCAL static std::array<float,19> weatherDefaults{};
RIDER_LOCAL static OriginalEnvironmentTransition weatherCamera;RIDER_LOCAL static float weatherShelter=0; // camera block +0x24
RIDER_LOCAL static std::vector<ssx::OriginalSnowfallLayer> weatherLayers;
RIDER_LOCAL static ssx::OriginalCameraSplash weatherSplash;RIDER_LOCAL static bool weatherSplashLive=false;
RIDER_LOCAL static std::array<float,4> weatherEye{0,0,0,1};RIDER_LOCAL static bool weatherEyeSet=false; // camera +0x20 after the last camera update
RIDER_LOCAL static float weatherLightningChance=0; // ScreenTint +0x1524 = 0x2EE738 (camera block property 10), set by 0x2F00A0
RIDER_LOCAL static uint32_t weatherLayerDraws=0,weatherSplashLcg=0,weatherSplashVisual=0,weatherCameraSteps=0,weatherSerial=0;
// Event-start seed (weather.json "ready": the ready savestate = the start of game tick 0).
struct WeatherSeed {bool set=false,rider=false,camera=false,splash=false;OriginalEnvironmentTransition riderPainter,cameraPainter;float shelter=0;
 std::vector<ssx::OriginalSnowfallLayer> layers;ssx::OriginalCameraSplash splashState;std::array<float,4> eye{0,0,0,1};float lightningChance=0;int32_t lightningCounter=-1,lightningDelay=-1;};
RIDER_LOCAL static WeatherSeed weatherSeed;
// Without a ready state (free ride, peak worlds): the snowfall object as 0x2E5920 builds it for one camera (4 flake layers,
// 2 fluff layers, 0x2E4D88: timer 0 so the first update draws a gust, +0xB0 = 1). The load-time seeds (3 visual RNG words
// per layer) are not on this core's stream: a fixed LCG stands in (presentation only: they place the flakes).
static std::vector<ssx::OriginalSnowfallLayer> weather_default_layers(){
 std::vector<ssx::OriginalSnowfallLayer> out;uint32_t w=0x6FF75EA3u;auto seed=[&]{w=w*1664525u+1013904223u;return w;};
 const std::array<std::array<float,5>,6> rows{{{0,1500,3,1,-200},{0,1500,3,1,-200},{0,2000,3,1,-200},{0,3000,3,1,-200},{1,2000,600,std::bit_cast<float>(0x3d75c28fu),-100},{1,2000,600,std::bit_cast<float>(0x3d75c28fu),-100}}};
 for(const auto& r:rows){ssx::OriginalSnowfallLayer L;L.kind=int32_t(r[0]);L.extent=r[1];L.speed=r[2];L.size=r[3];L.alpha=r[3];L.gravity=r[4];
  L.offset={0,0,0,1};L.seeds={seed(),seed(),seed()};L.first=1;L.timer=0;out.push_back(L);}
 return out;
}
static float weather_bits(const nlohmann::json& v){return v.is_number_unsigned()?std::bit_cast<float>(v.get<uint32_t>()):v.get<float>();}
static void weather_parse_painter(const nlohmann::json& p,OriginalEnvironmentTransition& t){
 t.distance=weather_bits(p.at("distance"));const auto& last=p.at("last");t.lastX=weather_bits(last.at(0));t.lastY=weather_bits(last.at(1));
 for(unsigned i=0;i<19;i++){t.properties.current[i]=weather_bits(p.at("current").at(i));t.properties.target[i]=weather_bits(p.at("target").at(i));}
}
static ssx::OriginalSnowfallLayer weather_parse_layer(const nlohmann::json& l){
 ssx::OriginalSnowfallLayer L;L.camera=l.at("camera");L.kind=l.at("kind");L.count=l.at("count");
 L.extent=weather_bits(l.at("extent"));L.speed=weather_bits(l.at("speed"));L.size=weather_bits(l.at("size"));L.alpha=weather_bits(l.at("alpha"));L.gravity=weather_bits(l.at("gravity"));
 for(unsigned k=0;k<4;k++){L.offset[k]=weather_bits(l.at("offset").at(k));L.gustNew[k]=weather_bits(l.at("gust_new").at(k));L.gustOld[k]=weather_bits(l.at("gust_old").at(k));}
 L.timer=weather_bits(l.at("timer"));L.period=weather_bits(l.at("period"));for(unsigned k=0;k<3;k++){L.seeds[k]=l.at("seeds").at(k).get<uint32_t>();L.colour[k]=weather_bits(l.at("colour").at(k));}
 L.first=l.at("first");return L;
}
static ssx::OriginalCameraSplash weather_parse_splash(const nlohmann::json& s){
 ssx::OriginalCameraSplash o;o.camera=s.value("camera",0);o.hasPrevious=s.at("has_previous");for(unsigned k=0;k<4;k++)o.previous[k]=weather_bits(s.at("previous").at(k));
 o.speed=weather_bits(s.at("speed"));o.pending=weather_bits(s.at("pending"));o.snowfall=weather_bits(s.at("snowfall"));
 for(const auto& d:s.value("drops",nlohmann::json::array())){ssx::OriginalSplashDrop x;const auto& f=d.at("f");x.alive=d.at("alive");x.x=weather_bits(f.at(0));x.y=weather_bits(f.at(1));x.w=weather_bits(f.at(2));x.h=weather_bits(f.at(3));x.vx=weather_bits(f.at(4));x.vy=weather_bits(f.at(5));x.age=weather_bits(f.at(6));x.life=weather_bits(f.at(7));for(unsigned k=0;k<4;k++)x.colour[k]=weather_bits(f.at(8+k));x.angle=weather_bits(f.at(12));x.spin=weather_bits(f.at(13));o.drops.push_back(x);}
 for(const auto& d:s.value("crystals",nlohmann::json::array())){ssx::OriginalSplashCrystal x;const auto& f=d.at("f");x.spawned=d.at("spawned");x.alive=d.at("alive");x.x=weather_bits(f.at(0));x.y=weather_bits(f.at(1));x.w=weather_bits(f.at(2));x.h=weather_bits(f.at(3));x.vx=weather_bits(f.at(4));x.vy=weather_bits(f.at(5));x.age=weather_bits(f.at(6));x.life=weather_bits(f.at(7));for(unsigned k=0;k<4;k++)x.colour[k]=weather_bits(f.at(8+k));x.angle=weather_bits(f.at(12));x.spin=weather_bits(f.at(13));x.timer=weather_bits(f.at(14));x.ax=weather_bits(f.at(15));x.ay=weather_bits(f.at(16));o.crystals.push_back(x);}
 return o;
}
// A weather state (weather.json "ready", or a mid-run savestate's, tools/export_weather.py): what it holds is applied.
static void weather_parse_state(const nlohmann::json& r,WeatherSeed& s){
 s={};s.set=true;
 if(r.contains("rider")){weather_parse_painter(r["rider"],s.riderPainter);s.rider=true;}
 if(r.contains("camera")){weather_parse_painter(r["camera"],s.cameraPainter);s.shelter=r["camera"].contains("shelter")?weather_bits(r["camera"]["shelter"]):0.f;s.camera=true;}
 if(r.contains("layers"))for(const auto& l:r["layers"])s.layers.push_back(weather_parse_layer(l));
 if(r.contains("splash")&&!r["splash"].is_null()){s.splashState=weather_parse_splash(r["splash"]);s.splash=true;}
 if(r.contains("eye")){for(unsigned k=0;k<4;k++)s.eye[k]=weather_bits(r["eye"].at(k));}
 if(r.contains("lightning_chance"))s.lightningChance=weather_bits(r["lightning_chance"]);
 if(r.contains("lightning")){s.lightningCounter=r["lightning"].value("counter",-1);s.lightningDelay=r["lightning"].value("delay",-1);}
}
static void weather_lightning_seed(int32_t counter,int32_t delay); // below, with the lightning state
static void weather_apply_state(const WeatherSeed& s){
 if(!s.set)return;
 if(s.rider&&snowReady)breathEnvironment=s.riderPainter;
 if(s.camera){weatherCamera=s.cameraPainter;weatherShelter=s.shelter;}
 if(!s.layers.empty())weatherLayers=s.layers;
 if(s.splash){weatherSplash=s.splashState;weatherSplashLive=true;}
 weatherEye=s.eye;weatherEyeSet=true; // camera +0x20 after the last camera update (the splash's next position)
 weatherLightningChance=s.lightningChance;weather_lightning_seed(s.lightningCounter,s.lightningDelay);++weatherSerial;
}
struct WeatherLightning;static void weather_lightning_reset();
// 0x2C03E8 -> 0x2C03A8: every painter wrapper (all environment blocks: riders' and cameras') gets +0 = -99999; each wrapper's
// next step blends with -1 (jumps to its payload). 0x111890 (a rider's FX reset: reset placement 11D660, location entry
// 11DE60, teleports 1234A4 / 123598 / 1238AC) calls it, for any rider: a computer rider's reset reaches the human's painters
// (web/shared_world.inc event 8). PS2 dbc2-weather 1657: Nate's reset placement re-seeds the human's Weather and the camera's.
void browser_environment_painters_reset(); // web/environment_bridge.cpp (Fog, Lighting)
RIDER_LOCAL static uint32_t weatherPainterResets=0;
static void weather_painters_reset(){breathEnvironment.distance=-99999.f;weatherCamera.distance=-99999.f;browser_environment_painters_reset();++weatherPainterResets;}
void shared_world_log(std::initializer_list<uint32_t>); // web/shared_world.inc
static void weather_rider_fx_reset(){
 weather_painters_reset();shared_world_log({8u});
 if(browserHumanRider&&weatherReady)ssx::originalSplashReset(weatherSplash); // 0x111890: the splash of camera rider+0x870 (-1 for computer riders)
}
// 0x2E47E8, the render of the camera's fade object (vtable 0x488158; update 0x2E46C8 in group 3): while a fade of the
// rendered camera is on screen with opacity >= 0.93 (gp-0x3A1C), 0x2C03E8 resets every world painter wrapper -- once per
// rendered frame, i.e. after each game tick's camera update. In the simulation the fade is the rider reset's (12F230 ->
// 2E4370: out 0.5 s, hold 0, in 0.5 s, its clock bound to the reset control's progress 12F398; only for a rider with a
// camera, rider+0x870: the human): opacity = engine/reset_fade.hpp originalResetFadeAlpha(progress), >= 0.93 on the three
// ticks of progress 0.475 / 0.5 / 0.525 around the placement. The world's other fades (NIS steps 277980, the world-state
// 15 white fade 236058) are drawn by web/cutscenes.js, which calls weather_fade_reset at the same threshold per frame.
RIDER_LOCAL static uint32_t weatherFadeResets=0;
static void weather_fade_painters_reset(){weather_painters_reset();shared_world_log({8u});++weatherFadeResets;}
static void weather_fade_render(){
 if(!(browserResetActive&&resetInputs.deviceEnabled&&resetInputs.deviceIndex>=0))return;
 if(originalResetFadeAlpha(resetControl.progress)>=std::bit_cast<float>(0x3f6e147bu))weather_fade_painters_reset(); // c.le.s 0.93, opacity
}
extern "C" EMSCRIPTEN_KEEPALIVE void weather_fade_reset(){weather_fade_painters_reset();} // web/cutscenes.js: a drawn fade at opacity >= 0.93
static void weather_reset_counters(){weatherLayerDraws=weatherSplashLcg=weatherSplashVisual=weatherCameraSteps=0;weather_lightning_reset();}
// The event start (start_event, after reset_animation): the ready savestate's state; else the painters' initial state.
void weather_event_seed_apply(){
 if(!weatherReady)return;weather_reset_counters();
 if(weatherSeed.set){weather_apply_state(weatherSeed);if(browserEventRolling){weather_painters_reset();ssx::originalSplashReset(weatherSplash);}return;} // the rolling start places the riders (0x111890: PS2 dbc2-weather record 0 has the camera block at -99999, record 1 the splash reset)
 weatherCamera={};weatherShelter=0;weatherLightningChance=0;weatherEyeSet=false;weatherSplash={};weatherSplashLive=false;weatherLayers=weather_default_layers();++weatherSerial;
}
// weather.json (tools/export_weather.py): {version:1, location, regions:{scale, origin, root, outside, nodes}, payloads:[{transition,
// values[19]}], defaults[19], ready?:{rider, camera, layers[], splash, eye, lightning_chance}}. The regions replace the rider's
// (breath) regions too: both blocks read the current location's painter.
struct WeatherLocation {OriginalEnvironmentRegions regions;std::vector<OriginalEnvironmentPayload> payloads;std::array<float,19> defaults{};};
static WeatherLocation weather_parse_package(const nlohmann::json& d){
 if(d.at("version")!=1)throw std::runtime_error("Unsupported weather.json");
 WeatherLocation L;const auto& r=d.at("regions");L.regions.scale=r.at("scale");L.regions.originX=r.at("origin").at(0);L.regions.originY=r.at("origin").at(1);L.regions.root=r.at("root");L.regions.outside=r.at("outside");
 for(const auto& n:r.at("nodes"))L.regions.nodes.push_back({n.get<std::array<uint16_t,4>>()});
 for(const auto& p:d.at("payloads"))L.payloads.push_back({p.at("transition").get<float>(),p.at("values").get<std::array<float,19>>()});
 if(L.regions.nodes.empty()||L.payloads.empty())throw std::runtime_error("Empty weather painter");
 L.defaults=d.at("defaults").get<std::array<float,19>>();return L;
}
// ---- streamed worlds (PEAK1..3, MOUNTAIN): one Weather section per location record ------------------------------------
// 2C0778 selects the painter record of location gp+0x770 for EVERY environment block (riders 0..5 and cameras 6/7): the
// track table entry (G+0x84 -> +0x10 -> +8 -> +4, 12 bytes per track) must be loaded (short state 3) with a record
// (+8, +4 > 0) holding a type-12 section, else the property object takes the class defaults with distance 0 (vt+0x228 =
// 2BE258); then the section's point tree at (x, y) as for a single location (engine/environment_transition.hpp). gp+0x770
// is written only by 2ED490 for block 0 (the human, 0x1218D0), after that block's wrappers have stepped: the track of
// rider+0x430 (the contact patch; -1 in the air keeps it). So the human's painters step with the previous tick's region,
// the computer riders' and the camera's (0x15EBCC, after the rider manager) with this tick's. web/peak-world.js feeds each
// location's weather.json with its collision data (weather_location), so a crossing switches on the PS2's tick instead of
// when the page's asynchronous fetch lands (the old web/main.js region hook: init_weather with keep_state is now ignored
// while located packages exist). Course events keep their one package (none located: the old path).
RIDER_LOCAL static std::vector<WeatherLocation> weatherLocationPackages;
RIDER_LOCAL static std::array<uint16_t,256> weatherLocationSlot{}; // track -> package index + 1 (0: not loaded)
RIDER_LOCAL static int32_t weatherRegionTrack=-1; // gp+0x770
RIDER_LOCAL static uint32_t weatherRegionChanges=0;
static bool weather_located(){return !weatherLocationPackages.empty();}
static const WeatherLocation* weather_region_package(){
 if(weatherRegionTrack<0||weatherRegionTrack>255)return nullptr;const uint16_t k=weatherLocationSlot[size_t(weatherRegionTrack)];
 return k?&weatherLocationPackages[k-1u]:nullptr;
}
// One wrapper step (2C0778) of a streamed world; false: no located packages (a course event: the caller's single package).
static bool weather_located_step(OriginalEnvironmentTransition& s,float x,float y){
 if(!weather_located())return false;
 if(const auto* L=weather_region_package()){originalEnvironmentTransitionStep(s,L->regions,L->payloads,L->defaults,x,y);return true;}
 // 0x2C09D8: no loaded record for the region: 2BE258 (current = the class defaults, +0 = 0) after the distance / last update.
 s.lastX=x;s.lastY=y;s.properties.current=weatherDefaults;s.distance=0;return true;
}
// 2ED490 block 0 (after its wrappers): gp+0x770 = rider+0x430 & 0xFF unless -1 (the human's rider pass; computer riders skip it).
static void weather_region_update(int patch){
 if(!browserHumanRider||patch==-1)return;const int32_t t=patch&0xFF;if(t!=weatherRegionTrack){weatherRegionTrack=t;++weatherRegionChanges;}
}
extern "C" EMSCRIPTEN_KEEPALIVE void weather_keep_state(int keep){weatherKeepState=keep!=0;}
extern "C" EMSCRIPTEN_KEEPALIVE int init_weather(const char* text){
 if(weatherKeepState&&weatherReady&&weather_located())return 1; // a streamed world's region hook: the core selects by gp+0x770
 if(!weatherKeepState){weatherLocationPackages.clear();weatherLocationSlot.fill(0);weatherRegionTrack=-1;} // a new world
 if(!text||!*text){weatherReady=false;return 0;}
 const auto d=nlohmann::json::parse(text);const auto L=weather_parse_package(d);
 weatherRegions=L.regions;weatherPayloads=L.payloads;weatherDefaults=L.defaults;
 if(snowReady){breathRegions=weatherRegions;breathPayloads=weatherPayloads;breathDefaults=weatherDefaults;}
 if(weatherKeepState&&weatherReady)return 1;
 weatherSeed={};if(d.contains("ready")&&d["ready"].is_object())weather_parse_state(d["ready"],weatherSeed);
 weatherReady=true;weather_event_seed_apply();return 1;
}
// A streamed-world location's weather.json (web/peak-world.js, with the location's collision data): its section becomes the
// painter record of `track`. The first one also starts the world's weather (no ready state: the painters' initial state).
extern "C" EMSCRIPTEN_KEEPALIVE int weather_location(int track,const char* text){
 if(track<0||track>255||!text||!*text)return 0;
 auto L=weather_parse_package(nlohmann::json::parse(text));const uint16_t k=weatherLocationSlot[size_t(track)];
 if(k)weatherLocationPackages[k-1u]=std::move(L);else{weatherLocationPackages.push_back(std::move(L));weatherLocationSlot[size_t(track)]=uint16_t(weatherLocationPackages.size());}
 weatherDefaults=weatherLocationPackages.front().defaults;
 if(!weatherReady){weatherSeed={};weatherReady=true;weather_event_seed_apply();}
 return int(weatherLocationPackages.size());
}
// Mid-run seeds (web/peak-capture.mjs): gp+0x770 of the baseline savestate (the track of its last contacted patch).
extern "C" EMSCRIPTEN_KEEPALIVE void weather_region_seed(int track){weatherRegionTrack=track>=0&&track<256?track:-1;}
// [region track gp+0x770, changes, located packages, loaded (1/0) for the region, fade resets].
extern "C" EMSCRIPTEN_KEEPALIVE int32_t* weather_region_info(){RIDER_LOCAL static int32_t v[5];v[0]=weatherRegionTrack;v[1]=int32_t(weatherRegionChanges);v[2]=int32_t(weatherLocationPackages.size());v[3]=weather_region_package()?1:0;v[4]=int32_t(weatherFadeResets);return v;}
extern "C" EMSCRIPTEN_KEEPALIVE void reset_weather(){weather_event_seed_apply();}
// The countdown grid (start_event, after begin_event_rider): 2ED490 steps the rider's painters at rider+0x460/+0x464, the grid
// contact of the ready state, which is the rider painter's last point there (it steps every pre-race tick at that point); the
// port's contact is the seed position (reset_rider), ~0.01 cm away, and that offset reached the painter distance at the
// departure (PS2 weather/era5-reset 187: 9.2589 vs 9.2653 cm), moving a later transition blend by a tick (1139).
void weather_event_contact_seed(){
 if(!weatherReady||!weatherSeed.set||!weatherSeed.rider||browserEventRolling)return;
 browserTrailContact[0]=weatherSeed.riderPainter.lastX;browserTrailContact[1]=weatherSeed.riderPainter.lastY;
}
// A mid-run savestate (compare-ps2-capture --seed-visual-rng with web/set_world_visual_state "weather").
static void weather_set_state(const nlohmann::json& r){WeatherSeed s;weather_parse_state(r,s);weather_apply_state(s);}
// 0x15EBCC: the camera's environment block (view + 6) at the camera eye (+0x20/+0x24), after the camera update; also
// keeps the eye (+0x20 quad, w 1) for the next tick's camera splash.
void browser_weather_camera_step(float x,float y,float z){
 if(weatherReady){
 if(!weather_located_step(weatherCamera,x,y)){
  const auto& regions=weatherRegions.nodes.empty()?breathRegions:weatherRegions;const auto& payloads=weatherPayloads.empty()?breathPayloads:weatherPayloads;
  originalEnvironmentTransitionStep(weatherCamera,regions,payloads,weatherDefaults,x,y);}
 ++weatherCameraSteps;
 // 0x2E6008 -> 0x2E5430 (the frame's render, after this camera update): every layer of the camera moves by -(eye - the eye
 // of the last render, snowfall object +0x40); presentation only (the layers' LCG draws do not depend on the offset).
 const auto eye=std::array<float,4>{x,y,z,1.f};
 if(weatherEyeSet){terrain_original::Rounding rounding;const ssx::weather::Quad d{terrain_original::sub(eye[0],weatherEye[0]),terrain_original::sub(eye[1],weatherEye[1]),terrain_original::sub(eye[2],weatherEye[2]),terrain_original::sub(1.f,1.f)};
  for(auto& L:weatherLayers)if(L.camera==0)ssx::originalSnowfallCameraShift(L,d);}
 weatherEye=eye;weatherEyeSet=true;
 }
 weather_fade_render(); // the frame's render after this camera update: 0x2E47E8 (painter resets under a fade)
}
// Group 2, first: the snowfall object 0x2E5DA0 (records per camera, 0x2F4330 into each camera's splash, then its layers).
static bool weather_group2_snowfall(uint32_t& lcg){
 if(!weatherReady||weatherLayers.empty())return false;
 const auto rec=ssx::originalWeatherRecord(weatherCamera.properties.current,weatherShelter);
 ssx::originalSplashSnowfall(weatherSplash,rec[0]);
 for(auto& L:weatherLayers)if(L.camera==0)weatherLayerDraws+=ssx::originalSnowfallLayerUpdate(L,rec,lcg);
 return true;
}
// Group 2, after the flag manager: the camera-0 splash 0x2F39E0 (camera 1's object returns before any draw in 1P).
static bool weather_group2_splash(uint32_t& lcg,OriginalRandomState& visual){
 if(!weatherReady||weatherLayers.empty())return false;
 const auto draws=ssx::originalSplashUpdate(weatherSplash,weatherEye,lcg,[&]{return visual.next();});
 weatherSplashLcg+=draws.lcg;weatherSplashVisual+=draws.visual;weatherSplashLive=true;return true;
}
// 0x2E1598 -> 0x2F4118: a large impact (0x2E162C: kind 5, rider+0xAC4 == 0) near the camera feeds the splash.
static void weather_splash_impact(const SnowVector& position,float strength){
 if(!weatherReady||weatherLayers.empty())return;ssx::originalSplashImpact(weatherSplash,{position[0],position[1],position[2],1.f},strength);
}
// 0x2F00A0 after the group-3 components: the lightning chance the next update's 0x390C60 reads (camera block property 10).
static void weather_group3_refresh(){if(weatherReady)weatherLightningChance=weatherCamera.properties.current[10];}
static float weather_lightning_chance(){return weatherReady?weatherLightningChance:0.f;}
// QA / renderer: [ready, serial, camera steps, layer LCG draws, splash LCG draws, splash visual draws, lightning chance,
// eye x y z, camera distance, then the camera block's 19 current properties, the 16-float record, painter resets (46)].
extern "C" EMSCRIPTEN_KEEPALIVE float* weather_info(){
 RIDER_LOCAL static std::array<float,11+19+16+1> v{};v.fill(0);v[46]=float(weatherPainterResets);
 v[0]=weatherReady;v[1]=float(weatherSerial);v[2]=float(weatherCameraSteps);v[3]=float(weatherLayerDraws);v[4]=float(weatherSplashLcg);v[5]=float(weatherSplashVisual);v[6]=weatherLightningChance;
 v[7]=weatherEye[0];v[8]=weatherEye[1];v[9]=weatherEye[2];v[10]=weatherCamera.distance;
 for(unsigned i=0;i<19;i++)v[11+i]=weatherCamera.properties.current[i];
 const auto rec=ssx::originalWeatherRecord(weatherCamera.properties.current,weatherShelter);for(unsigned i=0;i<16;i++)v[30+i]=rec[i];
 return v.data();
}
// Snowfall layers for web/weather-renderer.js: [count, then per layer 25 words: camera, kind, count, extent, speed, size, alpha,
// gravity, offset x y z, gust new x y z, gust old x y z, timer, period, seeds 0..2 (raw words), colour r g b] as float bits.
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t* weather_layers(){
 RIDER_LOCAL static std::vector<uint32_t> out;auto B=[](float x){return std::bit_cast<uint32_t>(x);};out.assign(1,0u);
 for(const auto& L:weatherLayers){++out[0];out.insert(out.end(),{uint32_t(L.camera),uint32_t(L.kind),uint32_t(L.count),B(L.extent),B(L.speed),B(L.size),B(L.alpha),B(L.gravity),
  B(L.offset[0]),B(L.offset[1]),B(L.offset[2]),B(L.gustNew[0]),B(L.gustNew[1]),B(L.gustNew[2]),B(L.gustOld[0]),B(L.gustOld[1]),B(L.gustOld[2]),B(L.timer),B(L.period),L.seeds[0],L.seeds[1],L.seeds[2]});
  out.insert(out.end(),{B(L.colour[0]),B(L.colour[1]),B(L.colour[2])});}
 return out.data();
}
// Camera splash for the renderer (0x2F3E28): [drops, crystals, speed, pending, snowfall, then per drop 14 floats (x, y, w, h,
// age, life, A, R, G, B, angle, spin, vx, vy), then per crystal 14 floats (same order)].
extern "C" EMSCRIPTEN_KEEPALIVE float* weather_splash(){
 RIDER_LOCAL static std::vector<float> out;const auto& s=weatherSplash;out.assign({float(s.drops.size()),float(s.crystals.size()),s.speed,s.pending,s.snowfall});
 for(const auto& d:s.drops)out.insert(out.end(),{d.x,d.y,d.w,d.h,d.age,d.life,d.colour[0],d.colour[1],d.colour[2],d.colour[3],d.angle,d.spin,d.vx,d.vy});
 for(const auto& c:s.crystals)out.insert(out.end(),{c.x,c.y,c.w,c.h,c.age,c.life,c.colour[0],c.colour[1],c.colour[2],c.colour[3],c.angle,c.spin,c.vx,c.vy});
 return out.data();
}
// ---- ScreenTint lightning 0x390C60 (group 3 component 7) ------------------------------------------------------------
// Tweakables 0x24A228 (gp+0x1528..): phases 2/2/2/7 ticks, intensities 0.4/1.0/0.5, colours (A R G? B) 1..3; Frame Id -1.
struct WeatherLightning {int32_t counter=-1,delay=0,thunders=0;float intensity=0,distance=0;std::array<float,3> colour{};uint32_t strikes=0,draws=0;};
RIDER_LOCAL static WeatherLightning weatherLightning;
RIDER_LOCAL static std::array<int32_t,2> weatherLightningSeed{-1,-1};
static void weather_lightning_reset(){weatherLightning={};weatherLightning.counter=weatherLightningSeed[0];weatherLightning.delay=weatherLightningSeed[1];}
static void weather_lightning_seed(int32_t counter,int32_t delay){weatherLightningSeed={counter,delay};weather_lightning_reset();}
static constexpr std::array<int32_t,4> weatherFlashPhases{2,2,2,7};
static const std::array<float,3> weatherFlashIntensity{std::bit_cast<float>(0x3ecccccdu),1.f,.5f};
static const std::array<std::array<float,3>,3> weatherFlashColour{{{std::bit_cast<float>(0x3fcac083u),std::bit_cast<float>(0x3ff3d70au),std::bit_cast<float>(0x3fffbe77u)},
 {std::bit_cast<float>(0x3eaaa64cu),std::bit_cast<float>(0x3edc28f6u),std::bit_cast<float>(0x3ffeb852u)},{.75f,.75f,2.f}}};
// 0x390EC8: a strike at distance (cm): the flash restarts and the thunder waits distance / 553.3 cm per tick.
static void weather_lightning_strike(float distance){
 terrain_original::Rounding rounding;auto& L=weatherLightning;L.counter=0;L.intensity=0;L.distance=distance;
 L.delay=ssx::weather::cvtWS(terrain_original::mul(distance,std::bit_cast<float>(0x3aece08au)));++L.strikes;
}
// One update while the game flow enables it (0x151C): returns the visual draws (0x390CA0 / 0x390CE0).
static uint32_t weather_lightning_update(OriginalRandomState& visual){
 terrain_original::Rounding rounding;auto& L=weatherLightning;uint32_t draws=0;
 if(L.delay==0)L.delay=-1; // the audio poll 0x390EF8 (0x285BF8, once per update): the thunder plays and the delay becomes -1
 if(L.delay>0)--L.delay;
 if(L.counter<0){
  const float chance=weather_lightning_chance();
  const uint32_t r=visual.next();++draws;const float u=originalScalarSubtract(std::bit_cast<float>((r&0x7FFFFFu)|0x3F800000u),1.f);
  if(u<terrain_original::mul(chance,chance)){
   const uint32_t r2=visual.next();++draws;
   float d=terrain_original::mul(originalScalarSubtract(std::bit_cast<float>((r2&0x7FFFFFu)|0x3F800000u),1.f),std::bit_cast<float>(0x3ffd70a4u));
   d=originalScalarAdd(d,std::bit_cast<float>(0x3ca3d70au));weather_lightning_strike(terrain_original::mul(d,100000.f));
  }
  L.draws+=draws;return draws;
 }
 const int32_t c=++L.counter,p1=weatherFlashPhases[0],p12=p1+weatherFlashPhases[1],p123=p12+weatherFlashPhases[2],all=p123+weatherFlashPhases[3];
 if(c<=p1){L.intensity=terrain_original::mul(originalScalarDivide(float(c),float(p1)),weatherFlashIntensity[0]);L.colour=weatherFlashColour[0];}
 else if(c<=p12){L.intensity=weatherFlashIntensity[0];L.colour=weatherFlashColour[0];}
 else if(c<=p123){L.intensity=weatherFlashIntensity[1];L.colour=weatherFlashColour[1];}
 else if(c<=all){const float k=originalScalarDivide(originalScalarSubtract(originalScalarSubtract(originalScalarSubtract(float(c),float(p1)),float(weatherFlashPhases[1])),float(weatherFlashPhases[2])),float(weatherFlashPhases[3]));
  L.intensity=terrain_original::mul(originalScalarSubtract(1.f,k),weatherFlashIntensity[2]);L.colour=weatherFlashColour[2];}
 else {L.counter=-1;L.intensity=0;L.colour={};}
 return 0;
}
// 0x390EF8 (audio 0x285BF8): the thunder when the delay reaches 0 after a strike (once).
extern "C" EMSCRIPTEN_KEEPALIVE int weather_thunder_poll(){auto& L=weatherLightning;if(L.delay==0&&L.strikes>uint32_t(L.thunders)){++L.thunders;return 1;}return 0;}
// [counter, intensity, colour r g b, distance cm, thunder delay, strikes, draws, chance].
extern "C" EMSCRIPTEN_KEEPALIVE float* weather_lightning(){RIDER_LOCAL static std::array<float,10> v{};const auto& L=weatherLightning;
 v={float(L.counter),L.intensity,L.colour[0],L.colour[1],L.colour[2],L.distance,float(L.delay),float(L.strikes),float(L.draws),weather_lightning_chance()};return v.data();}
