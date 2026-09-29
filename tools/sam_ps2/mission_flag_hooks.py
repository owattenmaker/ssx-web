"""Profile-zero per-mission status bits: preserve slot mapping and bit semantics."""
from profile_hooks import Code
CELL=0x530b78
WORDS=(0x24040f88,0x00108080,0x00441018,0x3c03004a)
READERS=[(0x153d78,3),(0x153dd0,1),(0x153e28,0),(0x153e80,2),(0x153ed8,4)]
SETTERS=[(0x154010,0),(0x154080,1),(0x1540f0,2),(0x154160,3),(0x1541d0,4)]
SITES=[address+0x20 for address,_ in READERS]+[address+0x24 for address,_ in SETTERS]

def hook():
 c=Code();c.emit(0x0040c021,WORDS[0],WORDS[1],0x00440018,0x00001012,WORDS[3],0x3b18001e)
 c.branch(5,24,0,'done');c.emit(0)
 c.emit(0x3c190053,0x8f2209fc,0x3c19004a,0x27396ca8,0x00591023)
 c.label('done');c.emit(0x03e00008,0)
 return c.bytes()
