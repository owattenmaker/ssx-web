#!/usr/bin/env python3
"""Copy an exporter's scratch output tree into web/public/assets (tools/setup_from_iso.py).

    python3 tools/install_export.py local/export/fog-puffs [--assets web/public/assets]

Some exporters refuse to write into web/public/assets and take --out (a folder laid out like the asset root:
<out>/<LOC>/fog-puffs.json ...). The pipeline runs them into local/export/<name> and installs the result with this
tool: every file is written to a temporary name and renamed over the target, so a hard-linked target is replaced, never
edited in place.

SPDX-License-Identifier: GPL-3.0
"""
import argparse
import os
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('source', type=Path)
    ap.add_argument('--assets', type=Path, default=ROOT / 'web/public/assets')
    a = ap.parse_args()
    src = a.source if a.source.is_absolute() else ROOT / a.source
    if not src.is_dir():
        raise SystemExit(f'install_export: {src} is not a folder')
    n = 0
    for p in sorted(src.rglob('*')):
        if not p.is_file():
            continue
        dest = a.assets / p.relative_to(src)
        dest.parent.mkdir(parents=True, exist_ok=True)
        tmp = dest.with_name(dest.name + '.install-tmp')
        shutil.copyfile(p, tmp)
        os.replace(tmp, dest)
        n += 1
    print(f'installed {n} files from {src.relative_to(ROOT) if src.is_relative_to(ROOT) else src} into {a.assets}')


if __name__ == '__main__':
    main()
