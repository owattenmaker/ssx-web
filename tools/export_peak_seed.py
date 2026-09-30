#!/usr/bin/env python3
"""Mid-run rider seed of the streamed Peak 1 world from a free-ride PS2 savestate (docs/peak-mountain.md "Captures").

The capture comparer (web/compare-ps2-capture.mjs) replays a mid-run capture from the browser's glide seed of the loaded
world: for a course that is its grounded riding checkpoint (tools/locations.py 'glide'); for the streamed Peak 1 world
(world location 'PEAK1') it is the baseline of the free-ride capture local/ps2-capture/runs/peak1-fr-aara1-glide (the
rider cruising on Green Base Station toward A_ARA1: control 0 / motion 0, a settled ride). This writes, like tools/prepare_location.py does for a course:
  local/assets/native/PEAK1/riding-start.json   tools/export_riding_start.py (the glide seed of web/generate-controllers.py)
  web/public/assets/PEAK1/initial.json          web/prepare-ui.py build_initial (animation, reset route, boost, snow, ...)
  web/public/assets/PEAK1/start.json            the seed's position and heading
The breath regions come from the painter of the location the rider is on (A). Pickups: none (free ride's
collectibles are stage programs). Nothing here writes a savestate.

    .venv/bin/python tools/export_peak_seed.py [--state local/ps2-capture/peak1/fr-aara1-glide.p2s] [--region A]
"""
import argparse, hashlib, json, struct, subprocess, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from locations import human_rider  # noqa: E402

NATIVE = ROOT / 'local/assets/native'
WEB = ROOT / 'web/public/assets/PEAK1'


