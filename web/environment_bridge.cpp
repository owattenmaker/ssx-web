#include <algorithm>
#include "rider_local.hpp"
#include "../engine/environment_lighting.hpp"
#include "../engine/environment_irradiance.hpp"
#include "../engine/lighting_brightness.hpp"
#include "../engine/air_trajectory.hpp"
#include "json.hpp"
#include <emscripten/emscripten.h>
#include <unordered_map>
#include <map>
#include <bit>
#include <cstring>
using namespace ssx;using nlohmann::json;
RIDER_LOCAL extern int browserGroundPatch;RIDER_LOCAL extern float browserGroundU,browserGroundV;RIDER_LOCAL extern OriginalAirTrajectory browserTrajectory;
RIDER_LOCAL_LAZY static std::unordered_map<uint32_t,OriginalEnvironmentPatch> patches;
// The environment textures (~5 MB on Snow Jam) never change after the load: every rider context of the core holds the
// same copy (the parse of the same package; web/rider_local.hpp), the rest of the environment state is per rider.
RIDER_LOCAL static std::shared_ptr<const std::vector<OriginalEnvironmentTexture>> textures;
namespace {std::string environmentTextureKey;std::shared_ptr<const std::vector<OriginalEnvironmentTexture>> environmentTextureCache;}
// The whole-mountain world (tools/export_mountain_world.py, web/free-ride.js): the lattices of every location are 68 MB, so
// each location's patches and textures are added when its data loads (environment_add) and dropped when it is released
// (environment_drop); texture ids are global there (track << 12 | index). Presentation only (rider lighting, no physics).
RIDER_LOCAL static bool environmentStreamed=false;
RIDER_LOCAL_LAZY static std::unordered_map<int32_t,std::shared_ptr<const OriginalEnvironmentTexture>> streamedTextures;
RIDER_LOCAL static OriginalEnvironmentGlobals globals;RIDER_LOCAL static OriginalEnvironmentState state;
RIDER_LOCAL static bool ready=false,gap=false;RIDER_LOCAL static unsigned updates=0;
RIDER_LOCAL static OriginalIrradianceCoefficients irradiance{},brightBank{},darkBank{},alternateBank{};
RIDER_LOCAL static bool irradianceReady=false;RIDER_LOCAL static unsigned irradianceUpdates=0;
RIDER_LOCAL static float irradianceBrightness=0,irradianceGain=0,irradianceIncoming=0;
// The rider's environment selector (block 0x4FA370 + i x 0xF0, +0x24): 2ED490 (1218D0, after the stage triggers 121818 of the
// same tick) eases it x gp-0x3974 (0.9) and adds gp-0x3970 (0.1) while rider+0x3FC (stage builtin 74, DBC2's tunnel volumes) is
// set; above 0.1 the rider irradiance takes the alternate bank. PS2 tunnel/dbc2-tunnel-long: 0.1 at 9831, 0.19, 0.271 ...
RIDER_LOCAL static float environmentSelector=0;
void browser_environment_selector_step(bool tunnel){
 OriginalRounding rounding;environmentSelector=terrain_original::mul(environmentSelector,std::bit_cast<float>(0x3f666666u));
 if(tunnel)environmentSelector=terrain_original::add(environmentSelector,std::bit_cast<float>(0x3dcccccdu));
}
extern "C" EMSCRIPTEN_KEEPALIVE float environment_selector(){return environmentSelector;} // QA
static void setup_lighting_painter(const json& irradiance); // below: spatial Lighting painter (type 11)
extern "C" EMSCRIPTEN_KEEPALIVE void init_environment(const char* metadata,const uint8_t* data,int length){
 const auto j=json::parse(metadata);if(j.at("version")!=1)throw std::runtime_error("Environment package version");textures.reset();patches.clear();environmentSelector=0;
 environmentStreamed=j.value("streamed",false);streamedTextures.clear();
 globals.multiplier=j.at("multiplier").get<EnvironmentColour>();globals.airAmbient=j.at("air_ambient").get<EnvironmentColour>();globals.airRatio=j.at("air_ratio").get<EnvironmentColour>();
 state.ambient=j.at("initial_ambient").get<EnvironmentColour>();state.ratio=j.at("initial_ratio").get<EnvironmentColour>();globals.forceNext=j.at("force_next");
 {uint32_t h=0x811c9dc5u;for(int i=0;i<length;++i){h^=data[i];h*=0x01000193u;}const std::string key=j.at("textures").dump()+":"+std::to_string(length)+":"+std::to_string(h);
  if(environmentTextureCache&&environmentTextureKey==key)textures=environmentTextureCache;
  else{auto built=std::make_shared<std::vector<OriginalEnvironmentTexture>>();
 for(const auto& t:j.at("textures")){OriginalEnvironmentTexture texture;texture.width=t.at("width");texture.height=t.at("height");size_t cells=(texture.width+1)*(texture.height+1),rgba=t.at("rgba_offset"),valid=t.at("valid_offset");if(rgba+cells*4>size_t(length)||valid+cells>size_t(length))throw std::runtime_error("Environment texture bounds");texture.rgba.resize(cells);std::memcpy(texture.rgba.data(),data+rgba,cells*4);texture.valid.assign(data+valid,data+valid+cells);built->push_back(std::move(texture));}
   textures=built;environmentTextureCache=std::move(built);environmentTextureKey=key;}}
 for(const auto& p:j.at("patches")){OriginalEnvironmentPatch patch;patch.resource=p.at("resource");patch.flags=p.at("flags");patch.eligible=p.at("eligible");patch.lightUV=p.at("light_uv").get<std::array<float,4>>();patch.baseUV=p.at("base_uv").get<std::array<std::array<float,2>,4>>();patch.textures=p.at("textures").get<std::array<int,3>>();patches.emplace(patch.resource,patch);}
 irradiance={};irradianceUpdates=0;irradianceBrightness=irradianceGain=irradianceIncoming=0;irradianceReady=j.contains("irradiance");
 if(irradianceReady){const auto& i=j.at("irradiance");brightBank=i.at("bright").get<OriginalIrradianceCoefficients>();darkBank=i.at("dark").get<OriginalIrradianceCoefficients>();alternateBank=i.at("alternate").get<OriginalIrradianceCoefficients>();irradianceGain=i.at("gain");irradianceIncoming=i.at("incoming");
  for(const auto* bank:{&brightBank,&darkBank,&alternateBank})for(const auto& row:*bank)for(float value:row)if(!std::isfinite(value))throw std::runtime_error("Nonfinite irradiance coefficient");
  if(!std::isfinite(irradianceGain)||!std::isfinite(irradianceIncoming))throw std::runtime_error("Nonfinite lighting weight");
 }
 setup_lighting_painter(irradianceReady?j.at("irradiance"):json());
 ready=true;gap=false;updates=0;
}
EnvironmentColour browser_environment_colour(int mode,EnvironmentColour fallback){
 if(!ready)return fallback;auto patch=[](int id)->const OriginalEnvironmentPatch*{auto found=patches.find(uint32_t(id));return found==patches.end()?nullptr:&found->second;};
 OriginalEnvironmentFrame frame;frame.motionMode=mode;frame.groundPatch=patch(browserGroundPatch);frame.u=browserGroundU;frame.v=browserGroundV;frame.predictionStatus=browserTrajectory.status;frame.predictedPatch=patch(browserTrajectory.patchId);frame.predictedU=browserTrajectory.patchU;frame.predictedV=browserTrajectory.patchV;frame.predictedTime=browserTrajectory.predictedTime;frame.elapsed=browserTrajectory.elapsed;
 try{originalEnvironmentUpdate(state,globals,frame,[](int id,float u,float v){
   if(environmentStreamed){auto t=streamedTextures.find(id);if(t==streamedTextures.end())throw OriginalEnvironmentUnavailable("Environment texture not streamed in");return originalEnvironmentTextureSample(*t->second,u,v);}
   return originalEnvironmentTextureSample(textures->at(id),u,v);});gap=false;}catch(const OriginalEnvironmentUnavailable&){gap=true;}
 if(irradianceReady&&!gap){
  irradianceBrightness=originalLightingBrightness(state.ratio);
  // The selector (above: stage builtin 74's tunnels) picks the alternate bank above 0.1. Local lights and rim composition remain
  // separate from this bank.
  irradiance=originalEnvironmentIrradiance(irradiance,brightBank,darkBank,alternateBank,environmentSelector,irradianceBrightness,irradianceGain,irradianceIncoming);
  ++irradianceUpdates;
 }
 ++updates;return state.ambient;
}
extern "C" EMSCRIPTEN_KEEPALIVE float* environment_info(){RIDER_LOCAL static float v[7];v[0]=ready;v[1]=gap;v[2]=updates;for(unsigned i=0;i<4;i++)v[3+i]=state.ambient[i];return v;}

