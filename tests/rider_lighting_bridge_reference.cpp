#include "json.hpp"
#include <fstream>
#include <iostream>
#include <array>
#include <cstring>
extern "C" {
void init_rider_lighting(const char*,const char*);void reset_rider_lighting();
uint32_t* refresh_rider_lighting(const float*,const float*);uint32_t* rider_lighting_selection();
float* shade_rider_lighting(const float*,const float*,const float*,float,const float*,const float*,int);float* rider_lighting_info();float* rider_lighting_gpu_coefficients();
}
int main(int argc,char**argv){
 if(argc!=4)return 2;std::ifstream cf(argv[1]),tf(argv[2]),ff(argv[3]);auto catalog=nlohmann::json::parse(cf),tree=nlohmann::json::parse(tf),fixtures=nlohmann::json::parse(ff);auto cs=catalog.dump(),ts=tree.dump();init_rider_lighting(cs.c_str(),ts.c_str());
 for(auto& frame:fixtures){
  if(frame.at("reset"))reset_rider_lighting();
  auto point=frame.at("point").get<std::array<float,4>>();
  if(frame.at("refresh")){auto bounds=frame.at("bounds").get<std::array<float,8>>();auto rankPoint=frame.at("rankPoint").get<std::array<float,3>>();refresh_rider_lighting(bounds.data(),rankPoint.data());}
  auto bank=frame.at("environment").get<std::array<float,40>>();auto view=frame.at("view").get<std::array<float,16>>();auto constants=frame.at("constants").get<std::array<float,5>>();auto extra=frame.at("extra").get<std::vector<float>>();
  auto output=shade_rider_lighting(bank.data(),view.data(),point.data(),frame.at("rimScale"),constants.data(),extra.empty()?nullptr:extra.data(),extra.empty()?0:(extra.size()-3)/6);
  std::array<uint32_t,40> words;std::memcpy(words.data(),output,160);frame["expectedWords"]=words;std::memcpy(words.data(),rider_lighting_gpu_coefficients(),160);frame["expectedGpuWords"]=words;
  std::array<uint32_t,8> ids;std::memcpy(ids.data(),rider_lighting_selection(),32);frame["expectedSelection"]=ids;
  std::array<float,5> info;std::memcpy(info.data(),rider_lighting_info(),20);frame["expectedInfo"]=info;
 }
 std::cout<<fixtures.dump()<<'\n';
}
