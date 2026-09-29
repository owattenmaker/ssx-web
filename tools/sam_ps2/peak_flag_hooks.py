"""Route peak-access bit reads/writes to Sam without changing flag semantics."""
from profile_hooks import Code
READ_SITE=0x145f90
READ_CELL=0x530b50
WRITES=[
 (0x1460cc,0x530b54,18,5,(0x24030f88,0x34029b50,0x00a31818,0x02421018)),
 (0x1460f4,0x530b58,18,6,(0x24030f88,0x34029b50,0x00c31818,0x02421018)),
 (0x150bac,0x530b5c,4,6,(0x24030f88,0x34029b50,0x00821018,0x00c31818)),
 (0x150be0,0x530b60,15,6,(0x34029b50,0x00821018,0x00c31818,0x3c04004a)),
]

def reader(original):
 c=Code();c.emit(0x2418001e);c.branch(5,6,24,'original');c.emit(0)
 c.branch(4,7,0,'zero');c.emit(0)
 c.emit(0x24180f88,0x00b80018,0x0000c012,0x3c190053,0x8f3909fc,
        0x0338c821,0x8f220278,0x24180001)
 c.branch(5,7,24,'second');c.emit(0)
 c.emit(0x00021302,0x03e00008,0x30420001) # low bit12
 c.label('second');c.emit(0x00021342,0x03e00008,0x30420001) # low bit13
 c.label('zero');c.emit(0x03e00008,0x00001021)
 c.label('original');return c.bytes()+original

def writer(site,profile,character,words):
 c=Code()
 if site==0x150be0:c.emit(0x00807821) # retain profile before displaced LUI overwrites A0
 for w in words:
  if w&0xfc00003f==0x18:
   rd=(w>>11)&31;c.emit(w&~(31<<11),(rd<<11)|0x12)
  else:c.emit(w)
 c.emit(0x2418001e);c.branch(5,character,24,'resume');c.emit(0)
 c.emit((profile<<16)|(24<<11)|(12<<6),(profile<<16)|(25<<11)|(7<<6),0x0319c023,
        (profile<<16)|(25<<11)|(3<<6),0x0319c021,0x3c190053,0x8f3909fc,
        0x0319c021,0x03021823,0x3c19004a,0x27396ca8,0x00791823)
 c.label('resume');c.emit((2<<26)|((site+16)>>2),0)
 return c.bytes()
