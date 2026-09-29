"""MIPS hooks for independently allocated Sam profile records."""
import struct
RECORD_SIZE=0xf88
PROFILE_COUNT=3
PROFILE_BYTES=RECORD_SIZE*PROFILE_COUNT
PROFILE_CELL=0x5309fc
GETTER_CELL=0x5309f8
INIT_CELL=0x530a00
class Code:
 def __init__(self):self.words=[];self.labels={};self.fixups=[]
 def emit(self,*words):self.words.extend(words)
 def label(self,name):self.labels[name]=len(self.words)
 def branch(self,op,rs,rt,label):self.fixups.append((len(self.words),label));self.emit((op<<26)|(rs<<21)|(rt<<16))
 def bytes(self):
  words=self.words.copy()
  for at,label in self.fixups:words[at]|=(self.labels[label]-at-1)&65535
  return struct.pack('<%dI'%len(words),*words)
def getter():
 c=Code();c.emit(0x2408001e);c.branch(5,6,8,'original');c.emit(0x24030f88)
 c.emit(0x2ca80003);c.branch(5,8,0,'sam');c.emit(0,0x0000000d)
 c.label('sam');c.emit(0x3c020053,0x8c4209fc,0x00a30018,0x00001812,0x03e00008,0x00431021)
 c.label('original');c.emit(0x34049b50,0x00c30018,0x00001012,0x00a40018,0x00002812,0x3c03004a,0x24636ca8,0x00431021,0x03e00008,0x00451021)
 return c.bytes()
def initialize_records(save_extension=False):
 c=Code();c.emit(0x27bdffe0,0xafbf0010,0x3c080053,0x8d0809fc,0xafa80014,0x24080003,0xafa80018)
 c.label('loop');c.emit(0x8fa40014,0x24050003,(3<<26)|(0x151600>>2),0x24060001)
 c.emit(0x8fa40014,(3<<26)|(0x151a88>>2),0x2405001e)
 # Use the ordinary gameplay head as Sam's default. The Mac-compatible reset
 # otherwise equips the cinematic-only item84, which disappears when riding.
 c.emit(0x8fa80014,0x8d090288)
 for item,on in [(84,False),(86,True)]:
  c.emit(0x852a0000|item*2);c.branch(1,10,0,f'head_{item}_done');c.emit(0)
  c.emit(0x000a5080,0x01485021,0x954b0292,0x356b0014 if on else 0x316bffeb,0xa54b0292)
  c.label(f'head_{item}_done')
 c.emit(0x8fa80014,0x8fa90018,0x25080f88,0x2529ffff,0xafa80014)
 c.branch(5,9,0,'loop');c.emit(0xafa90018)
 if save_extension:
  # Capture one immutable baseline for legacy saves, without owning its lookup.
  c.emit(0x3c080053,0x8d0909fc,0x8d0a0a38,0x240b03e2)
  c.label('default_copy');c.emit(0x8d2c0000,0xad4c0000,0x25290004,0x256bffff)
  c.branch(5,11,0,'default_copy');c.emit(0x254a0004)
  c.emit(0x8d0a0a38,0xad400288)
 c.emit(0x8fbf0010,0x27bd0020)
 # Replay displaced BE initializer instructions before returning to149DB0.
 c.emit(0x26620600,0xae600600,0x240500c8,0x03e00008,0x24030190)
 return c.bytes()

def effect_filter():
 c=Code();c.emit(0x8fa60044,0x24b50001,0x28c2000a)
 c.branch(5,2,0,'check');c.emit(0,0x38c2001e)
 c.branch(5,2,0,'bonus');c.emit(0)
 c.label('check');c.emit((2<<26)|(0x2ece54>>2),0)
 c.label('bonus');c.emit((2<<26)|(0x2ece64>>2),0x00001021)
 return c.bytes()

