#include "pixel_swizzle.hpp"
#include <vector>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <atomic>
static void scalar(const unsigned char* s,unsigned char* d,size_t n){for(size_t i=0;i<n;++i){d[4*i]=s[4*i+2];d[4*i+1]=s[4*i+1];d[4*i+2]=s[4*i];d[4*i+3]=s[4*i+3];}}
int main(){
 for(size_t width=0;width<1025;++width){std::vector<unsigned char> s(width*4+17),a(s.size(),0xa5),b=a;for(size_t i=0;i<s.size();++i)s[i]=(i*157+width*13)&255;scalar(s.data()+3,a.data()+5,width);aurora::gfx::bgra_to_rgba(s.data()+3,b.data()+5,width);assert(a==b);}
 size_t n=2560*1440;std::vector<unsigned char>s(n*4),a(n*4),b(n*4);for(size_t i=0;i<s.size();++i)s[i]=i*157;
 auto bench=[&](auto f,auto& out){auto start=std::chrono::steady_clock::now();for(int i=0;i<60;++i){f(s.data(),out.data(),n);std::atomic_signal_fence(std::memory_order_seq_cst);}return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();};
 double old=bench(scalar,a),simd=bench(aurora::gfx::bgra_to_rgba,b);assert(a==b);std::printf("60x2560x1440 channel conversions: scalar %.2fms SIMD %.2fms; byte-identical\n",old,simd);
}
