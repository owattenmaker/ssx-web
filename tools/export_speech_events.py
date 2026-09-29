#!/usr/bin/env python3
"""Export the EA speech-event data for web/audio-speech-events.js (read-only on the ISO).

Writes web/public/assets/AUDIO/speech/:
  Events.evt       the event script (59 events, EE 0x3D6498 register), copied as-is from SPEECH.BIG/headers.big
  registry.json    {"banks": [{name, id, F, N, H, masks, keys}]} in registration order: headers.big .hdr entries,
                   then ENGLISH.BIG/langhead.big ones (0x3D69F0 registry; the index is the no-repeat key of 0x3DB5C0).
                   .hdr: +0 u16 id, +4 F (low nibble), +5 N lines, +6 H history length, +0xC N x {u16 offset,
                   u8 key[F]} pad4, then u32 valueMask[F].

The .hdr line tables themselves are already in speech/<name>.json (tools/export_audio.py).
See docs/audio-logic.md section 4.

  python3 tools/export_speech_events.py [--iso PATH] [--out DIR]
"""
import argparse, json, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from inspect_disc import Disc  # noqa: E402
from export_audio import BigOnDisc, big_entries, ISO, OUT  # noqa: E402


def parse_hdr(name, b):
    F, N, H = b[4] & 15, b[5], b[6]
    es = F + 2
    keys = [list(b[12 + i * es + 2:12 + i * es + 2 + F]) for i in range(N)]
    mo = 12 + ((N * es + 3) & ~3)
    masks = [struct.unpack_from('<I', b, mo + 4 * j)[0] for j in range(F)]
    if b[4] & 0x80: raise ValueError(f'{name}: played bitmap not supported')
    return dict(name=name, id=struct.unpack_from('<H', b, 0)[0], F=F, N=N, H=H, masks=masks, keys=keys)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--iso', type=Path, default=ISO)
    ap.add_argument('--out', type=Path, default=OUT)
    args = ap.parse_args()
    folder = args.out / 'speech'; folder.mkdir(parents=True, exist_ok=True)
    disc = Disc(args.iso)
    try:
        names, events = [], None
        for big_path, head in (('DATA/AUDIO/SPEECH.BIG', 'headers.big'), ('DATA/AUDIO/ENGLISH.BIG', 'langhead.big')):
            hb = BigOnDisc(disc, big_path).read(head)
            for e in big_entries(hb):
                name = e['name'].split('\\')[-1]
                if name.lower().endswith('.hdr'): names.append(parse_hdr(name[:-4], hb[e['offset']:e['offset'] + e['size']]))
                elif name.lower() == 'events.evt' and events is None: events = hb[e['offset']:e['offset'] + e['size']]
        if events is None: raise SystemExit('Events.evt not found')
        (folder / 'Events.evt').write_bytes(events)
        (folder / 'registry.json').write_text(json.dumps(dict(banks=names), separators=(',', ':')))
        print(f'wrote {folder}/Events.evt ({len(events)} bytes), registry.json ({len(names)} banks)')
    finally:
        disc.close()


if __name__ == '__main__':
    main()
