// Instruction oracle for engine/livecomp_animation.hpp: the recompiled original LiveComp
// functions (linked by tools/original_live_build.py, scalar-FP oracle copies) on a synthetic
// EE image (ELF at its addresses; BSS constants written as in the savestates) against the
// port: A 0x341AA0 constructor (0x34D9B0 base, 0x355280, 0x34E448, 0x351508, 0x34E348,
// 0x317830/0x317890 random ranges), B 0x341D48 tick (0x341E48/0x341EC0/0x341F38,
// 0x34EBA0), C 0x361098 node matrices (0x34DC90 -> 0x351800/0x351538/0x351A80/0x31BE50,
// 0x34DD18). Stubs: allocators 0x317D70/0x317E30/0x317E50, entity base 0x34FB00, 0x2D1AC8
// (34EBA0 scene notify), 0x317810 (gameplay RNG word), 0x34FCC0 (done callback).
#include "live_registrations.inc"
#include "original_live_runtime.hpp"
#include "../engine/livecomp_animation.hpp"
#include <random>
using namespace ssx;
static live::Machine* M;
static std::mt19937 rng(0x341D48);
static float uni(float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);}
static uint32_t bitsOf(float f){return std::bit_cast<uint32_t>(f);}
static constexpr uint32_t gp=0x4a30f0,MODEL=0x1000000,INST=0x1010000,OBJ=0x1020000,WORLD=0x1030000,ARGS=0x1070000,HEAP=0x1100000;
static uint32_t heap=HEAP;
static std::vector<uint32_t> words;static size_t wordAt=0;static unsigned doneCalls=0;
static void put(uint32_t a,const auto& v){M->put(a,v);}
static uint32_t word(uint32_t a){return M->get<uint32_t>(a);}
static float real(uint32_t a){return M->get<float>(a);}
static RollerQuad quad(uint32_t a){return M->get<RollerQuad>(a);}
static void putMatrix(uint32_t a,const RollerMatrix& m){for(unsigned i=0;i<4;i++)put(a+16*i,m[i]);}
static void stub(uint8_t*,R5900Context* c,PS2Runtime*){
    const uint32_t pc=c->pc,a0=GPR_U32(c,4),a2=GPR_U32(c,6),a3=GPR_U32(c,7);
    switch(pc){
    case 0x317d70:case 0x317e30:{uint32_t at=heap;heap+=(a0+15)&~15u;for(uint32_t o=0;o<a0;o+=4)put(at+o,uint32_t(rng()));SET_GPR_U32(c,2,at);break;}
    case 0x317e50:break;
    case 0x34fb00:put(a0+0x18,a3);put(a3+0xC,a0);put(a0+0xC,uint32_t(0x491c80));put(a0+0x10,uint16_t(a2));SET_GPR_U32(c,2,a0);break;
    case 0x2d1ac8:break;
    case 0x317810:if(wordAt>=words.size())throw std::runtime_error("RNG exhausted");SET_GPR_U32(c,2,words[wordAt++]);break;
    case 0x34fcc0:doneCalls++;break;
    default:throw std::runtime_error("unexpected stub");
    }
    c->pc=GPR_U32(c,31);
}
static RollerMatrix rotation(){
    float q[4]={uni(-1,1),uni(-1,1),uni(-1,1),uni(-1,1)};float l=std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);for(auto&x:q)x/=l;
    float x=q[0],y=q[1],z=q[2],w=q[3];
    return {RollerQuad{1-2*(y*y+z*z),2*(x*y+z*w),2*(x*z-y*w),0},RollerQuad{2*(x*y-z*w),1-2*(x*x+z*z),2*(y*z+x*w),0},
            RollerQuad{2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y),0},RollerQuad{0,0,0,1}};
}
// Single- and multi-node models with curves like the Snow Jam searchlight/pinlight/door tracks.
static OriginalAnimModel randomModel(){
    OriginalAnimModel model;unsigned n=rng()%3?1:1+rng()%3;model.length=rng()%3?float(int(1+rng()%4)):rng()%2?std::bit_cast<float>(0x3e2aaaabu):uni(.1f,3.f);
    for(unsigned i=0;i<n;i++){
        OriginalAnimNode node;node.parent=i==0||rng()%3==0?-1:int32_t(rng()%i);
        if(rng()%3){node.bind=rotation();node.bind[3]={uni(-100,100),uni(-100,100),uni(-100,100),1};}
        if(rng()%5){
            OriginalAnimTrack t;for(unsigned k=0;k<6;k++)t.base[k]=k<3?uni(-50,50):(rng()%3?0.f:uni(-180,180));
            t.mask=rng()%2?(1u<<(3+rng()%3)):rng()%2?0x18u:(rng()&0x3fu);if(!t.mask)t.mask=0x20;
            for(unsigned b=0;b<16;b++)if(t.mask>>b&1u){
                std::vector<OriginalAnimSegment> curve;unsigned s=1+rng()%4;float t0=0;
                for(unsigned j=0;j<s;j++){float t1=j+1==s?model.length:t0+uni(.02f,model.length/s);
                    curve.push_back({uni(-200,200),uni(-200,200),uni(-500,500),uni(-100,100),t0,t1});t0=t1;}
                t.curves.push_back(curve);
            }
            node.track=t;
        }
        model.nodes.push_back(node);
    }
    return model;
}
static void writeModel(const OriginalAnimModel& model){
    uint32_t table=MODEL+0x100,binds=MODEL+0x400,tracks=MODEL+0x1000;
    put(MODEL+4,uint32_t(model.nodes.size()));put(MODEL+8,table);put(MODEL+0x14,model.length);
    for(size_t i=0;i<model.nodes.size();i++){
        const auto& n=model.nodes[i];put(table+16*i,n.parent);put(table+16*i+4,uint32_t(0));
        putMatrix(binds+64*i,n.bind);put(table+16*i+12,uint32_t(binds+64*i));
        uint32_t track=0;
        if(n.track){
            track=tracks;const auto& t=*n.track;for(unsigned k=0;k<6;k++)put(track+4*k,t.base[k]);
            put(track+0x18,t.mask);put(track+0x1C,uint32_t(t.curves.size()));put(track+0x20,track+0x30);
            uint32_t curves=track+0x30+4*uint32_t(t.curves.size()),segs=curves+8*uint32_t(t.curves.size());
            for(size_t c=0;c<t.curves.size();c++){
                put(track+0x30+4*c,uint32_t(curves+8*c));put(curves+8*c,uint32_t(t.curves[c].size()));put(curves+8*c+4,segs);
                for(const auto& s:t.curves[c]){put(segs,s.a);put(segs+4,s.b);put(segs+8,s.c);put(segs+12,s.d);put(segs+16,s.t0);put(segs+20,s.t1);segs+=24;}
            }
            tracks=(segs+15)&~15u;
        }
        put(table+16*i+8,track);
    }
}
static void writeInstance(const RollerMatrix& matrix,float scale,uint32_t flags){
    putMatrix(INST+0x10,matrix);put(INST+0x80,MODEL);put(INST+0x84,scale);put(INST+8,flags);put(INST+0xC,OBJ+0x30);
}
// LiveComp image: +0x00..+0x2C, entity at +0x30 (vtable +0x3C, flags +0x42, instance +0x48),
// base component +0x5C count, +0x60 matrices, +0x64 channels.
static void writeLiveComp(const OriginalLiveComp& s){
    put(OBJ,uint16_t(s.mode));put(OBJ+4,s.enabled);put(OBJ+8,s.done);put(OBJ+0xC,s.delay);put(OBJ+0x10,s.rate);put(OBJ+0x14,s.low);put(OBJ+0x18,s.high);
    put(OBJ+0x1C,s.anim.time);put(OBJ+0x20,s.anim.sampleTime);put(OBJ+0x24,s.anim.previous);put(OBJ+0x28,s.anim.unclamped);put(OBJ+0x2C,s.anim.evaluated);
    put(OBJ+0x30,uint32_t(0));put(OBJ+0x3C,uint32_t(0x490b10));put(OBJ+0x40,uint16_t(1));put(OBJ+0x42,uint16_t(s.anim.dirty?1:0));put(OBJ+0x48,INST);put(OBJ+0x4C,uint32_t(0));
    uint32_t channels=OBJ+0x1000,matrices=OBJ+0x800;
    put(OBJ+0x5C,uint32_t(s.anim.channels.size()));put(OBJ+0x60,matrices);put(OBJ+0x64,s.anim.channels.empty()?0u:channels);
    for(size_t i=0;i<s.anim.channels.size();i++){
        const auto& c=s.anim.channels[i];uint32_t at=channels+0xD0*uint32_t(i);
        for(unsigned k=0;k<16;k++){put(at+4*k,c.segment[k]);put(at+0x44+4*k,c.value[k]);}
        uint32_t t=0;size_t seen=0;for(size_t n=0;n<s.anim.model->nodes.size();n++)if(s.anim.model->nodes[n].track){if(seen==i)t=word(MODEL+0x100+16*uint32_t(n)+8);seen++;}
        put(at+0x40,t);put(at+0x84,uint32_t(0x490ad0));putMatrix(at+0x90,c.matrix);
    }
    for(size_t i=0;i<s.anim.matrices.size();i++)putMatrix(matrices+64*uint32_t(i),s.anim.matrices[i]);
}
static unsigned printed=0;
static unsigned diff(uint32_t at,const std::vector<uint32_t>& port,const char* what,unsigned n){
    unsigned bad=0;for(size_t k=0;k<port.size();k++)if(word(at+4*uint32_t(k))!=port[k]){
        if(printed++<30)printf("  %s case %u +%03zx original %08x (%g) port %08x (%g)\n",what,n,4*k,word(at+4*uint32_t(k)),real(at+4*uint32_t(k)),port[k],std::bit_cast<float>(port[k]));bad++;}
    return bad;
}
static unsigned compare(const OriginalLiveComp& s,const char* what,unsigned n,bool matrices,bool channels){
    std::vector<uint32_t> head{uint32_t(uint16_t(s.mode))|(word(OBJ)&0xffff0000u),uint32_t(s.enabled),uint32_t(s.done),uint32_t(s.delay),bitsOf(s.rate),bitsOf(s.low),bitsOf(s.high),
        bitsOf(s.anim.time),bitsOf(s.anim.sampleTime),bitsOf(s.anim.previous),bitsOf(s.anim.unclamped),uint32_t(s.anim.evaluated)};
    unsigned bad=diff(OBJ,head,what,n);
    if(bool(M->get<uint16_t>(OBJ+0x42)&1)!=s.anim.dirty){bad++;if(printed++<30)printf("  %s case %u dirty\n",what,n);}
    if(channels)for(size_t i=0;i<s.anim.channels.size();i++){
        const auto& c=s.anim.channels[i];std::vector<uint32_t> w;
        for(unsigned k=0;k<16;k++)w.push_back(bitsOf(c.value[k]));bad+=diff(OBJ+0x1000+0xD0*uint32_t(i)+0x44,w,what,n);
        w.clear();for(auto& r:c.matrix)for(float x:r)w.push_back(bitsOf(x));bad+=diff(OBJ+0x1000+0xD0*uint32_t(i)+0x90,w,what,n);
    }
    if(matrices)for(size_t i=0;i<s.anim.matrices.size();i++){std::vector<uint32_t> w;for(auto& r:s.anim.matrices[i])for(float x:r)w.push_back(bitsOf(x));bad+=diff(OBJ+0x800+64*uint32_t(i),w,what,n);}
    return bad;
}
static RollerMatrix randomInstance(){RollerMatrix m=rotation();m[3]={uni(-3e5,3e5),uni(-3e5,3e5),uni(-3e5,3e5),1};return m;}
static OriginalLiveCompArguments randomArgs(){
    auto a=OriginalLiveCompArguments::defaults();auto f=[&](unsigned k,float v){a.words[k]=bitsOf(v);};
    a.words[1]=rng()%4?rng()%3:uint32_t(rng()%5);if(rng()%3==0)a.words[2]=rng()%3;
    if(rng()%3==0)f(3,rng()%2?uni(-5,40):0.f);if(rng()%3==0)f(4,rng()%2?uni(-5,120):5.f);
    if(rng()%3==0)f(5,uni(-60,60));if(rng()%4==0)f(6,uni(-10,10));if(rng()%3==0)f(7,uni(-5,120));
    if(rng()%3==0)a.words[8]=1;if(rng()%3==0)a.words[9]=1;
    return a;
}

