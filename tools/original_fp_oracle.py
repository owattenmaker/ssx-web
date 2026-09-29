#!/usr/bin/env python3
"""Correct selected development oracle copies to PCSX2 EE scalar FP semantics.

Opcode-driven: scalar ADD.S/SUB.S use one guard bit, DIV.S/SQRT.S use nearest,
SQRT reads Ft. VU operations
and RSQRT.S remain untouched. Never edits the original/generated input file.
"""
import json
import re
from pathlib import Path

PREFIX='''// Development oracle: PCSX2 EE DIV.S/SQRT.S nearest; SQRT operandFt.
#include <cfenv>
#include <bit>
#include "ps2_runtime_macros.h"
#pragma STDC FENV_ACCESS ON
static float originalOracleScalarAddSub(float a,float b,bool subtract){
    uint32_t x=std::bit_cast<uint32_t>(a),y=std::bit_cast<uint32_t>(b);
    int shift=int((x>>23)&255)-int((y>>23)&255);
    if(shift>=25)y&=0x80000000u;
    else if(shift<=-25)x&=0x80000000u;
    else if(shift>0)y&=0xffffffffu<<(shift-1);
    else if(shift<0)x&=0xffffffffu<<(-shift-1);
    volatile float left=std::bit_cast<float>(x),right=std::bit_cast<float>(y);
    return subtract?left-right:left+right;
}
static float originalOracleScalarDivide(float a,float b){
    int old=std::fegetround();std::fesetround(FE_TONEAREST);
    volatile float left=a,right=b;float result=left/right;std::fesetround(old);return result;
}
static float originalOracleScalarSqrt(float a){
    int old=std::fegetround();std::fesetround(FE_TONEAREST);
    float result=FPU_SQRT_S(a);std::fesetround(old);return result;
}
'''

def correct_scalar_fp(source):
    if 'originalOracleScalarDivide' in source:raise ValueError('Oracle already corrected')
    matches=list(re.finditer(r'// (0x[0-9a-fA-F]+): (0x[0-9a-fA-F]+)',source));audit=[];changes=[]
    for index,m in enumerate(matches):
        word=int(m[2],16)
        if word>>26!=0x11 or (word>>21)&31!=16 or word&63 not in (0,1,3,4):continue
        fs=(word>>11)&31;ft=(word>>16)&31;fd=(word>>6)&31;kind={0:'ADD.S',1:'SUB.S',3:'DIV.S',4:'SQRT.S'}[word&63]
        end=matches[index+1].start() if index+1<len(matches) else len(source)
        chunk=source[m.start():end]
        if kind in ('ADD.S','SUB.S'):
            operation='ADD' if kind=='ADD.S' else 'SUB'
            pattern=rf'ctx->f\[{fd}\]\s*=\s*FPU_{operation}_S\(ctx->f\[{fs}\],\s*ctx->f\[{ft}\]\)'
            replacement=f'ctx->f[{fd}] = originalOracleScalarAddSub(ctx->f[{fs}],ctx->f[{ft}],{"false" if operation=="ADD" else "true"})'
        elif kind=='DIV.S':
            pattern=rf'ctx->f\[{fd}\]\s*=\s*ctx->f\[{fs}\]\s*/\s*ctx->f\[{ft}\]'
            replacement=f'ctx->f[{fd}] = originalOracleScalarDivide(ctx->f[{fs}],ctx->f[{ft}])'
        else:
            pattern=rf'ctx->f\[{fd}\]\s*=\s*FPU_SQRT_S\(ctx->f\[\d+\]\)'
            replacement=f'ctx->f[{fd}] = originalOracleScalarSqrt(ctx->f[{ft}])'
        corrected,count=re.subn(pattern,replacement,chunk)
        if count!=1:raise ValueError(f'Expected one generated {kind} at {m[1]}, found {count}')
        changes.append((m.start(),end,corrected));audit.append(dict(pc=m[1],opcode=m[2],operation=kind,fs=fs,ft=ft,fd=fd))
    for start,end,chunk in reversed(changes):source=source[:start]+chunk+source[end:]
    return PREFIX+source,audit

def write_scalar_fp_oracle(original,output,source=None):
    original=Path(original);output=Path(output)
    if original.resolve()==output.resolve():raise ValueError('Original/generated file must remain unchanged')
    corrected,audit=correct_scalar_fp(original.read_text() if source is None else source)
    output.parent.mkdir(parents=True,exist_ok=True);output.write_text(corrected)
    output.with_suffix(output.suffix+'.json').write_text(json.dumps(dict(original=str(original.resolve()),
        scalar_policy='PCSX2v2.8.2 EE ADD/SUB single guard bit; DIV/SQRT nearest, SQRT Ft; VU and RSQRT unchanged',operations=audit),indent=2)+'\n')
    return output
