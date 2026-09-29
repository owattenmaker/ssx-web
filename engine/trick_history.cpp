#include "trick_history.hpp"
#include <stdexcept>
namespace ssx {
int originalTrickRepeatCount(OriginalTrickHistory& h,const OriginalTrickIdentity& id,
    const OriginalTrickRepeatContext& c){
    if(c.field08||c.style20||c.active70||c.field7C||c.flag28)return 0;
    if(h.next>=h.entries.size())throw std::runtime_error("Original trick history cursor outside ten-entry ring");
    int repeats=0;
    if((id[1]&0x0003f800u)!=0x00000800u && (id[0]&0x0fc00000u)!=0x00400000u)
        for(const auto& old:h.entries)if(old==id)++repeats;
    h.entries[h.next]=id;h.next=(h.next+1)%10;
    return repeats;
}
}
