// The course core's calls into its host (EM_JS in the wasm build: the page's Module.riderHost / pairHost / finishHost /
// riderManagerHost) for a native host (docs/core-migration.md "Native core"). With the core's rider manager (rider_host bit 8) and
// its relationships, the stage and pair hooks never reach the host; the finish place and the manager's reaction / QA hooks are
// answered here with the page's defaults (no freestyle place, no presentation, no QA hooks).
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {
[[noreturn]] void notNative(const char* name) {
    throw std::runtime_error(std::string("host hook not used by a native host: ") + name);
}
}  // namespace

extern "C" {
// the page's rider manager hooks (rider_host bits 1 / 2 without bit 8)
void js_rider_after_pose() {
    notNative("js_rider_after_pose");
}
void js_rider_pairs() {
    notNative("js_rider_pairs");
}
// race_end calls it in every context; without the page's manager nothing runs
void js_rider_before_progress() {
}
// web/main.js finishHost: a freestyle run's place from the career standings (-1: none)
int js_finish_place(int) {
    return -1;
}
// the page's pair host (the online pair-net, or ai-racers.js without the core's manager)
void js_pair_view(int, uint32_t*) {
    notNative("js_pair_view");
}
void js_pair_translate(int, float, float, float) {
    notNative("js_pair_translate");
}
void js_pair_velocity(int, float, float, float, int) {
    notNative("js_pair_velocity");
}
void js_pair_react(int, int, int, int, const float*, int) {
    notNative("js_pair_react");
}
uint32_t js_pair_random() {
    notNative("js_pair_random");
}
void js_pair_attack(int, int, float, float, float, float) {
}
// the core's rider manager's page hooks: the reactions' speech / relationships (when the core does not hold them), the QA hooks
void js_manager_react(int, int, int, int) {
}
void js_manager_draws(int, const uint32_t*, int, int) {
}
int js_manager_after_rider(int) {
    return 0;
}
}
