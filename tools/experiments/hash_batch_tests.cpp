#include "texture_hash_batch.hpp"
#include <cassert>
int main(){
 aurora::gfx::gxcore::TextureHashBatch batch;unsigned calls=0;unsigned char data[64]{};
 auto hash=[&](const void* p,size_t n,unsigned long long seed){++calls;auto b=static_cast<const unsigned char*>(p);for(size_t i=0;i<n;++i)seed=seed*31+b[i];return seed;};
 batch.begin();auto first=batch.get(data,64,1,hash);assert(batch.get(data,64,1,hash)==first&&calls==1);
 batch.get(data,32,1,hash);batch.get(data,64,2,hash);assert(calls==3);
 batch.begin();batch.get(data,64,1,hash);batch.end();assert(calls==3);
 batch.invalidate();data[0]=1;assert(batch.get(data,64,1,hash)!=first&&calls==4);
 batch.end();data[0]=2;auto changed=batch.get(data,64,1,hash);assert(calls==5);batch.get(data,64,1,hash);assert(calls==6);
 batch.begin();assert(batch.get(data,64,1,hash)==changed&&calls==7);batch.end();
}
