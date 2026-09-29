#!/usr/bin/env python3
"""Local PCSX2 PINE client for reference-state inspection and guarded RAM probes.

Protocol verified against PCSX2 v2.8.2 PINE.cpp. No process-memory scraping or
UI input injection is used. Enable PINE in PCSX2's Advanced settings first.
"""
import argparse
import json
import os
import socket
import sys
import struct
from pathlib import Path


class Pine:
    def __init__(self,path=None,timeout=5):
        self.path=str(path or Path(os.environ.get('TMPDIR' if sys.platform=='darwin' else 'XDG_RUNTIME_DIR','/tmp'))/'pcsx2.sock')
        self.socket=socket.socket(socket.AF_UNIX,socket.SOCK_STREAM)
        self.socket.settimeout(timeout)
        try:self.socket.connect(self.path)
        except BaseException:self.socket.close();raise

    def close(self):self.socket.close()
    def __enter__(self):return self
    def __exit__(self,*args):self.close()

    def receive(self,size):
        data=bytearray()
        while len(data)<size:
            part=self.socket.recv(size-len(data))
            if not part:raise ConnectionError('PINE connection closed')
            data.extend(part)
        return bytes(data)

    def request(self,commands):
        if len(commands)+4>650000:raise ValueError('PINE request exceeds protocol limit')
        self.socket.sendall(struct.pack('<I',len(commands)+4)+commands)
        size,=struct.unpack('<I',self.receive(4))
        if not 5<=size<=450000:raise ValueError('Invalid PINE response size')
        data=self.receive(size-4)
        if data[0]!=0:raise RuntimeError('PCSX2 rejected the PINE request')
        return data[1:]

    def status(self):
        value,=struct.unpack('<I',self.request(b'\x0f'))
        return {0:'running',1:'paused',2:'shutdown'}.get(value,f'unknown:{value}')

    def info(self):
        data=self.request(bytes((8,11,12,13,14)));at=0;result={}
        for key in ('emulator','title','game_id','game_crc','game_version'):
            if at+4>len(data):raise ValueError('Truncated PINE string length')
            size,=struct.unpack_from('<I',data,at);at+=4
            if not size or at+size>len(data):raise ValueError('Truncated PINE string')
            result[key]=data[at:at+size].rstrip(b'\0').decode('utf-8');at+=size
        result['status']=self.status()
        return result

    def read(self,address,size):
        if address<0 or size<0 or address+size>32*1024*1024:raise ValueError('Probe outside EE RAM')
        output=bytearray()
        while size:
            count=min(size,256*1024);wide=count//8;tail=count%8
            command=b''.join(b'\x03'+struct.pack('<I',address+i*8) for i in range(wide))
            command+=b''.join(b'\x00'+struct.pack('<I',address+wide*8+i) for i in range(tail))
            data=self.request(command)
            if len(data)!=count:raise ValueError('PINE memory response size mismatch')
            output.extend(data);address+=count;size-=count
        return bytes(output)

    def guarded_write(self,address,expected,replacement):
        if len(expected)!=len(replacement):raise ValueError('RAM replacement must preserve size')
        if self.status()!='paused':raise ValueError('Pause PCSX2 before applying a RAM patch')
        if self.info()['game_id'] not in ('SLUS-20772','SLUS-207.72','SLUS_207.72'):
            raise ValueError('This patcher only targets SSX3 USA')
        if self.read(address,len(expected))!=expected:raise ValueError('RAM patch expected bytes do not match')
        self.request(b''.join(b'\x04'+struct.pack('<I',address+i)+bytes([v]) for i,v in enumerate(replacement)))
        if self.read(address,len(replacement))!=replacement:raise ValueError('RAM patch verification failed')


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--socket',type=Path)
    sub=parser.add_subparsers(dest='command',required=True)
    sub.add_parser('info')
    read=sub.add_parser('read');read.add_argument('address',type=lambda s:int(s,0));read.add_argument('size',type=lambda s:int(s,0));read.add_argument('--output',type=Path)
    args=parser.parse_args()
    with Pine(args.socket) as client:
        if args.command=='info':print(json.dumps(client.info(),indent=2))
        else:
            data=client.read(args.address,args.size)
            if args.output:args.output.write_bytes(data)
            else:print(data.hex(' '))


if __name__=='__main__':main()
