#!/usr/bin/env python3
"""ps2_music_drive.py BASELINE NAME PLAN.json [--seconds S] [--speed normal]   (docs/audio-logic.md 9.11; development reference only)

ARMSX2 on a derived copy of BASELINE (ps2_menu_capture pad hook with the table at 0x9B880, plus the ps2_music_log.py hooks).
Plans used for the recordings: tools/ps2_music_plans/*.json; reduced timelines: tools/ps2_music_timeline.py.
Runs a plan: a list of steps executed in order, each waiting for its `when` (python expr over the live vars), then doing:
  {"press": ["Cross"], "hold": 6}      buttons for N pad samples (then neutral / the autopilot)
  {"stick": [lx, ly], "hold": N}       sticks for N samples (N = 0: keep)
  {"auto": "A:0#0,A:0#3,..."}          closed-loop autopilot along those AIP track paths (stream_drive steering); "auto": null stops it
  {"save": "label"}                    savestate (hooks included; this tool re-derives from such states)
  {"shot": "label"}                    screenshot only
  {"note": "text"}                     marker in the log
  {"stop": true}
A plan may also be {"rules": [{"when": expr, "seq": [{"press": [...], "wait": s}], "gap": s}], "steps": [...]}: reactive presses.
vars: sample (pad samples), tick, ws, course, kind, mode, song, latch, t (s since start), x y z, loc (rider +0x434), step (seconds
in this step), ss (pad samples in this step), stalled (seconds since the game tick last changed: menus / overlays).
Output (local/ps2-capture/music/runs/NAME/): events.jsonl (mlog hook records), poll.jsonl (director state changes), steps.jsonl, PNG/p2s saves.
Original savestates are never modified.
"""
import argparse, json, math, random, shutil, signal, struct, sys, time, zipfile
def _term(*_): raise KeyboardInterrupt
signal.signal(signal.SIGTERM, _term)
from pathlib import Path
HERE = Path(__file__).resolve().parent
ROOT = HERE.parent
MUSIC = ROOT / 'local/ps2-capture/music'
sys.path.insert(0, str(HERE))
import ps2_menu_capture as pmc
from ps2_capture import decode_pad, GP, BUTTONS
from reference_replay import patch_state
import ps2_music_log as mlog

CLEAN = ROOT / 'local/ps2-capture/menus/ctm/state-select-character-zoe.p2s'   # unhooked code bytes for the hook sites
pmc.SCRIPT = 0x9B880
STEER = pmc.SCRIPT + 32

_orig_derive = pmc.derive
def derive(baseline, events, output):
    """pmc derive (pad hook) from a baseline that may carry this tool's hooks, then add the mlog hooks"""
    clean_mem = zipfile.ZipFile(CLEAN).read('eeMemory.bin')
    tmp = output.with_name(output.name + '.pre.p2s')
    with zipfile.ZipFile(baseline) as z: mem = z.read('eeMemory.bin')
    fix = mlog.clean_patches(mem, clean_mem)
    if fix: patch_state(Path(baseline), tmp, fix); src = tmp
    else: src = Path(baseline)
    end = _orig_derive(src, events, output)
    with zipfile.ZipFile(output) as z: mem = z.read('eeMemory.bin')
    tmp2 = output.with_name(output.name + '.m.p2s')
    patch_state(output, tmp2, mlog.patches(mem)); shutil.move(tmp2, output)
    if tmp.exists(): tmp.unlink()
    return end
pmc.derive = derive

