// Live SplineModifier parity (development oracle): engine/spline_modifier.hpp against the
// full recompiled original on the setpieces race savestates
// (local/ps2-capture/runs/setpieces/race.tick*.p2s).
//  * Launch: the real stage programs run through the original dispatchers: slot 2
//    (rider contact) 0x30A060(context, rider+A60, packet with +0x50 = trigger, rider+6C0)
//    and slot 1 (section activation) 0x30A298(context, instance): VM 0x309C88, builtin44/
//    52/0/3/19/26/31/1. The port constructs the same modifiers from the exported
//    arguments (originalSplineConstruct + originalSplineAttach) and must match every byte
//    the constructor/attach write; the shared-RNG advance of the whole program is reported.
//  * Lockstep: per tick the original entity update 0x356198 (modifier 0x359698, the
//    finished check 0x352ED0/0x352F08 and entity vt+0x114 -> slot-4 end program with
//    builtin16's type-13 conversion 0x355F10 -> PositionModifier) and the bounds pass
//    0x3568B0; the port runs originalSplineUpdate / originalSplineTakeFinished /
//    originalSplineFreeze (+ attach) / originalSplineEntityBounds. Every modeled byte of the
//    live modifier (SplineModifier 0xF0 or PositionModifier 0x90) is compared per tick.
//  * The raven already flying at tick 4319 is stepped to 4719/5118/5518 and compared with
//    those PS2 savestates.
// ~5,000 generated registrations (the builtins share one recompiled file that reaches most
// of the game): compile them without optimisation, the oracle code itself stays -O1.
#pragma clang optimize off
#include "live_registrations.inc"
#pragma clang optimize on
#include "original_live_runtime.hpp"
#include "../engine/spline_modifier.hpp"
#include <map>
#include <memory>
using namespace ssx;
static constexpr uint32_t arena=0xE0000,BIND=arena+0x400,PACKET=arena+0x500,RIDER=0x14701A0,GP=0x4A30F0;
static uint32_t bits(float x){return std::bit_cast<uint32_t>(x);}
static FILE* logf_=stdout;
struct Path {uint32_t resource=0,head=0;std::shared_ptr<OriginalRailRecord> record;};
static Path bindPath(live::Machine& m,uint32_t resource){
    m.call(0x3451c0,{BIND,resource});
    Path p;p.resource=resource;p.head=m.get<uint32_t>(BIND+8);p.record=std::make_shared<OriginalRailRecord>();p.record->packedId=resource;int32_t index=0;
    for(uint32_t seg=p.head;seg;seg=m.get<uint32_t>(seg+0x64)){
        if(seg!=p.head+0x90u*uint32_t(index))throw std::runtime_error("segments not contiguous");
        OriginalRailSegment s;s.index=index++;s.length=m.get<float>(seg+0xC);s.distance=m.get<float>(seg+0x84);
        for(unsigned row=0;row<4;row++)for(unsigned k=0;k<3;k++)s.coefficients[row][k]=m.get<float>(seg+0x10+16*row+4*k);
        for(unsigned k=0;k<4;k++)s.arcToParameter[k]=m.get<float>(seg+0x50+4*k);
        p.record->segments.push_back(s);
    }
    return p;
}
static unsigned diff(const uint8_t* a,const uint8_t* b,size_t n,const char* what,unsigned limit,const std::vector<uint32_t>& skip={}){
    unsigned bad=0;
    for(size_t o=0;o<n;o+=4){
        bool skipped=false;for(size_t s=0;s+1<skip.size();s+=2)skipped|=o>=skip[s]&&o<skip[s+1];
        if(skipped||!std::memcmp(a+o,b+o,4))continue;
        if(bad<limit){uint32_t x,y;std::memcpy(&x,a+o,4);std::memcpy(&y,b+o,4);fprintf(logf_,"    %s +%03zx original %08x (%.9g) port %08x (%.9g)\n",what,o,x,std::bit_cast<float>(x),y,std::bit_cast<float>(y));}
        bad++;
    }
    return bad;
}
struct Piece {const char* name;uint32_t instance,spline;int32_t endMode;float kmh,roll;};
struct Launch {int slot;uint32_t instance;};
struct Scenario {const char* state;const char* label;std::vector<Launch> launches;std::vector<Piece> pieces;int ticks;};
struct Live {
    Piece piece;Path path;uint32_t entity=0,modifier=0;bool spline=true;
    OriginalSplineModifier m;OriginalPositionModifier p;
    SplineQuad T{};std::array<float,3> low{},high{};uint32_t flags=0;
    unsigned exact=0,compared=0,bad=0;int finishedTick=-1;
};
static std::array<float,3> vec3(const live::Machine& m,uint32_t a){return {m.get<float>(a),m.get<float>(a+4),m.get<float>(a+8)};}
// Unwritten words of a freshly constructed SplineModifier (allocation contents).
static const std::vector<uint32_t> kStaleSpline={0x04,0x10,0x5C,0x60,0x60,0xB0,0xE8,0xF0};
static const std::vector<uint32_t> kStalePosition={0x04,0x10,0x48,0x50};

