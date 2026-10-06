"""Exact-mode copies of derived savestates that do not start from a menu (docs/ps2-float.md "Exact baselines").

usage: derive_exact_chain.py STATE.p2s [STATE.p2s ...] [--list] [--attempts K] [--width W] [--slots N]

tools/ps2-float/derive_exact_baselines.py handles the reference states made by ps2_navigate from menus. The free-ride, CTM
and world states the gates start from come from longer chains, so this tool walks a state's own records back to a root:

- patch: a STATE.patches.json beside the state (tools/reference_replay.py patch_state, clean_capture_state.py,
  ps2_navigate's cleaning). Its `source` is resolved again. A source that is gone is found by its `ee_sha256` among the
  roots below.
- nav: a ps2_navigate output. The navigate.json beside it lists the state; the run's baseline is resolved again. A state
  copied out of a run (e.g. peak3/fr-ebc3-arrival.p2s) is found by EE hash among the area's out-*/ directories.
- root, either:
  - a CTM session state (local/ps2-capture/menus/ctm/state-*.p2s, from ctm.session.json off the title screen), in place
    of which the exact replay of the same session (local/reference-exact/ctm/session-exact.fNNNNN.p2s) is used; the menu
    hook it holds is cleaned the way the chain cleaned it;
  - a menu state (no rider manager, nothing resident), used as it is.

The chain is then replayed with PS2_CAPTURE_FPU=exact:
- a patch step applies the same patches to the exact source. Hook and arena patches are written as they are. Every other
  patch (lock words, profile bits) must find its expected bytes, or the step stops.
- a nav step rebuilds the navigation state from the exact baseline with the run's segments, runs it, and keeps the save
  nearest the original sample at the original game tick (requested at the sample and --width samples on each side, with
  --attempts runs). A state with no game tick (a menu or a load screen) is kept at the original sample.

Output: local/reference-exact/chains/<path of the state under local/>, with .provenance.json beside it.
Steps already derived are reused (local/reference-exact/chains/steps.json).
"""
import argparse
import glob
import hashlib
import json
import os
import shutil
import subprocess
import sys
import time
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools/ps2-float'))
from reference_replay import patch_state  # noqa: E402
import derive_exact_baselines as nav_tool  # noqa: E402

OUT = ROOT / 'local/reference-exact/chains'
STEPS = OUT / 'steps.json'
SESSION = ROOT / 'local/ps2-capture/menus/ctm.session.json'
SESSION_EXACT = ROOT / 'local/reference-exact/ctm/session-exact'
# Hook sites the capture and navigation tools patch (their expected / replacement bytes are instructions, not game data),
# and the arena they use.
HOOK_SITES = {0x321298, 0x327208, 0x128630, 0x105D98, 0x10A87C, 0x128AF0}
ARENA = (0x90000, 0x100000)


def ee_hash(path):
    with zipfile.ZipFile(path) as archive:
        return hashlib.sha256(archive.read('eeMemory.bin')).hexdigest()


def relative(path):
    return os.path.relpath(path, ROOT)


def session_roots():
    """EE hash -> (session frame, cleaning) for every saved state of the CTM session and its cleaned variants."""
    roots = {}
    session = json.loads(SESSION.read_text())
    for shot in session['shots']:
        if not shot.get('state'):
            continue
        path = ROOT / shot['state']
        with zipfile.ZipFile(path) as archive:
            memory = bytearray(archive.read('eeMemory.bin'))
        roots[hashlib.sha256(memory).hexdigest()] = (shot['frame'], None)
        # The session's menu hook (j 0x96000 at 0x321298) restored, and the arena zeroed from 0x90000 or 0x96000.
        memory[0x321298:0x3212A0] = bytes.fromhex('c0ffbd273000b07f')
        for low in (0x90000, 0x96000):
            cleaned = bytearray(memory)
            cleaned[low:0x100000] = bytes(0x100000 - low)
            roots[hashlib.sha256(cleaned).hexdigest()] = (shot['frame'], low)
    return roots


