#pragma once
#include <array>
#include <cstdint>
namespace ssx {
using OriginalTrickIdentity=std::array<uint32_t,2>; //11A8C8 packed eight-byte record
struct OriginalTrickHistory {
    std::array<OriginalTrickIdentity,10> entries{}; //scoring+A8..F7
    uint32_t next=0; //scoring+F8
};
struct OriginalTrickRepeatContext {
    int32_t field08=0,style20=0,active70=0,field7C=0,flag28=0;
};
// Complete1190F0. Some contexts bypass both matching and insertion; the two
// identity masks exempt repetition but still insert into the circular history.
int originalTrickRepeatCount(OriginalTrickHistory&,const OriginalTrickIdentity&,
    const OriginalTrickRepeatContext&);
}
