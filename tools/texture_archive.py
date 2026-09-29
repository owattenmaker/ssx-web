#!/usr/bin/env python3
"""Browser texture archives (docs/asset-formats.md "Texture archives"): many textures in one file, one request.

Used for the shared world texture library (web/public/assets/TEXTURES/world.tex, tools/export_world_textures.py),
each package's GameCube lightmaps (<package>/lightmaps.tex) and each rider's wardrobe (WARDROBE/<ID>/textures.tex,
tools/export_wardrobe.py). The browser side is web/texture-archive.js.

Layout (little endian):
    0   8 bytes  magic b'SSXTEX01'
    8   u32      index length N
    12  N bytes  index, UTF-8 JSON: {version: 1, kind, format: 'png', entries: [{id, offset, size, width, height,
                 colours, rgba}], ...extra}; offset is relative to the payload; rgba = first 16 hex digits of the
                 SHA-256 of the decoded RGBA texels (verification)
    12+N, padded with zeros to a multiple of 16: payload = the entries' PNG files back to back

Payload: lossless PNG. Every texture of the PS2/GameCube data has <= 256 distinct RGBA values (paletted SHAPE records
and CMPR blocks), so it is stored as an indexed PNG (PLTE + tRNS, 1/2/4/8 bits per index, zlib level 9); anything with
more colours falls back to 8-bit RGBA. Both decode to exactly the RGBA texels of the source (checked on write by the
reference decoder below). Measured on the 777 world textures exported before the archive (2026-09-25): per-location
RGBA PNGs 7.55 MB, RGBA PNG level 9 6.37 MB, raw RGBA 41.0 MB (gzip -9 6.36 MB, brotli 4.87 MB), indexed PNG 4.74 MB.

SPDX-License-Identifier: GPL-3.0-only
"""
import hashlib
import json
import os
import struct
import zlib
from pathlib import Path

MAGIC = b'SSXTEX01'
PNG_SIGNATURE = b'\x89PNG\r\n\x1a\n'


def rgba_digest(rgba):
    return hashlib.sha256(rgba).hexdigest()[:16]


def _chunk(kind, data):
    return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data) & 0xffffffff)


