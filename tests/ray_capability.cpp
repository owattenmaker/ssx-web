#include "../engine/air_trajectory_world.hpp"
#include <cassert>
#include <iostream>
int main(){
 ssx::CollisionWorld terrain({});ssx::WorldBodyCollision world;ssx::WorldCollisionInstance instance;instance.resource=30472;instance.type=3;instance.flags=0x40210000;instance.low={-100,-100,-100};instance.high={100,100,100};instance.unsupported="Dynamic body callbacks unavailable";world.instances.push_back(instance);
 for(int mode:{0,2}){
  world.instances[0].rayAlwaysEmpty=false;auto missing=ssx::queryOriginalWorldSegment(terrain,&world,{0,0,200},{0,0,-200},mode);assert(!missing.complete&&missing.unavailableCause==2&&missing.unavailableResource==30472);
  world.instances[0].rayAlwaysEmpty=true;auto empty=ssx::queryOriginalWorldSegment(terrain,&world,{0,0,200},{0,0,-200},mode);assert(empty.complete&&empty.fraction<0&&!empty.hasInstance);
 }
 ssx::BodyCollisionVolume body;body.broadRadiusCm=30;body.count=1;body.activeMask=1;body.spheres[0]={{0,0,0},30,0};auto query=world.query(body,{0,0,1});assert(query.unsupportedInstances==std::vector<uint32_t>{30472});
 instance.resource=123;world.instances.push_back(instance);auto other=ssx::queryOriginalWorldSegment(terrain,&world,{0,0,200},{0,0,-200},0);assert(!other.complete&&other.unavailableResource==123);
 std::cout<<"Ray capability preserves empty source rays, incomplete body queries and unrelated dynamic failures\n";
}
