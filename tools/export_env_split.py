#!/usr/bin/env python3
"""SCRATCH CHECK ONLY (not a deploy path: web/prepare.py and tools/export_peak_world.py split the env batches themselves when they
re-split a package). The env-map batches (pv envMap, web/world-material.js envPassMaterial; docs/visual-parity.md 43) for the world packages as they are
shipped: each draw batch of web/public/assets/<LOC>/ and PEAK<N>/<LOC>/ is split into its triangles without and with a static-model env
material (the same rule as web/prepare.py mesh_env / tools/export_peak_world.py), tagged env = [second texture, 0x200000 | 0x600000], and
the second textures join the texture table. Nothing else of a package changes (a plain re-split would also take in any pipeline change
made since the last export). Writes <out>/<package>/world.json and indices.bin; web/public/assets is only read.

  python3 tools/export_env_split.py --out <scratch> [--only BRA2 PEAK1/ARA1 ...]"""
import argparse, array, json, struct, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from world_assets import world_chunks, records  # noqa: E402
from world_models import decode_model  # noqa: E402
from export_world_textures import library_index, LIBRARY_URL  # noqa: E402
ASSETS, NATIVE = ROOT / 'web/public/assets', ROOT / 'local/assets/native'
# material word (with group flag bit3 -> 0x40000) & 0x660000 -> the second pass's ALPHA_2 (37F2A4..37FD2C)
ENV_MODES = {0x200000: 0x200000, 0x220000: 0x200000, 0x260000: 0x200000, 0x600000: 0x600000, 0x620000: 0x600000, 0x660000: 0x600000}


def all_records():
    models, materials = {}, {}
    for chunk in world_chunks(ROOT / 'local/assets/source/ps2/bam.ssb'):
        for kind, track, rid, data in records(chunk):
            if kind == 2: models.setdefault((track, rid), data)
            elif kind == 0: materials.setdefault((track, rid), data)
    return models, materials


def packages(only):
    out = []
    for p in sorted(ASSETS.glob('*/world.json')) + sorted(ASSETS.glob('PEAK*/*/world.json')):
        rel = p.parent.relative_to(ASSETS)
        if rel.parts[0].startswith('RIDER_') or rel.parts[0] in ('SKY', 'WARDROBE', 'CUTSCENES'): continue
        src = NATIVE / rel
        if not (src / 'world.json').exists(): continue
        if only and str(rel) not in only: continue
        out.append((rel, p.parent, src))
    return out


def split(rel, pkg, src, models, materials, out):
    shipped = json.loads((pkg / 'world.json').read_text()); source = json.loads((src / 'world.json').read_text())
    sidx = array.array('I'); sidx.frombytes((src / 'indices.bin').read_bytes())
    idx = array.array('I'); idx.frombytes((pkg / 'indices.bin').read_bytes())
    cache, env_of = {}, {}
    for s in source.get('collision_sources', []):
        if s['kind'] != 'instance': continue
        key = tuple(s['model'])
        if key not in cache:
            envs = []
            for m in decode_model(models[key]):
                rec = materials[tuple(m['material'])]; word = struct.unpack_from('<I', rec, 12)[0] | (0x40000 if m['group_flags'] & 8 else 0)
                mode = ENV_MODES.get(word & 0x660000)
                envs.append((struct.unpack_from('<h', rec, 2)[0], mode) if mode else None)
            cache[key] = envs
        env = cache[key][s['mesh']]
        if env:
            for t in range(s['first_triangle'], s['first_triangle'] + s['triangle_count']): env_of[tuple(sidx[t * 3:t * 3 + 3])] = env
    batches, new, drawn = [], array.array('I'), set()
    for b in shipped['batches']:
        if 'env' in b: raise ValueError(f'{rel}: already split')
        parts = {}
        for at in range(b['first_index'], b['first_index'] + b['index_count'], 3):
            tri = tuple(idx[at:at + 3]); env = env_of.get(tri) if b.get('instance') else None
            parts.setdefault(env, array.array('I')).extend(tri)
        for env in sorted(parts, key=lambda e: (e is not None, e or (0, 0))):
            nb = dict(b); nb['first_index'] = len(new); nb['index_count'] = len(parts[env])
            if env: nb['env'] = list(env); drawn.update(tuple(parts[env][i:i + 3]) for i in range(0, len(parts[env]), 3))
            batches.append(nb); new.extend(parts[env])
    if len(new) != len(idx): raise ValueError(f'{rel}: split lost indices')
    if len(drawn) != len(env_of): print(f'  {rel}: {len(env_of) - len(drawn)} of {len(env_of)} source env triangles in no shipped batch')
    shipped['batches'] = batches
    for tex in sorted({b['env'][0] for b in batches if 'env' in b}):
        e = library_index()[tex]
        shipped['textures'].setdefault(f'9-{tex}', dict(width=e['width'], height=e['height'], source='ps2', fallback_reason=None, pack=LIBRARY_URL, id=tex))
    dest = out / rel; dest.mkdir(parents=True, exist_ok=True)
    (dest / 'world.json').write_text(json.dumps(shipped, separators=(',', ':'))); (dest / 'indices.bin').write_bytes(new.tobytes())
    return sum(1 for b in batches if 'env' in b), sum(b['index_count'] // 3 for b in batches if 'env' in b)


def main():
    a = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    a.add_argument('--out', required=True, type=Path); a.add_argument('--only', nargs='*')
    args = a.parse_args(); out = args.out.resolve()
    if out == ASSETS or ASSETS in out.parents: raise SystemExit('--out must not be web/public/assets (export to scratch)')
    models, materials = all_records()
    for rel, pkg, src in packages(set(args.only or [])):
        n, tris = split(rel, pkg, src, models, materials, out)
        print(f'{rel}: {n} env batches, {tris} env triangles')


if __name__ == '__main__':
    main()
