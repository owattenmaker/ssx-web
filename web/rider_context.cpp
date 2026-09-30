// Rider contexts: one core runs all six riders, as the original's rider manager does (docs/ai-racers.md "One core, six
// riders"). Every mutable global of the core is RIDER_LOCAL (thread-local, web/rider_local.hpp); a rider context is a
// TLS block. The human's is the static block; a computer rider's is a heap copy of the pristine block with this
// module's static-initialisation work run in it, so a new context starts exactly like a freshly instantiated core.
// Only the course geometry (web/core.cpp world, cameraTerrain) and constant tables are shared.
#include "rider_local.hpp"
#include <emscripten/emscripten.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
extern "C" uint32_t rider_tls_size_value(); // web/rider_tls.S
extern "C" void rider_tls_set(void*);
// Static-initialisation work per translation unit (link order, web/build-core.sh).
void rider_statics_core();void rider_statics_rails();void rider_statics_world();void rider_statics_animation();void rider_statics_environment();
namespace {
uint8_t* tlsTemplate=nullptr;uint32_t tlsSize=0;
constexpr uint32_t kAlign=64;
}
// Before any other constructor touches thread-local state: keep the pristine static block (the .tdata image).
__attribute__((constructor(101))) static void rider_tls_capture(){
 tlsSize=rider_tls_size_value();if(!tlsSize)abort(); // web/patch-rider-tls.mjs did not run
 if(reinterpret_cast<uintptr_t>(__builtin_wasm_tls_base())%16)abort();
 tlsTemplate=static_cast<uint8_t*>(malloc(tlsSize));std::memcpy(tlsTemplate,__builtin_wasm_tls_base(),tlsSize);
}
extern "C" {
// A new rider context (a fresh core's rider state); returns its TLS block. The current context is unchanged.
EMSCRIPTEN_KEEPALIVE void* rider_context_create(){
 void* block=aligned_alloc(kAlign,(tlsSize+kAlign-1)/kAlign*kAlign);if(!block)throw std::runtime_error("Rider context allocation");
 std::memcpy(block,tlsTemplate,tlsSize);
 void* previous=__builtin_wasm_tls_base();rider_tls_set(block);
 try{rider_statics_core();rider_statics_rails();rider_statics_world();rider_statics_animation();rider_statics_environment();}
 catch(...){rider_tls_set(previous);throw;}
 rider_tls_set(previous);return block;
}
EMSCRIPTEN_KEEPALIVE void* rider_context_current(){return __builtin_wasm_tls_base();}
EMSCRIPTEN_KEEPALIVE void rider_context_enter(void* block){rider_tls_set(block);}
EMSCRIPTEN_KEEPALIVE uint32_t rider_context_bytes(){return tlsSize;}
}

