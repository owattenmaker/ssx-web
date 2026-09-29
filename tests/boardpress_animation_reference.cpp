// Development-only instruction oracle: original board-press animation drivers
// against engine/board_press_animation.hpp.
//   kind 13 0x1047F0, kind 14 0x104728 (0x313CF0 seek, 0x313800 fade, 0x3145F8 removal are
//   recording stubs), kind 15 0x1046B0 (0x103CC8 is a recording stub: leaves, amount, time
//   scale), completion kind 9 0x104BD8 with the real 0x312BD0 and 0x144670 (0x3128E8 stub).
#include "handplant_reference_common.hpp"
#include "../engine/board_press_animation.hpp"
void sub_001047F0_0x1047f0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00104728_0x104728(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001046B0_0x1046b0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00104BD8_0x104bd8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00312BD0_0x312bd0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00144670_0x144670(uint8_t*,R5900Context*,PS2Runtime*);
using namespace hp;
static void fail(const char* what){throw std::runtime_error(what);}
constexpr uint32_t LIST=0x98000;
static std::vector<Call> calls;static bool fadeResult=false;
static void stub(uint8_t*,R5900Context*c,PS2Runtime*){
    const uint32_t a0=GPR_U32(c,4),a1=GPR_U32(c,5),a2=GPR_U32(c,6);
    switch(c->pc){
    case 0x313cf0:if(a0!=SEQ||a1!=0)fail("0x313CF0 arguments");calls.push_back({0x313cf0,0,fb(c->f[12])});break;
    case 0x313800:if(a0!=SEQ)fail("0x313800 sequence");calls.push_back({0x313800,0,fb(c->f[12])});SET_GPR_U32(c,2,fadeResult);break;
    case 0x3145f8:if(a0!=LIST||a1!=SEQ)fail("0x3145F8 arguments");calls.push_back({0x3145f8});break;
    case 0x103cc8:if(a0!=ANIM||a1!=LIST||a2!=SEQ)fail("0x103CC8 arguments");
        calls.push_back({0x103cc8,int32_t(GPR_U32(c,7)|GPR_U32(c,8)<<10|GPR_U32(c,9)<<20),fb(c->f[13])});calls.push_back({0x103cc9,0,fb(c->f[12])});SET_GPR_U32(c,2,fadeResult);break;
    case 0x3128e8:if(a0!=ANIM||a2!=0||fb(c->f[12])!=fb(-1.f))fail("0x3128E8 arguments");calls.push_back({0x3128e8,int32_t(a1)});break;
    default:std::printf("unexpected %x\n",c->pc);fail("unexpected stub");
    }
    ret(c);
}
int main(int argc,char**argv){
    if(argc!=2)return 2;auto ram=loadElf(argv[1]);M=ram.data();baseLayout();wr(ANIM+0x60,RIDER);
    PS2Runtime rt;rt.registerFunction(0x312bd0,sub_00312BD0_0x312bd0);rt.registerFunction(0x144670,sub_00144670_0x144670);
    for(uint32_t pc:{0x313cf0u,0x313800u,0x3145f8u,0x103cc8u,0x3128e8u})rt.registerFunction(pc,stub);
    std::fesetround(FE_TOWARDZERO);Rand g(0x1047f0);
    auto value=[&](std::initializer_list<float> specials){float v=g.coin(3)?g.uni(-1.3f,1.3f):*(specials.begin()+g.n(unsigned(specials.size())));
        switch(g.n(5)){case 0:return std::nextafter(v,INFINITY);case 1:return std::nextafter(v,-INFINITY);case 2:return -v;default:return v;}};
    unsigned seekCases=0,holdCases=0,completionCases=0,removed=0,clampedLow=0,clampedHigh=0;
    const int seekSemantics[]={24,25,32,33,29,30,37,38};
    for(unsigned test=0;test<120000;++test){
        const int semantic=seekSemantics[g.n(8)];const bool pivot=semantic==29||semantic==30||semantic==37||semantic==38;
        const float d268=value({0.f,1.f,.5f,-0.f}),p280=value({0.f,1.f,.5f,-1.f,.15f,.85f}),ts=g.coin(3)?g.uni(.25f,2):1.f,duration=g.uni(.05f,4);
        fadeResult=g.coin(4);wr(RIDER+0x268,d268);wr(RIDER+0x280,p280);wr(SEQ,semantic);wr(SEQ+0x10,duration);calls.clear();
        auto c=context(pivot?0x1047f0:0x104728);SET_GPR_U32(&c,4,ANIM);SET_GPR_U32(&c,5,LIST);SET_GPR_U32(&c,6,SEQ);c.f[12]=ts;
        (pivot?sub_001047F0_0x1047f0:sub_00104728_0x104728)(M,&c,&rt);if(c.pc!=DONE)return 3;
        ssx::OriginalRounding rounding;
        const float amount=ssx::originalBoardPressSeekAmount(pivot?13:14,semantic,d268,p280);
        std::vector<Call> expected{{0x313cf0,0,fb(ssx::terrain_original::mul(amount,duration))},{0x313800,0,fb(ssx::terrain_original::mul(ts,std::bit_cast<float>(0x3c888889u)))}};
        if(fadeResult)expected.push_back({0x3145f8});
        if(!compareCalls(calls,expected,pivot?"kind 13 0x1047F0":"kind 14 0x104728",test))return 4;
        if(GPR_U32((&c),2)!=uint32_t(fadeResult)){std::printf("seek return mismatch %u\n",test);return 5;}
        ++seekCases;removed+=fadeResult;clampedLow+=amount==0;clampedHigh+=amount==1;
    }
    for(unsigned test=0;test<60000;++test){
        const int semantic=g.coin()?28:36;const float d274=value({0.f,1.f,.5f,.25f,.75f}),ts=g.coin(3)?g.uni(.25f,2):1.f;fadeResult=g.coin();
        wr(RIDER+0x274,d274);wr(SEQ,semantic);calls.clear();
        auto c=context(0x1046b0);SET_GPR_U32(&c,4,ANIM);SET_GPR_U32(&c,5,LIST);SET_GPR_U32(&c,6,SEQ);c.f[12]=ts;sub_001046B0_0x1046b0(M,&c,&rt);if(c.pc!=DONE)return 6;
        auto leaves=ssx::originalBoardPressHoldLeaves(semantic);
        std::vector<Call> expected{{0x103cc8,int32_t(leaves[0]|leaves[1]<<10|leaves[2]<<20),fb(ssx::originalBoardPressHoldAmount(d274))},{0x103cc9,0,fb(ts)}};
        if(!compareCalls(calls,expected,"kind 15 0x1046B0",test))return 7;
        if(GPR_U32((&c),2)!=uint32_t(fadeResult))return 8;++holdCases;
    }
    for(unsigned test=0;test<20000;++test){
        const int32_t press=g.coin(4)?int32_t(g.g()):int32_t(g.n(3));wr(RIDER+0x330,press);
        const uint64_t latched=(uint64_t(g.g())<<32)|g.g(),raised=(uint64_t(g.g())<<32)|g.g();wr(SEQ+0xb0,latched);wr(SEQ+0xb8,raised);calls.clear();
        auto c=context(0x104bd8);SET_GPR_U32(&c,4,ANIM);SET_GPR_U32(&c,5,LIST);SET_GPR_U32(&c,6,SEQ);sub_00104BD8_0x104bd8(M,&c,&rt);if(c.pc!=DONE)return 9;
        std::vector<Call> expected{{0x3128e8,ssx::originalBoardPressCompletionSemantic(press)}};
        const uint64_t bit=uint64_t(1)<<63;
        if(!compareCalls(calls,expected,"completion kind 9 0x104BD8",test))return 10;
        if(rd<uint64_t>(SEQ+0xb0)!=(latched&~bit)||rd<uint64_t>(SEQ+0xb8)!=(raised|(latched&bit))){std::printf("completion flags mismatch %u\n",test);return 11;}
        ++completionCases;
    }
    std::printf("board press animation drivers: %u kind 13/14 seeks match (0x313CF0 time, 0x313800 dt, 0x3145F8 removal %u; amount clamped to 0 in %u, 1 in %u); %u kind-15 0x103CC8 calls match (leaves, clamp(2x-1), time scale); %u completion-kind-9 cases match (28/36, flag 63 latched->raised)\n",
        seekCases,removed,clampedLow,clampedHigh,holdCases,completionCases);
}
