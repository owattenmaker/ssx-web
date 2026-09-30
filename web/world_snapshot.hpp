#pragma once
// The rider-context snapshot (docs/replay.md §2a; pv eventReturnInWorld (b)): the port's form of the PS2's replay snapshots (the
// countdown's 0x26D818 and the results-time R+0x3D0 that 0x2706F0 restores). A rider context is a TLS block (web/rider_local.hpp);
// every file-scope RIDER_LOCAL variable is registered here at static initialisation (web/generated/snapshot/<unit>.inc, made by
// web/generate-snapshot-registry.mjs) with its offset in the block and its type. A save copies the block and deep-copies each
// variable that is not trivially copyable (containers own heap memory) into holders made at snapshot_init; a restore puts the
// trivially copyable ones back as bytes and copy-assigns the others (so a container keeps its own allocation). Variables on the
// keep list (web/snapshot-policy.mjs) and the function-local statics (the info exports' output buffers) keep their current value.
#include "rider_local.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ssx_snapshot {

inline uint64_t fnv(const void* p, size_t n, uint64_t h = 1469598103934665603ull) {
  const auto* b = static_cast<const uint8_t*>(p);
  for (size_t k = 0; k < n; ++k) { h ^= b[k]; h *= 1099511628211ull; }
  return h;
}
inline uint64_t mix(uint64_t h, uint64_t v) { return fnv(&v, sizeof v, h); }

// QA hash of a value (web/world_snapshot: the per-subsystem self-check). Containers of hashable elements hash their elements;
// a user struct that is not trivially copyable hashes only as "present" (its members are not reachable without reflection).
template <class T> struct is_vec : std::false_type {}; template <class U, class A> struct is_vec<std::vector<U, A>> : std::true_type {};
template <class T> struct is_deq : std::false_type {}; template <class U, class A> struct is_deq<std::deque<U, A>> : std::true_type {};
template <class T> struct is_arr : std::false_type {}; template <class U, size_t N> struct is_arr<std::array<U, N>> : std::true_type {};
template <class T> struct is_map : std::false_type {}; template <class K, class V, class C, class A> struct is_map<std::map<K, V, C, A>> : std::true_type {};
template <class T> struct is_set : std::false_type {}; template <class K, class C, class A> struct is_set<std::set<K, C, A>> : std::true_type {};
template <class T> struct is_umap : std::false_type {}; template <class K, class V, class H, class E, class A> struct is_umap<std::unordered_map<K, V, H, E, A>> : std::true_type {};
template <class T> struct is_uset : std::false_type {}; template <class K, class H, class E, class A> struct is_uset<std::unordered_set<K, H, E, A>> : std::true_type {};
template <class T> struct is_opt : std::false_type {}; template <class U> struct is_opt<std::optional<U>> : std::true_type {};
template <class T> struct is_uptr : std::false_type {}; template <class U, class D> struct is_uptr<std::unique_ptr<U, D>> : std::true_type {};
template <class T> struct is_pair : std::false_type {}; template <class A, class B> struct is_pair<std::pair<A, B>> : std::true_type {};

template <class T> uint64_t hash_value(const T& v, uint64_t h);
template <class T> uint64_t hash_value(const T& v, uint64_t h) {
  if constexpr (is_opt<T>::value) { return v ? hash_value(*v, mix(h, 1)) : mix(h, 0); } // (a disengaged optional's payload bytes are not its value)
  else if constexpr (is_pair<T>::value) return hash_value(v.second, hash_value(v.first, h)); // (a map's pairs: their members, not their bytes)
  else if constexpr (std::is_trivially_copyable_v<T>) return fnv(&v, sizeof(T), h);
  else if constexpr (std::is_array_v<T>) { for (const auto& e : v) h = hash_value(e, h); return h; }
  else if constexpr (is_vec<T>::value || is_deq<T>::value || is_arr<T>::value) { h = mix(h, v.size()); for (const auto& e : v) h = hash_value(e, h); return h; }
  else if constexpr (is_map<T>::value || is_set<T>::value) { h = mix(h, v.size()); for (const auto& e : v) h = hash_value(e, h); return h; }
  else if constexpr (is_umap<T>::value || is_uset<T>::value) { uint64_t s = 0; for (const auto& e : v) s += hash_value(e, 1469598103934665603ull); return mix(mix(h, v.size()), s); }
  else if constexpr (is_opt<T>::value) { return v ? hash_value(*v, mix(h, 1)) : mix(h, 0); }
  else if constexpr (is_uptr<T>::value) { if constexpr (std::is_copy_constructible_v<typename T::element_type>) return v ? hash_value(*v, mix(h, 1)) : mix(h, 0); else return mix(h, v ? 1 : 0); }
  else if constexpr (is_pair<T>::value) return hash_value(v.second, hash_value(v.first, h));
  else if constexpr (std::is_same_v<T, std::string>) return fnv(v.data(), v.size(), mix(h, v.size()));
  else if constexpr (requires { { v.dump() } -> std::convertible_to<std::string>; }) { const std::string d = v.dump(); return fnv(d.data(), d.size(), mix(h, d.size())); } // a JSON document
  else return mix(h, sizeof(T)); // not reachable member by member
}

