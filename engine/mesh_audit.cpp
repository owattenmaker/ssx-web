#include "collision.hpp"
#include <cstdio>
#include <fstream>
#include <iterator>

template<class T> std::vector<T> load(const std::string& path) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if (!file) throw std::runtime_error("Cannot open "+path);
    auto size=file.tellg();
    if (size<=0 || size%sizeof(T)) throw std::runtime_error("Invalid buffer "+path);
    file.seekg(0);
    std::vector<T> result(size/sizeof(T));
    if (!file.read(reinterpret_cast<char*>(result.data()),size)) throw std::runtime_error("Truncated buffer "+path);
    return result;
}
int main(int argc,char** argv) {
    try {
        if (argc!=2) throw std::runtime_error("Usage: ssx3_mesh_audit native-package-directory");
        std::string root=argv[1];
        auto vertices=load<float>(root+"/vertices.bin");
        auto indices=load<uint32_t>(root+"/indices.bin");
        if (vertices.size()%10 || indices.size()%3) throw std::runtime_error("Invalid mesh stride");
        auto point=[&](uint32_t i){
            if (i>=vertices.size()/10) throw std::runtime_error("Index outside mesh");
            return ssx::Vec3{vertices[i*10],vertices[i*10+1],vertices[i*10+2]};
        };
        std::vector<ssx::Triangle> triangles;
        for (size_t i=0;i<indices.size();i+=3) triangles.push_back({point(indices[i]),point(indices[i+1]),point(indices[i+2]),uint32_t(i/3)});
        ssx::CollisionWorld world(triangles);
        unsigned tested=0,missed=0;
        for (size_t i=0;i<triangles.size();i+=97) {
            const auto& t=triangles[i];
            auto e1=t.b-t.a,e2=t.c-t.a;
            auto n=ssx::cross(e1,e2);
            double magnitude=1;
            for (auto p:{t.a,t.b,t.c}) for (unsigned axis=0;axis<3;++axis) magnitude=std::max(magnitude,std::abs(p[axis]));
            double precision=magnitude*std::numeric_limits<float>::epsilon()*4;
            if (ssx::dot(n,n)<=precision*precision*(ssx::dot(e1,e1)+ssx::dot(e2,e2))) continue;
            n=ssx::unit(n);
            auto center=(t.a+t.b+t.c)*(1.0/3);
            auto hit=world.raycast(center+n*.1f,n*-1,.2f);
            ++tested;
            if (!hit.hit) ++missed;
        }
        std::printf("Native package audit: %zu vertices, %zu triangles, %u surface probes, %u misses\n",
                    vertices.size()/10,triangles.size(),tested,missed);
        if (!tested || missed) return 1;
    } catch(const std::exception& error) {std::fprintf(stderr,"%s\n",error.what());return 1;}
}
