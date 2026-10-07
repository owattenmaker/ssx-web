"""Exact-mode copies of the reference savestates, re-derived from their frontend roots (docs/ps2-float.md "Exact baselines").

usage: derive_exact_baselines.py [--map] [--only a,b] [--slots N] [--attempts K] [--width W] [--list]

A reference state (local/reference/pcsx2/<name>.p2s) that tools/ps2_navigate.py made is found by its EE memory hash among
the navigation outputs (local/ps2-capture/**/out-*/). Its run's navigate.json gives the run's baseline, pad segments and
save samples. The chain is followed back to the first baseline with no race loaded (a menu: game tick None).

From there every run is repeated with PS2_CAPTURE_FPU=exact:
- the first run starts from the menu state itself (it carries no float arithmetic history into a race: the oracle's
  --stale-ref check);
- later runs start from the exact copy of their baseline.

A state saved at a game tick (countdown anchors at 18, glides) is accepted only at the reference state's own tick. The
save is requested at the original sample and --width samples on each side (default 1), and the run is retried up to --attempts times.

Output: local/reference-exact/<name>.p2s plus <name>.provenance.json (chain, ticks, fpu_mode). The originals are only read.
"""
import argparse
import glob
import hashlib
import json
import os
import shutil
import struct
import subprocess
import sys
import time
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
REFERENCE = ROOT / 'local/reference/pcsx2'
OUT = ROOT / 'local/reference-exact'
WORK = OUT / 'work'
MAP = OUT / 'provenance-map.json'
STEPS = WORK / 'steps.json'
# Samples on each side of a tick-bound save (--width): PINE saves land a tick or two from the request.
WIDTH = 1
GP = 0x4A30F0


def ee_hash(path):
    with zipfile.ZipFile(path) as archive:
        return hashlib.sha256(archive.read('eeMemory.bin')).hexdigest()


def game_tick(path):
    """The rider manager's +8, or None when no race is loaded (the chain does not resolve)."""
    with zipfile.ZipFile(path) as archive:
        memory = archive.read('eeMemory.bin')

    def word(address):
        return struct.unpack_from('<I', memory, address & 0x1FFFFFF)[0]

    # tools/ps2_navigate.py game_tick: a zero link anywhere in the chain is a menu.
    try:
        pointer = word(GP - 0x848)
        for offset in (0x84, 0x0C):
            pointer = word(pointer + offset)
            if pointer == 0:
                return None
        return word(pointer + 8)
    except struct.error:
        return None


def race_phase(path):
    """The race clock's phase, or None with no race loaded.

    The clock is the object whose +8 is the game tick (game_tick's chain). 0x113B10(C, phase) keeps the old phase at C+4 and
    0x113B48 enters the new one: C+0 = phase (1..7), C+0x98 = C+0xAC + 4 * (phase - 1), C+0xA0 = its handler. Phases 3 PreRace
    (course flythrough and the event brief), 4 Countdown, 5 Race, 6 EndRace (docs/ctm-events-in-world.md). The game tick also counts
    during PreRace, so a tick alone does not tell the flythrough's tick 18 from the countdown's.
    """
    with zipfile.ZipFile(path) as archive:
        memory = archive.read('eeMemory.bin')

    def word(address):
        return struct.unpack_from('<I', memory, address & 0x1FFFFFF)[0]

    try:
        pointer = word(GP - 0x848)
        for offset in (0x84, 0x0C):
            pointer = word(pointer + offset)
            if pointer == 0:
                return None
        return word(pointer)
    except struct.error:
        return None


def same_phase(state, reference):
    """False when a tick-bound state is in another race phase than its reference (no reference: True)."""
    return not Path(reference).exists() or race_phase(state) == race_phase(reference)


def resident_locations(path):
    sys.path.insert(0, str(ROOT / 'tools'))
    import ps2_navigate
    with zipfile.ZipFile(path) as archive:
        return ps2_navigate.describe(archive.read('eeMemory.bin')).get('resident_locations', [])


