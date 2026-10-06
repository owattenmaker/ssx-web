"""Link the rider-parity agent's exact-baseline rider captures into the exact-base gate tree (docs/ps2-float.md).

usage: link_riders_exact.py [--from DIR] [--to DIR]

local/ps2-capture/runs/riders-exact/<id>-<kind>.* (captured from local/reference-exact/characters/<id>/ states) are
linked as local/ps2-capture/runs-exactbase/riders/<id>-<kind>.*, the gate names web/test-ps2-captures.mjs uses, so
tools/ps2-float/score_gates.mjs --runs local/ps2-capture/runs-exactbase --arith exact-base covers them. A capture is linked
once its run summary (<name>.json) exists and its manifest's baseline is an exact one (local/reference-exact); an existing file of the same name is replaced only if it is a link.
"""
import argparse
import json
import os
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--from', dest='source', default=str(ROOT / 'local/ps2-capture/runs/riders-exact'))
    parser.add_argument('--to', default=str(ROOT / 'local/ps2-capture/runs-exactbase/riders'))
    args = parser.parse_args()
    source = Path(args.source)
    target = Path(args.to)
    target.mkdir(parents=True, exist_ok=True)
    linked = 0
    for summary in sorted(source.glob('*.json')):
        stem = summary.name[:-len('.json')]
        if '.' in stem or not (source / f'{stem}.bin').exists():
            continue
        # Only captures from an exact baseline (the folder also holds older exact-mode runs from mode-1 baselines).
        manifest_path = source / f'{stem}.capture.json'
        if not manifest_path.exists() or 'reference-exact' not in json.loads(manifest_path.read_text()).get('baseline', ''):
            continue
        for path in source.glob(f'{stem}.*'):
            suffix = path.name[len(stem):]
            link = target / f'{stem}{suffix}'
            if link.exists() and not link.is_symlink():
                # A capture of our own with the same name: keep a copy aside, the rider-parity one is the reference set.
                link.rename(link.with_name(link.name + '.own'))
            if link.is_symlink():
                link.unlink()
            link.symlink_to(os.path.relpath(path, target))
        linked += 1
    print(f'{linked} rider captures linked into {target}')


if __name__ == '__main__':
    main()
