#include "ground_motion_golden.hpp"
#include "riding.hpp"
#include <bit>
#include <cfenv>
#include <cstdio>
#include <fstream>
int main(int argc,char** argv) {
    if(argc!=2)return 1;
    std::ifstream input(argv[1],std::ios::binary);
    if(!input){std::fprintf(stderr,"Run python3 engine/test_original_ground.py to generate private original-code golden cases\n");return 77;}
    uint32_t header[4];input.read(reinterpret_cast<char*>(header),sizeof(header));
    if(!input||header[0]!=ssx::GroundGoldenMagic||header[1]!=ssx::GroundGoldenVersion||header[2]!=sizeof(ssx::GroundGoldenCase)||header[3]!=ssx::GroundGoldenCount) {
        std::fprintf(stderr,"Regenerate ground golden cases after source/state layout changes\n");return 1;
    }
    auto equal=[](float a,float b){return std::bit_cast<uint32_t>(a)==std::bit_cast<uint32_t>(b);};
    for(unsigned i=0;i<header[3];++i) {
        ssx::GroundGoldenCase record;input.read(reinterpret_cast<char*>(&record),sizeof(record));if(!input)return 1;
        if(i==0) {
            ssx::CollisionWorld empty({});ssx::PrototypeRider rider;rider.reset({0,0,0},{0,1,0},0);
            rider.seedOriginalGround(record.profile,record.initial);ssx::RiderInput controls;
            ssx::WorldBodyCollision bodyWorld;bool queriedAsGround=false;
            ssx::BodyPoseProvider pose=[&](const ssx::PrototypeRider& state,double)->std::optional<ssx::BodyCollisionVolume>{queriedAsGround=state.grounded&&state.airborneTicks==0;return std::nullopt;};
            rider.advance(1.0/60,controls,empty,empty,&bodyWorld,&pose);
            if(!queriedAsGround||!rider.hasOriginalAirTrajectory())return 1;
            if(rider.grounded||rider.airborneTicks!=0)return 1; // Passive launch occurs after the ground tick.
            rider.advance(1.0/60,controls,empty,empty);
            if(rider.grounded||rider.airborneTicks!=1)return 1;
        }
        auto state=record.initial;int before=std::fegetround();ssx::originalGroundIntegrate(record.profile,state);
        if(before!=std::fegetround())return 1;
        for(unsigned axis=0;axis<3;++axis)
            if(!equal(state.position[axis],record.position[axis])||!equal(state.velocity[axis],record.velocity[axis]))return 1;
        if(!equal(state.depth1,record.depth1)||!equal(state.depth3,record.depth3)||!equal(state.distance,record.distance))return 1;
    }
    std::puts("32 private direct-original cruise golden cases: all9 position/velocity/depth/contact floats bit-identical");
}
