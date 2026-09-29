#!/usr/bin/env python3
"""Read the user's ISO without mounting it; extract boot code and inventory assets."""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

SECTOR = 2048
EXPECTED_SHA1 = '77114dfd1205eaccf1ccc18c5f9650097fa78bd8'


class Disc:
    def __init__(self, path):
        self.path = Path(path)
        self.size = self.path.stat().st_size
        self.stream = self.path.open('rb')
        try:
            self._load()
        except BaseException:
            self.stream.close()
            raise

    def _load(self):
        pvd = self.read(16 * SECTOR, SECTOR)
        if pvd[:7] != b'\x01CD001\x01':
            raise ValueError('Expected an ISO9660 primary volume descriptor')
        if int.from_bytes(pvd[128:130], 'little') != SECTOR:
            raise ValueError('Unsupported logical block size')
        self.volume = pvd[40:72].decode('ascii').strip()
        self.entries = []
        self._walk(pvd[156:156 + pvd[156]], '', set())

    def read(self, offset, size):
        if offset < 0 or size < 0 or offset + size > self.size:
            raise ValueError('Disc extent outside image')
        self.stream.seek(offset)
        data = self.stream.read(size)
        if len(data) != size:
            raise ValueError('Truncated disc image')
        return data

    def _walk(self, record, parent, visited):
        extent = int.from_bytes(record[2:6], 'little')
        size = int.from_bytes(record[10:14], 'little')
        if extent in visited:
            raise ValueError('Cyclic or duplicated directory extent')
        visited.add(extent)
        if size > 16 * 1024 * 1024:
            raise ValueError('Directory unexpectedly large')
        data = self.read(extent * SECTOR, size)
        pos = 0
        while pos < size:
            length = data[pos]
            if not length:
                pos = (pos // SECTOR + 1) * SECTOR
                continue
            if length < 34 or pos + length > size:
                raise ValueError('Invalid directory record')
            rec = data[pos:pos + length]
            pos += length
            raw = rec[33:33 + rec[32]]
            if raw in (b'\x00', b'\x01'):
                continue
            name = raw.decode('ascii').split(';')[0]
            if name in ('.', '..') or '/' in name or '\\' in name:
                raise ValueError('Unsafe disc filename')
            path = f'{parent}/{name}'.lstrip('/')
            if rec[25] & 0x80:
                raise ValueError('Multi-extent files are not supported')
            if rec[25] & 2:
                self._walk(rec, path, visited)
            else:
                offset = int.from_bytes(rec[2:6], 'little') * SECTOR
                length = int.from_bytes(rec[10:14], 'little')
                if offset + length > self.size:
                    raise ValueError('File extent outside image')
                self.entries.append(dict(path=path, offset=offset, size=length))

    def file(self, name):
        entry = next(e for e in self.entries if e['path'].upper() == name.upper())
        return self.read(entry['offset'], entry['size'])

    def close(self):
        self.stream.close()


def inspect_big(disc, entry):
    """Index BIGF/BIG4 records directly from the disc without copying payloads."""
    header = disc.read(entry['offset'], 16)
    if header[:4] not in (b'BIGF', b'BIG4'):
        raise ValueError(f'Unsupported archive: {entry["path"]}')
    count, end = struct.unpack_from('>II', header, 8)
    if not 16 <= end <= min(entry['size'], 16 * 1024 * 1024):
        raise ValueError('Invalid BIG directory size')
    directory = disc.read(entry['offset'], end)
    pos = 16
    files = []
    for _ in range(count):
        if pos + 8 > end:
            raise ValueError('Truncated BIG directory')
        offset, size = struct.unpack_from('>II', directory, pos)
        stop = directory.find(b'\0', pos + 8)
        if stop < 0 or (size and offset < end) or offset + size > entry['size']:
            raise ValueError('Invalid BIG record')
        name = directory[pos + 8:stop].decode('ascii')
        files.append(dict(path=name, archive_offset=offset, size=size,
                          disc_offset=entry['offset'] + offset))
        pos = stop + 1
    return dict(path=entry['path'], format=header[:4].decode(), files=files)


def inspect_elf(data):
    if data[:7] != b'\x7fELF\x01\x01\x01':
        raise ValueError('Expected a 32-bit little-endian ELF')
    h = struct.unpack_from('<HHIIIIIHHHHHH', data, 16)
    kind, machine, version, entry, phoff, shoff, flags, ehsize, phsize, phnum, shsize, shnum, shstr = h
    if machine != 8 or shsize != 40 or phsize != 32:
        raise ValueError('Unexpected PS2 ELF layout')
    sections = [struct.unpack_from('<10I', data, shoff + i * shsize) for i in range(shnum)]
    names_section = sections[shstr]
    names = data[names_section[4]:names_section[4] + names_section[5]]
    output = []
    for s in sections:
        name = names[s[0]:].split(b'\0', 1)[0].decode('ascii', errors='replace')
        output.append(dict(name=name, type=s[1], flags=s[2], address=s[3], offset=s[4], size=s[5]))
    loads = []
    for i in range(phnum):
        p = struct.unpack_from('<8I', data, phoff + i * phsize)
        if p[0] == 1:
            loads.append(dict(offset=p[1], address=p[2], file_size=p[4], memory_size=p[5], flags=p[6]))
    return dict(machine='MIPS R5900 / Emotion Engine', entry=entry, flags=flags,
                sections=output, load_segments=loads,
                has_symbol_table=any(s[1] == 2 for s in sections))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('iso', type=Path)
    parser.add_argument('--output', type=Path, default=Path('local/disc'))
    args = parser.parse_args()
    disc = Disc(args.iso)
    try:
        system = disc.file('SYSTEM.CNF')
        match = re.search(rb'BOOT2\s*=\s*cdrom0:\\([^;\r\n]+)', system, re.I)
        if not match:
            raise ValueError('Missing PS2 BOOT2 executable')
        boot = match[1].decode('ascii').replace('\\', '/')
        code = disc.file(boot)
        sha1 = hashlib.sha1(code).hexdigest()
        report = dict(volume=disc.volume, image_size=disc.size, boot_path=boot,
                      boot_sha1=sha1, matches_ssx3_decomp=sha1 == EXPECTED_SHA1,
                      boot_sha256=hashlib.sha256(code).hexdigest(),
                      elf=inspect_elf(code), files=disc.entries,
                      archives=[inspect_big(disc, e) for e in disc.entries
                                if e['path'].endswith('.BIG')])
        args.output.mkdir(parents=True, exist_ok=True)
        (args.output / Path(boot).name).write_bytes(code)
        (args.output / 'SYSTEM.CNF').write_bytes(system)
        (args.output / 'inventory.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps({k: v for k, v in report.items() if k not in ('files', 'elf', 'archives')}, indent=2))
        print(f'{len(disc.entries)} files; entry 0x{report["elf"]["entry"]:08x}; symbols: {report["elf"]["has_symbol_table"]}')
        print(f'{len(report["archives"])} BIG archives; {sum(len(a["files"]) for a in report["archives"])} archive members')
        print(f'Extracted boot executable and inventory into {args.output}')
    finally:
        disc.close()


if __name__ == '__main__':
    main()
