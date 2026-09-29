#include "riding.hpp"
#include <cassert>
#include <cstdio>

ssx::CollisionWorld slope(double gradient=0) {
    return ssx::CollisionWorld({{{-1000,1000*gradient,-1000},{1000,1000*gradient,-1000},{1000,-1000*gradient,1000}},
                               {{-1000,1000*gradient,-1000},{1000,-1000*gradient,1000},{-1000,-1000*gradient,1000}}});
}
int main() {
    auto flat=slope(),hill=slope(.15);
    ssx::PrototypeRider rider;
    rider.reset({0,0,0},{0,1,0},0);
    ssx::RiderInput input;
    rider.advance(0.25,input,flat,flat);
    assert(rider.grounded && rider.speed()==0 && std::abs(rider.position.y-.025)<1e-9);
    // Jump charges while held, launches on release, and lands without penetrating.
    input.jumpHeld=true;
    for (int i=0;i<60;++i) rider.advance(1.0/60,input,flat,flat);
    assert(rider.grounded && rider.jumpCharge>.99);
    input.jumpHeld=false;rider.advance(1.0/60,input,flat,flat);
    // Numerical takeoff fidelity is checked against PCSX2 in jump_motion_tests;
    // this scaffold test checks that the recovered launch reaches collision flow.
    assert(!rider.grounded && rider.velocity.y>0 && rider.position.y>.025);
    for (int i=0;i<180;++i) rider.advance(1.0/60,input,flat,flat);
    assert(rider.grounded && std::abs(rider.position.y-.025)<1e-8);
    input.jumpHeld=true;rider.advance(.25,input,flat,flat);rider.cancelInput();
    input.jumpHeld=false;rider.advance(1.0/60,input,flat,flat);
    assert(rider.grounded && rider.jumpCharge==0);
    // Equivalent elapsed time split across render cadences yields identical physics.
    ssx::PrototypeRider a,b;
    a.reset({0,0,0},ssx::unit({0,1,.15}),0);b=a;
    input.crouch=1;input.turn=.1;
    for (int i=0;i<600;++i) a.advance(1.0/60,input,hill,hill);
    for (int i=0;i<1440;++i) b.advance(1.0/144,input,hill,hill);
    assert(ssx::dot(a.position-b.position,a.position-b.position)<1e-16);
    assert(a.position.z>30 && a.grounded);
    assert(std::abs(a.position.y+.15*a.position.z-.025)<1e-8);
    // Braking stops a rider on flat ground; boost is isolated prototype acceleration.
    rider.reset({0,0,0},{0,1,0},0);input={};input.boostHeld=true;
    for (int i=0;i<120;++i) rider.advance(1.0/60,input,flat,flat);
    assert(rider.speed()>15);input={};input.brake=1;
    for (int i=0;i<180;++i) rider.advance(1.0/60,input,flat,flat);
    assert(rider.speed()==0);
    // A thin vertical wall stops motion before the board crosses it.
    ssx::CollisionWorld wall({{{-20,-1,3},{20,-1,3},{20,5,3}},{{-20,-1,3},{20,5,3},{-20,5,3}}});
    rider.reset({0,0,0},{0,1,0},0);input={};input.boostHeld=true;
    for (int i=0;i<240;++i) rider.advance(1.0/60,input,flat,wall);
    assert(rider.position.z<3 && rider.position.z>2.7);
    // Leaving a finite platform enters airborne state rather than snapping far down.
    ssx::CollisionWorld platform({{{-10,0,-10},{10,0,-10},{10,0,1}},{{-10,0,-10},{10,0,1},{-10,0,1}}});
    rider.reset({0,0,0},{0,1,0},0);
    for (int i=0;i<120;++i) rider.advance(1.0/60,input,platform,platform);
    assert(!rider.grounded && rider.position.y<0);
    // An isolated source-controller fixture can omit upper actions only when
    // metadata proves there is no active attack animation or requested attack.
    for(int upperClass:{0,3,13,-1})for(bool attack:{false,true}) {
        ssx::PrototypeRider controller;controller.reset({0,10,0},{0,1,0},0);controller.grounded=false;
        ssx::OriginalGroundState pose;pose.controlState=4;pose.turn.current=.25f;
        controller.seedOriginalPoseControls(pose);
        ssx::RiderControllerCallbacks callbacks;
        if(upperClass>=0)callbacks.upperAnimationClass=[upperClass](){return upperClass;};
        callbacks.railAction=[](){return false;}; // Explicit fixture result.
        controller.setControllerCallbacks(callbacks);
        ssx::RiderPoseStages stages;stages.mainAnimation=[](){return std::optional<ssx::BodyAnimationState>{{268,9,0,false}};};
        ssx::RiderInput neutral;neutral.passiveUpper14=attack;
        controller.beginFrame(neutral,flat,flat,nullptr,nullptr,&stages);
        bool rejected=false;try{controller.runFramePhase(ssx::RiderFramePhase::Controls);}catch(const std::runtime_error& error){rejected=std::string(error.what())=="Passive air upper-action callback missing";}
        assert(rejected==(attack||upperClass!=0));
    }
    std::puts("Native provisional riding: contact, jump/release/landing, frame cadence, braking, wall sweep, edge departure passed");
}
