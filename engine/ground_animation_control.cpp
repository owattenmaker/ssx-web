#include "ground_animation_control.hpp"
#include "original_float.hpp"
#include "terrain_contact_math.hpp"
#include <cfenv>
#include <cmath>
#include <bit>
#include <stdexcept>
#pragma STDC FENV_ACCESS ON
namespace ssx {
int originalPrewindAnimation(float spin,float flip,bool reverseStance,int style){
    if(style<0||style>=5)return -1;
    static constexpr int choices[9][5]={{245,245,245,263,258},{247,255,255,265,260},{253,253,253,265,260},{248,256,256,266,262},{252,252,252,264,259},{251,251,251,265,260},{249,257,257,267,261},{250,250,250,264,259},{246,254,254,264,259}};
    OriginalRounding rounding;
    float magnitude=std::max(std::abs(spin),std::abs(flip));float x=reverseStance?-spin:spin,y=-flip,angle=0;
    if(x==0){if(y!=0)angle=y>=0?std::bit_cast<float>(0x3fc90fdbu):std::bit_cast<float>(0xbfc90fdbu);}
    else {angle=originalAtan(originalScalarDivide(y,x));if(x<0)angle=y>0?originalScalarAdd(angle,std::bit_cast<float>(0x40490fdbu)):originalScalarSubtract(angle,std::bit_cast<float>(0x40490fdbu));}
    unsigned row=0;
    if(magnitude!=0){
        if(std::abs(angle)>std::bit_cast<float>(0x402fede0u))row=1;
        else if(angle>std::bit_cast<float>(0x3ffb53d3u))row=2;
        else if(angle>std::bit_cast<float>(0x3f96cbe5u))row=3;
        else if(angle>std::bit_cast<float>(0x3ec90fdcu))row=4;
        else if(angle<std::bit_cast<float>(0xbffb53d3u))row=5;
        else if(angle<std::bit_cast<float>(0xbf96cbe5u))row=6;
        else if(angle<std::bit_cast<float>(0xbec90fdcu))row=7;
        else row=8;
    }
    return choices[row][style];
}
int originalAirReleaseAnimation(float spin,float flip,bool reverseStance,int style){
    // All45 cells of43D788->43D840 give this unambiguous mapping. Reuse the
    // verified angle classifier rather than introducing different boundaries.
    constexpr int release[]={268,269,270,271,272,273,274,275,276,269,270,271,272,277,278,279,280,281,282,283,284,285,286};
    int prewind=originalPrewindAnimation(spin,flip,reverseStance,style);
    return prewind<245||prewind>267?-1:release[prewind-245];
}
float originalAirReleaseAnimationRate(float trickStat,float spinRate,float flipRate){
    OriginalRounding rounding;
    float value=originalScalarAdd(terrain_original::mul(trickStat,std::bit_cast<float>(0x3f0010fcu)),1.f);
    value=terrain_original::mul(value,std::max(spinRate,flipRate));value=terrain_original::mul(value,std::bit_cast<float>(0x3e124716u));
    return value;
}
bool originalSelectGroundAnimation(const OriginalGroundProfile&p,OriginalGroundState&s,const RiderInput&input,float prewindSpin,float prewindFlip,const std::function<uint32_t()>& random,bool reverseTurnTriggered){
    OriginalRounding rounding;
    constexpr float controlRate=std::bit_cast<float>(0x3d088889u);
    int desired=s.animationIndex;
    if(s.controlState==2){
        desired=originalPrewindAnimation(prewindSpin,prewindFlip,s.reverseStance,s.prewindStyle);
        if(desired<0){s.animationSelectionSupported=false;return false;}
        s.animationIndex=desired;s.animationClass=12;s.animationSelectionSupported=true;return true;
    }
    if(s.controlState!=0){s.animationSelectionSupported=false;return false;}
    if(s.animationIndex==22){
        s.animationTurn.target=s.brake.target=s.crouch.target=0;
        s.animationTurn.rate=s.brake.rate=s.crouch.rate=controlRate;
    }else if(s.animationIndex==21||(s.manualSpin==0&&reverseTurnTriggered)){
        desired=21;s.animationTurn.target=s.brake.target=0;s.animationTurn.rate=s.brake.rate=controlRate;
    }else if(s.animationClass==10){
        s.animationTurn.target=0;s.animationTurn.rate=controlRate;
    }else if(s.animationClass==5){
        s.animationTurn.target=s.turn.current;s.brake.target=0;s.animationTurn.rate=s.brake.rate=controlRate;
    }else{
        float x=terrain_original::mul(s.velocity[0],s.velocity[0]),y=terrain_original::mul(s.velocity[1],s.velocity[1]),z=terrain_original::mul(s.velocity[2],s.velocity[2]);float squared=terrain_original::add(x,y);squared=terrain_original::add(squared,z);squared=terrain_original::add(squared,0.f);float speed=terrain_original::sqrt(squared);
        if(s.brake.current==0){
            s.animationTurn.target=s.turn.current;s.animationTurn.rate=controlRate;
            if(speed<std::bit_cast<float>(0x44505556u)&&(input.crouch>0||std::abs(input.turn)>std::bit_cast<float>(0x3e4ccccdu)))desired=22;
            else{
                float turn=std::abs(s.turn.current),limit=terrain_original::mul(originalScalarSubtract(1.f,turn),.5f);
                if(p.surface.id==4){
                    if(s.crouch.current<=limit)desired=14;
                    else if(s.boost>0&&turn<std::bit_cast<float>(0x3e99999au))desired=8;
                    else desired=15;
                }else{
                    bool bob=false;
                    if(std::abs(s.animationTurn.current)>std::bit_cast<float>(0x3f19999au)&&std::abs(s.lateral[2])>std::bit_cast<float>(0x3f333333u)){
                        if(!random)throw std::runtime_error("Original ground bob selection requires shared RNG");
                        if(!(random()&1)){float lean=s.reverseStance?-s.animationTurn.current:s.animationTurn.current;desired=lean<0?16:17;bob=true;}
                    }
                    if(!bob){
                        if(s.crouch.current<=limit)desired=5;
                        else if(s.boost>0)desired=8;
                        else if(speed<std::bit_cast<float>(0x44d05556u)||p.surface.id==2||p.surface.id==3)desired=7;
                        else desired=6;
                    }
                }
            }
        }else{
            s.animationTurn.target=0;s.animationTurn.rate=controlRate;
            float threshold=std::bit_cast<float>(s.animationIndex==12||s.animationIndex==13?0x44730e39u:0x442d9c72u);
            if(speed>threshold||std::abs(s.brake.current)<std::bit_cast<float>(0x3f4ccccdu))desired=11;
            else desired=(s.brake.current>0)!=s.reverseStance?13:12;
        }
    }
    if(desired!=s.animationIndex){s.animationIndex=desired;s.animationClass=desired==16||desired==17?5:7;}
    s.animationSelectionSupported=true;return true;
}
}
