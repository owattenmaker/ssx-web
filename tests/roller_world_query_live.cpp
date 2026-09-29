// Live check of engine/roller_world_query.hpp against the complete original
// 0x3303F0 -> 0x336850(0x2D1BE0()) -> 0x3304E8 query (and 0x335D78 for the full
// packet list) on carve-bag savestates, versus the port on the browser's
// native world (world_collision.json + terrain.json). Poses: the two roller
// colliders as saved, the recorded roller trajectory (carve-bag.bin watch
// windows), and random rotations/translations set through the original
// 0x32C648 around the bags, the real-surface instances and over the slope.
#include "live_registrations.inc"
#include "original_live_runtime.hpp"
#include "../engine/roller_world_query.hpp"
#include <memory>
// tests/roller_world_query_live_world.mm: loadWorldBodyCollision with the terrain source hash.
std::unique_ptr<ssx::WorldBodyCollision> loadRollerNativeWorld(const char* folder);
#include <map>
#include <random>
#include <set>
#include <algorithm>
using namespace ssx;
using Quad=RollerQuad;
static constexpr uint32_t gp=0x4a30f0,QUERY=0x1D60000,PACKET=0x1D61000,PACKETS=0x1D62000,CACHE2=0x1D6A000,MATRIX=0x1D6A100;
static uint32_t bits(float f){return std::bit_cast<uint32_t>(f);}
static bool same(const Quad& a,const Quad& b){for(unsigned k=0;k<4;++k)if(bits(a[k])!=bits(b[k]))return false;return true;}
static std::string hexq(const Quad& q){char b[128];snprintf(b,sizeof b,"%a %a %a %a",q[0],q[1],q[2],q[3]);return b;}

struct Live {
    live::Machine& m;const WorldBodyCollision& world;
    std::map<uint32_t,uint32_t> patchRid,instanceRid;std::map<uint32_t,uint32_t> ridPatch;
    uint32_t worldPointer=0;
    template<class T=uint32_t> T get(uint32_t a){return m.get<T>(a);}
    // gray: 0x33CCF8 child order 0,1,3,2,6,7,5,4 (partially overlapping nodes); else 0x340FA0 0..7.
    void walk(uint32_t node,std::vector<uint32_t>& patchOrder,bool gray=false){
        if(!node)return;
        for(uint32_t p=get(node+0x24);p;p=get(p)){patchRid[p]=get(p+0x150);ridPatch[get(p+0x150)]=p;patchOrder.push_back(p);}
        for(uint32_t i=get(node+0x20);i;i=get(i))instanceRid[i]=get(i+0x78);
        static constexpr unsigned grayOrder[8]={0,1,3,2,6,7,5,4};
        for(unsigned c=0;c<8;++c)walk(get(node+4*(gray?grayOrder[c]:c)),patchOrder,gray);
    }
};
struct Stats {unsigned cases=0,hits=0,terrainHits=0,instanceHits=0,packets=0,terrainPackets=0,instancePackets=0,multi=0,cacheWrites=0,cacheClears=0,instanceCases=0;};

