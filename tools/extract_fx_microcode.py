#!/usr/bin/env python3
"""Recover original VIF MPG programs for development-only graphics conformance."""
import argparse,hashlib,json,struct
from pathlib import Path
from inspect_disc import EXPECTED_SHA1

def extract_programs(elf):
    if hashlib.sha1(elf).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected original executable')
    at=struct.unpack_from('<I',elf,32)[0];stride,count,names_index=struct.unpack_from('<3H',elf,46)
    rows=[struct.unpack_from('<10I',elf,at+i*stride) for i in range(count)];r=rows[names_index];names=elf[r[4]:r[4]+r[5]]
    section=next(r for r in rows if names[r[0]:].split(b'\0',1)[0]==b'.vutext');data=elf[section[4]:section[4]+section[5]]
    offset=0;result=[]
    while offset+16<=len(data):
        tag=struct.unpack_from('<I',data,offset)[0]
        if tag>>28!=6:break
        end=offset+16+(tag&65535)*16;at=offset+8;code=bytearray(16384);ranges=[]
        while at<end:
            word=struct.unpack_from('<I',data,at)[0];at+=4;command=(word>>24)&127;number=(word>>16)&255;immediate=word&65535
            if command==0x4a:
                length=(number or 256)*8;start=immediate*8
                if start+length>len(code):raise ValueError('MPG exceeds original micro-memory')
                code[start:start+length]=data[at:at+length];at+=length;ranges.append(dict(start=start,length=length))
            elif command==0x20:at+=4
            elif command in (0x30,0x31):at+=16
            elif command in (0x50,0x51):at+=immediate*16
            elif command>=0x60:
                channels=((command>>2)&3)+1;width=command&3;bits=16 if width==3 else channels*(32>>width);at+=(((number or 256)*bits+31)//32)*4
            elif command not in (0,1,2,3,4,5,6,7,0x10,0x11,0x13,0x14,0x15,0x17):raise ValueError(f'Unsupported VIF command{command:x}')
        if at!=end:raise ValueError('VIF stream does not end at DMA boundary')
        result.append((bytes(code),dict(source_address=section[3]+offset,uploads=ranges)));offset=end
    if len(result)!=6:raise ValueError('Unexpected original program count')
    return result

def export(output):
    output.mkdir(parents=True,exist_ok=True);manifest=[]
    for i,(code,source) in enumerate(extract_programs(Path('local/disc/SLUS_207.72').read_bytes())):
        name=f'program{i}.bin';(output/name).write_bytes(code);manifest.append(dict(file=name,sha256=hashlib.sha256(code).hexdigest(),**source))
    (output/'programs.json').write_text(json.dumps(dict(executable_sha1=EXPECTED_SHA1,role='Development-only original graphics conformance; not shipped with native engine',programs=manifest),indent=2)+'\n')
    return output

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=Path('build/snow-emission-oracle/vu'));args=p.parse_args();print(export(args.output))
