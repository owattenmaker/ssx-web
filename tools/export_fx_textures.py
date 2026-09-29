#!/usr/bin/env python3
"""Export FX textures (EFFECTS.SSH members named by the FX texture table 0x4891B0, 12-byte records with a
4-byte tag) used by the stage-script world effects (development export; git-ignored output):

  web/public/assets/FX/<tag>.gs.rgba   raw GS RGBA (alpha byte, 128 = 1.0), decoded like export_set_piece_particle_textures.py
                                       from PARTICLE.SSH or EFFECTS.SSH
  web/public/assets/FX/fx.json         {version, textures: {tag: {fx_index, width, height, file, sha256}}}

* HaloModifier (builtin97 0x3057C0, draw 0x3462A0 -> 0x2D1D10): FX 37 + key1 -> 37 blha, 38 bsha, 39 gcha,
  40 orha, 41 rdha, 42 whha (the courses use gcha / rdha / whha).
* CrowdMan2d camera flashes (0x229BA8): FX 65 flsh.
* Rider power-up aura (RFX+0x9C0 draw 0x2EB198, renderer+0x1048) FX 62 psmr; air streamers (RFX+0xAD0 draw 0x2EF950)
  FX 63 strm (renderer+0x104C), FX 61 prbn (+0x1044) while the trick boost holds (web/boost-renderer.js).
* Weather (docs/weather.md): snowfall flakes / fluff (0x2E6008, renderer+0xF70) FX 8 sfal; camera splash drops
  (0x2F2C30, renderer+0x1058) FX 66 ices, ice crystals (0x2F3418, renderer+0x105C) FX 67 icel.
* Terrain snow sparkle (0x38D968, renderer+0x64 = 68 -> renderer+0xF50 + 4 x 68) FX 68 gltr (docs/presentation.md 14).

usage: export_fx_textures.py [--ps2-iso PATH]
"""
import argparse, hashlib, json, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from inspect_disc import Disc, EXPECTED_SHA1  # noqa: E402
from export_sun_flare import ELF_BASE  # noqa: E402
from export_set_piece_particle_textures import decode  # noqa: E402

FX = {8: 'sfal', 37: 'blha', 38: 'bsha', 39: 'gcha', 40: 'orha', 41: 'rdha', 42: 'whha', 61: 'prbn', 62: 'psmr', 63: 'strm', 65: 'flsh', 66: 'ices', 67: 'icel', 68: 'gltr'}
REQUIRED = {'gcha', 'rdha', 'whha', 'flsh', 'prbn', 'psmr', 'strm'}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    from disc_paths import ps2_iso;p.add_argument('--ps2-iso', type=Path, default=ps2_iso())
    p.add_argument('--out', type=Path, default=ROOT / 'web/public/assets/FX')
    a = p.parse_args()
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
    if hashlib.sha1(elf).hexdigest() != EXPECTED_SHA1: raise ValueError('Unexpected original executable')
    for i, n in FX.items():
        at = 0x4891B0 + 12 * i - ELF_BASE
        if elf[at:at + 4] != n.encode(): raise ValueError(f'FX texture table mismatch at {i}: {elf[at:at + 4]!r}')
    disc = Disc(a.ps2_iso)
    try: archives = {n: disc.file(f'DATA/TEXTURES/{n}.SSH') for n in ('PARTICLE', 'EFFECTS')}
    finally: disc.close()
    out = a.out; out.mkdir(parents=True, exist_ok=True)
    textures = {}
    for i, n in FX.items():
        found = None
        for source, container in archives.items():
            try: info, raw = decode(container, n)
            except ValueError as e:
                if 'Missing original texture' in str(e): continue
                raise
            found = source; break
        if found is None:
            if n in REQUIRED: raise ValueError(f'FX {i} {n} not in PARTICLE/EFFECTS.SSH')
            continue  # the courses never select it
        (out / f'{n}.gs.rgba').write_bytes(raw)
        textures[n] = dict(fx_index=i, width=info['width'], height=info['height'], file=f'{n}.gs.rgba', encoding=info['encoding'],
                           source=f'PS2 DATA/TEXTURES/{found}.SSH', source_offset=info['source_offset'],
                           source_sha256=hashlib.sha256(archives[found]).hexdigest(), sha256=hashlib.sha256(raw).hexdigest(), alpha='raw GS byte (128 = 1.0)')
    (out / 'fx.json').write_text(json.dumps(dict(version=1, textures=textures), indent=1) + '\n')
    print(json.dumps({n: [t['width'], t['height']] for n, t in textures.items()}))


if __name__ == '__main__':
    main()
