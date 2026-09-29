#pragma once
#include "preview_rider_physics.h"
#include "grab_lifecycle.hpp"
#include "air_animation_selector.hpp"
#include <functional>
namespace ssx {
struct GameplayAnimationEvent {int scoreId=0;bool begin=false,advanced=false;};
// One original controller/sequence state shared by collision and rendered rigs.
class GameplayAnimation : public std::enable_shared_from_this<GameplayAnimation> {
    std::shared_ptr<OriginalRandomState> random;
    OriginalGrabState grabState;
    OriginalGrabProfile grabProfile;
    bool uberEnabled=false;
    float legWeight=1;
    OriginalAirAnimationState airAnimationState;
    bool grab(PrototypeRider&,const RiderInput&);
    void selectAir(PrototypeRider&,const OriginalAirControlFrame&);
public:
    std::shared_ptr<OriginalRiderAnimation> player;
    std::vector<GameplayAnimationEvent> events;
    std::function<void(const PrototypeRider&)> afterFrame;
    GameplayAnimation(std::shared_ptr<OriginalRiderAnimation>,NSDictionary* initial);
    void bind(PrototypeRider&);
};
}
