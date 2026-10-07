#!/bin/zsh
# tools/ps2-float/match_camera.sh GATE TICK CORE_DIR [COMPARER_ARGS...]: camera matcher round on an exact-base gate. The PS2 side is the oracle:
# the rider pass 0x128AF0 of pass TICK-1's snap, then the DEFAULT_3 update 0x176E10 on the gate's camera block (traced);
# the port side is the trace core's row TICK; only engine/original_camera* sites are matched.
cd "$(dirname "$0")/../.."
gate=$1; tick=$2; core=$3; shift 3
prev=$((tick - 1))
out=local/ps2-float/match/$gate-$prev-xb
O=local/ps2-float/bin/oracle
mkdir -p $(dirname $out)
camera=$(python3 -c "import json;print(json.load(open('local/ps2-capture/runs-exactbase/$gate.capture.json'))['camera'])")
if [ ! -d $out.state ]; then
  while [ "$(pgrep -x ARMSX2 | wc -l)" -ge 4 ]; do sleep 5; done
  nice -n 5 python3 tools/ps2-float/ee_oracle/snap_at.py local/ps2-capture/runs-exactbase/$gate.p2s $prev $out.p2s --timeout 900 > /dev/null || exit 1
  python3 tools/ps2-float/ee_oracle/state.py $out.p2s $out.state > /dev/null
fi
manager=$(python3 tools/ps2-float/ee_oracle/state.py $out.p2s $out.state | awk '/rider_manager/{print $4}')
rm -rf $out.after && cp -R $out.state $out.after
$O --state $out.state --call 0x128AF0 --a0 $manager --save $out.after > /dev/null 2>&1
$O --state $out.after --call 0x176E10 --a0 $camera --trace-fpu > $out.cam.fpu 2>&1
(cd web && PS2_ARITH=exact-base PS2_MATCH_TICK=$tick PS2_MATCH_OUT=../$out.camport.json TICK_HOOK=../tools/ps2-float/trace_hook.mjs \
  CORE_JS=../$core/core.js node compare-ps2-capture.mjs ../local/ps2-capture/runs-exactbase/$gate.bin --pad --sync-rng "$@" --ticks $((tick + 2)) > /dev/null 2>&1)
python3 tools/ps2-float/match.py $out.camport.json $out.cam.fpu --limit 25 --exclude '^(?!.*original_camera)' | head -24
