#!/usr/bin/env python3
"""Read a paused SSX3 camera checkpoint through PINE; never writes game RAM."""
import argparse
import hashlib
import json
import struct
import time
import zipfile
from pathlib import Path
from pcsx2_pine import Pine


def capture(client, baseline):
    if client.info()['game_id'] != 'SLUS-20772' or client.status() != 'paused':
        raise ValueError('Capture requires paused SSX3 USA')
    with zipfile.ZipFile(baseline) as archive:
        seed = archive.read('eeMemory.bin')
    candidates = []
    signature = struct.pack('<I', 0x45ca38)
    at = 0
    while (at := seed.find(signature, at)) >= 0:
        base = at - 16
        at += 4
        if base > 0 and base % 16 == 0:
            candidates.append(base)
    if not candidates:
        raise ValueError('Baseline has no DEFAULT_3 objects')
    u32 = lambda data, offset: struct.unpack_from('<I', data, offset)[0]
    pointer = u32(client.read(0x4a30f0-0x848,4),0)
    pointer = u32(client.read(pointer+0x84,4),0)
    pointer = u32(client.read(pointer+0xc,4),0)
    tick_address = pointer+8
    tick = u32(client.read(tick_address,4),0)
    cameras = []
    for base in candidates:
        data = client.read(base, 0x390)
        if u32(data, 0x10) != 0x45ca38:
            raise ValueError('Camera allocation changed since baseline')
        target = client.read(u32(data, 0x30), 8)
        rider = u32(target, 4)
        actor = client.read(rider, 0xb40)
        if u32(actor, 0x6c0) != 0x4583a8:
            raise ValueError('Camera target is not the baseline human rider')
        geometry = client.read(u32(actor, 0x780), 0x30)
        head = client.read(u32(geometry, 0x2c) + 32*u32(actor, 0x89c), 16)
        motion = client.read(u32(actor, 0x77c)+0xde0, 8)
        trajectory = client.read(u32(actor, 0x788), 0xb0)
        splines = []
        for offset in (0x304, 0x310, 0x31c, 0x328, 0x334):
            flags = u32(data, offset)
            count = (flags >> 3) & 255
            pointer = u32(data, offset+8)
            # Inactive spline storage may contain uninterpretable constructor data.
            if flags & 2 and 0 < count <= 13 and 0 < pointer < 32*1024*1024-count*20:
                splines.append({'offset':offset,'words':list(struct.unpack('<'+'I'*(count*5), client.read(pointer,count*20)))})
        cameras.append(dict(address=base,rider=rider,tick=tick,
            mode=u32(motion,0),head=list(struct.unpack('<4f',head)),
            algorithm_words=list(struct.unpack('<228I',data)),splines=splines,
            rider_words=list(struct.unpack('<720I',actor)),
            trajectory_words=list(struct.unpack('<44I',trajectory))))
    if client.status() != 'paused' or u32(client.read(tick_address,4),0) != tick:
        raise ValueError('Emulator resumed during capture')
    return dict(baseline_sha256=hashlib.sha256(baseline.read_bytes()).hexdigest(),cameras=cameras)


if __name__ == '__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--socket',type=Path,required=True)
    parser.add_argument('--baseline',type=Path,required=True)
    destination=parser.add_mutually_exclusive_group(required=True)
    destination.add_argument('--output',type=Path)
    destination.add_argument('--directory',type=Path)
    args=parser.parse_args()
    with Pine(args.socket) as client:
        deadline=time.monotonic()+3
        while client.status()!='paused' and time.monotonic()<deadline:
            time.sleep(.02)
        result=capture(client,args.baseline)
    output=args.output or args.directory / ('tick-%08d.json' % result['cameras'][0]['tick'])
    output.parent.mkdir(parents=True,exist_ok=True)
    with output.open('x') as file:
        file.write(json.dumps(result,separators=(',',':'))+'\n')
    print(json.dumps([dict(address=c['address'],tick=c['tick'],mode=c['mode']) for c in result['cameras']]))