// Heap bytes a copy holds (the budget: at most 5 MB per copy, docs/replay.md §2a).
template <class T> size_t heap_bytes(const T& v) {
  if constexpr (std::is_trivially_copyable_v<T>) return 0;
  else if constexpr (std::is_array_v<T>) { size_t n = 0; for (const auto& e : v) n += heap_bytes(e); return n; }
  else if constexpr (is_vec<T>::value) { size_t n = v.capacity() * sizeof(typename T::value_type); for (const auto& e : v) n += heap_bytes(e); return n; }
  else if constexpr (is_deq<T>::value || is_arr<T>::value) { size_t n = is_deq<T>::value ? v.size() * sizeof(typename T::value_type) : 0; for (const auto& e : v) n += heap_bytes(e); return n; }
  else if constexpr (is_map<T>::value || is_set<T>::value || is_umap<T>::value || is_uset<T>::value) { size_t n = v.size() * (sizeof(typename T::value_type) + 32); for (const auto& e : v) n += heap_bytes(e); return n; }
  else if constexpr (is_opt<T>::value) return v ? heap_bytes(*v) : 0;
  else if constexpr (is_uptr<T>::value) { if constexpr (std::is_copy_constructible_v<typename T::element_type>) return v ? sizeof(*v) + heap_bytes(*v) : 0; else return 0; }
  else if constexpr (is_pair<T>::value) return heap_bytes(v.first) + heap_bytes(v.second);
  else if constexpr (std::is_same_v<T, std::string>) return v.capacity();
  else return 0; // a user struct with containers: counted by the caller's total only as its size
}

// Deep copy: a unique_ptr copies its pointee (none stays none), a vector / deque / array / optional copies element by element
// (so a vector of unique_ptr works), everything else is assigned. deep_copyable says whether assign_deep compiles for T.
template <class T> struct deep_copyable;
template <class T> inline constexpr bool deep_copyable_v = deep_copyable<T>::value;
template <class T> struct deep_copyable { static constexpr bool value = std::is_copy_constructible_v<T> && std::is_copy_assignable_v<T>; };
template <class U, class D> struct deep_copyable<std::unique_ptr<U, D>> { static constexpr bool value = std::is_copy_constructible_v<U> && deep_copyable_v<U>; };
template <class E, class A> struct deep_copyable<std::vector<E, A>> { static constexpr bool value = std::is_default_constructible_v<E> && deep_copyable_v<E>; };
template <class E, class A> struct deep_copyable<std::deque<E, A>> { static constexpr bool value = std::is_default_constructible_v<E> && deep_copyable_v<E>; };
template <class E, size_t N> struct deep_copyable<std::array<E, N>> { static constexpr bool value = deep_copyable_v<E>; };
template <class E> struct deep_copyable<std::optional<E>> { static constexpr bool value = std::is_copy_constructible_v<E> && deep_copyable_v<E>; };
template <class K, class V, class C, class A> struct deep_copyable<std::map<K, V, C, A>> { static constexpr bool value = std::is_copy_constructible_v<V> && deep_copyable_v<V>; };
template <class K, class V, class H, class E, class A> struct deep_copyable<std::unordered_map<K, V, H, E, A>> { static constexpr bool value = std::is_copy_constructible_v<V> && deep_copyable_v<V>; };
template <class A, class B> struct deep_copyable<std::pair<A, B>> { static constexpr bool value = deep_copyable_v<A> && deep_copyable_v<B>; };
template <class U, size_t N> struct deep_copyable<U[N]> { static constexpr bool value = deep_copyable_v<U>; };

template <class T> void assign_deep(T& dst, const T& src) {
  if constexpr (std::is_trivially_copyable_v<T> && !std::is_array_v<T>) dst = src;
  else if constexpr (std::is_array_v<T>) { for (size_t k = 0; k < std::extent_v<T>; ++k) assign_deep(dst[k], src[k]); }
  else if constexpr (is_uptr<T>::value) { if (!src) dst.reset(); else if (dst) assign_deep(*dst, *src); else dst.reset(new typename T::element_type(*src)); }
  else if constexpr (is_vec<T>::value || is_deq<T>::value) { using E = typename T::value_type; if constexpr (is_uptr<E>::value || is_vec<E>::value || is_deq<E>::value || is_arr<E>::value || is_opt<E>::value) { dst.resize(src.size()); for (size_t k = 0; k < src.size(); ++k) assign_deep(dst[k], src[k]); } else dst = src; }
  else if constexpr (is_arr<T>::value) { for (size_t k = 0; k < src.size(); ++k) assign_deep(dst[k], src[k]); }
  else dst = src;
}

