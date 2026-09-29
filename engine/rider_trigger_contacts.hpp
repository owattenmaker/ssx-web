#pragma once
#include <array>
#include <cstdint>
namespace ssx {
// Original108C28: rider+5B8, at most64 instance resources, FFFFFFFF terminator.
inline bool originalRiderTouchesInstance(const std::array<uint32_t,64>& contacts,uint32_t resource){
 for(uint32_t id:contacts){if(id==0xffffffffu)return false;if(id==resource)return true;}
 return false;
}
}