def allocation_probe():
 """Diagnostic only: stop impossible allocations before allocator corruption."""
 c=Code();c.emit(0x3c190200,0x00b9c82b)
 c.branch(5,25,0,'original');c.emit(0)
 c.emit(0x3c190053,0x8f390a0c,0xaf240000,0xaf250004,0xaf3f0008,0xaf3d000c)
 # Retain a small caller stack window for return-address reconstruction.
 for i in range(32):c.emit((35<<26)|(29<<21)|(8<<16)|(i*4),(43<<26)|(25<<21)|(8<<16)|(16+i*4))
 c.emit(0x24080001,0xaf280090)
 c.label('stop');c.branch(4,0,0,'stop');c.emit(0)
 c.label('original');c.emit(0x27bdff60,0x3c02ffff,0x7fbe0010,0x3442ffdf,(2<<26)|(0x31e828>>2),0)
 return c.bytes()

def mesh_probe():
 """Stop a missing compiled-model reference at the first observed failure."""
 c=Code();c.emit(0x8cb90080);c.branch(5,25,0,'original');c.emit(0)
 c.emit(0x3c190053,0x8f390a0c,0xaf240000,0xaf250004,0xaf3f0008,0xaf3d000c)
 for i in range(32):c.emit((35<<26)|(29<<21)|(8<<16)|(i*4),(43<<26)|(25<<21)|(8<<16)|(16+i*4))
 c.emit(0x24080001,0xaf280090)
 c.label('stop');c.branch(4,0,0,'stop');c.emit(0)
 c.label('original');c.emit(0x27bdff70,0x7fb20060,0x7fb70010,0x00a0902d,(2<<26)|(0x3ac368>>2),0)
 return c.bytes()

def wardrobe_name_budget(save_extension=False):
 """Sam's primary-style name strings also need space in the retention pass."""
 c=Code()
 if save_extension:
  c.emit(0x3c180015,0x37180e18);c.branch(4,31,24,'purchase_refresh');c.emit(0)
 # Share the guarded trampoline with two submenu draw predicates. Their
 # return addresses identify the callers; both hold the model in s6.
 for return_address in [0x182f10,0x184cb8]:
  c.emit(0x3c180000|(return_address>>16),0x37180000|(return_address&65535))
  c.branch(4,31,24,'preview');c.emit(0)
 c.emit(0x3c180019,0x3718c048);c.branch(4,31,24,'gear_preview');c.emit(0)
 c.emit(0x3c18002c,0x37185b08);c.branch(4,31,24,'save_size');c.emit(0)
 c.emit(0x3c18002c,0x37185da4);c.branch(4,31,24,'save_read_count');c.emit(0)
 c.emit(0x27bdfff0,0xafbf0000,0x92480000,0x2409001e)
 c.branch(5,8,9,'done');c.emit(0,0x8e480034,0x31080f00)
 c.branch(4,8,0,'done');c.emit(0,0x8e440014)
 c.branch(4,4,0,'done');c.emit(0,(3<<26)|(0x416810>>2),0,0x24420001,0x0282a021)
 c.label('done');c.emit(0x8fbf0000,0x27bd0010,0x03e00008,0x27d50001)
 c.label('gear_preview');c.emit(0x8e820000);c.branch(4,0,0,'preview_predicate');c.emit(0)
 c.label('preview');c.emit(0x8ec20000)
 c.label('preview_predicate');c.emit(0x2c58000a,0x3842001e,0x2c420001,0x00581025,0x03e00008,0)
 c.label('save_read_count');c.emit(0x8f830850,0x8e220020)
 if save_extension:
  c.emit(0x3418ab0c);c.branch(5,2,24,'size_check');c.emit(0)
  c.emit(0x3c180053,0xaf030a30)
 c.branch(4,0,0,'size_check');c.emit(0)
 c.label('save_size');c.emit(0x8e230490,0x8e220024)
 c.label('size_check');c.emit(0x3418ab0c)
 c.branch(5,2,24,'size_done');c.emit(0,0x34189b61)
 c.branch(5,3,24,'size_done');c.emit(0,0x00601021)
 c.label('size_done');c.emit(0x03e00008,0)
 if save_extension:
  c.label('purchase_refresh');c.emit(0x3c190053,0x8f390b24,0x03200008,0)
 return c.bytes()
