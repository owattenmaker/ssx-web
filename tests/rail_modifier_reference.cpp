// Instruction oracle for engine/rail_modifier.hpp: the recompiled original AnimTeeter /
// RailModifier functions (linked by tools/original_live_build.py from the roots, scalar-FP
// oracle copies) run on a synthetic EE image (ELF loaded, so vtables and gp constants are
// real; BSS constants 0x4FF120/0x4FF150/0x4FF1A0 written as in the savestates) against the
// port with randomized models, states and queries. Stubs: allocators 0x317D70/0x317E30/0x317E50,
// entity base 0x34FB00, 0x2D1AC8 (34EBA0 scene notify), world/spatial 0x2D1BE0/0x328F28/
// 0x328C20/0x3284B8/0x3291E0, stage table 0x2D1BD8 (synthetic rail descriptor chain).
#include "live_registrations.inc"
#include "original_live_runtime.hpp"
#include "../engine/rail_modifier.hpp"
#include <random>
using namespace ssx;
static live::Machine* M;
static std::mt19937 rng(0x35C698);
static float uni(float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);}
static uint32_t bitsOf(float f){return std::bit_cast<uint32_t>(f);}
static constexpr uint32_t gp=0x4a30f0,MODEL=0x1000000,INST=0x1010000,OBJ=0x1020000,WORLD=0x1030000,RAIL=0x1040000,CHAIN=0x1050000,
    DESC=0x1060000,SEGS=0x1061000,ARGS=0x1070000,CONTACT=0x1070100,FORCE=0x1070200,POINT=0x1070300,OUT=0x1070400,FOUND=0x1070500,BEST=0x1070504,
    LEAF=0x1070600,TREE=0x1031000,HEAP=0x1100000;
static uint32_t heap=HEAP;
struct Event {uint32_t pc;std::vector<uint32_t> w;};
static std::vector<Event> events;
static void put(uint32_t a,const auto& v){M->put(a,v);}
static uint32_t word(uint32_t a){return M->get<uint32_t>(a);}
static float real(uint32_t a){return M->get<float>(a);}
static RollerQuad quad(uint32_t a){return M->get<RollerQuad>(a);}
static RollerMatrix matrixAt(uint32_t a){RollerMatrix m;for(unsigned i=0;i<4;i++)m[i]=quad(a+16*i);return m;}
static void putMatrix(uint32_t a,const RollerMatrix& m){for(unsigned i=0;i<4;i++)put(a+16*i,m[i]);}
static void stub(uint8_t*,R5900Context* c,PS2Runtime*){
    const uint32_t pc=c->pc,a0=GPR_U32(c,4),a1=GPR_U32(c,5),a2=GPR_U32(c,6),a3=GPR_U32(c,7);
    switch(pc){
    case 0x317d70:case 0x317e30:{uint32_t at=heap;heap+=(a0+15)&~15u;for(uint32_t o=0;o<a0;o+=4)put(at+o,uint32_t(rng()));SET_GPR_U32(c,2,at);events.push_back({pc,{a0}});break;}
    case 0x317e50:events.push_back({pc,{a0}});break;
    case 0x34fb00:put(a0+0x18,a3);put(a3+0xC,a0);put(a0+0xC,uint32_t(0x491c80));put(a0+0x10,uint16_t(a2));SET_GPR_U32(c,2,a0);events.push_back({pc,{a0,a2,a3}});break;
    case 0x2d1ac8:events.push_back({pc,{a0}});break;
    case 0x2d1be0:SET_GPR_U32(c,2,TREE);break;
    case 0x2d1bd8:SET_GPR_U32(c,2,CHAIN);break;
    case 0x328f28:put(a0+4,uint32_t(0));put(a0+8,uint32_t(0));put(a0+12,uint32_t(0));events.push_back({pc,{a1}});break;
    case 0x328c20:case 0x3284b8:case 0x3291e0:events.push_back({pc,{a0,a1,a2}});break;
    default:throw std::runtime_error("unexpected stub");
    }
    c->pc=GPR_U32(c,31);
}

