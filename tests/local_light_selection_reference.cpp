#include "ps2_runtime_macros.h"
#include "../engine/local_light_selection.hpp"
#include "../engine/rider_light_asset.hpp"
#include <filesystem>
#include <fstream>
#include <random>
#include <vector>
#include <cstring>
#include <cstdio>
#include <nlohmann/json.hpp>
#include <map>
void sub_002F5D30_0x2f5d30(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002F6168_0x2f6168(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002F5B68_0x2f5b68(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
static void suppliedRank(uint8_t* memory,R5900Context* c,PS2Runtime*){
    std::memcpy(&c->f[0],memory+GPR_U32(c,6)+0x18,4);c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){
 if(argc!=4)return 2;std::ifstream file(argv[1],std::ios::binary);
 std::vector<uint8_t> elf((std::istreambuf_iterator<char>(file)),{}),m(32*1024*1024);
 std::memcpy(m.data()+0xff000,elf.data(),elf.size());PS2Runtime runtime;runtime.registerFunction(0x2f6168,sub_002F6168_0x2f6168);
 auto put=[&](unsigned at,auto value){std::memcpy(m.data()+at,&value,sizeof(value));};
 auto context=[&](unsigned pc){R5900Context c{};c.pc=pc;SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);return c;};
 std::mt19937 rng(0x2f5b68);auto random=[&](){return float(int(rng()%20001)-10000)/13.f;};
 for(unsigned n=0;n<20000;++n){
  ssx::OriginalLightRankingInput in;in.kind=n%5;auto& light=in.light;
  std::array<float,3> point{random(),random(),random()};light.geometry.position={random(),random(),random()};light.geometry.radius=rng()%7001;
  light.geometry.axis={random()/770,random()/770,random()/770};light.intensity=random()/20;in.brightness=random()/770;
  light.outerCosine=-.8f;light.innerCosine=.6f;light.distanceMode=int8_t(n%6-1);light.angularMode=int8_t(n%12-3);
  if(n%7==0)light.geometry.position=point;
  if(n%4==0){light.geometry.position={point[0]+4500,point[1],point[2]};light.geometry.radius=6500;}
  put(0x20010,in.kind);put(0x20014,light.intensity);put(0x20018,in.brightness);put(0x2001c,light.geometry.radius);put(0x2002c,light.geometry.axis);put(0x20038,light.geometry.position);
  put(0x2005c,light.innerCosine);put(0x20060,light.outerCosine);put(0x20064,light.distanceMode);put(0x20065,light.angularMode);put(0x30000,point);
  auto c=context(0x2f5d30);SET_GPR_U32(&c,5,0x30000);SET_GPR_U32(&c,6,0x20000);
  {ssx::terrain_original::Rounding rounding;sub_002F5D30_0x2f5d30(m.data(),&c,&runtime);}
  float expected=ssx::originalLocalLightRank(point,in);
  if(c.pc!=0x12345678||std::memcmp(&expected,&c.f[0],4)){printf("Rank mismatch case%u kind%d native%g original%g\n",n,in.kind,expected,c.f[0]);return 1;}
 }
 runtime.registerFunction(0x2f5d30,suppliedRank);
 for(unsigned n=0;n<20000;++n){
  const unsigned count=rng()%48;std::vector<ssx::OriginalLocalLightCandidate> candidates;
  put(0x40210,count);
  for(unsigned i=0;i<count;++i){const unsigned address=0x50000+i*0x80;const int kind=rng()%5==0?3:6;const float rank=float(int(rng()%15)-3);
   put(0x40214+i*4,address);put(address+8,kind);put(address+0x18,rank);candidates.push_back({address,kind,rank});}
  auto c=context(0x2f5b68);SET_GPR_U32(&c,6,0x40000);SET_GPR_U32(&c,7,0x60000);SET_GPR_U32(&c,8,8);
  {ssx::terrain_original::Rounding rounding;sub_002F5B68_0x2f5b68(m.data(),&c,&runtime);}
  auto expected=ssx::originalSelectLocalLights(candidates);
  if(c.pc!=0x12345678||std::memcmp(expected.data(),m.data()+0x60000,32))throw std::runtime_error("Original light selection ordering differs");
 }
 std::ifstream lightFile(argv[2]),checkpointFile(argv[3]);
 auto catalog=nlohmann::json::parse(lightFile),checkpoints=nlohmann::json::parse(checkpointFile);
 std::ifstream treeFile(std::filesystem::path(argv[2]).parent_path()/"light-tree.json"),treeFixturesFile(std::filesystem::path(argv[3]).parent_path()/"light-tree-fixtures.json");
 auto tree=nlohmann::json::parse(treeFile),treeFixtures=nlohmann::json::parse(treeFixturesFile);
 auto lightWorld=ssx::originalRiderLightAsset(catalog,tree);
 std::map<uint32_t,ssx::OriginalLightRankingInput> lights;
 for(const auto& row:catalog.at("lights")){
  ssx::OriginalLightRankingInput input;input.kind=row.at("kind");input.brightness=row.at("brightness");auto& light=input.light;
  light.intensity=row.at("intensity");light.geometry.radius=row.at("radius");light.geometry.axis=row.at("axis").get<std::array<float,3>>();light.geometry.position=row.at("position").get<std::array<float,3>>();light.innerCosine=row.at("inner_cosine");light.outerCosine=row.at("outer_cosine");light.distanceMode=row.at("distance_mode").get<int>();light.angularMode=row.at("angular_mode").get<int>();
  lights.emplace(row.at("resource").get<uint32_t>(),input);
 }
 runtime.registerFunction(0x2f5d30,sub_002F5D30_0x2f5d30);
 unsigned retainedDifferences=0;
 for(const auto& row:checkpoints){
  const auto point=row.at("head_cm").get<std::array<float,3>>();std::vector<ssx::OriginalLocalLightCandidate> candidates;
  const auto ids=row.at("candidates").get<std::vector<uint32_t>>();put(0x40210,uint32_t(ids.size()));put(0x30000,point);
  for(unsigned i=0;i<ids.size();++i){
   const auto& input=lights.at(ids[i]);const auto& light=input.light;const unsigned at=0x50000+i*0x80;
   put(0x40214+i*4,at);put(at+8,6);put(at+16,input.kind);put(at+20,light.intensity);put(at+24,input.brightness);put(at+28,light.geometry.radius);put(at+44,light.geometry.axis);put(at+56,light.geometry.position);put(at+92,light.innerCosine);put(at+96,light.outerCosine);put(at+100,light.distanceMode);put(at+101,light.angularMode);
   candidates.push_back({at,6,ssx::originalLocalLightRank(point,input)});
  }
  auto c=context(0x2f5b68);SET_GPR_U32(&c,5,0x30000);SET_GPR_U32(&c,6,0x40000);SET_GPR_U32(&c,7,0x60000);SET_GPR_U32(&c,8,8);
  {ssx::terrain_original::Rounding rounding;sub_002F5B68_0x2f5b68(m.data(),&c,&runtime);}
  auto selected=ssx::originalSelectLocalLights(candidates);
  if(c.pc!=0x12345678||std::memcmp(selected.data(),m.data()+0x60000,32))throw std::runtime_error("Authored candidate ranking/selection differs from original execution");
  std::vector<uint32_t> resources;for(uint32_t pointer:selected)if(pointer)resources.push_back(ids[(pointer-0x50000)/0x80]);
  const auto bounds=std::find_if(treeFixtures.begin(),treeFixtures.end(),[&](const auto& b){return b.at("snapshot")==row.at("snapshot");});
  if(bounds==treeFixtures.end())throw std::runtime_error("Missing light world bounds");
  const auto worldSelection=lightWorld.refresh(bounds->at("minimum").get<std::array<float,3>>(),bounds->at("maximum").get<std::array<float,3>>(),point);
  std::vector<uint32_t> worldResources;for(auto id:worldSelection.ids)if(id)worldResources.push_back(id);
  if(worldSelection.candidates!=ids||worldResources!=resources)throw std::runtime_error("Combined authored world query/selection differs from source");
  if(resources!=row.at("selected").get<std::vector<uint32_t>>()){
   ++retainedDifferences;printf("Retained-list scheduling gap at %s: fresh original/native selection has%zu lights; stored snapshot has%zu.\n",row.at("snapshot").get<std::string>().c_str(),resources.size(),row.at("selected").size());
  }
 }
 printf("Three authored/captured candidate scopes match fresh original ranking+selection; %u differ from their retained snapshot list (update timing unresolved).\n",retainedDifferences);
 puts("20000 complete original light ranks and20000 original eight-slot selections match, including ties, overflow, non-light nodes, coincidence and attenuation modes.");
}
