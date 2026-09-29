#include "race_event.hpp"
#include <cassert>
#include <cfenv>
int main(){
 using namespace ssx;
 OriginalRaceClock clock;clock.phase=RacePhase::PreRace;clock.raceEnabled=1;
 const float unfinished=-1;originalRaceClockStep(clock,{&unfinished,1});assert(clock.raceTicks==0);
 originalRaceSelect(clock,RacePhase::Countdown);
 for(int i=0;i<180;i++){auto out=originalRaceClockStep(clock,{&unfinished,1});assert(clock.phase==RacePhase::Countdown);assert(clock.countdownTicks==179-i);assert(clock.raceTicks==0);if(i==0)assert(out.entered==RacePhase::Countdown);}
 originalRaceClockStep(clock,{&unfinished,1});assert(clock.phase==RacePhase::Race&&clock.previous==RacePhase::Countdown&&clock.raceTicks==0);
 auto started=originalRaceClockStep(clock,{&unfinished,1});assert(started.raceStartNotification&&clock.raceTicks==1);
 float finished=0;auto ending=originalRaceClockStep(clock,{&finished,1});assert(ending.requestResults&&clock.phase==RacePhase::EndRace&&clock.raceTicks==2);
 auto ended=originalRaceClockStep(clock,{&finished,1});assert(ended.raceFinishNotification&&clock.raceTicks==2);
 OriginalRaceCheckpoints checkpoints;checkpoints.count=3;
 assert(originalRaceRecordCheckpoint(checkpoints,0,0));assert(!originalRaceRecordCheckpoint(checkpoints,0,0));assert(originalRaceRecordCheckpoint(checkpoints,0,1));assert(checkpoints.humanMasks[0]==3&&checkpoints.pendingHumanMask==1);
 checkpoints.mode=10;assert(!originalRaceRecordCheckpoint(checkpoints,0,2));
 OriginalRacePath path;path.origin={0,0,0};path.low={-10,-10,-10};path.high={1010,10,10};path.remainingAtOrigin=1000;path.segments={{1,0,0,1000}};path.events={{11,0,300,300},{1,0,900,900}};
 OriginalRaceProgress progress;progress.pathIndex=0;progress.remaining=progress.bestRemaining=1000;
 auto events=originalRaceProgressStep({&path,1},progress,{301,0,0},{100,0,0},0);assert(events.size()==1&&events[0].type==11&&progress.bestRemaining==699);
 events=originalRaceProgressStep({&path,1},progress,{250,0,0},{-100,0,0},1);assert(events.empty()&&progress.bestRemaining==699&&progress.remaining==750);
 events=originalRaceProgressStep({&path,1},progress,{902,0,0},{100,0,0},2);assert(events.size()==1&&events[0].type==1);
 OriginalRiderRaceFinish rider;rider.penaltyTicks=120;originalRaceMarkFinished(rider,600);assert(rider.elapsed==0&&rider.finishTicks==720);
 OriginalRiderRaceFinish triggerFinish;checkpoints.mode=0;
 auto applied=originalRaceApplyCourseEvents(events,triggerFinish,checkpoints,0,999,true);assert(applied.finished&&triggerFinish.finishTicks==999);
 assert(!originalRaceApplyCourseEvents(events,triggerFinish,checkpoints,0,1000,true).finished);
 int old=std::fegetround();originalRaceFinishElapsedStep(rider);assert(rider.elapsed>0&&std::fegetround()==old);
}
