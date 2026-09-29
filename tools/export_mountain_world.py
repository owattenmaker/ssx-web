#!/usr/bin/env python3
"""The whole-mountain streamed world MOUNTAIN (docs/peak3.md section 6): Peak 3 + Peak 2 + Peak 1 as ONE world.

The original has one world for the whole mountain: the streaming table 0x442168 holds every location, the residency table
0x442488 every course row, and a run crosses the peak boundaries on the connectors' own trigger volumes (ERA5_C loads Peak
2's Yellow station C, DRA4_A loads Peak 1's Green station A). The All Peak Race / Jam (The Throne -> ... -> Metro-City) and
the Peak 2 Race (Ruthless -> ... -> Metro-City) need exactly that. This tool builds it from the per-peak exports
(tools/export_peak_world.py --peak N, which stay the source of every location package):

    web/public/assets/MOUNTAIN/peak.json        every location (root = its per-peak package, /assets/PEAK<N>/<LOC>/), the
                                                streaming rows of all 49 locations / skies / TRANSP, the 22 residency rows
    MOUNTAIN/SECTIONS/sections.json             the three peaks' section activation data, merged by resource
    MOUNTAIN/SETPIECES/*.json                   the three peaks' merged set pieces, merged again (by resource; cloths rebased)
    MOUNTAIN/LIGHT_GLOW/, lighting-banks.json   merged
    MOUNTAIN/environment.json/.bin              the world's scalars and irradiance (the Peak 3 base course's) WITHOUT patches:
    MOUNTAIN/ENV/<LOC>.json/.bin                each location's environment patches and lattice textures, added to the core when
                                                the location's data loads and dropped when it is released (environment_add /
                                                environment_drop, web/environment_bridge.cpp): all three peaks' lattices are
                                                68 MB, a location's 0.3..5 MB. Texture ids are global: track << 12 | index.

Shared connectors: ERA5_C (in PEAK2 and PEAK3) and DRA4_A (PEAK1, PEAK2) are the same disc data in both exports (only the
provenance of the rail flags differs); the copy of the peak whose event export covers them is used (ERA5_C: Gravitude's,
PEAK3; DRA4_A: Intimidator's, PEAK2), and merges take the first entry in that precedence (PEAK3, PEAK2, PEAK1).

    python3 tools/export_mountain_world.py [--measured READS.json]

--measured: {LOC: ticks} disc read times measured on the PS2 (tools/ps2_autopilot.py runs, docs/peak3.md), replacing the
chunk-size estimates. The stage tables (web/generated/mountain_stage_seed.hpp) come from tools/export_peak_stage.py --mountain.
Nothing here reads the disc or a savestate. SPDX-License-Identifier: GPL-3.0-only
"""
import argparse, json, shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'web/public/assets'
OUT = ASSETS / 'MOUNTAIN'
PEAKS = ['PEAK3', 'PEAK2', 'PEAK1']   # precedence for shared connectors and merges
NAME = 'MOUNTAIN'


def load(p): return json.loads(Path(p).read_text())


def manifest(measured):
    docs = {p: load(ASSETS / p / 'peak.json') for p in PEAKS}
    locations, seen = [], set()
    for p in PEAKS:
        for l in docs[p]['locations']:
            if l['code'] in seen: continue
            seen.add(l['code']); locations.append(dict(l, peak=int(p[-1])))
    rows = {}
    for p in PEAKS:
        for r in docs[p]['residency']: rows.setdefault(r['course'], r)
    streaming = {}
    for p in PEAKS:
        for s in docs[p]['streaming']:
            s = dict(s)
            if s['code'] in measured: s.update(read_ticks=measured[s['code']], measured='allpeak')   # PS2 reads of the All Peak Race capture (a read's time depends on the texture sub-chunks in flight: this run's)
            streaming.setdefault(s['id'], s)
    # every location a residency row wants must have a streaming row and a package
    codes = {l['code'] for l in locations}
    for r in rows.values():
        for c in r['locations']:
            if c not in codes: raise SystemExit(f'residency row {r["course"]} wants {c}, which no peak exported')
    ids = {}
    for p in PEAKS: ids.update(docs[p]['location_ids'])
    tracks = {}
    for p in PEAKS: tracks.update(docs[p]['sdb_tracks'])
    return dict(version=1, peak=0, name=NAME, world=NAME, environment_slices='ENV/',
                source='tools/export_mountain_world.py: the PEAK3 / PEAK2 / PEAK1 manifests (SLUS_207.72 tables 0x43E250, 0x43D950, 0x442488, 0x442168)',
                streaming=[streaming[k] for k in sorted(streaming)], residency=[rows[k] for k in sorted(rows)],
                locations=sorted(locations, key=lambda l: l['id']), location_ids=ids, sdb_tracks=tracks)


