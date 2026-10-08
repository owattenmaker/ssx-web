#!/usr/bin/env python3
"""Countdown exports of the exact-derived anchors, for the console-arithmetic start seeds.

For every course whose countdown anchor exists under local/reference-exact (same file name as the location's mode-1
countdown state; Snow Jam's is characters/zoe/countdown.p2s), run the rider-assembly audit and the event-start export
on that state:
  local/browser-validation-exact/<code>/countdown-rider-assemblies.json
  local/assets/native-exact/<code>/event-start.json
then `tools/generate_event_seed.py --exact` writes web/generated/event_start_seed_exact.hpp from them.
A course whose exact anchor is missing, or whose export fails, is reported and skipped (its mode-1 seed stays in use).

The backcountry courses (ABC1, DBC2, EBC3) have no countdown: their anchor is the exact ready state (rolling start), exported by
tools/export_backcountry.py event-start into the same event-start.json layout.

Per character: every exact rider countdown (characters/<id>/countdown.p2s on Snow Jam, characters/courses-<CODE>/<id>/countdown.p2s)
gives that rider's own grid state, the human's extract_ground state (the extraction tools/export_characters.py writes as settings
original_event_start): local/assets/native-exact/characters/<CODE>/<id>.json. The exact core takes it in place of the mode-1 seed
(settings original_event_start, lineups.json humanGridState) for that course and character (web/event_start_select.hpp).

Each roster rider's settings document from its exact Snow Jam countdown (tools/export_characters.py settings: the keys whose
extraction differs from Zoe's on her exact countdown), local/assets/native-exact/RIDER_<ID>/settings.json: the contact legs,
body scales and grid state of the console arithmetic. compare-ps2-capture.mjs reads it under PS2_ARITH=exact-base.
"""
import hashlib
import json
import struct
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from locations import LOCATIONS, human_rider  # noqa: E402

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
    if LOCATIONS[code].get('event') == 'backcountry':
        return export_backcountry(code, snapshot)
    audit = ROOT / f'local/browser-validation-exact/{code}/countdown-rider-assemblies.json'
    output = ROOT / f'local/assets/native-exact/{code}/event-start.json'
    # --discover reads the roster from the state (the exact states are not the hand-made Snow Jam fixture)
    command = ['python3', 'tools/audit_rider_assemblies.py', '--snapshot', str(snapshot), '--output', str(audit), '--discover']
    subprocess.run(command, cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    command = ['python3', 'tools/export_event_start.py', '--location', code, '--snapshot', str(snapshot),
               '--assemblies', str(audit), '--output', str(output)]
    subprocess.run(command, cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    return output


def export_backcountry(code, snapshot):
    output = ROOT / f'local/assets/native-exact/{code}/event-start.json'
    command = ['python3', 'tools/export_backcountry.py', 'event-start', '--location', code,
               '--snapshot', str(snapshot.relative_to(ROOT)), '--output', str(output)]
    subprocess.run(command, cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    return output


def character_states():
    """(code, character id, countdown state) of every exact rider countdown: Snow Jam's, then the other courses'."""
    found = []
    for countdown in sorted((EXACT / 'characters').glob('*/countdown.p2s')):
        found.append(('ARA1', countdown.parent.name, countdown))
    for countdown in sorted((EXACT / 'characters').glob('courses-*/*/countdown.p2s')):
        code = countdown.parent.parent.name.removeprefix('courses-')
        if code in LOCATIONS:
            found.append((code, countdown.parent.name, countdown))
    return found


def export_character(code, character, snapshot):
    """The human's grid state of one exact rider countdown (held on the grid: race clock phase 4, tick 0, control 6, motion 3)."""
    from reference_ground_profile import extract_ground
    from reference_race_event import extract_race_event
    memory = zipfile.ZipFile(snapshot).read('eeMemory.bin')
    clock = extract_race_event(memory)['clock']
    if clock['phase'] != 4 or clock['race_ticks'] != 0:
        raise ValueError(f'{snapshot}: not a countdown (phase {clock["phase"]}, race tick {clock["race_ticks"]})')
    actor = human_rider(memory)
    # the motion mode and control state words of the rider's owner (tools/reference_race_event.py: +0x77C -> +0xDE0 / +0xDE4)
    owner = struct.unpack_from('<I', memory, actor + 0x77C)[0]
    motion, control = struct.unpack_from('<ii', memory, owner + 0xDE0)
    if control != 6 or motion != 3:
        raise ValueError(f'{snapshot}: the human is not held on the grid (control {control}, motion {motion})')
    ground = extract_ground(memory, actor)
    doc = dict(version=1, location=code, character=character, state=ground['state'], body_scale=ground['profile']['body_scale'],
               provenance=dict(snapshot=str(snapshot.relative_to(ROOT)), ee_sha256=hashlib.sha256(memory).hexdigest(),
                               purpose='The human rider\'s grid state at the exact countdown (tools/export_exact_event_starts.py)'))
    output = ROOT / f'local/assets/native-exact/characters/{code}/{character}.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(doc, indent=1) + '\n')
    return output


def export_settings():
    """The roster riders' settings documents from their exact countdowns (Zoe is the exact initial.json itself)."""
    import export_characters
    roster = {r['id'] for r in json.loads((ROOT / 'web/public/assets/riders.json').read_text()) if r['kind'] in ('rider', 'cheat')}
    zoe = EXACT / 'characters/zoe'
    done = []
    for states in sorted((EXACT / 'characters').glob('*/countdown.p2s')):
        cid = states.parent.name
        if cid not in roster or cid == 'zoe':
            continue
        target = ROOT / f'local/assets/native-exact/RIDER_{cid.upper()}/settings.json'
        try:
            export_characters.settings(cid, states.parent, zoe, target=target)
            done.append(cid)
        except (ValueError, KeyError) as error:
            print(f'{cid}: settings export failed ({error})', file=sys.stderr)
    return done


def main():
    print('settings:', ' '.join(export_settings()))
    characters = []
    for code, character, snapshot in character_states():
        try:
            export_character(code, character, snapshot)
            characters.append(f'{code}/{character}')
        except (ValueError, KeyError) as error:
            print(f'{code}/{character}: character export failed ({error})', file=sys.stderr)
    print('characters:', ' '.join(characters))
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
