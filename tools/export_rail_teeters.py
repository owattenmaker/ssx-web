#!/usr/bin/env python3
"""Snow Jam log teeters (AnimTeeter + RailModifier) export (development; see engine/rail_modifier.hpp).

Authored handler rows 130..133 (slot 1, section activation) run programs 65..68:
    builtin6(gain 1000, restitution 0, maxSpeed 120, stiffness 0.5, rest 0)   -> AnimTeeter entity (3421A0)
    builtin48(rail, node 1) [x3 for logteetera_3000, x1 for each logbreakteeter] -> RailModifier (35B708)
Row 211 slot 2 (contact with mdl_ARA1_bcvolume_1001) runs program 144:
    builtin2(1) builtin3(fallingbb_1000, 0) builtin48(fallingbb_1000, rail 91, node 0) builtin48(.., rail 90, node 0)
(AnimObject 341AA0; decoded and reported, not exported for the browser).

Every program word, builtin table entry, call target, vtable slot, argument type table and
default argument constant used by the port is asserted against the ELF / bam.ssb. The model
animation data (node parents, bind matrices, curve segments) comes from the kind-2 model records
of bam.ssb chunk 33 (the loaded records are byte-identical, pointers relocated).
Writes local/event-activation/rail-teeters.json and web/generated/rail_teeter_seed.hpp.
"""
import hashlib, json, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from world_assets import world_chunks, records  # noqa: E402

ELF = (ROOT / 'local/disc/SLUS_207.72').read_bytes()


def u(a):
    return struct.unpack_from('<I', ELF, a - 0xFF000)[0]


def f(a):
    return struct.unpack_from('<f', ELF, a - 0xFF000)[0]


def jal(target):
    return 0x0C000000 | (target >> 2)


def cstr(a):
    o = a - 0xFF000
    return ELF[o:ELF.index(b'\0', o)].decode()


def fbits(x):
    return struct.unpack('<I', struct.pack('<f', x))[0]


GP = 0x4A30F0