def sections():
    merged = None; programs = {}; instances = {}
    for p in PEAKS:
        s = load(ASSETS / p / 'SECTIONS/sections.json')
        if merged is None: merged = {k: v for k, v in s.items() if k not in ('programs', 'instances')}
        for k, v in s['programs'].items():
            if k in programs and programs[k] != v: raise SystemExit(f'section program {k} differs between peaks')
            programs.setdefault(k, v)
        for i in s['instances']:
            if i['resource'] in instances and instances[i['resource']] != i: raise SystemExit(f'section instance {i["resource"]} differs between peaks')
            instances.setdefault(i['resource'], i)
    merged.update(location=NAME, name='The whole mountain (streamed world)', programs=programs,
                  instances=[instances[r] for r in sorted(instances)], provenance={p: load(ASSETS / p / 'SECTIONS/sections.json')['provenance'] for p in PEAKS})
    (OUT / 'SECTIONS').mkdir(parents=True, exist_ok=True)
    (OUT / 'SECTIONS/sections.json').write_text(json.dumps(merged, separators=(',', ':')))
    return len(instances), len(programs)


def setpieces():
    """The per-peak merges (tools/export_peak_world.py setpiece_packages) merged again with the same rules."""
    flags = uv = live = particles = stage = None; seen = set()
    for p in PEAKS:
        d = ASSETS / p / 'SETPIECES'
        F, U, L, P, S = (load(d / f) for f in ('flags.json', 'uv-scroll.json', 'livecomp.json', 'particles.json', 'stage-world.json'))
        if flags is None:
            flags = dict(F, location=NAME, cloths=[], instances=[]); uv = dict(U, location=NAME, instances=[])
            live = dict(L, location=NAME, instances=[], program_starts=[])
            particles = dict(P, location=NAME, programs=[], blocks=[], instances={}, carriers={}, multi=[], magnets=[], halos=[])
            stage = dict(S, location=NAME, meshanim=[], script=[], teleports=[], collections={})
        if F.get('sine_table') is not None and flags.get('sine_table') is not None and F['sine_table'] != flags['sine_table']: raise SystemExit(f'{p}: flag sine table differs')
        flags.setdefault('wind_by_peak', {})[p[-1]] = F.get('wind')   # the flag wind differs by peak (mode 1/2/3); `wind` is Peak 3's
        base = len(flags['cloths']); flags['cloths'] += F['cloths']
        flags['instances'] += [dict(x, cloth=x['cloth'] + base, peak=int(p[-1])) for x in F['instances'] if ('flag', x['resource']) not in seen]
        seen.update(('flag', x['resource']) for x in F['instances'])
        uv['instances'] += [x for x in U['instances'] if ('uv', x['resource']) not in seen]; seen.update(('uv', x['resource']) for x in U['instances'])
        live['instances'] += [x for x in L['instances'] if ('live', x['resource']) not in seen]; seen.update(('live', x['resource']) for x in L['instances'])
        live['program_starts'] += L.get('program_starts', [])
        particles['programs'] += P['programs']; particles['blocks'] += P['blocks']
        for k in ('instances', 'carriers'):
            for r, v in P[k].items(): particles[k].setdefault(r, v)
        for g in P.get('multi', []):
            if not any(m['group'] == g['group'] for m in particles['multi']): particles['multi'].append(g)
        particles['magnets'] += P.get('magnets', []); particles['halos'] += P.get('halos', [])
        for k in ('meshanim', 'script', 'teleports'):
            stage[k] += [x for x in S[k] if (k, x['resource']) not in seen]; seen.update((k, x['resource']) for x in S[k])
        for key, rows in S['collections'].items(): stage['collections'].setdefault(key, rows)
    (OUT / 'SETPIECES').mkdir(parents=True, exist_ok=True)
    for name, doc in (('flags.json', flags), ('uv-scroll.json', uv), ('livecomp.json', live), ('particles.json', particles), ('stage-world.json', stage)):
        (OUT / 'SETPIECES' / name).write_text(json.dumps(doc, separators=(',', ':')))
    return dict(flags=len(flags['instances']), uv=len(uv['instances']), livecomp=len(live['instances']), particle_owners=len(particles['instances']),
                meshanim=len(stage['meshanim']), script=len(stage['script']), collections=len(stage['collections']))


