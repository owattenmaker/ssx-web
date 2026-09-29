#!/usr/bin/env python3
"""SplineModifier set pieces: verified script arguments (development export; --location, default
ARA1 writes local/event-activation/spline-setpieces.json, other locations
local/event-activation/<LOC>/spline-setpieces.json; never published). Outside ARA1 the programs are
every stage handler program (any slot) that calls builtin19; stage/track/inputs per location from
tools/set_piece_location.py.

Script VM (interpreter 0x2228C4, opcode table 0x4797B0) semantics used here, all read
from the original handlers:
  0x21 rD = builtin[b2](argc b3): args = the last argc entries of the argument stack
       (0x223F58: a2 = args + (count - argc) * 16, count -= argc afterwards)
  0x25/0x27 arg[b1] = inline word (type 1)   0x26 arg[b1] = inline float (type 2)
  0x28 arg[b1] = byte2 (type 1)              0x29 arg[b1] = float(byte2) (type 2)
  0x20 arg[b1] = r[b2]   0x16 r[b1] = inline int   0x17 r[b1] = inline float
  0x23 r[b1] = -r[b2]    0x03 r[b1] = r[b2] == r[b3]   0x02 if !r[b1] goto (word >> 16)
  0x1E r[b1] = null      0x1F return r[b1]   0x2A return
builtin19 (0x2FDED0): default block 0x4FB778 (instance -1, spline -1, end mode 1,
orientation 3, 20 km/h, rest 0 except running 1); each argument (field, value, type)
is stored raw when its type equals the field type (0x4461D8) and converted with
cvt.s.w when the field is a float. Instance -1 = the script's current instance
(context +0x290: the handler's owner, or builtin44's argument). Nothing is created
when the spline is -1 or the instance has no entity that accepts modifiers
(entity vt+0x84). Then 0x355AD0: allocate 0xF0, 0x359460, 0x3554B0 attach.
The linear walk below ignores branches: every program here only branches around its
builtin52 "already has an entity -> return" guard, which is reported per piece.
"""
import hashlib, json, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from world_assets import world_chunks, records  # noqa: E402
from set_piece_location import Location  # noqa: E402
from locations import activation_dir, pickup_file  # noqa: E402

INLINE = {0x14, 0x15, 0x16, 0x17, 0x1D, 0x24, 0x25, 0x26, 0x27}
FIELDS19 = ['instance', 'spline', 'end_mode', 'orientation', 'speed_kmh', 'roll_degrees', 'start_distance',
            'start_jitter', 'running', 'acceleration', 'acceleration_time']
BUILTINS = {0: 0x2FC0D0, 1: 0x2FC7D0, 3: 0x2FBCB8, 16: 0x2FD420, 19: 0x2FDED0, 26: 0x2FEE98, 31: 0x3000A8,
            44: 0x302968, 52: 0x303130, 55: 0x3019C8}
ENTITY = {0: 'Object (0x356DB0, vtable 0x490E80)', 3: 'AnimObject (0x341AA0, LiveComp vtable 0x490B10)'}


def f32(word):
    return struct.unpack('<f', struct.pack('<I', word & 0xFFFFFFFF))[0]


