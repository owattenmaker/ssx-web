#!/usr/bin/env python3
"""Export a location's stage-script particle effects (builtin16 Particle / builtin26 DynamicParticle)
for the browser core (development export; git-ignored outputs):

  web/public/assets/<LOC>/PARTICLES/particles.json      (ARA1 too: .../ARA1/PARTICLES/)
  web/generated/set_piece_particles_<LOC>.hpp            (--header; namespace browser_set_piece_particles_<loc>)

Everything is derived from original data and asserted here (see engine/set_piece_particles.hpp for the
runtime semantics):
* Stage LUN programs (local/browser-pickups/.../disassembly.json) are decoded with the script VM's own
  argument rules (interpreter 0x2228C4, opcode table 0x4797B0): 0x25/0x27 arg = inline word (type 1),
  0x26 inline float (type 2), 0x28 arg = byte2 (type 1), 0x29 arg = float(byte2) (type 2), 0x16/0x17
  register int/float, 0x23 negate (negu / neg.s by register type), 0x20 arg = register, 0x21 builtin
  call consumes the last argc arguments. builtin55(G) + `if !r goto T` gates [.., T) on the owner
  LiveComp's time crossing G/30 s; builtin52(inst) == 1 -> return guards; builtin44 sets the current
  instance (script context +0x290).
* Parameter blocks: defaults of 0x2FD420 / 0x2FEE98 (0x4FB640 / 0x4FB938) + keyed arguments with the
  field types 0x4460B0 / 0x446380 (int literal for a float field -> cvt.s.w). builtin26 always clamps
  LifeR <= Life; builtin16 clamps only when the target instance has no entity yet (new type-13 entity),
  not when it converts an existing carrier (slot 4 at a spline end).
* Owners/slots: stage kind-16 handler rows (tools/set_piece_location.py): slot 1 section activation,
  2 selected-rider contact, 4 LiveComp/spline finished, 5 LiveComp timer tick.
* Initial effects: every Particle / DynamicParticle object alive in the location's race-tick-0 savestate
  (e.g. ARA1 snow-jam-ready: the snow wind emitters built during the load) with its emitter image and
  the 0x4FF018 (0x3177F0) state of that savestate.
* Textures: table 0x4891B0 (stride 12, 4-byte tag), PARTICLE.SSH; blend: table 0x44B420 (GS ALPHA per
  BlendMode, draw 0x3708C0 / 0x371380).

usage: export_set_piece_particles.py [--location ARA1|BRA2|BHP1] [--header]
"""
import argparse, json, struct, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from set_piece_location import Location  # noqa: E402
from locations import state as location_state  # noqa: E402

ELF = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
GP = 0x4A30F0
FIELDS = ('NumParticles', 'NumBlur', 'Duration', 'Damp', 'Size', 'Life', 'SizeR', 'LifeR', 'BlurStep',
          'OffX', 'OffY', 'OffZ', 'R0X', 'R0Y', 'R0Z', 'R1X', 'R1Y', 'R1Z', 'VelX', 'VelY', 'VelZ',
          'R0VX', 'R0VY', 'R0VZ', 'R1VX', 'R1VY', 'R1VZ', 'R2VX', 'R2VY', 'R2VZ', 'ForceX', 'ForceY', 'ForceZ',
          'StartColA', 'StartColR', 'StartColG', 'StartColB', 'EndColA', 'EndColR', 'EndColG', 'EndColB',
          'R0A', 'R0R', 'R0G', 'R0B', 'R1A', 'R1R', 'R1G', 'R1B', 'TextureId', 'BlendMode', 'SizeFinal',
          'NumFlipTextures', 'FlipTextureRate', 'Instance', 'Node', 'OffsetX', 'OffsetY', 'OffsetZ')
INT_FIELDS = {'NumParticles', 'NumBlur', 'TextureId', 'BlendMode', 'NumFlipTextures', 'Instance', 'Node'}


def u32(a): return struct.unpack_from('<I', ELF, a - 0xFF000)[0]
def f32(w): return struct.unpack('<f', struct.pack('<I', w & 0xFFFFFFFF))[0]
def fbits(x): return struct.unpack('<I', struct.pack('<f', x))[0]


