#include "wake_row.hpp"
#include <stdexcept>
namespace ssx {
float originalWakeNoise(const OriginalWakeNoiseTable& table,float phase){
 using namespace terrain_original;Rounding rounding;
 if(!std::isfinite(phase)||phase<0)throw std::runtime_error("Wake phase outside recovered nonnegative domain");
 float scaled=mul(phase,160.f);if(!(scaled<2147483648.f))throw std::runtime_error("Wake index exceeds signed conversion domain");int whole=int(scaled),index=whole%160;
 float fraction=originalScalarSubtract(scaled,float(whole));float other=originalScalarSubtract(1.f,fraction);
 return originalScalarAdd(mul(table[index],other),mul(table[index+1],fraction));
}
float originalWakeOctaves(const OriginalWakeNoiseTable& table,float phase,int octaves){
 using namespace terrain_original;Rounding rounding;float sum=0,last=0,frequency=1,weight=1;
 for(int i=0;i<octaves;i++){last=originalWakeNoise(table,mul(phase,frequency));frequency=originalScalarAdd(frequency,frequency);last=mul(last,weight);weight=mul(weight,.5f);sum=originalScalarAdd(sum,last);}
 // The final source return adds the last weighted sample again.
 return originalScalarAdd(sum,last);
}
OriginalWakeProfile originalWakeProfile(bool deviceBound){
 using namespace terrain_original;Rounding rounding;OriginalWakeProfile p;p.columns=deviceBound?5:4;p.capacity=deviceBound?32:22;p.lifetime=deviceBound?1.25f:.800000011920929f;
 for(unsigned i=0;i<5;i++){
  p.textureV[i]=originalScalarSubtract(.9900000095367432f,mul(float(i),originalScalarDivide(.9800000190734863f,float(p.columns-1))));
  p.verticalIncrement[i]=mul(originalScalarDivide(float(i),float(p.columns-1)),-25.000001907348633f);
 }
 return p;
}
void originalWakeAdvanceCursor(OriginalWakeCursor& state){
 using namespace terrain_original;Rounding rounding;if(state.capacity<=0)throw std::runtime_error("Invalid wake ring capacity");
 state.phase=originalScalarAdd(state.phase,.03333333507180214f);if(state.phase>=4)state.phase=originalScalarSubtract(state.phase,4.f);
 if(state.count<state.capacity)++state.count;state.head=(state.head+state.capacity-1)%state.capacity;
}
OriginalWakeRow originalWakeRow(const OriginalWakeRowInput& input,const OriginalWakeNoiseTable& table){
 using namespace terrain_original;Rounding rounding;if(input.columns!=4&&input.columns!=5)throw std::runtime_error("Unrecovered wake column count");
 OriginalWakeRow row;row.value60=input.value60;
 float variation=originalScalarAdd(mul(originalWakeOctaves(table,input.phase),.25f),1.f);
 float amplitude=mul(input.amplitude90,variation),speed=terrain_original::sqrt(dot(input.velocity,input.velocity));
 for(unsigned k=0;k<3;k++)row.drag50[k]=mul(sub(mul(mul(input.direction80[k],-1.f),speed),mul(input.velocity[k],-1.f)),.8500000238418579f);
 for(int i=0;i<input.columns;i++){
  float fraction=originalScalarDivide(float(i),float(input.columns-1)),square=mul(fraction,fraction);
  if(i==input.columns-1)fraction=mul(fraction,.8999999761581421f);
  for(unsigned k=0;k<3;k++)row.velocity[i][k]=mul(add(mul(mul(input.coefficient70[k],square),.75f),mul(input.coefficient60[k],fraction)),amplitude);
 }
 for(unsigned k=0;k<3;k++)row.anchor[k]=sub(input.point[k],mul(input.normal[k],5.f));
 for(int i=0;i<input.columns;i++)row.positions[i]=row.anchor;
 row.alpha=int32_t(mul(input.alpha94,128.f));return row;
}
}

namespace ssx {
void originalWakeAgeRows(OriginalWakeCursor& cursor,std::span<OriginalWakeRow> rows,int columns,float lifetime,const std::array<float,5>& verticalIncrement){
 using namespace terrain_original;Rounding rounding;
 if(cursor.capacity<=0||size_t(cursor.capacity)>rows.size()||cursor.head<0||cursor.head>=cursor.capacity||cursor.count<0||cursor.count>cursor.capacity||(columns!=4&&columns!=5))throw std::runtime_error("Invalid wake aging layout");
 constexpr float dt=.01666666753590107f;
 for(int ordinal=0;ordinal<cursor.count;ordinal++){
  auto& row=rows[(cursor.head+ordinal)%cursor.capacity];row.age64=originalScalarAdd(row.age64,dt);
  if(lifetime<row.age64){cursor.count=ordinal;break;}
  float dragFactor=std::max(originalScalarSubtract(1.f,mul(row.age64,3.3333332538604736f)),0.f);
  float growth=originalScalarSubtract(1.f,std::min(mul(row.age64,20.f),1.f));growth=mul(growth,row.value60);growth=originalScalarAdd(growth,1.f);
  const float step=mul(growth,dt);Vector dragStep;for(unsigned k=0;k<3;k++)dragStep[k]=mul(mul(row.drag50[k],dragFactor),dt);
  for(unsigned k=0;k<3;k++)row.positions[0][k]=add(row.positions[0][k],dragStep[k]);
  for(int column=1;column<columns;column++){
   for(unsigned k=0;k<3;k++)row.positions[column][k]=add(row.positions[column][k],add(mul(row.velocity[column][k],step),dragStep[k]));
   row.velocity[column][2]=originalScalarAdd(row.velocity[column][2],verticalIncrement[column]);
  }
 }
}
}
