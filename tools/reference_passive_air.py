#!/usr/bin/env python3
"""Read the original passive-air control4 state at motion-owner+210."""
import struct,math,json,argparse,zipfile
from pathlib import Path

def extract_passive_air(memory,rider):
    def read(fmt,at):
        if not 0<=at<=len(memory)-struct.calcsize('<'+fmt):raise ValueError('Passive-air pointer outside memory')
        return struct.unpack_from('<'+fmt,memory,at)[0]
    u=lambda a:read('I',a);i=lambda a:read('i',a)
    def f(a):
        value=read('f',a)
        if not math.isfinite(value):raise ValueError('Nonfinite passive-air state')
        return value
    owner=u(rider+0x77c);control=owner+0x210
    if i(owner+0xde4)!=4:raise ValueError('Passive-air extraction requires control4')
    if u(control+0x14)!=rider:raise ValueError('Passive-air actor backlink differs')
    return dict(state=dict(entry_angle=f(control),entry_magnitude=f(control+4),upper_latch=i(control+8),identity_latch=i(control+12),last_identity=i(control+16)),
        provenance=dict(control_address=hex(control),entry='12F620',update='12F730',exit='12FB68',source_owner_offset='210'))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('snapshot',type=Path);p.add_argument('--rider',type=lambda x:int(x,0),default=0x14701a0);p.add_argument('--output',type=Path,required=True);a=p.parse_args();m=zipfile.ZipFile(a.snapshot).read('eeMemory.bin');a.output.write_text(json.dumps(extract_passive_air(m,a.rider),indent=2)+'\n')
if __name__=='__main__':main()
