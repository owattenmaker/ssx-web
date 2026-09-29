#pragma once
#include "air_motion.hpp"
#include <functional>
#include <stdexcept>
namespace ssx {
struct OriginalAirTrajectoryUnavailable:std::runtime_error {using std::runtime_error::runtime_error;};
struct OriginalAirTrajectoryHit {
    bool complete=true; // False means an intersecting world resource is unsupported.
    float fraction=-1; // Original reverse query, predicted end towards start.
    std::array<float,3> position{},normal{};
    int surface=-1;bool hasPatch=false;int patchFlags=-1,patchId=-1;
    float patchU=0,patchV=0;
    // Host coverage diagnostics; never interpreted as a miss or physical contact.
    uint32_t unavailableResource=0;
    unsigned unavailableCause=0; //1 missing world,2 unsupported instance,3 capacity.
};
// The input endpoints are already expanded by the original 2% each end.
// Query starts at end, travels to start; mode is 0 (336850) or 2 (3378C0).
using OriginalAirTrajectoryQuery=std::function<OriginalAirTrajectoryHit(
    std::array<float,3> end,std::array<float,3> start,int mode)>;
struct OriginalAirTrajectory {
    std::array<float,3> hitPosition{},heading{},normal{0,0,1},apexPosition{};
    int patchId=-1;float patchU=0,patchV=0;
    OriginalAirState prediction{},integrated{}; // source+50/60, +70/80
    int surface=-1,patchFlags=0;
    float predictedTime=0,apexTime=0,elapsed=0,integratedTime=0;
    float speedLimit=3333.33349609375f;int status=0;
    // Original 1135B8/113198: new flight, retaining the untouched old hit/apex/flags.
    void begin(OriginalAirState current,float maximumSpeed=3333.33349609375f);
    // Original 113618: does not reset elapsed/integratedTime or hit metadata.
    void reseed(OriginalAirState current);
    // Original 113200: extend prediction by one adaptively sized chord/query.
    void extend(const OriginalAirTrajectoryQuery& query);
    // Original 113648: update prediction/cache and integrate/interpolate state.
    OriginalAirState step(float seconds,OriginalAirState current,const OriginalAirTrajectoryQuery& query);
    OriginalAirState stepLogic(float timeScale,OriginalAirState current,const OriginalAirTrajectoryQuery& query);
};
}
