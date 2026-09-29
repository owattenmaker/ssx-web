#include "../engine/air_motion.hpp"
#include <fstream>
#include <random>
#include <cstdio>
extern "C" void air_evaluate(float* rows,unsigned count){
 for(unsigned i=0;i<count;i++){
  auto* p=rows+i*9;ssx::OriginalAirState s{{p[0],p[1],p[2]},{p[3],p[4],p[5]}};
  p[8]=s.step(p[6],p+7);for(unsigned k=0;k<3;k++){p[k]=s.position[k];p[3+k]=s.velocity[k];}
 }
}
#ifndef __EMSCRIPTEN__
int main(int argc,char**argv){
 if(argc!=3)return 1;std::ofstream input(argv[1],std::ios::binary),output(argv[2],std::ios::binary);
 std::mt19937 rng(0x1139a0);std::uniform_real_distribution<float> position(-200000,200000),velocity(-6000,6000),cap(100,5000);
 for(unsigned i=0;i<20000;i++){float p[9];for(unsigned k=0;k<3;k++){p[k]=position(rng);p[3+k]=velocity(rng);}p[6]=cap(rng);p[7]=p[8]=0;input.write((char*)p,sizeof p);air_evaluate(p,1);output.write((char*)p,sizeof p);}
 puts("20000 native airborne steps captured, including capped velocities");return !input.good()||!output.good();
}
#endif
