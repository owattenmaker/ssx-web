// Development-only oracle: the original EA MicroTalk decoder (SND main-RAM voices) run on bank sample data.
// Per job: 0x3CDDB8 (ctx init), 0x3CE0D8(ctx, 1) (EA PCM-patch mode: tag 0x8C >= 3, 0x3C9420), 0x3CE0B8(ctx, {0, 0, 1})
// (fresh state + header parse, as 0x3C9420 / the loop restart 0x3C9520 do), 0x3CDDE8(ctx, data, 4n, n), then
// 0x3CDE68(ctx, &out, n) -> n raw float samples (the frame buffer ctx+0x684 copied out by 0x41605C).
// Usage: microtalk_reference ELF BANK OUT < jobs  (one "offset count" per line; OUT = the jobs' floats back to back).
// The original ELF is mapped at ram+0xFF000; the bank, context and output live in private scratch memory.
// Stream mode (music SCxl segments, the IOP-mixed stream voice): microtalk_reference ELF MUS OUT --stream < jobs, one
// "segOffset segSize version channel chunk nblocks {samples dataOffset}*" per line (dataOffset = the SCDl channel pointer
// relative to the segment, as 0x3B6EA8 builds it). The original stream-voice read 0x3C96F0 (sub_003C92F8, entered through
// an added label) runs on a voice object {+0x24 stream, +0x30 tag 0x80, +0x34 ctx, +0x38 channel}; its block fetch
// 0x3C6DE0 / release 0x3C7010 (the EE ring buffer) are stubbed to hand out the segment's SCDl blocks exactly as 0x3B6EA8
// describes them: count, data pointer, and the "continue" bit (bit 31) set on every block after the first of the SCHl.
#include "ps2_runtime_macros.h"
#include <fstream>
#include <iostream>
#include <cstring>
#include <cstdio>
#include <cfenv>
#include <vector>
#include <cstdlib>
#include <string>
#include <algorithm>
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
void sub_003CD1F8_0x3cd1f8(uint8_t*,R5900Context*,PS2Runtime*);void sub_003CD260_0x3cd260(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003CD2B0_0x3cd2b0(uint8_t*,R5900Context*,PS2Runtime*);void sub_003CD518_0x3cd518(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003CD590_0x3cd590(uint8_t*,R5900Context*,PS2Runtime*);void sub_003CD690_0x3cd690(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003CD6F0_0x3cd6f0(uint8_t*,R5900Context*,PS2Runtime*);void sub_003CD878_0x3cd878(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003CDDB8_0x3cddb8(uint8_t*,R5900Context*,PS2Runtime*);void sub_003CDDE8_0x3cdde8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003CDE68_0x3cde68(uint8_t*,R5900Context*,PS2Runtime*);void sub_003CE0B8_0x3ce0b8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003CE0D8_0x3ce0d8(uint8_t*,R5900Context*,PS2Runtime*);void sub_003CE410_0x3ce410(uint8_t*,R5900Context*,PS2Runtime*);
void sub_0041605C_0x41605c(uint8_t*,R5900Context*,PS2Runtime*);void sub_00416210_0x416210(uint8_t*,R5900Context*,PS2Runtime*);
void sub_003C92F8_0x3c92f8(uint8_t*,R5900Context*,PS2Runtime*);void sub_003CE078_0x3ce078(uint8_t*,R5900Context*,PS2Runtime*);
constexpr uint32_t BANK=0x1000000,OUT=0x1400000,CTX=0x1700000,STATE=0x1701000,CELL=0x1702000,VOICE=0x1703000,STACK=0x1f00000,RET=0x12345678;
static uint8_t* M;
static void wr(uint32_t a,uint32_t v){std::memcpy(M+a,&v,4);}
using Fn=void(*)(uint8_t*,R5900Context*,PS2Runtime*);
static uint32_t call(Fn f,uint32_t pc,PS2Runtime& rt,std::initializer_list<uint32_t> args){
    R5900Context c{};c.pc=pc;unsigned r=4;for(uint32_t a:args){SET_GPR_U32(&c,r,a);++r;} /*the macro evaluates its register argument more than once*/SET_GPR_U32(&c,29,STACK);SET_GPR_U32(&c,31,RET);
    f(M,&c,&rt);if(c.pc!=RET)throw std::runtime_error("MicroTalk oracle: call did not return");return GPR_U32((&c),2);
}
// Stream-mode block list handed out by the 0x3C6DE0 stub (a0 stream, a1 channel, a2 -> count, a3 -> continue flag).
struct StreamBlock{uint32_t samples,offset;};
static std::vector<StreamBlock> g_blocks;static size_t g_next=0;
static void fetchBlock(uint8_t*,R5900Context* c,PS2Runtime*){
    uint32_t v=0;
    if(g_next<g_blocks.size()){const StreamBlock& b=g_blocks[g_next];wr(GPR_U32(c,6),b.samples);wr(GPR_U32(c,7),g_next?0xffffffffu:0u);v=BANK+b.offset;++g_next;}
    SET_GPR_U32(c,2,v);c->pc=GPR_U32(c,31);
}
static void releaseBlock(uint8_t*,R5900Context* c,PS2Runtime*){c->pc=GPR_U32(c,31);}
static int streamJobs(const std::vector<uint8_t>& file,const char* outPath,PS2Runtime& rt){
    std::ofstream out(outPath,std::ios::binary);unsigned jobs=0;
    uint32_t segOffset,segSize,version,channel,chunk,nblocks;
    while(std::cin>>segOffset>>segSize>>version>>channel>>chunk>>nblocks){
        if(uint64_t(segOffset)+segSize>file.size()||segSize>0x400000||chunk==0)return 4;
        g_blocks.clear();g_next=0;uint32_t total=0;
        for(uint32_t i=0;i<nblocks;++i){StreamBlock b;std::cin>>b.samples>>b.offset;if(b.offset>=segSize)return 4;g_blocks.push_back(b);total+=b.samples;}
        if(total>0x100000)return 4;
        std::memset(M+BANK,0,0x400000);std::memcpy(M+BANK,file.data()+segOffset,segSize);
        std::memset(M+CTX,0,0x1000);std::memset(M+VOICE,0,0x40);std::memset(M+OUT,0,total*4+16);
        call(sub_003CDDB8_0x3cddb8,0x3cddb8,rt,{CTX});
        wr(VOICE+0x24,1);wr(VOICE+0x30,version);wr(VOICE+0x34,CTX);M[VOICE+0x38]=uint8_t(channel);
        uint32_t done=0;
        while(done<total){
            const uint32_t n=std::min(chunk,total-done);
            const uint32_t got=call(sub_003C92F8_0x3c92f8,0x3c96f0,rt,{VOICE,n,0,OUT+done*4});
            if(got!=n){std::fprintf(stderr,"stream job %u: read %u of %u samples at %u\n",jobs,got,n,done);return 5;}
            done+=n;
        }
        if(g_next!=g_blocks.size()){std::fprintf(stderr,"stream job %u: %zu of %zu blocks used\n",jobs,g_next,g_blocks.size());return 5;}
        out.write(reinterpret_cast<const char*>(M+OUT),total*4);++jobs;
    }
    std::printf("%u MicroTalk stream jobs decoded by the original\n",jobs);
    return 0;
}
int main(int argc,char**argv){
    const bool stream=argc==5&&std::string(argv[4])=="--stream";
    if(argc!=4&&!stream)return 2;
    std::ifstream ef(argv[1],std::ios::binary);std::vector<uint8_t> elf((std::istreambuf_iterator<char>(ef)),{});if(elf.size()<0x100000)return 2;
    std::ifstream bf(argv[2],std::ios::binary);std::vector<uint8_t> bank((std::istreambuf_iterator<char>(bf)),{});if(!stream&&bank.size()>0x400000)return 3;
    std::vector<uint8_t> ram(32*1024*1024);M=ram.data();std::memcpy(M+0xff000,elf.data(),elf.size());if(!stream)std::memcpy(M+BANK,bank.data(),bank.size());
    PS2Runtime rt;
    rt.registerFunction(0x3cd1f8,sub_003CD1F8_0x3cd1f8);rt.registerFunction(0x3cd260,sub_003CD260_0x3cd260);rt.registerFunction(0x3cd2b0,sub_003CD2B0_0x3cd2b0);
    rt.registerFunction(0x3cd518,sub_003CD518_0x3cd518);rt.registerFunction(0x3cd590,sub_003CD590_0x3cd590);rt.registerFunction(0x3cd690,sub_003CD690_0x3cd690);
    rt.registerFunction(0x3cd6f0,sub_003CD6F0_0x3cd6f0);rt.registerFunction(0x3cd878,sub_003CD878_0x3cd878);rt.registerFunction(0x3ce410,sub_003CE410_0x3ce410);
    rt.registerFunction(0x41605c,sub_0041605C_0x41605c);
    std::fesetround(FE_TOWARDZERO);
    if(stream){
        rt.registerFunction(0x3cdde8,sub_003CDDE8_0x3cdde8);rt.registerFunction(0x3cde68,sub_003CDE68_0x3cde68);
        rt.registerFunction(0x3ce078,sub_003CE078_0x3ce078);rt.registerFunction(0x3ce0b8,sub_003CE0B8_0x3ce0b8);
        rt.registerFunction(0x3ce0d8,sub_003CE0D8_0x3ce0d8);rt.registerFunction(0x416210,sub_00416210_0x416210);
        rt.registerFunction(0x3c6de0,fetchBlock);rt.registerFunction(0x3c7010,releaseBlock);
        return streamJobs(bank,argv[3],rt);
    }
    std::ofstream out(argv[3],std::ios::binary);unsigned jobs=0;uint32_t offset,count;
    if(const char* dump=std::getenv("MICROTALK_DUMP_FRAMES")){ // debug: per-frame context dumps (fixed gains .. frame buffer)
        std::cin>>offset;const unsigned frames=unsigned(std::atoi(dump));std::memset(M+CTX,0,0x1000);
        call(sub_003CDDB8_0x3cddb8,0x3cddb8,rt,{CTX});call(sub_003CE0D8_0x3ce0d8,0x3ce0d8,rt,{CTX,1});
        wr(STATE,0);wr(STATE+4,0);wr(STATE+8,1);call(sub_003CE0B8_0x3ce0b8,0x3ce0b8,rt,{CTX,STATE});
        call(sub_003CDDE8_0x3cdde8,0x3cdde8,rt,{CTX,BANK+offset,432u*frames*4,432u*frames});
        for(unsigned f=0;f<frames;++f){wr(CELL,OUT);call(sub_003CDE68_0x3cde68,0x3cde68,rt,{CTX,CELL,432});out.write(reinterpret_cast<const char*>(M+CTX),0x684+432*4);}
        return 0;
    }
    while(std::cin>>offset>>count){
        if(offset>=bank.size()||count>0x100000)return 4;
        std::memset(M+CTX,0,0x1000);std::memset(M+OUT,0,count*4+16);
        call(sub_003CDDB8_0x3cddb8,0x3cddb8,rt,{CTX});
        call(sub_003CE0D8_0x3ce0d8,0x3ce0d8,rt,{CTX,1});
        wr(STATE,0);wr(STATE+4,0);wr(STATE+8,1);call(sub_003CE0B8_0x3ce0b8,0x3ce0b8,rt,{CTX,STATE});
        const uint32_t opened=call(sub_003CDDE8_0x3cdde8,0x3cdde8,rt,{CTX,BANK+offset,count*4,count});
        if(opened!=0){uint32_t d54;std::memcpy(&d54,M+CTX+0xd54,4);std::fprintf(stderr,"job %u: open returned %d (D54 %u)\n",jobs,int(opened),d54);return 6;}
        wr(CELL,OUT);const uint32_t got=call(sub_003CDE68_0x3cde68,0x3cde68,rt,{CTX,CELL,count});
        if(std::getenv("MICROTALK_DEBUG")){float fr[4],o[4];uint32_t d44,d4c,d54;std::memcpy(fr,M+CTX+0x684,16);std::memcpy(o,M+OUT,16);std::memcpy(&d44,M+CTX+0xd44,4);std::memcpy(&d4c,M+CTX+0xd4c,4);std::memcpy(&d54,M+CTX+0xd54,4);
            std::fprintf(stderr,"got %u frame %g %g out %g %g D44 %u D4C %x D54 %u\n",got,fr[0],fr[1],o[0],o[1],d44,d4c,d54);}
        if(got!=count){std::fprintf(stderr,"job %u: decoded %u of %u samples\n",jobs,got,count);return 5;}
        out.write(reinterpret_cast<const char*>(M+OUT),count*4);++jobs;
    }
    std::printf("%u MicroTalk jobs decoded by the original\n",jobs);
}