ap = argparse.ArgumentParser()
ap.add_argument('baseline'); ap.add_argument('name'); ap.add_argument('plan')
ap.add_argument('--seconds', type=float, default=1800); ap.add_argument('--speed', default='normal')
ap.add_argument('--stall', type=float, default=90); ap.add_argument('--gain', type=float, default=1.6); ap.add_argument('--look', type=float, default=2500.0)
a = ap.parse_args()
plan = json.loads(Path(a.plan).read_text()) if Path(a.plan).exists() else json.loads(a.plan)
rules = []
if isinstance(plan, dict): rules = plan.get('rules', []); plan = plan['steps']
rule_q = []; tick_seen = [None, time.time()]
OUT = MUSIC / 'runs' / a.name; OUT.mkdir(parents=True, exist_ok=True)
pmc.OUT = OUT

# ---- autopilot paths ----
paths = []
ALL_PATHS = MUSIC / 'aip_paths.json'
def all_paths():
    if ALL_PATHS.exists(): return json.loads(ALL_PATHS.read_text())
    from world_assets import world_chunks, records, locations
    from race_event_assets import decode_aip
    names = [l['name'] for l in locations(ROOT / 'local/assets/source/ps2/bam.sdb')]
    out = []
    for chunk in world_chunks(ROOT / 'local/assets/source/ps2/bam.ssb'):
        for kind, t, rid, data in records(chunk):
            if kind == 14 and len(data) > 8:
                for p in decode_aip(data)['track_paths']:
                    nm = f"{names[t]}:{rid}#{p['index']}"
                    x, y, z = p['position']; pts = [(x, y, z)]
                    for dx, dy, dz, l in p['segments']: x += dx * l; y += dy * l; z += dz * l; pts.append((x, y, z))
                    out.append(dict(name=nm, pts=pts))
    ALL_PATHS.write_text(json.dumps(out)); return out
_ALL = all_paths()
def load_paths(spec):
    want = spec.split(','); out = []
    for w in want:
        for p in _ALL:
            nm = p['name']
            if (nm == w or (w.endswith(':*') and nm.startswith(w[:-1]))) and p not in out: out.append(p)
    return out

def nearest(pos):
    best = None
    for pi, p in enumerate(paths):
        pts = p['pts']
        for k in range(len(pts) - 1):
            ax, ay, az = pts[k]; bx, by, bz = pts[k + 1]
            vx, vy, vz = bx - ax, by - ay, bz - az; L2 = vx * vx + vy * vy + vz * vz or 1
            u = max(0, min(1, ((pos[0] - ax) * vx + (pos[1] - ay) * vy + (pos[2] - az) * vz) / L2))
            q = (ax + u * vx, ay + u * vy, az + u * vz)
            d = sum((c - e) ** 2 for c, e in zip(pos, q))
            if best is None or d < best[0]: best = (d, pi, k, u)
    return best

def ahead(pi, k, u, dist):
    p = paths[pi]['pts']; x0, y0, z0 = p[k]; x1, y1, z1 = p[k + 1]
    cur = (x0 + u * (x1 - x0), y0 + u * (y1 - y0), z0 + u * (z1 - z0)); seen = set()
    while True:
        nxt = paths[pi]['pts'][k + 1]; seg = math.dist(cur, nxt)
        if seg >= dist:
            f = dist / seg if seg else 0
            return tuple(c + f * (n - c) for c, n in zip(cur, nxt))
        dist -= seg; cur = nxt; k += 1
        if k + 1 >= len(paths[pi]['pts']):
            seen.add(pi)
            cand = [(math.dist(cur, q['pts'][0]), j) for j, q in enumerate(paths) if j != pi and j not in seen and q['pts'][0][2] <= cur[2] + 500]
            if not cand: return cur
            dd, pi = min(cand)
            if dd > 20000: return cur
            k = 0; cur = paths[pi]['pts'][0]

emu = pmc.Emulator(a.baseline, [], 'emu', a.speed)
pine = emu.pine
u32 = lambda addr: struct.unpack('<I', pine.read(addr, 4))[0]
ok = lambda p: 0x100000 <= p < 0x2000000

