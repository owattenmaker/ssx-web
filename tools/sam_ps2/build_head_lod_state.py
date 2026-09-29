"""Replay the head-LOD asset-only change before loading the course.
The executable is identical across these two discs. Only copy existing name
pointers for the exact rows whose on-disc LOD fields changed.
"""
from pathlib import Path
import struct,zipfile,sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from inspect_disc import Disc
from extend_wardrobe import parse,strings
ROOT=Path(__file__).resolve().parents[2];folder=ROOT/'local/sam-ps2/roster'
old_disc=Disc(folder/'sam-name-budget-v3-test.iso');new_disc=Disc(folder/'sam-head-lod-test.iso')
assert old_disc.file('SLUS_207.72')==new_disc.file('SLUS_207.72')
old=parse(old_disc.file('DATA/CHAR/BOLTPS2.DAT'));new=parse(new_disc.file('DATA/CHAR/BOLTPS2.DAT'))
changes={}
for a,b in zip(old[1],new[1]):
 sa,sb=strings(a,old[3]),strings(b,new[3])
 assert a[:20]+a[52:]==b[:20]+b[52:]
 if sa==sb:continue
 assert a[0]==30 and sa[0:2]==sb[0:2] and sa[5:]==sb[5:]
 assert sa[2:5]==[None]*3 and sb[2:5]==[sa[1]]*3
 changes[struct.unpack_from('<h',a,4)[0]]=sa[1]
with zipfile.ZipFile(folder/'mesh-probe/name-budget-before-load.p2s') as z:entries={n:z.read(n) for n in z.namelist()}
ram=bytearray(entries['eeMemory.bin'])
def word(a):return struct.unpack_from('<I',ram,a)[0]
assert ram[0x535b31]==30
db=word(0x4a6750);rows=word(db+4);count=word(db+0x1c);assert count==len(old[1])
patched=[]
for i in range(count):
 at=rows+i*56;item=struct.unpack_from('<h',ram,at+4)[0]
 if ram[at]!=30 or item not in changes:continue
 ptr=word(at+24);assert ram[ptr:ram.index(0,ptr)]==changes[item]
 assert ram[at+28:at+40]==bytes(12)
 ram[at+28:at+40]=struct.pack('<3I',ptr,ptr,ptr);patched.append(item)
assert set(patched)==set(changes)
entries['eeMemory.bin']=ram
output=folder/'mesh-probe/head-lod-before-load.p2s'
with zipfile.ZipFile(output,'w',compression=zipfile.ZIP_DEFLATED) as z:
 for n,b in entries.items():z.writestr(n,b)
print('Verified executable identity and patched only head LOD pointers:',patched)
print(output)
