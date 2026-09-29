"""Sam-only inventory access and equipped-flag commit routing."""
from profile_hooks import Code
SITES=[(0x14ada8,0x530b28,'entry',48),(0x14ae28,0x530b2c,'list',56),(0x14aea8,0x530b30,'commit',104)]

def hook(kind,original):
 c=Code();c.emit(0x2418001e);c.branch(5,6,24,'original');c.emit(0)
 c.emit(0x24180f88,0x00b80018,0x0000c012,0x3c190053,0x8f3909fc,0x03381021)
 if kind=='entry':c.emit(0x0007c080,0x00581021,0x03e00008,0x24420290)
 elif kind=='list':c.emit(0x8c45028c,0xace50000,0x03e00008,0x24420290)
 elif kind=='commit':c.emit(0x00402821,(2<<26)|(0x14aec8>>2),0)
 else:raise ValueError(kind)
 c.label('original');return c.bytes()+original


ROUTING_SITES=[
 (0x14acb0,0x530b34,'copy',(0x24020f88,0x34089b50,0x00c21818,0x00a82018)),
 (0x14afb0,0x530b38,'equip',(0x24020f88,0x34039b50,0x00c23018,0x00a32818)),
]

def routing_hook(site,kind,words):
 c=Code()
 if kind=='equip':c.emit(0x00c0c021,0x00a0c821) # retain character/profile before MULT
 for word in words:
  if word&0xfc00003f==0x18:
   rd=(word>>11)&31;c.emit(word&~(31<<11),(rd<<11)|0x12)
  else:c.emit(word)
 char=24 if kind=='equip' else 6
 c.emit(0x2418001e if kind=='copy' else 0x2402001e)
 c.branch(5,char,24 if kind=='copy' else 2,'resume');c.emit(0)
 if kind=='equip':
  c.emit((25<<16)|(6<<11)|(12<<6),(25<<16)|(24<<11)|(7<<6),0x00d83023,
         (25<<16)|(24<<11)|(3<<6),0x00d83021,0x3c190053,0x8f3909fc,
         0x00d93021,0x00c53023,0x3c19004a,0x27396ca8,0x00d93023)
 else:
  c.emit((5<<16)|(24<<11)|(12<<6),(5<<16)|(25<<11)|(7<<6),0x0319c023,
         (5<<16)|(25<<11)|(3<<6),0x0319c021,0x3c190053,0x8f3909fc,
         0x0319c021,0x03041823,0x3c19004a,0x27396ca8,0x00791823)
 c.label('resume');c.emit((2<<26)|((site+16)>>2),0)
 return c.bytes()
