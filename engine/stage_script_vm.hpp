#pragma once
// Original SSX 3 course stage-script VM ("Luno" LUN programs, docs/stage-scripts.md).
// Instruction-level port of the interpreter 0x2227D0 with its frame setup 0x2224B8 (host entry,
// reg1 = nil) / 0x222648 (op 0x1B call, reg1 = argument), the value helpers 0x224DF0 (release),
// 0x225068 (table copy), 0x225B90 (release + nil type), 0x226628 (for-loop add) and the hash
// table 0x224DA0/0x224E50 (create), 0x225248 (get), 0x225338 (set), 0x226768 (node destructor),
// 0x224D00 (destroy). Verified by tools/test_stage_script_vm_native.py against the recompiled
// originals (every stage program of the three courses plus randomized synthetic programs).
//
// Values are the original 12-byte {word0, word1, type}. word1 is only copied for type 5; every
// other copy writes word0 + type and leaves the destination's word1 as it was (modelled).
// Table values (type 4) and function values (type 5) carry host handles in word0 where the
// original carries heap / program-header addresses: handles are allocated in increasing order,
// so equality and ordering compares agree with an allocator that hands out rising addresses.
// Hashing a table or function value as a table key is address-dependent in the original.
#include "original_float.hpp"
#include "software_float.hpp"
#include <array>
#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <stdexcept>
#include <vector>

