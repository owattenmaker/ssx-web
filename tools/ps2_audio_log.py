#!/usr/bin/env python3
"""Sound / speech dispatch call log for tools/ps2_capture.py captures (development reference only).

`ps2_capture.py build --audio-log` adds these patches to the derived savestate: each hooked function entry
jumps to a stub in the free 0xF0000 arena that appends a 64-byte record to a ring at 0xF2000 (512 entries,
running count at 0xF1000), runs the two relocated prologue instructions and jumps back.  The stubs use only
v0/v1/t6-t9 (not argument registers in the EE EABI).  `ps2_capture.py run` streams the ring over PINE while
the capture runs and writes RUN.audio.json.

Record: tag, game tick (*(*(*(gp-0x848)+0x84)+0xC)+8, the capture record tick), a0, a1, a2, a3, t0, t1, t2, ra,
seq, x0, x1, x2 (per-hook dereferences, see HOOKS).

Hooks (docs/audio-logic.md section 5):
  2906B8  sound play (every SFX voice start): x0 = request index *(*(a0)+0x1F4), x1 = bank slot *(*(a0+4)+4i),
          x2 = sound index *(*(a0+8)+4i)
  2B1458  speech request (a2 = event id, e.g. 0x2133 Arcade_Uber)
  2A3DE0  Arcade_Uber category (a2 = variant mask: 1 Uber run count crossing 4 from 29B430, 2 Super Uber from 29B738,
          8 monster trick from 29B7E0)
  2A3C00  Arcade_Prompts (Tricky 0x10/8/4, time up 2)     2A3CE8 Arcade_Bonus     2A3EB8 Arcade_Icons
  2A3B18  Arcade_Power_Ups
  29B0E0  pending-Uber sound start (0x66)                 29B3C0 pending-Uber stop: x0 = voice handle *(a0+0x5FDC)
  299638  Tricky start                                    2997B8 Tricky end
  29B430  Uber combo commit (a2 = +0x54, a3 = +0x114)     29B7E0 monster trick speech
"""
import struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))

CODE, COUNT, RING, N = 0xF0000, 0xF1000, 0xF2000, 512
V0, V1, T6, T7, T8, T9, GP, RA = 2, 3, 14, 15, 24, 25, 28, 31
ARGS = [4, 5, 6, 7, 8, 9, 10]      # a0..a3, t0..t2
HOOKS = {
    0x2906B8: (1, 'play', 'play'),
    0x2B1458: (2, 'speech', None),
    0x2A3DE0: (3, 'arcade_uber', None),
    0x2A3C00: (4, 'arcade_prompts', None),
    0x2A3CE8: (5, 'arcade_bonus', None),
    0x2A3EB8: (6, 'arcade_icons', None),
    0x2A3B18: (7, 'arcade_power_ups', None),
    0x29B0E0: (8, 'pending_uber_start', None),
    0x29B3C0: (9, 'pending_uber_stop', 'voice'),
    0x299638: (10, 'tricky_start', None),
    0x2997B8: (11, 'tricky_end', None),
    0x29B430: (12, 'uber_commit', None),
    0x29B7E0: (13, 'monster_speech', None),
}
# PS2_AUDIO_LOG_EXTRA="1EA930:hud_update,1EC3F8:hud_draw": extra entry hooks (call order studies; tags from 40).
import os
for k, item in enumerate(filter(None, os.environ.get('PS2_AUDIO_LOG_EXTRA', '').split(','))):
    addr, name = item.split(':'); HOOKS[int(addr, 16)] = (40 + k, name, None)
TAGS = {tag: name for tag, name, _ in HOOKS.values()}


def assemble(memory):
    from ps2_capture import Asm
    a = Asm(CODE); entries = {}
    for addr, (tag, _, extra) in HOOKS.items():
        entries[addr] = a.here()
        a.li(V0, COUNT); a.lw(V1, 0, V0)
        a.andi(T6, V1, N - 1); a.sll(T6, T6, 6); a.li(T7, RING); a.addu(T6, T6, T7)
        a.addiu(T7, 0, tag); a.sw(T7, 0, T6)
        a.addiu(T8, 0, -1)
        a.lw(T7, -0x848, GP); a.beq(T7, 0, f'nt{addr}'); a.nop()
        a.lw(T7, 0x84, T7); a.beq(T7, 0, f'nt{addr}'); a.nop()
        a.lw(T7, 0x0C, T7); a.beq(T7, 0, f'nt{addr}'); a.nop()
        a.lw(T8, 8, T7)
        a.label(f'nt{addr}'); a.sw(T8, 4, T6)
        for k, reg in enumerate(ARGS): a.sw(reg, 8 + 4 * k, T6)
        a.sw(RA, 36, T6); a.sw(V1, 40, T6)
        if extra == 'play':
            a.lw(T8, 0, 4); a.lw(T8, 0x1F4, T8); a.sw(T8, 44, T6); a.sll(T9, T8, 2)
            a.lw(T7, 4, 4); a.addu(T7, T7, T9); a.lw(T7, 0, T7); a.sw(T7, 48, T6)
            a.lw(T7, 8, 4); a.addu(T7, T7, T9); a.lw(T7, 0, T7); a.sw(T7, 52, T6)
        elif extra == 'voice':
            a.lw(T7, 0x5FDC, 4); a.sw(T7, 44, T6)
        a.addiu(V1, V1, 1); a.sw(V1, 0, V0)
        a.emit(struct.unpack_from('<I', memory, addr)[0]); a.emit(struct.unpack_from('<I', memory, addr + 4)[0])
        a.j(addr + 8); a.nop()
    return a.link(), entries


def patches(memory):
    """ps2_capture.py patch records: the arena code, a zeroed count and one jump per hooked entry."""
    if any(memory[CODE:0x100000]): raise ValueError('0xF0000 arena not free')
    for addr in HOOKS:   # the two relocated instructions must not be branches/jumps
        for w in struct.unpack_from('<2I', memory, addr):
            op = w >> 26
            if op in (1, 2, 3, 4, 5, 6, 7, 20, 21, 22, 23) or (op == 0 and (w & 0x3F) in (8, 9)): raise ValueError(f'hook {addr:#x}: control transfer in prologue')
    code, entries = assemble(memory)
    j = lambda target: struct.pack('<2I', (2 << 26) | (target >> 2), 0)
    out = [dict(address=hex(CODE), expected='00' * len(code), replacement=code.hex())]
    for addr, e in entries.items():
        out.append(dict(address=hex(addr), expected=memory[addr:addr + 8].hex(), replacement=j(e).hex()))
    return out


def decode(raw):
    tag, tick, *regs = struct.unpack_from('<2i7I', raw, 0)
    ra, seq, x0, x1, x2 = struct.unpack_from('<IIiii', raw, 36)
    return dict(seq=seq, tag=tag, name=TAGS.get(tag, tag), tick=tick, a0=regs[0], a1=regs[1], a2=regs[2], a3=regs[3],
                t0=regs[4], t1=regs[5], t2=regs[6], ra=ra, x0=x0, x1=x1, x2=x2)


def read_new(pine, have):
    """Entries seq >= have from the live ring (PINE); raises on overrun."""
    count, = struct.unpack('<I', pine.read(COUNT, 4))
    if count - have > N: raise RuntimeError(f'audio log overrun: {count} written, {have} read')
    rows = []
    for s in range(have, count):
        raw = pine.read(RING + 64 * (s % N), 64); r = decode(raw)
        if r['seq'] != s: break
        rows.append(r)
    return rows
