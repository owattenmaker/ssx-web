// Instruction oracle for engine/roller_world_query.hpp. Runs the recompiled
// originals (PCSX2 scalar-FP oracle copies, tools/original_live_build.py) on
// synthetic memory laid over a carve-bag savestate (gp constants, VU0 0x6E0
// microprogram) and compares every output with the port:
//   A 0x330788 / 0x330828 (-> 0x32CA78 / 0x328030, 0x32DB40, 0x32DA40, 0x32D470,
//     0x32D440, 0x32B6A8, 0x32DF28 with its shared pools): random trees, masks,
//     levels, scales, orientations and triangles; point/normal/depth, +0x94, +0x9C.
//   B 0x32C0F8 (+0x330540 groups) with random small meshes, single/double sided.
//   C 0x335960 -> 0x32B6E0 coarse (0x3279D0 stubbed to the synthetic grid entry)
//     with random grids, flags, bounds and terrain-cache states.
//   D 0x336850 with 0x335D78 stubbed to random packet lists.
//   E 0x334888 (filter 2, type 1) on synthetic instance hierarchies, two per
//     query (shared scratch collider), 0x3A6CC8/0x3A6D00 stubbed.
#include "live_registrations.inc"
#include "original_live_runtime.hpp"
#include "../engine/roller_world_query.hpp"
#include <cmath>
#include <cstdio>
#include <random>
using namespace ssx;
using Quad=RollerQuad;
static live::Machine* M;
static constexpr uint32_t R=0x1D60000;
static constexpr uint32_t Q=R,C=R+0x100,SCR=R+0x300,LEV=R+0x500,MASK=R+0x600,SRC=R+0xA000,TRI=R+0x14000,OUT=R+0x14100;
static constexpr uint32_t POOL=R+0x15000,SBUF=R+0x16000,DBUF=R+0x20000;
static constexpr uint32_t MESH=R+0x40000,MIDX=R+0x40100,MGRP=R+0x40400,MVTX=R+0x40800,MNRM=R+0x42000,MPKT=R+0x48000;
static constexpr uint32_t GRID=R+0x50000,PATCH=R+0x53000,CACHE=R+0x53400,CTX=R+0x53500,COUNT=R+0x53580,TPKT=R+0x54000,GC=R+0x53600;
static constexpr uint32_t INST=R+0x60000,PAT=R+0x70000,SELOUT=R+0x78000;
static constexpr uint32_t OBJ=R+0x80000,MODEL=R+0x81000,NODES=R+0x81100,DESC=R+0x81400,ND=R+0x81800,LOCAL=R+0x82000,MESHES=R+0x84000,IPKT=R+0xC0000; // meshes: 8 x 0x4000
static constexpr uint32_t gp=0x4a30f0;
static std::mt19937 rng(0x336850);
static float U(float a,float b){return std::uniform_real_distribution<float>(a,b)(rng);}
static unsigned N(unsigned n){return unsigned(rng()%n);}
static uint32_t bits(float f){return std::bit_cast<uint32_t>(f);}
template<class T> static void put(uint32_t a,const T& v){M->put(a,v);}
template<class T=uint32_t> static T get(uint32_t a){return M->get<T>(a);}
static Quad quad(uint32_t a){return get<Quad>(a);}
static bool same(const Quad& a,const Quad& b){for(unsigned k=0;k<4;++k)if(bits(a[k])!=bits(b[k]))return false;return true;}
static void fail(const char* what,unsigned n){printf("MISMATCH %s case %u\n",what,n);exit(1);}
static R5900Context call(uint32_t pc,std::initializer_list<uint32_t> a,std::initializer_list<float> f={}){return M->call(pc,a,f);}

