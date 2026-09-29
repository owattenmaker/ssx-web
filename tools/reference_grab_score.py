#!/usr/bin/env python3
"""Extract initial ordinary grab scoring inputs; no score outcomes are injected."""
import struct,math,json,zipfile,argparse
from pathlib import Path

def extract_grab_score(memory,rider):
    def read(fmt,at):
        if not 0<=at<=len(memory)-struct.calcsize('<'+fmt):raise ValueError('Grab score pointer outside memory')
        return struct.unpack_from('<'+fmt,memory,at)[0]
    u=lambda a:read('I',a);i=lambda a:read('i',a)
    def f(a):
        value=read('f',a)
        if not math.isfinite(value):raise ValueError('Nonfinite grab score state')
        return value
    base=u(rider+0x790)
    if u(base+0x1ac)!=rider:raise ValueError('Grab scoring actor backlink differs')
    normal=[]
    for index in range(15):
        score_id=read('H',0x45aeb8+index*20+4)
        if not 0<=score_id<64:raise ValueError('Ordinary grab score ID outside verified table')
        normal.append(dict(score_id=score_id,begin_points=i(0x530600+score_id*8),hold_points=i(0x530604+score_id*8)))
    thresholds=[dict(seconds=f(0x459e20+n*8),points=f(0x459e24+n*8)) for n in range(4)]
    if thresholds[-1]['seconds']!=-1:raise ValueError('Missing authored grab threshold sentinel')
    state={name:f(base+offset) for name,offset in [('accumulated14',0x14),('hold_increment3c',0x3c),('hold_seconds40',0x40),('total_seconds44',0x44),('longest_seconds48',0x48),('combo_timeout_a4',0xa4),('multiplier1c4',0x1c4)]}
    state.update({name:i(base+offset) for name,offset in [('normal_count4c',0x4c),('tweak_count50',0x50),('uber_count54',0x54),('super_uber_count58',0x58),('active_uber5c',0x5c),('bonus_points84',0x84),('hold_threshold_index8c',0x8c)]})
    state['history60']=[i(base+0x60+n*4) for n in range(3)]
    repeat_history=dict(entries=[[u(base+0xa8+n*8),u(base+0xac+n*8)] for n in range(10)],next=u(base+0xf8))
    if repeat_history['next']>=10:raise ValueError('Original trick history cursor outside ten-entry ring')
    repeat_context={name:i(base+offset) for name,offset in [('field08',8),('style20',0x20),('active70',0x70),('field7c',0x7c),('flag28',0x28)]}
    return dict(profile=dict(normal=normal,hold_thresholds=thresholds),state=state,repeat_history=repeat_history,repeat_context=repeat_context,
        provenance=dict(score_address=hex(base),begin='119708',end='1197D8/119068',tick='117D24..117D70/119210',points='117948',ordinary_begin_end_boost_delta=0,scope='Grab subset of shared trick scoring; other categories and commit remain separate.'))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('snapshot',type=Path);p.add_argument('--rider',type=lambda s:int(s,0),default=0x14701a0);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    memory=zipfile.ZipFile(a.snapshot).read('eeMemory.bin');a.output.write_text(json.dumps(extract_grab_score(memory,a.rider),indent=2)+'\n')
if __name__=='__main__':main()