def verify_code():
    """Addresses the runtime port relies on (fails closed if the ELF differs)."""
    jal = lambda t: 0x0C000000 | (t >> 2)
    b = lambda i: u32(0x441F38 + 4 * i)
    assert (b(16), b(26), b(30), b(44), b(52), b(55), b(69)) == (0x2FD420, 0x2FEE98, 0x2FFF00, 0x302968, 0x303130, 0x3019C8, 0x302490)
    assert u32(0x4797B0 + 4 * 0x21) == 0x223F58 and u32(0x4797B0 + 4 * 0x28) == 0x2249D0 and u32(0x4797B0 + 4 * 0x29) == 0x224A3C
    assert u32(0x2FD6A4) == jal(0x2FAE38) and u32(0x2FD70C) == jal(0x3578A8) and u32(0x2FF168) == jal(0x355D10)
    assert u32(0x3578F4) == jal(0x355CB0) and u32(0x355CE8) == jal(0x3458C0) and u32(0x355D88) == jal(0x345C90)
    assert u32(0x3458EC) == jal(0x370018) and u32(0x345974) == jal(0x3705E0) and u32(0x345D38) == jal(0x370DC8)
    assert u32(0x3706C4) == jal(0x36CBF8) and u32(0x3706E0) == jal(0x370058) and u32(0x3700D0) == jal(0x36CCB8)
    assert u32(0x4912B0 + 0x2C) == 0x345B40 and u32(0x4912B0 + 0x34) == 0x345BC8 and u32(0x491268 + 0x2C) == 0x345F90
    assert u32(0x491268 + 0x1C) == 0x346060 and u32(0x491268 + 0x34) == 0x346070 and u32(0x48EE60 + 0x14) == 0x357950
    assert u32(0x352D54) & 0xFC00003F == 0x00000009   # 0x352D20 effect update loop (jalr)
    return True


def defaults(builtin):
    w = [0] * 60
    for k in range(5, 33): w[k] = fbits(0.0)
    w[0], w[1], w[2], w[3], w[4] = 1, 0, fbits(-1.0), fbits(1.0), fbits(4.0)
    w[33] = fbits(1.0); w[34:37] = [0, 0, 0]; w[37:42] = [fbits(1.0)] * 5; w[42:45] = [0, 0, 0]; w[45] = fbits(1.0); w[46:49] = [0, 0, 0]
    w[49], w[50], w[51], w[52], w[53], w[54] = 16, 0, 0, 1, fbits(20.0), 0xFFFFFFFF
    if builtin == 26: w[55:59] = [0, 0, 0, 0]
    # Cross-check a few default stores against the ELF prologues.
    base = {16: 0x2FD420, 25: 0x2FE840, 26: 0x2FEE98}[builtin]
    assert u32(base + (0x40 if builtin == 25 else 0x38)) == 0x3C01BF80   # lui at,0xBF80 (Duration -1)
    return w


TYPES = {16: [u32(0x4460B0 + 4 * i) for i in range(58)], 25: [u32(0x4462A0 + 4 * i) for i in range(58)],
         26: [u32(0x446380 + 4 * i) for i in range(62)]}
assert TYPES[25] == TYPES[16]


def apply(builtin, words, key, typ, raw):
    field = TYPES[builtin][key]
    if typ != field and field == 2: words[key] = fbits(float(struct.unpack('<i', struct.pack('<I', raw & 0xFFFFFFFF))[0]))
    else: words[key] = raw & 0xFFFFFFFF


def run_program(program):
    """Linear VM walk: every builtin call with its arguments, active builtin55 gates and builtin52 guard."""
    regs = {}; stack = []; calls = []; gates = []; guard = None
    ins = program['instructions']
    for n, i in enumerate(ins):
        op, w, inline, at = i['opcode'], i['word'], i['inline_words'], i['word_index']
        b1, b2, b3 = (w >> 8) & 255, (w >> 16) & 255, (w >> 24) & 255
        gates = [g for g in gates if at < g[1]]
        if guard is not None and guard['scope'] is not None and at >= guard['scope']: guard = None
        if op == 0x25 or op == 0x27: stack.append((b1, 1, inline[0]))
        elif op == 0x26: stack.append((b1, 2, inline[0]))
        elif op == 0x28: stack.append((b1, 1, b2))
        elif op == 0x29: stack.append((b1, 2, fbits(float(b2))))
        elif op == 0x16: regs[b1] = (1, inline[0])
        elif op == 0x17: regs[b1] = (2, inline[0])
        elif op == 0x23:
            t, v = regs[b2]; regs[b1] = (t, (-v) & 0xFFFFFFFF if t == 1 else v ^ 0x80000000)
        elif op == 0x20: t, v = regs[b2]; stack.append((b1, t, v))
        elif op == 0x21:
            argc = i['argument_count']; args = stack[len(stack) - argc:] if argc else []; del stack[len(stack) - argc:]
            bi, dest = i['builtin'], i['destination']
            calls.append(dict(word_index=at, builtin=bi, dest=dest, args=args, gates=[g[0] for g in gates],
                              guard=None if guard is None else {k: v for k, v in guard.items() if k != 'scope'}))
            nxt = ins[n + 1] if n + 1 < len(ins) else None
            if bi == 55 and nxt and nxt['opcode'] == 0x02 and (nxt['word'] >> 8) & 255 == dest:
                gates.append((args[0][2], (nxt['word'] >> 16) & 0xFFFF))
            if bi == 52:   # builtin52(inst); r = int V; rX = r == rV; if !rX goto T; rY = null; return rY
                seq = ins[n + 1:n + 6]; ops = [x['opcode'] for x in seq]
                if ops[:5] == [0x16, 0x03, 0x02, 0x1E, 0x1F]:
                    # The return ends the whole program for this tick; it guards the rest of its gate region.
                    guard = dict(instance=args[0][2], equals=seq[0]['inline_words'][0], returns_when_equal=True,
                                 scope=gates[-1][1] if gates else None)
        elif op == 0x2A: break
    return calls


