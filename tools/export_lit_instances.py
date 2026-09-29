#!/usr/bin/env python3
"""Lit static-model instances' light rows (development export; --location X, default every location with a countdown audit):
local/event-activation/<LOC>/lit-instances.json, read by web/prepare.py (web/world-batches.py tags each lit instance's batches with
its rows; web/world-material.js draws them, pv litInstances).

A lit instance (runtime flag 0x4000 = authored descriptor flag 0x40000000 in every countdown audit) is drawn by 37E238 with the rows
of its entry in the light cache 2F5400 (engine/lit_instance_lighting.hpp): the bank the cache's painter wrapper gives at the instance's
x/y (Lighting reference 3 of the payload there, else reference 0, else the course default gp+0x12D4; the course's environment.json
painter) plus up to 4 local lights ranked at its position (local-lights.json / light-tree.json). An instance without runtime flag
0x1000 is lit once, at its first draw, at its position then: the instance matrix's row 3 for static instances and resting entities
(crashbags); an instance with 0x1000 is relit as it moves (`relight`: web/lit-instances.js with the core's lit_instance_rows).
Checked against every PS2 light cache entry found in the savestates under local/ps2-capture (event courses: the object bank):
199 instances bit-exact at their instance position; the moving ones (BHP1's animated cars, CRA3's blimp pads, ERA5's ice bits) are
exact at the position the PS2 lit them at.
"""
import argparse, glob, hashlib, json, subprocess, sys
from pathlib import Path
root = Path(__file__).resolve().parents[1]; sys.path.insert(0, str(root / 'tools'))
from set_piece_location import Location  # noqa: E402
from locations import activation_dir  # noqa: E402

TOOL_SOURCE = root / 'tools/lit_instance_rows.cpp'
JSON_INCLUDE = root / 'build/ps2recomp/_deps/nlohmann_json-src/single_include'
if not JSON_INCLUDE.is_dir():   # a checkout without the PS2Recomp build: the vendored header (web/third_party/nlohmann)
    JSON_INCLUDE = root / 'web/third_party'


def tool():
    binary = root / 'build/lit_instance_rows'
    sources = [TOOL_SOURCE, root / 'engine/lit_instance_lighting.hpp']
    if not binary.exists() or any(s.stat().st_mtime > binary.stat().st_mtime for s in sources):
        binary.parent.mkdir(parents=True, exist_ok=True)
        import shutil  # macOS: xcrun clang++; elsewhere $CXX or c++
        cxx = ['xcrun', 'clang++'] if shutil.which('xcrun') else [__import__('os').environ.get('CXX', 'c++')]
        subprocess.run([*cxx, '-std=c++20', '-O2', '-frounding-math', '-ffp-contract=off', '-I', str(root / 'engine'),
                        '-I', str(JSON_INCLUDE), str(TOOL_SOURCE), '-o', str(binary)], check=True)
    return binary


def export(code, out=None):
    audit = json.loads((activation_dir(code) / 'countdown-instances.json').read_text())
    lit = [r for r in audit['instances'] if (r.get('runtime_flags') or 0) & 0x4000]
    target = Path(out) if out else activation_dir(code) / 'lit-instances.json'
    if not lit:
        return None
    loc = Location(code); byres = {(i['rid'] << 8) | i['track']: i for i in loc.world['instances']}
    assets = root / 'web/public/assets' / code
    env = json.loads((assets / 'environment.json').read_text())['irradiance']
    records = json.loads((root / 'local/assets/native/IRRADIANCE/irradiance.json').read_text())['records']
    queries = [dict(id=r['resource'], position=byres[r['resource']]['matrix'][12:15]) for r in lit]
    request = target.with_suffix('.request.json'); target.parent.mkdir(parents=True, exist_ok=True)
    request.write_text(json.dumps(dict(painter=env.get('painter'), banks={k: v['rows'] for k, v in records.items()}, queries=queries)))
    run = subprocess.run([str(tool()), str(assets / 'local-lights.json'), str(assets / 'light-tree.json'), str(request)], capture_output=True, text=True)
    request.unlink()
    if run.returncode: raise RuntimeError(f'{code}: {run.stderr.strip()}')
    rows = {x['id']: x for x in json.loads(run.stdout)}
    instances = [dict(resource=r['resource'], name=r['name'], runtime_flags=r['runtime_flags'], position=queries[k]['position'],
                      bank=rows[r['resource']]['bank'], lights=rows[r['resource']]['lights'], rows=rows[r['resource']]['rows'],
                      relight=bool(r['runtime_flags'] & 0x1000)) for k, r in enumerate(lit)]
    sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
    target.write_text(json.dumps(dict(version=1, location=code, world_package_sha256=audit['world_package_sha256'],
                                      light_catalog_sha256=sha(assets / 'local-lights.json'), light_tree_sha256=sha(assets / 'light-tree.json'),
                                      instances=instances), separators=(',', ':')))
    return target


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--location', action='append')
    p.add_argument('--output')
    a = p.parse_args()
    codes = a.location or sorted(Path(f).parent.name for f in glob.glob(str(root / 'local/event-activation/*/countdown-instances.json')))
    for code in codes:
        t = export(code, a.output if a.output and len(codes) == 1 else None)
        print(code, t if t else 'no lit instances')


if __name__ == '__main__':
    main()
