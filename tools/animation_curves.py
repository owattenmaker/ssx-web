"""SSX3 AFB scalar-curve investigation. Layout is validated against full banks.

The rotation representation, sampling cadence and playback semantics are still
being validated. Keep this decoder offline until that validation is complete.
"""
import math
import struct
from collections import Counter


class CurvePacket:
    def __init__(self,raw,frames):
        if not raw or frames<=0:raise ValueError('Empty AFB curve packet')
        self.raw,self.frames=raw,frames
        self.dofs=raw[0]
        if not self.dofs or 1+(self.dofs+1)//2>len(raw):raise ValueError('Truncated AFB type table')
        self.types=[(raw[1+i//2]>>(4 if i%2==0 else 0))&15 for i in range(self.dofs)]
        self.params=[]
        at=1+(self.dofs+1)//2
        def floating():
            nonlocal at
            if at+3>len(raw):raise ValueError('Truncated AFB coefficient')
            value=struct.unpack('<f',b'\0'+raw[at:at+3])[0];at+=3
            if not math.isfinite(value):raise ValueError('Nonfinite AFB coefficient')
            return value
        for kind in self.types:
            if kind>8:raise ValueError('Unsupported AFB curve encoding')
            count=kind if kind<=4 else 3 if kind==5 else 2
            params=[floating() for _ in range(count)]
            if kind==5:
                if at+2>len(raw):raise ValueError('Truncated AFB breakpoint')
                params.append(int.from_bytes(raw[at:at+2],'little'));at+=2
            self.params.append(params)
        self.counts=Counter(self.types)
        self.samples_at=at
        n=self.frames
        self.sizes={6:n*self.counts[6],7:(n//2+1)*self.counts[7],8:n*2*self.counts[8]}
        self.offsets={6:at,7:at+self.sizes[6]}
        word_at=self.offsets[7]+self.sizes[7]
        self.offsets[8]=(word_at+1)&~1 if self.sizes[8] else word_at
        self.consumed=self.offsets[8]+self.sizes[8]
        if not 0<=len(raw)-self.consumed<=3:raise ValueError('AFB packet extent mismatch')

    def sample(self,frame):
        frame=max(0,min(self.frames-1,float(frame)))
        used=Counter();result=[]
        for kind,p in zip(self.types,self.params):
            if kind==0:value=0
            elif kind<=4:
                value=0
                for coefficient in p:value=value*frame+coefficient
            elif kind==5:
                a,b,c,split=p
                value=a*frame+b if frame<=split else a*split+b+c*(frame-split)
            else:
                j=used[kind];used[kind]+=1
                if kind==7:
                    whole=int(frame*.5);fraction=frame*.5-whole
                    index=whole*self.counts[7]+j
                    quantized=self.raw[self.offsets[7]+index]
                    if fraction:
                        following=self.raw[self.offsets[7]+index+self.counts[7]]
                        quantized+=(following-quantized)*fraction
                else:
                    whole=int(frame);fraction=frame-whole
                    index=whole*self.counts[kind]+j
                    at=self.offsets[kind]+index*(2 if kind==8 else 1)
                    quantized=self.raw[at] if kind==6 else int.from_bytes(self.raw[at:at+2],'big')
                    if fraction:
                        at+=self.counts[kind]*(2 if kind==8 else 1)
                        following=self.raw[at] if kind==6 else int.from_bytes(self.raw[at:at+2],'big')
                        quantized+=(following-quantized)*fraction
                value=p[0]+p[1]*quantized
            result.append(value)
        return result