def light_glow():
    out = OUT / 'LIGHT_GLOW'; out.mkdir(parents=True, exist_ok=True); merged = None; seen = set()
    for p in PEAKS:
        g = load(ASSETS / p / 'LIGHT_GLOW/light-glow.json')
        if merged is None:
            merged = dict(g, location=NAME, lights=[])
            for t in g['textures'].values(): shutil.copy2(ASSETS / p / 'LIGHT_GLOW' / t['file'], out / t['file'])
        elif g['textures'] != merged['textures'] or g['constants'] != merged['constants']: raise SystemExit(f'{p}: light glow textures differ')
        merged['lights'] += [l for l in g['lights'] if (l['track'], l['rid']) not in seen]; seen.update((l['track'], l['rid']) for l in g['lights'])
    (out / 'light-glow.json').write_text(json.dumps(merged, indent=1) + '\n')
    return len(merged['lights'])


def lighting_banks():
    merged = None
    for p in PEAKS:
        b = load(ASSETS / p / 'lighting-banks.json')
        if merged is None: merged = dict(b, banks=dict(b['banks']) if isinstance(b['banks'], dict) else list(b['banks']))
        else:
            if b['defaults'] != merged['defaults']: raise SystemExit(f'{p}: default irradiance banks differ')
            if isinstance(b['banks'], dict):
                for k, v in b['banks'].items():
                    if k in merged['banks'] and merged['banks'][k] != v: raise SystemExit(f'{p}: bank {k} differs')
                    merged['banks'].setdefault(k, v)
            else:
                for v in b['banks']:
                    if v not in merged['banks']: merged['banks'].append(v)
    (OUT / 'lighting-banks.json').write_text(json.dumps(merged, separators=(',', ':')))
    return len(merged['banks'])


def environment(locations):
    """MOUNTAIN/environment.json (scalars + irradiance of the Peak 3 base course, no patches) and ENV/<LOC> slices."""
    (OUT / 'ENV').mkdir(parents=True, exist_ok=True)
    peaks = {p: (load(ASSETS / p / 'environment.json'), (ASSETS / p / 'environment.bin').read_bytes()) for p in PEAKS}
    base, _ = peaks['PEAK3']
    doc = dict(base, location=NAME, patches=[], textures=[], streamed=True,
               streaming_assumption='whole mountain: each location\'s patches / textures are added when it loads (MOUNTAIN/ENV/<LOC>)')
    (OUT / 'environment.json').write_text(json.dumps(doc, separators=(',', ':')))
    (OUT / 'environment.bin').write_bytes(b'')
    sizes = {}
    for l in locations:
        env, data = peaks[f'PEAK{l["peak"]}']; track = l['track']
        tex = {t['id']: t for t in env['textures']}
        patches = [p for p in env['patches'] if p['resource'] & 255 == track]
        used = sorted({t for p in patches for t in p['textures'] if t >= 0})
        if len(used) >= 4096: raise SystemExit(f'{l["code"]}: too many environment textures')
        remap = {old: (track << 12) | k for k, old in enumerate(used)}
        blob = bytearray(); textures = []
        for old in used:
            t = tex[old]; cells = (t['width'] + 1) * (t['height'] + 1)
            rgba = data[t['rgba_offset']:t['rgba_offset'] + cells * 4]; valid = data[t['valid_offset']:t['valid_offset'] + cells]
            textures.append(dict(t, id=remap[old], source_id_in_peak=old, rgba_offset=len(blob), valid_offset=len(blob) + len(rgba)))
            blob += rgba + valid
        slice_doc = dict(version=1, location=l['code'], track=track, peak=l['peak'],
                         patches=[dict(p, textures=[remap.get(t, -1) if t >= 0 else -1 for t in p['textures']]) for p in patches], textures=textures)
        (OUT / 'ENV' / f'{l["code"]}.json').write_text(json.dumps(slice_doc, separators=(',', ':')))
        (OUT / 'ENV' / f'{l["code"]}.bin').write_bytes(bytes(blob))
        sizes[l['code']] = len(blob)
    return sizes