def pad(names=(), lx=0.0, ly=0.0):
    data = struct.pack('<24f', *decode_pad(tuple(sorted(pmc.button(n) for n in names)), lx, ly, 0, 0))
    pine.request(b''.join(b'\x07' + struct.pack('<I', STEER + k) + data[k:k + 8] for k in range(0, 96, 8)))

def live():
    v = dict(sample=None, tick=None, ws=None, x=None, y=None, z=None, loc=None, vel=None)
    v['sample'] = struct.unpack('<I', pine.read(pmc.DATA + 4, 4))[0]
    c = pine.read(0x535C08, 12); v['course'] = c[0]; v['kind'] = c[8]; v['path'] = c[9]; v['mode'] = c[10]
    G = u32(GP - 0x848)
    if ok(G):
        S = u32(G + 0x84)
        if ok(S):
            v['ws'] = struct.unpack('<i', pine.read(S + 0x214, 4))[0]
            C = u32(S + 0xC)
            if ok(C):
                v['tick'] = u32(C + 8)
                r0 = u32(C + 0x40)
                if ok(r0):
                    r = u32(r0 + 0x18)
                    if ok(r):
                        v['x'], v['y'], v['z'] = struct.unpack('<3f', pine.read(r + 0x110, 12))
                        v['loc'] = struct.unpack('<i', pine.read(r + 0x434, 4))[0]
                        v['vel'] = struct.unpack('<3f', pine.read(r + 0x1E0, 12))
    return v