def png_bytes(width, height, rgba):
    """Lossless PNG of RGBA texels: indexed when <= 256 distinct colours, else 8-bit RGBA. Returns (png, colours)."""
    if len(rgba) != width * height * 4:
        raise ValueError('RGBA size does not match the dimensions')
    words = memoryview(rgba).cast('I')
    palette, index = {}, bytearray(width * height)
    for i, word in enumerate(words):
        j = palette.get(word)
        if j is None:
            j = len(palette)
            if j == 256:
                palette = None
                break
            palette[word] = j
        index[i] = j
    ihdr = lambda depth, colour: struct.pack('>2I5B', width, height, depth, colour, 0, 0, 0)
    if palette is None:
        stride = width * 4
        raw = b''.join(b'\0' + rgba[y * stride:(y + 1) * stride] for y in range(height))
        png = PNG_SIGNATURE + _chunk(b'IHDR', ihdr(8, 6)) + _chunk(b'IDAT', zlib.compress(raw, 9)) + _chunk(b'IEND', b'')
        return png, len(set(words))
    count = len(palette)
    bits = 1 if count <= 2 else 2 if count <= 4 else 4 if count <= 16 else 8
    stride = (width * bits + 7) // 8
    raw = bytearray(height * (stride + 1))
    per = 8 // bits
    for y in range(height):
        row = index[y * width:(y + 1) * width]
        at = y * (stride + 1) + 1
        if bits == 8:
            raw[at:at + width] = row
            continue
        for x in range(0, width, per):
            v = 0
            for k in range(per):
                v = (v << bits) | (row[x + k] if x + k < width else 0)
            raw[at + x // per] = v
    colours = [0] * count
    for word, j in palette.items():
        colours[j] = word
    plte = b''.join(struct.pack('<I', c)[:3] for c in colours)
    trns = bytes(c >> 24 for c in colours).rstrip(b'\xff')
    png = PNG_SIGNATURE + _chunk(b'IHDR', ihdr(bits, 3)) + _chunk(b'PLTE', plte)
    if trns:
        png += _chunk(b'tRNS', trns)
    return png + _chunk(b'IDAT', zlib.compress(bytes(raw), 9)) + _chunk(b'IEND', b''), count


def decode_png(data):
    """Reference PNG decoder (colour types 2/3/6, 8-bit RGB(A) or 1..8-bit indexed, filters 0..4, no interlace)."""
    if data[:8] != PNG_SIGNATURE:
        raise ValueError('Not a PNG')
    at, idat, plte, trns = 8, [], b'', b''
    while at < len(data):
        n, kind = struct.unpack_from('>I4s', data, at)
        body = data[at + 8:at + 8 + n]
        at += 12 + n
        if kind == b'IHDR':
            width, height, depth, colour, _, _, interlace = struct.unpack('>2I5B', body)
            if interlace:
                raise ValueError('Interlaced PNG')
        elif kind == b'PLTE':
            plte = body
        elif kind == b'tRNS':
            trns = body
        elif kind == b'IDAT':
            idat.append(body)
    raw = zlib.decompress(b''.join(idat))
    channels = {2: 3, 6: 4, 3: 1}[colour]
    bpp = max(1, channels * depth // 8)
    stride = (width * channels * depth + 7) // 8
    rows, prev = [], bytearray(stride)
    for y in range(height):
        f, line = raw[y * (stride + 1)], bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride if f else 0):
            a = line[i - bpp] if i >= bpp else 0
            b, c = prev[i], prev[i - bpp] if i >= bpp else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + ((a + b) >> 1)) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[i] = (line[i] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        rows.append(line)
        prev = line
    if colour == 6:
        return width, height, b''.join(bytes(line) for line in rows)
    if colour == 2:
        return width, height, b''.join(bytes(line[x * 3:x * 3 + 3]) + b'\xff' for line in rows for x in range(width))
    colours = [plte[j * 3:j * 3 + 3] + bytes([trns[j] if j < len(trns) else 255]) for j in range(len(plte) // 3)]
    mask, out = (1 << depth) - 1, []
    for line in rows:
        if depth == 8:
            out.extend(colours[j] for j in line[:width])
        else:
            per = 8 // depth
            shifts = [8 - depth * (k + 1) for k in range(per)]
            out.extend(colours[(line[x // per] >> shifts[x % per]) & mask] for x in range(width))
    return width, height, b''.join(out)


def build_archive(entries, kind, **extra):
    """entries: [{id, width, height, rgba, ...more index fields}] -> archive bytes (entries kept in the given order).
    An entry may instead carry an already encoded archive PNG: {id, width, height, png, colours, digest} (an entry of
    an existing archive, kept as it is)."""
    index, payload = [], bytearray()
    for e in entries:
        if 'png' in e:
            png, colours, digest = e['png'], e['colours'], e['digest']
        else:
            png, colours = png_bytes(e['width'], e['height'], e['rgba'])
            if decode_png(png)[2] != e['rgba']:
                raise ValueError(f'PNG round trip differs for {e["id"]}')
            digest = rgba_digest(e['rgba'])
        row = dict(id=e['id'], offset=len(payload), size=len(png), width=e['width'], height=e['height'], colours=colours,
                   rgba=digest)
        row.update({k: v for k, v in e.items() if k not in row and k not in ('rgba', 'png', 'colours', 'digest')})
        index.append(row)
        payload += png
    head = json.dumps(dict(version=1, kind=kind, format='png', entries=index, **extra), separators=(',', ':')).encode()
    data = MAGIC + struct.pack('<I', len(head)) + head
    return data + b'\0' * (-len(data) % 16) + bytes(payload)


def write_archive(path, entries, kind, **extra):
    """Write atomically; an unchanged archive keeps its file (and mtime, so the served ETag stays the same)."""
    data = build_archive(entries, kind, **extra)
    path = Path(path)
    if path.exists() and path.read_bytes() == data:
        return read_index(data)
    path.parent.mkdir(parents=True, exist_ok=True)
    temp = path.with_name(path.name + '.tmp')
    temp.write_bytes(data)
    os.replace(temp, path)
    return read_index(data)


def read_index(data):
    if data[:8] != MAGIC:
        raise ValueError('Not a texture archive')
    n, = struct.unpack_from('<I', data, 8)
    index = json.loads(data[12:12 + n])
    start = 12 + n + (-(12 + n) % 16)
    index['payload_offset'] = start
    return index


def read_archive(path):
    """-> (index, {id: png bytes})"""
    data = Path(path).read_bytes()
    index = read_index(data)
    start = index['payload_offset']
    return index, {e['id']: data[start + e['offset']:start + e['offset'] + e['size']] for e in index['entries']}


def entry_rgba(path, ident):
    index, files = read_archive(path)
    return decode_png(files[ident])
