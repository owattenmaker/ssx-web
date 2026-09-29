#!/usr/bin/env python3
"""Extract original landing material/stat, timing and animation-class context."""
import argparse,hashlib,json,math,struct,zipfile
from pathlib import Path

def extract_landing(memory,base=0x14701a0):
    def read(fmt,a):
        if not 0<=a<=len(memory)-struct.calcsize(fmt):raise ValueError('Landing pointer outside EE memory')
        return struct.unpack_from(fmt,memory,a)
    def u(a):return read('<I',a)[0]
    def i(a):return read('<i',a)[0]
    def f(a):
        v=read('<f',a)[0]
        if not math.isfinite(v):raise ValueError('Nonfinite landing field')
        return v
    root=u(0x4a30f0-0x848);world=u(root+0x84);surfaces=u(world+0x44);owner=u(base+0x77c)
    materials=[dict(depth1=f(surfaces+n*0xb0+0x14),depth3=f(surfaces+n*0xb0+0x18),
        normal_impulse_factor=f(surfaces+n*0xb0+0x3c),maximum_normal_speed=f(surfaces+n*0xb0+0x40),
        recovery=u(surfaces+n*0xb0+0x44)) for n in range(19)]
    slot=i(base+0x86c)
    if not 0<=slot<16:raise ValueError('Invalid landing rider slot')
    entry=i(0x5305b0+slot*4);record=0x535b20+entry*28;character=read('<b',record+17)[0];bank=2 if i(record+12)==-1 else u(record+16)&1
    override=i(base+0xb34)
    if u(0x534fe0+entry*28+16)&4:stat=.5
    else:
        numerator=override if override>0 else int(read('<b',0x535538+bank*70+character*7+6)[0]/5)
        denominator=read('<b',0x5308d8+character*15+14)[0]
        if denominator<=0:raise ValueError('Invalid original landing stat denominator')
        stat=struct.unpack('<f',struct.pack('<f',numerator/denominator))[0]
    animation=u(base+0x784);semantic=i(animation+8);node=u(u(animation+0x50)+20)
    flags=read('<Q',node+0xb0)[0] if node else 0
    return dict(profile=dict(materials=materials,landing_stat=stat,body_scale=f(u(base+0x780)+0x140)),
        runtime=dict(tick=u(u(world+12)+8),last_ground_leave_tick=u(owner+20),ground_focus_tick=u(owner+16),
            manual_state330=i(base+0x330),animation_class=i(0x446990+semantic*28) if semantic!=0x1b6 else 0,
            animation_flags=f'0x{flags:016x}'),
        provenance=dict(stat_getter='149120 progression6/maximum14, scalarDIV nearest',material_stride='0xB0',
            material_offsets=['14','18','3C','40','44'],source_axis='Z-up centimeters',
            timing_getter='1298C8 game-info totalTicks',landing_board_bone=i(base+0x8a0),
            probe='13A7B0 posed board center +/-200cm presentation-up; preferred0.574999988'))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('state',type=Path);p.add_argument('--output',type=Path);a=p.parse_args()
    with zipfile.ZipFile(a.state) as z:memory=z.read('eeMemory.bin')
    result=dict(state=str(a.state.resolve()),ee_sha256=hashlib.sha256(memory).hexdigest(),original_landing=extract_landing(memory));text=json.dumps(result,indent=2)+'\n'
    if a.output:a.output.write_text(text)
    print(text)
if __name__=='__main__':main()
