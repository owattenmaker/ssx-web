#!/usr/bin/env python3
"""Check the user's PS2 disc image and extract the files the importers read (tools/setup_from_iso.py, first step).

    python3 tools/extract_disc.py --iso "SSX 3 (USA).iso" [--image-hash]

Writes (git-ignored, only when the content differs):
  local/disc/SLUS_207.72, SYSTEM.CNF, inventory.json      the executable and the disc/BIG inventory (tools/inspect_disc.py)
  local/assets/source/ps2/bam.{ssb,sdb,phm,psm}          DATA/WORLDS/BAM.BIG members (the world stream)
  local/assets/source/ps2/irr.dat, mdlps2.big            DATA/WORLDS/IRR.DAT, DATA/CHAR/MDLPS2.BIG

The executable is the hard gate: its SHA-1 must be 77114dfd... (NTSC-U SLUS_207.72 v1.00, the one ssxdecomp/ssx3
matches). --image-hash also hashes the whole image and reports whether it is the redump dump (#6440); a different image
with the same files works as well.

SPDX-License-Identifier: GPL-3.0
"""
import argparse
import hashlib
import json
import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from inspect_disc import Disc, EXPECTED_SHA1, inspect_big, inspect_elf  # noqa: E402

REDUMP = dict(name='SSX 3 (USA) (redump #6440)', size=3005415424, sha1='667c9b2bbfef45ca3f89a55aab296701916120f6',
              md5='517f203e2c5458597f90f98103b86ead', crc32='a741bebf')
OTHER_SERIALS = {'SLES_516.97': 'PAL (Europe)', 'SLPM_550.77': 'NTSC-J (Japan)', 'SLKA_251.18': 'NTSC-K (Korea)',
                 'SLKA_905.02': 'NTSC-K (Korea)'}
FILES = {'DATA/WORLDS/IRR.DAT': 'irr.dat', 'DATA/CHAR/MDLPS2.BIG': 'mdlps2.big'}


def write_if_changed(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.stat().st_size == len(data) and path.read_bytes() == data:
        return False
    tmp = path.with_name(path.name + '.tmp')
    tmp.write_bytes(data)
    os.replace(tmp, path)
    return True


def image_hash(path):
    h1, h5 = hashlib.sha1(), hashlib.md5()
    with open(path, 'rb') as f:
        while True:
            b = f.read(1 << 24)
            if not b:
                break
            h1.update(b); h5.update(b)
    return h1.hexdigest(), h5.hexdigest()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--iso', type=Path, required=True)
    ap.add_argument('--image-hash', action='store_true', help='hash the whole image and compare it with the redump dump')
    a = ap.parse_args()
    if not a.iso.is_file():
        raise SystemExit(f'extract_disc: {a.iso}: no such file')
    try:
        disc = Disc(a.iso)
    except ValueError as e:
        raise SystemExit(f'extract_disc: {a.iso} is not a PS2 DVD image ({e}); a .bin/.cue CD image, a compressed .chd/.cso '
                         'or a split dump must be converted to a plain 2048-byte-sector ISO first')
    try:
        system = disc.file('SYSTEM.CNF')
        match = re.search(rb'BOOT2\s*=\s*cdrom0:\\([^;\r\n]+)', system, re.I)
        if not match:
            raise SystemExit('extract_disc: SYSTEM.CNF names no PS2 boot executable; is this SSX 3?')
        boot = match[1].decode('ascii').replace('\\', '/')
        if boot != 'SLUS_207.72':
            region = OTHER_SERIALS.get(boot, 'another game or region')
            raise SystemExit(f'extract_disc: this disc boots {boot} ({region}). The port needs the NTSC-U release, SLUS_207.72.')
        code = disc.file(boot)
        sha1 = hashlib.sha1(code).hexdigest()
        if sha1 != EXPECTED_SHA1:
            raise SystemExit(f'extract_disc: SLUS_207.72 has SHA-1 {sha1}, expected {EXPECTED_SHA1} (v1.00, redump #6440). '
                             'A second, patched SLUS-20772 executable exists (PCSX2 CRC E850623C) whose addresses differ; '
                             'the port only supports the redump one.')
        changed = []
        report = dict(volume=disc.volume, image_size=disc.size, boot_path=boot, boot_sha1=sha1, matches_ssx3_decomp=True,
                      boot_sha256=hashlib.sha256(code).hexdigest(), elf=inspect_elf(code), files=disc.entries,
                      archives=[inspect_big(disc, e) for e in disc.entries if e['path'].endswith('.BIG')])
        for rel, data in ((f'local/disc/{Path(boot).name}', code), ('local/disc/SYSTEM.CNF', system),
                          ('local/disc/inventory.json', (json.dumps(report, indent=2) + '\n').encode())):
            if write_if_changed(ROOT / rel, data):
                changed.append(rel)
        source = ROOT / 'local/assets/source/ps2'
        bam = next(e for e in disc.entries if e['path'] == 'DATA/WORLDS/BAM.BIG')
        for e in inspect_big(disc, bam)['files']:
            if e['path'].endswith(('.ssb', '.sdb', '.phm', '.psm')):
                if write_if_changed(source / Path(e['path']).name, disc.read(e['disc_offset'], e['size'])):
                    changed.append(f'local/assets/source/ps2/{Path(e["path"]).name}')
        for name, out in FILES.items():
            if write_if_changed(source / out, disc.file(name)):
                changed.append(f'local/assets/source/ps2/{out}')
    finally:
        disc.close()
    print(f'SLUS_207.72 verified (SHA-1 {sha1}); {len(disc.entries)} disc files; wrote {len(changed)} files')
    for c in changed:
        print('  ' + c)
    if a.image_hash:
        s1, m5 = image_hash(a.iso)
        same = s1 == REDUMP['sha1']
        print(f'image SHA-1 {s1}: ' + (f'the {REDUMP["name"]} image' if same else
                                        'not the redump image (fine if it holds the same files: the executable matched)'))


if __name__ == '__main__':
    main()