def verify_code():
    assert u(0x441F38 + 6 * 4) == 0x2FB498 and u(0x441F38 + 48 * 4) == 0x2FF1C8
    assert u(0x441F38 + 2 * 4) == 0x2FC420 and u(0x441F38 + 3 * 4) == 0x2FBCB8
    assert u(0x2FB658) == jal(0x317D70) and u(0x2FB64C) == 0x24040080       # builtin6: alloc 0x80
    assert u(0x2FB66C) == jal(0x3421A0) and u(0x2FB664) == 0x24050001       # AnimTeeter ctor (type 1 base)
    assert u(0x2FB650) == 0x24A59678 and cstr(0x489678) == 'AnimTeeter'
    assert u(0x3421CC) == jal(0x34D9B0) and u(0x3421D0) == 0x24060004       # anim base ctor, node type 4
    assert u(0x2FF348) == jal(0x355E38) and u(0x355E88) == jal(0x35B708)   # builtin48 -> factory -> ctor
    assert u(0x355E6C) == 0x24040090 and cstr(0x48E9B8) == 'RailModifier'  # 0x90-byte RailModifier
    assert u(0x355E94) == jal(0x3556A8) and u(0x3556D8) == jal(0x35B670)   # container+0x1C rail list
    assert u(0x2FBE80) == jal(0x341AA0) and cstr(0x4896A8) == 'AnimObject'  # builtin3 (program 144)
    for off, target in ((0xC, 0x35BD70), (0x14, 0x35C540), (0x1C, 0x35C4E0), (0x24, 0x35CFE0)):
        assert u(0x4911D0 + off) == target, hex(off)
    for off, target in ((0x14, 0x356198), (0x7C, 0x342358), (0x84, 0x360DD0), (0xD4, 0x361090), (0xEC, 0x3610E0),
                        (0x10C, 0x34DD18), (0x15C, 0x342538), (0x164, 0x3569D0), (0x16C, 0x356A00), (0x19C, 0x34DC90),
                        (0x1A4, 0x34E348), (0xC4, 0x356078)):
        assert u(0x4908F8 + off) == target, hex(off)
    assert u(0x4908F8 + 0x78) & 0xFFFF == 0xFFBC and u(0x4908F8 + 0x158) & 0xFFFF == 0xFFBC
    assert u(0x490AD0 + 0x14) == 0x351800                                 # channel eval
    assert u(0x352D84) == 0x0060F809 and u(0x352D80) == 0x8C43001C          # 352D20: rail list vfn +0x1C
    assert u(0x356230) == 0x0040F809 and u(0x35622C) == 0x8C62007C          # 356198: entity vfn +0x7C (teeter)
    # Rider callers of entity vtable+0x15C (apply force): rail attach 106848, rail snap 106F78, wipeout 137D18.
    assert u(0x1069C8) == 0x8CE3015C and u(0x10750C) == 0x8C43015C and u(0x138588) == 0x8CE3015C
    assert u(0x1069A8) == 0x84E40158 and u(0x106934) == 0xC7818130  # lwc1 gp-0x7ED0 (v.z *= 0.1)
    # 342538 lever axis (0,1,0,0) at 0x4FF150 is .bss (filled at boot); checked by the live oracle.
    # builtin6 argument type table (0x445ED0) and static defaults (2FB4CC..2FB52C).
    assert [u(0x445ED0 + 4 * i) for i in range(11)] == [1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 1]
    assert [u(0x446470 + 4 * i) for i in range(4)] == [1, 1, 1, 0]
    assert u(0x2FB4DC) == 0xC783CC64 and u(0x2FB4F4) == 0xC781CC68            # lwc1 gp-0x339C / gp-0x3398
    assert fbits(f(GP - 0x339C)) == 0x3F19999A and fbits(f(GP - 0x3398)) == 0x3CA3D70A
    constants = {k: fbits(f(GP - k)) for k in (0x2B6C, 0x2B68, 0x2B64, 0x2B60, 0x2B5C, 0x2B58, 0x2AA8, 0x2A90)}
    assert constants == {0x2B6C: 0x3D088889, 0x2B68: 0x3D088889, 0x2B64: 0x358637BD, 0x2B60: 0x3D088889,
                         0x2B5C: 0x3DCCCCCD, 0x2B58: 0x3DCCCCCD, 0x2AA8: 0x501502F9, 0x2A90: 0x3C8EFA36}, constants
    return constants


# LUN operand opcodes (interpreter 2228C4, table 4797B0): 0x26 key=b1 float inline, 0x27 key=b1 int inline,
# 0x28 key=b1 int b2, 0x29 key=b1 float(b2); 0x21 builtin call (index b2, argc b3); 0x2A return.
def decode(words):
    calls, args, i = [], [], 0
    while i < len(words):
        w = words[i]
        op = w & 0xFF
        if op in (0x26, 0x27):
            v = words[i + 1]
            args.append((w >> 8 & 0xFF, 'f' if op == 0x26 else 'i', v))
            i += 2
            continue
        if op == 0x28:
            args.append((w >> 8 & 0xFF, 'i', w >> 16 & 0xFF))
        elif op == 0x29:
            args.append((w >> 8 & 0xFF, 'f', fbits(float(w >> 16 & 0xFF))))
        elif op == 0x21:
            argc = w >> 24
            calls.append((w >> 16 & 0xFF, args[len(args) - argc:]))
            del args[len(args) - argc:]
        elif op == 0x2A:
            assert i == len(words) - 1
        else:
            raise ValueError(f'unexpected opcode {op:#x}')
        i += 1
    assert not args
    return calls


