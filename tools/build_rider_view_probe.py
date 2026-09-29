#!/usr/bin/env python3
"""Build a bounded, reversible rider-lighting view probe in a derived state."""
import hashlib,json,struct,zipfile
from pathlib import Path
from reference_replay import patch_state
ROOT=Path(__file__).resolve().parents[1]
CODE=0xc0000
DATA=0xc1000
HOOK=0x1220d8
CAPACITY=64

def build():
    source=ROOT/'local/reference/pcsx2/snow-jam-jump-31.p2s'
    for name in ['glide','jump-31','jump-90','charge-long-240']:
        with zipfile.ZipFile(ROOT/f'local/reference/pcsx2/snow-jam-{name}.p2s') as archive:memory=archive.read('eeMemory.bin')
        if any(memory[CODE:CODE+0x4000]):raise ValueError('Probe arena is occupied in reference checkpoints')
    with zipfile.ZipFile(source) as archive:memory=archive.read('eeMemory.bin')
    expected=struct.pack('<2I',0x27bdff70,0x7fb20060)
    if memory[HOOK:HOOK+8]!=expected:raise ValueError('Original rider lighting prologue differs')
    words=[];labels={};branches=[]
    def emit(word):words.append(word)
    def imm(op,rt,rs,value):emit((op<<26)|(rs<<21)|(rt<<16)|(value&65535))
    def constant(reg,value):imm(15,reg,0,value>>16);imm(13,reg,reg,value&65535)
    def addu(dst,a,b):emit((a<<21)|(b<<16)|(dst<<11)|0x21)
    def shift(dst,src,count):emit((src<<16)|(dst<<11)|(count<<6))
    def branch(op,a,b,label):branches.append((len(words),op,a,b,label));emit(0);emit(0)
    # Preserve all touched128-bit GPRs, SP, HI/LO and floating-point state.
    imm(9,29,29,-64)
    for i,reg in enumerate([8,9,10,11]):imm(31,reg,29,i*16)
    constant(8,0x14701a0);branch(5,4,8,'done')
    constant(8,DATA);imm(35,9,8,0);imm(11,10,9,CAPACITY);branch(4,10,0,'done')
    imm(9,10,9,1);imm(43,10,8,0);shift(11,9,7);addu(11,11,8);imm(9,11,11,128)
    imm(35,10,28,-0x848);imm(35,10,10,0x84);imm(35,10,10,0xc);imm(35,10,10,8);imm(43,10,11,0)
    imm(43,4,11,4)
    imm(35,8,28,0x2a90);imm(35,9,8,0x13e4);imm(43,9,11,8);imm(35,10,8,0x13e0);imm(43,10,11,12)
    for i in range(4):imm(30,10,9,i*16);imm(31,10,11,16+i*16)
    imm(30,10,4,0x110);imm(31,10,11,80)
    for i in range(8):imm(35,10,4,0x794+i*4);imm(43,10,11,96+i*4)
    labels['done']=len(words)
    for i,reg in enumerate([8,9,10,11]):imm(30,reg,29,i*16)
    imm(9,29,29,64)
    # Replay the original stack allocation/save, then resume the prologue.
    words.extend([0x27bdff70,0x7fb20060,(2<<26)|((HOOK+8)>>2),0])
    for at,op,a,b,label in branches:words[at]=(op<<26)|(a<<21)|(b<<16)|((labels[label]-at-1)&65535)
    code=struct.pack('<'+'I'*len(words),*words)
    if len(code)>0x1000:raise ValueError('Probe code exceeds its arena')
    patches=[dict(address=hex(HOOK),expected=expected.hex(),replacement=struct.pack('<2I',(2<<26)|(CODE>>2),0).hex()),
             dict(address=hex(CODE),expected=bytes(len(code)).hex(),replacement=code.hex())]
    output=ROOT/'local/rider-lighting/rider-view-probe.p2s';patch_state(source,output,patches)
    manifest=dict(source=str(source),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),output=str(output),
                  hook=hex(HOOK),code=hex(CODE),data=hex(DATA),capacity=CAPACITY,record_stride=128,record_offset=128,
                  fields={'tick':0,'actor':4,'view_pointer':8,'stack_depth':12,'view_matrix':16,'position':80,'selected':96},
                  note='Short disposable reference run only. Code/data arena is zero in four supplied checkpoints; do not use this state for ordinary gameplay.')
    (output.parent/'rider-view-probe.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps(manifest,indent=2))
if __name__=='__main__':build()
