#!/usr/bin/env python3
"""Build the browser game's data and core from your own discs (docs/iso-pipeline.md).

    python3 tools/setup_from_iso.py --iso "SSX 3 (USA).iso" --gamecube "SSX 3 (USA).rvz" --state-pack ssx3-statepack.zip

Every step writes into this checkout (web/public/assets, web/generated, engine/generated, web/runtime, local/). Steps are
stamped in local/pipeline/stamps: a rerun resumes where the last one stopped and skips finished steps whose command and
tool source did not change (--force reruns them). Logs: local/pipeline/logs/<step>.log.

Inputs
  --iso PATH          PS2 SSX 3 NTSC-U (SLUS_207.72, redump "SSX 3 (USA)"); the executable is checked by SHA-1
  --gamecube PATH     GameCube SSX 3 USA (GXBE69): .rvz/.iso image or an extracted folder (riders, world lighting)
  --state-pack PATH   the state pack release (tools/statepack.py): the bytes the exporters read from PS2 savestates
  --sam               also build Sam (needs his private inputs: sam_character/, config/characters/sam.json, ...)

Selection
  --list              print the steps and exit        --only PATTERN[,..]   run matching steps (fnmatch)
  --from STEP         start at a step                  --until STEP          stop after a step
  --force             rerun finished steps             --keep-going          continue after a failed step
  --skip PATTERN[,..] leave matching steps out         --no-movies           skip the movie transcode (ffmpeg)

Maintainers
  --trace --states-from LIVE   run with the real savestates of another tree (linked read-only; writes into it refused)
                               and record the footprint for tools/statepack.py
  --make-pack FILE             after the run, build the state pack from its footprint (tools/statepack.py)
  --verify-against DIR         compare web/public/assets and web/generated with another tree (tools/verify_assets.py)

SPDX-License-Identifier: GPL-3.0
"""
import argparse
import fnmatch
import hashlib
import json
import os
import shlex
import shutil
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
STAMPS = ROOT / 'local/pipeline/stamps'
LOGS = ROOT / 'local/pipeline/logs'
STATE_ROOTS = ('local/reference', 'local/ps2-capture', 'build/snow-emission-oracle')
# Working folders the tools write reports and intermediates into without creating them.
WORK_DIRS = ('web/generated', 'engine/generated', 'web/public/assets', 'local/assets/native', 'local/event-activation', 'local/browser-validation', 'local/rider-lighting', 'local/browser-ui',
             'local/browser-pickups', 'local/native-qa', 'local/export', 'local/career', 'local/visual')


class Step:
    def __init__(self, name, cmd, needs=(), sources=(), note='', env=None, shell=False):
        self.name, self.cmd, self.needs, self.note, self.env, self.shell = name, cmd, set(needs), note, env or {}, shell
        self.sources = list(sources)

    def command(self, ctx):
        return [ctx.fmt(str(c)) for c in self.cmd]

    def source_fingerprint(self):
        """The step's command template and tool sources: the same on every machine (compared with the state pack's)."""
        h = hashlib.sha256(json.dumps([str(c) for c in self.cmd]).encode())
        for f in list(self.sources) + [c for c in self.cmd if isinstance(c, str) and c.endswith(('.py', '.sh', '.mjs'))]:
            p = ROOT / f
            if p.is_file():
                h.update(p.read_bytes())
        return h.hexdigest()

    def fingerprint(self, ctx):
        h = hashlib.sha256(json.dumps(self.command(ctx)).encode())
        files = list(self.sources) + [c for c in self.cmd if isinstance(c, str) and c.endswith(('.py', '.sh', '.mjs'))]
        for f in files:
            p = ROOT / f
            if p.is_file():
                h.update(p.read_bytes())
        return h.hexdigest()


class Context:
    def __init__(self, args):
        self.args = args
        self.py = sys.executable
        # relative paths are the caller's: `npm run setup -- --iso ...` runs in web/, npm keeps the caller's folder in INIT_CWD
        base = Path(os.environ.get('INIT_CWD') or os.getcwd())
        full = lambda p: str((base / Path(p).expanduser()).resolve()) if p else None
        self.iso, self.gamecube, self.pack = full(args.iso), full(args.gamecube), full(args.state_pack)
        if args.emsdk:
            args.emsdk = full(args.emsdk)
        if args.states_from:
            args.states_from = full(args.states_from)

    def fmt(self, s):
        if '{emxx}' in s:
            from setup_toolchain import find_emxx
            emxx = find_emxx([self.args.emsdk])
            if not emxx:
                raise SystemExit('setup_from_iso: no Emscripten 6.0.9 (the core-emsdk step installs it; or pass --emsdk DIR)')
            s = s.replace('{emxx}', str(emxx))
        return s.format(py=self.py, iso=self.iso or '', gamecube=self.gamecube or '', pack=self.pack or '', root=str(ROOT),
                        emsdk=self.args.emsdk or '')

    def have(self, need):
        a = self.args
        return {'iso': bool(self.iso), 'gamecube': bool(self.gamecube), 'states': bool(self.pack) or a.trace,
                'pack': bool(self.pack), 'sam': a.sam, 'movies': not a.no_movies, 'trace': a.trace}.get(need, True)


