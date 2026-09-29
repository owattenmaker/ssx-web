// Development-only private original instruction oracle; no guest runtime in the app.
// The LUN stage-script interpreter 0x2227D0 (entered through 0x2224B8, nested frames 0x222648,
// tables 0x224DA0/0x225248/0x225338/0x226768/0x224D00, value helpers 0x224DF0/0x225068/0x225B90/
// 0x226600..0x226628) against engine/stage_script_vm.hpp. The builtin table 0x441F38 is redirected
// to a recording stub returning seeded random values; allocations come from a zero-filled bump
// heap that never reuses memory. Compared per run: the builtin call trace (index, argument keys
// and values), the return slot, the top-level register file just before it is freed (words and
// types), and every table (refcount, buckets, head and chain nodes); table and function values
// are compared as table creation index / program handle instead of addresses.
#include "ps2_runtime_macros.h"
#include "ps2_runtime.h"
#include "../engine/stage_script_vm.hpp"
#include <bit>
#include <cfenv>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

void sub_002227D0_0x2227d0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002224A0_0x2224a0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002224A8_0x2224a8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002224B8_0x2224b8(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00222648_0x222648(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00224DA0_0x224da0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00224E50_0x224e50(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00224DF0_0x224df0(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00224F30_0x224f30(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00224D00_0x224d00(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00225068_0x225068(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00225248_0x225248(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00225338_0x225338(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00225B90_0x225b90(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00226600_0x226600(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00226610_0x226610(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00226618_0x226618(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00226620_0x226620(uint8_t*,R5900Context*,PS2Runtime*);
void sub_00226628_0x226628(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};

namespace {
using Value=ssx::OriginalScriptValue;
constexpr uint32_t STACK=0xF0000,FRAME=0xF8000,FUNCREF=0xF8010,GLOBALV=0xF8020,RET=0xF8030,KEYTMP=0xF8040,VALTMP=0xF8050,TBLTMP=0xF8060;
constexpr uint32_t DONE=0x12345678,PROG=0x1000000,HEAP=0x1400000,HEAP_END=0x1F00000,BUILTINS=0x441F38;
constexpr int kMaxDepth=4;
uint8_t* M=nullptr;
PS2Runtime* RT=nullptr;
uint32_t rd(uint32_t a){uint32_t v;std::memcpy(&v,M+a,4);return v;}
void wr(uint32_t a,uint32_t v){std::memcpy(M+a,&v,4);}
[[noreturn]] void fail(const std::string& s){std::printf("FAIL: %s\n",s.c_str());std::fflush(stdout);std::exit(1);}

struct Rng {
    uint64_t s;
    uint64_t next(){uint64_t z=(s+=0x9E3779B97F4A7C15ull);z=(z^(z>>30))*0xBF58476D1CE4E5B9ull;z=(z^(z>>27))*0x94D049BB133111EBull;return z^(z>>31);}
    uint32_t u32(){return uint32_t(next()>>32);}
    uint32_t below(uint32_t n){return n?uint32_t((next()>>33)%n):0;}
    bool chance(uint32_t percent){return below(100)<percent;}
};

uint32_t randomFloatBits(Rng& r){
    static const float specials[]={0.f,-0.f,1.f,-1.f,0.5f,-0.5f,2.f,3.f,-3.f,10.f,100.f,0.25f,1.5f,-2.5f,16777217.f,3e9f,-3e9f,1e-6f};
    if(r.chance(35))return std::bit_cast<uint32_t>(specials[r.below(sizeof specials/sizeof specials[0])]);
    if(r.chance(25))return std::bit_cast<uint32_t>(float(int32_t(r.below(41))-20));
    const uint32_t exponent=107+r.below(41);
    return (r.u32()&0x807FFFFFu)|(exponent<<23);
}
uint32_t randomIntBits(Rng& r){
    static const int32_t specials[]={0,1,-1,2,3,-2,7,31,100,INT32_MIN,INT32_MAX,65535,-100000};
    if(r.chance(40))return uint32_t(specials[r.below(sizeof specials/sizeof specials[0])]);
    if(r.chance(60))return uint32_t(int32_t(r.below(200))-100);
    return r.u32();
}
// Keys the synthetic programs and the globals share (small ints/symbols/floats so buckets collide).
const uint32_t kSymbolPool[]={0x0DFB527Eu,0x0A3FBCE3u,0x7u,0xEu,0x12345678u,0xCAFEBABEu,0x1Fu,0x3Eu};
Value poolKey(Rng& r){
    switch(r.below(3)){
        case 0:return Value::integer(int32_t(r.below(45))-2);
        case 1:return Value::symbol(kSymbolPool[r.below(8)]);
        default:{static const float f[]={0.f,-0.f,1.f,2.5f,-7.f,31.f};return Value::real(f[r.below(6)]);}
    }
}

// A value independent of addresses: tables by creation index, functions by record index.
struct Logical {Value v;int table=-1;};
struct Profile {uint32_t nil,integer,real,symbol,table,function,other;};
constexpr Profile kCourseProfile{14,32,32,14,3,3,2};
constexpr Profile kSyntheticProfile{14,28,28,12,7,7,4};
Logical randomLogical(Rng& r,const Profile& p,int tables,uint32_t functions){
    const uint32_t total=p.nil+p.integer+p.real+p.symbol+p.table+p.function+p.other;
    uint32_t pick=r.below(total);
    Logical l;l.v.word1=r.chance(50)?r.u32():0;
    auto take=[&](uint32_t w){if(pick<w)return true;pick-=w;return false;};
    if(take(p.nil)){l.v.type=0;l.v.word0=r.chance(70)?0:r.below(1000);}
    else if(take(p.integer)){l.v.type=1;l.v.word0=randomIntBits(r);}
    else if(take(p.real)){l.v.type=2;l.v.word0=randomFloatBits(r);}
    else if(take(p.symbol)){l.v.type=3;l.v.word0=r.chance(50)?kSymbolPool[r.below(8)]:r.u32()&0x0FFFFFFFu;}
    else if(take(p.table)&&tables>0){l.v.type=4;l.table=int(r.below(uint32_t(tables)));l.v.word0=0;}
    else if(pick<p.function&&functions>0){l.v.type=5;l.v.word0=0;l.v.word1=r.below(functions);}
    else {l.v.type=r.chance(50)?6:int32_t(0x7FFFFFFF);l.v.word0=randomIntBits(r);}
    return l;
}

struct ArgRecord {int32_t key;Value v;bool operator==(const ArgRecord&) const=default;};
struct CallRecord {int index,count;std::vector<ArgRecord> args;bool operator==(const CallRecord&) const=default;};
struct NodeDump {Value key,value;bool operator==(const NodeDump&) const=default;};
struct TableDump {int32_t refcount=0;uint32_t buckets=0,modulus=0;std::vector<std::vector<NodeDump>> chains;bool operator==(const TableDump&) const=default;};
struct Outcome {
    std::vector<CallRecord> trace;
    bool aborted=false,depthAbort=false;
    std::string error;
    Value ret;
    std::vector<Value> regs;
    std::vector<TableDump> tables;
};

// ---------------- guest side ----------------
uint32_t heapTop=HEAP;
std::vector<uint32_t> tableAddrs;
int depth=0;
bool snapshotTaken=false;
uint32_t snapshotCount=0;
std::vector<Value> snapshotRegs;
bool addressKeyed=false;
std::vector<CallRecord>* guestTrace=nullptr;
uint64_t runSeed=0;
uint32_t callNumber=0,runFunctions=0;
const Profile* runProfile=&kCourseProfile;
struct DepthAbort {};

uint32_t allocate(uint32_t size){
    const uint32_t a=(heapTop+15)&~15u;heapTop=a+size;
    if(heapTop>HEAP_END)fail("guest heap exhausted");
    if(size==0x14)tableAddrs.push_back(a);
    return a;
}
void resetHeap(){std::memset(M+HEAP,0,heapTop-HEAP);heapTop=HEAP;tableAddrs.clear();}
void ret(R5900Context* c){c->pc=GPR_U32(c,31);}
R5900Context context(uint32_t pc){
    R5900Context c{};c.pc=pc;c.vu0_vf[0]=_mm_set_ps(1,0,0,0);
    SET_GPR_U32(&c,28,0x4a30f0);SET_GPR_U32(&c,29,STACK);SET_GPR_U32(&c,31,DONE);return c;
}
uint32_t canonicalWord(uint32_t w){
    if(w==PROG)return 1;
    for(size_t i=0;i<tableAddrs.size();i++)if(tableAddrs[i]==w)return uint32_t(i+1);
    return w;
}
Value readGuest(uint32_t a,bool tombstone=false){
    Value v{rd(a),rd(a+4),int32_t(rd(a+8))};
    if(v.type==4||v.type==5||(tombstone&&v.type==0))v.word0=canonicalWord(v.word0);
    return v;
}
void writeGuest(uint32_t a,const Logical& l){
    uint32_t w0=l.v.word0;
    if(l.v.type==4)w0=tableAddrs.at(size_t(l.table));
    if(l.v.type==5)w0=PROG;
    wr(a,w0);wr(a+4,l.v.word1);wr(a+8,uint32_t(l.v.type));
}
Logical builtinResult(int tables){
    Rng r{runSeed^(0xB5AD4ECEDA1CE2A9ull*(callNumber+1))};
    Logical l=randomLogical(r,*runProfile,tables,runFunctions);
    if(l.table>=0&&int32_t(rd(tableAddrs.at(size_t(l.table))+8))<=0)l.table=0;
    return l;
}
void builtinStub(uint8_t*,R5900Context* c,PS2Runtime*){
    const uint32_t entry=GPR_U32(c,2);
    const int index=int((entry-BUILTINS)/4);
    if(entry<BUILTINS||index>=int(ssx::kOriginalScriptBuiltinCount)||rd(entry)!=c->pc)fail("builtin stub entry");
    const uint32_t result=GPR_U32(c,4),args=GPR_U32(c,6);
    const int count=int(GPR_U32(c,5));
    CallRecord rec{index,count,{}};
    for(int i=0;i<count;i++)rec.args.push_back({int32_t(rd(args+16*uint32_t(i))),readGuest(args+16*uint32_t(i)+4)});
    guestTrace->push_back(rec);
    const Logical l=builtinResult(int(tableAddrs.size()));
    ++callNumber;
    writeGuest(result,l);
    if(l.v.type==4){const uint32_t t=tableAddrs.at(size_t(l.table));wr(t+8,rd(t+8)+1);}
    ret(c);
}
void interpreterWrapper(uint8_t* m,R5900Context* c,PS2Runtime* rt){
    if(++depth>kMaxDepth){--depth;throw DepthAbort{};}
    try{sub_002227D0_0x2227d0(m,c,rt);}catch(...){--depth;throw;}
    --depth;
}
void releaseWrapper(uint8_t* m,R5900Context* c,PS2Runtime* rt){
    if(GPR_U32(c,5)==0&&depth==1&&!snapshotTaken){
        snapshotTaken=true;
        const uint32_t base=GPR_U32(c,4)-(snapshotCount-1)*12;
        snapshotRegs.clear();
        for(uint32_t i=0;i<snapshotCount;i++)snapshotRegs.push_back(readGuest(base+12*i));
    }
    sub_00225B90_0x225b90(m,c,rt);
}
uint64_t loopSteps=0;
std::string currentCase;
const std::vector<uint32_t>* currentWords=nullptr;
void loopAddWrapper(uint8_t* m,R5900Context* c,PS2Runtime* rt){
    if(++loopSteps>2000000){
        std::string w;for(uint32_t x:*currentWords){char b[16];std::snprintf(b,sizeof b," %08x",x);w+=b;}
        fail("runaway for-loop in "+currentCase+"\n words"+w);
    }
    sub_00226628_0x226628(m,c,rt);
}
void keyCheck(R5900Context* c){const int32_t t=int32_t(rd(GPR_U32(c,5)+8));if(t==4||t==5)addressKeyed=true;}
void getWrapper(uint8_t* m,R5900Context* c,PS2Runtime* rt){keyCheck(c);sub_00225248_0x225248(m,c,rt);}
void setWrapper(uint8_t* m,R5900Context* c,PS2Runtime* rt){keyCheck(c);sub_00225338_0x225338(m,c,rt);}

void registerGuest(PS2Runtime& rt){
    rt.registerFunction(0x2227d0,interpreterWrapper);
    rt.registerFunction(0x2224a0,sub_002224A0_0x2224a0);rt.registerFunction(0x2224a8,sub_002224A8_0x2224a8);
    rt.registerFunction(0x2224b8,sub_002224B8_0x2224b8);rt.registerFunction(0x222648,sub_00222648_0x222648);
    rt.registerFunction(0x224da0,sub_00224DA0_0x224da0);rt.registerFunction(0x224e50,sub_00224E50_0x224e50);
    rt.registerFunction(0x224df0,sub_00224DF0_0x224df0);rt.registerFunction(0x224f30,sub_00224F30_0x224f30);
    rt.registerFunction(0x224d00,sub_00224D00_0x224d00);rt.registerFunction(0x225068,sub_00225068_0x225068);
    rt.registerFunction(0x225248,getWrapper);rt.registerFunction(0x225338,setWrapper);
    rt.registerFunction(0x225b90,releaseWrapper);
    rt.registerFunction(0x226600,sub_00226600_0x226600);rt.registerFunction(0x226610,sub_00226610_0x226610);
    rt.registerFunction(0x226618,sub_00226618_0x226618);rt.registerFunction(0x226620,sub_00226620_0x226620);
    rt.registerFunction(0x226628,loopAddWrapper);rt.registerFunction(0x226768,sub_00226628_0x226628); /* node destructor, vtable 0x479890+0xC */
    auto alloc=[](uint8_t*,R5900Context* c,PS2Runtime*){SET_GPR_U32(c,2,allocate(GPR_U32(c,4)));ret(c);};
    auto release=[](uint8_t*,R5900Context* c,PS2Runtime*){ret(c);};
    rt.registerFunction(0x317d70,alloc);rt.registerFunction(0x317e30,alloc);
    rt.registerFunction(0x317e98,release);rt.registerFunction(0x317e50,release);
    for(uint32_t i=0;i<ssx::kOriginalScriptBuiltinCount;i++)rt.registerFunction(rd(BUILTINS+4*i),builtinStub);
}
void guestCall(uint32_t pc,void(*fn)(uint8_t*,R5900Context*,PS2Runtime*),uint32_t a0,uint32_t a1,uint32_t a2=0,uint32_t a3=0){
    auto c=context(pc);SET_GPR_U32(&c,4,a0);SET_GPR_U32(&c,5,a1);SET_GPR_U32(&c,6,a2);SET_GPR_U32(&c,7,a3);
    fn(M,&c,RT);
    if(c.pc!=DONE)fail("guest helper continuation");
}
uint32_t guestNewTable(uint32_t buckets){
    const uint32_t handle=allocate(4);
    guestCall(0x224da0,sub_00224DA0_0x224da0,handle,buckets);
    return rd(handle);
}

// Setup shared by both sides: extra tables (created after the globals) and global entries.
struct Setup {
    std::vector<uint32_t> extraTables; /* bucket counts */
    std::vector<std::pair<Logical,Logical>> globals;
    Logical returnSlot;
    uint32_t function=0;
};

TableDump dumpGuestTable(uint32_t t){
    TableDump d;d.refcount=int32_t(rd(t+8));d.modulus=rd(t+0xC);
    const uint32_t nodes=rd(t+0x10);d.buckets=rd(nodes-0x10);
    if(d.refcount<=0)return TableDump{d.refcount,0,0,{}};
    for(uint32_t i=0;i<d.buckets;i++){
        std::vector<NodeDump> chain;
        for(uint32_t n=nodes+32*i,guard=0;n;n=rd(n+0x18)){
            if(++guard>100000)fail("guest chain cycle");
            chain.push_back({readGuest(n,true),readGuest(n+0xC,true)});
        }
        d.chains.push_back(chain);
    }
    return d;
}

Outcome runGuest(const std::vector<uint32_t>& words,uint32_t registerCount,const Setup& s,uint64_t seed,const Profile& profile,uint32_t functions){
    Outcome o;
    resetHeap();
    std::memcpy(M+PROG,words.data(),words.size()*4);
    depth=0;snapshotTaken=false;snapshotCount=registerCount;snapshotRegs.clear();addressKeyed=false;
    guestTrace=&o.trace;runSeed=seed;callNumber=0;runFunctions=functions;runProfile=&profile;
    const uint32_t globals=guestNewTable(ssx::kOriginalScriptHostTableBuckets);
    for(uint32_t b:s.extraTables)guestNewTable(b);
    for(auto& [k,v]:s.globals){
        wr(TBLTMP,globals);wr(TBLTMP+4,0);wr(TBLTMP+8,4);
        writeGuest(KEYTMP,k);writeGuest(VALTMP,v);
        guestCall(0x225338,sub_00225338_0x225338,TBLTMP,KEYTMP,VALTMP);
    }
    wr(GLOBALV,globals);wr(GLOBALV+4,0);wr(GLOBALV+8,4);
    wr(FUNCREF,PROG);wr(FUNCREF+4,s.function);
    writeGuest(RET,s.returnSlot);
    for(uint32_t i=0;i<16;i+=4)wr(FRAME+i,0);
    auto c=context(0x2224b8);
    SET_GPR_U32(&c,4,FRAME);SET_GPR_U32(&c,5,FUNCREF);SET_GPR_U32(&c,6,GLOBALV);SET_GPR_U32(&c,7,RET);
    try{
        sub_002224B8_0x2224b8(M,&c,RT);
        if(c.pc!=DONE)fail("0x2224B8 continuation");
    }catch(DepthAbort&){o.aborted=o.depthAbort=true;depth=0;return o;}
    if(addressKeyed)fail("table key of type 4/5 (address-dependent hashing)");
    o.ret=readGuest(RET);
    o.regs=snapshotRegs;
    for(uint32_t t:tableAddrs)o.tables.push_back(dumpGuestTable(t));
    return o;
}

// ---------------- port side ----------------
uint64_t executed[256]={};
std::string executedSummary(){
    std::string ops;uint64_t wild=0;
    for(int op=0;op<0x2B;op++){char b[32];std::snprintf(b,sizeof b," %02X:%llu",op,(unsigned long long)executed[op]);ops+=b;}
    for(int op=0x2B;op<256;op++)wild+=executed[op];
    return ops+" 2B..FF:"+std::to_string(wild);
}
Value portValue(const Logical& l){
    Value v=l.v;
    if(v.type==4)v.word0=uint32_t(l.table+1);
    if(v.type==5)v.word0=1;
    return v;
}
TableDump dumpPortTable(const ssx::OriginalScriptTable& t){
    TableDump d;d.refcount=t.refcount;d.buckets=t.buckets;d.modulus=t.modulus;
    if(t.destroyed||d.refcount<=0)return TableDump{d.refcount,0,0,{}};
    for(uint32_t i=0;i<t.buckets;i++){
        std::vector<NodeDump> chain;
        for(int32_t n=int32_t(i);n>=0;n=t.nodes[size_t(n)].next)chain.push_back({t.nodes[size_t(n)].key,t.nodes[size_t(n)].value});
        d.chains.push_back(chain);
    }
    return d;
}
Outcome runPort(const ssx::OriginalScriptProgram& program,const Setup& s,uint64_t seed,const Profile& profile,uint32_t functions){
    Outcome o;
    ssx::OriginalScriptVM vm;vm.maxCallDepth=kMaxDepth;
    vm.onInstruction=[](uint32_t,int32_t,uint32_t w){++executed[w&255];};
    const uint32_t handle=vm.addProgram(program);
    const uint32_t globals=vm.newTable(ssx::kOriginalScriptHostTableBuckets);
    for(uint32_t b:s.extraTables)vm.newTable(b);
    for(auto& [k,v]:s.globals)vm.set(vm.tableValue(globals),portValue(k),portValue(v));
    uint32_t calls=0;
    auto builtin=[&](int index,const ssx::OriginalScriptArg* args,int count)->Value{
        CallRecord rec{index,count,{}};
        for(int i=0;i<count;i++)rec.args.push_back({args[i].key,args[i].value});
        o.trace.push_back(rec);
        Rng r{seed^(0xB5AD4ECEDA1CE2A9ull*(calls+1))};++calls;
        Logical l=randomLogical(r,profile,int(vm.tableCount()),functions);
        if(l.table>=0&&vm.table(uint32_t(l.table+1)).destroyed)l.table=0;
        Value v=portValue(l);
        vm.retain(v);
        return v;
    };
    try{
        o.ret=vm.run(handle,s.function,vm.tableValue(globals),builtin,&o.regs,portValue(s.returnSlot));
    }catch(ssx::OriginalScriptError& e){
        o.aborted=true;o.error=e.what();o.depthAbort=o.error=="LUN call depth limit";return o;
    }
    for(uint32_t t=1;t<=vm.tableCount();t++)o.tables.push_back(dumpPortTable(vm.table(t)));
    return o;
}

std::string show(const Value& v){char b[64];std::snprintf(b,sizeof b,"{%08x,%08x,%d}",v.word0,v.word1,v.type);return b;}
std::string showCall(const CallRecord& c){
    std::string s="builtin "+std::to_string(c.index)+"("+std::to_string(c.count)+"):";
    for(auto& a:c.args)s+=" "+std::to_string(a.key)+"="+show(a.v);
    return s;
}
void compare(const Outcome& g,const Outcome& p,const std::string& what){
    if(g.aborted!=p.aborted||g.depthAbort!=p.depthAbort)fail(what+": abort guest "+std::to_string(g.aborted)+" port "+std::to_string(p.aborted)+" "+p.error);
    const size_t n=std::min(g.trace.size(),p.trace.size());
    for(size_t i=0;i<n;i++)if(!(g.trace[i]==p.trace[i]))fail(what+": call "+std::to_string(i)+"\n guest "+showCall(g.trace[i])+"\n port  "+showCall(p.trace[i]));
    if(g.trace.size()!=p.trace.size())fail(what+": call count guest "+std::to_string(g.trace.size())+" port "+std::to_string(p.trace.size()));
    if(g.aborted)return;
    if(!(g.ret==p.ret))fail(what+": return guest "+show(g.ret)+" port "+show(p.ret));
    if(g.regs.size()!=p.regs.size())fail(what+": register count");
    for(size_t i=0;i<g.regs.size();i++)if(!(g.regs[i]==p.regs[i]))fail(what+": register "+std::to_string(i)+" guest "+show(g.regs[i])+" port "+show(p.regs[i]));
    if(g.tables.size()!=p.tables.size())fail(what+": table count guest "+std::to_string(g.tables.size())+" port "+std::to_string(p.tables.size()));
    for(size_t t=0;t<g.tables.size();t++){
        const TableDump &a=g.tables[t],&b=p.tables[t];
        if(a.refcount!=b.refcount)fail(what+": table "+std::to_string(t)+" refcount guest "+std::to_string(a.refcount)+" port "+std::to_string(b.refcount));
        if(a==b)continue;
        for(size_t i=0;i<std::min(a.chains.size(),b.chains.size());i++)if(!(a.chains[i]==b.chains[i])){
            std::string s=what+": table "+std::to_string(t)+" bucket "+std::to_string(i)+"\n guest";
            for(auto& x:a.chains[i])s+=" "+show(x.key)+"="+show(x.value);
            s+="\n port ";for(auto& x:b.chains[i])s+=" "+show(x.key)+"="+show(x.value);
            fail(s);
        }
        fail(what+": table "+std::to_string(t)+" shape");
    }
}

struct Stats {uint64_t runs=0,calls=0,aborted=0,tableNodes=0,functionsRun=0;};
void check(const std::vector<uint32_t>& words,const Setup& s,uint64_t seed,const Profile& profile,Stats& st,const std::string& what){
    const ssx::OriginalScriptProgram program=ssx::originalScriptProgramFromWords(words);
    const uint32_t functions=uint32_t(program.functionCount());
    currentCase=what;loopSteps=0;currentWords=&words;
    const Outcome g=runGuest(words,uint32_t(program.function(0).registerCount),s,seed,profile,functions);
    const Outcome p=runPort(program,s,seed,profile,functions);
    compare(g,p,what);
    ++st.runs;st.calls+=g.trace.size();st.aborted+=g.aborted;
    for(auto& t:g.tables)for(auto& c:t.chains)st.tableNodes+=c.size()>1?c.size()-1:0;
}

// ---------------- synthetic programs ----------------
// Registers: 0 globals, 1..6 general, then reserved: table, key, function, divisor, for-loop
// i/limit/step, while-loop counter/one/zero/test. Control flow is forward except the two
// bounded loop forms; argument pushes are consumed by their own call group inside loops.
enum Reg:uint32_t {G0=1,GN=6,TR=7,KR,FR,DR,LI,LL,LS,LC,L1,LZ,LT,REGS};
struct Unit {std::vector<uint32_t> w;int jump=0;int target=-1;bool inLoop=false;};
uint32_t ins(uint32_t op,uint32_t b1=0,uint32_t b2=0,uint32_t b3=0){return op|(b1<<8)|(b2<<16)|(b3<<24);}
struct Synth {std::vector<uint32_t> words;uint32_t functions=0;int pushes=0;};
uint32_t anyReg(Rng& r){return r.below(REGS);}
uint32_t general(Rng& r){return G0+r.below(GN-G0+1);}
void loadImmediate(Rng& r,Unit& u,uint32_t reg){
    switch(r.below(5)){
        case 0:u.w.push_back(ins(0x14,reg));u.w.push_back(r.chance(50)?kSymbolPool[r.below(8)]:r.u32());break;
        case 1:u.w.push_back(ins(0x15,reg));u.w.push_back(randomIntBits(r));break;
        case 2:u.w.push_back(ins(0x16,reg));u.w.push_back(randomIntBits(r));break;
        default:u.w.push_back(ins(0x17,reg));u.w.push_back(randomFloatBits(r));break;
    }
}
void loadKey(Rng& r,Unit& u){
    if(r.chance(8)){u.w.push_back(ins(0x1E,KR));return;}
    const Value k=poolKey(r);
    const uint32_t op=k.type==3?0x14:k.type==2?0x17:(r.chance(50)?0x15:0x16);
    u.w.push_back(ins(op,KR));u.w.push_back(k.word0);
}
void pushGroup(Rng& r,Unit& u,int& pushes,bool balanced){
    const uint32_t k=r.below(5);
    for(uint32_t i=0;i<k;i++){
        const uint32_t key=r.chance(80)?r.below(8):r.below(256);
        switch(r.below(6)){
            case 0:u.w.push_back(ins(0x20,key,anyReg(r)));break;
            case 1:u.w.push_back(ins(0x25,key));u.w.push_back(randomIntBits(r));break;
            case 2:u.w.push_back(ins(0x26,key));u.w.push_back(randomFloatBits(r));break;
            case 3:u.w.push_back(ins(0x27,key));u.w.push_back((r.below(0xFFFF)<<8)|r.below(20));break;
            case 4:u.w.push_back(ins(0x28,key,r.below(256)));break;
            default:u.w.push_back(ins(0x29,key,r.below(256)));break;
        }
        ++pushes;
    }
    const uint32_t c=balanced?k:k-r.below(k+1);
    u.w.push_back(ins(0x21,general(r),r.below(ssx::kOriginalScriptBuiltinCount),c));
}
Unit randomUnit(Rng& r,int& pushes,bool inLoop,uint32_t functions){
    Unit u;u.inLoop=inLoop;
    const uint32_t kind=r.below(inLoop?20:24);
    switch(kind){
        case 0:case 1:case 2:u.w.push_back(ins(3+r.below(9),general(r),anyReg(r),anyReg(r)));break; /* 3..0xB */
        case 3:u.w.push_back(ins(0x0C+r.below(3),general(r),anyReg(r),anyReg(r)));break;
        case 4:{
            const bool mod=r.chance(30);
            if(mod||r.chance(50)){u.w.push_back(ins(0x16,DR));int32_t v=int32_t(randomIntBits(r));if(!v)v=-1;u.w.push_back(uint32_t(v));}
            else{u.w.push_back(ins(0x17,DR));float f;do f=std::bit_cast<float>(randomFloatBits(r));while(!(std::fabs(f)>=1.f));u.w.push_back(std::bit_cast<uint32_t>(f));}
            u.w.push_back(ins(mod?0x11:0x0F,general(r),anyReg(r),DR));
            break;
        }
        case 5:u.w.push_back(ins(r.chance(50)?0x0B:0x13,general(r),r.chance(20)?G0+r.below(2):anyReg(r)));break;
        case 6:loadImmediate(r,u,general(r));break;
        case 7:u.w.push_back(ins(r.below(3)==0?0x10:r.chance(50)?0x1E:0x22+r.below(2),general(r),anyReg(r),anyReg(r)));break;
        case 8:case 9:{
            loadKey(r,u);
            const uint32_t table=r.chance(50)?0:TR;
            switch(r.below(3)){
                case 0:u.w.push_back(ins(0x12,table,KR,anyReg(r)));break;
                case 1:u.w.push_back(ins(0x18,general(r),table,KR));break;
                default:u.w.push_back(ins(0x1C,table,KR,anyReg(r)));break;
            }
            break;
        }
        case 10:u.w.push_back(ins(0x19,general(r)));break;
        case 11:case 12:pushGroup(r,u,pushes,inLoop);break;
        case 13:u.jump=1;u.w.push_back(ins(0x00));break;
        case 14:u.jump=1;u.w.push_back(ins(1+r.below(2),anyReg(r)));break;
        case 15:u.w.push_back(r.chance(50)?ins(0x1A,r.below(256),r.below(256),r.below(256)):ins(0x2B+r.below(0xD5),r.below(256),r.below(256),r.below(256)));break;
        case 16:
            if(r.chance(35))u.w.push_back(r.chance(50)?ins(0x1F,anyReg(r)):ins(0x2A,0xFF));
            else u.w.push_back(ins(0x03+r.below(6),general(r),anyReg(r),anyReg(r)));
            break;
        case 17:u.jump=2;u.w.push_back(ins(0x24,LI,LL,LS));u.w.push_back(0);break;
        case 18:case 19:u.w.push_back(ins(0x0C+r.below(3),general(r),anyReg(r),anyReg(r)));break;
        /* outside loops only */
        case 20:pushGroup(r,u,pushes,false);break;
        case 21:case 22:u.w.push_back(ins(0x20,r.below(8),anyReg(r)));++pushes;break;
        default:{
            if(r.chance(80)){u.w.push_back(ins(0x1D,FR));u.w.push_back(r.below(functions));}
            else u.w.push_back(ins(0x1E,FR));
            u.w.push_back(ins(0x1B,general(r),FR,anyReg(r)));
            break;
        }
    }
    return u;
}
Synth makeProgram(Rng& r){
    Synth s;s.functions=1+r.below(4);
    std::vector<Unit> units;std::vector<size_t> starts;
    for(uint32_t f=0;f<s.functions;f++){
        starts.push_back(units.size());
        Unit prologue;prologue.w.push_back(r.chance(60)?ins(0x19,TR):ins(0x13,TR,0));
        for(uint32_t reg:{LI,LS,L1}){prologue.w.push_back(ins(0x16,reg));prologue.w.push_back(1);}
        for(uint32_t reg:{LL,LC,LZ}){prologue.w.push_back(ins(0x16,reg));prologue.w.push_back(0);}
        units.push_back(prologue);
        const uint32_t n=4+r.below(24);
        for(uint32_t i=0;i<n;i++){
            if(r.chance(10)){
                /* bounded loop: init, body (forward jumps may leave it), back edge */
                const bool forLoop=r.chance(60);
                Unit init;
                if(forLoop){
                    /* nil <= nil holds and same-type floats compare as signed ints, so float loops
                       stay non-negative (a negative float counter can walk away from its limit) */
                    const bool fi=r.chance(40),fl=r.chance(40),fs=r.chance(40);
                    const int32_t i0=fi||fl?int32_t(r.below(21)):int32_t(r.below(21))-10,l0=fi||fl?int32_t(1+r.below(20)):int32_t(r.below(21))-10;
                    int32_t st=int32_t(r.below(3))+1;if(r.chance(50))st=-st;
                    init.w.push_back(ins(fi?0x17:0x16,LI));init.w.push_back(fi?std::bit_cast<uint32_t>(float(i0)+(r.chance(50)?0.5f:0.f)):uint32_t(i0));
                    init.w.push_back(ins(fl?0x17:0x16,LL));init.w.push_back(fl?std::bit_cast<uint32_t>(float(l0)-0.25f):uint32_t(l0));
                    init.w.push_back(ins(fs?0x17:0x16,LS));init.w.push_back(fs?std::bit_cast<uint32_t>(float(st)*(fi?0.5f:1.f)):uint32_t(st));
                }else{
                    init.w.push_back(ins(0x16,LC));init.w.push_back(1+r.below(4));
                    init.w.push_back(ins(0x16,L1));init.w.push_back(1);
                    init.w.push_back(ins(0x16,LZ));init.w.push_back(0);
                }
                units.push_back(init);
                const size_t body=units.size();
                const uint32_t m=1+r.below(5);
                for(uint32_t j=0;j<m;j++)units.push_back(randomUnit(r,s.pushes,true,s.functions));
                Unit back;back.jump=3;back.target=int(body);
                if(forLoop){back.w.push_back(ins(0x24,LI,LL,LS));back.w.push_back(0);back.jump=4;}
                else{back.w.push_back(ins(0x0D,LC,LC,L1));back.w.push_back(ins(0x06,LT,LC,LZ));back.w.push_back(ins(0x01,LT));}
                units.push_back(back);
                continue;
            }
            units.push_back(randomUnit(r,s.pushes,false,s.functions));
        }
        Unit end;end.w.push_back(r.chance(50)?ins(0x2A,0xFF):ins(0x1F,anyReg(r)));units.push_back(end);
    }
    std::vector<uint32_t> offsets;uint32_t at=0;
    for(auto& u:units){offsets.push_back(at);at+=uint32_t(u.w.size());}
    offsets.push_back(at);
    const uint32_t codeWords=at;
    for(size_t i=0;i<units.size();i++){
        Unit& u=units[i];
        if(u.jump==1||u.jump==2){
            const size_t t=i+1+r.below(uint32_t(units.size()-i));
            const uint32_t target=offsets[t];
            if(u.jump==1)u.w[0]|=target<<16;else u.w[1]=target;
        }else if(u.jump==3){u.w.back()|=offsets[size_t(u.target)]<<16;}
        else if(u.jump==4){u.w[1]=offsets[size_t(u.target)];}
    }
    const uint32_t codeEnd=16+4*codeWords;
    s.words={ssx::kOriginalScriptMagic,codeEnd,codeEnd+16*s.functions,codeEnd+16*s.functions};
    for(auto& u:units)s.words.insert(s.words.end(),u.w.begin(),u.w.end());
    for(uint32_t f=0;f<s.functions;f++){
        const uint32_t capacity=uint32_t(s.pushes)+r.below(3);
        const uint32_t regs=f==0?uint32_t(REGS):1+r.below(REGS);
        s.words.push_back(uint32_t(-int32_t(codeEnd+16*f)));
        s.words.push_back(offsets[starts[f]]);
        s.words.push_back(regs);
        s.words.push_back(capacity);
    }
    return s;
}

Setup randomSetup(Rng& r,const std::vector<uint32_t>& symbols,uint32_t functions,const Profile& profile){
    Setup s;
    const uint32_t extra=r.below(3);
    for(uint32_t i=0;i<extra;i++)s.extraTables.push_back(r.chance(50)?8:32);
    const int tables=int(1+extra);
    const uint32_t entries=r.below(14);
    for(uint32_t i=0;i<entries;i++){
        Logical k;
        if(!symbols.empty()&&r.chance(60))k.v=Value::symbol(symbols[r.below(uint32_t(symbols.size()))]);
        else k.v=poolKey(r);
        s.globals.push_back({k,randomLogical(r,profile,tables,functions)});
    }
    s.returnSlot=randomLogical(r,profile,0,0);
    if(s.returnSlot.v.type>=4)s.returnSlot.v=Value::integer(int32_t(r.below(10)));
    return s;
}
}

int main(int argc,char** argv){
    if(argc!=4){std::puts("usage: reference SLUS_207.72 programs.bin synthetic-count");return 2;}
    std::ifstream elf(argv[1],std::ios::binary);std::vector<uint8_t> image((std::istreambuf_iterator<char>(elf)),{});
    if(image.size()<0x100000)return 2;
    std::vector<uint8_t> ram(32*1024*1024);std::memcpy(ram.data()+0xFF000,image.data(),image.size());M=ram.data();
    PS2Runtime rt;RT=&rt;registerGuest(rt);
    std::fesetround(FE_TOWARDZERO);

    // Stage programs: u32 count, then {u32 location, u32 track, u32 index, u32 words, words...}.
    std::ifstream in(argv[2],std::ios::binary);std::vector<uint8_t> blob((std::istreambuf_iterator<char>(in)),{});
    size_t at=0;auto next=[&]{uint32_t v;if(at+4>blob.size())fail("programs.bin truncated");std::memcpy(&v,blob.data()+at,4);at+=4;return v;};
    const uint32_t programs=next();
    static const char* names[]={"ARA1","BRA2","BHP1"};
    Stats course[3];uint32_t perCourse[3]={};
    constexpr uint32_t kRuns=160;
    for(uint32_t pi=0;pi<programs;pi++){
        const uint32_t location=next(),track=next(),index=next(),count=next();
        std::vector<uint32_t> words(count);for(auto& w:words)w=next();
        const ssx::OriginalScriptProgram program=ssx::originalScriptProgramFromWords(words);
        std::vector<uint32_t> symbols;
        for(size_t i=0;i<program.code.size();i++){
            const uint32_t op=program.code[i]&255;
            if(op==0x14&&i+1<program.code.size())symbols.push_back(program.code[i+1]);
            if(op==0x14||op==0x15||op==0x16||op==0x17||op==0x1d||op==0x24||op==0x25||op==0x26||op==0x27)++i;
        }
        const uint32_t functions=uint32_t(program.functionCount());
        ++perCourse[location];
        for(uint32_t f=0;f<functions;f++){
            ++course[location].functionsRun;
            for(uint32_t run=0;run<kRuns;run++){
                const uint64_t seed=(uint64_t(location)<<56)^(uint64_t(track)<<48)^(uint64_t(index)<<32)^(uint64_t(f)<<24)^run;
                Rng r{seed*0x2545F4914F6CDD1Dull+1};
                Setup s=randomSetup(r,symbols,functions,kCourseProfile);s.function=f;
                check(words,s,seed,kCourseProfile,course[location],std::string(names[location])+" track "+std::to_string(track)+" program "+std::to_string(index)+" function "+std::to_string(f)+" run "+std::to_string(run));
            }
        }
    }
    uint64_t totalRuns=0,totalCalls=0;
    for(int l=0;l<3;l++){
        std::printf("%s: %u stage programs (%llu function entries) x %u randomized runs = %llu runs, %llu builtin calls, %llu chained table nodes: all match\n",
            names[l],perCourse[l],(unsigned long long)course[l].functionsRun,kRuns,(unsigned long long)course[l].runs,(unsigned long long)course[l].calls,(unsigned long long)course[l].tableNodes);
        totalRuns+=course[l].runs;totalCalls+=course[l].calls;
    }
    std::printf("stage programs: %llu runs, %llu builtin calls match exactly\n",(unsigned long long)totalRuns,(unsigned long long)totalCalls);
    std::printf("stage programs executed opcodes:%s\n",executedSummary().c_str());
    std::fill(std::begin(executed),std::end(executed),0);

    Stats synth;
    const uint32_t synthetic=uint32_t(std::stoul(argv[3]));
    for(uint32_t k=0;k<synthetic;k++){
        Rng r{0x5CA1AB1Eull*(k+1)};
        const Synth s=makeProgram(r);
        for(uint32_t run=0;run<4;run++){
            Setup setup=randomSetup(r,{},s.functions,kSyntheticProfile);
            setup.function=r.below(s.functions);
            check(s.words,setup,(uint64_t(k)<<8)^run^0xABCDEF0000ull,kSyntheticProfile,synth,"synthetic "+std::to_string(k)+" run "+std::to_string(run));
        }
    }
    std::printf("synthetic: %u programs x 4 runs = %llu runs (%llu depth-limit aborts), %llu builtin calls, %llu chained table nodes: all match\n",
        synthetic,(unsigned long long)synth.runs,(unsigned long long)synth.aborted,(unsigned long long)synth.calls,(unsigned long long)synth.tableNodes);
    std::printf("synthetic executed opcodes:%s\n",executedSummary().c_str());
    return 0;
}
