#!/usr/bin/env python3
"""Package the native RIDER_SAM (tools/sam_mesh.py) for the browser and compile its source skin.

  python3 tools/sam_mesh.py --all && python3 tools/build_sam_web.py --all

Writes web/public/assets/RIDER_SAM: PNG textures, vertex/index/colour buffers, world.json
and rider.json, then tools/compile_sam_skin.py adds the source-engine bind matrices and
integer-percent weights to both the native and the browser rider.json. The browser keeps
its existing animation-samples.json (the gameplay clip table is not part of the model).
The same packaging as web/prepare.py's package_world, limited to Sam's package; the
Select Character preview copy (RIDER_SAM/fe) is refreshed with tools/export_fe_preview.py --character sam.
"""
import argparse, json, shutil, subprocess, sys
from pathlib import Path
from PIL import Image
from compile_sam_skin import compile_files

ROOT = Path(__file__).resolve().parents[1]


def package(native, out):
    out.mkdir(parents=True, exist_ok=True)
    world = json.loads((native / 'world.json').read_text())
    keep = set()
    for key, t in world['textures'].items():
        image = Image.frombytes('RGBA', (t['width'], t['height']), (native / t['path']).read_bytes())
        image.save(out / (key + '.png'))
        t['path'] = key + '.png'; keep.add(t['path'])
    for stale in out.glob('9-*.png'):
        if stale.name not in keep: stale.unlink()
    for name in ['vertices.bin', 'indices.bin', 'colors.bin', 'rider.json']:
        shutil.copy2(native / name, out / name)
    if not (out / 'animation-samples.json').exists():
        shutil.copy2(native / 'animation-samples.json', out / 'animation-samples.json')
    (out / 'world.json').write_text(json.dumps(world, separators=(',', ':')))


def fe_copy(out, template):
    """Select Character / Equip Gear preview copy of an outfit package, in the tools/export_fe_preview.py sam()
    layout (RIDER_SAM/fe is written by that tool): the package's own mesh, every part drawn, the board parts
    flagged, no morphs, and RIDER_SAM's `fe` block (same character, preview slot, IRR record, clips)."""
    dest = out / 'fe'
    if dest.exists(): shutil.rmtree(dest)
    dest.mkdir(parents=True)
    for f in out.iterdir():
        if f.is_file() and (f.suffix in ('.png', '.bin') or f.name == 'world.json'): shutil.copy2(f, dest / f.name)
    rig = json.loads((out / 'rider.json').read_text())
    vertices = (out / 'vertices.bin').stat().st_size // 40
    rig['parts'] = [dict(part=p['name'], resource=None, file=None, first_vertex=p['first_vertex'], vertex_count=p['vertex_count'],
                         first_index=p['first_index'], index_count=p['index_count'], board=p['name'].startswith(('derived_BindingsA', 'K2_')),
                         morph_count=0, morphs=[]) for p in rig['parts']]
    if sum(p['vertex_count'] for p in rig['parts']) != vertices: raise ValueError('Sam parts do not cover the mesh')
    rig['fe'] = json.loads((template / 'fe/rider.json').read_text())['fe']
    (dest / 'morphs.bin').write_bytes(b'')
    (dest / 'rider.json').write_text(json.dumps(rig, separators=(',', ':')))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--native', type=Path, default=ROOT / 'local/assets/native/RIDER_SAM')
    parser.add_argument('--out', type=Path, default=ROOT / 'web/public/assets/RIDER_SAM')
    parser.add_argument('--report', type=Path, default=ROOT / 'local/rider-lighting/sam-skin-compilation.json')
    parser.add_argument('--all', action='store_true', help='also every outfit package local/assets/native/RIDER_SAM_* (Equip Gear)')
    args = parser.parse_args()
    package(args.native, args.out)
    print(json.dumps(compile_files(args.native / 'rider.json', args.out / 'rider.json', args.report)))
    # The Select Character preview (RIDER_SAM/fe, tools/export_fe_preview.py) is a copy of this package.
    if args.out.resolve() == (ROOT / 'web/public/assets/RIDER_SAM').resolve():
        subprocess.run([sys.executable, str(ROOT / 'tools/export_fe_preview.py'), '--character', 'sam'], check=True)
    if args.all:
        web = ROOT / 'web/public/assets'
        for native in sorted((ROOT / 'local/assets/native').glob('RIDER_SAM_*')):
            out = web / native.name
            package(native, out)
            shutil.copy2(web / 'RIDER_SAM/animation-samples.json', out / 'animation-samples.json')
            report = compile_files(native / 'rider.json', out / 'rider.json', ROOT / f'local/rider-lighting/sam-skin-{native.name}.json')
            fe_copy(out, web / 'RIDER_SAM')
            print(native.name, report['vertices'], report['groups'])
    from export_rider_textures import pack_all; pack_all()   # the packages' PNGs -> Sam's texture archive (WARDROBE/SAM)


if __name__ == '__main__':
    main()
