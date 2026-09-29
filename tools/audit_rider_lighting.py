#!/usr/bin/env python3
"""Read-only audit of the original Zoe lighting assembly inputs and outputs."""
import hashlib,json,struct,zipfile
from pathlib import Path
from export_irradiance import decode_bank
ROOT=Path(__file__).resolve().parents[1]
def audit():
 snapshot=ROOT/'local/reference/pcsx2/snow-jam-glide.p2s';memory=zipfile.ZipFile(snapshot).read('eeMemory.bin');rig=json.loads((ROOT/'local/assets/native/RIDER_ZOE/rider.json').read_text());digest=hashlib.sha256(memory).hexdigest()
 if digest!=rig['assembly_evidence']['ee_sha256']:raise ValueError('Zoe assembly snapshot mismatch')
 def read(fmt,at):
  if at<0 or at+struct.calcsize(fmt)>len(memory):raise ValueError('Lighting address out of snapshot')
  return struct.unpack_from(fmt,memory,at)
 u=lambda at:read('<I',at)[0];actor=int(rig['assembly_evidence']['actor'],16)
 interface=u(actor+0x6c0)
 if read('<h',interface+0x38)[0]!=-0x6c0 or u(interface+0x3c)!=0x140b80:raise ValueError('Unexpected rider lighting-index getter')
 index=u(actor+0x86c)
 if index>=6:raise ValueError('Not a rider environment slot')
 group=0x4fa370+index*240;driver=u(group+28);painter=u(driver)
 if u(painter+4)!=0x484290:raise ValueError('Not the Lighting painter class')
 bank=decode_bank((ROOT/'local/assets/source/ps2/irr.dat').read_bytes(),'<');names={value['index']:name for name,value in bank.items()};references=[]
 for i in range(4):
  words=read('<2I',painter+8+i*16)
  if words[1]==0x123400:
   if words[0] not in names:raise ValueError('Invalid resolved irradiance index')
   name=names[words[0]];kind='resolved-index'
  else:name=memory[painter+8+i*16:painter+16+i*16].rstrip(b'\0').decode('ascii');kind='name'
  if name not in bank:raise ValueError('Unknown irradiance reference')
  references.append(dict(name=name,storage=kind,words=list(words)))
 lights=[]
 for slot in range(8):
  pointer=u(actor+0x794+slot*4)
  if pointer:lights.append(dict(slot=slot,address=hex(pointer),kind=u(pointer+16),header_words=list(read('<8I',pointer))))
 controller=u(actor+0x77c);extra=None
 if controller:extra=dict(address=hex(controller+0xd30),ambient=list(read('<3f',controller+0xd30)),directional_count=read('<i',controller+0xd3c)[0])
 renderer=u(0x4a30f0-0x854);render_vtable=u(renderer+0x10d8)
 offset=read('<h',render_vtable+0x228)[0];method=u(render_vtable+0x22c)
 if offset!=0 or method!=0x3954d0:raise ValueError('Unexpected renderer irradiance setter')
 stages={}
 for slot,expected in [(0x31c,0x37a610),(0x37c,0x386bd0),(0x3e4,0x396b40)]:
  target=u(render_vtable+slot);adjustment=read('<h',render_vtable+slot-4)[0]
  if target!=expected or adjustment!=0:raise ValueError('Unexpected rider draw renderer stage')
  stages[hex(slot)]=dict(target=hex(target),adjustment=adjustment)
 renderer_binding=dict(draw_stages=stages,renderer=hex(renderer),vtable=hex(render_vtable),setter=hex(method),adjustment=offset,
                       storage=hex(renderer+0x6bb0),coefficient_words=list(read('<40I',renderer+0x6bb0)),
                       source_call='1224C8..1224E4',source_upload='396B40..396C28',
                       note='Renderer storage at snapshot time may belong to a later draw; no equality with rider bank is assumed.')
 texture_manager=u(renderer+0x18f4);geometry=u(actor+0x780);parts=u(geometry+12);materials=[]
 #310640 chooses active parts;37A610 walks signed header+48 records of20 bytes.
 for part_index in range(u(geometry+8)):
  part=parts+part_index*0x58
  if not u(part+0x18):continue
  model=u(part+0x1c);header=u(model);data=u(model+4)
  name=memory[header:header+16].split(b'\0')[0].decode('ascii')
  count=read('<h',header+0x48)[0]
  if not 0<=count<=128:raise ValueError('Unexpected rider material count')
  table=data+u(header+0x20)
  for material_index in range(count):
   entry=table+20*material_index;flags=u(entry);handle=u(entry+4)
   if handle==0xffffffff:raise ValueError('Unbound material in audited rider')
   descriptor=u(texture_manager+8+4*handle);tex0=read('<Q',descriptor+0x38)[0]
   tfx=(tex0>>35)&3;tcc=(tex0>>34)&1
   if (tfx,tcc)!=(3,1):raise ValueError('Audited rider texture is not RGBA HIGHLIGHT2')
   materials.append(dict(part=part_index,model=name,index=material_index,flags=hex(flags),texture_handle=hex(handle),
     descriptor=hex(descriptor),tex0=hex(tex0),tfx=tfx,tcc=tcc,psm=(tex0>>20)&63,
     width=1<<((tex0>>26)&15),height=1<<((tex0>>30)&15),normal_path_blend_enum=5 if flags&0x8000 else 1))
 result=dict(snapshot=str(snapshot),ee_sha256=digest,actor=hex(actor),environment_index=index,selector=read('<f',group+36)[0],rider_flag_3fc=u(actor+0x3fc),references=references,scalars=list(read('<4f',painter+72)),environment_coefficients=list(read('<40I',group+80)),rider_coefficients=list(read('<40I',actor+0x7c0)),local_lights=lights,control_extra=extra,renderer_binding=renderer_binding,materials=materials,scope='Captured assembly evidence, not live lighting inputs or proof of same-frame renderer scheduling.')
 out=ROOT/'local/event-activation/rider-lighting-glide.json';out.write_text(json.dumps(result,indent=2)+'\n');return result
if __name__=='__main__':
 r=audit();print(json.dumps(dict(references=[x['name']for x in r['references']],local_light_kinds=[x['kind']for x in r['local_lights']],extra=r['control_extra'],selector=r['selector'])))