# builtin6 block (2FB498): key k -> +4k, types 445ED0; int -> float conversion when the key is float.
B6_TYPES = [1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 1]
B6_DEFAULTS = [0xFFFFFFFF, 0x3F19999A, 0, 0x42700000, 0x3CA3D70A, 0xC1200000, 0x3F19999A, 0, 0xBF800000, 0xBF800000]


def builtin6_block(args):
    block = list(B6_DEFAULTS)
    for key, kind, value in args:
        if B6_TYPES[key] == 2 and kind == 'i':
            value = fbits(float(value))
        block[key] = value
    return block


def model_record(chunk, resource):
    for k, t, r, d in records(chunk):
        if k == 2 and (r << 8 | t) == resource:
            return d
    raise ValueError(f'model {resource:#x} missing')


def parse_model(d):
    count, table = struct.unpack_from('<2I', d, 4)
    nodes = []
    for i in range(count):
        parent, mesh, anim, bind = struct.unpack_from('<iIII', d, table + 16 * i)
        matrix = list(struct.unpack_from('<16I', d, bind)) if bind != 0xFFFFFFFF else \
            [fbits(x) for x in (1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1)]
        track = None
        if anim:
            base = list(struct.unpack_from('<6I', d, anim))
            mask, n, curves_at = struct.unpack_from('<3I', d, anim + 0x18)
            assert n == bin(mask).count('1')
            curves = []
            for c in range(n):
                cv = struct.unpack_from('<I', d, curves_at + 4 * c)[0]
                segs, at = struct.unpack_from('<2I', d, cv)
                curves.append([list(struct.unpack_from('<6I', d, at + 24 * s)) for s in range(segs)])
            track = dict(base=base, mask=mask, curves=curves)
        groups = struct.unpack_from('<I', d, mesh + 28)[0] if mesh not in (0, 0xFFFFFFFF) else 0
        nodes.append(dict(parent=parent, bind=matrix, track=track, meshGroups=groups))
    return dict(nodes=nodes, length=struct.unpack_from('<I', d, 0x14)[0])


