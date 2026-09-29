#include "terrain_contact_math.hpp"
#include "snow_emission.hpp"
#include "original_float.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <algorithm>
#pragma STDC FENV_ACCESS ON
namespace ssx {namespace {
using Round=OriginalRounding;
SnowVector scale(SnowVector v,float s){for(auto&x:v)x=terrain_original::mul(x,s);return v;}
SnowVector add(SnowVector a,SnowVector b){for(unsigned i=0;i<3;++i)a[i]=terrain_original::add(a[i],b[i]);return a;}
}
float OriginalSnowRandom::next(){Round rounding;word=((word*0x18fcdu+0xe9507cu)&0x7fffffu)|0x3f800000u;return originalScalarSubtract(std::bit_cast<float>(word),1.f);}
OriginalSnowRiderCache originalSnowRiderCache(const OriginalSnowRiderInput&i){
    Round round;OriginalSnowRiderCache out;
    if(i.manualState!=0){bool front=(i.animationSemantic>=23&&i.animationSemantic<=28)||i.animationSemantic==37||i.animationSemantic==38;out.edgeBias=(front!=i.reverse)?1.f:-1.f;}
    out.turnAmount=terrain_original::mul(-i.turn,originalScalarSubtract(1.f,std::abs(i.brake)));if(i.reverse)out.turnAmount=-out.turnAmount;
    float square=terrain_original::mul(i.velocityCmps[0],i.velocityCmps[0]);square=terrain_original::add(square,terrain_original::mul(i.velocityCmps[1],i.velocityCmps[1]));square=terrain_original::add(square,terrain_original::mul(i.velocityCmps[2],i.velocityCmps[2]));square=terrain_original::add(square,0.f);out.speedCmps=terrain_original::sqrt(square);out.direction=i.velocityCmps;
    if(out.speedCmps>0){float inverse=terrain_original::div(1.f,out.speedCmps);for(auto&v:out.direction)v=terrain_original::mul(v,inverse);}
    out.groundEmission=i.motionMode!=1&&i.motionMode!=2&&i.motionMode!=4&&i.controlState!=9;
    if(i.trackingInhibited&&(!i.trackingAFC||i.trackingAD0||!i.trackingB00))out.groundEmission=false;return out;
}
void originalSnowBoardJitter(SnowVector&position,const OriginalSnowBoardFrame&board,float bias,OriginalSnowRandom&random){
    Round rounding;float span=terrain_original::mul(originalScalarSubtract(1.f,std::abs(bias)),55.f);
    float center=terrain_original::mul(bias,55.f),lo=-span;float offset=terrain_original::mul(originalScalarSubtract(span,lo),random.next());offset=originalScalarAdd(lo,offset);offset=originalScalarAdd(center,offset);
    position=add(position,scale(board.right,offset));
    float vertical=terrain_original::mul(random.next(),20.f);vertical=originalScalarAdd(vertical,-10.f);position=add(position,scale(board.up,vertical));
}
OriginalSnowEmission originalSnowTrailEmission(const OriginalSnowTrailProfile&p,const OriginalSnowTrailContext&c,OriginalSnowRandom&random){
    Round rounding;OriginalSnowEmission result;result.positionCm=c.board.originCm;
    if(!c.groundEmission||!(222.22222900390625f<c.absoluteSpeedCmps)||!c.surfaceActive||c.suppressed)return result;
    result.active=true;result.positionCm=add(c.board.originCm,scale(c.board.forward,15.f));originalSnowBoardJitter(result.positionCm,c.board,c.edgeBias,random);
    auto velocity=add(scale(c.velocityCmps,p.velocityScale),scale(c.groundNormal,p.normalScale));
    velocity=add(velocity,scale(scale(c.groundNormal,p.normalSpeedScale),c.absoluteSpeedCmps));result.velocityCmps=velocity;result.colour=c.colour;return result;
}
OriginalSnowImpactEmission originalSnowImpactEmission(OriginalSnowImpactState&s,const OriginalSnowImpactContext&c,OriginalSnowRandom&random){
    Round rounding;OriginalSnowImpactEmission out;
    if(!(27.77777862548828f<s.strength)||!c.smallActive){out.births[0].emitter=6;out.births[1].emitter=5;for(auto&b:out.births)b.positionCm=c.boardOriginCm;s.strength=0;return out;}
    bool large=c.largeActive&&277.77777099609375f<s.strength;
    out.requestSecondaryImpact=large&&!c.trackingInhibited;
    float scaleFactor=large?1.f:.5f;auto position=add(s.positionCm,scale(s.normal,large?45.f:20.f));
    float amount=std::clamp(originalScalarDivide(s.strength,3333.33349609375f),0.f,1.f);
    if(s.buildup<1||large){
        if(c.motionMode==2){if(s.buildup<2){float x=terrain_original::mul(originalScalarAdd(amount,amount),scaleFactor);s.buildup=std::min(originalScalarAdd(s.buildup,x),2.f);}}
        else if(!s.kind){if(s.buildup<1.5f){float x=terrain_original::mul(scaleFactor,1.5f);x=terrain_original::mul(amount,x);s.buildup=std::min(originalScalarAdd(s.buildup,x),2.f);}}
        else if(s.buildup<1){float x=terrain_original::mul(amount,scaleFactor);s.buildup=std::min(originalScalarAdd(s.buildup,x),2.f);}
    }
    float scatter=c.largeActive&&s.wideScatter?60.f:25.f;float span=originalScalarSubtract(scatter,-scatter);
    SnowVector jitter{originalScalarAdd(-scatter,terrain_original::mul(span,random.next())),originalScalarAdd(-scatter,terrain_original::mul(span,random.next())),0};position=add(position,jitter);
    float dot=terrain_original::mul(c.velocityCmps[0],c.groundNormal[0]);dot=terrain_original::add(dot,terrain_original::mul(c.velocityCmps[1],c.groundNormal[1]));dot=terrain_original::add(dot,terrain_original::mul(c.velocityCmps[2],c.groundNormal[2]));dot=terrain_original::add(dot,0.f);dot=terrain_original::mul(dot,.75f);
    auto normal=scale(c.groundNormal,dot),velocity=c.velocityCmps;for(unsigned i=0;i<3;++i)velocity[i]=terrain_original::sub(velocity[i],normal[i]);velocity=scale(velocity,s.kind?.30000001192092896f:.4000000059604645f);
    float target=s.kind?.5f:1.f;if(s.alpha<target)s.alpha=target;else s.alpha=originalScalarAdd(terrain_original::mul(s.alpha,.800000011920929f),terrain_original::mul(target,.19999998807907104f));
    auto&birth=out.births[0];birth.emitter=large?5:6;birth.active=true;birth.positionCm=position;birth.velocityCmps=velocity;birth.colour=c.colour;birth.colour->at(3)=s.alpha;
    out.births[1].emitter=large?6:5;out.births[1].positionCm=s.positionCm;
    if(s.strength<83.33333587646484f)s.strength=0;else{float decay=c.decay;if(!s.wideScatter)decay=terrain_original::mul(decay,.4000000059604645f);s.strength=terrain_original::mul(s.strength,decay);}
    return out;
}
OriginalSnowEmission originalSnowCloudEmission(const OriginalSnowTrailProfile&p,const OriginalSnowCloudContext&cloud,OriginalSnowRandom&random){
    Round round;const auto&c=cloud.trail;OriginalSnowEmission out;out.emitter=7;out.positionCm=c.board.originCm;
    float dot=terrain_original::mul(cloud.lateral[0],cloud.direction[0]);dot=terrain_original::add(dot,terrain_original::mul(cloud.lateral[1],cloud.direction[1]));dot=terrain_original::add(dot,terrain_original::mul(cloud.lateral[2],cloud.direction[2]));dot=terrain_original::add(dot,0.f);
    float slip=std::abs(dot);slip=originalScalarAdd(slip,slip);slip=std::abs(std::min(slip,1.f));
    float speed=originalScalarSubtract(c.absoluteSpeedCmps,416.66668701171875f);speed=std::min(originalScalarDivide(speed,1250.f),1.f);
    float chance=originalScalarSubtract(originalScalarAdd(std::abs(cloud.turn),slip),.10000000149011612f);speed=terrain_original::mul(speed,speed);chance=terrain_original::mul(chance,speed);
    float roll=terrain_original::mul(random.next(),.4000000059604645f);
    if(!c.groundEmission||!(roll<=chance)||!c.surfaceActive||!(std::abs(c.edgeBias)<.20000000298023224f))return out;
    float factor=originalScalarAdd(random.next(),.5f);float height=terrain_original::mul(factor,cloud.maxHeightCm);
    auto forwardOffset=scale(c.velocityCmps,3.f);forwardOffset=scale(forwardOffset,.01666666753590107f);forwardOffset=scale(forwardOffset,factor);
    auto offset=scale(c.groundNormal,height);for(unsigned i=0;i<3;++i)offset[i]=terrain_original::sub(offset[i],forwardOffset[i]);out.positionCm=add(c.board.originCm,offset);
    SnowVector cross{terrain_original::sub(terrain_original::mul(c.board.right[1],c.groundNormal[2]),terrain_original::mul(c.board.right[2],c.groundNormal[1])),terrain_original::sub(terrain_original::mul(c.board.right[2],c.groundNormal[0]),terrain_original::mul(c.board.right[0],c.groundNormal[2])),terrain_original::sub(terrain_original::mul(c.board.right[0],c.groundNormal[1]),terrain_original::mul(c.board.right[1],c.groundNormal[0]))};if(cloud.turnAmount<0)cross=scale(cross,-1.f);
    auto longitudinal=cloud.reverse?scale(c.board.right,-1.f):c.board.right;auto backward=scale(longitudinal,-1.f);backward=scale(backward,c.absoluteSpeedCmps);auto negativeVelocity=scale(c.velocityCmps,-1.f);for(unsigned i=0;i<3;++i)backward[i]=terrain_original::sub(backward[i],negativeVelocity[i]);backward=scale(backward,.30000001192092896f);
    auto velocity=add(scale(c.velocityCmps,p.velocityScale),scale(scale(cross,c.absoluteSpeedCmps),0.f));velocity=add(velocity,scale(c.groundNormal,p.normalScale));velocity=add(velocity,scale(scale(c.groundNormal,p.normalSpeedScale),c.absoluteSpeedCmps));velocity=add(velocity,backward);
    out.active=true;out.velocityCmps=velocity;out.colour=c.colour;return out;
}
std::array<OriginalSnowEmission,2> originalSnowChunkEmission(const std::array<OriginalSnowChunkProfile,2>&profiles,const OriginalSnowChunkContext&chunk,OriginalSnowRandom&random){
    Round round;const auto&cloud=chunk.cloud;const auto&c=cloud.trail;std::array<OriginalSnowEmission,2> out;
    for(unsigned i=0;i<2;++i){out[i].emitter=2-i;out[i].positionCm=c.board.originCm;}
    if(55.55555725097656f<chunk.impact.strength&&!chunk.impact.kind){
        for(auto&birth:out){birth.active=true;birth.positionCm=chunk.impact.positionCm;if(chunk.motionMode==2){SnowVector jitter{originalScalarAdd(terrain_original::mul(random.next(),110.f),-55.f),originalScalarAdd(terrain_original::mul(random.next(),110.f),-55.f),0};birth.positionCm=add(birth.positionCm,jitter);}
            float amount=std::min(originalScalarDivide(c.absoluteSpeedCmps,2.5199999809265137f),1.f);amount=terrain_original::mul(amount,amount);if(!chunk.largeImpactActive)amount=terrain_original::mul(amount,.4000000059604645f);amount=terrain_original::mul(amount,std::min(originalScalarDivide(chunk.impact.strength,833.3333740234375f),1.f));
            SnowVector jitter{originalScalarAdd(terrain_original::mul(random.next(),1000.f),-500.f),originalScalarAdd(terrain_original::mul(random.next(),1000.f),-500.f),terrain_original::mul(random.next(),500.f)};auto velocity=add(scale(chunk.impact.normal,1200.f),jitter);velocity=scale(velocity,amount);velocity=add(velocity,scale(c.velocityCmps,.75f));birth.velocityCmps=velocity;birth.colour=c.colour;
        }return out;
    }
    if(!c.groundEmission||!(chunk.chanceScales[0]>.009999999776482582f||chunk.chanceScales[1]>.009999999776482582f)){for(auto&birth:out)birth.velocityCmps=SnowVector{};return out;}
    float dot=terrain_original::mul(cloud.lateral[0],cloud.direction[0]);dot=terrain_original::add(dot,terrain_original::mul(cloud.lateral[1],cloud.direction[1]));dot=terrain_original::add(dot,terrain_original::mul(cloud.lateral[2],cloud.direction[2]));dot=terrain_original::add(dot,0.f);
    float slip=std::min(terrain_original::mul(std::abs(dot),1.5f),1.f);float turn=originalScalarSubtract(std::abs(cloud.turnAmount),-.20000000298023224f);turn=terrain_original::mul(turn,.800000011920929f);turn=std::clamp(originalScalarAdd(turn,slip),0.f,1.f);
    float brake=terrain_original::mul(originalScalarSubtract(std::abs(chunk.brake),.20000000298023224f),3.f);float secondary=originalScalarSubtract(std::abs(chunk.secondaryBrake274),.4000000059604645f);brake=std::max(brake,secondary);
    bool carve=brake<turn;float speed=originalScalarSubtract(c.absoluteSpeedCmps,carve?555.5555419921875f:416.66668701171875f);speed=originalScalarDivide(speed,carve?1944.4444580078125f:833.3333740234375f);speed=std::clamp(speed,0.f,1.2000000476837158f);float chance=terrain_original::mul((carve?turn:brake),speed);
    SnowVector side{terrain_original::sub(terrain_original::mul(c.board.right[1],c.groundNormal[2]),terrain_original::mul(c.board.right[2],c.groundNormal[1])),terrain_original::sub(terrain_original::mul(c.board.right[2],c.groundNormal[0]),terrain_original::mul(c.board.right[0],c.groundNormal[2])),terrain_original::sub(terrain_original::mul(c.board.right[0],c.groundNormal[1]),terrain_original::mul(c.board.right[1],c.groundNormal[0]))};if(cloud.turnAmount<0)side=scale(side,-1.f);
    for(unsigned i=0;i<2;++i){auto&birth=out[i];const auto&p=profiles[birth.emitter-1];originalSnowBoardJitter(birth.positionCm,c.board,c.edgeBias,random);float probability=terrain_original::mul(chance,chunk.chanceScales[i]);birth.active=random.next()<probability;SnowVector velocity;
        if(carve&&chunk.wakeVelocity){const auto&w=*chunk.wakeVelocity;float square=terrain_original::mul(w[0],w[0]);square=terrain_original::add(square,terrain_original::mul(w[1],w[1]));square=terrain_original::add(square,terrain_original::mul(w[2],w[2]));square=terrain_original::add(square,0.f);float magnitude=terrain_original::sqrt(square);
            float factor=originalScalarSubtract(chunk.maxWakeScale,chunk.minWakeScale);factor=originalScalarAdd(chunk.minWakeScale,terrain_original::mul(factor,random.next()));float lateralFactor=originalScalarAdd(terrain_original::mul(random.next(),.800000011920929f),-.30000001192092896f);
            velocity=scale(w,factor);auto lateral=scale(side,magnitude);lateral=scale(lateral,factor);lateral=scale(lateral,lateralFactor);velocity=add(velocity,lateral);
            float length=originalScalarAdd(terrain_original::mul(random.next(),-.10000000149011612f),.05000000074505806f);length=terrain_original::mul(length,magnitude);auto extra=add(scale(c.board.right,length),scale(c.groundNormal,terrain_original::mul(magnitude,.8399999737739563f)));velocity=add(velocity,extra);
        }else if(carve){velocity=add(scale(c.velocityCmps,p.velocityScale),scale(c.groundNormal,p.normalScale));velocity=add(velocity,scale(scale(c.groundNormal,p.normalSpeedScale),c.absoluteSpeedCmps));auto lateral=scale(side,std::abs(cloud.turnAmount));lateral=scale(lateral,p.sideScale);lateral=scale(lateral,c.absoluteSpeedCmps);velocity=add(velocity,lateral);
        }else{float factor=std::min(originalScalarDivide(c.absoluteSpeedCmps,1250.f),1.f);SnowVector jitter{originalScalarAdd(terrain_original::mul(random.next(),400.f),-200.f),originalScalarAdd(terrain_original::mul(random.next(),400.f),-200.f),terrain_original::mul(random.next(),200.f)};velocity=add(scale(c.velocityCmps,.800000011920929f),scale(scale(c.groundNormal,1100.f),factor));velocity=add(velocity,scale(jitter,factor));}
        birth.velocityCmps=velocity;birth.colour=c.colour;
    }return out;
}
}

