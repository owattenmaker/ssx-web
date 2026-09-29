"""Append hashed UTF-16 entries while preserving original LOC text IDs/bytes."""
import struct

def name_hash(name):
 h=0
 for c in name.encode('ascii'):
  h=((h<<4)+c)&0xffffffff;g=h&0xf0000000
  if g:h^=(g>>23)^g
 return h

def entries(data):
 assert data[:4]==b'LOCH' and data[20:24]==b'LOCT'
 at=struct.unpack_from('<I',data,16)[0];assert data[at:at+4]==b'LOCL'
 assert (at-36)%8==0
 hashes=[struct.unpack_from('<II',data,p) for p in range(36,at,8)]
 size,_,count=struct.unpack_from('<III',data,at+4);assert at+size==len(data)
 offsets=struct.unpack_from('<%dI'%count,data,at+16)
 texts=[]
 for offset in offsets:
  p=at+offset;end=p
  while data[end:end+2]!=b'\0\0':
   end+=2
   if end+2>len(data):raise ValueError('Unterminated locale text')
  texts.append(data[p:end].decode('utf-16le'))
 return {key:texts[i] for key,i in hashes}

def append(data,key,text):
 old=entries(data);assert key not in old
 at=struct.unpack_from('<I',data,16)[0]
 hashes=[struct.unpack_from('<II',data,p) for p in range(36,at,8)]
 size,unknown,count=struct.unpack_from('<III',data,at+4)
 offsets=list(struct.unpack_from('<%dI'%count,data,at+16))
 body=data[at+16+4*count:];encoded=text.encode('utf-16le')+b'\0\0\0\0'
 hashes.append((key,count));hashes.sort();assert len({k for k,i in hashes})==len(hashes)
 result=bytearray(data[:36]);struct.pack_into('<I',result,16,at+8)
 result+=b''.join(struct.pack('<II',*e) for e in hashes)
 result+=b'LOCL'+struct.pack('<III',size+4+len(encoded),unknown,count+1)
 result+=struct.pack('<%dI'%(count+1),*[x+4 for x in offsets],size+4)+body+encoded
 decoded=entries(result);assert all(decoded[k]==v for k,v in old.items());assert decoded[key]==text
 return bytes(result)
