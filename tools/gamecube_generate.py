#!/usr/bin/env python3
"""Generate native engine code and insert verified SDK service boundaries."""
import filecmp
import hashlib
import json
import shutil
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    dol = ROOT / 'local/gamecube/disc/sys/main.dol'
    if hashlib.sha256(dol.read_bytes()).hexdigest() != 'b92162d6c616be3ce46b4eb61d5ddbb49891bc387ea7ddb2fea5792842fa29ce':
        raise SystemExit('Unsupported SSX 3 GameCube executable')
    hooks = json.loads((ROOT / 'config/gamecube-sdk.json').read_text())
    with tempfile.TemporaryDirectory(prefix='gc-gen-', dir=ROOT / 'local') as tmp:
        with (ROOT / 'local/gamecube/translate-dolrecomp.log').open('w') as log:
            subprocess.run([str(ROOT / 'build/dolrecomp/dolrecomp'), '--gamecube', '--game-id', 'GXBE69',
                            '--runtime', 'recompcore', '-j6', str(dol), tmp],
                           check=True, stdout=log, stderr=subprocess.STDOUT)
        generated = Path(tmp) / 'generated'
        hits = {entry['address']: 0 for entry in hooks}
        for path in (generated / 'chunks').glob('*.c'):
            source = path.read_text()
            for entry in hooks:
                address = int(entry['address'], 0)
                label = f'label_{address:08X}:\n'
                count = source.count(label)
                if count:
                    source = source.replace(label, label + f'    if (ssx3_sdk_dispatch(ctx, 0x{address:08X}u)) return;\n')
                    hits[entry['address']] += count
            path.write_text(source)
        if any(count == 0 for count in hits.values()):
            raise SystemExit(f'SDK boundary absent from native code: {hits}')
        output = ROOT / 'local/gamecube/dolrecomp/generated'
        changed = 0
        for source in generated.rglob('*'):
            if not source.is_file():
                continue
            dest = output / source.relative_to(generated)
            dest.parent.mkdir(parents=True, exist_ok=True)
            if not dest.exists() or not filecmp.cmp(source, dest, shallow=False):
                shutil.copy2(source, dest)
                changed += 1
        print(f'Updated {changed} native engine files. SDK entries: {hits}')


if __name__ == '__main__':
    main()
