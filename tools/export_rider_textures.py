#!/usr/bin/env python3
"""Per-rider texture archives (docs/asset-formats.md "Rider texture archives").

The disc keeps each character's textures in the character's own archive (PS2 DATA/CHAR/<X>TXP.BIG, GameCube
data/char/<x>txn.big: every outfit variant of the character, SSH/GSH members named <prefix>_<part>_<variant>); the game
loads the members the equipped outfit names (texture rule 0x11BE88 / 0x14B988). The browser does the same with one
archive per rider (tools/texture_archive.py layout, web/texture-archive.js), downloaded once when the rider is first
needed (Select Character preview, race human and computer riders, another player's outfit online, cutscene actors):

  WARDROBE/<ID>/textures.tex   the textures of the rider's default outfits (what the RIDER_* packages draw: race,
                               Select Character, computer riders, cutscene actors), id = the wardrobe texture stem;
                               plus any texture only a RIDER_* package uses (Sam's painted maps, 'package_<digest>')
  WARDROBE/<ID>/gear.tex       every other texture the rider can wear (the Equip Gear items), read when an outfit or
                               the Equip Gear screen first needs one; --single puts everything in textures.tex
  WARDROBE/<ID>/icons.tex      the Equip Gear item icons (id = iNNN), read only by the Equip Gear screen
  WARDROBE/<ID>/wardrobe.json  + texture_pack / gear_pack / icon_pack (archive URLs); textures[stem].pack
  RIDER_<ID>[/fe]/world.json   (and RIDER_SAM_*): textures {..., pack: '/assets/WARDROBE/<ID>/textures.tex', id}

It runs after the rider exporters (web/package.json `npm run setup`) and converts what they wrote as PNG files
(WARDROBE/<ID>/textures/*.png, icons/*.png, RIDER_*/[fe/]9-N.png): every texel is checked on the way (a package PNG
must equal an archive entry, else it is added), and the PNG copies are removed. Idempotent: an archive entry stays as
it is unless a fresh PNG of the same id replaces it.

    .venv/bin/python tools/export_rider_textures.py [--rider zoe ...] [--backup local/rider-texture-backup]

SPDX-License-Identifier: GPL-3.0-only
"""
import argparse
import json
import os
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from texture_archive import write_archive, read_archive, decode_png, png_bytes, rgba_digest  # noqa: E402

ASSETS = ROOT / 'web/public/assets'
WARDROBE = ASSETS / 'WARDROBE'


def _existing(path):
    """{id: pre-encoded archive entry} of an archive already written (kept byte for byte)."""
    if not path.exists():
        return {}
    index, files = read_archive(path)
    return {e['id']: dict(id=e['id'], width=e['width'], height=e['height'], png=files[e['id']], colours=e['colours'], digest=e['rgba'])
            for e in index['entries']}


def _fresh(path):
    w, h, rgba = decode_png(path.read_bytes())
    png, colours = png_bytes(w, h, rgba)
    return dict(id=path.stem, width=w, height=h, png=png, colours=colours, digest=rgba_digest(rgba))


def rider_packages(rid):
    """RIDER_* package folders (race and front-end) whose textures come from WARDROBE/<rid>."""
    names = [p for p in ASSETS.glob(f'RIDER_{rid}*') if p.is_dir() and (p.name == f'RIDER_{rid}' or (rid == 'SAM' and p.name.startswith('RIDER_SAM_')))]
    return [d for p in sorted(names) for d in (p, p / 'fe') if (d / 'world.json').exists()]


