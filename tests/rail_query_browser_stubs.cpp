// Native rail-query oracle (tools/test_browser_rails.sh -> local/browser-validation/rail-queries.bin, web/test-rails.mjs):
// the symbols web/rail_bridge.cpp takes from the rest of the core, as a rails-only load has them (no streamed world, no
// shared-world log, no set-piece entities, so no dynamic rails).
#include "../web/rider_local.hpp"
#include "../engine/roller_modifier.hpp"
#include <cstdint>
#include <initializer_list>
RIDER_LOCAL bool browserWorldAppend=false;
void shared_world_log(std::initializer_list<uint32_t>){}
bool browser_dynamic_rail_entity(uint32_t,int32_t,ssx::RollerMatrix*,ssx::RollerQuad*,ssx::RollerQuad*){return false;}
bool browser_dynamic_rail_contact_velocity(uint32_t,int32_t,const ssx::RollerQuad&,ssx::RollerQuad&,ssx::RollerQuad&){return false;}
