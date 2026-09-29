#!/usr/bin/env python3
"""Conquer the Mountain flow captures (docs/ctm-parity.md): tools/ps2_navigate.py `run` of a pad-scripted derived savestate
that records, for every snapshot, the PNG and the CTM flow facts of the same savestate (tools/ctm_flow_state.py: world
state, course, NIS lists and the playing script, the career block of player 0) into OUTDIR/navigate.json, plus the world
state changes as they are polled. Optional live PINE pokes (derived runs only) and saves by sample, tick or world state.

  python3 tools/ps2_navigate.py build BASE.p2s SCRIPT.json HOOKED.p2s
  python3 tools/ctm_flow_capture.py HOOKED.p2s OUTDIR --frames N [--snap-every 20] [--save NAME@SAMPLE|NAME@tTICK|NAME@wsWS]
                 [--poke JSON]  (list of {"when": "sample>=N" | "ws==5" | "tick>=T", "addr": int, "value": int, "size": 1|4})
  MEMCARD=card.ps2 [MEMCARD_OUT=out.ps2]: insert a memory card in slot 1 (a missing file = ARMSX2 makes an unformatted one).
  RIDER_TRACE=1: OUTDIR/rider-trace.json, the human rider's position / rotation / velocity / +0xAC4 at every game tick polled (PINE).

Named states are cleaned (hook removed). The ladder of states and runs used by the parity doc: local/ps2-capture/ctm-parity/.
Original discs and savestates are never modified. One emulator at a time; it is stopped when the run ends.
"""
import argparse, json, random, shutil, struct, subprocess, sys, time, zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
sys.path.insert(0, str(HERE))
import ps2_navigate as N  # noqa: E402
import ps2_audio_log as AL  # noqa: E402
AL.CODE, AL.COUNT, AL.RING = 0xF4000, 0xF5000, 0xF6000   # relocated above the navigation arena
from ps2_capture import Pine, prepare_datapath, PCSX2, ISO, GP  # noqa: E402
from reference_replay import patch_state  # noqa: E402
from ctm_flow_state import flow_state, brief  # noqa: E402


def pine_write(pine, addr, value, size):
    op = {1: 4, 2: 5, 4: 6}[size]
    fmt = {1: '<B', 2: '<H', 4: '<I'}[size]
    pine.request(bytes([op]) + struct.pack('<I', addr) + struct.pack(fmt, value & ((1 << (8 * size)) - 1)))


def live(pine):
    u = lambda a: struct.unpack('<I', pine.read(a, 4))[0]
    ok = lambda p: 0x100000 <= p < 0x2000000
    G = u(0x4A28A8); tick = ws = None
    if ok(G):
        S = u(G + 0x84)
        if ok(S):
            ws = struct.unpack('<i', pine.read(S + 0x214, 4))[0]
            C = u(S + 0xC)
            if ok(C): tick = u(C + 8)
    return tick, ws


def rider_trace(pine):
    """RIDER_TRACE=1: the human rider (race C+0x28) as the world ticks: position +0x110, rotation +0x120, velocity +0x1E0, the NIS-driven
    flag +0xAC4 (123640 / 123B48) and the forward row +0x1B0; None when there is no race."""
    u = lambda a: struct.unpack('<I', pine.read(a, 4))[0]
    ok = lambda p: 0x100000 <= p < 0x2000000
    G = u(0x4A28A8); S = u(G + 0x84) if ok(G) else 0; C = u(S + 0xC) if ok(S) else 0
    R = u(C + 0x28) if ok(C) else 0
    if not ok(R): return None
    f = lambda a, n: [round(x, 3) for x in struct.unpack(f'<{n}f', pine.read(a, 4 * n))]
    return dict(pos=f(R + 0x110, 3), q=f(R + 0x120, 4), fwd=f(R + 0x1B0, 3), vel=f(R + 0x1E0, 3), ac4=u(R + 0xAC4))


def cond(expr, v):
    return bool(eval(expr, {}, v))


