#!/usr/bin/env python3
"""Run a bounded native startup diagnostic. Success does not imply game playability."""
import argparse
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('iso', type=Path)
    args = parser.parse_args()
    iso = args.iso.resolve(strict=True)
    log = ROOT / 'local/boot.log'
    with log.open('w') as stream:
        try:
            result = subprocess.run([str(ROOT / 'build/native/ssx3_boot_probe'),
                                     str(ROOT / 'local/disc/SLUS_207.72'), str(iso)],
                                    cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, timeout=20)
        except subprocess.TimeoutExpired:
            print(f'Probe exceeded 20 seconds and was stopped. See {log}')
            return 124
    print(f'Probe exit code: {result.returncode}. See {log}; a clean exit is not proof of a successful game boot.')
    return result.returncode


if __name__ == '__main__':
    raise SystemExit(main())
