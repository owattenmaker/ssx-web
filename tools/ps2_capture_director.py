#!/usr/bin/env python3
"""Camera-director capture on top of tools/ps2_capture.py (development reference only).

`build` derives the usual pad/provider capture state (tools/ps2_capture.py build) and adds:

* a second guarded hook at 0x15DFC8, the epilogue of the outer-camera update 0x15DF98 (director
  update 0x161BB8 then compositor 0x15E668), logging one 4 KiB record per camera update of the
  human outer camera into its own ring (0xE2000..0xF2000, control block 0xE1000):
  tick, director +0..+0x50, list header, the first three nodes (0x20 bytes each), the first two
  algorithm objects (+0..+0x3B0), outer camera +0..+0x480, rider +0x470..+0x480 (finish marker),
  game-info +0..+0x20 (race phase / ticks) and level manager +0 / +0x484;
* optionally (--finish-ahead CM) a runtime finish event (type 1) on the human's current course
  path, CM ahead of the rider, so the original rider finish routine 0x125108 -> 0x162258 runs
  through the normal course-event path (0x10E5D8) a moment later.

`run` streams both rings over PINE. Original discs and reference savestates are never modified.
"""
import argparse, hashlib, json, os, shutil, struct, subprocess, sys, time, zipfile, random
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ps2_capture as base  # noqa: E402
from reference_replay import patch_state  # noqa: E402
from reference_race_event import extract_race_event  # noqa: E402

CODE = 0xE0000
DATA = 0xE1000
RING = 0xE2000
RECORD = 4096
CAPACITY = 16
HOOK = 0x15DFC8
HOOK_BYTES = struct.pack('<2I', 0x7BB00010, 0xDFBF0000)   # lq s0,0x10(sp); ld ra,0(sp)
EVENT = 0xF8000
F_ENABLED, F_WRITES = 0, 4
LAYOUT = dict(seq=0, tick=4, director_ptr=8, list_ptr=12, director_00_50=16, list_00_08=96, node_ptrs=104, alg_ptrs=116,
              nodes_3x32=128, alg0_000_3b0=224, alg1_000_3b0=1168, outer_000_480=2112, rider_470_480=3264,
              game_00_20=3280, manager_0=3312, manager_484=3316)
T0, T1, T2, T3, T4, T5, T6, T7, T8, T9 = 8, 9, 10, 11, 12, 13, 14, 15, 24, 25
A0, A1, A2, A3, V0, V1, S0, SP, GPR, ZERO = 4, 5, 6, 7, 2, 3, 16, 29, 28, 0


def assemble():
    a = base.Asm(CODE)
    a.addiu(SP, SP, -0x100)
    regs = [T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, V0, V1, A0, A1, A2, A3]
    for n, reg in enumerate(regs): a.sq(reg, n * 16, SP)
    a.li(T0, DATA)
    a.lw(T1, F_ENABLED, T0); a.beq(T1, ZERO, 'done'); a.nop()
    a.li(T1, base.OUTER); a.bne(S0, T1, 'done'); a.nop()
    a.lw(T2, F_WRITES, T0); a.andi(T3, T2, CAPACITY - 1); a.sll(T3, T3, 12)
    a.li(T4, RING); a.addu(T3, T3, T4)
    base.tick_into(a, T5); a.sw(T5, LAYOUT['tick'], T3)
    n = [0]

    def copy(src, dst, words):
        name = f'copy{n[0]}'; n[0] += 1
        a.beq(src, ZERO, name + '_skip'); a.nop()
        a.addiu(T6, src, 0); a.addiu(T7, T3, dst); a.addiu(T8, ZERO, words)
        a.label(name)
        a.lw(T5, 0, T6); a.sw(T5, 0, T7); a.addiu(T6, T6, 4); a.addiu(T7, T7, 4); a.addiu(T8, T8, -1)
        a.bne(T8, ZERO, name); a.nop()
        a.label(name + '_skip')

    def load(dst, src, offset, store_at):
        name = f'load{n[0]}'; n[0] += 1
        a.addiu(dst, ZERO, 0)
        a.beq(src, ZERO, name); a.nop()
        a.lw(dst, offset, src)
        a.label(name)
        a.sw(dst, store_at, T3)
    load(A0, S0, 0xA8, LAYOUT['director_ptr'])                 # director
    load(A1, A0, 0x08, LAYOUT['list_ptr'])                     # node list
    load(V0, A1, 0x00, LAYOUT['node_ptrs'])                    # head node
    load(V1, V0, 0x14, LAYOUT['node_ptrs'] + 4)                # second node
    load(T9, V1, 0x14, LAYOUT['node_ptrs'] + 8)                # third node
    copy(A0, LAYOUT['director_00_50'], 20)
    copy(A1, LAYOUT['list_00_08'], 2)
    copy(V0, LAYOUT['nodes_3x32'], 8); copy(V1, LAYOUT['nodes_3x32'] + 32, 8); copy(T9, LAYOUT['nodes_3x32'] + 64, 8)
    load(A2, V0, 0, LAYOUT['alg_ptrs']); load(A3, V1, 0, LAYOUT['alg_ptrs'] + 4); load(T1, T9, 0, LAYOUT['alg_ptrs'] + 8)
    copy(A2, LAYOUT['alg0_000_3b0'], 236); copy(A3, LAYOUT['alg1_000_3b0'], 236)
    copy(S0, LAYOUT['outer_000_480'], 288)
    a.li(A2, base.RIDER + 0x470); copy(A2, LAYOUT['rider_470_480'], 4)
    a.lw(A2, -0x848, GPR); a.lw(A2, 0x84, A2); a.lw(A3, 0x28, A2); a.lw(A2, 0x0C, A2)
    copy(A2, LAYOUT['game_00_20'], 8)
    a.lw(T1, 0, A3); a.sw(T1, LAYOUT['manager_0'], T3); a.lw(T1, 0x484, A3); a.sw(T1, LAYOUT['manager_484'], T3)
    a.addiu(T2, T2, 1); a.sw(T2, 0, T3); a.sw(T2, F_WRITES, T0)
    a.label('done')
    for n_, reg in enumerate(regs): a.lq(reg, n_ * 16, SP)
    a.addiu(SP, SP, 0x100)
    a.emit(0x7BB00010); a.emit(0xDFBF0000); a.emit(0x03E00008); a.emit(0x27BD0020)   # relocated epilogue
    return a.link()


