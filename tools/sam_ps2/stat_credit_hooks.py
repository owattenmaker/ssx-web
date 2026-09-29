"""Address-only routing for the original cash-credit/stat-adjustment routine.

150E68 credits cash and increments a stat. Its user-facing purpose is not yet
verified; do not reinterpret it as a conventional downgrade/refund routine.
"""
import struct
from profile_hooks import Code
SITES=[(at,0x530b00+i*4,'stat',(0x24030f88,0x34049b50,0x01831818,0x01642018)) for i,at in enumerate(range(0x150ea8,0x150f99,0x28))]
SITES += [(0x151008,0x530b1c,'credit',(0x01673818,0x00a72821,0x00a32821,0x8cc30004))]

def hook(site,kind,words):
 c=Code()
 for word in words:
  if word&0xfc00003f==0x18:
   rd=(word>>11)&31;c.emit(word&~(31<<11),(rd<<11)|0x12)
  else:c.emit(word)
 c.emit(0x2418001e);c.branch(5,12,24,'done');c.emit(0)
 # T3=profile, T4=character. Keep HI/LO and the leaf function's RA intact.
 c.emit((11<<16)|(24<<11)|(12<<6),(11<<16)|(25<<11)|(7<<6),0x0319c023,
        (11<<16)|(25<<11)|(3<<6),0x0319c021,0x3c190053,0x8f3909fc,0x0319c021)
 if kind=='stat':
  c.emit(0x03041823,0x3c19004a,0x27396ca8,0x00791823)
 else:c.emit(0x03002821)
 c.label('done');c.emit((2<<26)|((site+16)>>2),0)
 return c.bytes()

def patch_words(cell):
 return struct.pack('<4I',0x3c190053,0x8f390000|(cell&65535),0x03200008,0)
