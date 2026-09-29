// Instruction oracle for engine/rail_snap_torque.hpp on a synthetic EE image (ELF loaded; BSS
// constants 0x4FF120/0x4FF150/0x4FF1A0 as in the savestates; *(gp-0x848) = *(gp+0x2A74) = clock
// with +0x10 = 60):
//  A. 0x34E698 AnimTeeter contact velocity (0x356AE0, 0x34E798 node velocity table with 0x351660
//     / 0x3424D0 / 0x31BE50 / 0x3610E0, 0x34E600) vs originalAnimTeeterContactVelocity: random
//     models/teeter states/nodes/points, stale tables when +0x40 "evaluated" is clear.
//  B. The complete 0x106F78 with a real teeter entity behind the rail (out+0x50 = instance, +0x5C
//     node): real 0x1231A8, entity vtable+0x154 0x34E698 and +0x15C 0x342538; recording stubs for
//     0x11FE98, 0x334680 (scripted result), 0x108A48, 0x106538, 0x1065B0, 0x11E098, 0x105D98.
//     Port: originalRailSnapContact with the teeter hooks. Compared: result, rider velocity,
//     callee order/arguments, the teeter object (torque) and the velocity table.
// ~10,000 generated registrations (0x106F78 reaches most of the rider code): compile them without
// optimisation, the oracle code itself stays -O1.
#pragma clang optimize off
#include "live_registrations.inc"
#pragma clang optimize on
#include "original_live_runtime.hpp"
#include "../engine/rail_snap_torque.hpp"
#include <random>
using namespace ssx;
static live::Machine* M;
static std::mt19937 rng(0x106F78);
static float uni(float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);}
static uint32_t bitsOf(float f){return std::bit_cast<uint32_t>(f);}
static constexpr uint32_t gp=0x4a30f0,MODEL=0x1000000,INST=0x1010000,OBJ=0x1020000,WORLD=0x1030000,VEL=0x1036000,OUT=0x1070400,
    RIDER=0x1200000,OWNER=0x1210000,SKELETON=0x1220000,BONES=0x1230000,RAILS=0x1240000;
struct Event {uint32_t pc;std::vector<uint32_t> w;bool operator==(const Event&)const=default;};
static std::vector<Event> events;
static void put(uint32_t a,const auto& v){M->put(a,v);}
static uint32_t word(uint32_t a){return M->get<uint32_t>(a);}
static float real(uint32_t a){return M->get<float>(a);}
static RollerQuad quad(uint32_t a){return M->get<RollerQuad>(a);}
static void putMatrix(uint32_t a,const RollerMatrix& m){for(unsigned i=0;i<4;i++)put(a+16*i,m[i]);}
static std::vector<uint32_t> xyz(uint32_t a){return {word(a),word(a+4),word(a+8)};}
static std::vector<uint32_t> xyz(const RailVector& v){return {bitsOf(v[0]),bitsOf(v[1]),bitsOf(v[2])};}
static std::vector<uint32_t> xyz(const ContactQuad& v){return {bitsOf(v[0]),bitsOf(v[1]),bitsOf(v[2])};}
static void append(std::vector<uint32_t>& a,const std::vector<uint32_t>& b){a.insert(a.end(),b.begin(),b.end());}
static std::vector<uint32_t> recordWords(uint32_t a){std::vector<uint32_t> w;for(uint32_t o:{0u,16u,32u})append(w,xyz(a+o));w.push_back(word(a+12));w.push_back(word(a+48));return w;}
static std::vector<uint32_t> recordWords(const OriginalInstanceContactRecord& r){std::vector<uint32_t> w;append(w,xyz(r.point));append(w,xyz(r.direction));append(w,xyz(r.normal));w.push_back(bitsOf(r.point[3]));w.push_back(bitsOf(r.closingSpeed));return w;}

