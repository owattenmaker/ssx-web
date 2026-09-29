#!/usr/bin/env python3
"""PS2 ground truth for the browser's stage-world particle effects (web/stage_world.inc): every Particle /
DynamicParticle object alive in each kept savestate of a capture run (development export; git-ignored output).

For every savestate RUN.tickN.p2s the objects are found like tools/export_set_piece_particles.py (effect
vtables 0x4912B0 / 0x491268 linked in their entity's effect list) and written with their raw words:
  local/reference/set-piece-particles/<name>.snapshots.json
    {"run": ..., "snapshots": [{"tick": N, "state": path, "visual_random_0x4FF018": [6],
       "effects": [{resource, kind, dead, [node, stopped], matrix (16 words, Particle), emitter (words),
                    ring_position / ring_velocity (DynamicParticle)}]}]}
web/test-stage-world.mjs replays the same pad script with the six-rider world and compares the browser core's
effects (stage_world_effects) with these at the same ticks.

usage: export_particle_snapshots.py RUN_GLOB [RUN_GLOB...] --name NAME [--location ARA1]
  e.g. export_particle_snapshots.py 'local/ps2-capture/runs/setpieces-fx/vis.tick*.p2s' --name ara1-vis
"""
import argparse, glob, json, re, struct, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from export_set_piece_particles import particle_objects  # noqa: E402
from set_piece_location import Location  # noqa: E402


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('runs', nargs='+'); p.add_argument('--name', required=True); p.add_argument('--location', default='ARA1')
    a = p.parse_args()
    loc = Location(a.location)
    paths = sorted({s for pat in a.runs for s in glob.glob(str(ROOT / pat))}, key=lambda s: int(re.search(r'tick(\d+)', s).group(1)))
    if not paths: raise SystemExit('no savestates')
    out = []
    for path in paths:
        with zipfile.ZipFile(path) as z: memory = z.read('eeMemory.bin')
        effects = particle_objects(memory, loc.track)
        for e in effects: e.pop('block', None); e.pop('object', None); e.pop('entity', None)
        # MeshAnim break pieces (vtable 0x48F6D8, 0x351B40): life +0x1C, fade +0x24, spin max +0x20, end mode +0x6B0,
        # gravity +0x6B4, per node: quaternion +0x30, axis +0x1B0, spin +0x330, velocity (cm/tick) +0x530, position +0x3B0.
        import array
        u = lambda x: struct.unpack_from('<I', memory, x & 0x1FFFFFF)[0]; f = lambda x: struct.unpack_from('<f', memory, x & 0x1FFFFFF)[0]
        meshanims = []
        for i, v in enumerate(array.array('I', memory)):
            if v != 0x48F6D8: continue
            obj = i * 4 - 0xC; inst = u(obj + 0x18)
            if not 0x100000 < inst < 0x2000000 or u(inst + 0xC) != obj: continue
            n = u(u(inst + 0x80) + 4)
            meshanims.append(dict(resource=u(inst + 0x78), flags=u(inst + 8), life=f(obj + 0x1C), fade=f(obj + 0x24), spin_max=f(obj + 0x20),
                                  end_mode=u(obj + 0x6B0), gravity=f(obj + 0x6B4), nodes=n,
                                  q=[[f(obj + 0x30 + 16 * k + 4 * c) for c in range(4)] for k in range(n)],
                                  pos=[[f(obj + 0x3B0 + 16 * k + 4 * c) for c in range(4)] for k in range(n)],
                                  vel=[[f(obj + 0x530 + 16 * k + 4 * c) for c in range(4)] for k in range(n)],
                                  axis=[[f(obj + 0x1B0 + 16 * k + 4 * c) for c in range(4)] for k in range(n)],
                                  spin=[f(obj + 0x330 + 4 * k) for k in range(n)]))
        # MultiParticle groups (MultiParticleMan *(gp+0xF38): +0 capacity, +4 count, +0xC members, +0x10 emitter 0x190).
        multi = []; man = u(0x4A30F0 + 0xF38)
        for g in range(4):
            obj = u(man + 4 * g) if man else 0
            if not obj: continue
            em = u(obj + 0x10); count = u(obj + 4)
            multi.append(dict(group=g, capacity=u(obj), members=[u(u(obj + 0xC) + 4 * k) for k in range(count)], emitter=[u(em + 4 * k) for k in range(0x190 // 4)]))
        # One-way volumes (builtin7 Boost, vtable 0x4914E0 at +0xC, 0x50 bytes): +0x18 instance, +0x20 direction quad,
        # +0x30 speed, +0x34 gain, +0x38 duration, +0x3C countdown, +0x40 mode.
        boosts = []
        for i, v in enumerate(array.array('I', memory)):
            if v != 0x4914E0: continue
            obj = i * 4 - 0xC; inst = u(obj + 0x18)
            if not 0x100000 < inst < 0x2000000 or u(inst + 0xC) != obj: continue
            boosts.append(dict(resource=u(inst + 0x78), words=[u(obj + 0x20 + 4 * k) for k in range(9)], flags=u(inst + 8)))
        out.append(dict(tick=int(re.search(r'tick(\d+)', path).group(1)), state=str(Path(path).relative_to(ROOT)),
                        visual_random_0x4FF018=list(struct.unpack_from('<6I', memory, 0x4FF018)), effects=effects, meshanims=meshanims, multi=multi, boosts=boosts))
        print(f'{Path(path).name}: {len(effects)} effects, {len(meshanims)} MeshAnims', file=sys.stderr)
    target = ROOT / 'local/reference/set-piece-particles' / f'{a.name}.snapshots.json'
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(json.dumps(dict(run=a.runs, location=a.location, snapshots=out)) + '\n')
    print(json.dumps(dict(output=str(target), snapshots=len(out))))


if __name__ == '__main__':
    main()
