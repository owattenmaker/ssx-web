#include "ps2_runtime_macros.h"
#include "../engine/feedback_queue.hpp"
#include <cstring>
#include <random>
#include <cstdio>
void sub_002B1458_0x2b1458(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002B1720_0x2b1720(uint8_t*,R5900Context*,PS2Runtime*);
void sub_002B1428_0x2b1428(uint8_t*,R5900Context*,PS2Runtime*);
extern const uint32_t g_ps2RecompiledFunctionTableBase=0x100000,g_ps2RecompiledFunctionTableEnd=0x420000,g_ps2RecompiledFunctionTableSlotCount=0xc8000;
PS2Runtime::RecompiledFunction g_ps2RecompiledFunctionTable[0xc8000]={};
int main(){static_assert(sizeof(ssx::OriginalFeedbackEntry)==32);std::vector<uint8_t>m(32*1024*1024);PS2Runtime rt;std::mt19937 random(0x2b1458);unsigned accepted=0;
 for(unsigned n=0;n<20000;n++){
  ssx::OriginalFeedbackQueue queue;
  for(auto&e:queue)e={uint32_t(random()%3),int32_t(random()%400)-10,uint32_t(random()%4),uint32_t(random()%9),uint32_t(random()),uint32_t(random()),float(int(random()%100)-50)*.125f,uint32_t(random())};
  if(n%7==0)queue[0].ticks=INT32_MIN;if(n%17==0)queue[1].ticks=INT32_MAX;
  if(n%11==0)for(auto&e:queue)e.active=1;
  if(n%13==0)for(auto&e:queue)e.active=0;
  uint32_t group=random()%4,id=random()%9,a=random(),b=random(),d=random();float scalar=float(int(random()%100)-50)*.125f;bool refresh=n%2;
  std::memcpy(m.data()+0x10018,queue.data(),sizeof(queue));R5900Context c{};c.pc=0x2b1458;SET_GPR_U32(&c,4,0x10000);SET_GPR_U32(&c,5,group);SET_GPR_U32(&c,6,id);SET_GPR_U32(&c,7,a);SET_GPR_U32(&c,8,b);SET_GPR_U32(&c,9,d);SET_GPR_U32(&c,10,refresh);SET_GPR_U32(&c,31,0x12345678);c.f[12]=scalar;
  sub_002B1458_0x2b1458(m.data(),&c,&rt);bool result=ssx::originalFeedbackEnqueue(queue,group,id,a,b,scalar,d,refresh);
  if(c.pc!=0x12345678||GPR_U32((&c),2)!=uint32_t(result)||std::memcmp(m.data()+0x10018,queue.data(),sizeof(queue))){printf("Feedback queue mismatch %u\n",n);return 1;}accepted+=result;
  c.pc=0x2b1720;SET_GPR_U32(&c,4,0x10000);sub_002B1720_0x2b1720(m.data(),&c,&rt);ssx::originalFeedbackTick(queue);
  if(c.pc!=0x12345678||std::memcmp(m.data()+0x10018,queue.data(),sizeof(queue))){printf("Feedback tick mismatch %u\n",n);return 2;}
  if(n%3==0){c.pc=0x2b1428;SET_GPR_U32(&c,4,0x10000);sub_002B1428_0x2b1428(m.data(),&c,&rt);ssx::originalFeedbackClear(queue);if(c.pc!=0x12345678||std::memcmp(m.data()+0x10018,queue.data(),sizeof(queue)))return 3;}

 }
 printf("20,000 original feedback enqueue/tick cases plus6,667 clears match all slots and return values; %u accepted\n",accepted);
}
