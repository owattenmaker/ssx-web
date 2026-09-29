"""Stage a heap-backed PS2 character database and guarded executable patches.
Not installed: save/equipment bounds and runtime lifetime still require auditing.
The loader's own allocation contains a small MIPS initializer followed by data;
no guessed free RAM address or new executable segment is used.
"""
import hashlib,json,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from inspect_disc import Disc
ROOT=Path(__file__).resolve().parents[2]
BASE=0x530970
FIELDS=(0,32,48,76,100,116)
DATA_OFFSET=64
SAM_ID=30
SHA1='77114dfd1205eaccf1ccc18c5f9650097fa78bd8'

def initializer(data_offset=DATA_OFFSET,stats_offset=None,extra_pointers=(),bolt_name_offset=None,profile_name_offset=None,probe=False,relocations=()):
 words=[0x3c040053] # lui a0,0x53
 if stats_offset is not None:
  # Preserve legacy cache bytes on first initialization; subsequent loads
  # copy the previously expanded cache, including independent Sam values.
  words += [0x24865538,0x8c880970,0x8c895538,0x25081078,
            0x15090003,0x240a00d2,0x01203021,0x240a01a4,
            (9<<26)|(16<<21)|(5<<16)|stats_offset,0x00ca5021,
            0x90c20000,0xa0a20000,0x24c60001,0x24a50001,
            0x14cafffb,0]
 for field in FIELDS:
  words += [(9<<26)|(16<<21)|(5<<16)|(data_offset+field), # addiu a1,s0,offset
            (43<<26)|(4<<21)|(5<<16)|(0x970+field)]      # sw a1,fixed_pointer_cell(a0)
 if stats_offset is not None:words += [(9<<26)|(16<<21)|(5<<16)|stats_offset,0xac855538]
 for cell,offset in extra_pointers:
  assert cell>>16==0x53 and 0<=offset<32768
  words += [(9<<26)|(16<<21)|(5<<16)|offset,(43<<26)|(4<<21)|(5<<16)|(cell&65535)]
 for offset,count in relocations:
  words += [0x26050000|offset,0x24060000|count,0x8ca80000,0x2d097fff,0x11200002,0,0x01104021,0xaca80000,0x24a50004,0x24c6ffff,0x14c0fff7,0]
 if probe:
  # The allocator is needed to load this module. Install only after the hook
  # pointer exists; an ELF-time patch would jump through a null pointer at boot.
  hook=0x3ac358 if probe=='mesh' else 0x31e818
  words += [0x3c080000|((hook+0x8000)>>16),0x25080000|(hook&65535)]
  for i,w in enumerate([0x3c190053,0x8f390a08,0x03200008,0]):
   words += [0x3c090000|(w>>16),0x35290000|(w&65535),0xad090000|(i*4)]
 if profile_name_offset is not None:
  # Sam's baseline/limit row lives in the vacated fixed CHARDB region.
  words += [0x2405001e,0xa0850a9a,0x24050005]
  words += [(40<<26)|(4<<21)|(5<<16)|(0xa9b+i) for i in range(7)]
  words += [0x2405000b]
  words += [(40<<26)|(4<<21)|(5<<16)|(0xaa2+i) for i in range(7)]
 if bolt_name_offset is not None:
  # Allocate the relocated wardrobe object once, before its original initializer.
  words += [0x27bdfff0,0xafbf0000,0x3c08004a,0x8d046750]
  branch=len(words);words += [0,0] # skip allocation if already present
  words += [0x240405b0,(9<<26)|(16<<21)|(5<<16)|bolt_name_offset,0x00003021,
            (3<<26)|(0x317e30>>2),0x00003821,
            0x14400002,0,0x0000000d, # trap an allocation failure
            0x3c08004a,0xad026750,0x00402021,0x00002821,
            (3<<26)|(0x416210>>2),0x240605b0]
  finish=len(words);words[branch]=(5<<26)|(4<<21)|((finish-branch-1)&65535)
  words += [0x8fbf0000,0x27bd0010]
 if profile_name_offset is not None:
  words += [0x27bdfff0,0xafbf0000,0x3c080053,0x8d0409fc]
  branch=len(words);words += [0,0]
  words += [0x24042e98,(9<<26)|(16<<21)|(5<<16)|profile_name_offset,0x00003021,
            (3<<26)|(0x317e30>>2),0x00003821,0x14400002,0,0x0000000d,
            0x3c080053,0xad0209fc,0x00402021,0x00002821,(3<<26)|(0x416210>>2),0x24062e98]
  finish=len(words);words[branch]=(5<<26)|(4<<21)|((finish-branch-1)&65535)
  words += [0x8fbf0000,0x27bd0010]
 words += [0x03e00008,0] # jr ra; nop
 result=struct.pack('<%dI'%len(words),*words)
 assert len(result)<=data_offset
 return result.ljust(data_offset,b'\0')