ev = open(OUT / 'events.jsonl', 'w'); pl = open(OUT / 'poll.jsonl', 'w'); sl = open(OUT / 'steps.jsonl', 'w')
last_sample = [None, time.time()]; have = 0; last_poll = None; last_poll_t = 0; t0 = time.time(); step_i = 0; step_t0 = time.time(); step_s0 = None
auto = False; press_until = None; last_steer = 0; cur_stick = (0.0, 0.0)
def logstep(d, v): d.update(sample=v.get('sample'), tick=v.get('tick'), ws=v.get('ws'), t=round(time.time() - t0, 2)); sl.write(json.dumps(d) + '\n'); sl.flush(); print('step', json.dumps(d), flush=True)
try:
    while time.time() - t0 < a.seconds:
        if emu.proc.poll() is not None: print('emulator exited', flush=True); break
        try:
            for r in mlog.read_new(pine, have):
                have += 1; ev.write(json.dumps(r) + '\n'); ev.flush()
            v = live()
        except (OSError, RuntimeError, ConnectionError, struct.error) as e:
            if 'overrun' in str(e): print('OVERRUN', e, flush=True); raise
            time.sleep(0.05); continue
        now = time.time(); v['t'] = now - t0; v['step'] = now - step_t0
        if v['sample'] != last_sample[0]: last_sample[:] = [v['sample'], now]
        elif now - last_sample[1] > a.stall: print('STALL at sample', v['sample'], flush=True); break
        if now - last_poll_t > 0.03:
            last_poll_t = now
            try:
                s = mlog.summarize(mlog.poll(pine))
                if s is not None:
                    key = json.dumps(s, sort_keys=True)
                    if key != last_poll:
                        last_poll = key; pl.write(json.dumps(dict(sample=v['sample'], tick=v['tick'], ws=v['ws'], loc=v['loc'], t=round(v['t'], 2), **s)) + '\n'); pl.flush()
                    v['song'] = s['song']; v['latch'] = s['latch']
            except (OSError, RuntimeError, ConnectionError, struct.error): pass
        v.setdefault('song', None); v.setdefault('latch', None)
        if v.get('tick') != tick_seen[0]: tick_seen[:] = [v.get('tick'), now]
        v['stalled'] = now - tick_seen[1]
        if not rule_q:
            for r in rules:
                try: fire = eval(r['when'], {'math': math}, v)
                except Exception: fire = False
                if fire and now - r.get('_last', 0) > r.get('gap', 5):
                    r['_last'] = now; rule_q.extend([dict(p) for p in r['seq']]); logstep(dict(rule=r['when']), v); break
        if rule_q and press_until is None and now - rule_q[0].get('_t', 0) >= 0:
            p = rule_q.pop(0)
            pad(p['press'], *cur_stick); press_until = v['sample'] + p.get('hold', 6)
            if rule_q: rule_q[0]['_t'] = now + p.get('wait', 0.6)
        # button hold release
        if press_until is not None and v['sample'] >= press_until:
            press_until = None; pad((), *cur_stick)
        # plan
        while step_i < len(plan) and press_until is None:
            st = plan[step_i]
            v['step'] = time.time() - step_t0; v['ss'] = v['sample'] - (step_s0 if step_s0 is not None else v['sample'])
            try: go = eval(st.get('when', 'True'), {'math': math}, v)
            except Exception: go = False
            if not go: break
            step_i += 1; step_t0 = time.time(); step_s0 = v['sample']
            if 'press' in st:
                pad(st['press'] if isinstance(st['press'], list) else [st['press']], *cur_stick); press_until = v['sample'] + st.get('hold', 6)
                logstep(dict(press=st['press']), v)
            elif 'stick' in st:
                cur_stick = tuple(st['stick']); pad((), *cur_stick); logstep(dict(stick=cur_stick), v)
                if st.get('hold'): press_until = v['sample'] + st['hold']; cur_stick = (0.0, 0.0)
            elif 'auto' in st:
                if st['auto']: paths = load_paths(st['auto']); auto = True; logstep(dict(auto=[p['name'] for p in paths]), v)
                else: auto = False; cur_stick = (0.0, 0.0); pad(); logstep(dict(auto=None), v)
            elif 'shot' in st:
                e = emu.save(st['shot'], False); logstep(dict(shot=st['shot']), v)
            elif 'save' in st:
                e = emu.save(st['save'], True); logstep(dict(save=st['save'], state=e.get('state')), v)
            elif 'note' in st: logstep(dict(note=st['note'], x=v['x'], y=v['y'], z=v['z'], loc=v['loc'], course=v['course']), v)
            elif 'stop' in st: logstep(dict(stop=True), v); raise KeyboardInterrupt
        if step_i >= len(plan) and not auto: pass
        # autopilot
        if auto and press_until is None and now - last_steer > 0.03 and v['x'] is not None and v['vel'] is not None:
            last_steer = now
            pos = (v['x'], v['y'], v['z']); vel = v['vel']
            b = nearest(pos)
            if b:
                tgt = ahead(b[1], b[2], b[3], a.look)
                h = math.atan2(vel[1], vel[0]); tt = math.atan2(tgt[1] - pos[1], tgt[0] - pos[0])
                err = (tt - h + math.pi) % (2 * math.pi) - math.pi
                lx = max(-1, min(1, -a.gain * err))
                if math.hypot(vel[0], vel[1]) < 200: lx = max(-1, min(1, -2 * err))
                cur_stick = (round(lx, 3), 1.0); pad((), *cur_stick)
        if step_i >= len(plan) and not auto and press_until is None and plan and 'stop' not in plan[-1]:
            pass
        time.sleep(0.004)
except KeyboardInterrupt: pass
finally:
    try:
        for r in mlog.read_new(pine, have): ev.write(json.dumps(r) + '\n')
    except Exception as e: print('final read failed', e)
    ev.close(); pl.close(); sl.close()
    try: emu.save('end', False)
    except Exception as ex: print('end shot failed', ex)
    emu.proc.terminate()
    try: emu.proc.wait(10)
    except Exception: emu.proc.kill()
    shutil.rmtree(emu.datapath, ignore_errors=True)
    for f in [emu.derived] + list(emu.derived.parent.glob(emu.derived.name + '*.patches.json')):
        try: f.unlink()
        except Exception: pass
print('done', a.name, 'events', have, 'seconds', round(time.time() - t0))