static int runScenario(const std::string& folder,const Scenario& s,unsigned& failures){
    live::Machine A(folder+"/"+s.state+".ee",folder+"/"+s.state+".vuc",folder+"/"+s.state+".vud");
    const uint32_t context=A.get<uint32_t>(GP+0xCE8);const float dt=A.get<float>(A.get<uint32_t>(GP-0x848)+0x14);
    std::vector<Live> pieces;
    for(const auto& piece:s.pieces){
        Live l;l.piece=piece;l.path=bindPath(A,piece.spline);l.T=A.get<SplineQuad>(piece.instance+0x40);
        l.low=vec3(A,piece.instance+0x60);l.high=vec3(A,piece.instance+0x6C);l.flags=A.get<uint32_t>(piece.instance+8);
        if(A.get<uint32_t>(piece.instance+0xC))throw std::runtime_error(std::string(piece.name)+" already has an entity");
        pieces.push_back(std::move(l));
    }
    OriginalRandomState before;for(unsigned k=0;k<6;k++)before.words[k]=A.get<uint32_t>(0x4FF030+4*k);
    for(const auto& launch:s.launches){
        if(launch.slot==2){
            for(uint32_t o=0;o<0x80;o+=4)A.put(PACKET+o,A.get<uint32_t>(RIDER+0x9E0+o));
            A.put(PACKET+0x50,launch.instance);
            A.call(0x30a060,{context,RIDER+0xA60,PACKET,RIDER+0x6C0});
        }else A.call(0x30a298,{context,launch.instance});
    }
    OriginalRandomState after;for(unsigned k=0;k<6;k++)after.words[k]=A.get<uint32_t>(0x4FF030+4*k);
    int draws=-1;{OriginalRandomState r=before;for(int k=0;k<100000;k++){if(r.words==after.words){draws=k;break;}r.next();}}
    fprintf(logf_,"[%s] launched from %s: shared RNG advanced by %d draws\n",s.label,s.state,draws);
    // Construction
    for(auto& l:pieces){
        l.entity=A.get<uint32_t>(l.piece.instance+0xC);
        const uint32_t container=l.entity?A.get<uint32_t>(l.entity+0x1C):0;l.modifier=container?A.get<uint32_t>(container):0;
        if(!l.modifier||A.get<uint32_t>(l.modifier)!=0x48F250u){fprintf(logf_,"  %s: no SplineModifier after the launch\n",l.piece.name);failures++;continue;}
        const uint8_t* original=A.ee.data()+l.modifier;
        OriginalSplineModifier m;std::memcpy(m.word04.data(),original+4,12);std::memcpy(&m.word5C,original+0x5C,4);std::memcpy(m.wordE8.data(),original+0xE8,8);
        std::memcpy(m.matrix.data(),original+0x60,0x40);std::memcpy(m.position.data(),original+0xA0,16);
        OriginalSplineScriptArgs a;a.spline=l.piece.spline;a.endMode=l.piece.endMode;a.speedKmh=l.piece.kmh;a.rollDegrees=l.piece.roll;
        OriginalRandomState scratch=before;originalSplineConstruct(m,a,*l.path.record,scratch);
        originalSplineAttach(m,l.T,l.low,l.high,l.flags);
        const bool evaluated=A.get<uint32_t>(l.modifier+0x58)==0;
        if(evaluated)originalSplineMatrix(m);
        auto b=originalSplineModifierBytes(m,l.path.head);
        unsigned d=diff(original,b.data(),0xF0,l.piece.name,8,evaluated?std::vector<uint32_t>{0x04,0x10,0x5C,0x60,0xE8,0xF0}:kStaleSpline);
        fprintf(logf_,"  %-28s entity %06x (vtable %06x) modifier %06x: construction+attach %s (%u words differ; %s); instance flags %08x -> %08x\n",l.piece.name,l.entity,A.get<uint32_t>(l.entity+0xC),l.modifier,
            d?"DIFFERS":"byte-identical",d,evaluated?"already evaluated during the program":"dirty, matrix unevaluated",l.flags,A.get<uint32_t>(l.piece.instance+8));
        failures+=d!=0;l.m=m;
    }
    // Lockstep
    for(int t=0;t<s.ticks;t++){
        for(auto& l:pieces){
            if(!l.modifier)continue;
            if(A.get<uint32_t>(l.piece.instance+0xC)!=l.entity)continue;
            A.call(0x356198,{l.entity});
            const uint32_t entity=A.get<uint32_t>(l.piece.instance+0xC);
            if(l.spline){
                originalSplineUpdate(l.m,dt);
                if(originalSplineTakeFinished(l.m)){
                    l.finishedTick=t;l.spline=false;l.p=originalSplineFreeze(l.m);
                    originalPositionAttach(l.p,l.T,l.low,l.high,0);originalPositionEntityBounds(l.p);
                    if(!entity||entity==l.entity){fprintf(logf_,"  %s: finished at tick %d but the entity was not converted (%06x)\n",l.piece.name,t,entity);failures++;l.modifier=0;continue;}
                    l.entity=entity;l.modifier=A.get<uint32_t>(A.get<uint32_t>(entity+0x1C));
                    fprintf(logf_,"  %-28s finished at lockstep tick %d: entity -> %06x (vtable %06x), modifier %06x vtable %06x, instance flags %08x\n",l.piece.name,t,entity,A.get<uint32_t>(entity+0xC),l.modifier,A.get<uint32_t>(l.modifier),A.get<uint32_t>(l.piece.instance+8));
                    std::memcpy(l.p.word04.data(),A.ee.data()+l.modifier+4,12);std::memcpy(l.p.word48.data(),A.ee.data()+l.modifier+0x48,8);
                }
            }
            if(A.get<uint32_t>(l.piece.instance+0xC)!=l.entity){fprintf(logf_,"  %s: entity removed at tick %d\n",l.piece.name,t);l.modifier=0;continue;}
            A.call(0x3568b0,{l.entity});
            unsigned d;
            if(l.spline){originalSplineEntityBounds(l.m);auto b=originalSplineModifierBytes(l.m,l.path.head);d=diff(A.ee.data()+l.modifier,b.data(),0xF0,l.piece.name,l.bad<2?8:0,{0x04,0x10,0x5C,0x60,0xE8,0xF0});}
            else{originalPositionEntityBounds(l.p);auto b=originalPositionModifierBytes(l.p);d=diff(A.ee.data()+l.modifier,b.data(),0x90,l.piece.name,l.bad<2?8:0,kStalePosition);}
            if(l.spline&&bits(l.m.distance)==0x45B4FD05u&&l.piece.spline==0x5408)fprintf(logf_,"  %s reaches the tick-4319 distance 5791.6274 after %d updates: slot 1 ran at race tick %d\n",l.piece.name,t+1,4319-(t+1));
            l.compared++;if(d){if(l.bad<3)fprintf(logf_,"  tick %d %s: %u words differ\n",t,l.piece.name,d);l.bad++;}else l.exact++;
        }
    }
    for(auto& l:pieces){
        fprintf(logf_,"  %-28s lockstep %d ticks: %u/%u compared ticks exact%s%s; instance flags now %08x\n",l.piece.name,s.ticks,l.exact,l.compared,
            l.finishedTick>=0?", path end -> PositionModifier at tick ":"",l.finishedTick>=0?std::to_string(l.finishedTick).c_str():"",A.get<uint32_t>(l.piece.instance+8));
        failures+=l.compared-l.exact;
    }
    return 0;
}