def main():
 probe='mesh' if '--mesh-probe' in sys.argv else '--allocation-probe' in sys.argv
 save_extension='--save-extension' in sys.argv
 bio='--bio' in sys.argv or save_extension
 roster=ROOT/'local/sam-ps2/roster';fx_filter='--fx-filter' in sys.argv or probe or bio;profiles='--profiles' in sys.argv or fx_filter;bolt_runtime='--bolt-runtime' in sys.argv or profiles;preview='--preview' in sys.argv or bolt_runtime;attributes='--attributes' in sys.argv or preview
 out=roster/'fx-filter' if fx_filter else roster/'profiles' if profiles else roster/'bolts' if bolt_runtime else roster/'preview' if preview else roster/'attributes' if attributes else roster;out.mkdir(parents=True,exist_ok=True)
 if probe:out=roster/'allocation-probe';out.mkdir(parents=True,exist_ok=True)
 if probe=='mesh':out=roster/'mesh-probe';out.mkdir(parents=True,exist_ok=True)
 if bio:out=roster/'bio';out.mkdir(parents=True,exist_ok=True)
 if save_extension:out=roster/'save-extension';out.mkdir(parents=True,exist_ok=True)
 data_offset=1024 if save_extension else 768 if probe or bio else 512 if profiles else 256 if attributes else DATA_OFFSET
 stats_offset=data_offset+31*136 if attributes else None
 d=Disc(Path.home()/'Downloads/SSX 3 (USA).iso');elf=d.file('SLUS_207.72')
 assert hashlib.sha1(elf).hexdigest()==SHA1
 raw=d.file('DATA/BE/CHARDB.DBL');assert len(raw)==1360
 phoff=struct.unpack_from('<I',elf,28)[0];phsize,phnum=struct.unpack_from('<HH',elf,42)
 segments=[]
 for i in range(phnum):
  typ,off,va,_,size,_,_,_=struct.unpack_from('<8I',elf,phoff+i*phsize)
  if typ==1:segments.append((va,off,size))
 def read(at,n):
  for va,off,size in segments:
   if va<=at and at+n<=va+size:return elf[off+at-va:off+at-va+n]
  raise ValueError('Patch outside executable file')
 patches=[]
 def patch(at,old,new,purpose):
  assert read(at,len(old))==old,hex(at)
  assert len(old)==len(new)
  patches.append(dict(address=hex(at),expected=old.hex(),replacement=new.hex(),purpose=purpose))
 # Keep s0=FILE_load result at149c80; invoke the initializer inside that file.
 expected=bytes.fromhex('5300043c700984242d2800021758100c50050624040000125300023ca65f0c0c2d200002')
 assert len(expected)==36
 if not bolt_runtime:
  patch(0x149c84,expected,struct.pack('<I',0x0200f809)+bytes(32),
        'Initialize database field pointers from the retained FILE_load allocation; remove fixed memcpy/free')
 else:
  # Preserve prologue saves, but bootstrap CHARDB before the wardrobe init.
  for at,new in [(0x149bbc,0x3c040046),(0x149bc4,0x2484a2e0),
                 (0x149bd8,(3<<26)|(0x3e18c8>>2)),(0x149bdc,0x24050100),
                 (0x149be0,0x0040802d),(0x149be4,0x0040f809),(0x149be8,0),(0x149bec,0x3c130053)]:
   patch(at,read(at,4),struct.pack('<I',new),'Bootstrap retained module before wardrobe initialization; preserve saved registers')
  replacement=struct.pack('<8I',0x3c04004a,0x8c846750,(3<<26)|(0x14c6a0>>2),0,
                          0x3c04004c,0x24843e98,(3<<26)|(0x15ad58>>2),0x0000282d)
  # Reuse the displaced loader's 24-byte region for a skipped call trampoline.
  # The normal initializer jumps over it; retention calls it with its own RA.
  replacement+=struct.pack('<6I',(2<<26)|(0x149ca8>>2),0,0x3c190053,0x8f390a10,0x03200008,0)
  patch(0x149c70,read(0x149c70,56),replacement,'Initialize relocated wardrobe and remaining BE state after module bootstrap')

 refs=json.loads((roster/'chardb-static-reference-candidates.json').read_text())
 for ref in refs:
  at=int(ref['use'],16)
  if at==0x149c88:continue
  old=int(ref['opcode'],16);target=int(ref['target'],16)
  assert old>>26==9 and target-BASE in FIELDS
  # Original LUI remains; ADDIU absolute field base becomes LW field pointer.
  new=(old&0x03ffffff)|(35<<26)
  patch(at,struct.pack('<I',old),struct.pack('<I',new),'Load retained character-table field pointer')
 if attributes:
  refs=json.loads((roster/'attribute-storage-reference-candidates.json').read_text())
  for ref in refs:
   at=int(ref['use'],16);old=int(ref['opcode'],16);assert old>>26==9 and old&65535==0x5538
   patch(at,struct.pack('<I',old),struct.pack('<I',(old&0x03ffffff)|(35<<26)),'Load expanded attribute cache pointer; preserve original profile stride')
 extra_pointers=[];dispatch_tables=[];extra_payload=b'';relocations=[]
 if preview:
  start=data_offset+31*136+420
  for original_table,cell,default in [(0x460b00,0x5309f0,0x19eb7c),(0x460b40,0x5309f4,0x19f020)]:
   original=list(struct.unpack('<10I',read(original_table,40)));targets=original+[default]*20+[original[3]]
   offset=start+len(extra_payload);extra_pointers.append((cell,offset));extra_payload+=struct.pack('<31I',*targets)
   dispatch_tables.append(dict(original_table=hex(original_table),pointer_cell=hex(cell),payload_offset=offset,original_targets=original,default_target=default,sam_target=original[3]))
  for at,old,new in [
   (0x19e5e0,0x2c42000a,0x2c42001f),(0x19f19c,0x2c42000a,0x2c42001f),
   (0x19eb08,0x2c62000a,0x2c62001f),(0x19eb10,0x3c020046,0x3c020053),(0x19eb18,0x24420b00,0x8c4209f0),
   (0x19ef0c,0x2c82000a,0x2c82001f),(0x19ef14,0x3c020046,0x3c020053),(0x19ef1c,0x24420b40,0x8c4209f4)]:
   patch(at,struct.pack('<I',old),struct.pack('<I',new),'Expanded preview/animation dispatch; Sam shares compatible Mac animation branch')
 bolt_name_offset=None
 if bolt_runtime:
  # The gameplay compactor separately marks IDs10..29 for retention. Sam30
  # was dropped here, leaving an empty gameplay model list. Retain its rows
  # too; gameplay still filters equipped items from Sam's independent profile.
  patch(0x14cd74,struct.pack('<I',0x2a02001e),struct.pack('<I',0x2a020020),'Retain Sam wardrobe rows during gameplay compaction')
  layout=json.loads((roster/'bolt-layout-candidates.json').read_text())
  for entry in layout['member_candidates']:
   patch(int(entry['address'],16),bytes.fromhex(entry['expected']),bytes.fromhex(entry['replacement']),'Expand wardrobe member arrays to 32 entries')
  for entry in layout['count_candidates']:
   at=int(entry['address'],16);old=int(entry['word'],16);new={29:31,30:32,90:96,120:128}[entry['value']]
   patch(at,struct.pack('<I',old),struct.pack('<I',(old&0xffff0000)|new),'Expand wardrobe allocation/initialization count')
  unique={e['address']:e for e in layout['global_candidates']}
  for at,entry in unique.items():
   if int(at,16) in (0x149bc4,0x15b844):continue # bootstrap moved; static constructor must initialize the pre-bootstrap anchor
   patch(int(at,16),bytes.fromhex(entry['expected']),bytes.fromhex(entry['replacement']),'Use persistent heap wardrobe object')
  patch(0x15bab4,struct.pack('<I',0x26a46750),struct.pack('<I',0x8ea46750),'Destroy the relocated wardrobe object at global shutdown')
  patch(0x15babc,struct.pack('<I',0x24050002),struct.pack('<I',0x24050003),'Delete the heap wardrobe object after releasing its contents')
  bolt_name_offset=data_offset+31*136+420+len(extra_payload)
  extra_payload+=b'Sam wardrobe DB\0'
  from profile_hooks import wardrobe_name_budget
  while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
  extra_pointers.append((0x530a10,data_offset+31*136+420+len(extra_payload)))
  extra_payload+=wardrobe_name_budget(save_extension)
  patch(0x14ccec,struct.pack('<I',0x27d50001),struct.pack('<I',(3<<26)|(0x149c98>>2)),
        'Budget Sam name strings copied by wardrobe compaction; replay retention-loop setup')
  for at in [0x182f08,0x184cb0]:
   patch(at,struct.pack('<2I',0x8ec20000,0x2c42000a),struct.pack('<2I',(3<<26)|(0x149c98>>2),0),
         'Draw Sam in Setup/Rider Details while preserving original primary/bonus eligibility')
  patch(0x19c040,struct.pack('<2I',0x8e820000,0x2c42000a),struct.pack('<2I',(3<<26)|(0x149c98>>2),0),
        'Draw Sam in equipment preview; retain original readiness checks and bonus eligibility')
 profile_name_offset=None;profile_hooks={}
 if profiles:
  from profile_hooks import getter,initialize_records,effect_filter,GETTER_CELL,INIT_CELL
  for label,cell,code in [('get_record',GETTER_CELL,getter()),('initialize_records',INIT_CELL,initialize_records(save_extension))]:
   while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
   offset=data_offset+31*136+420+len(extra_payload);extra_payload+=code
   extra_pointers.append((cell,offset));profile_hooks[label]=dict(offset=offset,bytes=len(code),pointer_cell=hex(cell))
  if fx_filter:
   while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
   offset=data_offset+31*136+420+len(extra_payload);code=effect_filter();extra_payload+=code
   extra_pointers.append((0x530a04,offset));profile_hooks['effect_filter']=dict(offset=offset,bytes=len(code),pointer_cell='0x530a04')
   patch(0x2ece44,bytes.fromhex('4400a68f0a00c228050040100100b524'),struct.pack('<4I',0x3c190053,0x8f390a04,0x03200008,0),'Sam effects honor equipped flags; preserve original bonus bypass and displaced delay slot')
  if probe:
   from profile_hooks import allocation_probe,mesh_probe
   while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
   extra_pointers.append((0x530a08,data_offset+31*136+420+len(extra_payload)))
   extra_payload+=mesh_probe() if probe=='mesh' else allocation_probe()
   extra_pointers.append((0x530a0c,data_offset+31*136+420+len(extra_payload)))
   extra_payload+=bytes(160)
   assert read(0x31e818,16)==bytes.fromhex('60ffbd27ffff023c1000be7fdfff4234')
  if bio:
   from rider_bio import table_hook
   while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
   table_offset=data_offset+31*136+420+len(extra_payload)
   keys=[f'kT_{kind}{i+1}Sam'.encode()+b'\0' for kind,count in [('DNA',8),('FAVES',12),('QNA',4)] for i in range(count)]
   offsets=[];pos=table_offset+len(keys)*4
   for key in keys:offsets.append(pos);pos+=len(key)
   extra_payload+=struct.pack('<24I',*offsets)+b''.join(keys);relocations.append((table_offset,24))
   for kind,cell,table_delta,at,expected in [
    ('DNA',0x530a20,0,0x190ef4,[0x8e820048,0x00021140,0x00621021,0x03a21821]),
    ('FAVES',0x530a24,32,0x191064,[0x8e820048,0x00551018,0x00621021,0x03a21821]),
    ('QNA',0x530a28,80,0x1911d4,[0x8e820048,0x00021100,0x00621021,0x03a21821])]:
    table_cell={'DNA':0x530a14,'FAVES':0x530a18,'QNA':0x530a1c}[kind]
    extra_pointers.append((table_cell,table_offset+table_delta))
    while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
    offset=data_offset+31*136+420+len(extra_payload);code=table_hook(kind);extra_payload+=code
    extra_pointers.append((cell,offset));profile_hooks['bio_'+kind]=dict(offset=offset,bytes=len(code),pointer_cell=hex(cell))
    patch(at,struct.pack('<4I',*expected),struct.pack('<4I',0x3c190053,0x8f390000|(cell&65535),0x0320f809,0),'Route Sam biography lookup to private keys; original stack tables unchanged')
  if save_extension:
   from save_extension import writer,validator,loader_check,loader_finish,RECORD_SIZE
   from snapshot_hooks import SITES,pointer_hook,patch_words
   import upgrade_hooks
   import inventory_hooks
   import mission_flag_hooks
   while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
   offset=data_offset+31*136+420+len(extra_payload);code=mission_flag_hooks.hook();extra_payload+=code
   extra_pointers.append((mission_flag_hooks.CELL,offset));profile_hooks['mission_status_flags']=dict(offset=offset,bytes=len(code),pointer_cell=hex(mission_flag_hooks.CELL))
   for site in mission_flag_hooks.SITES:
    patch(site,struct.pack('<4I',*mission_flag_hooks.WORDS),struct.pack('<4I',0x3c190053,0x8f390b78,0x0320f809,0),'Route Sam per-mission status reads/writes to owned profile zero; retain original slot and bit rules')
   import score_record_hooks
   for site,cell,profile,character,char_product,profile_product,words in score_record_hooks.SITES:
    while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
    offset=data_offset+31*136+420+len(extra_payload);code=score_record_hooks.hook(site,profile,character,char_product,profile_product,words);extra_payload+=code
    extra_pointers.append((cell,offset));profile_hooks[f'score_record_{site:x}']=dict(offset=offset,bytes=len(code),pointer_cell=hex(cell))
    patch(site,struct.pack('<4I',*words),patch_words(cell),'Route Sam medal/score/result record pointers; preserve original result logic')
   import peak_flag_hooks
   while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
   offset=data_offset+31*136+420+len(extra_payload);code=peak_flag_hooks.reader(read(peak_flag_hooks.READ_SITE,0x78));extra_payload+=code
   extra_pointers.append((peak_flag_hooks.READ_CELL,offset));profile_hooks['peak_flag_read']=dict(offset=offset,bytes=len(code),pointer_cell=hex(peak_flag_hooks.READ_CELL))
   patch(peak_flag_hooks.READ_SITE,read(peak_flag_hooks.READ_SITE,16),patch_words(peak_flag_hooks.READ_CELL),'Read Sam peak-access bits from owned profile; preserve original modes and rider fallback')
   for site,cell,profile,character,words in peak_flag_hooks.WRITES:
    while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
    offset=data_offset+31*136+420+len(extra_payload);code=peak_flag_hooks.writer(site,profile,character,words);extra_payload+=code
    extra_pointers.append((cell,offset));profile_hooks[f'peak_flag_write_{site:x}']=dict(offset=offset,bytes=len(code),pointer_cell=hex(cell))
    patch(site,struct.pack('<4I',*words),patch_words(cell),'Route Sam peak-access flag mutations to owned profile; preserve original bit operations')
   import gear_icon_hook
   while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
   offset=data_offset+31*136+420+len(extra_payload);code=gear_icon_hook.hook(read(gear_icon_hook.SITE,0xa8));extra_payload+=code
   extra_pointers.append((gear_icon_hook.CELL,offset));profile_hooks['gear_icon_archive']=dict(offset=offset,bytes=len(code),pointer_cell=hex(gear_icon_hook.CELL))
   extra_pointers.append((gear_icon_hook.PATH_CELL,data_offset+31*136+420+len(extra_payload)));extra_payload+=gear_icon_hook.PATH
   patch(gear_icon_hook.SITE,read(gear_icon_hook.SITE,16),patch_words(gear_icon_hook.CELL),'Supply Sam icon archive; preserve original character archive lookup')
   import default_head_hook
   while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
   offset=data_offset+31*136+420+len(extra_payload);code=default_head_hook.reset_metadata_hook();extra_payload+=code
   extra_pointers.append((default_head_hook.RESET_CELL,offset));profile_hooks['default_head_metadata']=dict(offset=offset,bytes=len(code),pointer_cell=hex(default_head_hook.RESET_CELL))
   patch(default_head_hook.RESET_SITE,struct.pack('<4I',*default_head_hook.RESET_WORDS),default_head_hook.reset_patch(),'Use regular Sam head when resetting an outfit loaded from older saves')
   while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
   offset=data_offset+31*136+420+len(extra_payload);code=default_head_hook.hook();extra_payload+=code
   extra_pointers.append((default_head_hook.CELL,offset));profile_hooks['default_head_reset']=dict(offset=offset,bytes=len(code),pointer_cell=hex(default_head_hook.CELL))
   patch(default_head_hook.SITE,struct.pack('<4I',*default_head_hook.RESTORE),default_head_hook.patch(),'Keep Sam regular gameplay head as reset/default equipment, preserving original rider initialization')
   import inventory_reset_hook
   while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
   offset=data_offset+31*136+420+len(extra_payload);code=inventory_reset_hook.hook();extra_payload+=code
   extra_pointers.append((inventory_reset_hook.CELL,offset));profile_hooks['inventory_reset']=dict(offset=offset,bytes=len(code),pointer_cell=hex(inventory_reset_hook.CELL))
   patch(inventory_reset_hook.SITE,struct.pack('<4I',*inventory_reset_hook.WORDS),inventory_reset_hook.patch(),'Route extra equipment reset iteration to Sam owned profile; preserve original ten records')
   patch(0x14af88,struct.pack('<I',0x2862000a),struct.pack('<I',0x2862000b),'Reset eleven riders including Sam in each profile')
   for site,cell,kind,words in inventory_hooks.ROUTING_SITES:
    while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
    offset=data_offset+31*136+420+len(extra_payload);code=inventory_hooks.routing_hook(site,kind,words);extra_payload+=code
    extra_pointers.append((cell,offset));profile_hooks[f'inventory_{kind}']=dict(offset=offset,bytes=len(code),pointer_cell=hex(cell))
    patch(site,struct.pack('<4I',*words),patch_words(cell),'Route Sam inventory bulk copy/equipment mutation to owned record; retain original logic')
   for site,cell,kind,size in inventory_hooks.SITES:
    while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
    offset=data_offset+31*136+420+len(extra_payload);code=inventory_hooks.hook(kind,read(site,size));extra_payload+=code
    extra_pointers.append((cell,offset));profile_hooks[f'inventory_{kind}']=dict(offset=offset,bytes=len(code),pointer_cell=hex(cell))
    patch(site,read(site,16),patch_words(cell),'Route Sam inventory entry/list/flag commit to owned records; preserve original rider fallback')
   while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
   offset=data_offset+31*136+420+len(extra_payload);code=upgrade_hooks.refresh_after_purchase();extra_payload+=code
   extra_pointers.append((0x530b24,offset));profile_hooks['purchase_cache_refresh']=dict(offset=offset,bytes=len(code),pointer_cell='0x530b24')
   patch(0x150e10,struct.pack('<I',(3<<26)|(0x147f78>>2)),struct.pack('<I',(3<<26)|(0x149c98>>2)),'Refresh Sam gameplay attributes after successful purchase, then invoke original notification getter')
   import attribute_read_hook
   while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
   offset=data_offset+31*136+420+len(extra_payload);code=attribute_read_hook.hook();extra_payload+=code
   extra_pointers.append((attribute_read_hook.CELL,offset));profile_hooks['attribute_read']=dict(offset=offset,bytes=len(code),pointer_cell=hex(attribute_read_hook.CELL))
   for site in attribute_read_hook.SITES:
    patch(site,struct.pack('<4I',*attribute_read_hook.WORDS),attribute_read_hook.patch(),'Read Sam raw attributes from owned record; preserve original stat getter behavior')
   import stat_credit_hooks
   for site,cell,kind,words in stat_credit_hooks.SITES:
    while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
    offset=data_offset+31*136+420+len(extra_payload);code=stat_credit_hooks.hook(site,kind,words);extra_payload+=code
    extra_pointers.append((cell,offset));profile_hooks[f'stat_credit_{site:x}']=dict(offset=offset,bytes=len(code),pointer_cell=hex(cell))
    patch(site,struct.pack('<4I',*words),stat_credit_hooks.patch_words(cell),'Route Sam cash-credit stat adjustment to owned records; preserve original operations')
   for kind,cell in [('balance',0x530a70),('stat',0x530a74),('debit',0x530a78)]:
    while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
    offset=data_offset+31*136+420+len(extra_payload);code=upgrade_hooks.hook(kind);extra_payload+=code
    extra_pointers.append((cell,offset));profile_hooks[f'upgrade_{kind}']=dict(offset=offset,bytes=len(code),pointer_cell=hex(cell))
   for site,cell,kind,words in upgrade_hooks.SITES:
    patch(site,struct.pack('<4I',*words),upgrade_hooks.patch_words(cell),'Route Sam upgrade balance/stat/debit addresses; retain original price and limit logic')
   from economy_hooks import direct_getter,current_cash,mutation
   for site,cell,code in [(0x150928,0x530a54,direct_getter(read(0x150928,56),0xac4)),(0x1509c0,0x530a58,direct_getter(read(0x1509c0,56),0xac8)),(0x150988,0x530a5c,current_cash()),(0x150a20,0x530a60,current_cash(0x150a30)),(0x150a58,0x530a64,mutation(read(0x150a58,56),0x150a80,2)),(0x150a90,0x530a68,mutation(read(0x150a90,80),0x150ab8)),(0x150b48,0x530a6c,mutation(read(0x150b48,64),0x150b70))]:
    while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
    offset=data_offset+31*136+420+len(extra_payload);extra_payload+=code
    extra_pointers.append((cell,offset));profile_hooks[f'economy_{site:x}']=dict(offset=offset,bytes=len(code),pointer_cell=hex(cell))
    patch(site,read(site,16),patch_words(cell),'Route Sam economy reads and mutations to owned profile; preserve original rider fallback')
   for site,profile,cell,words in SITES:
    while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
    offset=data_offset+31*136+420+len(extra_payload)
    code=pointer_hook(site,profile,words);extra_payload+=code
    extra_pointers.append((cell,offset));profile_hooks[f'snapshot_{site:x}']=dict(offset=offset,bytes=len(code),pointer_cell=hex(cell))
    patch(site,struct.pack('<4I',*words),patch_words(cell),'Route Sam active-profile snapshot copy/restore to owned records; original riders unchanged')
   for label,cell,code in [('save_writer',0x530a2c,writer()),('save_validator',0x530a34,validator()),('save_default',0x530a38,bytes(RECORD_SIZE)),('save_check',0x530a3c,loader_check()),('save_finish',0x530a40,loader_finish())]:
    while (data_offset+31*136+420+len(extra_payload))%16:extra_payload+=b'\0'
    offset=data_offset+31*136+420+len(extra_payload);extra_payload+=code
    extra_pointers.append((cell,offset));profile_hooks[label]=dict(offset=offset,bytes=len(code),pointer_cell=hex(cell))
   for at,cell,tail in [(0x152b64,0x530a2c,0x0320f809),(0x152e08,0x530a3c,0x0320f809),(0x15302c,0x530a40,0x03200008)]:
    patch(at,read(at,16),struct.pack('<4I',0x3c190053,0x8f390000|(cell&65535),tail,0),'Versioned Sam save extension writer/loader')
   patch(0x152bac,struct.pack('<I',0x34029b61),struct.pack('<I',0x3402ab0c),'Allocate/write extended Sam profile')
   patch(0x152dfc,struct.pack('<I',(3<<26)|(0x152ba8>>2)),struct.pack('<I',0x34029b61),'Keep original loader legacy checksum position')
   for at in [0x2c5b00,0x2c5d9c]:
    patch(at,read(at,8),struct.pack('<2I',(3<<26)|(0x149c98>>2),0),'Accept legacy profile length and capture actual read count')
  profile_name_offset=data_offset+31*136+420+len(extra_payload);extra_payload+=b'Sam profiles\0'
  patch(0x14ad28,read(0x14ad28,16),struct.pack('<4I',0x3c190053,0x8f3909f8,0x03200008,0),'Route only Sam ID30 to independent profile records')
  patch(0x149da0,read(0x149da0,16),struct.pack('<4I',0x3c190053,0x8f390a00,0x0320f809,0),'Initialize Sam records after original profiles and replay displaced BE setup')
 sam=bytearray(raw[3*136:4*136])
 def text(at,n,value):
  b=value.encode('ascii');assert len(b)<n;sam[at:at+n]=b.ljust(n,b'\0')
 text(0,32,'Sam');text(32,16,'Sam');text(48,16,'Chopped Unc')
 struct.pack_into('<I',sam,64,73) # integer kilograms; approximately160lb
 struct.pack_into('<I',sam,68,0)  # original Mac regular-stance encoding
 text(76,16,'');struct.pack_into('<I',sam,96,0) # unspecified blood type/age, UI needs explicit handling
 text(100,16,"5'11\"");text(116,16,'American');struct.pack_into('<I',sam,132,SAM_ID)
 records=raw+bytes(20*136)+sam
 payload=initializer(data_offset,stats_offset,extra_pointers,bolt_name_offset,profile_name_offset,probe,relocations)+records+(bytes([5])*420 if attributes else b'')+extra_payload
 assert records[:1360]==raw and len(records)==31*136
 (out/'CHARDB-expanded-candidate.DBL').write_bytes(payload)
 report=dict(original_elf_sha1=SHA1,sam_id=SAM_ID,data_offset=data_offset,record_stride=136,
             original_records=10,record_capacity=31,fx_filter=fx_filter,profiles=profiles,profile_hooks=profile_hooks,profile_bytes=11928 if profiles else 0,bolt_runtime=bolt_runtime,bolt_object_bytes=0x5b0 if bolt_runtime else 0,dispatch_tables=dispatch_tables,attribute_offset=stats_offset,attribute_size=420 if attributes else 0,pointer_offsets={**{hex(BASE+f):data_offset+f for f in FIELDS},**({'0x535538':stats_offset} if attributes else {}),**{hex(cell):offset for cell,offset in extra_pointers}},pointer_cells=[hex(BASE+x) for x in FIELDS],
             payload_sha256=hashlib.sha256(payload).hexdigest(),patches=patches,installed=False,
             limitations=['Candidate reference list is not an exhaustive access audit','Repeated initialization/lifetime needs testing','Save/progression and wardrobe still unexpanded','No instruction-cache/hardware execution validation yet'])
 (out/'character-storage-patches.json').write_text(json.dumps(report,indent=2)+'\n')
 print(f'Staged {len(payload)}-byte database and {len(patches)} guarded patches; original ten records preserved')
if __name__=='__main__':main()
