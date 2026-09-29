"""Data-only NPC grab selection inputs, from original mappings and AFL markers."""
import struct,math
from reference_input import float32_zero

def extract_npc_grabs(memory,rider):
    def read(fmt,at):
        if not 0<=at<=len(memory)-struct.calcsize('<'+fmt):raise ValueError('NPC grab data outside memory')
        return struct.unpack_from('<'+fmt,memory,at)[0]
    u=lambda at:read('I',at);i=lambda at:read('i',at);h=lambda at:read('H',at);b=lambda at:read('b',at);f=lambda at:read('f',at)
    gp=0x4a30f0;slot=i(rider+0x86c);entry=i(0x5305b0+slot*4);record=0x535b20+entry*28
    char=b(record+17);special=b(record+18);bank=2 if i(record+12)==-1 else u(record+16)&1
    override=i(rider+0xb34)
    if u(0x534fe0+entry*28+16)&4:stat=.5
    else:
        numerator=override if override>0 else int(b(0x535538+bank*70+char*7+2)/5)
        denominator=b(0x5308d8+char*15+10)
        if denominator<=0:raise ValueError('NPC grab stat denominator')
        stat=struct.unpack('<f',struct.pack('<f',numerator/denominator))[0]
    def timing(semantic):
        if semantic==438:return dict(semantic=438,marker1=-1.,marker2=-1.)
        at=u(gp+0xd0c)+semantic*4;variant=read('h',at);count=read('h',at+2)
        if count!=1:raise ValueError(f'NPC grab semantic{semantic} has{count} random variants; requires runtime marker selection')
        basic=i(0x449960+variant*12)
        if basic==519:return dict(semantic=semantic,marker1=0.,marker2=0.)
        animation=u(u(gp+0xd8c)+0x1030+basic*4);animation_bank=u(u(gp+0xd08)+(animation&255)*4)
        descriptor=u(animation_bank+8)+(animation>>8)*20
        first=h(descriptor+16);length=h(descriptor+18)
        def marker(index):return float32_zero(h(u(animation_bank+16)+(first+index)*4)*f(gp-0x322c)) if index<length else -1.
        return dict(semantic=semantic,marker1=marker(1),marker2=marker(2))
    normal=[];tweak=[];uber=[[],[]]
    for index in range(15):
        at=0x45aeb8+index*20;normal.append(timing(h(at)));tweak.append(timing(h(at+6)))
        for tier in range(2):
            fixed={20:(4,160),24:(4,159),25:(9,162),28:(4,161)}.get(special)
            if fixed and fixed[0]==index:semantic=fixed[1]
            elif h(at+12)==0:semantic=438
            else:
                variant=read('B',0x530ec0+bank*0x13ec+char*0x1fe+index*6+tier)
                if variant>=h(at+12):raise ValueError('NPC uber mapping outside authored table')
                semantic=h(u(at+16)+variant*8)
            uber[tier].append(timing(semantic))
    return dict(normal=normal,tweak=tweak,uber=uber,grab_stat=stat,event_id=i(0x535c08),
        provenance=dict(mapping='1500D8/150138/150198/14FEA8',timing='104CF8/311710(count1)/312820',stat='149690 progression2/maximum10',all_timing_variants_single=True))