def main(location='ARA1'):
    loc = Location(location); track = loc.track; ara1 = location == 'ARA1'
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
    u = lambda a: struct.unpack_from('<I', elf, a - 0xFF000)[0]
    jal = lambda target: 0x0C000000 | (target >> 2)
    for index, target in BUILTINS.items():
        assert u(0x441F38 + 4 * index) == target, index
    assert u(0x4797B0 + 4 * 0x21) == 0x223F58 and u(0x4797B0 + 4 * 0x2A) == 0x224AB0
    # builtin19 default block writes (0x2FDF00..0x2FDF54) and field types 0x4461D8.
    assert [u(a) for a in (0x2FDF14, 0x2FDF18, 0x2FDF20, 0x2FDF30, 0x2FDF34, 0x2FDF40, 0x2FDF50)] == \
        [0x24050003, 0x3C0141A0, 0x24040001, 0xAC45000C, 0xE4410010, 0xAC440008, 0xAC440020]
    types = [u(0x4461D8 + 4 * i) for i in range(11)]
    assert types == [1, 1, 1, 1, 2, 2, 2, 2, 1, 2, 2], types
    defaults = dict(instance=-1, spline=0xFFFFFFFF, end_mode=1, orientation=3, speed_kmh=20.0, roll_degrees=0.0,
                    start_distance=0.0, start_jitter=0.0, running=1, acceleration=0.0, acceleration_time=0.0)
    assert u(0x2FE078) == jal(0x355AD0) and u(0x355B04) == jal(0x359460) and u(0x355B10) == jal(0x3554B0)
    assert u(0x355AF4) == 0x240400F0                                   # allocate 0xF0
    assert u(0x3594E4) == jal(0x3451C0) and u(0x3594F0) == jal(0x359688) and u(0x359554) == jal(0x317830)
    for offset, target in ((0x14, 0x359698), (0x1C, 0x359830), (0x2C, 0x359EB8), (0x34, 0x361B48), (0x3C, 0x361B50),
                           (0x8C, 0x361B88), (0x94, 0x361B90), (0xB4, 0x359CF8)):
        assert u(0x48F250 + offset) == target, hex(offset)
    for offset, target in ((0x14, 0x3618F8), (0x1C, 0x356FF0), (0x94, 0x361940)):
        assert u(0x48F5F0 + offset) == target, hex(offset)
    assert u(0x3578E8) == jal(0x355F10) and u(0x355FE4) == jal(0x355918)   # type-13 conversion freezes the matrix
    assert u(0x2FD70C) == jal(0x3578A8) and u(0x2FAF60) == jal(0x3578A8)   # builtin16 -> type-13 entity
    assert u(0x490E80 + 0x114) == 0x34FD00 and u(0x490B10 + 0x14) == 0x356198

    stage = loc.stage; row_base = loc.rows[1]
    scripts = json.loads(pickup_file(location, 'scripts').read_text())['programs']
    world = loc.world
    audit = json.loads((activation_dir(location) / 'countdown-instances.json').read_text())
    runtime = {r['resource']: r for r in audit['instances']}
    rails = {r['packed_id']: r for r in json.loads((ROOT / f'local/assets/native/{location}/rails.json').read_text())['rails']}
    names, slots = {}, {}
    for inst in world['instances']:
        resource = (inst['rid'] << 8) | inst['track']
        names[resource] = inst['name']
        d = world['bindings'][str(inst['track'])]['descriptors'][inst['collision_descriptor']]
        if d['resource08'] != 0xFFFFFFFF and d['resource08'] & 255 == track:   # the location track's stage handler table
            row = struct.unpack_from('<6I', stage, row_base + (d['resource08'] >> 8) * 24)
            slots[resource] = [None if w == 0xFFFFFFFF else w >> 8 for w in row]

    def decode(program):
        words = scripts[program]['code_words']
        i, regs, stack, calls = 0, {}, [], []
        while i < len(words):
            w = words[i]; op, b1, b2, b3 = w & 255, (w >> 8) & 255, (w >> 16) & 255, w >> 24
            inline = words[i + 1] if op in INLINE else None
            if op == 0x21:
                argc = b3
                if argc > len(stack):
                    raise ValueError(f'program {program}: builtin{b2} takes {argc} of {len(stack)} arguments')
                args = stack[len(stack) - argc:]; del stack[len(stack) - argc:]
                calls.append(dict(word=i, builtin=b2, args=args))
                regs[b1] = None
            elif op in (0x25, 0x27): stack.append((b1, 1, inline))
            elif op == 0x26: stack.append((b1, 2, inline))
            elif op == 0x28: stack.append((b1, 1, b2))
            elif op == 0x29: stack.append((b1, 2, struct.unpack('<I', struct.pack('<f', float(b2)))[0]))
            elif op == 0x20: stack.append((b1,) + regs[b2])
            elif op == 0x16: regs[b1] = (1, inline)
            elif op == 0x17: regs[b1] = (2, inline)
            elif op == 0x23:
                t, v = regs[b2]
                regs[b1] = (t, (-v) & 0xFFFFFFFF) if t == 1 else (t, v ^ 0x80000000)
            elif op in (0x02, 0x03, 0x1E, 0x1F, 0x2A):
                pass
            elif op == 0x08:   # x < y into a register (Crow's Nest osprey: random(0, 100) < 60 skips the flight)
                regs[b1] = None
            else:
                raise ValueError(f'program {program}: unhandled opcode {op:#x} at word {i}')
            i += 2 if op in INLINE else 1
        if stack:
            raise ValueError(f'program {program}: {len(stack)} arguments left on the stack')
        return calls

    def fields19(args):
        out = dict(defaults)
        for field, typ, value in args:
            name = FIELDS19[field]
            want = types[field]
            if want == 2:
                out[name] = f32(value) if typ == 2 else float(struct.unpack('<i', struct.pack('<I', value))[0])
            else:
                out[name] = struct.unpack('<i', struct.pack('<I', value))[0] if name != 'spline' else value
        return out

    def owner_of(program):
        owners = [(r, s.index(program)) for r, s in slots.items() if program in s]
        return owners

    pieces, programs = [], {}
    if ara1: program_list = (54, 120, 131, 136, 137, 142)
    else:
        program_list = []
        for program in sorted({p for row in slots.values() for p in row if p is not None}):
            try: calls = decode(program)
            except (ValueError, KeyError, TypeError): continue
            if any(c['builtin'] == 19 for c in calls): program_list.append(program)
    for program in program_list:
        calls = decode(program)
        owners = owner_of(program)
        current = owners[0][0] if len(owners) == 1 else None
        guards, entity, entity_args, rng_draws = {}, {}, {}, 0
        last52 = None; random_gate = False
        for call in calls:
            b, args = call['builtin'], call['args']
            if b == 44:
                current = args[0][2]
            elif b == 52:
                last52 = args[0][2]
            elif b == 77:   # 0x303598 random float: a later builtin19 runs only on some branches (launched by the core VM)
                random_gate = True
            elif b in (0, 3):
                entity[current] = b
                entity_args[current] = [dict(field=f, type=t, value=(f32(v) if t == 2 else v)) for f, t, v in args]
            elif b == 19:
                a = fields19(args)
                target = current if a['instance'] == -1 else a['instance']
                rail = rails.get(a['spline'])
                if rail is None:
                    raise ValueError(f'program {program}: spline {a["spline"]:#x} is not a kind-8 record')
                row = slots.get(target, [None] * 6)
                r = runtime[target]
                kind = entity.get(target)
                flags = r['runtime_flags']
                # Entity draw (export_event_membership draw_class): bit 4, which the entity setup
                # derives from flags & 3 == 3 (raven 0x10003 -> 0x10145; flag-2 pieces -> 0x142/0x342).
                drawn = (flags & 3) == 3
                rng_draws += 1
                pieces.append(dict(
                    instance=names[target], resource=target, program=program, word=call['word'],
                    trigger=[dict(instance=names[o], slot=s) for o, s in owners],
                    guard_builtin52=last52 == target, entity=ENTITY.get(kind, 'existing'),
                    entity_builtin=kind, entity_args=entity_args.get(target),
                    arguments=[dict(field=FIELDS19[f], type=t, value=(f32(v) if types[f] == 2 and t == 2 else v)) for f, t, v in args],
                    resolved=dict(a, spline=a['spline']),
                    spline=dict(resource=a['spline'], name=rail['name'], segments=rail['segment_count'],
                                length_cm_json=rail['total_length_cm']),
                    derived=dict(speed_cms=a['speed_kmh'] * 27.77777862548828, roll_radians=a['roll_degrees'] * 0.01745329424738884,
                                 end='loop' if a['end_mode'] not in (0, 2, 4) else ('ping-pong' if a['end_mode'] == 2 else 'stop'),
                                 seconds_per_pass=rail['total_length_cm'] / abs(a['speed_kmh'] * 27.77777862548828)),
                    countdown_runtime_flags=flags, drawn_while_on_path=drawn, **(dict(vm_launch=True) if random_gate else {}),
                    end_handler_slot4=row[4], restore_without_slot4=row[4] is None))
                last52 = None
        programs[program] = dict(sha256=scripts[program]['sha256'], owners=[dict(instance=names[o], slot=s) for o, s in owners],
                                 builtins=[c['builtin'] for c in calls], spline_modifiers=rng_draws)
    # Slot-4 end handlers: builtin16 (0x2FD420) particle burst -> 0x2FAE38 type-13 conversion.
    for p in pieces:
        h = p['end_handler_slot4']
        if p['resolved']['end_mode'] in (0, 4):
            if h is not None and [c['builtin'] for c in decode(h)] == [2]:   # Crow's Nest osprey: slot 4 only kills the node
                p['end_behaviour'] = f'finished -> entity vt+0x114(1) runs slot-4 program {h} (builtin2 SetNodeState); not drawn'
                continue
            if h is not None and [c['builtin'] for c in decode(h)] == [73]:   # Happiness osprey: slot 4 only stops its sound (300260)
                p['end_behaviour'] = f'finished -> entity vt+0x114(1) runs slot-4 program {h} (builtin73 stop sound)'
                continue
            if h is not None and [c['builtin'] for c in decode(h)] == [2, 73]:   # Style Mile osprey: kills the node, stops its sound
                p['end_behaviour'] = f'finished -> entity vt+0x114(1) runs slot-4 program {h} (builtin2 SetNodeState, builtin73 stop sound); not drawn'
                continue
            if h is None:   # Schizophrenia rocketCore (docs/peak2.md): no slot-4 row, vt+0x114(1) runs nothing (restore_without_slot4)
                p['end_behaviour'] = 'finished -> no slot-4 program: the entity stays where the path ended'
                continue
            if 16 not in [c['builtin'] for c in decode(h)]:
                raise ValueError(f'{p["instance"]}: unexpected end handler {h} (builtins {[c["builtin"] for c in decode(h)] if h is not None else None})')
            p['end_behaviour'] = (f'finished -> entity vt+0x114(1) runs slot-4 program {h} (builtin16): type-13 entity '
                                  '0x48EE60 replaces the entity, PositionModifier with the spline matrix at L-0.1; not drawn')
        else:
            p['end_behaviour'] = 'never finishes (loop)'
    expected = {} if not ara1 else {
        'mdl_ARA1_ravensplineanima_1000': (0x5408, 1, 45.0, -90.0, 3),
        'mdl_ARA1_brocket_1000': (0xA908, 0, 150.0, 0.0, 0), 'mdl_ARA1_brocket_1001': (0xAA08, 0, 150.0, 0.0, 0),
        'mdl_ARA1_spintwin_1000': (0xA208, 0, 160.0, 0.0, 3), 'mdl_ARA1_spintwin_1001': (0xA108, 0, 160.0, 0.0, 3),
        'mdl_ARA1_chasingdragon_1000': (0xA608, 0, 90.0, 0.0, 0), 'mdl_ARA1_chasingdragon_1001': (0xA508, 0, 90.0, 0.0, 0),
        'mdl_ARA1_chasingdragon_1100': (0xA408, 0, 120.0, 0.0, 0), 'mdl_ARA1_chasingdragon_1101': (0xA308, 0, 120.0, 0.0, 0),
        'mdl_ARA1_EZrocketCore_1000': (0xA708, 0, 150.0, 1.194, 0), 'mdl_ARA1_EZrocketCore_1001': (0xA808, 0, 150.0, None, 0),
    }
    got = {p['instance']: p for p in pieces}
    for name, (spline, end, speed, roll, entity) in expected.items():
        p = got[name]['resolved']
        assert p['spline'] == spline and p['end_mode'] == end and p['speed_kmh'] == speed and got[name]['entity_builtin'] == entity, (name, p)
        if roll is not None and name != 'mdl_ARA1_EZrocketCore_1000':
            assert p['roll_degrees'] == roll, (name, p)
        assert p['orientation'] == 3 and p['running'] == 1 and p['start_distance'] == 0 and p['start_jitter'] == 0
        assert p['acceleration'] == 0 and p['acceleration_time'] == 0
    target = activation_dir(location) / 'spline-setpieces.json'
    target.write_text(json.dumps(dict(
        elf_sha256=hashlib.sha256(elf).hexdigest(), scope=('Snow Jam' if ara1 else location) + ' SplineModifier (builtin19) users: exact script arguments, '
        'triggers, entity kind, draw class and end behaviour. Development export (never published).',
        **({} if ara1 else dict(location=location)),
        builtin19_defaults=defaults, field_types=types, programs={str(k): v for k, v in programs.items()}, pieces=pieces), indent=1) + '\n')
    for p in pieces:
        r = p['resolved']
        print(f'{p["instance"]:32s} prog {p["program"]:3d} {p["spline"]["name"]:30s} end {r["end_mode"]} orient {r["orientation"]} '
              f'{r["speed_kmh"]:6.1f} km/h roll {r["roll_degrees"]:7.3f} entity {p["entity_builtin"]} drawn {p["drawn_while_on_path"]} '
              f'slot4 {p["end_handler_slot4"]} guard {p["guard_builtin52"]}')


if __name__ == '__main__':
    import argparse
    ap = argparse.ArgumentParser(description=__doc__); ap.add_argument('--location', default='ARA1')
    main(ap.parse_args().location)
