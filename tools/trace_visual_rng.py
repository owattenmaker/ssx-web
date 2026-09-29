#!/usr/bin/env python3
"""Trace every draw of the particle/visual six-word RNG 0x3177F0 (state 0x4FF018) in a PS2 run
(development reference only; original discs, reference savestates and ps2_capture.py are unchanged).

`build` derives a normal `tools/ps2_capture.py build` state (pad script, provider-exit records) and then
adds, in the free 0xE0000..0xF2000 arena of non --ai-state builds:

* an entry hook on 0x3177F0 that appends one 32-byte entry per draw to a 2048-entry ring:
  seq, game tick, $ra (the calling site), $s0 (for 0x3710D0 the emitter), the current context marker
  (function, a0, $ra of its caller) and 0x4FF018 word 5 before the draw (the generator's draw counter);
* entry hooks on context marker functions (entity update 0x356198, particle effect updates 0x345B40/0x345F90,
  flag manager 0x34C668, rider manager 0x128AF0, rider snow FX 0x2DF920, ...): each stores (function, a0,
  caller $ra) as the current context and, on its first call in a game tick, appends a phase entry whose
  $ra field is function|1, so the log also shows the order of the passes inside a tick.

`run` launches ARMSX2/PCSX2 headless like ps2_capture.run (same datapath/PINE handling), follows the ring
over PINE and writes OUTPUT.trace.json (all entries in order) plus a per-tick summary.

usage:
  trace_visual_rng.py build BASELINE SCRIPT OUT.p2s [--isolate] [--lcg]
  trace_visual_rng.py run OUT.p2s OUT.bin --frames N
  trace_visual_rng.py summary OUT.trace.json [--from T --to T]
"""
import argparse, json, shutil, struct, subprocess, sys, time, zipfile, random
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import ps2_capture as cap  # noqa: E402
from reference_replay import patch_state  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
TRACE_CODE, TRACE_DATA, TRACE_RING, TRACE_ENTRIES = 0xE0000, 0xE1F00, 0xE2000, 2048  # data: seq + context + 4 bytes per marker
D_SEQ, D_FN, D_A0, D_RA, D_LAST = 0, 4, 8, 12, 64
RNG_HOOK = 0x3177F0
RNG_BYTES = struct.pack('<2I', 0x27BDFFF0, 0x3C040050)     # addiu sp,sp,-0x10 / lui a0,0x50
# Context markers. All have relocatable first two instructions (checked at build).
MARKERS = {
    0x128AF0: 'rider manager 128AF0',
    0x111948: 'rider FX components 111948',
    0x2DF920: 'rider snow FX 2DF920',
    0x34C668: 'flag manager tick 34C668',
    0x34B228: 'flag grid build 34B228',
    0x356198: 'entity update 356198',
    0x357950: 'type-13 entity update 357950',
    0x341D48: 'LiveComp tick 341D48',
    0x345B40: 'Particle effect update 345B40',
    0x345F90: 'DynamicParticle effect update 345F90',
    0x3705E0: 'static emitter init 3705E0',
    0x370DC8: 'dynamic emitter init 370DC8',
    0x2FD420: 'builtin16 2FD420',
    0x2FEE98: 'builtin26 2FEE98',
    0x30A060: 'slot-2 contact dispatcher 30A060',
    0x30A298: 'slot-1 section dispatcher 30A298',
    0x165938: 'camera shake apply 165938',
    0x35F410: 'TexFlip update 35F410',
}