// ---- sphere trees ---------------------------------------------------------
struct TreeDef {uint32_t key;CollisionSphereTree tree;std::vector<uint8_t> encoded;};
static std::vector<TreeDef> trees;
static std::vector<uint8_t> encode(const std::vector<uint8_t>& masks){ // 0x32B620 literal runs
    std::vector<uint8_t> out;
    for(size_t at=0;at<masks.size();){size_t n=std::min<size_t>(128,masks.size()-at);out.push_back(uint8_t(int8_t(-int(n))));out.insert(out.end(),masks.begin()+at,masks.begin()+at+n);at+=n;}
    out.push_back(0);return out;
}
static CollisionSphereTree randomTree(unsigned levels,const CollisionSphereTree* shape=nullptr){
    CollisionSphereTree t;
    if(shape){t=*shape;}
    else {
        t.centerCm={U(-30,30),U(-30,30),U(-30,30)};float r=U(15,180);uint32_t stride=1;
        for(unsigned l=0;l<levels;++l){float offset=l?r*U(.45f,.95f):0.f;t.levels.push_back({r,offset,stride});stride*=8;r*=U(.4f,.72f);}
        if(N(4)==0)for(unsigned l=1;l<levels;++l)t.levels[l].childOffsetCm=t.levels[l-1].radiusCm*U(.2f,1.3f);
    }
    size_t count=0;for(auto& l:t.levels)count+=l.stride;
    t.masks.assign(count,0);float density=U(.2f,1.f);
    for(auto& m:t.masks){uint8_t v=0;for(unsigned b=0;b<8;++b)if(U(0,1)<density)v|=uint8_t(1u<<b);m=v;}
    if(N(3))t.masks[0]=uint8_t(t.masks[0]|1u<<N(8));
    t.compressed=N(3)!=0;return t;
}
static void buildTrees(const CollisionSphereTree& bag){
    // 12 keys; keys 3 and 7 are shared by two trees of one shape with different masks (0x32DF28 alias).
    for(unsigned i=0;i<16;++i) {
        TreeDef d;unsigned levels=1+N(5);if(i==5||i==11)levels=6; // deep pool (depth 5)
        if(i==0){d.tree=bag;d.tree.compressed=true;}
        else if(i==12||i==13)d.tree=randomTree(0,&trees[i==12?3:7].tree);
        else if(i==14||i==15)d.tree=randomTree(0,&bag);
        else d.tree=randomTree(levels);
        d.key=i==12?trees[3].key:i==13?trees[7].key:0x10008+0x100*i;
        if(i==12||i==13)d.tree.compressed=trees[i==12?3:7].tree.compressed;
        d.encoded=encode(d.tree.masks);trees.push_back(d);
    }
}
static void resetPools(WorldBodyCollision& world){
    put(gp+0xed8,POOL);put(gp+0xedc,POOL+0x40);put(gp+0xee0,10u);put(gp+0xee4,0u);
    put(gp+0xee8,POOL+0x80);put(gp+0xeec,POOL+0x90);put(gp+0xef0,2u);put(gp+0xef4,0u);put(gp+0xef8,5u);
    for(uint32_t i=0;i<10;++i){put(POOL+4*i,SBUF+i*0x1400);put(POOL+0x40+4*i,0xffffffffu);}
    for(uint32_t i=0;i<2;++i){put(POOL+0x80+4*i,DBUF+i*0xA000);put(POOL+0x90+4*i,0xffffffffu);}
    world.resetSphereTreeCache();
}
// Tree header embedded at collider+0xC0 (as RollerModifier+0x1A0).
static void writeTree(uint32_t collider,const TreeDef& d){
    const auto& t=d.tree;uint32_t h=collider+0xc0;
    put(h,d.key);put(h+4,uint32_t(N(10)));put(h+8,uint32_t(t.compressed));put(h+0xc,uint32_t(t.levels.size()-1));put(h+0x10,1u);
    put(h+0x14,t.centerCm);put(h+0x20,LEV);put(h+0x24,SRC);put(h+0x28,MASK);
    for(size_t l=0;l<t.levels.size();++l){put(LEV+12*l,t.levels[l].radiusCm);put(LEV+12*l+4,t.levels[l].childOffsetCm);put(LEV+12*l+8,t.levels[l].stride);}
    put(LEV+12*t.levels.size(),0.f);put(LEV+12*t.levels.size()+4,0.f);put(LEV+12*t.levels.size()+8,0u);
    std::memcpy(M->ee.data()+MASK,t.masks.data(),t.masks.size());std::memcpy(M->ee.data()+SRC,d.encoded.data(),d.encoded.size());
    put(collider+0x98,h);
}
// Random rotation * 0x4FF640 table * scale; corners[7-i] = -corners[i] (w -0).
static std::array<float,9> rotation(){
    float q[4];float l=0;for(auto& x:q){x=U(-1,1);l+=x*x;}l=std::sqrt(l);for(auto& x:q)x/=l;
    float w=q[0],x=q[1],y=q[2],z=q[3];
    return {1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y),2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x),2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y)};
}
static OriginalSphereTreeCollider writeCollider(uint32_t at,const TreeDef& d,Quad center,float scale){
    OriginalSphereTreeCollider c;auto r=rotation();
    for(unsigned i=0;i<4;++i) {
        float t[3]={i&4?1.f:-1.f,i&2?1.f:-1.f,i&1?1.f:-1.f};
        for(unsigned k=0;k<3;++k)c.corners[i][k]=(r[k]*t[0]+r[3+k]*t[1]+r[6+k]*t[2])*scale;
        c.corners[i][3]=0;for(unsigned k=0;k<4;++k)c.corners[7-i][k]=-c.corners[i][k];
    }
    c.center=center;c.scale=scale;c.depth=U(-5,5);for(auto& o:c.order)o=int32_t(rng());
    c.tree=&d.tree;c.treeResource=d.key;
    for(unsigned i=0;i<8;++i)put(at+16*i,c.corners[i]);
    put(at+0x80,c.center);put(at+0x90,c.scale);put(at+0x94,c.depth);put(at+0x9c,c.order);writeTree(at,d);
    return c;
}
static void checkCollider(uint32_t at,const OriginalSphereTreeCollider& c,const char* what,unsigned n){
    if(bits(get<float>(at+0x94))!=bits(c.depth))fail(what,n);
    for(unsigned i=0;i<8;++i)if(int32_t(get(at+0x9c+4*i))!=c.order[i])fail(what,n);
}
static void writeQuery(uint32_t collider,uint32_t mode=0){
    put(Q,mode);put(Q+4,0u);put(Q+8,0u);put(Q+0x50,0x48e5f0u);put(Q+0x60,collider);put(Q+0x64,collider);put(Q+0x68,SCR);
}
struct Triangle {Quad a,b,c,n;};
static Quad unitNormal(const Quad& a,const Quad& b,const Quad& c){
    double e[3],f[3],n[3];for(int k=0;k<3;++k){e[k]=b[k]-a[k];f[k]=c[k]-b[k];}
    n[0]=e[1]*f[2]-e[2]*f[1];n[1]=e[2]*f[0]-e[0]*f[2];n[2]=e[0]*f[1]-e[1]*f[0];double l=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);if(l==0)l=1;
    return {float(n[0]/l),float(n[1]/l),float(n[2]/l),0.f};
}
static Triangle randomTriangle(Quad around,float size){
    Triangle t;auto p=[&]{return Quad{around[0]+U(-size,size),around[1]+U(-size,size),around[2]+U(-size,size)*(N(2)?.2f:1.f),1.f};};
    t.a=p();t.b=p();t.c=p();
    if(N(40)==0)t.b=t.a;                 // degenerate edge (div.s by zero)
    if(N(40)==0)t.c=t.b;
    t.n=unitNormal(t.a,t.b,t.c);
    if(N(8)==0)for(unsigned k=0;k<3;++k)t.n[k]=-t.n[k];
    if(N(20)==0)for(unsigned k=0;k<3;++k)t.n[k]*=U(.5f,1.5f);
    return t;
}
// Collider centre near a triangle point, on either side, often touching.
static Quad nearCenter(const Triangle& t,float reach){
    float u=U(-.2f,1.f),v=U(-.2f,1.f-std::max(u,0.f)),w=1-u-v;Quad p;
    for(unsigned k=0;k<3;++k)p[k]=t.a[k]*u+t.b[k]*v+t.c[k]*w;
    float d=U(-.4f,1.15f)*reach;for(unsigned k=0;k<3;++k)p[k]+=t.n[k]*d+U(-.1f,.1f)*reach;
    p[3]=1;return p;
}
static float reachOf(const TreeDef& d,float scale){return d.tree.levels[0].radiusCm*scale;}

