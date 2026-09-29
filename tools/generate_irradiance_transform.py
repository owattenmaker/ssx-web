#!/usr/bin/env python3
"""Translate the fixed 389840 scalar coefficient transform into native arithmetic.
No instruction decoding or guest memory is used by the generated runtime helper.
Output: engine/generated/irradiance_transform.hpp (git-ignored, like web/generated: lifted from the ELF, so it is
built from your own disc and never published; web/build-core.sh runs this when it is missing).
"""
import hashlib,struct
from pathlib import Path
from inspect_disc import EXPECTED_SHA1
ROOT=Path(__file__).resolve().parents[1]
def generate():
 elf=(ROOT/'local/disc/SLUS_207.72').read_bytes()
 if hashlib.sha1(elf).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected original executable')
 #Original loop pointers, in coefficient rows, indexed by the current lane.
 rows={7:2,4:1,5:3,6:9,9:5,10:4,11:6,8:8,3:7}
 def emit(lo,hi,prefix=False):
  lines=[]
  for pc in range(lo,hi,4):
   word=struct.unpack_from('<I',elf,pc-0xff000)[0];op=word>>26;rs=(word>>21)&31;rt=(word>>16)&31;off=word&65535;off=off if off<32768 else off-65536
   if op in [0x31,0x39]:
    if off%4:raise ValueError('Unaligned scalar operation')
    if rs==29 and 0<=off<64:memory=f'scratch[{off//4}]'
    elif prefix and rs==5 and 0<=off<36:memory=f'matrix[{off//4}]'
    elif not prefix and rs in rows and off==0:memory=f'coefficients[{rows[rs]}][lane]'
    else:raise ValueError(f'Unknown scalar address at{pc:x}')
    line=f'f[{rt}]={memory};' if op==0x31 else f'{memory}=f[{rt}];'
   elif op==0x11 and rs==16:
    ft=rt;fs=(word>>11)&31;fd=(word>>6)&31;fn=word&63
    operation={0:'originalScalarAdd',1:'originalScalarSubtract',2:'terrain_original::mul'}.get(fn)
    if not operation:raise ValueError(f'Unknown scalar op at{pc:x}')
    line=f'f[{fd}]={operation}(f[{fs}],f[{ft}]);'
   elif op in [0,4,5,9,10]:continue #Pointer arithmetic and the source lane loop.
   else:raise ValueError(f'Unknown instruction at{pc:x}: {word:x}')
   lines.append(f' {line} //{pc:06X}')
  return lines
 prefix=emit(0x389884,0x3898bc,True);body=emit(0x3898c0,0x389bfc)
 text=['#pragma once','#include "../irradiance.hpp"','namespace ssx {','//Generated from SHA1-verified389840. Inclusive channel range; constant row retained.',
 'inline void originalIrradianceTransform(OriginalIrradianceCoefficients& coefficients,const std::array<float,9>& matrix,unsigned firstLane=0,unsigned lastLane=2){',
 ' if(firstLane>3||lastLane>3)throw std::runtime_error("Irradiance lane outside coefficient bank");',
 ' terrain_original::Rounding rounding;float f[32]{},scratch[16]{};',*prefix,
 ' for(unsigned lane=firstLane;lane<=lastLane;++lane){',*body,' }','}','}']
 path=ROOT/'engine/generated/irradiance_transform.hpp';path.parent.mkdir(exist_ok=True);path.write_text('\n'.join(text)+'\n');print(path)
if __name__=='__main__':generate()