namespace ssx {
OriginalSnowEmission originalBreathEmission(OriginalBreathState&s,const OriginalSnowTrailProfile&p,const OriginalBreathContext&c,OriginalSnowRandom&r){
    OriginalRounding rounding;using namespace terrain_original;
    OriginalSnowEmission e;e.emitter=4;
    for(unsigned i=0;i<3;i++)e.positionCm[i]=add(add(add(mul(c.headMatrix[0][i],-3.f),mul(c.headMatrix[1][i],0.f)),mul(c.headMatrix[2][i],4.f)),c.headMatrix[3][i]);
    if(c.speedCmps>1111.111083984375f){s.accumulator=0;return e;}
    float target=c.animationClass==30?1.f:0.f;
    s.effort=s.effort<=target?target:originalScalarSubtract(s.effort,.0016666668234393f);
    s.clock=originalScalarAdd(s.clock,.01666666753590107f);
    if(s.clock>=s.duration){s.clock=0;s.phase^=1;s.duration=s.phase?.4000000059604645f:originalScalarAdd(mul(originalScalarSubtract(1.f,s.effort),.6000000238418579f),.4000000059604645f);}
    float alpha=mul(originalScalarAdd(s.effort,.5f),originalScalarSubtract(1.f,mul(originalScalarAdd(c.environmentValue,5.f),.03999999910593033f)));
    alpha=alpha>=0?std::min(alpha,2.f):0.f;
    if(!s.phase||!(alpha>.019999999552965164f)){s.accumulator=0;return e;}
    s.accumulator=originalScalarAdd(s.accumulator,mul(originalScalarAdd(mul(r.next(),6.f),18.f),.01666666753590107f));
    if(!(s.accumulator>1.f))return e;
    s.accumulator=originalScalarSubtract(s.accumulator,1.f);e.active=true;e.colour=SnowColour{1,1,1,alpha};
    SnowVector velocity;
    for(unsigned i=0;i<3;i++){
        float base=add(add(mul(c.velocityCmps[i],p.velocityScale),mul(c.groundNormal[i],p.normalScale)),mul(mul(c.groundNormal[i],p.normalSpeedScale),c.speedCmps));
        //2E13D4 replaces translation with4FF130=(0,0,0,1) before this transform.
        float direction=add(add(add(mul(c.headMatrix[0][i],0.f),mul(c.headMatrix[1][i],0.f)),mul(c.headMatrix[2][i],150.f)),0.f);
        velocity[i]=add(base,direction);
    }
    e.velocityCmps=velocity;return e;
}
}