def build_map():
    """Reference state name -> the navigation output with the same EE memory."""
    references = {}
    for path in sorted(REFERENCE.glob('*.p2s')):
        try:
            references.setdefault(ee_hash(path), path.name)
        except (zipfile.BadZipFile, KeyError):
            continue
    found = {}
    for path in sorted(glob.glob(str(ROOT / 'local/ps2-capture/**/out-*/*.p2s'), recursive=True)):
        try:
            digest = ee_hash(path)
        except (zipfile.BadZipFile, KeyError):
            continue
        name = references.get(digest)
        if name and name not in found:
            found[name] = os.path.relpath(path, ROOT)
    MAP.parent.mkdir(parents=True, exist_ok=True)
    MAP.write_text(json.dumps(found, indent=1) + '\n')
    return found


def run_record(output_state):
    """The navigate.json of the run that wrote output_state, and the save result for it."""
    record = json.loads((Path(output_state).parent / 'navigate.json').read_text())
    for result in record['results']:
        if result.get('state') and Path(result['state']).name == Path(output_state).name:
            return record, result
    raise ValueError(f'{output_state}: not in its navigate.json')


def resolve(path):
    path = Path(path)
    return path if path.is_absolute() else ROOT / path


_file_hashes = {}


def baseline_of(record):
    """The run's baseline; a deleted one is found among the references by the manifest's file hash."""
    baseline = resolve(record['manifest']['baseline'])
    if baseline.exists():
        return baseline
    wanted = record['manifest'].get('baseline_sha256')
    for path in sorted(REFERENCE.glob('*.p2s')):
        if path not in _file_hashes:
            _file_hashes[path] = hashlib.sha256(path.read_bytes()).hexdigest()
        if _file_hashes[path] == wanted:
            return path
    raise ValueError(f'{baseline}: missing, and no reference has its sha256')


def chain_of(name, mapping):
    """[(output_state, record, result)] from the first run off a menu state to the run that wrote `name`."""
    chain = []
    current = mapping.get(name)
    if current is None:
        raise ValueError(f'{name}: no navigation output has its EE memory')
    while True:
        record, result = run_record(resolve(current))
        chain.append((current, record, result))
        baseline = baseline_of(record)
        if game_tick(baseline) is None:
            # A root must be a menu: no rider manager and no course resident (ps2_navigate describe()).
            if resident_locations(baseline):
                raise ValueError(f'{baseline}: no game tick but locations resident (a load in progress), not a menu root')
            break
        # The baseline is a race state: a reference (resolved through the map) or another run's output.
        if baseline.parent == REFERENCE:
            current = mapping.get(baseline.name)
            if current is None:
                raise ValueError(f'{baseline.name}: race-state baseline without navigation provenance')
        else:
            current = os.path.relpath(baseline, ROOT)
    chain.reverse()
    return chain


def emulators():
    result = subprocess.run(['pgrep', '-x', 'ARMSX2'], capture_output=True, text=True)
    return len(result.stdout.split())


def wait_for_slot(limit):
    while emulators() >= limit:
        time.sleep(10)


