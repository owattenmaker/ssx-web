#!/usr/bin/env python3
"""Build deterministic PCSX2 input movies and inspect their state fixtures.

P2M2 version-1 layout verified against PCSX2 v2.8.2 fd9d310c source.
Original disc images are inputs only. RAM patches are applied to derived states.
"""
import argparse
import hashlib
import json
import shutil
import struct
import zipfile
from pathlib import Path

BUTTONS={
    'Select':(0,0,None),'L3':(0,1,None),'R3':(0,2,None),'Start':(0,3,None),
    'Up':(0,4,8),'Right':(0,5,6),'Down':(0,6,9),'Left':(0,7,7),
    'L2':(1,0,16),'R2':(1,1,17),'L1':(1,2,14),'R1':(1,3,15),
    'Triangle':(1,4,10),'Circle':(1,5,11),'Cross':(1,6,12),'Square':(1,7,13)}


def pad_frame(buttons=(),left=(127,127),right=(127,127)):
    if len(left)!=2 or len(right)!=2:raise ValueError('Each stick needs two axes')
    if any(not isinstance(v,int) or not 0<=v<=255 for v in (*left,*right)):
        raise ValueError('Analog values must be bytes')
    data=bytearray((255,255,*right,*left,*([0]*12)))
    for name in buttons:
        if name not in BUTTONS:raise ValueError(f'Unknown button {name}')
        group,bit,pressure=BUTTONS[name]
        data[group]&=~(1<<bit)
        if pressure is not None:data[pressure]=255
    return bytes(data)


def string_field(text,size):
    raw=text.encode('utf-8')
    if len(raw)>=size:raise ValueError('Recording metadata too long')
    return raw+b'\0'*(size-len(raw))


def build(spec,output):
    baseline=Path(spec['baseline']).resolve()
    if not baseline.is_file():raise ValueError('Missing machine-state baseline')
    if output.suffix!='.p2m2' or output.resolve()==baseline:raise ValueError('Movie output must be a separate .p2m2 file')
    frames=spec['frames']
    if not isinstance(frames,int) or not 1<=frames<=60*60*10:raise ValueError('Invalid recording duration')
    events=spec.get('events',[])
    for event in events:
        if not 0<=event['start']<event['end']<=frames:raise ValueError('Input range outside recording')
    output.parent.mkdir(parents=True,exist_ok=True)
    header=(b'\x01'+string_field('PCSX2-v2.8.2',50)+string_field('SSX3 native comparison',255)
            +string_field('SSX 3',255)+struct.pack('<II?',frames,0,True))
    assert len(header)==570
    payload=bytearray()
    for frame in range(frames):
        buttons=[];left=(127,127);right=(127,127)
        for event in events:
            if event['start']<=frame<event['end']:
                buttons.extend(event.get('buttons',[]))
                left=event.get('left',left);right=event.get('right',right)
        payload.extend(pad_frame(buttons,left,right));payload.extend(pad_frame())
    movie=header+payload
    state=Path(str(output)+'_SaveState.p2s')
    if state.resolve()!=baseline:shutil.copy2(baseline,state)
    output.write_bytes(movie)
    manifest=dict(spec=spec,pcsx2='v2.8.2',source_state_sha256=hashlib.sha256(baseline.read_bytes()).hexdigest(),
                  movie_sha256=hashlib.sha256(movie).hexdigest(),frame_bytes=36,
                  note='Load with Tools > Input Recording > Play. PCSX2 pauses at the final replay frame.')
    output.with_suffix('.json').write_text(json.dumps(manifest,indent=2)+'\n')
    return manifest


def inspect_state(path):
    with zipfile.ZipFile(path) as archive:
        data=archive.read('eeMemory.bin')
        if len(data)!=32*1024*1024:raise ValueError('Unexpected EE memory size')
        return dict(path=str(path),sha256=hashlib.sha256(Path(path).read_bytes()).hexdigest(),
                    ee_sha256=hashlib.sha256(data).hexdigest(),entries=archive.namelist())


def compare_states(first,second):
    result={}
    with zipfile.ZipFile(first) as a,zipfile.ZipFile(second) as b:
        for name in ('eeMemory.bin','iopMemory.bin','GS.bin','Screenshot.png'):
            x,y=a.read(name),b.read(name)
            result[name]=dict(identical=x==y,first_sha256=hashlib.sha256(x).hexdigest(),
                              second_sha256=hashlib.sha256(y).hexdigest())
    return result


def patch_state(source,output,patches):
    if source.resolve()==output.resolve():raise ValueError('Patch output must be a derived state')
    with zipfile.ZipFile(source) as archive:
        memory=bytearray(archive.read('eeMemory.bin'))
        # Validate every expected byte before applying any patch.
        for patch in patches:
            at=int(patch['address'],0);old=bytes.fromhex(patch['expected']);new=bytes.fromhex(patch['replacement'])
            if len(old)!=len(new) or at<0 or at+len(old)>len(memory):raise ValueError('Invalid RAM patch extent')
            if memory[at:at+len(old)]!=old:raise ValueError(f'Expected bytes do not match at {at:#x}')
        for patch in patches:
            at=int(patch['address'],0);new=bytes.fromhex(patch['replacement']);memory[at:at+len(new)]=new
        output.parent.mkdir(parents=True,exist_ok=True)
        with zipfile.ZipFile(output,'w',compression=zipfile.ZIP_DEFLATED) as result:
            for entry in archive.infolist():
                data=bytes(memory) if entry.filename=='eeMemory.bin' else archive.read(entry.filename)
                result.writestr(entry.filename,data)
    output.with_suffix('.patches.json').write_text(json.dumps(dict(source=inspect_state(source),patches=patches),indent=2)+'\n')


def main():
    parser=argparse.ArgumentParser(description=__doc__);sub=parser.add_subparsers(dest='command',required=True)
    make=sub.add_parser('build');make.add_argument('spec',type=Path);make.add_argument('output',type=Path)
    inspect=sub.add_parser('inspect');inspect.add_argument('state',type=Path)
    patch=sub.add_parser('patch');patch.add_argument('source',type=Path);patch.add_argument('output',type=Path);patch.add_argument('patches',type=Path)
    compare=sub.add_parser('compare');compare.add_argument('first',type=Path);compare.add_argument('second',type=Path)
    args=parser.parse_args()
    if args.command=='build':print(json.dumps(build(json.loads(args.spec.read_text()),args.output),indent=2))
    elif args.command=='inspect':print(json.dumps(inspect_state(args.state),indent=2))
    elif args.command=='compare':print(json.dumps(compare_states(args.first,args.second),indent=2))
    else:patch_state(args.source,args.output,json.loads(args.patches.read_text()))


if __name__=='__main__':main()
