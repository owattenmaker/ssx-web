#!/usr/bin/env python3
"""Audit the disposable lighting view capture against the unmodified jump replay."""
import hashlib,json,struct,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'local/rider-lighting'
CAMERA=0x1597d10
ACTOR=0x14701a0

def audit():
 manifest=json.loads((OUT/'rider-view-probe.json').read_text())
 source=Path(manifest['source'])
 if hashlib.sha256(source.read_bytes()).hexdigest()!=manifest['source_sha256']:raise ValueError('Baseline state changed')
 data=(OUT/'rider-view-records.bin').read_bytes();memory=(OUT/'rider-view-memory.bin').read_bytes()
 count=struct.unpack_from('<I',data)[0]
 if not 1<=count<=manifest['capacity']:raise ValueError('Invalid view record count')
 records=[]
 for i in range(count):
  offset=manifest['record_offset']+i*manifest['record_stride']
  tick,actor,pointer,depth=struct.unpack_from('<4I',data,offset)
  if actor!=ACTOR:raise ValueError('Wrong rider captured')
  if records and tick!=records[-1]['tick']+1:raise ValueError('Capture is not consecutive')
  view=list(struct.unpack_from('<16I',data,offset+16))
  records.append(dict(tick=tick,actor=hex(actor),view_pointer=hex(pointer),stack_depth=depth,view_words=view,
                      position_words=list(struct.unpack_from('<4I',data,offset+80)),selected=list(struct.unpack_from('<8I',data,offset+96))))
 last=records[-1];stored=list(struct.unpack_from('<16I',memory,CAMERA+0x40))
 if stored!=last['view_words']:raise ValueError('Final captured view differs from completed camera matrix')
 reference=json.loads((ROOT/f"local/camera-continuous/jump/tick-{last['tick']:08d}.json").read_text())
 original=next(c for c in reference['cameras'] if c['address']==0x1a58650)
 if list(struct.unpack_from('<720I',memory,ACTOR))!=original['rider_words']:raise ValueError('Probe changed captured rider state')
 # Snapshot input/output pairs for native view-kernel verification; never runtime assets.
 fixtures=bytearray();names=[]
 for name in ['glide','jump-31','jump-90']:
  with zipfile.ZipFile(ROOT/f'local/reference/pcsx2/snow-jam-{name}.p2s') as archive:m=archive.read('eeMemory.bin')
  fixtures.extend(m[CAMERA+0x20:CAMERA+0x80]);names.append(name)
 fixtures.extend(memory[CAMERA+0x20:CAMERA+0x80]);names.append('probe-final')
 (OUT/'camera-view-snapshot-fixtures.bin').write_bytes(fixtures)
 report=dict(camera=hex(CAMERA),records=records,last_matrix_matches_camera=True,unchanged_rider_words=720,
             snapshot_fixtures=names,baseline_sha256=manifest['source_sha256'],
             memory_sha256=hashlib.sha256(memory).hexdigest(),
             scope='Five-frame lighting-entry capture. Final matrix matches stored camera; complete rider state matches unmodified replay. Other phases/players not established.')
 (OUT/'rider-view-probe-audit.json').write_text(json.dumps(report,indent=2)+'\n')
 print(f"{count} consecutive lighting views; final view equals camera+40; all720 rider words unchanged; {len(names)} snapshot view fixtures exported.")
if __name__=='__main__':audit()