namespace ssx {
OriginalSnowEmission originalRockSprayEmission(const OriginalSnowChunkProfile&p,const OriginalSnowCloudContext&cloud,float brake,float chance,OriginalSnowRandom&random){
 Round rounding;const auto&c=cloud.trail;OriginalSnowEmission e;e.emitter=3;e.positionCm=c.board.originCm;
 if(!c.groundEmission)return e;
 if(!(random.next()<chance))return e;
 e.positionCm=add(c.board.originCm,scale(c.board.forward,15));originalSnowBoardJitter(e.positionCm,c.board,c.edgeBias,random);
 using namespace terrain_original;
 float skid=add(add(mul(cloud.lateral[0],cloud.direction[0]),mul(cloud.lateral[1],cloud.direction[1])),mul(cloud.lateral[2],cloud.direction[2]));skid=add(skid,0.f);skid=std::min(mul(std::abs(skid),1.5f),1.f);
 auto clamp=[](float x){return x>=0?std::min(x,1.f):0.f;};
 float braking=clamp(mul(originalScalarSubtract(std::abs(brake),.20000000298023224f),3.f));
 float turning=originalScalarAdd(mul(originalScalarSubtract(std::abs(cloud.turnAmount),-.20000000298023224f),.800000011920929f),skid);
 float rate=std::max(mul(braking,.30000001192092896f),clamp(turning));
 float speed=clamp(mul(originalScalarSubtract(mul(c.absoluteSpeedCmps,.035999998450279236f),20.f),.01666666753590107f));
 rate=mul(rate,speed);e.active=random.next()<rate;
 SnowVector side={sub(mul(c.board.right[1],c.groundNormal[2]),mul(c.board.right[2],c.groundNormal[1])),sub(mul(c.board.right[2],c.groundNormal[0]),mul(c.board.right[0],c.groundNormal[2])),sub(mul(c.board.right[0],c.groundNormal[1]),mul(c.board.right[1],c.groundNormal[0]))};
 if(cloud.turnAmount<0)side=scale(side,-1);
 auto velocity=add(scale(c.velocityCmps,p.velocityScale),scale(c.groundNormal,p.normalScale));velocity=add(velocity,scale(scale(c.groundNormal,p.normalSpeedScale),c.absoluteSpeedCmps));velocity=add(velocity,scale(scale(side,p.sideScale),c.absoluteSpeedCmps));
 e.velocityCmps=velocity;e.colour=c.colour;return e;
}
}