// ---- random models ---------------------------------------------------------
static RollerMatrix rotation(float scale=1){
    float q[4]={uni(-1,1),uni(-1,1),uni(-1,1),uni(-1,1)};float l=std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);for(auto&x:q)x/=l;
    float x=q[0],y=q[1],z=q[2],w=q[3];
    RollerMatrix m{RollerQuad{1-2*(y*y+z*z),2*(x*y+z*w),2*(x*z-y*w),0},RollerQuad{2*(x*y-z*w),1-2*(x*x+z*z),2*(y*z+x*w),0},
                   RollerQuad{2*(x*z+y*w),2*(y*z-x*w),1-2*(x*x+y*y),0},RollerQuad{0,0,0,1}};
    for(unsigned i=0;i<3;i++)for(unsigned k=0;k<3;k++)m[i][k]*=scale;return m;
}
static OriginalAnimModel randomModel(){
    OriginalAnimModel model;unsigned n=1+rng()%4;model.length=rng()%3?1.f:uni(.3f,2.f);
    for(unsigned i=0;i<n;i++){
        OriginalAnimNode node;node.parent=i==0||rng()%3==0?-1:int32_t(rng()%i);
        if(rng()%4){node.bind=rotation();node.bind[3]={uni(-100,100),uni(-100,100),uni(-100,100),1};}
        if(i>0&&rng()%4){
            OriginalAnimTrack t;for(unsigned k=0;k<6;k++)t.base[k]=k<3?uni(-50,50):(rng()%3?0.f:uni(-180,180));
            if(rng()%3==0)t.base[4]=-0.f;
            t.mask=rng()%3?(1u<<(3+rng()%3)):(rng()&0xffffu);if(!t.mask)t.mask=0x10;
            for(unsigned b=0;b<16;b++)if(t.mask>>b&1u){
                std::vector<OriginalAnimSegment> curve;unsigned s=1+rng()%4;float t0=rng()%4?0.f:uni(-.2f,.2f);
                for(unsigned j=0;j<s;j++){float t1=j+1==s?model.length:t0+uni(.05f,model.length/s);
                    curve.push_back({uni(-50,50),uni(-50,50),rng()%2?uni(-100,100):40.6815f,uni(-30,30),t0,t1});t0=t1;}
                t.curves.push_back(curve);
            }
            node.track=t;
        }
        model.nodes.push_back(node);
    }
    return model;
}
static uint32_t writeModel(const OriginalAnimModel& model){
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
    return MODEL;
}
static void writeInstance(const RollerMatrix& matrix,float scale,const std::array<float,3>& low,const std::array<float,3>& high){
    putMatrix(INST+0x10,matrix);for(unsigned k=0;k<3;k++){put(INST+0x60+4*k,low[k]);put(INST+0x6C+4*k,high[k]);}
    put(INST+0x80,MODEL);put(INST+0x84,scale);put(INST+8,uint32_t(0x10003));put(INST+0xC,OBJ+0x44);
}
// AnimTeeter object image (+0x00..+0x7F) + channels + matrices.
static void writeTeeter(const OriginalAnimTeeter& e){
    const float f[]={e.torque,e.step,e.gain,e.restitution,e.minimum,e.maximum,e.maxStep,e.damping,e.stiffness,e.rest,e.velocity};
    for(unsigned k=0;k<11;k++)put(OBJ+4*k,f[k]);put(OBJ+0x2C,e.flipMask);
    put(OBJ+0x30,e.time);put(OBJ+0x34,e.sampleTime);put(OBJ+0x38,e.previous);put(OBJ+0x3C,e.unclamped);put(OBJ+0x40,e.evaluated);
    put(OBJ+0x50,uint32_t(0x4908f8));put(OBJ+0x54,uint16_t(4));put(OBJ+0x56,uint16_t(e.dirty?1:0));put(OBJ+0x5C,INST);put(OBJ+0x60,uint32_t(0));
    uint32_t channels=OBJ+0x1000,matrices=OBJ+0x800;
    put(OBJ+0x70,uint32_t(e.channels.size()));put(OBJ+0x74,matrices);put(OBJ+0x78,e.channels.empty()?0u:channels);
    uint32_t track=0;(void)track;
    for(size_t i=0;i<e.channels.size();i++){
        const auto& c=e.channels[i];uint32_t at=channels+0xD0*uint32_t(i);
        for(unsigned k=0;k<16;k++){put(at+4*k,c.segment[k]);put(at+0x44+4*k,c.value[k]);}
        // track pointer: the node's track in the model image
        uint32_t t=0;size_t seen=0;for(size_t n=0;n<e.model->nodes.size();n++)if(e.model->nodes[n].track){if(seen==i)t=word(MODEL+0x100+16*uint32_t(n)+8);seen++;}
        put(at+0x40,t);put(at+0x84,uint32_t(0x490ad0));putMatrix(at+0x90,c.matrix);
    }
    for(size_t i=0;i<e.matrices.size();i++)putMatrix(matrices+64*uint32_t(i),e.matrices[i]);
}
static unsigned diffWords(uint32_t at,const std::vector<uint32_t>& port,const char* what,unsigned n,unsigned& printed){
    unsigned bad=0;
    for(size_t k=0;k<port.size();k++)if(word(at+4*uint32_t(k))!=port[k]){
        if(printed<30){printf("  %s case %u +%03zx original %08x (%g) port %08x (%g)\n",what,n,4*k,word(at+4*uint32_t(k)),real(at+4*uint32_t(k)),port[k],std::bit_cast<float>(port[k]));printed++;}
        bad++;
    }
    return bad;
}
static std::vector<uint32_t> teeterWords(const OriginalAnimTeeter& e){
    const float f[]={e.torque,e.step,e.gain,e.restitution,e.minimum,e.maximum,e.maxStep,e.damping,e.stiffness,e.rest,e.velocity};
    std::vector<uint32_t> w;for(float x:f)w.push_back(bitsOf(x));w.push_back(e.flipMask);
    for(float x:{e.time,e.sampleTime,e.previous,e.unclamped})w.push_back(bitsOf(x));w.push_back(uint32_t(e.evaluated));return w;
}
static unsigned compareTeeter(const OriginalAnimTeeter& e,const char* what,unsigned n,unsigned& printed,bool matrices){
    unsigned bad=diffWords(OBJ,teeterWords(e),what,n,printed);
    if(bool(M->get<uint16_t>(OBJ+0x56)&1)!=e.dirty){bad++;if(printed++<30)printf("  %s case %u dirty original %u port %u\n",what,n,M->get<uint16_t>(OBJ+0x56)&1,e.dirty);}
    for(size_t i=0;i<e.channels.size();i++){
        const auto& c=e.channels[i];std::vector<uint32_t> w;
        for(unsigned k=0;k<16;k++)w.push_back(uint32_t(c.segment[k]));bad+=diffWords(OBJ+0x1000+0xD0*uint32_t(i),w,what,n,printed);
        w.clear();for(unsigned k=0;k<16;k++)w.push_back(bitsOf(c.value[k]));bad+=diffWords(OBJ+0x1000+0xD0*uint32_t(i)+0x44,w,what,n,printed);
        w.clear();for(auto& r:c.matrix)for(float x:r)w.push_back(bitsOf(x));bad+=diffWords(OBJ+0x1000+0xD0*uint32_t(i)+0x90,w,what,n,printed);
    }
    if(matrices)for(size_t i=0;i<e.matrices.size();i++){
        std::vector<uint32_t> w;for(auto& r:e.matrices[i])for(float x:r)w.push_back(bitsOf(x));bad+=diffWords(OBJ+0x800+64*uint32_t(i),w,what,n,printed);
    }
    return bad;
}
static float pick(const OriginalAnimModel& m){
    switch(rng()%6){case 0:return 0;case 1:return m.length;case 2:return uni(-.3f,m.length+.3f);
    case 3:{for(const auto& n:m.nodes)if(n.track&&!n.track->curves.empty()){const auto& c=n.track->curves[rng()%n.track->curves.size()];const auto& s=c[rng()%c.size()];return rng()%2?s.t0:s.t1;}return 0;}
    default:return uni(0,m.length);}
}
static OriginalAnimTeeter randomTeeter(const OriginalAnimModel& model,const RollerMatrix& instance,float scale){
    OriginalAnimTeeterArgs args;args.gain=rng()%3?1000.f:uni(-3000,3000);args.maxSpeed=rng()%3?120.f:uni(1,300);args.stiffness=rng()%3?.5f:uni(0,4);
    args.rest=rng()%3?0.f:uni(-5,20);args.damping=rng()%3?.6f:uni(0,3);args.restitution=rng()%3?0.f:uni(-1,1);args.flipMask=rng()%3?0u:uint32_t(rng());
    args.minimum=rng()%3?-1.f:uni(-5,10);args.maximum=rng()%3?-1.f:uni(-5,40);
    auto e=originalAnimTeeterConstruct(model,instance,scale,args);
    e.torque=rng()%3?0.f:uni(-1e6,1e6);e.velocity=rng()%4?uni(-6,6):(rng()%2?0.f:uni(-.12f,.12f));e.time=pick(model);e.sampleTime=rng()%2?e.time:pick(model);
    e.previous=uni(-1,2);e.unclamped=uni(-1,2);e.step=uni(-.1f,.1f);e.dirty=rng()%3!=0;e.evaluated=int32_t(rng()%2);
    for(auto& c:e.channels)for(size_t k=0;k<c.track->curves.size();k++)c.segment[k]=int32_t(rng()%c.track->curves[k].size());
    for(auto& m:e.matrices)for(auto& r:m)for(auto& x:r)x=uni(-9,9);
    if(!e.dirty)originalAnimComposeNodes(e); // a consistent clean state: matrices = compose(current channels)
    return e;
}
static RollerMatrix randomInstance(){RollerMatrix m=rotation();m[3]={uni(-3e5,3e5),uni(-3e5,3e5),uni(-3e5,3e5),1};return m;}

