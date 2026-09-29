#!/usr/bin/env python3
"""Extract the GameCube SSX 3 files the PS2 port still reads (tools/setup_from_iso.py --gamecube).

    python3 tools/extract_gamecube.py IMAGE

IMAGE is the user's GameCube disc (USA, GXBE69): a plain .iso/.gcm image (read here), an already extracted folder
(with sys/main.dol and files/), or any format nodtool reads (.rvz, .wia, .ciso, .nfs; nodtool on PATH, or
local/bin/nodtool from tools/bootstrap_gamecube.py). Only these subtrees are written, under local/gamecube/disc:

  sys/main.dol                     checked by SHA-256 (tools/prepare_gamecube.py EXPECTED)
  files/data/char/*                rider models, skins, textures and the item database (riders, wardrobe)
  files/data/worlds/*              bam.big (world light pages, 2 world textures), irrngc.dat
  files/data/textures/*, confman/* particle.gsh (snow flipbooks), cmrender.h

Why a PS2 port reads GameCube data: the rider/wardrobe geometry and the world light-page import were built from the
GameCube files (docs/iso-pipeline.md "GameCube"); a PS2-only path is planned.

SPDX-License-Identifier: GPL-3.0
"""
import argparse
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / 'local/gamecube/disc'
DOL_SHA256 = 'b92162d6c616be3ce46b4eb61d5ddbb49891bc387ea7ddb2fea5792842fa29ce'
KEEP = ('files/data/char/', 'files/data/worlds/', 'files/data/textures/', 'files/data/confman/')
GCM_MAGIC = 0xC2339F3D


def wanted(rel):
    return rel == 'sys/main.dol' or rel.startswith(KEEP)


def write(rel, data):
    path = DEST / rel
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists() and path.stat().st_size == len(data) and path.read_bytes() == data:
        return False
    tmp = path.with_name(path.name + '.tmp')
    tmp.write_bytes(data)
    os.replace(tmp, path)
    return True


def dol_size(header):
    end = 0x100
    for i in range(18):
        off = struct.unpack_from('>I', header, i * 4)[0]
        size = struct.unpack_from('>I', header, 0x90 + i * 4)[0]
        if size:
            end = max(end, off + size)
    return end


def from_gcm(path):
    with open(path, 'rb') as f:
        head = f.read(0x440)
        if struct.unpack_from('>I', head, 0x1C)[0] != GCM_MAGIC:
            raise SystemExit(f'extract_gamecube: {path} is not a GameCube disc image (no GCM magic); convert an .rvz with nodtool')
        if head[:6] != b'GXBE69':
            raise SystemExit(f'extract_gamecube: the disc ID is {head[:6].decode(errors="replace")}; SSX 3 USA is GXBE69')
        dol_off, fst_off, fst_size = struct.unpack_from('>III', head, 0x420)
        f.seek(dol_off); dol_head = f.read(0x100); f.seek(dol_off)
        yield 'sys/main.dol', f.read(dol_size(dol_head))
        f.seek(fst_off); fst = f.read(fst_size)
        count = struct.unpack_from('>I', fst, 8)[0]
        names = fst[count * 12:]
        dirs = [(count, '')]      # (end index, path prefix)
        for i in range(1, count):
            while dirs and i >= dirs[-1][0]:
                dirs.pop()
            flags_name, a, b = struct.unpack_from('>III', fst, i * 12)
            name = names[flags_name & 0xFFFFFF:].split(b'\0', 1)[0].decode('ascii', errors='replace')
            prefix = dirs[-1][1] if dirs else ''
            if flags_name >> 24:
                dirs.append((b, f'{prefix}{name}/'))
                continue
            rel = f'files/{prefix}{name}'
            if wanted(rel):
                f.seek(a)
                yield rel, f.read(b)


def from_folder(folder):
    for p in sorted(folder.rglob('*')):
        rel = p.relative_to(folder).as_posix()
        if p.is_file() and wanted(rel):
            yield rel, p.read_bytes()


def nodtool():
    for cand in (shutil.which('nodtool'), ROOT / 'local/bin/nodtool'):
        if cand and Path(cand).is_file() and os.access(cand, os.X_OK):
            return str(cand)
    raise SystemExit('extract_gamecube: this image format needs nodtool (https://github.com/encounter/nod): put it on PATH '
                     '(macOS arm64: python3 tools/bootstrap_gamecube.py fetches a pinned build), or convert the image to a '
                     'plain .iso first')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('image', type=Path)
    a = ap.parse_args()
    src = a.image
    tmp = None
    if src.is_dir():
        files = from_folder(src)
    else:
        with open(src, 'rb') as f:
            head = f.read(0x20)
        if len(head) == 0x20 and struct.unpack_from('>I', head, 0x1C)[0] == GCM_MAGIC:
            files = from_gcm(src)
        else:
            tmp = Path(tempfile.mkdtemp(prefix='gc-extract-', dir=ROOT / 'local'))
            info = subprocess.run([nodtool(), 'info', str(src)], capture_output=True, text=True)
            if 'GXBE69' not in info.stdout:
                shutil.rmtree(tmp)
                raise SystemExit(f'extract_gamecube: {src} is not SSX 3 USA (GXBE69):\n{info.stdout[-500:]}{info.stderr[-500:]}')
            subprocess.run([nodtool(), 'extract', '--quiet', str(src), str(tmp / 'disc')], check=True)
            files = from_folder(tmp / 'disc')
    written = n = 0
    try:
        for rel, data in files:
            if rel == 'sys/main.dol' and hashlib.sha256(data).hexdigest() != DOL_SHA256:
                raise SystemExit('extract_gamecube: main.dol is not the SSX 3 USA executable this port was built against')
            written += write(rel, data)
            n += 1
    finally:
        if tmp:
            shutil.rmtree(tmp, ignore_errors=True)
    if not (DEST / 'sys/main.dol').exists():
        raise SystemExit('extract_gamecube: no sys/main.dol in the image')
    # The GameCube world stream (light pages, two world textures), as tools/import_world.py would unpack it on demand.
    sys.path.insert(0, str(ROOT / 'tools'))
    from compare_character_assets import big_members
    source = ROOT / 'local/assets/source/gamecube'
    for name, data in big_members((DEST / 'files/data/worlds/bam.big').read_bytes()):
        if name.endswith(('.gsb', '.gdb')):
            path = source / Path(name).name
            path.parent.mkdir(parents=True, exist_ok=True)
            if not (path.exists() and path.read_bytes() == data):
                tmp = path.with_name(path.name + '.tmp'); tmp.write_bytes(data); os.replace(tmp, path); written += 1
    print(f'GameCube GXBE69: {n} files kept under {DEST.relative_to(ROOT)}, world stream in {source.relative_to(ROOT)} '
          f'({written} written)')


if __name__ == '__main__':
    main()
