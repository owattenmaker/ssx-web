// Live MultiSplineModifier parity for any location (development oracle; tools/test_multi_spline_location.py).
// engine/multi_spline_modifier.hpp against the full recompiled original on a location savestate:
//  1. Lockstep: machine A runs 0x35A560 (modifier vtable+0x14) and the entity update 0x3568B0 (Object and
//     LiveComp share it) for every configured modifier per tick; the port runs originalMultiSplineUpdate +
//     originalMultiSplineBounds. Compared per tick: every modeled modifier word, car records, clone
//     matrices/bounds.
//  2. Later kept snapshots of the same PS2 run ("target OFFSET PATH"): the port state after OFFSET ticks
//     equals the snapshot memory word for word (modifiers still live at the same address).
//  3. Randomized on machine R: 0x345248, 0x35AC20 and 0x35B200 per modifier.
//  4. "construct PATH": ticks since construction (0x359F88 start distance d0 given per modifier, then
//     0x35A560 steps) and the construction-time state (distance d0, 0x35AC20 + bounds) as raw words.
//  5. "spline MODIFIER ENTITY NAME": a looping SplineModifier (engine/spline_modifier.hpp) stepped by the
//     original entity update 0x356198 + 0x3568B0 against originalSplineUpdate + originalSplineEntityBounds,
//     every modeled byte per tick and in the later snapshots (BHP1 blimp).
// Config lines: "mod MODIFIER ENTITY NAME D0BITS", "spline MODIFIER ENTITY NAME", "target OFFSET EEPATH",
// "construct JSONPATH".
#include "live_registrations.inc"
#include "original_live_runtime.hpp"
#include "../engine/multi_spline_modifier.hpp"
#include "../engine/spline_modifier.hpp"
#include <memory>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
using namespace ssx;
static constexpr uint32_t arena=0xE0000;
struct Piece {uint32_t modifier,entity,d0;std::string name;};
static uint32_t bits(float x){return std::bit_cast<uint32_t>(x);}
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
static unsigned compare(const OriginalMultiSplineModifier& x,const uint8_t* ee,uint32_t a,uint32_t records,const std::vector<uint32_t>& clones,FILE* log,const char* what,unsigned limit){
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
struct SplinePiece {uint32_t modifier,entity,head=0;std::string name;std::shared_ptr<OriginalRailRecord> record;OriginalSplineModifier m;unsigned exact=0,bad=0;};
static unsigned diffBytes(const uint8_t* a,const uint8_t* b,size_t n,const char* what,unsigned limit){
    // never-written words of the SplineModifier: +0x04..+0x0C, +0x5C, +0xE8/+0xEC
    unsigned bad=0;
    for(size_t o=0;o<n;o+=4){
        if((o>=0x04&&o<0x10)||o==0x5C||o>=0xE8||!std::memcmp(a+o,b+o,4))continue;
        if(bad<limit){uint32_t x,y;std::memcpy(&x,a+o,4);std::memcpy(&y,b+o,4);fprintf(stdout,"    %s +%03zx original %08x port %08x\n",what,o,x,y);}
        bad++;
    }
    return bad;
}
int main(int argc,char** argv){
    if(argc<5){fprintf(stderr,"usage: live FOLDER TICKS RANDOM CONFIG\n");return 2;}
    std::string folder=argv[1];int ticks=atoi(argv[2]);unsigned randomCases=unsigned(atoi(argv[3]));FILE* log=stdout;
    std::vector<Piece> pieces;std::vector<std::pair<int,std::string>> targets;std::string constructPath;std::vector<SplinePiece> splines;
    {std::ifstream in(argv[4]);std::string line;
     while(std::getline(in,line)){std::istringstream s(line);std::string kind;s>>kind;
        if(kind=="mod"){Piece p;std::string a,e,d;s>>a>>e>>p.name>>d;p.modifier=uint32_t(std::stoul(a,nullptr,16));p.entity=uint32_t(std::stoul(e,nullptr,16));p.d0=uint32_t(std::stoul(d,nullptr,16));pieces.push_back(p);}
        else if(kind=="spline"){SplinePiece p;std::string a,e;s>>a>>e>>p.name;p.modifier=uint32_t(std::stoul(a,nullptr,16));p.entity=uint32_t(std::stoul(e,nullptr,16));splines.push_back(p);}
        else if(kind=="target"){int o;std::string p;s>>o>>p;targets.emplace_back(o,p);}
        else if(kind=="construct")s>>constructPath;}}
    live::Machine A(folder+"/start.ee",folder+"/start.vuc",folder+"/start.vud");
    const uint32_t clock=A.get<uint32_t>(0x4A30F0-0x848);
    const size_t n=pieces.size();
    std::vector<OriginalRailRecord> paths(n);std::vector<OriginalMultiSplineModifier> port(n);std::vector<std::vector<uint32_t>> clones(n);std::vector<uint32_t> records(n);
    for(size_t i=0;i<n;i++){
        const auto& p=pieces[i];
        if(A.get<uint32_t>(p.modifier)!=0x48F168||A.get<uint32_t>(A.get<uint32_t>(p.entity+0x1C))!=p.modifier)throw std::runtime_error("modifier layout "+p.name);
        paths[i]=readPath(A,p.modifier);port[i]=readModifier(A,p.modifier,paths[i]);
        records[i]=A.get<uint32_t>(p.modifier+0x3C);for(int32_t k=0;k<port[i].count;k++)clones[i].push_back(A.get<uint32_t>(A.get<uint32_t>(p.modifier+0x44)+4*k));
        if(bits(originalSplineLength(paths[i]))!=bits(port[i].path.length))throw std::runtime_error("0x3454E8 length mismatch");
        fprintf(log,"%s: path %x (%zu segments, length %.3f), count %d, distance %.3f, speed %.4f, radius %.3f, angle %.6f, mode %d\n",p.name.c_str(),port[i].path.resource,paths[i].segments.size(),
            port[i].path.length,port[i].count,port[i].distance,port[i].speed,port[i].radius,port[i].angle,port[i].mode);
    }
    for(auto& sp:splines){
        if(A.get<uint32_t>(sp.modifier)!=0x48F250u||A.get<uint32_t>(A.get<uint32_t>(sp.entity+0x1C))!=sp.modifier)throw std::runtime_error("spline layout "+sp.name);
        uint32_t seg=A.get<uint32_t>(sp.modifier+0xE0);while(A.get<uint32_t>(seg+0x60))seg=A.get<uint32_t>(seg+0x60);sp.head=seg;
        sp.record=std::make_shared<OriginalRailRecord>();sp.record->packedId=A.get<uint32_t>(sp.modifier+0xD8);int32_t index=0;
        for(uint32_t g=seg;g;g=A.get<uint32_t>(g+0x64)){
            if(g!=seg+0x90u*uint32_t(index))throw std::runtime_error("segments not contiguous");
            OriginalRailSegment r;r.index=index++;r.length=A.get<float>(g+0xC);r.distance=A.get<float>(g+0x84);
            for(unsigned row=0;row<4;row++)for(unsigned k=0;k<3;k++)r.coefficients[row][k]=A.get<float>(g+0x10+16*row+4*k);
            for(unsigned k=0;k<4;k++)r.arcToParameter[k]=A.get<float>(g+0x50+4*k);
            sp.record->segments.push_back(r);
        }
        sp.m=originalSplineModifierFromBytes(A.ee.data()+sp.modifier,sp.record.get(),sp.head);
        fprintf(log,"%s: SplineModifier path %x (%zu segments, length %.3f), distance %.3f, speed %.4f, end %d, orientation %d, roll %.6f\n",sp.name.c_str(),
            sp.m.path.resource,sp.record->segments.size(),sp.m.path.length,sp.m.distance,sp.m.speed,sp.m.endMode,sp.m.orientation,sp.m.roll);
    }
    // ---- 4. construction (before stepping) ----
    if(!constructPath.empty()){
        FILE* out=fopen(constructPath.c_str(),"w");fprintf(out,"{\"pieces\":[");
        const float dt=A.get<float>(clock+0x14);
        for(size_t i=0;i<n;i++){
            OriginalMultiSplineModifier x=port[i];x.distance=std::bit_cast<float>(pieces[i].d0);
            long steps=-1;{OriginalMultiSplineModifier y=x;for(long k=0;k<=400000;k++){if(bits(y.distance)==bits(port[i].distance)){steps=k;break;}originalMultiSplineUpdate(y,dt);}}
            x.path.index=0;x.path.cursor=0;x.dirty=1;originalMultiSplineBounds(x);
            // replay: the construction state stepped `steps` ticks must equal the savestate bit for bit
            OriginalMultiSplineModifier z=x;for(long k=0;k<steps;k++){originalMultiSplineUpdate(z,dt);originalMultiSplineBounds(z);}
            unsigned replay=steps>=0?compare(z,A.ee.data(),pieces[i].modifier,records[i],clones[i],log,"construct-replay",4):~0u;
            fprintf(log,"%s: constructed %ld ticks before the savestate (d0 %08x), replay %s\n",pieces[i].name.c_str(),steps,pieces[i].d0,replay==0?"bit-exact":"DIFFERS");
            fprintf(out,"%s{\"name\":\"%s\",\"ticks_since_construction\":%ld,\"replay_exact\":%s,\"distance\":%u,\"cursor\":%d,\"cars\":[",i?",":"",pieces[i].name.c_str(),steps,replay==0?"true":"false",bits(x.distance),x.path.index);
            for(int32_t c=0;c<x.count;c++){
                const auto& car=x.cars[c];std::vector<uint32_t> rec;
                for(auto* q:{&car.position,&car.tangent,&car.curvature,&car.boundsMin,&car.boundsMax})for(unsigned k=0;k<4;k++)rec.push_back(bits((*q)[k]));
                rec.push_back(bits(car.tangentXX));rec.push_back(bits(car.tangentYY));rec.push_back(car.record58);rec.push_back(car.record5C);
                fprintf(out,"%s{\"record\":[",c?",":"");for(size_t k=0;k<rec.size();k++)fprintf(out,"%s%u",k?",":"",rec[k]);
                fprintf(out,"],\"matrix\":[");for(unsigned r=0;r<4;r++)for(unsigned k=0;k<4;k++)fprintf(out,"%s%u",r||k?",":"",bits(car.matrix[r][k]));
                fprintf(out,"],\"low\":[%u,%u,%u],\"high\":[%u,%u,%u]}",bits(car.instanceLow[0]),bits(car.instanceLow[1]),bits(car.instanceLow[2]),bits(car.instanceHigh[0]),bits(car.instanceHigh[1]),bits(car.instanceHigh[2]));
            }
            fprintf(out,"]}");
        }
        fprintf(out,"]}\n");fclose(out);
    }
    // ---- 1./2. lockstep and later snapshots ----
    std::map<int,std::vector<uint8_t>> targetMemory;for(auto& [o,p]:targets)targetMemory[o]=live::readFile(p);
    std::vector<unsigned> exact(n,0),bad(n,0);unsigned targetOk=0,targetCases=0,targetAbsent=0;
    for(int t=1;t<=ticks;t++){
        const float dt=A.get<float>(clock+0x14);
        for(size_t i=0;i<n;i++){
            A.call(0x35a560,{pieces[i].modifier});A.call(0x3568b0,{pieces[i].entity});
            originalMultiSplineUpdate(port[i],dt);originalMultiSplineBounds(port[i]);
            unsigned d=compare(port[i],A.ee.data(),pieces[i].modifier,records[i],clones[i],log,pieces[i].name.c_str(),t<3?8:2);
            if(d){if(bad[i]<40)fprintf(log,"  tick %d %s: %u words differ\n",t,pieces[i].name.c_str(),d);bad[i]+=d;}else exact[i]++;
        }
        for(auto& sp:splines){
            A.call(0x356198,{sp.entity});A.call(0x3568b0,{sp.entity});
            originalSplineUpdate(sp.m,dt);if(originalSplineTakeFinished(sp.m))throw std::runtime_error(sp.name+" finished");
            originalSplineEntityBounds(sp.m);
            auto b=originalSplineModifierBytes(sp.m,sp.head);unsigned d=diffBytes(A.ee.data()+sp.modifier,b.data(),0xF0,sp.name.c_str(),t<3?8:0);
            if(d){if(sp.bad<40)fprintf(log,"  tick %d %s: %u words differ\n",t,sp.name.c_str(),d);sp.bad+=d;}else sp.exact++;
        }
        auto it=targetMemory.find(t);
        if(it!=targetMemory.end()){
            const uint8_t* ee=it->second.data();
            for(size_t i=0;i<n;i++){
                uint32_t v;std::memcpy(&v,ee+pieces[i].modifier,4);uint32_t c0;std::memcpy(&c0,ee+(A.get<uint32_t>(pieces[i].modifier+0x44)&0x1ffffff),4);
                if(v!=0x48F168||c0!=clones[i][0]){targetAbsent++;fprintf(log,"  target +%d %s: modifier no longer live\n",t,pieces[i].name.c_str());continue;}
                targetCases++;unsigned d=compare(port[i],ee,pieces[i].modifier,records[i],clones[i],log,"snapshot",4);
                if(!d)targetOk++;fprintf(log,"  target +%d %s: %s\n",t,pieces[i].name.c_str(),d?"DIFFERS":"bit-exact");
            }
            for(auto& sp:splines){
                uint32_t v;std::memcpy(&v,ee+sp.modifier,4);if(v!=0x48F250u){targetAbsent++;continue;}
                auto b=originalSplineModifierBytes(sp.m,sp.head);unsigned d=diffBytes(ee+sp.modifier,b.data(),0xF0,"snapshot",4);
                targetCases++;if(!d)targetOk++;fprintf(log,"  target +%d %s: %s\n",t,sp.name.c_str(),d?"DIFFERS":"byte-identical");
            }
        }
    }
    for(size_t i=0;i<n;i++)fprintf(log,"lockstep %s: %u/%d ticks exact (%u words differ)\n",pieces[i].name.c_str(),exact[i],ticks,bad[i]);
    for(auto& sp:splines)fprintf(log,"lockstep %s (SplineModifier): %u/%d ticks byte-identical (%u words differ)\n",sp.name.c_str(),sp.exact,ticks,sp.bad);
    fprintf(log,"later snapshots: %u/%u bit-exact (%u modifier states no longer live)\n",targetOk,targetCases,targetAbsent);
    // ---- 3. randomized ----
    live::Machine R(folder+"/start.ee",folder+"/start.vuc",folder+"/start.vud");
    std::mt19937 rng(0x35AC20);auto uni=[&](float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);};
    unsigned evalOk=0,evalCases=0,pathOk=0,pathCases=0,velOk=0,velCases=0;
    for(unsigned c=0;c<randomCases&&n;c++){
        const size_t i=c%n;const Piece& piece=pieces[i];OriginalRailRecord& path=paths[i];
        {
            const uint32_t cursorObject=arena,out=arena+0x40;const float L=port[i].path.length;
            float d=c%7==0?uni(-2*L,3*L):c%11==0?float(int(uni(0,4))-1)*L:uni(0,L);
            int32_t index=int32_t(rng()%path.segments.size());
            uint32_t seg=R.get<uint32_t>(piece.modifier+0x50);while(R.get<uint32_t>(seg+0x60))seg=R.get<uint32_t>(seg+0x60);for(int32_t k=0;k<index;k++)seg=R.get<uint32_t>(seg+0x64);
            R.put(cursorObject,port[i].path.resource);R.put(cursorObject+4,index);R.put(cursorObject+8,seg);R.put(cursorObject+12,L);
            R.call(0x345248,{cursorObject,out,out+0x10,out+0x20},{d});
            OriginalSplinePath p=port[i].path;p.index=index;p.cursor=size_t(index);auto s=originalSplineEvaluate(p,d);
            bool same=R.get<int32_t>(cursorObject+4)==p.index;
            for(unsigned k=0;k<4;k++)same&=bits(R.get<float>(out+4*k))==bits(s.position[k])&&bits(R.get<float>(out+0x10+4*k))==bits(s.tangent[k])&&bits(R.get<float>(out+0x20+4*k))==bits(s.curvature[k]);
            if(same)pathOk++;pathCases++;
        }
        {
            OriginalMultiSplineModifier x=readModifier(R,piece.modifier,path);
            x.distance=c%5==0?uni(-x.path.length,2*x.path.length):uni(0,x.path.length);x.mode=int32_t(rng()%4);x.angle=c%3==0?0.f:uni(-7,7);
            R.put(piece.modifier+0x10,x.distance);R.put(piece.modifier+8,x.mode);R.put(piece.modifier+0xC,x.angle);
            R.call(0x35ac20,{piece.modifier});originalMultiSplineEvaluate(x);
            if(!compare(x,R.ee.data(),piece.modifier,records[i],clones[i],log,"35AC20",evalCases-evalOk<3?6:0))evalOk++;
            evalCases++;
            const unsigned car=unsigned(rng()%unsigned(x.count));const uint32_t packet=arena+0x100;
            SplineQuad point{uni(-400,400),uni(-400,400),uni(-400,400),1.f};for(unsigned k=0;k<3;k++)point[k]=terrain_original::add(x.cars[car].position[k],point[k]);
            SplineQuad v0{uni(-900,900),uni(-900,900),uni(-900,900),0.f},w0{uni(-2,2),uni(-2,2),uni(-2,2),0.f};
            R.put(packet,point);R.put(packet+0x20,v0);R.put(packet+0x30,w0);R.put(packet+0x50,clones[i][car]);
            R.call(0x35b200,{piece.modifier,packet});
            auto v=originalMultiSplineContactVelocity(x,car,point);
            SplineQuad v1,w1;{OriginalRounding rounding;v1=roller_math::vadd(v0,v.linear);w1=roller_math::vadd(w0,v.angular);}
            bool same=true;for(unsigned k=0;k<4;k++)same&=bits(R.get<float>(packet+0x20+4*k))==bits(v1[k])&&bits(R.get<float>(packet+0x30+4*k))==bits(w1[k]);
            if(same)velOk++;velCases++;
        }
    }
    fprintf(log,"randomized: 0x345248 %u/%u, 0x35AC20 %u/%u, 0x35B200 %u/%u\n",pathOk,pathCases,evalOk,evalCases,velOk,velCases);
    bool ok=pathOk==pathCases&&evalOk==evalCases&&velOk==velCases&&targetOk==targetCases;
    for(size_t i=0;i<n;i++)ok&=exact[i]==unsigned(ticks);
    for(auto& sp:splines)ok&=sp.exact==unsigned(ticks);
    return ok?0:1;
}
