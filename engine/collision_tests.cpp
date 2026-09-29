#include "collision.hpp"
#include <cassert>
#include <cstdio>
using namespace ssx;
int main() {
    std::vector<Triangle> geometry;
    for (int x=0;x<100;++x) for (int z=0;z<100;++z) {
        float a=float(x),b=float(z);
        geometry.push_back({{a,0,b},{a+1,0,b},{a,0,b+1},7});
        geometry.push_back({{a+1,0,b},{a+1,0,b+1},{a,0,b+1},7});
    }
    // Nearest surface wins, independent of primitive insertion order.
    geometry.push_back({{10,2,10},{20,2,10},{10,2,20},9});
    geometry.push_back({{0,0,0},{0,0,0},{0,0,0},10});
    CollisionWorld world(std::move(geometry));
    auto hit=world.raycast({11,10,11},{0,-3,0},20);
    assert(hit.hit && std::abs(hit.distance-8)<1e-5 && hit.source==9);
    assert(hit.normal.y>.999 && hit.position.y==2);
    assert(!world.raycast({11,10,11},{0,-1,0},7).hit);
    assert(!world.raycast({-1,10,11},{0,-1,0},20).hit);
    assert(!world.raycast({11,10,11},{0,0,0},20).hit);
    assert(!world.raycast({11,10,11},{1,0,0},20).hit);
    auto fromBelow=world.raycast({50,-3,50},{0,1,0},10);
    assert(fromBelow.hit && fromBelow.normal.y<-.999 && fromBelow.distance==3);
    CollisionWorld empty({});assert(!empty.raycast({},{0,1,0},1).hit);
    bool rejected=false;
    try { CollisionWorld bad({{{NAN,0,0},{1,0,0},{0,0,1},0}}); } catch (...) { rejected=true; }
    assert(rejected);
    std::puts("Native collision: 20,002 triangles, nearest surface, parallel/missing/short/zero rays, two-sided hits, malformed geometry passed");
}
