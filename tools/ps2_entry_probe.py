#!/usr/bin/env python3
"""Entry probe (development reference only): log (ra, a0, a1, sp, game tick 1298C8, world state, 48 stack words and up to
16 fixed EE words, MEMWIN=addr,addr,...) at the entry of one or more functions of a running savestate.

ps2_entry_probe.py STATE OUT.json --hook 0x1297C8 [--hook ...] [--min-tick N] [--seconds 20]
Each hook relocates the function's first two instructions into a stub at 0xF0000 (they must not branch); the log at 0xF4000
holds 48 entries. Used for docs/peak3.md "Past the crash contacts" 5, 8 (1297C8's caller, 11FEC8 at a scenery landing).
Derives a state (scratch) with the probe stubs, runs ARMSX2 headless (muted by ps2_capture.prepare_datapath),
polls the log over PINE, writes OUT.json. The original state is never modified.
"""
import argparse, json, os, random, shutil, struct, subprocess, sys, time, zipfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import ps2_capture as pc

STUB = 0xF0000
LOG = 0xF4000           # u32 count, then entries
ENTRY = 0x140
MAXN = 48
SAVE = 0xF3F00          # temporaries
MEMWIN = [int(x, 0) for x in os.environ.get('MEMWIN', '').split(',') if x]


def build_stub(hooks, orig):
    a = pc.Asm(STUB); T0, T1, T2, T3 = pc.T0, pc.T1, pc.T2, pc.T3
    for k, h in enumerate(hooks):
        a.label(f'h{k}')
        a.li(T0, SAVE); a.sq(T1, 0, T0); a.sq(T2, 0x10, T0); a.sq(T3, 0x20, T0)
        # game tick >= MIN_TICK (LOG+4) only
        pc.tick_into(a, T3); a.li(T0, LOG); a.lw(T2, 4, T0); a.sltu(T2, T3, T2); a.bne(T2, pc.ZERO, f'd{k}'); a.nop()
        a.lw(T1, 0, T0)
        a.sltiu(T2, T1, MAXN); a.beq(T2, pc.ZERO, f'd{k}'); a.nop()
        a.addiu(T2, T1, 1); a.sw(T2, 0, T0)
        # entry = LOG + 0x10 + n * ENTRY  (ENTRY = 0x140 = 0x100 + 0x40)
        a.sll(T2, T1, 8); a.sll(T3, T1, 6); a.addu(T2, T2, T3); a.addu(T2, T2, T0); a.addiu(T2, T2, 0x10)
        a.li(T3, h); a.sw(T3, 0, T2); a.sw(31, 4, T2); a.sw(pc.A0, 8, T2); a.sw(pc.A1, 12, T2); a.sw(pc.SP, 16, T2)
        pc.tick_into(a, T3); a.sw(T3, 20, T2)
        a.lw(T3, -0x848, pc.GPR); a.lw(T3, 0x84, T3); a.lw(T3, 0x214, T3); a.sw(T3, 24, T2)
        for w in range(48):
            a.lw(T3, 4 * w, pc.SP); a.sw(T3, 0x40 + 4 * w, T2)
        # fixed memory windows (MEMWIN): 16 words into entry +0x100
        for w, addr in enumerate(MEMWIN[:16]):
            a.li(T1, addr); a.lw(T3, 0, T1); a.sw(T3, 0x100 + 4 * w, T2)
        a.label(f'd{k}')
        a.li(T0, SAVE); a.lq(T1, 0, T0); a.lq(T2, 0x10, T0); a.lq(T3, 0x20, T0)
        a.emit(orig[h][0]); a.emit(orig[h][1])
        a.j(h + 8); a.nop()
    return a


