#!/usr/bin/env python3
"""Stage-world data of a location's race event (development export; git-ignored output
web/public/assets/<LOC>/STAGE/stage-world.json) for web/stage_world.inc (core) and web/prepare.py (draw batches).

Every LUN program of every stage of the location (web/generated/stage_scripts_seed.hpp: all tracks of the event world,
e.g. ARA1 tracks 3/8/9) is walked like the VM executes its straight-line calls: keyed pushes (ops 0x20/0x25..0x29 with
register loads 0x16/0x17/0x23), builtin44 (0x302968) switching the current instance (context +0x290), key 0 = -1 meaning
the current instance. Programs reach the world through their handler rows (slots 1..5) or the stage globals.

Output:
  meshanim: builtin13 (0x2FCFF0 -> MeshAnim 0x351B40) targets: resource, name, matrix / scale (bits), model nodes
            (parent, local matrix bits: node table *(model+8), 16-byte records {parent, mesh, anim, local*}) and meshNodes
            (collision-source mesh index -> node, for per-node draw batches), countdown runtime flags.
  script:   every instance whose flags a stage program changes at run time (builtins 1 Debounce, 2 SetNodeState,
            13 MeshAnim, 29 Hide, 58 Unhide): web/prepare.py gives them their own draw batches (script_resource) and the
            renderer follows the core's instance state (stage_world_instances).
  teleports: builtin34 (0x300770) destinations: resource, matrix (bits).
  collections: builtin38 (0x300F50 -> 0x30C4A8) lists by collection id: index (the career collect bit), resource, name.
  calls:    per program the decoded calls of those builtins (diagnostics).

usage: export_stage_world.py [--location ARA1|BRA2|BHP1]
"""
import argparse, json, re, struct, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from world_assets import world_chunks, records  # noqa: E402
from export_rail_teeters import parse_model  # noqa: E402
from export_livecomp import mesh_nodes  # noqa: E402
from locations import activation_dir  # noqa: E402

SEED = ROOT / 'web/generated/stage_scripts_seed.hpp'
INLINE = {0x14, 0x15, 0x16, 0x17, 0x1D, 0x24, 0x25, 0x26, 0x27}
SCRIPT_BUILTINS = {1, 2, 13, 29, 58}


def fbits(x): return struct.unpack('<I', struct.pack('<f', x))[0]
def f32(w): return struct.unpack('<f', struct.pack('<I', w & 0xFFFFFFFF))[0]


def seed(ns):
    text = SEED.read_text()
    def arr(name):
        m = re.search(r'namespace %s \{.*?%s=\{\{(.*?)\}\};' % (ns, name), text, re.S)
        if not m: raise SystemExit(f'{ns}.{name} missing from {SEED}')
        return m.group(1)
    nums = lambda s: [int(x.rstrip('u'), 0) for x in re.findall(r'-?(?:0x[0-9a-fA-F]+|\d+)u?', s)]
    stages = [nums(x) for x in re.findall(r'\{([^{}]*)\}', arr('stages'))]
    programs = [nums(x) for x in re.findall(r'\{([^{}]*)\}', arr('programs'))]
    words = nums(arr('words')); globals_ = nums(arr('globals'))
    handlers = [(int(m.group(1)), nums(m.group(2))) for m in re.finditer(r'\{(\d+)u,\{\{([^}]*)\}\}\}', arr('handlers'))]
    return stages, programs, words, globals_, handlers


