"""Re-run every gated PS2 capture in ARMSX2's exact FPU / VU0 mode into a parallel tree (docs/ps2-float.md).

usage: recapture_exact.py [--list] [--only NAME,...] [--jobs N] [--slots N] [--out DIR] [--mode exact|mode1]

For each gate name in web/test-ps2-captures.mjs (human gates first, then the computer-rider races):
  - the savestate is the gated run's own (its .json "state"), or, when that file is gone, rebuilt
    by tools/ps2_capture.py build from the gate's capture.json (baseline, script, isolation, camera
    variant, watches, --ai-state, pokes, audio log), which is deterministic;
  - it runs with PS2_CAPTURE_FPU=exact for the gate's record count, into OUT/NAME.bin (+ .json);
  - OUT/NAME.capture.json is the gate's manifest with fpu_mode "exact";
  - every other file of the gate (pads, weather maps, reports) is linked into OUT unchanged;
  - a gate with an .rng-order.json gets its --ai-state companion run re-captured too, and the sidecar
    regenerated against the new ring (web/ps2-capture-ai.mjs --rng-order).
Gates captured by other flows (no capture.json script, closed-loop pads) are listed as "manual".
A finished gate (OUT/NAME.json with fpu_mode exact and the full record count) is skipped, so the
script resumes. Emulators: at most --slots concurrent ARMSX2 processes on the machine (pgrep -x).
"""
import argparse
import concurrent.futures
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RUNS = ROOT / 'local/ps2-capture/runs'
TEST = ROOT / 'web/test-ps2-captures.mjs'
EMULATOR_OUTPUTS = ('.bin', '.json', '.pcsx2.log', '.audio.json', '.capture.json', '.rng-order.json', '.p2s')
start_lock = threading.Lock()


def gate_names():
    source = TEST.read_text()
    ai_start = source.index('const aiCases = [')
    human = re.findall(r"name: '([^']+)'", source[:ai_start])
    computer = re.findall(r"name: '([^']+)'", source[ai_start:])
    ids = re.findall(r"'([^']+)'", re.search(r'RIDER_GATE_IDS = \[(.*?)\];', source, re.S).group(1))
    riders = [f'riders/{rider}-{kind}' for rider in ids for kind in ('race', 'hl')]
    ordered = []
    for name in riders + human + computer:
        if name not in ordered:
            ordered.append(name)
    return ordered


def emulators_running():
    result = subprocess.run(['pgrep', '-x', 'ARMSX2'], capture_output=True, text=True)
    return len(result.stdout.split())


def wait_for_slot(slots):
    # Hold the lock until our emulator is visible, so two workers cannot take the same slot.
    start_lock.acquire()
    while emulators_running() >= slots:
        time.sleep(5)


def release_after_start(process_started):
    deadline = time.time() + 60
    while time.time() < deadline and not process_started():
        time.sleep(0.5)
    start_lock.release()


def resolve(path):
    return Path(os.path.realpath(path))


def plan(name, source=None):
    """What it takes to re-run one capture: the state to run (or how to rebuild it) and the record count.

    name is the path under the output tree; source is the capture's path without .bin (default RUNS / name)."""
    source = Path(source) if source else RUNS / name
    gate_bin = Path(f'{source}.bin')
    manifest_path = Path(f'{source}.capture.json')
    if not gate_bin.exists() or not manifest_path.exists():
        return dict(name=name, kind='missing', source=str(source))
    manifest = json.loads(manifest_path.read_text())
    record = manifest.get('record', 8192)
    records = gate_bin.stat().st_size // record
    summary_path = Path(f'{source}.json')
    summary = json.loads(summary_path.read_text()) if summary_path.exists() else {}
    if summary.get('records'):
        records = summary['records']
    state = summary.get('state')
    if state and (ROOT / state).exists() and resolve(ROOT / state).with_suffix('.capture.json').exists():
        return dict(name=name, kind='run', state=str(resolve(ROOT / state)), records=records, manifest=manifest, source=str(source))
    script = manifest.get('script')
    if manifest.get('baseline') and script and Path(script).exists() and Path(manifest['baseline']).exists():
        return dict(name=name, kind='rebuild', records=records, manifest=manifest, source=str(source))
    return dict(name=name, kind='manual', records=records, source=str(source))