int main(int argc,char** argv){
    if(argc<2){fprintf(stderr,"usage: live FOLDER\n");return 2;}
    const std::string folder=argv[1];unsigned failures=0;
    // ---- raven in flight at tick 4319: lockstep and the later PS2 savestates ----------
    {
        live::Machine A(folder+"/4319.ee",folder+"/4319.vuc",folder+"/4319.vud");
        const uint32_t instance=0x1075470,entity=A.get<uint32_t>(instance+0xC),modifier=A.get<uint32_t>(A.get<uint32_t>(entity+0x1C));
        if(A.get<uint32_t>(modifier)!=0x48F250u||modifier!=0x5AC500)throw std::runtime_error("raven modifier");
        Path path=bindPath(A,A.get<uint32_t>(modifier+0xD8));
        OriginalSplineModifier m=originalSplineModifierFromBytes(A.ee.data()+modifier,path.record.get(),path.head);
        const float dt=A.get<float>(A.get<uint32_t>(GP-0x848)+0x14);
        std::map<int,std::vector<uint8_t>> snapshots;
        for(int tick:{4719,5118,5518}){auto e=live::readFile(folder+"/"+std::to_string(tick)+".ee");snapshots[tick-4319]=std::vector<uint8_t>(e.begin()+modifier,e.begin()+modifier+0xF0);}
        unsigned exact=0,wraps=0;int ticks=1300;std::map<int,std::string> snapshotResult;
        for(int t=1;t<=ticks;t++){
            A.call(0x356198,{entity});A.call(0x3568b0,{entity});
            const float d0=m.distance;originalSplineUpdate(m,dt);wraps+=m.distance<d0;
            if(originalSplineTakeFinished(m))throw std::runtime_error("raven finished");
            originalSplineEntityBounds(m);
            auto b=originalSplineModifierBytes(m,path.head);
            unsigned d=diff(A.ee.data()+modifier,b.data(),0xF0,"raven",t<3?8:0);if(!d)exact++;
            for(int shift=-1;shift<=1;shift++){
                auto it=snapshots.find(t+shift);if(it==snapshots.end())continue;
                unsigned ds=diff(it->second.data(),b.data(),0xF0,"snapshot",0);
                snapshotResult[it->first]+=(snapshotResult[it->first].empty()?"":", ")+std::string("port tick +")+std::to_string(t)+(ds?" differs ("+std::to_string(ds)+" words)":" byte-identical");
            }
        }
        fprintf(logf_,"[raven 4319] ravensplineanima_1000 modifier %06x (entity %06x LiveComp): lockstep %u/%d ticks byte-identical (original 0x356198 + 0x3568B0; %u loop wraps)\n",modifier,entity,exact,ticks,wraps);
        for(auto& [k,v]:snapshotResult)fprintf(logf_,"  PS2 savestate tick %d: %s\n",4319+k,v.c_str());
        failures+=unsigned(ticks)-exact;
    }
    // ---- launches of the real stage programs, then lockstep through the path end ------
    const std::vector<Scenario> scenarios={
        {"718","rockets (triggerRockets_1100 contact, program 120)",{{2,0xFC22C0}},
            {{"mdl_ARA1_brocket_1000",0x10B2AE0,0xA908,0,150.f,0.f},{"mdl_ARA1_brocket_1001",0xFFF400,0xAA08,0,150.f,0.f}},240},
        {"3519","spintwins (spintwintrig_1000 contact, program 131)",{{2,0x104BF00}},
            {{"mdl_ARA1_spintwin_1000",0x104B780,0xA208,0,160.f,0.f},{"mdl_ARA1_spintwin_1001",0xFCE4D0,0xA108,0,160.f,0.f}},240},
        {"3519","raven (ravensplineanima_1000 slot 1, program 54)",{{1,0x1075470}},
            {{"mdl_ARA1_ravensplineanima_1000",0x1075470,0x5408,1,45.f,-90.f}},900},
        {"4319","dragons (dragontrig_1000 + dragontrig_1100 contacts, programs 136/137)",{{2,0xFE1980},{2,0x109D000}},
            {{"mdl_ARA1_chasingdragon_1000",0x105B840,0xA608,0,90.f,0.f},{"mdl_ARA1_chasingdragon_1001",0xF965E0,0xA508,0,90.f,0.f},
             {"mdl_ARA1_chasingdragon_1100",0xF44800,0xA408,0,120.f,0.f},{"mdl_ARA1_chasingdragon_1101",0x10D0320,0xA308,0,120.f,0.f}},360},
    };
    for(const auto& s:scenarios){
        try{runScenario(folder,s,failures);}
        catch(const std::exception& e){fprintf(logf_,"[%s] original run stopped: %s\n",s.label,e.what());failures++;}
    }
    fprintf(logf_,"original entries called:");for(auto& [pc,n]:callCounts)if(n&&(pc>>12==0x359||pc>>12==0x356||pc==0x355f10||pc==0x3578a8||pc==0x2fded0||pc==0x2fd420))fprintf(logf_," %x:%llu",pc,(unsigned long long)n);fprintf(logf_,"\n");
    fprintf(logf_,failures?"FAILED (%u)\n":"all live checks match\n",failures);
    return failures?1:0;
}
