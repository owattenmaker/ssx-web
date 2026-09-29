#!/usr/bin/env python3
"""Build tools of the WASM core (tools/setup_from_iso.py core-* steps). Downloads, so they run once.

    python3 tools/setup_toolchain.py venv            .venv with rabbitizer (engine/generated: the ELF lift)
    python3 tools/setup_toolchain.py emsdk [DIR]     Emscripten 6.0.9: use DIR/$EMSDK if it has it, else install into
                                                     local/vendor/emsdk (git + network, about 1 GB)
    python3 tools/setup_toolchain.py emxx            print the em++ the core build will use

SPDX-License-Identifier: GPL-3.0
"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EMSDK_VERSION = '6.0.9'
EMSDK_REPO = 'https://github.com/emscripten-core/emsdk.git'
RABBITIZER = 'rabbitizer==1.14.3'


def em_version(emxx):
    try:
        out = subprocess.run([str(emxx), '--version'], capture_output=True, text=True, timeout=120).stdout
    except (OSError, subprocess.TimeoutExpired):
        return None
    import re
    m = re.search(r'\) (\d+\.\d+\.\d+)', out.splitlines()[0] if out else '')
    return m[1] if m else None


def find_emxx(candidates=()):
    roots = [Path(c) for c in candidates if c]
    if os.environ.get('EMSDK'):
        roots.append(Path(os.environ['EMSDK']))
    roots.append(ROOT / 'local/vendor/emsdk')
    for r in roots:
        emxx = r / 'upstream/emscripten/em++'
        if emxx.exists() and em_version(emxx) == EMSDK_VERSION:
            return emxx
    on_path = shutil.which('em++')
    if on_path and em_version(on_path) == EMSDK_VERSION:
        return Path(on_path)
    return None


def emsdk(given=None):
    emxx = find_emxx([given])
    if emxx:
        print(f'Emscripten {EMSDK_VERSION}: {emxx}')
        return emxx
    dest = ROOT / 'local/vendor/emsdk'
    if not dest.exists():
        subprocess.run(['git', 'clone', '--depth', '1', EMSDK_REPO, str(dest)], check=True)
    subprocess.run([str(dest / 'emsdk'), 'install', EMSDK_VERSION], check=True)
    subprocess.run([str(dest / 'emsdk'), 'activate', EMSDK_VERSION], check=True)
    emxx = find_emxx()
    if not emxx:
        raise SystemExit(f'setup_toolchain: installed emsdk {EMSDK_VERSION} but em++ does not report it')
    print(f'Emscripten {EMSDK_VERSION}: {emxx}')
    return emxx


def venv():
    py = ROOT / '.venv/bin/python3'
    if not py.exists():
        subprocess.run([sys.executable, '-m', 'venv', str(ROOT / '.venv')], check=True)
    if subprocess.run([str(py), '-c', 'import rabbitizer'], capture_output=True).returncode:
        subprocess.run([str(py), '-m', 'pip', 'install', '--quiet', RABBITIZER], check=True)
    print(f'.venv ready ({RABBITIZER})')


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else ''
    if cmd == 'venv':
        venv()
    elif cmd == 'emsdk':
        emsdk(sys.argv[2] if len(sys.argv) > 2 else None)
    elif cmd == 'emxx':
        emxx = find_emxx([sys.argv[2] if len(sys.argv) > 2 else None])
        if not emxx:
            raise SystemExit(f'no Emscripten {EMSDK_VERSION} (run: python3 tools/setup_toolchain.py emsdk)')
        print(emxx)
    else:
        raise SystemExit(__doc__)


if __name__ == '__main__':
    main()