def same_patches(a, b):
    """Same baseline (by hash) and the same patch list; the baseline's path spelling may differ."""
    first = json.loads(Path(a).read_text())
    second = json.loads(Path(b).read_text())
    same_source = first['source'].get('sha256') == second['source'].get('sha256')
    return same_source and first['patches'] == second['patches']


def build_arguments(manifest, output_state):
    arguments = [sys.executable, str(ROOT / 'tools/ps2_capture.py'), 'build', manifest['baseline'], manifest['script'], str(output_state)]
    if manifest.get('isolated_from_computer_riders'):
        arguments.append('--isolate')
    if manifest.get('camera_variant') is not None:
        arguments += ['--camera-variant', str(manifest['camera_variant'])]
    for watch in manifest.get('layout', {}).get('watches', []):
        arguments += ['--watch', f"{watch['address']}:{watch['length']}"]
    if manifest.get('ai_state'):
        arguments.append('--ai-state')
    for poke in manifest.get('pokes', []):
        arguments += ['--poke', f"{poke['address']}:{poke['value']}"]
    if manifest.get('audio_log'):
        arguments.append('--audio-log')
    return arguments


def link_inputs(name, source, out):
    """Link the capture's other files (inputs, maps, reports) into the parallel tree."""
    source_dir = Path(source).parent
    stem = Path(source).name
    target_dir = (out / name).parent
    target_dir.mkdir(parents=True, exist_ok=True)
    for path in source_dir.glob(f'{stem}.*'):
        suffix = path.name[len(stem):]
        if suffix in EMULATOR_OUTPUTS or not path.is_file():
            continue
        link = target_dir / path.name
        if not link.exists() and not link.is_symlink():
            link.symlink_to(resolve(path))


def finished(out_bin, records):
    summary = out_bin.with_suffix('.json')
    if not summary.exists():
        return False
    data = json.loads(summary.read_text())
    return data.get('fpu_mode') == ENV_MODE and data.get('records', 0) >= records


def run_one(job, out, slots, log):
    name = job['name']
    out_bin = out / f'{name}.bin'
    out_bin.parent.mkdir(parents=True, exist_ok=True)
    if finished(out_bin, job['records']):
        return name, 'done'
    environment = dict(os.environ, PS2_CAPTURE_FPU=ENV_MODE)
    with open(out / f'{name}.recapture.log', 'w') as status:
        if job['kind'] == 'rebuild':
            state = out / f'{name}.p2s'
            subprocess.run(build_arguments(job['manifest'], state), cwd=ROOT, env=environment, stdout=status, stderr=subprocess.STDOUT, check=True)
            # The rebuilt savestate must carry exactly the gate's patches (same baseline hash, same hooks and pad).
            gate_patches = Path(f"{job['source']}.patches.json")
            rebuilt_patches = state.with_suffix('.patches.json')
            if not gate_patches.exists() or not same_patches(gate_patches, rebuilt_patches):
                return name, 'manual (rebuilt patches differ from the gate)'
        else:
            state = Path(job['state'])
            manifest = dict(job['manifest'], fpu_mode=ENV_MODE)
            (out / f'{name}.capture.json').write_text(json.dumps(manifest, indent=2) + '\n')
        timeout = int(job['records'] / 60 * 4) + 600
        arguments = ['nice', '-n', '10', sys.executable, str(ROOT / 'tools/ps2_capture.py'), 'run', str(state), str(out_bin),
                     '--frames', str(job['records']), '--timeout', str(timeout)]
        wait_for_slot(slots)
        before = emulators_running()
        process = subprocess.Popen(arguments, cwd=ROOT, env=environment, stdout=status, stderr=subprocess.STDOUT)
        release_after_start(lambda: emulators_running() > before or process.poll() is not None)
        code = process.wait()
    if code != 0:
        return name, f'FAILED ({code})'
    if job['kind'] == 'rebuild':
        shutil.copyfile(state.with_suffix('.capture.json'), out / f'{name}.capture.json')
    link_inputs(name, job['source'], out)
    log(f'{name} ok')
    if job.get('gate'):
        regenerate_rng_order(job['gate'], out_bin, out, log)
    if job.get('self_rng'):
        regenerate_rng_order(name, out_bin, out, log)
    return name, 'ok'


