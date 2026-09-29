#!/usr/bin/env python3
"""Batch decode every owned PS2 animation into a shared native preview library."""
import array
import hashlib
import json
import re
import sys
from pathlib import Path
from animation_bank import read_bank
from animation_curves import CurvePacket

ROOT=Path(__file__).resolve().parents[1]

def main():
    folder=ROOT/'local/assets/native/ANIMATIONS';folder.mkdir(parents=True,exist_ok=True)
    payload=bytearray();clips=[];sources=[]
    for bank_name in ('basic','fe'):
        path=ROOT/'local/assets/animation/ps2'/f'{bank_name}.afl';raw=path.read_bytes();bank=read_bank(raw)
        inventory=json.loads(path.with_suffix('.json').read_text())
        if inventory['source_sha256']!=bank['source_sha256']:raise ValueError('Animation names refer to a different bank')
        sources.append(dict(bank=bank_name,sha256=hashlib.sha256(raw).hexdigest(),clips=len(bank['animations'])))
        for index,animation in enumerate(bank['animations']):
            parts={}
            for channel in bank['channels'][animation['first_channel']:animation['first_channel']+animation['part_count']*animation['segment_count']]:
                at=bank['payload_offset']+channel['offset'];data=bytearray(raw[at:at+channel['byte_size']])
                packet=CurvePacket(data,channel['frame_field'])
                for word in range(packet.offsets[8],packet.offsets[8]+packet.sizes[8],2):data[word:word+2]=data[word:word+2][::-1]
                packet=CurvePacket(data,channel['frame_field']);frames=parts.setdefault(str(channel['part']),[])
                frames.extend(packet.sample(frame) for frame in range(1 if frames else 0,packet.frames))
            streams={}
            for part,frames in parts.items():
                if len(frames)!=animation['frame_count_field']:raise ValueError(f'{bank_name}/{index}: segment join differs')
                count=len(frames[0]);values=array.array('f',(v for frame in frames for v in frame))
                if any(len(frame)!=count for frame in frames):raise ValueError('Changing channel layout')
                if sys.byteorder!='little':values.byteswap()
                streams[part]=dict(offset=len(payload),channels=count,frames=len(frames));payload.extend(values.tobytes())
            candidates=inventory['animations'][index].get('name_candidates',[])
            names=[n for n in candidates if re.fullmatch(r'[A-Z][A-Z0-9_]*',n)]
            name=names[0] if names else f'{bank_name.upper()}_{animation["hash"]}'
            clips.append(dict(id=f'{bank_name}:{index}',name=name,bank=bank_name,source_hash=animation['hash'],
                source_name_candidates=candidates,fps=30,frame_count=animation['frame_count_field'],
                duration=(animation['frame_count_field']-1)/30,loop=name.endswith('_CYC') or name=='A_CYC_2',events=animation['events'],streams=streams))
    binary=folder/'samples.f32';temporary=binary.with_suffix('.tmp');temporary.write_bytes(payload);temporary.replace(binary)
    manifest=dict(version=1,encoding='little-endian float32',sources=sources,byte_length=len(payload),
        sha256=hashlib.sha256(payload).hexdigest(),clips=clips,
        scope='Every source clip and part decoded at authored30Hz; original names/events retained. Gameplay selection is separate.')
    target=folder/'library.json';temporary=target.with_suffix('.tmp');temporary.write_text(json.dumps(manifest,indent=2)+'\n');temporary.replace(target)
    print(f'Exported {len(clips)} original clips, {len(payload)/1048576:.2f} MiB: {target}')

if __name__=='__main__':main()
