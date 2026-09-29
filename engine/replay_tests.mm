#include "replay_io.h"
#include <cassert>
#include <cstdio>

int main() {
    @autoreleasepool {
        auto flat=ssx::CollisionWorld({{{-100,0,-100},{100,0,-100},{100,0,100}},{{-100,0,-100},{100,0,100},{-100,0,100}}});
        // Half-open ranges: adjacent pulse produces release exactly on frame2.
        ssx::InputReplay jump(5,{{0,2,ssx::replayButton("Cross"),{},{} },{3,4,ssx::replayButton("Cross"),{},{} }});
        ssx::PrototypeRider a;a.reset({0,0,0},{0,1,0},0);auto b=a;
        auto first=ssx::runReplay(jump,a,flat,flat),second=ssx::runReplay(jump,b,flat,flat);
        assert(first.size()==6 && first[0].inputFrame==-1 && first[0].frame==0 && first[0].simulationTime==0);
        assert(first[1].input.jumpPressed && first[2].input.jumpHeld && !first[2].input.jumpPressed);
        assert(first[2].grounded && first[3].jumpReleased && !first[3].grounded && first[3].position.y>first[2].position.y);
        assert(first[4].input.jumpPressed && first[5].jumpReleased);
        for (size_t i=0;i<first.size();++i) {
            assert(first[i].position.x==second[i].position.x && first[i].position.y==second[i].position.y && first[i].position.z==second[i].position.z);
            assert(first[i].velocity.x==second[i].velocity.x && first[i].velocity.y==second[i].velocity.y && first[i].velocity.z==second[i].velocity.z);
            assert(first[i].inputFrame==int(i)-1 && first[i].frame==int(i));
        }
        // Overlapping buttons union; each last active analog pair overrides independently.
        ssx::InputReplay overlap(4,{{0,4,ssx::replayButton("L1"),std::array<uint8_t,2>{0,255},{}},
                                  {1,3,ssx::replayButton("R2"),{},std::array<uint8_t,2>{255,0}},
                                  {2,3,ssx::replayButton("Square"),std::array<uint8_t,2>{127,127},{}}});
        assert(overlap.at(0).right[0]==127 && overlap.at(1).left[0]==0 && overlap.at(2).left[0]==127 && overlap.at(3).left[0]==0);
        auto input=ssx::replayInput(overlap.at(1));assert(input.leftX==-1&&input.leftY==-1&&input.rightX==1&&input.rightY==1);
        ssx::InputMapper mapper;assert(mapper.update(input).grabMask==9);
        auto bytes=overlap.at(1).movieBytes();assert(bytes[0]==255 && bytes[1]==249 && bytes[2]==255 && bytes[3]==0 && bytes[4]==0 && bytes[5]==255 && bytes[14]==255 && bytes[17]==255);
        for (unsigned chord=0;chord<16;++chord) {
            ssx::ReplayPad pad;const char* names[]={"L1","L2","R1","R2"};
            for (unsigned bit=0;bit<4;++bit) if (chord&(1u<<bit)) pad.buttons|=ssx::replayButton(names[bit]);
            assert(mapper.update(ssx::replayInput(pad)).grabMask==chord);
        }
        // Start edges pause/unpause while replay time and event boundaries advance.
        ssx::InputReplay pause(5,{{1,3,ssx::replayButton("Start"),{},{} },{4,5,ssx::replayButton("Start"),{},{} }});
        a.reset({0,0,0},{0,1,0},0);a.velocity={0,0,5};auto paused=ssx::runReplay(pause,a,flat,flat);
        assert(paused[2].paused && paused[4].paused && !paused[5].paused);
        assert(paused[1].position.z==paused[4].position.z && paused[5].position.z>paused[4].position.z);
        // Parse the shared JSON schema and restore exact initial state, without the
        // spawn projection/clearance adding an invisible position offset.
        NSString* directory=[NSTemporaryDirectory() stringByAppendingPathComponent:NSUUID.UUID.UUIDString];
        [NSFileManager.defaultManager createDirectoryAtPath:directory withIntermediateDirectories:YES attributes:nil error:nil];
        NSString* path=[directory stringByAppendingPathComponent:@"scenario.json"];
        NSString* json=@"{\"frames\":2,\"events\":[{\"start\":0,\"end\":1,\"buttons\":[\"Cross\"],\"left\":[127,0]}],\"native\":{\"location\":\"ARA1\",\"initial\":{\"position\":[1,4,3],\"velocity\":[2,5,6],\"heading\":0.7,\"grounded\":false}}}";
        [json writeToFile:path atomically:YES encoding:NSUTF8StringEncoding error:nil];
        auto spec=readNativeReplay(path);initializeNativeReplay(a,spec.source,flat,{0,0,0},{0,1,0},0);
        auto restored=ssx::runReplay(spec.input,a,flat,flat);
        assert(restored[0].position.x==1&&restored[0].position.y==4&&restored[0].position.z==3);
        assert(restored[0].velocity.x==2&&restored[0].velocity.y==5&&restored[0].velocity.z==6&&restored[0].heading==.7&&!restored[0].grounded);
        writeNativeReplay([directory stringByAppendingPathComponent:@"a"],spec,restored);
        writeNativeReplay([directory stringByAppendingPathComponent:@"b"],spec,restored);
        for (NSString* extension in @[@"csv",@"json"]) {
            NSData* x=[NSData dataWithContentsOfFile:[directory stringByAppendingPathComponent:[@"a." stringByAppendingString:extension]]];
            NSData* y=[NSData dataWithContentsOfFile:[directory stringByAppendingPathComponent:[@"b." stringByAppendingString:extension]]];
            assert(x.length && [x isEqualToData:y]);
        }
        for (NSString* bad in @[@"{\"frames\":2,\"events\":[{\"start\":0,\"end\":3}]}",
                                @"{\"frames\":2,\"events\":[{\"start\":0,\"end\":2,\"left\":[127,256]}]}",
                                @"{\"frames\":2,\"events\":[{\"start\":0,\"end\":2,\"buttons\":[\"cross\"]}]}",
                                @"{\"frames\":true}",@"{\"frames\":2.5}"]) {
            [bad writeToFile:path atomically:YES encoding:NSUTF8StringEncoding error:nil];
            bool rejected=false;try {readNativeReplay(path);} catch(const std::exception&) {rejected=true;}assert(rejected);
        }
        // Identical zero words mean cruise-neutral or airborne grab0 according
        // to the running controller. The raw format must not guess a landing.
        NSString* rawJSON=@"{\"frames\":1,\"events\":[],\"native\":{\"accepted_input\":{\"version\":1,\"frames\":1,\"decoding\":\"runtime_control_state\",\"initial_control_state\":5,\"previous_jump_held\":false,\"segments\":[{\"start\":0,\"end\":1,\"word0\":\"0x00000000\",\"word1\":\"0x00000000\"}]}}}";
        [rawJSON writeToFile:path atomically:YES encoding:NSUTF8StringEncoding error:nil];auto rawSpec=readNativeReplay(path);
        assert(rawSpec.input.decodeAcceptedAtRuntime&&rawSpec.input.expectedInitialControlState==5);
        a.reset({0,10,0},{0,1,0},0);bool wrongState=false;
        try{ssx::runReplay(rawSpec.input,a,flat,flat);}catch(const std::exception&){wrongState=true;}assert(wrongState);
        a.grounded=false;
        a.seedOriginalAirControl({},ssx::OriginalAirControlState{},{{0,0,1000},{0,0,0,1}},{0,0,0});
        auto airRaw=ssx::runReplay(rawSpec.input,a,flat,flat);
        assert(airRaw[0].riderControlState==5&&airRaw[1].accepted->controlState==5&&airRaw[1].input.grabMask==1);
        a.reset({0,0,0},{0,1,0},0);rawSpec.input.expectedInitialControlState=0;
        auto cruiseRaw=ssx::runReplay(rawSpec.input,a,flat,flat);
        assert(cruiseRaw[1].accepted->controlState==0&&cruiseRaw[1].input.grabMask==0);
        auto softInput=ssx::originalDecodeCommand(3,(1u<<13)|(1u<<14)|(31u<<15)|(33u<<21),0);
        assert(softInput.boostPressed&&softInput.boostHeld&&softInput.turn>.999f&&softInput.boardPress<-.999f&&!softInput.jumpHeld);
        bool unknownSoft=false;try{ssx::originalDecodeCommand(3,0,1);}catch(const std::exception&){unknownSoft=true;}assert(unknownSoft);
        auto mixed=[rawJSON stringByReplacingOccurrencesOfString:@"\"word0\":" withString:@"\"controls\":{},\"word0\":"];
        [mixed writeToFile:path atomically:YES encoding:NSUTF8StringEncoding error:nil];bool rejectedMixed=false;
        try{readNativeReplay(path);}catch(const std::exception&){rejectedMixed=true;}assert(rejectedMixed);
        [NSFileManager.defaultManager removeItemAtPath:directory error:nil];
        std::puts("Native replay: frame boundaries, repeatability, overlap/axes, all shoulders, pause edges, exact initial state and telemetry passed");
    }
}
