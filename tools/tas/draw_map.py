#!/usr/bin/env python3
"""Top-down map of a course for the TAS route work (docs/tas.md "Route"): the reset / AI path network (initial.json
original_reset.paths, PS2 cm, z up), guide lines and run traces (tools/tas/eval.mjs --trace, native metres: x, y up, z), drawn
into a PPM and turned into a PNG with ffmpeg. Pure Python (no imaging library on this machine).

    python3 tools/tas/draw_map.py OUT.png [--course BRA2] [--guide G.json ...] [--trace T.json ...] [--box x0,y0,x1,y1] [--size 1600]
"""
import argparse, json, math, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def path_points(p):
    x, y, z = p['origin']
    pts = [(x, y, z)]
    for dx, dy, dz, length in p['segments']:
        x, y, z = x + dx * length, y + dy * length, z + dz * length
        pts.append((x, y, z))
    return pts


def main():
    a = argparse.ArgumentParser()
    a.add_argument('out')
    a.add_argument('--course', default='BRA2')
    a.add_argument('--guide', action='append', default=[])
    a.add_argument('--trace', action='append', default=[])
    a.add_argument('--box', default=None)
    a.add_argument('--size', type=int, default=1600)
    args = a.parse_args()
    init = json.loads((ROOT / f'web/public/assets/{args.course}/initial.json').read_text())
    paths = [path_points(p) for p in init['original_reset']['paths']]
    # everything in PS2 cm (x, y) top-down
    layers = [('path', i, [(q[0], q[1]) for q in pts]) for i, pts in enumerate(paths)]
    for g in args.guide:
        pts = json.loads(Path(g).read_text())['points']
        layers.append(('guide', 0, [(p[0] * 100, -p[2] * 100) for p in pts]))
    for t in args.trace:
        rows = json.loads(Path(t).read_text())
        layers.append(('trace', 0, [(r[1] * 100, -r[3] * 100) for r in rows]))
    allpts = [p for _, _, pts in layers for p in pts]
    if args.box:
        x0, y0, x1, y1 = map(float, args.box.split(','))
    else:
        x0, x1 = min(p[0] for p in allpts), max(p[0] for p in allpts)
        y0, y1 = min(p[1] for p in allpts), max(p[1] for p in allpts)
    N = args.size
    scale = (N - 20) / max(x1 - x0, y1 - y0)
    img = bytearray([255] * (N * N * 3))

    def plot(x, y, c):
        if 0 <= x < N and 0 <= y < N:
            o = (y * N + x) * 3
            img[o:o + 3] = bytes(c)

    def line(p, q, c, w=1):
        ax, ay = int(10 + (p[0] - x0) * scale), int(N - 10 - (p[1] - y0) * scale)
        bx, by = int(10 + (q[0] - x0) * scale), int(N - 10 - (q[1] - y0) * scale)
        n = max(abs(bx - ax), abs(by - ay), 1)
        for k in range(n + 1):
            x, y = ax + (bx - ax) * k // n, ay + (by - ay) * k // n
            for dx in range(-(w // 2), w // 2 + 1):
                for dy in range(-(w // 2), w // 2 + 1):
                    plot(x + dx, y + dy, c)

    def hue(i):
        h = (i * 0.618) % 1
        r, g, b = [max(0, min(1, abs(h * 6 - k) - 1)) for k in (3, 2, 4)]
        r, g, b = 1 - r, 1 - g, 1 - b
        return (int(60 + 150 * r), int(60 + 150 * g), int(60 + 150 * b))

    for kind, i, pts in layers:
        c, w = {'path': (hue(i), 1), 'guide': ((0, 0, 255), 3), 'trace': ((230, 0, 0), 2)}[kind]
        for p, q in zip(pts, pts[1:]):
            line(p, q, c, w)
        if kind == 'path':
            # the start of a path: a small square in its colour
            for dx in range(-3, 4):
                for dy in range(-3, 4):
                    plot(int(10 + (pts[0][0] - x0) * scale) + dx, int(N - 10 - (pts[0][1] - y0) * scale) + dy, hue(i))
    ppm = Path(args.out).with_suffix('.ppm')
    ppm.write_bytes(b'P6 %d %d 255\n' % (N, N) + bytes(img))
    subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-i', str(ppm), args.out], check=True)
    ppm.unlink()
    print(json.dumps(dict(box=[x0, y0, x1, y1], scale=scale, paths=len(paths))))


if __name__ == '__main__':
    main()
