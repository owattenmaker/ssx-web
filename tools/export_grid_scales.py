#!/usr/bin/env python3
"""Grid spots of every body scale in every countdown slot, from the PS2 (docs/multiplayer.md "Grid spot").

A countdown slot's spot depends on the slot and the rider's body scale (lineups.json `grid[slot][scale]`,
tools/export_lineups.py). The computer riders only ever have scales 0.7..1.0 (base riders and the seven Tricky cheat
skins), so the recorded lineups do not hold the spots of the cheat skins outside that range -- Stretch 1.2, Bunny San /
Churchill 1.3, North West Legend 1.5, Canhuck 0.7 (a different float from Griff's), Far East Myth 2.0 -- which an
online racer in slot k can be.

`states`: derived countdown states where every computer slot holds one such skin. From characters/zoe/select.p2s the
race-copy routine 0x23A668 gets one instruction changed in the state's RAM: `lw v1,0(a2)` at 0x23A700 (the roster
value of the slot) becomes `addiu v1,zero,CHEAT`, so all six slot values are that cheat id; the human's slot keeps
its own identity (0x23A71C..0x23A728 rewrite it), and the five computer riders race as the skin on the human's base
(Zoe), as a cheat computer rider always does. The rest of the original menu path and event load run unmodified in
ARMSX2 (tools/ps2_navigate.py, the same script as characters/scripts/make_lineup_states.py). One control state uses
Hiro (0.75, a recorded scale): its spots must equal lineups.json's.

`export`: the grid parts (the paths lineups.json `paths.grid` names) of every computer rider in those states ->
web/public/assets/<course>/grid-scales.json {grid: {slot: {scale: part}}}; every part of a recorded scale must equal
lineups.json's exactly.

usage: export_grid_scales.py states --course ARA1|BRA2 [--parallel 3]
       export_grid_scales.py export --course ARA1|BRA2
"""
import argparse, json, shutil, struct, subprocess, sys, zipfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from export_lineups import participants_from_memory, leaves, f32key, NAME_OF, ROOT  # noqa: E402
from export_npc_riders import extract_document  # noqa: E402
from reference_replay import patch_state  # noqa: E402

CHARS = ROOT / 'local/reference/pcsx2/characters'
PATCH_AT, PATCH_OLD = 0x23A700, 0x8CC30000         # lw v1, 0(a2): the slot's roster value in 0x23A668
CHEATS = [20, 22, 26, 28, 29]                     # stretch, bunnysan (= churchill's scale), nwlegend, canhuck, fareastmyth
CONTROL = 17                                      # hiro: 0.75, recorded in lineups.json


def folder(course): return CHARS / ('grid-scales' if course == 'ARA1' else f'grid-scales-{course}')


def script(course):
    P = lambda b, f=10: {'frames': f, 'buttons': [b]}
    seg = []
    for n, wait in enumerate((120, 150, 120, 120, 100)):
        if n == 4 and course == 'BRA2': seg += [P('DPadDown', 6), {'frames': 40}]   # Select Event: Metro-City
        seg += [P('Cross'), {'frames': wait}]
    seg += [P('Cross'), {'frames': 2600, 'save': 'ready'}, P('Cross', 15), {'frames': 5, 'save': 'countdown'}]
    return {'segments': seg}


