#include "ps2_runtime_macros.h"
#include "../engine/crash_entry.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <random>
void sub_0010EB30_0x10eb30(uint8_t*,R5900Context*,PS2Runtime*);void sub_0011DFE0_0x11dfe0(uint8_t*,R5900Context*,PS2Runtime*);void sub_0011E098_0x11e098(uint8_t*,R5900Context*,PS2Runtime*);void sub_0031BE50_0x31be50(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x340000,g_ps2RecompiledFunctionTableSlotCount=(0x340000-0x100000)/4;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[(0x340000-0x100000)/4]={};
using namespace ssx;using namespace ssx::terrain_original;
struct Call{int kind=0,value=0;float scalar=0;AnimationVector position{};AnimationQuaternion rotation{};bool operator==(const Call&)const=default;};
static std::vector<Call> actual,expected;static AnimationTransform presented,preview,entered;static Vector enteredUp;static float penalty;
template<class T>static void write(uint8_t*m,uint32_t p,T v){std::memcpy(m+p,&v,sizeof(v));}
template<class T>static T read(uint8_t*m,uint32_t p){T v;std::memcpy(&v,m+p,sizeof(v));return v;}
static void vec(uint8_t*m,uint32_t p,Vector v){write(m,p,v);write(m,p+12,1.f);}
static void pose(uint8_t*m,uint32_t p,AnimationTransform v){vec(m,p,v.position);write(m,p+16,v.rotation);}
static Call transformCall(int kind,AnimationTransform p){return {kind,0,0,p.position,p.rotation};}
static void external(uint8_t*m,R5900Context*c,PS2Runtime*){
    uint32_t pc=c->pc;Call call;switch(pc){case 0x33fff0:call={1,0,c->f[12]};break;
    case 0x119b08:call={2,int(GPR_U32(c,5))};c->f[0]=penalty;break;case 0x10e098:call={3,0,c->f[12]};break;
    case 0x296310:call={4,0,0};break;case 0x29f660:call={4,1,0};break;case 0x2961f0:call={4,2,c->f[12]};break;
    case 0x111aa0:call={5,int(GPR_U32(c,7)),c->f[12]};break;
    case 0x11fec8:call={6,int(GPR_U32(c,5))};if(call.value==13){pose(m,0x10110,entered);vec(m,0x101c0,enteredUp);}break;
    case 0x311b48:call={7,0,c->f[12]};break;
    case 0x11fa10:call=transformCall(8,{read<Vector>(m,0x10110),read<AnimationQuaternion>(m,0x10120)});pose(m,GPR_U32(c,5),presented);break;
    case 0x312660:call={9,int(GPR_U32(c,6)),c->f[12]};break;
    case 0x30ecd8:pose(m,GPR_U32(c,4),preview);break;
    case 0x311bf0:{uint32_t p=GPR_U32(c,5);call=transformCall(10,{read<Vector>(m,p),read<AnimationQuaternion>(m,p+16)});break;}
    case 0x3128e8:call={11,int(GPR_U32(c,5)),c->f[12]};break;case 0x11fe78:call={12,int(GPR_U32(c,5))};break;
    default:SET_GPR_U32(c,2,0);break;}
    if(call.kind)actual.push_back(call);c->pc=GPR_U32(c,31);
}
int main(int argc,char**argv){if(argc!=2)return 1;PS2Runtime runtime;std::vector<uint8_t> memory(32*1024*1024);auto*m=memory.data();std::ifstream ee(argv[1],std::ios::binary);ee.read((char*)m,memory.size());if(!ee)return 2;
    for(uint32_t pc:{0x33fff0,0x119b08,0x10e098,0x296310,0x29f660,0x2961f0,0x111aa0,0x11fec8,0x311b48,0x11fa10,0x312660,0x30ecd8,0x311bf0,0x3128e8,0x11fe78,0x28b180,0x116930})runtime.registerFunction(pc,external);
    runtime.registerFunction(0x11dfe0,sub_0011DFE0_0x11dfe0);runtime.registerFunction(0x11e098,sub_0011E098_0x11e098);runtime.registerFunction(0x31be50,sub_0031BE50_0x31be50);
    write(m,0x106c0,0x30000u);write(m,0x30088,int16_t(0));write(m,0x3008c,0x33fff0u);write(m,0x10780,0x40000u);write(m,0x40024,0x41000u);write(m,0x40028,0x42000u);write(m,0x1089c,0u);write(m,0x10784,0x50000u);
    std::mt19937 random(0x48415244);std::uniform_real_distribution<float>coord(-10000,10000),local(-100,100),unit(-1,1),scale(.7f,1.5f);
    auto transform=[&](auto& d){AnimationTransform p;p.position={d(random),d(random),d(random)};p.rotation={unit(random),unit(random),unit(random),unit(random)};float l=0;for(auto x:p.rotation)l+=x*x;l=std::sqrt(l);for(auto&x:p.rotation)x/=l;return p;};
    unsigned rotations=0;
    for(unsigned n=0;n<20000;++n){OriginalHardCrashEntryState state;state.physical=transform(coord);state.physicalUp={0,0,1};state.prewindStyle328=n%6;state.steering1F0={1,2,3};state.control208={4,5,6};state.control250={7,8,9};state.roll330=3;
        entered=transform(coord);enteredUp=originalOrientationBasis(entered.rotation).up;presented=transform(coord);preview=transform(local);auto current=transform(local);Vector scaling{scale(random),scale(random),scale(random)};penalty=n%3?-.25f:0.f;int semantic=328+n%34,impactType=int(n%20)-1;bool attacked=n%2;OriginalCollisionEvent event;event.pointCm={local(random),local(random),local(random)};event.normal={0,0,1};event.closingSpeedCmps=local(random)*10;
        pose(m,0x10110,state.physical);vec(m,0x101c0,state.physicalUp);write(m,0x10328,state.prewindStyle328);write(m,0x101f0,state.steering1F0);write(m,0x10208,state.control208);write(m,0x10250,state.control250);write(m,0x10330,state.roll330);vec(m,0x41000,current.position);write(m,0x42000,current.rotation);vec(m,0x40140,scaling);vec(m,0x60000,event.pointCm);vec(m,0x60010,event.incomingDirection);vec(m,0x60020,event.normal);write(m,0x60030,event.closingSpeedCmps);actual.clear();expected.clear();
        R5900Context c{};c.pc=0x10eb30;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,4,0x10000);SET_GPR_U32(&c,5,semantic);SET_GPR_U32(&c,6,attacked);SET_GPR_U32(&c,7,impactType);SET_GPR_U32(&c,8,0x60000);SET_GPR_U32(&c,29,0x70000);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,31,0x12345678);
        Rounding rounding;sub_0010EB30_0x10eb30(m,&c,&runtime);bool resetBasis=false;OriginalHardCrashEntryCallbacks cb;
        cb.reportPeakImpact=[&](float x){expected.push_back({1,0,x});};cb.recordCrash=[&](bool x){expected.push_back({2,int(x)});return penalty;};cb.changeBoostMeter=[&](float x){expected.push_back({3,0,x});};cb.notify=[&](OriginalHardCrashObserver o,float x){expected.push_back({4,int(o),x});};cb.reportImpact=[&](const OriginalCollisionEvent& e,int t){expected.push_back({5,t,e.closingSpeedCmps});};
        cb.enterControl=[&](int n,OriginalHardCrashEntryState& s){expected.push_back({6,n});if(n==13){s.physical=entered;s.physicalUp=enteredUp;}};cb.rotateAnimationRoot=[&](float x){expected.push_back({7,0,x});};cb.resetAnimationRootBasis=[&](const AnimationTransform&){resetBasis=true;};cb.presentedRoot=[&](const OriginalHardCrashEntryState& s){expected.push_back(transformCall(8,s.physical));return presented;};cb.previewCrashRoot=[&](int n){expected.push_back({9,n,0});return preview;};cb.currentScaledLocalRoot=[&](){auto result=current;for(unsigned k=0;k<3;++k)result.position[k]=mul(result.position[k],scaling[k]);return result;};cb.offsetAnimationRoots=[&](const AnimationTransform& p){expected.push_back(transformCall(10,p));};cb.playAnimation=[&](int n){expected.push_back({11,n,-1});};cb.enterMotion=[&](int n,OriginalHardCrashEntryState&){expected.push_back({12,n});};originalHardCrashEnter(state,semantic,attacked,impactType,event,cb);
        if(c.pc!=0x12345678||actual!=expected||read<Vector>(m,0x10110)!=state.physical.position||read<AnimationQuaternion>(m,0x10120)!=state.physical.rotation||read<int>(m,0x10328)!=0||read<Vector>(m,0x101f0)!=state.steering1F0||read<Vector>(m,0x10208)!=state.control208||read<Vector>(m,0x10250)!=state.control250||read<float>(m,0x10330)!=state.roll330||(resetBasis&&(read<Vector>(m,0x50030)!=Vector{}||read<AnimationQuaternion>(m,0x50040)!=AnimationQuaternion{0,0,0,1}))){std::cerr<<"Hard crash entry mismatch "<<n<<" events "<<(actual==expected)<<" physical "<<(read<Vector>(m,0x10110)==state.physical.position)<<'\n';for(unsigned j=0;j<std::min(actual.size(),expected.size());++j)if(!(actual[j]==expected[j])){std::cerr<<"entry "<<j<<" kinds "<<actual[j].kind<<'/'<<expected[j].kind<<" values "<<actual[j].value<<'/'<<expected[j].value<<" scalar "<<actual[j].scalar<<'/'<<expected[j].scalar<<'\n';for(unsigned k=0;k<4;++k)std::cerr<<actual[j].rotation[k]<<'/'<<expected[j].rotation[k]<<' ';std::cerr<<'\n';}return 3;}rotations+=resetBasis;
    }
    std::cout<<"20000 complete original hard-crash entry sequences match physics/root bake, control resets and callback order; "<<rotations<<" prewind compensation rotations\n";
}
