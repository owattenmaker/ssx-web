#!/usr/bin/env python3
"""Read-only XDVDFS (Xbox disc / XISO) reader: list and extract files without mounting or modifying the image.

Layout (little endian, 2048-byte sectors, all sector numbers relative to the game partition):
    partition base   0 for a plain XISO; 0x18300000 for a full redump image (video partition first)
    base + 32*2048   volume descriptor: 'MICROSOFT*XBOX*MEDIA' (20 bytes), u32 root directory sector,
                     u32 root directory size, u64 FILETIME, 1992 unused bytes, the magic again (at +0x7EC)
    directory        a binary search tree of 4-byte aligned entries:
                         u16 left subtree offset (in dwords, 0 = none), u16 right subtree offset,
                         u32 start sector, u32 size, u8 attributes (0x10 = directory), u8 name length, name
                     an entry never crosses a sector; 0xFFFF fills the rest of a sector

    python3 tools/xdvdfs.py list  IMAGE [PREFIX]
    python3 tools/xdvdfs.py extract IMAGE OUTDIR [PREFIX ...]   # only files under the prefixes (all if none)

Used for the Xbox SSX 3 texture evaluation (docs/xbox-textures.md). The image is opened read-only.
"""
import argparse
import os
import struct
import sys
from pathlib import Path

SECTOR = 2048
MAGIC = b'MICROSOFT*XBOX*MEDIA'
BASES = (0, 0x18300000, 0x2080000, 0xFD90000)


class XISO:
    def __init__(self, path):
        self.path = Path(path)
        self.size = self.path.stat().st_size
        self.stream = self.path.open('rb')
        for base in BASES:
            if base + 33 * SECTOR > self.size:
                continue
            head = self.read_at(base + 32 * SECTOR, SECTOR)
            if head[:20] == MAGIC and head[0x7EC:0x7EC + 20] == MAGIC:
                self.base = base
                self.root_sector, self.root_size = struct.unpack_from('<II', head, 20)
                break
        else:
            raise ValueError('No XDVDFS volume descriptor found')

    def read_at(self, offset, size):
        if offset < 0 or size < 0 or offset + size > self.size:
            raise ValueError('Extent outside image')
        self.stream.seek(offset)
        data = self.stream.read(size)
        if len(data) != size:
            raise ValueError('Truncated image')
        return data

    def extent(self, sector, size):
        return self.read_at(self.base + sector * SECTOR, size)

    def directory(self, sector, size, parent=''):
        """-> [(path, sector, size, is_dir)] depth first, in tree order."""
        if size == 0:
            return []
        data = self.extent(sector, size)
        out, stack, seen = [], [0], set()
        while stack:
            at = stack.pop()
            if at in seen or at + 14 > len(data):
                continue
            seen.add(at)
            left, right, start, length, attrs, n = struct.unpack_from('<HHIIBB', data, at)
            if left == 0xFFFF:   # sector padding: continue at the next sector
                continue
            name = data[at + 14:at + 14 + n].decode('latin-1')
            path = f'{parent}/{name}' if parent else name
            is_dir = bool(attrs & 0x10)
            out.append((path, start, length, is_dir))
            if is_dir:
                out.extend(self.directory(start, length, path))
            if right:
                stack.append(right * 4)
            if left:
                stack.append(left * 4)
        return out

    def files(self):
        return [e for e in self.directory(self.root_sector, self.root_size) if not e[3]]

    def read_file(self, entry):
        _, start, length, _ = entry
        return self.extent(start, length)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='cmd', required=True)
    ls = sub.add_parser('list')
    ls.add_argument('image')
    ls.add_argument('prefix', nargs='?', default='')
    ex = sub.add_parser('extract')
    ex.add_argument('image')
    ex.add_argument('outdir')
    ex.add_argument('prefix', nargs='*')
    a = ap.parse_args()
    iso = XISO(a.image)
    if a.cmd == 'list':
        print(f'# partition base 0x{iso.base:X}, root sector {iso.root_sector} size {iso.root_size}')
        for path, start, length, _ in sorted(iso.files()):
            if path.lower().startswith(a.prefix.lower()):
                print(f'{length:12d}  {path}')
        return
    out = Path(a.outdir)
    total = 0
    for entry in sorted(iso.files()):
        path = entry[0]
        if a.prefix and not any(path.lower().startswith(p.lower()) for p in a.prefix):
            continue
        target = out / path
        if '..' in Path(path).parts:
            raise ValueError(f'Unsafe path {path}')
        target.parent.mkdir(parents=True, exist_ok=True)
        tmp = target.with_name(target.name + '.part')
        with tmp.open('wb') as w:
            left, sector = entry[2], entry[1]
            while left:
                n = min(left, 64 * SECTOR * 16)
                w.write(iso.extent(sector, n))
                sector += n // SECTOR
                left -= n
        os.replace(tmp, target)
        total += entry[2]
    print(f'extracted {total} bytes to {out}', file=sys.stderr)


if __name__ == '__main__':
    main()
