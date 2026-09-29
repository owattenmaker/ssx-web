// Real-game check of the camera director port against an ARMSX2 finish capture
// (tools/ps2_capture_director.py; driver tools/test_camera_director_capture.py).
// Camera record k is written after the outer-camera update of frame k; main record k holds the
// rider state that camera update consumed (verified: DEFAULT_3 +0x1B0 equals main[k] velocity).
#define main camera_unit_test_main
#include "../engine/original_camera_tests.cpp"
#undef main
#include "../engine/original_camera_director.hpp"
#include <cstring>
#include <fstream>
#include <map>
#include <string>

namespace {
constexpr unsigned CAMERA_RECORD=4096,ALG0=224,ALG1=1168,OUTER=2112,NODES=128;
struct Record {const uint8_t* p;uint32_t u(unsigned o)const{uint32_t v;std::memcpy(&v,p+o,4);return v;}float fl(unsigned o)const{return f(u(o));}
    Quad quad(unsigned o)const{return {fl(o),fl(o+4),fl(o+8),fl(o+12)};}};
std::vector<uint8_t> load(const char* path){std::ifstream in(path,std::ios::binary);return {std::istreambuf_iterator<char>(in),{}};}
OriginalChaseAlgorithmState algorithmAt(const Record& r,unsigned at){
    std::array<uint32_t,228> w;std::memcpy(w.data(),r.p+at,sizeof w);
    auto s=fromWords(w);s.variantType=int(r.u(at+0xC));
    s.postRaceDirection=r.quad(at+0x390);s.postRacePhase=int32_t(r.u(at+0x3A0));return s;
}
}

