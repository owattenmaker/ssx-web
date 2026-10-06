#!/bin/sh
# One matcher round (docs/ps2-float.md "The matcher"): the PS2's pass TICK (exact mode) against the port's row TICK+1.
# usage: tools/ps2-float/match_tick.sh GATE TICK TRACE_CORE_DIR -- COMPARER_ARGS...
#   GATE: a gate name (its savestate local/ps2-capture/runs/GATE.p2s, its exact ring local/ps2-capture/runs-exact/GATE.bin)
# Writes local/ps2-float/match/GATE-TICK.{p2s,fpu,port.json,txt}; one ARMSX2 instance (waits for a slot).
# Exact-baseline gates: MATCH_STATES=<dir of GATE.p2s> MATCH_RUNS=<dir of GATE.bin> MATCH_ARITH=exact-base MATCH_TAG=<suffix>.
# MATCH_ACTOR=<rider address> also writes the human-only PS2 trace (oracle --actor) and matches against it.
set -eu
cd "$(dirname "$0")/../.."
gate=$1
tick=$2
core=$3
shift 4
states=${MATCH_STATES:-local/ps2-capture/runs}
runs=${MATCH_RUNS:-local/ps2-capture/runs-exact}
arith=${MATCH_ARITH:-exact}
out=local/ps2-float/match/$gate-$tick${MATCH_TAG:-}
mkdir -p "$(dirname "$out")"
if [ ! -f "$out.fpu" ]; then
  while [ "$(pgrep -x ARMSX2 | wc -l)" -ge 4 ]; do sleep 5; done
  nice -n 5 python3 tools/ps2-float/ee_oracle/snap_at.py "$states/$gate.p2s" "$tick" "$out.p2s" --timeout 900 > /dev/null
  manager=$(python3 tools/ps2-float/ee_oracle/state.py "$out.p2s" "$out.state" | awk '/rider_manager/{print $4}')
  local/ps2-float/bin/oracle --state "$out.state" --call 0x128AF0 --a0 "$manager" --trace-fpu > "$out.fpu"
fi
(cd web && PS2_ARITH=$arith PS2_MATCH_TICK=$((tick + 1)) PS2_MATCH_OUT="../$out.port.json" TICK_HOOK=../tools/ps2-float/trace_hook.mjs \
  CORE_JS="../$core/core.js" node compare-ps2-capture.mjs "../$runs/$gate.bin" --pad --sync-rng "$@" \
  --ticks $((tick + 3)) > /dev/null 2>&1 || true)
trace="$out.fpu"
if [ -n "${MATCH_ACTOR:-}" ]; then
  if [ ! -f "$out.actor.fpu" ]; then
    manager=$(python3 tools/ps2-float/ee_oracle/state.py "$out.p2s" "$out.state" | awk '/rider_manager/{print $4}')
    local/ps2-float/bin/oracle --state "$out.state" --call 0x128AF0 --a0 "$manager" --trace-fpu --actor "$MATCH_ACTOR" > "$out.actor.fpu"
  fi
  trace="$out.actor.fpu"
fi
python3 tools/ps2-float/match.py "$out.port.json" "$trace" --limit 25 | tee "$out.txt"
