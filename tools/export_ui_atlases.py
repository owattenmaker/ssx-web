#!/usr/bin/env python3
"""The front-end/HUD atlases and bitmap fonts as PNGs (web/public/assets/UI/<NAME>-<i>.png + <NAME>.json).

    python3 tools/export_ui_atlases.py [--source local/browser-ui] [--out web/public/assets/UI]

Reads the SHPS packs web/prepare-ui.py copies from the disc into local/browser-ui (DATA/UI/FE_1, OV_1, SU_1.SSH,
DATA/FONTS/MENU.SSH, and FEFONT/HUDFONT.SSH, the SHPS wrapped around the SFN fonts' image). This replaces the .NET
export (tools/sam_ps2 --export-browser-ui, GlitcherOG's SSX-Library OldShapeHandler) with the same decoding in Python:

* shape records per image: 1 = 4-bit, 2 = 8-bit (indices), 5 = 32-bit RGBA, 33 = palette, 112 = long name; flag 0x2000
  on the image record = GS-swizzled indices (PSMT8 / PSMT4 page layout), on the palette = CSM1 CLUT order;
* palette alpha: when no palette entry exceeds 0x80 (the PS2 unity), every alpha is doubled (min 255): OldShapeHandler's
  AlphaFix; otherwise the 32-bit/unfixed alpha is doubled here (A = min(255, 2 x raw), docs/visual-parity.md section 6);
* fonts: A = max(R, G, B) (the glyph mask is the luminance).

The PNGs carry the same pixels as the .NET export (checked texel for texel); the encoder differs, so the files do not.
SPDX-License-Identifier: GPL-3.0
"""
import argparse
import json
import os
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NAMES = ('FE_1', 'OV_1', 'SU_1', 'MENU', 'FEFONT', 'HUDFONT')


def unswizzle8(buf, w, h):
    out = bytearray(w * h)
    for y in range(h):
        block_row = (y & ~0xF) * w
        swap = (((y + 2) >> 2) & 1) * 4
        pos_y = (((y & ~3) >> 1) + (y & 1)) & 7
        col_row = pos_y * w * 2
        byte_y = (y >> 1) & 1
        for x in range(w):
            i = block_row + (x & ~0xF) * 2 + col_row + ((x + swap) & 7) * 4 + byte_y + ((x >> 2) & 2)
            if i < len(buf):
                out[y * w + x] = buf[i]
    return bytes(out)


