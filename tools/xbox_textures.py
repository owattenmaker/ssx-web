#!/usr/bin/env python3
"""Xbox SSX 3 texture decoding (EA SHPX shapes and the Xbox world stream), for the texture comparison against the PS2
data (docs/xbox-textures.md). Offline only: nothing here ships in the browser.

SHPX container (little endian, same framing as the PS2 SHPS and GameCube SHPG):
    0   'SHPX'
    4   u32 total size
    8   u32 entry count N
    12  4 bytes directory id ('G357', 'G264', ...)
    16  N x {4-char name, u32 offset of the entry's first block}
        (then a 'Buy ERTS' filler, padded)
    entry = a chain of blocks, each with a 16-byte header:
        +0  u8 code          image codes below; 0x70 = long name (C string), 0x6F / 0x69 = other metadata
        +1  u24 next         bytes to the next block of the entry (0 = last block)
        +4  u16 width, +6 u16 height
        +8  u16 x2 centre, +12 u16 x2 position (the PS2 keeps its mip count in +15 >> 4; the Xbox shapes carry none)
        +16 texels

Image codes seen on the Xbox disc (code & 0x80 = RefPack-compressed texels):
    0x60  DXT1 (BC1: 4 bits a texel, 1-bit alpha when c0 <= c1)
    0x61  DXT3 (BC2: explicit 4-bit alpha)
    0x62  DXT5 (BC3: interpolated 8-bit alpha)
    0x7D  32-bit B8G8R8A8, linear rows (not NV2A-swizzled: checked on dot.xsh, a radial falloff), alpha 0..255
    0x78  16-bit R5G6B5, linear (only the debug dbgirad.xsh)
The PS2 SHPS instead has 1/2 = 4/8-bit paletted (GS PSMT4/PSMT8 swizzle, CLUT 0x21 or 0x2A block) and 5 = 32-bit, with
GS alpha 0..128 (128 = opaque).

World stream (BAM.BIG -> bam.xsb, the PS2 bam.ssb's CBXS/CEND RefPack framing and 8-byte record headers): a kind-9
texture record is a 128-byte header (+0 code 0x60/0x61, +4 u16 width, +6 u16 height, +14 u16 >> 12 = the number of
extra mip levels) followed by the DXT data of level 0 and its mips. bam.xdb declares the same 788 world textures as
the PS2 bam.sdb (+0x2A), every id at the same resolution as the PS2 one; light pages 455 (PS2 623).

    python3 tools/xbox_textures.py FILE.xsh [OUTDIR]    # list the entries (and write PNGs to OUTDIR)

SPDX-License-Identifier: GPL-3.0-only
"""
import struct
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from world_assets import refpack  # noqa: E402

CODES = {0x60: 'DXT1', 0x61: 'DXT3', 0x62: 'DXT5', 0x7D: 'BGRA8888', 0x78: 'RGB565'}
BLOCK_BYTES = {0x60: 8, 0x61: 16, 0x62: 16}


def _565(c):
    r, g, b = (c >> 11) & 31, (c >> 5) & 63, c & 31
    return (r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)


