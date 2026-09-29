#!/usr/bin/env python3
"""Set up pinned native GameCube tools without a Dolphin application or JIT."""
import hashlib
import subprocess
import sys
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPOS = {
    'DolRecomp': ('https://github.com/ExpansionPak/DolRecomp.git', '40637c4683bd2820ac5b23607ee344720beb26df'),
    'GXRuntime': ('https://github.com/aharonahdoot/GXRuntime.git', 'f83d25877c7b701468c978c09559d513c4971ddd'),
}
NOD_URL = 'https://github.com/encounter/nod/releases/download/v2.0.0-alpha.10/nodtool-macos-arm64'
NOD_SHA256 = 'e23ca466999b720c55e6d29c9683fce8cc74451ba64ead2e543d50129f24528a'


def run(*command):
    subprocess.run([str(part) for part in command], cwd=ROOT, check=True)


def main():
    for name, (url, revision) in REPOS.items():
        path = ROOT / 'local/vendor' / name
        if not path.exists():
            path.parent.mkdir(parents=True, exist_ok=True)
            run('git', 'init', path)
            run('git', '-C', path, 'remote', 'add', 'origin', url)
            run('git', '-C', path, 'fetch', '--depth', '1', 'origin', revision)
            run('git', '-C', path, 'checkout', '--detach', 'FETCH_HEAD')
        actual = subprocess.check_output(['git', '-C', str(path), 'rev-parse', 'HEAD'], text=True).strip()
        if actual != revision:
            raise SystemExit(f'{name} is at {actual}; expected {revision}. Checkout left untouched.')
    runtime = ROOT / 'local/vendor/GXRuntime'
    for patch in sorted((ROOT / 'patches/gamecube').glob('gxruntime-*.patch')):
        applied = subprocess.run(['git', '-C', str(runtime), 'apply', '--reverse', '--check', str(patch)],
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0
        if not applied:
            run('git', '-C', runtime, 'apply', '--check', patch)
            run('git', '-C', runtime, 'apply', patch)
    venv = ROOT / '.venv'
    if not (venv / 'bin/python').exists():
        run(sys.executable, '-m', 'venv', venv)
    run(venv / 'bin/python', '-m', 'pip', 'install', 'cmake==4.4.3', 'ninja==1.13.2', 'capstone==5.0.9')
    nod = ROOT / 'local/bin/nodtool'
    if not nod.exists():
        data = urllib.request.urlopen(NOD_URL).read()
        if hashlib.sha256(data).hexdigest() != NOD_SHA256:
            raise SystemExit('Disc tool download checksum mismatch')
        nod.parent.mkdir(parents=True, exist_ok=True)
        nod.write_bytes(data)
        nod.chmod(0o755)
    if hashlib.sha256(nod.read_bytes()).hexdigest() != NOD_SHA256:
        raise SystemExit('Existing disc tool checksum mismatch')
    run(venv / 'bin/cmake', '-S', ROOT / 'local/vendor/DolRecomp', '-B', ROOT / 'build/dolrecomp',
        '-G', 'Ninja', f'-DCMAKE_MAKE_PROGRAM={venv / "bin/ninja"}', '-DCMAKE_BUILD_TYPE=Release')
    run(venv / 'bin/cmake', '--build', ROOT / 'build/dolrecomp', '--target', 'dolrecomp', '-j', '8')


if __name__ == '__main__':
    main()
