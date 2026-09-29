// The fast toward-zero paths of engine/software_float.hpp (and the rider-context helpers built on them) against their
// reference definitions, and on arm64 against the FPU itself (FPCR toward zero + flush-to-zero = the EE's DAZ/FTZ).
// docs/sim-performance.md "Fast exact primitives". Built natively and as WebAssembly by tools/test_exact_float.sh.
#include "../engine/terrain_contact_math.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
namespace sf=ssx::software_float;
static inline uint32_t B(float f){return std::bit_cast<uint32_t>(f);}
static inline float F(uint32_t u){return std::bit_cast<float>(u);}
struct Rng{uint64_t s;uint64_t next(){uint64_t z=(s+=0x9e3779b97f4a7c15ull);z=(z^(z>>30))*0xbf58476d1ce4e5b9ull;z=(z^(z>>27))*0x94d049bb133111ebull;return z^(z>>31);}uint32_t u32(){return uint32_t(next()>>32);}};
// The previous originalScalarAddSub (toward zero): the guard-bit masking, then the reference sum.
static float scalarAddSubReference(float a,float b,bool subtract){
 uint32_t x=B(a),y=B(b);int difference=int((x>>23)&255)-int((y>>23)&255);
 if(difference>=25)y&=0x80000000u;else if(difference<=-25)x&=0x80000000u;else if(difference>0)y&=0xffffffffu<<(difference-1);else if(difference<0)x&=0xffffffffu<<(-difference-1);
 return sf::addReference(F(x),subtract?-F(y):F(y));
}
static uint64_t cases=0,bad=0;
static void expect(const char* what,float a,float b,float fast,float reference){
 ++cases;if(B(fast)==B(reference))return;
 if(bad<10)std::printf("MISMATCH %s(%08x, %08x): fast %08x reference %08x\n",what,B(a),B(b),B(fast),B(reference));++bad;
}
static void pair(float a,float b){
 expect("add",a,b,sf::add(a,b),sf::addReference(a,b));
 expect("sub",a,b,sf::sub(a,b),sf::addReference(a,-b));
 expect("mul",a,b,sf::mul(a,b),sf::mulReference(a,b));
#if defined(__EMSCRIPTEN__)
 {ssx::OriginalRounding chop;
  expect("scalarAdd",a,b,ssx::originalScalarAdd(a,b),scalarAddSubReference(a,b,false));
  expect("scalarSub",a,b,ssx::originalScalarSubtract(a,b),scalarAddSubReference(a,b,true));
  expect("to.add",a,b,ssx::terrain_original::add(a,b),sf::addReference(a,b));
  expect("to.mul",a,b,ssx::terrain_original::mul(a,b),sf::mulReference(a,b));}
#endif
}
static void generate(int g,Rng& r,float& a,float& b){
 auto mant=[&](){uint32_t m=r.u32()&0x7fffffu;switch(r.u32()%4){case 0:return m;case 1:return m&~((1u<<(r.u32()%24))-1u);case 2:return m|((1u<<(r.u32()%23))-1u);default:return m&(0x7fffffu<<(r.u32()%23));}};
 auto sign=[&](){return (r.u32()&1)<<31;};
 switch(g){
  case 0:a=F(r.u32());b=F(r.u32());break;                                                                   // every bit pattern
  case 1:{int ea=int(r.u32()%256),eb=std::clamp(ea+int(r.u32()%81)-40,0,255);a=F(sign()|(uint32_t(ea)<<23)|mant());b=F(sign()|(uint32_t(eb)<<23)|mant());break;} // near exponents
  case 2:{uint32_t u=r.u32();int64_t w=std::clamp<int64_t>(int64_t(u&0x7fffffffu)+int(r.u32()%9)-4,0,0x7fffffff);a=F(u);b=F(uint32_t(w)|(~u&0x80000000u));break;} // b ~ -a
  case 3:{int ea=1+int(r.u32()%254),target=(r.u32()&1)?1+int(r.u32()%5)-2:254+int(r.u32()%5)-2,eb=std::clamp(target-ea+127,0,255);a=F(sign()|(uint32_t(ea)<<23)|mant());b=F(sign()|(uint32_t(eb)<<23)|mant());break;} // product thresholds
  default:{int ea=1+int(r.u32()%254),eb=std::max(0,ea-int(r.u32()%60));a=F(sign()|(uint32_t(ea)<<23)|mant());b=F(sign()|(uint32_t(eb)<<23)|((r.u32()&1)?0u:mant()));if(r.u32()&1)std::swap(a,b);break;} // far exponents
 }
}
#if defined(__aarch64__)&&!defined(__EMSCRIPTEN__)
static inline uint64_t getFpcr(){uint64_t v;__asm__ volatile("mrs %0, fpcr":"=r"(v));return v;}
static inline void setFpcr(uint64_t v){__asm__ volatile("msr fpcr, %0"::"r"(v));}
__attribute__((noinline)) static void hardware(const std::vector<float>& a,const std::vector<float>& b,std::vector<float>& out,int op){
 const uint64_t saved=getFpcr();setFpcr((saved&~(uint64_t(3)<<22))|(uint64_t(3)<<22)|(uint64_t(1)<<24)); // RMode toward zero, FZ
 for(size_t i=0;i<a.size();i++){volatile float x=a[i],y=b[i];out[i]=op==0?x+y:op==1?x-y:x*y;}
 setFpcr(saved);
}
#endif
int main(int argc,char** argv){
 const uint64_t n=argc>1?std::strtoull(argv[1],nullptr,10):2000000;
 std::vector<uint32_t> special;
 for(uint32_t v:{0u,1u,2u,0x7fffffu,0x400000u,0x800000u,0x800001u,0xffffffu,0x1000000u,0x7f7fffffu,0x7f7ffffeu,0x7f000000u,0x7f800000u,0x7f800001u,0x7fc00000u,0x7fffffffu,0x3f800000u,0x3f800001u,0x3f7fffffu,0x3f000000u,0x40000000u,0x33800000u,0x34000000u,0x4b000000u,0x4b7fffffu,0x4b800000u,0x00ffffffu,0x01000000u,0x3eaaaaabu,0x40490fdbu,0x33000000u}){special.push_back(v);special.push_back(v|0x80000000u);}
 for(uint32_t e=0;e<255;e+=3){special.push_back(e<<23);special.push_back((e<<23)|0x7fffffu);special.push_back((e<<23)|0x807fffffu);}
 for(uint32_t x:special)for(uint32_t y:special)pair(F(x),F(y));
 Rng r{0x5eed};float a,b;
 for(int g=0;g<5;g++)for(uint64_t i=0;i<n;i++){generate(g,r,a,b);pair(a,b);}
 // every binary32 against a few operands, both orders
 const uint64_t stride=argc>2?std::strtoull(argv[2],nullptr,10):1;
 for(uint32_t fixed:{0x3f800000u,0xbf800001u,0x4b000000u,0x00800000u,0x3eaaaaabu})for(uint64_t u=0;u<(1ull<<32);u+=stride){pair(F(uint32_t(u)),F(fixed));pair(F(fixed),F(uint32_t(u)));}
 std::printf("%llu fast/reference results: %llu mismatches\n",(unsigned long long)cases,(unsigned long long)bad);
#if defined(__aarch64__)&&!defined(__EMSCRIPTEN__)
 {std::vector<float> x(n),y(n),h(n);uint64_t hwBad=0,hwCases=0;Rng q{0xf00d};
  for(int g=0;g<5;g++){for(uint64_t i=0;i<n;i++)generate(g,q,x[i],y[i]);
   for(int op=0;op<3;op++){hardware(x,y,h,op);for(uint64_t i=0;i<n;i++){const float s=op==0?sf::add(x[i],y[i]):op==1?sf::sub(x[i],y[i]):sf::mul(x[i],y[i]);++hwCases;
     if(B(s)!=B(h[i])&&!(std::isnan(s)&&std::isnan(h[i]))){if(hwBad<10)std::printf("HARDWARE MISMATCH op %d (%08x, %08x): %08x fpu %08x\n",op,B(x[i]),B(y[i]),B(s),B(h[i]));++hwBad;}}}}
  std::printf("%llu results vs the arm64 FPU (toward zero, flush to zero): %llu mismatches (NaN payloads not compared)\n",(unsigned long long)hwCases,(unsigned long long)hwBad);bad+=hwBad;}
#endif
 return bad?1:0;
}