def nav_output_of(state):
    """(record, result) of the ps2_navigate run that wrote `state`: by name in its own folder, else by EE hash in the area."""
    state = Path(state)
    for record_path in [state.parent / 'navigate.json'] + sorted(state.parent.glob('*.navigate.json')):
        if not record_path.exists():
            continue
        record = json.loads(record_path.read_text())
        for result in record.get('results', []):
            if result.get('state') and Path(result['state']).name == state.name:
                return record, result, state
    # Every navigation output under local/: the zip's CRC of eeMemory.bin filters, the EE hash decides.
    crc = ee_crc(state)
    digest = None
    for record_path in navigation_records():
        record = json.loads(Path(record_path).read_text())
        for result in record['results']:
            candidate = result.get('state') and resolve_path(result['state'])
            if not candidate or candidate == state or not candidate.exists() or ee_crc(candidate) != crc:
                continue
            digest = digest or ee_hash(state)
            if ee_hash(candidate) == digest:
                return record, result, candidate
    return None


_records = []


def navigation_records():
    if not _records:
        # The capture trees only (local/vendor, the emulator data paths and the assets are large and hold no runs).
        for tree in ('ps2-capture', 'ctm-events', 'career-rival', 'ctm-parity', 'rider-parity'):
            _records.extend(sorted(glob.glob(str(ROOT / 'local' / tree / '**/navigate.json'), recursive=True)))
            _records.extend(sorted(glob.glob(str(ROOT / 'local' / tree / '**/*.navigate.json'), recursive=True)))
    return _records


_sources = {}


def source_hashes():
    """Every patches.json source path in the capture trees -> its recorded EE hash (to name deleted files)."""
    if not _sources:
        for tree in ('ps2-capture', 'ctm-events', 'career-rival', 'ctm-parity', 'rider-parity', 'reference'):
            for path in glob.glob(str(ROOT / 'local' / tree / '**/*.patches.json'), recursive=True):
                try:
                    spec = json.loads(Path(path).read_text())
                except (json.JSONDecodeError, OSError):
                    continue
                source = spec.get('source') if isinstance(spec, dict) else None
                if isinstance(source, dict) and source.get('ee_sha256'):
                    _sources[str(resolve_path(source['path']))] = source['ee_sha256']
    return _sources


def ee_crc(path):
    try:
        with zipfile.ZipFile(path) as archive:
            return archive.getinfo('eeMemory.bin').CRC
    except (zipfile.BadZipFile, KeyError, OSError):
        return None


def record_file(output):
    """The navigation record that lists `output`."""
    for record_path in [output.parent / 'navigate.json'] + sorted(output.parent.glob('*.navigate.json')):
        if record_path.exists() and any(r.get('state') and Path(r['state']).name == output.name
                                        for r in json.loads(record_path.read_text()).get('results', [])):
            return record_path
    return output.parent / 'navigate.json'


def capture_run_of(state):
    """(summary, snapshot) of the ps2_capture run that kept `state` as RUN.tickN.p2s."""
    name = state.name
    if '.tick' not in name:
        return None
    run_name = name.split('.tick')[0]
    summary_path = state.parent / f'{run_name}.json'
    if not summary_path.exists():
        return None
    summary = json.loads(summary_path.read_text())
    for snapshot in summary.get('snapshots', []):
        if snapshot.get('state') and Path(snapshot['state']).name == name:
            return summary_path, summary, snapshot
    return None


def resolve_path(path):
    path = Path(path)
    return path if path.is_absolute() else ROOT / path