# ------------------------------------------------------------------------------------------------------ the steps
def steps():
    from iso_pipeline_steps import registry
    return registry(Step)


# ------------------------------------------------------------------------------------------------------ running
def step_env(ctx, step):
    env = dict(os.environ)
    env['PYTHONDONTWRITEBYTECODE'] = '1'
    if ctx.iso:
        env['SSX3_ISO'] = ctx.iso
    if ctx.gamecube:
        env['SSX3_GAMECUBE_IMAGE'] = ctx.gamecube
    hook = str(ROOT / 'tools/pipeline_hook')
    env['PYTHONPATH'] = hook + (os.pathsep + env['PYTHONPATH'] if env.get('PYTHONPATH') else '')
    env['SSX3_HOOK_ROOT'] = str(ROOT)
    env['SSX3_HOOK_STEP'] = step.name
    env['SSX3_HOOK_STATE_ROOTS'] = ':'.join(STATE_ROOTS)
    if ctx.args.trace:
        env['SSX3_HOOK_MODE'] = 'trace'
        env['SSX3_HOOK_DIR'] = str(ROOT / 'local/pipeline/trace')
        env['SSX3_HOOK_GUARD'] = str(Path(ctx.args.states_from).resolve())
    elif ctx.pack:
        env['SSX3_HOOK_MODE'] = 'restore'
    env.update({k: ctx.fmt(v) for k, v in step.env.items()})
    return env


def run_step(ctx, step):
    cmd = step.command(ctx)
    LOGS.mkdir(parents=True, exist_ok=True)
    log = LOGS / f'{step.name}.log'
    started = time.time()
    print(f'[{time.strftime("%H:%M:%S")}] {step.name}: {" ".join(shlex.quote(c) for c in cmd)}', flush=True)
    with open(log, 'w') as out:
        out.write('$ ' + ' '.join(shlex.quote(c) for c in cmd) + '\n')
        out.flush()
        if ctx.args.trace:
            # a step's trace starts clean (a rerun must not keep the reads of an earlier attempt)
            shutil.rmtree(ROOT / 'local/pipeline/trace' / step.name, ignore_errors=True)
        r = subprocess.run(cmd, cwd=ROOT, env=step_env(ctx, step), stdout=out, stderr=subprocess.STDOUT)
    took = time.time() - started
    if r.returncode:
        tail = log.read_text(errors='replace').splitlines()[-25:]
        print('\n'.join('    ' + t for t in tail))
        print(f'  FAILED ({r.returncode}) after {took:.0f} s; log {log.relative_to(ROOT)}', flush=True)
        return False
    STAMPS.mkdir(parents=True, exist_ok=True)
    (STAMPS / f'{step.name}.json').write_text(json.dumps(dict(fingerprint=step.fingerprint(ctx), sources=step.source_fingerprint(),
                                                                 seconds=round(took, 1),
                                                                 finished=time.strftime('%Y-%m-%d %H:%M:%S'))))
    print(f'  ok ({took:.0f} s)', flush=True)
    return True


def done(ctx, step):
    stamp = STAMPS / f'{step.name}.json'
    if not stamp.exists():
        return False
    return json.loads(stamp.read_text()).get('fingerprint') == step.fingerprint(ctx)


def link_states(live):
    """--trace: the live tree's savestates and captures, linked read-only (the hook refuses writes into LIVE)."""
    live = Path(live).resolve()
    for rel in STATE_ROOTS:
        src, dst = live / rel, ROOT / rel
        if not src.exists():
            continue
        if dst.is_symlink():
            if dst.resolve() == src:
                continue
            dst.unlink()
        elif dst.exists():
            raise SystemExit(f'--trace: {dst} exists and is not a link to {src}; move it away first')
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.symlink_to(src)
        print(f'linked {rel} -> {src}')
    from iso_pipeline_steps import EVIDENCE
    for rel in EVIDENCE:
        src, dst = live / rel, ROOT / rel
        if src.is_file() and not dst.exists():
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
            print(f'copied evidence {rel}')


