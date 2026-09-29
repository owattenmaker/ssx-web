#include "../engine/terrain_contact_math.hpp"
#include <fstream>
#include <iostream>
int main(int argc,char**argv) {
    if(argc!=2)return 2;std::ifstream in(argv[1]);unsigned count;in>>count;
    unsigned scalars=0;
    for(unsigned i=0;i<count;++i) {
        unsigned resource;in>>resource;ssx::terrain_original::Coefficients c;
        for(auto&v:c)for(auto&x:v)in>>x;
        auto grid=ssx::terrain_original::coarseGrid(c);
        for(auto v:grid)for(auto x:v){float expected;in>>expected;if(x!=expected){std::cerr<<"Grid mismatch resource "<<resource<<" scalar "<<scalars<<" delta(cm) "<<x-expected<<'\n';return 1;}++scalars;}
    }
    std::cout<<count<<" original cached grids: "<<scalars<<" source-coordinate floats match exactly\n";
    return in?0:3;
}
