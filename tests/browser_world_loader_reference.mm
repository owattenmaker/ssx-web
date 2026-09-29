#import <Foundation/Foundation.h>
#include "../engine/world_collision_asset.h"
#include "json.hpp"
#include <fstream>
#include <cfenv>
#include <cstdio>
extern std::unique_ptr<ssx::WorldBodyCollision> browserBodies;
extern "C" void init_world_collision(const char*,const char*);
extern "C" void init_body_terrain(const char*);
int main(int argc,char**argv){@autoreleasepool{
 if(argc!=3)return 1;std::fesetround(FE_TONEAREST); // Asset parsing uses host default; transform helpers scope source rounding.
 std::ifstream f(argv[1]);std::string text((std::istreambuf_iterator<char>(f)),{});auto data=nlohmann::json::parse(text);auto hash=data.at("source_sha256").get<std::string>();
 auto native=loadWorldBodyCollision([NSString stringWithUTF8String:argv[1]],[NSString stringWithUTF8String:hash.c_str()],[NSString stringWithUTF8String:argv[2]]);init_world_collision(text.c_str(),hash.c_str());std::ifstream terrainFile(argv[2]);std::string terrainText((std::istreambuf_iterator<char>(terrainFile)),{});init_body_terrain(terrainText.c_str());
 if(native->instances.size()!=browserBodies->instances.size())return 2;unsigned nodes=0,triangles=0,trees=0;
 for(size_t k=0;k<native->instances.size();k++){
  const auto&a=native->instances[k];const auto&b=browserBodies->instances[k];
  if(a.resource!=b.resource||a.type!=b.type||a.flags!=b.flags||a.scale!=b.scale||a.rayAlwaysEmpty!=b.rayAlwaysEmpty||a.low!=b.low||a.high!=b.high)return 3;
  if(a.unsupported!=b.unsupported&&(!a.unsupported.empty()||!b.unsupported.empty())){if(a.unsupported.empty()!=b.unsupported.empty())return 4;}
  if(a.nodes.size()!=b.nodes.size())return 5;
  for(size_t n=0;n<a.nodes.size();n++){
   auto& x=a.nodes[n];auto& y=b.nodes[n];nodes++;
   if(x.index!=y.index||x.type!=y.type||x.surface!=y.surface||x.world!=y.world||x.inverse!=y.inverse||x.low!=y.low||x.high!=y.high||x.doubleSided!=y.doubleSided||x.collisionFlags!=y.collisionFlags||x.collisionValue!=y.collisionValue)return 6;
   if(bool(x.sphereTree)!=bool(y.sphereTree)||x.sphereTreeResource!=y.sphereTreeResource)return 9;
   if(x.sphereTree){trees++;auto&v=*x.sphereTree;auto&w=*y.sphereTree;if(v.centerCm!=w.centerCm||v.compressed!=w.compressed||v.radiusScale!=w.radiusScale||v.masks!=w.masks||v.levels.size()!=w.levels.size())return 10;for(size_t j=0;j<v.levels.size();j++)if(v.levels[j].radiusCm!=w.levels[j].radiusCm||v.levels[j].childOffsetCm!=w.levels[j].childOffsetCm||v.levels[j].stride!=w.levels[j].stride)return 11;}
   if(bool(x.triangles)!=bool(y.triangles))return 7;
   if(x.triangles){auto&v=*x.triangles;auto&w=*y.triangles;triangles+=v.indices.size()/3;if(v.vertices!=w.vertices||v.normals!=w.normals||v.indices!=w.indices){printf("Geometry mismatch instance%zu node%zu vertices%zu/%zu normals%zu/%zu\n",k,n,v.vertices.size(),w.vertices.size(),v.normals.size(),w.normals.size());for(size_t j=0;j<std::min(v.vertices.size(),w.vertices.size());j++)if(v.vertices[j]!=w.vertices[j]){printf("vertex%zu native %.9g %.9g %.9g browser %.9g %.9g %.9g\n",j,v.vertices[j][0],v.vertices[j][1],v.vertices[j][2],w.vertices[j][0],w.vertices[j][1],w.vertices[j][2]);break;}for(size_t j=0;j<std::min(v.normals.size(),w.normals.size());j++)if(v.normals[j]!=w.normals[j]){printf("normal%zu native %.9g %.9g %.9g browser %.9g %.9g %.9g\n",j,v.normals[j][0],v.normals[j][1],v.normals[j][2],w.normals[j][0],w.normals[j][1],w.normals[j][2]);break;}return 8;}}
  }
 }
 if(native->terrain.size()!=browserBodies->terrain.size())return 12;
 for(size_t i=0;i<native->terrain.size();i++){const auto&a=native->terrain[i];const auto&b=browserBodies->terrain[i];if(a.resource!=b.resource||a.flags!=b.flags||a.surface!=b.surface||a.low!=b.low||a.high!=b.high||a.grid!=b.grid||a.insertionOrder!=b.insertionOrder)return 13;}
 printf("Body terrain matches native: %zu patches, all coarse-grid coordinates and traversal order.\n",native->terrain.size());
 printf("Browser world loader matches native: %zu instances, %u nodes, %u triangle references, %u sphere-tree references.\n",native->instances.size(),nodes,triangles,trees);
 return 0;
}}
