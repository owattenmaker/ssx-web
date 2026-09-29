"""Keep Sam's playable head as the default after any wardrobe initialization."""
import struct
from profile_hooks import Code
SITE=0x151bb4
CELL=0x530b40
RESTORE=(0x7bb00040,0x7bb10030,0x7bb20020,0x7bb30010)

def hook():
 c=Code();c.emit(0x2408001e);c.branch(5,17,8,'restore');c.emit(0)
 c.emit(0x02604021,0x8d090288);c.branch(4,9,0,'restore');c.emit(0)
 c.emit(0x8d0c028c)
 for item,enabled in [(84,False),(86,True)]:
  label=f'skip{item}'
  c.emit(0x852a0000|item*2);c.branch(1,10,0,label);c.emit(0)
  c.emit(0x014c582b);c.branch(4,11,0,label);c.emit(0)
  c.emit(0x000a5080,0x01485021,0x954b0292,
         0x356b0034 if enabled else 0x316bffcb,0xa54b0292)
  c.label(label)
 c.label('restore');c.emit(*RESTORE,(2<<26)|(0x151bc4>>2),0)
 return c.bytes()

def patch():return struct.pack('<4I',0x3c190053,0x8f390b40,0x03200008,0)

RESET_SITE=0x151bd0
RESET_CELL=0x530b44
RESET_WORDS=(0x8c82028c,0x18400010,0x0000302d,0x2408ffef)

def reset_metadata_hook():
 """Correct old Sam default bits only when restoring the default outfit."""
 c=Code();c.emit(0x90880bc0,0x2409001e);c.branch(5,8,9,'original');c.emit(0)
 c.emit(0x8c890288);c.branch(4,9,0,'original');c.emit(0)
 c.emit(0x8c8c028c)
 for item,enabled in [(84,False),(86,True)]:
  label=f'skip{item}'
  c.emit(0x852a0000|item*2);c.branch(1,10,0,label);c.emit(0)
  c.emit(0x014c582b);c.branch(4,11,0,label);c.emit(0)
  c.emit(0x000a5080,0x01445021,0x954b0292,
         0x356b0020 if enabled else 0x316bffdf,0xa54b0292)
  c.label(label)
 c.label('original');c.emit(0x8c82028c,0x00003021)
 c.branch(6,2,0,'empty');c.emit(0x2408ffef)
 c.emit((2<<26)|(0x151be0>>2),0)
 c.label('empty');c.emit((2<<26)|(0x151c18>>2),0)
 return c.bytes()

def reset_patch():return struct.pack('<4I',0x3c190053,0x8f390b44,0x03200008,0)