// ---- scripted 106F78 callees ----
static int stubMode=0;static bool stubAttach=false;static OriginalRailQueryResult scripted;static int32_t scriptedNode=-1;static uint32_t scriptedInstance=0,boneRecord=0;
static void stub(uint8_t*,R5900Context* c,PS2Runtime*){
    const uint32_t pc=c->pc,a0=GPR_U32(c,4),a1=GPR_U32(c,5),a2=GPR_U32(c,6),a3=GPR_U32(c,7);
    switch(pc){
    case 0x11fe98:if(a0!=RIDER)throw std::runtime_error("11FE98 this");SET_GPR_U32(c,2,uint32_t(stubMode));break;
    case 0x334680:{
        if(a0!=RAILS||a1!=boneRecord||a3!=1||bitsOf(c->f[12])!=bitsOf(300.f))throw std::runtime_error("334680 arguments");
        events.push_back({0x334680,xyz(a1)});
        for(uint32_t o=0;o<0x70;o+=4)put(a2+o,uint32_t(0xcdcdcdcd));
        const auto& r=scripted;put(a2,RollerQuad{r.point[0],r.point[1],r.point[2],1});put(a2+16,RollerQuad{r.tangent[0],r.tangent[1],r.tangent[2],0});
        put(a2+32,RollerQuad{0,0,0,0});put(a2+48,RollerQuad{0,0,0,0});put(a2+0x44,uint32_t(2));put(a2+0x4c,r.surface);put(a2+0x50,scriptedInstance);put(a2+0x5c,scriptedNode);
        SET_GPR_U32(c,2,r.found?1u:0u);break;}
    case 0x108a48:if(a0!=RIDER)throw std::runtime_error("108A48 this");events.push_back({0x108a48,{}});SET_GPR_U32(c,2,stubAttach?1u:0u);break;
    case 0x106538:if(a0!=RIDER)throw std::runtime_error("106538 this");events.push_back({0x106538,xyz(a1)});break;
    case 0x1065b0:if(a0!=RIDER)throw std::runtime_error("1065B0 this");events.push_back({0x1065b0,xyz(a1+16)});break;
    case 0x11e098:if(a0!=RIDER)throw std::runtime_error("11E098 this");events.push_back({0x11e098,{}});break;
    case 0x105d98:{if(a0!=RIDER)throw std::runtime_error("105D98 this");auto w=recordWords(a1);w.push_back(a2);events.push_back({0x105d98,w});break;}
    default:throw std::runtime_error("unexpected stub");
    }
    c->pc=GPR_U32(c,31);
}

