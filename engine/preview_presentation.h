#pragma once
// Lightweight visual feedback for the playable preview, independent of physics.
static float3 previewTravel(const ssx::PrototypeRider& rider){
    auto v=nativeVector(rider.velocity);v.y=0;
    return simd_length_squared(v)>.25f?simd_normalize(v):float3{float(std::sin(rider.heading)),0,float(std::cos(rider.heading))};
}
static float4x4 previewRiderTransform(const ssx::PrototypeRider& rider){
    auto up=simd_normalize(nativeVector(rider.grounded?rider.normal:ssx::Vec3{0,1,0}));
    auto front=previewTravel(rider);front=simd_normalize(front-up*simd_dot(front,up));
    auto right=simd_normalize(simd_cross(front,up));auto p=nativeVector(rider.position);
    // Imported characters and board noses point along local -Z, not +Z.
    return {float4{right.x,right.y,right.z,0},float4{up.x,up.y,up.z,0},float4{-front.x,-front.y,-front.z,0},float4{p.x,p.y,p.z,1}};
}
static FollowCamera previewFollowCamera(const ssx::PrototypeRider& rider,const ssx::CollisionWorld& world){
    auto front=previewTravel(rider);auto target=nativeVector(rider.position)+float3{0,1.1f,0}+front*1.3f;
    auto eye=target-front*(6.5f+std::min(float(rider.speed()),35.f)*.045f)+float3{0,2.6f,0};
    auto delta=eye-target;auto hit=world.raycast({target.x,target.y,target.z},{delta.x,delta.y,delta.z},simd_length(delta));
    if(hit.hit)eye=target+simd_normalize(delta)*float(std::max(.7,hit.distance-.3));return {target,eye};
}