# Markers logged on every call (not only the first per tick) with a chosen argument in the a0 field:
# 0x30A060(context, rider, packet, ...) -> the contacted trigger instance *(packet+0x50);
# 0x30A298(context, instance) -> the activated instance.
EVERY_CALL = {0x30A060: (6, 0x50), 0x30A298: (5, None)}
# --lcg: every function that stores the other visual generator, the LCG gp+0xA0C (0x4A3AFC; board track, sparks, snow,
# fist sparkle, camera splash), is also an every-call marker whose entry logs the LCG word at entry in the ctx a0 field
# (its a0 goes to the s0 field). Consecutive entries give the LCG draws of each call.
LCG_WORD = 0x4A3AFC
LCG_MARKERS = {
    0x2DABC8: 'board track 2DABC8', 0x2DE398: 'board jitter 2DE398', 0x2DFE88: 'rock spray 2DFE88', 0x2E02B8: 'chunky 2E02B8',
    0x2E0EE8: 'snow trail 2E0EE8', 0x2E1120: 'rider breath 2E1120', 0x2E1598: 'impact 2E1598', 0x2E1A80: 'cloudy 2E1A80',
    0x2E4F50: 'LCG 2E4F50', 0x2E8938: 'board sparks 2E8938', 0x2F1150: 'fist sparkle 2F1150', 0x2F1A08: 'LCG 2F1A08',
    0x2F2598: 'splash 2F2598', 0x2F2810: 'splash 2F2810', 0x2F2D70: 'splash 2F2D70', 0x2F3030: 'splash 3030', 0x2F3640: 'splash 3640',
    0x2F3810: 'splash 3810',
}


def u32(data, at): return struct.unpack_from('<I', data, at)[0]


def assemble(marker_words, lcg=False):
    a = cap.Asm(TRACE_CODE)
    T0, T1, T2, T3, SP, ZERO, A0 = cap.T0, cap.T1, cap.T2, cap.T3, cap.SP, cap.ZERO, cap.A0
    S0 = cap.S0

    def append(ra_reg_or_value, s0_reg):
        """t0 = TRACE_DATA, t1 = tick; uses t2/t3."""
        a.lw(T2, D_SEQ, T0); a.andi(T3, T2, TRACE_ENTRIES - 1); a.sll(T3, T3, 5)
        a.addu(T3, T3, T0); a.addiu(T3, T3, TRACE_RING - TRACE_DATA)
        a.sw(T2, 0, T3); a.sw(T1, 4, T3)
        if isinstance(ra_reg_or_value, int) and ra_reg_or_value > 31:
            a.li(T2, ra_reg_or_value); a.sw(T2, 8, T3)
        else:
            a.sw(ra_reg_or_value, 8, T3)
        a.sw(s0_reg, 12, T3)
        a.lw(T2, D_FN, T0); a.sw(T2, 16, T3); a.lw(T2, D_A0, T0); a.sw(T2, 20, T3); a.lw(T2, D_RA, T0); a.sw(T2, 24, T3)
        a.li(T2, 0x4FF018); a.lw(T2, 20, T2); a.sw(T2, 28, T3)
        a.lw(T2, D_SEQ, T0); a.addiu(T2, T2, 1); a.sw(T2, D_SEQ, T0)

    def save(): a.addiu(SP, SP, -0x40); [a.sq(r, 16 * n, SP) for n, r in enumerate((T0, T1, T2, T3))]
    def restore(): [a.lq(r, 16 * n, SP) for n, r in enumerate((T0, T1, T2, T3))]; a.addiu(SP, SP, 0x40)

    a.label('rng'); save(); a.li(T0, TRACE_DATA); cap.tick_into(a, T1); append(31, S0); restore()
    for w in struct.unpack('<2I', RNG_BYTES): a.emit(w)
    a.j(RNG_HOOK + 8); a.nop()
    entries = {}
    markers = {**MARKERS, **(LCG_MARKERS if lcg else {})}
    for k, fn in enumerate(sorted(markers)):
        w1, w2 = marker_words[fn]
        # 0x2E1A80 starts addiu sp / lui $at: the trampoline's j + nop after the relocated pair leave $at intact.
        at_ok = fn == 0x2E1A80 and w2 >> 16 == 0x3C01
        if not (cap.relocatable(w1) and (cap.relocatable(w2) or at_ok)): raise ValueError(f'marker {fn:#x} not relocatable')
        a.label(f'm{fn:x}'); entries[fn] = a.here(); save(); a.li(T0, TRACE_DATA)
        a.li(T1, fn); a.sw(T1, D_FN, T0); a.sw(A0, D_A0, T0); a.sw(31, D_RA, T0)
        if lcg and fn in LCG_MARKERS:
            a.li(T2, LCG_WORD); a.lw(T2, 0, T2); a.sw(T2, D_A0, T0)
            cap.tick_into(a, T1); append(fn | 1, A0)   # ctx a0 field = the LCG at entry, s0 field = a0
        elif fn in EVERY_CALL:
            reg, off = EVERY_CALL[fn]
            if off is None: a.sw(reg, D_A0, T0)
            else: a.lw(T2, off, reg); a.sw(T2, D_A0, T0)
            cap.tick_into(a, T1); append(fn | 1, A0)   # the argument is in the ctx a0 field
        else:
            cap.tick_into(a, T1); a.lw(T2, D_LAST + 4 * k, T0); a.beq(T1, T2, f'd{fn:x}'); a.nop()
            a.sw(T1, D_LAST + 4 * k, T0); append(fn | 1, A0)
        a.label(f'd{fn:x}'); restore(); a.emit(w1); a.emit(w2); a.j(fn + 8); a.nop()
    code = a.link()
    if TRACE_CODE + len(code) > TRACE_DATA: raise ValueError('trace code too large')
    return code, a.labels, entries


