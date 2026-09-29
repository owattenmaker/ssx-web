#!/usr/bin/env python3
"""Export source snow-FX configuration and initial state, never recorded poses."""
import argparse,hashlib,json,struct,zipfile
from pathlib import Path
from reference_board_trail import extract_memory as trail_context
from reference_ground_profile import extract_surface_catalog

SURFACE_FIELDS=(('cloud_active',0x50,'I'),('cloud_max_height_cm',0x54,'f'),('trail_active',0x58,'I'),('kicker_capacity',0x5c,'f'),
 ('impact_active',0x60,'I'),('large_impact_active',0x64,'I'),('impact_multiplier',0x68,'f'),('rock_chance',0x6c,'f'),
 ('small_chunk_chance',0x70,'f'),('large_chunk_chance',0x74,'f'),('chunks_use_wake_velocity',0x78,'I'),
 ('min_wake_velocity_scale',0x7c,'f'),('max_wake_velocity_scale',0x80,'f'))

def extract_memory(m,rider=0x14701a0):
    def u(a):return struct.unpack_from('<I',m,a)[0]
    def i(a):return struct.unpack_from('<i',m,a)[0]
    def f(a,n=1):
        values=list(struct.unpack_from('<'+'f'*n,m,a));return values[0]if n==1 else values
    parent=u(rider+0x77c);fx=parent+0xb40
    if u(fx)!=rider:raise ValueError('Unexpected original DynamicSpray owner relationship')
    trail=trail_context(m,rider);context=trail['initial_context'];emitters=u(fx+0x28)
    surfaces=[]
    for s in extract_surface_catalog(m):
        address=int(s['source_address'],16);data={'id':s['surface']['id']}
        for name,offset,kind in SURFACE_FIELDS:
            value=struct.unpack_from('<'+kind,m,address+offset)[0];data[name]=bool(value)if kind=='I'else value
        data['raw_words_48_to_8c']=list(struct.unpack_from('<18I',m,address+0x48));surfaces.append(data)
    state=dict(kicker_buildup=f(fx+0x10),previous_speed_cmps=f(fx+0x74),visibility_mode=i(fx+0x128),impact_surface=i(fx+0x110),
        emitter_enabled=[bool(u(emitters+index*0x210+0x174))for index in range(10)],
        emitter_flipbook_phases=[f(emitters+index*0x210+0x10)for index in range(10)],
        impact=dict(strength=f(fx+0xe0),buildup=f(fx+4),alpha=f(fx+0x120),position_cm=f(fx+0xf0,3),normal=f(fx+0x100,3),kind=bool(u(fx+0x114)),wide_scatter=bool(u(fx+0x124))))
    return dict(schema_version=1,rider=rider,state=state,surfaces=surfaces,
        environment_index=trail['environment_index'],environment_argb=trail['environment_argb'],
        particle_rgba=[2*x for x in trail['environment_argb'][1:]]+[1.0],
        shared_visual_lcg=u(0x4a3afc),particle_random_state=list(struct.unpack_from('<6I',m,0x4ff018)),
        board_bone_index=u(rider+0x8a4),initial_wake_entry_count=u(parent+0x3b4),suppressed=bool(u(u(rider+0x88c)+0xa0)),
        initial_rider=dict(turn=f(rider+0x1f0),brake=f(rider+0x214),secondary_brake274=f(rider+0x274),
            manual_state=u(rider+0x330),animation_semantic=context['semantic'],motion_mode=context['motion'],
            control_state=u(parent+0xde4),reverse=bool(u(rider+0x320)),tracking_inhibited=context['flagAC4'],trackingAD0=context['flagAD0'],trackingAFC=context['flagAFC'],trackingB00=context['flagB00'],visibility_mode=i(rider+0x898)),
        provenance=dict(source_fx=hex(fx),source='2DFB60..2DFDA8;2E23E0;2E2550',surface_fields={name:hex(offset)for name,offset,_ in SURFACE_FIELDS},
            board='Live native-generated geometry+30 unit matrix at rider8A4, not geometry+34 scaled matrix',
            environment='4FA398+environmentIndex*F0 ARGB; source2DFD6C reordersRGB, doublesRGB, forcesA1',
            random='Shared effectsLCG4A3AFC; independent six-word particle seed4FF018; gameplay4FF030 excluded',
            pose_policy='No recorded board matrices, rider velocity or contact trajectories exported. Impact position/normal are only initial effect-state anchors.',
            remaining='Live wake-query list (owner3B0) and environment table lighting updates must come from native world state.'))

def extract(path,rider=0x14701a0):
    with zipfile.ZipFile(path)as z:m=z.read('eeMemory.bin')
    result=extract_memory(m,rider);result['provenance'].update(snapshot=str(path),ee_sha256=hashlib.sha256(m).hexdigest());return result

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('snapshot',type=Path);p.add_argument('--rider',type=lambda x:int(x,0),default=0x14701a0);p.add_argument('--output',type=Path,required=True);a=p.parse_args();result=extract(a.snapshot,a.rider);a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n');print(f'Exported19 source surface FX profiles and initial context for rider{a.rider:#x}')
if __name__=='__main__':main()