def resolve(state, roots, depth=0):
    """The chain behind `state`, root first: a list of step dicts."""
    state = Path(state)
    if depth > 30:
        raise ValueError(f'{state}: chain too long')
    if state.exists():
        digest = ee_hash(state)
        if digest in roots:
            frame, cleaning = roots[digest]
            return [dict(kind='session', frame=frame, cleaning=cleaning, state=relative(state))]
    patches_path = state.with_suffix('.patches.json')
    found = nav_output_of(state) if state.exists() else None
    if found:
        record, result, output = found
        baseline = resolve_path(record['manifest']['baseline'])
        step = dict(kind='nav', state=relative(state), output=relative(output), record=relative(record_file(output)),
                    sample=result['requested'] if result.get('requested') is not None else result['sample'], tick=result.get('tick'))
        if not baseline.exists():
            # A deleted baseline: the navigation state's own patches file names it by EE hash (a session root), else a
            # reference with the manifest's file hash.
            nav_patches = resolve_path(record['state']).with_suffix('.patches.json')
            wanted = json.loads(nav_patches.read_text())['source'].get('ee_sha256') if nav_patches.exists() else None
            wanted = wanted or source_hashes().get(str(baseline))
            if wanted in roots:
                frame, cleaning = roots[wanted]
                return [dict(kind='session', frame=frame, cleaning=cleaning, state=record['manifest']['baseline'], recovered=True), step]
            baseline = nav_tool.baseline_of(record)
        return resolve(baseline, roots, depth + 1) + [step]
    kept = capture_run_of(state)
    if kept:
        summary_path, summary, snapshot = kept
        baseline = resolve_path(summary['manifest']['baseline'])
        step = dict(kind='capture', state=relative(state), summary=relative(summary_path), tick=snapshot['tick'],
                    first_tick=summary.get('first_tick'))
        return resolve(baseline, roots, depth + 1) + [step]
    if patches_path.exists():
        spec = json.loads(patches_path.read_text())
        source = resolve_path(spec['source']['path'])
        step = dict(kind='patch', state=relative(state), patches=relative(patches_path))
        if source.exists():
            return resolve(source, roots, depth + 1) + [step]
        wanted = spec['source'].get('ee_sha256')
        if wanted in roots:
            frame, cleaning = roots[wanted]
            return [dict(kind='session', frame=frame, cleaning=cleaning, state=spec['source']['path'], recovered=True), step]
        raise ValueError(f'{state}: patch source {source} is gone and its EE hash is no known root')
    if state.exists() and nav_tool.game_tick(state) is None and not nav_tool.resident_locations(state):
        return [dict(kind='menu', state=relative(state))]
    # A copy with no records of its own: another file with the same EE memory that has them.
    if state.exists():
        digest = ee_hash(state)
        for alias in crc_index().get(ee_crc(state), []):
            if alias == state or ee_hash(alias) != digest:
                continue
            if (alias.with_suffix('.patches.json').exists() or capture_run_of(alias)
                    or (alias.parent / 'navigate.json').exists() or list(alias.parent.glob('*.navigate.json'))):
                return resolve(alias, roots, depth + 1)
    raise ValueError(f'{state}: no patches, navigation or root record')


_crcs = {}


def crc_index():
    """EE CRC -> savestates in the capture trees (zip central directories only)."""
    if not _crcs:
        for tree in ('ps2-capture', 'ctm-events', 'career-rival'):
            for path in glob.glob(str(ROOT / 'local' / tree / '**/*.p2s'), recursive=True):
                _crcs.setdefault(ee_crc(path), []).append(Path(path))
    return _crcs


def apply_patches(source, patches_path, output):
    """The chain's patches onto the exact source: hook / arena patches as written, game-data patches checked first."""
    spec = json.loads(Path(patches_path).read_text())
    with zipfile.ZipFile(source) as archive:
        memory = archive.read('eeMemory.bin')
    forced = []
    for patch in spec['patches']:
        address = int(patch['address'], 0)
        expected = bytes.fromhex(patch['expected'])
        current = memory[address:address + len(expected)]
        hook = address in HOOK_SITES or ARENA[0] <= address < ARENA[1]
        if current != expected and not hook:
            raise ValueError(f'{patches_path}: game data at {address:#x} differs in the exact source')
        forced.append(dict(address=patch['address'], expected=current.hex(), replacement=patch['replacement']))
    patch_state(Path(source), Path(output), forced)


