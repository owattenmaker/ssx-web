// Development-only conformance of the complete human input provider against
// recompiled original instructions: INPUT.MAP mapping VM 0x325450 (through the
// real 0x320BF0/0x320C48 getters and the snapshot's compiled map), grab choice
// 0x1276F0, rail Uber identity 0x127848, and command packing 0x127998.
#include "ps2_runtime_macros.h"
#include "../engine/original_input.hpp"
#include "../engine/original_input_provider.hpp"
#include "../engine/original_command.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <random>
#include <vector>
#define ORIGINAL(name) void name(uint8_t*,R5900Context*,PS2Runtime*);
ORIGINAL(sub_00127848_0x127848) ORIGINAL(sub_001276F0_0x1276f0) ORIGINAL(sub_0011FEE8_0x11fee8)
ORIGINAL(sub_00320BF0_0x320bf0) ORIGINAL(sub_00320C48_0x320c48) ORIGINAL(sub_00320FA8_0x320fa8)
ORIGINAL(sub_00321108_0x321108) ORIGINAL(sub_00325250_0x325250) ORIGINAL(sub_00325260_0x325260)
ORIGINAL(sub_003252E8_0x3252e8) ORIGINAL(sub_003252F8_0x3252f8) ORIGINAL(sub_00325430_0x325430)
ORIGINAL(sub_00325450_0x325450)
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000;
extern const uint32_t g_ps2RecompiledFunctionTableEnd=0x410000;
extern const uint32_t g_ps2RecompiledFunctionTableSlotCount=0xc4000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc4000]={nullptr};
namespace {
constexpr unsigned count=ssx::originalGameplayActionCount;
ssx::OriginalActionValues stubActions{};
std::vector<uint8_t> ram;
PS2Runtime rt;
unsigned owner=0,padContext=0,pad=0;
uint32_t ru(unsigned at){uint32_t x;std::memcpy(&x,ram.data()+at,4);return x;}
template<class T> void wr(unsigned at,const T& v){std::memcpy(ram.data()+at,&v,sizeof(v));}
R5900Context context(){R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,0x10000);SET_GPR_U32(&c,31,0x12345678);return c;}
[[noreturn]] void fail(const char* what,unsigned index,int state,uint32_t expected0,uint32_t got0,uint32_t expected1=0,uint32_t got1=0){
    std::printf("%s case %u state %d original %08x %08x native %08x %08x\n",what,index,state,expected0,expected1,got0,got1);std::exit(20);
}
void finish(R5900Context& c,const char* what){
    // 0x127ee0/0x127fd8 JAL 0x127848 inside the same generated function returns at its guest continuation.
    while(c.pc==0x127ee8||c.pc==0x127fe0)sub_00127848_0x127848(ram.data(),&c,&rt);
    if(c.pc!=0x12345678){std::printf("%s bad continuation %x\n",what,c.pc);std::exit(21);}
}
ssx::OriginalCommandWords provider(int state){
    wr(owner+0xde4,int32_t(state));wr(0x90000,uint64_t(0xa5a5a5a5a5a5a5a5ull));
    auto c=context();SET_GPR_U32(&c,4,owner);SET_GPR_U32(&c,5,0x90000);c.pc=0x127998;
    sub_00127848_0x127848(ram.data(),&c,&rt);finish(c,"provider");
    return {ru(0x90000),ru(0x90004)};
}
int callInt(void(*fn)(uint8_t*,R5900Context*,PS2Runtime*),uint32_t pc){
    auto c=context();SET_GPR_U32(&c,4,owner);c.pc=pc;fn(ram.data(),&c,&rt);finish(c,"selector");return int32_t(GPR_U32((&c),2));
}
[[noreturn]] void failDecode(const char* what,unsigned index,int state,uint32_t word0,uint32_t word1){
    std::printf("decoded %s case %u state %d words %08x %08x\n",what,index,state,word0,word1);std::exit(19);
}
uint32_t bitsOf(float x){return std::bit_cast<uint32_t>(x);}
// Decoded six-bit field of an action value; CVT.W.S saturates outside int32.
float expectedAxis(float v){
    if(double(v)*31.0>=2147483648.0)return std::bit_cast<float>(0xbd042108u); // 0x7FFFFFFF&63 = -1
    if(double(v)*31.0<-2147483647.0)return 0.f;                               // 0x80000000&63 = 0
    return ssx::originalQuantizeAxis(v);
}
// Driver value for one pressure byte: byte*float(0x3B808081) toward zero (0x327210..0x3276BC).
float padByte(unsigned byte){return ssx::originalDecodePad([&]{ssx::OriginalPadPacket p{};p[2]=p[3]=255;p[4]=p[5]=p[6]=p[7]=128;p[8]=uint8_t(byte);return p;}())[4];}
// Every decoded field must equal the native action values that produced the words.
void checkDecode(int state,const ssx::OriginalCommandWords& w,const ssx::OriginalActionValues& a,unsigned index,std::array<unsigned,16>& strictAccepted){
    using A=ssx::OriginalAction;
    auto f=ssx::originalDecodeCommandFields(state,w.word0,w.word1);
    auto flag=[&](A x,bool got){if(f.carries(x)&&got!=ssx::originalActionActive(a[unsigned(x)]))failDecode(ssx::originalActionName(x),index,state,w.word0,w.word1);};
    auto axis=[&](A x,float got){if(f.carries(x)&&bitsOf(got)!=bitsOf(expectedAxis(a[unsigned(x)])))failDecode(ssx::originalActionName(x),index,state,w.word0,w.word1);};
    flag(A::ResetPath,f.resetPath);flag(A::Handplant,f.handplant);flag(A::JumpPressed,f.jumpPressed);flag(A::JumpHeld,f.jumpHeld);
    flag(A::BoostPressed,f.boostPressed);flag(A::BoostHeld,f.boostHeld);flag(A::AttackLeft,f.attackLeft);flag(A::AttackRight,f.attackRight);
    flag(A::OllieHeld,f.ollieHeld);flag(A::Tweak,f.tweak);flag(A::LateSpin,f.lateSpin);flag(A::WipeoutRecover,f.wipeoutRecover);
    axis(A::CruiseTurn,f.cruiseTurn);axis(A::CruiseCrouch,f.cruiseCrouch);axis(A::CruiseBrake,f.cruiseBrake);
    if(state!=5)axis(A::BoardPress,f.boardPress);
    axis(A::BoardPivot,f.boardPivot);axis(A::PrewindTurn,f.prewindTurn);axis(A::PrewindSpin,f.prewindSpin);axis(A::PrewindFlip,f.prewindFlip);
    axis(A::AirAdjRotFB,f.airAdjRotFB);axis(A::AirAdjRotLR,f.airAdjRotLR);axis(A::RailSpin,f.railSpin);axis(A::Spin,f.spin);axis(A::Flip,f.flip);
    axis(A::RailBalance,f.railBalance);axis(A::HandplantBalance,f.handplantBalance);axis(A::GateAnticipate,f.gateAnticipate);
    if(state==5){float p=a[unsigned(A::BoardPress)];int step=p<-.5f?-1:p>.5f?1:0;if(f.boardPressStep!=step)failDecode("BoardPress step",index,state,w.word0,w.word1);}
    if((state==4||state==5)&&f.grab!=ssx::originalSelectedGrab(a))failDecode("decoded grab",index,state,w.word0,w.word1);
    if((state==7||state==12)&&f.railUberIdentity!=ssx::originalRailUberSelection(a))failDecode("decoded Uber identity",index,state,w.word0,w.word1);
    unsigned expected=0;for(unsigned i=0;i<count;i++)if(f.carries(ssx::OriginalAction(i)))expected++;
    if(state==9||state==13||state<0||state>13){if(f.carried||w.word0||w.word1)failDecode("empty controller",index,state,w.word0,w.word1);}
    else if(!expected)failDecode("nothing carried",index,state,w.word0,w.word1);
    // The lenient RiderInput view equals the strict pre-existing decoder whenever it accepts.
    auto lenient=ssx::originalCommandRiderInput(state,w.word0,w.word1);
    if(state==0||state==2||state==3||state==4||state==5){
        ssx::RiderInput strict;bool accepted=true;
        try{strict=ssx::originalDecodeCommand(state,w.word0,w.word1);}catch(const std::exception&){accepted=false;}
        if(accepted){
            strictAccepted[unsigned(state)]++;
            for(auto m:{&ssx::RiderInput::turn,&ssx::RiderInput::crouch,&ssx::RiderInput::brake,&ssx::RiderInput::boardPress,&ssx::RiderInput::boardPivot,
                &ssx::RiderInput::prewindTurn,&ssx::RiderInput::spin,&ssx::RiderInput::flip,&ssx::RiderInput::airAdjustLR,&ssx::RiderInput::airAdjustFB,
                &ssx::RiderInput::railSpin,&ssx::RiderInput::railBalance,&ssx::RiderInput::handplantBalance,&ssx::RiderInput::gateAnticipate})
                if(bitsOf(strict.*m)!=bitsOf(lenient.*m))failDecode("strict/lenient axis",index,state,w.word0,w.word1);
            for(auto m:{&ssx::RiderInput::jumpHeld,&ssx::RiderInput::jumpPressed,&ssx::RiderInput::boostHeld,&ssx::RiderInput::boostPressed,&ssx::RiderInput::handplant,
                &ssx::RiderInput::ollieHeld,&ssx::RiderInput::recoverPressed,&ssx::RiderInput::pausePressed,&ssx::RiderInput::passiveUpper14,&ssx::RiderInput::passiveUpper15,
                &ssx::RiderInput::attackLeft,&ssx::RiderInput::attackRight,&ssx::RiderInput::tweak,&ssx::RiderInput::lateSpin,&ssx::RiderInput::wipeoutRecover})
                if(strict.*m!=lenient.*m)failDecode("strict/lenient flag",index,state,w.word0,w.word1);
            if(strict.grabMask!=lenient.grabMask||strict.passiveInputCode!=lenient.passiveInputCode||strict.railUberIdentity!=lenient.railUberIdentity)
                failDecode("strict/lenient code",index,state,w.word0,w.word1);
        }
    }else if(state>=0&&state<=13){
        auto viaStrict=ssx::originalDecodeCommand(state,w.word0,w.word1); // new states route to the full decoder
        if(viaStrict.railUberIdentity!=lenient.railUberIdentity||bitsOf(viaStrict.railBalance)!=bitsOf(lenient.railBalance))failDecode("strict routing",index,state,w.word0,w.word1);
    }
}
}
int main(int argc,char**argv){
    if(argc<2)return 2;
    {std::ifstream input(argv[1],std::ios::binary);ram.assign(std::istreambuf_iterator<char>(input),{});}
    if(ram.size()!=32*1024*1024)return 2;
    unsigned rider=0x14701a0;owner=ru(rider+0x77c);padContext=ru(owner+0xdf0);pad=ru(padContext);
    unsigned iface=ru(owner+0xde8);
    if(ru(owner+0x18)!=rider||ru(iface+12)!=0x127998||ru(pad)!=24){std::puts("unexpected snapshot input provider layout");return 3;}
    wr(owner+0xdf8,uint32_t(0)); // accepted-command recorder 0x26D178 is a side effect, not an input
    std::fesetround(FE_TOWARDZERO);
    rt.registerFunction(0x3E6448,[](uint8_t*m,R5900Context*c,PS2Runtime*){std::memset(m+GPR_U32(c,4),GPR_U32(c,5)&255,GPR_U32(c,6));c->pc=GPR_U32(c,31);});
    rt.registerFunction(0x11FEE8,sub_0011FEE8_0x11fee8); // reads *(*(rider+0x77C)+0xDE4)
    rt.registerFunction(0x1276F0,sub_001276F0_0x1276f0);
    std::mt19937_64 rng(0x127998);
    auto uniform=[&](float lo,float hi){return lo+(hi-lo)*float(double(rng()>>11)*(1.0/9007199254740992.0));};
    std::vector<int> states;for(int s=-1;s<=15;s++)states.push_back(s);states.push_back(100);
    std::array<unsigned,16> strictAccepted{};
    // Phase A: provider packing with arbitrary action values through stubbed getters.
    // This isolates 0x127998's field layout from INPUT.MAP (e.g. LateSpin is literal 0 there).
    rt.registerFunction(0x320BF0,[](uint8_t*,R5900Context*c,PS2Runtime*){c->f[0]=stubActions.at(GPR_U32(c,5));c->pc=GPR_U32(c,31);});
    rt.registerFunction(0x320C48,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,stubActions.at(GPR_U32(c,5))!=0.f);c->pc=GPR_U32(c,31);});
    constexpr unsigned stubCases=20000;
    for(int state:states)for(unsigned n=0;n<stubCases;n++){
        for(unsigned i=0;i<count;i++){
            unsigned kind=unsigned(rng()%100);float v;
            if(i>=27&&i<=45)v=kind<85?0.f:kind<95?1.f:uniform(-2,2); // selector priority with several active
            else if(kind<25)v=0.f;else if(kind<28)v=-0.f;else if(kind<35)v=(rng()&1)?1.f:-1.f;
            else if(kind<60)v=uniform(-1,1);else if(kind<70)v=uniform(-2.5f,2.5f);
            else if(kind<82){ // quantization boundaries k/31 and their neighbours
                v=float(int(rng()%127)-63)/31.f;for(unsigned d=unsigned(rng()%5);d<2;d++)v=std::nextafter(v,-INFINITY);
                for(unsigned d=unsigned(rng()%5);d>2;d--)v=std::nextafter(v,INFINITY);}
            else if(kind<92){v=(rng()&1)?.5f:-.5f;unsigned d=unsigned(rng()%3);if(d==0)v=std::nextafter(v,-INFINITY);if(d==2)v=std::nextafter(v,INFINITY);}
            else if(kind<97)v=uniform(-1,1)*std::ldexp(1.f,int(rng()%40)-8); // saturating CVT.W.S up to 2^31
            else v=std::bit_cast<float>(uint32_t(rng()%0x800000)|(uint32_t(rng()&1)<<31)); // subnormals
            stubActions[i]=v;
        }
        auto original=provider(state);auto native=ssx::originalPackCommand(state,stubActions);
        if(original!=native)fail("stub packing",n,state,original.word0,native.word0,original.word1,native.word1);
        if(original.word0&0xfff)fail("run-length bits",n,state,original.word0,native.word0);
        std::fesetround(FE_TONEAREST); // native API must not rely on the caller's rounding mode
        checkDecode(state,native,stubActions,n,strictAccepted);
        std::fesetround(FE_TOWARDZERO);
    }
    std::printf("%u stubbed-action provider packings match 0x127998 for control states -1..15 and 100\n",stubCases*unsigned(states.size()));
    // Phase B: complete path from button records through the original compiled map.
    rt.registerFunction(0x320BF0,sub_00320BF0_0x320bf0);rt.registerFunction(0x320C48,sub_00320C48_0x320c48);
    rt.registerFunction(0x320FA8,sub_00320FA8_0x320fa8);rt.registerFunction(0x321108,sub_00321108_0x321108);
    rt.registerFunction(0x325250,sub_00325250_0x325250);rt.registerFunction(0x325260,sub_00325260_0x325260);
    rt.registerFunction(0x3252e8,sub_003252E8_0x3252e8);rt.registerFunction(0x3252f8,sub_003252F8_0x3252f8);
    rt.registerFunction(0x325430,sub_00325430_0x325430);rt.registerFunction(0x325450,sub_00325450_0x325450);
    constexpr unsigned padCases=32768;
    ssx::OriginalPadState history{};ssx::OriginalPadPacket packet{};packet[2]=packet[3]=255;
    for(unsigned i=4;i<8;i++)packet[i]=128;
    std::array<unsigned,count> nonzero{};std::array<unsigned,16> grabSeen{},uberSeen{};unsigned realistic=0;
    for(unsigned n=0;n<padCases;n++){
        ssx::OriginalPadState state{};bool pressure=rng()&1,analog=(rng()%8)!=0;
        if(n%2==0){
            // A consumed-sample history: buttons change rarely, so held/pressed/released/repeat vary.
            for(unsigned b:{2u,3u})for(unsigned bit=0;bit<8;bit++)if(rng()%6==0)packet[b]^=uint8_t(1u<<bit);
            for(unsigned i=4;i<8;i++){unsigned k=unsigned(rng()%8);if(k==0)packet[i]=uint8_t(rng());else if(k==1)packet[i]=(rng()&1)?255:0;else if(k==2)packet[i]=128;}
            for(unsigned i=8;i<20;i++){if(rng()%4==0)packet[i]=uint8_t(rng()%3==0?255:rng());}
            ssx::OriginalPadValues sample=ssx::originalDecodePad(packet,pressure,analog);
            auto words=ssx::originalHumanInputTick(history,sample,int(rng()%14));(void)words;
            state=history;realistic++;
        }else{
            for(auto& b:state){
                unsigned k=unsigned(rng()%100);
                b.value=k<30?0.f:k<45?1.f:k<50?-0.f:k<75?uniform(0,1):k<88?padByte(unsigned(rng()&255)):k<97?uniform(-2,2):
                    std::bit_cast<float>(uint32_t(rng()%0x800000));
                auto flag=[&]{unsigned f=unsigned(rng()%100);return f<55?0u:f<95?1u:uint32_t(rng());};
                b.pressed=flag();b.released=flag();b.held=flag();b.repeat=flag();b.repeatTimer=uint32_t(rng());b.edgeAge=uint32_t(rng());
            }
        }
        std::memcpy(ram.data()+pad+4,state.data(),sizeof(state));
        std::fesetround(FE_TONEAREST);
        auto actions=ssx::originalEvaluateActions(state);
        std::fesetround(FE_TOWARDZERO);
        for(unsigned action=0;action<count;action++){
            auto c=context();SET_GPR_U32(&c,4,padContext);SET_GPR_U32(&c,5,action);c.pc=0x320BF0;
            sub_00320BF0_0x320bf0(ram.data(),&c,&rt);finish(c,"float getter");
            if(bitsOf(c.f[0])!=bitsOf(actions[action])){std::printf("action %s case %u original %.9g (%08x) native %.9g (%08x)\n",ssx::originalActionName(ssx::OriginalAction(action)),n,c.f[0],bitsOf(c.f[0]),actions[action],bitsOf(actions[action]));return 22;}
            if(bitsOf(ssx::originalEvaluateAction(state,ssx::OriginalAction(action)))!=bitsOf(actions[action]))return 23;
            c=context();SET_GPR_U32(&c,4,padContext);SET_GPR_U32(&c,5,action);c.pc=0x320C48;
            sub_00320C48_0x320c48(ram.data(),&c,&rt);finish(c,"bool getter");
            if(bool(GPR_U32((&c),2))!=ssx::originalActionActive(actions[action])){std::printf("bool action %u case %u\n",action,n);return 24;}
            if(actions[action]!=0.f)nonzero[action]++;
        }
        int grab=callInt(sub_001276F0_0x1276f0,0x1276F0),uber=callInt(sub_00127848_0x127848,0x127848);
        if(grab!=ssx::originalSelectedGrab(actions)||uber!=ssx::originalRailUberSelection(actions)){std::printf("selector case %u grab %d/%d uber %d/%d\n",n,grab,ssx::originalSelectedGrab(actions),uber,ssx::originalRailUberSelection(actions));return 25;}
        grabSeen[unsigned(grab+1)]++;uberSeen[unsigned(uber+1)]++;
        for(int s:states){
            auto original=provider(s);
            std::fesetround(FE_TONEAREST);
            auto native=ssx::originalProvideCommand(s,state);
            if(original!=native)fail("pad provider",n,s,original.word0,native.word0,original.word1,native.word1);
            checkDecode(s,native,actions,n,strictAccepted);
            std::fesetround(FE_TOWARDZERO);
        }
    }
    for(unsigned i=0;i<count;i++)if(i!=unsigned(ssx::OriginalAction::LateSpin)&&!nonzero[i]){std::printf("action %u never exercised\n",i);return 26;}
    for(unsigned i=0;i<16;i++)if(!grabSeen[i]){std::printf("grab %d never selected\n",int(i)-1);return 27;}
    for(unsigned i=0;i<5;i++)if(!uberSeen[i]){std::printf("uber %d never selected\n",int(i)-1);return 28;}
    std::printf("%u pad states (%u consumed-sample histories): all %u actions match mapping VM 0x325450 through 0x320BF0/0x320C48 bit-for-bit; 0x1276F0/0x127848 match\n",padCases,realistic,count);
    std::printf("%u pad-state provider commands match 0x127998 bit-for-bit for control states -1..15 and 100; decoded fields match\n",padCases*unsigned(states.size()));
    std::printf("strict originalDecodeCommand accepted (and equals full decoder):");
    for(int s:{0,2,3,4,5})std::printf(" state%d=%u",s,strictAccepted[unsigned(s)]);
    std::printf("\n");
}