// ---- random teeters (as tests/rail_modifier_reference.cpp) ----
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
        if(rng()%4){
            OriginalAnimTrack t;for(unsigned k=0;k<6;k++)t.base[k]=k<3?uni(-50,50):(rng()%3?0.f:uni(-180,180));
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
static void writeInstance(const RollerMatrix& matrix,float scale){
    putMatrix(INST+0x10,matrix);put(INST+0x80,MODEL);put(INST+0x84,scale);put(INST+8,uint32_t(0x10005));put(INST+0xC,OBJ+0x44);
}
static void writeTeeter(const OriginalAnimTeeter& e){
    const float f[]={e.torque,e.step,e.gain,e.restitution,e.minimum,e.maximum,e.maxStep,e.damping,e.stiffness,e.rest,e.velocity};
    for(unsigned k=0;k<11;k++)put(OBJ+4*k,f[k]);put(OBJ+0x2C,e.flipMask);
    put(OBJ+0x30,e.time);put(OBJ+0x34,e.sampleTime);put(OBJ+0x38,e.previous);put(OBJ+0x3C,e.unclamped);put(OBJ+0x40,e.evaluated);
    put(OBJ+0x50,uint32_t(0x4908f8));put(OBJ+0x54,uint16_t(4));put(OBJ+0x56,uint16_t(e.dirty?1:0));put(OBJ+0x5C,INST);put(OBJ+0x60,uint32_t(0));
    uint32_t channels=OBJ+0x1000,matrices=OBJ+0x800;
    put(OBJ+0x70,uint32_t(e.channels.size()));put(OBJ+0x74,matrices);put(OBJ+0x78,e.channels.empty()?0u:channels);put(OBJ+0x7C,VEL);
    for(size_t i=0;i<e.channels.size();i++){
        const auto& c=e.channels[i];uint32_t at=channels+0xD0*uint32_t(i);
        for(unsigned k=0;k<16;k++){put(at+4*k,c.segment[k]);put(at+0x44+4*k,c.value[k]);}
        uint32_t t=0;size_t seen=0;for(size_t n=0;n<e.model->nodes.size();n++)if(e.model->nodes[n].track){if(seen==i)t=word(MODEL+0x100+16*uint32_t(n)+8);seen++;}
        put(at+0x40,t);put(at+0x84,uint32_t(0x490ad0));putMatrix(at+0x90,c.matrix);
    }
    for(size_t i=0;i<e.matrices.size();i++)putMatrix(matrices+64*uint32_t(i),e.matrices[i]);
}
static void writeTable(const OriginalAnimNodeVelocityTable& t){for(size_t i=0;i<t.nodes.size();i++){put(VEL+32*uint32_t(i),t.nodes[i].linear);put(VEL+32*uint32_t(i)+16,t.nodes[i].angular);}}
static unsigned diffWords(uint32_t at,const std::vector<uint32_t>& port,const char* what,unsigned n,unsigned& printed){
    unsigned bad=0;
    for(size_t k=0;k<port.size();k++)if(word(at+4*uint32_t(k))!=port[k]){
        if(printed<40){printf("  %s case %u +%03zx original %08x (%g) port %08x (%g)\n",what,n,4*k,word(at+4*uint32_t(k)),real(at+4*uint32_t(k)),port[k],std::bit_cast<float>(port[k]));printed++;}
        bad++;
    }
    return bad;
}
static std::vector<uint32_t> quadWords(const RollerQuad& q){return {bitsOf(q[0]),bitsOf(q[1]),bitsOf(q[2]),bitsOf(q[3])};}
static unsigned compareState(const OriginalAnimTeeter& e,const OriginalAnimNodeVelocityTable& table,bool tableValid,const char* what,unsigned n,unsigned& printed){
    unsigned bad=0;
    const float f[]={e.torque,e.step,e.gain,e.restitution,e.minimum,e.maximum,e.maxStep,e.damping,e.stiffness,e.rest,e.velocity};
    std::vector<uint32_t> w;for(float x:f)w.push_back(bitsOf(x));w.push_back(e.flipMask);for(float x:{e.time,e.sampleTime,e.previous,e.unclamped})w.push_back(bitsOf(x));w.push_back(uint32_t(e.evaluated));
    bad+=diffWords(OBJ,w,what,n,printed);
    if(bool(M->get<uint16_t>(OBJ+0x56)&1)!=e.dirty){bad++;if(printed++<40)printf("  %s case %u dirty\n",what,n);}
    for(size_t i=0;i<e.channels.size();i++){
        w.clear();for(unsigned k=0;k<16;k++)w.push_back(uint32_t(e.channels[i].segment[k]));bad+=diffWords(OBJ+0x1000+0xD0*uint32_t(i),w,what,n,printed);
    }
    if(!e.dirty)for(size_t i=0;i<e.matrices.size();i++){w.clear();for(auto& r:e.matrices[i])for(float x:r)w.push_back(bitsOf(x));bad+=diffWords(OBJ+0x800+64*uint32_t(i),w,what,n,printed);}
    if(tableValid)for(size_t i=0;i<table.nodes.size();i++){
        w=quadWords(table.nodes[i].linear);auto a=quadWords(table.nodes[i].angular);w.insert(w.end(),a.begin(),a.end());
        bad+=diffWords(VEL+32*uint32_t(i),w,"table",n,printed);
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
    if(rng()%5==0)e.time=rng()%2?e.minimum:e.maximum;
    e.previous=uni(-1,2);e.unclamped=uni(-1,2);e.step=rng()%6?uni(-.1f,.1f):0.f;e.dirty=rng()%3!=0;e.evaluated=int32_t(rng()%2);
    for(auto& c:e.channels)for(size_t k=0;k<c.track->curves.size();k++)c.segment[k]=int32_t(rng()%c.track->curves[k].size());
    for(auto& m:e.matrices)for(auto& r:m)for(auto& x:r)x=uni(-9,9);
    if(!e.dirty){originalAnimEvaluateChannels(e);originalAnimComposeNodes(e);e.evaluated=int32_t(rng()%2);} // a consistent clean state
    return e;
}
static RollerMatrix randomInstance(){RollerMatrix m=rotation();m[3]={uni(-3e5,3e5),uni(-3e5,3e5),uni(-3e5,3e5),1};return m;}
static OriginalAnimNodeVelocityTable randomTable(size_t n){OriginalAnimNodeVelocityTable t;t.nodes.resize(n);for(auto& v:t.nodes){v.linear={uni(-500,500),uni(-500,500),uni(-500,500),uni(-1,1)};v.angular={uni(-5,5),uni(-5,5),uni(-5,5),uni(-1,1)};}return t;}

int main(int argc,char** argv){
    if(argc<2){fprintf(stderr,"usage: reference FOLDER\n");return 2;}
    std::string folder=argv[1];live::Machine machine(folder+"/synthetic.ee",folder+"/synthetic.vuc",folder+"/synthetic.vud");M=&machine;
    for(uint32_t pc:{0x11fe98u,0x334680u,0x108a48u,0x106538u,0x1065b0u,0x11e098u,0x105d98u})
        if(!machine.runtime.registerFunction(pc,stub))throw std::runtime_error("stub registration");
    put(0x4FF120,RollerQuad{0,0,0,0});put(0x4FF150,RollerQuad{0,1,0,0});putMatrix(0x4FF1A0,rail_modifier_math::identity());
    put(gp-0x848,WORLD);put(gp+0x2A74,WORLD);put(WORLD+0x10,int32_t(60));put(WORLD+0x14,std::bit_cast<float>(0x3c888889u));
    unsigned failures=0,printed=0,total=0;
    // ---- A. 0x34E698 contact velocity ----
    {
        unsigned bad=0,cases=0,recomputed=0,moving=0;
        for(unsigned n=0;n<40000;n++,cases++){
            auto model=randomModel();writeModel(model);auto inst=randomInstance();float scale=rng()%3?1.f:uni(.5f,2);writeInstance(inst,scale);
            auto e=randomTeeter(model,inst,scale);
            auto table=randomTable(model.nodes.size());writeTable(table);writeTeeter(e);
            const bool willRecompute=e.evaluated!=0||e.dirty;
            int32_t node=rng()%10==0?-1:int32_t(rng()%model.nodes.size());
            RollerQuad point{inst[3][0]+uni(-800,800),inst[3][1]+uni(-800,800),inst[3][2]+uni(-800,800),rng()%3?1.f:uni(-2,2)};
            RollerQuad p20{uni(-100,100),uni(-100,100),uni(-100,100),0},p30{uni(-3,3),uni(-3,3),uni(-3,3),0};if(rng()%2){p20=p30=RollerQuad{0,0,0,0};}
            put(OUT,point);put(OUT+0x20,p20);put(OUT+0x30,p30);put(OUT+0x50,INST);put(OUT+0x5C,node);
            machine.call(0x34e698,{OBJ+0x30,OUT});
            // Port: the original recomputes only when +0x40 (evaluated) is set (a dirty entity is cleaned by
            // 0x34E798's own 0x34DC90, which sets it): the stale table stays otherwise.
            bool changedTable=false;
            if(node>=0){
                if(e.dirty&&!e.evaluated){/* 34E698 reads +0x10 before cleaning: a dirty entity with evaluated 0 keeps the stale table */}
                changedTable=e.evaluated!=0;
                if(changedTable){originalAnimNodeVelocities(e,originalAnimTeeterRate(e),table);recomputed++;}
                const RollerQuad origin=originalAnimNodeMatrix(e,node)[3];
                OriginalRounding rounding;const auto& v=table.nodes.at(size_t(node));
                p20=roller_math::vadd(p20,roller_math::vadd(v.linear,roller_math::cross(roller_math::vsub(point,origin),v.angular)));p30=roller_math::vadd(p30,v.angular);
                moving+=originalAnimTeeterRate(e)!=0;
            }
            unsigned d=diffWords(OUT+0x20,quadWords(p20),"packet20",n,printed)+diffWords(OUT+0x30,quadWords(p30),"packet30",n,printed);
            d+=compareState(e,table,true,"state",n,printed);
            (void)willRecompute;bad+=d!=0;
        }
        printf("A 34E698 teeter contact velocity (34E798 table, 351660, 3424D0, 34E600): %u/%u cases identical (%u tables recomputed, %u with a nonzero rate)\n",cases-bad,cases,recomputed,moving);
        failures+=bad;total+=cases;
    }
    // ---- A2. the library entry originalAnimTeeterContactVelocity (same state machine) ----
    {
        unsigned bad=0,cases=0;
        for(unsigned n=0;n<20000;n++,cases++){
            auto model=randomModel();writeModel(model);auto inst=randomInstance();float scale=rng()%3?1.f:uni(.5f,2);writeInstance(inst,scale);
            auto e=randomTeeter(model,inst,scale);auto table=randomTable(model.nodes.size());writeTable(table);writeTeeter(e);
            int32_t node=rng()%10==0?-1:int32_t(rng()%model.nodes.size());
            RollerQuad point{inst[3][0]+uni(-800,800),inst[3][1]+uni(-800,800),inst[3][2]+uni(-800,800),1};
            RollerQuad p20{0,0,0,0},p30{0,0,0,0};
            put(OUT,point);put(OUT+0x20,p20);put(OUT+0x30,p30);put(OUT+0x50,INST);put(OUT+0x5C,node);
            machine.call(0x34e698,{OBJ+0x30,OUT});
            originalAnimTeeterContactVelocity(e,table,node,point,p20,p30);
            unsigned d=diffWords(OUT+0x20,quadWords(p20),"entry20",n,printed)+diffWords(OUT+0x30,quadWords(p30),"entry30",n,printed)+compareState(e,table,true,"entry-state",n,printed);
            bad+=d!=0;
        }
        printf("A2 originalAnimTeeterContactVelocity: %u/%u cases identical\n",cases-bad,cases);
        failures+=bad;total+=cases;
    }
    // ---- B. complete 0x106F78 with the teeter entity ----
    {
        put(RIDER+0x77c,OWNER);put(RIDER+0x780,SKELETON);put(SKELETON+0x2c,BONES);put(RIDER+0x860,RAILS);
        unsigned bad=0,cases=0,stages[6]={},entityContacts=0,torqued=0;
        for(unsigned n=0;n<60000;n++,cases++){
            events.clear();
            auto model=randomModel();writeModel(model);auto inst=randomInstance();float scale=rng()%3?1.f:uni(.5f,2);writeInstance(inst,scale);
            auto e=randomTeeter(model,inst,scale);e.torque=rng()%2?0.f:uni(-1e5,1e5);auto table=randomTable(model.nodes.size());writeTable(table);writeTeeter(e);
            OriginalRailBodyContactRider state;state.motionMode=stubMode=int(std::array{0,1,2,4,0,1}[rng()%6]);state.ownerWord30=int(rng()%2);
            const uint32_t hips=rng()%4;put(RIDER+0x89c,hips);boneRecord=BONES+hips*32;
            std::array<float,4> q{uni(-1,1),uni(-1,1),uni(-1,1),uni(-1,1)};{float l=std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);for(auto& x:q)x/=l;}
            RailVector P{inst[3][0]+uni(-500,500),inst[3][1]+uni(-500,500),inst[3][2]+uni(-500,500)};
            state.hipsPosition=P;state.hipsQuaternion=q;put(boneRecord,RollerQuad{P[0],P[1],P[2],1});put(boneRecord+16,q);
            RailVector axis;{OriginalRounding r;axis=originalRailBoardAxes(q).y;}
            RailVector side{uni(-1,1),uni(-1,1),uni(-1,1)};{float l=std::sqrt(side[0]*side[0]+side[1]*side[1]+side[2]*side[2]);if(l>1e-3)for(auto& x:side)x/=l;}
            float along=uni(-80,80),off=rng()%6==0?uni(0,5):uni(0,70);
            scripted={};scripted.found=rng()%12!=0;for(unsigned k=0;k<3;k++)scripted.point[k]=P[k]+axis[k]*along+side[k]*off;
            scripted.tangent={uni(-1,1),uni(-1,1),uni(-1,1)};{float l=std::sqrt(scripted.tangent[0]*scripted.tangent[0]+scripted.tangent[1]*scripted.tangent[1]+scripted.tangent[2]*scripted.tangent[2]);if(l>1e-3)for(auto& x:scripted.tangent)x/=l;}
            scripted.surface=rng()%3?-1:int(rng()%19);
            const bool withEntity=rng()%5!=0;scriptedInstance=withEntity?INST:0;scriptedNode=rng()%8==0?-1:int32_t(rng()%model.nodes.size());
            state.velocity=n%25==0?RailVector{0,0,0}:RailVector{uni(-2500,2500),uni(-2500,2500),uni(-2500,2500)};
            state.groundNormal={uni(-1,1),uni(-1,1),uni(-1,1)};{float l=std::sqrt(state.groundNormal[0]*state.groundNormal[0]+state.groundNormal[1]*state.groundNormal[1]+state.groundNormal[2]*state.groundNormal[2]);if(l>1e-3)for(auto& x:state.groundNormal)x/=l;}
            stubAttach=rng()%5==0;
            put(OWNER+0x30,state.ownerWord30);put(RIDER+0x1e0,RollerQuad{state.velocity[0],state.velocity[1],state.velocity[2],0});put(RIDER+0x370,RollerQuad{state.groundNormal[0],state.groundNormal[1],state.groundNormal[2],0});
            auto c=machine.call(0x106f78,{RIDER});
            const uint32_t originalResult=GPR_U32((&c),2);auto originalEvents=events;events.clear();
            OriginalRailBodyContactHost host;
            host.query=[&](RailVector p){events.push_back({0x334680,xyz(p)});return scripted;};
            host.attachable=[&]{events.push_back({0x108a48,{}});return stubAttach;};
            host.translate=[&](const RailVector& d){events.push_back({0x106538,xyz(d)});};
            host.steer=[&](const RailVector& v){events.push_back({0x1065b0,xyz(v)});};
            host.rebuild=[&]{events.push_back({0x11e098,{}});};
            host.notify=[&](const OriginalInstanceContactRecord& r,int kind){auto w=recordWords(r);w.push_back(uint32_t(kind));events.push_back({0x105d98,w});};
            OriginalRailSnapEntityHooks hooks;
            hooks.hasEntity=[&](const OriginalRailQueryResult&){return withEntity;};
            hooks.contactVelocity=[&](const OriginalRailQueryResult& h,RollerQuad& o20,RollerQuad& o30){originalAnimTeeterContactVelocity(e,table,scriptedNode,{h.point[0],h.point[1],h.point[2],1.f},o20,o30);};
            hooks.applyForce=[&](const OriginalRailQueryResult& h,const RollerQuad& force){originalAnimTeeterApplyForce(e,scriptedNode,{h.point[0],h.point[1],h.point[2],1.f},force);};
            const float torqueBefore=e.torque;
            auto result=originalRailSnapContact(state,host,hooks);
            unsigned d=0;
            if(originalEvents!=events){d++;if(printed++<40){printf("  106F78 event mismatch case %u stage %d\n",n,result.stage);for(auto& ev:originalEvents){printf("   o %x:",ev.pc);for(auto w:ev.w)printf(" %08x",w);puts("");}for(auto& ev:events){printf("   p %x:",ev.pc);for(auto w:ev.w)printf(" %08x",w);puts("");}}}
            if(originalResult!=(result.contact?1u:0u)){d++;if(printed++<40)printf("  106F78 result mismatch case %u\n",n);}
            if(xyz(RIDER+0x1e0)!=xyz(state.velocity)){d++;if(printed++<40)printf("  106F78 velocity mismatch case %u stage %d: original %g %g %g port %g %g %g\n",n,result.stage,real(RIDER+0x1e0),real(RIDER+0x1e4),real(RIDER+0x1e8),state.velocity[0],state.velocity[1],state.velocity[2]);}
            d+=compareState(e,table,result.entity&&scriptedNode>=0,"106F78-teeter",n,printed);
            ++stages[result.stage];entityContacts+=result.entity;torqued+=e.torque!=torqueBefore;
            bad+=d!=0;
        }
        printf("B 106F78 with a teeter entity (real 34E698 / 342538 / 1231A8): %u/%u cases identical (motion4 %u, no rail %u, behind %u, beyond 50 cm %u, attachable %u, contact %u; %u entity contacts, %u torqued)\n",
            cases-bad,cases,stages[0],stages[1],stages[2],stages[3],stages[4],stages[5],entityContacts,torqued);
        failures+=bad;total+=cases;
    }
    printf("total %u cases, %u failing\n",total,failures);
    return failures?1:0;
}
