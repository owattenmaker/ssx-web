#!/usr/bin/env python3
"""Generate experimental C++ from a fingerprint-checked SSX 3 executable."""
import hashlib
import filecmp
import json
import re
import shutil
import subprocess
import tempfile
from pathlib import Path
from inspect_disc import EXPECTED_SHA1
from function_map import write_function_map

ROOT = Path(__file__).resolve().parents[1]


def main():
    elf = ROOT / 'local/disc/SLUS_207.72'
    if hashlib.sha1(elf.read_bytes()).hexdigest() != EXPECTED_SHA1:
        raise SystemExit('Executable does not match supported SSX 3 NTSC-U build')
    def logged(name, command):
        log = ROOT / f'local/{name}.log'
        print(f'Running {name}; log: {log}', flush=True)
        with log.open('w') as stream:
            subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, check=True)
    logged('analyzer', ['build/ps2recomp/ps2xAnalyzer/ps2_analyzer', 'local/disc/SLUS_207.72', 'local/ssx3.toml'])
    with tempfile.TemporaryDirectory(prefix='translate-', dir=ROOT / 'local') as temporary:
        stage = Path(temporary)
        config = (ROOT / 'local/ssx3.toml').read_text()
        config, count = re.subn(r'^output = .*$', 'output = ' + json.dumps(str(stage / 'output')), config, flags=re.M)
        if count != 1:
            raise SystemExit('Unexpected analyzer config output field')
        (stage / 'config.toml').write_text(config)
        logged('recompiler', ['build/ps2recomp/ps2xRecomp/ps2_recomp', str(stage / 'config.toml')])
        function_map = ROOT / 'local/functions.csv'
        count = write_function_map(stage / 'output', ROOT / 'config/function_entries.json', function_map)
        print(f'Recompiling with {count} heuristic and verified function entries.', flush=True)
        config = re.sub(r'^ghidra_output = .*$', 'ghidra_output = ' + json.dumps(str(function_map)), config, flags=re.M)
        (stage / 'config.toml').write_text(config)
        logged('recompiler', ['build/ps2recomp/ps2xRecomp/ps2_recomp', str(stage / 'config.toml')])
        destination = ROOT / 'local/output'
        destination.mkdir(exist_ok=True)
        changed = 0
        for source in (stage / 'output').iterdir():
            target = destination / source.name
            if not target.exists() or not filecmp.cmp(source, target, shallow=False):
                shutil.copy2(source, target)
                changed += 1
        print(f'Updated {changed} generated files; unchanged files retain build timestamps.')
    print('C++ generated in local/output. This does not build or run the game.')


if __name__ == '__main__':
    main()
