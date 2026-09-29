"""Create an isolated diagnostic state from the retained-wardrobe pre-load state.

Reuses that diagnostic build's allocated probe code/buffer. Does not modify the
ISO, original save, cards, or running emulator. Load only with the matching ISO.
"""
from pathlib import Path
import struct,zipfile,sys
from profile_hooks import Code,allocation_probe

ROOT=Path(__file__).resolve().parents[2]
folder=ROOT/'local/sam-ps2/roster/allocation-probe'
source=folder/'retained-before-load.p2s'
lookup='--lookup' in sys.argv
output=folder/('mesh-lookup-before-load.p2s' if lookup else 'mesh-null-before-load.p2s')
with zipfile.ZipFile(source) as z:
 entries={name:z.read(name) for name in z.namelist()}
ram=bytearray(entries['eeMemory.bin'])
def word(at):return struct.unpack_from('<I',ram,at)[0]
assert word(0x14cd74)==0x2a020020
codeptr,buffer=word(0x530a08),word(0x530a0c)
assert 0x100000<codeptr<buffer<0x2000000-160
assert ram[codeptr:codeptr+len(allocation_probe())]==allocation_probe()
assert ram[0x31e818:0x31e828]==struct.pack('<4I',0x3c190053,0x8f390a08,0x03200008,0)
original=bytes.fromhex('70ffbd276000b27f1000b77f2d90a000')
assert ram[0x3ac358:0x3ac368]==original
c=Code();c.emit(0x8cb90080);c.branch(5,25,0,'original');c.emit(0)
c.emit(0x3c190053,0x8f390a0c,0xaf240000,0xaf250004,0xaf3f0008,0xaf3d000c)
for i in range(32):c.emit((35<<26)|(29<<21)|(8<<16)|(i*4),(43<<26)|(25<<21)|(8<<16)|(16+i*4))
c.emit(0x24080001,0xaf280090)
c.label('stop');c.branch(4,0,0,'stop');c.emit(0)
c.label('original');c.emit(*struct.unpack('<4I',original),(2<<26)|(0x3ac368>>2),0)
code=c.bytes();assert len(code)<=buffer-codeptr
target=0x3ac358
if lookup:
 target=0x3ac2fc
 assert ram[target:target+16]==struct.pack('<4I',0x0000102d,0xace20080,0x3c06ffff,0x0100202d)
 c=Code();c.emit(0x3c190053,0x8f390a0c,0xaf280000,0xaf270004,0xaf3f0008,0xaf3d000c)
 for i in range(28):c.emit((35<<26)|(29<<21)|(8<<16)|(i*4),(43<<26)|(25<<21)|(8<<16)|(16+i*4))
 c.emit(0xaf250080,0xaf230084,0xaf220088,0x24080001,0xaf280090)
 c.label('stop');c.branch(4,0,0,'stop');c.emit(0)
 code=c.bytes();assert len(code)<=buffer-codeptr
ram[codeptr:codeptr+len(code)]=code
ram[buffer:buffer+160]=bytes(160)
ram[0x31e818:0x31e828]=bytes.fromhex('60ffbd27ffff023c1000be7fdfff4234')
if lookup:
 # Successful lookups branch to3AC300. Keep that destination and its body
 # intact; only the failed path at3AC2FC jumps to the diagnostic code. Its
 # delay slot replays the original store of the zero resolved model pointer.
 ram[target:target+4]=struct.pack('<I',(2<<26)|(codeptr>>2))
else:
 ram[target:target+16]=struct.pack('<4I',0x3c190053,0x8f390a08,0x03200008,0)
entries['eeMemory.bin']=ram
with zipfile.ZipFile(output,'w',compression=zipfile.ZIP_DEFLATED) as z:
 for name,data in entries.items():z.writestr(name,data)
print(output)
