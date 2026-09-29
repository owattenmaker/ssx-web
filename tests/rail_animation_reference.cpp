// Development oracle: original rail animation drivers vs engine/rail_animation.hpp.
// Built and run by tools/test_rail_animation_native.py.
#include "ps2_runtime_macros.h"
#include "../engine/rail_animation.hpp"
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <random>
#include <string>
#include <vector>
void sub_00104238_0x104238(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00103CC8_0x103cc8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003135B0_0x3135b0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003139A8_0x3139a8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00313C50_0x313c50(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00313CF0_0x313cf0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00313D28_0x313d28(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00313D40_0x313d40(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00313800_0x313800(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001326C8_0x1326c8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00104660_0x104660(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001042E0_0x1042e0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00103BE0_0x103be0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00136268_0x136268(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00136508_0x136508(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00312AA0_0x312aa0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_001446A0_0x1446a0(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x320000,g_ps2RecompiledFunctionTableSlotCount=0x88000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0x88000]={nullptr};
namespace {
std::vector<std::string> events;            // ordered oracle events
uint32_t scriptCompleted=0,scriptClass=21,scriptMotion=4,scriptAttached=0;
constexpr unsigned SEQ=0x60000,CONTROL=0x66000,RIDER=0x71000,ANIM=0x70000,OWNER=0x63000,SEQUENCER=0x73000,COMMAND=0x64000,PRIMARY=0x65000;
std::vector<uint8_t> ram;
template<class T> void wr(unsigned a,const T&v){memcpy(ram.data()+a,&v,sizeof(v));}
float rf(unsigned a){float v;memcpy(&v,ram.data()+a,4);return v;}
uint32_t ru(unsigned a){uint32_t v;memcpy(&v,ram.data()+a,4);return v;}
uint32_t bits(float v){return std::bit_cast<uint32_t>(v);}
void ret(R5900Context*c){c->pc=GPR_U32(c,31);}
int fail(const char*what,unsigned i){printf("%s mismatch at %u\n",what,i);for(auto&l:events)printf("  oracle: %s\n",l.c_str());return 1;}
}
int main(int argc,char**argv){
    PS2Runtime rt;std::ifstream f(argv[1],std::ios::binary);ram.assign((std::istreambuf_iterator<char>(f)),{});
    std::map<uint32_t,float> leafDuration;for(int i=2;i<argc;++i){unsigned leaf,word;sscanf(argv[i],"%u=%x",&leaf,&word);leafDuration[leaf]=std::bit_cast<float>(word);}
    rt.registerFunction(0x313C50,sub_00313C50_0x313c50);rt.registerFunction(0x313CF0,sub_00313CF0_0x313cf0);rt.registerFunction(0x313D28,sub_00313D28_0x313d28);rt.registerFunction(0x313D40,sub_00313D40_0x313d40);
    rt.registerFunction(0x3135B0,sub_003135B0_0x3135b0);rt.registerFunction(0x313800,sub_00313800_0x313800);rt.registerFunction(0x3139A8,sub_003139A8_0x3139a8);rt.registerFunction(0x103CC8,sub_00103CC8_0x103cc8);rt.registerFunction(0x103BE0,sub_00103BE0_0x103be0);
    for(unsigned pc:{0x313938,0x313868,0x116930,0x114130,0x113F38,0x113F88})rt.registerFunction(pc,[](uint8_t*,R5900Context*c,PS2Runtime*){ret(c);});
    rt.registerFunction(0x3145F8,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back("remove");ret(c);});
    rt.registerFunction(0x3128E8,[](uint8_t*m,R5900Context*c,PS2Runtime*){uint32_t semantic=GPR_U32(c,5);events.push_back("play "+std::to_string(semantic)+" blend "+std::to_string(bits(c->f[12]))+" flags "+std::to_string(GPR_U32(c,6)));
        memcpy(m+GPR_U32(c,4)+8,&semantic,4);scriptCompleted=0;scriptClass=21;uint64_t zero=0;memcpy(m+PRIMARY+0xB0,&zero,8);SET_GPR_U32(c,2,semantic);ret(c);});
    rt.registerFunction(0x31BE50,[](uint8_t*m,R5900Context*c,PS2Runtime*){float s=std::sin(c->f[12]),k=std::cos(c->f[12]);memcpy(m+GPR_U32(c,4),&s,4);memcpy(m+GPR_U32(c,5),&k,4);ret(c);});
    rt.registerFunction(0x119938,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back("tier "+std::to_string(GPR_U32(c,5))+" style "+std::to_string(GPR_U32(c,6)));c->f[0]=1;ret(c);});
    rt.registerFunction(0x119958,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back("score style "+std::to_string(GPR_U32(c,5)));c->f[0]=1;ret(c);});
    rt.registerFunction(0x10E098,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back("effect "+std::to_string(GPR_U32(c,5)));ret(c);});
    rt.registerFunction(0x311E88,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back("fade channel "+std::to_string(GPR_U32(c,5))+" "+std::to_string(bits(c->f[12])));ret(c);});
    rt.registerFunction(0x116120,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,0);ret(c);});
    rt.registerFunction(0x106848,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,scriptAttached);ret(c);});
    rt.registerFunction(0x312AE8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,scriptCompleted);ret(c);});
    rt.registerFunction(0x311AE8,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,scriptClass);ret(c);});
    rt.registerFunction(0x311B20,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,PRIMARY);ret(c);});
    rt.registerFunction(0x11FE98,[](uint8_t*,R5900Context*c,PS2Runtime*){SET_GPR_U32(c,2,scriptMotion);ret(c);});
    rt.registerFunction(0x11FEC8,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back("control "+std::to_string(GPR_U32(c,5)));ret(c);});
    rt.registerFunction(0x115640,[](uint8_t*,R5900Context*c,PS2Runtime*){events.push_back("restore");ret(c);});
    rt.registerFunction(0x312AA0,sub_00312AA0_0x312aa0);rt.registerFunction(0x1446A0,sub_001446A0_0x1446a0);rt.registerFunction(0x1326C8,sub_001326C8_0x1326c8);
    std::fesetround(FE_TOWARDZERO);std::mt19937 rng(0x104238);std::uniform_real_distribution<float> v(-1,1);
    auto context=[&](){R5900Context c{};c.vu0_vf[0]=_mm_set_ps(1,0,0,0);SET_GPR_U32((&c),28,0x4a30f0);SET_GPR_U32((&c),29,0x10000);SET_GPR_U32((&c),31,0x12345678);return c;};
    wr(ANIM+0x60,uint32_t(RIDER));wr(RIDER+0x784,uint32_t(ANIM));wr(RIDER+0x77C,uint32_t(OWNER));wr(CONTROL+8,uint32_t(RIDER));wr(OWNER+0x2B8,uint32_t(RIDER)); // control 7 block owner+0x2B0 (+8 rider) reached through 0x1326C8
    // 1. Kind-5 driver 0x104238 with the real lookup table, bank durations and 0x103CC8 chain.
    for(unsigned t=0;t<30000;++t){
        int semantic=18+t%3;auto clips=ssx::originalRailCycleClips(semantic);auto leaves=ssx::originalRailCycleLeaves(semantic);
        std::array<float,3> durations{leafDuration.at(leaves.negative),leafDuration.at(leaves.center),leafDuration.at(leaves.positive)};
        ssx::OriginalAnimationSequence s;s.semantic=semantic;s.rate=(v(rng)+1)*1.5f;
        unsigned primary=t%7==0?0:1,secondary=t%5==0?2:0;bool enabled1=t%11!=0;
        s.slots={{clips[primary],durations[primary]*(v(rng)+1)*.5f,durations[primary],v(rng)*2,1,true,true},{clips[secondary],std::abs(v(rng)),durations[secondary],1,0,false,enabled1}};
        if(t%13==0){s.targetWeight=0;s.stopWhenFaded=true;s.fadeRemaining=std::abs(v(rng))*.02f;s.weight=std::abs(v(rng));}
        float balance=t<3?float(int(t)-1):v(rng)*1.2f,timeScale=1+v(rng)*.5f;uint32_t switched=t&1;
        memset(ram.data()+SEQ,0,0xd0);wr(SEQ,uint32_t(semantic));for(unsigned i=0;i<2;++i){unsigned at=SEQ+4+i*28;wr(at,s.slots[i].clip);wr(at+4,s.slots[i].time);wr(at+8,s.slots[i].rate);wr(at+12,s.slots[i].duration);wr(at+16,s.slots[i].weight);wr(at+20,uint32_t(s.slots[i].enabled));wr(at+24,uint32_t(s.slots[i].loop));}
        wr(SEQ+0x90,s.rate);wr(SEQ+0x94,s.weight);wr(SEQ+0x98,s.targetWeight);wr(SEQ+0x9c,s.fadeRemaining);wr(SEQ+0xa0,uint32_t(s.stopWhenFaded));
        wr(RIDER+0x320,switched);wr(RIDER+0x238,balance);
        auto c=context();SET_GPR_U32((&c),4,ANIM);SET_GPR_U32((&c),5,SEQUENCER);SET_GPR_U32((&c),6,SEQ);c.f[12]=timeScale;events.clear();
        sub_00104238_0x104238(ram.data(),&c,&rt);if(c.pc!=0x12345678)return 2;
        bool removed=ssx::originalRailCycleStep(s,clips,durations,balance,switched,timeScale);
        if(removed!=(GPR_U32((&c),2)!=0)||removed!=(events.size()==1))return fail("kind5 removal",t);
        if(bool(ru(SEQ+0xc0))!=s.completed||ru(SEQ+0xc4)==0||!s.seekPending)return fail("kind5 completion/seek",t);
        for(unsigned i=0;i<2;++i){unsigned at=SEQ+4+i*28;if(ru(at)!=s.slots[i].clip||bits(rf(at+4))!=bits(s.slots[i].time)||bits(rf(at+8))!=bits(s.slots[i].rate)||bits(rf(at+12))!=bits(s.slots[i].duration)||bits(rf(at+16))!=bits(s.slots[i].weight)||bool(ru(at+20))!=s.slots[i].enabled||bool(ru(at+24))!=s.slots[i].loop){printf("slot%u clip %x/%x time %x/%x dur %x/%x weight %x/%x\n",i,ru(at),s.slots[i].clip,bits(rf(at+4)),bits(s.slots[i].time),bits(rf(at+12)),bits(s.slots[i].duration),bits(rf(at+16)),bits(s.slots[i].weight));return fail("kind5 slot",t);}}
        if(bits(rf(SEQ+0x94))!=bits(s.weight)||bits(rf(SEQ+0x9c))!=bits(s.fadeRemaining))return fail("kind5 fade",t);
    }
    // 2. Attach semantic 0x1326C8 for every style and both flags.
    for(unsigned t=0;t<16;++t){
        int style=1+t%4;bool airborne=t/4%2;wr(RIDER+0x328,uint32_t(style));
        auto c=context();SET_GPR_U32((&c),4,CONTROL);SET_GPR_U32((&c),5,uint32_t(airborne));events.clear();
        sub_001326C8_0x1326c8(ram.data(),&c,&rt);if(c.pc!=0x12345678)return 2;
        std::string expected="play "+std::to_string(ssx::originalRailAttachSemantic(style,airborne))+" blend "+std::to_string(0xbf800000u)+" flags 0";
        if(events.size()!=1||events[0]!=expected)return fail("attach semantic",t);
    }
    // 3. Kind-9 driver 0x104660 (half-pipe two-way seek, rider+0x244) and kind-10 0x1042E0 (uber BAL_R/BAL_L, rider+0x238).
    for(unsigned t=0;t<40000;++t){
        bool uber=t>=20000;int semantic=uber?218+8*(t%4):(t%2?45:40);auto leaves=uber?ssx::originalRailUberBalanceLeaves(semantic):ssx::originalHalfpipeBalanceLeaves(semantic);
        ssx::OriginalAnimationSequence s;s.semantic=semantic;s.slots={{t%3?ssx::originalRailLeafClipId(leaves.nonNegative):0u,std::abs(v(rng)),1,1,1,false,t%9!=0}};
        if(t%13==0){s.targetWeight=0;s.stopWhenFaded=true;s.fadeRemaining=std::abs(v(rng))*.02f;s.weight=std::abs(v(rng));}
        float amount=t%20000<3?float(int(t%20000)-1):v(rng)*1.5f,timeScale=1+v(rng)*.5f;
        memset(ram.data()+SEQ,0,0xd0);wr(SEQ,uint32_t(semantic));wr(SEQ+4,s.slots[0].clip);wr(SEQ+8,s.slots[0].time);wr(SEQ+12,s.slots[0].rate);wr(SEQ+16,s.slots[0].duration);wr(SEQ+20,s.slots[0].weight);wr(SEQ+24,uint32_t(s.slots[0].enabled));
        wr(SEQ+0x90,s.rate);wr(SEQ+0x94,s.weight);wr(SEQ+0x98,s.targetWeight);wr(SEQ+0x9c,s.fadeRemaining);wr(SEQ+0xa0,uint32_t(s.stopWhenFaded));wr(RIDER+0x244,uber?-9.f:amount);wr(RIDER+0x238,uber?amount:-9.f);wr(RIDER+0x320,uint32_t(t&1));
        auto c=context();SET_GPR_U32((&c),4,ANIM);SET_GPR_U32((&c),5,SEQUENCER);SET_GPR_U32((&c),6,SEQ);c.f[12]=timeScale;events.clear();
        if(uber)sub_001042E0_0x1042e0(ram.data(),&c,&rt);else sub_00104660_0x104660(ram.data(),&c,&rt);if(c.pc!=0x12345678)return 2;
        bool removed=uber?ssx::originalRailUberBalanceStep(s,semantic,leafDuration.at(leaves.negative),leafDuration.at(leaves.nonNegative),amount,timeScale)
            :ssx::originalTwoWayBalanceStep(s,ssx::originalRailLeafClipId(leaves.negative),leafDuration.at(leaves.negative),ssx::originalRailLeafClipId(leaves.nonNegative),leafDuration.at(leaves.nonNegative),amount,timeScale);
        if(removed!=(GPR_U32((&c),2)!=0))return fail("two-way removal",t);
        if(ru(SEQ+4)!=s.slots[0].clip||bits(rf(SEQ+8))!=bits(s.slots[0].time)||bits(rf(SEQ+16))!=bits(s.slots[0].duration)||bits(rf(SEQ+20))!=bits(s.slots[0].weight)||bool(ru(SEQ+24))!=s.slots[0].enabled||ru(SEQ+0xc4)==0){printf("clip %x/%x time %x/%x\n",ru(SEQ+4),s.slots[0].clip,bits(rf(SEQ+8)),bits(s.slots[0].time));return fail("two-way slot",t);}
        if(bits(rf(SEQ+0x94))!=bits(s.weight)||bits(rf(SEQ+0x9c))!=bits(s.fadeRemaining))return fail("two-way fade",t);
    }
    // 4. Control-12 enter 0x136268.
    for(unsigned t=0;t<32;++t){
        int identity=t%4,style=1+t/4%4;uint32_t switched=t/16;wr(CONTROL+4,uint32_t(identity));wr(RIDER+0x328,uint32_t(style));wr(RIDER+0x320,switched);wr(ANIM+0x18,uint32_t(7));wr(CONTROL,uint32_t(5));
        memset(ram.data()+ANIM+0x30,0xEE,32);
        auto c=context();SET_GPR_U32((&c),4,CONTROL);events.clear();sub_00136268_0x136268(ram.data(),&c,&rt);if(c.pc!=0x12345678)return 2;
        auto e=ssx::originalRailUberEnter(identity,style);
        std::vector<std::string> expected{"play "+std::to_string(e.semantic)+" blend "+std::to_string(0xbf800000u)+" flags 0","tier "+std::to_string(e.tier)+" style "+std::to_string(style),"effect 1","fade channel 1 "+std::to_string(bits(e.channelFade)),"fade channel 0 "+std::to_string(bits(e.channelFade))};
        if(events!=expected||ru(RIDER+0x328)!=3||ru(CONTROL)!=0)return fail("uber enter",t);
        if(ru(RIDER+0x320)!=(e.clearSwitch?0u:switched)||ru(ANIM+0x18)!=(e.clearSwitch?0u:7u))return fail("uber enter switch",t);
        if(e.setRoot){float s=std::sin(e.rootHalfAngle),k=std::cos(e.rootHalfAngle);float ax=rf(0x4FF160),ay=rf(0x4FF164),az=rf(0x4FF168);
            if(memcmp(ram.data()+ANIM+0x30,ram.data()+0x4FF130,16)||bits(rf(ANIM+0x40))!=bits(s*ax)||bits(rf(ANIM+0x44))!=bits(s*ay)||bits(rf(ANIM+0x48))!=bits(s*az)||bits(rf(ANIM+0x4c))!=bits(k))return fail("uber enter root",t);}
        else if(ru(ANIM+0x40)!=0xEEEEEEEEu)return fail("uber enter root untouched",t);
    }
    // 5. Control-12 update 0x136508 as random episodes; the oracle keeps its own phase.
    unsigned ticks=0;
    for(unsigned episode=0;episode<3000;++episode){
        int identity=episode%4,style=1+episode/4%4;ssx::OriginalRailUberState state{0,identity};auto record=ssx::originalRailUberRecord(identity);
        wr(CONTROL,uint32_t(0));wr(CONTROL+4,uint32_t(identity));wr(RIDER+0x328,uint32_t(style));wr(RIDER+0x2F4,uint32_t(episode%12));wr(RIDER+0x2F0,0.f);
        for(unsigned tick=0;tick<40;++tick,++ticks){
            ssx::OriginalRailUberInputs in;in.attached=rng()%23==0;in.commandIdentity=rng()%9==0?int(rng()%5)-1:identity;in.motionMode=std::array{4,4,4,1,0}[rng()%5];
            int pool[]={record.into[0],record.cycle,record.balance,record.land,record.outOf,19,438};in.currentSemantic=pool[rng()%7];in.primaryCompleted=rng()%3==0;in.primaryClass=rng()%4?21:15;in.primaryRaisedBit0=rng()%4==0;
            in.balance238=v(rng)*.3f;in.railBalance=v(rng);
            scriptAttached=in.attached;scriptCompleted=in.primaryCompleted;scriptClass=in.primaryClass;scriptMotion=in.motionMode;
            uint64_t raised=in.primaryRaisedBit0;wr(PRIMARY+0xB0,raised);wr(ANIM+8,uint32_t(in.currentSemantic));wr(RIDER+0x238,in.balance238);wr(OWNER+0xC8,in.railBalance);
            uint32_t command=uint32_t(in.commandIdentity&0xff)<<15;wr(COMMAND,command);wr(RIDER+0x23C,-1.f);wr(RIDER+0x240,-1.f);wr(PRIMARY+0x90,-1.f);uint32_t counter=ru(RIDER+0x2F4);
            auto c=context();SET_GPR_U32((&c),4,CONTROL);SET_GPR_U32((&c),5,COMMAND);events.clear();sub_00136508_0x136508(ram.data(),&c,&rt);if(c.pc!=0x12345678)return 2;
            std::vector<ssx::OriginalRailUberRequest> out;ssx::originalRailUberUpdate(state,in,style,out);
            std::vector<std::string> expected;bool balanceWritten=false;float target=0;bool rate1=false,counterBump=false;
            for(auto&r:out){using R=ssx::OriginalRailUberRequest;switch(r.kind){
                case R::Play:expected.push_back("play "+std::to_string(r.value)+" blend "+std::to_string(0xbf800000u)+" flags 0");break;
                case R::PrimaryRate:rate1=true;break;
                case R::BalanceTarget:balanceWritten=true;target=r.amount;break;
                case R::ReturnToRail:expected.push_back("play "+std::to_string(r.value)+" blend "+std::to_string(0xbf800000u)+" flags 0");break;
                case R::RestoreStance:expected.push_back("restore");break;
                case R::RequestControl:expected.push_back("control "+std::to_string(r.value));break;
                case R::Score:expected.push_back("score style "+std::to_string(r.value));expected.push_back("effect 1");break;
                case R::UberCounter:counterBump=true;break;}}
            if(events!=expected){for(auto&e:expected)printf("  native: %s\n",e.c_str());return fail("uber update events",ticks);}
            if(int(ru(CONTROL))!=state.phase)return fail("uber phase",ticks);
            if(balanceWritten?(bits(rf(RIDER+0x240))!=bits(target)||bits(rf(RIDER+0x23C))!=0x3d6eeef0u):(bits(rf(RIDER+0x240))!=bits(-1.f)))return fail("uber balance target",ticks);
            if(rate1!=(rf(PRIMARY+0x90)==1.f))return fail("uber primary rate",ticks);
            uint32_t expectedCounter=counterBump?(counter<10?counter+1:counter):counter;if(ru(RIDER+0x2F4)!=expectedCounter||(counterBump&&counter<10&&expectedCounter==10&&ru(RIDER+0x2F0)!=0x42700000u)||(!(counterBump&&counter==9)&&ru(RIDER+0x2F0)!=0)){printf("counter before %u after %u expected %u bump %d meter %x\n",counter,ru(RIDER+0x2F4),expectedCounter,int(counterBump),ru(RIDER+0x2F0));return fail("uber counter",ticks);}
            if(state.phase==2&&expected.size()>1)break; // episode finished
        }
    }
    puts("30,000 kind-5 rail cycle ticks bit-identical (real lookup leaves, bank durations, clocks, weights, fades)");
    puts("16 attach semantics, 20,000 kind-9 + 20,000 kind-10 seek ticks, 32 control-12 entries and 3,000 control-12 episodes match");
}