def seed_state(state):
    """Stage / section state of the seed savestate (web/peak-capture.mjs -> peak_world_seed, set_world_visual_state):
    the node entities (6 DeadNode, 16 Hide, 19 RestoreNode) with their instance flags, the entity class of every octree
    instance with an entity, and the 0x101B60 list (tools/export_world_visual_state.py "sections")."""
    import export_sections as X
    m = X.Memory(state); nodes, entities = [], []
    for r in sorted(X.instance_rows(m).values(), key=lambda r: r['resource']):
        e = m.u(r['inst'] + 0xC)
        if not e: continue
        entities.append([r['resource'], r['entity']])
        typ = m.h(e + 0x10)
        if typ in (6, 16, 19): nodes.append([r['resource'], typ, r['flags']])
    act = m.activation()
    sections = dict(game_tick=m.tick(), last_scan=m.i(act + 0xD0), last_bits=[X.fbits(x) for x in m.v(act + 0x20)], parity=m.u(act + 0xD4),
                    listed=[dict(resource=m.u(i + 0x78), parity=int(bool(m.u(i + 8) & 0x200))) for i in m.active_list()])
    return dict(version=1, state=__import__('disc_paths').repo_relative(Path(state).absolute()), nodes=nodes, entities=entities, sections=sections)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--state', type=Path, default=ROOT / 'local/ps2-capture/peak1/fr-aara1-glide.p2s')
    p.add_argument('--region', default='A', help='location whose environment painter holds the rider (breath regions)')
    p.add_argument('--peak', type=int, default=1, choices=[1, 2, 3], help='PEAK<N> (docs/peak3.md: --peak 3 --state local/ps2-capture/peak3/... --region EBC3)')
    p.add_argument('--world', help='a whole-mountain seed name instead of PEAK<N>: MOUNTAIN (the All Peak runs, The Throne) or MOUNTAIN<x> '
                   '(another run of the same world, e.g. MOUNTAIN2 = the Peak 2 Race at Ruthless; docs/peak3.md section 6)')
    p.add_argument('--out', type=Path, help='write everything under OUT/<name>/ (web/, native/, validation/) instead of web/public/assets and '
                   'local/assets/native (a scratch export: the coordinator copies it in; web/generate-controllers.py compiles native seeds)')
    a = p.parse_args()
    global WEB
    name = a.world or f'PEAK{a.peak}'; WEB = (a.out / name / 'web') if a.out else ROOT / f'web/public/assets/{name}'
    riding = (a.out / name / 'native/riding-start.json') if a.out else NATIVE / f'{name}/riding-start.json'; riding.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(ROOT / 'tools/export_riding_start.py'), str(a.state), str(riding), '--location', name], cwd=ROOT, check=True)
    initial = json.loads(riding.read_text())['native']['initial']
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
    ph = struct.unpack_from('<I', elf, 28)[0]; stride, count = struct.unpack_from('<HH', elf, 42); table = None
    for i in range(count):
        typ, off, addr, _, size, _, _, _ = struct.unpack_from('<8I', elf, ph + i * stride)
        if typ == 1 and addr <= 0x459e20 and 0x459e40 <= addr + size:
            table = elf[off + 0x459e20 - addr:off + 0x459e40 - addr]
    thresholds = [dict(seconds=s, points=q) for s, q in struct.iter_unpack('<ff', table)]
    initial['original_grab_score'] = dict(profile=dict(normal=initial['original_grab_control']['profile']['grabs'], hold_thresholds=thresholds),
                                          provenance=dict(elf_sha256=hashlib.sha256(elf).hexdigest(), table='0x459e20'))
    from reference_trick_identity import extract_trick_identity
    from reference_boost import extract_boost
    from reference_reset import extract_reset
    from reference_snow_context import extract_memory as extract_snow
    from reference_rail_context import extract_memory as extract_rail_context
    from export_trick_names import extract as extract_trick_names
    from reference_secondary_motion import extract_secondary_motion
    memory = zipfile.ZipFile(a.state).read('eeMemory.bin'); rider = human_rider(memory)
    initial['original_trick_identity'] = extract_trick_identity(memory, rider)
    initial['original_boost']['award_context'] = extract_boost(memory, rider)['award_context']
    initial['original_reset'] = extract_reset(memory, rider)
    initial['original_reset']['event_route'] = initial['original_reset']['route']  # no grid start in free ride
    initial['original_board_trail']['profile'] = json.loads((NATIVE / 'BOARD_TRAIL/board_trail.json').read_text())['profile']
    initial['original_snow']['emitter_profiles'] = json.loads((NATIVE / 'SNOW_FX/snow-fx.json').read_text())['profiles']
    snow = extract_snow(memory, rider)
    initial['original_snow']['state']['kicker_buildup'] = snow['state']['kicker_buildup']
    initial['original_snow']['state']['emitter_flipbook_phases'] = snow['state']['emitter_flipbook_phases']
    initial['original_snow']['wake_noise_table'] = list(struct.unpack_from('<161f', memory, 0x445ab0))
    breath = (a.out / name / 'validation/breath-context.json') if a.out else ROOT / f'local/browser-validation/{name}/breath-context.json'; breath.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(ROOT / 'tools/export_breath_context.py'), '--output', str(breath), '--location', a.region, '--state', str(a.state), '--rider', hex(rider)], cwd=ROOT, check=True)
    initial['original_breath'] = json.loads(breath.read_text())
    base = json.loads((ROOT / 'web/public/assets/ANIMATIONS/initial.json').read_text())
    initial['original_pickups'] = dict(base['original_pickups'], items=[])
    initial['original_rail_context'] = extract_rail_context(memory, rider)
    initial['original_trick_names'] = extract_trick_names()
    initial['original_animation']['secondary_motion'] = extract_secondary_motion(memory, rider)
    initial['peak_seed'] = dict(state=__import__('disc_paths').repo_relative(a.state.absolute()), state_sha256=hashlib.sha256(a.state.read_bytes()).hexdigest(), rider=hex(rider), region=a.region)
    WEB.mkdir(parents=True, exist_ok=True)
    (WEB / 'initial.json').write_text(json.dumps(initial))
    (WEB / 'seed-state.json').write_text(json.dumps(seed_state(a.state)))
    (WEB / 'start.json').write_text(json.dumps(dict(position=initial['position'], heading=initial['heading'])))
    print(name, 'seed:', a.state.name, 'rider', hex(rider), 'position', initial['position'])


if __name__ == '__main__':
    main()
