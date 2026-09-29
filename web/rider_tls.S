# Rider contexts (web/rider_local.hpp): the core's per-rider state is thread-local. With a non-shared memory wasm-ld
# gives no mutable __tls_base (and no __tls_size), so it is defined here, mutable, and web/patch-rider-tls.mjs sets
# the initial values after the link: __tls_base = the static TLS block (.tdata, the human's context), rider_tls_size =
# its size. Switching the rider context is one global.set (from JS: Module.___tls_base.value).
.globaltype __tls_base, i32
.globl __tls_base
__tls_base:
.globaltype rider_tls_size, i32
.globl rider_tls_size
rider_tls_size:
.globl rider_tls_size_value
rider_tls_size_value:
  .functype rider_tls_size_value () -> (i32)
  global.get rider_tls_size
  end_function
.globl rider_tls_set
rider_tls_set:
  .functype rider_tls_set (i32) -> ()
  local.get 0
  global.set __tls_base
  end_function
