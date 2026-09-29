#include "../engine/rider_texture.hpp"
#include <cstdio>
#include <fstream>
#include "rider_texture_original.inc"
int main(int argc,char**argv){
    GSTex0Reg tex{};tex.tfx=3;tex.tcc=1;
    for(unsigned t=0;t<256;++t)for(unsigned l=0;l<256;++l)for(unsigned rim=0;rim<256;++rim){
        const std::array<uint8_t,4> texture={uint8_t(t),uint8_t(t^0x55),uint8_t(255-t),uint8_t(rim^0x5a)};
        const std::array<uint8_t,4> light={uint8_t(l),uint8_t(255-l),uint8_t(l^0xaa),uint8_t(rim)};
        const auto actual=ssx::originalRiderHighlight2(texture,light);
        const auto expected=combineTexture(tex,light[0],light[1],light[2],light[3],texture[0],texture[1],texture[2],texture[3]);
        if(actual!=std::array<uint8_t,4>{expected.r,expected.g,expected.b,expected.a})return 1;
    }
    if(argc==2){
        std::ofstream output(argv[1],std::ios::binary);
        for(unsigned rim:{0u,1u,127u,128u,254u,255u})for(unsigned light=0;light<256;++light)for(unsigned texture=0;texture<256;++texture){
            auto color=combineTexture(tex,light,255-light,light^0xaa,rim,texture,texture^0x55,255-texture,(texture+light)&255);
            const std::array<uint8_t,4> bytes={color.r,color.g,color.b,color.a};output.write(reinterpret_cast<const char*>(bytes.data()),4);
        }
        if(!output)return 2;
    }
    std::puts("16777216 texture/light/rim combinations match the source-extracted GS HIGHLIGHT2 RGBA combiner; texture alpha is preserved.");
}
