// Development-only instruction oracle: original board-press scoring 0x1199F8 (press,
// with the real 0x1176F8), 0x119AD8 (pivot, with the real 0x119898) and 0x119A38 (end,
// with the real 0x117838 reset; 0x11A228 commit is a recording stub) against
// engine/board_press.hpp. The state helpers are shared with the handplant score oracle.
#include "handplant_reference_common.hpp"
#include "../engine/board_press.hpp"
void sub_001199F8_0x1199f8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00119AD8_0x119ad8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00119898_0x119898(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00119A38_0x119a38(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001176F8_0x1176f8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00117838_0x117838(uint8_t*,R5900Context*,PS2Runtime*);
using namespace hp;
using State=ssx::OriginalScoreBoundaryState;
static unsigned commits=0;static std::array<int32_t,5> commitArguments{};static float returned=0,seenAccumulated=0;static int32_t seenField80=0;
static const std::pair<unsigned,float State::*> directFloats[]={{0x18,&State::multiplier18},{0x1c,&State::inverted1C},{0x30,&State::airSeconds30},{0x6c,&State::activeSeconds6C},{0x78,&State::timer78}};
static const std::pair<unsigned,int32_t State::*> directInts[]={{0x74,&State::field74},{0x80,&State::field80},{0x88,&State::threshold88},{0x90,&State::threshold90},{0x94,&State::threshold94},{0x98,&State::threshold98}};
static const std::pair<unsigned,float ssx::OriginalGrabScoreState::*> scoreFloats[]={{0x14,&ssx::OriginalGrabScoreState::accumulated14},{0x3c,&ssx::OriginalGrabScoreState::holdIncrement3C},{0x40,&ssx::OriginalGrabScoreState::holdSeconds40},
    {0x44,&ssx::OriginalGrabScoreState::totalSeconds44},{0x48,&ssx::OriginalGrabScoreState::longestSeconds48},{0xa4,&ssx::OriginalGrabScoreState::comboTimeoutA4},{0x1c4,&ssx::OriginalGrabScoreState::multiplier1C4}};
static const std::pair<unsigned,int32_t ssx::OriginalGrabScoreState::*> scoreInts[]={{0x4c,&ssx::OriginalGrabScoreState::normalCount4C},{0x50,&ssx::OriginalGrabScoreState::tweakCount50},{0x54,&ssx::OriginalGrabScoreState::uberCount54},
    {0x58,&ssx::OriginalGrabScoreState::superUberCount58},{0x5c,&ssx::OriginalGrabScoreState::activeUber5C},{0x84,&ssx::OriginalGrabScoreState::bonusPoints84},{0x8c,&ssx::OriginalGrabScoreState::holdThresholdIndex8C}};
static const std::pair<unsigned,float ssx::OriginalTrickIdentityState::*> identityFloats[]={{0x24,&ssx::OriginalTrickIdentityState::time24},{0x2c,&ssx::OriginalTrickIdentityState::time2C},{0x34,&ssx::OriginalTrickIdentityState::spin34},{0x38,&ssx::OriginalTrickIdentityState::flip38}};
static const std::pair<unsigned,int32_t ssx::OriginalTrickIdentityState::*> identityInts[]={{0,&ssx::OriginalTrickIdentityState::stance00},{4,&ssx::OriginalTrickIdentityState::field04},{8,&ssx::OriginalTrickIdentityState::field08},
    {0xc,&ssx::OriginalTrickIdentityState::style0C},{0x10,&ssx::OriginalTrickIdentityState::flag10},{0x20,&ssx::OriginalTrickIdentityState::style20},{0x28,&ssx::OriginalTrickIdentityState::flag28},{0x70,&ssx::OriginalTrickIdentityState::active70},{0x7c,&ssx::OriginalTrickIdentityState::field7C}};
static void storeState(const State& s,std::vector<uint8_t>* img){
    for(auto [o,f]:directFloats)put(img,SCORER+o,s.*f);for(auto [o,f]:directInts)put(img,SCORER+o,s.*f);
    for(auto [o,f]:scoreFloats)put(img,SCORER+o,s.score.*f);for(auto [o,f]:scoreInts)put(img,SCORER+o,s.score.*f);
    for(auto [o,f]:identityFloats)put(img,SCORER+o,s.identity.*f);for(auto [o,f]:identityInts)put(img,SCORER+o,s.identity.*f);
    for(int i=0;i<3;i++){if(s.score.history60[i]!=s.identity.grabs60[i])throw std::runtime_error("history/grabs alias diverged");put(img,SCORER+0x60+4*i,s.score.history60[i]);}
}
static float randomFloat(Rand& g){
    switch(g.n(8)){case 0:return 0;case 1:return -0.f;case 2:return -1;case 3:return g.uni(-1e-6f,1e-6f);case 4:return g.uni(-1e7f,1e7f);case 5:return std::bit_cast<float>(0x3d4cccccu)*float(int(g.n(5))-2);default:return g.uni(-50,50);}
}
static State randomState(Rand& g){
    State s;for(auto [o,f]:directFloats)s.*f=randomFloat(g);for(auto [o,f]:directInts)s.*f=int32_t(g.coin(3)?g.g():g.n(9));
    for(auto [o,f]:scoreFloats)s.score.*f=randomFloat(g);for(auto [o,f]:scoreInts)s.score.*f=int32_t(g.coin(3)?g.g():g.n(9));
    for(auto [o,f]:identityFloats)s.identity.*f=randomFloat(g);for(auto [o,f]:identityInts)s.identity.*f=int32_t(g.coin(3)?g.g():g.n(9));
    if(g.coin(10))s.field80=int32_t(0x7fffffff);
    for(int i=0;i<3;i++)s.score.history60[i]=s.identity.grabs60[i]=int32_t(g.g());
    if(g.coin(4)){float a=s.score.accumulated14;int e=int((std::bit_cast<uint32_t>(std::bit_cast<float>(0x3d4cccccu))>>23)&255)+int(g.n(52))-26;s.score.accumulated14=std::bit_cast<float>((std::bit_cast<uint32_t>(a)&0x807fffffu)|(uint32_t(std::clamp(e,1,254))<<23));}
    return s;
}
int main(int argc,char**argv){
    if(argc!=2)return 2;auto ram=loadElf(argv[1]);M=ram.data();baseLayout();
    PS2Runtime rt;rt.registerFunction(0x1176f8,sub_001176F8_0x1176f8);rt.registerFunction(0x117838,sub_00117838_0x117838);rt.registerFunction(0x119898,sub_00119898_0x119898);
    rt.registerFunction(0x3e6448,[](uint8_t*m,R5900Context*c,PS2Runtime*){std::memset(m+GPR_U32(c,4),GPR_U32(c,5)&255,GPR_U32(c,6));c->pc=GPR_U32(c,31);});
    rt.registerFunction(0x11a228,[](uint8_t*,R5900Context*c,PS2Runtime*){
        if(GPR_U32(c,4)!=SCORER)throw std::runtime_error("0x11A228 scorer");++commits;for(unsigned i=0;i<5;i++)commitArguments[i]=int32_t(GPR_U32(c,5+i));
        seenAccumulated=rd<float>(SCORER+0x14);seenField80=rd<int32_t>(SCORER+0x80);
        wr(SCORER+0x70,0);wr(SCORER+0xa4,4.5f);wr(SCORER+0x1c4,2.25f);c->f[0]=returned;c->pc=GPR_U32(c,31);});
    std::fesetround(FE_TOWARDZERO);Rand g(0x1199f8);
    auto style=[&]{return g.coin(4)?int32_t(g.g()):int32_t(g.n(3));};
    unsigned pressCases=0,pivotCases=0,endCases=0,clamped=0,airKept=0;
    for(unsigned test=0;test<40000;++test){
        auto s=randomState(g);if(g.coin(3))s.identity.time2C=g.coin()?-0.f:0.f;std::memset(M+SCORER,0xa5,0x1c8);storeState(s,nullptr);
        const int32_t st=style();auto& expected=snapshot();
        auto c=context(0x1199f8);SET_GPR_U32(&c,4,SCORER);SET_GPR_U32(&c,5,uint32_t(st));c.f[0]=123;sub_001199F8_0x1199f8(M,&c,&rt);if(c.pc!=DONE)return 3;
        float award=ssx::originalBoardPressScorePress(s,st);storeState(s,&expected);
        if(!same(award,c.f[0])){std::printf("score press return mismatch case %u\n",test);return 4;}
        if(!compareImage(expected,"score press 0x1199F8",test))return 5;++pressCases;
    }
    for(unsigned test=0;test<60000;++test){
        auto s=randomState(g);if(g.coin(3))s.identity.spin34=g.uni(-20,20);if(g.coin(4))s.score.accumulated14=g.uni(-.1f,.1f);
        std::memset(M+SCORER,0xa5,0x1c8);storeState(s,nullptr);const int32_t st=style();auto& expected=snapshot();
        auto c=context(0x119ad8);SET_GPR_U32(&c,4,SCORER);SET_GPR_U32(&c,5,uint32_t(st));c.f[0]=123;sub_00119AD8_0x119ad8(M,&c,&rt);if(c.pc!=DONE)return 6;
        float award=ssx::originalBoardPressScorePivot(s,st);storeState(s,&expected);clamped+=s.score.accumulated14==0;
        if(!same(award,c.f[0])){std::printf("score pivot return mismatch case %u\n",test);return 7;}
        if(!compareImage(expected,"score pivot 0x119AD8",test))return 8;++pivotCases;
    }
    for(unsigned test=0;test<40000;++test){
        auto s=randomState(g);std::memset(M+SCORER,0xa5,0x1c8);storeState(s,nullptr);returned=randomFloat(g);commits=0;auto& expected=snapshot();
        auto c=context(0x119a38);SET_GPR_U32(&c,4,SCORER);sub_00119A38_0x119a38(M,&c,&rt);if(c.pc!=DONE)return 9;
        unsigned nativeCommits=0;
        float result=ssx::originalBoardPressScoreEnd(s,[&](State& state,int a,int b,int d,int e,int flag){
            ++nativeCommits;if(commitArguments!=std::array<int32_t,5>{a,b,d,e,flag}||!same(seenAccumulated,state.score.accumulated14)||seenField80!=state.field80)throw std::runtime_error("0x11A228 commit inputs differ");
            state.identity.active70=0;state.score.comboTimeoutA4=4.5f;state.score.multiplier1C4=2.25f;return returned;});
        storeState(s,&expected);airKept+=commitArguments[4]==1;
        if(commits!=1||nativeCommits!=1||!same(result,c.f[0])){std::printf("score end commit/return mismatch case %u\n",test);return 10;}
        if(!compareImage(expected,"score end 0x119A38",test))return 11;++endCases;
    }
    std::printf("board press scoring: %u 0x1199F8 press cases match (+0x28/+0x10 style, +0x2C max 0, 0x1176F8, return); %u 0x119AD8/0x119898 pivot cases match (+0x28, +0x34 += pi, +0x14 trade clamped at 0 in %u, return); %u 0x119A38 end cases match (0x11A228 arguments 0,0,+0x20,0,+0x30>=0 [t1=1 in %u], 0x117838 reset, +0x30/+0x20/+0x24 kept, +0x0C, return)\n",
        pressCases,pivotCases,clamped,endCases,airKept);
}