def main():
    p = argparse.ArgumentParser(); p.add_argument('state'); p.add_argument('out')
    p.add_argument('--hook', action='append', required=True); p.add_argument('--seconds', type=float, default=20); p.add_argument('--min-tick', type=int, default=0)
    args = p.parse_args(); hooks = [int(h, 0) for h in args.hook]
    src = Path(args.state)
    with zipfile.ZipFile(src) as z: mem = z.read('eeMemory.bin')
    u = lambda at: struct.unpack_from('<I', mem, at)[0]
    orig = {h: (u(h), u(h + 4)) for h in hooks}
    for h, (w0, w1) in orig.items():
        # relocating the first two instructions must be position independent (no branches / jal)
        for w in (w0, w1):
            op = w >> 26
            if op in (1, 2, 3, 4, 5, 6, 7, 20, 21, 22, 23) or (op == 0 and (w & 0x3F) in (8, 9)): raise SystemExit(f'{h:#x}: first instructions not relocatable')
    a = build_stub(hooks, orig); code = a.link()
    assert STUB + len(code) <= SAVE
    patches = [dict(address=hex(STUB), expected=mem[STUB:STUB + len(code)].hex(), replacement=code.hex()),
               dict(address=hex(LOG), expected=mem[LOG:LOG + 8].hex(), replacement=struct.pack('<2I', 0, args.min_tick).hex())]
    for k, h in enumerate(hooks):
        target = STUB + 4 * a.labels[f'h{k}']
        jw = struct.pack('<2I', (2 << 26) | ((target >> 2) & 0x3FFFFFF), 0)
        patches.append(dict(address=hex(h), expected=mem[h:h + 8].hex(), replacement=jw.hex()))
    scratch = Path(args.out).with_suffix('.p2s')
    from reference_replay import patch_state
    patch_state(src, scratch, patches)
    slot = random.randint(28100, 28999); datapath = pc.DATAPATH.parent / f'pcsx2-{slot}'
    pc.prepare_datapath(datapath, slot)
    cmd = [str(pc.PCSX2), '-datapath', str(datapath), '-batch', '-nogui', '-statefile', str(scratch.resolve()), '--', str(pc.ISO)]
    log = open(Path(args.out).with_suffix('.log'), 'w')
    proc = subprocess.Popen(cmd, stdout=log, stderr=subprocess.STDOUT)
    pine = None; started = time.time(); ticks = []; entries = []
    try:
        while time.time() - started < args.seconds + 30:
            if proc.poll() is not None: raise RuntimeError('emulator exited')
            if pine is None:
                try: pine = pc.Pine(slot)
                except OSError: time.sleep(0.5); continue
                started = time.time()
            try:
                g = struct.unpack('<I', pine.read(pc.GP - 0x848, 4))[0]; s = struct.unpack('<I', pine.read(g + 0x84, 4))[0]
                gi = struct.unpack('<I', pine.read(s + 0xC, 4))[0]; t = struct.unpack('<I', pine.read(gi + 8, 4))[0]
                ws = struct.unpack('<I', pine.read(s + 0x214, 4))[0]
            except (OSError, RuntimeError, ConnectionError): time.sleep(0.2); continue
            if not ticks or ticks[-1][:2] != [t, ws]: ticks.append([t, ws, round(time.time() - started, 2)])
            if time.time() - started > args.seconds: break
            time.sleep(0.01)
        n = struct.unpack('<I', pine.read(LOG, 4))[0]
        for k in range(min(n, MAXN)):
            d = pine.read(LOG + 0x10 + k * ENTRY, ENTRY); w = struct.unpack('<80I', d)
            entries.append(dict(hook=hex(w[0]), ra=hex(w[1]), a0=hex(w[2]), a1=hex(w[3]), sp=hex(w[4]), tick=w[5], ws=w[6], stack=[hex(x) for x in w[16:64]], mem=[struct.unpack('<f', struct.pack('<I', x))[0] for x in w[64:80]], memhex=[hex(x) for x in w[64:80]]))
    finally:
        if pine: pine.close()
        proc.terminate()
        try: proc.wait(10)
        except subprocess.TimeoutExpired: proc.kill()
        log.close(); shutil.rmtree(datapath, ignore_errors=True)
    # compress tick log: keep changes that are not +1
    jumps = [ticks[i] for i in range(len(ticks)) if i == 0 or ticks[i][0] != ticks[i - 1][0] + 1 or ticks[i][1] != ticks[i - 1][1]]
    Path(args.out).write_text(json.dumps(dict(hooks=[hex(h) for h in hooks], entries=entries, tick_jumps=jumps, last=ticks[-1:] if ticks else None), indent=1))
    print(json.dumps(dict(n=len(entries), jumps=jumps[:40], last=ticks[-1:] if ticks else None)))
    for e in entries: print(e['hook'], 'ra', e['ra'], 'a0', e['a0'], 'a1', e['a1'], 'tick', e['tick'], 'ws', e['ws'], 'mem', ' '.join('%.6g' % x for x in e['mem'][:len(MEMWIN)]))


if __name__ == '__main__':
    main()
