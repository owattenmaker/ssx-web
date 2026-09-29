#include "ground_pose_motion.hpp"
#include <cassert>
#include <bit>
#include <cfenv>
#include <cstdio>
int main(){
    // Consecutive original Snow Jam glide andglide-1 snapshots: controller+0,
    // post-contact velocity+1E0 and board lift+31C. No captured pose is injected.
    ssx::OriginalGroundState state;state.boardBouncePhase=5.991682529449463f;
    std::array<float,3> velocity{-1744.1370849609375f,-324.979736328125f,-609.049560546875f};
    auto bits=[](float x){return std::bit_cast<uint32_t>(x);};
    int prior=std::fegetround();ssx::originalGroundBoardLift(state,velocity);assert(std::fegetround()==prior);
    assert(bits(state.boardBouncePhase)==bits(6.1706318855285645f));
    assert(bits(state.boardLift)==bits(.5032570362091064f));
    state.boardNormal={-.3196742832660675f,-.06936278939247131f,.9449852108955383f};
    state.normal={-.3221125602722168f,-.06371768563985825f,.9445548057556152f};
    ssx::originalGroundBoardNormal(state);std::array<float,3> expected{-.32048842310905457f,-.0674813762307167f,.9448457360267639f};
    for(unsigned i=0;i<3;++i)assert(bits(state.boardNormal[i])==bits(expected[i]));
    assert(ssx::originalCosine(0)==1);
    puts("Original consecutive-frame board phase/lift/filtered normal is bit-identical");
}
