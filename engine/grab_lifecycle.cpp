#include "grab_lifecycle.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <cfenv>
#include <stdexcept>
#include <bit>
namespace ssx {
float originalGrabPlaybackRate(float stat){OriginalRounding rounding;float rate=originalScalarAdd(terrain_original::mul(stat,.29988324642181396f),1.f);return rate;}
float originalGrabLegWeight(float previous,int animationClass){
    OriginalRounding rounding;
    constexpr float step=std::bit_cast<float>(0x3d4cccceu);bool disable=uint32_t(animationClass)-20u<7u;
    float upper=disable?step:std::bit_cast<float>(0x3f866666u),lower=disable?-step:std::bit_cast<float>(0x3f733333u);
    float result=previous>upper?originalScalarSubtract(previous,step):previous<lower?originalScalarAdd(previous,step):disable?0.f:1.f;
    return result;
}
OriginalGrabResult originalGrabLifecycle(OriginalGrabState&s,const OriginalGrabProfile&p,
        int requested,bool tweak,const OriginalGrabAnimationAccess&a,const OriginalGrabContext& context){
    if(requested<-1||requested>=15||s.index<-1||s.index>=15||s.state<0||s.state>5)return {false,false};
    if((s.state==3||s.state==4)&&!p.extendedDefinitions)return {false,false};
    const unsigned tier=context.boostTier>=5;OriginalGrabResult result;
    auto current=[&]()->const OriginalGrabDefinition&{if(s.index<0)throw std::runtime_error("Active original grab has no index");return p.grabs.at(s.index);};
    auto extended=[&]()->const OriginalGrabDefinition&{if(s.index<0)throw std::runtime_error("Active advanced grab has no index");return a.mainClass()==19?p.tweak.at(s.index):p.uber[tier].at(s.index);};
    auto score=[&](const OriginalGrabDefinition& definition,bool begin,bool ordinary){
        if(a.mappedScore&&definition.scoreId>=0)a.mappedScore(definition.scoreId,begin);
        else if(ordinary&&a.score)a.score(s.index,begin);
        else throw std::runtime_error("Original advanced grab score callback/definition missing");
    };
    auto upper=[&](const OriginalGrabDefinition& definition){if(definition.upperSemantic!=438){a.play(definition.upperSemantic,false);a.setRate(1,p.playbackRate);}};
    if(s.state==0){
        if(requested==-1){if(s.index!=-1){a.setRate(2,-1);a.play(287,false);s.index=-1;}}
        else if(s.index!=requested){s.index=requested;bool force=a.mainClass()==18;a.play(current().semantic,force);a.setRate(2,p.playbackRate);}
        else if(a.mainFlags()&1)s.state=1;
    }
    if(s.state==1&&(a.mainFlags()&2)){
        score(current(),true,true);a.setRate(2,0);upper(current());s.state=2;
    }
    if(s.state==2){
        if(requested!=s.index){score(current(),false,true);a.fade(1,.33000001311302185f);a.setRate(2,p.playbackRate);s.state=5;}
        else if(tweak){
            if(!p.extendedDefinitions)return {false,false};
            a.fade(1,.10000000149011612f);a.fade(2,.10000000149011612f);
            bool uber=context.superTime!=0&&context.uberEnabled&&p.uber[tier].at(s.index).semantic!=438;
            a.play(uber?p.uber[tier].at(s.index).semantic:p.tweak.at(s.index).semantic,false);
            result.advancedStarted=true;if(a.advancedStarted)a.advancedStarted();
            a.setRate(2,p.playbackRate);s.state=3;
        }
    }
    if(s.state==3){
        if(requested==s.index&&tweak&&(a.mainFlags()&2)){
            a.setRate(2,0);const auto& definition=extended();score(definition,true,false);upper(definition);s.state=4;
        }else if((a.mainFlags()&2)||(a.mainFlags()&8)||(a.mainFlags()&4)){
            const auto&definition=extended();score(definition,true,false);score(definition,false,false);s.state=5;
        }
    }
    if(s.state==4&&(requested!=s.index||!tweak)){
        score(extended(),false,false);a.fade(1,.33000001311302185f);a.setRate(2,p.playbackRate);s.state=5;
    }
    int cls=a.mainClass();
    if(s.state==5&&(cls==2||(a.mainFlags()&8))){
        bool restart=false;
        if(cls==20&&tweak&&requested!=-1){
            if(!p.extendedDefinitions)return {false,false};
            if(p.uber[tier].at(requested).semantic!=438){
                s.index=requested;a.fade(1,.33000001311302185f);a.play(p.uber[tier].at(s.index).semantic,true);a.setRate(2,p.playbackRate);s.state=3;restart=true;
            }
        }
        if(!restart){s.state=0;s.index=-1;}
    }
    result.active=cls>=18&&cls<=20;return result;
}
}