// ---- The rider-context snapshot (web/world_snapshot.hpp; docs/replay.md §2a; pv eventReturnInWorld (b); built with SSX_SNAPSHOT_REGISTRY) ----
#ifdef SSX_SNAPSHOT_REGISTRY
#include "world_snapshot.hpp"
#include <malloc.h>
namespace ssx_snapshot {
std::vector<Entry>& registry(){static std::vector<Entry> entries;return entries;}
std::vector<Hook>& hooks(){static std::vector<Hook> list;return list;}
bool qa=false; // snapshot_qa: the kept variables' hashes at every save / restore (?qa, the gates)
}
namespace {
// One per context (its own TLS block): two slots (0 the countdown, 1 the results time), each the block's bytes and the holders.
struct SnapshotContext{std::array<std::vector<uint8_t>,2> raw;std::array<bool,2> saved{};std::vector<std::unique_ptr<ssx_snapshot::HolderBase>> holders;std::array<std::vector<uint32_t>,2> firstHeap;std::array<std::vector<uint64_t>,2> keepHash;std::vector<uint32_t> keepChanged;bool hookFailed=false;const char* failedHook=nullptr;};
// A kept variable's hash (web/snapshot-policy.mjs: checked at every restore against its save).
uint64_t keep_hash(const ssx_snapshot::Entry& e){return e.hash(static_cast<const uint8_t*>(__builtin_wasm_tls_base())+e.offset,1469598103934665603ull);}
RIDER_LOCAL SnapshotContext* snapshotContext=nullptr;
RIDER_LOCAL_LAZY std::vector<uint32_t> snapshotReport; // snapshot_hashes' output
}
extern "C" {
// At the in-world attach, for each context: the buffers and holders, made once (a later save reuses them).
EMSCRIPTEN_KEEPALIVE uint32_t snapshot_init(){
 auto& reg=ssx_snapshot::registry();if(!snapshotContext){snapshotContext=new SnapshotContext;for(auto& r:snapshotContext->raw)r.assign(tlsSize,0);snapshotContext->holders.resize(reg.size());for(size_t k=0;k<reg.size();++k)if(!reg[k].trivial&&!reg[k].keep&&reg[k].make)snapshotContext->holders[k]=reg[k].make();}
 uint32_t missing=0;for(const auto& e:reg)if(!e.trivial&&!e.keep&&!e.make)++missing;return missing; // variables that can be neither copied nor kept (must be 0)
}
// The unrestorable variables' names (QA), one per call index; nullptr past the end.
EMSCRIPTEN_KEEPALIVE const char* snapshot_missing(uint32_t k){uint32_t n=0;for(const auto& e:ssx_snapshot::registry())if(!e.trivial&&!e.keep&&!e.make){if(n==k)return e.name;++n;}return nullptr;}
EMSCRIPTEN_KEEPALIVE int snapshot_save(uint32_t slot){
 if(!snapshotContext||slot>1)return 0;auto* tls=static_cast<const uint8_t*>(__builtin_wasm_tls_base());std::memcpy(snapshotContext->raw[slot].data(),tls,tlsSize);
 const auto& reg=ssx_snapshot::registry();auto& fh=snapshotContext->firstHeap[slot];const bool first=fh.empty();if(first)fh.assign(reg.size()+1,0);
 for(size_t k=0;k<reg.size();++k)if(auto& h=snapshotContext->holders[k]){const auto before=first?mallinfo().uordblks:0;h->save(tls+reg[k].offset,slot);if(first)fh[k]=uint32_t(mallinfo().uordblks-before);}
 if(first){const auto before=mallinfo().uordblks;for(const auto& hk:ssx_snapshot::hooks())hk.save(slot);fh[reg.size()]=uint32_t(mallinfo().uordblks-before);}
 auto& kh=snapshotContext->keepHash[slot];kh.assign(reg.size(),0);if(ssx_snapshot::qa)for(size_t k=0;k<reg.size();++k)if(reg[k].keep&&!reg[k].own)kh[k]=keep_hash(reg[k]);
 for(const auto& h:ssx_snapshot::hooks())h.save(slot);
 snapshotContext->saved[slot]=true;return 1;
}
// Whether slot can be restored in this context, changing nothing: 0 yes; 1 no save; 2 a kept table changed since the save (QA:
// snapshot_keep_changed_name); 3 a hook's world no longer matches its save (snapshot_failed_hook).
EMSCRIPTEN_KEEPALIVE int snapshot_check(uint32_t slot){
 if(!snapshotContext||slot>1||!snapshotContext->saved[slot])return 1;
 const auto& reg=ssx_snapshot::registry();auto& changed=snapshotContext->keepChanged;changed.clear();const auto& kh=snapshotContext->keepHash[slot];
 if(ssx_snapshot::qa)for(size_t k=0;k<reg.size();++k){const auto& e=reg[k];if(e.keep&&!e.own&&k<kh.size()&&keep_hash(e)!=kh[k])changed.push_back(uint32_t(k));}
 if(!changed.empty())return 2;
 snapshotContext->failedHook=nullptr;for(const auto& h:ssx_snapshot::hooks())if(h.check&&!h.check(slot)){snapshotContext->failedHook=h.name;return 3;}
 return 0;
}
// Restores slot (after snapshot_check: returns its code, nothing changed, when it is not 0); 1 when restored.
EMSCRIPTEN_KEEPALIVE int snapshot_restore(uint32_t slot){
 if(const int c=snapshot_check(slot))return c==1?0:c==2?4:2;
 auto* tls=static_cast<uint8_t*>(__builtin_wasm_tls_base());const auto& raw=snapshotContext->raw[slot];const auto& reg=ssx_snapshot::registry();
 for(size_t k=0;k<reg.size();++k){const auto& e=reg[k];if(e.keep)continue;if(e.trivial)std::memcpy(tls+e.offset,raw.data()+e.offset,e.size);else if(auto& h=snapshotContext->holders[k])h->restore(tls+e.offset,slot);}
 snapshotContext->hookFailed=false;for(const auto& h:ssx_snapshot::hooks())if(!h.restore(slot)){snapshotContext->hookFailed=true;snapshotContext->failedHook=h.name;}
 return snapshotContext->hookFailed?2:1;
}
// Bytes held by this context's snapshot (both slots): the block copies and the holders' copies.
EMSCRIPTEN_KEEPALIVE uint32_t snapshot_bytes(){if(!snapshotContext)return 0;size_t n=0;for(const auto& r:snapshotContext->raw)n+=r.capacity();for(const auto& h:snapshotContext->holders)if(h)n+=h->bytes();for(const auto& h:ssx_snapshot::hooks())n+=h.bytes();return uint32_t(n);}
// The last restore's kept variables whose value differed from their save (must be none: a kept table a race changed).
EMSCRIPTEN_KEEPALIVE uint32_t snapshot_keep_changed_count(){return snapshotContext?uint32_t(snapshotContext->keepChanged.size()):0;}
EMSCRIPTEN_KEEPALIVE const char* snapshot_keep_changed_name(uint32_t k){if(!snapshotContext||k>=snapshotContext->keepChanged.size())return nullptr;return ssx_snapshot::registry()[snapshotContext->keepChanged[k]].name;}
EMSCRIPTEN_KEEPALIVE void snapshot_qa(int on){ssx_snapshot::qa=on!=0;}
// The hook whose last restore failed (its state no longer matches the save's world: see its note), or nullptr.
EMSCRIPTEN_KEEPALIVE const char* snapshot_failed_hook(){return snapshotContext?snapshotContext->failedHook:nullptr;}
// QA (memory attribution): drops slot's copies in this context (the next save of it allocates them again).
EMSCRIPTEN_KEEPALIVE void snapshot_clear(uint32_t slot){if(!snapshotContext||slot>1)return;for(auto& h:snapshotContext->holders)if(h)h->clear(slot);snapshotContext->saved[slot]=false;}
EMSCRIPTEN_KEEPALIVE void snapshot_release(){delete snapshotContext;snapshotContext=nullptr;}
// QA (the per-subsystem self-check): [n, then per source file: file index, hash lo, hash hi] of the current context's registered
// state (kept variables excluded); snapshot_file(i) names file i.
EMSCRIPTEN_KEEPALIVE const uint32_t* snapshot_hashes(){
 auto& reg=ssx_snapshot::registry();std::vector<const char*> files;std::vector<uint64_t> h;auto* tls=static_cast<const uint8_t*>(__builtin_wasm_tls_base());
 for(const auto& e:reg){if(e.keep||e.rederived)continue;size_t f=0;for(;f<files.size()&&std::strcmp(files[f],e.file);++f){}if(f==files.size()){files.push_back(e.file);h.push_back(1469598103934665603ull);}h[f]=e.hash(tls+e.offset,ssx_snapshot::mix(h[f],e.offset));}
 for(const auto& hk:ssx_snapshot::hooks()){files.push_back(hk.name);h.push_back(hk.hash());}
 snapshotReport.assign(1,uint32_t(files.size()));for(size_t f=0;f<files.size();++f){snapshotReport.push_back(uint32_t(f));snapshotReport.push_back(uint32_t(h[f]));snapshotReport.push_back(uint32_t(h[f]>>32));}
 return snapshotReport.data();
}
EMSCRIPTEN_KEEPALIVE const char* snapshot_file(uint32_t k){std::vector<const char*> files;for(const auto& e:ssx_snapshot::registry()){if(e.keep||e.rederived)continue;bool seen=false;for(auto* f:files)if(!std::strcmp(f,e.file))seen=true;if(!seen)files.push_back(e.file);}for(const auto& hk:ssx_snapshot::hooks())files.push_back(hk.name);return k<files.size()?files[k]:nullptr;}
// QA: the heap in use (dlmalloc's allocated bytes): the snapshot's real footprint is the difference around its init / saves.
EMSCRIPTEN_KEEPALIVE uint32_t snapshot_heap_used(){return uint32_t(mallinfo().uordblks);}
// QA: dlmalloc's high-water mark of the allocated space (the wasm memory grows to hold it).
EMSCRIPTEN_KEEPALIVE uint32_t snapshot_heap_peak(){return uint32_t(mallinfo().usmblks);}
EMSCRIPTEN_KEEPALIVE uint32_t snapshot_entries(){return uint32_t(ssx_snapshot::registry().size());}
// QA: entry k's name / source file / the bytes its holder keeps (both slots; 0 for a byte-copied or kept one) / its size / flags
// (1 trivially copyable, 2 kept, 4 no holder).
EMSCRIPTEN_KEEPALIVE const char* snapshot_entry_name(uint32_t k){auto& r=ssx_snapshot::registry();return k<r.size()?r[k].name:nullptr;}
EMSCRIPTEN_KEEPALIVE const char* snapshot_entry_file(uint32_t k){auto& r=ssx_snapshot::registry();return k<r.size()?r[k].file:nullptr;}
EMSCRIPTEN_KEEPALIVE uint32_t snapshot_entry_bytes(uint32_t k){if(!snapshotContext||k>=snapshotContext->holders.size()||!snapshotContext->holders[k])return 0;return uint32_t(snapshotContext->holders[k]->bytes());}
// QA: entry k's current value's hash (low 32 bits; a user struct that is not trivially copyable hashes only as present).
EMSCRIPTEN_KEEPALIVE uint32_t snapshot_entry_hash(uint32_t k){auto& r=ssx_snapshot::registry();if(k>=r.size())return 0;return uint32_t(r[k].hash(static_cast<const uint8_t*>(__builtin_wasm_tls_base())+r[k].offset,1469598103934665603ull));}
// QA: the heap entry k's first save allocated (its copy's real size, containers inside user structs too).
EMSCRIPTEN_KEEPALIVE uint32_t snapshot_entry_heap(uint32_t k){return snapshotContext&&k<snapshotContext->firstHeap[0].size()?snapshotContext->firstHeap[0][k]:0;}
// QA: the heap slot's first save allocated in total (entries, then the hooks at index entries).
EMSCRIPTEN_KEEPALIVE uint32_t snapshot_entry_heap_slot(uint32_t k,uint32_t slot){return snapshotContext&&slot<2&&k<snapshotContext->firstHeap[slot].size()?snapshotContext->firstHeap[slot][k]:0;}
EMSCRIPTEN_KEEPALIVE uint32_t snapshot_slot_heap(uint32_t slot){if(!snapshotContext||slot>1)return 0;uint64_t n=0;for(auto v:snapshotContext->firstHeap[slot])n+=v;return uint32_t(n);}
EMSCRIPTEN_KEEPALIVE uint32_t snapshot_entry_size(uint32_t k){auto& r=ssx_snapshot::registry();return k<r.size()?r[k].size:0;}
EMSCRIPTEN_KEEPALIVE uint32_t snapshot_entry_flags(uint32_t k){auto& r=ssx_snapshot::registry();if(k>=r.size())return 0;return (r[k].trivial?1:0)|(r[k].keep?2:0)|(!r[k].trivial&&!r[k].make?4:0);}
}
#endif // SSX_SNAPSHOT_REGISTRY
