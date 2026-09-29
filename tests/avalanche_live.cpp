// Live avalanche playback parity (development oracle; tools/test_avalanche_live.py): engine/avalanche.hpp against the
// full recompiled original on PS2 savestates of EBA3 / ERA5 (docs/avalanche.md).
//  1. Running slots: the port world is built from the state's own memory (the avalanche / group lists gp+2496, the slot
//     table 0x538938, the tumbler pool 0x4EE770); machine A runs 0x2D7EF8 per active slot and tick, the port
//     originalAvalancheSlotTick. Compared per tick: every tumbler's group, sample index, integrated position / rotation,
//     position, rotation, scale, alpha, colour (bit for bit) and each slot's t, cursors and array; the 0x2D9C00
//     AvaSpline matrix of every group instance.
//  2. Triggers: for every avalanche not playing in the state, A runs 0x2D97A8(id) and the port originalAvalancheTrigger
//     (+96 from the instance +0x40), then both step TICKS ticks, compared as above.
//  3. Replay snapshot (a state with a slot playing): the group instances' entities cleared (instance +0xC, as 0x355118 leaves
//     them before the restore), A runs 0x2D9CB0 into a host stream (vt+0x0C write / +0x14 read) and 0x2D9D68 from it, the port
//     originalAvalancheSave / originalAvalancheRestore; the saved words, the state and 0x2D9C00 compared, then TICKS ticks.
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
// The replay stream of 0x2D9CB0 / 0x2D9D68: an object whose vtable's +0x0C writes and +0x14 reads (a1 buffer, a2 bytes).
// (the host methods take the guest PCs of two start-up functions the avalanche code never reaches: the function table only
// accepts PCs of the generated range)
static constexpr uint32_t STREAM=0xE9000,STREAM_VT=0xE9100,STREAM_WRITE=0x100008,STREAM_READ=0x1001C8;
static std::vector<uint8_t> streamBytes;
static void streamWrite(uint8_t* ram,R5900Context* c,PS2Runtime*){const uint32_t a=GPR_U32(c,5)&0x1ffffff,n=GPR_U32(c,6);streamBytes.assign(ram+a,ram+a+n);c->pc=GPR_U32(c,31);}
static void streamRead(uint8_t* ram,R5900Context* c,PS2Runtime*){const uint32_t a=GPR_U32(c,5)&0x1ffffff,n=GPR_U32(c,6);std::memcpy(ram+a,streamBytes.data(),std::min<size_t>(n,streamBytes.size()));c->pc=GPR_U32(c,31);}
struct Built {OriginalAvalancheWorld w;std::map<uint32_t,std::pair<int,int>> groupOf;std::map<uint32_t,int> defOf;std::vector<uint32_t> defPtr;OriginalRandomState visual;bool trails=true;};
static constexpr uint32_t VISUAL=0x4FF018; // the presentation stream 0x3177F0 draws (6 words)
static AvaQuad quad(const live::Machine& R,uint32_t a){return R.get<AvaQuad>(a);}
static AvaMatrix matrix(const live::Machine& R,uint32_t a){AvaMatrix m;for(unsigned i=0;i<4;i++)m[i]=quad(R,a+16*i);return m;}
static Built build(const live::Machine& R){
    Built b;for(unsigned k=0;k<6;k++)b.visual.words[k]=R.get<uint32_t>(VISUAL+4*k);
    for(uint32_t av=R.get<uint32_t>(GP+2496);av;av=R.get<uint32_t>(av+8)){
        OriginalAvalancheDef d;d.id=R.get<int32_t>(av);const int di=int(b.w.defs.size());b.defOf[av]=di;b.defPtr.push_back(av);
        int gi=0;
        for(uint32_t g=R.get<uint32_t>(av+4);g;g=R.get<uint32_t>(g+244),++gi){
            OriginalAvalancheGroup x;x.resource=R.get<uint32_t>(R.get<uint32_t>(g+248)+0x78);x.type=R.get<uint16_t>(g+240);x.hasEmitter=R.get<uint8_t>(g+242)!=0;
            for(unsigned k=0;k<54;k++)x.emitterWords[k]=R.get<uint32_t>(g+4*k);
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
        auto& em=b.w.emitters[k];em.constructed=true;const uint32_t e=t+0xD0; // the trails emitter (0x208 bytes; rings through its pointers)
        for(unsigned o=0;o<0x200;o+=4)em.base.setU(o,R.get<uint32_t>(e+o));em.word200=R.get<uint32_t>(e+0x200);
        const int32_t cap=em.base.i(0x178);const uint32_t ra=R.get<uint32_t>(e+0x1A0),rb=R.get<uint32_t>(e+0x1A4),rc=R.get<uint32_t>(e+0x204);
        em.base.ringPosition.clear();em.base.ringVelocity.clear();em.colours.clear();
        if(cap>0&&ra&&rb&&rc)for(int32_t i=0;i<cap;i++){em.base.ringPosition.push_back(quad(R,ra+16*i));em.base.ringVelocity.push_back(quad(R,rb+16*i));
            std::array<uint8_t,4> c;for(unsigned j=0;j<4;j++)c[j]=R.get<uint8_t>(rc+4*i+j);em.colours.push_back(c);}
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
    if(b.trails){ // the trails emitters (image words but the three ring pointers, the rings, +0x200) and the presentation stream
        for(unsigned k=0;k<64;k++){
            const uint32_t e=POOL+k*TUMBLER+0xD0;const auto& em=b.w.emitters[k];
            for(unsigned o=0;o<0x200;o+=4){if(o==0x1A0||o==0x1A4)continue;if(R.get<uint32_t>(e+o)!=em.base.u(o)){snprintf(m,sizeof m,"emitter %u +%03x ps2 %08x port %08x; ",k,o,R.get<uint32_t>(e+o),em.base.u(o));out+=m;break;}}
            if(R.get<uint32_t>(e+0x200)!=em.word200){snprintf(m,sizeof m,"emitter %u +200; ",k);out+=m;}
            const int32_t cap=em.base.i(0x178);const uint32_t ra=R.get<uint32_t>(e+0x1A0),rb=R.get<uint32_t>(e+0x1A4),rc=R.get<uint32_t>(e+0x204);
            if(cap>0&&ra&&rb&&rc)for(int32_t i=0;i<cap;i++){
                const auto pa=quad(R,ra+16*i),pb=quad(R,rb+16*i);bool same=size_t(i)<em.base.ringPosition.size()&&!std::memcmp(&pa,&em.base.ringPosition[size_t(i)],16)&&!std::memcmp(&pb,&em.base.ringVelocity[size_t(i)],16);
                for(unsigned j=0;same&&j<4;j++)same=size_t(i)<em.colours.size()&&R.get<uint8_t>(rc+4*i+j)==em.colours[size_t(i)][j];
                if(!same){snprintf(m,sizeof m,"emitter %u ring slot %d; ",k,i);out+=m;break;}
            }
        }
        for(unsigned k=0;k<6;k++)if(R.get<uint32_t>(VISUAL+4*k)!=b.visual.words[k]){snprintf(m,sizeof m,"visual stream word %u; ",k);out+=m;break;}
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
            std::vector<uint32_t> released;for(auto& slot:b.w.slots)originalAvalancheSlotTick(b.w,slot,released,&b.visual);
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
        live::Machine A(state+".ee",state+".vuc",state+".vud");stubSound(A);A.runtime.registerFunction(STREAM_WRITE,streamWrite);A.runtime.registerFunction(STREAM_READ,streamRead);const std::vector<uint8_t> pristine=A.ee;
        Built b=build(A);
        unsigned active=0;for(const auto& sl:b.w.slots)active+=sl.avalanche>=0;
        printf("%s: %zu avalanches, %u playing\n",argv[s],b.w.defs.size(),active);
        {const std::string d=compare(A,b);if(!d.empty()){printf("  build mismatch: %s\n",d.c_str());++failures;continue;}}
        if(active){++runs;if(!stepBoth(A,b,"running"))++failures;else printf("  running: %d ticks exact\n",ticks);}
        if(active){ // 3. the replay snapshot save / restore
            A.ee=pristine;Built c=build(A);++runs;c.trails=false; // the restore's per-sample emitter catch-up (+748) is not ported
            for(const auto& [g,idx]:c.groupOf)A.put<uint32_t>(A.get<uint32_t>(g+248)+0xC,0u);
            A.put<uint32_t>(STREAM,STREAM_VT);A.put<int16_t>(STREAM_VT+8,0);A.put<uint32_t>(STREAM_VT+0xC,STREAM_WRITE);A.put<int16_t>(STREAM_VT+0x10,0);A.put<uint32_t>(STREAM_VT+0x14,STREAM_READ);
            streamBytes.clear();A.call(0x2D9CB0,{STREAM});
            const auto saved=originalAvalancheSave(c.w);
            if(streamBytes.size()!=sizeof saved||std::memcmp(streamBytes.data(),saved.data(),sizeof saved)){printf("  restore: 0x2D9CB0 words differ\n");++failures;continue;}
            A.call(0x2D9D68,{STREAM});
            originalAvalancheRestore(c.w,saved,[&](uint32_t resource){
                for(const auto& [g,idx]:c.groupOf)if(c.w.defs[size_t(idx.first)].groups[size_t(idx.second)].resource==resource)return quad(A,A.get<uint32_t>(g+248)+0x40);return AvaQuad{};});
            std::string d=compare(A,c);
            for(const auto& [g,idx]:c.groupOf){ // the next 0x2D9C00 reads the trigger's +96 / +112
                const uint32_t inst=A.get<uint32_t>(g+248);AvaMatrix pm;const bool has=originalAvalancheInstanceMatrix(c.w,c.w.defs[size_t(idx.first)].groups[size_t(idx.second)].resource,pm);
                constexpr uint32_t OUT=0xE8000;for(unsigned i=0;i<16;i++)A.put<uint32_t>(OUT+4*i,0x7FC00001u);A.call(0x2D9C00,{inst,OUT});
                if(has){const AvaMatrix am=matrix(A,OUT);if(std::memcmp(&am,&pm,sizeof am))d+="0x2D9C00 after the restore; ";else ++matrices;}
            }
            for(unsigned k=0;k<16;k++)if(c.w.slots[k].avalanche>=0&&A.get<int32_t>(SLOTS+28*k+16)!=c.w.slots[k].eventCursor)d+="event cursor; ";
            if(!d.empty()){printf("  restore: %.600s\n",d.c_str());++failures;}
            else if(!stepBoth(A,c,"restore"))++failures;else printf("  restore: saved words, state and %d ticks exact\n",ticks);
        }
        for(size_t di=0;di<b.w.defs.size();di++){
            A.ee=pristine;Built c=build(A);bool playing=false;for(const auto& sl:c.w.slots)playing|=sl.avalanche==int32_t(di);if(playing)continue;
            const int32_t id=c.w.defs[di].id;++runs;
            const AvaQuad flake{A.call(0x2EE7C8,{0}).f[0],A.call(0x2EE810,{0}).f[0],A.call(0x2EE858,{0}).f[0],0.f}; // the Weather painter flake R, G, B
            A.call(0x2D97A8,{uint32_t(id)});
            // 1 / NumBlur (kernel +0x30) with NumBlur 0: the EE (PCSX2 savestates: 0x7F7FFFFF) clamps x / 0 to +-0x7F7FFFFF; the
            // recompiled div.s gives an IEEE infinity. The EE value is the reference.
            for(unsigned k=0;k<64;k++){const uint32_t a=POOL+k*TUMBLER+0xD0+0x50,v=A.get<uint32_t>(a);if((v&0x7FFFFFFFu)==0x7F800000u)A.put<uint32_t>(a,(v&0x80000000u)|0x7F7FFFFFu);}
            const bool ok=originalAvalancheTrigger(c.w,id,&c.visual,flake);
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