def build(baseline, script, output, isolate=False, finish_ahead=None):
    output = Path(output)
    intermediate = output.with_name(output.stem + '.base.p2s')
    manifest = base.build(baseline, script, intermediate, isolate)
    with zipfile.ZipFile(intermediate) as archive: memory = archive.read('eeMemory.bin')
    u = lambda at: struct.unpack_from('<I', memory, at)[0]
    if memory[HOOK:HOOK + 8] != HOOK_BYTES: raise ValueError('Camera update epilogue differs')
    if u(base.OUTER + 0xA8) != u(base.OUTER + 0xA0) or u(u(base.OUTER + 0xA8) + 0x14) != 0x45B908: raise ValueError('Director is not the active camera controller')
    if any(memory[CODE:RING + RECORD * CAPACITY]) or any(memory[EVENT:EVENT + 16]): raise ValueError('Director probe arena is not free')
    code = assemble()
    if len(code) > DATA - CODE: raise ValueError('Director probe code too large')
    data = struct.pack('<2I', 1, 0)
    j = struct.pack('<2I', (2 << 26) | (CODE >> 2), 0)
    patches = [dict(address=hex(CODE), expected='00' * len(code), replacement=code.hex()),
               dict(address=hex(DATA), expected='00' * len(data), replacement=data.hex()),
               dict(address=hex(HOOK), expected=HOOK_BYTES.hex(), replacement=j.hex())]
    finish = None
    if finish_ahead is not None:
        event = extract_race_event(memory)
        human = next(p for p in event['participants'] if p['human'])
        path = event['paths'][human['path_index']]
        at = path['remaining_at_origin'] - human['remaining'] + float(finish_ahead)
        record = int(event['provenance']['path_base'], 16) + 60 * human['path_index']
        count, pointer = struct.unpack_from('<iI', memory, record)
        if count != 0: raise ValueError('Human path already has course events')
        patches.append(dict(address=hex(EVENT), expected='00' * 16, replacement=struct.pack('<2I2f', 1, 0, at, at).hex(),
                            reason='Runtime course event 1 (finish, 0x10E5D8 -> 0x125108) ahead of the human rider'))
        patches.append(dict(address=hex(record), expected=struct.pack('<iI', count, pointer).hex(), replacement=struct.pack('<iI', 1, EVENT).hex(),
                            reason='Point the human path event list at the finish event'))
        finish = dict(path_index=human['path_index'], distance=at, rider_distance=at - float(finish_ahead), path_record=hex(record))
    patch_state(intermediate, output, patches)
    manifest.update(director=dict(code=hex(CODE), data=hex(DATA), ring=hex(RING), record=RECORD, capacity=CAPACITY, hook=hex(HOOK),
                                  layout=LAYOUT, finish_event=finish))
    output.with_suffix('.capture.json').write_text(json.dumps(manifest, indent=2) + '\n')
    intermediate.unlink(); intermediate.with_suffix('.capture.json').unlink(missing_ok=True); intermediate.with_suffix('.patches.json').unlink(missing_ok=True)
    return manifest