def preflight(ctx):
    problems = []
    if sys.version_info < (3, 10):
        problems.append(f'Python 3.10 or newer is required (this is {sys.version.split()[0]})')
    if not ctx.iso:
        problems.append('--iso is required (your PS2 SSX 3 NTSC-U disc image)')
    elif not Path(ctx.iso).is_file():
        problems.append(f'--iso {ctx.iso}: no such file')
    if ctx.gamecube and not Path(ctx.gamecube).exists():
        problems.append(f'--gamecube {ctx.gamecube}: no such file or folder')
    if ctx.pack and not Path(ctx.pack).is_file():
        problems.append(f'--state-pack {ctx.pack}: no such file')
    if ctx.args.trace and not ctx.args.states_from:
        problems.append('--trace needs --states-from (the tree with the real savestates)')
    if ctx.args.trace and ctx.pack:
        problems.append('--trace and --state-pack exclude each other')
    if ctx.args.make_pack and not ctx.args.trace:
        problems.append('--make-pack needs --trace')
    if problems:
        raise SystemExit('setup_from_iso: ' + '\n  '.join(['cannot start:'] + problems))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--iso'); ap.add_argument('--gamecube'); ap.add_argument('--state-pack')
    ap.add_argument('--sam', action='store_true'); ap.add_argument('--no-movies', action='store_true')
    ap.add_argument('--list', action='store_true'); ap.add_argument('--only'); ap.add_argument('--skip')
    ap.add_argument('--from', dest='start'); ap.add_argument('--until'); ap.add_argument('--force', action='store_true')
    ap.add_argument('--keep-going', action='store_true'); ap.add_argument('--trace', action='store_true')
    ap.add_argument('--states-from'); ap.add_argument('--verify-against'); ap.add_argument('--emsdk'); ap.add_argument('--make-pack')
    args = ap.parse_args()
    ctx = Context(args)
    all_steps = steps()
    names = [s.name for s in all_steps]
    if len(set(names)) != len(names):
        raise SystemExit('duplicate step names: ' + ', '.join(sorted({n for n in names if names.count(n) > 1})))
    selected = all_steps
    if args.start:
        selected = selected[names.index(args.start):]
    if args.until:
        selected = selected[:[s.name for s in selected].index(args.until) + 1]
    if args.only:
        pats = args.only.split(',')
        selected = [s for s in selected if any(fnmatch.fnmatch(s.name, p) for p in pats)]
    if args.skip:
        pats = args.skip.split(',')
        selected = [s for s in selected if not any(fnmatch.fnmatch(s.name, p) for p in pats)]
    if args.list:
        for s in selected:
            flags = ','.join(sorted(s.needs))
            print(f'{s.name:48s} {flags:22s} {" ".join(s.cmd)[:110]}')
        print(f'{len(selected)} steps')
        return
    preflight(ctx)
    for d in WORK_DIRS:
        (ROOT / d).mkdir(parents=True, exist_ok=True)
    if args.trace:
        link_states(args.states_from)
    pack_steps = None
    if ctx.pack:
        import zipfile
        with zipfile.ZipFile(ctx.pack) as z:
            pack_steps = json.loads(z.read('manifest.json')).get('steps', {})
    skipped_needs, failed = [], []
    t0 = time.time()
    for s in selected:
        missing = [n for n in s.needs if not ctx.have(n)]
        if missing:
            skipped_needs.append((s.name, missing))
            continue
        if not args.force and done(ctx, s):
            continue
        if pack_steps is not None and s.name in pack_steps and pack_steps[s.name] != s.source_fingerprint():
            print(f'  note: {s.name} changed since the state pack was made; if it fails, it may need a newer pack')
        if not run_step(ctx, s):
            failed.append(s.name)
            if not args.keep_going:
                break
    print(f'\n{len(selected)} steps selected; {len(skipped_needs)} skipped for missing inputs; {len(failed)} failed; '
          f'{(time.time() - t0) / 60:.1f} min')
    for name, missing in skipped_needs[:20]:
        print(f'  skipped {name} (needs {", ".join(missing)})')
    if failed:
        print('FAILED: ' + ', '.join(failed))
        sys.exit(1)
    if args.make_pack:
        # maintainers: the footprint of this traced run -> the state pack release file
        subprocess.run([sys.executable, str(ROOT / 'tools/statepack.py'), 'collect'], check=True)
        subprocess.run([sys.executable, str(ROOT / 'tools/statepack.py'), 'build', '--out', str(Path(args.make_pack).resolve())], check=True)
    if args.verify_against:
        subprocess.run([sys.executable, str(ROOT / 'tools/verify_assets.py'), str(ROOT), args.verify_against], check=False)


if __name__ == '__main__':
    main()
