"""Return Sam's icon archive through the existing character-specific getter."""
from profile_hooks import Code
SITE=0x14b700
CELL=0x530b48
PATH_CELL=0x530b4c
PATH=b'data/char/mactxp.big|sam_icons.ssh\0'

def hook(original):
 c=Code();c.emit(0x2418001e);c.branch(5,5,24,'original');c.emit(0)
 c.emit(0x3c020053,0x8c420b4c,0x03e00008,0)
 c.label('original');return c.bytes()+original