def main():
    constants = verify_code()
    chunk = next(c for i, c in enumerate(world_chunks(ROOT / 'local/assets/source/ps2/bam.ssb')) if i == 33)
    stage = next(d for k, t, r, d in records(chunk) if k == 16)
    programs = {p['index']: p for p in json.loads((ROOT / 'local/browser-pickups/ara1-scripts.json').read_text())['programs']}
    world_path = ROOT / 'local/assets/native/ARA1/world_collision.json'
    world = json.loads(world_path.read_text())
    rails = {r['packed_id']: r for r in json.loads((ROOT / 'web/public/assets/ARA1/rails.json').read_text())['rails']}
    expected = {
        'mdl_ARA1_logteetera_3000': (130, 65, [0x8008, 0x8108, 0x8208]),
        'mdl_ARA1_logbreakteeter_1000': (131, 66, [0x1108]),
        'mdl_ARA1_logbreakteeter_1200': (132, 67, [0x1D08]),
        'mdl_ARA1_logbreakteeter_2000': (133, 68, [0x6608]),
    }
    teeter_words = [294, 0x447A0000, 552, 1321, 1062, 0x3F000000, 7865129, 84279841]  # builtin6(1:1000,2:0,5:0,4:.5,3:120)
    out = []
    for inst in world['instances']:
        if inst['name'] not in expected:
            continue
        row, program, rail_ids = expected[inst['name']]
        d = world['bindings'][str(inst['track'])]['descriptors'][inst['collision_descriptor']]
        assert d['resource08'] >> 8 == row, inst['name']
        handlers = struct.unpack_from('<6I', stage, 0x70 + row * 24)
        assert handlers == (0xFFFFFFFF, program << 8 | 8, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF), handlers
        words = programs[program]['code_words']
        assert words[:8] == teeter_words and words[-1] == 65322
        rail_words = []
        for rid in rail_ids:
            rail_words += [295, rid, 0x10228, 36700705]                       # builtin48(1:rail, 2:node 1)
        assert words[8:-1] == rail_words, inst['name']
        calls = decode(words)
        assert calls[0][0] == 6 and all(c[0] == 48 for c in calls[1:])
        block = builtin6_block(calls[0][1])
        assert block[1:6] == [0x447A0000, 0, 0x42F00000, 0x3F000000, 0] and block[6:] == B6_DEFAULTS[6:]
        attach = []
        for _, args in calls[1:]:
            a = {key: value for key, _, value in args}
            assert set(a) == {1, 2} and a[2] == 1                             # instance = current, node 1
            rail = rails[a[1]]
            attach.append(dict(packedId=a[1], node=a[2], name=rail['name'], segments=rail['segment_count']))
        model = parse_model(model_record(chunk, inst['model_resource']))
        for rail in attach:
            assert model['nodes'][rail['node']]['track'], 'rail node must be animated'
        resource = inst['rid'] << 8 | inst['track']
        out.append(dict(name=inst['name'], resource=resource, row=row, program=program, model=inst['model_resource'],
                        matrix=[fbits(x) for x in inst['matrix']], scale=fbits(inst['scale']),
                        boundsMin=[fbits(x) for x in inst['bounds_min_cm']], boundsMax=[fbits(x) for x in inst['bounds_max_cm']],
                        authoredFlags=d['flags'], teeterArgs=block, rails=attach, animModel=model))
    assert len(out) == 4
    # Program 144 (bcvolume_1001 contact): decoded for the report, not exported.
    words = programs[144]['code_words']
    assert words == [65832, 16908833, 39, 420616, 296, 33751585, 39, 420616, 295, 23304, 552, 53477921,
                     39, 420616, 295, 23048, 552, 53477921, 65322]
    assert struct.unpack_from('<6I', stage, 0x70 + 211 * 24)[2] == 144 << 8 | 8
    bc = [dict(builtin=b, args={k: v for k, _, v in a}) for b, a in decode(words)]
    report = dict(source='tools/export_rail_teeters.py', worldPackageSha256=hashlib.sha256(world_path.read_bytes()).hexdigest(),
                  constants={hex(k): hex(v) for k, v in constants.items()}, teeters=out, program144=bc)
    path = ROOT / 'local/event-activation/rail-teeters.json'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(report, indent=1) + '\n')
    write_header(out, report['worldPackageSha256'])
    print(f'{len(out)} teeters, {sum(len(t["rails"]) for t in out)} rails; program 144 = {bc}')


def arr(values):
    return '{' + ','.join(f'0x{v:08x}u' for v in values) + '}'


