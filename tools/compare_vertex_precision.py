#!/usr/bin/env python3
"""Measure GameCube position quantization against corresponding PS2 meshes.

SPDX-License-Identifier: GPL-3.0-only
PS2 mesh-block format reference: SSX-Library SSX3PS2MPF.
"""
import json
import struct
from pathlib import Path
from inspect_disc import Disc,inspect_big
from compare_character_assets import big_members
from world_assets import refpack
from world_models import Reader,align16
from rider_assets import decode_high_model


def ps2_positions(data):
    r=Reader(data)
    _,header,base=r.read('HHI',4)
    offset=r.u32(header+16)
    body=refpack(data[base+offset:]);b=Reader(body)
    groups_at=r.u32(header+32);count=r.read('H',header+72)[0]
    result=[]
    for group in range(count):
        kind,_,_,refs,refs_at=b.read('5I',groups_at+group*20)
        for ref in range(refs):
            meshes_at,meshes=b.read('2I',refs_at+ref*8)
            for mesh in range(meshes):
                at,_=b.read('2I',meshes_at+mesh*8)
                if at==0xffffffff:continue
                for _ in range(1024):
                    if b.read('B',at+31)[0]!=0x6c:break
                    strips,_,attributes,vertices=b.read('4I',at+48)
                    if vertices>256 or strips>256:raise ValueError('Unexpected MPF mesh block')
                    at+=80+strips*16
                    if attributes:
                        at=align16(at+48+vertices*8)
                        at=align16(at+48+vertices*6)
                    if vertices:
                        at+=48
                        stride=16 if kind==17 else 12
                        result.extend(b.read('3f',at+i*stride) for i in range(vertices))
                        at=align16(at+vertices*stride)
                    at+=32
                else:raise ValueError('Unterminated MPF mesh block chain')
    return list(set(result))


def main():
    gc=dict(big_members(Path('local/gamecube/disc/files/data/char/mdlngc.big').read_bytes()))
    from disc_paths import ps2_iso;disc=Disc(ps2_iso())
    report=[]
    try:
        archive=next(e for e in disc.entries if e['path']=='DATA/CHAR/MDLPS2.BIG')
        ps={e['path']:e for e in inspect_big(disc,archive)['files']}
        for part in ('HeadA','HandsA','TopB','BottomA','BootsA'):
            e=ps[f'mac_{part}.mpf'];positions=ps2_positions(disc.read(e['disc_offset'],e['size']))
            model=decode_high_model(gc[f'mac_{part}.mnf'])
            packed=[tuple(round(v[i]*12700) for i in range(3)) for v in model['vertices']]
            candidates={}
            for scale in (127,127.5,128):
                errors=[]
                for q in packed:
                    errors.append(min(sum((q[k]/scale-p[k])**2 for k in range(3)) for p in positions)**.5)
                candidates[str(scale)]=dict(mean_cm=sum(errors)/len(errors),max_cm=max(errors))
            report.append(dict(part=part,ps2_unique_positions=len(positions),gamecube_vertices=len(packed),candidates=candidates))
    finally:disc.close()
    Path('local/assets/vertex-precision.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))


if __name__=='__main__':main()
