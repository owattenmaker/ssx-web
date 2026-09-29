#!/usr/bin/env python3
"""Static native translation of the two leaf float kernels used by2EDF00.
Only build-time decoding; output contains direct arithmetic and fixed branches.
Output: engine/generated/lighting_math.hpp (git-ignored, like web/generated: lifted from the ELF, so it is built from
your own disc and never published; web/build-core.sh runs this when it is missing).
"""
from pathlib import Path
import struct,hashlib
from inspect_disc import EXPECTED_SHA1
ROOT=Path(__file__).resolve().parents[1]
def generate():
 elf=(ROOT/'local/disc/SLUS_207.72').read_bytes()
 if hashlib.sha1(elf).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected math executable')
 def word(pc):return struct.unpack_from('<I',elf,pc-0xff000)[0]
 def instruction(pc):
  w=word(pc);op=w>>26;rs=w>>21&31;rt=w>>16&31;rd=w>>11&31;sa=w>>6&31;fn=w&63;imm=w&65535;si=imm if imm<32768 else imm-65536
  r=lambda i:'0u' if i==0 else f'r[{i}]'
  def assign(i,value):return '' if i==0 else f'r[{i}]={value};'
  if w==0:return ''
  if op==0:
   if fn in [0,2,3]:return assign(rd,[f'{r(rt)}<<{sa}',f'{r(rt)}>>{sa}',f'uint32_t(int32_t({r(rt)})>>{sa})'][[0,2,3].index(fn)])
   if fn in [0x21,0x2d]:return assign(rd,f'{r(rs)}+{r(rt)}')
   if fn==0x23:return assign(rd,f'{r(rs)}-{r(rt)}')
   if fn in [0x24,0x25,0x26]:return assign(rd,f'{r(rs)}'+{0x24:'&',0x25:'|',0x26:'^'}[fn]+r(rt))
   if fn==0x2a:return assign(rd,f'int32_t({r(rs)})<int32_t({r(rt)})')
   if fn==0x2b:return assign(rd,f'{r(rs)}<{r(rt)}')
  if op==9:return assign(rt,f'{r(rs)}+uint32_t({si})')
  if op==10:return assign(rt,f'int32_t({r(rs)})<{si}')
  if op==15:return assign(rt,f'0x{imm<<16:08x}u')
  if op in [12,13,14]:return assign(rt,f'{r(rs)}'+{12:'&',13:'|',14:'^'}[op]+f'{imm}u')
  if op==0x31:return f'f[{rt}]=lightingMathConstant({r(rs)}+uint32_t({si}));'
  if op==17:
   if rs==0:return assign(rt,f'std::bit_cast<uint32_t>(f[{rd}])')
   if rs==4:return f'f[{rd}]=std::bit_cast<float>({r(rt)});'
   fd=sa;fs=rd;ft=rt
   if rs==20 and fn==32:return f'f[{fd}]=float(std::bit_cast<int32_t>(f[{fs}]));'
   if rs==16:
    if fn in [0,1,2,3]:return f'f[{fd}]='+{0:'originalScalarAdd',1:'originalScalarSubtract',2:'terrain_original::mul',3:'originalScalarDivide'}[fn]+f'(f[{fs}],f[{ft}]);'
    if fn==6:return f'f[{fd}]=f[{fs}];'
    if fn==36:return f'f[{fd}]=std::bit_cast<float>(int32_t(f[{fs}]));'
    if fn in [50,52,54,60,62]:return f'condition=f[{fs}]'+{50:'==',52:'<',54:'<=',60:'<',62:'<='}[fn]+f'f[{ft}];'
  raise ValueError(f'Unsupported instruction {pc:x}: {w:x}')
 def function(name,lo,hi):
  lines=[f'inline float {name}(float input){{',' terrain_original::Rounding rounding;uint32_t r[32]{};float f[32]{};bool condition=false;f[12]=input;']
  for pc in range(lo,hi,4):
   w=word(pc);op=w>>26;rs=w>>21&31;rt=w>>16&31;imm=w&65535;si=imm if imm<32768 else imm-65536
   lines.append(f'L{pc:x}: //{w:08X}')
   if op==0 and w&63==8:
    if rs!=31:raise ValueError('Unexpected indirect branch')
    lines+=[instruction(pc+4),' return f[0];'];continue
   branch=None;likely=False
   if op in [4,5,20,21]:
    branch=('true' if op in [4,20] else 'false') if rs==rt else f'r[{rs}]'+('==' if op in [4,20] else '!=')+f'r[{rt}]';likely=op>=20
   elif op==1 and rt==1:branch=f'int32_t(r[{rs}])>=0'
   elif op==6:branch=f'int32_t(r[{rs}])<=0'
   elif op==17 and rs==8:branch='condition' if rt&1 else '!condition';likely=bool(rt&2)
   if branch is not None:
    target=pc+4+si*4
    if not lo<=target<hi:raise ValueError('Branch outside kernel')
    delay=instruction(pc+4)
    if likely:lines.append(f' if({branch}){{{delay}goto L{target:x};}} goto L{pc+8:x};')
    else:lines.append(f' {{bool taken={branch};{delay}if(taken)goto L{target:x};}} goto L{pc+8:x};')
   else:lines.append(' '+instruction(pc))
  lines+=[' throw std::runtime_error("Unexpected lighting math fallthrough");','}'];return lines
 constants=struct.unpack_from('<80I',elf,0x499100-0xff000)
 lines=['#pragma once','#include "../terrain_contact_math.hpp"','namespace ssx {',
 'inline float lightingMathConstant(uint32_t address){',
 ' static constexpr std::array<uint32_t,80> values={'+','.join(hex(v)+'u' for v in constants)+'};',
 ' if(address<0x499100u||address>=0x499240u||(address&3))throw std::runtime_error("Lighting math constant outside pool");',
 ' return std::bit_cast<float>(values[(address-0x499100u)/4]);','}',
 *function('originalLightingLog',0x40e550,0x40e878),*function('originalLightingExp',0x40e048,0x40e300),'}']
 (ROOT/'engine/generated').mkdir(exist_ok=True);(ROOT/'engine/generated/lighting_math.hpp').write_text('\n'.join(lines)+'\n')
if __name__=='__main__':generate()