// ---- stubs ----------------------------------------------------------------
static std::vector<std::array<uint8_t,128>> stubPackets;
static std::vector<uint32_t> meshTable;
static void stub(uint8_t*,R5900Context* c,PS2Runtime*){
    switch(c->pc){
    case 0x335d78:{uint32_t out=GPR_U32(c,6);if(GPR_U32(c,7)!=64)throw std::runtime_error("335D78 capacity");
        for(size_t i=0;i<stubPackets.size();++i)std::memcpy(M->ee.data()+out+128*i,stubPackets[i].data(),128);SET_GPR_U32(c,2,uint32_t(stubPackets.size()));break;}
    case 0x3279d0:put(GPR_U32(c,6),GRID);break;
    case 0x3a6cc8:SET_GPR_U32(c,2,GPR_U32(c,5));break;
    case 0x3a6d00:{uint32_t i=GPR_U32(c,5);if(i>=meshTable.size())throw std::runtime_error("3A6D00 index");SET_GPR_U32(c,2,meshTable[i]);break;}
    default:throw std::runtime_error("unexpected stub");
    }
    c->pc=GPR_U32(c,31);
}

// ---- meshes ---------------------------------------------------------------
static CollisionTriangleMesh randomMesh(Quad around,float size,unsigned triangles){
    CollisionTriangleMesh m;unsigned verts=3+N(std::min(60u,triangles*3));
    for(unsigned i=0;i<verts;++i)m.vertices.push_back({around[0]+U(-size,size),around[1]+U(-size,size),around[2]+U(-size,size)});
    for(unsigned t=0;t<triangles;++t) {
        unsigned a=N(verts),b=N(verts),c=N(verts);m.indices.insert(m.indices.end(),{a,b,c});
        Quad n=unitNormal(Quad{m.vertices[a][0],m.vertices[a][1],m.vertices[a][2],1},Quad{m.vertices[b][0],m.vertices[b][1],m.vertices[b][2],1},Quad{m.vertices[c][0],m.vertices[c][1],m.vertices[c][2],1});
        if(N(6)==0)for(unsigned k=0;k<3;++k)n[k]=-n[k];
        m.normals.push_back({n[0],n[1],n[2]});
    }
    return m;
}
static void writeMesh(uint32_t at,uint32_t idx,uint32_t grp,uint32_t vtx,uint32_t nrm,const CollisionTriangleMesh& m){
    unsigned count=unsigned(m.indices.size()/3);
    put(at,uint16_t(count));put(at+4,idx);put(at+8,grp);put(at+0xc,vtx);put(at+0x10,nrm);
    for(size_t i=0;i<m.indices.size();++i)put(idx+uint32_t(i),uint8_t(m.indices[i]));
    for(size_t i=0;i<m.vertices.size();++i){put(vtx+16*uint32_t(i),m.vertices[i]);put(vtx+16*uint32_t(i)+12,1.f);}
    for(unsigned t=0;t<count;++t){put(nrm+16*t,m.normals[t]);put(nrm+16*t+12,0.f);}
    for(unsigned g=0;g<(count+9)/10;++g) {
        terrain_original::Vector lo=m.vertices[m.indices[g*30]],hi=lo;
        for(unsigned t=g*10;t<std::min(g*10+10,count);++t)for(unsigned j=0;j<3;++j)for(unsigned k=0;k<3;++k){lo[k]=std::min(lo[k],m.vertices[m.indices[t*3+j]][k]);hi[k]=std::max(hi[k],m.vertices[m.indices[t*3+j]][k]);}
        put(grp+24*g,lo);put(grp+24*g+12,hi);
    }
}

