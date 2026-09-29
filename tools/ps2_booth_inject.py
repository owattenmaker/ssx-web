#!/usr/bin/env python3
"""PS2 booth-contact injection capture (derived savestates only, silent: ps2_capture forces the volume to 0).

ps2_capture's derived state from the Metro-City countdown anchor (the setpieces-bra2-full pads, human <-> computer
pairs isolated as that run), plus one guarded hook at 0x121818 (the rider's post pass: rider+0xA30 = the selected
contact's instance, 30A060 runs its slot-2 program): on game tick T, for the given rider, rider+0xA30 is set to a stage
instance, so 121818 runs that instance's slot-2 program exactly as a real contact on that tick would (the booth /
water-tower programs: builtin 77 draw, builtin 34 -> rider vt+0x54 0x123210). Everything else is the original code.

    python3 tools/ps2_booth_inject.py build OUT.p2s PREFIX NEUTRAL [--inject TICK:RES[:RIDER]] ... [--ai-state] [--log LO:HI]
    python3 tools/ps2_capture.py run OUT.p2s OUT.bin --frames N [--snap ...]

PREFIX script indexes of scripts/setpieces-bra2-full.json, then NEUTRAL neutral indexes. RIDER: a rider address from the
manifest's roster (default the human). docs/stage-teleport.md section 4; local/ps2-capture/runs/booth/.
"""
import json, struct, sys, zipfile
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import ps2_capture as cap  # noqa: E402
from reference_replay import patch_state  # noqa: E402

ANCHOR = ROOT / 'local/reference/pcsx2/metro-city-countdown-anchor.p2s'
FULL = json.loads((ROOT / 'local/ps2-capture/scripts/setpieces-bra2-full.json').read_text())['segments']
HOOK = 0x121820   # inside 121818 after its prologue (its first two words are the --ai-state marker hook)
HOOK_BYTES = struct.pack('<2I', 0xFFBF0000, 0x0080802D)   # sd ra,0x0(sp) / daddu s0,a0,zero
INJ_CODE, INJ_DATA = 0xFF000, 0xFF300                     # free arena words (ring to 0xE0000; --ai-state: code 0xF8000, data 0xFE000..0xFE500)
GP = 0x4A30F0
# Optional camera set-target log (--log LO:HI): 0x166C60 (base set-target: a0 algorithm, a1 rider id) entry, per call on game
# ticks LO..HI: tick, a0, a1, $ra and the human's +0x1E0 velocity at that moment. Watched into every record (layout watches).
LOG_HOOK, LOG_BYTES = 0x166C60, struct.pack('<2I', 0x27BDFF70, 0x7FB00070)   # addiu sp,sp,-0x90 / sq s0,0x70(sp)
LOG_CODE, LOG_CTL, LOG_MAX = 0xFF100, 0xFF400, 48


def assemble_log():
    T0, T1, T3, T4, T5, T6, T7 = cap.T0, cap.T1, cap.T3, cap.T4, cap.T5, cap.T6, cap.T7
    a = cap.Asm(LOG_CODE)
    a.li(T0, LOG_CTL); cap.tick_into(a, T3)
    a.lw(T1, 0, T0); a.sltu(T4, T3, T1); a.bne(T4, cap.ZERO, 'done'); a.nop()
    a.lw(T1, 4, T0); a.sltu(T4, T1, T3); a.bne(T4, cap.ZERO, 'done'); a.nop()
    a.lw(T5, 8, T0); a.lw(T6, 12, T0); a.sltu(T4, T5, T6); a.beq(T4, cap.ZERO, 'done'); a.nop()
    a.sll(T7, T5, 5); a.addu(T7, T7, T0); a.addiu(T7, T7, 16)
    a.sw(T3, 0, T7); a.sw(cap.A0, 4, T7); a.sw(cap.A1, 8, T7); a.sw(31, 12, T7)
    a.li(T6, cap.DATA); a.lw(T6, cap.F_RIDER, T6)
    for k in range(3): a.lw(T4, 0x1E0 + 4 * k, T6); a.sw(T4, 16 + 4 * k, T7)
    a.addiu(T5, T5, 1); a.sw(T5, 8, T0)
    a.label('done')
    a.emit(0x27BDFF70); a.emit(0x7FB00070)
    a.j(LOG_HOOK + 8); a.nop()
    return a.link()


def instance_of(mem, res):
    u = lambda a: struct.unpack_from('<I', mem, a & 0x1FFFFFF)[0]
    W = u(u(GP + 0x16C8)); track = u(u(W + 8) + 4 * (res & 0xFF)); e = u(u(track + 0x1C) + 4 * (res >> 8))
    return (e >> 8) << 2


