// Audit only: input centers are produced by the native animation audit, not
// captured expected bone positions. Exercise the real runtime volume builder.
#include "../engine/body_collision.hpp"
#include <fstream>
#include <cstdio>
int main(int argc,char**argv){
    if(argc!=2)return 2;std::ifstream input(argv[1],std::ios::binary);float scale;
    std::array<ssx::terrain_original::Vector,22> centers;
    if(!input.read(reinterpret_cast<char*>(&scale),4)||!input.read(reinterpret_cast<char*>(centers.data()),sizeof(centers)))return 3;
    auto volume=ssx::bodyVolumeFromBones(centers,scale);
    printf("{\"count\":%u,\"active_mask\":%u,\"broad_radius_cm\":%.9g,\"broad_center_cm\":[%.9g,%.9g,%.9g],\"spheres\":[",volume.count,volume.activeMask,volume.broadRadiusCm,volume.broadCenterCm[0],volume.broadCenterCm[1],volume.broadCenterCm[2]);
    for(unsigned i=0;i<volume.count;++i){auto&s=volume.spheres[i];printf("%s{\"bone\":%u,\"radius_cm\":%.9g}",i?",":"",s.bone,s.radiusCm);}puts("]}");
}
