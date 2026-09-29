#include <fstream>
#include <vector>
#include <cstdio>
#include "json.hpp"
extern "C" {void init_world(float*,int);void init_terrain(const char*);void init_world_collision(const char*,const char*);void reset_rider(float,float,float,float);float* step_rider(float,int,int,int);float* prediction_info();}
int main(int argc,char**argv){
 if(argc!=5)return 2;
 std::ifstream f(argv[1],std::ios::binary|std::ios::ate);auto n=f.tellg();f.seekg(0);std::vector<float> data(size_t(n)/4);f.read(reinterpret_cast<char*>(data.data()),n);init_world(data.data(),data.size());
 auto read=[](const char* path){std::ifstream f(path);return std::string((std::istreambuf_iterator<char>(f)),{});};
 auto terrain=read(argv[2]);init_terrain(terrain.c_str());auto hash=nlohmann::json::parse(terrain).at("source_sha256").get<std::string>();
 auto world=read(argv[3]);init_world_collision(world.c_str(),hash.c_str());auto start=nlohmann::json::parse(read(argv[4]));auto pos=start.at("position");reset_rider(pos[0],pos[1],pos[2],start.at("heading"));
 for(int i=0;i<600;i++){auto s=step_rider(0,i>90&&i<150,0,0);for(int j=0;j<16;j++)printf("%.9g,",s[j]);auto p=prediction_info();printf("%d,%d,%d\n",int(p[0]),int(p[1]),int(p[6]));}
}
