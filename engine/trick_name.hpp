#pragma once
#include "trick_history.hpp"
#include <optional>
#include <string>
#include <vector>
#include <stdexcept>
namespace ssx {
using OriginalTrickNameTables=std::array<std::vector<std::optional<std::string>>,17>;
//116950: concatenation order and bit fields of the original packed identity.
inline std::string originalTrickName(const OriginalTrickIdentity&id,const OriginalTrickNameTables&tables){
 constexpr std::array<unsigned,17> word={0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1};
 constexpr std::array<unsigned,17> shift={0,3,7,10,12,16,19,22,28,3,10,11,0,18,20,24,27};
 constexpr std::array<unsigned,17> mask={7,15,7,3,15,7,7,63,15,127,1,127,7,3,15,7,31};
 std::string result;
 for(unsigned i=0;i<17;i++){unsigned index=(id[word[i]]>>shift[i])&mask[i];if(index>=tables[i].size()||!tables[i][index])throw std::runtime_error("Unsupported original trick-name index");result+=*tables[i][index];}
 return result;
}
}
