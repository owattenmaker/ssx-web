#!/usr/bin/env python3
"""Prepare this SSX 3 GameCube image for the native build; keep all data local."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path
from dol import Dol

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = 'b92162d6c616be3ce46b4eb61d5ddbb49891bc387ea7ddb2fea5792842fa29ce'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    args = parser.parse_args()
    source = args.image.resolve(strict=True)
    nod = ROOT / 'local/bin/nodtool'
    base = ROOT / 'local/gamecube'
    base.mkdir(parents=True, exist_ok=True)
    info = subprocess.check_output([str(nod), 'info', str(source)], text=True)
    if 'Game ID: GXBE69' not in info:
        raise SystemExit('Expected SSX 3 GameCube USA GXBE69')
    destination = base / 'disc'
    if not destination.exists():
        subprocess.run([str(nod), 'extract', '--quiet', str(source), str(destination)], check=True)
    dol = Dol(destination / 'sys/main.dol')
    if hashlib.sha256(dol.data).hexdigest() != EXPECTED:
        raise SystemExit('Unexpected executable in extracted data; existing files left intact')
    iso = base / 'ssx3.iso'
    if not iso.exists():
        subprocess.run([str(nod), 'convert', str(source), str(iso)], check=True)
    with iso.open('rb') as stream:
        if stream.read(6) != b'GXBE69':
            raise SystemExit('Existing ISO has an unexpected disc ID')
    report = dol.report()
    report['disc_id'] = 'GXBE69'
    report['files'] = [dict(path=str(p.relative_to(destination / 'files')), size=p.stat().st_size)
                       for p in sorted((destination / 'files').rglob('*')) if p.is_file()]
    (base / 'inventory.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'Verified GXBE69 executable, {len(report["files"])} assets. Prepared {destination}')


if __name__ == '__main__':
    main()
