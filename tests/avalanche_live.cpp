// Live avalanche playback parity (development oracle; tools/test_avalanche_live.py): engine/avalanche.hpp against the
// full recompiled original on PS2 savestates of EBA3 / ERA5 (docs/avalanche.md).
//  1. Running slots: the port world is built from the state's own memory (the avalanche / group lists gp+2496, the slot
//     table 0x538938, the tumbler pool 0x4EE770); machine A runs 0x2D7EF8 per active slot and tick, the port
//     originalAvalancheSlotTick. Compared per tick: every tumbler's group, sample index, integrated position / rotation,
//     position, rotation, scale, alpha, colour (bit for bit) and each slot's t, cursors and array; the 0x2D9C00
//     AvaSpline matrix of every group instance.
//  2. Triggers: for every avalanche not playing in the state, A runs 0x2D97A8(id) and the port originalAvalancheTrigger
//     (+96 from the instance +0x40), then both step TICKS ticks, compared as above.
// Args: FOLDER TICKS STATE...   (STATE = prefix of .ee/.vuc/.vud in FOLDER)
#include "live_registrations.inc"
#include "original_live_runtime.hpp"
#include "../engine/avalanche.hpp"
#include <map>
using namespace ssx;
static constexpr uint32_t GP=0x4A30F0,SLOTS=0x538938,POOL=0x4EE770,TUMBLER=0x2F0;
static uint32_t bits(float x){return std::bit_cast<uint32_t>(x);}
// Sound (0x29DEF0 the loop on / off, 0x29E560 a play) reach the IOP library: returns here (the port records them apart).
static void returnToCaller(uint8_t*,R5900Context* c,PS2Runtime*){c->pc=GPR_U32(c,31);}
static void stubSound(live::Machine& A){A.runtime.registerFunction(0x29DEF0,returnToCaller);A.runtime.registerFunction(0x29E560,returnToCaller);}
struct Built {OriginalAvalancheWorld w;std::map<uint32_t,std::pair<int,int>> groupOf;std::map<uint32_t,int> defOf;std::vector<uint32_t> defPtr;};
static AvaQuad quad(const live::Machine& R,uint32_t a){return R.get<AvaQuad>(a);}
static AvaMatrix matrix(const live::Machine& R,uint32_t a){AvaMatrix m;for(unsigned i=0;i<4;i++)m[i]=quad(R,a+16*i);return m;}
static Built build(const live::Machine& R){
    Built b;
    for(uint32_t av=R.get<uint32_t>(GP+2496);av;av=R.get<uint32_t>(av+8)){
        OriginalAvalancheDef d;d.id=R.get<int32_t>(av);const int di=int(b.w.defs.size());b.defOf[av]=di;b.defPtr.push_back(av);
        int gi=0;
        for(uint32_t g=R.get<uint32_t>(av+4);g;g=R.get<uint32_t>(g+244),++gi){
            OriginalAvalancheGroup x;x.resource=R.get<uint32_t>(R.get<uint32_t>(g+248)+0x78);x.type=R.get<uint16_t>(g+240);x.hasEmitter=R.get<uint8_t>(g+242)!=0;
            x.duration=R.get<float>(g+256);x.fadeIn=R.get<float>(g+260);x.fadeOut=R.get<float>(g+264);x.speed=R.get<float>(g+268);x.start=quad(R,g+224);
            const uint32_t n=uint32_t(int32_t(x.duration)),s=R.get<uint32_t>(g+252);x.samples.resize(size_t(n)*10);for(uint32_t k=0;k<n*10;k++)x.samples[k]=R.get<uint8_t>(s+k);
            d.groups.push_back(std::move(x));b.groupOf[g]={di,gi};
        }
        const uint16_t n14=R.get<uint16_t>(av+14);for(uint16_t k=0;k<n14;k++)d.sounds.push_back({R.get<uint16_t>(av+272+4*k),R.get<uint16_t>(av+274+4*k)});
        b.w.defs.push_back(std::move(d));
    }
    auto poolIndex=[](uint32_t p){return p?int32_t((p-POOL)/TUMBLER):-1;};
    for(unsigned k=0;k<64;k++){
        const uint32_t t=POOL+k*TUMBLER;auto& tu=b.w.pool[k];const uint32_t g=R.get<uint32_t>(t+736);
        if(g){auto it=b.groupOf.find(g);if(it==b.groupOf.end())throw std::runtime_error("tumbler group outside the lists");tu.avalanche=it->second.first;tu.group=it->second.second;}
        tu.next=poolIndex(R.get<uint32_t>(t+740));tu.sample=R.get<int32_t>(t);tu.integrated=quad(R,t+16);tu.rotationIntegrated=matrix(R,t+32);
        tu.position=quad(R,t+96);tu.rotation=matrix(R,t+112);tu.scale=R.get<float>(t+176);tu.alpha=R.get<float>(t+180);tu.colour=quad(R,t+192);tu.emitterActive=R.get<uint32_t>(t+720)!=0;
    }
    for(unsigned k=0;k<16;k++){
        const uint32_t s=SLOTS+28*k;auto& slot=b.w.slots[k];const uint32_t av=R.get<uint32_t>(s);
        slot.avalanche=av?b.defOf.at(av):-1;slot.list=poolIndex(R.get<uint32_t>(s+4));slot.eventCursor=R.get<int32_t>(s+16);slot.soundCursor=R.get<int32_t>(s+20);slot.t=R.get<float>(s+24);
        if(av){const uint32_t arr=R.get<uint32_t>(s+8);int n=0;for(int32_t p=slot.list;p>=0;p=b.w.pool[size_t(p)].next)++n;for(int i=0;i<n;i++)slot.array.push_back(arr?poolIndex(R.get<uint32_t>(arr+4*i)):-1);}
    }
    return b;
}
// Differences between machine A's avalanche state and the port's (empty: equal).
static std::string compare(const live::Machine& R,const Built& b){
    char m[256];std::string out;
    auto eq=[&](const char* what,unsigned k,const float* a,const float* p,unsigned n){for(unsigned i=0;i<n;i++)if(bits(a[i])!=bits(p[i])){snprintf(m,sizeof m,"tumbler %u %s[%u] ps2 %08x port %08x; ",k,what,i,bits(a[i]),bits(p[i]));out+=m;return;}};
    for(unsigned k=0;k<64;k++){
        const uint32_t t=POOL+k*TUMBLER;const auto& tu=b.w.pool[k];const uint32_t g=R.get<uint32_t>(t+736);
        const bool live=g!=0;if(live!=(tu.group>=0)){snprintf(m,sizeof m,"tumbler %u live ps2 %d port %d; ",k,live,tu.group>=0);out+=m;continue;}
        if(!live)continue;
        if(b.groupOf.at(g)!=std::make_pair(tu.avalanche,tu.group)){snprintf(m,sizeof m,"tumbler %u group; ",k);out+=m;}
        if(R.get<int32_t>(t)!=tu.sample){snprintf(m,sizeof m,"tumbler %u sample ps2 %d port %d; ",k,R.get<int32_t>(t),tu.sample);out+=m;}
        const auto a16=quad(R,t+16);eq("integrated",k,a16.data(),tu.integrated.data(),4);
        const auto a32=matrix(R,t+32);eq("rotIntegrated",k,&a32[0][0],&tu.rotationIntegrated[0][0],16);
        const auto a96=quad(R,t+96);eq("position",k,a96.data(),tu.position.data(),4);
        const auto a112=matrix(R,t+112);eq("rotation",k,&a112[0][0],&tu.rotation[0][0],16);
        const float sa[2]={R.get<float>(t+176),R.get<float>(t+180)},sp[2]={tu.scale,tu.alpha};eq("scale/alpha",k,sa,sp,2);
        const auto a192=quad(R,t+192);eq("colour",k,a192.data(),tu.colour.data(),4);
    }
    for(unsigned k=0;k<16;k++){
        const uint32_t s=SLOTS+28*k;const auto& slot=b.w.slots[k];const bool on=R.get<uint32_t>(s)!=0;
        if(on!=(slot.avalanche>=0)){snprintf(m,sizeof m,"slot %u on ps2 %d port %d; ",k,on,slot.avalanche>=0);out+=m;continue;}
        if(!on)continue;
        if(bits(R.get<float>(s+24))!=bits(slot.t)||R.get<int32_t>(s+20)!=slot.soundCursor){snprintf(m,sizeof m,"slot %u t/cursor; ",k);out+=m;}
    }
    return out;
}
int main(int argc,char** argv){
    if(argc<4){fprintf(stderr,"usage: live FOLDER TICKS STATE...\n");return 2;}
    const std::string folder=argv[1];const int ticks=atoi(argv[2]);unsigned failures=0,runs=0,exactTicks=0,matrices=0;
    auto stepBoth=[&](live::Machine& A,Built& b,const char* label)->bool{
        for(int tick=0;tick<ticks;tick++){
            bool any=false;
            for(unsigned k=0;k<16;k++)if(A.get<uint32_t>(SLOTS+28*k)){any=true;A.call(0x2D7EF8,{SLOTS+28*k});}
            std::vector<uint32_t> released;for(auto& slot:b.w.slots)originalAvalancheSlotTick(b.w,slot,released);
            const std::string d=compare(A,b);
            if(!d.empty()){printf("  %s: tick %d differs: %.600s\n",label,tick+1,d.c_str());return false;}
            // 0x2D9C00 for every group instance (the AvaSpline modifier's matrix)
            for(const auto& [g,idx]:b.groupOf){
                const uint32_t inst=A.get<uint32_t>(g+248);AvaMatrix pm;const bool has=originalAvalancheInstanceMatrix(b.w,b.w.defs[size_t(idx.first)].groups[size_t(idx.second)].resource,pm);
                constexpr uint32_t OUT=0xE8000;for(unsigned i=0;i<16;i++)A.put<uint32_t>(OUT+4*i,0x7FC00001u);A.call(0x2D9C00,{inst,OUT});
                if(has){const AvaMatrix am=matrix(A,OUT);if(std::memcmp(&am,&pm,sizeof am)){printf("  %s: tick %d 0x2D9C00 differs for %x\n",label,tick+1,A.get<uint32_t>(inst+0x78));return false;}++matrices;}
            }
            ++exactTicks;if(!any)break;
        }
        return true;
    };
    for(int s=3;s<argc;s++){
        const std::string state=folder+"/"+argv[s];
        live::Machine A(state+".ee",state+".vuc",state+".vud");stubSound(A);const std::vector<uint8_t> pristine=A.ee;
        Built b=build(A);
        unsigned active=0;for(const auto& sl:b.w.slots)active+=sl.avalanche>=0;
        printf("%s: %zu avalanches, %u playing\n",argv[s],b.w.defs.size(),active);
        {const std::string d=compare(A,b);if(!d.empty()){printf("  build mismatch: %s\n",d.c_str());++failures;continue;}}
        if(active){++runs;if(!stepBoth(A,b,"running"))++failures;else printf("  running: %d ticks exact\n",ticks);}
        for(size_t di=0;di<b.w.defs.size();di++){
            A.ee=pristine;Built c=build(A);bool playing=false;for(const auto& sl:c.w.slots)playing|=sl.avalanche==int32_t(di);if(playing)continue;
            const int32_t id=c.w.defs[di].id;++runs;
            A.call(0x2D97A8,{uint32_t(id)});
            const bool ok=originalAvalancheTrigger(c.w,id);
            for(unsigned k=0;k<16;k++)if(c.w.slots[k].avalanche==int32_t(di))originalAvalancheSetStart(c.w,int32_t(k),[&](uint32_t resource){
                for(const auto& [g,idx]:c.groupOf)if(c.w.defs[size_t(idx.first)].groups[size_t(idx.second)].resource==resource)return quad(A,A.get<uint32_t>(g+248)+0x40);return AvaQuad{};});
            const std::string d=compare(A,c);char label[64];snprintf(label,sizeof label,"trigger %d",id);
            if(!ok||!d.empty()){printf("  %s: after the trigger (port %d): %.600s\n",label,ok,d.c_str());++failures;continue;}
            if(!stepBoth(A,c,label))++failures;else printf("  %s: %d ticks exact\n",label,ticks);
        }
    }
    printf("avalanche live: %u runs, %u exact ticks, %u AvaSpline matrices, %u failures\n",runs,exactTicks,matrices,failures);
    return failures?1:0;
}
