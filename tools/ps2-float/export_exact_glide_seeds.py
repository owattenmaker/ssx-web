"""Exact-arithmetic glide seeds for the capture comparer and the core (docs/ps2-float.md "Exact baselines").

usage: export_exact_glide_seeds.py [--only CODE,...]

A course's glide seed (local/assets/native/<CODE>/riding-start.json, tools/export_riding_start.py) is a recorded result: the
rider block of a mode-1 glide savestate. A gate captured from the exact copy of that savestate (local/reference-exact) starts
from other bits. For every course whose glide source has an exact copy this writes, under local/assets/native-exact/<CODE>/:
- riding-start.json: tools/export_riding_start.py of the exact glide state (web/generate-controllers.py compiles its ground
  state into generated/physics_seed_exact.hpp, selected under SSX_PS2_EXACT_FPU by the exact-seeds switch);
- initial.json: the course's web/public/assets initial.json (ANIMATIONS/initial.json for Snow Jam) with every section the
  riding start also exports replaced by the exact one, merged key by key so that keys only the browser file has stay; the
  secondary motion, the breath context and the retained reset route are re-read from the exact state;
- start.json: the exact riding start's position and heading.
The comparers read the last two with PS2_ARITH=exact-base. Nothing is written into web/public/assets.

The glide source of a course is the reference state whose file hash its riding start records; its exact copy is the
same name under local/reference-exact, or for a re-derived chain state the same path under local/reference-exact/chains.
"""
import argparse
import hashlib
import json
import subprocess
import sys
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'tools'))
from reference_secondary_motion import extract_secondary_motion  # noqa: E402
from reference_reset import extract_reset  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
NATIVE = ROOT / 'local/assets/native'
EXACT_NATIVE = ROOT / 'local/assets/native-exact'
REFERENCE = ROOT / 'local/reference/pcsx2'
EXACT_REFERENCE = ROOT / 'local/reference-exact'


CHAINS = EXACT_REFERENCE / 'chains'


def file_hashes():
    """Mode-1 glide source hash -> (mode-1 state, its exact copy): the references (local/reference-exact/<name>) and the states
    tools/ps2-float/derive_exact_chain.py re-derived (local/reference-exact/chains/<path under local/>, e.g. the Peak 2 free ride)."""
    hashes = {}
    for path in sorted(REFERENCE.glob('*.p2s')):
        hashes[hashlib.sha256(path.read_bytes()).hexdigest()] = (path, EXACT_REFERENCE / path.name)
    for exact in sorted(CHAINS.rglob('*.p2s')):
        source = ROOT / 'local' / exact.relative_to(CHAINS)
        if source.exists():
            hashes.setdefault(hashlib.sha256(source.read_bytes()).hexdigest(), (source, exact))
    return hashes


def breath_region(code):
    """The location whose painter holds the rider in a course's glide source: the course itself, or for a peak / mountain world
    the region its seed was exported with (tools/iso_pipeline_steps.py PEAK_SEEDS / MOUNTAIN_SEEDS)."""
    sys.path.insert(0, str(ROOT / 'tools'))
    import iso_pipeline_steps
    peaks = {f'PEAK{n}': region for n, (_, region) in iso_pipeline_steps.PEAK_SEEDS.items()}
    worlds = {world: region for world, (_, region) in iso_pipeline_steps.MOUNTAIN_SEEDS.items()}
    return peaks.get(code) or worlds.get(code) or code


def merge(base, exact):
    """base with exact's values, recursively for dictionaries (base-only keys are kept)."""
    if isinstance(base, dict) and isinstance(exact, dict):
        out = dict(base)
        for key, value in exact.items():
            out[key] = merge(base[key], value) if key in base else value
        return out
    return exact


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--only', default='')
    args = parser.parse_args()
    hashes = file_hashes()
    wanted = set(args.only.split(',')) if args.only else None
    for start in sorted(NATIVE.glob('*/riding-start.json')):
        code = start.parent.name
        # Hidden folders are kept-aside older exports.
        if code.startswith('.'):
            continue
        if wanted and code not in wanted:
            continue
        digest = json.loads(start.read_text())['reference_start'].get('source_state_sha256')
        source, exact_state = hashes.get(digest, (None, None))
        if not exact_state or not exact_state.exists():
            print(f'{code}: no exact copy of its glide source ({source.name if source else "source not among the references"})')
            continue
        out = EXACT_NATIVE / code
        out.mkdir(parents=True, exist_ok=True)
        riding = out / 'riding-start.json'
        subprocess.run([sys.executable, str(ROOT / 'tools/export_riding_start.py'), str(exact_state), str(riding), '--location', code],
                       cwd=ROOT, check=True, stdout=subprocess.DEVNULL)
        initial = json.loads(riding.read_text())['native']['initial']
        browser_initial = ROOT / 'web/public/assets' / ('ANIMATIONS/initial.json' if code == 'ARA1' else f'{code}/initial.json')
        if browser_initial.exists():
            merged = merge(json.loads(browser_initial.read_text()), initial)
            # Sections only the browser file has that are read from the glide savestate itself: re-read them from the exact one.
            rider = int(json.loads(riding.read_text())['reference_initialization']['rider_address'], 0)
            with zipfile.ZipFile(exact_state) as archive:
                memory = archive.read('eeMemory.bin')
            # The retained human route (path, segment cache, closest / lookahead points, heading +0x4CC) is the glide state's own:
            # its cache origin is a running sum (0x26A638), so a mode-1 or other-state copy carries other bits (pipe-air 339).
            if 'original_reset' in merged:
                merged['original_reset'] = dict(merged['original_reset'], route=extract_reset(memory, rider)['route'])
            if 'secondary_motion' in merged.get('original_animation', {}):
                merged['original_animation']['secondary_motion'] = extract_secondary_motion(memory, rider)
            if 'original_breath' in merged:
                breath = out / 'breath-context.json'
                done = subprocess.run([sys.executable, str(ROOT / 'tools/export_breath_context.py'), '--state', str(exact_state), '--output',
                                       str(breath), '--rider', hex(rider), '--location', breath_region(code)], cwd=ROOT, stdout=subprocess.DEVNULL,
                                       stderr=subprocess.PIPE, text=True)
                # A state export_breath_context cannot read (a streamed mountain world) keeps the browser's breath context.
                if done.returncode == 0:
                    merged['original_breath'] = merge(merged['original_breath'], json.loads(breath.read_text()))
                else:
                    print(f'{code}: breath context not re-read ({done.stderr.strip().splitlines()[-1] if done.stderr.strip() else done.returncode})')
            (out / 'initial.json').write_text(json.dumps(merged) + '\n')
        (out / 'start.json').write_text(json.dumps(dict(position=initial['position'], heading=initial['heading'])) + '\n')
        provenance = dict(glide_state=str(exact_state.relative_to(ROOT)), mode1_glide_state=str(source.relative_to(ROOT)),
                          browser_initial=str(browser_initial.relative_to(ROOT)) if browser_initial.exists() else None)
        (out / 'glide-seed.provenance.json').write_text(json.dumps(provenance, indent=1) + '\n')
        print(f'{code}: {exact_state.name}')


if __name__ == '__main__':
    main()
