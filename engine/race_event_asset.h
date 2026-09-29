#pragma once
#import <Foundation/Foundation.h>
#include "race_session.hpp"
#include <string>
namespace ssx {
OriginalRaceEventAsset readOriginalRaceEventAsset(NSDictionary* source);
OriginalRaceEventAsset loadOriginalRaceEventAsset(NSString* path);
}
