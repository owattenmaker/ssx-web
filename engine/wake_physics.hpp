#pragma once
#include "wake_row.hpp"
#include "wake_control.hpp"
#include "wake_render.hpp"
#include <optional>
namespace ssx {
struct OriginalWakePhysics {
 OriginalWakeProfile profile;OriginalWakeCursor cursor;OriginalWakeControlState control;
 std::array<OriginalWakeRow,32> rows{};OriginalWakeNoiseTable noise{};
 void reset(bool deviceBound){profile=originalWakeProfile(deviceBound);cursor={};cursor.capacity=profile.capacity;control={};rows={};}
 void step(const OriginalWakeFrameInput& input,int motionMode,float roll274,std::array<float,4> environmentARGB={.5f,1,1,1}){
  originalWakeAgeRows(cursor,rows,profile.columns,profile.lifetime,profile.verticalIncrement);
  auto target=originalWakeTargets(input);OriginalWakeControlCallbacks callbacks;
  callbacks.advanceRow=[&]{originalWakeAdvanceCursor(cursor);auto visual=originalWakeVisual(environmentARGB,control.counter40);rows[cursor.head].textureU=visual.u;rows[cursor.head].rgb=visual.rgb;++control.counter40;};
  callbacks.createRow=[&](auto point,float value){OriginalWakeRowInput row;row.coefficient60=control.normal60;row.coefficient70=control.side70;row.direction80=control.direction80;row.velocity=input.velocity;row.point=point;row.normal=input.normal;row.amplitude90=control.amplitude90;row.alpha94=control.alpha94;row.value60=value;row.phase=cursor.phase;row.columns=profile.columns;const float u=rows[cursor.head].textureU;const auto rgb=rows[cursor.head].rgb;rows[cursor.head]=originalWakeRow(row,noise);rows[cursor.head].textureU=u;rows[cursor.head].rgb=rgb;};
  originalWakeControl(control,target,motionMode,roll274,callbacks);
 }
 std::optional<terrain_original::Vector> tip()const{if(cursor.count==0)return {};return rows[cursor.head].velocity[profile.columns-1];}
};
}