def run(state, outdir, frames, snap_every=0, saves=(), pokes=(), timeout=1800, keep=False, speed='normal', snap_from=0, coarse=0, snaps=''):
    state = Path(state).resolve(); outdir = Path(outdir); outdir.mkdir(parents=True, exist_ok=True)
    mpath = state.with_suffix('.nav.json')
    manifest = json.loads(mpath.read_text()) if mpath.exists() else {}
    requests = [(s['sample'], s['name']) for s in manifest.get('saves', [])]
    cond_saves = []
    for s in saves:
        name, at = s.split('@')
        if at.startswith('t'): cond_saves.append((f'tick is not None and {int(at[1:])} <= tick <= {int(at[1:]) + 5}', name))
        elif at.startswith('ws'): cond_saves.append((f'ws == {int(at[2:])}', name))
        else: requests.append((int(at), name))
    if snap_every: requests += [(k, None) for k in range(max(snap_every, snap_from), frames + 1, snap_every)]
    if coarse: requests += [(k, None) for k in range(coarse, snap_from, coarse)]
    for part in filter(None, snaps.split(',')):   # A-B:STEP
        ab, _, st = part.partition(':'); a, _, b = ab.partition('-')
        requests += [(k, None) for k in range(int(a), int(b) + 1, int(st or 1))]
    requests.append((frames, 'final'))
    requests = sorted(set(requests), key=lambda r: (r[0], r[1] or ''))
    slot = random.randint(28100, 28999)
    datapath = ROOT / f'local/ps2-capture/pcsx2-ctm-{slot}'
    prepare_datapath(datapath, slot)
    import os, re
    env = dict(os.environ)
    if os.environ.get('MEMCARD'):   # derived runs only: insert a memory card file (copied from MEMCARD, or created by ARMSX2)
        src = Path(os.environ['MEMCARD'])
        if src.exists(): shutil.copy(src, datapath / 'memorycards/Mcd001.ps2')
        for ini in (datapath / 'inis/PCSX2.ini', datapath / 'ARMSX2/inis/PCSX2.ini'):
            t = ini.read_text(); t = re.sub(r'(?m)^Slot1_Enable = .*$', 'Slot1_Enable = true', t);
            t = re.sub(r'(?m)^MemoryCards = .*$', f'MemoryCards = {datapath / "memorycards"}', t); ini.write_text(t)
    if os.environ.get('CAPTURE_AUDIO'):   # SDL3 disk audio driver: raw PCM of the SPU2 output mix
        for ini in (datapath / 'inis/PCSX2.ini', datapath / 'ARMSX2/inis/PCSX2.ini'):
            t = ini.read_text()
            for k, v in (('StandardVolume', '100'), ('Backend', 'SDL'), ('SyncMode', os.environ.get('AUDIO_SYNC', 'Disabled')), ('OutputMuted', 'false')):
                t = re.sub(rf'(?m)^{k} = .*$', f'{k} = {v}', t)
            ini.write_text(t)
        env.update(SDL_AUDIO_DRIVER='disk', SDL_AUDIO_DISK_OUTPUT_FILE=os.environ['CAPTURE_AUDIO'])
    args = [str(PCSX2), '-datapath', str(datapath), '-batch', '-nogui', '-statefile', str(state)]
    if speed == 'turbo': args.append('-turbo')
    args += ['--', str(ISO)]
    log = open(outdir / 'navigate.pcsx2.log', 'w')
    proc = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT, env=env); t_launch = time.time()
    started = time.time(); pine = None; pending = list(requests); inflight = []; samples = 0
    collected = []; rawdir = outdir / '.raw'; rawdir.mkdir(exist_ok=True); trace = []
    pokes = [dict(p, done=False) for p in pokes]; clock = []; alog = []; poke_log = []; ws_log = []; last_ws = None
    try:
        while time.time() - started < timeout:
            if proc.poll() is not None: raise RuntimeError(f'ARMSX2 exited early ({proc.returncode})')
            if pine is None:
                try: pine = Pine(slot)
                except OSError: time.sleep(0.5); continue
            try:
                samples, = struct.unpack('<I', pine.read(N.NAV_DATA + N.D_SAMPLES, 4))
                tick, ws = live(pine)
            except (OSError, RuntimeError, ConnectionError): time.sleep(0.2); continue
            v = dict(sample=samples, tick=tick, ws=ws)
            if os.environ.get('AUDIO_LOG'):
                try:
                    for r in AL.read_new(pine, len(alog)): r['sample'] = samples; alog.append(r)
                except (OSError, RuntimeError, ConnectionError): pass
            if not clock or samples != clock[-1][0]: clock.append((samples, round(time.time() - t_launch, 4), tick, ws))
            if os.environ.get('RIDER_TRACE') and tick is not None and (not trace or trace[-1]['tick'] != tick):
                try:
                    r = rider_trace(pine)
                    if r: trace.append(dict(sample=samples, tick=tick, ws=ws, **r))
                except (OSError, RuntimeError, ConnectionError, struct.error): pass
            if ws != last_ws: ws_log.append(dict(sample=samples, tick=tick, ws=ws)); last_ws = ws; print('ws', ws_log[-1], flush=True)
            for p in pokes:
                if (not p['done'] or p.get('repeat')) and cond(p['when'], v):
                    try:
                        pine_write(pine, p['addr'], p['value'], p.get('size', 4))
                        if not p['done']:
                            poke_log.append(dict(v, addr=hex(p['addr']), value=p['value'], size=p.get('size', 4)))
                            print('poke', poke_log[-1], flush=True)
                        p['done'] = True
                    except (OSError, RuntimeError, ConnectionError): pass
            for item in list(cond_saves):
                c, name = item
                if samples > 0 and cond(c, v): pending.insert(0, (samples, name)); cond_saves.remove(item)
            while pending and samples >= pending[0][0] and len(inflight) < 8:
                want, name = pending.pop(0)
                used = {k for k, *_ in inflight}
                k = next(n for n in range(1, 10) if n not in used)
                for old in datapath.rglob(f'*.{k:02d}.p2s*'): old.unlink()
                pine.request(b'\x09' + bytes([k])); inflight.append((k, want, name, samples, time.time()))
            for item in list(inflight):
                k, want, name, asked, t0 = item
                files = [f for f in datapath.rglob(f'*.{k:02d}.p2s') if f.is_file()]
                if not files:
                    if time.time() - t0 > 60: raise RuntimeError(f'Savestate slot {k} never appeared')
                    continue
                try:
                    with zipfile.ZipFile(files[0]) as z:
                        if not {'eeMemory.bin', 'Screenshot.png'} <= set(z.namelist()): continue
                except (zipfile.BadZipFile, OSError): continue
                raw = rawdir / f'{len(collected):05d}.p2s'; files[0].rename(raw)
                collected.append((want, name, raw)); inflight.remove(item)
            if not pending and not inflight: break
            time.sleep(0.01)
        else:
            raise RuntimeError(f'timed out at sample {samples}')
    finally:
        if pine: pine.close()
        t_term = time.time() - t_launch
        proc.terminate()
        try: proc.wait(10)
        except subprocess.TimeoutExpired: proc.kill()
        t_dead = time.time() - t_launch
        log.close()
        if os.environ.get('MEMCARD_OUT') and (datapath / 'memorycards/Mcd001.ps2').exists(): shutil.copy(datapath / 'memorycards/Mcd001.ps2', os.environ['MEMCARD_OUT'])
        shutil.rmtree(datapath, ignore_errors=True)
    results = []
    for want, name, raw in collected:
        with zipfile.ZipFile(raw) as z: memory = z.read('eeMemory.bin'); png = z.read('Screenshot.png')
        got = struct.unpack_from('<I', memory, N.NAV_DATA + N.D_SAMPLES)[0]
        label = name or f'sample{got:05d}'
        (outdir / f'{label}.png').write_bytes(png)
        try: info = flow_state(memory)
        except Exception as e: info = dict(error=str(e))
        entry = dict(requested=want, sample=got, tick=info.get('tick'), png=f'{label}.png', nis=info)
        if name or keep:
            target = outdir / f'{label}.p2s'
            patch_state(raw, target, N.clean_memory_patches(memory))
            entry['state'] = str(target)
        results.append(entry); raw.unlink()
        print(f'{label}: {brief(info)}', flush=True)
    shutil.rmtree(rawdir, ignore_errors=True)
    results.sort(key=lambda e: e['sample'])
    if trace: (outdir / 'rider-trace.json').write_text(json.dumps(trace, separators=(',', ':')) + '\n')
    summary = dict(audio_log=alog, clock=clock, t_term=t_term, t_dead=t_dead, t_exit=round(time.time() - t_launch, 3), state=str(state), manifest=manifest, frames=frames, pokes=poke_log, ws_log=ws_log, results=results)
    (outdir / 'navigate.json').write_text(json.dumps(summary, indent=1) + '\n')
    return summary


if __name__ == '__main__':
    p = argparse.ArgumentParser()
    p.add_argument('state'); p.add_argument('outdir'); p.add_argument('--frames', type=int, required=True)
    p.add_argument('--snap-every', type=int, default=0); p.add_argument('--save', action='append', default=[])
    p.add_argument('--poke', default=None, help='JSON file or inline JSON list')
    p.add_argument('--timeout', type=int, default=1800); p.add_argument('--keep', action='store_true')
    p.add_argument('--speed', default='normal'); p.add_argument('--snap-from', type=int, default=0); p.add_argument('--coarse', type=int, default=0); p.add_argument('--snaps', default='')
    a = p.parse_args()
    pk = []
    if a.poke: pk = json.loads(Path(a.poke).read_text() if Path(a.poke).exists() else a.poke)
    for x in pk:
        if isinstance(x.get('addr'), str): x['addr'] = int(x['addr'], 0)
        if isinstance(x.get('value'), str): x['value'] = int(x['value'], 0)
    s = run(a.state, a.outdir, a.frames, a.snap_every, a.save, pk, a.timeout, a.keep, a.speed, a.snap_from, a.coarse, a.snaps)
    print(json.dumps(dict(results=len(s['results']), pokes=s['pokes'])))