namespace ssx {

inline constexpr int32_t kOriginalScriptNil=0;      /* nil constant 0x4C9098 = {0,0,0} */
inline constexpr int32_t kOriginalScriptInt=1;
inline constexpr int32_t kOriginalScriptFloat=2;
inline constexpr int32_t kOriginalScriptSymbol=3;   /* 32-bit name hash (op 0x14), table keys */
inline constexpr int32_t kOriginalScriptTable=4;    /* refcounted hash table ("cLunoTable") */
inline constexpr int32_t kOriginalScriptFunction=5; /* {program, function record index} */
inline constexpr uint32_t kOriginalScriptMagic=0x004E554Cu; /* "LUN\0" */
inline constexpr uint32_t kOriginalScriptBuiltinCount=111;  /* table 0x441F38 */
inline constexpr uint32_t kOriginalScriptOpcodeCount=0x2B;  /* jump table 0x4797B0 */
inline constexpr uint32_t kOriginalScriptVmTableBuckets=32; /* op 0x19 -> 0x224DA0(handle, 0x20) */
inline constexpr uint32_t kOriginalScriptHostTableBuckets=8; /* 0x308DB8 / 0x30A060 / 0x30AC98 */

struct OriginalScriptError : std::runtime_error {using std::runtime_error::runtime_error;};

struct OriginalScriptValue {
    uint32_t word0=0,word1=0;
    int32_t type=kOriginalScriptNil;
    static constexpr OriginalScriptValue nil(){return {};}
    static constexpr OriginalScriptValue integer(int32_t v){return {uint32_t(v),0,kOriginalScriptInt};}
    static OriginalScriptValue real(float v){return {std::bit_cast<uint32_t>(v),0,kOriginalScriptFloat};}
    static constexpr OriginalScriptValue symbol(uint32_t hash){return {hash,0,kOriginalScriptSymbol};}
    static constexpr OriginalScriptValue function(uint32_t program,uint32_t index){return {program,index,kOriginalScriptFunction};}
    int32_t asInt() const{return int32_t(word0);}
    float asFloat() const{return std::bit_cast<float>(word0);}
    friend bool operator==(const OriginalScriptValue&,const OriginalScriptValue&)=default;
};

// Argument stack entry (16 bytes at frame sp+0x7C): {key, value}. Builtins receive the top
// `count` entries in push order.
struct OriginalScriptArg {
    int32_t key=0;
    OriginalScriptValue value;
};

// Trailer record (16 bytes each, trailer starts at the code end).
struct OriginalScriptFunctionRecord {
    int32_t headerOffset=0;     /* +0: byte offset from this record back to the LUN header */
    int32_t startPc=0;          /* +4: first instruction (code word index) */
    int32_t registerCount=0;    /* +8: frame registers (only record 0's is used, see below) */
    int32_t argumentCapacity=0; /* +C: argument stack entries allocated by 0x2227D0 */
};

struct OriginalScriptProgram {
    std::vector<uint32_t> code;    /* words after the 16-byte header, up to header+4 */
    std::vector<uint32_t> trailer; /* words from the code end to the extent */
    size_t functionCount() const{return trailer.size()/4;}
    OriginalScriptFunctionRecord function(size_t index) const{
        if(index>=functionCount())throw OriginalScriptError("LUN function record out of range");
        const uint32_t* r=trailer.data()+index*4;
        return {int32_t(r[0]),int32_t(r[1]),int32_t(r[2]),int32_t(r[3])};
    }
    // The interpreter reads inline operands past the code end straight from the trailer.
    uint32_t word(int64_t pc) const{
        if(pc>=0&&size_t(pc)<code.size())return code[size_t(pc)];
        if(pc>=0&&size_t(pc)-code.size()<trailer.size())return trailer[size_t(pc)-code.size()];
        throw OriginalScriptError("LUN fetch outside the program");
    }
};

// scripts.json layout (code_words, trailer_words).
inline OriginalScriptProgram originalScriptProgram(std::vector<uint32_t> code,std::vector<uint32_t> trailer){
    if(trailer.size()<4)throw OriginalScriptError("LUN program without a function record");
    OriginalScriptProgram p;p.code=std::move(code);p.trailer=std::move(trailer);return p;
}
// Raw program words as stored in the stage (header 0x004E554C, code end, extent, extent; code;
// trailer), e.g. web/generated/stage_scripts_seed.hpp words[first, first+count).
inline OriginalScriptProgram originalScriptProgramFromWords(const uint32_t* words,size_t count){
    if(count<4||words[0]!=kOriginalScriptMagic)throw OriginalScriptError("Not a LUN program");
    const uint32_t codeEnd=words[1];
    if(codeEnd<16||codeEnd%4||codeEnd/4>count)throw OriginalScriptError("Bad LUN code end");
    return originalScriptProgram(std::vector<uint32_t>(words+4,words+codeEnd/4),std::vector<uint32_t>(words+codeEnd/4,words+count));
}
inline OriginalScriptProgram originalScriptProgramFromWords(const std::vector<uint32_t>& words){
    return originalScriptProgramFromWords(words.data(),words.size());
}

namespace original_script_detail {
inline float bitsFloat(uint32_t w){return std::bit_cast<float>(w);}
inline uint32_t floatBits(float f){return std::bit_cast<uint32_t>(f);}
// CVT.S.W under the EE's chop rounding, exact in any host rounding mode.
inline float cvtSW(int32_t value){
    const bool negative=value<0;
    uint64_t magnitude=negative?uint64_t(-int64_t(value)):uint64_t(value);
    if(magnitude>=(1ull<<24)){int bits=64-std::countl_zero(magnitude);magnitude=(magnitude>>(bits-24))<<(bits-24);}
    const float f=float(magnitude);
    return negative?-f:f;
}
// CVT.W.S: truncate, saturate (NaN -> 0 as the recompiled cast).
inline int32_t cvtWS(float x){
    if(!(x==x))return 0;
    if(x>=2147483648.f)return INT32_MAX;
    if(x<=-2147483648.f)return INT32_MIN;
    return int32_t(x);
}
inline float mulS(float a,float b){
#if defined(__EMSCRIPTEN__)
    if(originalRoundingMode==FE_TOWARDZERO)return software_float::mul(a,b);
#endif
    volatile float left=a,right=b;float r=left*right;return r;
}
// DIV.S: nearest; a zero-exponent divisor yields sign(a^b)|0x7F7FFFFF like the EE.
inline float divS(float a,float b){
    const uint32_t x=floatBits(a),y=floatBits(b);
    if((y&0x7F800000u)==0)return bitsFloat(((x^y)&0x80000000u)|0x7F7FFFFFu);
    return originalScalarDivide(a,b);
}
// MIPS DIV (the interpreter traps with BREAK 7 on a zero divisor).
inline int32_t divW(int32_t x,int32_t y){
    if(y==0)throw OriginalScriptError("LUN integer divide by zero (BREAK 7)");
    if(y==-1&&x==INT32_MIN)return INT32_MIN;
    return x/y;
}
inline int32_t remW(int32_t x,int32_t y){
    if(y==0)throw OriginalScriptError("LUN integer divide by zero (BREAK 7)");
    if(y==-1&&x==INT32_MIN)return 0;
    return x%y;
}
enum class Compare {Equal,NotEqual,GreaterEqual,Greater,LessEqual,Less};
// Ops 3..8 (x = byte2 register, y = byte3 register), 0x225248/0x225338 key match (x = node key)
// and the 0x2224 for-loop tests. Same types compare word0 (signed for ordering, raw bits for
// floats too); float/int converts the int (CVT.S.W); int/float truncates the float (CVT.W.S);
// symbol/int compares unsigned; any other pairing is false for every operator.
inline int32_t compare(Compare op,const OriginalScriptValue& x,const OriginalScriptValue& y){
    auto order=[op](auto a,auto b)->int32_t{
        switch(op){
            case Compare::Equal:return a==b;
            case Compare::NotEqual:return !(a==b);
            case Compare::GreaterEqual:return b<=a;
            case Compare::Greater:return b<a;
            case Compare::LessEqual:return a<=b;
            case Compare::Less:return a<b;
        }
        return 0;
    };
    if(x.type==y.type){
        if(op==Compare::Equal||op==Compare::NotEqual)return order(x.word0,y.word0);
        return order(int32_t(x.word0),int32_t(y.word0));
    }
    if(x.type==kOriginalScriptFloat&&y.type==kOriginalScriptInt)return order(bitsFloat(x.word0),cvtSW(int32_t(y.word0)));
    if(x.type==kOriginalScriptInt&&y.type==kOriginalScriptFloat)return order(int32_t(x.word0),cvtWS(bitsFloat(y.word0)));
    if(x.type==kOriginalScriptSymbol&&y.type==kOriginalScriptInt)return order(x.word0,y.word0);
    return 0;
}
enum class Arithmetic {Add,Subtract,Multiply,Divide};
// Ops 0xC..0xF and 0x226628 (x = byte2 / loop variable, y = byte3 / step). Symbols only add.
inline OriginalScriptValue arithmetic(Arithmetic op,const OriginalScriptValue& x,const OriginalScriptValue& y){
    auto real=[op](float a,float b){
        switch(op){
            case Arithmetic::Add:return originalScalarAdd(a,b);
            case Arithmetic::Subtract:return originalScalarSubtract(a,b);
            case Arithmetic::Multiply:return mulS(a,b);
            case Arithmetic::Divide:break;
        }
        return divS(a,b);
    };
    auto whole=[op](int32_t a,int32_t b)->int32_t{
        switch(op){
            case Arithmetic::Add:return int32_t(uint32_t(a)+uint32_t(b));
            case Arithmetic::Subtract:return int32_t(uint32_t(a)-uint32_t(b));
            case Arithmetic::Multiply:return int32_t(uint32_t(a)*uint32_t(b));
            case Arithmetic::Divide:break;
        }
        return divW(a,b);
    };
    if(x.type==y.type){
        if(x.type==kOriginalScriptFloat)return OriginalScriptValue::real(real(bitsFloat(x.word0),bitsFloat(y.word0)));
        if(x.type==kOriginalScriptInt)return OriginalScriptValue::integer(whole(int32_t(x.word0),int32_t(y.word0)));
        if(x.type==kOriginalScriptSymbol&&op==Arithmetic::Add)return OriginalScriptValue::symbol(x.word0+y.word0);
        return OriginalScriptValue::nil();
    }
    if(x.type==kOriginalScriptFloat&&y.type==kOriginalScriptInt)return OriginalScriptValue::real(real(bitsFloat(x.word0),cvtSW(int32_t(y.word0))));
    if(x.type==kOriginalScriptInt&&y.type==kOriginalScriptFloat)return OriginalScriptValue::integer(whole(int32_t(x.word0),cvtWS(bitsFloat(y.word0))));
    return OriginalScriptValue::nil();
}
// Ops 1/2: nil false, int/symbol word0 != 0, float != 0.0 (so -0.0 is false), tables,
// functions and any other type true.
inline bool truthy(const OriginalScriptValue& v){
    switch(v.type){
        case kOriginalScriptNil:return false;
        case kOriginalScriptInt:case kOriginalScriptSymbol:return v.word0!=0;
        case kOriginalScriptFloat:return !(bitsFloat(v.word0)==0.f);
        default:return true;
    }
}
}

// Mirrors the builtins' keyed-argument parsing (e.g. builtin 27 at 0x2FF850): start from the
// builtin's default words, then for each argument in push order: word0 is stored when the
// argument type equals expectedTypes[key]; when the builtin expects a float (2) the argument's
// word0 is converted as an int (CVT.S.W, whatever its type); otherwise word0 is stored as is.
template<size_t N>
inline std::array<uint32_t,N> originalScriptKeyedArguments(const std::array<uint32_t,N>& defaults,
        const std::array<int32_t,N>& expectedTypes,const OriginalScriptArg* args,int count){
    std::array<uint32_t,N> out=defaults;
    for(int i=0;i<count;i++){
        const uint32_t key=uint32_t(args[i].key);
        if(key>=N)throw OriginalScriptError("LUN builtin argument key out of range");
        const OriginalScriptValue& v=args[i].value;
        if(v.type==expectedTypes[key])out[key]=v.word0;
        else if(expectedTypes[key]==kOriginalScriptFloat)out[key]=original_script_detail::floatBits(original_script_detail::cvtSW(int32_t(v.word0)));
        else out[key]=v.word0;
    }
    return out;
}

// Hash table ("cLunoTable", 0x224E50): `buckets` embedded head nodes, bucket = key.word0 %
// (buckets - 1) (unsigned; the last head is never used), collisions append to the head's
// chain. A head whose value is nil counts as an empty bucket: get() then returns nil without
// walking its chain and set() overwrites the head. Assigning nil to a chained key unlinks the
// node and runs its destructor, which also destroys every node after it while the previous
// node still points at them; the port keeps those nodes as tombstones (nil key and value,
// stale words, links intact), exactly what the original reads from freed memory that the
// allocator has not reused.
struct OriginalScriptTableNode {
    OriginalScriptValue key,value;
    int32_t next=-1; /* node index, -1 = none */
};
struct OriginalScriptTable {
    int32_t refcount=0;
    uint32_t buckets=0,modulus=0;
    bool destroyed=false;
    std::vector<OriginalScriptTableNode> nodes; /* [0, buckets) heads, then chain nodes */
};

class OriginalScriptVM {
public:
    using Builtin=std::function<OriginalScriptValue(int index,const OriginalScriptArg* args,int count)>;
    // Nested op 0x1B calls beyond this depth throw (the original has no limit).
    int maxCallDepth=64;
    // Optional debug hook, called before each instruction executes (program handle, pc, word).
    std::function<void(uint32_t program,int32_t pc,uint32_t word)> onInstruction;

