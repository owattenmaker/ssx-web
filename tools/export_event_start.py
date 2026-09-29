#!/usr/bin/env python3
"""Export a verified original countdown/grid fixture with real rider identities."""
import argparse,hashlib,json,struct,zipfile
from pathlib import Path
from reference_race_event import extract_race_event
from reference_ground_profile import extract_ground
from locations import state as location_state
ROOT=Path(__file__).resolve().parents[1]
def main():
 # --location X: X's countdown savestate, local/browser-validation/X/countdown-rider-assemblies.json
 # (audit_rider_assemblies.py --discover), web/public/assets/X/initial.json paths, native X/event-start.json.
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--location',default='ARA1');p.add_argument('--snapshot',type=Path);p.add_argument('--assemblies',type=Path);p.add_argument('--output',type=Path);a=p.parse_args()
 X=a.location;a.snapshot=a.snapshot or location_state(X,'countdown');a.output=a.output or ROOT/f'local/assets/native/{X}/event-start.json'
 a.assemblies=a.assemblies or ROOT/('local/browser-validation/countdown-rider-assemblies.json' if X=='ARA1' else f'local/browser-validation/{X}/countdown-rider-assemblies.json')
 memory=zipfile.ZipFile(a.snapshot).read('eeMemory.bin');digest=hashlib.sha256(memory).hexdigest();audit=json.loads(a.assemblies.read_text())
 if audit['ee_sha256']!=digest:raise ValueError('Rider assembly audit belongs to another snapshot')
 race=extract_race_event(memory);clock=race['clock']
 if clock['phase']!=4 or clock['race_ticks']!=0 or len(race['participants'])!=(6 if X=='ARA1' else len(audit['participants'])) or not race['participants']:raise ValueError('Fixture is not a countdown with the audited roster')
 course=json.loads((ROOT/('web/public/assets/ANIMATIONS/initial.json' if X=='ARA1' else f'web/public/assets/{X}/initial.json')).read_text())['original_race_event']['original_race_event']
 if course['provenance']['course_sha256']!=race['provenance']['course_sha256']:raise ValueError('Countdown course differs from current event paths')
 u=lambda at:struct.unpack_from('<I',memory,at)[0];game=int(race['provenance']['game_address'],16);participants=[]
 for actor,assembly in zip(race['participants'],audit['participants']):
  slot=actor['index'];address=u(game+0x28+slot*4)
  if assembly['slot']!=slot or int(assembly['rider_address'],0)!=address:raise ValueError('Rider identity/roster mismatch')
  if actor['control_state']!=6 or actor['motion_mode']!=3 or any(actor['velocity']):raise ValueError('Rider is not held on the starting grid')
  owner=u(address+0x77c);phase,steady,low,high,pose,rider=struct.unpack_from('<I4fI',memory,owner+0x290)
  if rider!=address or phase!=0:raise ValueError('Unexpected start controller identity/phase')
  ground=extract_ground(memory,address)
  if ground['provenance']['character_id']!=assembly['gameplay_character_id']:raise ValueError('Rider gameplay identity differs from audited model')
  participants.append(dict(slot=slot,character=assembly['character'],gameplay_character_id=assembly['gameplay_character_id'],render_package='RIDER_'+assembly['character'].upper(),race=actor,original_ground=ground,start_control=dict(phase=phase,steady_time=steady,low=low,high=high,pose=pose),progress_origin=struct.unpack_from('<f',memory,address+0x4d8)[0],start_delay_seconds=struct.unpack_from('<i',memory,address+0xb30)[0],reference_stance=u(address+0x324)))
 result=dict(version=1,location=X,clock=clock,participants=participants,configuration=race['provenance']['configuration_bytes'],course_sha256=race['provenance']['course_sha256'],provenance=dict(snapshot=str(a.snapshot),ee_sha256=digest,assembly_audit_sha256=hashlib.sha256(a.assemblies.read_bytes()).hexdigest(),purpose='Captured countdown grid and source profiles; not a replay or a complete fresh-start initializer'))
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(output=str(a.output),clock=clock,riders=[x['character'] for x in participants]),indent=2))
if __name__=='__main__':main()
