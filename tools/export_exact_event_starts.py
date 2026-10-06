#!/usr/bin/env python3
"""Countdown exports of the exact-derived anchors, for the console-arithmetic start seeds.

For every course whose countdown anchor exists under local/reference-exact (same file name as the location's mode-1
countdown state; Snow Jam's is characters/zoe/countdown.p2s), run the rider-assembly audit and the event-start export
on that state:
  local/browser-validation-exact/<code>/countdown-rider-assemblies.json
  local/assets/native-exact/<code>/event-start.json
then `tools/generate_event_seed.py --exact` writes web/generated/event_start_seed_exact.hpp from them.
A course whose exact anchor is missing, or whose export fails, is reported and skipped (its mode-1 seed stays in use). The
backcountry courses (ABC1, DBC2, EBC3: ready states, tools/export_backcountry.py) are not covered yet.
"""
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from locations import LOCATIONS  # noqa: E402

EXACT = ROOT / 'local/reference-exact'
# Snow Jam's mode-1 countdown is a hand-made savestate (countdown-1.p2m2_SaveState.p2s). Its exact counterpart is Zoe's derived
# Snow Jam countdown (characters/zoe/countdown.p2s, the riders/zoe-race lineup): snow-jam-countdown-anchor.p2s comes from the menu
# path and carries Zoe's NIS head model, which the assembly audit rejects.
ANCHOR_NAME = {'ARA1': 'characters/zoe/countdown.p2s'}


def anchor(code):
    name = ANCHOR_NAME.get(code) or Path(LOCATIONS[code]['states'].get('countdown', '')).name
    path = EXACT / name if name else None
    return path if path and path.exists() else None


def export(code, snapshot):
    audit = ROOT / f'local/browser-validation-exact/{code}/countdown-rider-assemblies.json'
    output = ROOT / f'local/assets/native-exact/{code}/event-start.json'
    # --discover reads the roster from the state (the exact states are not the hand-made Snow Jam fixture)
    command = ['python3', 'tools/audit_rider_assemblies.py', '--snapshot', str(snapshot), '--output', str(audit), '--discover']
    subprocess.run(command, cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    command = ['python3', 'tools/export_event_start.py', '--location', code, '--snapshot', str(snapshot),
               '--assemblies', str(audit), '--output', str(output)]
    subprocess.run(command, cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    return output


def main():
    done = []
    for code in LOCATIONS:
        snapshot = anchor(code)
        if snapshot is None:
            continue
        try:
            export(code, snapshot)
            done.append(code)
        except subprocess.CalledProcessError as error:
            print(f'{code}: export failed ({error})', file=sys.stderr)
    print('exported:', ' '.join(done))


if __name__ == '__main__':
    main()