    // Returns the handle stored in function values' word0 (1, 2, ...).
    uint32_t addProgram(OriginalScriptProgram program){
        programs_.push_back(std::move(program));return uint32_t(programs_.size());
    }
    const OriginalScriptProgram& program(uint32_t handle) const{
        if(handle==0||handle>programs_.size())throw OriginalScriptError("Invalid LUN program handle");
        return programs_[handle-1];
    }
    // 0x224DA0: a new table holding one reference (the creator's handle). Returns its handle.
    uint32_t newTable(uint32_t buckets=kOriginalScriptHostTableBuckets){
        OriginalScriptTable t;t.refcount=1;t.buckets=buckets;t.modulus=buckets-1;t.nodes.resize(buckets);
        tables_.push_back(std::move(t));return uint32_t(tables_.size());
    }
    static OriginalScriptValue tableValue(uint32_t handle){return {handle,0,kOriginalScriptTable};}
    const OriginalScriptTable& table(uint32_t handle) const{return tables_.at(checkTable(handle)-1);}
    size_t tableCount() const{return tables_.size();}
    // 0x225068-style reference taken by a builtin that returns a table value.
    void retain(const OriginalScriptValue& v){if(v.type==kOriginalScriptTable&&v.word0)++mutableTable(v.word0).refcount;}
    // 0x225B90(value, 2): drop a table reference (destroying the table at zero), type = nil.
    void release(OriginalScriptValue& v){releaseValue(v);}

