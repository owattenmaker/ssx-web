"""Correct the seven active-profile raw stat reads used by Buy Attributes."""
import struct
from profile_hooks import Code
SITES=range(0x1481a0,0x148291,0x28)
WORDS=(0x24030f88,0x34049b50,0x00e31818,0x02042018)
CELL=0x530b20

def hook():
 c=Code();c.emit(WORDS[0],WORDS[1],0x00e30018,0x00001812,0x02040018,0x00002012)
 c.emit(0x2418001e);c.branch(5,7,24,'done');c.emit(0)
 c.emit((16<<16)|(24<<11)|(12<<6),(16<<16)|(25<<11)|(7<<6),0x0319c023,
        (16<<16)|(25<<11)|(3<<6),0x0319c021,0x3c190053,0x8f3909fc,
        0x0319c021,0x03041823,0x3c19004a,0x27396ca8,0x00791823)
 c.label('done');c.emit(0x03e00008,0)
 return c.bytes()

def patch():return struct.pack('<4I',0x3c190053,0x8f390b20,0x0320f809,0)
