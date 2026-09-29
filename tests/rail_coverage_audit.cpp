#include "../web/rail_bridge.hpp"
#include "json.hpp"
#include <fstream>
#include <iostream>
using nlohmann::json;
extern "C" void init_rails(const char*,const char*);
int main(int argc,char**argv){
 if(argc!=3)return 2;
 std::ifstream file(argv[1]);auto data=json::parse(file);auto text=data.dump();auto hash=data.at("source_sha256").get<std::string>();init_rails(text.c_str(),hash.c_str());
 ssx::OriginalRailAccess access;access.query=browserRailQuery;access.channel2Class=[](){return 0;};access.channel2SequenceFlag=[](unsigned){return false;};
 json rejected=json::array();unsigned cases=0,accepted=0,competing=0;ssx::rail_original::Rounding rounding;
 for(const auto& rail:data.at("rails"))for(const auto& part:rail.at("segments")){
  ssx::OriginalRailSegment segment;segment.coefficients=part.at("source").at("coefficients").get<std::array<ssx::RailVector,4>>();
  for(unsigned sample=1;sample<20;++sample)for(int direction:{-1,1}){
   float t=float(sample)/20,t2=ssx::rail_original::mul(t,t);
   auto point=ssx::originalRailCurvePoint(segment,{ssx::rail_original::mul(t,t2),t2,t,1});
   auto tangent=ssx::rail_original::normalizeRsqrt(ssx::originalRailCurvePoint(segment,{3*t2,2*t,1,0}));
   ssx::OriginalRailRider rider;rider.motionMode=1;
   for(unsigned k=0;k<3;++k){rider.bonePosition[k]=point[k]-direction*tangent[k]*10;rider.velocity[k]=direction*tangent[k]*900;}
   ssx::OriginalRailQueryResult hit;bool attached=ssx::originalRailAttachTest(rider,access,hit);++cases;accepted+=attached;
   if(hit.found&&hit.record->packedId!=rail.at("packed_id").get<unsigned>())++competing;
   if(!attached){float behind=hit.found?ssx::rail_original::dot(ssx::rail_original::vsub(hit.point,rider.bonePosition),rider.velocity)/ssx::rail_original::length(rider.velocity):0;
    rejected.push_back({{"rail",rail.at("name")},{"segment",part.at("index")},{"t",t},{"direction",direction},{"found",hit.found},{"hitId",hit.found?hit.record->packedId:0},{"distanceCm",hit.distance},{"aheadCm",behind}});
   }
  }
 }
 json report={{"scope","Ideal airborne approaches, ten cm before each sample in both directions; neutral grab class, identity board quaternion. Not full gameplay or dynamic-object rail coverage."},{"sourceSha256",hash},{"cases",cases},{"accepted",accepted},{"competing",competing},{"rejected",rejected}};
 std::ofstream output(argv[2]);output<<report.dump(2)<<'\n';std::cout<<cases<<" approaches: "<<accepted<<" accepted, "<<rejected.size()<<" rejected, "<<competing<<" competing hits\n";
}
