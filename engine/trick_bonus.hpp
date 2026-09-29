#pragma once
#include "trick_history.hpp"
namespace ssx {
struct OriginalNamedTrickRule {
 uint32_t id=0;int32_t points=0;
 std::array<uint8_t,7> identityFields{};
};
using OriginalNamedTrickTable=std::array<OriginalNamedTrickRule,24>;
//11B1A8: first matching authored combination replaces the ordinary identity
// with its named-trick ID and returns the authored point bonus.
int32_t originalNamedTrickBonus(OriginalTrickIdentity&,const OriginalNamedTrickTable&);
}
