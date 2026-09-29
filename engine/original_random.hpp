#pragma once
#include <array>
#include <cstdint>
namespace ssx {
// Original317A08, used by317810 with the shared state at4FF030. This state is
// global to the game; independent per-rider generators would change draw order.
struct OriginalRandomState {
    std::array<uint32_t,6> words{};
    uint32_t next() {
        uint32_t value=words[5]+words[4];uint32_t carry=value<words[5]||value<words[4];words[4]=value;
        for(int i=3;i>=1;--i){value=value+words[i]+carry;carry=value<words[i];words[i]=value;}
        value=value+words[0]+carry;words[0]=value;
        if(++words[5]==0) {
            int i=4;while(i>=1&&++words[i]==0)--i;
            if(i==0)words[0]=++value;
        }
        return value;
    }
};
}