def pack_rider(rid, backup=None, log=print, single=False):
    folder = WARDROBE / rid
    doc_path = folder / 'wardrobe.json'
    doc = json.loads(doc_path.read_text())
    url = f'/assets/WARDROBE/{rid}/'
    moved = []
    # textures: existing archive + fresh PNGs of the exporter
    entries = {**_existing(folder / 'textures.tex'), **_existing(folder / 'gear.tex')}
    fresh_files = sorted((folder / 'textures').glob('*.png')) if (folder / 'textures').is_dir() else []
    for f in fresh_files:
        entries[f.stem] = _fresh(f)
    missing = [s for s in doc.get('textures', {}) if s not in entries]
    if missing:
        raise ValueError(f'{rid}: wardrobe textures without texels: {missing[:5]}')
    by_digest = {}
    for e in entries.values():
        by_digest.setdefault(e['digest'], []).append(e['id'])
    # RIDER_* packages: every PNG must be one of the rider's textures (else it joins the archive)
    rewrites = []
    for pkg in rider_packages(rid):
        world_path = pkg / 'world.json'
        world = json.loads(world_path.read_text())
        changed = False
        for key, t in world.get('textures', {}).items():
            if 'path' not in t:
                continue
            png_path = pkg / t['path']
            w, h, rgba = decode_png(png_path.read_bytes())
            digest = rgba_digest(rgba)
            stem = (t.get('resource') or '').lower().rsplit('.', 1)[0]
            ids = by_digest.get(digest, [])
            ident = stem if stem in ids else (ids[0] if ids else f'package_{digest}')
            if not ids:
                png, colours = png_bytes(w, h, rgba)
                entries[ident] = dict(id=ident, width=w, height=h, png=png, colours=colours, digest=digest)
                by_digest[digest] = [ident]
            e = entries[ident]
            if (e['width'], e['height']) != (w, h):
                raise ValueError(f'{png_path}: dimensions differ from {ident}')
            world['textures'][key] = {**{k: v for k, v in t.items() if k != 'path'}, 'pack': url + 'textures.tex', 'id': ident}
            moved.append(png_path)
            changed = True
        if changed:
            rewrites.append((world_path, world))
    # keep: the wardrobe's textures and whatever a RIDER_* package names (an entry nothing uses any more goes)
    keep = set(doc.get('textures', {}))
    for pkg in rider_packages(rid):
        world = next((w for p, w in rewrites if p == pkg / 'world.json'), None) or json.loads((pkg / 'world.json').read_text())
        keep |= {t['id'] for t in world.get('textures', {}).values() if t.get('pack') == url + 'textures.tex'}
    order = sorted(k for k in entries if k in keep)
    # base = what the default RIDER_* packages draw (race, Select Character, computer riders, cutscene actors); gear =
    # every other Equip Gear texture, read only when an outfit or the Equip Gear screen needs one (single: one archive)
    base = set()
    for pkg in rider_packages(rid):
        world = next((w for p, w in rewrites if p == pkg / 'world.json'), None) or json.loads((pkg / 'world.json').read_text())
        base |= {t['id'] for t in world.get('textures', {}).values() if t.get('pack') == url + 'textures.tex'}
    if single:
        base = set(order)
    base_ids, gear_ids = [k for k in order if k in base], [k for k in order if k not in base]
    source = 'the character texture archive (DATA/CHAR/<X>TXP.BIG members, PS2 texel domain)'
    write_archive(folder / 'textures.tex', [entries[k] for k in base_ids], 'rider-textures', rider=rid,
                  source=source + ('' if single else ': the default outfits (RIDER_* packages)'))
    if gear_ids:
        write_archive(folder / 'gear.tex', [entries[k] for k in gear_ids], 'rider-gear', rider=rid, source=source + ': the other Equip Gear textures')
    else:
        (folder / 'gear.tex').unlink(missing_ok=True)
    for stem, t in doc.get('textures', {}).items():
        t['pack'] = url + ('gear.tex' if stem in gear_ids else 'textures.tex')
    # icons (Equip Gear screen only)
    icons = _existing(folder / 'icons.tex')
    icon_files = sorted((folder / 'icons').glob('*.png')) if (folder / 'icons').is_dir() else []
    for f in icon_files:
        icons[f.stem] = _fresh(f)
    if icons:
        write_archive(folder / 'icons.tex', [icons[k] for k in sorted(icons)], 'rider-icons', rider=rid, source='Equip Gear item icons')
    # metadata, then the PNG copies go (backup first)
    doc['texture_pack'] = url + 'textures.tex'
    if gear_ids:
        doc['gear_pack'] = url + 'gear.tex'
    else:
        doc.pop('gear_pack', None)
    if icons:
        doc['icon_pack'] = url + 'icons.tex'
    for path, data in [(doc_path, doc)] + rewrites:
        if backup:
            target = backup / path.relative_to(ASSETS)
            target.parent.mkdir(parents=True, exist_ok=True)
            if not target.exists():
                shutil.copy2(path, target)
        temp = path.with_name(path.name + '.tmp')
        raw = path.read_bytes()
        temp.write_text(json.dumps(data, separators=(',', ':')) if b'", "' not in raw[:4096] and b'": ' not in raw[:4096] else json.dumps(data, indent=None))
        os.replace(temp, path)
    for f in moved + fresh_files + icon_files:
        if backup:
            target = backup / f.relative_to(ASSETS)
            target.parent.mkdir(parents=True, exist_ok=True)
            if not target.exists():
                shutil.copy2(f, target)
        f.unlink()
    for d in (folder / 'textures', folder / 'icons'):
        if d.is_dir() and not any(d.iterdir()):
            d.rmdir()
    log(json.dumps(dict(rider=rid, textures=len(base_ids), texture_bytes=(folder / 'textures.tex').stat().st_size,
                        gear=len(gear_ids), gear_bytes=(folder / 'gear.tex').stat().st_size if gear_ids else 0,
                        icons=len(icons), icon_bytes=(folder / 'icons.tex').stat().st_size if icons else 0,
                        packages=len(rewrites), png_removed=len(moved) + len(fresh_files) + len(icon_files))))


def riders():
    return sorted(p.name for p in WARDROBE.iterdir() if (p / 'wardrobe.json').exists())


def pack_all(backup=None, log=print, single=False):
    """Every rider (the rider exporters call this at their end, so their PNG output never stays in the assets)."""
    if WARDROBE.exists():
        for rid in riders():
            pack_rider(rid, backup, log, single)


def rider_texture_png(rid, stem):
    """The PNG bytes of a wardrobe texture (the archive entry, or the exporter's PNG before it is packed)."""
    folder = WARDROBE / rid.upper()
    loose = folder / 'textures' / f'{stem}.png'
    if loose.exists():
        return loose.read_bytes()
    for name in ('textures.tex', 'gear.tex'):
        if (folder / name).exists():
            files = read_archive(folder / name)[1]
            if stem in files:
                return files[stem]
    raise KeyError(f'{rid}: no texture {stem}')


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--rider', action='append', help='rider id (default: every WARDROBE/<ID>)')
    p.add_argument('--backup', type=Path, help='copy every replaced file here first')
    p.add_argument('--single', action='store_true', help='one archive per rider (default: textures.tex = the default outfits, gear.tex = the rest)')
    a = p.parse_args()
    for rid in [r.upper() for r in a.rider] if a.rider else riders():
        pack_rider(rid, a.backup, single=a.single)


if __name__ == '__main__':
    main()
