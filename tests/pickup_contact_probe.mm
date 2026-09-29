#import <Foundation/Foundation.h>
#include "../engine/world_collision_asset.h"
#include "json.hpp"
#include <fstream>
#include <cstdio>
extern std::unique_ptr<ssx::WorldBodyCollision> browserBodies;
extern "C" void init_world_collision(const char*,const char*);
int main(int argc,char**argv){@autoreleasepool{
 if(argc!=3)return 1;std::ifstream f(argv[1]),cf(argv[2]);std::string text((std::istreambuf_iterator<char>(f)),{});auto world=nlohmann::json::parse(text),catalog=nlohmann::json::parse(cf);auto hash=world.at("source_sha256").get<std::string>();init_world_collision(text.c_str(),hash.c_str());unsigned tested=0;
 for(const auto& item:catalog.at("items")){
 auto kind=item.at("authored_name_category").get<std::string>();if(kind!="speedboost"&&kind!="trickboost")continue;
 uint32_t resource=item.at("resource");auto found=std::find_if(browserBodies->instances.begin(),browserBodies->instances.end(),[&](const auto&i){return i.resource==resource;});if(found==browserBodies->instances.end()||!found->unsupported.empty())return 2;
 ssx::WorldBodyCollision isolated;isolated.instances.push_back(*found);ssx::BodyCollisionVolume body;body.count=1;body.activeMask=1;body.broadRadiusCm=30;
 for(unsigned k=0;k<3;k++)body.broadCenterCm[k]=(found->low[k]+found->high[k])*.5f;body.spheres[0]={body.broadCenterCm,30,0};
 ssx::WorldCollisionTerrain floor;floor.flags=1;floor.low=found->low;floor.high=found->high;floor.surface=0;floor.low={body.broadCenterCm[0]-100,body.broadCenterCm[1]-100,body.broadCenterCm[2]-1};floor.high={body.broadCenterCm[0]+100,body.broadCenterCm[1]+100,body.broadCenterCm[2]+1};
 for(unsigned u=0;u<10;u++)for(unsigned v=0;v<10;v++)floor.grid[u*10+v]={body.broadCenterCm[0]+(float(u)-5)*20,body.broadCenterCm[1]+(float(v)-5)*20,body.broadCenterCm[2]};
 ssx::WorldBodyCollision terrainOnly;terrainOnly.terrain.push_back(floor);if(!terrainOnly.query(body,{0,0,1},nullptr,true,2).best.hit)return 8;
 isolated.terrain.push_back(floor);
 auto trigger=isolated.query(body,{0,0,1},nullptr,true,1);auto solidWorld=isolated;solidWorld.terrain.clear();auto solid=solidWorld.query(body,{0,0,1},nullptr,true,2);
 if(trigger.contacts!=trigger.instanceContacts.size()||trigger.instanceContacts.empty()||!trigger.complete()||!trigger.best.hit||trigger.best.instance!=resource||trigger.best.surface!=-1||solid.best.hit){printf("Contact filter failed %s trigger%d solid%d\n",item.at("name").get<std::string>().c_str(),trigger.best.hit,solid.best.hit);return 3;}
 for(const auto& hit:trigger.instanceContacts){if(hit.terrain||hit.instance!=resource||hit.surface!=-1)return 6;const auto node=std::find_if(found->nodes.begin(),found->nodes.end(),[&](const auto&n){return n.index==hit.node;});if(node==found->nodes.end()||hit.priority!=bool((node->collisionFlags&1)&&std::bit_cast<float>(node->collisionValue)!=0))return 7;}
 isolated.terrain.clear();
 body.broadCenterCm[0]=found->high[0]+1000;body.spheres[0].centerCm=body.broadCenterCm;
 if(isolated.query(body,{0,0,1},nullptr,true,1).best.hit)return 4;
 printf("%s: authored trigger hit, excluded from solid response, outside miss\n",item.at("name").get<std::string>().c_str());tested++;
 }
 return tested==5?0:5;
}}
