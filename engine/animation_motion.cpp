#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include "animation_motion.hpp"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {
namespace {
using Round=OriginalRounding;
AnimationVector cross(AnimationVector a,AnimationVector b) {
    AnimationVector r;for(unsigned i=0;i<3;++i){unsigned j=(i+1)%3,k=(i+2)%3;float p=terrain_original::mul(a[j],b[k]),q=terrain_original::mul(b[j],a[k]);r[i]=terrain_original::sub(p,q);}return r;
}
}
AnimationQuaternion originalAnimationQuaternion(AnimationVector coordinates) {
    Round round;float s[3],c[3];
    for(unsigned i=0;i<3;++i) {
        float x=coordinates[i];if(!std::isfinite(x)||std::abs(x)>100000)throw std::runtime_error("Invalid animation rotation");
        int quadrant=int(x);if(x<=0)--quadrant;
        float half=float(quadrant>>1);float r=terrain_original::sub(x,half);r=terrain_original::sub(r,half);
        float a=terrain_original::sub(r,1.f);a=terrain_original::sub(a,1.f);float negative=terrain_original::mul(a,r);
        s[i]=(quadrant&2)?negative:terrain_original::sub(0.f,negative);
        float squared=terrain_original::mul((-negative),negative);float radicand=terrain_original::add(squared,1.f);
        // PCSX2 recSQRT_S_xmm forces nearest independently of FPUFPCR.
        c[i]=originalScalarSqrt(std::abs(radicand));if((quadrant+1)&2)c[i]=terrain_original::sub(0.f,c[i]);
    }
    float common=terrain_original::mul(s[0],s[1]);return {c[0],terrain_original::mul(common,c[2]),terrain_original::mul(s[0],c[1]),terrain_original::mul(common,s[2])};
}
AnimationTransform originalAnimationCompose(const AnimationTransform& parent,const AnimationTransform& local,AnimationVector scale) {
    Round round;const auto&a=parent.rotation;const auto&b=local.rotation;
    AnimationVector av{a[0],a[1],a[2]},bv{b[0],b[1],b[2]},p;
    for(unsigned i=0;i<3;++i)p[i]=terrain_original::mul(local.position[i],scale[i]);
    auto v=cross(av,bv);AnimationTransform result;
    for(unsigned i=0;i<3;++i){float x=terrain_original::mul(a[i],b[3]),y=terrain_original::mul(b[i],a[3]);x=terrain_original::add(x,y);result.rotation[i]=terrain_original::add(x,v[i]);}
    float w=terrain_original::mul(a[3],b[3]),x=terrain_original::mul(a[0],b[0]),y=terrain_original::mul(a[1],b[1]),z=terrain_original::mul(a[2],b[2]);w=terrain_original::sub(w,x);w=terrain_original::sub(w,y);result.rotation[3]=terrain_original::sub(w,z);
    auto first=cross(av,p),second=cross(av,first);
    for(unsigned i=0;i<3;++i){float term=terrain_original::mul(first[i],a[3]);float r=terrain_original::add(p[i],term);r=terrain_original::add(r,term);r=terrain_original::add(r,second[i]);r=terrain_original::add(r,second[i]);result.position[i]=terrain_original::add(r,parent.position[i]);}
    return result;
}
AnimationPacket::AnimationPacket(std::span<const uint8_t> data,unsigned frames,bool bigEndian):frames_(frames) {
    if(data.empty()||!frames||frames>65535||!data[0])throw std::runtime_error("Empty animation packet");
    channels.resize(data[0]);size_t at=1+(channels.size()+1)/2;
    auto byte=[&](size_t i){if(i>=data.size())throw std::runtime_error("Truncated animation packet");return data[i];};
    auto floating=[&](){uint32_t bits=uint32_t(byte(at))<<8|uint32_t(byte(at+1))<<16|uint32_t(byte(at+2))<<24;at+=3;float f=std::bit_cast<float>(bits);if(!std::isfinite(f))throw std::runtime_error("Nonfinite animation coefficient");return f;};
    for(size_t i=0;i<channels.size();++i){auto&c=channels[i];c.type=(byte(1+i/2)>>(i%2?0:4))&15;if(c.type>8)throw std::runtime_error("Unsupported animation packet type");unsigned count=c.type<=4?c.type:c.type==5?3:2;for(unsigned j=0;j<count;++j)c.parameters[j]=floating();if(c.type==5){c.parameters[3]=float(byte(at)|(unsigned(byte(at+1))<<8));at+=2;}}
    for(unsigned type=6;type<=8;++type){bool present=false;for(auto&c:channels)present|=c.type==type;if(!present)continue;if(type==8)at=(at+1)&~size_t(1);unsigned rows=type==7?frames/2+1:frames;for(unsigned row=0;row<rows;++row)for(auto&c:channels)if(c.type==type){uint16_t v=byte(at++);if(type==8){uint16_t second=byte(at++);v=bigEndian?(v<<8)|second:v|(second<<8);}c.samples.push_back(v);}}
    if(at>data.size()||data.size()-at>3)throw std::runtime_error("Animation packet extent mismatch");
}
std::vector<float> AnimationPacket::sample(float frame) const {
    Round round;if(!std::isfinite(frame))throw std::runtime_error("Nonfinite animation frame");frame=std::clamp(frame,0.f,float(frames_-1));std::vector<float> result;result.reserve(channels.size());
    // Source gp-3228 = tiny interpolation snap threshold.
    constexpr float snap=std::bit_cast<float>(0x3a83126fu);
    for(const auto&c:channels){const auto&p=c.parameters;float value=0;
        if(c.type<=4){if(c.type){value=p[0];for(unsigned j=1;j<c.type;++j){value=terrain_original::mul(value,frame);value=originalScalarAdd(value,p[j]);}}}
        else if(c.type==5){if(frame<=p[3]){value=terrain_original::mul(p[0],frame);value=originalScalarAdd(value,p[1]);}else{value=terrain_original::mul(p[0],p[3]);value=originalScalarAdd(value,p[1]);float delta=originalScalarSubtract(frame,p[3]),term=terrain_original::mul(p[2],delta);value=originalScalarAdd(value,term);}}
        else{float clock=c.type==7?terrain_original::mul(frame,.5f):frame;unsigned index=unsigned(clock);float fraction=originalScalarSubtract(clock,float(index));if(fraction<snap)fraction=0;float q=float(c.samples.at(index));if(fraction){float difference=originalScalarSubtract(float(c.samples.at(index+1)),q);float increment=terrain_original::mul(fraction,difference);q=originalScalarAdd(q,increment);}value=terrain_original::mul(q,p[1]);value=originalScalarAdd(value,p[0]);}
        result.push_back(value);
    }return result;
}
}
namespace ssx {
std::vector<float> AnimationClip::sample(int part,float seconds)const {
    Round round;float frame=terrain_original::mul(seconds,30.f);
    const AnimationPacket* last=nullptr;
    for(const auto&s:segments)if(s.part==part){
        if(last){if(frame<float(last->frames()-1))return last->sample(frame);frame=originalScalarSubtract(frame,float(last->frames()-1));}
        last=&s.packet;
    }
    // At an overlapping segment boundary, original3113C0 selects the NEXT
    // packet's first row. Quantization can make it differ from the prior last row.
    return last?last->sample(frame):std::vector<float>{};
}
std::vector<AnimationTransform> originalAnimationLocalPose(const std::vector<AnimationBone>& bones,const std::vector<AnimationClip>& clips,const std::vector<AnimationLayer>& layers,uint64_t activeBoneMask) {
    Round round;std::vector<AnimationTransform> result;for(const auto&b:bones)result.push_back(b.bind);
    // AnimationClip::sample(part,time) decodes every channel of the part's packet and is a pure function of its
    // arguments; each bone reads only its own channels, so one sample per (layer, part) serves every bone.
    std::vector<std::vector<std::pair<int,std::vector<float>>>> sampled(layers.size());
    // A layer's clip, looked up by id (a scan of ~500 clips) on the first bone that uses the layer, then reused: the
    // lookup is pure, and a missing clip still throws at that first use (docs/sim-performance.md "Pose clip lookup").
    std::vector<const AnimationClip*> layerClip(layers.size(),nullptr);
    for(size_t i=0;i<bones.size();++i){if(i>=64||!(activeBoneMask&(uint64_t(1)<<i)))continue;float covered=0,total=0;int priority=layers.empty()?0:layers.front().priority;
        for(const auto&layer:layers){if(layer.priority>priority)throw std::runtime_error("Animation priorities must descend");if(layer.priority<priority){float remaining=originalScalarSubtract(1.f,covered);remaining=terrain_original::mul(remaining,total);covered=std::min(1.f,originalScalarAdd(covered,remaining));total=0;priority=layer.priority;}
            if(i>=64||!(layer.mask&(uint64_t(1)<<i))||layer.weight<.03f)continue;
            const AnimationClip*& clip=layerClip[size_t(&layer-layers.data())];
            if(!clip){auto found=std::find_if(clips.begin(),clips.end(),[&](const auto&c){return c.id==layer.clip;});if(found==clips.end())throw std::runtime_error("Missing original animation clip");clip=&*found;}
            auto& cached=sampled[size_t(&layer-layers.data())];const std::vector<float>* found=nullptr;
            for(const auto& entry:cached)if(entry.first==bones[i].part){found=&entry.second;break;}
            if(!found){cached.emplace_back(bones[i].part,clip->sample(bones[i].part,layer.time));found=&cached.back().second;}
            const auto& scalar=*found;if(scalar.empty())continue;
            float remaining=originalScalarSubtract(1.f,covered);float numerator=terrain_original::mul(remaining,layer.weight);total=originalScalarAdd(total,layer.weight);
            // Original30F628 uses EE DIV.S (PCSX2's separate nearest mode).
            float weight=originalScalarDivide(numerator,total);
            if(weight<.03f)continue;if(weight>=.97f)weight=1;
            auto pose=bones[i].bind;const auto&source=layer.mirror?bones.at(bones[i].mirrorSource):bones[i];int t=source.translationChannel,q=source.rotationChannel;
            if(t>=0){if(size_t(t+3)>scalar.size())throw std::runtime_error("Animation translation channel extent");pose.position={scalar[t],scalar[t+1],scalar[t+2]};}
            if(q>=0){if(size_t(q+3)>scalar.size())throw std::runtime_error("Animation rotation channel extent");pose.rotation=originalAnimationQuaternion({scalar[q],scalar[q+1],scalar[q+2]});}
            if(layer.mirror){if(t>=0)for(unsigned k=0;k<3;++k)pose.position[k]=terrain_original::mul(pose.position[k],bones[i].mirrorTranslation[k]);if(q>=0){auto raw=pose.rotation;for(unsigned k=0;k<4;++k){unsigned component=bones[i].mirrorQuaternion[k];if(component>7)throw std::runtime_error("Invalid original quaternion mirror map");pose.rotation[k]=component>=4?-raw[component&3]:raw[component];}}}
            if(bones[i].parent<0){auto transformed=originalAnimationCompose(layer.root,pose);if(t>=0)pose.position=transformed.position;if(q>=0)pose.rotation=transformed.rotation;}
            if(weight==1){result[i]=pose;continue;}
            if(t>=0)for(unsigned k=0;k<3;++k){float old=result[i].position[k],sub=terrain_original::mul(old,weight),add=terrain_original::mul(pose.position[k],weight);old=terrain_original::sub(old,sub);result[i].position[k]=terrain_original::add(old,add);}
            if(q>=0){float dot=0;for(unsigned k=0;k<4;++k){float product=terrain_original::mul(result[i].rotation[k],pose.rotation[k]);dot=terrain_original::add(dot,product);}float squared=0;for(unsigned k=0;k<4;++k){float old=result[i].rotation[k],sub=terrain_original::mul(old,weight),add=terrain_original::mul((dot<0?-pose.rotation[k]:pose.rotation[k]),weight);old=terrain_original::sub(old,sub);result[i].rotation[k]=terrain_original::add(old,add);float product=terrain_original::mul(result[i].rotation[k],result[i].rotation[k]);squared=terrain_original::add(squared,product);}float norm=terrain_original::div(1.f,terrain_original::sqrt(squared));for(float&v:result[i].rotation)v=terrain_original::mul(v,norm);}
        }
    }return result;
}
// 30F2B0 per layer: the layer's weight in the slot's group is the bones' (0x30F604..0x30F664: (1 - covered) x w / total,
// < 0.03 -> skipped, >= 0.97 -> 1), over the layers whose clip has the part (0x30F4B0) and whose mask has the slot bit.
// Weight 1 samples straight into the weights (311318, channels 0..count) and marks the part written (sp+0x820);
// a smaller weight blends: out = (1 - w) out + w s (sub.s, mul.s, mul.s, add.s at 0x30F944..0x30F970).
// A mirrored layer (+0x40) samples into a scratch row and takes channel mirror[i] (part+0x40, 0x30F898).
// A part no layer wrote at weight 1 is cleared (0x3100A4..0x3100D0: memset of the part's weights).
std::vector<float> originalAnimationMorphWeights(const std::vector<AnimationClip>& clips,const std::vector<AnimationLayer>& layers,int part,unsigned slotBit,unsigned count,const std::vector<uint8_t>& mirror) {
    Round round;
    std::vector<float> out(count,0.f);
    if(slotBit>=64||!count)return out;
    bool written=false;
    float covered=0;
    float total=0;
    int priority=layers.empty()?0:layers.front().priority;
    for(const auto& layer:layers){
        if(layer.priority>priority)throw std::runtime_error("Animation priorities must descend");
        if(layer.priority<priority){
            float remaining=originalScalarSubtract(1.f,covered);
            remaining=terrain_original::mul(remaining,total);
            covered=std::min(1.f,originalScalarAdd(covered,remaining));
            total=0;
            priority=layer.priority;
        }
        if(!(layer.mask&(uint64_t(1)<<slotBit))||layer.weight<.03f)continue;
        const auto found=std::find_if(clips.begin(),clips.end(),[&](const auto& c){return c.id==layer.clip;});
        if(found==clips.end())throw std::runtime_error("Missing original animation clip");
        const auto scalar=found->sample(part,layer.time);
        if(scalar.empty())continue;
        float remaining=originalScalarSubtract(1.f,covered);
        float numerator=terrain_original::mul(remaining,layer.weight);
        total=originalScalarAdd(total,layer.weight);
        float weight=originalScalarDivide(numerator,total);
        if(weight<.03f)continue;
        if(weight>=.97f)weight=1;
        std::vector<float> sampled(count,0.f);
        for(unsigned i=0;i<count;++i){
            const unsigned channel=layer.mirror&&i<mirror.size()?mirror[i]:i;
            if(channel>=scalar.size())throw std::runtime_error("Animation morph channel extent");
            sampled[i]=scalar[channel];
        }
        if(weight==1){
            out=sampled;
            written=true;
            continue;
        }
        const float keep=originalScalarSubtract(1.f,weight);
        for(unsigned i=0;i<count;++i){
            const float old=terrain_original::mul(keep,out[i]);
            const float add=terrain_original::mul(weight,sampled[i]);
            out[i]=originalScalarAdd(old,add);
        }
    }
    if(!written)std::fill(out.begin(),out.end(),0.f);
    return out;
}
std::vector<AnimationTransform> originalAnimationWorldPose(const std::vector<AnimationBone>& bones,const std::vector<AnimationTransform>& local,AnimationTransform root,AnimationVector scale,const std::vector<AnimationTransform>& roots) {
    if(bones.size()!=local.size())throw std::runtime_error("Animation pose extent");std::vector<AnimationTransform> world;size_t rootIndex=0;
    for(size_t i=0;i<bones.size();++i){int parent=bones[i].parent;if(parent>=int(i))throw std::runtime_error("Animation bones must be parent ordered");world.push_back(originalAnimationCompose(parent<0?(roots.empty()?root:roots.at(rootIndex++)):world[parent],local[i],scale));}return world;
}
}
