#!/usr/bin/env python3
"""Export verified static original bind matrices with explicit active-bone slots.

Captured animated poses and per-frame skin matrices are never runtime assets.

python3 tools/export_rider_bind_matrices.py                 Zoe (human actor 0x14701A0; historical default)
python3 tools/export_rider_bind_matrices.py --rider psymon  one Snow Jam computer opponent (NPC_PRESETS actor)
python3 tools/export_rider_bind_matrices.py --opponents     all five Snow Jam opponents
"""
import argparse,hashlib,json,math,struct,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
# Human Zoe: the savestates the original export used (unchanged). Opponents: the
# countdown anchor (event start) and the glide snapshot hold the same live actors.
ZOE=dict(actor=0x14701a0,snapshots=('glide','jump-31','jump-90'),slot_count=29,audit='bind-matrix-audit.json')
OPPONENT_SNAPSHOTS=('countdown-anchor','glide')
def multiply(a,b):
 return [sum(a[k*4+r]*b[c*4+k]for k in range(4))for c in range(4)for r in range(4)]
def local_matrix(bone):
 x,y,z,w=bone['source_rotation'];p=bone['source_translation_cm']
 return [1-2*y*y-2*z*z,2*x*y+2*z*w,2*x*z-2*y*w,0,2*x*y-2*z*w,1-2*z*z-2*x*x,2*y*z+2*x*w,0,2*x*z+2*y*w,2*y*z-2*x*w,1-2*x*x-2*y*y,0,*p]
def derive(rig,actor,snapshots,slot_count=None):
 """Read actor+0x780 geometry (bank +0x38, parts +0x0C) in each savestate and map
 every active original bone slot onto the browser skeleton; returns the verified
 static bind rows (identical in every snapshot) and the rest-identity residuals."""
 lookup={(b['file'],b['index']):i for i,b in enumerate(rig['bones'])}
 baseline=None;provenance=[]
 for name in snapshots:
  # name: a snow-jam-NAME.p2s reference state, or a Path (derived per-character states, tools/export_characters.py)
  with zipfile.ZipFile(name if isinstance(name,Path) else ROOT/f'local/reference/pcsx2/snow-jam-{name}.p2s')as archive:m=archive.read('eeMemory.bin')
  u=lambda a:struct.unpack_from('<I',m,a)[0]
  geometry=u(actor+0x780);count=u(geometry+16);parts=u(geometry+12);bank=u(geometry+0x38)
  if slot_count is not None and count!=slot_count:raise ValueError('Unexpected original bone slot count')
  if not len(rig['bones'])<=count<=64:raise ValueError('Original bone slot count outside browser skeleton')
  slots=[None]*len(rig['bones']);inactive=[]
  for i in range(u(geometry+8)):
   part=parts+i*88;file=u(part);base=u(part+4);boneCount=u(part+0x44);bones=u(part+0x38);active=bool(u(part+0x18))
   for j in range(boneCount):
    address=bones+j*80;boneName=m[address:address+16].split(b'\0')[0].decode();slot=base+j
    if slot>=count:raise ValueError('Bone slot outside bank')
    if not active:inactive.append(dict(slot=slot,file=file,index=j,name=boneName));continue
    key=(file,j)
    if key not in lookup:raise ValueError('Active source bone missing in browser')
    index=lookup[key];bone=rig['bones'][index]
    if bone['name']!=boneName or slots[index]is not None:raise ValueError('Bone mapping differs')
    if list(struct.unpack_from('<8I',m,address+32))!=bone['source_translation_bits']+bone['source_rotation_bits']:raise ValueError('Authored bind components differ')
    slots[index]=slot
  if any(s is None for s in slots):raise ValueError('Browser bone not active in original assembly')
  matrices=[list(struct.unpack_from('<16I',m,bank+slot*64))for slot in slots]
  current=dict(slots=slots,matrices=matrices,inactive=inactive,count=count)
  if baseline is not None and current!=baseline:raise ValueError('Static bind/mapping changes across poses')
  baseline=current;provenance.append(dict(snapshot=str(name.relative_to(ROOT)) if isinstance(name,Path) else name,ee_sha256=hashlib.sha256(m).hexdigest()))
 # Independently check meaning: authored rest hierarchy times inverse bind is
 # identity (double precision sanity check; original bit-exact construction is
 # separately recovered, not claimed from this tolerance).
 world=[];max_linear=0;max_translation=0
 for index,bone in enumerate(rig['bones']):
  local=local_matrix(bone);parent=bone['parent'];matrix=local if parent<0 else multiply(world[parent],local);world.append(matrix)
  inverse=list(struct.unpack('<16f',struct.pack('<16I',*baseline['matrices'][index])));product=multiply(matrix,inverse)
  max_linear=max(max_linear,max(abs(product[c*4+r]-(1 if c==r else 0))for c in range(3)for r in range(3)))
  max_translation=max(max_translation,max(abs(x)for x in product[12:15]))
 if max_linear>1e-4 or max_translation>.001:raise ValueError(f'Not authored inverse bind matrices: {max_linear}, {max_translation}')
 return baseline,provenance,max_linear,max_translation
def export(rider='zoe'):
 opponent=rider!='zoe'
 if opponent:
  from rider_assets import NPC_PRESETS
  preset=NPC_PRESETS[rider];config=dict(actor=preset['actor'],snapshots=OPPONENT_SNAPSHOTS,slot_count=None,audit=f'bind-matrix-audit-{rider}.json')
 else:config=ZOE
 package=f'RIDER_{rider.upper()}'
 path=ROOT/f'local/assets/native/{package}/rider.json';rig=json.loads(path.read_text())
 baseline,provenance,max_linear,max_translation=derive(rig,config['actor'],config['snapshots'],config['slot_count'])
 slots=baseline['slots'];count=baseline['count']
 update=dict(source_bone_slots=slots,source_bone_slot_count=count,source_bind_matrix_words=baseline['matrices'],source_bind_matrix_space='source-centimeters-Z-up')
 if opponent:update['source_bind_provenance']=f"live PS2 actor {config['actor']:#x} geometry+0x38 bank, identical in snow-jam-{'/'.join(config['snapshots'])}"
 # Zoe's browser package already exists; opponent packages are written by
 # tools/export_opponent_packages.py, which copies the native rider.json after this.
 target=ROOT/f'web/public/assets/{package}/rider.json';web=json.loads(target.read_text())if target.exists()or not opponent else None
 if web is not None and web['bones']!=rig['bones']:raise ValueError('Browser skeleton differs from native package')
 rig.update(update);path.write_text(json.dumps(rig,indent=2)+'\n')
 if web is not None:web.update(update);target.write_text(json.dumps(web,indent=2)+'\n')
 report=dict(active_bones=len(slots),source_slots=count,active_to_source=slots,inactive=baseline['inactive'],provenance=provenance,max_rest_identity_linear_error=max_linear,max_rest_identity_translation_cm=max_translation,scope='Static bind matrices only; no sampled pose/per-frame skin matrix exported. Renderer hookup remains separate.')
 if opponent:report=dict(rider=rider,actor=hex(config['actor']),**report)
 (ROOT/'local/rider-lighting'/config['audit']).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
 return report
if __name__=='__main__':
 parser=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
 parser.add_argument('--rider',default='zoe',help='zoe (default) or a Snow Jam opponent from rider_assets.NPC_PRESETS')
 parser.add_argument('--opponents',action='store_true',help='export all five Snow Jam opponents')
 args=parser.parse_args()
 if args.opponents:
  from rider_assets import NPC_PRESETS
  for name in NPC_PRESETS:export(name)
 else:export(args.rider)
