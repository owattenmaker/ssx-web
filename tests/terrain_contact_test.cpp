#include "collision.hpp"
#include <fstream>
#include <iostream>
#include <iomanip>

int main(int argc,char** argv) {
    if(argc!=2) return 2;
    std::ifstream input(argv[1]);size_t patchCount,caseCount;input>>patchCount>>caseCount;
    std::vector<ssx::TerrainPatch> patches(patchCount);
    for(auto& p:patches) {
        input>>p.resource;
        for(auto& c:p.coefficients) input>>c.x>>c.y>>c.z;input>>p.authoredSurface;
    }
    ssx::CollisionWorld terrain({});terrain.setTerrainPatches(patches);
    for(size_t i=0;i<caseCount;++i) {
        ssx::Vec3 center,previous,expected,expectedPoint,lateral;uint32_t resource;float turn,scale,distance,expectedU,expectedV;int expectedSurface;
        input>>center.x>>center.y>>center.z>>previous.x>>previous.y>>previous.z
             >>expected.x>>expected.y>>expected.z>>resource>>expectedPoint.x>>expectedPoint.y>>expectedPoint.z
             >>lateral.x>>lateral.y>>lateral.z>>turn>>scale>>distance>>expectedU>>expectedV>>expectedSurface;
        auto hit=terrain.surfaceContact(center,previous);
        if(!hit.hit||hit.resource!=resource||!hit.analytic) {std::cerr<<"Missing original patch "<<i<<'\n';return 1;}
        double error=std::sqrt(ssx::dot(hit.normal-expected,hit.normal-expected));
        auto sourceHit=terrain.sourceGroundContact(center,previous,lateral,turn,scale);
        if(!sourceHit.hit||sourceHit.terrainQuery!=3||sourceHit.position.x!=expectedPoint.x||sourceHit.position.y!=expectedPoint.y||sourceHit.position.z!=expectedPoint.z) {
            std::cerr<<"Source float contact point differs "<<i<<'\n';return 8;
        }
        double sourceNormalError=std::sqrt(ssx::dot(sourceHit.normal-expected,sourceHit.normal-expected));
        if(sourceNormalError!=0||sourceHit.u!=expectedU||sourceHit.v!=expectedV||sourceHit.surface!=expectedSurface)return 11;
        std::cout<<std::setprecision(12)<<i<<" original point exact; resource "<<sourceHit.resource<<" normal error "<<sourceNormalError<<'\n';
        if(std::abs(sourceHit.contactSignedDistanceCm-distance)>1e-5) {std::cerr<<"Original signed contact distance differs "<<i<<'\n';return 10;}
        if(turn==0&&error>3e-6) {std::cerr<<"Original contact normal differs "<<error<<'\n';return 1;}
    }
    if(!input) return 3;
    size_t polynomialProbes=0;
    for(size_t i=0;i<patches.size();i+=17) {
        ssx::CollisionWorld isolated({});isolated.setTerrainPatches({patches[i]});
        for(auto uv: {std::array<double,2>{.13,.27},std::array<double,2>{.51,.49},std::array<double,2>{.87,.79}}) {
            ssx::Vec3 point,du,dv;patches[i].evaluate(uv[0],uv[1],point,du,dv);
            auto normal=ssx::unit(ssx::cross(du,dv));
            if(ssx::dot(normal,normal)<.5) continue;
            auto contact=isolated.surfaceContact(point+normal*.01,normal);
            if(!contact.hit||contact.surface!=patches[i].authoredSurface||ssx::dot(contact.position-point,contact.position-point)>1e-12) {
                std::cerr<<"Polynomial contact probe missed patch "<<patches[i].resource<<" UV "<<uv[0]<<','<<uv[1]<<'\n';return 7;
            }
            ++polynomialProbes;
        }
    }
    std::cout<<polynomialProbes<<" independent polynomial contact probes passed\n";

    // Exact synthetic curved patch has known analytic slope, independent of any
    // visual triangles. A second surface verifies selection nearest the query midpoint.
    ssx::TerrainPatch low,high;low.resource=1;high.resource=2;
    low.coefficients[1]={1,0,0};low.coefficients[4]={0,0,1};low.coefficients[2]={0,1,0};high=low;high.resource=2;high.coefficients[0]={0,1,0};
    ssx::CollisionWorld curved({});curved.setTerrainPatches({low,high});
    auto hit=curved.surfaceContact({.4,1.2,.6},{0,1,0});
    if(!hit.hit||hit.resource!=2||std::abs(hit.position.y-1.16)>1e-10) return 4;
    auto normal=ssx::unit(ssx::Vec3{-.8,1,0});
    if(ssx::dot(hit.normal-normal,hit.normal-normal)>1e-16) return 5;
    if(curved.raycast({2,10,2},{0,-1,0},20).hit) return 6;
    // A folded patch has two intersections on one vertical segment. Original
    // cached-cell preference must retain the later cell instead of restarting
    // at the first cell in the scan on every frame.
    ssx::terrain_original::Coefficients folded{};
    folded[0]={625,0,0};folded[1]={-2500,0,1000};folded[2]={2500,0,0};folded[4]={0,1000,0};
    auto grid=ssx::terrain_original::coarseGrid(folded);
    ssx::terrain_original::GroundProbe probe;probe.origin={225,420,-100};probe.direction={0,0,1200};
    auto first=ssx::terrain_original::coarseContact(grid,probe);
    ssx::terrain_original::ContactCache cache{true,99,7,3,0};
    auto cached=ssx::terrain_original::coarseContact(grid,probe,&cache);
    if(!first.hit||!cached.hit||first.cellU!=1||cached.cellU!=7||cached.fraction<=first.fraction)return 9;
    std::cout<<"Original folded-patch cached-cell preference passed\n";

}
