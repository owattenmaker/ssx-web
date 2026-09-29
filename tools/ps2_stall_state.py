#!/usr/bin/env python3
"""Derive a stall variant of a built capture savestate (tools/ps2_capture.py build output): at game tick T the capture's
provider-exit hook first spins until the vblank counter 0x50AAA8 (VBLANK_S handler 0x3C1980: *(0x50A8E8+0x1C0) += 1)
has advanced K times. The game's own frame loop then shows what it does after a K-vblank frame.
usage: ps2_stall_state.py IN.p2s OUT.p2s TICK K [dump]   (IN = a ps2_capture.py build output; its .capture.json is copied
next to OUT with a 'stall' entry; 'dump' also copies sp and 0x3F0 stack bytes to 0xE1000 at TICK, read with --watch 0xE1000:0x400).
Frame-pacing study, docs/workers.md "Frame pacing after a hitch". The derived state is disposable; baselines are never modified."""
import sys, json, struct, zipfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import ps2_capture as pc
from reference_replay import patch_state
src, dst, tick, k = Path(sys.argv[1]), Path(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
with zipfile.ZipFile(src) as z: mem = z.read('eeMemory.bin')
code, pad_entry, log_entry, collide_entry = pc.assemble()
first2 = mem[log_entry:log_entry + 8]
assert struct.unpack('<2I', first2)[0] == 0x27BDFF40, hex(struct.unpack('<2I', first2)[0])   # addiu sp, sp, -0xC0
STUB, VB = 0xE0000, 0x50AAA8
assert not any(mem[STUB:STUB + 0x400])
a = pc.Asm(STUB)
T0, T1, T2, T3, SP, ZERO = pc.T0, pc.T1, pc.T2, pc.T3, pc.SP, pc.ZERO
a.addiu(SP, SP, -0x40)
for n, r in enumerate((T0, T1, T2, T3)): a.sq(r, 16 * n, SP)
pc.tick_into(a, T0)
a.li(T1, tick); a.bne(T0, T1, 'done'); a.nop()
a.li(T1, VB); a.lw(T2, 0, T1); a.addiu(T2, T2, k)
a.label('spin'); a.lw(T3, 0, T1); a.sltu(T0, T3, T2); a.bne(T0, ZERO, 'spin'); a.nop()
a.label('done')
if len(sys.argv) > 5:   # STACKDUMP: at the stall tick copy sp and 0x3F0 bytes above it to 0xE1000 (read with --watch 0xE1000:0x400)
    a.li(T1, tick); pc.tick_into(a, T0); a.bne(T0, T1, 'nodump'); a.nop()
    a.li(T1, 0xE1000); a.addiu(T2, SP, 0x40); a.sw(T2, 0, T1); a.sw(pc.GPR, 4, T1); a.addiu(T1, T1, 16); a.addiu(T3, ZERO, 0xFC)
    a.label('dump'); a.lw(T0, 0, T2); a.sw(T0, 0, T1); a.addiu(T2, T2, 4); a.addiu(T1, T1, 4); a.addiu(T3, T3, -1); a.bne(T3, ZERO, 'dump'); a.nop()
    a.label('nodump')
for n, r in enumerate((T0, T1, T2, T3)): a.lq(r, 16 * n, SP)
a.addiu(SP, SP, 0x40)
a.emit(struct.unpack('<I', first2[:4])[0]); a.emit(struct.unpack('<I', first2[4:])[0])
a.j(log_entry + 8); a.nop()
stub = a.link()
j = struct.pack('<2I', (2 << 26) | (STUB >> 2), 0)
patch_state(src, dst, [dict(address=hex(STUB), expected='00' * len(stub), replacement=stub.hex()),
                       dict(address=hex(log_entry), expected=first2.hex(), replacement=j.hex())])
m = json.loads(src.with_suffix('.capture.json').read_text()); m['stall'] = dict(tick=tick, vblanks=k, stub=hex(STUB))
dst.with_suffix('.capture.json').write_text(json.dumps(m, indent=2) + '\n')
print('stall stub at', hex(STUB), 'log entry', hex(log_entry), 'tick', tick, 'vblanks', k)
