#!/usr/bin/env python3
"""Extract data-only original ground physics parameters from an owned snapshot.

Offsets/getter equations are verified against PS2 USA SLUS_207.72. No original
instructions or guest memory are included in the exported native profile.
"""
import math
import struct


def extract_surface_catalog(memory):
    """The race's 19 authored 0xB0-byte surface records, including raw fields."""
    def read(fmt,at):
        if not 0<=at<=len(memory)-struct.calcsize(fmt):raise ValueError('Surface catalog outside EE memory')
        return struct.unpack_from(fmt,memory,at)
    def u(at):return read('<I',at)[0]
    race=u(u(0x4a30f0-0x848)+0x84);base=u(race+0x44);result=[]
    for index in range(19):
        at=base+index*0xb0
        def f(offset):
            value=read('<f',at+offset)[0]
            if not math.isfinite(value):raise ValueError('Nonfinite original surface field')
            return value
        result.append(dict(surface=dict(id=index,gravity=f(0),lateral_drag=f(8),powder_damping=f(0x1c),
                                        slip_friction=[[f(0x90+n*8),f(0x94+n*8)] for n in range(4)]),
                           depth_target1=f(0x14),depth_target3=f(0x18),max_turn_angle=f(0xc),air_height=f(0x10),
                           auto_boost_speed=f(0x20),auto_boost_factor=f(0x24),alignment_rate=f(0x38),
                           surface_terminal_velocity=f(4),heading_profile=dict(surface28=f(0x28),surface2c=f(0x2c),surface30=f(0x30),surface34=f(0x34)),
                           property44=read('<i',at+0x44)[0],source_address=hex(at),raw_words=list(read('<44I',at))))
    return result