    // 0x225248: value stored under key (nil when absent). table.word0 must be a table.
    OriginalScriptValue get(const OriginalScriptValue& table,const OriginalScriptValue& key) const{
        const OriginalScriptTable& t=tables_.at(checkTable(table.word0)-1);
        if(t.modulus==0)throw OriginalScriptError("LUN table modulus zero (BREAK 7)");
        int32_t n=int32_t(key.word0%t.modulus);
        if(t.nodes[size_t(n)].value.type==kOriginalScriptNil)return OriginalScriptValue::nil();
        for(;n>=0;n=t.nodes[size_t(n)].next)
            if(original_script_detail::compare(original_script_detail::Compare::Equal,t.nodes[size_t(n)].key,key))return t.nodes[size_t(n)].value;
        return OriginalScriptValue::nil();
    }
    // 0x225338.
    void set(const OriginalScriptValue& table,const OriginalScriptValue& key,const OriginalScriptValue& value){
        using original_script_detail::compare;using original_script_detail::Compare;
        if(key.type==kOriginalScriptNil)return;
        const uint32_t handle=checkTable(table.word0);
        const OriginalScriptValue keyCopy=key,valueCopy=value;
        auto node=[&](int32_t i)->OriginalScriptTableNode&{return mutableTable(handle).nodes[size_t(i)];};
        if(mutableTable(handle).modulus==0)throw OriginalScriptError("LUN table modulus zero (BREAK 7)");
        const int32_t head=int32_t(keyCopy.word0%mutableTable(handle).modulus);
        if(node(head).value.type==kOriginalScriptNil){
            assignTo([&]()->OriginalScriptValue&{return node(head).key;},keyCopy);
            assignTo([&]()->OriginalScriptValue&{return node(head).value;},valueCopy);
            return;
        }
        if(compare(Compare::Equal,node(head).key,keyCopy)){
            assignTo([&]()->OriginalScriptValue&{return node(head).value;},valueCopy);
            return;
        }
        int32_t previous=head;
        for(int32_t n=node(head).next;n>=0;previous=n,n=node(n).next){
            if(!compare(Compare::Equal,node(n).key,keyCopy))continue;
            assignTo([&]()->OriginalScriptValue&{return node(n).value;},valueCopy);
            if(node(n).value.type!=kOriginalScriptNil)return;
            node(previous).next=node(n).next;
            destroyNode(handle,n);
            return;
        }
        if(valueCopy.type==kOriginalScriptNil)return;
        OriginalScriptTableNode fresh;
        copyInto(fresh.key,keyCopy);fresh.key.type=keyCopy.type;
        copyInto(fresh.value,valueCopy);fresh.value.type=valueCopy.type;
        mutableTable(handle).nodes.push_back(fresh);
        node(previous).next=int32_t(mutableTable(handle).nodes.size()-1);
    }