def session_state(step):
    path = Path(f'{SESSION_EXACT}.f{step["frame"]:05d}.p2s')
    if not path.exists():
        raise ValueError(f'{path}: the exact CTM session replay has no state at frame {step["frame"]}')
    if step['cleaning'] is None:
        return path
    cleaned = OUT / 'session' / f'session-exact.f{step["frame"]:05d}.clean{step["cleaning"]:x}.p2s'
    if not cleaned.exists():
        with zipfile.ZipFile(path) as archive:
            memory = archive.read('eeMemory.bin')
        low = step['cleaning']
        patches = [dict(address='0x321298', expected=memory[0x321298:0x3212A0].hex(), replacement='c0ffbd273000b07f'),
                   dict(address=hex(low), expected=memory[low:0x100000].hex(), replacement='00' * (0x100000 - low))]
        patch_state(path, cleaned, patches)
    return cleaned


def nav_step(baseline, step, attempts, width, slots, log):
    record = json.loads((ROOT / step['record']).read_text())
    sample = step['sample']
    wanted = step['tick']
    name = Path(step['output']).stem
    offsets = range(-width, width + 1) if wanted else range(0, 1)
    candidates = [(f'{name}-s{sample + d}', sample + d) for d in offsets if sample + d > 0]
    for attempt in range(1, attempts + 1):
        workdir = OUT / 'work' / Path(step['state']).with_suffix('').name / f'{name}-a{attempt}'
        if workdir.exists():
            shutil.rmtree(workdir)
        code, out = nav_tool.exact_run(baseline, record, candidates, workdir, slots)
        ticks = {}
        for candidate, _ in candidates:
            state = out / f'{candidate}.p2s'
            if state.exists():
                ticks[candidate] = nav_tool.game_tick(state)
        exact = [c for c, t in ticks.items() if wanted is None or t == wanted]
        log(f'  nav {name} attempt {attempt}: exit {code}, ticks {sorted(set(ticks.values()), key=str)} (want {wanted})')
        if exact:
            return out / f'{exact[0]}.p2s'
    return None


def capture_step(baseline, step, attempts, width, slots, log):
    """ps2_capture build + run from the exact baseline with the run's script and options, keeping the state at the tick."""
    import recapture_exact
    summary = json.loads((ROOT / step['summary']).read_text())
    manifest = summary['manifest']
    wanted = step['tick']
    record = wanted - (step['first_tick'] or manifest.get('first_tick', 1)) + 1
    for attempt in range(1, attempts + 1):
        workdir = OUT / 'work' / Path(step['state']).with_suffix('').name / f'capture-a{attempt}'
        if workdir.exists():
            shutil.rmtree(workdir)
        workdir.mkdir(parents=True)
        built = workdir / 'run.p2s'
        environment = dict(os.environ, PS2_CAPTURE_FPU='exact', SSX3_CAPTURE_DERIVED='1')
        arguments = recapture_exact.build_arguments(dict(manifest, baseline=str(baseline)), built)
        with open(workdir / 'build.log', 'w') as log_file:
            if subprocess.run(arguments, cwd=ROOT, env=environment, stdout=log_file, stderr=subprocess.STDOUT).returncode:
                raise ValueError(f'{step["state"]}: exact build failed ({workdir / "build.log"})')
        snaps = ','.join(str(record + d) for d in range(-width, width + 1) if record + d > 0)
        nav_tool.wait_for_slot(slots)
        frames = record + width + 2
        with open(workdir / 'run.log', 'w') as log_file:
            code = subprocess.run(['nice', '-n', '5', sys.executable, str(ROOT / 'tools/ps2_capture.py'), 'run', str(built), str(workdir / 'run.bin'),
                                   '--frames', str(frames), '--timeout', str(int(frames / 60 * 6) + 900), '--speed', 'turbo', '--snap', snaps,
                                   '--keep-states'], cwd=ROOT, env=environment, stdout=log_file, stderr=subprocess.STDOUT).returncode
        kept = workdir / f'run.tick{wanted}.p2s'
        ticks = sorted(int(p.name.split('.tick')[1].split('.')[0]) for p in workdir.glob('run.tick*.p2s'))
        log(f'  capture {Path(step["state"]).name} attempt {attempt}: exit {code}, kept ticks {ticks} (want {wanted})')
        if kept.exists():
            # Its patches file (the hooks the build installed) is what clean_capture_state.py restores.
            shutil.copyfile(workdir / 'run.patches.json', kept.with_name(kept.stem + '.build-patches.json'))
            return kept
    return None