extern "C" EMSCRIPTEN_KEEPALIVE float* environment_irradiance(){return irradiance[0].data();}
extern "C" EMSCRIPTEN_KEEPALIVE float* environment_irradiance_info(){RIDER_LOCAL static float values[10];values[0]=irradianceReady;values[1]=gap;values[2]=irradianceUpdates;values[3]=irradianceBrightness;values[4]=irradianceGain;values[5]=irradianceIncoming;for(unsigned i=0;i<4;++i)values[6+i]=state.ratio[i];return values;}

#include "../engine/painter_tree.hpp"
#include "../engine/painter_driver.hpp"
#include "../engine/fog_painter.hpp"
#include "../engine/fog_depth_table.hpp"
namespace {
RIDER_LOCAL std::vector<std::array<uint16_t,4>> fogNodes;
struct FogPayload {float rate;std::array<float,6> values;};
RIDER_LOCAL std::vector<FogPayload> fogPayloads;
RIDER_LOCAL OriginalPainterTree fogTree;RIDER_LOCAL OriginalPainterDriverState fogDriver;RIDER_LOCAL OriginalFogPainterState fogState;
RIDER_LOCAL bool fogReady=false;RIDER_LOCAL int fogSelected=-1;RIDER_LOCAL unsigned fogTicks=0;
constexpr std::array<float,5> fogDefaults={3000.f,30000.f,.4300000071525574f,.550000011920929f,.7099999785423279f};
RIDER_LOCAL uint32_t fogOutside=0xffffffffu;
RIDER_LOCAL std::array<uint8_t,1024> fogPaletteBytes{};
RIDER_LOCAL OriginalFogDepthParameters fogDepthParameters;RIDER_LOCAL OriginalFogDepthProjection fogProjection;
RIDER_LOCAL float fogPaletteDensity=0;RIDER_LOCAL unsigned fogPaletteRevision=0;RIDER_LOCAL bool fogPaletteValid=false;
void prepare_fog_palette(float nearCm,float farCm){
 fogProjection=originalFogDepthProjection(nearCm,farCm,0,0x31);
 std::array<float,16> matrix{};matrix[10]=fogProjection.slope;matrix[11]=1;matrix[14]=fogProjection.offset;
 auto parameters=originalFogDepthParameters(matrix,fogState.current[1],fogState.current[2],{fogState.current[3],fogState.current[4],fogState.current[5]});
 if(fogPaletteValid&&parameters.nearBin==fogDepthParameters.nearBin&&parameters.farBin==fogDepthParameters.farBin&&parameters.rgb==fogDepthParameters.rgb&&fogPaletteDensity==fogState.current[0])return;
 std::array<uint32_t,256> packed{};originalFogDepthTable(packed,parameters.nearBin,parameters.farBin,1,fogState.current[0],parameters.rgb);
 for(unsigned index=0;index<256;++index){unsigned swizzled=(index&~24u)|((index&8u)<<1)|((index&16u)>>1);for(unsigned channel=0;channel<4;++channel)fogPaletteBytes[index*4+channel]=(packed[swizzled]>>(channel*8))&255;}
 fogDepthParameters=parameters;fogPaletteDensity=fogState.current[0];fogPaletteValid=true;++fogPaletteRevision;
}
}
extern "C" EMSCRIPTEN_KEEPALIVE void reset_fog(){
 fogDriver={};fogState.current={0,fogDefaults[0],fogDefaults[1],fogDefaults[2],fogDefaults[3],fogDefaults[4]};fogState.sample=fogState.current;fogSelected=-1;fogTicks=0;fogPaletteValid=false;++fogPaletteRevision;fogPaletteBytes={};fogProjection={};fogDepthParameters={};
}
// Streamed locations (web/free-ride.js, docs/peak-mountain.md): each location has its own Fog painter record; the painter
// region follows the rider's contacted location, so a region change swaps the tree and keeps the blend state.
RIDER_LOCAL static bool fogKeepState=false;
extern "C" EMSCRIPTEN_KEEPALIVE void fog_keep_state(int keep){fogKeepState=keep!=0;}
// A location record's Fog section (fog-tree.json): the point tree and payloads (init_fog, fog_location).
struct FogRecord {std::vector<std::array<uint16_t,4>> nodes;std::vector<FogPayload> payloads;float scale=1,originX=0,originY=0;uint16_t root=0;uint32_t outside=0xffffffffu;};
static FogRecord fog_parse(const char* text){
 const auto p=json::parse(text);if(p.at("version")!=1||!p.at("location").is_string()||!p.at("track").is_number_unsigned())throw std::runtime_error("Unsupported fog region");
 auto nodes=p.at("nodes").get<std::vector<std::array<uint16_t,4>>>();std::vector<FogPayload> payloads;
 for(const auto& row:p.at("payloads")){auto rgb=row.at("color").get<std::array<float,3>>();FogPayload value{row.at("blend_rate"),{row.at("mode"),row.at("near_cm"),row.at("far_cm"),rgb[0],rgb[1],rgb[2]}};if(!std::isfinite(value.rate))throw std::runtime_error("Invalid fog rate");for(auto x:value.values)if(!std::isfinite(x))throw std::runtime_error("Invalid fog value");payloads.push_back(value);}
 const auto origin=p.at("origin").get<std::array<float,2>>();float scale=p.at("scale");unsigned root=p.at("root");
 if(!std::isfinite(scale)||scale<=0||!std::isfinite(origin[0])||!std::isfinite(origin[1])||root>=nodes.size()||nodes.size()>32768||payloads.empty())throw std::runtime_error("Invalid fog tree");
 for(auto node:nodes){if(node[0]&1){for(auto child:node)if((child>>1)>=nodes.size())throw std::runtime_error("Invalid fog child");}else{auto id=uint32_t(node[2])|(uint32_t(node[3])<<16);if(id!=0xffffffffu&&id>=payloads.size())throw std::runtime_error("Invalid fog leaf");}}
 const auto outside=p.at("outside_words").get<std::array<uint32_t,2>>();if(outside[0]&1||(outside[1]!=0xffffffffu&&outside[1]>=payloads.size()))throw std::runtime_error("Invalid fog outside leaf");
 return {std::move(nodes),std::move(payloads),scale,origin[0],origin[1],uint16_t(root),outside[1]};
}
static void fog_use(const FogRecord& r){fogNodes=r.nodes;fogPayloads=r.payloads;fogTree={r.scale,r.originX,r.originY,r.root,fogNodes};fogOutside=r.outside;}
// Streamed worlds with pv regionTick (docs/weather.md 10): every location record's Fog section (web/peak-world.js ->
// fog_location), selected in the camera block's step (0x15EBCC, after the human's 2ED490 set gp+0x770 this tick) by the
// painter region instead of the page's asynchronous hook (init_fog with fog_keep_state, then ignored). A region whose
// record is not loaded takes the class defaults with +0 = 0 (0x2C09D8). Until the world's first contact (gp+0x770 = -1)
// the course package's tree stays.
RIDER_LOCAL static std::vector<FogRecord> fogRecords;RIDER_LOCAL static std::array<uint16_t,256> fogRecordSlot{};
RIDER_LOCAL static int fogRecordApplied=-2;RIDER_LOCAL static bool fogRecordMissing=false;
int browser_painter_region_track(); // web/presentation_core.inc (gp+0x770)
extern "C" EMSCRIPTEN_KEEPALIVE void init_fog(const char* text){
 if(fogKeepState&&!fogRecords.empty())return; // the located records are selected by gp+0x770 (browser_fog_step)
 if(!fogKeepState){fogRecords.clear();fogRecordSlot.fill(0);fogRecordApplied=-2;fogRecordMissing=false;} // a new world
 fog_use(fog_parse(text));if(fogKeepState&&fogReady)return;fogReady=true;reset_fog();
}
extern "C" EMSCRIPTEN_KEEPALIVE int fog_location(int track,const char* text){
 if(track<0||track>255||!text||!*text)return 0;
 auto r=fog_parse(text);const uint16_t k=fogRecordSlot[size_t(track)];
 if(k)fogRecords[k-1u]=std::move(r);else{fogRecords.push_back(std::move(r));fogRecordSlot[size_t(track)]=uint16_t(fogRecords.size());}
 fogRecordApplied=-2;return int(fogRecords.size());
}
// [located records, applied region, missing (1/0)].
extern "C" EMSCRIPTEN_KEEPALIVE int32_t* fog_location_info(){RIDER_LOCAL static int32_t v[3];v[0]=int32_t(fogRecords.size());v[1]=fogRecordApplied;v[2]=fogRecordMissing;return v;}
void browser_fog_step(float x,float y,float nearCm,float farCm){
 if(!fogReady)return;if(!std::isfinite(x)||!std::isfinite(y))throw std::runtime_error("Nonfinite fog sample position");
 if(!fogRecords.empty()){
  const int t=browser_painter_region_track();
  if(t>=0&&t<256&&t!=fogRecordApplied){fogRecordApplied=t;const uint16_t k=fogRecordSlot[size_t(t)];fogRecordMissing=!k;if(k)fog_use(fogRecords[k-1u]);}
  if(fogRecordApplied>=0&&fogRecordMissing){fogDriver.lastX=x;fogDriver.lastY=y;originalFogPainterDefaults(fogState,fogDriver.distance,fogDefaults);fogSelected=-1;++fogTicks;prepare_fog_palette(nearCm,farCm);return;}
 }
 OriginalPainterDriverAccess access;
 access.sample=[&](float px,float py)->std::optional<OriginalPainterSample>{auto selected=originalPainterPayload(fogTree,px,py,fogOutside,fogPayloads.size());fogSelected=selected?int(*selected):-1;if(!selected)return {};return OriginalPainterSample{5,fogPayloads[*selected].rate};};
 access.matches=[](){return originalFogPainterMatches(fogState,fogPayloads.at(fogSelected).values);};
 access.blend=[](float weight){originalFogPainterBlend(fogState,fogPayloads.at(fogSelected).values,weight);};
 access.reset=[](){originalFogPainterDefaults(fogState,fogDriver.distance,fogDefaults);};
 originalPainterDriverStep(fogDriver,x,y,5,OriginalPainterAvailability::Ready,-99999.f,access);++fogTicks;prepare_fog_palette(nearCm,farCm);
}
extern "C" EMSCRIPTEN_KEEPALIVE float* fog_info(){RIDER_LOCAL static float result[11];for(unsigned i=0;i<6;++i)result[i]=fogState.current[i];result[6]=fogSelected;result[7]=fogTicks;result[8]=fogDriver.lastX;result[9]=fogDriver.lastY;result[10]=fogReady;return result;}