def regenerate_rng_order(gate, companion_bin, out, log):
    """The gate's rng-order sidecar from the re-captured companion, once both rings exist."""
    gate_bin = out / f'{gate}.bin'
    for _ in range(720):
        if finished(gate_bin, 1):
            break
        time.sleep(5)
    result = subprocess.run(['node', 'ps2-capture-ai.mjs', str(companion_bin), '--rng-order', str(gate_bin)], cwd=ROOT / 'web',
                            capture_output=True, text=True)
    log(f'{gate} rng-order {"ok" if result.returncode == 0 else "FAILED " + result.stderr.strip()[-200:]}')


def rng_companions(names):
    """Gates whose rng-order.json came from an --ai-state companion run: gate -> companion bin."""
    companions = {}
    for name in names:
        sidecar = RUNS / f'{name}.rng-order.json'
        if sidecar.exists():
            source = json.loads(sidecar.read_text()).get('source')
            if source:
                companions[name] = source
    return companions


ENV_MODE = 'exact'


def main():
    global ENV_MODE
    parser = argparse.ArgumentParser()
    parser.add_argument('--list', action='store_true')
    parser.add_argument('--only', default='')
    parser.add_argument('--jobs', type=int, default=4)
    parser.add_argument('--slots', type=int, default=4)
    parser.add_argument('--out', default=str(ROOT / 'local/ps2-capture/runs-exact'))
    parser.add_argument('--mode', default='exact')
    args = parser.parse_args()
    ENV_MODE = args.mode
    out = Path(args.out)
    names = gate_names()
    if args.only:
        wanted = args.only.split(',')
        names = [name for name in names if name in wanted]
    jobs = [plan(name) for name in names]
    if args.list:
        for job in jobs:
            print(job['kind'], job['name'], job.get('records', ''))
        counts = {}
        for job in jobs:
            counts[job['kind']] = counts.get(job['kind'], 0) + 1
        print(counts, 'records', sum(job.get('records', 0) for job in jobs if job['kind'] in ('run', 'rebuild')))
        companions = rng_companions(names)
        print('rng-order companions', len(companions))
        return
    out.mkdir(parents=True, exist_ok=True)
    print_lock = threading.Lock()

    def log(text):
        with print_lock:
            print(text, flush=True)

    runnable = [job for job in jobs if job['kind'] in ('run', 'rebuild')]
    # rng-order sidecars come from an --ai-state companion run: the gate itself (regenerated from its own new ring) or
    # another capture (re-captured too, under OUT/_companions/).
    by_name = {job['name']: job for job in runnable}
    for gate, source in rng_companions(list(by_name)).items():
        source_path = resolve(source if os.path.isabs(source) else ROOT / 'web' / source)
        source_stem = str(source_path)[:-len('.bin')]
        if source_path == resolve(RUNS / f'{gate}.bin'):
            by_name[gate]['self_rng'] = True
            continue
        companion = plan(f'_companions/{gate}', source_stem)
        companion['gate'] = gate
        if companion['kind'] in ('run', 'rebuild'):
            runnable.append(companion)
        else:
            log(f'{gate} rng-order companion {source_path} {companion["kind"]}')
    for job in jobs:
        if job['kind'] not in ('run', 'rebuild'):
            log(f"{job['name']} {job['kind']}")
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(run_one, job, out, args.slots, log) for job in runnable]
        for future in concurrent.futures.as_completed(futures):
            try:
                name, status = future.result()
                if status != 'ok':
                    log(f'{name} {status}')
            except Exception as error:
                log(f'ERROR {error}')


if __name__ == '__main__':
    main()