def exact_run(baseline, record, saves, workdir, slots):
    """Build a navigation state from `baseline` with the run's segments and run it in exact mode."""
    workdir.mkdir(parents=True, exist_ok=True)
    script = workdir / 'script.json'
    script.write_text(json.dumps({'segments': record['manifest']['segments']}) + '\n')
    nav_state = workdir / 'nav.p2s'
    subprocess.run([sys.executable, str(ROOT / 'tools/ps2_navigate.py'), 'build', str(baseline), str(script), str(nav_state)],
                   cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
    arguments = [sys.executable, str(ROOT / 'tools/ps2_navigate.py'), 'run', str(nav_state), str(workdir / 'out'),
                 '--frames', str(record['frames'] + 2), '--speed', 'turbo', '--timeout', '3600']
    for save_name, sample in saves:
        arguments += ['--save', f'{save_name}@{sample}']
    wait_for_slot(slots)
    environment = dict(os.environ, PS2_CAPTURE_FPU='exact')
    with open(workdir / 'run.log', 'w') as log:
        code = subprocess.run(['nice', '-n', '5'] + arguments, cwd=ROOT, env=environment, stdout=log, stderr=subprocess.STDOUT).returncode
    return code, workdir / 'out'


def derive(name, mapping, slots, attempts, log):
    target = OUT / name
    if target.exists():
        log(f'{name} done')
        return True
    chain = chain_of(name, mapping)
    steps = []
    baseline = baseline_of(chain[0][1])
    for index, (output, record, result) in enumerate(chain):
        wanted_tick = result.get('tick')
        sample = result['requested'] if result.get('requested') is not None else result['sample']
        step_name = Path(output).stem
        # A tick-bound state: ask for the sample and its neighbours (WIDTH each side), keep the one at the reference tick.
        offsets = range(-WIDTH, WIDTH + 1)
        candidates = [(f'{step_name}-s{sample + d}', sample + d) for d in offsets if sample + d > 0] if wanted_tick else [(step_name, sample)]
        # A step another target already derived (e.g. the ready state behind an anchor and a glide) is reused.
        cache = json.loads(STEPS.read_text()) if STEPS.exists() else {}
        chosen = Path(cache[output]) if output in cache and Path(cache[output]).exists() else None
        if chosen:
            log(f'{name} step {index} {step_name}: reused {os.path.relpath(chosen, ROOT)}')
        for attempt in range(1, attempts + 1):
            if chosen:
                break
            workdir = WORK / name / f'step{index}-{step_name}-a{attempt}'
            if workdir.exists():
                shutil.rmtree(workdir)
            code, out = exact_run(baseline, record, candidates, workdir, slots)
            for candidate, _ in candidates:
                state = out / f'{candidate}.p2s'
                if not state.exists():
                    continue
                if wanted_tick is None or (game_tick(state) == wanted_tick and same_phase(state, ROOT / output)):
                    chosen = state
                    break
            log(f'{name} step {index} {step_name} attempt {attempt}: exit {code}, {"kept " + chosen.name if chosen else "no state at tick " + str(wanted_tick)}')
            if chosen:
                cache = json.loads(STEPS.read_text()) if STEPS.exists() else {}
                cache[output] = str(chosen)
                STEPS.write_text(json.dumps(cache, indent=1) + '\n')
                break
        if not chosen:
            return False
        steps.append(dict(reference_output=output, baseline=os.path.relpath(baseline, ROOT), segments_from=record['state'],
                          frames=record['frames'], sample=sample, tick=wanted_tick, state=os.path.relpath(chosen, ROOT)))
        baseline = chosen
    shutil.copyfile(baseline, target)
    provenance = dict(reference=os.path.relpath(REFERENCE / name, ROOT), fpu_mode='exact', root=steps[0]['baseline'], steps=steps,
                      tick=game_tick(target), ee_sha256=ee_hash(target))
    target.with_suffix('.provenance.json').write_text(json.dumps(provenance, indent=1) + '\n')
    log(f'{name} written (tick {provenance["tick"]})')
    return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--map', action='store_true', help='rebuild the reference -> navigation output map')
    parser.add_argument('--only', default='')
    parser.add_argument('--slots', type=int, default=4, help='machine-wide ARMSX2 limit (pgrep -x ARMSX2)')
    parser.add_argument('--attempts', type=int, default=4)
    parser.add_argument('--list', action='store_true')
    parser.add_argument('--width', type=int, default=1)
    args = parser.parse_args()
    global WIDTH
    WIDTH = args.width
    mapping = build_map() if args.map or not MAP.exists() else json.loads(MAP.read_text())
    # Menu states (no race loaded) are roots, not targets.
    names = sorted(name for name in mapping if game_tick(REFERENCE / name) is not None)
    if args.only:
        names = [name for name in names if name in args.only.split(',')]
    if args.list:
        for name in names:
            chain = chain_of(name, mapping)
            print(name, '<-', ' <- '.join(Path(step[0]).parent.name for step in reversed(chain)), '<-', os.path.relpath(baseline_of(chain[0][1]), ROOT))
        return
    ok = True
    for name in names:
        try:
            ok = derive(name, mapping, args.slots, args.attempts, lambda text: print(text, flush=True)) and ok
        except Exception as error:
            print(f'{name} ERROR {error}', flush=True)
            ok = False
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
