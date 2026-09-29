#include "trick_bonus.hpp"
namespace ssx {
int32_t originalNamedTrickBonus(OriginalTrickIdentity& id,const OriginalNamedTrickTable& table){
 const std::array<uint8_t,7> fields={uint8_t((id[0]>>10)&3),uint8_t((id[0]>>12)&15),uint8_t((id[0]>>16)&7),uint8_t((id[0]>>19)&7),uint8_t(id[0]>>28),uint8_t((id[1]>>3)&127),uint8_t((id[1]>>11)&127)};
 for(const auto& row:table)if(row.identityFields==fields){id={0,row.id<<27};return row.points;}
 return 0;
}
}
