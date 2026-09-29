#include "input.hpp"
#include <cassert>
#include <cstdio>
int main() {
    ssx::InputMapper mapper;
    for (unsigned mask=0;mask<16;++mask) {
        ssx::InputSample sample;
        for (unsigned i=0;i<4;++i) if (mask&(1u<<i)) sample.buttons|=ssx::bit(mapper.bindings.grabs[i]);
        assert(mapper.update(sample).grabMask==mask);
    }
    ssx::InputSample s; s.buttons=ssx::bit(ssx::Button::South);
    assert(mapper.update(s).jumpPressed);
    assert(!mapper.update(s).jumpPressed);
    assert(mapper.update(s).jumpHeld);
    mapper.update({});
    assert(mapper.update(s).jumpPressed);
    mapper.bindings.jump=ssx::Button::North;
    assert(!mapper.update(s).jumpHeld);
    s.buttons=ssx::bit(ssx::Button::North);
    assert(mapper.update(s).jumpPressed);
    s.leftX=-.4f; s.dpadX=1; s.leftY=-.8f; s.dpadY=.2f;
    auto r=mapper.update(s);
    // Hardware samples pass through original byte/deadzone response and signed6bit packing.
    assert(r.turn>.93f&&r.turn<.97f&&r.brake>.67f&&r.brake<.68f&&r.crouch>.19f&&r.crouch<.20f);
    std::puts("Native input: 16 independent grab chords, press edges, remapping, PS2 analog/D-pad rules passed");
}