def assemble_hook():
    T0, T1, T2, T3, T4, T5, T6, T7 = cap.T0, cap.T1, cap.T2, cap.T3, cap.T4, cap.T5, cap.T6, cap.T7
    a = cap.Asm(INJ_CODE)
    a.li(T0, INJ_DATA); a.lw(T1, 0, T0); a.beq(T1, cap.ZERO, 'done'); a.nop()
    cap.tick_into(a, T3)
    a.addiu(T2, T0, 16)
    a.label('loop')
    a.lw(T4, 0, T2); a.bne(T4, T3, 'next'); a.nop()
    a.lw(T5, 4, T2); a.bne(T5, cap.ZERO, 'cmp'); a.nop()
    a.li(T6, cap.DATA); a.lw(T5, cap.F_RIDER, T6)
    a.label('cmp')
    a.bne(T5, cap.A0, 'next'); a.nop()
    a.lw(T6, 8, T2); a.sw(T6, 0xA30, cap.A0)
    a.lw(T7, 12, T2); a.addiu(T7, T7, 1); a.sw(T7, 12, T2)
    a.label('next')
    a.addiu(T2, T2, 16); a.addiu(T1, T1, -1); a.bne(T1, cap.ZERO, 'loop'); a.nop()
    a.label('done')
    a.emit(0xFFBF0000); a.emit(0x0080802D)   # the two relocated words
    a.j(HOOK + 8); a.nop()
    return a.link()


def main():
    if sys.argv[1] != 'build': raise SystemExit(__doc__)
    out, prefix, neutral = Path(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
    injects = [x for k, x in enumerate(sys.argv) if k and sys.argv[k - 1] == '--inject']
    per = [dict((k, v) for k, v in s.items() if k != 'frames') for s in FULL for _ in range(int(s['frames']))][:prefix]
    segs = []
    for s in per + [{}] * neutral:
        if segs and {k: v for k, v in segs[-1].items() if k != 'frames'} == s: segs[-1]['frames'] += 1
        else: segs.append(dict(s, frames=1))
    script = out.with_suffix('.script.json'); script.write_text(json.dumps({'segments': segs}) + '\n')
    staged = out.with_name(out.stem + '.stage.p2s')
    log = next((x for k, x in enumerate(sys.argv) if k and sys.argv[k - 1] == '--log'), None)
    manifest = cap.build(str(ANCHOR), str(script), str(staged), isolate=True, ai_state='--ai-state' in sys.argv, watches=[(LOG_CTL, 16 + 32 * LOG_MAX)] if log else ())
    mem = zipfile.ZipFile(staged).read('eeMemory.bin')
    rows = []
    for x in injects:
        parts = x.split(':'); tick, res = int(parts[0], 0), int(parts[1], 0); rider = int(parts[2], 0) if len(parts) > 2 else 0
        rows.append((tick, rider, instance_of(mem, res), res))
    code = assemble_hook()
    data = struct.pack('<4I', len(rows), 0, 0, 0) + b''.join(struct.pack('<4I', t, r, i, 0) for t, r, i, _ in rows)
    for at, blob in ((INJ_CODE, code), (INJ_DATA, data)):
        if any(mem[at:at + len(blob)]): raise ValueError(f'arena {at:#x} not free')
    if mem[HOOK:HOOK + 8] != HOOK_BYTES: raise ValueError('0x121820 differs')
    j = struct.pack('<2I', (2 << 26) | (INJ_CODE >> 2), 0)
    extra = []
    if log:
        lo, hi = (int(v, 0) for v in log.split(':')); lc = assemble_log(); ctl = struct.pack('<4I', lo, hi, 0, LOG_MAX)
        if mem[LOG_HOOK:LOG_HOOK + 8] != LOG_BYTES or any(mem[LOG_CODE:LOG_CODE + len(lc)]) or any(mem[LOG_CTL:LOG_CTL + 16 + 32 * LOG_MAX]): raise ValueError('log hook')
        extra = [dict(address=hex(LOG_CODE), expected='00' * len(lc), replacement=lc.hex()), dict(address=hex(LOG_CTL), expected='00' * 16, replacement=ctl.hex()),
                 dict(address=hex(LOG_HOOK), expected=LOG_BYTES.hex(), replacement=struct.pack('<2I', (2 << 26) | (LOG_CODE >> 2), 0).hex())]
    patch_state(staged, out, extra + [dict(address=hex(INJ_CODE), expected='00' * len(code), replacement=code.hex()),
                              dict(address=hex(INJ_DATA), expected='00' * len(data), replacement=data.hex()),
                              dict(address=hex(HOOK), expected=HOOK_BYTES.hex(), replacement=j.hex())])
    manifest['booth_injections'] = [dict(tick=t, rider=hex(r), instance=hex(i), resource=hex(s)) for t, r, i, s in rows]
    manifest['booth_hook'] = dict(hook=hex(HOOK), code=hex(INJ_CODE), data=hex(INJ_DATA))
    out.with_suffix('.capture.json').write_text(json.dumps(manifest, indent=2) + '\n')
    for f in (staged, staged.with_suffix('.capture.json'), staged.with_suffix('.patches.json')):
        if f.exists() and f != out: f.unlink()
    print(json.dumps(dict(out=str(out), injections=manifest['booth_injections'], frames=len(per) + neutral)))


if __name__ == '__main__':
    main()