extern "C" EMSCRIPTEN_KEEPALIVE uint8_t* fog_palette_rgba(){return fogPaletteBytes.data();}
extern "C" EMSCRIPTEN_KEEPALIVE float* fog_palette_info(){RIDER_LOCAL static float v[9];v[0]=fogPaletteValid;v[1]=fogPaletteRevision;v[2]=fogDepthParameters.nearBin;v[3]=fogDepthParameters.farBin;v[4]=fogProjection.slope;v[5]=fogProjection.offset;v[6]=fogPaletteDensity;v[7]=1;v[8]=0x31;return v;}

// Spatial Lighting painter (type 11, class 0x484290): 2ED490 runs the rider's environment wrapper
// 2C0778 (vtable 0x483E00 slot 10) at rider+0x460/+0x464 with the -99999 weight sentinel, like the
// breath (type 12) and fog (type 5) painters. Blend 2BD5A8 copies the four 8-byte bank references and
// blends the two scalars (+0x48 gain, +0x50 rim); compare 2BDE30 checks all four references and both
// scalars; reset 2BE1F8 stores the null reference (gp+0x22E0) in all four slots and scalars (1, 0.75).
// 2EE010 resolves a null reference to the course default bank (gp+0x12D4). 2EDA4C then builds the
// rider bank from references 0/1/2 (bright/dark/alternate, 2EE318/2EE340/2EE368) and the gain.
// Metro-City (BRA2) selects five payloads (BPBRR/B/G/Y/2 bright banks) by area; PS2 savestates hold
// BPBRR at the grid and BPBRY at the glide checkpoint, both what this tree selects there.
#include "../engine/lighting_painter.hpp"
namespace {
struct LightingPayload {float rate=0;std::array<OriginalLightingReference,4> references{};std::array<float,2> values{};};
RIDER_LOCAL std::vector<std::array<uint16_t,4>> lightingNodes;RIDER_LOCAL std::vector<LightingPayload> lightingPayloads;
RIDER_LOCAL OriginalPainterTree lightingTree;RIDER_LOCAL uint32_t lightingOutside=0xffffffffu;RIDER_LOCAL OriginalPainterDriverState lightingDriver;RIDER_LOCAL OriginalLightingPainterState lightingState;
RIDER_LOCAL_LAZY std::map<uint64_t,OriginalIrradianceCoefficients> lightingBanks;RIDER_LOCAL uint64_t lightingDefaultBank=0;
RIDER_LOCAL bool lightingLive=false;RIDER_LOCAL int lightingSelected=-1;RIDER_LOCAL unsigned lightingTicks=0;
RIDER_LOCAL bool lightingStreamed=false,lightingHasSection=false; // streamed Peak 1 world (lighting_banks / lighting_region below)
OriginalLightingReference lighting_reference(const std::string& name){
 if(name.empty()||name.size()>8)throw std::runtime_error("Invalid Lighting reference name");std::array<char,8> bytes{};std::memcpy(bytes.data(),name.data(),name.size());
 OriginalLightingReference r;std::memcpy(r.data(),bytes.data(),8);return r;}
uint64_t lighting_key(const OriginalLightingReference& r){return uint64_t(r[0])|(uint64_t(r[1])<<32);}
const OriginalIrradianceCoefficients& lighting_bank(const OriginalLightingReference& r){
 const uint64_t key=r==OriginalLightingReference{}?lightingDefaultBank:lighting_key(r); // 2EE010: an empty name reads the default index gp+0x12D4
 auto found=lightingBanks.find(key);if(found==lightingBanks.end())throw std::runtime_error("Lighting reference has no exported bank");return found->second;}
void lighting_apply(){
 brightBank=lighting_bank(lightingState.references[0]);darkBank=lighting_bank(lightingState.references[1]);alternateBank=lighting_bank(lightingState.references[2]);irradianceGain=lightingState.values[0];}
}
// Located Lighting records (pv regionTick; lighting_streamed_step below).
RIDER_LOCAL static std::vector<std::string> lightingRecordText;RIDER_LOCAL static std::array<uint16_t,256> lightingRecordSlot{};
RIDER_LOCAL static int lightingRecordApplied=-2;RIDER_LOCAL static bool lightingRecordMissing=false;
static void setup_lighting_painter(const json& irradiance){
 lightingLive=false;lightingStreamed=lightingHasSection=false;lightingSelected=-1;lightingTicks=0;lightingDriver={};lightingState={};lightingNodes.clear();lightingPayloads.clear();lightingBanks.clear();lightingRecordText.clear();lightingRecordSlot.fill(0);lightingRecordApplied=-2;lightingRecordMissing=false;
 if(!irradiance.is_object()||!irradiance.contains("painter"))return;const auto& p=irradiance.at("painter");
 if(!p.at("spatially_varying").get<bool>())return; // uniform sections (ARA1, BHP1) keep the start payload, as before
 if(!p.contains("default_reference"))throw std::runtime_error("Spatial Lighting painter needs the course default bank (gp+0x12D4); re-run web/prepare-environment.py");
 for(const auto& [name,rows]:p.at("banks").items()){OriginalIrradianceCoefficients bank=rows.get<OriginalIrradianceCoefficients>();for(const auto& row:bank)for(float v:row)if(!std::isfinite(v))throw std::runtime_error("Nonfinite Lighting bank");lightingBanks[lighting_key(lighting_reference(name))]=bank;}
 lightingDefaultBank=lighting_key(lighting_reference(p.at("default_reference").get<std::string>()));if(!lightingBanks.contains(lightingDefaultBank))throw std::runtime_error("Missing default Lighting bank");
 for(const auto& e:p.at("entries")){LightingPayload payload;payload.rate=e.at("blend_rate");const auto names=e.at("references").get<std::vector<std::string>>();if(names.size()!=4)throw std::runtime_error("Lighting payload references");
  for(unsigned i=0;i<4;++i)payload.references[i]=lighting_reference(names[i]);payload.values=e.at("scalars").get<std::array<float,2>>();
  if(!std::isfinite(payload.rate)||!std::isfinite(payload.values[0])||!std::isfinite(payload.values[1]))throw std::runtime_error("Invalid Lighting payload");
  for(unsigned i=0;i<3;++i)if(!lightingBanks.contains(lighting_key(payload.references[i])))throw std::runtime_error("Lighting payload bank missing");lightingPayloads.push_back(payload);}
 const auto& t=p.at("tree");auto nodes=t.at("nodes").get<std::vector<std::array<uint16_t,4>>>();const auto origin=t.at("origin").get<std::array<float,2>>();float scale=t.at("scale");unsigned root=t.at("root");
 if(!std::isfinite(scale)||scale<=0||root>=nodes.size()||lightingPayloads.empty())throw std::runtime_error("Invalid Lighting tree");
 for(auto node:nodes){if(node[0]&1){for(auto child:node)if((child>>1)>=nodes.size())throw std::runtime_error("Invalid Lighting child");}else{auto id=uint32_t(node[2])|(uint32_t(node[3])<<16);if(id!=0xffffffffu&&id>=lightingPayloads.size())throw std::runtime_error("Invalid Lighting leaf");}}
 const auto outside=t.at("outside_words").get<std::array<uint32_t,2>>();if(outside[0]&1||(outside[1]!=0xffffffffu&&outside[1]>=lightingPayloads.size()))throw std::runtime_error("Invalid Lighting outside leaf");
 lightingNodes=std::move(nodes);lightingTree={scale,origin[0],origin[1],uint16_t(root),lightingNodes};lightingOutside=outside[1];
 // Until the first wrapper step the rider bank is the start payload (prepare-environment.py), as before.
 const auto& start=lightingPayloads.at(p.at("start_entry").get<unsigned>());lightingState.references=start.references;lightingState.values=start.values;lightingState.samples=start.values;lightingLive=true;
}
// A location record's Lighting section (lighting.json {painter: null | {entries, tree}}) into the painter's section; the driver
// state is kept (lighting_region, and the located records of pv regionTick below).
static int lighting_use_section(const char* text){
 const auto doc=json::parse(text);lightingPayloads.clear();lightingNodes.clear();lightingHasSection=false;
 if(!doc.contains("painter")||doc.at("painter").is_null())return 0;const auto& p=doc.at("painter");
 for(const auto& e:p.at("entries")){LightingPayload payload;payload.rate=e.at("blend_rate");const auto names=e.at("references").get<std::vector<std::string>>();if(names.size()!=4)throw std::runtime_error("Lighting payload references");
  for(unsigned i=0;i<4;++i)payload.references[i]=lighting_reference(names[i]);payload.values=e.at("scalars").get<std::array<float,2>>();
  for(unsigned i=0;i<3;++i)if(!lightingBanks.contains(lighting_key(payload.references[i])))throw std::runtime_error("Lighting payload bank missing");lightingPayloads.push_back(payload);}
 const auto& t=p.at("tree");auto nodes=t.at("nodes").get<std::vector<std::array<uint16_t,4>>>();const auto origin=t.at("origin").get<std::array<float,2>>();float scale=t.at("scale");unsigned root=t.at("root");
 if(!std::isfinite(scale)||scale<=0||root>=nodes.size()||lightingPayloads.empty())throw std::runtime_error("Invalid Lighting tree");
 for(auto node:nodes){if(node[0]&1){for(auto child:node)if((child>>1)>=nodes.size())throw std::runtime_error("Invalid Lighting child");}else{auto id=uint32_t(node[2])|(uint32_t(node[3])<<16);if(id!=0xffffffffu&&id>=lightingPayloads.size())throw std::runtime_error("Invalid Lighting leaf");}}
 const auto outside=t.at("outside_words").get<std::array<uint32_t,2>>();if(outside[0]&1||(outside[1]!=0xffffffffu&&outside[1]>=lightingPayloads.size()))throw std::runtime_error("Invalid Lighting outside leaf");
 lightingNodes=std::move(nodes);lightingTree={scale,origin[0],origin[1],uint16_t(root),lightingNodes};lightingOutside=outside[1];lightingHasSection=true;return int(lightingPayloads.size());
}
// Streamed worlds with pv regionTick (docs/weather.md 10): every location record's Lighting section (web/peak-world.js ->
// lighting_location, kept as text: its payloads need the banks, which arrive after the start locations), selected in the
// rider block's step by the painter region gp+0x770 as it stands before the human's 2ED490 update of this tick (the
// human's own painters switch the tick after the crossing, PS2 weather/frd-regions) instead of the page's hook
// (lighting_region, then ignored). A region whose record is not loaded: the class reset with +0 = 0 (0x2C09D8).
// Streamed Peak 1 world (web/free-ride.js, docs/peak-mountain.md): every location has its own world painter record with a
// Lighting section; 2C0778 samples the section of the current painter region (the rider's contacted location) and a region
// without one takes the missing-section path (2BE1F8 reset: empty references = the default bank gp+0x12D4, which 22E180
// sets to xPBR1 by course at every Load trigger and crossing end). Uniform sections run the driver too (their blend rate).
static void lighting_streamed_step(float x,float y){
 OriginalPainterDriverAccess access;
 if(!lightingRecordText.empty()){
  const int t=browser_painter_region_track();
  if(t>=0&&t<256&&t!=lightingRecordApplied){lightingRecordApplied=t;const uint16_t k=lightingRecordSlot[size_t(t)];lightingRecordMissing=!k;
   if(k){try{lighting_use_section(lightingRecordText[k-1u].c_str());}catch(const std::exception&){lightingPayloads.clear();lightingNodes.clear();lightingHasSection=false;}}}
  if(lightingRecordApplied>=0&&lightingRecordMissing){lightingDriver.lastX=x;lightingDriver.lastY=y;lightingDriver.distance=0;lightingState.references={};lightingState.values={1.f,.75f};lightingSelected=-1;++lightingTicks;lighting_apply();return;}
 }
 access.sample=[&](float px,float py)->std::optional<OriginalPainterSample>{auto selected=originalPainterPayload(lightingTree,px,py,lightingOutside,lightingPayloads.size());lightingSelected=selected?int(*selected):-1;if(!selected)return {};return OriginalPainterSample{11,lightingPayloads[*selected].rate};};
 access.matches=[](){const auto& p=lightingPayloads.at(lightingSelected);return lightingState.references==p.references&&lightingState.values[0]==p.values[0]&&lightingState.values[1]==p.values[1];};
 access.blend=[](float weight){const auto& p=lightingPayloads.at(lightingSelected);originalLightingPainterBlend(lightingState,p.references,p.values,weight);};
 access.reset=[](){lightingState.references={};lightingState.values={1.f,.75f};};
 if(!lightingHasSection)lightingSelected=-1;
 originalPainterDriverStep(lightingDriver,x,y,11,lightingHasSection?OriginalPainterAvailability::Ready:OriginalPainterAvailability::MissingSection,-99999.f,access);++lightingTicks;lighting_apply();
}
extern "C" {
// Every IRR bank a Peak 1 Lighting payload or 22E180 default references (PEAK1/lighting-banks.json); turns the streamed mode on.
EMSCRIPTEN_KEEPALIVE int lighting_banks(const char* text){
 const auto p=json::parse(text);if(p.at("version")!=1)throw std::runtime_error("Lighting bank package version");lightingBanks.clear();
 for(const auto& [name,rows]:p.at("banks").items()){OriginalIrradianceCoefficients bank=rows.get<OriginalIrradianceCoefficients>();for(const auto& row:bank)for(float v:row)if(!std::isfinite(v))throw std::runtime_error("Nonfinite Lighting bank");lightingBanks[lighting_key(lighting_reference(name))]=bank;}
 lightingStreamed=lightingLive=true;lightingHasSection=false;lightingDriver={};lightingState={};lightingState.values={1.f,.75f};lightingSelected=-1;lightingTicks=0;lightingNodes.clear();lightingPayloads.clear();lightingRecordApplied=-2;
 return int(lightingBanks.size());
}
// 22E180: the default rider irradiance bank (gp+0x12D4) by name (xPBR1).
EMSCRIPTEN_KEEPALIVE int lighting_default(const char* name){const auto key=lighting_key(lighting_reference(name));if(!lightingBanks.contains(key))return 0;lightingDefaultBank=key;if(lightingStreamed)lighting_apply();return 1;}
// The painter region's Lighting section (PEAK1/<LOC>/lighting.json {painter: null | {entries, tree}}); the driver state is kept.
EMSCRIPTEN_KEEPALIVE int lighting_region(const char* text){
 if(!lightingRecordText.empty())return lightingHasSection?int(lightingPayloads.size()):0; // pv regionTick: the located records rule
 return lighting_use_section(text);
}
EMSCRIPTEN_KEEPALIVE int lighting_location(int track,const char* text){
 if(track<0||track>255||!text||!*text)return 0;const uint16_t k=lightingRecordSlot[size_t(track)];
 if(k)lightingRecordText[k-1u]=text;else{lightingRecordText.emplace_back(text);lightingRecordSlot[size_t(track)]=uint16_t(lightingRecordText.size());}
 lightingRecordApplied=-2;return int(lightingRecordText.size());
}
// [located records, applied region, missing (1/0), section (1/0), selected payload].
EMSCRIPTEN_KEEPALIVE int32_t* lighting_location_info(){RIDER_LOCAL static int32_t v[5];v[0]=int32_t(lightingRecordText.size());v[1]=lightingRecordApplied;v[2]=lightingRecordMissing;v[3]=lightingHasSection;v[4]=lightingSelected;return v;}
EMSCRIPTEN_KEEPALIVE void lighting_streamed_off(){lightingStreamed=false;}
}
// 22E180(streamer, course) from 22D088 (web/peak_world.inc request): gp+0x12D4 = the IRR bank xPBR1, x by course (jump table 0x47B4E0).
void browser_lighting_default_course(int course){if(course<0||course>=22||!lightingStreamed)return;const char name[6]={"ABCDEADEACEBCEADEABCDE"[course],'P','B','R','1',0};lighting_default(name);}
void browser_lighting_painter_step(float x,float y){
 if(lightingStreamed){if(!std::isfinite(x)||!std::isfinite(y))throw std::runtime_error("Nonfinite Lighting sample position");lighting_streamed_step(x,y);return;}
 if(!lightingLive)return;if(!std::isfinite(x)||!std::isfinite(y))throw std::runtime_error("Nonfinite Lighting sample position");
 OriginalPainterDriverAccess access;
 access.sample=[&](float px,float py)->std::optional<OriginalPainterSample>{auto selected=originalPainterPayload(lightingTree,px,py,lightingOutside,lightingPayloads.size());lightingSelected=selected?int(*selected):-1;if(!selected)return {};return OriginalPainterSample{11,lightingPayloads[*selected].rate};};
 access.matches=[](){const auto& p=lightingPayloads.at(lightingSelected);return lightingState.references==p.references&&lightingState.values[0]==p.values[0]&&lightingState.values[1]==p.values[1];}; //2BDE30
 access.blend=[](float weight){const auto& p=lightingPayloads.at(lightingSelected);originalLightingPainterBlend(lightingState,p.references,p.values,weight);}; //2BD5A8
 access.reset=[](){lightingState.references={};lightingState.values={1.f,.75f};}; //2BE1F8
 originalPainterDriverStep(lightingDriver,x,y,11,OriginalPainterAvailability::Ready,-99999.f,access);++lightingTicks;lighting_apply();
}
// [live, selected payload (-1 outside), gain +0x48, rim +0x50, wrapper steps, wrapper distance, reference 0 words (bit patterns;
// a name's first eight bytes, zero for the reset reference)].
extern "C" EMSCRIPTEN_KEEPALIVE float* environment_lighting_info(){RIDER_LOCAL static float v[8];v[0]=lightingLive;v[1]=lightingSelected;v[2]=lightingState.values[0];v[3]=lightingState.values[1];v[4]=lightingTicks;v[5]=lightingDriver.distance;
 v[6]=std::bit_cast<float>(lightingState.references[0][0]);v[7]=std::bit_cast<float>(lightingState.references[0][1]);return v;}
