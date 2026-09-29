#!/usr/bin/env python3
"""Extract verified SSX3 race clock, authored runtime course and participant state."""
import argparse,hashlib,json,math,struct,zipfile
from pathlib import Path

def extract_race_event(memory):
    def read(fmt,address):
        size=struct.calcsize('<'+fmt)
        if not 0<=address<=len(memory)-size:raise ValueError('Race pointer outside EE memory')
        return struct.unpack_from('<'+fmt,memory,address)
    def u(a):return read('I',a)[0]
    def i(a):return read('i',a)[0]
    def f(a):
        value=read('f',a)[0]
        if not math.isfinite(value):raise ValueError('Nonfinite original race field')
        return value
    def vec(a,n=3):return [f(a+4*k) for k in range(n)]
    world=u(u(0x4a30f0-0x848)+0x84);game=u(world+12)
    if u(game+0xcc)!=0x458488:raise ValueError('Unexpected original game-info interface')
    offsets=[0,0xac,0xb0,0xb4,0xbc,0xc0,0xc4,0xc8]
    names=['None','GameInit','Freeride','PreRace','Countdown','Race','EndRace','Shutdown']
    phase=i(game);previous=i(game+4)
    if not 0<=phase<8 or not 0<=previous<8:raise ValueError('Invalid race phase')
    old=u(game+0x9c);oldphase=0 if not old else next((n for n,x in enumerate(offsets) if n and old==game+x),None)
    if oldphase is None:raise ValueError('Unknown retained race state handler')
    clock=dict(phase=phase,previous=previous,previous_handler=oldphase,total_ticks=i(game+8),race_ticks=i(game+12),
        countdown_ticks=i(game+0x1c),race_enabled=i(game+0x14),pre_race_local=i(game+0xb8))
    path_count=i(0x4d33a0+0x10);paths_base=u(0x4d33a0+0x14)
    if not 0<path_count<=128:raise ValueError('Invalid authored course path count')
    paths=[]
    for n in range(path_count):
        p=paths_base+n*60;events=i(p);eventdata=u(p+4);segments=i(p+8);segmentdata=u(p+0x18)
        if not 0<=events<=4096 or not 0<segments<=100000:raise ValueError('Invalid course path extents')
        if u(p+0x34)!=0x481448:raise ValueError('Unsupported course path interface')
        paths.append(dict(index=n,origin=vec(p+12),low=vec(p+0x1c),high=vec(p+0x28),remaining_at_origin=f(p+0x38),
            segments=[vec(segmentdata+j*16,4) for j in range(segments)],
            events=[dict(type=u(eventdata+j*16),value=u(eventdata+j*16+4),start=f(eventdata+j*16+8),end=f(eventdata+j*16+12)) for j in range(events)]))
    count=i(game+0x78)
    if not 0<=count<=6:raise ValueError('Invalid rider roster count')
    humans=[];human_count=i(game+0x7c)
    if not 0<=human_count<=4:raise ValueError('Invalid human roster count')
    for n in range(human_count):
        owner=u(game+0x40+n*4);humans.append(u(owner+0x18))
    participants=[]
    for n in range(count):
        rider=u(game+0x28+n*4);path=u(rider+0xab4);cache=u(rider+0xac0)
        path_index=(path-paths_base)//60
        if not 0<=path_index<path_count or paths_base+60*path_index!=path:raise ValueError('Participant path not in course')
        participants.append(dict(index=n,human=rider in humans,position=vec(rider+0x110),quaternion=vec(rider+0x120,4),
            velocity=vec(rider+0x1e0),motion_mode=i(u(rider+0x77c)+0xde0),control_state=i(u(rider+0x77c)+0xde4),
            finish_elapsed=f(rider+0x470),finish_ticks=i(rider+0x478),penalty_ticks=i(rider+0x47c),dnf=i(rider+0x480),
            path_index=path_index,remaining=f(rider+0x4d0),best_remaining=f(rider+0x4d4),
            path_cache=dict(origin=vec(cache),distance=f(cache+0x10),segment=i(cache+0x14))))
    manager=u(world+0x28);checkpoint_count=i(manager+0x10)
    checkpoint=dict(count=checkpoint_count,pending_human_mask=read('B',u(manager+0x3b4)+0x1f)[0],human_masks=[u(manager+0x5fc+j*4) for j in range(len(humans))],
        inhibited_fields={f'{x:x}':u(manager+x) for x in [0,0x60c,0x610,0x61c,0x620]})
    digest=hashlib.sha256(json.dumps(paths,separators=(',',':')).encode()).hexdigest()
    return dict(clock=clock,paths=paths,participants=participants,checkpoints=checkpoint,
        provenance=dict(game_address=hex(game),world_address=hex(world),phase_name=names[phase],path_base=hex(paths_base),
            course_sha256=digest,units='source Z-up centimeters and60Hz integer ticks',configuration_bytes=list(memory[0x535c10:0x535c14]),
            state_dispatch='113B10/113B48/113C20',path_projection='26A638',event_query='26A090/26AA80',
            finish_callback='10E5D8 type1 ->125108',checkpoint_callback='10E5D8 type11 ->270AB0',
            unsupported='UI/loading/start ordering, full path selection, external event side effects and opponents remain separate recovery'))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('state',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    with zipfile.ZipFile(a.state) as z:memory=z.read('eeMemory.bin')
    event=extract_race_event(memory);out=dict(state=str(a.state.resolve()),ee_sha256=hashlib.sha256(memory).hexdigest(),original_race_event=event)
    a.output.write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(dict(clock=event['clock'],paths=len(event['paths']),participants=len(event['participants']),checkpoints=event['checkpoints']),indent=2))
if __name__=='__main__':main()