def build(baseline, script, output, isolate=False, lcg=False):
    output = Path(output); tmp = output.with_name(output.stem + '.base.p2s')
    manifest = cap.build(baseline, script, tmp, isolate=isolate)
    with zipfile.ZipFile(tmp) as z: memory = z.read('eeMemory.bin')
    markers = {**MARKERS, **(LCG_MARKERS if lcg else {})}
    words = {fn: struct.unpack_from('<2I', memory, fn) for fn in markers}
    code, labels, entries = assemble(words, lcg)
    j = lambda target: struct.pack('<2I', (2 << 26) | (target >> 2), 0)
    if memory[RNG_HOOK:RNG_HOOK + 8] != RNG_BYTES: raise ValueError('0x3177F0 prologue differs')
    if any(memory[TRACE_CODE:TRACE_RING + 32 * TRACE_ENTRIES]): raise ValueError('trace arena not free')
    patches = [dict(address=hex(TRACE_CODE), expected='00' * len(code), replacement=code.hex()),
               dict(address=hex(RNG_HOOK), expected=RNG_BYTES.hex(), replacement=j(TRACE_CODE).hex())]
    for fn, entry in entries.items():
        patches.append(dict(address=hex(fn), expected=struct.pack('<2I', *words[fn]).hex(), replacement=j(entry).hex()))
    patch_state(tmp, output, patches)
    manifest['trace'] = dict(code=hex(TRACE_CODE), data=hex(TRACE_DATA), ring=hex(TRACE_RING), entries=TRACE_ENTRIES,
                             entry='seq, tick, ra (fn|1 = phase marker), s0 (marker: a0), ctx fn, ctx a0, ctx caller ra, 4FF018 word5 before',
                             markers={hex(k): v for k, v in markers.items()}, lcg=lcg)
    output.with_suffix('.capture.json').write_text(json.dumps(manifest, indent=2) + '\n')
    for suffix in ('.p2s', '.capture.json', '.patches.json'):
        p = tmp.with_suffix(suffix)
        if p.exists(): p.unlink()
    return manifest['trace']


