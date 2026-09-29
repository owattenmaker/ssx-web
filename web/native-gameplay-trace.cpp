#include "json.hpp"
#include <fstream>
#include <vector>
#include <iostream>
#include <cstdint>
using nlohmann::json;
extern "C" {
void rail_shoulder_input(int);void award_trick_meter(float);float* rail_uber_info();float* rail_exit_info();
void hard_crash_begin(int,float);void soft_collision_begin(int,float);
void init_environment(const char*,const uint8_t*,int);float* environment_info();float* environment_irradiance_info();float* environment_irradiance();
void start_event();void init_fog(const char*);void reset_fog();float* fog_info();float* fog_palette_info();uint8_t* fog_palette_rgba();
void init_rails(const char*,const char*);void init_world(float*,int);void init_terrain(const char*);void init_world_collision(const char*,const char*);void init_body_terrain(const char*);
void init_animation(const char*,const char*,const char*,const uint8_t*,int);void init_race(const char*);void animation_use_physics(int);
float* boost_fx_info();float* boost_fx_vertices(unsigned);
float* pickup_info();float* snow_info();float* snow_flipbook_info();float* snow_particles(unsigned);float* wake_info();float* wake_vertices();float* boost_hud_info();float* rail_score_info();void rail_rotation_input(float);float* rail_rotation_info();float* rail_jump_info();void rail_preinput(float);float* rail_gameplay_info();void set_rider_velocity(float,float,float);void request_rider_reset(int);void reset_rider(float,float,float,float);void reset_animation();void reset_race();void race_begin();float* race_end();
float* camera_render_view();float* rider_query_bounds();float* source_motion_audit();float* sampled_local_pose();float* pose_controls_info();
int rider_skin_palette_count();float* rider_skin_palette();
int rider_skin_matrix_count();float* rider_skin_matrices();
float* step_rider(float,int,int,int);float* rider_state();float* animation_info();float* pose_physical();float* reset_info();float* crash_info();
float* animation_tick(float,float,float,float,int,int,int,int,int,float,float,float);float* step_camera_head(float,float,float);
}
int main(int argc,char**argv){try{
 if(argc!=4&&argc!=5)return 1;const std::string root=argv[1],riderPackage=argc==5?argv[4]:"RIDER_SAM";if(riderPackage!="RIDER_SAM"&&riderPackage!="RIDER_ZOE")throw std::runtime_error("Unknown trace rider");
 auto read=[](const std::string& path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("Missing "+path);return std::string((std::istreambuf_iterator<char>(f)),{});};
 auto meta=read(root+"/ANIMATIONS/animation-packets.json"),rig=read(root+"/"+riderPackage+"/rider.json"),cfg=read(root+"/ANIMATIONS/initial.json"),packets=read(root+"/ANIMATIONS/animation-packets.bin");
 init_animation(meta.c_str(),rig.c_str(),cfg.c_str(),(const uint8_t*)packets.data(),packets.size());init_race(cfg.c_str());animation_use_physics(1);
 auto mesh=read(root+"/ARA1/collision.bin");std::vector<float> geometry(mesh.size()/4);std::memcpy(geometry.data(),mesh.data(),mesh.size());init_world(geometry.data(),geometry.size());
 auto terrain=read(root+"/ARA1/terrain.json"),world=read(root+"/ARA1/world_collision.json"),hash=json::parse(terrain).at("source_sha256").get<std::string>();init_terrain(terrain.c_str());init_world_collision(world.c_str(),hash.c_str());init_body_terrain(terrain.c_str());auto rails=read(root+"/ARA1/rails.json");init_rails(rails.c_str(),hash.c_str());
 auto env=read(root+"/ARA1/environment.json"),envBytes=read(root+"/ARA1/environment.bin");init_environment(env.c_str(),reinterpret_cast<const uint8_t*>(envBytes.data()),envBytes.size());
 auto fog=read(root+"/ARA1/fog-tree.json");init_fog(fog.c_str());
 auto start=json::parse(read(root+"/ARA1/start.json")),inputs=json::parse(read(argv[2]));const auto pos=start["position"];
 const size_t poseCount=json::parse(rig)["bones"].size()*7;std::ofstream out(argv[3],std::ios::binary);
 auto emit=[&](const float*p,size_t count){out.write((const char*)p,count*4);};
 auto fingerprint=[&](const float* data,size_t count){uint32_t hash=2166136261u;auto bytes=reinterpret_cast<const unsigned char*>(data);for(size_t i=0;i<count*4;i++)hash=(hash^bytes[i])*16777619u;float halves[]{float(hash&65535),float(hash>>16)};emit(halves,2);};
 for(const auto& scenario:inputs){reset_fog();reset_animation();reset_race();const auto initial=scenario.contains("spawn")?scenario["spawn"]:start;reset_rider(initial["position"][0],initial["position"][1],initial["position"][2],initial["heading"]);if(initial.contains("velocity"))set_rider_velocity(initial["velocity"][0],initial["velocity"][1],initial["velocity"][2]);if(scenario.value("eventStart",false))start_event();
  unsigned tick=0;
  for(const auto& input:scenario["frames"]){
   if(scenario.contains("meterAward")&&scenario["meterAward"]["tick"]==tick)award_trick_meter(scenario["meterAward"]["amount"]);
   if(scenario.contains("collisionEvent")&&scenario["collisionEvent"]["tick"]==tick){const auto& event=scenario["collisionEvent"];if(!rail_gameplay_info()[0])throw std::runtime_error("Rail crash fixture is not grinding");if(event.value("kind",std::string{})=="soft")soft_collision_begin(event["animation"],0);else {hard_crash_begin(event["animation"],event["speed"]);if(crash_info()[2]!=1||rail_gameplay_info()[0])throw std::runtime_error("Rail crash handoff failed");}}++tick;
   float turn=input[0];int held=input[1],brake=input[2],grab=input[3];rail_shoulder_input(grab);rail_preinput(input.size()>5?float(input[5]):0);rail_rotation_input(input.size()>6?float(input[6]):0);if(input.size()>4&&int(input[4])>=0)request_rider_reset(input[4]);race_begin();auto r=step_rider(turn,held,brake,0);
   auto pose=animation_tick(r[7],turn,brake,r[9],int(r[8]),held,grab,0,0,0,r[15],input.size()>5?float(input[5]):0);
   // Copy the pose before any further host callbacks can replace its backing data.
   std::vector<float> sampled(pose,pose+poseCount);auto physical=pose_physical();auto camera=step_camera_head(physical[9],physical[10],physical[11]);auto race=race_end();
   emit(rider_state(),16);emit(animation_info(),19);emit(pose_physical(),12);float skinCount=rider_skin_matrix_count();emit(&skinCount,1);if(skinCount>0)emit(rider_skin_matrices(),size_t(skinCount)*16);float paletteCount=rider_skin_palette_count();emit(&paletteCount,1);if(paletteCount>0)emit(rider_skin_palette(),size_t(paletteCount)*16);emit(race,8);emit(camera,9);emit(camera_render_view(),16);emit(fog_info(),11);emit(fog_palette_info(),9);emit(environment_info(),7);emit(environment_irradiance_info(),10);emit(environment_irradiance(),40);emit(reset_info(),9);emit(crash_info(),12);emit(sampled.data(),sampled.size());emit(sampled_local_pose(),poseCount);emit(pose_controls_info(),17);emit(source_motion_audit(),20);emit(rider_query_bounds(),12);emit(rail_gameplay_info(),8);emit(rail_jump_info(),3);emit(rail_rotation_info(),4);emit(rail_score_info(),8);emit(rail_uber_info(),6);emit(rail_exit_info(),4);emit(boost_hud_info(),13);emit(pickup_info(),26);auto snow=snow_info(),wake=wake_info();emit(snow,23);emit(snow_flipbook_info(),20);emit(wake,13);auto boost=boost_fx_info();emit(boost,12);for(unsigned i=0;i<10;i++)fingerprint(snow_particles(i),size_t(snow[i])*8);fingerprint(wake_vertices(),size_t(wake[11])*9);for(unsigned i=0;i<3;i++)fingerprint(boost_fx_vertices(i),size_t(boost[6+i])*9);fingerprint(reinterpret_cast<const float*>(fog_palette_rgba()),256);
  }
 }
 if(!out.good())throw std::runtime_error("Trace write failed");std::cout<<"Native animation/pose/race gameplay trace written\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