def texture_name(tid):
    at = 0x4891B0 + 12 * tid
    return bytes(ELF[at - 0xFF000:at - 0xFF000 + 4]).split(b'\0')[0].decode('latin1')


def particle_objects(memory, track):
    """Particle (0x4912B0) and DynamicParticle (0x491268) objects in a savestate."""
    import array
    u = lambda a: struct.unpack_from('<I', memory, a & 0x1FFFFFF)[0]
    words = array.array('I', memory); out = []
    for idx in range(0x100000 // 4, len(words) - 4):
        v = words[idx]
        if v not in (0x4912B0, 0x491268): continue
        obj = idx * 4 - 8; inst = u(obj + 0xC)
        if not 0x100000 < inst < 0x2000000: continue
        resource = u(inst + 0x78); entity = u(inst + 0xC)
        if not 0x100000 < entity < 0x2000000: continue
        container = u(entity + 0x1C); found = False; e = u(container + 0x10) if container else 0
        while 0x100000 < e < 0x2000000:
            if e == obj: found = True; break
            e = u(e)
        if not found: continue
        rec = dict(object=obj, resource=resource, entity=entity, entity_vtable=u(entity + 0xC))
        if v == 0x4912B0:
            rec.update(kind='Particle', matrix=[u(obj + 0x10 + 4 * k) for k in range(16)],
                       emitter=[u(obj + 0x50 + 4 * k) for k in range(0x190 // 4)], dead=u(obj + 0x1E4),
                       block=[u(u(obj + 0x1E0) + 4 * k) for k in range(0xE0 // 4)])
        else:
            E = obj + 0x60; cap = struct.unpack('<i', struct.pack('<I', u(E + 0x178)))[0]
            rec.update(kind='DynamicParticle', node=u(obj + 0x10), stopped=u(obj + 0x14), dead=u(obj + 0x18),
                       words_20_60=[u(obj + 0x20 + 4 * k) for k in range(16)], emitter=[u(E + 4 * k) for k in range(0x200 // 4)],
                       ring_position=[u(u(E + 0x1A0) + 4 * k) for k in range(4 * max(cap, 0))],
                       ring_velocity=[u(u(E + 0x1A4) + 4 * k) for k in range(4 * max(cap, 0))],
                       block=[u(u(obj + 0x260) + 4 * k) for k in range(0xF0 // 4)])
        out.append(rec)
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__); p.add_argument('--location', default='ARA1'); p.add_argument('--header', action='store_true')
    a = p.parse_args(); verify_code()
    loc = Location(a.location); track = loc.track
    names = {(i['rid'] << 8) | i['track']: i['name'] for i in loc.world['instances']}
    by_name = {i['name']: i for i in loc.world['instances']}
    owners = {}
    for inst, row in loc.handler_rows():
        for slot, w in enumerate(row):
            if w != 0xFFFFFFFF and w & 255 == track: owners.setdefault(w >> 8, []).append(dict(owner=inst['name'], resource=loc.resource(inst), slot=slot))
    programs = {p['index']: p for p in loc.programs}
    out_programs = []; blocks = []
    for index in sorted(programs):
        try: calls = run_program(programs[index])
        except (KeyError, IndexError): continue
        if not any(c['builtin'] in (16, 25, 26, 69) for c in calls): continue
        prog_owners = owners.get(index, [])
        records = []; current = None
        for c in calls:
            bi = c['builtin']
            if bi == 44: current = c['args'][0][2] if c['args'] else None
            rec = dict(builtin=bi, gates=c['gates'], guard=c['guard'])
            if bi in (16, 25, 26):
                words = defaults(bi)
                for key, typ, raw in c['args']: apply(bi, words, key, typ, raw)
                if bi in (25, 26) and f32(words[5]) < f32(words[7]): words[7] = words[5]
                target = words[54] if words[54] != 0xFFFFFFFF else current
                values = {FIELDS[k]: (struct.unpack('<i', struct.pack('<I', words[k]))[0] if FIELDS[k] in INT_FIELDS else f32(words[k]))
                          for k in range(59 if bi == 26 else 55)}
                if bi == 25: rec['condition'] = 'race mode ((gp-0x84C)+0x550 == 2): created only if a player rider is within 30000 cm'
                rec.update(target=target, target_name=names.get(target, 'self' if target is None else hex(target)),
                           block_index=len(blocks), texture=texture_name(words[49]), params=values)
                blocks.append(dict(program=index, builtin=bi, target=target, words=words[:0xF0 // 4 if bi == 26 else 0xE0 // 4]))
            else:
                rec['args'] = [[k, t, r] for k, t, r in c['args']]
                if bi == 69:
                    keyed = {k: r for k, t, r in c['args']}; tgt = keyed.get(0, 0xFFFFFFFF)
                    tgt = current if tgt == 0xFFFFFFFF else tgt
                    rec.update(target=tgt, target_name=names.get(tgt, 'self' if tgt is None else hex(tgt)), mode=keyed.get(1, 0),
                               effect='mode 0: every effect of the entity gets vt+0x1C (stop); mode 1: effects whose vt+0x14 == 3 are deleted')
            records.append(rec)
        out_programs.append(dict(program=index, owners=prog_owners, calls=records))
    # Initial state (race tick 0 savestate).
    ready = location_state(a.location, 'ready')
    with zipfile.ZipFile(ready) as z: memory = z.read('eeMemory.bin')
    rng = list(struct.unpack_from('<6I', memory, 0x4FF018))
    initial = particle_objects(memory, track)
    for r in initial: r['name'] = names.get(r['resource'], hex(r['resource']))
    blend = [u32(0x44B420 + 4 * k) for k in range(8)]
    # Instance matrices (+0x10) and scales (+0x84) as raw bits, and the static node chains 0x34FED8 walks for
    # every DynamicParticle carrier node (model node table *(instance+0x80)+8, 16-byte records {parent, ?, ?,
    # local matrix*}); web/stage_world.inc composes them onto the carrier's entity matrix.
    mu = lambda a: struct.unpack_from('<I', memory, a & 0x1FFFFFF)[0]
    def inst_addr(resource):
        t = mu(mu(mu(mu(GP + 0x16C8)) + 8) + (resource & 0xFF) * 4)
        if not t: return None
        e = mu(mu(t + 0x1C) + (resource >> 8) * 4); a = (e >> 8) << 2
        return a if a and mu(a + 0x78) == resource else None
    wanted = sorted({b['target'] for b in blocks if b['target'] is not None} | {o['resource'] for po in out_programs for o in po['owners']} |
                    {e['resource'] for e in initial})
    instance_bits = {}
    for r in wanted:
        addr = inst_addr(r)
        if addr: instance_bits[r] = dict(matrix_bits=[mu(addr + 0x10 + 4 * k) for k in range(16)], scale_bits=mu(addr + 0x84))
    carriers = {}
    for b in blocks:
        if b['builtin'] != 26 or b['target'] is None: continue
        addr = inst_addr(b['target']); node = struct.unpack('<i', struct.pack('<I', b['words'][55]))[0]
        if not addr: raise SystemExit(f'carrier {b["target"]:#x} not resident in the ready savestate')
        table = mu(mu(addr + 0x80) + 8); chain = []; n = node
        while n != -1:
            chain.append([mu(mu(table + 16 * n + 0xC) + 4 * k) for k in range(16)])
            n = struct.unpack('<i', struct.pack('<I', mu(table + 16 * n)))[0]
            if len(chain) > 64: raise SystemExit('node chain loop')
        c = carriers.setdefault(str(b['target']), dict(name=names.get(b['target']), scale_bits=mu(addr + 0x84), chains={}))
        c['chains'][str(node)] = chain
    # MultiParticle groups (builtin105 0x306A90 -> MultiParticleMan *(gp+0xF38), 4 slots; 0x357DD8 layout: +0 capacity,
    # +4 count, +8 parameter block (0xD8), +0xC member resources, +0x10 static emitter (0x190), +0x20/+0x30 emitter
    # bounds, +0x40 kernel position base (w 0)) as built during the load (global programs), for the roadflare flames.
    multi = []; man = mu(GP + 0xF38)
    for g in range(4):
        obj = mu(man + 4 * g) if man else 0
        if not obj: continue
        cap, count, block, members, em = mu(obj), mu(obj + 4), mu(obj + 8), mu(obj + 0xC), mu(obj + 0x10)
        multi.append(dict(group=g, capacity=cap, members=[mu(members + 4 * k) for k in range(count)], block=[mu(block + 4 * k) for k in range(0xD8 // 4)],
                          emitter=[mu(em + 4 * k) for k in range(0x190 // 4)], bounds=[mu(obj + 0x20 + 4 * k) for k in range(8)],
                          position_base=[mu(obj + 0x40 + 4 * k) for k in range(4)], texture=texture_name(mu(em + 4))))
    # MagnetModifiers (vtable 0x48F420, 0xB0 bytes, engine/magnet_modifier.hpp) built during the load (BHP1 pointa/pointc
    # slot-1 programs run before the ready savestate): the whole object, by instance (+0xA0 -> +0x78).
    import array as _array
    magnets = []; ww = _array.array('I', memory)
    for idx in range(0x100000 // 4, len(ww) - 44):
        if ww[idx] != 0x48F420: continue
        obj = idx * 4; inst = mu(obj + 0xA0)
        if not 0x100000 < inst < 0x2000000: continue
        resource = mu(inst + 0x78); ent = mu(inst + 0xC)
        if not 0x100000 < ent < 0x2000000: continue
        magnets.append(dict(resource=resource, name=names.get(resource, hex(resource)), bytes=[mu(obj + 4 * k) for k in range(0xB0 // 4)], instance_flags=mu(inst + 8)))
    magnets.sort(key=lambda m: m['resource'])
    # HaloModifiers (vtable 0x491220 at +8, 0x40 bytes; builtin97) alive at race tick 0, in each entity's list order.
    fb = lambda x: struct.unpack('<f', struct.pack('<I', x))[0]
    halos = []
    for idx in range(0x100000 // 4, len(ww) - 16):
        if ww[idx] != 0x491220: continue
        obj = idx * 4 - 8; inst = mu(obj + 0xC)
        if not 0x100000 < inst < 0x2000000: continue
        resource = mu(inst + 0x78); ent = mu(inst + 0xC)
        if not 0x100000 < ent < 0x2000000: continue
        container = mu(ent + 0x1C); e = mu(container + 0x10) if container else 0; order = 0; found = False
        while 0x100000 < e < 0x2000000 and order < 64:
            if e == obj: found = True; break
            e = mu(e); order += 1
        if not found: continue
        i32 = lambda x: struct.unpack('<i', struct.pack('<I', x))[0]
        halos.append(dict(resource=resource, name=names.get(resource, hex(resource)), order=order, r=fb(mu(obj + 0x10)), g=fb(mu(obj + 0x14)), b=fb(mu(obj + 0x18)),
                          a=fb(mu(obj + 0x1C)), size=fb(mu(obj + 0x20)), texture=i32(mu(obj + 0x24)), node=i32(mu(obj + 0x28)), spin=i32(mu(obj + 0x2C)),
                          angle_bits=mu(obj + 0x30)))
    halos.sort(key=lambda h: (h['resource'], h['order']))
    # CrowdMan2d *(gp-0x6F0) (0x229180): +0x8 u32 rid[128] (-1 free), +0x210 + 0x40*i: centre/axis1/axis2, +0x30 s32
    # countdown; +0x2210 cached |cheer|, +0x2214 ring cursor, +0x2220 40 x {pos vec4, s32 life}. The race's first
    # 0x229530 continues these countdowns (the 128 load-time arms 0x2292E0 are before the savestate).
    cm = mu(GP - 0x6F0); crowd = None
    if 0x100000 < cm < 0x2000000:
        i32 = lambda x: struct.unpack('<i', struct.pack('<I', x))[0]
        crowd = dict(object=cm, slots=[mu(cm + 8 + 4 * k) for k in range(128)], countdowns=[i32(mu(cm + 0x240 + 0x40 * k)) for k in range(128)],
                     cheer=mu(cm + 0x2210), cursor=mu(cm + 0x2214), ring_life=[i32(mu(cm + 0x2230 + 0x20 * k)) for k in range(40)])
    # One-way volumes (builtin7 Boost, vtable 0x4914E0 at +0xC) built during the load (R&B A_ASS1 / Crow's Nest onewayvolume
    # slot 1 before the ready savestate): +0x20 direction quad, speed, gain, duration, countdown, mode (raw words).
    boosts = []
    for idx in range(0x100000 // 4, len(ww)):
        if ww[idx] != 0x4914E0: continue
        obj = idx * 4 - 0xC; inst = mu(obj + 0x18)
        if not 0x100000 < inst < 0x2000000 or mu(inst + 0xC) != obj: continue
        boosts.append(dict(resource=mu(inst + 0x78), name=names.get(mu(inst + 0x78), hex(mu(inst + 0x78))), words=[mu(obj + 0x20 + 4 * k) for k in range(9)], instance_flags=mu(inst + 8)))
    boosts.sort(key=lambda b: b['resource'])
    out = dict(version=1, location=a.location, coordinate_system='Original source centimeters, Z-up, row vectors p * M (native = (x, z, -y)/100)',
               engine='engine/set_piece_particles.hpp', fields=list(FIELDS), blend_table_0x44B420=blend,
               programs=out_programs, blocks=blocks,
               initial=dict(savestate=str(ready.relative_to(ROOT)), visual_random_0x4FF018=rng, effects=initial),
               instances={str(r): dict(name=names[r], matrix=by_name[names[r]]['matrix'], **instance_bits.get(r, {})) for r in wanted if r in names},
               carriers=carriers, multi=multi, magnets=magnets, halos=halos, crowd=crowd, **(dict(boosts=boosts) if boosts else {}))
    target = ROOT / 'web/public/assets' / a.location / 'PARTICLES'; target.mkdir(parents=True, exist_ok=True)
    (target / 'particles.json').write_text(json.dumps(out, indent=1) + '\n')
    summary = dict(location=a.location, programs=len(out_programs), blocks=len(blocks), initial_effects=len(initial), multi_groups=[(m['group'], len(m['members']), m['texture']) for m in multi], magnets=len(magnets), halos=len(halos), crowd_slots=sum(1 for r in (crowd or {}).get('slots', []) if r != 0xFFFFFFFF), output=str(target / 'particles.json'))
    if a.header: summary['header'] = str(write_header(a.location, out))
    print(json.dumps(summary))


def write_header(code, data):
    ns = f'browser_set_piece_particles_{code.lower()}'
    lines = ['#pragma once', f'// Generated by tools/export_set_piece_particles.py --location {code} (see engine/set_piece_particles.hpp).',
             '#include <array>', '#include <cstdint>', f'namespace {ns} {{',
             'struct Block {uint16_t program;uint8_t builtin;uint32_t target;std::array<uint32_t,60> words;};']
    bl = data['blocks']
    lines.append(f'inline constexpr std::array<Block,{len(bl)}> blocks={{{{')
    for b in bl:
        w = b['words'] + [0] * (60 - len(b['words']))
        lines.append(f' {{{b["program"]}u,{b["builtin"]}u,{b["target"] if b["target"] is not None else 0xFFFFFFFF}u,{{' + ','.join(f'0x{x:08x}u' for x in w) + '}},')
    lines.append('}};')
    lines.append('inline constexpr std::array<uint32_t,6> visualRandomTick0={' + ','.join(f'0x{x:08x}u' for x in data['initial']['visual_random_0x4FF018']) + '};')
    lines.append('struct InitialParticle {uint32_t resource;uint32_t dead;std::array<uint32_t,16> matrix;std::array<uint32_t,100> emitter;};')
    ps = [e for e in data['initial']['effects'] if e['kind'] == 'Particle']
    lines.append(f'inline constexpr std::array<InitialParticle,{len(ps)}> initialParticles={{{{')
    for e in ps:
        lines.append(f' {{{e["resource"]}u,{e["dead"]}u,{{' + ','.join(f'0x{x:08x}u' for x in e['matrix']) + '},{' + ','.join(f'0x{x:08x}u' for x in e['emitter']) + '}},')
    lines.append('}};')
    lines.append(f'}} // namespace {ns}')
    path = ROOT / f'web/generated/set_piece_particles_{code}.hpp'; path.write_text('\n'.join(lines) + '\n')
    return path


if __name__ == '__main__':
    main()