def unswizzle4(buf, w, h):
    """4-bit indices out of the GS PSMT4 page layout (SSX-Library ByteUtil.Unswizzle4bpp), one index per byte."""
    out = bytearray(w * h)
    pages_h, pages_v = (w + 127) // 128, (h + 127) // 128
    for y in range(h):
        for x in range(w):
            page = ((y & ~0x7F) // 128) * pages_h + ((x & ~0x7F) // 128)
            page_loc = (page // pages_v) * 32 * h * 2 + (page % pages_v) * 64 * 4
            lx, ly = x & 0x7F, y & 0x7F
            block = ((lx & ~0x1F) >> 1) * h + (ly & ~0xF) * 2
            swap = (((y + 2) >> 2) & 1) * 4
            pos_y = (((y & ~3) >> 1) + (y & 1)) & 7
            column = pos_y * h * 2 + ((x + swap) & 7) * 4
            pos = page_loc + block + column + ((x >> 3) & 3)
            if pos < len(buf):
                shift = ((y >> 1) & 1) * 4
                out[y * w + x] = (buf[pos] >> shift) & 0xF
    return bytes(out)


def shape_images(data):
    """SHPS images as dicts {name, width, height, rgba, alpha_fix} (OldShapeHandler.LoadShape, PS2 formats)."""
    if data[:4] != b'SHPS':
        raise ValueError('not a SHPS pack')
    count = struct.unpack_from('<I', data, 8)[0]
    entries = [(data[16 + 8 * i:20 + 8 * i].decode('latin1'), struct.unpack_from('<I', data, 20 + 8 * i)[0]) for i in range(count)]
    images = []
    for i, (name, start) in enumerate(entries):
        end = entries[i + 1][1] if i + 1 < count else len(data)
        erts = data.find(b'Buy ERTS', start, end)
        end = erts if erts >= 0 else end
        headers, at = [], start
        while at < end:
            if data[at:at + 8] == b'Buy ERTS':
                break
            kind = data[at]
            if kind == 112:                      # long name: 3 bytes, a NUL-terminated string, aligned to 16 from the image
                stop = data.index(b'\0', at + 4)
                at = start + ((stop + 1 - start + 15) & ~15)
                continue
            if kind == 0 or at + 16 > end:        # alignment padding / the end of the wrapped font image
                break
            size = int.from_bytes(data[at + 1:at + 4], 'little')
            w, h, _, _ = struct.unpack_from('<4h', data, at + 4)
            flags = struct.unpack_from('<i', data, at + 12)[0]
            n = size - 16 if size else w * h * (4 if kind in (33, 5) else 1)
            headers.append(dict(kind=kind, size=size, w=w, h=h, flags=flags, matrix=data[at + 16:at + 16 + n]))
            at += 16 + n
        image = next((x for x in headers if x['kind'] in (1, 2, 5)), headers[0])
        w, h = image['w'], image['h']
        alpha_fix = False
        if image['kind'] in (1, 2):
            pal = next(x for x in headers if x['kind'] == 33)
            n = (pal['size'] - 16) // 4 if pal['size'] else pal['w'] * pal['h']
            raw = pal['matrix'][:4 * n]
            if pal['flags'] & 0x2000:
                unswz = bytearray(4 * n)
                for p in range(n):
                    q = (p & 0xE7) | ((p & 8) << 1) | ((p & 0x10) >> 1)
                    if q < n:
                        unswz[4 * p:4 * p + 4] = raw[4 * q:4 * q + 4]
                raw = bytes(unswz)
            colours = [list(raw[4 * k:4 * k + 4]) for k in range(n)]
            if all(c[3] <= 0x80 for c in colours):
                alpha_fix = True
                for c in colours:
                    c[3] = min(255, c[3] * 2)
            if image['kind'] == 2:
                idx = image['matrix']
                if image['flags'] & 0x2000:
                    idx = unswizzle8(idx, w, h)
            else:
                m = image['matrix']
                if image['flags'] & 0x2000:
                    idx = unswizzle4(m, w, h)
                else:
                    idx = bytes(v for b in m for v in (b & 0xF, b >> 4))
            rgba = b''.join(bytes(colours[k]) for k in idx[:w * h])
        elif image['kind'] == 5:
            rgba = bytes(image['matrix'][:w * h * 4])
        else:
            raise ValueError(f'{name}: unsupported SHPS image kind {image["kind"]}')
        images.append(dict(name=name, width=w, height=h, rgba=rgba, alpha_fix=alpha_fix))
    return images


def png(w, h, rgba):
    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)
    raw = b''.join(b'\0' + rgba[y * w * 4:(y + 1) * w * 4] for y in range(h))
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>2I5B', w, h, 8, 6, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def export(source, out):
    out.mkdir(parents=True, exist_ok=True)
    written = 0
    for name in NAMES:
        path = source / f'{name}.SSH'
        font = 'FONT' in name
        rows = []
        for i, im in enumerate(shape_images(path.read_bytes())):
            px = bytearray(im['rgba'])
            for k in range(0, len(px), 4):
                if font:
                    px[k + 3] = max(px[k], px[k + 1], px[k + 2])
                elif not im['alpha_fix']:
                    px[k + 3] = min(255, px[k + 3] * 2)
            file = f'{name}-{i}.png'
            target = out / file
            tmp = target.with_name(file + '.tmp')
            tmp.write_bytes(png(im['width'], im['height'], bytes(px)))
            os.replace(tmp, target)
            rows.append(dict(index=i, name=im['name'], file=file, width=im['width'], height=im['height']))
            written += 1
        (out / f'{name}.json').write_text(json.dumps(rows, separators=(',', ':'), ensure_ascii=False))
    print(f'UI atlases: {written} images from {len(NAMES)} packs -> {out}')


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--source', type=Path, default=ROOT / 'local/browser-ui')
    ap.add_argument('--out', type=Path, default=ROOT / 'web/public/assets/UI')
    a = ap.parse_args()
    export(a.source, a.out)


if __name__ == '__main__':
    main()
