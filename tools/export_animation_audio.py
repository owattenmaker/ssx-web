#!/usr/bin/env python3
"""Export the animation-event sound ids for web/sfx-game.js (docs/audio-logic.md 5.3).

103AA0 walks the event list of every playing sequence's clip; an event latched this tick whose id has the 0x8000
flag is consumed (144670) and passed to 104CC8 -> 289B18: ids 0x50 / 0x51 / 0x52 play bank 0 sounds 0x51 / 0x50 /
0x52 (29BCF8 / 29B968 / 29C088) for the human rider. The core reports every latched event bit with the clip id
(AE_ANIM_EVENT); this table maps (animation index = clip id >> 8, event bit) -> the event's id word.

Writes web/public/assets/AUDIO/anim-events.json: {"clips": {"<animation index>": [id per event bit]}} for the
clips that carry a 0x8000-flagged id. Source: local/assets/animation/ps2/basic.afl (tools/animation_bank.py).
"""
import argparse, json, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from animation_bank import read_bank  # noqa: E402


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--bank', type=Path, default=ROOT / 'local/assets/animation/ps2/basic.afl')
    ap.add_argument('--out', type=Path, default=ROOT / 'web/public/assets/AUDIO/anim-events.json')
    a = ap.parse_args()
    bank = read_bank(a.bank.read_bytes())
    clips = {}
    for i, anim in enumerate(bank['animations']):
        ids = [eid for _, eid in anim['events']]
        if any(e & 0x8000 and (e & 0x7FFF) in (0x50, 0x51, 0x52) for e in ids): clips[str(i)] = ids
    a.out.parent.mkdir(parents=True, exist_ok=True)
    a.out.write_text(json.dumps(dict(source=str(a.bank.relative_to(ROOT)), clips=clips), separators=(',', ':')))
    print(f'wrote {a.out} ({len(clips)} clips)')


if __name__ == '__main__':
    main()