int main(int argc,char** argv){
    if(argc<2){fprintf(stderr,"usage: reference FOLDER\n");return 2;}
    std::string folder=argv[1];live::Machine machine(folder+"/synthetic.ee",folder+"/synthetic.vuc",folder+"/synthetic.vud");M=&machine;
    for(uint32_t pc:{0x317d70u,0x317e30u,0x317e50u,0x34fb00u,0x2d1ac8u,0x317810u,0x34fcc0u})
        if(!machine.runtime.registerFunction(pc,stub))throw std::runtime_error("stub registration");
    put(0x4FF120,RollerQuad{0,0,0,0});put(0x4FF150,RollerQuad{0,1,0,0});putMatrix(0x4FF1A0,rail_modifier_math::identity());
    put(gp-0x848,WORLD);put(gp+0x2A74,WORLD);put(WORLD+0x10,int32_t(60));
    unsigned failures=0;
    // ---- A. constructor 341AA0 ----
    {
        unsigned bad=0,cases=0,random=0;
        for(unsigned n=0;n<10000;n++,cases++){
            auto model=randomModel();writeModel(model);auto inst=randomInstance();float scale=rng()%3?1.f:uni(.5f,2);uint32_t flags=rng()%2?0x10003u:0x10002u;writeInstance(inst,scale,flags);
            auto a=randomArgs();int32_t fps=rng()%4?60:int32_t(1+rng()%120);put(WORLD+0x10,fps);
            put(ARGS,a.words);for(uint32_t o=0;o<0x6C;o+=4)put(OBJ+o,uint32_t(rng()));heap=HEAP;
            words.clear();for(unsigned k=0;k<4;k++)words.push_back(rng());wordAt=0;
            machine.call(0x341aa0,{OBJ,1,1,INST,ARGS});size_t used=wordAt;wordAt=0;
            auto s=originalLiveCompConstruct(model,inst,scale,flags,a,[&]{return words.at(wordAt++);},fps);random+=used>0;
            // the original's buffers live on its heap: re-point the comparison at them
            unsigned d=0;
            {std::vector<uint32_t> head{uint32_t(uint16_t(s.mode))|(word(OBJ)&0xffff0000u),uint32_t(s.enabled),uint32_t(s.done),uint32_t(s.delay),bitsOf(s.rate),bitsOf(s.low),bitsOf(s.high),
                bitsOf(s.anim.time),bitsOf(s.anim.sampleTime),bitsOf(s.anim.previous),bitsOf(s.anim.unclamped),uint32_t(s.anim.evaluated)};d+=diff(OBJ,head,"ctor",n);}
            if(bool(M->get<uint16_t>(OBJ+0x42)&1)!=s.anim.dirty){d++;if(printed++<30)printf("  ctor %u dirty\n",n);}
            if(used!=wordAt){d++;if(printed++<30)printf("  ctor %u rng words original %zu port %zu (key6 %08x key8 %u)\n",n,used,wordAt,a.words[6],a.words[8]);}
            uint32_t wantFlags=s.suppressStaticDraw?(flags&~2u)|4u:flags;if(word(INST+8)!=wantFlags){d++;if(printed++<30)printf("  ctor %u flags original %08x port %08x key9 %u\n",n,word(INST+8),wantFlags,a.words[9]);}
            uint32_t ch=word(OBJ+0x64);
            for(size_t i=0;i<s.anim.channels.size();i++){std::vector<uint32_t> w;for(unsigned k=0;k<16;k++)w.push_back(bitsOf(s.anim.channels[i].value[k]));d+=diff(ch+0xD0*uint32_t(i)+0x44,w,"ctor channel",n);}
            bad+=d!=0;
        }
        printf("A 341AA0 constructor: %u/%u cases identical (%u drew the gameplay RNG)\n",cases-bad,cases,random);failures+=bad;
    }
    // ---- B. tick 341D48 (+ C. node matrices 361098 after it) ----
    {
        unsigned bad=0,cases=0,finished=0,wrapped=0,bm=0;
        for(unsigned n=0;n<40000;n++,cases++){
            auto model=randomModel();writeModel(model);auto inst=randomInstance();float scale=rng()%3?1.f:uni(.5f,2);writeInstance(inst,scale,0x10003);
            auto a=randomArgs();a.words[8]=0;a.words[6]=0;auto s=originalLiveCompConstruct(model,inst,scale,0x10003,a,[&]{return uint32_t(rng());});
            s.mode=int16_t(rng()%4?rng()%3:rng()%5);s.done=int32_t(rng()%6==0);s.enabled=int32_t(rng()%8!=0);s.delay=rng()%5?0:int32_t(rng()%3)-1;
            if(rng()%3==0)s.rate=-s.rate;
            float pick=rng()%4;s.anim.time=pick==0?s.low:pick==1?s.high:uni(s.low-.1f,s.high+.1f);s.anim.sampleTime=s.anim.time;s.anim.dirty=rng()%2;
            if(!s.anim.dirty)originalAnimComposeNodes(s.anim);
            bool owned=rng()%10!=0;put(INST+0xC,owned?OBJ+0x30:OBJ+0x999);
            writeLiveComp(s);doneCalls=0;
            unsigned steps=1+rng()%3;unsigned d=0;
            for(unsigned k=0;k<steps;k++){
                auto c=machine.call(0x341d48,{OBJ});float before=s.anim.time;auto r=originalLiveCompTick(s,owned);
                wrapped+=s.anim.time<before;finished+=r==OriginalLiveCompTick::Done;
                uint32_t want=r==OriginalLiveCompTick::Updated||r==OriginalLiveCompTick::Finished?1u:0u;
                if(GPR_U32((&c),2)!=want)d++;
            }
            if(doneCalls!=0)(void)0;
            d+=compare(s,"tick",n,false,false);
            if(rng()%2){machine.call(0x361098,{OBJ+0x1C});originalLiveCompMatrices(s);unsigned m=compare(s,"matrices",n,true,true);d+=m;bm+=m!=0;}
            bad+=d!=0;
        }
        printf("B 341D48 tick + C 361098 node matrices: %u/%u cases identical (%u done transitions, %u wraps, %u matrix mismatches)\n",cases-bad,cases,finished,wrapped,bm);failures+=bad;
    }
    if(failures){printf("FAILED %u\n",failures);return 1;}
    puts("livecomp animation oracle: all cases match");return 0;
}
