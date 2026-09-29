#!/usr/bin/env python3
"""Read original PS2 input/provider state from a private PCSX2 snapshot.

The snapshot's consumed input sample is not assumed to equal movie frame N.
"""
import argparse,hashlib,json,math,struct,zipfile
from collections import deque
from pathlib import Path

ACTION_NAMES=('CruiseTurn','CruiseCrouch','CruiseBrake','BoardPress','BoardPivot',
 'AttackLeft','AttackRight','PrewindTurn','PrewindSpin','PrewindFlip','AirAdjRotFB',
 'AirAdjRotLR','LateSpin','RailSpin','Spin','Flip','WipeoutRecover','JumpPressed',
 'JumpHeld','BoostPressed','BoostHeld','Tweak','OllieHeld','RailBalance',
 'HandplantBalance','Handplant','GateAnticipate')
GRAB_MASKS=(1,2,4,8,3,5,9,6,10,12,7,11,13,14,15)
BUTTON_NAMES=('Select','Start','L3','R3','DPadRight','DPadLeft','DPadUp','DPadDown',
 'Triangle','Circle','Cross','Square','L1','R1','L2','R2','RightStickLeft',
 'RightStickRight','RightStickUp','RightStickDown','LeftStickLeft','LeftStickRight',
 'LeftStickUp','LeftStickDown')

def float32_zero(value):
    bits=struct.unpack('<I',struct.pack('<f',value))[0]
    result=struct.unpack('<f',struct.pack('<I',bits))[0]
    if (value>=0 and result>value) or (value<0 and result<value):bits-=1
    return struct.unpack('<f',struct.pack('<I',bits))[0]

