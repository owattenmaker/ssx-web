#!/usr/bin/env python3
"""Sam's Select Character silhouettes, made like the ten originals (docs/characters.md "Sam in the roster row").

The original row (FE.LUI 08sel_char, FE_1-20 page) is flat colour silhouettes of the riders' own models in their
front-end standing poses, about 1 px of soft edge: one white strip sprite (0x00A79FCC) and one orange (194,76,0)
highlight sprite per rider (Elise 5'11" is 19x78 on the page, Mac 5'6" 26x74), all drawn at 130% x 110% with the
group offset (-4,12) and feet on one line (y + 12 + h x 1.1 = ~364.5).

Sam's is made the same way from his model: tools/render/sam-silhouette-pose.mjs renders his front-end preview (Mac's
idle FE_GEAR_MAC_CYC at t=0, the preview camera) on a magenta and a green clear colour to
local/assets/sam-roster/pose-sam-<yaw>-{m,g}.png; this tool recovers the exact coverage from the two (a = 1 - (m - g) / 255 per
key channel) as the shape, scales it to the height of a 5'11" rider (Elise's 78), box-filters it (the soft
edge), and writes the 128x128 page web/public/assets/UI/sam-roster.png: white in x 0..63, orange in x 64..127.
It also returns the layout the Select Character screen uses (tools/export_character_select.py `sam`).
"""
import json, struct, sys, zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
POSE = ROOT / 'local/assets/sam-roster/pose-sam-0'   # -m.png / -g.png: magenta and green clear colours
OUT = ROOT / 'web/public/assets/UI/sam-roster.png'
ORANGE = (194, 76, 0)              # the originals' highlight colour (FE_1-20, every orange sprite pixel)
HEIGHT = 78                        # page pixels: Elise, the same 5'11" (tools/export_roster.py Sam: model size 96)
FEET = 364.5                       # LUI y of the originals' feet: y + 12 + h * 1.1
KAORI_RIGHT = 481 - 4 + 27 * 1.3   # Kaori's orange sprite's drawn right edge (the last original, rider state frame 85)


def read_png(path):
    data = path.read_bytes(); at = 8; idat = b''
    while at < len(data):
        n, kind = struct.unpack('>I4s', data[at:at + 8]); body = data[at + 8:at + 8 + n]; at += 12 + n
        if kind == b'IHDR': w, h, depth, ctype = struct.unpack('>IIBB', body[:10])
        elif kind == b'IDAT': idat += body
    if depth != 8 or ctype not in (2, 6): raise ValueError('8-bit RGB/RGBA PNG expected')
    bpp = 4 if ctype == 6 else 3; raw = zlib.decompress(idat); stride = w * bpp; prev = bytearray(stride); out = bytearray(); i = 0
    for _ in range(h):
        f = raw[i]; line = bytearray(raw[i + 1:i + 1 + stride]); i += 1 + stride
        for x in range(stride):
            a = line[x - bpp] if x >= bpp else 0; b = prev[x]; c = prev[x - bpp] if x >= bpp else 0
            if f == 1: line[x] = (line[x] + a) & 255
            elif f == 2: line[x] = (line[x] + b) & 255
            elif f == 3: line[x] = (line[x] + (a + b) // 2) & 255
            elif f == 4:
                p = a + b - c; pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        out += line; prev = line
    return w, h, bpp, out


def png(w, h, rgba):
    def chunk(t, b): return struct.pack('>I', len(b)) + t + b + struct.pack('>I', zlib.crc32(t + b) & 0xffffffff)
    raw = b''.join(b'\0' + rgba[y * w * 4:(y + 1) * w * 4] for y in range(h))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')


def export(pose=POSE, out=OUT):
    w, h, bpp, m = read_png(Path(f'{pose}-m.png')); w2, h2, bpp2, g = read_png(Path(f'{pose}-g.png'))
    if (w, h) != (w2, h2): raise ValueError('key renders differ in size')
    # observed = a F + (1 - a) K: magenta (255,0,255) minus green (0,255,0) = (1 - a) (255, -255, 255)
    alpha = []
    for i in range(w * h):
        r1, g1, b1 = m[i * bpp:i * bpp + 3]; r2, g2, b2 = g[i * bpp2:i * bpp2 + 3]
        alpha.append(round(255 * min(1, max(0, 1 - ((r1 - r2) + (g2 - g1) + (b1 - b2)) / 765))))
    ys = [i // w for i, a in enumerate(alpha) if a > 128]; xs = [i % w for i, a in enumerate(alpha) if a > 128]
    x0, x1, y0, y1 = min(xs), max(xs) + 1, min(ys), max(ys) + 1
    scale = HEIGHT / (y1 - y0); sw = max(1, round((x1 - x0) * scale))
    if sw > 62: raise ValueError(f'silhouette too wide for its 64-px cell: {sw}')
    # box filter: each page pixel averages the coverage of the source pixels it covers (the originals' soft edge)
    cov = []
    for v in range(HEIGHT):
        row = []
        for u in range(sw):
            sx0, sx1 = x0 + u / scale, x0 + (u + 1) / scale; sy0, sy1 = y0 + v / scale, y0 + (v + 1) / scale
            total = n = 0
            for yy in range(int(sy0), min(y1, int(sy1) + 1)):
                for xx in range(int(sx0), min(x1, int(sx1) + 1)):
                    total += alpha[yy * w + xx] / 255; n += 1
            row.append(total / max(n, 1))
        cov.append(row)
    page = bytearray(128 * 128 * 4)
    left = 1 + (62 - sw) // 2
    for v in range(HEIGHT):
        for u in range(sw):
            a = round(255 * min(1.0, cov[v][u] / 0.45))   # thin limbs stay solid; ~1 px soft edge like the originals
            for cell, rgb in ((0, (255, 255, 255)), (64, ORANGE)):
                o = ((1 + v) * 128 + cell + left + u) * 4; page[o:o + 4] = bytes((*rgb, a))
    out.write_bytes(png(128, 128, bytes(page)))
    x = round(KAORI_RIGHT - 12 + 4)                     # overlap Kaori like the originals overlap their neighbours (~10-15 px)
    y = round(FEET - 12 - HEIGHT * 1.1)
    layout = dict(screen_index=10, silhouette=[x, y, sw, HEIGHT], white_uv=[left, 1, left + sw, 1 + HEIGHT], orange_uv=[64 + left, 1, 64 + left + sw, 1 + HEIGHT],
                  right_arrow_x=round(x - 4 + sw * 1.3 + 6), source=f'{Path(pose).resolve().relative_to(ROOT)}-{{m,g}}.png (tools/render/sam-silhouette-pose.mjs) -> tools/export_sam_roster.py')
    return layout


if __name__ == '__main__':
    print(json.dumps(export(Path(sys.argv[1]) if len(sys.argv) > 1 else POSE)))
