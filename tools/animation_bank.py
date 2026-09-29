#!/usr/bin/env python3
"""Inventory original SSX3 animation banks without inventing motion data.

AFB directory layout recovered from the user's assets. Curve decoding is WIP.
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path
from collections import Counter
from world_models import Reader
from compare_character_assets import big_members
from world_assets import refpack


def game_hash(data):
    value=0
    for byte in data:
        if not byte:break
        value=(value*16+(byte if byte<128 else byte-256))&0xffffffff
        high=value&0xf0000000
        value^=high>>23
        value^=high
    return value


def read_bank(data):
    endian='<' if data[:2]==b'\x4c\x13' else '>'
    r=Reader(data,endian)
    magic,count,channel_at,event_at,payload_at=r.read('HHIII',0)
    if magic not in (0x4213,0x134c) or channel_at!=16+count*20 or not channel_at<=event_at<=payload_at<=len(data):
        raise ValueError('Unexpected AFB layout')
    if (event_at-channel_at)%8 or (payload_at-event_at)%4:raise ValueError('Misaligned AFB tables')
    n=(event_at-channel_at)//8
    channels=[]
    for i in range(n):
        offset,part,flags,frames=r.read('IBBH',channel_at+i*8)
        if payload_at+offset>=len(data):raise ValueError('AFB payload offset outside bank')
        channels.append(dict(offset=offset,part=part,flags=flags,frame_field=frames))
    offsets=sorted({c['offset'] for c in channels}|{len(data)-payload_at})
    next_offset=dict(zip(offsets,offsets[1:]))
    for c in channels:
        raw=data[payload_at+c['offset']:payload_at+next_offset[c['offset']]]
        dofs=raw[0]
        if not dofs or 1+(dofs+1)//2>len(raw):raise ValueError('Invalid AFB curve header')
        types=[(raw[1+i//2]>>(4 if i%2==0 else 0))&15 for i in range(dofs)]
        c.update(byte_size=len(raw),dofs=dofs,curve_types=types)
    animations=[]
    for i in range(count):
        hash_value,first,part_count,segments,frames,frame_field,event_first,event_count=r.read('II6H',16+i*20)
        if first+part_count*segments>n or (event_first+event_count)*4>payload_at-event_at:
            raise ValueError('AFB animation table span outside bank')
        events=[r.read('2H',event_at+(event_first+j)*4) for j in range(event_count)]
        animations.append(dict(hash=f'{hash_value:08x}',first_channel=first,part_count=part_count,
                               segment_count=segments,frame_count_field=frames,
                               frame_field=frame_field,events=events))
    return dict(magic=f'{magic:04x}',endianness=endian,source_sha256=hashlib.sha256(data).hexdigest(),
                payload_offset=payload_at,channels=channels,animations=animations)


def main():
    from disc_paths import ps2_iso;parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--source',choices=('gamecube','ps2'),default='gamecube');parser.add_argument('--ps2-iso',type=Path,default=ps2_iso());args=parser.parse_args()
    target=Path('local/assets/animation')/('ps2' if args.source=='ps2' else '');target.mkdir(parents=True,exist_ok=True)
    if args.source=='ps2':
        from inspect_disc import Disc
        disc=Disc(args.ps2_iso)
        try:archive=disc.file('DATA/CHAR/ANM.BIG')
        finally:disc.close()
    else:archive=Path('local/gamecube/disc/files/data/char/anmb.big').read_bytes()
    strings=set()
    for p in (Path('local/gamecube/disc/sys/main.dol'),Path('local/disc/SLUS_207.72')):
        for raw in re.findall(rb'[\x20-\x7e]{3,}',p.read_bytes()):
            strings.update((raw,raw.lower(),raw.upper()))
    names={}
    for raw in strings:names.setdefault(f'{game_hash(raw):08x}',set()).add(raw.decode())
    reports=[]
    for path,packed in big_members(archive):
        data=refpack(packed)
        name=Path(path).stem
        report=read_bank(data)
        for animation in report['animations']:
            animation['name_candidates']=sorted(names.get(animation['hash'],set()))
        report['note']='Recovered bank directory and matching original names. Frame field semantics and curve codec are still under investigation; no animation playback is claimed.'
        (target/(name+('.afl' if args.source=='ps2' else '.afb'))).write_bytes(data)
        (target/(name+'.json')).write_text(json.dumps(report,indent=2)+'\n')
        reports.append(dict(bank=name,animations=len(report['animations']),channels=len(report['channels']),
                            named=sum(bool(a['name_candidates']) for a in report['animations'])))
    print(json.dumps(reports))


if __name__=='__main__':main()
