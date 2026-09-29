"""Read-only validation of the candidate database in isolated PCSX2 slot28020."""
from pathlib import Path
import hashlib,json,os,struct,sys
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from pcsx2_pine import Pine
from build_character_storage import BASE,FIELDS,DATA_OFFSET
ROOT=Path(__file__).resolve().parents[2];folder=ROOT/'local/sam-ps2/roster'
if '--fx-filter' in sys.argv:folder=folder/'fx-filter'
elif '--profiles' in sys.argv:folder=folder/'profiles'
elif '--bolt-runtime' in sys.argv:folder=folder/'bolts'
elif '--preview' in sys.argv:folder=folder/'preview'
elif '--attributes' in sys.argv:folder=folder/'attributes'
payload=(folder/'CHARDB-expanded-candidate.DBL').read_bytes();manifest=json.loads((folder/'character-storage-patches.json').read_text())
with Pine(Path(os.environ.get('TMPDIR','/tmp'))/'pcsx2.sock.28020') as pine:
 info=pine.info();assert info['game_id']=='SLUS-20772' and info['status']=='running'
 pointer=struct.unpack('<I',pine.read(BASE,4))[0];assert 0x100000<=pointer<0x2000000-4216
 fields={hex(BASE+f):struct.unpack('<I',pine.read(BASE+f,4))[0] for f in FIELDS}
 assert all(fields[hex(BASE+f)]==pointer+f for f in FIELDS)
 actual=pine.read(pointer,31*136);assert actual==payload[manifest['data_offset']:manifest['data_offset']+31*136]
 assert pine.read(pointer-manifest['data_offset'],manifest['data_offset'])==payload[:manifest['data_offset']]
 for p in manifest['patches']:assert pine.read(int(p['address'],16),len(bytes.fromhex(p['replacement'])))==bytes.fromhex(p['replacement'])
 for table in manifest.get('dispatch_tables',[]):
  addr=struct.unpack('<I',pine.read(int(table['pointer_cell'],16),4))[0];expected=pointer-manifest['data_offset']+table['payload_offset'];assert addr==expected
  assert pine.read(addr,124)==payload[table['payload_offset']:table['payload_offset']+124]
 result=dict(emulator=info,database_pointer=hex(pointer),field_pointers=fields,record_bytes_sha256=hashlib.sha256(actual).hexdigest(),records_equal_candidate=True,original_ten_preserved=True,live_patch_sites_verified=len(manifest['patches']),limitations='Boot-time data/patch validation only; no extra slot, outfit, gameplay or save roundtrip verified')
 if manifest.get('attribute_offset') is not None:
  attrs=struct.unpack('<I',pine.read(0x535538,4))[0];assert attrs==pointer+31*136
  cache=pine.read(attrs,420)
  result['attribute_pointer']=hex(attrs);result['original_cache_values']=sorted(set(cache[:210]));result['sam_cache_values']=[list(cache[x:x+7]) for x in (210,280,350)]
 if manifest.get('bolt_runtime'):
  bolt=struct.unpack('<I',pine.read(0x4a6750,4))[0];assert 0x100000<=bolt<0x2000000-0x5b0
  fields={}
  for name,at in [('moby_count',0x2c),('moby_start',0xac),('sam_count',0x2c+30*4),('sam_start',0xac+30*4),('sam_lookup',0x12c+30*4)]:fields[name]=struct.unpack('<I',pine.read(bolt+at,4))[0]
  assert fields['moby_count']==503 and fields['moby_start']==0 and fields['sam_count']==443 and fields['sam_start']==4829,fields
  result['wardrobe_object']=hex(bolt);result['wardrobe_fields']=fields
 if manifest.get('profiles'):
  profile=struct.unpack('<I',pine.read(0x5309fc,4))[0];assert 0x100000<=profile<0x2000000-11928
  records=[]
  for i in range(3):
   data=pine.read(profile+i*0xf88,0xf88);lookup=struct.unpack_from('<I',data,0x288)[0]
   records.append(dict(address=hex(profile+i*0xf88),character_id=data[0xbc0],equipment_lookup=hex(lookup),equipment_count=struct.unpack_from('<I',data,0x28c)[0],attributes=list(data[0xbdf:0xbe6])))
  result['sam_profiles']=records
  assert all(x['character_id']==30 and x['equipment_count']==443 and int(x['equipment_lookup'],16)>=0x100000 for x in records),records
  assert len({x['equipment_lookup'] for x in records})==3
  result['sam_limits']=list(pine.read(0x530a9a,15));assert result['sam_limits']==[30]+[5]*7+[11]*7
 (folder/'storage-live-verification.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
