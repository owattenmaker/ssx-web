// Behavioural tests of the camera director port (engine/original_camera_director.hpp).
// Instruction-level conformance is tools/test_camera_director_native.py.
#include "original_camera_director.hpp"
#include <bit>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
namespace {
using namespace ssx;
using Quad=OriginalCameraQuad;
uint32_t u(float x){return std::bit_cast<uint32_t>(x);}
bool same(const Quad& a,const Quad& b){return std::memcmp(a.data(),b.data(),16)==0;}

// Synthetic straight run on flat ground (cm, Z up) with one jump.
OriginalCameraInput frame(int i){
    OriginalCameraInput in;
    const float x=float(i)*30.f;
    const bool air=i>=100&&i<140;
    const float z=air?float((i-100)*(140-i))*0.6f:0.f;
    in.headPosition={x,0,z+150,1};in.velocity={1800,float(i%7)*3.f,air?float(1200-(i-100)*60):0.f,0};
    in.riderForward={1,0,0,0};in.previousContactNormal={0,0,1,0};
    in.motionMode=air?1:0;in.jumpCharge=air?0.f:.25f;in.tick=uint32_t(1000+i);in.raceRiderCount=6;
    return in;
}
}

int main(){
    // 1. One DEFAULT_3 node at weight 1 is the exact copy path: identical to the single-algorithm step.
    {
        OriginalCameraState single;single.begin(frame(0));
        OriginalDirectedCameraState directed;
        directed.director.nodes.push_back({single.algorithm,1.f,1.f,1.f});
        for(int i=1;i<300;++i){
            auto a=originalChaseCameraStep(single,frame(i));
            auto b=originalDirectedCameraStep(directed,frame(i));
            assert(same(a.eye,b.eye)&&same(a.lookAt,b.lookAt)&&u(a.fov)==u(b.fov)&&u(a.near)==u(b.near)&&u(a.far)==u(b.far));
        }
    }
    // 2. Begin = outer ctor request 0x4C: one preferred node at weight 1.
    OriginalDirectedCameraState camera;
    originalDirectedCameraBegin(camera,frame(0),0x3D);
    assert(camera.director.nodes.size()==1&&camera.director.nodes[0].weight==1.f&&camera.director.nodes[0].rate==1.f);
    assert(camera.director.currentType==0x3D);
    for(int i=1;i<200;++i)originalDirectedCameraStep(camera,frame(i));
    // 3. Finish: POST_RACE_1 pushed at weight 0, rate 0.016793445; the old node fades out in 60 updates.
    originalDirectedCameraFinish(camera,frame(200));
    assert(camera.director.nodes.size()==2&&camera.director.nodes[0].algorithm.variantType==0x44);
    assert(camera.director.nodes[0].weight==0.f&&camera.director.currentType==0x44);
    // ctor phase 0, set-target runs twice (two updates each) -> phase 4 before the first director step.
    assert(camera.director.nodes[0].algorithm.postRacePhase==4);
    int removedAt=-1;OriginalCameraOutput out;
    for(int k=1;k<=80;++k){
        out=originalDirectedCameraStep(camera,frame(200+k));
        const auto& nodes=camera.director.nodes;
        if(nodes.size()==2){
            float sum=nodes[0].smoothed+nodes[1].smoothed;assert(std::fabs(sum-1.f)<1e-5f);
            assert(nodes[0].weight>0.f&&nodes[0].weight<1.f);
        } else if(removedAt<0)removedAt=k;
    }
    std::printf("DEFAULT_3 node unlinked at director update %d after the finish request\n",removedAt);
    assert(removedAt==60);
    assert(camera.director.nodes.size()==1&&camera.director.nodes[0].weight==1.f);
    // POST_RACE_1 geometry in a 6-rider race: look-at 3 cm below the head, eye 326 cm back, 123 cm up.
    {
        const auto& post=camera.director.nodes[0].algorithm;
        Quad head=frame(280).headPosition;
        assert(std::fabs(post.lookAt[2]-(head[2]-3.f))<1e-3f);
        float dx=post.eye[0]-post.lookAt[0],dy=post.eye[1]-post.lookAt[1];
        assert(std::fabs(std::sqrt(dx*dx+dy*dy)-326.f)<0.05f);
        assert(std::fabs(post.eye[2]-post.lookAt[2]-123.f)<1e-3f);
        assert(u(out.fov)==u(post.fov));
    }
    // 4. Restart: instant cut back to the preferred algorithm, lift cleared, first-frame flag set.
    camera.compositor.lift=12.f;
    originalDirectedCameraRestart(camera,frame(300));
    assert(camera.director.nodes.size()==1&&camera.director.nodes[0].algorithm.variantType==0x3D&&camera.director.nodes[0].weight==1.f);
    assert(camera.compositor.lift==0.f&&camera.compositor.firstFrameAfterReset==1&&camera.director.flags==0);
    // 5. Camera select cuts instantly; SPOKE requests are counted as unsupported (not ported).
    originalDirectedCameraSelect(camera,frame(301),0x3E);
    assert(camera.director.nodes.size()==1&&camera.director.nodes[0].algorithm.variantType==0x3E&&camera.director.preferredType==0x3E);
    auto hand=frame(302);hand.motionMode=5;hand.velocity={100,0,0,0};
    originalDirectedCameraStep(camera,hand);
    assert(camera.director.spokeLatch==1&&(camera.director.flags&4)&&camera.director.unsupportedRequests==1&&camera.director.lastUnsupportedType==0x42);
    std::puts("Camera director: exact single-node path, 60-update finish fade, POST_RACE_1 geometry, restart and select pass.");
}
