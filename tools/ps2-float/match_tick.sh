#!/bin/sh
# One matcher round (docs/ps2-float.md "The matcher"): the PS2's pass TICK (exact mode) against the port's row TICK+1.
# usage: tools/ps2-float/match_tick.sh GATE TICK TRACE_CORE_DIR -- COMPARER_ARGS...
#   GATE: a gate name (its savestate local/ps2-capture/runs/GATE.p2s, its exact ring local/ps2-capture/runs-exact/GATE.bin)
# Writes local/ps2-float/match/GATE-TICK.{p2s,fpu,port.json,txt}; one ARMSX2 instance (waits for a slot).
set -eu
cd "$(dirname "$0")/../.."
gate=$1
tick=$2
core=$3
shift 4
out=local/ps2-float/match/$gate-$tick
mkdir -p "$(dirname "$out")"
if [ ! -f "$out.fpu" ]; then
  while [ "$(pgrep -x ARMSX2 | wc -l)" -ge 4 ]; do sleep 5; done
  nice -n 5 python3 tools/ps2-float/ee_oracle/snap_at.py "local/ps2-capture/runs/$gate.p2s" "$tick" "$out.p2s" --timeout 900 > /dev/null
  manager=$(python3 tools/ps2-float/ee_oracle/state.py "$out.p2s" "$out.state" | awk '/rider_manager/{print $4}')
  local/ps2-float/bin/oracle --state "$out.state" --call 0x128AF0 --a0 "$manager" --trace-fpu > "$out.fpu"
fi
(cd web && PS2_ARITH=exact PS2_MATCH_TICK=$((tick + 1)) PS2_MATCH_OUT="../$out.port.json" TICK_HOOK=../tools/ps2-float/trace_hook.mjs \
  CORE_JS="../$core/core.js" node compare-ps2-capture.mjs "../local/ps2-capture/runs-exact/$gate.bin" --pad --sync-rng "$@" \
  --ticks $((tick + 3)) > /dev/null 2>&1 || true)
python3 tools/ps2-float/match.py "$out.port.json" "$out.fpu" --limit 25 | tee "$out.txt"