// Diagnostic: 0x32D440 inputs/results against terrain_original::pointInTriangle.
static unsigned pointTests=0,pointMismatch=0;
static void tracePoint(uint8_t* m,R5900Context* c,PS2Runtime* r){
    uint32_t a=GPR_U32(c,5),b=GPR_U32(c,6),cc=GPR_U32(c,7),p=GPR_U32(c,8),ra=GPR_U32(c,31);
    Quad A=quad(a),B=quad(b),Cq=quad(cc),P=quad(p);
    sub_0032D440_0x32d440(m,c,r);
    if(c->pc!=ra)throw std::runtime_error("32D440 trace continuation");
    bool port;{OriginalRounding rr;using roller_world_query_detail::xyz;port=terrain_original::pointInTriangle(xyz(A),xyz(B),xyz(Cq),xyz(P));}
    ++pointTests;
    if(port!=bool(GPR_U32(c,2))){if(++pointMismatch<6)printf("32D440 mismatch orig %u port %d A %a %a %a %a B %a %a %a %a C %a %a %a %a P %a %a %a %a\n",GPR_U32(c,2),port,A[0],A[1],A[2],A[3],B[0],B[1],B[2],B[3],Cq[0],Cq[1],Cq[2],Cq[3],P[0],P[1],P[2],P[3]);}
}
static bool traceTree=false;
static void portTrace(const OriginalSphereTreeCollider& col,const std::vector<uint8_t>& masks,const Quad& a,const Quad& b,const Quad& c,const Quad& nrm,const Quad& center,unsigned level,uint32_t index,float& best){
    using namespace roller_world_query_detail;using namespace terrain_original;OriginalRounding rr;const auto& tree=*col.tree;
    float distance=qdot(nrm,qsub(center,a)),radius=mul(tree.levels[level].radiusCm,col.scale);
    uint8_t mask=masks[index];bool leaf=!(level<tree.levels.size()-1)||!(level<3)||mask==0;
    Quad projected=qsub(center,qscale(nrm,distance));bool inside=pointInTriangle(xyz(a),xyz(b),xyz(c),xyz(projected));
    printf("  port level %u index %u dist %.9g r %.9g sum %.9g mask %02x leaf %d inside %d best %.9g\n",level,index,distance,radius,originalScalarAdd(radius,best),mask,leaf,inside,best);
    if(level==0&&distance<0)return;if(!(distance<originalScalarAdd(radius,best)))return;
    if(!inside){if(leaf)return;if(radius<20.f){puts("   r<20");return;}}
    if(leaf){float d=originalScalarSubtract(distance,radius);if(d<best)best=d;printf("   leaf depth %.9g\n",d);return;}
    for(unsigned k=0;k<8;++k){int32_t child=col.order[k];if(!((mask>>child)&1))continue;Quad next=qadd(center,qscale(col.corners[child],tree.levels[level+1].childOffsetCm));portTrace(col,masks,a,b,c,nrm,next,level+1,index+uint32_t(child+1)*tree.levels[level].stride,best);}
}
static void traceRecurse(uint8_t* m,R5900Context* c,PS2Runtime* r){
    uint32_t ra=GPR_U32(c,31),col=GPR_U32(c,4);Quad ctr=quad(GPR_U32(c,9));unsigned level=GPR_U32(c,10),index=GPR_U32(c,11);
    if(traceTree)printf("  32D470 level %u index %u center %.9g %.9g %.9g %.9g best %.9g\n",level,index,ctr[0],ctr[1],ctr[2],ctr[3],get<float>(col+0x94));
    sub_0032D470_0x32d470(m,c,r);
    while(c->pc!=ra)r->lookupFunction(c->pc)(m,c,r);
}
int main(int argc,char** argv){
    if(argc!=5){fprintf(stderr,"usage: EE VUC VUD bag-tree.bin\n");return 2;}
    live::Machine machine(argv[1],argv[2],argv[3]);M=&machine;
    for(uint32_t pc:{0x335d78u,0x3279d0u,0x3a6cc8u,0x3a6d00u})machine.runtime.registerFunction(pc,stub);
    if(getenv("TRACE_POINT"))machine.runtime.registerFunction(0x32d440,tracePoint);
    if(getenv("TRACE_TREE"))machine.runtime.registerFunction(0x32d470,traceRecurse);
    // Real crashbag tree (world_collision.json resource 8:170) passed as a flat file.
    CollisionSphereTree bag;{auto raw=live::readFile(argv[4]);size_t at=0;auto rd=[&](auto& v){std::memcpy(&v,raw.data()+at,sizeof(v));at+=sizeof(v);};
        uint32_t levels,masks;rd(levels);rd(bag.centerCm);for(uint32_t l=0;l<levels;++l){CollisionSphereTreeLevel v;rd(v.radiusCm);rd(v.childOffsetCm);rd(v.stride);bag.levels.push_back(v);}
        rd(masks);bag.masks.assign(raw.begin()+long(at),raw.begin()+long(at+masks));}
    buildTrees(bag);
    WorldBodyCollision world;resetPools(world);
    // ---------------- A: triangle kernels ----------------
    unsigned hits[2]={},cases[2]={},aliasHits=0;
    for(unsigned n=0;n<120000;++n) {
        if(n%997==0)resetPools(world);
        unsigned pick=N(10)<3?0:N(unsigned(trees.size()));const auto& d=trees[pick];
        float scale=N(2)?1.f:U(.3f,2.5f);float reach=reachOf(d,scale);
        Triangle t=randomTriangle({U(-500,500),U(-500,500),U(-200,200),1},reach*U(.3f,4.f));
        Quad center=nearCenter(t,reach);
        auto col=writeCollider(C,d,center,scale);writeQuery(C);
        put(TRI,t.a);put(TRI+16,t.b);put(TRI+32,t.c);put(TRI+48,t.n);
        bool twoSided=n&1;
        Quad sentinel{U(-9,9),U(-9,9),U(-9,9),U(-9,9)};put(OUT,sentinel);put(OUT+16,sentinel);put(OUT+32,sentinel[0]);
        traceTree=getenv("TRACE_TREE")&&n==unsigned(atoi(getenv("TRACE_TREE")));
        if(traceTree){for(auto& l:d.tree.levels)printf("level r %.9g off %.9g stride %u\n",l.radiusCm,l.childOffsetCm,l.stride);printf("masks");for(size_t i=0;i<std::min<size_t>(80,d.tree.masks.size());++i)printf(" %02x",d.tree.masks[i]);puts("");}
        auto c=call(twoSided?0x330828:0x330788,{Q,TRI,TRI+16,TRI+32,TRI+48,OUT,OUT+16,OUT+32});
        OriginalRollerTriangleHit hit;
        {OriginalRounding r;hit=twoSided?originalRollerDoubleSidedTriangle(col,&world,t.a,t.b,t.c,t.n):originalRollerTriangle(col,&world,t.a,t.b,t.c,t.n);}
        if(getenv("TRACE_POINT")&&pointMismatch){printf("point mismatches %u/%u by case %u\n",pointMismatch,pointTests,n);exit(1);}
        if(bool(GPR_U32((&c),2))!=hit.hit){
            printf("pick %u twoSided %d orig %u port %d mem94 %.9g port94 %.9g scale %.9g reach %.9g\n",pick,twoSided,GPR_U32((&c),2),hit.hit,get<float>(C+0x94),col.depth,scale,reach);
            for(unsigned i=0;i<8;++i)printf(" order %d/%d",int32_t(get(C+0x9c+4*i)),col.order[i]);puts("");
            printf("A %.9g %.9g %.9g B %.9g %.9g %.9g C %.9g %.9g %.9g N %.9g %.9g %.9g ctr %.9g %.9g %.9g\n",t.a[0],t.a[1],t.a[2],t.b[0],t.b[1],t.b[2],t.c[0],t.c[1],t.c[2],t.n[0],t.n[1],t.n[2],center[0],center[1],center[2]);
            if(getenv("TRACE_TREE")){auto copy=col;{OriginalRounding rr;originalRollerChildOrder(copy,t.n);}float best=0;
                Quad nn=t.n;{OriginalRounding rr;float dd=roller_world_query_detail::qdot(t.n,roller_world_query_detail::qsub(col.center,t.a));if(twoSided&&dd<0)nn=roller_world_query_detail::qscale(t.n,-1.f);}
                portTrace(copy,originalRollerTreeMasks(&world,copy),t.a,t.b,t.c,nn,copy.center,0,0,best);}
            fail("A hit",n);}
        if(hit.hit){if(!same(quad(OUT),hit.point)||!same(quad(OUT+16),hit.normal)||bits(get<float>(OUT+32))!=bits(hit.depth))fail("A contact",n);++hits[twoSided];if(pick>=12&&pick<=13)++aliasHits;}
        else if(!same(quad(OUT),sentinel)||!same(quad(OUT+16),sentinel))fail("A untouched",n);
        checkCollider(C,col,"A collider scratch",n);++cases[twoSided];
    }
    printf("A: %u single-sided 0x330788 (%u hits) + %u double-sided 0x330828 (%u hits) kernel cases match (point, normal, depth, +0x94, +0x9C; %u alias-key hits)\n",cases[0],hits[0],cases[1],hits[1],aliasHits);
    // ---------------- B: 0x32C0F8 meshes ----------------
    unsigned meshCases=0,meshPackets=0,grouped=0;
    for(unsigned n=0;n<12000;++n) {
        if(n%997==0)resetPools(world);
        const auto& d=trees[N(10)<4?0:N(unsigned(trees.size()))];float scale=N(2)?1.f:U(.5f,2.f),reach=reachOf(d,scale);
        Quad around{U(-300,300),U(-300,300),U(-300,300),1};
        auto mesh=randomMesh(around,reach*U(.4f,2.5f),1+N(N(3)?12:40));unsigned count=unsigned(mesh.indices.size()/3);grouped+=count>=11;
        Quad center=around;for(unsigned k=0;k<3;++k)center[k]+=U(-.8f,.8f)*reach;
        auto col=writeCollider(C,d,center,scale);writeQuery(C);writeMesh(MESH,MIDX,MGRP,MVTX,MNRM,mesh);
        bool twoSided=N(2);
        auto c=call(0x32c0f8,{Q,MESH,uint32_t(twoSided),MPKT,64});
        std::vector<OriginalRollerWorldPacket> packets;
        {OriginalRounding r;packets=originalRollerMesh(col,&world,mesh,twoSided);}
        if(GPR_U32((&c),2)!=packets.size())fail("B count",n);
        for(size_t i=0;i<packets.size();++i) {
            uint32_t p=MPKT+128*uint32_t(i);
            if(!same(quad(p),packets[i].point)||!same(quad(p+16),packets[i].normal)||bits(get<float>(p+0x40))!=bits(packets[i].depth)||int32_t(get(p+0x60))!=packets[i].triangle||get(p+0x44)!=2)fail("B packet",n);
        }
        checkCollider(C,col,"B collider scratch",n);++meshCases;meshPackets+=unsigned(packets.size());
    }
    printf("B: %u 0x32C0F8 meshes (%u with 10-triangle groups) match: %u packets (point, normal, depth, triangle), +0x94, +0x9C\n",meshCases,grouped,meshPackets);
    // ---------------- C: 0x335960 -> 0x32B6E0 coarse ----------------
    unsigned terrainCases=0,terrainHits=0,cachedHits=0,cacheCleared=0,degenerate=0,zeroCells=0;WorldCollisionTerrain patch;
    auto resetGrid=[&]{
        for(uint32_t cell=0;cell<81+9;++cell){uint32_t at=GRID+(cell<81?0x650+0x40*cell:0x1a90+0x40*(cell-81));
            put(at,Quad{1e10f,0,0,0});put(at+16,Quad{1e10f,0,0,0});put(at+32,Quad{1e10f,0,0,0});put(at+48,Quad{0,0,0,0});}
    };
    const uint32_t patchIds[2]={0x6bd08,0x6be08};
    for(unsigned n=0;n<40000;++n) {
        if(n%997==0)resetPools(world);
        if(n%4==0) {
            resetGrid();Quad o{U(-2000,2000),U(-2000,2000),U(-500,500),1};float step=U(40,400),tilt=U(-.8f,.8f),wave=U(0,60);
            for(unsigned u=0;u<10;++u)for(unsigned v=0;v<10;++v) {
                terrain_original::Vector p{o[0]+u*step+U(-5,5),o[1]+v*step+U(-5,5),o[2]+u*step*tilt+wave*std::sin(float(u+v))};
                patch.grid[u*10+v]=p;
            }
            if(N(6)==0){unsigned u=N(9),v=N(9);patch.grid[u*10+v+1]=patch.grid[u*10+v];patch.grid[(u+1)*10+v]=patch.grid[u*10+v];++degenerate;} // zero-area
            if(N(8)==0){unsigned u=3*N(3),v=3*N(3);auto p=patch.grid[u*10+v];for(unsigned k=0;k<3;++k){patch.grid[(u+3)*10+v][k]=p[k]+.001f*k;patch.grid[u*10+v+3][k]=p[k]+.002f;}} // Q > 10
            if(N(3)==0){unsigned u=3*N(3),v=3*N(3);patch.grid[(u+3)*10+v]=patch.grid[u*10+v];patch.grid[u*10+v+3]=patch.grid[u*10+v];++zeroCells;} // exact zero area: VRSQRT(0)
            for(unsigned i=0;i<100;++i){put(GRID+0x10+16*i,patch.grid[i]);put(GRID+0x1c+16*i,1.f);}
            patch.resource=patchIds[N(2)];patch.surface=int(N(20));
            patch.low=patch.grid[0];patch.high=patch.grid[0];
            for(auto& p:patch.grid)for(unsigned k=0;k<3;++k){patch.low[k]=std::min(patch.low[k],p[k]);patch.high[k]=std::max(patch.high[k],p[k]);}
        }
        patch.flags=std::array{9u,11u,8u,1u,0u}[N(5)];if(N(3))patch.flags|=1;
        put(PATCH+0xa,uint16_t(patch.flags|0x40));put(PATCH+8,int16_t(patch.surface));put(PATCH+0x150,patch.resource);
        put(PATCH+0x158,patch.low);put(PATCH+0x164,patch.high);
        const auto& d=trees[N(10)<4?0:N(unsigned(trees.size()))];float scale=N(2)?1.f:U(.5f,2.f),reach=reachOf(d,scale);
        unsigned cu=N(3),cv=N(3);Triangle t;
        {auto P=[&](unsigned u,unsigned v){auto g=patch.grid[u*10+v];return Quad{g[0],g[1],g[2],1};};
         t.a=P(3*cu,3*cv);t.b=P(3*cu+3,3*cv);t.c=P(3*cu,3*cv+3);t.n=unitNormal(t.c,t.b,t.a);for(auto& x:t.n)x=-x;}
        Quad center=nearCenter(t,reach);if(N(10)==0)center={U(-3000,3000),U(-3000,3000),U(-800,800),1};
        auto col=writeCollider(C,d,center,scale);writeQuery(C);
        OriginalRollerQueryBounds bounds;{OriginalRounding r;bounds=originalRollerQueryBounds(col);}
        if(N(10)==0)for(unsigned k=0;k<3;++k){bounds.low[k]-=U(0,300);bounds.high[k]+=U(0,300);}
        put(Q+0x20,bounds.high);put(Q+0x30,bounds.low);
        // Cache state: this patch (random cell/half), the other patch, detailed mismatch, zero or garbage.
        OriginalRollerTerrainCache cache;unsigned kind=N(6);
        if(n%4&&N(2)){cache.patch=get(CACHE)==PATCH?patch.resource:0;cache.cellU=get<uint16_t>(CACHE+4);cache.cellV=get<uint16_t>(CACHE+6);cache.half=get(CACHE+8);cache.detailed=get(CACHE+12);} // carry over
        else if(kind<3){cache={patch.resource,uint16_t(N(3)),uint16_t(N(3)),N(2),0};}
        else if(kind==3){cache={patchIds[0]^patchIds[1]^patch.resource,uint16_t(N(3)),uint16_t(N(3)),N(2),0};}
        else if(kind==4){cache={patch.resource,uint16_t(N(3)),uint16_t(N(3)),N(2),1};}
        else cache={0,uint16_t(rng()),uint16_t(rng()),uint32_t(rng()),uint32_t(rng()%3)};
        if(cache.patch==patch.resource&&cache.detailed==0&&(cache.cellU>2||cache.cellV>2))cache.cellU=cache.cellV=0;
        uint32_t cachePointer=cache.patch==patch.resource?PATCH:cache.patch?PATCH+0x1000:0;
        put(CACHE,cachePointer);put(CACHE+4,cache.cellU);put(CACHE+6,cache.cellV);put(CACHE+8,cache.half);put(CACHE+12,cache.detailed);
        auto before=cache;
        put(CTX,Q);put(CTX+4,TPKT);put(CTX+0xc,COUNT);put(CTX+0x10,CACHE);put(CTX+0x14,GC);put(COUNT,0u);
        call(0x335960,{CTX,PATCH});
        std::optional<OriginalRollerWorldPacket> packet;
        {OriginalRounding r;packet=originalRollerPatchQuery(patch,bounds,col,&world,cache);}
        if(get(COUNT)!=(packet?1u:0u))fail("C count",n);
        if(packet) {
            if(!same(quad(TPKT),packet->point)||!same(quad(TPKT+16),packet->normal)||bits(get<float>(TPKT+0x40))!=bits(packet->depth))fail("C contact",n);
            if(int32_t(get(TPKT+0x4c))!=packet->surface||get(TPKT+0x54)!=PATCH||get(TPKT+0x44)!=2||bits(get<float>(TPKT+0x6c))!=bits(packet->u)||bits(get<float>(TPKT+0x70))!=bits(packet->v))fail("C packet fields",n);
            ++terrainHits;if(before.patch==cache.patch&&before.cellU==cache.cellU&&before.cellV==cache.cellV&&before.half==cache.half&&before.patch==patch.resource&&before.detailed==0)++cachedHits;
        }
        uint32_t expectedPointer=cache.patch==patch.resource?PATCH:cache.patch==0?0:cachePointer;
        if(get(CACHE)!=expectedPointer||get<uint16_t>(CACHE+4)!=cache.cellU||get<uint16_t>(CACHE+6)!=cache.cellV||get(CACHE+8)!=cache.half||get(CACHE+12)!=cache.detailed)fail("C cache",n);
        cacheCleared+=before.patch==patch.resource&&before.detailed==0&&cache.patch==0;
        checkCollider(C,col,"C collider scratch",n);++terrainCases;
    }
    printf("C: %u 0x335960/0x32B6E0 coarse patch queries match (%u contacts, %u from the cached cell, %u cache clears; grids with %u zero-area fine cells, %u zero-area coarse cells): packet, u/v, cache, +0x94, +0x9C\n",terrainCases,terrainHits,cachedHits,cacheCleared,degenerate,zeroCells);
    // ---------------- D: 0x336850 selection ----------------
    unsigned selections=0,ties=0,empty=0;
    for(unsigned n=0;n<40000;++n) {
        unsigned count=n%50==0?0:1+N(N(4)?8:64);stubPackets.assign(count,{});std::vector<OriginalRollerWorldPacket> list(count);
        float depths[5]={U(0,20),U(0,20),U(0,3),0.f,U(-1.5f,-.5f)};
        for(unsigned i=0;i<count;++i) {
            auto& p=list[i];for(auto& b:stubPackets[i])b=uint8_t(rng());
            p.point={U(-9,9),U(-9,9),U(-9,9),1};p.normal={U(-1,1),U(-1,1),U(-1,1),0};p.depth=N(3)?depths[N(5)]:U(-3,30);p.terrain=N(2);
            p.resource=N(6);
            uint32_t inst=INST+0x100*p.resource,pat=PAT+0x200*p.resource;put(inst+0x78,0x100u+p.resource*3);put(pat+0x150,0x200u+p.resource*5);p.resource=p.terrain?0x200u+p.resource*5:0x100u+p.resource*3;
            auto store=[&](unsigned o,auto v){std::memcpy(stubPackets[i].data()+o,&v,sizeof(v));};
            store(0,p.point);store(16,p.normal);store(0x40,p.depth);store(0x50,p.terrain?0u:inst);store(0x54,p.terrain?pat:0u);
        }
        uint32_t mode=N(5)==0;const auto& d=trees[0];auto col=writeCollider(C,d,{0,0,0,1},N(2)?1.f:U(.5f,2.f));writeQuery(C,mode);
        std::array<uint8_t,128> sentinel;for(auto& b:sentinel)b=uint8_t(rng());std::memcpy(M->ee.data()+SELOUT,sentinel.data(),128);
        auto c=call(0x336850,{0x5b9d00,Q,SELOUT,CACHE});
        float preferred=-1.f;if(mode){OriginalRounding r;preferred=originalScalarAdd(terrain_original::mul(d.tree.levels[0].radiusCm,col.scale),terrain_original::mul(d.tree.levels[0].radiusCm,col.scale));}
        int selected=originalRollerSelect(list,preferred);
        if(selected<0){if(bits(c.f[0])!=bits(-1.f)||std::memcmp(M->ee.data()+SELOUT,sentinel.data(),128))fail("D empty",n);++empty;continue;}
        if(bits(c.f[0])!=bits(list[size_t(selected)].depth)||std::memcmp(M->ee.data()+SELOUT,stubPackets[size_t(selected)].data(),128))fail("D selection",n);
        ++selections;for(unsigned i=0;i<count;++i)if(int(i)!=selected&&list[i].depth==list[size_t(selected)].depth){++ties;break;}
    }
    printf("D: %u 0x336850 selections (+%u empty) match the selected 128-byte packet and returned depth (%u with equal-depth rivals)\n",selections,empty,ties);
    // ---------------- E: 0x334888 filter 2, type 1 ----------------
    unsigned instanceCases=0,instancePackets=0,skippedNodes=0,scaled=0;
    for(unsigned n=0;n<12000;++n) {
        if(n%997==0)resetPools(world);
        const auto& d=trees[N(10)<4?0:N(unsigned(trees.size()))];float cscale=N(2)?1.f:U(.5f,2.f),reach=reachOf(d,cscale);
        Quad around{U(-3000,3000),U(-3000,3000),U(-1000,1000),1};
        Quad center=around;for(unsigned k=0;k<3;++k)center[k]+=U(-.5f,.5f)*reach;
        auto col=writeCollider(C,d,center,cscale);writeQuery(C);
        // fresh 0x32C508 scratch (+0x90 = 1) shared by the two instances of this "query"
        put(SCR+0x90,1.f);put(SCR+0x94,0.f);put(SCR+0x98,0u);OriginalSphereTreeCollider scratch;scratch.scale=1.f;
        uint32_t meshAt=MESHES;meshTable.clear();
        for(unsigned which=0;which<2;++which) {
            uint32_t obj=OBJ+0x400*which,model=MODEL+0x100*which,nodes=NODES+0x100*which,desc=DESC+0x200*which,nd=ND+0x400*which,local=LOCAL+0x800*which;
            unsigned count=1+N(4);float scale=std::array{1.f,.9999999403953552f,1.f,U(.5f,2.f)}[N(4)];scaled+=scale!=1.f;
            auto r=rotation();collision_transform::Matrix inst{r[0],r[1],r[2],0,r[3],r[4],r[5],0,r[6],r[7],r[8],0,around[0]+U(-100,100),around[1]+U(-100,100),around[2]+U(-100,100),1};
            put(obj+8,0x210023u);put(obj+0xc,0u);put(obj+0x10,inst);put(obj+0x78,0x5000u+which);put(obj+0x80,model);put(obj+0x84,scale);put(obj+0x88,desc);
            put(model+4,count);put(model+8,nodes);put(desc,1u);
            WorldCollisionInstance instance;instance.resource=0x5000u+which;instance.type=1;instance.scale=scale;
            std::vector<collision_transform::Matrix> worlds;uint32_t ordinal=0;
            for(unsigned i=0;i<count;++i) {
                int32_t parent=i&&N(2)?int32_t(N(i)):-1;auto lr=rotation();
                collision_transform::Matrix l{lr[0],lr[1],lr[2],0,lr[3],lr[4],lr[5],0,lr[6],lr[7],lr[8],0,U(-60,60),U(-60,60),U(-60,60),1};
                put(nodes+16*i,parent);put(nodes+16*i+4,nd+0x40*i);put(nodes+16*i+8,0u);put(nodes+16*i+0xc,local+0x40*i);put(local+0x40*i,l);
                worlds.push_back(collision_transform::scaledNode(l,parent<0?inst:worlds[size_t(parent)],scale));
                int16_t surface=N(5)<2?int16_t(-1):int16_t(N(12));put(desc+0x10+12*i+0xa,surface);skippedNodes+=surface==-1;
                // Mesh in node-local space around the collider's local position.
                auto inverse=collision_transform::inverseRigid(worlds.back());Quad localCenter;
                {OriginalRounding rr;localCenter=roller_world_query_detail::qapply(inverse,center);for(unsigned k=0;k<3;++k)localCenter[k]=terrain_original::mul(localCenter[k],1.f/scale);}
                auto mesh=std::make_shared<CollisionTriangleMesh>(randomMesh(localCenter,reach*U(.4f,2.f)/scale,1+N(N(3)?10:25)));
                uint32_t header=meshAt;meshAt+=0x4000;writeMesh(header,header+0x100,header+0x400,header+0x800,header+0x2000,*mesh);meshTable.push_back(header);
                terrain_original::Vector lo=mesh->vertices[0],hi=lo;for(auto& v:mesh->vertices)for(unsigned k=0;k<3;++k){lo[k]=std::min(lo[k],v[k]);hi[k]=std::max(hi[k],v[k]);}
                if(N(4)==0)for(unsigned k=0;k<3;++k){lo[k]+=U(-50,50);hi[k]+=U(-50,50);}
                bool twoSided=N(2);put(nd+0x40*i,lo);put(nd+0x40*i+12,hi);put(nd+0x40*i+0x18,uint32_t(twoSided)|(rng()&~1u));
                WorldCollisionNode node;node.index=i;node.type=1;node.surface=surface;node.world=worlds.back();node.inverse=inverse;node.low=lo;node.high=hi;node.doubleSided=twoSided;node.triangles=mesh;
                instance.nodes.push_back(node);++ordinal;
            }
            // 0x334888 fetches mesh s6 of the instance resource: the table is per instance.
            std::vector<uint32_t> table(meshTable.end()-long(count),meshTable.end());auto saved=meshTable;meshTable=table;
            put(IPKT-4,0xdeadbeefu);
            auto c=call(0x334888,{Q,obj,IPKT,64,2});
            meshTable=saved;
            std::vector<OriginalRollerWorldPacket> packets;
            {OriginalRounding rr;originalRollerInstanceQuery(instance,nullptr,col,scratch,&world,packets);}
            if(GPR_U32((&c),2)!=packets.size()){
                printf("which %u orig %u port %zu scale %.9g nodes %u scratch mem %.9g port %.9g\n",which,GPR_U32((&c),2),packets.size(),scale,count,get<float>(SCR+0x90),scratch.scale);
                for(unsigned i=0;i<count;++i)printf(" node %u parent %d surface %d tris %zu ds %d\n",i,int32_t(get(nodes+16*i)),instance.nodes[i].surface,instance.nodes[i].triangles->indices.size()/3,instance.nodes[i].doubleSided);
                for(unsigned i=0;i<GPR_U32((&c),2);++i)printf(" orig node %u tri %d depth %.9g\n",get(IPKT+128*i+0x5c),int32_t(get(IPKT+128*i+0x60)),get<float>(IPKT+128*i+0x40));
                for(auto& e:packets)printf(" port node %u tri %d depth %.9g\n",e.node,e.triangle,e.depth);
                for(unsigned k=0;k<4;++k)printf(" scratch centre %.9g/%.9g",get<float>(SCR+0x80+4*k),scratch.center[k]);puts("");
                fail("E count",n);}
            for(size_t i=0;i<packets.size();++i) {
                uint32_t p=IPKT+128*uint32_t(i);const auto& e=packets[i];
                if(!same(quad(p),e.point)||!same(quad(p+16),e.normal)||bits(get<float>(p+0x40))!=bits(e.depth))fail("E contact",n);
                if(get(p+0x50)!=obj||get(p+0x5c)!=e.node||int32_t(get(p+0x4c))!=e.surface||int32_t(get(p+0x60))!=e.triangle||get(p+0x44)!=1)fail("E packet fields",n);
            }
            if(bits(get<float>(SCR+0x90))!=bits(scratch.scale))fail("E scratch scale",n);
            if(get(SCR+0x98)) {
                for(unsigned i=0;i<8;++i)if(!same(quad(SCR+16*i),scratch.corners[i]))fail("E scratch corners",n);
                if(!same(quad(SCR+0x80),scratch.center))fail("E scratch centre",n);
                if(bits(get<float>(SCR+0x94))!=bits(scratch.depth))fail("E scratch depth",n);
            }
            checkCollider(C,col,"E main collider",n);
            ++instanceCases;instancePackets+=unsigned(packets.size());
        }
    }
    printf("E: %u 0x334888 filter-2 type-1 instances (two per shared scratch; %u surface -1 nodes skipped, %u scaled) match: %u packets (world point/normal, local depth, node, surface, triangle), scratch +0x90 scale/corners/centre\n",instanceCases,skippedNodes,scaled,instancePackets);
    if(getenv("COUNTS"))for(auto [pc,count]:callCounts)fprintf(stderr,"%06x %llu\n",pc,(unsigned long long)count);
    puts("roller world query oracle: all cases match");
}
