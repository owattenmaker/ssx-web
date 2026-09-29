#pragma once
#include "trick_history.hpp"
namespace ssx {
struct OriginalTrickIdentityState {
 int32_t stance00=0,field04=0,field08=0,style0C=0,flag10=0,style20=0,flag28=0,active70=0,field7C=0;
 //117838 disables distance/manual accrual with -1; +24 is centimetres.
 float time24=-1,time2C=-1,spin34=0,flip38=0;
 std::array<int32_t,3> grabs60{};
};
struct OriginalTrickIdentityProfile {
 float spinDegrees=0,flipDegrees=0;
 std::array<uint8_t,319> ordinary{},alternate{}; //43D388/43D4C8
};
struct OriginalTrickIdentityInput {bool stanceChanged=false,alternate=false;int32_t style=0,flag=0;bool riderStance=false;};
struct OriginalTrickIdentityResult {OriginalTrickIdentity identity{};bool valid=false;};
OriginalTrickIdentityResult originalTrickIdentity(OriginalTrickIdentityState&,
 const OriginalTrickIdentityProfile&,const OriginalTrickIdentityInput&);
}
