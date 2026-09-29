#!/usr/bin/env python3
"""Verify pinned runtime patch reproducibility without altering its checkout."""
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
RUNTIME = ROOT / 'local/vendor/GXRuntime'
PATCH = ROOT / 'patches/gamecube/gxruntime-ssx3.patch'
MANIFEST = ROOT / 'patches/gamecube/active-patch-manifest.json'

def run(*args, **kwargs):
    return subprocess.run(args, check=True, capture_output=True, **kwargs).stdout

def main():
    manifest = json.loads(MANIFEST.read_text())
    revision = run('git', '-C', str(RUNTIME), 'rev-parse', 'HEAD').decode().strip()
    if revision != manifest['upstream_revision']:
        raise SystemExit('Runtime revision differs from patch baseline')
    if hashlib.sha256(PATCH.read_bytes()).hexdigest() != manifest['patch_sha256']:
        raise SystemExit('Patch changed; review and regenerate its manifest')
    modified = set(run('git', '-C', str(RUNTIME), 'diff', '--name-only').decode().splitlines())
    if modified - manifest['files'].keys():
        raise SystemExit('Modified runtime files missing from manifest: ' + ', '.join(sorted(modified - manifest['files'].keys())))
    with tempfile.TemporaryDirectory(prefix='ssx-runtime-verify-') as folder:
        folder = Path(folder)
        baseline = {}
        for name, expected in manifest['files'].items():
            current = (RUNTIME / name).read_bytes()
            if hashlib.sha256(current).hexdigest() != expected:
                raise SystemExit('Unrecorded runtime change: ' + name)
            result = subprocess.run(['git', '-C', str(RUNTIME), 'show', 'HEAD:' + name], capture_output=True)
            if result.returncode:
                # Confirm absence rather than interpreting any git error as a new file.
                if run('git', '-C', str(RUNTIME), 'ls-tree', 'HEAD', '--', name):
                    raise SystemExit('Could not read baseline: ' + name)
                baseline[name] = None
            else:
                baseline[name] = result.stdout
                target = folder / name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(result.stdout)
        run('git', 'apply', '--check', str(PATCH), cwd=folder)
        run('git', 'apply', str(PATCH), cwd=folder)
        for name, expected in manifest['files'].items():
            if hashlib.sha256((folder / name).read_bytes()).hexdigest() != expected:
                raise SystemExit('Clean patch result differs: ' + name)
        run('git', 'apply', '--reverse', str(PATCH), cwd=folder)
        for name, original in baseline.items():
            path = folder / name
            if original is None:
                if path.exists(): raise SystemExit('Reverse left a new file: ' + name)
            elif path.read_bytes() != original:
                raise SystemExit('Reverse changed baseline: ' + name)
    run('git', '-C', str(RUNTIME), 'apply', '--reverse', '--check', str(PATCH))
    print(f"Verified {len(manifest['files'])} runtime files: clean apply, exact reproduction, reverse, idempotence")

if __name__ == '__main__':
    main()
