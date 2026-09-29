"""Lossless SSX3 LUI screen records, verified against the original PS2 FE.LUI.
Names are game hashes. Unknown record payload bytes are retained verbatim.
"""
from dataclasses import dataclass
import struct

def records(data,at,count,end):
 result=[]
 for _ in range(count):
  if at+4>end:raise ValueError('Truncated LUI record')
  _,size=struct.unpack_from('<HH',data,at)
  if size<4 or at+size>end:raise ValueError('Invalid LUI record size')
  result.append(data[at:at+size]);at+=size
 if at!=end:raise ValueError('Unaccounted LUI record bytes')
 return result

@dataclass
class State:
 name:int
 flags:int # Legacy name: on-disk timeline frame number (returned by39C778).
 records:list

@dataclass
class Screen:
 header:bytes
 definitions:list
 states:list
 @classmethod
 def decode(cls,data):
  if len(data)<24:raise ValueError('Short LUI screen')
  split=struct.unpack_from('<I',data,8)[0]
  if not 20<=split<=len(data)-4:raise ValueError('Invalid state offset')
  count=struct.unpack_from('<I',data,16)[0]
  definitions=records(data,20,count,split)
  count=struct.unpack_from('<I',data,split)[0];at=split+4;states=[]
  for _ in range(count):
   if at+12>len(data):raise ValueError('Short state header')
   name,size,flags=struct.unpack_from('<III',data,at)
   if size<12 or at+size>len(data):raise ValueError('Invalid state size')
   states.append(State(name,flags&65535,records(data,at+12,flags>>16,at+size)))
   at+=size
  if at!=len(data):raise ValueError('Trailing screen bytes')
  return cls(data[:16],definitions,states)
 def encode(self):
  definitions=b''.join(self.definitions);header=bytearray(self.header)
  struct.pack_into('<I',header,8,20+len(definitions))
  result=header+struct.pack('<I',len(self.definitions))+definitions+struct.pack('<I',len(self.states))
  for s in self.states:
   if len(s.records)>65535:raise ValueError('Too many state records')
   payload=b''.join(s.records)
   result+=struct.pack('<III',s.name,len(payload)+12,(len(s.records)<<16)|s.flags)+payload
  return bytes(result)