def write_header(teeters, sha):
    segs, curves, nodes, rails, lines = [], [], [], [], []
    for t in teeters:
        first_node, first_rail = len(nodes), len(rails)
        for n in t['animModel']['nodes']:
            tr = n['track']
            first_curve = len(curves)
            if tr:
                for c in tr['curves']:
                    curves.append((len(segs), len(c)))
                    segs.extend(c)
            nodes.append(f'{{{n["parent"]},{arr(n["bind"])},{1 if tr else 0},{arr(tr["base"] if tr else [0] * 6)},'
                         f'0x{(tr["mask"] if tr else 0):x}u,{first_curve}u,{len(curves) - first_curve}u,{n["meshGroups"]}u}}')
        for r in t['rails']:
            rails.append(f'{{0x{r["packedId"]:x}u,{r["node"]}}}')
        lines.append(f'{{"{t["name"]}",{t["resource"]}u,{t["program"]}u,0x{t["authoredFlags"]:x}u,{arr(t["matrix"])},0x{t["scale"]:08x}u,'
                     f'{arr(t["boundsMin"])},{arr(t["boundsMax"])},0x{t["animModel"]["length"]:08x}u,{arr(t["teeterArgs"])},'
                     f'{first_node}u,{len(nodes) - first_node}u,{first_rail}u,{len(rails) - first_rail}u}}')
    text = f'''#pragma once
// Generated by tools/export_rail_teeters.py (bam.ssb chunk 33 model records, programs 65..68, ELF-verified).
// Snow Jam log teeters: slot-1 (section activation) programs build an AnimTeeter entity (builtin6, 3421A0)
// and RailModifiers (builtin48, 35B708) binding authored rails to animated model nodes. engine/rail_modifier.hpp.
#include <array>
#include <cstdint>
struct BrowserRailTeeterSegment {{uint32_t a,b,c,d,t0,t1;}};
struct BrowserRailTeeterCurve {{uint32_t first,count;}};
struct BrowserRailTeeterNode {{int32_t parent;std::array<uint32_t,16> bind;uint32_t animated;std::array<uint32_t,6> base;uint32_t mask,firstCurve,curveCount,meshGroups;}};
struct BrowserRailTeeterRail {{uint32_t packedId;int32_t node;}};
// teeterArgs: the builtin6 argument block (+0 instance ref, +4 gain, +8 restitution, +0xC max speed, +0x10 stiffness,
// +0x14 rest, +0x18 damping, +0x1C flip mask (int), +0x20 minimum, +0x24 maximum; -1 = model range).
struct BrowserRailTeeterSeed {{const char* name;uint32_t resource,program,authoredFlags;std::array<uint32_t,16> matrix;uint32_t scale;
 std::array<uint32_t,3> boundsMin,boundsMax;uint32_t length;std::array<uint32_t,10> teeterArgs;uint32_t firstNode,nodeCount,firstRail,railCount;}};
inline constexpr const char* browserRailTeeterWorldHash="{sha}";
inline constexpr std::array<BrowserRailTeeterSegment,{len(segs)}> browserRailTeeterSegments={{{{{','.join('{' + ','.join(f'0x{v:08x}u' for v in s) + '}' for s in segs)}}}}};
inline constexpr std::array<BrowserRailTeeterCurve,{len(curves)}> browserRailTeeterCurves={{{{{','.join(f'{{{a}u,{b}u}}' for a, b in curves)}}}}};
inline constexpr std::array<BrowserRailTeeterNode,{len(nodes)}> browserRailTeeterNodes={{{{{','.join(nodes)}}}}};
inline constexpr std::array<BrowserRailTeeterRail,{len(rails)}> browserRailTeeterRails={{{{{','.join(rails)}}}}};
inline constexpr std::array<BrowserRailTeeterSeed,{len(lines)}> browserRailTeeterSeeds={{{{{','.join(lines)}}}}};
'''
    (ROOT / 'web/generated/rail_teeter_seed.hpp').write_text(text)


