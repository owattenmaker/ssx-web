#pragma once
// The course core's C exports the native host calls (their definitions: web/*.cpp, web/*.inc EMSCRIPTEN_KEEPALIVE).
#include <cstdint>
#include <functional>

void native_run_in_context(void* block, const std::function<void()>& fn);

extern "C" {
// rider contexts (native/core/rider_context_native.cpp)
void* rider_context_create();
void* rider_context_current();
// the course
void init_world(float* data, int count);
int init_sections(const char* text);
int init_stage_world(const char* particlesText, const char* liveCompText, const char* stageText);
int init_avalanches(const char* text, int append);
void avalanche_clear();
int init_stage_flags(const char* flagsText, const char* readyText);
void init_terrain(const char* text);
void init_world_collision(const char* text, const char* expectedHash);
void init_body_terrain(const char* text);
void init_fog(const char* text);
int init_weather(const char* text);
void init_rider_lighting(const char* catalog, const char* tree);
void init_rails(const char* text, const char* worldHash);
uint32_t world_load_hash();
uint32_t body_load_hash();
uint32_t rail_load_hash();
// a rider
void set_fx_deferred(int on);
void init_animation(const char* metadata, const char* rig, const char* settings, const uint8_t* packets, uint32_t size);
void init_race(const char* text);
void animation_use_physics(int on);
float* animation_tick(float, float, float, float, int, int, int, int, int, float, float, float);
void reset_rider(float x, float y, float z, float angle);
void set_rider_velocity(float x, float y, float z);
void set_rider_attributes(const int32_t* raw, int32_t override);
void npc_configure(const char* rider, const char* document);
void npc_set_relationships(const float* row);
void human_grid_seed(const char* stateText);
void event_kind(int kind);
// the per-frame presentation flags (main.js applyPresentationFast)
void set_presentation_fast(int on);
void set_rider_fx_render_scale(int on);
void set_fx_reset_kicker(int on);
void stage_teleport_enable(int on);
// the rider manager (web/rider_manager.cpp)
void rider_host(int flags);
void rider_parse_cache_clear();
void manager_configure(void* const* blocks, const int* slots, int riders, int count, int flags);
void manager_set_anchor(const uint32_t* words);
void manager_event_start(const char* const* riders, const char* context, const char* world, int gridStart, int hold, int isolate, const int32_t* ranks);
float* manager_begin_tick(int anchorTick, int scores);
void manager_end_tick();
int manager_relationships(const int32_t* spec);
void manager_relationships_off();
void race_world_set_relationships(const float* m);
// shared world nodes (web/shared_world.inc)
void shared_world_nodes(int on);
uint32_t* world_instance_states();
uint32_t* world_node_states(const uint32_t* resources, int count);
int world_node_states_apply(const uint32_t* list);
// the run start (main.js startRun)
void race_time_limit(int32_t ticks);
void set_race_bonus(const int32_t* words, int mode, int handler, int freestyleKind, uint32_t flags);
void reset_fog();
void reset_rider_lighting();
void reset_animation();
void reset_race();
void reset_pad_history();
void set_input_map(int variant);
int set_camera_view(int type);
void set_score_career_cash(int32_t cash);
void set_stage_collect_state(uint32_t path, uint32_t lo, uint32_t hi);
void set_stage_collect_row(int track, uint32_t lo, uint32_t hi);
void start_event();
int replay_camera_triggers(const char* text);
void replay_camera_begin(int mode, int fromLive);
// the tick (web/game_tick.cpp)
float* tick_begin(const float* pad, float lastRescues, float lastResetPlacement, int animationReady, int boneCount, int teleportKnown, float teleportCount,
                  int tweakWord);
void tick_mid();
float* tick_end(int hasAnimPose, float pose7, float hx, float hy, float hz, int replayCamera, float mx, float my, float mz, int animationReady, int finished);
void tick_finish(int freestyle, int aiActive, float aiPlace, int backcountry);
void tick_results(float* io);
int race_timed_out();
void race_give_up();
uint32_t game_tick();
// read-outs
float* rider_state();
float* pose_physical();
float* world_pose_bones();
float* rider_world_state();
uint32_t* visual_rng_words();
uint32_t* animation_rng_words();
uint32_t* visual_lcg_word();
uint32_t* score_object_dump();
float* animation_info();
float* boost_info();
// the presentation read-outs the native front end draws from
uint32_t* section_chunks();
float* set_piece_info();
uint32_t* set_piece_sections();
float* fog_palette_info();
uint8_t* fog_palette_rgba();
float* fog_info();
float* weather_info();
float* weather_lightning();
uint32_t environment_world_loads();
int pickup_visible(uint32_t resource);
float* rider_skin_palette();
int rider_skin_palette_count();
uint32_t* rider_skin_palette_indices();
// rider lighting (web/rider-material.js createRiderLightingUpdate)
void init_environment(const char* metadata, const uint8_t* data, int length);
float* rider_query_bounds();
uint32_t* refresh_rider_lighting(const float* bounds, const float* rankPoint);
float* shade_rider_lighting(const float* environment, const float* view, const float* point, float rimScale, const float* constants, const float* extra, int count);
float* environment_irradiance();
float* camera_render_view();
float* rider_lighting_gpu_coefficients();
float* environment_lighting_info();
float* reset_info();
// the rider effects (web/board-trail.js, wake-renderer.js, snow-renderer.js), built when read
float* trail_info();
float* trail_ribbon();
float* trail_roof();
float* wake_info();
float* wake_vertices();
float* snow_info();
float* snow_particles(unsigned emitter);
float* snow_flipbook_info();
float* boost_fx_info();
float* boost_fx_vertices(unsigned strip);
float* rider_fx_info();
float* rider_fx_vertices(unsigned strip);
// the race HUD (web/game-tick.js readTrickHudSlots, main.js ui.draw state)
float* score_hud_slots();
const char* score_hud_text(int slot);
float* boost_hud_info();
float* race_result_info();
float* rider_stance_info();
float* race_progress_info();
}
