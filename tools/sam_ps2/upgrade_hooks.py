"""Route the existing stat-purchase logic; prices and limits stay in game code."""
import struct
from profile_hooks import Code
SITES=[(0x150c50,0x530a70,'balance',(0x24020f88,0x34049b50,0x02421018,0x02242018))]
SITES += [(at,0x530a74,'stat',(0x24020f88,0x34039b50,0x02421018,0x02231818)) for at in range(0x150ca0,0x150d79,0x24)]
SITES += [(0x150de4,0x530a78,'debit',(0x34029b50,0x02221018,0x02442018,0x26636ca8))]

def hook(kind):
 c=Code();words=next(row[3] for row in SITES if row[2]==kind)
 for word in words:
  if word&0xfc00003f==0x18:
   rd=(word>>11)&31;c.emit(word&~(31<<11),(rd<<11)|0x12)
  else:c.emit(word)
 c.emit(0x2418001e);c.branch(5,18,24,'done');c.emit(0)
 # profile * F88 via shifts: retain the displaced MULT's exact HI/LO.
 c.emit((17<<16)|(24<<11)|(12<<6), (17<<16)|(25<<11)|(7<<6),0x0319c023,
        (17<<16)|(25<<11)|(3<<6),0x0319c021,0x3c190053,0x8f3909fc,0x0319c021)
 dest,other=(4,2) if kind=='debit' else (2,4 if kind=='balance' else 3)
 c.emit((24<<21)|(other<<16)|(dest<<11)|0x23)
 # Remaining original instructions add the original global base.
 c.emit(0x3c19004a,0x27396ca8,(dest<<21)|(25<<16)|(dest<<11)|0x23)
 c.label('done');c.emit(0x03e00008,0)
 return c.bytes()

def patch_words(cell):
 return struct.pack('<4I',0x3c190053,0x8f390000|(cell&65535),0x0320f809,0)


def refresh_after_purchase():
 """Refresh Sam's gameplay cache before the unchanged purchase notification.

 The call site's original delay slot has already stored the new stat byte.
 S1 is the profile index, S2 the character. 147F78 is a no-argument getter.
 """
 c=Code();c.emit(0x2418001e);c.branch(5,18,24,'notify');c.emit(0)
 c.emit(0x3c080053,0x8d0909fc,0x24180f88,0x02380018,0x0000c012,0x01384821,
        0x8d085538,0x24180046,0x02380018,0x0000c012,0x01184021)
 for i in range(7):c.emit(0x912a0000|(0xbdf+i),0xa10a0000|(210+i))
 c.label('notify');c.emit((2<<26)|(0x147f78>>2),0)
 return c.bytes()
