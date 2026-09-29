#include "../web/rail_bridge.hpp"
#include "json.hpp"
#include <fstream>
#include <iostream>
using nlohmann::json;
extern "C" {void init_rails(const char*,const char*);float* rail_query(float,float,float);float* rail_info();}
int main(int argc,char**argv){
 if(argc!=3)return 1;std::ifstream f(argv[1]);auto data=json::parse(f);auto text=data.dump(),hash=data.at("source_sha256").get<std::string>();init_rails(text.c_str(),hash.c_str());std::ofstream out(argv[2],std::ios::binary);unsigned queries=0;
 for(const auto& rail:data.at("rails")){
  // runtime_flags bit 0 is the rail query mask 1 (0x334680 a3 = 1): a rail the loader leaves without it is not found by its own samples.
  const bool queryable=!rail.contains("runtime_flags")||(rail.at("runtime_flags").get<uint32_t>()&1u);
  for(const auto& part:rail.at("segments")){
  ssx::OriginalRailSegment segment;segment.coefficients=part.at("source").at("coefficients").get<std::array<ssx::RailVector,4>>();
  for(float t:{0.f,.25f,.5f,.75f,1.f}){auto point=ssx::originalRailPoint(segment,t);point[2]+=20;auto result=rail_query(point[0],point[1],point[2]);if(!result[1]&&queryable)throw std::runtime_error("Authored rail sample not found");out.write((char*)point.data(),12);out.write((char*)result,56);++queries;}
 }}
 std::cout<<queries<<" original rail queries across "<<rail_info()[1]<<" rails and "<<rail_info()[2]<<" segments\n";return !out.good();
}