int main(int argc,char** argv){
    if(argc<4){fprintf(stderr,"usage: ASSETS TRAJECTORY STATE...(ee,vuc,vud,tick)\n");return 2;}
    auto world=loadRollerNativeWorld(argv[1]);
    const CollisionSphereTree* bagTree=nullptr;
    for(const auto& inst:world->instances)for(const auto& node:inst.nodes)if(node.sphereTreeResource==0xaa08&&node.sphereTree)bagTree=node.sphereTree.get();
    if(!bagTree){fprintf(stderr,"no crashbag tree 0xAA08\n");return 3;}
    std::vector<uint8_t> trajectory=live::readFile(argv[2]);
    Stats total;std::mt19937 rng(0x3303f0);
    auto U=[&](float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);};
    for(int s=3;s+3<argc;s+=4) {
        live::Machine m(argv[s],argv[s+1],argv[s+2]);unsigned tick=unsigned(atoi(argv[s+3]));
        Live L{m,*world};std::vector<uint32_t> patchOrder;
        auto c=m.call(0x2d1be0);L.worldPointer=GPR_U32((&c),2);
        for(unsigned slot=0;slot<8;++slot)L.walk(L.get(L.worldPointer+slot*20+16),patchOrder);
        Stats st;
        // --- static facts the port relies on ---------------------------------
        std::map<uint32_t,const WorldCollisionTerrain*> nativePatch;for(const auto& p:world->terrain)nativePatch[p.resource]=&p;
        unsigned flagDiff=0,boundDiff=0,missing=0;
        for(auto p:patchOrder){auto it=nativePatch.find(L.patchRid[p]);if(it==nativePatch.end()){++missing;continue;}const auto& n=*it->second;
            if(uint32_t(m.get<uint16_t>(p+0xa))!=(n.flags|0x40u))++flagDiff;
            for(unsigned k=0;k<3;++k)if(bits(m.get<float>(p+0x158+4*k))!=bits(n.low[k])||bits(m.get<float>(p+0x164+4*k))!=bits(n.high[k]))++boundDiff;}
        // terrain relative order: octree order == native order restricted to loaded patches
        std::map<uint32_t,size_t> nativeIndex;for(size_t i=0;i<world->terrain.size();++i)nativeIndex[world->terrain[i].resource]=i;
        unsigned orderDiff=0;size_t previous=SIZE_MAX;
        for(auto p:patchOrder){auto it=nativeIndex.find(L.patchRid[p]);if(it==nativeIndex.end())continue;if(previous!=SIZE_MAX&&previous>it->second)++orderDiff;previous=it->second;}
        // Octree patches the browser world lacks (streamed tracks other than 8): queries overlapping them are excluded.
        std::vector<uint32_t> absentPatches;std::set<unsigned> absentTracks;
        for(auto p:patchOrder)if(!nativePatch.count(L.patchRid[p])){absentPatches.push_back(p);absentTracks.insert(L.patchRid[p]&0xff);}
        unsigned absentInstances=0,absentSurface=0;
        for(auto [pointer,rid]:L.instanceRid){if(!std::count_if(world->instances.begin(),world->instances.end(),[&](auto& i){return i.resource==rid;})){++absentInstances;
            uint32_t desc=L.get(pointer+0x88),model=L.get(pointer+0x80);if(desc&&model)for(uint32_t i=0;i<L.get(model+4);++i)absentSurface+=m.get<int16_t>(desc+0x10+12*i+0xa)!=-1;}}
        // 0x33CCF8 (Gray) visit order vs originalRollerSpatialBefore on the native patches
        std::vector<uint32_t> grayOrder;for(unsigned slot=0;slot<8;++slot)L.walk(L.get(L.worldPointer+slot*20+16),grayOrder,true);
        std::vector<const WorldCollisionTerrain*> sorted;for(const auto& p:world->terrain)sorted.push_back(&p);
        std::stable_sort(sorted.begin(),sorted.end(),[](auto* a,auto* b){if(originalRollerSpatialBefore(a->spatial,b->spatial))return true;if(originalRollerSpatialBefore(b->spatial,a->spatial))return false;return a->insertionOrder>b->insertionOrder;});
        std::map<uint32_t,size_t> grayIndex;for(size_t i=0;i<sorted.size();++i)grayIndex[sorted[i]->resource]=i;
        unsigned grayDiff=0;previous=SIZE_MAX;
        for(auto p:grayOrder){auto it=grayIndex.find(L.patchRid[p]);if(it==grayIndex.end())continue;if(previous!=SIZE_MAX&&previous>it->second)++grayDiff;previous=it->second;}
        printf("tick %u: 0x33CCF8 Gray-order patch sequence vs originalRollerSpatialBefore: %u inversions\n",tick,grayDiff);if(grayDiff)return 10;
        printf("tick %u: %zu octree patches (%u absent natively, tracks",tick,patchOrder.size(),missing);for(auto k:absentTracks)printf(" %u",k);
        printf("): runtime flags != authored|0x40: %u, bounds diffs: %u, order inversions vs native: %u; %u octree instances absent natively (%u real-surface nodes)\n",flagDiff,boundDiff,orderDiff,absentInstances,absentSurface);
        if(flagDiff||boundDiff||orderDiff||absentSurface)return 9;
        unsigned excluded=0;
        // surface instances: runtime route/bounds and the 0x32C0F8 group boxes
        std::map<uint32_t,const WorldCollisionInstance*> nativeInstance;for(const auto& i:world->instances)nativeInstance[i.resource]=&i;
        std::vector<const WorldCollisionInstance*> surfaceInstances;
        for(auto [pointer,rid]:L.instanceRid) {
            auto it=nativeInstance.find(rid);if(it==nativeInstance.end())continue;const auto& n=*it->second;
            bool surface=false;for(auto& node:n.nodes)surface|=node.surface!=-1;if(!surface)continue;
            surfaceInstances.push_back(&n);uint32_t flags=L.get(pointer+8);bool bounds=true;
            for(unsigned k=0;k<3;++k)bounds&=bits(m.get<float>(pointer+0x60+4*k))==bits(n.low[k])&&bits(m.get<float>(pointer+0x6c+4*k))==bits(n.high[k]);
            unsigned groupDiff=0,groups=0;
            auto r=m.call(0x3a6cc8,{L.get(gp+0x16c8),pointer});uint32_t resource=GPR_U32((&r),2);unsigned ordinal=0;
            for(const auto& node:n.nodes) {
                auto mr=m.call(0x3a6d00,{resource,ordinal++});uint32_t mesh=GPR_U32((&mr),2);if(!node.triangles)continue;
                unsigned count=unsigned(node.triangles->indices.size()/3);if(m.get<uint16_t>(mesh)!=count)++groupDiff;
                if(count<11)continue;
                for(unsigned g=0;g<(count+9)/10;++g){++groups;const auto& mm=*node.triangles;terrain_original::Vector lo=mm.vertices[mm.indices[g*30]],hi=lo;
                    for(unsigned t=g*10;t<std::min(g*10+10,count);++t)for(unsigned j=0;j<3;++j)for(unsigned k=0;k<3;++k){lo[k]=std::min(lo[k],mm.vertices[mm.indices[t*3+j]][k]);hi[k]=std::max(hi[k],mm.vertices[mm.indices[t*3+j]][k]);}
                    for(unsigned k=0;k<3;++k)if(bits(m.get<float>(L.get(mesh+8)+24*g+4*k))!=bits(lo[k])||bits(m.get<float>(L.get(mesh+8)+24*g+12+4*k))!=bits(hi[k]))++groupDiff;}
            }
            printf("  surface instance %05x: runtime flags %08x (static %d), bounds equal %d, %u 10-triangle group boxes, diffs %u\n",rid,flags,bool(flags&0x20),bounds,groups,groupDiff);
            if(!(flags&0x20)||!bounds||groupDiff)return 4;
        }
        // 0x32DF28 pool masks for 0xAA08 vs the native stream
        for(uint32_t i=0;i<L.get(gp+0xee0);++i)if(L.get(L.get(gp+0xedc)+4*i)==0xaa08){
            uint32_t buffer=L.get(L.get(gp+0xed8)+4*i);unsigned diff=0;for(size_t j=0;j<bagTree->masks.size();++j)diff+=m.get<uint8_t>(buffer+uint32_t(j))!=bagTree->masks[j];
            printf("  0x32DF28 slot %u holds 0xAA08: %u mask bytes differ from the native tree\n",i,diff);if(diff)return 5;}
        // 0x3279D0 grid entries (built by the emulator before the savestate) vs native coarseGrid
        auto gridCheck=[&](const char* when)->std::set<uint32_t>{
            uint32_t cache=L.get(L.worldPointer+0xa4),buckets=L.get(cache+8),count=L.get(cache+0xc);unsigned entries=0,diffEntries=0,points=0;std::set<uint32_t> ids;
            for(uint32_t b=0;b<count;++b)for(uint32_t e=L.get(buckets+4*b);e;e=L.get(e+0xc)){uint32_t rid=L.get(e);auto it=nativePatch.find(rid);if(it==nativePatch.end())continue;++entries;ids.insert(rid);
                unsigned d=0;for(unsigned i=0;i<100;++i)for(unsigned k=0;k<3;++k)d+=bits(m.get<float>(e+0x10+16*i+4*k))!=bits(it->second->grid[i][k]);points+=d;diffEntries+=d!=0;
                if(d&&diffEntries<4)printf("   grid %05x: %u coordinates differ\n",rid,d);}
            printf("  %s: %u native grid entries in the 0x3279D0 cache, %u differ (%u coordinates)\n",when,entries,diffEntries,points);return ids;};
        auto initialGrids=gridCheck("savestate grids");
        // The recompiled 0x327C00 (VU0 grid program 0x391418/0x391480) is not bit-exact with the
        // emulator; the emulator-built grids equal the native coarseGrid. Replace recompiled-built
        // grids by the native points and reset their lazy cells as 0x327C68 does.
        unsigned rebuiltGrids=0;
        auto fixGrids=[&]{
            uint32_t cache=L.get(L.worldPointer+0xa4),buckets=L.get(cache+8),count=L.get(cache+0xc);
            for(uint32_t b=0;b<count;++b)for(uint32_t e=L.get(buckets+4*b);e;e=L.get(e+0xc)){uint32_t rid=L.get(e);auto it=nativePatch.find(rid);if(it==nativePatch.end())continue;
                bool differs=false;for(unsigned i=0;i<100;++i)for(unsigned k=0;k<3;++k)differs|=bits(m.get<float>(e+0x10+16*i+4*k))!=bits(it->second->grid[i][k]);
                if(!differs)continue;++rebuiltGrids;
                for(unsigned i=0;i<100;++i)for(unsigned k=0;k<3;++k)m.put(e+0x10+16*i+4*k,it->second->grid[i][k]);
                for(unsigned c=0;c<81;++c)for(unsigned o:{0u,0x10u,0x20u})m.put(e+0x650+0x40*c+o,m.get<float>(gp-0x2e04));
                for(unsigned c=0;c<9;++c)for(unsigned o:{0u,0x10u,0x20u})m.put(e+0x1a90+0x40*c+o,m.get<float>(gp-0x2e00));}
        };
        // --- one comparison -------------------------------------------------
        uint32_t modifiers[2]={0x1CEF550,0x1CEF830};
        auto compare=[&](uint32_t modifier,const char* label)->bool {
            const uint32_t col=modifier+0xe0,cacheAt=modifier+0x2c0;
            // header checks
            if(L.get(col+0x98)!=col+0xc0||L.get(col+0xc0)!=0xaa08)throw std::runtime_error("collider tree header");
            OriginalSphereTreeCollider collider;
            for(unsigned i=0;i<8;++i)collider.corners[i]=m.get<Quad>(col+16*i);
            collider.center=m.get<Quad>(col+0x80);collider.scale=m.get<float>(col+0x90);collider.depth=m.get<float>(col+0x94);
            for(unsigned i=0;i<8;++i)collider.order[i]=m.get<int32_t>(col+0x9c+4*i);
            collider.tree=bagTree;collider.treeResource=0xaa08;
            OriginalRollerTerrainCache cache;uint32_t rawPatch=L.get(cacheAt);
            cache.patch=L.patchRid.count(rawPatch)?L.patchRid[rawPatch]:rawPatch;
            if(!L.patchRid.count(rawPatch)&&L.ridPatch.count(rawPatch))throw std::runtime_error("garbage cache aliases a patch id");
            cache.cellU=m.get<uint16_t>(cacheAt+4);cache.cellV=m.get<uint16_t>(cacheAt+6);cache.half=L.get(cacheAt+8);cache.detailed=L.get(cacheAt+12);
            {   // world-data gap: the query box reaches a patch the native world does not have
                float r=terrain_original::mul(bagTree->levels[0].radiusCm,collider.scale);OriginalRounding rr;
                for(auto p:absentPatches){if((m.get<uint16_t>(p+0xa)&0x41)!=0x41)continue;bool overlap=true;
                    for(unsigned k=0;k<3;++k)overlap&=m.get<float>(p+0x158+4*k)<originalScalarAdd(collider.center[k],r)&&originalScalarSubtract(collider.center[k],r)<m.get<float>(p+0x164+4*k);
                    if(overlap){++excluded;return true;}}
            }
            std::array<uint8_t,0x140> savedCollider;std::memcpy(savedCollider.data(),m.ee.data()+col,0x140);
            // pass 0: let the original build the grid entries of this query, then make them emulator-exact
            for(unsigned k=0;k<16;k+=4)m.put(CACHE2+k,L.get(cacheAt+k));
            m.call(0x3303f0,{QUERY,col,1});m.call(0x335d78,{L.worldPointer,QUERY,PACKETS,64,CACHE2});m.call(0x3304e8,{QUERY,2});
            std::memcpy(m.ee.data()+col,savedCollider.data(),0x140);fixGrids();
            // pass 1: full 0x335D78 list on a cache copy
            for(unsigned k=0;k<16;k+=4)m.put(CACHE2+k,L.get(cacheAt+k));
            m.call(0x3303f0,{QUERY,col,1});auto r1=m.call(0x335d78,{L.worldPointer,QUERY,PACKETS,64,CACHE2});m.call(0x3304e8,{QUERY,2});
            unsigned count=GPR_U32((&r1),2);
            std::memcpy(m.ee.data()+col,savedCollider.data(),0x140);
            // pass 2: the real selection
            std::array<uint8_t,128> sentinel;for(auto& b:sentinel)b=uint8_t(rng());std::memcpy(m.ee.data()+PACKET,sentinel.data(),128);
            m.call(0x3303f0,{QUERY,col,1});auto r2=m.call(0x336850,{L.worldPointer,QUERY,PACKET,cacheAt});m.call(0x3304e8,{QUERY,2});
            float depth=r2.f[0];
            // port
            auto port=originalRollerWorldQueryDetailed(*world,collider,cache);
            auto bad=[&](const char* what){printf("MISMATCH %s: tick %u %s, %u original packets / %zu port\n",what,tick,label,count,port.packets.size());return false;};
            // packet lists
            if(count!=port.packets.size())return bad("packet count");
            std::vector<OriginalRollerWorldPacket> original;
            for(unsigned i=0;i<count;++i){uint32_t p=PACKETS+128*i;OriginalRollerWorldPacket e;e.point=m.get<Quad>(p);e.normal=m.get<Quad>(p+16);e.depth=m.get<float>(p+0x40);
                uint32_t inst=L.get(p+0x50),patch=L.get(p+0x54);e.terrain=inst==0;e.resource=e.terrain?L.patchRid.at(patch):L.get(inst+0x78);e.node=e.terrain?0:L.get(p+0x5c);e.triangle=e.terrain?-1:int32_t(L.get(p+0x60));
                e.surface=m.get<int32_t>(p+0x4c);if(e.terrain){e.u=m.get<float>(p+0x6c);e.v=m.get<float>(p+0x70);}original.push_back(e);}
            auto split=[](const std::vector<OriginalRollerWorldPacket>& v,bool terrain){std::vector<OriginalRollerWorldPacket> out;for(auto& e:v)if(e.terrain==terrain)out.push_back(e);
                if(!terrain)std::stable_sort(out.begin(),out.end(),[](auto& a,auto& b){return a.resource<b.resource;});return out;};
            for(bool terrain:{true,false}){auto a=split(original,terrain),b=split(port.packets,terrain);
                for(size_t i=0;i<a.size();++i){const auto& x=a[i];const auto& y=b[i];
                    if(!same(x.point,y.point)||!same(x.normal,y.normal)||bits(x.depth)!=bits(y.depth)||x.resource!=y.resource||x.node!=y.node||x.triangle!=y.triangle||x.surface!=y.surface||(terrain&&(bits(x.u)!=bits(y.u)||bits(x.v)!=bits(y.v)))){
                        printf(" packet %zu terrain %d: orig %s | %s | %a res %x node %u tri %d\n   port %s | %s | %a res %x node %u tri %d\n",i,terrain,hexq(x.point).c_str(),hexq(x.normal).c_str(),x.depth,x.resource,x.node,x.triangle,hexq(y.point).c_str(),hexq(y.normal).c_str(),y.depth,y.resource,y.node,y.triangle);
                        gridCheck("at mismatch");return bad("packet contents");}}}
            // selection
            if(bits(depth)!=bits(port.contact.depth))return bad("selected depth");
            if(port.contact.hit) {
                if(!same(m.get<Quad>(PACKET),port.contact.point)||!same(m.get<Quad>(PACKET+16),port.contact.normal)||bits(m.get<float>(PACKET+0x40))!=bits(port.contact.depth))return bad("selected packet");
                uint32_t inst=L.get(PACKET+0x50);uint32_t rid=inst?L.get(inst+0x78):L.patchRid.at(L.get(PACKET+0x54));
                if(rid!=port.contact.resource||bool(inst)==port.contact.terrain)return bad("selected identity");
            } else if(std::memcmp(m.ee.data()+PACKET,sentinel.data(),128))return bad("packet written without contact");
            // terrain cache (pointer <-> patch id) and collider scratch
            // A patch id maps back to its pointer; any other value is the untouched original word.
            uint32_t expectedPointer=L.ridPatch.count(cache.patch)?L.ridPatch[cache.patch]:cache.patch;
            if(L.get(cacheAt)!=expectedPointer||m.get<uint16_t>(cacheAt+4)!=cache.cellU||m.get<uint16_t>(cacheAt+6)!=cache.cellV||L.get(cacheAt+8)!=cache.half||L.get(cacheAt+12)!=cache.detailed)return bad("terrain cache");
            if(bits(m.get<float>(col+0x94))!=bits(collider.depth)){
                printf(" +0x94 orig %a port %a centre %s\n",m.get<float>(col+0x94),collider.depth,hexq(collider.center).c_str());
                float r=terrain_original::mul(bagTree->levels[0].radiusCm,collider.scale);
                for(auto [pointer,rid]:L.instanceRid){bool ov=true;for(unsigned k=0;k<3;++k)ov&=m.get<float>(pointer+0x60+4*k)<collider.center[k]+r&&collider.center[k]-r<m.get<float>(pointer+0x6c+4*k);
                    if(ov)printf("  instance %x flags %08x entity %x desc %x\n",rid,L.get(pointer+8),L.get(pointer+0xc),L.get(pointer+0x88));}
                for(auto p:patchOrder){bool ov=true;for(unsigned k=0;k<3;++k)ov&=m.get<float>(p+0x158+4*k)<collider.center[k]+r&&collider.center[k]-r<m.get<float>(p+0x164+4*k);
                    if(ov)printf("  patch %x flags %x\n",L.patchRid[p],m.get<uint16_t>(p+0xa));}
                gridCheck("at mismatch");return bad("collider +0x94");}
            for(unsigned i=0;i<8;++i)if(m.get<int32_t>(col+0x9c+4*i)!=collider.order[i])return bad("collider +0x9C");
            if(std::memcmp(m.ee.data()+col,savedCollider.data(),0x94))return bad("collider pose modified");
            ++st.cases;st.packets+=count;st.multi+=count>1;
            for(auto& e:port.packets){if(e.terrain)++st.terrainPackets;else ++st.instancePackets;}
            if(port.contact.hit){++st.hits;if(port.contact.terrain)++st.terrainHits;else ++st.instanceHits;}
            bool anyInstance=false;for(auto& e:port.packets)anyInstance|=!e.terrain;st.instanceCases+=anyInstance;
            if(rawPatch!=L.get(cacheAt))++st.cacheWrites;if(L.get(cacheAt)==0&&rawPatch!=0)++st.cacheClears;
            return true;
        };
        auto pose=[&](uint32_t modifier,const float r[9],Quad translation,float scale) {
            float matrix[16]={r[0],r[1],r[2],0,r[3],r[4],r[5],0,r[6],r[7],r[8],0,translation[0],translation[1],translation[2],1};
            for(unsigned i=0;i<16;++i)m.put(MATRIX+4*i,matrix[i]);
            m.call(0x32c648,{modifier+0xe0,MATRIX},{scale});
        };
        auto rotation=[&](float out[9]){float q[4],l=0;for(auto& x:q){x=U(-1,1);l+=x*x;}l=std::sqrt(l);for(auto& x:q)x/=l;float w=q[0],x=q[1],y=q[2],z=q[3];
            float r[9]={1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y),2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x),2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y)};std::memcpy(out,r,sizeof r);};
        auto treeCenterOffset=[&](){return Quad{bagTree->centerCm[0],bagTree->centerCm[1],bagTree->centerCm[2],0};};
        // (1) the saved rollers
        for(auto mod:modifiers)if(!compare(mod,"saved roller"))return 6;
        unsigned saved=st.cases;
        // (2) recorded trajectory (watch windows; collider and cache of both rollers)
        unsigned recorded=0;
        if(tick==607)for(size_t k=0;k*16384<trajectory.size();++k) {
            uint32_t t;std::memcpy(&t,trajectory.data()+k*16384+4,4);if(t<604||t>778)continue;
            const uint8_t* window=trajectory.data()+k*16384+10240;
            for(unsigned which=0;which<2;++which) {
                uint32_t mod=modifiers[which];const uint8_t* src=window+(which?0x2f0:0x10);
                uint32_t header;std::memcpy(&header,src+0xe0+0xc0,4);if(header!=0xaa08)continue;
                std::memcpy(m.ee.data()+mod+0xe0,src+0xe0,0x98);std::memcpy(m.ee.data()+mod+0xe0+0x9c,src+0xe0+0x9c,0x20);
                std::memcpy(m.ee.data()+mod+0x2c0,src+0x2c0,16);
                char label[64];snprintf(label,sizeof label,"trajectory record tick %u roller %u",t,which);
                unsigned before=st.cases;if(!compare(mod,label))return 7;recorded+=st.cases-before;
            }
        }
        // (3) random poses via 0x32C648
        std::vector<Quad> anchors;
        for(auto mod:modifiers){Quad c=m.get<Quad>(mod+0xe0+0x80);if(std::fabs(c[0])>1000)anchors.push_back(c);}
        for(auto [pointer,rid]:L.instanceRid)if(rid==0x47508||rid==0x7708)anchors.push_back({m.get<float>(pointer+0x60),m.get<float>(pointer+0x64),m.get<float>(pointer+0x68),1});
        std::vector<Quad> surfaceAnchors;for(auto* inst:surfaceInstances)for(auto& node:inst->nodes)if(node.surface!=-1)surfaceAnchors.push_back({node.world[12],node.world[13],node.world[14],1});
        unsigned randomCases=0;const float R0=bagTree->levels[0].radiusCm;
        for(unsigned n=0;n<(tick==607?4000u:1200u);++n) {
            uint32_t mod=modifiers[n&1];float r[9];rotation(r);float scale=n%7==0?U(.5f,1.5f):1.f;Quad center;unsigned kind=n%10;
            if(kind<3&&!anchors.empty()){center=anchors[size_t(rng()%anchors.size())];for(unsigned k=0;k<3;++k)center[k]+=U(-250,250);}
            else if(kind<5&&!surfaceAnchors.empty()){center=surfaceAnchors[size_t(rng()%surfaceAnchors.size())];for(unsigned k=0;k<3;++k)center[k]+=U(-400,400);}
            else {
                // over the slope: a loaded patch near the bags (kind 5..7) or anywhere (8..9)
                const WorldCollisionTerrain* patch=nullptr;
                for(unsigned tries=0;tries<200&&!patch;++tries){uint32_t p=patchOrder[size_t(rng()%patchOrder.size())];auto it=nativePatch.find(L.patchRid[p]);if(it==nativePatch.end()||!(it->second->flags&1))continue;
                    const auto& g=it->second->grid[0];if(kind<8&&!anchors.empty()&&std::fabs(g[0]-anchors[0][0])+std::fabs(g[1]-anchors[0][1])>30000)continue;patch=it->second;}
                if(!patch)continue;
                unsigned u=unsigned(rng()%9),v=unsigned(rng()%9);auto a=patch->grid[u*10+v],b=patch->grid[(u+1)*10+v],c=patch->grid[u*10+v+1];
                float e[3],f[3],nn[3];for(unsigned k=0;k<3;++k){e[k]=b[k]-a[k];f[k]=c[k]-a[k];}
                nn[0]=e[1]*f[2]-e[2]*f[1];nn[1]=e[2]*f[0]-e[0]*f[2];nn[2]=e[0]*f[1]-e[1]*f[0];float l=std::sqrt(nn[0]*nn[0]+nn[1]*nn[1]+nn[2]*nn[2]);if(l==0)l=1;
                if(nn[2]<0)l=-l;float d=U(-.3f,1.2f)*R0*scale;
                for(unsigned k=0;k<3;++k)center[k]=a[k]+U(0,1)*e[k]+U(0,1)*f[k]*.5f+nn[k]/l*d;center[3]=1;
            }
            // 0x32C648 places the tree centre at M*(c*s); pick the translation that puts it at `center`.
            Quad offset=treeCenterOffset();Quad translation;
            for(unsigned k=0;k<3;++k)translation[k]=center[k]-(r[k]*offset[0]+r[3+k]*offset[1]+r[6+k]*offset[2])*scale;
            pose(mod,r,translation,scale);
            if(rng()%5==0)m.put(mod+0x2c0,0u);
            char label[64];snprintf(label,sizeof label,"random pose %u kind %u",n,kind);
            unsigned before=st.cases;if(!compare(mod,label))return 8;randomCases+=st.cases-before;
        }
        printf("  tick %u: %u recompiled 0x3279D0 grids replaced by the native (emulator-exact) grid\n",tick,rebuiltGrids);
        printf("  tick %u: %u queries excluded (box reaches a patch of a track absent from terrain.json)\n",tick,excluded);
        printf("  tick %u: %u queries match (%u saved rollers, %u trajectory records, %u random poses): %u hits (%u terrain, %u instance), %u packets (%u terrain, %u instance; %u queries with several), %u queries with instance contacts, %u cache rewrites (%u clears)\n",
            tick,st.cases,saved,recorded,randomCases,st.hits,st.terrainHits,st.instanceHits,st.packets,st.terrainPackets,st.instancePackets,st.multi,st.instanceCases,st.cacheWrites,st.cacheClears);
        total.cases+=st.cases;total.hits+=st.hits;total.packets+=st.packets;total.instancePackets+=st.instancePackets;
        if(getenv("COUNTS"))for(auto [pc,count]:callCounts)fprintf(stderr,"%06x %llu\n",pc,(unsigned long long)count);
    }
    printf("roller world query live: %u queries match exactly (%u hits, %u packets, %u instance packets)\n",total.cases,total.hits,total.packets,total.instancePackets);
}