struct HolderBase {
  virtual ~HolderBase() = default;
  virtual void save(const void* var, unsigned slot) = 0;
  virtual bool restore(void* var, unsigned slot) = 0;
  virtual size_t bytes() const = 0;
};

// A copy of one variable per slot. unique_ptr<U> keeps a U (or none); a C array is copied element by element.
template <class T> struct Holder final : HolderBase {
  std::array<std::optional<T>, 2> copies;
  void save(const void* var, unsigned slot) override { const T& v = *static_cast<const T*>(var); auto& c = copies[slot]; if (!c) { if constexpr (std::is_default_constructible_v<T>) c.emplace(); else c.emplace(v); } assign_deep(*c, v); }
  bool restore(void* var, unsigned slot) override { auto& c = copies[slot]; if (!c) return false; assign_deep(*static_cast<T*>(var), *c); return true; }
  size_t bytes() const override { size_t n = 0; for (const auto& c : copies) if (c) n += sizeof(T) + heap_bytes(*c); return n; }
};
template <class U, class D> struct Holder<std::unique_ptr<U, D>> final : HolderBase {
  std::array<std::optional<U>, 2> copies; std::array<bool, 2> present{}, saved{};
  void save(const void* var, unsigned slot) override { const auto& v = *static_cast<const std::unique_ptr<U, D>*>(var); saved[slot] = true; present[slot] = !!v; if (v) { auto& c = copies[slot]; if (c) *c = *v; else c.emplace(*v); } }
  bool restore(void* var, unsigned slot) override { if (!saved[slot]) return false; auto& v = *static_cast<std::unique_ptr<U, D>*>(var); if (!present[slot]) { v.reset(); return true; } if (v) *v = *copies[slot]; else v.reset(new U(*copies[slot])); return true; }
  size_t bytes() const override { size_t n = 0; for (unsigned k = 0; k < 2; ++k) if (present[k] && copies[k]) n += sizeof(U) + heap_bytes(*copies[k]); return n; }
};
template <class U, size_t N> struct Holder<U[N]> final : HolderBase {
  std::array<std::optional<std::array<U, N>>, 2> copies;
  void save(const void* var, unsigned slot) override { const U* v = static_cast<const U*>(var); auto& c = copies[slot]; if (!c) c.emplace(); for (size_t k = 0; k < N; ++k) (*c)[k] = v[k]; }
  bool restore(void* var, unsigned slot) override { auto& c = copies[slot]; if (!c) return false; U* v = static_cast<U*>(var); for (size_t k = 0; k < N; ++k) v[k] = (*c)[k]; return true; }
  size_t bytes() const override { size_t n = 0; for (const auto& c : copies) if (c) n += sizeof(U) * N + heap_bytes(*c); return n; }
};

struct Entry {
  uint32_t offset, size; const char* name; const char* file; bool trivial, keep; bool own; // own: a hook's storage (not restored, not checked)
  bool rederived; // restored, then re-derived by a restore hook (out of the self-check hashes)
  std::unique_ptr<HolderBase> (*make)(); uint64_t (*hash)(const void*, uint64_t);
};
std::vector<Entry>& registry(); // web/rider_context.cpp (shared by every context: the layout of the TLS block is one)
// A subsystem's own save / restore of state that is not a registered variable's value (web/world_bridge.cpp: the collision world's
// run-time instance state), run in the current context after the registered variables.
struct Hook { const char* name; void (*save)(unsigned slot); bool (*restore)(unsigned slot); uint64_t (*hash)(); size_t (*bytes)(); bool (*check)(unsigned slot); }; // check: the restore can run (nothing changed yet)
std::vector<Hook>& hooks();
extern bool qa; // snapshot_qa (web/rider_context.cpp): the kept tables' and hooks' QA checks

template <class T> std::unique_ptr<HolderBase> make_holder() { return std::make_unique<Holder<T>>(); }
template <class T> uint64_t hash_entry(const void* p, uint64_t h) { return hash_value(*static_cast<const T*>(p), h); }

// Registers one RIDER_LOCAL variable (in the static TLS block, at static initialisation). keep: its value is left at a restore.
template <class T> void add(T* p, const char* name, const char* file, int keep) {
  const auto base = reinterpret_cast<uintptr_t>(__builtin_wasm_tls_base()), at = reinterpret_cast<uintptr_t>(p);
  Entry e{uint32_t(at - base), uint32_t(sizeof(T)), name, file, std::is_trivially_copyable_v<T>, keep == 1 || keep == 2, keep == 2, keep == 3, nullptr, &hash_entry<T>};
  if constexpr (!std::is_trivially_copyable_v<T>) {
    if constexpr (is_uptr<T>::value) { if constexpr (deep_copyable_v<T>) e.make = &make_holder<T>; }
    else if constexpr (deep_copyable_v<T>) e.make = &make_holder<T>;
  }
  registry().push_back(e);
}

} // namespace ssx_snapshot
