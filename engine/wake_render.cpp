#include "wake_render.hpp"
#include <algorithm>
#include <stdexcept>
namespace ssx {
OriginalWakeVisual originalWakeVisual(std::array<float,4> colour,int32_t counter){
 using namespace terrain_original;Rounding rounding;OriginalWakeVisual out;
 for(unsigned i=0;i<4;i++)out.environment[i]=std::clamp(originalScalarAdd(colour[i],colour[i]),0.f,1.f);
 float u=mul(float(counter),.5f);float block=float(int32_t(mul(u,.015625f)));out.u=originalScalarSubtract(u,originalScalarAdd(mul(block,64.f),32.f));
 for(unsigned i=0;i<3;i++)out.rgb[i]=uint32_t(int32_t(mul(out.environment[i+1],128.f)));return out;
}
OriginalWakeDrawWindow originalWakeDrawWindow(const OriginalWakeCursor& cursor,std::span<const OriginalWakeRow> rows,float lifetime){
 using namespace terrain_original;Rounding rounding;if(cursor.count==0)return {};
 if(cursor.capacity<=0||size_t(cursor.capacity)>rows.size()||cursor.count<0||cursor.count>cursor.capacity||cursor.head<0||cursor.head>=cursor.capacity||!(lifetime>0))throw std::runtime_error("Invalid wake draw window");
 const auto& oldest=rows[(cursor.head+cursor.count-1)%cursor.capacity];
 float remaining=originalScalarDivide(originalScalarSubtract(lifetime,oldest.age64),lifetime);
 float tail=remaining<=.800000011920929f?mul(remaining,1.25f):1.f;
 OriginalWakeDrawWindow out;out.start=cursor.head;out.count=std::min(cursor.count,cursor.capacity-2);out.step=originalScalarDivide(originalScalarSubtract(tail,1.25f),float(cursor.count));return out;
}
}