def extract_input(memory,rider=0x14701a0):
    def read(at,fmt):
        if not 0<=at<=len(memory)-struct.calcsize(fmt):raise ValueError('Input pointer outside EE memory')
        return struct.unpack_from(fmt,memory,at)
    def u(at):return read(at,'<I')[0]
    def f(at):return read(at,'<f')[0]
    owner=u(rider+0x77c);interface=u(owner+0xde8)
    provider=u(interface+12);adjust=read(interface+8,'<h')[0]
    if provider!=0x127998 or adjust:raise ValueError('Unsupported original input provider')
    context=u(owner+0xdf0);pad=u(context);mapping=u(context+4);count=u(pad)
    if not 0<count<=160:raise ValueError('Invalid original button count')
    buttons=[]
    for i in range(count):
        values=read(pad+4+28*i,'<f6I')
        if not math.isfinite(values[0]):raise ValueError('Nonfinite button value')
        buttons.append(dict(index=i,name=BUTTON_NAMES[i] if i<len(BUTTON_NAMES) else str(i),
            **dict(zip(('value','pressed','released','held','repeat','repeat_timer','edge_age'),values))))
    entrypoints=u(mapping+4);program=u(mapping+20);word_count=u(mapping+12)
    def evaluate(action):
        pc=u(entrypoints+action*4);registers=[0.]*64
        def operand(index):
            if index<64:return registers[index]
            if index<96:raise ValueError('Unimplemented VM constant operand')
            index-=96;button,field=divmod(index,6)
            if button>=len(buttons):return 0.
            name=('value','held','pressed','released','repeat','value')[field]
            return float(buttons[button][name])
        for _ in range(256):
            if pc>=word_count:raise ValueError('Input program extent')
            instruction=u(program+4*pc);pc+=1
            opcode=instruction&63;dest=(instruction>>6)&63
            left=(instruction>>12)&1023;right=(instruction>>22)&1023
            if opcode==32:return registers[0]
            if opcode==0:
                if pc>=word_count:raise ValueError('Input literal extent')
                registers[dest]=f(program+4*pc);pc+=1
            elif opcode==1:registers[dest]=operand(left)
            elif opcode==13:registers[dest]=float32_zero(operand(left)-operand(right))
            elif opcode==16:registers[dest]=max(operand(left),operand(right))
            else:raise ValueError(f'Unsupported main-action opcode {opcode} at {pc-1}')
        raise ValueError('Input program did not terminate')
    actions={name:evaluate(i) for i,name in enumerate(ACTION_NAMES)}
    ring=u(0x4a30f0-0x850);read_cursor=u(ring+0x2ee0);write_cursor=u(ring+0x2ee4)
    if not read_cursor<30 or not write_cursor<30:raise ValueError('Invalid input ring cursors')
    driver=u(ring+0x2eec);driver_interface=u(driver)
    recorder=u(owner+0xdf8);records=[]
    if recorder and u(recorder+12):
        record_count=u(recorder);record_buffer=u(recorder+12)
        if record_count>65536:raise ValueError('Input RLE count exceeds audit bound')
        frame=0
        for i in range(record_count):
            word0,word1=read(record_buffer+i*8,'<2I');duration=word0&0xfff
            if not duration:raise ValueError('Zero-duration accepted input record')
            records.append(dict(start=frame,end=frame+duration,duration=duration,
                word0=f'0x{word0&0xfffff000:08x}',word1=f'0x{word1:08x}'))
            frame+=duration
        accepted=dict(address=f'0x{recorder:08x}',buffer=f'0x{record_buffer:08x}',
            record_count=record_count,frames=frame,records=records,
            note='Original0x26D2B0 records provider commands with run lengths in low12bits. Records reflect actual dispatched inputs; bits depend on current rider control state.')
    else:accepted=None
    return dict(rider=f'0x{rider:08x}',motion_owner=f'0x{owner:08x}',
        control_state=u(owner+0xde4),provider=f'0x{provider:08x}',
        context=f'0x{context:08x}',pad=f'0x{pad:08x}',mapping=f'0x{mapping:08x}',
        buttons=buttons,actions=actions,accepted_commands=accepted,
        input_ring=dict(address=f'0x{ring:08x}',read_cursor=read_cursor,write_cursor=write_cursor,
          driver=f'0x{driver:08x}',reader=f'0x{u(driver_interface+20):08x}',
          consumed_raw_values=read(ring+read_cursor*0x190+4,'<24f')),
        note='Values are frozen input-state observations. Movie endpoint and EE logic phase can differ; do not infer a fixed extra game-input delay from snapshot filenames.')

def raw_accepted_delta(baseline,outcome):
    """Verify the original RLE prefix without inferring control transitions.

    These words are interpreted by the native controller at execution time.
    Decoded controls are not independently verified original input semantics.
    """
    before=baseline['accepted_commands'];after=outcome['accepted_commands']
    if not before or not after:raise ValueError('Original command recording missing')
    offset=before['frames'];frames=after['frames']-offset
    if frames<0:raise ValueError('Outcome input history is shorter than baseline')
    if not 1<=frames<=36000:raise ValueError('Accepted delta must contain1..36000 commands')
    def words(record):return struct.pack('<II',int(record['word0'],16),int(record['word1'],16))
    def prefix(records,count):
        result=bytearray();cursor=0
        for record in records:
            if record['start']!=cursor or record['end']!=cursor+record['duration'] or record['duration']<=0:
                raise ValueError('Invalid original RLE coverage')
            if int(record['word0'],16)&0xfff:raise ValueError('Original command contains duration bits')
            length=min(record['duration'],max(0,count-cursor));result.extend(words(record)*length);cursor=record['end']
        if cursor<count:raise ValueError('Truncated original RLE coverage')
        return bytes(result)
    before_bytes=prefix(before['records'],offset)
    after_prefix=prefix(after['records'],offset)
    if before_bytes!=after_prefix:raise ValueError('Accepted input history diverges before baseline')
    # Validate all records, not only the prefix, before emitting any commands.
    expanded=prefix(after['records'],after['frames'])[offset*8:]
    segments=[]
    for record in after['records']:
        if record['end']<=offset:continue
        start=max(offset,record['start'])-offset;end=record['end']-offset
        segments.append(dict(start=start,end=end,duration=end-start,word0=record['word0'],word1=record['word1']))
    return dict(version=1,decoding='runtime_control_state',frames=frames,segments=segments,
        initial_control_state=baseline['control_state'],final_control_state=outcome['control_state'],
        previous_jump_held=bool(baseline['actions']['JumpHeld']),
        input_stream_sha256=hashlib.sha256(expanded).hexdigest(),
        baseline_prefix=dict(verified=True,frames=offset,sha256=hashlib.sha256(before_bytes).hexdigest()),
        source='Original SSX3 accepted recorder words; decoded using native runtime state',
        hash_encoding='SHA256 of little-endian uint32 word0 then word1 repeated per logic input, duration bits removed',
        decoding_scope='Raw words verified independently; native state transitions select their meaning. Endpoint state and physics must be compared separately.')