def calls_of(words, first, count, owner):
    """Straight-line walk: [(builtin, {key: (type, raw)}, current instance)]."""
    hdr = words[first:first + count]; code = hdr[4:4 + (hdr[1] - 0x10) // 4]
    regs, stack, out, current, pc = {}, [], [], owner, 0
    while pc < len(code):
        w = code[pc]; op, b1, b2, b3 = w & 255, (w >> 8) & 255, (w >> 16) & 255, (w >> 24) & 255
        inline = (code[pc + 1] if pc + 1 < len(code) else None) if op in INLINE else None
        pc += 2 if op in INLINE else 1
        if op in (0x25, 0x27): stack.append((b1, 1, inline))
        elif op == 0x26: stack.append((b1, 2, inline))
        elif op == 0x28: stack.append((b1, 1, b2))
        elif op == 0x29: stack.append((b1, 2, fbits(float(b2))))
        elif op == 0x16: regs[b1] = (1, inline)
        elif op == 0x17: regs[b1] = (2, inline)
        elif op == 0x23 and b2 in regs: t, v = regs[b2]; regs[b1] = (t, (-v) & 0xFFFFFFFF if t == 1 else v ^ 0x80000000)
        elif op == 0x20: t, v = regs.get(b2, (0, None)); stack.append((b1, t, v))  # an untracked register (e.g. builtin61's result) keeps its slot
        elif op == 0x21:
            args = stack[len(stack) - b3:] if b3 else []; del stack[len(stack) - b3:]
            keys = {k: (t, v) for k, t, v in args}
            if b2 == 44 and 0 in keys: current = keys[0][1]
            out.append((b2, keys, current))
        elif op == 0x2A: break
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__); p.add_argument('--location', default='ARA1'); a = p.parse_args()
    code = a.location
    stages, programs, words, globals_, handlers = seed(f'browser_stage_{code.lower()}')
    world = json.loads((ROOT / f'web/public/assets/{code}/world_collision.json').read_text())
    by_res = {(i['rid'] << 8) | i['track']: i for i in world['instances']}
    audit = {r['resource']: r for r in json.loads((activation_dir(code) / 'countdown-instances.json').read_text())['instances']}
    stage_of = {s[0]: s for s in stages}
    runs = []  # (program index global, owner resource or None, slot)
    for res, slots in handlers:
        st = stage_of.get(res & 255)
        if not st: continue
        for slot, prog in enumerate(slots):
            if prog >= 0: runs.append((st[1] + prog, res, slot))
    for st in stages:
        for g in range(st[4]):
            prog = globals_[st[3] + g]
            if prog >= 0: runs.append((st[1] + prog, None, 'global'))
    script, meshanim_targets, teleports, calls, collections = set(), {}, {}, {}, {}
    for gp, owner, slot in runs:
        first, count = programs[gp]
        for builtin, keys, current in calls_of(words, first, count, owner):
            if builtin == 38:  # 0x300F50 collection append: id (key0), resources keys 1..20 in list order (index = position)
                cid = keys.get(0, (1, None))[1]
                collections.setdefault(str(cid), []).extend(v for k, (t, v) in sorted(keys.items()) if 1 <= k <= 20 and v not in (None, 0xFFFFFFFF))
                continue
            if builtin not in SCRIPT_BUILTINS and builtin != 34: continue
            key0 = keys.get(0, (1, 0xFFFFFFFF))[1]
            if key0 is None: continue
            target = current if key0 == 0xFFFFFFFF else key0
            if target is None or target not in by_res: continue
            calls.setdefault(str(gp), []).append(dict(builtin=builtin, slot=slot, owner=owner, target=target,
                                                      keys={str(k): (f32(v) if t == 2 and v is not None else v) for k, (t, v) in keys.items()}))
            if builtin == 34: teleports[target] = by_res[target]; continue
            script.add(target)
            if builtin == 13: meshanim_targets[target] = by_res[target]
    # Models of the MeshAnim targets (their tracks' kind-2 records).
    want = {i['model_resource'] for i in meshanim_targets.values()}
    models = {}
    for chunk in world_chunks(ROOT / 'local/assets/source/ps2/bam.ssb'):
        for kind, track, rid, data in records(chunk):
            key = (rid << 8) | track
            if kind == 2 and key in want and key not in models: models[key] = data
    meshanim = []
    for res, inst in sorted(meshanim_targets.items()):
        data = models.get(inst['model_resource'])
        if data is None: raise SystemExit(f'model {inst["model_resource"]:#x} of {inst["name"]} not found')
        model = parse_model(data)
        meshanim.append(dict(resource=res, name=inst['name'], model=inst['model_resource'], matrix_bits=[fbits(x) for x in inst['matrix']],
                             scale_bits=fbits(inst['scale']), nodes=[dict(parent=n['parent'], local_bits=n['bind']) for n in model['nodes']],
                             meshNodes=mesh_nodes(data), runtime_flags=audit.get(res, {}).get('runtime_flags'),
                             draw=audit.get(res, {}).get('draw')))
    out = dict(version=1, location=code, source=str(SEED.relative_to(ROOT)),
               meshanim=meshanim,
               script=[dict(resource=r, name=by_res[r]['name'], runtime_flags=audit.get(r, {}).get('runtime_flags'), draw=audit.get(r, {}).get('draw')) for r in sorted(script)],
               teleports=[dict(resource=r, name=i['name'], matrix_bits=[fbits(x) for x in i['matrix']]) for r, i in sorted(teleports.items())],
               collections={k: [dict(index=i, resource=r, name=by_res.get(r, {}).get('name')) for i, r in enumerate(v)] for k, v in collections.items()},
               calls=calls)
    target = ROOT / 'web/public/assets' / code / 'STAGE'; target.mkdir(parents=True, exist_ok=True)
    (target / 'stage-world.json').write_text(json.dumps(out, separators=(',', ':')) + '\n')
    print(json.dumps(dict(location=code, meshanim=len(meshanim), script=len(script), teleports=len(teleports), collections={k: len(v) for k, v in collections.items()}, output=str(target / 'stage-world.json'))))


if __name__ == '__main__':
    main()