def _colour_block(data, at, force4):
    c0, c1, bits = struct.unpack_from('<HHI', data, at)
    a, b = _565(c0), _565(c1)
    if c0 > c1 or force4:
        pal = (a + (255,), b + (255,),
               tuple((2 * a[k] + b[k]) // 3 for k in range(3)) + (255,),
               tuple((a[k] + 2 * b[k]) // 3 for k in range(3)) + (255,))
    else:
        pal = (a + (255,), b + (255,), tuple((a[k] + b[k]) // 2 for k in range(3)) + (255,), (0, 0, 0, 0))
    return [pal[(bits >> (2 * i)) & 3] for i in range(16)]


def _dxt5_alpha(data, at):
    a0, a1 = data[at], data[at + 1]
    bits = int.from_bytes(data[at + 2:at + 8], 'little')
    if a0 > a1:
        table = [a0, a1] + [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
    else:
        table = [a0, a1] + [((5 - i) * a0 + i * a1) // 5 for i in range(1, 5)] + [0, 255]
    return [table[(bits >> (3 * i)) & 7] for i in range(16)]


def _dxt3_alpha(data, at):
    bits = int.from_bytes(data[at:at + 8], 'little')
    return [((bits >> (4 * i)) & 15) * 17 for i in range(16)]


def decode_dxt(code, data, w, h, at=0):
    """BC1/2/3 blocks (row-major blocks, D3D layout) -> RGBA bytes."""
    out = bytearray(w * h * 4)
    step = BLOCK_BYTES[code]
    bw, bh = max(1, (w + 3) // 4), max(1, (h + 3) // 4)
    if at + bw * bh * step > len(data):
        raise ValueError('Truncated DXT data')
    for by in range(bh):
        for bx in range(bw):
            p = at + (by * bw + bx) * step
            if code == 0x60:
                texels = _colour_block(data, p, False)
            else:
                alpha = _dxt3_alpha(data, p) if code == 0x61 else _dxt5_alpha(data, p)
                texels = [c[:3] + (alpha[i],) for i, c in enumerate(_colour_block(data, p + 8, True))]
            for i, t in enumerate(texels):
                x, y = bx * 4 + (i & 3), by * 4 + (i >> 2)
                if x < w and y < h:
                    o = (y * w + x) * 4
                    out[o:o + 4] = bytes(t)
    return bytes(out)


def decode_dxt_fast(code, data, w, h, at=0):
    """decode_dxt with numpy when it is installed (same integer rules, same bytes), else decode_dxt."""
    try:
        import numpy as np
    except ImportError:
        return decode_dxt(code, data, w, h, at)
    step = BLOCK_BYTES[code]
    bw, bh = max(1, (w + 3) // 4), max(1, (h + 3) // 4)
    n = bw * bh
    raw = np.frombuffer(data, np.uint8, n * step, at).reshape(n, step)
    col = raw[:, step - 8:]
    c0 = col[:, 0].astype(np.int32) | (col[:, 1].astype(np.int32) << 8)
    c1 = col[:, 2].astype(np.int32) | (col[:, 3].astype(np.int32) << 8)
    bits = col[:, 4:8].copy().view('<u4')[:, 0].astype(np.int64)

    def e565(c):
        r, g, b = (c >> 11) & 31, (c >> 5) & 63, c & 31
        return np.stack([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)], -1)
    a, b = e565(c0), e565(c1)
    four = (c0 > c1) | (code != 0x60)
    p2 = np.where(four[:, None], (2 * a + b) // 3, (a + b) // 2)
    p3 = np.where(four[:, None], (a + 2 * b) // 3, 0)
    pal = np.stack([a, b, p2, p3], 1)
    idx = (bits[:, None] >> (2 * np.arange(16))) & 3
    rgb = np.take_along_axis(pal, idx[:, :, None].repeat(3, 2), 1)
    alpha = np.where(idx == 3, np.where(four, 255, 0)[:, None], 255)
    if code == 0x61:
        ab = raw[:, :8].copy().view('<u8')[:, 0]
        alpha = ((ab[:, None] >> (4 * np.arange(16, dtype=np.uint64))) & 15).astype(np.int64) * 17
    elif code == 0x62:
        return decode_dxt(code, data, w, h, at)
    out = np.concatenate([rgb, alpha[:, :, None]], 2).astype(np.uint8)
    out = out.reshape(bh, bw, 4, 4, 4).transpose(0, 2, 1, 3, 4).reshape(bh * 4, bw * 4, 4)
    return out[:h, :w].tobytes()


def _morton(w, h):
    """Xbox (NV2A) swizzle: texel (x, y) -> index; x and y bits interleaved (x first) while both remain."""
    order = [0] * (w * h)
    for y in range(h):
        for x in range(w):
            i = o = 0
            bit = 1
            xs, ys = x, y
            ww, hh = w, h
            while ww > 1 or hh > 1:
                if ww > 1:
                    i |= (xs & 1) * bit
                    bit <<= 1
                    xs >>= 1
                    ww >>= 1
                if hh > 1:
                    i |= (ys & 1) * bit
                    bit <<= 1
                    ys >>= 1
                    hh >>= 1
            order[y * w + x] = i
    return order


def decode_linear(code, data, w, h, at=0, swizzled=False):
    texel = 4 if code == 0x7D else 2
    if at + w * h * texel > len(data):
        raise ValueError('Truncated texels')
    order = _morton(w, h) if swizzled else range(w * h)
    out = bytearray(w * h * 4)
    for dst, src in enumerate(order):
        p = at + src * texel
        if code == 0x7D:
            b, g, r, a = data[p:p + 4]
        else:
            r, g, b = _565(struct.unpack_from('<H', data, p)[0])
            a = 255
        out[dst * 4:dst * 4 + 4] = bytes((r, g, b, a))
    return bytes(out)


def blocks(data, offset):
    """Blocks of the SHPX entry at offset: [(code, block offset, width, height, header words, next)]."""
    out, p = [], offset
    while True:
        code = data[p]
        nxt = int.from_bytes(data[p + 1:p + 4], 'little')
        w, h = struct.unpack_from('<HH', data, p + 4)
        out.append((code, p, w, h, struct.unpack_from('<4H', data, p + 8), nxt))
        if not nxt:
            return out
        p += nxt


def shpx_entries(data):
    """-> [{name, long_name, code, format, width, height, texels offset, size}] for an SHPX container."""
    if data[:4] != b'SHPX':
        raise ValueError('Expected an SHPX container')
    count, = struct.unpack_from('<I', data, 8)
    starts = [(data[16 + 8 * i:20 + 8 * i].decode('latin-1'), struct.unpack_from('<I', data, 20 + 8 * i)[0]) for i in range(count)]
    out = []
    for i, (name, off) in enumerate(starts):
        chain = blocks(data, off)
        image = chain[0]
        long_name = None
        for code, p, *_ in chain[1:]:
            if code == 0x70:
                long_name = data[p + 4:data.index(b'\0', p + 4)].decode('latin-1')
        end = off + image[5] if image[5] else (starts[i + 1][1] if i + 1 < len(starts) else len(data))
        out.append(dict(name=name, long_name=long_name, code=image[0], format=CODES.get(image[0] & 0x7F, hex(image[0])),
                        width=image[2], height=image[3], at=image[1] + 16, end=end))
    return out


def entry_rgba(data, e):
    code, w, h = e['code'], e['width'], e['height']
    texels, at = data, e['at']
    if code & 0x80:
        texels, at, code = refpack(data[at:e['end']]), 0, code & 0x7F
    if code in BLOCK_BYTES:
        return decode_dxt(code, texels, w, h, at)
    if code in (0x7D, 0x78):
        return decode_linear(code, texels, w, h, at)
    raise ValueError(f'Unsupported Xbox shape code 0x{code:02X}')


def world_texture(record):
    """bam.xsb kind-9 record -> (width, height, code, mip levels, level-0 RGBA)."""
    code = record[0]
    w, h = struct.unpack_from('<HH', record, 4)
    mips = struct.unpack_from('<H', record, 14)[0] >> 12
    if code not in BLOCK_BYTES:
        raise ValueError(f'Unexpected world texture code 0x{code:02X}')
    return w, h, code, mips, decode_dxt(code, record, w, h, 128)


def unpack_member(data):
    """BIG members may be RefPack-compressed shapes."""
    if len(data) > 2 and data[1] == 0xFB and data[0] & 0x7E == 0x10:
        return refpack(data)
    return data


def png(w, h, rgba):
    raw = b''.join(b'\0' + rgba[y * w * 4:(y + 1) * w * 4] for y in range(h))

    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body) & 0xFFFFFFFF)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>2I5B', w, h, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))


if __name__ == '__main__':
    data = unpack_member(Path(sys.argv[1]).read_bytes())
    outdir = Path(sys.argv[2]) if len(sys.argv) > 2 else None
    for i, e in enumerate(shpx_entries(data)):
        print(f"{i:3d} {e['name']:4s} {e['format']:9s} {e['width']:4d}x{e['height']:<4d} {e['long_name'] or ''}")
        if outdir:
            outdir.mkdir(parents=True, exist_ok=True)
            (outdir / f"{i:03d}_{e['name'].strip()}.png").write_bytes(png(e['width'], e['height'], entry_rgba(data, e)))