class Overrun(Exception): pass


def run(state, output, frames, timeout=600):
    slot = random.randint(28100, 28999)
    datapath = base.DATAPATH.parent / f'pcsx2-{slot}'
    base.prepare_datapath(datapath, slot)
    args = [str(base.PCSX2), '-datapath', str(datapath), '-batch', '-nogui', '-statefile', str(Path(state).resolve()), '--', str(base.ISO)]
    log = open(Path(output).with_suffix('.pcsx2.log'), 'w')
    proc = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT)
    rings = {'main': (base.DATA + base.F_WRITES, base.RING, base.RECORD, base.CAPACITY, {}),
             'camera': (DATA + F_WRITES, RING, RECORD, CAPACITY, {})}
    started = time.time(); pine = None
    try:
        while time.time() - started < timeout:
            if proc.poll() is not None: raise RuntimeError(f'PCSX2 exited early ({proc.returncode})')
            if pine is None:
                try: pine = base.Pine(slot)
                except OSError: time.sleep(0.5); continue
            try:
                for name, (writes_at, ring, size, capacity, records) in rings.items():
                    writes, = struct.unpack('<I', pine.read(writes_at, 4))
                    have = max(records) if records else 0
                    if writes - have > capacity: raise Overrun(f'{name} ring overrun: {writes} written, {have} read')
                    for seq in range(have + 1, writes + 1):
                        data = pine.read(ring + ((seq - 1) % capacity) * size, size)
                        if struct.unpack_from('<I', data, 0)[0] != seq: break
                        records[seq] = data
            except Overrun: raise
            except (OSError, RuntimeError, ConnectionError):   # PINE rejects reads until the VM runs
                time.sleep(0.2); continue
            if rings['camera'][4] and max(rings['camera'][4]) >= frames: break
            time.sleep(0.01)
        else:
            raise RuntimeError('Capture timed out')
    finally:
        if pine: pine.close()
        proc.terminate()
        try: proc.wait(10)
        except subprocess.TimeoutExpired: proc.kill()
        log.close()
        shutil.rmtree(datapath, ignore_errors=True)
    out = Path(output)
    main = [rings['main'][4][k] for k in sorted(rings['main'][4])]
    camera = [rings['camera'][4][k] for k in sorted(rings['camera'][4])]
    out.write_bytes(b''.join(main)); out.with_suffix('.camera.bin').write_bytes(b''.join(camera))
    summary = dict(state=str(state), records=len(main), camera_records=len(camera),
                   first_tick=struct.unpack_from('<I', main[0], 4)[0] if main else None,
                   first_camera_tick=struct.unpack_from('<I', camera[0], 4)[0] if camera else None,
                   last_camera_tick=struct.unpack_from('<I', camera[-1], 4)[0] if camera else None,
                   sha256=hashlib.sha256(b''.join(main)).hexdigest(), camera_sha256=hashlib.sha256(b''.join(camera)).hexdigest())
    out.with_suffix('.json').write_text(json.dumps(summary, indent=2) + '\n')
    return summary


def main():
    p = argparse.ArgumentParser(description=__doc__); sub = p.add_subparsers(dest='cmd', required=True)
    b = sub.add_parser('build'); b.add_argument('baseline'); b.add_argument('script'); b.add_argument('output')
    b.add_argument('--isolate', action='store_true'); b.add_argument('--finish-ahead', type=float)
    r = sub.add_parser('run'); r.add_argument('state'); r.add_argument('output'); r.add_argument('--frames', type=int, required=True)
    r.add_argument('--timeout', type=int, default=600)
    t = sub.add_parser('trace'); t.add_argument('baseline'); t.add_argument('output'); t.add_argument('--until-tick', type=int, default=24)
    args = p.parse_args()
    if args.cmd == 'trace':
        for row in trace(args.baseline, args.output, args.until_tick): print(row)
        return
    if args.cmd == 'build': print(json.dumps(build(args.baseline, args.script, args.output, args.isolate, args.finish_ahead)['director'], indent=2))
    else: print(json.dumps(run(args.state, args.output, args.frames, args.timeout), indent=2))



