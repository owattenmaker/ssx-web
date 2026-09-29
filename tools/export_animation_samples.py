#!/usr/bin/env python3
"""Export original body scalar curves for native pose-decoder validation."""
import argparse
import struct
import hashlib
import json
from pathlib import Path
from animation_bank import read_bank,game_hash
from animation_curves import CurvePacket
from rider_assets import RIDER_NAMES


def original_duration(frame_count):
    """3127F4..312808: (u16 frame_count−1) * float32(1/30), chop.

    The integer and float32 product fit exactly in a Python binary64 before
    directed narrowing (at most16+24 significant bits).
    """
    if not 1<=frame_count<=65535:raise ValueError('Invalid original frame count')
    factor=struct.unpack('<f',struct.pack('<I',0x3d088889))[0]
    exact=(frame_count-1)*factor
    bits=struct.unpack('<I',struct.pack('<f',exact))[0]
    value=struct.unpack('<f',struct.pack('<I',bits))[0]
    return struct.unpack('<f',struct.pack('<I',bits-1))[0] if value>exact else value

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--rider',choices=RIDER_NAMES,default='mac');parser.add_argument('--source',choices=('gamecube','ps2'),default='gamecube');parser.add_argument('--lookup-snapshot',type=Path,default=Path('local/reference/pcsx2/snow-jam-glide.p2s'));args=parser.parse_args()
    data=Path('local/assets/animation/basic.afb' if args.source=='gamecube' else 'local/assets/animation/ps2/basic.afl').read_bytes()
    bank=read_bank(data)
    clips=[];native_clips=[];packet_data=bytearray()
    for name in ('RNORM_FWD_CYC','FL_STAND_CYC','G_NOSE_GRAB_CYC','TW_METHOD_CYC','RCROUCH_FWD_CYC','RAIR_NORM_CYC','RNORM_TURN_HS_1_CYC','RNORM_TURN_TS_1_CYC','HEADCHECK_TS','SH_RIGHT_CYC'):
        animation=next(a for a in bank['animations'] if a['hash']==f'{game_hash(name.encode()):08x}')
        selected=bank['channels'][animation['first_channel']:animation['first_channel']+animation['part_count']*animation['segment_count']]
        parts={};packets=[]
        for c in selected:
            frames=parts.setdefault(str(c['part']),[])
            at=bank['payload_offset']+c['offset']
            raw=bytearray(data[at:at+c['byte_size']]);packet=CurvePacket(raw,c['frame_field'])
            if args.source=='ps2':
                for word in range(packet.offsets[8],packet.offsets[8]+packet.sizes[8],2):raw[word:word+2]=raw[word:word+2][::-1]
                packet=CurvePacket(raw,c['frame_field'])
            packets.append(dict(part=c['part'],offset=len(packet_data),size=c['byte_size'],frames=c['frame_field']))
            packet_data.extend(raw)
            frames.extend(packet.sample(f) for f in range(1 if frames else 0,packet.frames))
        for frames in parts.values():
            if len(frames)!=animation['frame_count_field']:raise ValueError('Animation segment joins do not match frame count')
        if '0' in parts and len(parts['0'][0])!=69:raise ValueError('Unexpected main body channel count')
        native_clips.append(dict(name=name,id=bank['animations'].index(animation)<<8,duration=original_duration(animation['frame_count_field']),packets=packets))
        if '0' in parts:clips.append(dict(name=name,fps=30,frames=parts['0'],parts=parts))
    # Keep every original bank entry available to the native state machine;
    # only the small inspector clip list needs expanded integer-frame samples.
    exported={c['id'] for c in native_clips}
    for index,animation in enumerate(bank['animations']):
        if index<<8 in exported:continue
        packets=[]
        for c in bank['channels'][animation['first_channel']:animation['first_channel']+animation['part_count']*animation['segment_count']]:
            at=bank['payload_offset']+c['offset'];raw=bytearray(data[at:at+c['byte_size']])
            if args.source=='ps2':
                decoded=CurvePacket(raw,c['frame_field'])
                for word in range(decoded.offsets[8],decoded.offsets[8]+decoded.sizes[8],2):raw[word:word+2]=raw[word:word+2][::-1]
            packets.append(dict(part=c['part'],offset=len(packet_data),size=c['byte_size'],frames=c['frame_field']))
            packet_data.extend(raw)
        native_clips.append(dict(name=animation['hash'],id=index<<8,duration=original_duration(animation['frame_count_field']),packets=packets))
    for clip in native_clips:
        clip['event_times']=[original_duration(event[0]+1) for event in bank['animations'][clip['id']>>8]['events']]
    output=Path(f'local/assets/native/RIDER_{args.rider.upper()}/animation-samples.json')
    def cycle_ids(names):
        return [next(i<<8 for i,a in enumerate(bank['animations']) if a['hash']==f'{game_hash(n.encode()):08x}') for n in names]
    cycle_maps={}
    for semantic,prefix in [(5,'RNORM'),(6,'RCROUCH'),(8,'RFAST'),(14,'RICE'),(15,'RICECROUCH')]:
        names=(["RICE_HS_2_CYC","RICE_HS_1_CYC",prefix+"_FWD_CYC","RICE_TS_1_CYC","RICE_TS_2_CYC"] if semantic in (14,15) else [prefix+'_TURN_HS_2_CYC',prefix+'_TURN_HS_1_CYC',prefix+'_FWD_CYC',prefix+'_TURN_TS_1_CYC',prefix+'_TURN_TS_2_CYC'])
        cycle_maps[str(semantic)]=cycle_ids(names) #104178: ice shares side cycles32/31/29/30
    three_way_maps={'7':cycle_ids(['RCROUCHSLOW_TURN_HS_CYC','RCROUCHSLOW_FWD_CYC','RCROUCHSLOW_TURN_TS_CYC']),'11':cycle_ids(['R_BRAKE_HS2_CYC','R_BRAKE_FWD_CYC','R_BRAKE_TS2_CYC'])}
    three_way_maps.update({'9':cycle_ids(['RAIR_HS_CYC','RAIR_NORM_CYC','RAIR_TS_CYC']),'10':cycle_ids(['RAIR_HS_CYC','RAIR_CROUCH_CYC','RAIR_TS_CYC'])})
    # Rail balance cycles (kind5 driver 0x104238 -> 0x103CC8): {negative,centre,positive} = {BAL_R,FWD,BAL_L} on rider+238,
    # negated by the switch flag. See engine/RAIL_ANIMATION_RECOVERY.md.
    three_way_maps.update({'18':cycle_ids(['RS_BAL_R_CYC','RS_FWD_CYC','RS_BAL_L_CYC']),'19':cycle_ids(['RSFS_BAL_R_CYC','RSFS_FWD_CYC','RSFS_BAL_L_CYC']),'20':cycle_ids(['RSBS_BAL_R_CYC','RSBS_FWD_CYC','RSBS_BAL_L_CYC'])})
    elf=Path('local/disc/SLUS_207.72').read_bytes()
    from inspect_disc import EXPECTED_SHA1
    if hashlib.sha1(elf).hexdigest()!=EXPECTED_SHA1:raise ValueError('Unexpected original state-definition executable')
    def elf_at(address,size):
        table=struct.unpack_from('<I',elf,28)[0];stride,count=struct.unpack_from('<2H',elf,42)
        for i in range(count):
            kind,offset,virtual,_,length=struct.unpack_from('<5I',elf,table+i*stride)
            if kind==1 and virtual<=address and address+size<=virtual+length:return elf[offset+address-virtual:offset+address-virtual+size]
        raise ValueError('State definition outside original executable segments')
    # Original311710 lookup leaves95..117 are single, unconditional choices.
    prewind_names=('PW_JUMP', 'PW_SPIN_BS', 'PW_SPIN_FS', 'PW_FLIP_BWD', 'PW_FLIP_FWD', 'PW_MISTY_BS', 'PW_MISTY_FS', 'PW_RODEO_BS', 'PW_RODEO_FS', 'PW_RS_SPIN_BS', 'PW_RS_SPIN_FS', 'PW_RS_FLIP_BWD', 'PW_RS_FLIP_FWD', 'PW_RSBS', 'PW_RSBS_SPIN_BS', 'PW_RSBS_SPIN_FS', 'PW_RSBS_FLIP_BWD', 'PW_RSBS_FLIP_FWD', 'PW_RSFS', 'PW_RSFS_SPIN_BS', 'PW_RSFS_SPIN_FS', 'PW_RSFS_FLIP_BWD', 'PW_RSFS_FLIP_FWD')
    release_names=('A_INTO_AIR','A_INTO_SPIN_BS','A_INTO_SPIN_FS','A_INTO_FLIP_BWD','A_INTO_FLIP_FWD','A_INTO_MISTY_BS','A_INTO_MISTY_FS','A_INTO_RODEO_BS','A_INTO_RODEO_FS','A_INTO_RSBS_AIR','A_INTO_RSBS_SPIN_BS','A_INTO_RSBS_SPIN_FS','A_INTO_RSBS_FLIP_BWD','A_INTO_RSBS_FLIP_FWD','A_INTO_RSFS_AIR','A_INTO_RSFS_SPIN_BS','A_INTO_RSFS_SPIN_FS','A_INTO_RSFS_FLIP_BWD','A_INTO_RSFS_FLIP_FWD')
    air_cycle_names=('A_SPIN_BS_CYC','A_SPIN_FS_CYC','A_FLIP_BWD_CYC','A_FLIP_FWD_CYC','A_MISTY_BS_CYC','A_MISTY_FS_CYC','A_RODEO_BS_CYC','A_RODEO_FS_CYC')
    grab_names=('G_CHICKEN_SALAD','G_INDY','G_LEIN','G_MELANCHOLY','G_METHOD','G_MUTE','G_NOSE_GRAB','G_SEATBELT','G_SHIFTY','G_SPAGHETTI','G_STALEFISH','G_STALEMASKY','G_STIFFY','G_SWISS_CHEESE','G_TAIL_GRAB','G_INDY_CYC','G_METHOD_CYC','G_MUTE_CYC','G_STALEFISH_CYC','G_NOSE_GRAB_CYC','G_TAIL_GRAB_CYC')
    state_properties=[]
    for semantic in range(437):
        cls,kind,callback,channel,blend,first_fade,end_fade=struct.unpack('<4I3f',elf_at(0x446990+semantic*28,28))
        state_properties.append(dict(semantic=semantic,animation_class=cls,kind=kind,completion_kind=callback,channel=channel,blend_seconds=blend,first_fade_in=first_fade,end_fade_out=end_fade))
    weighted=(62,314,315,319,320,321) if args.source=='ps2' else ()
    animation_variants={};lookup_provenance=None
    if args.source=='ps2':
        import zipfile
        from reference_animation_variants import extract_animation_variants
        memory=zipfile.ZipFile(args.lookup_snapshot).read('eeMemory.bin')
        authored=extract_animation_variants(memory)
        table=struct.unpack_from('<I',memory,0x4a30f0+0xd0c)[0]
        # Board-press drivers (kinds 13/14 0x1047F0/0x104728 seek the single authored leaf,
        # kind 15 0x1046B0 blends it as the centre of a 0x103CC8 three-way) play their
        # authored 0x311710 leaf like kinds 0..2; see docs/attack-boardpress-recovery.md.
        simple=[p['semantic'] for p in state_properties if (p['kind'] in (0,1,2,13,14,15) or p['semantic']==0) and authored.get(str(p['semantic'])) and all(v['clip'] is None or v['clip']&255==0 for v in authored[str(p['semantic'])])]
        # 0x1046B0 leaves {negative,centre,positive}: 28 -> 49/48/50, 36 -> 60/58/59, packed ids
        # from the loaded lookup *(gp+0xD8C)+0x1030+leaf*4 (the same table as the variant clips).
        packed_lookup=struct.unpack_from('<I',memory,0x4a30f0+0xd8c)[0]+0x1030
        for semantic,leaves in ((28,(49,48,50)),(36,(60,58,59))):
            if state_properties[semantic]['kind']!=15:raise ValueError('Board-press hold semantic is not kind 15')
            three_way_maps[str(semantic)]=[struct.unpack_from('<I',memory,packed_lookup+leaf*4)[0] for leaf in leaves]
            if three_way_maps[str(semantic)][1]!=authored[str(semantic)][0]['clip']:raise ValueError('Kind-15 centre leaf differs from the authored clip')
        for semantic in sorted(set(weighted)|set(simple)):
            first,count=struct.unpack_from('<2h',memory,table+semantic*4)
            if semantic in weighted and count<=1:raise ValueError('Expected weighted original animation')
            if memory[0x449960+first*12:0x449960+(first+count)*12]!=elf_at(0x449960+first*12,count*12):raise ValueError('Runtime variant records differ from original executable')
            animation_variants[str(semantic)]=authored[str(semantic)]
        lookup_provenance=dict(snapshot=str(args.lookup_snapshot),ee_sha256=hashlib.sha256(memory).hexdigest(),elf_sha1=EXPECTED_SHA1,verified='Every selected leaf/weight/flags record equals original ELF449960; packed IDs from original loaded lookup')
    adjust_primary=[135,141,138,142,136,140,137,139]
    adjust_secondary=[143,149,146,150,144,148,145,147]
    state_definitions=[]
    for semantic in (5,6,7,8,9,10,11,12,13,14,15,16,17,21,22,55,56,57,58,59,60,61,63,64,65,66,67,*range(71,92),*range(245,268),*range(268,287),287,*range(289,297),*range(297,305),305,316,317,*weighted):
        cls,kind,flags,channel,blend,first_fade,end_fade=struct.unpack('<4I3f',elf_at(0x446990+semantic*28,28))
        initial=(adjust_primary[semantic-297]<<8) if 297<=semantic<=304 else 0xffffffff if semantic in weighted else cycle_maps[str(semantic)][2] if str(semantic) in cycle_maps else three_way_maps[str(semantic)][1] if str(semantic) in three_way_maps else cycle_ids([grab_names[semantic-71] if 71<=semantic<=91 else prewind_names[semantic-245] if 245<=semantic<=267 else release_names[semantic-268] if 268<=semantic<=286 else air_cycle_names[semantic-289] if 289<=semantic<=296 else {12:'R_BRAKE_TS_STOP_CYC',13:'R_BRAKE_HS_STOP_CYC',16:'RNORM_BOB_HS_1_CYC',17:'RNORM_BOB_TS_1_CYC',22:'R_GET_GOING',55:'B_FROM_BEHIND',56:'B_FROM_FRONT',57:'B_FROM_TS',58:'B_HARD_FROM_TS',59:'B_FROM_HS',60:'B_HARD_FROM_HS',21:'R_SWITCH',64:'L_SWITCH',65:'L_SWITCH_HARD',61:'L_NORMAL',63:'L_HARD',66:'L_SPIN_180_CCW',67:'L_SPIN_180_CW',245:'PW_JUMP',268:'A_INTO_AIR',287:'A_CYC_1',305:'A_OUTOF_AIR',316:'HEADCHECK_TS',317:'HEADCHECK_HS'}[semantic]])[0]
        state_definitions.append(dict(semantic=semantic,animation_class=cls,kind=kind,completion_kind=flags,channel=channel,blend_seconds=blend,first_fade_in=first_fade,end_fade_out=end_fade,initial_clip=initial,followup_clip=(adjust_secondary[semantic-297]<<8) if 297<=semantic<=304 else cycle_ids(["A_CYC_2"])[0] if semantic==287 else 0))
    # Rail kind-5 cycles (0x104238): the centre clip doubles as the state's initial clip for
    # 0x312790 durations (previously patched into the packaged file by hand).
    for semantic in (18,19,20):
        state_properties[semantic].update(initial_clip=three_way_maps[str(semantic)][1],rail_cycle_note='0x104238 kind-5 three-way balance cycle {BAL_R,FWD,BAL_L} on rider+238 (RAIL_ANIMATION_RECOVERY.md)')
    (output.parent/'animation-packets.bin').write_bytes(packet_data)
    (output.parent/'animation-packets.json').write_text(json.dumps(dict(source=args.source,source_sha256=bank['source_sha256'],clips=native_clips,cycle_maps=cycle_maps,three_way_maps=three_way_maps,state_definitions=state_definitions,state_properties=state_properties,animation_variants=animation_variants,variant_lookup_provenance=lookup_provenance,state_definition_elf_sha1=EXPECTED_SHA1),indent=2)+'\n')
    output.write_text(json.dumps(dict(source_sha256=bank['source_sha256'],clips=clips,
        note='Original scalar curves. 30fps cadence and spherical quaternion conversion were recovered from the PS2 executable. This preview still needs original-engine pose comparison.'),separators=(',',':'))+'\n')
    print([(c['name'],len(c['frames'])) for c in clips])


if __name__=='__main__':main()
