"""Inputs for native1352A8 ordinary/tweak/Uber grab lifecycle."""
import struct,math
from reference_npc_air import extract_npc_grabs


def extract_grab_lifecycle(memory, rider):
    def read(fmt,at):
        if not 0<=at<=len(memory)-struct.calcsize('<'+fmt):raise ValueError('Grab lifecycle pointer outside memory')
        return struct.unpack_from('<'+fmt,memory,at)[0]
    u=lambda at:read('I',at);h=lambda at:read('H',at);i=lambda at:read('i',at);b=lambda at:read('b',at)
    owner=u(rider+0x77c);state,index=struct.unpack_from('<2i',memory,owner+0x234)
    catalog=extract_npc_grabs(memory,rider);slot=i(rider+0x86c);entry=i(0x5305b0+slot*4);record=0x535b20+entry*28
    character=b(record+17);special=b(record+18);bank=2 if i(record+12)==-1 else u(record+16)&1
    def definition(semantic,upper,score):
        if not 0<=score<=84:raise ValueError('Grab score ID outside authored range')
        return dict(semantic=semantic,upper_semantic=upper,score_id=score,
            begin_points=i(0x530600+score*8),hold_points=i(0x530604+score*8))
    normal=[];tweak=[];uber=[[],[]]
    for n in range(15):
        at=0x45aeb8+n*20;normal.append(definition(h(at),h(at+2),h(at+4)));tweak.append(definition(h(at+6),h(at+8),h(at+10)))
        for tier in range(2):
            override={20:(4,160,210,82),24:(4,159,209,81),25:(9,162,212,84),28:(4,161,211,83)}.get(special)
            if override and override[0]==n:item=definition(*override[1:])
            elif h(at+12)==0:item=definition(438,438,0)
            else:
                variant=read('B',0x530ec0+bank*0x13ec+character*0x1fe+n*6+tier)
                if variant>=h(at+12):raise ValueError('Grab Uber variant outside authored table')
                row=u(at+16)+variant*8;item=definition(h(row),h(row+2),h(row+4))
            if item['semantic']!=catalog['uber'][tier][n]['semantic']:raise ValueError('Independent Uber mapping differs')
            uber[tier].append(item)
    super_time=read('f',rider+0x2f0)
    if not math.isfinite(super_time):raise ValueError('Nonfinite original super time')
    return dict(state=dict(state=state,index=index),
        profile=dict(grab_stat=catalog['grab_stat'],grabs=normal,tweak=tweak,uber=uber,extended_definitions=True),
        context=dict(leg_weight=read('f',rider+0x318),super_time=super_time,boost_tier=i(rider+0x2f4),uber_enabled=bool(u(rider+0xb2c)&1)),
        provenance=dict(mapping='1500D8/1500F8/150118;150138/150158/150178;150198/1502C8/1503F8;14FEA8',
            rate='120038: 1 +149690 stat *0.29988324642181396, EE scalar arithmetic',
            supported='Complete states0..5; score IDs and29A530 notification are typed subsystem requests'))
