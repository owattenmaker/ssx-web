#!/usr/bin/env python3
"""Terrain glint ("Patch Reflection", patch layer type 6) of the world packages (docs/visual-parity.md 41.6,
docs/terrain-render-fidelity.md, web/world-material.js pv terrainGlint).

The PS2 (38CA70 / 38CE20 -> 38D168) draws an extra context-2 pass over a patch whose word +0x0C has 0x600000 (layer type 6;
the debug toggle gp+0x13E8 "Disable Patch Reflection" is 0): texture = the patch's +0x1A4 halfword (renderer +0x10A8 when
negative; no retail patch is), CLAMP, blend ALPHA_2 enum 17 = 0x58 (Cd + Cs x Ad >> 7) when +0x0C & 0x400000 (every retail
glint patch), else enum 2. The UV is the patch's unit normal through E = B(beta) A(alpha) M (38B370, terrain +0x360).

Every retail glint texture is one of two 32 x 32 images: 62 / 198 (grey, identical texels) and 297 (blue). The output keeps
them once per package, with the glint patches by terrain-render.json resource:
{version, source, textures: {grey|blue: {width, height, rgba (base64, texture_rgba: GS alpha doubled)}},
 patches: [[resource, 1 grey | 2 blue], ...]}.
Default output is a scratch folder (never web/public/assets).
"""
import argparse, base64, hashlib, json, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from world_assets import records, world_chunks, texture_rgba  # noqa: E402

SOURCE = ROOT / 'local/assets/source/ps2'
ASSETS = ROOT / 'web/public/assets'
KINDS = {'4476d46833ba': 'grey', 'ebb5410643f8': 'blue'}   # texture_rgba sha256 prefixes of the two glint images


def packages():
    for world in sorted(ASSETS.glob('*/world.json')) + sorted(ASSETS.glob('PEAK*/*/world.json')):
        if not (world.parent / 'terrain-render.json').exists(): continue
        doc = json.loads(world.read_text())
        sources = doc.get('event_locations') if world.parent.parent == ASSETS else None
        if sources is None: sources = doc.get('source') if isinstance(doc.get('source'), list) else doc.get('event_locations')
        if sources: yield world.parent, sources


def export(out_root):
    chunks = list(world_chunks(SOURCE / 'bam.ssb'))
    total = 0
    for folder, sources in packages():
        patches, textures = {}, {}
        for src in sources:
            lo, hi = src['chunks']
            for ci in range(lo, hi + 1):
                for kind, track, rid, d in records(chunks[ci]):
                    if kind == 1 and len(d) == 432: patches[(rid << 8) | track] = d
                    elif kind == 9: textures.setdefault(rid, d)
        render = json.loads((folder / 'terrain-render.json').read_text())
        rows, images = [], {}
        for p in render['patches']:
            d = patches.get(p['resource'])
            if d is None: raise SystemExit(f'{folder.name}: patch {p["resource"]} not in the SSB ranges')
            w = struct.unpack_from('<I', d, 12)[0]
            if not w & 0x600000: continue
            if not w & 0x400000: raise SystemExit(f'{folder.name}: patch {p["resource"]} glint without 0x400000 (enum 2)')
            t = struct.unpack_from('<h', d, 420)[0]
            if t < 0 or t not in textures: raise SystemExit(f'{folder.name}: patch {p["resource"]} glint texture {t}')
            tw, th, rgba = texture_rgba(textures[t]); kind = KINDS.get(hashlib.sha256(rgba).hexdigest()[:12])
            if kind is None: raise SystemExit(f'{folder.name}: glint texture {t} is neither known image')
            images[kind] = dict(width=tw, height=th, rgba=base64.b64encode(rgba).decode())
            rows.append([p['resource'], 1 if kind == 'grey' else 2])
        if not rows: continue
        rel = folder.relative_to(ASSETS)
        out = Path(out_root) / rel / 'terrain-glint.json'; out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(dict(version=1, source='SSX3 USA PS2 BAM.SSB patch +0x0C & 0x600000, +0x1A4 (tools/export_terrain_glint.py)',
                                       textures=images, patches=rows), separators=(',', ':')) + '\n')
        total += len(rows); print(f'{rel}: {len(rows)} glint patches ({", ".join(sorted(images))}) -> {out}')
    print('glint patches', total)


if __name__ == '__main__':
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--out', required=True, help='output root (a scratch folder; never web/public/assets)')
    a = ap.parse_args()
    if Path(a.out).resolve().is_relative_to(ASSETS.resolve()): raise SystemExit('refusing to write into web/public/assets')
    export(a.out)
