#!/usr/bin/env python3
"""Poll EE words over PINE while a savestate runs (development reference only); print every change with the game tick 1298C8.
poll.py STATE SECONDS NAME=ADDR[:+OFF...] ...   ADDR may be 'rm' (0x5B0700), 'S' (the world), 'ws10' (S+0x158) + offset.
Optional --hook ADDR installs ra_probe's entry logger too (read at the end)."""
import sys, time, struct, random, shutil, subprocess, json
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import ps2_capture as pc


def main():
    state, seconds = sys.argv[1], float(sys.argv[2]); specs = []
    for s in sys.argv[3:]:
        name, expr = s.split('='); specs.append((name, expr))
    slot = random.randint(28100, 28999); datapath = pc.DATAPATH.parent / f'pcsx2-{slot}'
    pc.prepare_datapath(datapath, slot)
    cmd = [str(pc.PCSX2), '-datapath', str(datapath), '-batch', '-nogui', '-statefile', str(Path(state).resolve()), '--', str(pc.ISO)]
    log = open(Path(datapath) / 'poll.log', 'w'); proc = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)
    pine = None; started = None; last = None; out = []
    try:
        while True:
            if proc.poll() is not None: raise RuntimeError('emulator exited')
            if pine is None:
                try: pine = pc.Pine(slot); started = time.time()
                except OSError: time.sleep(0.3); continue
            if time.time() - started > seconds: break
            try:
                rd = lambda a: struct.unpack('<I', pine.read(a, 4))[0]
                g = rd(pc.GP - 0x848); S = rd(g + 0x84); rm = rd(S + 0xC); t = rd(rm + 8)
                base = dict(rm=rm, S=S, ws10=S + 0x158, g=g)
                vals = []
                for name, expr in specs:
                    parts = expr.split('+'); a = base.get(parts[0]) if parts[0] in base else int(parts[0], 0)
                    for off in parts[1:]:
                        if off.startswith('*'): a = rd(a + int(off[1:], 0))
                        else: a += int(off, 0)
                    vals.append(rd(a))
            except (OSError, RuntimeError, ConnectionError): time.sleep(0.05); continue
            key = tuple(vals)
            if key != last:
                out.append([t] + vals); print(t, ' '.join('%s=%x' % (n, v) for (n, _), v in zip(specs, vals)), flush=True); last = key
            time.sleep(0.003)
    finally:
        if pine: pine.close()
        proc.terminate()
        try: proc.wait(10)
        except subprocess.TimeoutExpired: proc.kill()
        log.close(); shutil.rmtree(datapath, ignore_errors=True)


if __name__ == '__main__':
    main()
