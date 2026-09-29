// Replay of the PS2 sections captures through engine/far_painter.hpp and the chunk streaming of
// engine/section_streaming.hpp (driver: tools/test_far_painter.py).
//   S <distance> <current> <sample> <lastX> <lastY> <cameras>     block-6 painter of the capture baseline (float bits)
//   R <tick> <ex> <ey> <ez> <far> <vx> <vy> <vz> <range> <nearMin> <farMax> <chunk:state ...>   (floats as bits;
//     e = outer+0x20, far = outer+0x08, v/range = W+0x250 viewer 0 eye/range, the eye the chunk walk used)
// Checks per record T >= 1: painter step at record T's outer+0x20/+0x24 -> outer+0x08 and the W+0x250 viewer range
// (bitwise), then ChunkStreaming::frame(T, T, eye, range) vs every captured chunk state, for three ranges: the
// predicted one, the captured one and the fixed 45000.  A cold run (painter constructed at record 0, sentinel -99999)
// is reported as well.
#include "../engine/far_painter.hpp"
#include "../engine/section_streaming.hpp"
#include "json.hpp"
#include <bit>
#include <cstdio>
#include <fstream>
#include <sstream>
using namespace ssx;
static float fb(uint32_t b){return std::bit_cast<float>(b);}
int main(int argc,char** argv){
    if(argc<4){std::fprintf(stderr,"usage: %s far-painter.json sections.json replay.txt\n",argv[0]);return 2;}
    std::ifstream fj(argv[1]),sj(argv[2]);auto farDoc=nlohmann::json::parse(fj),secDoc=nlohmann::json::parse(sj);
    OriginalFarPainter painter,cold;loadFarPainter(farDoc,painter);loadFarPainter(farDoc,cold);
    painter.tree.nodes=painter.nodes;cold.tree.nodes=cold.nodes;
    sections::Activation act;sections::ChunkStreaming predicted,captured,fixed;
    sections::loadSections(secDoc,act,&predicted);captured=predicted;fixed=predicted;
    struct Record{int32_t tick;sections::Vec3 eye,viewer;float far,range,nearMin,farMax;std::vector<std::pair<int32_t,int>> chunks;};
    std::vector<Record> records;uint32_t seed[5]{};unsigned cameras=1;
    std::ifstream in(argv[3]);std::string line;
    while(std::getline(in,line)){
        std::istringstream s(line);char kind;s>>kind;
        if(kind=='S'){for(auto& v:seed)s>>v;s>>cameras;}
        else if(kind=='R'){Record r;uint32_t b[10];s>>r.tick;for(auto& v:b)s>>v;r.eye={fb(b[0]),fb(b[1]),fb(b[2])};r.far=fb(b[3]);r.viewer={fb(b[4]),fb(b[5]),fb(b[6])};r.range=fb(b[7]);r.nearMin=fb(b[8]);r.farMax=fb(b[9]);
            std::string cs;while(s>>cs){auto c=cs.find(':');r.chunks.push_back({std::stoi(cs.substr(0,c)),std::stoi(cs.substr(c+1))});}records.push_back(std::move(r));}
    }
    if(records.empty())return 2;
    painter.seed(fb(seed[0]),fb(seed[1]),fb(seed[2]),fb(seed[3]),fb(seed[4]));
    const bool seedMatches=fb(seed[3])==records[0].eye[0]&&fb(seed[4])==records[0].eye[1]&&
        originalCameraFarClip(kCameraAlgorithmFar,painter.cap(),records[0].nearMin,records[0].farMax)==records[0].far;
    cold.construct();cold.frame(records[0].eye[0],records[0].eye[1],cameras);
    unsigned stale=0,farErrors=0,rangeErrors=0,coldErrors=0,coldFirstMatch=0,n=0,chunkErr[3]{},changes=0;float lastFar=records[0].far,prevRange=originalStreamingRange(originalStreamingFar(records[0].far,cameras));
    for(size_t k=0;k+1<records.size();++k){
        const Record& rec=records[k+1];
        if(rec.tick!=records[k].tick+1)break;
        const float cap=painter.step(rec.eye[0],rec.eye[1]);
        const float far=originalCameraFarClip(kCameraAlgorithmFar,cap,rec.nearMin,rec.farMax);
        const float range=originalStreamingRange(originalStreamingFar(far,cameras));
        ++n;changes+=rec.far!=lastFar;lastFar=rec.far;stale+=rec.viewer!=rec.eye;
        // the viewer lags the camera on frames where 15E668 ran without 22E8B8/15EC98 (dropped render frame):
        // then the captured range belongs to the previous camera frame.
        const float expectRange=rec.viewer==rec.eye?range:prevRange;prevRange=range;
        if(std::bit_cast<uint32_t>(far)!=std::bit_cast<uint32_t>(rec.far)){if(farErrors++<10)std::printf("record %d far model %.9g captured %.9g (selected %d)\n",rec.tick,far,rec.far,painter.selected);}
        if(std::bit_cast<uint32_t>(expectRange)!=std::bit_cast<uint32_t>(rec.range)){if(rangeErrors++<10)std::printf("record %d range model %.9g captured %.9g\n",rec.tick,expectRange,rec.range);}
        const auto c=cold.frame(rec.eye[0],rec.eye[1],cameras);
        if(c.far!=rec.far)++coldErrors;else if(!coldFirstMatch)coldFirstMatch=unsigned(rec.tick);
        sections::ChunkStreaming* runs[3]={&predicted,&captured,&fixed};const float ranges[3]={expectRange,rec.range,45000.f};
        for(unsigned r=0;r<3;++r){
            runs[r]->frame(rec.tick,rec.tick,rec.viewer,ranges[r]);bool ok=true;
            for(auto [id,st]:rec.chunks){auto it=runs[r]->chunks.find(id);if(it!=runs[r]->chunks.end()&&it->second.state!=st)ok=false;}
            if(!ok)++chunkErr[r];
        }
    }
    std::printf("seed consistent with record 0: %s; records %u (far changes %u, stale viewer %u): far bitwise errors %u, range bitwise errors %u; "
                "cold start (ctor at record 0): far errors %u, first exact record %u; chunk-state record errors: predicted range %u, captured range %u, fixed 45000 %u\n",
                seedMatches?"yes":"NO",n,changes,stale,farErrors,rangeErrors,coldErrors,coldFirstMatch,chunkErr[0],chunkErr[1],chunkErr[2]);
    return seedMatches&&!farErrors&&!rangeErrors&&!chunkErr[0]?0:1;
}
