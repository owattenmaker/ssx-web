#include "world_collision_asset.h"
#include <functional>
#include <map>
namespace {
using Matrix=ssx::collision_transform::Matrix;using Vector=ssx::terrain_original::Vector;
float number(id value) {
    if(![value isKindOfClass:NSNumber.class]||!std::isfinite([value doubleValue]))throw std::runtime_error("Invalid collision number");
    return [value floatValue];
}
Vector vector(NSArray* values,unsigned offset=0) {
    if(![values isKindOfClass:NSArray.class]||values.count<offset+3)throw std::runtime_error("Invalid collision vector");
    return {number(values[offset]),number(values[offset+1]),number(values[offset+2])};
}
Matrix matrix(NSArray* values) {
    if(values.count!=16)throw std::runtime_error("Invalid collision matrix");Matrix result;
    for(unsigned i=0;i<16;++i)result[i]=number(values[i]);return result;
}
NSString* key(uint32_t resource){return [NSString stringWithFormat:@"%u:%u",resource&255,resource>>8];}
}
std::unique_ptr<ssx::WorldBodyCollision> loadWorldBodyCollision(NSString* path,NSString* expectedSourceHash,NSString* terrainPath) {
    NSData* bytes=[NSData dataWithContentsOfFile:path];NSError* error=nil;
    NSDictionary* package=bytes?[NSJSONSerialization JSONObjectWithData:bytes options:0 error:&error]:nil;
    if(!package||[package[@"version"] intValue]!=1||![package[@"source_sha256"] isEqual:expectedSourceHash])
        throw std::runtime_error("Missing or mismatched authored world collision package");
    auto result=std::make_unique<ssx::WorldBodyCollision>();
    std::map<std::pair<uint32_t,unsigned>,std::shared_ptr<const ssx::CollisionTriangleMesh>> meshes;
    std::map<std::pair<uint32_t,unsigned>,std::shared_ptr<const ssx::CollisionSphereTree>> trees;
    for(NSDictionary* source in package[@"instances"]) {
        ssx::WorldCollisionInstance instance;
        unsigned track=[source[@"track"] unsignedIntValue],rid=[source[@"rid"] unsignedIntValue];instance.resource=(rid<<8)|track;
        NSDictionary* table=package[@"bindings"][[NSString stringWithFormat:@"%u",track]];
        NSArray* descriptors=table[@"descriptors"];unsigned descriptorIndex=[source[@"collision_descriptor"] unsignedIntValue];
        if(descriptorIndex>=descriptors.count)throw std::runtime_error("Collision descriptor index outside table");
        NSDictionary* descriptor=descriptors[descriptorIndex];instance.type=[descriptor[@"type"] unsignedIntValue];instance.flags=[descriptor[@"flags"] unsignedIntValue];
        instance.scale=number(source[@"scale"]);
        if(source[@"ray_policy"]){if(![source[@"ray_policy"] isEqual:@"sphere-tree-no-override"]||instance.type!=3)throw std::runtime_error("Invalid authored ray capability");instance.rayAlwaysEmpty=true;}
        instance.low=vector(source[@"bounds_min_cm"]);instance.high=vector(source[@"bounds_max_cm"]);
        for(unsigned k=0;k<3;++k)if(instance.high[k]<instance.low[k])throw std::runtime_error("Reversed authored collision bounds");
        if(instance.type==0){result->instances.push_back(std::move(instance));continue;}
        if(instance.scale==0)instance.unsupported="Zero-scale original collision requires saturated division behavior";
        else if(instance.type!=1&&instance.type!=2&&instance.type!=3)instance.unsupported="Unknown authored collision descriptor type";
        else if(!(instance.flags&0x200000)||instance.flags&0x40000000)instance.unsupported="Original dynamic entity collision callbacks are not instantiated";
        uint32_t model=[source[@"model_resource"] unsignedIntValue];NSDictionary* modelSource=package[@"render_model_nodes"][key(model)];NSArray* nodes=modelSource[@"nodes"];
        if(!nodes.count){instance.unsupported="Missing original model hierarchy";result->instances.push_back(std::move(instance));continue;}
        Matrix instanceMatrix=matrix(source[@"matrix"]);std::vector<Matrix> worlds(nodes.count);std::vector<unsigned char> state(nodes.count);
        std::function<Matrix(unsigned)> compose=[&](unsigned i)->Matrix {
            if(i>=nodes.count||state[i]==1)throw std::runtime_error("Invalid collision node hierarchy");
            if(state[i]==2)return worlds[i];state[i]=1;
            NSDictionary* node=nodes[i];uint32_t parent=[node[@"parent"] unsignedIntValue];
            auto parentMatrix=parent==0xffffffff?instanceMatrix:compose(parent);
            worlds[i]=ssx::collision_transform::scaledNode(matrix(node[@"matrix"]),parentMatrix,instance.scale);state[i]=2;return worlds[i];
        };
        unsigned meshOrdinal=0;
        for(unsigned i=0;i<nodes.count;++i) {
            NSDictionary* node=nodes[i];auto world=compose(i);
            if(!node[@"draw_bounds_cm"])continue;
            ssx::WorldCollisionNode target;target.index=i;target.type=instance.type;target.world=world;target.inverse=ssx::collision_transform::inverseRigid(world);
            target.low=vector(node[@"draw_bounds_cm"]);target.high=vector(node[@"draw_bounds_cm"],3);
            if(!node[@"draw_flags"])throw std::runtime_error("Missing original draw/collision flags; re-run tools/world_collision.py");
            target.doubleSided=([node[@"draw_flags"] unsignedIntValue]&1)!=0;
            NSArray* nodeDescriptors=descriptor[@"nodes"];
            if(i>=nodeDescriptors.count)throw std::runtime_error("Collision descriptor/model node count mismatch");
            target.surface=[nodeDescriptors[i][@"surface_id"] intValue];target.collisionFlags=[nodeDescriptors[i][@"flags"] unsignedShortValue];target.collisionValue=[nodeDescriptors[i][@"value"] unsignedIntValue];target.collisionAuxiliary=[nodeDescriptors[i][@"auxiliary"] unsignedIntValue];
            if(instance.type==3) {
                uint32_t resource=[descriptor[@"collision_resource"] unsignedIntValue];auto treeKey=std::make_pair(resource,meshOrdinal);
                target.sphereTreeResource=resource;
                auto found=trees.find(treeKey);
                if(found!=trees.end())target.sphereTree=found->second;
                else {
                    NSDictionary* collisionSource=package[@"collision_meshes"][key(resource)];NSArray* sourceModels=collisionSource[@"models"];
                    if([collisionSource[@"format"] unsignedIntValue]!=3||meshOrdinal>=sourceModels.count)instance.unsupported="Missing original sphere-tree collision resource/model";
                    else {
                        NSDictionary* geometry=sourceModels[meshOrdinal];auto data=std::make_shared<ssx::CollisionSphereTree>();
                        data->compressed=[geometry[@"compressed"] boolValue];
                        data->centerCm=vector(geometry[@"center_cm"]);size_t total=0,stride=1;
                        NSArray* levels=geometry[@"levels"];
                        if(!levels.count||levels.count>8)throw std::runtime_error("Invalid original sphere-tree depth");
                        for(NSDictionary* level in levels) {
                            float radius=number(level[@"radius_cm"]),offset=number(level[@"child_offset_cm"]);
                            if(radius<0||[level[@"stride"] unsignedLongLongValue]!=stride)throw std::runtime_error("Invalid original sphere-tree level");
                            data->levels.push_back({radius,offset,uint32_t(stride)});total+=stride;stride*=8;
                        }
                        NSArray* masks=geometry[@"masks"];
                        if(masks.count!=total)throw std::runtime_error("Original sphere-tree mask count mismatch");
                        for(NSNumber* mask in masks){if(mask.unsignedIntValue>255)throw std::runtime_error("Invalid sphere-tree mask");data->masks.push_back(mask.unsignedCharValue);}
                        target.sphereTree=data;trees.emplace(treeKey,data);
                    }
                }
                ++meshOrdinal;
            } else if(instance.type==1) {
                uint32_t resource=[descriptor[@"collision_resource"] unsignedIntValue];auto meshKey=std::make_pair(resource,meshOrdinal);
                auto found=meshes.find(meshKey);
                if(found!=meshes.end())target.triangles=found->second;
                else {
                    NSDictionary* collisionSource=package[@"collision_meshes"][key(resource)];NSArray* sourceModels=collisionSource[@"models"];
                    if(meshOrdinal>=sourceModels.count){instance.unsupported="Missing original triangle collision resource/model";}
                    else {
                        NSDictionary* geometry=sourceModels[meshOrdinal];auto data=std::make_shared<ssx::CollisionTriangleMesh>();
                        for(NSArray* point in geometry[@"vertices_cm"])data->vertices.push_back(vector(point));
                        for(NSArray* normal in geometry[@"normals"])data->normals.push_back(vector(normal));
                        for(NSNumber* index in geometry[@"indices"]){unsigned at=index.unsignedIntValue;if(at>=data->vertices.size())throw std::runtime_error("Original collision triangle index outside geometry");data->indices.push_back(at);}
                        if(data->indices.size()%3||data->normals.size()!=data->indices.size()/3)throw std::runtime_error("Collision triangle/normal count mismatch");
                        target.triangles=data;meshes.emplace(meshKey,data);
                    }
                }
                ++meshOrdinal;
            }
            instance.nodes.push_back(std::move(target));
        }
        result->instances.push_back(std::move(instance));
    }
    if(terrainPath) {
        NSData* terrainBytes=[NSData dataWithContentsOfFile:terrainPath];
        NSDictionary* data=terrainBytes?[NSJSONSerialization JSONObjectWithData:terrainBytes options:0 error:nil]:nil;
        if(![data[@"source_sha256"] isEqual:expectedSourceHash])throw std::runtime_error("Terrain/body collision source mismatch");
        for(NSDictionary* source in data[@"patches"]) {
            ssx::WorldCollisionTerrain patch;patch.resource=[source[@"resource_id"] unsignedIntValue];patch.flags=[source[@"authored_flags"] unsignedIntValue];patch.surface=[source[@"authored_surface_id"] intValue];
            NSArray* low=source[@"authored_bounds_min"],*high=source[@"authored_bounds_max"];
            vector(low);vector(high); // Validate before preserving the source-float round trip.
            patch.low={float([low[0] doubleValue]*100),float(-[high[2] doubleValue]*100),float([low[1] doubleValue]*100)};
            patch.high={float([high[0] doubleValue]*100),float(-[low[2] doubleValue]*100),float([high[1] doubleValue]*100)};
            NSArray* values=source[@"coefficients"];if(values.count!=16)throw std::runtime_error("Invalid body terrain coefficients");
            ssx::terrain_original::Coefficients coefficients;
            for(unsigned i=0;i<16;++i) {
                NSArray* value=values[i];if(value.count!=3)throw std::runtime_error("Invalid terrain coefficient dimensions");
                coefficients[i]={float([value[0] doubleValue]*100),float(-[value[2] doubleValue]*100),float([value[1] doubleValue]*100)};
            }
            patch.grid=ssx::terrain_original::coarseGrid(coefficients);result->terrain.push_back(std::move(patch));
        }
        result->prepareTerrainTraversal();
    }
    return result;
}