def one(cheat, work, course):
    name = f'cheat{cheat}-{NAME_OF[(True, cheat)]}'
    out = folder(course) / name; out.mkdir(parents=True, exist_ok=True)
    if (out / 'countdown.p2s').exists(): return name, 'exists'
    select = CHARS / 'zoe' / 'select.p2s'
    patches = [dict(address=hex(PATCH_AT), expected=struct.pack('<I', PATCH_OLD).hex(), replacement=struct.pack('<I', 0x24030000 | cheat).hex())]
    (out / 'poke.patches.json').write_text(json.dumps(dict(source=str(select.relative_to(ROOT)), cheat=cheat, skin=NAME_OF[(True, cheat)],
                                                            note='0x23A700 lw v1,0(a2) -> addiu v1,zero,cheat: every slot value of 0x23A668 is the cheat id', patches=patches), indent=1) + '\n')
    (out / 'script.json').write_text(json.dumps(script(course), indent=0))
    poked, nav = work / f'{course}-{name}.poked.p2s', work / f'{course}-{name}.p2s'
    patch_state(select, poked, patches)
    subprocess.run([sys.executable, str(ROOT / 'tools/ps2_navigate.py'), 'build', str(poked), str(out / 'script.json'), str(nav)], check=True, capture_output=True)
    log = subprocess.run([sys.executable, str(ROOT / 'tools/ps2_navigate.py'), 'run', str(nav), str(work / f'out-{course}-{name}'), '--frames', '3400'], capture_output=True, text=True)
    if log.returncode: return name, 'failed: ' + log.stderr[-400:]
    for f in ('countdown.p2s', 'countdown.png', 'ready.png', 'navigate.json'):
        if (work / f'out-{course}-{name}' / f).exists(): shutil.copy2(work / f'out-{course}-{name}' / f, out / f)
    return name, 'ok'


def export(course):
    lineups = json.loads((ROOT / f'web/public/assets/{course}/lineups.json').read_text())
    grid_paths = set(lineups['paths']['grid'])
    grid, checked, sources = {}, 0, {}
    for state in sorted(p for p in folder(course).iterdir() if (p / 'countdown.p2s').exists()):
        memory = zipfile.ZipFile(state / 'countdown.p2s').read('eeMemory.bin')
        participants = participants_from_memory(memory)
        cheat = json.loads((state / 'poke.patches.json').read_text())['cheat']
        if participants[0]['character'] != 'zoe': raise ValueError(f'{state.name}: the human is not Zoe')
        doc = extract_document(memory, state / 'countdown.p2s', participants, course, check_human=True)
        for r in doc['riders']:
            if r['character'] != NAME_OF[(True, cheat)] or r['gameplay_character_id'] != 4: raise ValueError(f'{state.name}: slot {r["slot"]} is {r["character"]}')
            slot, scale = str(r['slot']), f32key(r['ground']['profile']['body_scale'])
            part = {p: v for p, v in leaves(r).items() if p in grid_paths}
            if set(part) != grid_paths: raise ValueError(f'{state.name}: grid paths differ')
            recorded = lineups['grid'].get(slot, {}).get(scale)
            if recorded is not None:
                if recorded != part: raise ValueError(f'{state.name} slot {slot}: the spot of recorded scale {scale} differs from lineups.json')
                checked += 1
                continue
            if grid.setdefault(slot, {}).setdefault(scale, part) != part: raise ValueError(f'{state.name}: slot {slot} scale {scale} differs between states')
            sources.setdefault(scale, state.name)
    missing = sorted({(s, k) for k in set(lineups['human_scale'].values()) for s in '12345' if k not in lineups['grid'][s] and k not in grid.get(s, {})})
    if missing: raise ValueError(f'scales still without a recorded spot: {missing}')
    out = dict(version=1, course=course, grid=grid, provenance=dict(tool='tools/export_grid_scales.py', states=sources, control_checked=checked,
               technique='0x23A668 slot values replaced by one cheat id (0x23A700 addiu), zoe/select.p2s, original menu path and load'))
    (ROOT / f'web/public/assets/{course}/grid-scales.json').write_text(json.dumps(out, indent=1) + '\n')
    print(course, 'scales', {s: sorted(v) for s, v in grid.items()}, 'control parts equal to lineups.json:', checked)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('command', choices=['states', 'export']); p.add_argument('--course', default='ARA1', choices=['ARA1', 'BRA2'])
    p.add_argument('--parallel', type=int, default=3); p.add_argument('--work', default='/tmp/ssx3-grid-scale-work')
    a = p.parse_args()
    if a.command == 'export': export(a.course); return
    work = Path(a.work); work.mkdir(parents=True, exist_ok=True)
    with ThreadPoolExecutor(a.parallel) as pool:
        for name, status in pool.map(lambda c: one(c, work, a.course), CHEATS + [CONTROL]): print(name, status, flush=True)


if __name__ == '__main__':
    main()
