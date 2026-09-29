#!/usr/bin/env python3
"""Inspect original NPC controller initialization and authored AI-path data.

Unknown AI words are diagnostic only. They are not executable native state and
are never interpreted as the human input recorder's pointer layout.
"""
import argparse, hashlib, json, math, struct, zipfile
from pathlib import Path
from reference_ground_profile import extract_ground
from reference_npc_air import extract_npc_grabs


def extract_npc_relationships(memory):
    def read(fmt, at):
        size=struct.calcsize('<'+fmt)
        if not 0<=at<=len(memory)-size: raise ValueError('NPC relationship pointer outside memory')
        return struct.unpack_from('<'+fmt,memory,at)[0]
    entries=[read('i',0x5305b0+slot*4) for slot in range(6)]
    if any(not 0<=entry<16 for entry in entries): raise ValueError('NPC relationship roster index')
    characters=[read('b',0x535b20+entry*28+17) for entry in entries]
    if any(not 0<=character<10 for character in characters): raise ValueError('NPC relationship character index')
    banks=[2 if read('i',0x535b20+entry*28+12)==-1 else read('I',0x535b20+entry*28+16)&1 for entry in entries]
    first=next((slot for slot,entry in enumerate(entries) if read('i',0x534fe0+entry*28+12)>=0),None)
    if first is None: raise ValueError('Original145750 requires a configured persistent human')
    human=read('b',0x534fe0+entries[first]*28+17)
    event=read('i',0x535c08)
    if not 0<=event<128: raise ValueError('NPC relationship event index')
    kind=read('i',0x43d950+event*100+0x54)
    rival=(5 if human==3 else 3) if kind==0 else (4 if human==7 else 7) if kind==1 else (6 if human==8 else 8) if kind==2 else 3
    scores=[[3 if own==rival else read('b',0x4a6ca8+banks[peer]*0x9b50+characters[peer]*0xf88+own*3+0xbc2) for peer in range(6)] for own in characters]
    return dict(scores=scores,characters=characters,target_banks=banks,persistent_human_character=human,
        event_kind=kind,rival_character=rival,source='155B50;14A080/14A0E0 roster;145750 rival;146E98/147398 persistent human;144C78 event kind')


