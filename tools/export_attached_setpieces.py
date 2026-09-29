#!/usr/bin/env python3
"""Attached set pieces (development export; --location, default ARA1): LiveComp players whose
entity carries a primary Spline modifier (the node animation composes onto the spline matrix) and
ParentModifier links (a child instance follows a parent entity's node). engine/parent_modifier.hpp.

Outputs (git-ignored):
  web/public/assets/[<LOC>/]LIVECOMP/attached.json     browser data (schema below)
  web/public/test-data/[<LOC>/]LIVECOMP/attached-snapshots.json   PS2 snapshot records (tests)
  web/generated/attached_seed_<LOC>.hpp                core seed (models, spline LiveComps, parents)

Sources: stage kind-16 handler rows (slots 1..5) and global handlers -> LUN programs (decoded like
tools/export_livecomp.py). builtin3 (0x2FBCB8 -> LiveComp 0x341AA0; key block defaults 0x4FB498)
followed in the same program by builtin19 (0x2FDED0 -> Spline) on the same instance = a spline
LiveComp; builtin18 (0x2FDC60, defaults 0x4FB758, key types 0x4461C0: parent, child, node int,
offset x/y/z float) = Parent(child <- parent node) via 0x355978 -> ctor 0x357038. Programs
already handled elsewhere keep their owners (Snow Jam 54 raven: spline piece in
set_piece_gameplay.inc; The Junction 39 blimp: resident looping spline). Resident pieces (built
at the load, before race tick 0) carry their LiveComp state from the location's race-tick-0
savestate (locations.state(LOC, 'ready')).

attached.json:
  splineLiveComps[]: resource, name, model, length, nodes (livecomp.json node schema: parent, bind
     rows, track {base, mask, curves [[a,b,c,d,t0,t1]...]}), matrix (authored rows, 16), scale,
     meshNodes (collision-source mesh index -> node), words (builtin3 key block, 11 words), program,
     slot, owner, spline (packed id), resident (bool), state (resident only: LiveComp words
     +0x00..+0x2C, channel segment caches, dirty) and drawn (runtime flag bit0 of the countdown audit).
  parents[]: program, slot, owner, parent (resource), parentName, child, childName, node, offset
     [x,y,z,0], childMatrix (authored rows, 16), childScale, childMeshes (mesh count), parentKind
     ('livecomp' = parent in LIVECOMP/livecomp.json (web/livecomp-animation.js), 'spline-livecomp' =
     parent in splineLiveComps (core), 'object' = static node path 0x34FED8, not ported).
"""
import argparse, glob, hashlib, json, struct, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from set_piece_location import Location, SNAPSHOTS, web_asset_dir  # noqa: E402
from export_startfire import decode_program  # noqa: E402
from export_rail_teeters import parse_model  # noqa: E402
from export_livecomp import mesh_nodes  # noqa: E402
from locations import state as location_state  # noqa: E402

GP = 0x4A30F0
B3_DEFAULT = None  # filled from the ELF in main()


def fb(b): return struct.unpack('<f', struct.pack('<I', b & 0xFFFFFFFF))[0]
def bits(x): return struct.unpack('<I', struct.pack('<f', float(x)))[0]


def verify_elf(elf):
    u = lambda a: struct.unpack_from('<I', elf, a - 0xFF000)[0]
    jal = lambda t: 0x0C000000 | (t >> 2)
    assert u(0x441F38 + 3 * 4) == 0x2FBCB8 and u(0x441F38 + 19 * 4) == 0x2FDED0 and u(0x441F38 + 18 * 4) == 0x2FDC60
    assert u(0x441F38 + 31 * 4) == 0x3000A8 and u(0x300200) == jal(0x28B180) and u(0x300214) == jal(0x297950)  # builtin31: audio
    assert [u(0x4461C0 + 4 * k) for k in range(7)] == [1, 1, 1, 2, 2, 2, 1]                   # builtin18 key types
    assert u(0x2FDE84) == jal(0x355978) and u(0x3559C4) == jal(0x357038) and u(0x3559D0) == jal(0x3554B0)
    assert u(0x3559A4) == 0x240400B0                                                        # 0xB0-byte ParentModifier
    assert u(0x357058) == jal(0x356EB8)
    for off, target in ((0x14, 0x3619B8), (0x1C, 0x357108), (0x94, 0x361940), (0xB4, 0x357210)):
        assert u(0x48F508 + off) == target, hex(off)
    assert u(0x490B10 + 0xD4) == 0x361090 and u(0x490B10 + 0xEC) == 0x3610E0 and u(0x490E80 + 0xD4) == 0x360990
    assert u(0x490B10 + 0xC4) == 0x356078 and u(0x48F250 + 0x94) == 0x361B90
    return u