    // 0x2224B8 (host entry, e.g. 0x309C88 / 0x309E50 / 0x30A060): new frame with the program's
    // record-0 register count, reg0 = globals (copied as a table, type forced to 4), reg1 = nil,
    // then 0x2227D0 from record `function`. Returns the return slot, which only ops 0x1F / 0x2A
    // write (initial value `returnSlot`); a returned table carries one reference for the caller.
    // finalRegisters receives the top-level register file as it is just before the frame is freed.
    OriginalScriptValue run(uint32_t programHandle,uint32_t function,const OriginalScriptValue& globals,const Builtin& builtin,
            std::vector<OriginalScriptValue>* finalRegisters=nullptr,OriginalScriptValue returnSlot={}){
        OriginalRounding rounding;
        const OriginalScriptValue nilArgument;
        enter(programHandle,function,globals,nilArgument,returnSlot,builtin,finalRegisters);
        return returnSlot;
    }
    // Host call of a function value (0x309E50: globals lookup by symbol, then 0x2224B8).
    OriginalScriptValue call(const OriginalScriptValue& function,const OriginalScriptValue& globals,const Builtin& builtin,
            std::vector<OriginalScriptValue>* finalRegisters=nullptr,OriginalScriptValue returnSlot={}){
        if(function.type!=kOriginalScriptFunction)throw OriginalScriptError("LUN call of a non-function value");
        return run(function.word0,function.word1,globals,builtin,finalRegisters,returnSlot);
    }

private:
    std::deque<OriginalScriptProgram> programs_;
    std::deque<OriginalScriptTable> tables_;
    int depth_=0;

    uint32_t checkTable(uint32_t handle) const{
        if(handle==0||handle>tables_.size()||tables_[handle-1].destroyed)throw OriginalScriptError("LUN value is not a live table");
        return handle;
    }
    OriginalScriptTable& mutableTable(uint32_t handle){return tables_[checkTable(handle)-1];}

