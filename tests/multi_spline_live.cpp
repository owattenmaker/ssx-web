// Live MultiSplineModifier parity (development oracle): engine/multi_spline_modifier.hpp and
// engine/original_spline_path.hpp against the full recompiled original on the Snow Jam
// event savestate snow-jam-ready (race tick 0, both chairlift modifiers live).
//  1. Lockstep: machine A runs the original 0x35A560 (modifier vtable+0x14) and the LiveComp
//     entity update 0x3568B0 (-> 0x361C20/0x35AC20 evaluation, car-0 bounds, 0x35A918) for
//     both modifiers per tick; the port runs originalMultiSplineUpdate + originalMultiSplineBounds.
//     Compared per tick: every modifier word the port models, all car records and every clone
//     instance matrix/bounds; and against the capture records' watch windows
//     (local/ps2-capture/runs/setpieces/race.bin: 0x592E80, 0x593580, 0x5CF200, 0x5CEE00, 0xBAB330).
//  2. Randomized on machine R: 0x345248 (random distances incl. negative/over-length, random
//     cursors), 0x35AC20 (random distance, rotation angle, mode 0..3), 0x35B200 (random contact
//     points on each car).
#include "live_registrations.inc"
#include "original_live_runtime.hpp"
#include "../engine/multi_spline_modifier.hpp"
#include <map>
#include <random>
using namespace ssx;
static constexpr uint32_t arena=0xE0000;
struct Lift {uint32_t modifier,entity;const char* name;};
static const Lift lifts[2]={{0x592E80,0x592DB0,"tramlores_0"},{0x593580,0x5934B0,"tramlores_1"}};
static uint32_t bits(float x){return std::bit_cast<uint32_t>(x);}
// Spline record from the runtime segments (chain from the head, 0x3451C0 cursor at +0x50).
static OriginalRailRecord readPath(const live::Machine& m,uint32_t modifier){
    uint32_t seg=m.get<uint32_t>(modifier+0x50);while(m.get<uint32_t>(seg+0x60))seg=m.get<uint32_t>(seg+0x60);
    OriginalRailRecord r;r.packedId=m.get<uint32_t>(modifier+0x48);int32_t index=0;
    for(;seg;seg=m.get<uint32_t>(seg+0x64)){
        OriginalRailSegment s;s.index=index++;s.length=m.get<float>(seg+0xC);s.distance=m.get<float>(seg+0x84);
        for(unsigned row=0;row<4;row++){for(unsigned k=0;k<3;k++)s.coefficients[row][k]=m.get<float>(seg+0x10+16*row+4*k);
            if(m.get<float>(seg+0x10+16*row+12)!=(row==3?1.f:0.f))throw std::runtime_error("segment w lanes");}
        for(unsigned k=0;k<4;k++)s.arcToParameter[k]=m.get<float>(seg+0x50+4*k);
        r.segments.push_back(s);
    }
    return r;
}
static OriginalMultiSplineModifier readModifier(const live::Machine& m,uint32_t a,const OriginalRailRecord& path){
    OriginalMultiSplineModifier x;x.count=m.get<int32_t>(a+4);x.mode=m.get<int32_t>(a+8);x.angle=m.get<float>(a+0xC);x.distance=m.get<float>(a+0x10);
    x.speed=m.get<float>(a+0x14);x.radius=m.get<float>(a+0x18);x.dirty=m.get<uint32_t>(a+0x30);
    x.path.record=&path;x.path.resource=m.get<uint32_t>(a+0x48);x.path.index=m.get<int32_t>(a+0x4C);x.path.cursor=size_t(x.path.index);x.path.length=m.get<float>(a+0x54);
    uint32_t records=m.get<uint32_t>(a+0x3C),clones=m.get<uint32_t>(a+0x44);
    for(int32_t i=0;i<x.count;i++){
        OriginalMultiSplineCar c;uint32_t r=records+0x60*i,inst=m.get<uint32_t>(clones+4*i);
        c.position=m.get<SplineQuad>(r);c.tangent=m.get<SplineQuad>(r+0x10);c.curvature=m.get<SplineQuad>(r+0x20);c.boundsMin=m.get<SplineQuad>(r+0x30);c.boundsMax=m.get<SplineQuad>(r+0x40);
        c.tangentXX=m.get<float>(r+0x50);c.tangentYY=m.get<float>(r+0x54);c.record58=m.get<uint32_t>(r+0x58);c.record5C=m.get<uint32_t>(r+0x5C);
        for(unsigned k=0;k<4;k++)c.matrix[k]=m.get<RollerQuad>(inst+0x10+16*k);
        for(unsigned k=0;k<3;k++){c.instanceLow[k]=m.get<float>(inst+0x60+4*k);c.instanceHigh[k]=m.get<float>(inst+0x6C+4*k);}
        x.cars.push_back(c);
    }
    return x;
}
// Word-by-word comparison of the port's state with memory (modifier fields, records, clone matrices/bounds).
static unsigned compare(const OriginalMultiSplineModifier& x,const uint8_t* ee,uint32_t a,uint32_t records,uint32_t clones[3],FILE* log,const char* what,unsigned limit){
    unsigned bad=0;
    auto word=[&](uint32_t address,uint32_t value,const char* field){uint32_t v;std::memcpy(&v,ee+(address&0x1ffffff),4);if(v!=value){if(bad<limit)fprintf(log,"    %s %s @%x original %08x port %08x\n",what,field,address,v,value);bad++;}};
    auto quad=[&](uint32_t address,const SplineQuad& q,const char* field){for(unsigned k=0;k<4;k++)word(address+4*k,bits(q[k]),field);};
    word(a+4,uint32_t(x.count),"count");word(a+8,uint32_t(x.mode),"mode");word(a+0xC,bits(x.angle),"angle");word(a+0x10,bits(x.distance),"distance");
    word(a+0x14,bits(x.speed),"speed");word(a+0x18,bits(x.radius),"radius");word(a+0x30,x.dirty,"dirty");
    word(a+0x48,x.path.resource,"path");word(a+0x4C,uint32_t(x.path.index),"cursor");word(a+0x54,bits(x.path.length),"length");
    for(int32_t i=0;i<x.count;i++){
        const auto& c=x.cars[i];uint32_t r=records+0x60*i;
        quad(r,c.position,"position");quad(r+0x10,c.tangent,"tangent");quad(r+0x20,c.curvature,"curvature");quad(r+0x30,c.boundsMin,"boundsMin");quad(r+0x40,c.boundsMax,"boundsMax");
        word(r+0x50,bits(c.tangentXX),"xx");word(r+0x54,bits(c.tangentYY),"yy");
        for(unsigned k=0;k<4;k++)quad(clones[i]+0x10+16*k,c.matrix[k],"matrix");
        for(unsigned k=0;k<3;k++){word(clones[i]+0x60+4*k,bits(c.instanceLow[k]),"instLow");word(clones[i]+0x6C+4*k,bits(c.instanceHigh[k]),"instHigh");}
    }
    return bad;
}
int main(int argc,char** argv){
    if(argc<4){fprintf(stderr,"usage: live FOLDER CAPTURE TICKS [RANDOM]\n");return 2;}
    std::string folder=argv[1];int ticks=atoi(argv[3]);unsigned randomCases=argc>4?unsigned(atoi(argv[4])):20000;FILE* log=stdout;
    auto capture=live::readFile(argv[2]);
    std::map<uint32_t,size_t> captureRecords;for(size_t k=0;k+16384<=capture.size();k+=16384){uint32_t t;std::memcpy(&t,capture.data()+k+4,4);captureRecords[t]=k;}
    live::Machine A(folder+"/ready.ee",folder+"/ready.vuc",folder+"/ready.vud");
    const uint32_t clock=A.get<uint32_t>(0x4A30F0-0x848);
    OriginalRailRecord paths[2];OriginalMultiSplineModifier port[2];uint32_t clones[2][3],records[2];
    for(unsigned i=0;i<2;i++){
        if(A.get<uint32_t>(lifts[i].modifier)!=0x48F168||A.get<uint32_t>(A.get<uint32_t>(lifts[i].entity+0x1C))!=lifts[i].modifier)throw std::runtime_error("modifier layout");
        paths[i]=readPath(A,lifts[i].modifier);port[i]=readModifier(A,lifts[i].modifier,paths[i]);
        records[i]=A.get<uint32_t>(lifts[i].modifier+0x3C);for(unsigned k=0;k<3;k++)clones[i][k]=A.get<uint32_t>(A.get<uint32_t>(lifts[i].modifier+0x44)+4*k);
        if(port[i].count!=3)throw std::runtime_error("car count");
        if(bits(originalSplineLength(paths[i]))!=bits(port[i].path.length))throw std::runtime_error("0x3454E8 length mismatch");
        fprintf(log,"%s: path %x (%zu segments, length %.3f), distance %.3f, speed %.4f, radius %.3f, angle %.6f, mode %d, clones %x %x %x\n",lifts[i].name,port[i].path.resource,paths[i].segments.size(),
            port[i].path.length,port[i].distance,port[i].speed,port[i].radius,port[i].angle,port[i].mode,clones[i][0],clones[i][1],clones[i][2]);
    }
    // ---- 1. lockstep ---------------------------------------------------------------
    unsigned exactTicks[2]={},badWords[2]={},captureMatch[2][2]={},captureCompared[2][2]={};int firstCaptureMiss[2][2]={{-1,-1},{-1,-1}};
    for(int t=0;t<ticks;t++){
        const float dt=A.get<float>(clock+0x14);
        for(unsigned i=0;i<2;i++){
            A.call(0x35a560,{lifts[i].modifier});A.call(0x3568b0,{lifts[i].entity});
            originalMultiSplineUpdate(port[i],dt);originalMultiSplineBounds(port[i]);
            unsigned d=compare(port[i],A.ee.data(),lifts[i].modifier,records[i],clones[i],log,lifts[i].name,t<3?8:2);
            if(d){if(badWords[i]<40)fprintf(log,"  tick %d %s: %u words differ\n",t,lifts[i].name,d);badWords[i]+=d;}else exactTicks[i]++;
            // capture record T holds the state after tick T's entity update; port after t+1 steps = tick t.
            for(unsigned shift=0;shift<2;shift++){
                auto r=captureRecords.find(uint32_t(t+int(shift)));if(r==captureRecords.end())continue;
                const uint8_t* w=capture.data()+r->second+10240;
                // windows: 0x592E80:0xC0, 0x593580:0xC0, 0x5CF200:0x120, 0x5CEE00:0x120, 0xBAB330:0x3C0
                static std::vector<uint8_t> image(32*1024*1024,0);
                auto place=[&](uint32_t address,size_t offset,size_t length){std::memcpy(image.data()+address,w+offset,length);};
                place(0x592E80,0,0xC0);place(0x593580,0xC0,0xC0);place(0x5CF200,0x180,0x120);place(0x5CEE00,0x2A0,0x120);place(0xBAB330,0x3C0,0x3C0);
                // clone 0 of tramlores_0 (0xBAB6A0) ends at 0xBAB718, past the 0xBAB6F0 window: its bounds
                // (+0x60..+0x74, constant authored bounds) are taken from machine A.
                std::memcpy(image.data()+0xBAB6F0,A.ee.data()+0xBAB6F0,0x30);
                captureCompared[i][shift]++;
                if(!compare(port[i],image.data(),lifts[i].modifier,records[i],clones[i],log,"capture",0))captureMatch[i][shift]++;
                else if(firstCaptureMiss[i][shift]<0)firstCaptureMiss[i][shift]=t+int(shift);
            }
        }
    }
    fprintf(log,"lockstep %d ticks from snow-jam-ready: %s %u/%d ticks exact (%u words differ), %s %u/%d (%u)\n",ticks,lifts[0].name,exactTicks[0],ticks,badWords[0],lifts[1].name,exactTicks[1],ticks,badWords[1]);
    for(unsigned shift=0;shift<2;shift++)
        fprintf(log,"  capture record tick = port tick + %u: %s %u/%u match (first miss %d), %s %u/%u (first miss %d)\n",shift,lifts[0].name,captureMatch[0][shift],captureCompared[0][shift],firstCaptureMiss[0][shift],
            lifts[1].name,captureMatch[1][shift],captureCompared[1][shift],firstCaptureMiss[1][shift]);
    // ---- 2. randomized ----------------------------------------------------------------
    live::Machine R(folder+"/ready.ee",folder+"/ready.vuc",folder+"/ready.vud");
    std::mt19937 rng(0x35AC20);auto uni=[&](float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);};
    unsigned evalOk=0,evalCases=0,pathOk=0,pathCases=0,velOk=0,velCases=0,velHits=0;
    for(unsigned n=0;n<randomCases;n++){
        const unsigned i=n&1;const Lift& lift=lifts[i];
        OriginalRailRecord& path=paths[i];
        // 0x345248 on a scratch path cursor
        {
            const uint32_t cursorObject=arena,out=arena+0x40;const float L=port[i].path.length;
            float d=n%7==0?uni(-2*L,3*L):n%11==0?float(int(uni(0,4))-1)*L:uni(0,L);
            int32_t index=int32_t(rng()%path.segments.size());
            uint32_t seg=R.get<uint32_t>(lift.modifier+0x50);while(R.get<uint32_t>(seg+0x60))seg=R.get<uint32_t>(seg+0x60);for(int32_t k=0;k<index;k++)seg=R.get<uint32_t>(seg+0x64);
            R.put(cursorObject,port[i].path.resource);R.put(cursorObject+4,index);R.put(cursorObject+8,seg);R.put(cursorObject+12,L);
            R.call(0x345248,{cursorObject,out,out+0x10,out+0x20},{d});
            OriginalSplinePath p=port[i].path;p.index=index;p.cursor=size_t(index);auto s=originalSplineEvaluate(p,d);
            bool same=R.get<int32_t>(cursorObject+4)==p.index;
            for(unsigned k=0;k<4;k++)same&=bits(R.get<float>(out+4*k))==bits(s.position[k])&&bits(R.get<float>(out+0x10+4*k))==bits(s.tangent[k])&&bits(R.get<float>(out+0x20+4*k))==bits(s.curvature[k]);
            if(same)pathOk++;else if(pathCases-pathOk<5)fprintf(log,"  345248 %s d=%.6g idx %d: cursor %d/%d pos %.9g/%.9g\n",lift.name,d,index,R.get<int32_t>(cursorObject+4),p.index,R.get<float>(out),s.position[0]);
            pathCases++;
        }
        // 0x35AC20 with random options (restore the real fields afterwards is unnecessary: R is scratch)
        {
            OriginalMultiSplineModifier x=readModifier(R,lift.modifier,path);
            x.distance=n%5==0?uni(-x.path.length,2*x.path.length):uni(0,x.path.length);x.mode=int32_t(rng()%4);x.angle=n%3==0?0.f:uni(-7,7);
            R.put(lift.modifier+0x10,x.distance);R.put(lift.modifier+8,x.mode);R.put(lift.modifier+0xC,x.angle);
            R.call(0x35ac20,{lift.modifier});originalMultiSplineEvaluate(x);
            if(!compare(x,R.ee.data(),lift.modifier,records[i],clones[i],log,"35AC20",evalCases-evalOk<3?6:0))evalOk++;
            evalCases++;
            // 0x35B200 on a random car/point (packet +0x20/+0x30 start at random values)
            const unsigned car=rng()%3;const uint32_t packet=arena+0x100;
            SplineQuad point{uni(-400,400),uni(-400,400),uni(-400,400),1.f};for(unsigned k=0;k<3;k++)point[k]=terrain_original::add(x.cars[car].position[k],point[k]);
            SplineQuad v0{uni(-900,900),uni(-900,900),uni(-900,900),0.f},w0{uni(-2,2),uni(-2,2),uni(-2,2),0.f};
            R.put(packet,point);R.put(packet+0x20,v0);R.put(packet+0x30,w0);R.put(packet+0x50,clones[i][car]);
            R.call(0x35b200,{lift.modifier,packet});
            auto v=originalMultiSplineContactVelocity(x,car,point);
            SplineQuad v1,w1;{OriginalRounding rounding;v1=roller_math::vadd(v0,v.linear);w1=roller_math::vadd(w0,v.angular);}
            bool same=true;for(unsigned k=0;k<4;k++)same&=bits(R.get<float>(packet+0x20+4*k))==bits(v1[k])&&bits(R.get<float>(packet+0x30+4*k))==bits(w1[k]);
            if(same)velOk++;else if(velCases-velOk<5)fprintf(log,"  35B200 %s car %u: v %.9g/%.9g w %.9g/%.9g\n",lift.name,car,R.get<float>(packet+0x20),v1[0],R.get<float>(packet+0x30),w1[0]);
            velCases++;velHits+=std::fabs(x.cars[car].tangent[0])>multi_spline_constants::kVelocityEpsilon;
        }
    }
    fprintf(log,"randomized: 0x345248 %u/%u, 0x35AC20 %u/%u, 0x35B200 %u/%u (%u with the rate branch)\n",pathOk,pathCases,evalOk,evalCases,velOk,velCases,velHits);
    fprintf(log,"original entries called:");for(auto& [pc,n]:callCounts)if(n)fprintf(log," %x:%llu",pc,(unsigned long long)n);fprintf(log,"\n");
    bool ok=exactTicks[0]==unsigned(ticks)&&exactTicks[1]==unsigned(ticks)&&pathOk==pathCases&&evalOk==evalCases&&velOk==velCases;
    return ok?0:1;
}
