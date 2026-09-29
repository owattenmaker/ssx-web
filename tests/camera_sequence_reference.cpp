// Reuse the source-offset state mapping used by the existing savestate tests.
#define main camera_unit_test_main
#include "../engine/original_camera_tests.cpp"
#undef main
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <map>
using Json=nlohmann::json;

static OriginalChaseAlgorithmState stateFrom(const Json& row){
 auto state=fromWords(row.at("algorithm_words").get<std::array<uint32_t,228>>());
 OriginalCubicSpline* splines[]={&state.lookSplineB,&state.eyeSplineB,&state.lookSplineA,&state.eyeSplineA,&state.swingSpline};
 for(auto& sp:row.at("splines")){
  auto words=sp.at("words").get<std::vector<uint32_t>>();
  auto& spline=*splines[(sp.at("offset").get<unsigned>()-0x304)/12];
  for(unsigned i=0;i<spline.records.size()&&i*5+4<words.size();++i){
   spline.records[i]={f(words[i*5]),f(words[i*5+1]),f(words[i*5+2]),f(words[i*5+3]),f(words[i*5+4])};
  }
 }
 return state;
}
static OriginalCameraInput inputFromRow(const Json& row){
 auto rider=row.at("rider_words").get<std::vector<uint32_t>>();
 auto trajectory=row.at("trajectory_words").get<std::vector<uint32_t>>();
 auto quad=[&](unsigned offset){return q({rider[offset/4],rider[offset/4+1],rider[offset/4+2],rider[offset/4+3]});};
 OriginalCameraInput in;in.headPosition=row.at("head").get<Quad>();
 in.tick=row.at("tick");in.motionMode=row.at("mode");in.velocity=quad(0x1e0);
 in.riderForward=quad(0x1b0);in.previousContactNormal=quad(0x380);in.wallNormal=quad(0x3c0);
 in.boostLevel=f(rider[0x2fc/4]);in.jumpCharge=f(rider[0x220/4]);
 in.surfaceId=rider[0x438/4];in.riderType=rider[0x434/4];
 in.launchValue=f(rider[0x5a4/4]);in.proximityFlag=rider[0x5ac/4]!=0;
 in.predictedAirTime=f(trajectory[0x98/4]);in.trajectoryStatusActive=trajectory[0xac/4]==1||trajectory[0xac/4]==3;
 for(unsigned i=0;i<4;++i){in.trajectoryHeading[i]=f(trajectory[4+i]);in.trajectoryNormal[i]=f(trajectory[8+i]);}
 return in;
}
static OriginalCameraInput browserInput(const Json& row){
 auto values=row.at("input").get<std::array<float,38>>();
 OriginalCameraInput in;
 OriginalCameraQuad* quads[]={&in.headPosition,&in.riderForward,&in.velocity,&in.previousContactNormal,&in.wallNormal,&in.trajectoryHeading,&in.trajectoryNormal};
 for(unsigned q=0;q<7;++q)for(unsigned k=0;k<4;++k)(*quads[q])[k]=values[q*4+k];
 in.motionMode=values[28];in.boostLevel=values[29];in.jumpCharge=values[30];
 in.surfaceId=values[31];in.riderType=values[32];in.launchValue=values[33];
 in.proximityFlag=values[34];in.trajectoryStatusActive=values[35];in.predictedAirTime=values[36];in.tick=values[37];
 if(in.tick!=row.at("tick").get<unsigned>())throw std::runtime_error("Browser input tick differs");
 return in;
}
static void advance(OriginalChaseAlgorithmState& state,const OriginalCameraInput& input){
 original_camera::Rounding rounding;
 if(state.resetPending)original_camera::setTarget(state,input);
 original_camera::update(state,input);original_camera::finish(state,input);
}
int main(int argc,char**argv){
 if(argc!=4&&argc!=5)return 2;
 const unsigned address=std::stoul(argv[2],nullptr,0);
 std::vector<std::filesystem::path> paths;
 for(auto& entry:std::filesystem::directory_iterator(argv[1]))if(entry.path().extension()==".json")paths.push_back(entry.path());
 std::sort(paths.begin(),paths.end());
 std::vector<Json> rows;
 for(auto& path:paths){std::ifstream file(path);auto data=Json::parse(file);for(auto& camera:data.at("cameras"))if(camera.at("address")==address)rows.push_back(camera);}
 if(rows.size()<2)throw std::runtime_error("Insufficient camera samples");
 auto carried=stateFrom(rows[0]);Json output=Json::array();
 std::map<unsigned,Json> browserRows;
 std::map<unsigned,OriginalChaseAlgorithmState> coldStates;
 auto browserSeeded=carried;
 if(argc==5){
  std::ifstream file(argv[4]);auto trace=Json::parse(file);
  OriginalChaseAlgorithmState cold;bool begun=false;unsigned previousTick=0;
  for(const auto& row:trace){
   auto input=browserInput(row);
   if(begun&&input.tick!=previousTick+1)throw std::runtime_error("Browser camera trace gap");
   if(!begun){original_camera::Rounding rounding;original_camera::setTarget(cold,input);begun=true;}
   advance(cold,input);browserRows[input.tick]=row;coldStates[input.tick]=cold;previousTick=input.tick;
  }
 }
 for(unsigned i=1;i<rows.size();++i){
  if(rows[i].at("tick").get<unsigned>()!=rows[i-1].at("tick").get<unsigned>()+1)throw std::runtime_error("Camera capture gap");
  auto input=inputFromRow(rows[i]);auto expected=stateFrom(rows[i]);
  auto single=stateFrom(rows[i-1]);
  {original_camera::Rounding rounding;for(auto* s:{&single,&carried}){if(s->resetPending)original_camera::setTarget(*s,input);original_camera::update(*s,input);original_camera::finish(*s,input);}}
  auto words=toWords(single),want=toWords(expected),carriedWords=toWords(carried);unsigned different=0,carriedDifferent=0;
  for(auto offset:comparedOffsets){different+=words[offset/4]!=want[offset/4];carriedDifferent+=carriedWords[offset/4]!=want[offset/4];}
  output.push_back({{"tick",input.tick},{"mode",input.motionMode},{"single_step_different_words",different},{"carried_different_words",carriedDifferent},
   {"single_eye_error_cm",dist3(single.outputEye,expected.outputEye)},
   {"carried_eye_error_cm",dist3(carried.outputEye,expected.outputEye)},
   {"carried_target_error_cm",dist3(carried.lookAt,expected.lookAt)}});
  if(argc==5){
   const auto& row=browserRows.at(input.tick);advance(browserSeeded,browserInput(row));
   const auto& cold=coldStates.at(input.tick);
   auto measured=row.at("algorithm").get<std::array<float,6>>();
   Quad measuredEye{measured[0],measured[1],measured[2],1},measuredTarget{measured[3],measured[4],measured[5],1};
   output.back()["browser_seeded_eye_error_cm"]=dist3(browserSeeded.outputEye,expected.outputEye);
   output.back()["browser_seeded_target_error_cm"]=dist3(browserSeeded.lookAt,expected.lookAt);
   output.back()["browser_cold_eye_error_cm"]=dist3(cold.outputEye,expected.outputEye);
   output.back()["browser_cold_target_error_cm"]=dist3(cold.lookAt,expected.lookAt);
   output.back()["browser_replay_eye_error_cm"]=dist3(cold.outputEye,measuredEye);
   output.back()["browser_replay_target_error_cm"]=dist3(cold.lookAt,measuredTarget);
  }
 }
 std::ofstream file(argv[3]);file<<output.dump(2)<<'\n';
 std::printf("Compared %zu consecutive original camera updates; error report: %s\n",output.size(),argv[3]);
}
