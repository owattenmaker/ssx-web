"""Include Sam in the existing three-profile equipment-flag reset loop."""
import struct
from profile_hooks import Code
SITE=0x14af38
CELL=0x530b3c
WORDS=(0x24670001,0x008c1021,0x01221021,0x8c43028c)

def hook():
 c=Code();c.emit(*WORDS[:3],0x2418000b)
 # A3 is loop index+1; the new eleventh iteration represents Sam, not ID10.
 c.branch(5,7,24,'load');c.emit(0)
 c.emit(0x2558ffff,(24<<16)|(25<<11)|(12<<6),(24<<16)|(24<<11)|(7<<6),0x0338c823,
        (10<<16)|(24<<11)|(3<<6),0x2718fff8,0x0338c821,0x3c180053,0x8f1809fc,0x03191021)
 c.label('load');c.emit(WORDS[3],(2<<26)|(0x14af48>>2),0)
 return c.bytes()

def patch():return struct.pack('<4I',0x3c190053,0x8f390b3c,0x03200008,0)