namespace ssx {
OriginalSnowEmission originalSnowKickerEmission(float&buildup,const OriginalSnowTrailProfile&p,const OriginalSnowTrailContext&c,float surfaceCapacity,OriginalSnowRandom&random){
 Round rounding;OriginalSnowEmission e;e.emitter=8;e.positionCm=c.board.originCm;
 bool accumulating=c.groundEmission&&!(surfaceCapacity<.009999999776482582f);
 if(accumulating)buildup=std::min(originalScalarAdd(buildup,.010000000707805157f),surfaceCapacity);
 else buildup=std::max(originalScalarSubtract(buildup,.01666666753590107f),0.f);
 if(!c.groundEmission&&c.velocityCmps[2]<-3333.33349609375f)buildup=0;
 if(c.absoluteSpeedCmps<277.77777099609375f)buildup=0;
 if(accumulating||!(buildup>0))return e;
 auto clamp=[](float x){return x>=0?std::min(x,1.f):0.f;};
 float alpha=clamp(terrain_original::mul(buildup,1.25f));
 if(c.velocityCmps[2]<0)alpha=terrain_original::mul(alpha,clamp(originalScalarDivide(originalScalarAdd(c.velocityCmps[2],3333.33349609375f),3333.33349609375f)));
 auto colour=c.colour;colour[3]=alpha;
 auto velocity=add(scale(c.velocityCmps,p.velocityScale),scale(c.board.forward,p.normalScale));velocity=add(velocity,scale(scale(c.board.forward,p.normalSpeedScale),c.absoluteSpeedCmps));
 originalSnowBoardJitter(e.positionCm,c.board,c.edgeBias,random);e.velocityCmps=velocity;e.colour=colour;e.active=true;return e;
}
}
