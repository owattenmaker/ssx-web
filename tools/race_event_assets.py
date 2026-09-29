#!/usr/bin/env python3
"""Extract authored SSX3 AIP paths/events without inventing checkpoint geometry.

SPDX-License-Identifier: GPL-3.0-only
Binary layout reference: local pinned SSX-Library WorldAIP.cs; runtime semantics
are independently recovered from the owned PS2 executable. Original data stays private.
"""
import argparse,hashlib,json,math,struct
from pathlib import Path

def decode_aip(data):
    pos=0
    def read(fmt):
        nonlocal pos
        size=struct.calcsize('<'+fmt)
        if pos+size>len(data):raise ValueError('Truncated authored AIP resource')
        values=struct.unpack_from('<'+fmt,data,pos);pos+=size
        if any(isinstance(x,float) and not math.isfinite(x) for x in values):raise ValueError('Nonfinite AIP field')
        return list(values)
    def count():
        n=read('I')[0]
        if n>100000:raise ValueError('AIP count exceeds validated extent')
        return n
    magic=read('I')[0];ai=[];track=[]
    for index in range(count()):
        offset=pos;fields=read('7I');points=count();events=count()
        entry=dict(index=index,offset=offset,fields=fields,position=read('3f'),low=read('3f'),high=read('3f'))
        entry['segments']=[read('4f') for _ in range(points)]
        entry['events']=[dict(zip(('type','value','start','end'),read('IIff'))) for _ in range(events)];ai.append(entry)
    for index in range(count()):
        offset=pos;kind,a,b,c=read('IIIf');points=count();events=count()
        entry=dict(index=index,offset=offset,type=kind,field0=a,field1=b,field2=c,position=read('3f'),low=read('3f'),high=read('3f'))
        entry['segments']=[read('4f') for _ in range(points)]
        entry['events']=[dict(zip(('type','value','start','end'),read('IIff'))) for _ in range(events)];track.append(entry)
    links=[read('II') for _ in range(count())]
    regions=[read('II6fII') for _ in range(count())]
    if pos!=len(data):raise ValueError(f'Unparsed AIP bytes: {len(data)-pos}')
    return dict(magic=magic,ai_paths=ai,track_paths=track,links=links,regions=regions,
        provenance=dict(sha256=hashlib.sha256(data).hexdigest(),format='PS2 SSB kind14 AIP',axis='original Z-up',
            segment_semantics='raw float4 retained pending source path-construction verification',
            event_semantics='type1 reaches rider finish callback10E5D8; other types retained without interpretation'))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('aip',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    value=decode_aip(a.aip.read_bytes());a.output.write_text(json.dumps(value,indent=2)+'\n')
    print(json.dumps(dict(ai_paths=len(value['ai_paths']),track_paths=len(value['track_paths']),
        track_events=[dict(index=x['index'],type=x['type'],events=x['events']) for x in value['track_paths']]),indent=2))
if __name__=='__main__':main()
