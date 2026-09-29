#pragma once
#include <string>
// A streamed peak world (docs/peak-mountain.md, docs/peak3.md): the world location "PEAK1", "PEAK2" or "PEAK3"
// (tools/export_peak_world.py --peak N; web/free-ride.js), or the whole mountain "MOUNTAIN" (tools/export_mountain_world.py;
// "MOUNTAIN<x>" names the same world with another glide seed, web/generate-controllers.py). Everything the core does only for
// the streamed world (section residency, per-location collectible rows, stage track setup/teardown, Big Challenges) keys on this.
inline bool browser_mountain_world(const std::string& location){return location.compare(0,8,"MOUNTAIN")==0;}
inline bool browser_streamed_world(const std::string& location){
 return (location.size()==5&&location.compare(0,4,"PEAK")==0&&location[4]>='1'&&location[4]<='3')||browser_mountain_world(location);
}
// The world a location's exports belong to: "MOUNTAIN<x>" (a glide seed of the whole mountain) loads MOUNTAIN's sections and
// stage world, whose "location" is "MOUNTAIN" (the Peak 2 Race / All Peak Jam captures: section slot-1 programs, the
// collectibles' MagnetModifiers).
inline bool browser_same_world(const std::string& exported,const std::string& location){
 return exported==location||(exported=="MOUNTAIN"&&browser_mountain_world(location));
}