def load(path):
    with zipfile.ZipFile(path) as z: return z.read('eeMemory.bin')


def entities(ee):
    """instance resource -> (entity, vtable, primary modifier) for every live Object/LiveComp entity."""
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    arr = memoryview(ee).cast('I'); out = {}
    for i in range(0x100000 // 4, len(arr) - 8):
        if arr[i] not in (0x490E80, 0x490B10): continue
        e = i * 4 - 12; inst = u(e + 0x18)
        if not 0x100000 < inst < 0x2000000 or u(inst + 0xC) != e: continue
        c = u(e + 0x1C); mod = u(c) if 0x100000 < c < 0x2000000 else 0
        out[u(inst + 0x78)] = (e, arr[i], mod, inst)
    return out


def livecomp_state(ee, entity, nodes):
    u = lambda a: struct.unpack_from('<I', ee, a & 0x1FFFFFF)[0]
    obj = entity - 0x30
    channels = u(obj + 0x64); animated = sum(1 for n in nodes if n['track'])
    return dict(object=obj, words=[u(obj + 4 * k) for k in range(12)], dirty=(u(obj + 0x40) >> 16) & 1,
                caches=[[u(channels + 0xD0 * c + 4 * k) for k in range(16)] for c in range(animated)],
                matrices=[u(u(obj + 0x60) + 4 * k) for k in range(16 * len(nodes))])


def main():
    p = argparse.ArgumentParser(description=__doc__); p.add_argument('--location', default='ARA1')
    p.add_argument('--snapshots', nargs='*', default=None, help='savestate globs (default: set_piece_location.SNAPSHOTS; ARA1 adds the full-course run)')
    a = p.parse_args()
    L = a.location; loc = Location(L); track = loc.track
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes(); u = verify_elf(elf)
    key_types3 = [u(0x445F48 + 4 * k) for k in range(11)]
    world = loc.world; programs = loc.programs; stage = loc.stage
    by_rid = {i['rid']: i for i in world['instances'] if i['track'] == track}
    res = lambda inst: (inst['rid'] << 8) | track
    audit = {r['resource']: r for r in loc.audit['instances']}
    descriptors = world['bindings'][str(track)]['descriptors']
    count, base = loc.rows
    sources = []
    for inst in world['instances']:
        if inst['track'] != track: continue
        r08 = descriptors[inst['collision_descriptor']]['resource08']
        if r08 == 0xFFFFFFFF or r08 & 255 != track: continue
        row = loc.row(r08)
        for slot in (1, 2, 4, 5):
            if row[slot] != 0xFFFFFFFF: sources.append((row[slot] >> 8, slot, inst))
    for g, w in enumerate(loc.globals):
        if w != 0xFFFFFFFF: sources.append((w >> 8, f'global{g}', None))
    livecomp_json = web_asset_dir(L, 'LIVECOMP') / 'livecomp.json'
    livecomp_resources = {x['resource'] for x in json.loads(livecomp_json.read_text())['instances']} if livecomp_json.exists() else set()
    spline_lc, parents = {}, []
    def model_nodes(inst):
        model = parse_model(loc.models[inst['model_resource'] >> 8])
        nodes = [dict(parent=n['parent'], bind=[fb(x) for x in n['bind']],
                      track=None if not n['track'] else dict(base=[fb(x) for x in n['track']['base']], mask=n['track']['mask'],
                                                             curves=[[[fb(x) for x in seg] for seg in c] for c in n['track']['curves']]))
                 for n in model['nodes']]
        return model, nodes
    for program, slot, own in sources:
        try: calls = decode_program(programs[program])
        except (KeyError, IndexError): continue
        current = own; pending = {}
        for builtin, _, keys in calls:
            if builtin == 44 and 0 in keys: current = by_rid.get(keys[0][1] >> 8)
            if builtin == 3:
                words = [0xFFFFFFFF, 1, 0, bits(-1), bits(-1), bits(30), bits(0), bits(-1), 0, 0, 0]
                for k, (kind, v) in keys.items():
                    if kind == 'resource': words[k] = v
                    elif kind == 'float': words[k] = bits(v)
                    else: words[k] = bits(float(v)) if key_types3[k] == 2 else v & 0xFFFFFFFF
                target = current if words[0] == 0xFFFFFFFF else by_rid.get(words[0] >> 8)
                if target is not None: pending[res(target)] = (target, words)
            elif builtin == 19:
                target = current if 0 not in keys else by_rid.get(keys[0][1] >> 8)
                if target is None or res(target) not in pending: continue
                inst, words = pending[res(target)]
                model, nodes = model_nodes(inst)
                if not any(n['track'] for n in nodes): continue
                spline = keys[1][1] if 1 in keys else None
                spline_lc[res(inst)] = dict(resource=res(inst), name=inst['name'], model=inst['model_resource'], length=fb(model['length']),
                                            nodes=nodes, matrix=inst['matrix'], scale=inst['scale'],
                                            meshNodes=mesh_nodes(loc.models[inst['model_resource'] >> 8]), words=words, program=program,
                                            slot=slot, owner=own['name'] if own else None, spline=spline,
                                            drawn=bool(audit.get(res(inst), {}).get('runtime_flags', 0) & 1))
            elif builtin == 18:
                parent = current if 0 not in keys else by_rid.get(keys[0][1] >> 8)
                child = by_rid.get(keys[1][1] >> 8) if 1 in keys else None
                if parent is None or child is None: continue
                node = keys[2][1] if 2 in keys else 0
                offset = [float(keys[k][1]) if k in keys else 0.0 for k in (3, 4, 5)] + [0.0]
                parents.append(dict(program=program, slot=slot, owner=own['name'] if own else None, parent=res(parent), parentName=parent['name'],
                                    child=res(child), childName=child['name'], node=node, offset=offset, childMatrix=child['matrix'],
                                    childScale=child['scale'], childMeshes=len(mesh_nodes(loc.models[child['model_resource'] >> 8]))))
    for x in parents:
        x['parentKind'] = 'spline-livecomp' if x['parent'] in spline_lc else 'livecomp' if x['parent'] in livecomp_resources else 'object'
    # Resident pieces: LiveComp state at race tick 0.
    ready_path = location_state(L, 'ready'); ready = load(ready_path) if ready_path.exists() else None
    ready_entities = entities(ready) if ready else {}
    for r, x in spline_lc.items():
        found = ready_entities.get(r)
        x['resident'] = bool(found and found[1] == 0x490B10 and found[2] and struct.unpack_from('<I', ready, found[2])[0] == 0x48F250)
        if x['resident']:
            st = livecomp_state(ready, found[0], x['nodes'])
            x['state'] = dict(words=st['words'], dirty=st['dirty'], caches=st['caches'])
    for x in parents:
        found = ready_entities.get(x['child'])
        x['resident'] = bool(found and found[2] and struct.unpack_from('<I', ready, found[2])[0] == 0x48F508)
    out = web_asset_dir(L, 'LIVECOMP'); out.mkdir(parents=True, exist_ok=True)
    (out / 'attached.json').write_text(json.dumps(dict(
        version=1, location=L, fps=60, coordinate_system='Original source centimeters, Z-up, row vectors p * M (native = (x, z, -y)/100)',
        source='tools/export_attached_setpieces.py', world_package_sha256=loc.audit['world_package_sha256'],
        splineLiveComps=sorted(spline_lc.values(), key=lambda x: x['resource']), parents=parents), separators=(',', ':')))
    # Snapshot records (tests): every ParentModifier child and every spline LiveComp.
    records_out = []
    globs = a.snapshots or [SNAPSHOTS[L]] + ([str(ROOT / 'local/ps2-capture/runs/setpieces/full*.tick*.p2s')] if L == 'ARA1' else [])
    paths = sorted({q for g in globs for q in glob.glob(g)}) + ([str(ready_path)] if ready else [])
    for path in paths:
        ee = load(path); U = lambda addr: struct.unpack_from('<I', ee, addr & 0x1FFFFFF)[0]
        tick = U(U(U(U(GP - 0x848) + 0x84) + 0xC) + 8)
        ents = entities(ee)
        for x in parents:
            c = ents.get(x['child'])
            if not c or not c[2] or U(c[2]) != 0x48F508: continue
            mod = c[2]
            rec = dict(snapshot=Path(path).name, tick=tick, kind='parent', child=x['child'], parent=x['parent'], node=U(mod + 0x90),
                       offset=[U(mod + 0x30 + 4 * k) for k in range(4)], dirty=U(mod + 0x44), matrix=[U(mod + 0x50 + 4 * k) for k in range(16)])
            pe = ents.get(x['parent'])
            if pe and pe[1] == 0x490B10:
                nodes = spline_lc[x['parent']]['nodes'] if x['parent'] in spline_lc else None
                obj = pe[0] - 0x30
                n = len(nodes) if nodes else U(U(pe[3] + 0x80) + 4)
                rec['parentState'] = dict(words=[U(obj + 4 * k) for k in range(12)], dirty=(U(obj + 0x40) >> 16) & 1,
                                          matrices=[U(U(obj + 0x60) + 4 * k) for k in range(16 * n)])
            records_out.append(rec)
        for r, x in spline_lc.items():
            e = ents.get(r)
            if not e or e[1] != 0x490B10 or not e[2] or U(e[2]) != 0x48F250: continue
            st = livecomp_state(ee, e[0], x['nodes'])
            records_out.append(dict(snapshot=Path(path).name, tick=tick, kind='spline-livecomp', resource=r, modifier=[U(e[2] + 0x60 + 4 * k) for k in range(16)],
                                    modifierDirty=U(e[2] + 0x58), distance=U(e[2] + 0x3C), **{k: st[k] for k in ('words', 'dirty', 'caches', 'matrices')}))
    __import__('disc_paths').test_data(out / 'attached-snapshots.json').write_text(json.dumps(dict(version=1, location=L, records=records_out), separators=(',', ':')))
    write_header(L, sorted(spline_lc.values(), key=lambda x: x['resource']), parents, loc.audit['world_package_sha256'])
    print(f'{L}: {len(spline_lc)} spline LiveComps {[x["name"] for x in spline_lc.values()]}, {len(parents)} parent links '
          f'({sum(x["parentKind"] == "livecomp" for x in parents)} on LiveComps, {sum(x["parentKind"] == "spline-livecomp" for x in parents)} on spline LiveComps), '
          f'{len(records_out)} snapshot records from {len(paths)} savestates')


def arr(values): return '{' + ','.join(f'0x{v & 0xFFFFFFFF:08x}u' for v in values) + '}'


def write_header(L, pieces, parents, sha):
    segs, curves, nodes, lines, links = [], [], [], [], []
    for x in pieces:
        first = len(nodes)
        for n in x['nodes']:
            tr = n['track']; fc = len(curves)
            if tr:
                for c in tr['curves']:
                    curves.append((len(segs), len(c))); segs.extend([bits(v) for v in seg] for seg in c)
            nodes.append(f'{{{n["parent"]},{arr(bits(v) for v in n["bind"])},{1 if tr else 0},{arr(bits(v) for v in (tr["base"] if tr else [0] * 6))},'
                         f'0x{(tr["mask"] if tr else 0):x}u,{fc}u,{len(curves) - fc}u}}')
        st = x.get('state')
        caches = [c for cs in (st['caches'] if st else []) for c in cs]
        lines.append(f'{{"{x["name"]}",{x["resource"]}u,{x["program"]}u,{1 if x["resident"] else 0}u,{1 if x["drawn"] else 0}u,{arr(bits(v) for v in x["matrix"])},0x{bits(x["scale"]):08x}u,'
                     f'0x{bits(x["length"]):08x}u,{arr(x["words"])},{first}u,{len(nodes) - first}u,{arr(st["words"] if st else [0] * 12)},{st["dirty"] if st else 0}u,'
                     f'{len(caches)}u,{arr(caches) if caches else "{}"}}}')
    for x in parents:
        if x['parentKind'] != 'spline-livecomp': continue
        links.append(f'{{{x["child"]}u,{x["parent"]}u,{x["node"]},{arr(bits(v) for v in x["offset"])},{arr(bits(v) for v in x["childMatrix"])},0x{bits(x["childScale"]):08x}u}}')
    ns = 'browser_attached_' + L.lower()
    text = f'''#pragma once
// Generated by tools/export_attached_setpieces.py --location {L} (bam.ssb models, stage programs, ELF-verified;
// resident LiveComp state from the race-tick-0 savestate). engine/parent_modifier.hpp, web/attached_setpieces.inc.
#include <array>
#include <cstdint>
#ifndef BROWSER_ATTACHED_TYPES
#define BROWSER_ATTACHED_TYPES
struct BrowserAttachedSegment {{uint32_t a,b,c,d,t0,t1;}};
struct BrowserAttachedCurve {{uint32_t first,count;}};
struct BrowserAttachedNode {{int32_t parent;std::array<uint32_t,16> bind;uint32_t animated;std::array<uint32_t,6> base;uint32_t mask,firstCurve,curveCount;}};
// A LiveComp whose entity carries the Spline modifier of the same resource. words: builtin3 key block;
// state: LiveComp words +0x00..+0x2C at race tick 0 (resident only), caches: channel segment caches (16 per channel).
template<size_t N> struct BrowserAttachedSplineLiveComp {{const char* name;uint32_t resource,program,resident,drawn;std::array<uint32_t,16> matrix;uint32_t scale,length;
 std::array<uint32_t,11> words;uint32_t firstNode,nodeCount;std::array<uint32_t,12> state;uint32_t dirty,cacheCount;std::array<uint32_t,N> caches;}};
// ParentModifier child <- (parent spline LiveComp, node) with offset (w 0); childMatrix = authored rows.
struct BrowserAttachedParent {{uint32_t child,parent;int32_t node;std::array<uint32_t,4> offset;std::array<uint32_t,16> childMatrix;uint32_t childScale;}};
#endif
namespace {ns} {{
inline constexpr const char* worldHash="{sha}";
inline constexpr std::array<BrowserAttachedSegment,{len(segs)}> segments={{{{{','.join('{' + ','.join(f'0x{v:08x}u' for v in s) + '}' for s in segs)}}}}};
inline constexpr std::array<BrowserAttachedCurve,{len(curves)}> curves={{{{{','.join(f'{{{a}u,{b}u}}' for a, b in curves)}}}}};
inline constexpr std::array<BrowserAttachedNode,{len(nodes)}> nodes={{{{{','.join(nodes)}}}}};
'''
    width = max([len([c for cs in (x.get('state') or {}).get('caches', []) for c in cs]) for x in pieces] + [1])
    text += f'inline constexpr std::array<BrowserAttachedSplineLiveComp<{width}>,{len(lines)}> splineLiveComps={{{{{",".join(lines)}}}}};\n'
    text += f'inline constexpr std::array<BrowserAttachedParent,{len(links)}> parents={{{{{",".join(links)}}}}};\n}}\n'
    text += (f'struct {ns}_ns {{static constexpr const auto& segments={ns}::segments;static constexpr const auto& curves={ns}::curves;'
             f'static constexpr const auto& nodes={ns}::nodes;static constexpr const auto& splineLiveComps={ns}::splineLiveComps;'
             f'static constexpr const auto& parents={ns}::parents;}};\n')
    (ROOT / f'web/generated/attached_seed_{L}.hpp').write_text(text)


if __name__ == '__main__':
    main()