    // 0x224DF0(value, 2): refcount--, at zero destroy the table (0x224F30/0x224D00) and clear
    // word0. The type is left alone.
    void dropReference(OriginalScriptValue& v){
        if(v.word0==0)throw OriginalScriptError("LUN release of a null table");
        const uint32_t handle=v.word0;
        if(--mutableTable(handle).refcount==0){destroyTable(handle);v.word0=0;}
    }
    // 0x225B90(value, flags).
    void releaseValue(OriginalScriptValue& v){
        if(v.type==kOriginalScriptTable)dropReference(v);
        v.type=kOriginalScriptNil;
    }
    // 0x225068 on a cleared destination, 8-byte copy for functions, word0 otherwise.
    void copyInto(OriginalScriptValue& dst,const OriginalScriptValue& src){
        if(src.type==kOriginalScriptTable){
            if(dst.word0!=src.word0){
                if(dst.word0)dropReference(dst);
                dst.word0=src.word0;
                if(src.word0)++mutableTable(src.word0).refcount;
            }
        }else if(src.type==kOriginalScriptFunction){dst.word0=src.word0;dst.word1=src.word1;}
        else dst.word0=src.word0;
    }
    // The interpreter's register assignment: release the destination (table), type = nil,
    // word0 = 0, copy, type = source type. Reads the source after clearing, so a self move
    // (0x0B / 0x13 with equal registers) leaves nil. `slot` re-resolves the destination
    // because a table release can reallocate node storage.
    template<class Slot>
    void assignTo(Slot slot,const OriginalScriptValue& src){
        if(slot().type==kOriginalScriptTable)dropReference(slot());
        slot().type=kOriginalScriptNil;slot().word0=0;
        const OriginalScriptValue s=src;
        copyInto(slot(),s);
        slot().type=s.type;
    }
    void assign(OriginalScriptValue& dst,const OriginalScriptValue& src){
        if(dst.type==kOriginalScriptTable)dropReference(dst);
        dst.type=kOriginalScriptNil;dst.word0=0;
        if(src.type==kOriginalScriptTable)copyInto(dst,src);
        else if(src.type==kOriginalScriptFunction){dst.word0=src.word0;dst.word1=src.word1;}
        else dst.word0=src.word0;
        dst.type=src.type;
    }
    // Result writes of compares / loads: release, clear, then word0 + type (word1 untouched).
    void store(OriginalScriptValue& dst,uint32_t word0,int32_t type){
        if(dst.type==kOriginalScriptTable)dropReference(dst);
        dst.type=kOriginalScriptNil;dst.word0=0;
        dst.word0=word0;dst.type=type;
    }
    // Constructed temporary (arithmetic, not/neg, builtin result, loop add): assign, release.
    void storeTemporary(OriginalScriptValue& dst,OriginalScriptValue temp){
        assign(dst,temp);
        releaseValue(temp);
    }
    // 0x226768: destroys the following chain first, then value and key (0x225B90 each).
    void destroyNode(uint32_t handle,int32_t index){
        const int32_t next=mutableTable(handle).nodes[size_t(index)].next;
        if(next>=0)destroyNode(handle,next);
        releaseValue(mutableTable(handle).nodes[size_t(index)].value);
        releaseValue(mutableTable(handle).nodes[size_t(index)].key);
    }
    // 0x224D00: head nodes from last to first, then the table is freed.
    void destroyTable(uint32_t handle){
        for(int32_t i=int32_t(mutableTable(handle).buckets)-1;i>=0;i--)destroyNode(handle,i);
        tables_[handle-1].destroyed=true;
    }

    // 0x2224B8 / 0x222648 frame setup followed by 0x2227D0.
    void enter(uint32_t programHandle,uint32_t function,const OriginalScriptValue& globals,const OriginalScriptValue& argument,
            OriginalScriptValue& returnSlot,const Builtin& builtin,std::vector<OriginalScriptValue>* finalRegisters){
        const OriginalScriptProgram& p=program(programHandle);
        const int32_t count=p.function(0).registerCount;
        if(count<2)throw OriginalScriptError("LUN frame with fewer than two registers");
        std::vector<OriginalScriptValue> regs(static_cast<size_t>(count));
        regs[0].type=kOriginalScriptNil;regs[0].word0=0;
        if(globals.word0){regs[0].word0=globals.word0;++mutableTable(globals.word0).refcount;}
        regs[0].type=kOriginalScriptTable;
        assign(regs[1],argument);
        struct Depth {int& depth;~Depth(){--depth;}};
        ++depth_;
        const Depth guard{depth_};
        if(depth_>maxCallDepth)throw OriginalScriptError("LUN call depth limit");
        execute(programHandle,function,regs,returnSlot,builtin,finalRegisters);
    }

