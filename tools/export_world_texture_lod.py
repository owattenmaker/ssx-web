#!/usr/bin/env python3
"""The PS2 mip LOD of every world texture (docs/visual-parity.md section 28): TEXTURES/world-lod.json.

A world texture (SSB kind-9 record, the world texture library's id) is bound through a "strm_tex" descriptor whose TEX1
the streamer fills when the texture arrives (0x37CA30):
  - header +0xC bits 28..31 (byte 15 >> 4) = the extra mip levels (MXL); none: no LOD K is set;
  - header +0x8 (s16) v: 0 keeps the descriptor's default K -185 (0x367880); else
      K = trunc(-(ln(240 / (v * 2^-14)) * (1 / ln 2)) * 16), clamped to -2047 .. -135 (x 1/16),
    i.e. level = log2(1/Q) + K = log2(w * (v / 16384) / 240): v / 16384 is the texture's texels per cm on the ground, 240
    the focal constant, so level 0 is where one texel covers one pixel of a surface facing the camera;
  - TEX1 = LCM 0 (level from Q only, never from the surface slope), MXL, MMAG linear, MMIN linear-mipmap-linear, L 0.
Checked against the live descriptors of 8 PS2 states (291 streamed textures: every K equal).

Output: {version: 1, source, focal: 240, textures: {rid: [K (x 1/16, int), MXL]}} for every kind-9 id with MXL > 0.

usage: export_world_texture_lod.py [--out web/public/assets/TEXTURES/world-lod.json]
"""
import argparse, json, math, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from world_assets import records, world_chunks  # noqa: E402

DEFAULT_K = -185   # 0x367880: a mipmapped descriptor's K before (or without) the streamer's own


def lod_k(v):
    """0x37CA30: the streamer's K (x 1/16) for the header's s16 +8 (0: the descriptor keeps DEFAULT_K)."""
    if v == 0: return DEFAULT_K
    x = struct.unpack('<f', struct.pack('<f', v * 2.0 ** -14))[0]              # cvt.s.w, mul.s 2^-14 (0x38800000)
    q = struct.unpack('<f', struct.pack('<f', 240.0 / x))[0]                   # div.s 240.0 (0x43700000)
    k = math.trunc((0.0 - math.log(q) * (1 / math.log(2))) * 16.0)             # log (double), x 1/ln2, 0 - x, x 16, to int
    return max(-0x7FF, min(k, -135))


def export(ssb=ROOT / 'local/assets/source/ps2/bam.ssb'):
    out = {}
    for chunk in world_chunks(ssb):
        for kind, _track, rid, data in records(chunk):
            if kind != 9 or len(data) < 16: continue
            mxl = data[15] >> 4
            if not mxl: continue
            entry = [lod_k(struct.unpack_from('<h', data, 8)[0]), mxl]
            if out.setdefault(str(rid), entry) != entry: raise ValueError(f'texture {rid}: two LOD headers')
    return {'version': 1, 'source': 'bam.ssb kind-9 headers (+8 s16, +15 >> 4)', 'focal': 240,
            'textures': dict(sorted(out.items(), key=lambda kv: int(kv[0])))}


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', default=str(ROOT / 'web/public/assets/TEXTURES/world-lod.json'))
    a = ap.parse_args()
    doc = export()
    Path(a.out).write_text(json.dumps(doc, separators=(',', ':')) + '\n')
    print(a.out, len(doc['textures']), 'textures')


if __name__ == '__main__':
    main()