def derive(target, roots, attempts, width, slots, log):
    target = Path(target).resolve()
    destination = OUT / relative(target).removeprefix('local/')
    if destination.exists():
        log(f'{relative(target)} done')
        return True
    chain = resolve(target, roots)
    cache = json.loads(STEPS.read_text()) if STEPS.exists() else {}
    current = None
    record = []
    for step in chain:
        key = step['state'] + ':' + step['kind']
        if key in cache and Path(cache[key]).exists():
            current = Path(cache[key])
            record.append(dict(step, exact=relative(current), reused=True))
            continue
        if step['kind'] == 'session':
            current = session_state(step)
        elif step['kind'] == 'menu':
            current = ROOT / step['state']
        elif step['kind'] == 'patch':
            output = OUT / 'work' / 'patched' / (Path(step['state']).stem + '.p2s')
            output.parent.mkdir(parents=True, exist_ok=True)
            apply_patches(current, ROOT / step['patches'], output)
            current = output
        elif step['kind'] == 'capture':
            current = capture_step(current, step, attempts, width, slots, log)
            if current is None:
                log(f'{relative(target)}: {step["state"]} not reached at tick {step["tick"]}')
                return False
        elif step['kind'] == 'nav':
            current = nav_step(current, step, attempts, width, slots, log)
            if current is None:
                log(f'{relative(target)}: {step["state"]} not reached at tick {step["tick"]}')
                return False
        cache[key] = str(current)
        STEPS.parent.mkdir(parents=True, exist_ok=True)
        STEPS.write_text(json.dumps(cache, indent=1) + '\n')
        record.append(dict(step, exact=relative(current)))
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(current, destination)
    provenance = dict(reference=relative(target), fpu_mode='exact', chain=record, tick=nav_tool.game_tick(destination),
                      ee_sha256=ee_hash(destination))
    destination.with_suffix('.provenance.json').write_text(json.dumps(provenance, indent=1) + '\n')
    log(f'{relative(target)} written: {relative(destination)} (tick {provenance["tick"]})')
    return True


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('states', nargs='+')
    parser.add_argument('--list', action='store_true')
    parser.add_argument('--attempts', type=int, default=4)
    parser.add_argument('--width', type=int, default=1)
    parser.add_argument('--slots', type=int, default=4, help='machine-wide ARMSX2 limit (pgrep -x ARMSX2)')
    args = parser.parse_args()
    roots = session_roots()
    ok = True
    for state in args.states:
        try:
            if args.list:
                chain = resolve(Path(state).resolve(), roots)
                print(relative(Path(state).resolve()))
                for step in chain:
                    print('   ', step['kind'], step['state'], {k: v for k, v in step.items() if k in ('frame', 'cleaning', 'sample', 'tick', 'recovered', 'first_tick')})
                continue
            ok = derive(state, roots, args.attempts, args.width, args.slots, lambda text: print(text, flush=True)) and ok
        except Exception as error:
            print(f'{state} ERROR {error}', flush=True)
            ok = False
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