    // 0x2227D0.
    void execute(uint32_t programHandle,uint32_t function,std::vector<OriginalScriptValue>& regs,OriginalScriptValue& returnSlot,
            const Builtin& builtin,std::vector<OriginalScriptValue>* finalRegisters){
        using namespace original_script_detail;
        const OriginalScriptProgram& p=program(programHandle);
        const OriginalScriptFunctionRecord record=p.function(function);
        std::vector<OriginalScriptArg> args(static_cast<size_t>(record.argumentCapacity>0?record.argumentCapacity:0));
        int32_t count=0;
        const int32_t codeWords=int32_t(p.code.size());
        int32_t pc=record.startPc;
        auto reg=[&](uint32_t i)->OriginalScriptValue&{
            if(i>=regs.size())throw OriginalScriptError("LUN register out of range");
            return regs[i];
        };
        auto push=[&](uint32_t key)->OriginalScriptValue&{
            if(count<0||size_t(count)>=args.size())throw OriginalScriptError("LUN argument stack overflow");
            OriginalScriptArg& a=args[size_t(count)];
            a.key=int32_t(key);
            if(a.value.type==kOriginalScriptTable)dropReference(a.value);
            a.value.type=kOriginalScriptNil;a.value.word0=0;
            return a.value;
        };
        auto pushImmediate=[&](uint32_t key,uint32_t word0,int32_t type){
            OriginalScriptValue& v=push(key);
            ++count;v.word0=word0;v.type=type;
        };
        while(pc<codeWords){
            const uint32_t w=p.word(pc);
            if(onInstruction)onInstruction(programHandle,pc,w);
            ++pc;
            const uint32_t op=w&255,b1=(w>>8)&255,b2=(w>>16)&255,b3=w>>24;
            switch(op){
            case 0x00:pc=int32_t(w>>16);break;
            case 0x01:if(truthy(reg(b1)))pc=int32_t(w>>16);break;
            case 0x02:if(!truthy(reg(b1)))pc=int32_t(w>>16);break;
            case 0x03:case 0x04:case 0x05:case 0x06:case 0x07:case 0x08:{
                static constexpr Compare kind[]={Compare::Equal,Compare::NotEqual,Compare::GreaterEqual,Compare::Greater,Compare::LessEqual,Compare::Less};
                const int32_t r=compare(kind[op-3],reg(b2),reg(b3));
                store(reg(b1),uint32_t(r),kOriginalScriptInt);
                break;
            }
            case 0x09:{
                const OriginalScriptValue &x=reg(b2),&y=reg(b3);
                const uint32_t r=x.type==kOriginalScriptNil?0u:x.word0!=0?1u:uint32_t(y.type!=kOriginalScriptNil&&y.word0!=0);
                store(reg(b1),r,kOriginalScriptInt);
                break;
            }
            case 0x0A:{
                const OriginalScriptValue &x=reg(b2),&y=reg(b3);
                const uint32_t r=uint32_t(y.type!=kOriginalScriptNil&&x.type!=kOriginalScriptNil&&x.word0!=0&&y.word0!=0);
                store(reg(b1),r,kOriginalScriptInt);
                break;
            }
            case 0x0B:case 0x13:{
                OriginalScriptValue& dst=reg(b1);
                OriginalScriptValue& src=reg(b2);
                if(dst.type==kOriginalScriptTable)dropReference(dst);
                dst.type=kOriginalScriptNil;dst.word0=0;
                if(src.type==kOriginalScriptTable)copyInto(dst,src);
                else if(src.type==kOriginalScriptFunction){dst.word0=src.word0;dst.word1=src.word1;}
                else dst.word0=src.word0;
                dst.type=src.type;
                break;
            }
            case 0x0C:case 0x0D:case 0x0E:case 0x0F:{
                static constexpr Arithmetic kind[]={Arithmetic::Add,Arithmetic::Subtract,Arithmetic::Multiply,Arithmetic::Divide};
                storeTemporary(reg(b1),arithmetic(kind[op-0x0C],reg(b2),reg(b3)));
                break;
            }
            case 0x10:storeTemporary(reg(b1),OriginalScriptValue::nil());break;
            case 0x11:{
                const OriginalScriptValue &x=reg(b2),&y=reg(b3);
                OriginalScriptValue r;
                if(x.type==y.type&&x.type==kOriginalScriptInt)r=OriginalScriptValue::integer(remW(int32_t(x.word0),int32_t(y.word0)));
                storeTemporary(reg(b1),r);
                break;
            }
            case 0x12:set(reg(b1),reg(b2),reg(b3));break;
            case 0x14:{const uint32_t v=p.word(pc);++pc;store(reg(b1),v,kOriginalScriptSymbol);break;}
            case 0x15:case 0x16:{const uint32_t v=p.word(pc);++pc;store(reg(b1),v,kOriginalScriptInt);break;}
            case 0x17:{const uint32_t v=p.word(pc);++pc;store(reg(b1),v,kOriginalScriptFloat);break;}
            case 0x18:{
                const OriginalScriptValue v=get(reg(b2),reg(b3));
                assign(reg(b1),v);
                break;
            }
            case 0x19:{
                const uint32_t handle=newTable(kOriginalScriptVmTableBuckets);
                OriginalScriptValue& dst=reg(b1);
                if(dst.type==kOriginalScriptTable)dropReference(dst);
                dst.type=kOriginalScriptNil;dst.word0=0;
                copyInto(dst,tableValue(handle));
                dst.type=kOriginalScriptTable;
                break;
            }
            case 0x1B:{
                const OriginalScriptValue f=reg(b2);
                if(f.type==kOriginalScriptNil)break;
                if(f.type!=kOriginalScriptFunction)throw OriginalScriptError("LUN call of a non-function value");
                enter(f.word0,f.word1,reg(0),reg(b3),reg(b1),builtin,nullptr);
                break;
            }
            case 0x1C:set(reg(b1),reg(b2),reg(b3));++reg(b2).word0;break;
            case 0x1D:{
                const uint32_t index=p.word(pc);++pc;
                OriginalScriptValue& dst=reg(b1);
                if(dst.type==kOriginalScriptTable)dropReference(dst);
                dst.type=kOriginalScriptNil;dst.word0=0;
                dst.word0=programHandle;dst.word1=index;dst.type=kOriginalScriptFunction;
                break;
            }
            case 0x1E:assign(reg(b1),OriginalScriptValue::nil());break;
            case 0x1F:assign(returnSlot,reg(b1));pc=codeWords;break;
            case 0x20:{
                const OriginalScriptValue& src=reg(b2);
                OriginalScriptValue& v=push(b1);
                if(src.type==kOriginalScriptTable)copyInto(v,src);
                else if(src.type==kOriginalScriptFunction){v.word0=src.word0;v.word1=src.word1;}
                else v.word0=src.word0;
                ++count;v.type=src.type;
                break;
            }
            case 0x21:{
                if(int32_t(b3)>count)throw OriginalScriptError("LUN argument stack underflow");
                OriginalScriptValue temp=builtin(int(b2),args.data()+(count-int32_t(b3)),int(b3));
                assign(reg(b1),temp);
                count-=int32_t(b3);
                releaseValue(temp);
                break;
            }
            case 0x22:{
                const OriginalScriptValue& x=reg(b2);
                OriginalScriptValue r;
                if(x.type==kOriginalScriptInt)r=OriginalScriptValue::integer(x.word0==0);
                else if(x.type==kOriginalScriptFloat)r=OriginalScriptValue::integer(bitsFloat(x.word0)==0.f);
                storeTemporary(reg(b1),r);
                break;
            }
            case 0x23:{
                const OriginalScriptValue& x=reg(b2);
                OriginalScriptValue r;
                if(x.type==kOriginalScriptInt)r=OriginalScriptValue::integer(int32_t(0u-x.word0));
                else if(x.type==kOriginalScriptFloat)r=OriginalScriptValue::real(-bitsFloat(x.word0));
                storeTemporary(reg(b1),r);
                break;
            }
            case 0x24:{
                const int32_t target=int32_t(p.word(pc));++pc;
                const bool negative=compare(Compare::Less,reg(b3),OriginalScriptValue::real(0.f));
                if(!compare(negative?Compare::GreaterEqual:Compare::LessEqual,reg(b1),reg(b2)))break;
                storeTemporary(reg(b1),arithmetic(Arithmetic::Add,reg(b1),reg(b3)));
                pc=target;
                break;
            }
            case 0x25:case 0x27:{const uint32_t v=p.word(pc);++pc;pushImmediate(b1,v,kOriginalScriptInt);break;}
            case 0x26:{const uint32_t v=p.word(pc);++pc;pushImmediate(b1,v,kOriginalScriptFloat);break;}
            case 0x28:pushImmediate(b1,b2,kOriginalScriptInt);break;
            case 0x29:pushImmediate(b1,floatBits(cvtSW(int32_t(b2))),kOriginalScriptFloat);break;
            case 0x2A:assign(returnSlot,OriginalScriptValue::nil());pc=codeWords;break;
            default:break; /* 0x1A and 0x2B..0xFF */
            }
        }
        if(finalRegisters)*finalRegisters=regs;
        for(size_t i=args.size();i-->0;)releaseValue(args[i].value);
        for(size_t i=regs.size();i-->0;)releaseValue(regs[i]);
    }
};

}
