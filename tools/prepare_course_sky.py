#!/usr/bin/env python3
"""Package a location's native sky (tools/import_sky.py sky.json) for the browser.

SPDX-License-Identifier: GPL-3.0-only

Same packaging as web/prepare.py did inline for Snow Jam (-> web/public/assets/SKY):
textures become references into the shared world texture library (TEXTURES/world.tex, tools/export_world_textures.py;
they were PNG copies before 2026-09-25), the three sky-*.bin buffers are copied
without the prefix, every batch gets lightmap=-1/instance=False and the metadata is
written as world.json. Default output: tools/locations.web_manifest_entry(code)['sky']
(ARA1 -> web/public/assets/SKY, others -> web/public/assets/<code>/sky).

Usage: python3 tools/prepare_course_sky.py [--location BRA2] [--output DIR]
Import: from prepare_course_sky import prepare_course_sky; prepare_course_sky('BRA2')
"""
import argparse
import json
import shutil
from pathlib import Path

from locations import ROOT, native_dir, web_manifest_entry


def sky_output(code):
    return ROOT / 'web/public' / web_manifest_entry(code)['sky'].strip('/')


def prepare_course_sky(code='ARA1', output=None):
    source = native_dir(code)
    dest = Path(output) if output else sky_output(code)
    dest.mkdir(parents=True, exist_ok=True)
    d = json.loads((source / 'sky.json').read_text())
    if d['location'] != code:
        raise ValueError(f'{source / "sky.json"} belongs to {d["location"]}')
    # The dome's textures are world textures (kind 9 of the sky location): references into the shared world texture
    # library (tools/export_world_textures.py), no PNG copies.
    from export_world_textures import package_textures
    package_textures(dest, d['textures'], lambda key, t: (source / t['path']).read_bytes())
    for f in ['vertices.bin', 'indices.bin', 'colors.bin']:
        shutil.copy2(source / ('sky-' + f), dest / f)
    for b in d['batches']:
        b['lightmap'] = -1
        b['instance'] = False
    (dest / 'world.json').write_text(json.dumps(d))
    return dest, d


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--location', default='ARA1')
    p.add_argument('--output', type=Path, help='Override the browser sky folder (e.g. a scratch dir)')
    a = p.parse_args()
    dest, d = prepare_course_sky(a.location, a.output)
    print(json.dumps(dict(location=a.location, output=str(dest), sky_location=d['sky_location'], textures=len(d['textures']),
                          fog_start=d['fog']['start'] and d['fog']['start']['entry'], painted_fog=len(d['fog']['painted']))))


if __name__ == '__main__':
    main()
