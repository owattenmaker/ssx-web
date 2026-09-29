#include "ps2_runtime_macros.h"
#include "../engine/spatial_light_query.hpp"
#include <fstream>
#include <random>
#include <cstring>
#include <cstdio>
#include <nlohmann/json.hpp>
void sub_00332DB8_0x332db8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00328360_0x328360(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0033B748_0x33b748(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00340DC0_0x340dc0(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(int argc,char**argv){
 if(argc!=2&&argc!=5)return 2;std::ifstream file(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(file)),{}),m(32*1024*1024);
 if(argc==5){if(elf.size()!=m.size())return 2;std::memcpy(m.data(),elf.data(),elf.size());}else std::memcpy(m.data()+0xff000,elf.data(),elf.size());PS2Runtime runtime;
 runtime.registerFunction(0x328360,sub_00328360_0x328360);runtime.registerFunction(0x33b748,sub_0033B748_0x33b748);runtime.registerFunction(0x340dc0,sub_00340DC0_0x340dc0);
 // Collision-instance list is independent of the extra-node output under test.
 runtime.registerFunction(0x3309d8,[](uint8_t*,R5900Context* c,PS2Runtime*){c->pc=GPR_U32(c,31);});
 auto put=[&](unsigned at,auto value){std::memcpy(m.data()+at,&value,sizeof(value));};
 auto get=[&](unsigned at){uint32_t value;std::memcpy(&value,m.data()+at,4);return value;};
 if(argc==5){
  std::ifstream treeFile(argv[2]),fixtureFile(argv[3]);auto tree=nlohmann::json::parse(treeFile),fixtures=nlohmann::json::parse(fixtureFile);
  std::vector<ssx::OriginalSpatialLightNode> nodes;
  for(const auto& row:tree.at("nodes"))nodes.push_back({row.at("children").get<std::array<int32_t,8>>(),row.at("lights").get<std::vector<uint32_t>>()});
  std::array<ssx::OriginalSpatialLightRoot,8> roots;
  for(unsigned i=0;i<8;++i){const auto& row=tree.at("roots").at(i);roots[i]={{row.at("exponent"),row.at("cell").get<std::array<int32_t,3>>()},row.at("node")};}
  for(const auto& row:fixtures)if(row.at("snapshot")==argv[4]){
   const auto low=row.at("minimum").get<std::array<float,3>>(),high=row.at("maximum").get<std::array<float,3>>();
   put(0x30000,row.at("manager").get<uint32_t>());put(0x20000,low);put(0x20010,high);
   R5900Context c{};c.pc=0x332db8;SET_GPR_U32(&c,4,0x30000);SET_GPR_U32(&c,5,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
   {ssx::terrain_original::Rounding rounding;sub_00332DB8_0x332db8(m.data(),&c,&runtime);}
   if(c.pc!=0x12345678||get(0x30210)>512)throw std::runtime_error("Invalid captured world traversal");
   std::vector<uint32_t> expected;for(unsigned i=0;i<get(0x30210);++i){uint32_t pointer=get(0x30214+i*4);if(get(pointer+8)==6)expected.push_back(row.at("pointer_to_resource").at(std::to_string(pointer)));}
   const auto actual=ssx::originalSpatialLightQuery(nodes,roots,low,high);
   if(actual!=expected)throw std::runtime_error("Pruned authored light index differs from original world traversal");
   printf("Original/pruned world light query matches at %s: %zu ordered candidates.\n",argv[4],actual.size());return 0;
  }
  throw std::runtime_error("Missing captured query fixture");
 }
 std::mt19937 rng(0x332db8);unsigned nonempty=0,totalLights=0;
 for(unsigned test=0;test<5000;++test){
  std::memset(m.data()+0x50000,0,0xa4);std::memset(m.data()+0x600000,0,64*64);std::memset(m.data()+0x700000,0,256*32);
  std::vector<ssx::OriginalSpatialLightNode> nodes;std::array<ssx::OriginalSpatialLightRoot,8> roots;unsigned objects=0;
  auto make=[&](auto&& self,unsigned depth)->int{
   const int index=nodes.size();nodes.emplace_back();const unsigned address=0x600000+index*64;unsigned prior=0;
   for(unsigned i=0,count=rng()%3;i<count;++i){const unsigned object=0x700000+objects++*32;const bool light=rng()%3!=0;put(object+8,light?6:3);if(prior)put(prior,object);else put(address+40,object);prior=object;if(light)nodes[index].lights.push_back(object);}
   if(depth)for(unsigned child=0;child<8;++child)if(nodes.size()<64&&rng()%4==0){int next=self(self,depth-1);nodes[index].children[child]=next;put(address+child*4,uint32_t(0x600000+next*64));}
   return index;
  };
  for(unsigned i=0;i<8;++i)if(nodes.size()<60&&rng()%3==0){auto& root=roots[i];root.region.exponent=11+rng()%4;for(auto& x:root.region.cell)x=int(rng()%5)-2;root.node=make(make,root.region.exponent-11);put(0x50000+i*20,root.region.exponent);put(0x50004+i*20,root.region.cell);put(0x50010+i*20,uint32_t(0x600000+root.node*64));}
  std::array<float,3> low,high;for(unsigned i=0;i<3;++i){low[i]=float(int(rng()%100001)-50000);high[i]=low[i]+float(rng()%80001);}
  if(test%7==0)low=high={0,0,0};
  put(0x30000,uint32_t(0x50000));put(0x20000,low);put(0x20010,high);
  R5900Context c{};c.pc=0x332db8;SET_GPR_U32(&c,4,0x30000);SET_GPR_U32(&c,5,0x20000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);
  {ssx::terrain_original::Rounding rounding;sub_00332DB8_0x332db8(m.data(),&c,&runtime);}
  if(c.pc!=0x12345678||get(0x30210)>256){fprintf(stderr,"Invalid source traversal case%u pc%x count%u nodes%zu objects%u\n",test,c.pc,get(0x30210),nodes.size(),objects);return 1;}
  std::vector<uint32_t> expected;for(unsigned i=0;i<get(0x30210);++i){const auto pointer=get(0x30214+i*4);if(get(pointer+8)==6)expected.push_back(pointer);}
  const auto actual=ssx::originalSpatialLightQuery(nodes,roots,low,high);
  if(actual!=expected){fprintf(stderr,"Spatial light query differs case%u native%zu original%zu\n",test,actual.size(),expected.size());for(auto id:actual)fprintf(stderr," n%x",id);fprintf(stderr,"\n");for(auto id:expected)fprintf(stderr," o%x",id);fprintf(stderr,"\n");for(unsigned i=0;i<roots.size();++i)if(roots[i].node>=0)fprintf(stderr,"root%u node%d e%d cell%d,%d,%d\n",i,roots[i].node,roots[i].region.exponent,roots[i].region.cell[0],roots[i].region.cell[1],roots[i].region.cell[2]);for(unsigned i=0;i<nodes.size();++i){fprintf(stderr,"node%u children",i);for(auto child:nodes[i].children)fprintf(stderr," %d",child);fprintf(stderr," lights");for(auto id:nodes[i].lights)fprintf(stderr," %x",id);fprintf(stderr,"\n");}return 1;}
  nonempty+=!actual.empty();totalLights+=actual.size();
 }
 if(!nonempty)return 2;
 printf("5000 original spatial light queries match ordered candidates; %u nonempty queries, %u light visits. Collision-instance output is excluded.\n",nonempty,totalLights);
}
