"""Append a Sam UI texture while retaining all original texture bytes/offsets."""
from pathlib import Path
import struct,json
ROOT=Path(__file__).resolve().parents[2];folder=ROOT/'local/sam-ps2/roster'
old=(ROOT/'local/sam-ps2/original/FE_1.SSH').read_bytes()
extra=(folder/'sam-roster-atlas.ssh').read_bytes()
assert old[:4]==extra[:4]==b'SHPS'
count=struct.unpack_from('<I',old,8)[0];assert count==22 and struct.unpack_from('<I',extra,8)[0]==1
offsets=[struct.unpack_from('<I',old,20+8*i)[0] for i in range(count)]
first=min(offsets);table_end=16+8*count
footer=old[table_end:old.index(0,table_end)+1]
assert table_end+8+len(footer)<=first
start=struct.unpack_from('<I',extra,20)[0]
new=bytearray(old);new+=bytes((-len(new))%16);added_offset=len(new);new+=extra[start:]
new[table_end:first]=bytes(first-table_end)
struct.pack_into('<4sI',new,table_end,b'samr',added_offset)
new[table_end+8:table_end+8+len(footer)]=footer
struct.pack_into('<I',new,4,len(new));struct.pack_into('<I',new,8,count+1)
assert new[first:len(old)]==old[first:]
assert new[16:table_end]==old[16:table_end]
(folder/'FE_1-roster.SSH').write_bytes(new)
print('Appended texture22; all22originaltextureblocks and offsets preserved; bytes',len(new))
