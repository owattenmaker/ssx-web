#include "../engine/original_spatial.hpp"
#include <fstream>
#include <iostream>
#include <vector>
int main(int argc,char**argv){if(argc!=2)return 2;std::ifstream in(argv[1]);unsigned count;in>>count;
    struct Item{unsigned resource,order;ssx::OriginalSpatialCell cell;};std::vector<Item> items;
    for(unsigned i=0;i<count;++i){Item item;ssx::terrain_original::Vector low,high;ssx::OriginalSpatialCell expected;in>>item.resource>>item.order;for(auto*p:{&low,&high})for(auto&x:*p)in>>x;in>>expected.level;for(auto&x:expected.coordinate)in>>x;
        item.cell=ssx::originalSpatialCell(low,high);if(item.cell.level!=expected.level||item.cell.coordinate!=expected.coordinate){std::cerr<<"Live spatial assignment differs for "<<item.resource<<'\n';return 1;}items.push_back(item);}
    auto sorted=items;std::sort(sorted.begin(),sorted.end(),[](const Item&a,const Item&b){if(ssx::originalSpatialBefore(a.cell,b.cell))return true;if(ssx::originalSpatialBefore(b.cell,a.cell))return false;return a.order>b.order;});
    for(unsigned i=0;i<count;++i)if(items[i].resource!=sorted[i].resource){std::cerr<<"Live terrain traversal order differs at "<<i<<" original "<<items[i].resource<<" native "<<sorted[i].resource<<'\n';return 3;}
    std::cout<<count<<" authored terrain cell assignments and complete original traversal order match\n";
}