def extract_npcs(memory, authored=None):
    def read(fmt, at):
        size = struct.calcsize('<' + fmt)
        if not 0 <= at <= len(memory) - size:
            raise ValueError('NPC pointer outside EE memory')
        return struct.unpack_from('<' + fmt, memory, at)
    def u(at): return read('I', at)[0]
    def i(at): return read('i', at)[0]
    def f(at):
        value = read('f', at)[0]
        if not math.isfinite(value): raise ValueError('Nonfinite NPC scalar')
        return value
    def vector(at, n=3): return [f(at + k * 4) for k in range(n)]
    world = u(u(0x4a30f0 - 0x848) + 0x84)
    game = u(world + 12)
    count = u(game + 0x78)
    if not 1 <= count <= 6: raise ValueError('Invalid NPC roster count')
    path_count, path_base = u(0x4d33a8), u(0x4d33ac)
    if not 0 < path_count <= 100000: raise ValueError('Invalid AI path count')
    paths = []
    for index in range(path_count):
        at = path_base + index * 64
        if u(at + 0x34) != 0x481478: raise ValueError('Unexpected AI path interface')
        event_count, event_base = u(at), u(at + 4)
        segment_count, segment_base = u(at + 8), u(at + 0x18)
        if event_count > 10000 or not 0 < segment_count <= 100000: raise ValueError('Invalid AI path extent')
        paths.append(dict(index=index, origin=vector(at+12), low=vector(at+0x1c), high=vector(at+0x28),
            segments=[vector(segment_base+n*16, 4) for n in range(segment_count)],
            events=[dict(zip(('type','value','start','end'), read('IIff', event_base+n*16))) for n in range(event_count)],
            flags38=u(at+0x38), field3c=u(at+0x3c)))
    marker_count, marker_base = u(0x4d33a0), u(0x4d33a4)
    track_count, track_base = u(0x4d33b0), u(0x4d33b4)
    if marker_count > 10000: raise ValueError('Invalid AI marker count')
    markers = []
    for index in range(marker_count):
        at = marker_base + index * 40
        track_pointer, ai_pointer = u(at+32), u(at+36)
        track_index, ai_index = (track_pointer-track_base)//60, (ai_pointer-path_base)//64
        if not 0 <= track_index < track_count or track_pointer != track_base+track_index*60: raise ValueError('Marker track pointer outside bank')
        if not 0 <= ai_index < path_count or ai_pointer != path_base+ai_index*64: raise ValueError('Marker AI pointer outside bank')
        markers.append(dict(index=index, id=u(at), runtime_flags=u(at+4), position=vector(at+8),
            direction=vector(at+20),track_path_index=track_index,ai_path_index=ai_index))
    authored_check = None
    if authored:
        source = authored['ai_paths']
        if len(source) != len(paths): raise ValueError('Authored/runtime AI path count differs')
        if len(authored['regions']) != len(markers): raise ValueError('Authored/runtime marker count differs')
        for raw, live in zip(authored['regions'], markers):
            if raw[0] != live['id'] or raw[2:5] != live['position'] or raw[5:8] != live['direction'] or raw[8] != live['ai_path_index'] or raw[9] != live['track_path_index']:
                raise ValueError('Authored/runtime marker data differs')
        mapping = {}
        for raw, live in zip(source, paths):
            for left, right in [('position','origin'),('low','low'),('high','high'),('segments','segments')]:
                if raw[left] != live[right]: raise ValueError(f'AI path{live["index"]} authored geometry differs: {left}')
            if raw['fields'][3] != live['flags38'] or raw['fields'][6] != live['field3c']:
                raise ValueError('AI authored/runtime flags differ')
            if len(raw['events']) != len(live['events']): raise ValueError('AI event count differs')
            for a,b in zip(raw['events'],live['events']):
                if any(a[k] != b[k] for k in ('value','start','end')): raise ValueError('AI event geometry/value differs')
                if a['type'] in mapping and mapping[a['type']] != b['type']: raise ValueError('Ambiguous AI event type mapping')
                mapping[a['type']] = b['type']
        authored_check = dict(all_geometry_and_flags_exact=True, path_count=len(paths),marker_geometry_and_links_exact=True,marker_count=len(markers),
            event_type_mapping={str(k):v for k,v in sorted(mapping.items())},
            note='Type pairs observed consistently across this authored/live asset; controller event semantics remain separate.')
    riders = []
    participant_routes = []
    for slot in range(count):
        rider = u(game+0x28+slot*4)
        owner = u(rider+0x77c)
        interface = u(owner+0xde8)
        ai_path, cache = u(rider+0xab8), u(rider+0xabc)
        path_index = (ai_path-path_base)//64
        if not 0 <= path_index < path_count or ai_path != path_base+path_index*64: raise ValueError('Participant path outside AI bank')
        human = u(interface+12) == 0x127998
        participant_routes.append(dict(slot=slot,human=human,path_index=path_index,
            cache=dict(origin=vector(cache),distance=f(cache+0x10),segment=i(cache+0x14)),
            closest_point=vector(rider+0x490),closest_w=f(rider+0x49c),
            lookahead_point=vector(rider+0x4a0),lookahead_w=f(rider+0x4ac),
            previous_lookahead_point=vector(rider+0x4b0),previous_lookahead_w=f(rider+0x4bc),
            previous_distance=f(rider+0x4c0),current_distance=f(rider+0x4c4),
            lateral_distance=f(rider+0x4c8),heading=f(rider+0x4cc)))
        if human: continue
        if interface != 0x4585f0 or u(interface+12) != 0x10a768: raise ValueError('Unknown original NPC provider')
        if u(owner+0x18) != rider: raise ValueError('NPC owner/rider link differs')
        ai_path, cache = u(rider+0xab8), u(rider+0xabc)
        path_index = (ai_path-path_base)//64
        if not 0 <= path_index < path_count or ai_path != path_base+path_index*64: raise ValueError('NPC path outside AI bank')
        ground = extract_ground(memory,rider)
        follow_slot = i(rider+0xf8) if u(rider+0xf0) else None
        followed_path = None
        if follow_slot is not None:
            if not 0 <= follow_slot < count: raise ValueError('NPC followed slot outside roster')
            followed_pointer = u(u(game+0x28+follow_slot*4)+0xab8)
            followed_path = (followed_pointer-path_base)//64
            if not 0 <= followed_path < path_count or followed_pointer != path_base+followed_path*64: raise ValueError('NPC followed path outside AI bank')

        adjustment, vindex, function = read('hhI',owner+0xf44)
        if vindex >= 0: raise ValueError('Virtual NPC behavior delegate not yet decoded')
        riders.append(dict(slot=slot, rider=f'0x{rider:08x}', motion_owner=f'0x{owner:08x}',
            actor_interface=f'0x{u(rider+0x6c0):08x}',provider_interface=f'0x{interface:08x}',
            provider='0x0010a768',motion_mode=i(owner+0xde0),control_state=i(owner+0xde4),
            character_id=ground['provenance']['character_id'],ground=ground,
            ai_path_index=path_index,progress4c0=f(rider+0x4c0),progress4c4=f(rider+0x4c4),
            ai_path_cache=dict(origin=vector(cache),distance=f(cache+0x10),segment=i(cache+0x14)),
            route_state=dict(closest_point=vector(rider+0x490),lookahead_point=vector(rider+0x4a0),previous_lookahead_point=vector(rider+0x4b0),lateral_distance=f(rider+0x4c8),heading=f(rider+0x4cc)),
            score_state=dict(role_e00=read('h',owner+0xe00)[0],allow_flag0_e04=bool(u(owner+0xe04)),
                randomize_e08=bool(u(owner+0xe08)),followed_participant_slot=follow_slot,followed_path_index=followed_path),
            crouch_parameter_df8=f(owner+0xdf8),
            behavior=dict(this_adjust=adjustment,virtual_index=vindex,function=f'0x{function:08x}'),
            grab_catalog=extract_npc_grabs(memory,rider),
            driving_state=dict(long_flight_ticks484=i(rider+0x484),desired_speed_df0=f(owner+0xdf0),parameter_df8=f(owner+0xdf8),parameter_dfc=f(owner+0xdfc),
                board_timer_e40=i(owner+0xe40),target_peer_e70=i(owner+0xe70),off_route_ticks_e74=i(owner+0xe74),opposite_heading_ticks_e78=i(owner+0xe78),
                defensive_timer_f30=f(owner+0xf30),defensive_decision_f34=i(owner+0xf34),last_attack_tick_f4=i(rider+0xf4),behavior_counter_f38=read('h',owner+0xf38)[0],
                region_start_e50=vector(owner+0xe50),region_end_e60=vector(owner+0xe60),
                trick=dict(index_e0c=i(owner+0xe0c),enabled_e10=i(owner+0xe10),decision_e14=i(owner+0xe14),phase_e18=i(owner+0xe18),kind_e1c=i(owner+0xe1c),field_e20=i(owner+0xe20),field_e24=i(owner+0xe24),field_e28=i(owner+0xe28),release_e2c=f(owner+0xe2c),start_e30=f(owner+0xe30),normal_counts=[i(owner+0xe7c+n*4) for n in range(15)],uber_counts=[i(owner+0xeb8+n*4) for n in range(15)],tweak_counts=[i(owner+0xef4+n*4) for n in range(15)],remaining_e34=f(owner+0xe34),spin_e38=f(owner+0xe38),flip_e3c=f(owner+0xe3c))),
            pacing_state=dict(negative_threshold_dc=f(rider+0xdc),positive_threshold_e0=f(rider+0xe0),mode_e4=i(rider+0xe4)),
            accepted_command_history=None,
            diagnostic_ai_words={f'{offset:03x}':f'0x{u(owner+offset):08x}' for offset in range(0xdf0,0xf40,4)}))
    return dict(riders=riders,participant_routes=participant_routes,event_variant=read('b',0x535c11)[0],ai_paths=paths,markers=markers,relationships=extract_npc_relationships(memory),authored_verification=authored_check,
        provenance=dict(total_ticks=i(game+8), path_count=path_count,path_base=f'0x{path_base:08x}',
            path_stride=64,ai_path_interface='0x00481478',
            human_recorder_layout_does_not_apply=True,npc_extension='owner+DF0..F40:150-byte serialized data block',
            provider='10A768: common pacing, modifier, then control-state dispatch',
            behavior_delegate='owner+F44: signed16 this-adjust, signed16 virtual index, uint32 target',
            scope='Inspection/fixture data; not a completed native AI driver. Unknown words remain diagnostic only.'))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('snapshot',type=Path)
    parser.add_argument('--authored-aip',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    with zipfile.ZipFile(args.snapshot) as archive: memory=archive.read('eeMemory.bin')
    authored=json.loads(args.authored_aip.read_text()) if args.authored_aip else None
    result=dict(snapshot=str(args.snapshot.resolve()),ee_sha256=hashlib.sha256(memory).hexdigest(),original_npcs=extract_npcs(memory,authored))
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps(dict(riders=[{k:r[k] for k in ('slot','rider','character_id','control_state','ai_path_index','behavior')} for r in result['original_npcs']['riders']],authored_verification=result['original_npcs']['authored_verification']),indent=2))

if __name__=='__main__': main()