# ---- set-target trace (development): which callers re-seat the chase algorithm around race start ----
TRACE_CODE, TRACE_DATA, TRACE_RING = 0xF3000, 0xF3F00, 0xF4000
TRACE_SITES = {0x176FE0: 'DEFAULT_3 set-target 0x176FE0', 0x166F28: 'vtable-0x20 re-target 0x166F28',
               0x15D078: 'factory 0x15D078', 0x15DB58: 'restart 0x15DB58', 0x161EF0: 'select 0x161EF0', 0x162060: 'request 0x162060'}


def trace_patches(memory):
    a = base.Asm(TRACE_CODE); entries = {}
    for n, (site, _) in enumerate(TRACE_SITES.items()):
        first = memory[site:site + 8]
        w0, w1 = struct.unpack('<2I', first)
        if w0 >> 16 != 0x27BD or (w1 >> 26) in (1, 2, 3, 4, 5, 6, 7, 20, 21, 22, 23): raise ValueError(f'Unexpected prologue at {site:#x}')
        entries[site] = (a.here(), first)
        a.li(T0, TRACE_DATA); a.lw(T1, 0, T0); a.andi(T2, T1, 255); a.sll(T2, T2, 5); a.li(T3, TRACE_RING); a.addu(T2, T2, T3)
        base.tick_into(a, T4); a.sw(T4, 0, T2); a.li(T5, site); a.sw(T5, 4, T2); a.sw(A0, 8, T2); a.sw(A1, 12, T2); a.sw(31, 16, T2)
        a.lw(T4, -0x848, GPR); a.lw(T4, 0x84, T4); a.lw(T4, 0x0C, T4); a.lw(T4, 0, T4); a.sw(T4, 20, T2)   # race phase
        a.addiu(T1, T1, 1); a.sw(T1, 0, T0)
        a.emit(w0); a.emit(w1); a.j(site + 8); a.nop()
    code = a.link()
    patches = [dict(address=hex(TRACE_CODE), expected='00' * len(code), replacement=code.hex())]
    for site, (entry, first) in entries.items():
        patches.append(dict(address=hex(site), expected=first.hex(), replacement=struct.pack('<2I', (2 << 26) | (entry >> 2), 0).hex()))
    return patches


def trace(baseline, output, until_tick=24, timeout=300):
    baseline, output = Path(baseline), Path(output)
    with zipfile.ZipFile(baseline) as archive: memory = archive.read('eeMemory.bin')
    if any(memory[TRACE_CODE:TRACE_RING + 0x2000]): raise ValueError('Trace arena is not free')
    patch_state(baseline, output, trace_patches(memory))
    u = lambda at: struct.unpack_from('<I', memory, at)[0]
    game = u(u(u(base.GP - 0x848) + 0x84) + 0x0C)
    slot = random.randint(28100, 28999); datapath = base.DATAPATH.parent / f'pcsx2-{slot}'; base.prepare_datapath(datapath, slot)
    proc = subprocess.Popen([str(base.PCSX2), '-datapath', str(datapath), '-batch', '-nogui', '-statefile', str(output.resolve()), '--', str(base.ISO)],
                            stdout=open(output.with_suffix('.pcsx2.log'), 'w'), stderr=subprocess.STDOUT)
    events, seen, started, pine = [], 0, time.time(), None
    try:
        while time.time() - started < timeout:
            if proc.poll() is not None: raise RuntimeError('PCSX2 exited early')
            if pine is None:
                try: pine = base.Pine(slot)
                except OSError: time.sleep(0.5); continue
            try:
                count, = struct.unpack('<I', pine.read(TRACE_DATA, 4))
                if count - seen > 256: raise Overrun('trace ring overrun')
                for k in range(seen, count):
                    events.append(struct.unpack('<6I', pine.read(TRACE_RING + (k % 256) * 32, 24)))
                seen = count
                phase, tick = struct.unpack('<iI', pine.read(game, 4) + pine.read(game + 8, 4))
            except Overrun: raise
            except (OSError, RuntimeError, ConnectionError): time.sleep(0.2); continue
            if phase == 4 and tick >= until_tick: break
            time.sleep(0.01)
    finally:
        if pine: pine.close()
        proc.terminate()
        try: proc.wait(10)
        except subprocess.TimeoutExpired: proc.kill()
        shutil.rmtree(datapath, ignore_errors=True)
    rows = [dict(tick=t, phase=ph, site=TRACE_SITES[s], a0=hex(a0), a1=hex(a1), ra=hex(ra)) for t, s, a0, a1, ra, ph in events]
    output.with_suffix('.trace.json').write_text(json.dumps(rows, indent=1) + '\n')
    return rows


if __name__ == '__main__':
    main()
