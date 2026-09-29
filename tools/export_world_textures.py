#!/usr/bin/env python3
"""The shared world texture library of SSX 3 as one browser archive (docs/asset-formats.md "World texture library").

The original keeps one texture table for the whole world stream: BAM.SDB declares 788 textures (u16 at +0x2A) and 623
light pages (+0x2C); the chunk resolver (cPS2ChunkResolve, record dispatch 0x494FB0) registers an SSB kind-9 record as
texture handle = its resource id ("strm_tex", 0x3AAF5C) and a kind-10 record as handle 788 + id ("strm_lpg",
0x3AAF88), in the renderer's texture manager (0x367440: explicit handle -> slot, manager + 8 + 4 * handle). Materials
(kind 0, s16 texture id) and terrain patches (+416 texture, +418 light page) name those ids, so a texture carried by
several locations is the same texture everywhere (every copy of an id on the disc is byte-identical).

Output (git-ignored): web/public/assets/TEXTURES/world.tex (tools/texture_archive.py layout), entries id = SSB kind-9
resource id, decoded like tools/import_world.py (PS2 SHAPE; a record the PS2 decoder rejects takes the GameCube CMPR
copy of the same id, source 'gamecube'). World packages (web/prepare.py, tools/prepare_course_sky.py,
tools/export_peak_world.py, tools/export_cutscene_sets.py) reference it instead of carrying PNG copies:
    world.json textures['9-<rid>'] = {width, height, ..., pack: '/assets/TEXTURES/world.tex', id: rid}
and their GameCube lightmaps (only drawn with ?originalWorld=0) go to <package>/lightmaps.tex:
    world.json textures['10-<rid>'] = {width, height, ..., pack: 'lightmaps.tex', id: rid}

    .venv/bin/python tools/export_world_textures.py              # (re)build the library from the disc
    .venv/bin/python tools/export_world_textures.py --repackage  # convert already exported packages in place:
        every 9-/10- PNG is checked texel for texel against its archive entry, then removed (backup of the
        replaced files under local/texture-library-backup/)

SPDX-License-Identifier: GPL-3.0-only
"""
import argparse
import hashlib
import json
import os
import shutil
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from texture_archive import write_archive, read_archive, read_index, rgba_digest, decode_png  # noqa: E402

PS2 = ROOT / 'local/assets/source/ps2'
GC = ROOT / 'local/assets/source/gamecube'
ASSETS = ROOT / 'web/public/assets'
LIBRARY = ASSETS / 'TEXTURES/world.tex'
LIBRARY_URL = '/assets/TEXTURES/world.tex'
LIGHTMAPS = 'lightmaps.tex'
BACKUP = ROOT / 'local/texture-library-backup'


def sdb_counts(path=PS2 / 'bam.sdb'):
    """BAM.SDB header +0x2A world texture count, +0x2C light page count (the light page handle base is the former)."""
    data = Path(path).read_bytes()
    return struct.unpack_from('<HH', data, 0x2A)


def build_library(output=LIBRARY):
    from world_assets import world_chunks, records, locations, texture_rgba, gamecube_cmpr
    textures, owners = {}, {}
    locs = locations(PS2 / 'bam.sdb')
    ends = [l['chunk_end'] for l in locs]
    for i, chunk in enumerate(world_chunks(PS2 / 'bam.ssb')):
        owner = next(k for k, e in enumerate(ends) if i <= e)
        for kind, track, rid, data in records(chunk):
            if kind != 9:
                continue
            if track != 255:
                raise ValueError(f'kind-9 record {rid} on track {track} (expected the global track 255)')
            if rid in textures and textures[rid] != data:
                raise ValueError(f'Texture {rid} differs between chunks')   # every copy of an id is the same texture
            textures[rid] = data
            owners.setdefault(rid, set()).add(locs[owner]['name'])
    count, pages = sdb_counts()
    if sorted(textures) != list(range(count)):
        raise ValueError(f'Texture ids {min(textures)}..{max(textures)} ({len(textures)}) differ from the SDB count {count}')
    gc = {}
    if (GC / 'bam.gsb').exists():
        for chunk in world_chunks(GC / 'bam.gsb'):
            for kind, track, rid, data in records(chunk, 'big'):
                if kind == 9:
                    gc.setdefault(rid, data)
    entries = []
    for rid in range(count):
        data, source, reason = textures[rid], 'ps2', None
        try:
            w, h, rgba = texture_rgba(data)
        except ValueError as error:   # tools/import_world.py: the GameCube CMPR copy of the same id
            alternate = gc.get(rid)
            if alternate is None or alternate[0] != 30:
                raise ValueError(f'Texture {rid}: {error}') from error
            w, h, rgba = gamecube_cmpr(alternate)
            if (w, h) != struct.unpack_from('<HH', data, 4):
                raise ValueError(f'Fallback texture dimensions differ: {rid}') from error
            source, reason = 'gamecube', str(error)
        e = dict(id=rid, width=w, height=h, rgba=rgba, source=source, locations=len(owners[rid]),
                 source_sha256=hashlib.sha256(data).hexdigest()[:16])
        if reason:
            e['fallback_reason'] = reason
        entries.append(e)
    index = write_archive(output, entries, 'world', texture_count=count, light_page_count=pages,
                          source='SSX3 USA PS2 BAM.SSB kind 9 (texture handle = resource id)',
                          source_sha256=hashlib.sha256((PS2 / 'bam.ssb').read_bytes()).hexdigest())
    shared = sum(1 for r in owners if len(owners[r]) > 1)
    print(json.dumps(dict(output=str(output), textures=count, bytes=output.stat().st_size, shared_by_locations=shared,
                          records=sum(len(o) for o in owners.values()), gamecube_fallback=[e['id'] for e in entries if e['source'] != 'ps2'])))
    return index