int main(int argc,char** argv){
    if(argc<2){fprintf(stderr,"usage: reference FOLDER\n");return 2;}
    std::string folder=argv[1];live::Machine machine(folder+"/synthetic.ee",folder+"/synthetic.vuc",folder+"/synthetic.vud");M=&machine;
    for(uint32_t pc:{0x317d70u,0x317e30u,0x317e50u,0x34fb00u,0x2d1ac8u,0x2d1be0u,0x2d1bd8u,0x328f28u,0x328c20u,0x3284b8u,0x3291e0u})
        if(!machine.runtime.registerFunction(pc,stub))throw std::runtime_error("stub registration");
    put(0x4FF120,RollerQuad{0,0,0,0});put(0x4FF150,RollerQuad{0,1,0,0});putMatrix(0x4FF1A0,rail_modifier_math::identity());
    put(gp-0x848,WORLD);put(gp+0x2A74,WORLD);put(WORLD+0x10,int32_t(60));
    unsigned failures=0,printed=0,total=0;
    // ---- A. node matrices (3610E0 -> 34DC90 -> 34E348/351800/351538/351A80/31BE50 + 34DD18) ----
    {
        unsigned bad=0,cases=0,clean=0;
        for(unsigned n=0;n<20000;n++,cases++){
            auto model=randomModel();writeModel(model);auto inst=randomInstance();float scale=rng()%3?1.f:uni(.5f,2);
            writeInstance(inst,scale,{0,0,0},{1,1,1});
            auto e=randomTeeter(model,inst,scale);clean+=!e.dirty;writeTeeter(e);
            int32_t node=int32_t(rng()%model.nodes.size());
            machine.call(0x3610e0,{OBJ+0x30,uint32_t(node)});
            originalAnimNodeMatrix(e,node);
            unsigned d=compareTeeter(e,"matrix",n,printed,true);bad+=d!=0;
        }
        printf("A 3610E0 node matrices (channel eval 351800/351538/351A80/31BE50, compose 34DD18): %u/%u cases identical (%u clean)\n",cases-bad,cases,clean);
        failures+=bad;total+=cases;
    }
    // ---- B. teeter update 342358 ----
    {
        unsigned bad=0,cases=0,clamped=0;
        for(unsigned n=0;n<40000;n++,cases++){
            auto model=randomModel();writeModel(model);auto inst=randomInstance();writeInstance(inst,1,{0,0,0},{1,1,1});
            auto e=randomTeeter(model,inst,1);float dt=rng()%3?std::bit_cast<float>(0x3c888889u):uni(0,.1f);
            if(rng()%4==0){e.time=rng()%2?e.minimum:e.maximum;e.velocity=uni(-.3f,.3f);}
            put(WORLD+0x14,dt);writeTeeter(e);
            auto c=machine.call(0x342358,{OBJ});bool r=originalAnimTeeterUpdate(e,dt);
            clamped+=e.time==e.minimum||e.time==e.maximum;
            unsigned d=compareTeeter(e,"update",n,printed,false);if(GPR_U32((&c),2)!=uint32_t(r))d++;bad+=d!=0;
        }
        printf("B 342358 teeter update: %u/%u cases identical (%u at a range end)\n",cases-bad,cases,clamped);
        failures+=bad;total+=cases;
    }
    // ---- C. apply force 342538 ----
    {
        unsigned bad=0,cases=0,applied=0;
        for(unsigned n=0;n<30000;n++,cases++){
            auto model=randomModel();writeModel(model);auto inst=randomInstance();float scale=rng()%4?1.f:uni(.5f,2);writeInstance(inst,scale,{0,0,0},{1,1,1});
            auto e=randomTeeter(model,inst,scale);writeTeeter(e);
            int32_t node=rng()%10==0?-1:int32_t(rng()%model.nodes.size());
            RollerQuad point{inst[3][0]+uni(-800,800),inst[3][1]+uni(-800,800),inst[3][2]+uni(-800,800),rng()%3?1.f:uni(-2,2)};
            float s=rng()%4?uni(90,110):std::pow(10.f,uni(1,5));RollerQuad force{uni(-1,1)*s,uni(-1,1)*s,uni(-1,1)*s,rng()%3?0.f:uni(-50,50)};
            put(CONTACT,point);put(CONTACT+0x5C,node);put(FORCE,force);
            float before=e.torque;
            machine.call(0x342538,{OBJ,CONTACT,FORCE});originalAnimTeeterApplyForce(e,node,point,force);applied+=e.torque!=before;
            unsigned d=compareTeeter(e,"force",n,printed,!e.dirty);bad+=d!=0;
        }
        printf("C 342538 apply force: %u/%u cases identical (%u changed the torque)\n",cases-bad,cases,applied);
        failures+=bad;total+=cases;
    }
    // ---- D. AnimTeeter constructor 3421A0 (34D9B0/355280/34E448/351508/34E348) ----
    {
        unsigned bad=0,cases=0;
        for(unsigned n=0;n<10000;n++,cases++){
            auto model=randomModel();writeModel(model);auto inst=randomInstance();float scale=rng()%3?1.f:uni(.5f,2);writeInstance(inst,scale,{0,0,0},{1,1,1});
            OriginalAnimTeeterArgs a;a.gain=rng()%2?1000.f:uni(-3000,3000);a.restitution=rng()%2?0.f:uni(-1,1);a.maxSpeed=rng()%2?120.f:uni(-10,400);
            a.stiffness=uni(0,2);a.rest=uni(-10,10);a.damping=uni(0,2);a.flipMask=uint32_t(rng());a.minimum=rng()%2?-1.f:uni(-5,10);a.maximum=rng()%2?-1.f:uni(-5,40);
            if(rng()%5==0)a.minimum=0;
            const float block[]={0,a.gain,a.restitution,a.maxSpeed,a.stiffness,a.rest,a.damping,0,a.minimum,a.maximum};
            for(unsigned k=0;k<10;k++)put(ARGS+4*k,block[k]);put(ARGS+0x1C,a.flipMask);put(ARGS,uint32_t(0xffffffff));
            for(uint32_t o=0;o<0x80;o+=4)put(OBJ+o,uint32_t(rng()));heap=HEAP;
            machine.call(0x3421a0,{OBJ,1,INST,ARGS});
            auto e=originalAnimTeeterConstruct(model,inst,scale,a);
            // The ctor's allocations are the stub heap: channel array word 0x78, matrices 0x74.
            uint32_t channels=word(OBJ+0x78),matrices=word(OBJ+0x74);unsigned d=diffWords(OBJ,teeterWords(e),"ctor",n,printed);
            if(!(M->get<uint16_t>(OBJ+0x56)&1)||word(OBJ+0x70)!=e.channels.size()||word(OBJ+0x50)!=0x4908f8)d++;
            for(size_t i=0;i<e.channels.size();i++){
                const auto& c=e.channels[i];std::vector<uint32_t> w;for(unsigned k=0;k<16;k++)w.push_back(uint32_t(c.segment[k]));
                for(unsigned k=0;k<16;k++)w.push_back(bitsOf(c.value[k]));
                d+=diffWords(channels+0xD0*uint32_t(i),std::vector<uint32_t>(w.begin(),w.begin()+16),"ctor-seg",n,printed);
                d+=diffWords(channels+0xD0*uint32_t(i)+0x44,std::vector<uint32_t>(w.begin()+16,w.end()),"ctor-val",n,printed);
                w.clear();for(auto& r:c.matrix)for(float x:r)w.push_back(bitsOf(x));d+=diffWords(channels+0xD0*uint32_t(i)+0x90,w,"ctor-mat",n,printed);
            }
            (void)matrices;
            uint32_t flags=word(INST+8);if(flags!=0x10005u){d++;if(printed++<30)printf("  ctor flags %08x\n",flags);}
            bad+=d!=0;
        }
        printf("D 3421A0 AnimTeeter constructor: %u/%u cases identical (fields, channels, instance flags 0x10003 -> 0x10005)\n",cases-bad,cases);
        failures+=bad;total+=cases;
    }
    // ---- E. RailModifier constructor 35B708 (35C0E8 rest inverse, 35C040), 35C4E0, 35C5A0 ----
    {
        unsigned bad=0,cases=0,removed=0;
        for(unsigned n=0;n<20000;n++,cases++){
            auto model=randomModel();writeModel(model);auto inst=randomInstance();float scale=rng()%3?1.f:uni(.5f,2);
            std::array<float,3> low{inst[3][0]-uni(0,900),inst[3][1]-uni(0,900),inst[3][2]-uni(0,900)},high{inst[3][0]+uni(0,900),inst[3][1]+uni(0,900),inst[3][2]+uni(0,900)};
            writeInstance(inst,scale,low,high);
            auto e=randomTeeter(model,inst,scale);writeTeeter(e);
            int32_t node=int32_t(rng()%model.nodes.size());uint32_t packed=(uint32_t(rng()%200)<<8)|8u;
            // stage chain: [CHAIN]=CHAIN+0x10, [+8]=table, table[8]=kind, [kind+0x44]=list, list[rid]=(DESC>>2)<<8
            put(CHAIN,CHAIN+0x10);put(CHAIN+0x18,CHAIN+0x20);put(CHAIN+0x20+4*8,CHAIN+0x100);put(CHAIN+0x144,CHAIN+0x200);
            put(CHAIN+0x200+4*(packed>>8),uint32_t((DESC>>2)<<8));
            unsigned segments=1+rng()%3;put(DESC+0x20,segments);put(DESC+0x24,SEGS);put(DESC+0x1C,uint32_t(rng()%3?1:3));put(DESC+0x28,int32_t(rng()%5)-1);
            for(unsigned s=0;s<segments;s++){put(SEGS+0x90*s+0x64,uint32_t(SEGS+0x90*(s+1)));for(unsigned k=0;k<6;k++)put(SEGS+0x90*s+0x6C+4*k,uni(-1e5,1e5));}
            put(TREE+0x10,LEAF);put(LEAF,uint32_t(1));put(TREE+0xA0,uint32_t(0));
            put(ARGS,uint32_t(0xffffffff));put(ARGS+4,packed);put(ARGS+8,node);
            for(uint32_t o=0;o<0x90;o+=4)put(RAIL+o,uint32_t(rng()));events.clear();
            machine.call(0x35b708,{RAIL,ARGS,INST});
            unsigned removes=0;for(auto& ev:events)removes+=ev.pc==0x3284b8;removed+=removes;
            auto m=originalRailModifierConstruct(packed,node,INST,model,inst,scale,low,high);
            std::vector<uint32_t> w;for(auto& q:{m.boundsMin,m.boundsMax})for(float x:q)w.push_back(bitsOf(x));
            unsigned d=diffWords(RAIL+0x10,w,"rail-bounds",n,printed);
            w.clear();for(auto& r:m.restInverse)for(float x:r)w.push_back(bitsOf(x));d+=diffWords(RAIL+0x50,w,"rail-inverse",n,printed);
            if(word(RAIL+0x30)!=packed||M->get<int32_t>(RAIL+0x34)!=node||word(RAIL+0x40)!=INST||word(RAIL+8)!=0x4911d0||removes!=segments)d++;
            // 35C4E0: bounds unchanged (entity reports none), relocation 3291E0 with the same bounds.
            events.clear();machine.call(0x35c4e0,{RAIL});
            d+=diffWords(RAIL+0x10,std::vector<uint32_t>(w.begin(),w.begin()),"rail-tick",n,printed);
            {std::vector<uint32_t> b;for(auto& q:{m.boundsMin,m.boundsMax})for(float x:q)b.push_back(bitsOf(x));d+=diffWords(RAIL+0x10,b,"rail-tick",n,printed);}
            if(events.size()!=1||events[0].pc!=0x3291e0)d++;
            // 35C5A0 with the entity's current node world (34FED8 -> vtable+0xEC).
            machine.call(0x35c5a0,{RAIL,OUT});
            RollerMatrix t=originalRailModifierTransform(m,e);
            w.clear();for(auto& r:t)for(float x:r)w.push_back(bitsOf(x));d+=diffWords(OUT,w,"rail-transform",n,printed);
            d+=compareTeeter(e,"rail-transform-teeter",n,printed,true);
            bad+=d!=0;
        }
        printf("E 35B708 RailModifier ctor (+35C0E8/35C040, %u static segment removals), 35C4E0 tick, 35C5A0 transform: %u/%u cases identical\n",removed,cases-bad,cases);
        failures+=bad;total+=cases;
    }
    // ---- F. type-2 rail query 35C698 ----
    {
        unsigned bad=0,cases=0,hits=0,masked=0;
        for(unsigned n=0;n<40000;n++,cases++){
            auto model=randomModel();writeModel(model);auto inst=randomInstance();float scale=rng()%4?1.f:uni(.5f,2);writeInstance(inst,scale,{0,0,0},{1,1,1});
            auto e=randomTeeter(model,inst,scale);writeTeeter(e);
            int32_t node=int32_t(rng()%model.nodes.size());uint32_t packed=(uint32_t(rng()%200)<<8)|8u;
            OriginalRailModifier m;m.packedId=packed;m.node=node;m.instance=INST;m.restInverse=rotation(uni(.9f,1.1f));m.restInverse[3]={uni(-3e5,3e5),uni(-3e5,3e5),uni(-3e5,3e5),1};
            if(rng()%3==0)m.restInverse=originalRailModifierRestInverse(model,node,inst,scale);
            put(RAIL+0x30,packed);put(RAIL+0x34,node);put(RAIL+0x40,INST);putMatrix(RAIL+0x50,m.restInverse);
            put(CHAIN,CHAIN+0x10);put(CHAIN+0x18,CHAIN+0x20);put(CHAIN+0x20+4*8,CHAIN+0x100);put(CHAIN+0x144,CHAIN+0x200);
            put(CHAIN+0x200+4*(packed>>8),uint32_t((DESC>>2)<<8));
            OriginalRailRecord record;record.packedId=packed;record.flags=rng()%8?1u:(rng()%2?2u:3u);record.surface=int32_t(rng()%6)-1;
            unsigned segments=1+rng()%3;std::vector<RollerMatrix> rows;
            RollerQuad start{uni(-300,300),uni(-300,300),uni(-300,300),1};
            for(unsigned s=0;s<segments;s++){
                RollerMatrix r;RollerQuad dir{uni(-400,400),uni(-400,400),uni(-100,100),0};
                r[0]={uni(-60,60),uni(-60,60),uni(-60,60),0};r[1]={uni(-80,80),uni(-80,80),uni(-80,80),0};r[2]=dir;r[3]=start;
                if(rng()%10==0)r[0]=r[1]=RollerQuad{0,0,0,0};
                rows.push_back(r);for(unsigned k=0;k<3;k++)start[k]+=r[0][k]+r[1][k]+r[2][k];
            }
            put(DESC+0x1C,record.flags);put(DESC+0x20,segments);put(DESC+0x24,SEGS);put(DESC+0x28,record.surface);
            for(unsigned s=0;s<segments;s++){putMatrix(SEGS+0x90*s+0x10,rows[s]);put(SEGS+0x90*s+0x64,uint32_t(SEGS+0x90*(s+1)));}
            // query point near the transformed rail (or anywhere)
            RollerMatrix tm=originalRailModifierTransform(m,e);
            RollerQuad onRail=roller_math::transform(rail_modifier_math::product(rows[rng()%segments],tm),{0,0,0,1});
            RollerQuad q{onRail[0]+uni(-400,400),onRail[1]+uni(-400,400),onRail[2]+uni(-400,400),rng()%4?1.f:uni(-2,2)};
            if(rng()%8==0)q={uni(-3e5,3e5),uni(-3e5,3e5),uni(-3e5,3e5),1};
            uint32_t mask=rng()%10?1u:2u;bool found=rng()%3==0;float best=found?uni(0,600):uni(-1,1);
            put(POINT,q);put(FOUND,uint32_t(found));put(BEST,best);
            for(uint32_t o=0;o<0x74;o+=4)put(OUT+o,uint32_t(0xdeadbeef));
            writeTeeter(e); // the port transform above cleaned a copy only
            machine.call(0x35c698,{RAIL,0,0,POINT,mask,FOUND,BEST,OUT});
            OriginalRailModifierHit hit;bool pf=found;float pb=best;bool before=found;float beforeBest=best;
            originalRailModifierQuery(m,record,rows,originalRailModifierTransform(m,e),q,mask,pf,pb,hit);
            unsigned d=0;
            if(word(FOUND)!=uint32_t(pf)||word(BEST)!=bitsOf(pb)){d++;if(printed++<30)printf("  query case %u found %u/%u best %g/%g\n",n,word(FOUND),pf,real(BEST),pb);}
            bool replaced=pf&&(!before||pb!=beforeBest||hit.record);
            if(hit.record){
                hits++;
                std::vector<uint32_t> w;for(float x:hit.point)w.push_back(bitsOf(x));for(float x:hit.tangent)w.push_back(bitsOf(x));
                for(unsigned k=0;k<8;k++)w.push_back(0);
                d+=diffWords(OUT,w,"query-out",n,printed);
                const uint32_t expect[]={2,0xdeadbeefu,uint32_t(record.surface),INST,0,DESC,uint32_t(node),0xffffffffu,0xffffffffu,bitsOf(hit.t),0,0};
                d+=diffWords(OUT+0x44,std::vector<uint32_t>(std::begin(expect),std::end(expect)),"query-fields",n,printed);
            }else if(word(OUT)!=0xdeadbeef){d++;if(printed++<30)printf("  query case %u original wrote a result the port did not\n",n);}
            (void)replaced;masked+=!(record.flags&mask);
            bad+=d!=0;
        }
        printf("F 35C698 type-2 rail query: %u/%u cases identical (%u results written, %u mask-rejected)\n",cases-bad,cases,hits,masked);
        failures+=bad;total+=cases;
    }
    printf("total %u cases, %u failing\n",total,failures);
    if(getenv("COUNTS"))for(auto [pc,n]:callCounts)fprintf(stderr,"%06x %llu\n",pc,(unsigned long long)n);
    return failures?1:0;
}
