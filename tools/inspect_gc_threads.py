#!/usr/bin/env python3
"""Inspect original SDK thread waits in a matched live RAM/register snapshot."""
import json,struct
from pathlib import Path
root=Path(__file__).resolve().parents[1]/'local/gamecube'
r=(root/'live-ram.bin').read_bytes();state=json.loads((root/'live-state.json').read_text())
def u32(a): return struct.unpack_from('>I',r,a&0x1ffffff)[0]
def u16(a): return struct.unpack_from('>H',r,a&0x1ffffff)[0]
print('CPU',hex(state['pc']),'LR',hex(state['lr']),'current thread',hex(u32(0x800000e4)))
r13=state['gpr'][13]
print('Queue count',u32(r13-0x504c),'token',r[(r13-0x506f)&0x1ffffff])
a=u32(0x800000dc);seen=set()
while a and a not in seen and len(seen)<128:
 seen.add(a)
 print(f'thread={a:08x} state={u16(a+0x2c8)} suspend={u32(a+0x2cc)} priority={u32(a+0x2d0)} wait={u32(a+0x2dc):08x} pc={u32(a+0x198):08x} lr={u32(a+0x84):08x}')
 a=u32(a+0x2fc)