int main(int argc,char** argv){
    if(argc!=4)return 2;
    auto camera=load(argv[1]),rider=load(argv[2]);const unsigned mainRecord=unsigned(std::stoul(argv[3]));
    std::map<uint32_t,Record> mainByTick;
    for(size_t at=0;at+mainRecord<=rider.size();at+=mainRecord){Record r{rider.data()+at};mainByTick.emplace(r.u(4),r);} // first pass (the race before any replay)
    auto input=[&](uint32_t tick,const Quad& head)->std::optional<OriginalCameraInput>{
        auto it=mainByTick.find(tick);if(it==mainByTick.end())return {};const Record& m=it->second;auto riderField=[&](unsigned off){return 32+off-0x100;};
        OriginalCameraInput in;in.headPosition=head;in.velocity=m.quad(riderField(0x1E0));in.riderForward=m.quad(riderField(0x1B0));
        in.previousContactNormal=m.quad(riderField(0x380));in.wallNormal=m.quad(riderField(0x3C0));in.motionMode=int(m.u(16));
        in.boostLevel=m.fl(riderField(0x2FC));in.jumpCharge=m.fl(riderField(0x220));in.surfaceId=int(m.u(riderField(0x438)));in.riderType=int(m.u(riderField(0x434)));
        in.launchValue=m.fl(riderField(0x5A4));in.proximityFlag=m.u(riderField(0x5AC))!=0;in.tick=tick;in.raceRiderCount=6;
        return in;
    };
    const size_t count=camera.size()/CAMERA_RECORD;
    unsigned weightChecks=0,postChecks=0,pushChecks=0,lensChecks=0,bad=0,pushes=0,unlinks=0;int finishIndex=-1;
    for(size_t k=1;k<count;++k){
        Record a{camera.data()+(k-1)*CAMERA_RECORD},b{camera.data()+k*CAMERA_RECORD};
        if(b.u(4)!=a.u(4)+1)continue;                      // replay restarts / repeated ticks
        const unsigned before=a.u(100),after=b.u(100);
        if(before>2||after>2)throw std::runtime_error("More than two camera nodes captured");
        // Director weights: previous nodes (+ a pushed head) advanced by the node pass.
        OriginalCameraDirectorState d;
        for(unsigned i=0;i<before;++i){OriginalCameraDirectorNode n;n.algorithm.variantType=int(a.u(i?ALG1+0xC:ALG0+0xC));n.algorithm.postRacePhase=int32_t(a.u(116+4*i));
            n.weight=a.fl(NODES+32*i+4);n.rate=a.fl(NODES+32*i+8);n.smoothed=a.fl(NODES+32*i+12);d.nodes.push_back(n);}
        const bool pushed=b.u(116)!=a.u(116);
        if(pushed){
            ++pushes;if(finishIndex<0&&b.u(ALG0+0xC)==0x44)finishIndex=int(k);
            OriginalChaseAlgorithmState incoming;incoming.variantType=int(b.u(ALG0+0xC));incoming.postRacePhase=int32_t(b.u(116));
            original_camera::Rounding rounding;original_camera_director::insertAlgorithm(d,incoming,b.fl(NODES+8),[](OriginalChaseAlgorithmState&){});
        }
        {original_camera::Rounding rounding;original_camera_director::advanceNodes(d,[](OriginalCameraDirectorNode&,int){});}
        unlinks+=before+(pushed?1:0)-d.nodes.size();
        bool ok=d.nodes.size()==after;
        for(unsigned i=0;ok&&i<after;++i)ok=uint32_t(d.nodes[i].algorithm.postRacePhase)==b.u(116+4*i)&&u(d.nodes[i].weight)==b.u(NODES+32*i+4)&&u(d.nodes[i].smoothed)==b.u(NODES+32*i+12)&&u(d.nodes[i].rate)==b.u(NODES+32*i+8);
        if(!ok){++bad;std::printf("weights differ at camera record %zu tick %u\n",k,b.u(4));}
        ++weightChecks;
        // Lens blend: raw weights with each algorithm's fov/near (clamped by outer +0x0C/+0x10/+0x14).
        {
            std::vector<OriginalCameraDirectorNode> nodes(after);
            for(unsigned i=0;i<after;++i){unsigned at=i?ALG1:ALG0;nodes[i].weight=b.fl(NODES+32*i+4);nodes[i].algorithm.fov=b.fl(at);nodes[i].algorithm.near=b.fl(at+4);nodes[i].algorithm.far=b.fl(at+8);}
            float fov,near,far;{original_camera::Rounding rounding;original_camera_director::blendLens(nodes,fov,near,far);}
            fov=(0.f<=fov)?std::min(fov,b.fl(OUTER+0x0C)):0.f;near=(b.fl(OUTER+0x10)<=near)?std::min(near,b.fl(OUTER+0x14)):b.fl(OUTER+0x10);
            if(u(fov)!=b.u(OUTER)||u(near)!=b.u(OUTER+4)){++bad;std::printf("fov/near differ at camera record %zu: %08x %08x / %08x %08x\n",k,u(fov),u(near),b.u(OUTER),b.u(OUTER+4));}
            ++lensChecks;
        }
        // POST_RACE_1 while the DEFAULT_3 node still supplies the exact head (+0xA0 = target slot 0x08).
        if(after==2&&b.u(ALG0+0xC)==0x44&&b.u(ALG1+0xC)==0x3D){
            Quad head=b.quad(ALG1+0xA0);auto in=input(b.u(4),head);
            if(!in)continue;
            OriginalChaseAlgorithmState post;
            if(pushed){
                // Finish request 0x162258 -> 0x15D078: ctor, set-target in the push and the factory tail, then the director step.
                original_camera::Rounding rounding;original_camera::postRaceConstruct(post,*in);
                original_camera::setTarget(post,*in);original_camera::setTarget(post,*in);original_camera::stepAlgorithm(post,*in);
                auto expected=algorithmAt(b,ALG0);unsigned words=0;
                visitFields(post,[&](unsigned off,const auto& v){using V=std::remove_cv_t<std::remove_reference_t<decltype(v)>>;uint32_t mine;if constexpr(std::is_same_v<V,float>)mine=u(v);else mine=uint32_t(v);
                    if(mine!=b.u(ALG0+off)){if(words++<6)std::printf("  push +0x%03X original %08x (%g) native %08x (%g)\n",off,b.u(ALG0+off),b.fl(ALG0+off),mine,f(mine));}});
                if(post.postRaceDirection!=expected.postRaceDirection||post.postRacePhase!=expected.postRacePhase)++words;
                if(words){++bad;std::printf("POST_RACE_1 construction differs in %u words at camera record %zu\n",words,k);}
                ++pushChecks;
            } else if(a.u(ALG0+0xC)==0x44){
                post=algorithmAt(a,ALG0);
                {original_camera::Rounding rounding;original_camera::stepAlgorithm(post,*in);}
                bool same=std::memcmp(post.lookAt.data(),b.p+ALG0+0x20,16)==0&&std::memcmp(post.eye.data(),b.p+ALG0+0x40,16)==0&&std::memcmp(post.outputEye.data(),b.p+ALG0+0x60,16)==0&&
                          uint32_t(post.postRacePhase)==b.u(ALG0+0x3A0)&&u(post.yaw)==b.u(ALG0+0x50)&&u(post.pitch)==b.u(ALG0+0x54);
                if(!same){++bad;std::printf("POST_RACE_1 step differs at camera record %zu (phase %d/%u)\n",k,post.postRacePhase,b.u(ALG0+0x3A0));}
                ++postChecks;
            }
        }
    }
    if(finishIndex<0)throw std::runtime_error("Capture holds no POST_RACE_1 finish fade");
    if(bad)throw std::runtime_error("Captured camera director differs in "+std::to_string(bad)+" checks");
    std::printf("Original finish capture: POST_RACE_1 pushed at camera record %d; %u director weight/smoothstep/unlink updates (%u pushes, %u unlinks), %u fov/near blends, "
                "the finish construction (ctor + two set-targets + step, every decoded word) and %u POST_RACE_1 steps (eye, look-at, output eye, angles, phase) match bit for bit.\n",
                finishIndex,weightChecks,pushes,unlinks,lensChecks,postChecks);
}
