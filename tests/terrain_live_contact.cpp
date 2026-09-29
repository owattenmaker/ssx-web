#include "../engine/terrain_contact_math.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>
int main(int argc,char** argv) {
    using namespace ssx::terrain_original;
    if(argc!=2)return 2;std::ifstream in(argv[1]);unsigned count;in>>count;
    for(unsigned i=0;i<count;++i) {
        Coefficients c;for(auto& vector:c)for(auto& x:vector)in>>x;
        Vector pos,prior,lateral;for(auto* vector:{&pos,&prior,&lateral})for(auto& x:*vector)in>>x;
        float turn,scale,u,v;in>>turn>>scale>>u>>v;
        auto probe=groundProbe(pos,prior,lateral,turn,scale);
        float initialU,initialV;
        {Rounding rounding;initialU=ssx::originalScalarAdd(mul(float(std::floor(u*9)),0.1111111119389534f),0.0555555559694767f);initialV=ssx::originalScalarAdd(mul(float(std::floor(v*9)),0.1111111119389534f),0.0555555559694767f);}
        auto result=refine(c,probe.origin,probe.direction,initialU,initialV);
        std::cout<<std::setprecision(17)<<i<<' '<<result.valid<<' '<<result.u<<' '<<result.v;
        for(auto vector:{result.point,result.normal})for(auto x:vector)std::cout<<' '<<x;
        std::cout<<'\n';
    }
    return in?0:3;
}
