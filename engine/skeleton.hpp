#pragma once
#include <simd/simd.h>
#include <functional>
#include <cmath>
#include <stdexcept>
#include <vector>
#include <array>

namespace ssx {
// Recovered from SSX3 USA PS2 0x30EFE0..0x30F108, followed by the
// per-bone signed component mapping at 0x30F10C..0x30F1F0.
// These are quarter-turn spherical coordinates with the game's parabolic
// sine approximation, not Euler angles or raw quaternion XYZ components.
inline simd::float4 afbQuaternion(simd::float3 coordinates,const std::array<uint8_t,4>& mapping) {
    float s[3],c[3];
    for (unsigned i=0;i<3;++i) {
        if (!std::isfinite(coordinates[i])) throw std::runtime_error("Nonfinite AFB rotation");
        double a=std::fmod(double(coordinates[i]),4.0);if (a<0) a+=4;
        unsigned quadrant=unsigned(a);
        double r=a-2*std::floor(a*.5);
        s[i]=float(r*(2-r))*((quadrant&2)?-1:1);
        c[i]=std::sqrt(std::max(0.f,1-s[i]*s[i]))*(((quadrant+1)&2)?-1:1);
    }
    simd::float4 raw={c[0],s[0]*s[1]*c[2],s[0]*c[1],s[0]*s[1]*s[2]},mapped;
    for (unsigned i=0;i<4;++i) {
        if (mapping[i]>7) throw std::runtime_error("Invalid AFB quaternion component map");
        mapped[i]=raw[mapping[i]&3]*(mapping[i]<4?1:-1);
    }
    // Apply the same Z-up to Y-up basis change as the imported bind mesh.
    return simd_normalize(simd::float4{mapped.x,mapped.z,-mapped.y,mapped.w});
}
// Original mirror translation multiplication at PS2 0x30EE80..0x30EEBC,
// followed by centimeters->meters and the mesh's Z-up->Y-up basis conversion.
inline simd::float3 afbTranslation(simd::float3 coordinates,simd::float3 mirrorScale={1,1,1}) {
    auto p=coordinates*mirrorScale;
    return {p.x/100,p.z/100,-p.y/100};
}
struct Bone {
    int parent=-1;
    simd::float3 translation={0,0,0};
    simd::float4 rotation={0,0,0,1};
};
class Skeleton {
    std::vector<Bone> bones;
    std::vector<simd::float4x4> inverseBind;
    std::vector<simd::float4x4> globals(const std::vector<simd::float4>& adjustments,simd::float3 rootOffset={0,0,0},bool absolute=false,const std::vector<simd::float3>& rootOffsets={},const std::vector<simd::float3>& translations={}) const {
        if (!adjustments.empty() && adjustments.size()!=bones.size()) throw std::runtime_error("Pose size mismatch");
        if (!rootOffsets.empty() && rootOffsets.size()!=bones.size()) throw std::runtime_error("Root offset count mismatch");
        if (!translations.empty() && translations.size()!=bones.size()) throw std::runtime_error("Translation count mismatch");
        std::vector<simd::float4x4> result(bones.size());
        std::vector<uint8_t> state(bones.size());
        std::function<void(size_t)> visit=[&](size_t i) {
            if (state[i]==2) return;
            if (state[i]==1) throw std::runtime_error("Cyclic skeleton");
            state[i]=1;
            const auto& b=bones[i];
            simd_quatf q=simd_quaternion(b.rotation);
            if (!adjustments.empty()) q=absolute ? simd_quaternion(adjustments[i]) : simd_mul(q,simd_quaternion(adjustments[i]));
            simd::float4x4 local=simd::float4x4(simd_normalize(q));
            auto t=translations.empty()?b.translation:translations[i];
            local.columns[3]=simd::float4{t.x,t.y,t.z,1};
            if (b.parent>=0) {
                if (size_t(b.parent)>=bones.size()) throw std::runtime_error("Skeleton parent outside bone array");
                visit(b.parent);result[i]=result[b.parent]*local;
            } else {
                auto offset=rootOffsets.empty()?rootOffset:rootOffsets[i];
                local.columns[3]+=simd::float4{offset.x,offset.y,offset.z,0};
                result[i]=local;
            }
            state[i]=2;
        };
        for (size_t i=0;i<bones.size();++i) visit(i);
        return result;
    }
public:
    explicit Skeleton(std::vector<Bone> input):bones(std::move(input)) {
        if (bones.empty() || bones.size()>256) throw std::runtime_error("Invalid skeleton size");
        for (const auto& b:bones) {
            bool finite=true;
            for (int i=0;i<4;++i) finite=finite&&std::isfinite(b.rotation[i]);
            for (int i=0;i<3;++i) finite=finite&&std::isfinite(b.translation[i]);
            if (b.parent< -1 || !finite || simd_length_squared(b.rotation)<1e-8f) throw std::runtime_error("Invalid bone transform");
        }
        inverseBind=globals({});
        for (auto& m:inverseBind) m=simd_inverse(m);
    }
    std::vector<simd::float4x4> palette(const std::vector<simd::float4>& adjustments={},simd::float3 rootOffset={0,0,0},bool absolute=false,const std::vector<simd::float3>& rootOffsets={},const std::vector<simd::float3>& translations={}) const {
        auto result=globals(adjustments,rootOffset,absolute,rootOffsets,translations);
        for (size_t i=0;i<result.size();++i) result[i]=result[i]*inverseBind[i];
        return result;
    }
    std::vector<simd::float4x4> paletteFromGlobals(std::vector<simd::float4x4> transforms) const {
        if(transforms.size()!=bones.size())throw std::runtime_error("World pose size mismatch");
        for(size_t i=0;i<transforms.size();++i)transforms[i]=transforms[i]*inverseBind[i];
        return transforms;
    }
    size_t size() const { return bones.size(); }
    std::vector<simd::float3> restTranslations() const {
        std::vector<simd::float3> result;
        for (const auto& bone:bones) result.push_back(bone.translation);
        return result;
    }
    std::vector<simd::float4> restRotations() const {
        std::vector<simd::float4> result;
        for (const auto& b:bones) result.push_back(b.rotation);
        return result;
    }
};
}