def extract_ground(memory,base):
    def read(fmt,address):
        if not 0<=address<=len(memory)-struct.calcsize(fmt):raise ValueError('Ground profile pointer outside EE memory')
        return struct.unpack_from(fmt,memory,address)
    def u(address):return read('<I',address)[0]
    def i(address):return read('<i',address)[0]
    def f(address):
        value=read('<f',address)[0]
        if not math.isfinite(value):raise ValueError('Nonfinite original ground field')
        return value
    def vec(address):return [f(address+n*4) for n in range(3)]
    def curve(address):return [[f(address+n*8),f(address+n*8+4)] for n in range(4)]
    def control(offset):return dict(current=f(base+offset),rate=f(base+offset+4),target=f(base+offset+8))
    def byte(address):return read('<b',address)[0]
    gp=0x4a30f0
    owner=u(base+0x77c);race=u(u(gp-0x848)+0x84)
    surface_id=i(base+0x438)
    if not 0<=surface_id<256:raise ValueError('Unexpected surface index')
    surface=u(race+0x44)+surface_id*0xb0
    slot=i(base+0x86c)
    if not 0<=slot<16:raise ValueError('Unexpected rider slot')
    entry=i(0x5305b0+slot*4);record=0x535b20+entry*28
    character=byte(record+17)
    bank=2 if i(record+12)==-1 else u(record+16)&1
    override=i(base+0xb34)
    def stat(progress_offset,maximum_offset):
        if u(0x534fe0+entry*28+16)&4:return .5
        numerator=override if override>0 else int(byte(0x535538+bank*70+character*7+progress_offset)/5)
        denominator=byte(0x5308d8+character*15+maximum_offset)
        if denominator<=0:raise ValueError('Invalid original stat divisor')
        exact=numerator/denominator
        # These getters use EE DIV.S, not VU division. The reference PCSX2
        # configuration uses FPUDiv nearest even while ordinary FPU/VU ops chop.
        return struct.unpack('<f',struct.pack('<f',exact))[0]
    profile=dict(surface=dict(id=surface_id,gravity=f(surface),lateral_drag=f(surface+8),
                     powder_damping=f(surface+0x1c),slip_friction=curve(surface+0x90)),
        extra_lean_curve=curve(u(gp-0x1fc8)),lateral_speed_curve=curve(u(gp-0x1fb0)),turn_min_curve=curve(u(gp-0x1fa8)),turn_max_curve=curve(u(gp-0x1fa0)),
        depth_target1=f(surface+0x14),depth_target3=f(surface+0x18),max_turn_angle=f(surface+0xc),air_height=f(surface+0x10),
        auto_boost_speed=f(surface+0x20),auto_boost_factor=f(surface+0x24),
        alignment_rate=f(surface+0x38),
        surface_terminal_velocity=f(surface+4),speed_limit_table=[f(0x4a6310+n*4) for n in range(48)],top_speed_stat=stat(0,8),
        heading_profile=dict(surface28=f(surface+0x28),surface2c=f(surface+0x2c),surface30=f(surface+0x30),surface34=f(surface+0x34),
            crouch_turn_curve=curve(u(gp-0x1fc0)),crouch_speed_curve=curve(u(gp-0x1fb8)),direct_steer_curve=curve(u(gp-0x1f98))),
        body_scale=f(u(base+0x780)+0x140),speed_limit=f(base+0x2e4),speed_stat=stat(1,9),edge_stat=stat(3,11))
    state=dict(position=vec(base+0x110),velocity=vec(base+0x1e0),normal=vec(base+0x370),previous_normal=vec(base+0x380),board_normal=vec(base+0x390),presentation_up=vec(base+0x180),
        quaternion=[f(base+0x120+n*4) for n in range(4)],board_up=vec(base+0x1c0),manual_spin=f(base+0x2dc),
        prewind_style=i(base+0x328),control_state=i(owner+0xde4),reverse_stance=bool(i(base+0x320)),
        board_bounce_phase=f(owner),board_lift=f(base+0x31c),presentation_lift=control(0x2c8),
        boost_tier_counter=i(base+0x2f4),boost_speed_floor=f(base+0x30c),
        forward=vec(base+0x3a0),lateral=vec(base+0x3b0),surface_velocity=vec(base+0x3d0),physical_forward=vec(base+0x1b0),
        turn=control(0x1f0),brake=control(0x214),crouch=control(0x220),
        balance280=control(0x280),adjustment28c=control(0x28c),adjustment298=control(0x298),
        animation_turn=control(0x1fc),extra_lean=control(0x208),board_alignment=control(0x2bc),presentation_roll=control(0x250),
        depth1=f(owner+4),depth3=f(owner+8),distance=f(base+0x454),time_scale=f(base+0x300),
        boost=f(base+0x2fc),boost_window=f(base+0x2e8),mode_timing=f(base+0x470),heading_offset=f(base+0x4cc),
        flags308=u(base+0x308),rider_type=i(base+0x434),animation_index=i(u(base+0x784)+8),
        animation_class=i(0x446990+i(u(base+0x784)+8)*28),
        force_heading_boost=bool(read('<H',base+0x2d4)[0]&0x10),state320_equals324=i(base+0x320)==i(base+0x324))
    def read_cache(offset,kinds):
        pointer=u(base+offset)
        kind=u(pointer+12) if pointer else -1
        result=dict(valid=False,resource=0,cell_u=0,cell_v=0,half=0,query_kind=kind)
        if pointer and u(pointer) and kind in kinds:
            result.update(valid=True,resource=u(u(pointer)+0x150),cell_u=read('<H',pointer+4)[0],cell_v=read('<H',pointer+6)[0],half=u(pointer+8))
        return result
    cache=read_cache(0x864,(2,));body_cache=read_cache(0x868,(0,1))
    properties=[i(u(race+0x44)+n*0xb0+0x44) for n in range(19)]
    return dict(profile=profile,state=state,cache=cache,body_cache=body_cache,surface_properties44=properties,surface_catalog=extract_surface_catalog(memory),provenance=dict(source='Original PS2 USA live data and verified stat getters',
        surface_address=hex(surface),character_id=character,progress_bank=bank,stat_level_override=override,
        scalar_division_rounding='nearest; verified PCSX2 v2.8.2 FPUDivFPCR policy and current reference configuration',
        speed_stat_getter='0x1493D8',edge_stat_getters=['0x148D80','0x148E68']))
