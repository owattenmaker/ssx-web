#!/usr/bin/env python3
"""Extract authoritative air-entry profile, prewind and pivot in any rider mode."""
import argparse,hashlib,json,math,struct,zipfile
from pathlib import Path
from reference_input import float32_zero

def extract_air_entry(memory,base=0x14701a0):
    def read(fmt,at):
        if not 0<=at<=len(memory)-struct.calcsize(fmt):raise ValueError('Air-entry pointer outside EE memory')
        return struct.unpack_from(fmt,memory,at)
    def u(at):return read('<I',at)[0]
    def i(at):return read('<i',at)[0]
    def f(at):
        v=read('<f',at)[0]
        if not math.isfinite(v):raise ValueError('Nonfinite air-entry field')
        return v
    def triplet(at):return dict(current=f(at),rate=f(at+4),target=f(at+8))
    slot=i(base+0x86c)
    if not 0<=slot<16:raise ValueError('Invalid original rider slot')
    entry=i(0x5305b0+slot*4);record=0x535b20+entry*28
    char=read('<b',record+17)[0];bank=2 if i(record+12)==-1 else u(record+16)&1;override=i(base+0xb34)
    if u(0x534fe0+entry*28+16)&4:stat=.5
    else:
        numerator=override if override>0 else int(read('<b',0x535538+bank*70+char*7+4)[0]/5)
        denominator=read('<b',0x5308d8+char*15+12)[0]
        if denominator<=0:raise ValueError('Invalid original trick-stat denominator')
        stat=struct.unpack('<f',struct.pack('<f',numerator/denominator))[0]
    skeleton=u(base+0x780);pivot_index=i(base+0x89c)
    if not 0<=pivot_index<256:raise ValueError('Invalid original pivot index')
    bone=u(skeleton+0x24)+pivot_index*16;pivot=[float32_zero(f(bone+4*k)*f(skeleton+0x140+4*k)) for k in range(3)]
    animation_id=i(u(base+0x784)+8)
    return dict(profile=dict(trick_stat=stat,boost_modifier=f(base+0x2ec)>0,landing_animation=animation_id==0x120),
        prewind=dict(spin=triplet(base+0x2a4),flip=triplet(base+0x2b0),jump_gate=f(base+0x360)),pivot=pivot,
        provenance=dict(stat_getter='1495A8 scalarDIV nearest',character=char,progress_bank=bank,stat_override=override,
            source_axis='Z-up',pivot_index=pivot_index,pivot_address=hex(bone),prewind_offsets=['2A4','2B0'],
            pivot_lifecycle='seed only; animation player must supply current animated pivot for dynamic presentation'))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('state',type=Path);p.add_argument('--output',type=Path);a=p.parse_args()
    with zipfile.ZipFile(a.state) as z:memory=z.read('eeMemory.bin')
    result=dict(state=str(a.state.resolve()),ee_sha256=hashlib.sha256(memory).hexdigest(),original_air_entry=extract_air_entry(memory));text=json.dumps(result,indent=2)+'\n'
    if a.output:a.output.write_text(text)
    print(text)
if __name__=='__main__':main()
