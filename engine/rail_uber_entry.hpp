#pragma once
#include <cstdint>
namespace ssx {
struct OriginalRailUberEntryResult {bool enter=false,notifyUnavailable=false;};
//132620: pending identity belongs to owner+394; remembered identity is
//ordinary rail controller+4. Successful handoff does not update that latch.
inline OriginalRailUberEntryResult originalRailUberEntry(int32_t& remembered,int32_t& pending,
        int32_t requested,float trickyTime,uint32_t abilityFlags){
 if(requested==-1){remembered=-1;return {};}
 if(trickyTime>0&&(abilityFlags&2)){pending=requested;return {true,false};}
 bool notify=remembered!=requested;remembered=requested;return {false,notify};
}
}

namespace ssx {
//PS2 INPUT.MAP UberGrind1..4 are exclusive L1/L2/R1/R2 held expressions.
//127848 checks mapped actions27..30 and returns -1 when none is active.
inline int originalRailUberIdentity(uint8_t shoulderMask){switch(shoulderMask){case 1:return 0;case 2:return 1;case 4:return 2;case 8:return 3;default:return -1;}}
//127FEC..127FF8: control12 uses bits15..22, unlike control7's17..24.
inline int originalRailUberCommandIdentity(uint32_t word){return int8_t((word>>15)&255);}
}