def export_location(code):
    """Other locations (--location ASS1): the resident extra location's log teeter (R&B: mdl_A_ASS1_logbreakteeter_1000,
    track 4 row 30 slot 1 program 13 = the Snow Jam builtin6 words + builtin48(rail 0x404, node 1)). Writes
    local/event-activation/<LOC>/rail-teeters.json and web/generated/rail_teeter_seed_<LOC>.hpp (namespace
    browser_rail_teeter_<loc>, the types of rail_teeter_seed.hpp)."""
    from export_stage_scripts import stage_records, parse_stage
    from locations import activation_dir
    constants = verify_code()
    stages, locs = stage_records()
    world_path = ROOT / f'local/assets/native/{code}/world_collision.json'
    world = json.loads(world_path.read_text())
    rails = {r['packed_id']: r for r in json.loads((ROOT / f'web/public/assets/{code}/rails.json').read_text())['rails']}
    teeter_words = [294, 0x447A0000, 552, 1321, 1062, 0x3F000000, 7865129, 84279841]
    chunks = list(world_chunks(ROOT / 'local/assets/source/ps2/bam.ssb'))
    out = []
    for track in [e['track'] for e in world['event_locations']]:
        programs, globals_, rows = parse_stage(stages[track])
        for inst in world['instances']:
            if inst['track'] != track: continue
            d = world['bindings'][str(track)]['descriptors'][inst['collision_descriptor']]
            if d['resource08'] == 0xFFFFFFFF: continue
            row = rows[d['resource08'] >> 8]
            if row[1] == 0xFFFFFFFF: continue
            body = programs[row[1] >> 8]; words = body[4:body[1] // 4]
            if words[:8] != teeter_words: continue
            assert row == (0xFFFFFFFF, row[1], 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF), (inst['name'], row)
            assert words[-1] == 65322
            calls = decode(words)
            assert calls[0][0] == 6 and all(c[0] == 48 for c in calls[1:])
            block = builtin6_block(calls[0][1])
            assert block[1:6] == [0x447A0000, 0, 0x42F00000, 0x3F000000, 0] and block[6:] == B6_DEFAULTS[6:]
            attach = []
            for _, args in calls[1:]:
                a = {key: value for key, _, value in args}
                assert set(a) == {1, 2} and a[2] == 1
                rail = rails[a[1]]
                attach.append(dict(packedId=a[1], node=a[2], name=rail['name'], segments=rail['segment_count']))
            model = None
            for chunk in chunks:
                try: model = parse_model(model_record(chunk, inst['model_resource'])); break
                except ValueError: continue
            assert model is not None
            for rail in attach:
                assert model['nodes'][rail['node']]['track'], 'rail node must be animated'
            out.append(dict(name=inst['name'], resource=inst['rid'] << 8 | track, row=d['resource08'] >> 8, program=row[1] >> 8, model=inst['model_resource'],
                            matrix=[fbits(x) for x in inst['matrix']], scale=fbits(inst['scale']),
                            boundsMin=[fbits(x) for x in inst['bounds_min_cm']], boundsMax=[fbits(x) for x in inst['bounds_max_cm']],
                            authoredFlags=d['flags'], teeterArgs=block, rails=attach, animModel=model))
    report = dict(source='tools/export_rail_teeters.py --location ' + code, worldPackageSha256=hashlib.sha256(world_path.read_bytes()).hexdigest(),
                  constants={hex(k): hex(v) for k, v in constants.items()}, teeters=out)
    path = activation_dir(code) / 'rail-teeters.json'; path.write_text(json.dumps(report, indent=1) + '\n')
    text = header_text(out, report['worldPackageSha256'])
    body = text[text.index('inline constexpr const char* browserRailTeeterWorldHash'):]
    for name in ('browserRailTeeterWorldHash', 'browserRailTeeterSegments', 'browserRailTeeterCurves', 'browserRailTeeterNodes', 'browserRailTeeterRails', 'browserRailTeeterSeeds'):
        body = body.replace(name + '=', name.replace('browserRailTeeter', '') + '=')
    ns = f'browser_rail_teeter_{code.lower()}'
    (ROOT / f'web/generated/rail_teeter_seed_{code}.hpp').write_text(
        f'#pragma once\n// Generated by tools/export_rail_teeters.py --location {code} (the stage programs of the event world\'s tracks, ELF-verified).\n'
        f'#include "rail_teeter_seed.hpp"\nnamespace {ns} {{\n{body}}}\n')
    print(f'{code}: {len(out)} teeters', [(t['name'], t['program'], [hex(r['packedId']) for r in t['rails']]) for t in out])


def header_text(teeters, sha):
    import io
    captured = {}
    real = Path.write_text
    def capture(self, text, *a, **k): captured['text'] = text
    Path.write_text = capture
    try: write_header(teeters, sha)
    finally: Path.write_text = real
    return captured['text']


if __name__ == '__main__':
    if len(sys.argv) > 2 and sys.argv[1] == '--location' and sys.argv[2] != 'ARA1': export_location(sys.argv[2])
    else: main()
