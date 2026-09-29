#!/usr/bin/env python3
"""Compare the generated data of two trees file by file (tools/setup_from_iso.py --verify-against).

    python3 tools/verify_assets.py BUILT REFERENCE [--report local/pipeline/verify.json] [--scope web/public/assets ...]

For every file under the scopes (default web/public/assets, web/generated, engine/generated): identical, different,
only in BUILT, only in REFERENCE. Different files get a first classification so the report can be read quickly:

  json-same-value   the JSON parses to the same value (formatting / key order only)
  json-values       the JSON differs in values: the first differing paths are listed
  tex-same-texels   a texture archive (.tex) whose entries decode to the same texels (tools/texture_archive.py digests)
  png-same-pixels   a PNG with the same pixels (tools/texture_archive.py decode_png)
  bytes             anything else (first differing offset)

SPDX-License-Identifier: GPL-3.0
"""
import argparse
import hashlib
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
SCOPES = ('web/public/assets', 'web/generated', 'engine/generated')


def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def json_paths(a, b, path='', out=None, limit=12):
    out = [] if out is None else out
    if len(out) >= limit:
        return out
    if type(a) is not type(b):
        out.append(f'{path or "/"}: {type(a).__name__} vs {type(b).__name__}')
    elif isinstance(a, dict):
        for k in sorted(set(a) | set(b), key=str):
            if k not in a:
                out.append(f'{path}/{k}: only in reference')
            elif k not in b:
                out.append(f'{path}/{k}: only in built')
            elif a[k] != b[k]:
                json_paths(a[k], b[k], f'{path}/{k}', out, limit)
            if len(out) >= limit:
                break
    elif isinstance(a, list):
        if len(a) != len(b):
            out.append(f'{path}: {len(a)} vs {len(b)} items')
        for i, (x, y) in enumerate(zip(a, b)):
            if x != y:
                json_paths(x, y, f'{path}[{i}]', out, limit)
            if len(out) >= limit:
                break
    else:
        s = lambda v: (repr(v)[:60])
        out.append(f'{path}: {s(a)} vs {s(b)}')
    return out


def classify(a, b):
    name = a.name
    if name.endswith('.json'):
        try:
            ja, jb = json.loads(a.read_bytes()), json.loads(b.read_bytes())
        except ValueError:
            return dict(kind='bytes')
        if ja == jb:
            return dict(kind='json-same-value')
        return dict(kind='json-values', paths=json_paths(ja, jb))
    if name.endswith('.tex'):
        try:
            from texture_archive import read_index
            ia, ib = read_index(a), read_index(b)
            ea = {e['id']: e.get('rgba') for e in ia['entries']}
            eb = {e['id']: e.get('rgba') for e in ib['entries']}
            if ea == eb:
                return dict(kind='tex-same-texels', entries=len(ea))
            changed = sorted(str(k) for k in set(ea) | set(eb) if ea.get(k) != eb.get(k))
            return dict(kind='tex-entries', changed=len(changed), first=changed[:10])
        except Exception as e:  # noqa: BLE001
            return dict(kind='bytes', note=f'tex index unreadable: {e}')
    if name.endswith('.png'):
        try:
            from texture_archive import decode_png
            if decode_png(a.read_bytes()) == decode_png(b.read_bytes()):
                return dict(kind='png-same-pixels')
        except Exception:  # noqa: BLE001
            pass
    da, db = a.read_bytes(), b.read_bytes()
    at = next((i for i, (x, y) in enumerate(zip(da, db)) if x != y), min(len(da), len(db)))
    return dict(kind='bytes', sizes=[len(da), len(db)], first_difference=at)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('built', type=Path); ap.add_argument('reference', type=Path)
    ap.add_argument('--scope', action='append'); ap.add_argument('--report', type=Path)
    ap.add_argument('--no-classify', action='store_true')
    a = ap.parse_args()
    report = dict(built=str(a.built), reference=str(a.reference), scopes={})
    for scope in a.scope or SCOPES:
        A, B = a.built / scope, a.reference / scope
        fa = {p.relative_to(A).as_posix(): p for p in A.rglob('*') if p.is_file()} if A.exists() else {}
        fb = {p.relative_to(B).as_posix(): p for p in B.rglob('*') if p.is_file()} if B.exists() else {}
        same, diff = [], {}
        for rel in sorted(set(fa) & set(fb)):
            if fa[rel].stat().st_size == fb[rel].stat().st_size and sha(fa[rel]) == sha(fb[rel]):
                same.append(rel)
            else:
                diff[rel] = {} if a.no_classify else classify(fa[rel], fb[rel])
        report['scopes'][scope] = dict(identical=len(same), different=diff, only_built=sorted(set(fa) - set(fb)),
                                       only_reference=sorted(set(fb) - set(fa)))
        s = report['scopes'][scope]
        kinds = {}
        for d in diff.values():
            kinds[d.get('kind', '?')] = kinds.get(d.get('kind', '?'), 0) + 1
        print(f'{scope}: {len(same)} identical, {len(diff)} different {kinds}, {len(s["only_built"])} only built, '
              f'{len(s["only_reference"])} only reference')
    out = a.report or (a.built / 'local/pipeline/verify.json')
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, indent=1))
    print(f'report: {out}')


if __name__ == '__main__':
    main()