def request_rows(m):
    """The streaming requests of every trigger volume (stage builtin 68 = 0x302210 in the slot-2 programs of the connectors'
    Load / Unload volumes and the skybox triggers), decoded from web/generated/mountain_stage_seed.hpp: key0 is the course
    table +0x5C map id, which the residency rows map to the course whose row is requested (22CEA8 action 0, 22D088 action 2;
    action 5 allows the dome switch). The peak boundaries: ERA5_C -> C (19) and DRA4_A -> A (17)."""
    import sys; sys.path.insert(0, str(ROOT / 'tools'))
    import export_stage_world as E
    E.SEED = ROOT / 'web/generated/mountain_stage_seed.hpp'
    if not E.SEED.exists(): return None
    stages, programs, words, globals_, handlers = E.seed('browser_stage_mountain')
    stage_of = {st[0]: st for st in stages}; course_of = {r['map_id']: r['course'] for r in m['residency']}
    names = {}
    for l in m['locations']:
        for i in load(ROOT / 'web/public' / l['root'].lstrip('/') / 'world_collision.json')['instances']:
            names[(i['rid'] << 8) | i['track']] = (l['code'], i.get('name'))
    rows = []
    for res, slots in handlers:
        st = stage_of.get(res & 255)
        if not st or len(slots) < 3 or slots[2] < 0: continue
        first, count = programs[st[1] + slots[2]]
        for builtin, keys, _ in E.calls_of(words, first, count, res):
            if builtin != 68: continue
            key0, action, entry = (keys.get(k, (1, None))[1] for k in (0, 1, 2))
            code, name = names.get(res, ('?', None))
            rows.append(dict(location=code, instance=name, resource=res, action=action, map_id=key0,
                             course=course_of.get(key0) if action in (0, 2) else None, entry=entry))
    return sorted(rows, key=lambda r: (r['location'], r['resource'], r['action'] if r['action'] is not None else -1))


def world_audio():
    """AUDIO/world/MOUNTAIN.json: the three peaks' world sounds (tools/export_world_audio.py per peak), merged by location
    (web/game-audio.js loads world/<course code>.json; the WATRIG table is the same on every peak)."""
    base = ASSETS / 'AUDIO/world'; merged = None
    for p in PEAKS:
        d = load(base / f'{p}.json')
        if merged is None: merged = dict(d, event=NAME, locations={})
        elif d['watrig'] != merged['watrig'] or d['units'] != merged['units']: raise SystemExit(f'{p}: world audio tables differ')
        for code, v in d['locations'].items(): merged['locations'].setdefault(code, v)
    (base / f'{NAME}.json').write_text(json.dumps(merged, separators=(',', ':')))
    return len(merged['locations'])


def attached(src=None, out=None):
    """MOUNTAIN/SETPIECES/attached.json: the peaks' ParentModifier parents and spline LiveComps (tools/export_peak_world.py attached_package)
    concatenated (resources carry the SDB track; web/peak-set-pieces.js AttachedSetPieces). Skipped while no peak has one."""
    src, out = Path(src or ASSETS), Path(out or OUT)
    docs = [load(src / p / 'SETPIECES/attached.json') for p in PEAKS if (src / p / 'SETPIECES/attached.json').exists()]
    if not docs: return None
    merged = dict(docs[0], location=NAME, splineLiveComps=[x for d in docs for x in d['splineLiveComps']], parents=[x for d in docs for x in d['parents']])
    (out / 'SETPIECES').mkdir(parents=True, exist_ok=True)
    (out / 'SETPIECES/attached.json').write_text(json.dumps(merged, separators=(',', ':')))
    return dict(parents=len(merged['parents']), spline_livecomps=len(merged['splineLiveComps']))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--measured', help='JSON {LOC: read ticks} measured on the PS2')
    a = p.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    m = manifest(load(a.measured) if a.measured else {})
    rows = request_rows(m)
    if rows is not None: m['requests'] = rows
    (OUT / 'peak.json').write_text(json.dumps(m, indent=1) + '\n')
    for r in rows or []:
        if r['location'] in ('ERA5_C', 'DRA4_A') and r['action'] in (0, 2): print('boundary request', r['location'], r['instance'], 'action', r['action'], '-> course', r['course'])
    print('manifest', len(m['locations']), 'locations', len(m['streaming']), 'streaming rows', len(m['residency']), 'residency rows')
    print('sections', sections())
    print('setpieces', setpieces())
    print('attached', attached())
    print('light glows', light_glow(), 'lighting banks', lighting_banks())
    print('world audio locations', world_audio())
    sizes = environment(m['locations'])
    print('environment slices', len(sizes), 'MB', round(sum(sizes.values()) / 1e6, 1), 'largest', sorted(sizes.items(), key=lambda x: -x[1])[:5])


if __name__ == '__main__':
    main()