def run(state, output, frames, timeout=900):
    manifest = json.loads(Path(state).with_suffix('.capture.json').read_text())
    slot = random.randint(28100, 28999); datapath = cap.DATAPATH.parent / f'pcsx2-{slot}'
    cap.prepare_datapath(datapath, slot)
    args = [str(cap.PCSX2), '-datapath', str(datapath), '-batch', '-nogui', '-statefile', str(Path(state).resolve()), '--', str(cap.ISO)]
    log = open(Path(output).with_suffix('.pcsx2.log'), 'w')
    proc = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT)
    entries, pine, started, read_seq, lost = [], None, time.time(), 0, 0
    try:
        while time.time() - started < timeout:
            if proc.poll() is not None: raise RuntimeError(f'emulator exited early ({proc.returncode})')
            if pine is None:
                try: pine = cap.Pine(slot)
                except OSError: time.sleep(0.5); continue
            try:
                seq = u32(pine.read(TRACE_DATA + D_SEQ, 4), 0); writes = u32(pine.read(cap.DATA + cap.F_WRITES, 4), 0)
            except (OSError, RuntimeError, ConnectionError): time.sleep(0.2); continue
            if seq - read_seq > TRACE_ENTRIES:
                lost += seq - read_seq - TRACE_ENTRIES; read_seq = seq - TRACE_ENTRIES
            while read_seq < seq:
                first = read_seq % TRACE_ENTRIES; count = min(seq - read_seq, TRACE_ENTRIES - first)
                data = pine.read(TRACE_RING + 32 * first, 32 * count)
                for n in range(count):
                    e = struct.unpack_from('<8I', data, 32 * n)
                    if e[0] != read_seq + n: raise RuntimeError(f'ring entry {e[0]} != {read_seq + n}')
                    entries.append(e)
                read_seq += count
            if writes >= frames: break
            time.sleep(0.01)
        else: raise RuntimeError('trace timed out')
    finally:
        if pine: pine.close()
        proc.terminate()
        try: proc.wait(10)
        except subprocess.TimeoutExpired: proc.kill()
        log.close(); shutil.rmtree(datapath, ignore_errors=True)
    keys = ('seq', 'tick', 'ra', 's0', 'ctx_fn', 'ctx_a0', 'ctx_ra', 'w5')
    out = dict(state=str(state), frames=frames, lost=lost, markers=manifest['trace']['markers'],
               entries=[dict(zip(keys, e)) for e in entries])
    Path(output).with_suffix('.trace.json').write_text(json.dumps(out) + '\n')
    return dict(entries=len(entries), lost=lost, first_tick=entries[0][1] if entries else None, last_tick=entries[-1][1] if entries else None)


def function_of(pc):
    from original_live_build import containing_file
    import re
    try: return int(re.search(r'sub_([0-9A-F]{8})_', containing_file(pc)).group(1), 16)
    except Exception: return None


def summary(path, first=None, last=None):
    d = json.loads(Path(path).read_text()); names = {int(k, 16): v for k, v in d['markers'].items()}
    ticks = {}
    for e in d['entries']:
        if first is not None and e['tick'] < first: continue
        if last is not None and e['tick'] > last: continue
        ticks.setdefault(e['tick'], []).append(e)
    for t, es in sorted(ticks.items()):
        line = []
        for e in es:
            if e['ra'] & 1: line.append(f"<{names.get(e['ra'] & ~1, hex(e['ra']))}>")
            else: line.append(f"{e['ra']:x}[{names.get(e['ctx_fn'], hex(e['ctx_fn']))} a0={e['ctx_a0']:x} s0={e['s0']:x}]")
        print(t, len([e for e in es if not e['ra'] & 1]), 'draws:', ' '.join(line))


def main():
    p = argparse.ArgumentParser(description=__doc__); sub = p.add_subparsers(dest='cmd', required=True)
    b = sub.add_parser('build'); b.add_argument('baseline'); b.add_argument('script'); b.add_argument('output'); b.add_argument('--isolate', action='store_true'); b.add_argument('--lcg', action='store_true')
    r = sub.add_parser('run'); r.add_argument('state'); r.add_argument('output'); r.add_argument('--frames', type=int, required=True); r.add_argument('--timeout', type=int, default=900)
    s = sub.add_parser('summary'); s.add_argument('trace'); s.add_argument('--from', dest='first', type=int); s.add_argument('--to', dest='last', type=int)
    a = p.parse_args()
    if a.cmd == 'build': print(json.dumps(build(a.baseline, a.script, a.output, a.isolate, a.lcg), indent=2))
    elif a.cmd == 'run': print(json.dumps(run(a.state, a.output, a.frames, a.timeout), indent=2))
    else: summary(a.trace, a.first, a.last)


if __name__ == '__main__':
    main()