// The whole mountain: a location's environment slice (MOUNTAIN/ENV/<LOC>.json + .bin: patches, textures with global ids and
// offsets into the slice's bytes). Returns the number of patches added.
extern "C" EMSCRIPTEN_KEEPALIVE int environment_add(const char* metadata,const uint8_t* data,int length){
 const auto j=json::parse(metadata);if(!environmentStreamed)throw std::runtime_error("Environment is not streamed");int n=0;
 for(const auto& t:j.at("textures")){auto texture=std::make_shared<OriginalEnvironmentTexture>();texture->width=t.at("width");texture->height=t.at("height");
  const size_t cells=(texture->width+1)*(texture->height+1),rgba=t.at("rgba_offset"),valid=t.at("valid_offset");
  if(rgba+cells*4>size_t(length)||valid+cells>size_t(length))throw std::runtime_error("Environment texture bounds");
  texture->rgba.resize(cells);std::memcpy(texture->rgba.data(),data+rgba,cells*4);texture->valid.assign(data+valid,data+valid+cells);streamedTextures[t.at("id").get<int32_t>()]=std::move(texture);}
 for(const auto& p:j.at("patches")){OriginalEnvironmentPatch patch;patch.resource=p.at("resource");patch.flags=p.at("flags");patch.eligible=p.at("eligible");patch.lightUV=p.at("light_uv").get<std::array<float,4>>();patch.baseUV=p.at("base_uv").get<std::array<std::array<float,2>,4>>();patch.textures=p.at("textures").get<std::array<int,3>>();patches[patch.resource]=patch;++n;}
 return n;
}
// Tests (web/test-ctm-stream.mjs, pv sliceLoad): FNV-1a over what a load put in, the same whichever store holds the textures (the
// indexed one of init_environment, or environment_add's by id): the patches by resource, the textures by id, the globals.
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t environment_load_hash(){
 uint32_t h=0x811c9dc5u;auto mix=[&](const void* p,size_t n){const auto* b=static_cast<const uint8_t*>(p);for(size_t i=0;i<n;++i){h^=b[i];h*=0x01000193u;}};
 std::vector<uint32_t> keys;for(const auto& [r,p]:patches)keys.push_back(r);std::sort(keys.begin(),keys.end());
 for(uint32_t r:keys){const auto& p=patches.at(r);mix(&p.resource,4);mix(&p.flags,4);const uint8_t e=p.eligible;mix(&e,1);mix(p.lightUV.data(),sizeof(p.lightUV));mix(p.baseUV.data(),sizeof(p.baseUV));mix(p.textures.data(),sizeof(p.textures));}
 auto tex=[&](int32_t id,const OriginalEnvironmentTexture& t){mix(&id,4);mix(&t.width,4);mix(&t.height,4);mix(t.rgba.data(),t.rgba.size()*4);mix(t.valid.data(),t.valid.size());};
 if(environmentStreamed){std::vector<int32_t> ids;for(const auto& [id,t]:streamedTextures)ids.push_back(id);std::sort(ids.begin(),ids.end());for(int32_t id:ids)tex(id,*streamedTextures.at(id));}
 else if(textures)for(size_t i=0;i<textures->size();++i)tex(int32_t(i),(*textures)[i]);
 mix(&globals.multiplier,sizeof(globals.multiplier));mix(&globals.airAmbient,sizeof(globals.airAmbient));mix(&globals.airRatio,sizeof(globals.airRatio));const uint8_t fn=globals.forceNext;mix(&fn,1);
 mix(&state.ambient,sizeof(state.ambient));mix(&state.ratio,sizeof(state.ratio));
 return h;
}
// A released location: its patches and textures leave (the rider lighting keeps its last colour over a missing patch).
extern "C" EMSCRIPTEN_KEEPALIVE int environment_drop(int track){
 int n=0;for(auto it=patches.begin();it!=patches.end();)if(int(it->first&255u)==track){it=patches.erase(it);++n;}else ++it;
 for(auto it=streamedTextures.begin();it!=streamedTextures.end();)if((it->first>>12)==track)it=streamedTextures.erase(it);else ++it;
 return n;
}
// 0x2C03E8 -> 0x2C03A8 (web/weather.inc weather_painters_reset): every world painter wrapper of the global list gp+0x790 gets
// +0 = -99999, so its next step (2C0778) blends with -1: the Fog (camera blocks) and Lighting (rider block) drivers here.
void browser_environment_painters_reset(){fogDriver.distance=-99999.f;lightingDriver.distance=-99999.f;}
// A world load's location entry (pv painterWorldLoad: web/free-ride.js placeRegion with the new rider, after the placement's
// 0x2C03E8). The PS2 places the new rider during the load (11DE60 -> 111890 -> 0x2C03E8, +0 = -99999) and the load's steps
// then run in the stale painter region gp+0x770, whose record is not loaded yet (0x2C09D8): every wrapper holds its class
// defaults with +0 = 0 when the ride starts, so the start location's record blends in at its rate instead of jumping. PS2
// peak1-green-start (the lodge's Return to Game, watched): Fog A density 0 -> 1.2 and far 300 -> 70 m at 0.25 %/tick, Sun A
// 1 %/tick (azimuth 108.7 of 124.9 deg at tick 203), exact from tick 1. The rider block's Lighting / Weather are not reset:
// Weather jumps at its next step either way and A's Lighting scalars are the class defaults. The page's ScreenTint / Sun /
// glare painters follow environment_world_loads() (web/painter-regions.js followWorldLoad).
RIDER_LOCAL static uint32_t environmentWorldLoads=0;
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t environment_world_load(){
 if(fogReady){originalFogPainterDefaults(fogState,fogDriver.distance,fogDefaults);fogSelected=-1;}
 return ++environmentWorldLoads;
}
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t environment_world_loads(){return environmentWorldLoads;}
// Per rider context (web/rider_context.cpp): construct this translation unit's RIDER_LOCAL_LAZY containers.
void rider_statics_environment(){rider_touch(&patches);rider_touch(&streamedTextures);}
static const bool riderStaticsEnvironmentReady=(rider_statics_environment(),true);
#ifdef SSX_SNAPSHOT_REGISTRY
// The rider-context snapshot's re-derivation after a restore (web/world_snapshot.hpp; docs/replay.md §2a): the painter trees are
// views (std::span) of their packages' node lists, which a restore copies back into their own buffers: the views are pointed
// at them again. Nothing is saved.
#include "world_snapshot.hpp"
namespace {
void painter_trees_save(unsigned){}
bool painter_trees_restore(unsigned){fogTree.nodes=fogNodes;lightingTree.nodes=lightingNodes;return true;}
uint64_t painter_trees_hash(){return 0;}
size_t painter_trees_bytes(){return 0;}
bool painter_trees_check(unsigned){return true;}
struct PainterTreesHook{PainterTreesHook(){ssx_snapshot::hooks().push_back({"painter trees (re-derived)",&painter_trees_save,&painter_trees_restore,&painter_trees_hash,&painter_trees_bytes,&painter_trees_check});}};
[[maybe_unused]] PainterTreesHook painterTreesHook;
}
#endif
#ifdef SSX_SNAPSHOT_REGISTRY // the rider-context snapshot's registry (web/generate-snapshot-registry.mjs, docs/replay.md §2a)
#include "generated/snapshot/environment_bridge.inc"
#endif