def accepted_delta(baseline,outcome):
    before=baseline['accepted_commands'];after=outcome['accepted_commands']
    if not before or not after:raise ValueError('Original command recording missing')
    if before['frames']>after['frames']:raise ValueError('Outcome input history is shorter than baseline')
    def key(record):return (int(record['word0'],16),int(record['word1'],16))
    def stream_hash(records):
        digest=hashlib.sha256()
        for r in records:digest.update(struct.pack('<2I',*key(r))*r['duration'])
        return digest.hexdigest()
    i=j=0;a_left=b_left=0
    while i<len(before['records']):
        if j>=len(after['records']):raise ValueError('Truncated accepted input prefix')
        a,b=before['records'][i],after['records'][j]
        if key(a)!=key(b):raise ValueError(f'Accepted input history diverges before baseline at record {i}')
        if not a_left:a_left=a['duration']
        if not b_left:b_left=b['duration']
        amount=min(a_left,b_left);a_left-=amount;b_left-=amount
        if not a_left:i+=1
        if not b_left:j+=1
    offset=before['frames'];segments=[];control=baseline['control_state']
    if control not in (0,2,4,5):raise ValueError(f'Unsupported baseline control state {control}')
    def axis(word,shift):
        value=(word>>shift)&63
        if value>=32:value-=64
        return float32_zero(value*struct.unpack('<f',struct.pack('<I',0x3d042108))[0])
    pending=deque(after['records'])
    while pending:
        raw=pending.popleft()
        if raw['end']<=offset:continue
        duration=raw['end']-max(raw['start'],offset)
        word0,word1=key(raw)
        start=max(raw['start'],offset)-offset;end=start+duration
        if word0&0x1000:raise ValueError('Unsupported reset/recovery command bit12')
        controls={}
        if control==0:
            if word0&0xc2000 or word1&~0xfff:raise ValueError('Unsupported cruise attack/handplant command')
            controls=dict(turn=axis(word0,20),crouch=axis(word0,26),brake=axis(word1,0),
                boardPress=axis(word1,6),jumpPressed=bool(word0&0x4000),jumpHeld=bool(word0&0x8000),
                boostPressed=bool(word0&0x10000),boostHeld=bool(word0&0x20000))
            next_control=2 if controls['jumpPressed'] or controls['jumpHeld'] else control
        elif control==2:
            if word0&0xf8000000 or word1&~0xfff:raise ValueError('Unsupported crouch command bits')
            if word1&0xfc0:raise ValueError('Crouch secondary axis semantics require recovery')
            prewind=axis(word1,0)
            controls=dict(jumpHeld=bool(word0&0x2000),jumpPressed=False,
                boostHeld=bool(word0&0x4000),turn=max(-.5,min(.5,prewind)),prewindTurn=prewind,
                spin=axis(word0,15),flip=axis(word0,21),crouch=1. if word0&0x2000 else 0.,brake=0.)
            next_control=control if controls['jumpHeld'] else 5
        elif control==5:
            if word0&0xc000a000 or word1&~0xfffff:
                raise ValueError('Unsupported airborne handplant/late-spin/command bits')
            grab=(word0>>16)&255
            if grab!=255 and grab>=len(GRAB_MASKS):raise ValueError('Invalid original airborne grab index')
            board_press=(word1>>18)&3
            if board_press==2:raise ValueError('Invalid original airborne board-press field')
            if board_press==3:board_press=-1
            controls=dict(jumpHeld=False,jumpPressed=False,boostHeld=bool(word0&0x4000),
                spin=axis(word0,24),flip=axis(word1,0),airAdjustFB=axis(word1,6),
                airAdjustLR=axis(word1,12),boardPress=float(board_press),
                grabMask=0 if grab==255 else GRAB_MASKS[grab])
            next_control=control
        else:
            # State4 has a different axis layout; keep its neutral case bounded.
            if word0!=0x00ff0000 or word1:raise ValueError(f'Nonneutral airborne control-state {control} command not decoded')
            controls=dict(jumpHeld=False,jumpPressed=False)
            next_control=control
        # A transition command is interpreted by the old controller for one tick;
        # a repeated identical bit pattern after it would need decoding anew.
        if next_control!=control and duration>1:
            pending.appendleft(dict(raw,start=offset+start+1))
            duration=1;end=start+1
        segments.append(dict(start=start,end=end,duration=duration,word0=f'0x{word0:08x}',
            word1=f'0x{word1:08x}',control_state=control,controls=controls))
        control=next_control
    frames=after['frames']-offset
    if not 1<=frames<=36000:raise ValueError('Accepted delta must contain1..36000 commands')
    # Nonneutral commands cannot be safely decoded across unobserved physical
    # state changes. Endpoint consistency is necessary, not proof of all states.
    terminal=outcome['control_state']
    if control!=terminal and any(int(r['word0'],16) or int(r['word1'],16) for r in segments):
        raise ValueError(f'Unsupported control transition: inferred {control}, observed {terminal}')
    return dict(version=1,frames=frames,segments=segments,
        initial_control_state=baseline['control_state'],final_control_state=terminal,
        previous_jump_held=bool(baseline['actions']['JumpHeld']),
        input_stream_sha256=stream_hash(segments),
        baseline_prefix=dict(verified=True,frames=offset,sha256=stream_hash(before['records'])),
        source='Original SSX3 decoded-command RLE at motion-owner+0xDF8; low12 duration bits removed before hashing',
        hash_encoding='SHA256 of little-endian uint32 word0 then word1 repeated per accepted logic input; word0 low12 bits zero',
        decoding_scope='Verified cruise/crouch/basic jump transition plus state5 spin/flip/air-adjust/board-press/tweak/grab layout; state4 neutral only. Handplant/late-spin/unknown transitions fail closed.')

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('state',type=Path);p.add_argument('--output',type=Path);p.add_argument('--baseline',type=Path);a=p.parse_args()
    with zipfile.ZipFile(a.state) as z:memory=z.read('eeMemory.bin')
    result=dict(state=str(a.state.resolve()),ee_sha256=hashlib.sha256(memory).hexdigest(),input=extract_input(memory))
    if a.baseline:
        with zipfile.ZipFile(a.baseline) as z:base_memory=z.read('eeMemory.bin')
        accepted=accepted_delta(extract_input(base_memory),result['input'])
        accepted['baseline_state']=str(a.baseline.resolve());accepted['baseline_ee_sha256']=hashlib.sha256(base_memory).hexdigest()
        accepted['outcome_state']=str(a.state.resolve());accepted['outcome_ee_sha256']=hashlib.sha256(memory).hexdigest()
        result['accepted_input']=accepted
    text=json.dumps(result,indent=2)+'\n'
    if a.output:a.output.write_text(text)
    print(text)
if __name__=='__main__':main()
