// Replay of a PS2 race capture through engine/section_streaming.hpp (driver: tools/test_sections_replay.py).
// Inputs: sections.json (tools/export_sections.py) and a text file with the captured human positions,
// outer-camera viewer, 0x101B60 lists (watch window act+0xD0) and chunk states (W+0x3F0) per record.
//   A <parity> <lastScan> <x> <y> <z> <res...>   list after the tick-0 scan (anchor savestate)
//   R <tick> <px> <py> <pz> <ex> <ey> <ez> <range> <chunk:state ...>   record `tick`
//   L <d0> <draws> <res...>                        captured list after the scan of tick d0 (+ 0x101B60 RNG draws)
//   N <tick> <res>                                 instance inserted between scans (0x1032C0 oracle)
// Checks: scan ticks, handler-instance lists (stage slot 1/3, not movers) after every scan, the
// slot-1 actions' RNG draws vs the captured 0x101B60 draws, and every chunk state of every record.
// Entity-only instances (trigger pieces, timers, clones) and movers are resynchronised from the
// capture after each scan: their lifetimes come from the browser's set-piece systems.
#include "../engine/section_streaming.hpp"
#include "json.hpp"
#include <cstdio>
#include <fstream>
#include <set>
#include <sstream>
using namespace ssx::sections;
int main(int argc,char** argv){
    if(argc<3){std::fprintf(stderr,"usage: %s sections.json replay.txt\n",argv[0]);return 2;}
    std::ifstream js(argv[1]);auto doc=nlohmann::json::parse(js);
    Activation act;ChunkStreaming chunks;loadSections(doc,act,&chunks);
    std::set<uint32_t> handler,movers;
    for(const auto& j:doc["instances"]){
        const uint32_t r=j["resource"].get<uint32_t>();
        if(j["slot1"].get<int>()>=0||j["slot3"].get<int>()>=0)handler.insert(r);
        const auto& e=j["entity_at_start"];const bool multi=!e.is_null()&&e.get<std::string>()=="multispline";
        if(j["moving"].get<bool>()||multi)movers.insert(r);
    }
    for(auto it=doc["programs"].begin();it!=doc["programs"].end();++it)
        if(it.value().contains("creates")&&!it.value()["creates"].is_null()&&it.value()["creates"].get<std::string>()=="multispline")
            for(const auto& j:doc["instances"])if(j["slot1"].get<int>()==std::stoi(it.key()))movers.insert(j["resource"].get<uint32_t>());
    std::ifstream in(argv[2]);std::string line;
    std::map<int32_t,std::pair<int,std::vector<uint32_t>>> lists;std::map<int32_t,std::vector<uint32_t>> inserts;
    struct Record{int32_t tick;Vec3 p,eye;float range;std::vector<std::pair<int32_t,int>> chunks;};std::vector<Record> records;
    std::vector<uint32_t> anchor;int anchorParity=0;int32_t anchorScan=0;Vec3 anchorPos{};
    while(std::getline(in,line)){
        std::istringstream s(line);char kind;s>>kind;
        if(kind=='A'){s>>anchorParity>>anchorScan>>anchorPos[0]>>anchorPos[1]>>anchorPos[2];uint32_t r;while(s>>r)anchor.push_back(r);}
        else if(kind=='R'){Record r;s>>r.tick>>r.p[0]>>r.p[1]>>r.p[2]>>r.eye[0]>>r.eye[1]>>r.eye[2]>>r.range;std::string cs;
            while(s>>cs){auto c=cs.find(':');r.chunks.push_back({std::stoi(cs.substr(0,c)),std::stoi(cs.substr(c+1))});}records.push_back(std::move(r));}
        else if(kind=='L'){int32_t d0;int draws;s>>d0>>draws;std::vector<uint32_t> v;uint32_t r;while(s>>r)v.push_back(r);lists[d0]={draws,v};}
        else if(kind=='N'){int32_t t;uint32_t r;s>>t>>r;inserts[t].push_back(r);}
    }
    // tick 0: the first scan after 0x103358 from the ready savestate's entities must give the anchor list
    unsigned tick0Errors=0,tick0Slot1=0,tick0Draws=0;{
        Activation fresh=act;std::vector<Event> ev;const auto& p0=doc["initial"]["scan_tick0_position"];
        fresh.update(0,{p0[0].get<float>(),p0[1].get<float>(),p0[2].get<float>()},ev);
        std::set<uint32_t> want,have;for(const auto& r:doc["initial"]["active_after_tick0_scan"])want.insert(r.get<uint32_t>());
        for(uint32_t i:fresh.list)have.insert(fresh.instances[i].resource);
        for(uint32_t r:want)if(!have.count(r)&&act.find(r))++tick0Errors;
        for(uint32_t r:have)if(!want.count(r))++tick0Errors;
        for(const auto& e:ev){tick0Slot1+=e.action==Action::Slot1;tick0Draws+=e.rngDraws;}
        std::printf("tick 0 scan: %zu listed, %u differences from the anchor list, %u slot-1 runs, %u RNG draws\n",have.size(),tick0Errors,tick0Slot1,tick0Draws);
    }
    // state after the tick-0 scan (anchor savestate)
    for(uint32_t r:anchor)if(Instance* i=act.find(r)){i->listed=true;act.list.push_back(uint32_t(i-act.instances.data()));}
    act.parity=anchorParity!=0;for(uint32_t k:act.list)act.instances[k].parity=!act.parity;act.lastScan=anchorScan;act.last=anchorPos;
    unsigned scans=0,scanTickErrors=0,listExact=0,listErrors=0,drawExact=0,drawErrors=0,chunkRecords=0,chunkErrors=0,slot1=0,slot3=0,destroys=0;
    std::vector<Event> events;
    for(size_t k=0;k+1<records.size();++k){
        const Record& rec=records[k+1];const int32_t tick=records[k].tick;   // scan of tick T uses record T+1's position
        if(rec.tick!=tick+1)break;
        for(uint32_t r:inserts[tick])act.entityCreated(r,act.find(r)&&act.find(r)->entity!=EntityKind::None?act.find(r)->entity:EntityKind::Plain);
        const bool scanned=act.update(tick,rec.p,events);auto captured=lists.find(tick);
        if(scanned!=(captured!=lists.end())){if(scanTickErrors++<10)std::printf("scan tick mismatch at %d (model %d)\n",tick,int(scanned));}
        if(scanned&&captured!=lists.end()){
            ++scans;std::set<uint32_t> want(captured->second.second.begin(),captured->second.second.end()),have;
            for(uint32_t i:act.list)have.insert(act.instances[i].resource);
            bool ok=true;for(uint32_t r:handler){if(movers.count(r))continue;if(want.count(r)!=have.count(r)){ok=false;if(listErrors<10)std::printf("tick %d handler %#x captured %d model %d\n",tick,r,int(want.count(r)),int(have.count(r)));}}
            ok?++listExact:++listErrors;
            int draws=0;for(const auto& e:events){draws+=e.rngDraws;slot1+=e.action==Action::Slot1;slot3+=e.action==Action::Slot3;destroys+=e.action==Action::Destroy;}
            if(draws==captured->second.first)++drawExact;else if(drawErrors++<10)std::printf("tick %d draws model %d captured %d\n",tick,draws,captured->second.first);
            // resynchronise entity-only instances and movers from the capture
            std::vector<uint32_t> list;
            for(auto& i:act.instances){
                const bool dynamic=!handler.count(i.resource)||movers.count(i.resource);
                if(dynamic){const bool in=want.count(i.resource)>0;if(in&&i.entity==EntityKind::None)i.entity=EntityKind::Plain;if(!in&&!handler.count(i.resource))i.entity=EntityKind::None;i.listed=in;i.parity=!act.parity;}
                if(i.listed)list.push_back(uint32_t(&i-act.instances.data()));
            }
            act.list=list;
        }
        // chunk streaming: frame of record T uses its viewer
        chunks.frame(rec.tick,rec.tick,rec.eye,rec.range);++chunkRecords;bool ok=true;
        for(auto [c,st]:rec.chunks){auto it=chunks.chunks.find(c);if(it!=chunks.chunks.end()&&it->second.state!=st){ok=false;if(chunkErrors<10)std::printf("record %d chunk %d model %d captured %d\n",rec.tick,c,int(it->second.state),st);}}
        if(!ok)++chunkErrors;
    }
    std::printf("scans %u (tick mismatches %u), handler lists exact %u / errors %u, 0x101B60 draws exact %u / errors %u, events slot1 %u slot3 %u destroy %u; chunk records %u errors %u\n",
                scans,scanTickErrors,listExact,listErrors,drawExact,drawErrors,slot1,slot3,destroys,chunkRecords,chunkErrors);
    return scanTickErrors||listErrors||drawErrors||tick0Errors?1:0;
}
