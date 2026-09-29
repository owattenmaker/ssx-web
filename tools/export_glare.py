#!/usr/bin/env python3
"""Export the original world-painter type-6 (framebuffer glare) painter of a course.

Recovered from SLUS_207.72 (docs/terrain-render-fidelity.md, "Framebuffer glare pass"):
- Painter type 6: factory 2C0408, ctor 2BC830, vtable 484DA8, blend 2BD068 (w = weight^2,
  current = w*payload + (1-w)*current), compare 2BDBD0, reset 2BE140 (1,1,1,1,1,0,0),
  getters 2C14F0..2C1520 (+8,+10,..,+38) reached through 2EEDB0..2EEF60 (environment slot +8).
- Payload (8 floats): rate, then the seven values 2F00A0 copies to gp+12E4..12F4/1300/1304
  (debug menu names from 249200): Minimum Intensity Cutoff, Post-Cutoff Scale, Copy
  Intensity, Frame Source Intensity, Frame Blend Intensity, Blend Texture 2, Blend Texture 3.
- The pass is 36C790 (web/glare-pass.js); it is a no-op while all four blend-texture bytes
  are 0 (Blend Texture 0/1 are debug-only gp+12F8/12FC, 0 in the retail executable).

Writes web/public/assets/<X>/glare.json (git-ignored). A course record without a type-6
section (ARA1) is written as painter=null: the painter keeps its reset defaults and the
pass never runs. `--survey` lists every SDB world painter record that authors type 6.
"""
import argparse, hashlib, json, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from world_assets import world_chunks, records, locations  # noqa: E402
from import_sky import painter_sections, painted_entries  # noqa: E402
from course_painters import course_painter, point_tree  # noqa: E402

SOURCE = ROOT / 'local/assets/source/ps2'
FIELDS = ['cutoff', 'post_cutoff_scale', 'copy_intensity', 'frame_source_intensity',
          'frame_blend_intensity', 'blend_texture2', 'blend_texture3']
DEFAULTS = [1.0, 1.0, 1.0, 1.0, 1.0, 0.0, 0.0]  # reset 2BE140 / live context of courses without type 6


def payloads(section):
    out = []
    for kind, values in painted_entries(section, 8):
        if kind != 6:
            raise ValueError('Glare section entry with another type')
        out.append(dict(rate=values[0], **dict(zip(FIELDS, values[1:8]))))
    return out


def glare_package(code):
    found = course_painter(code)
    if found is None:
        raise ValueError(f'{code}: no world painter record')
    chunk, track, rid, data, sections = found
    base = dict(version=1, location=code, chunk=chunk, track=track, rid=rid,
                record_sha256=hashlib.sha256(data).hexdigest(), fields=FIELDS, defaults=DEFAULTS,
                debug=dict(enable=1, capture_log2=8, blend_texture0=0.0, blend_texture1=0.0, jitter=2.0))
    if 6 not in sections:
        return dict(base, painter=None)
    section = sections[6]
    pays = payloads(section)
    tree = point_tree(section, len(pays))
    for p in pays:
        for k in ('cutoff', 'frame_source_intensity', 'post_cutoff_scale', 'frame_blend_intensity', 'blend_texture2', 'blend_texture3'):
            if not 0 <= p[k] * 127.5 < 256:
                raise ValueError(f'{code}: glare byte outside 0..255 ({k}={p[k]})')
    return dict(base, painter=dict(tree, payloads=pays))


def survey():
    locs = locations(SOURCE / 'bam.sdb')
    def name(ci):
        return next(l['name'] for l in locs if ci <= l['chunk_end'])
    rows = []
    for ci, chunk in enumerate(world_chunks(SOURCE / 'bam.ssb')):
        for kind, track, rid, data in records(chunk):
            if kind == 15 and len(data) >= 64 and 6 in painter_sections(data):
                pays = payloads(painter_sections(data)[6])
                rows.append(dict(location=name(ci), chunk=ci, track=track, payloads=len(pays),
                                 active=sum(1 for p in pays if p['blend_texture2'] or p['blend_texture3'])))
    return rows


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--location', default='ARA1')
    p.add_argument('--output', type=Path, help='Default: web/public/assets/<X>/glare.json')
    p.add_argument('--survey', action='store_true')
    a = p.parse_args()
    if a.survey:
        for row in survey():
            print(row)
        return
    package = glare_package(a.location)
    out = a.output or ROOT / 'web/public/assets' / a.location / 'glare.json'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(package, separators=(',', ':')))
    painter = package['painter']
    print(a.location, 'glare painter', 'absent' if painter is None else f"{len(painter['payloads'])} payloads, {len(painter['nodes'])} nodes", '->', out)


if __name__ == '__main__':
    main()
