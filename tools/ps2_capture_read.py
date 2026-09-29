#!/usr/bin/env python3
"""Decode tools/ps2_capture.py ring records (8 KiB layout)."""
import struct
from pathlib import Path
RECORD=8192
RIDER,CAMERA,OTHERS,OWNER,BONES,BODY=32,2656,3008,3168,3264,8192
def records(path):
    import json
    data=Path(path).read_bytes()
    manifest=Path(path).with_suffix('.capture.json')
    RECORD=json.loads(manifest.read_text()).get('record',8192) if manifest.exists() else 8192
    out=[]
    for i in range(0,len(data),RECORD):
        r=data[i:i+RECORD]
        u=lambda o:struct.unpack_from('<I',r,o)[0]
        rider=(lambda rr:lambda off,fmt='<f':struct.unpack_from(fmt,rr,RIDER+off-0x100))(r)
        others=[dict(position=struct.unpack_from('<3f',r,OTHERS+32*k),velocity=struct.unpack_from('<3f',r,OTHERS+16+32*k)) for k in range(5)]
        owner=struct.unpack_from('<16i',r,OWNER)
        bones=[struct.unpack_from('<8f',r,BONES+32*b) for b in range(32)]
        out.append(dict(seq=u(0),tick=u(4),word0=u(8),word1=u(12),mode=u(16),control=u(20),pad_calls=u(24),index=u(28),
            position=rider(0x110,'<3f'),quat=rider(0x120,'<4f'),velocity=rider(0x1e0,'<3f'),normal=rider(0x370,'<3f'),
            turn=rider(0x1f0)[0],brake=rider(0x214)[0],crouch=rider(0x220)[0],others=others,owner=owner,bones=bones,rider=rider,raw=r))
    return out
