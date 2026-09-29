#!/usr/bin/env python3
"""Freestyle event configuration of a course (docs/slopestyle-bigair.md), from its countdown-anchor savestate.

    .venv/bin/python tools/export_freestyle_event.py --location ASS1
    -> web/public/assets/<X>/freestyle-event.json (and local/assets/native/<X>/freestyle-event.json)

Everything here is the state the original sets up while loading the event (read from EE RAM, never guessed):
* byte 0x535C10 event kind, 0x535C11 path (1 single event), 0x535C12 game mode (1 slope style, 2 pipe, 3 big air);
* the event handler index GMM+4 (*0x4A2C6C; 0 = freestyle handler 0x47CFA0, 1 = race), GMM+8 the freestyle kind
  (= the game mode), GMM+0x10 computer riders, GMM+0x14 posted riders (0x238E20: 4 when the kind is 1, else 5),
  GMM+0x78 the time limit in ticks (0x1454F8(course, round) x 60), GMM+0x88 timed;
* the checkpoint-bonus list 0x4D33B8 (six {int32 value, float remaining distance}; path manager 0x4D33A0 +0x18):
  R&B carries two +60 entries (0x2398E8: slope style adds value x 60 ticks to the limit and pays the popup);
* the game-info rank mode (+0x74: 1 races, 2 freestyle) and the event rider count (+0x78).
The browser passes mode/handler/kind/table to set_race_bonus (web/race_bridge.cpp) and the limit to race_time_limit.
"""
import argparse, hashlib, json, struct, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from locations import location as location_info, state as location_state, native_dir, web_dir  # noqa: E402

GMM_PTR = 0x4A2C6C
GP = 0x4A30F0


def extract(memory):
    u = lambda a: struct.unpack_from('<I', memory, a & 0x1FFFFFF)[0]
    i = lambda a: struct.unpack_from('<i', memory, a & 0x1FFFFFF)[0]
    f = lambda a: struct.unpack_from('<f', memory, a & 0x1FFFFFF)[0]
    gmm = u(GMM_PTR)
    if not gmm: raise ValueError('No GameModeMan in this savestate')
    game = u(u(u(GP - 0x848) + 0x84) + 0x0C)
    words = [i(gmm + 4 * k) for k in range(0x28)]
    bonus = []
    for k in range(6):
        value, distance = i(0x4D33B8 + 8 * k), f(0x4D33B8 + 8 * k + 4)
        bonus.append(dict(value=value, distance=distance))
    raw = [struct.unpack_from('<i', memory, 0x4D33B8 + 4 * k)[0] for k in range(12)]
    return dict(event_kind=memory[0x535C10], path=memory[0x535C11], game_mode=memory[0x535C12], course_index=memory[0x535C08],
                handler=words[1], freestyle_kind=words[2], round=words[0], computer_riders=words[4], posted_riders=words[5],
                time_limit_ticks=words[0x78 // 4], timed=words[0x88 // 4], gmm_words=words,
                bonus=bonus, bonus_words=raw, rank_mode=u(game + 0x74), event_riders=u(game + 0x78),
                global_flags=u(0x5308D0))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--location', required=True)
    code = p.parse_args().location
    info = location_info(code)
    snapshot = location_state(code, 'anchor')
    memory = zipfile.ZipFile(snapshot).read('eeMemory.bin')
    doc = extract(memory)
    if doc['course_index'] != info['course_index']:
        raise ValueError(f'{snapshot.name} is course {doc["course_index"]}, expected {info["course_index"]}')
    doc = dict(version=1, location=code, **doc, provenance=dict(snapshot=str(snapshot.relative_to(ROOT)),
                                                               ee_sha256=hashlib.sha256(memory).hexdigest()))
    text = json.dumps(doc, indent=1) + '\n'
    for out in (native_dir(code) / 'freestyle-event.json', web_dir(code) / 'freestyle-event.json'):
        out.parent.mkdir(parents=True, exist_ok=True); out.write_text(text)
    print(json.dumps({k: doc[k] for k in ('location', 'game_mode', 'handler', 'freestyle_kind', 'posted_riders', 'time_limit_ticks', 'rank_mode', 'event_riders')}))
    print('bonus', [(b['value'], round(b['distance'], 2)) for b in doc['bonus'] if b['value']])


if __name__ == '__main__':
    main()
