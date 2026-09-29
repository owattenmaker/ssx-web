#pragma once
#include <cstdint>
namespace ssx {
//342C7C..342C94, pickup script Debounce mode0.
inline uint32_t originalPickupDebounceFlags(uint32_t flags){return flags&0xfffffff0u;}
//350FA0..350FC8, type19 replacement construction. The bit4 lifecycle
//must not be inferred as a one-frame pending flag; see pickup-recovery.md.
inline uint32_t originalPickupRestorePendingFlags(uint32_t flags){return (flags&0xffffff9fu&0xfffffffdu)|4u;}
}
namespace ssx {
//34FC1C..34FC60: preserve authored flags and sign-extend their high half.
inline uint32_t originalPickupRestoredFlags(uint32_t flags){
 uint32_t kept=flags&0xffff0300u;
 uint32_t authored=kept>>16;if(kept&0x80000000u)authored|=0xffff0000u;
 return kept|authored|2u;
}
}
