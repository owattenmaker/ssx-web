#!/usr/bin/env python3
"""Build pinned PS2 translation tools locally. Does not install system packages."""
import argparse
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REPOS = {
    'PS2Recomp': ('https://github.com/ran-j/PS2Recomp.git', '14b1e5cb39b4af7e6fc12f9a29fdc751efde49d7'),
    'ssx3-decomp': ('https://github.com/ssxdecomp/ssx3.git', '9cd4626054ba96143a849978b7c00bca7d85bcc0'),
}


def run(*args):
    subprocess.run([str(a) for a in args], cwd=ROOT, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--runtime', action='store_true', help='Also attempt experimental runtime build (no movie decoder)')
    args = parser.parse_args()
    for name, (url, revision) in REPOS.items():
        dest = ROOT / 'local/vendor' / name
        if not dest.exists():
            dest.parent.mkdir(parents=True, exist_ok=True)
            run('git', 'init', dest)
            run('git', '-C', dest, 'remote', 'add', 'origin', url)
            run('git', '-C', dest, 'fetch', '--depth', '1', 'origin', revision)
            run('git', '-C', dest, 'checkout', '--detach', 'FETCH_HEAD')
        actual = subprocess.check_output(['git', '-C', str(dest), 'rev-parse', 'HEAD'], text=True).strip()
        if actual != revision:
            raise SystemExit(f'{dest} is at {actual}; expected {revision}. Existing checkout left untouched.')
    upstream = ROOT / 'local/vendor/PS2Recomp'
    for patch in sorted((ROOT / 'patches').glob('*.patch')):
        applied = subprocess.run(['git', '-C', str(upstream), 'apply', '--reverse', '--check', str(patch)],
                                 stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL).returncode == 0
        if not applied:
            run('git', '-C', upstream, 'apply', '--check', patch)
            run('git', '-C', upstream, 'apply', patch)
    venv = ROOT / '.venv'
    if not (venv / 'bin/python').exists():
        run(sys.executable, '-m', 'venv', venv)
    run(venv / 'bin/python', '-m', 'pip', 'install', 'cmake==4.4.3', 'ninja==1.13.2')
    cmake = venv / 'bin/cmake'
    build = ROOT / 'build/ps2recomp'
    run(cmake, '-S', ROOT / 'local/vendor/PS2Recomp', '-B', build, '-G', 'Ninja',
        f'-DCMAKE_MAKE_PROGRAM={venv / "bin/ninja"}', '-DCMAKE_BUILD_TYPE=Release',
        '-DPS2X_BUILD_TEST=OFF', '-DPS2X_BUILD_STUDIO=OFF',
        f'-DPS2X_BUILD_RUNTIME={"ON" if args.runtime else "OFF"}',
        '-DPS2X_ENABLE_DEBUG_UI=OFF', '-DPS2X_ENABLE_FFMPEG=OFF')
    targets = ['ps2_recomp', 'ps2_analyzer']
    if args.runtime:
        targets.append('ps2_runtime')
    run(cmake, '--build', build, '--target', *targets, '-j', '6')


if __name__ == '__main__':
    main()