_library = None


def library_index():
    global _library
    if _library is None:
        if not LIBRARY.exists():
            build_library()
        _library = {e['id']: e for e in read_index(LIBRARY.read_bytes())['entries']}
    return _library


def world_entry(key, meta, rgba):
    """A package's '9-<rid>' texture as a library reference; the texels must equal the library's."""
    rid = int(key.split('-')[1])
    e = library_index().get(rid)
    if e is None or (e['width'], e['height']) != (meta['width'], meta['height']) or e['rgba'] != rgba_digest(rgba):
        raise ValueError(f'{key}: texels differ from the world texture library ({LIBRARY})')
    out = {k: v for k, v in meta.items() if k != 'path'}
    out.update(pack=LIBRARY_URL, id=rid)
    return out


def write_lightmaps(dest, lightmaps):
    """lightmaps: {'10-<rid>': (meta, rgba)} -> <dest>/lightmaps.tex; returns the world.json entries (no file if none)."""
    dest = Path(dest)
    if not lightmaps:
        (dest / LIGHTMAPS).unlink(missing_ok=True)
        return {}
    items = sorted(lightmaps.items(), key=lambda kv: int(kv[0].split('-')[1]))
    write_archive(dest / LIGHTMAPS, [dict(id=int(k.split('-')[1]), width=m['width'], height=m['height'], rgba=rgba) for k, (m, rgba) in items],
                  'lightmaps', source='GameCube lightmaps (SSB kind 10) of the package; drawn only with ?originalWorld=0')
    out = {}
    for k, (m, _) in items:
        e = {x: v for x, v in m.items() if x != 'path'}
        e.update(pack=LIGHTMAPS, id=int(k.split('-')[1]))
        out[k] = e
    return out


def remove_png_copies(dest, keys):
    for k in keys:
        (Path(dest) / (k + '.png')).unlink(missing_ok=True)


def package_textures(dest, textures, rgba_of):
    """Rewrite a world package's texture table: 9- keys -> library, 10- keys -> lightmaps.tex; other keys untouched.
    rgba_of(key, meta) -> RGBA texels of the package's texture. Removes the package's old 9-/10- PNG copies."""
    lightmaps = {}
    for key, meta in list(textures.items()):
        if key.startswith('9-'):
            textures[key] = world_entry(key, meta, rgba_of(key, meta))
        elif key.startswith('10-'):
            lightmaps[key] = (meta, rgba_of(key, meta))
    textures.update(write_lightmaps(dest, lightmaps))
    remove_png_copies(dest, [k for k in textures if k.startswith(('9-', '10-'))])
    return textures


def world_packages(root=ASSETS):
    """Every exported package that draws world textures (course, connector, peak location, sky dome, cutscene set)."""
    for path in sorted(root.rglob('world.json')):
        rel = path.relative_to(root).parts
        if rel[0].startswith('RIDER_') or rel[0] == 'WARDROBE' or rel[:2] == ('CUTSCENES', 'PROPS'):
            continue
        yield path


def repackage(root=ASSETS, backup=BACKUP, only=None):
    library_index()
    converted = removed = kept = 0
    for path in world_packages(root):
        if only and str(path.parent.relative_to(root)) not in only:
            continue
        d = json.loads(path.read_text())
        textures = d.get('textures', {})
        pending = {k: t for k, t in textures.items() if k.startswith(('9-', '10-')) and 'path' in t}
        if not pending:
            kept += 1
            continue
        dest = path.parent
        cache = {}

        def rgba_of(key, meta):
            if key not in cache:
                w, h, rgba = decode_png((dest / meta['path']).read_bytes())
                if (w, h) != (meta['width'], meta['height']):
                    raise ValueError(f'{dest / meta["path"]}: dimensions differ from world.json')
                cache[key] = rgba
            return cache[key]
        # Lightmaps already archived stay as they are; a package is converted as a whole.
        for key, meta in textures.items():
            if key.startswith('10-') and 'path' not in meta:
                raise ValueError(f'{path}: mixed lightmap entries')
        for key in pending:
            rgba_of(key, textures[key])
        rel = path.parent.relative_to(root)
        (backup / rel).mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, backup / rel / 'world.json')
        for key, meta in pending.items():
            shutil.copy2(dest / meta['path'], backup / rel / meta['path'])
        package_textures(dest, textures, rgba_of)
        separators = (',', ':') if b'", "' not in path.read_bytes()[:4096] else None
        temp = path.with_name('world.json.tmp')
        temp.write_text(json.dumps(d, separators=separators) if separators else json.dumps(d))
        os.replace(temp, path)
        converted += 1
        removed += len(pending)
        print(f'{rel}: {sum(k.startswith("9-") for k in pending)} library textures, {sum(k.startswith("10-") for k in pending)} lightmaps', flush=True)
    print(json.dumps(dict(converted=converted, unchanged=kept, png_removed=removed, backup=str(backup))))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--repackage', action='store_true', help='convert the exported world packages to the archives')
    p.add_argument('--output', type=Path, default=LIBRARY)
    p.add_argument('--only', nargs='*', help='with --repackage: only these package folders (relative to web/public/assets)')
    a = p.parse_args()
    if a.repackage:
        repackage(only=set(a.only or []))
    else:
        build_library(a.output)


if __name__ == '__main__':
    main()
