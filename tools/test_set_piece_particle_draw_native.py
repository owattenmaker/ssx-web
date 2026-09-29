#!/usr/bin/env python3
"""Original draw oracle for the set-piece particle systems (builtin16 / builtin26).

Runs the original particle VU1 microprogram (program 4 of the .vutext MPG at
0x439A40) on the live emitters of PS2 savestates, exactly as the renderer uploads
them, and records the GIF sprite packets for web/test-set-piece-particle-sprites.mjs:

- Particle modifier 0x4912B0 (type-13 emitter, builtin16): system at modifier+0x50
  (class 3705E0), kernel = system+0x10 (0x150 bytes). Draw 345BC8 -> 3708C0 ->
  renderer slot 0x290 = 380518: UNPACK V4-32 21 qw at TOPS, MSCAL 0x000.
- DynamicParticle modifier 0x491268 (builtin26): emitter at modifier+0x60 (the
  0x210-byte snow class), draw 346070 -> 371380(e, 7) -> renderer slot 0x298 =
  3807A0: per 64-group batch UNPACK 152 qw at TOPS (kernel e+0x20 21 qw, header
  {rows, perBirth, perBirth float, startAge}, 65 position + 65 velocity rows of
  the birth ring from cursor+1), MSCAL 0x510.
- Constant rows 0..6 = 364CD0 program-4 case (context 0 set): rows 0..3 =
  list+0x69CD0+view*0x180 +0x80..+0xB0 (world -> guard-band clip), row 4/5 =
  +0xC0/+0xD0 with z*0.0625 (viewport scale/offset), row 6 = (+0x40, +0x54, 0, 0).

Padding/beyond-upload VU rows are zero here (in the game they hold stale data);
the evaluator marks particles depending on them as approximate.
Output: local/browser-validation/set-piece-particle-draw-oracle.json
"""
import argparse,json,struct,subprocess,zipfile,hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
DEFAULT_STATES=[
 'local/ps2-capture/runs/setpieces-fx/vis.tick449.p2s',    # startfire pops (tex 28, blend 0)
 'local/ps2-capture/runs/setpieces-fx/vis.tick924.p2s',    # rocket smoke trails (DynamicParticle tex 25)
 'local/ps2-capture/runs/setpieces-fx/vis.tick969.p2s',    # rocket bursts (tex 13, NumBlur 8)
 'local/ps2-capture/runs/setpieces-fx/vis.tick1398.p2s',   # snow crumbs (DynamicParticle cap 120 -> 2 batches)
 'local/ps2-capture/runs/setpieces-fx/vis.tick2003.p2s',   # fire gushes (tex 22/24)
 'local/ps2-capture/runs/setpieces/fulldense.tick3739.p2s',# spintwin sparks (DynamicParticle tex 24)
 'local/ps2-capture/runs/setpieces-fx/vis.tick4119.p2s',   # midfire pop (tex 22, NumBlur 2)
 'local/ps2-capture/runs/setpieces/fulldense.tick11199.p2s',# dragon fire + ambient emitters (tex 4/11/25)
 'local/ps2-capture/runs/setpieces-fx/vis.tick11739.p2s',  # EZ rocket core (DynamicParticle cap 72) + EZ pops
 'local/ps2-capture/runs/setpieces-fx/vis.tick498.p2s',    # startfire pops, late
 'local/ps2-capture/runs/setpieces/fulldense.tick894.p2s', # rocket smoke, early
 'local/ps2-capture/runs/setpieces/fulldense.tick938.p2s', # rocket smoke, full ring
 'local/ps2-capture/runs/setpieces-fx/vis.tick1988.p2s',   # fire gush start
 'local/ps2-capture/runs/setpieces-fx/vis.tick2624.p2s',   # fire gush 1400
 'local/ps2-capture/runs/setpieces/fulldense.tick3779.p2s',# spintwin sparks, later
 'local/ps2-capture/runs/setpieces/fulldense.tick10889.p2s',# dragon fire
 'local/ps2-capture/runs/setpieces-fx/vis.tick11249.p2s',  # dragon fire
 'local/ps2-capture/runs/setpieces-fx/vis.tick11709.p2s',  # EZ rocket core + EZ emitters
]
class State:
    def __init__(s,path):
        z=zipfile.ZipFile(path);s.m=z.read('eeMemory.bin');s.path=path
    def w(s,a):return struct.unpack_from('<I',s.m,a)[0]
    def f(s,a):return struct.unpack_from('<f',s.m,a)[0]
    def raw(s,a,n):return s.m[a:a+n]
    def find(s,v):
        b=struct.pack('<I',v);i=s.m.find(b)
        while i>=0:
            if i%4==0:yield i
            i=s.m.find(b,i+1)
def chop(x):
    """float32 of a double, rounded toward zero (EE mul.s)."""
    r=struct.unpack('<f',struct.pack('<f',x))[0]
    if r!=0 and abs(r)>abs(x):
        b=struct.unpack('<I',struct.pack('<f',r))[0]-1;r=struct.unpack('<f',struct.pack('<I',b))[0]
    return r
def vu_rows(s,view=0):
    renderer=s.w(0x4a30f0+0x2a90);lst=s.w(renderer+0x18f0);t8=lst+0x69cd0+view*0x180
    rows=[list(struct.unpack_from('<4I',s.m,t8+o)) for o in (0x80,0x90,0xa0,0xb0,0xc0,0xd0)]
    for r in (4,5):rows[r][2]=struct.unpack('<I',struct.pack('<f',chop(struct.unpack('<f',struct.pack('<I',rows[r][2]))[0]*0.0625)))[0]
    rows.append([s.w(t8+0x40),s.w(t8+0x54),0,0])
    return rows
def image(rows,top,uploads):
    data=bytearray(16384)
    for i,r in enumerate(rows):struct.pack_into('<4I',data,i*16,*r)
    for row,qw in uploads:data[(top+row)*16:(top+row)*16+16]=qw
    return bytes(data)
def particle_cases(s):
    for a in s.find(0x4912b0):
        mod=a-8;system=mod+0x50;kernel=system+0x10
        if s.w(mod+0x1e4) or not s.w(system+0xc):continue   # finished (345BC8) / draw disabled (3708C0)
        yield dict(kind='particle',modifier=mod,texture=s.w(system+4),blend=s.w(system+8),flipPhase=s.f(system+0x184),
            flipFrames=s.w(system+0x180),kernel=list(struct.unpack_from('<84I',s.m,kernel)))
def trail_cases(s):
    for a in s.find(0x491268):
        mod=a-8;e=mod+0x60
        if s.w(mod+0x18) or not s.w(e+0x174) or not s.w(e+0x1e0):continue  # finished (346070) / 371380 gates
        cap=s.w(e+0x178);cursor=s.w(e+0x17c);pos=s.w(e+0x1a0);vel=s.w(e+0x1a4)
        if not 0<cap<4096:continue
        yield dict(kind='trail',modifier=mod,texture=s.w(e+4),blend=s.w(e+8),flipPhase=s.f(e+0x10),flipFrames=s.w(e+0xc),
            capacity=cap,cursor=cursor,particleCount=s.w(e+0x20),kernel=list(struct.unpack_from('<84I',s.m,e+0x20)),
            positions=list(struct.unpack_from('<%dI'%(cap*4),s.m,pos)),velocities=list(struct.unpack_from('<%dI'%(cap*4),s.m,vel)),
            material=list(struct.unpack_from('<5I',s.m,e+0x1e4)))
def trail_batches(case):
    """3807A0: one VU image per 64-group batch (header row 21, positions 22.., velocities 87..)."""
    cap=case['capacity'];k=case['kernel'];per=int(case['particleCount']/cap)  # div (signed, trunc)
    perf=float(per);age_step=struct.unpack('<f',struct.pack('<I',k[3]))[0]
    batches=(cap>>6)+1;done=0;out=[]
    q=lambda words,i:struct.pack('<4I',*words[i*4:i*4+4])
    for b in range(batches):
        rows=cap-done
        if rows>=65:rows=64
        start_age=chop(chop(perf*float(b<<6))*age_step)
        up=[(i,struct.pack('<4I',*k[i*4:i*4+4])) for i in range(21)]
        up.append((21,struct.pack('<IIff',rows,per,perf,start_age)))
        first=(done+case['cursor']+1)%cap
        done+=rows
        nxt=(done+case['cursor']+1)%cap if done<cap else (done-1+case['cursor']+1)%cap
        idx=[(first+i)%cap for i in range(rows)]+[nxt]
        for j,slot in enumerate(idx):
            up.append((22+j,q(case['positions'],slot)));up.append((87+j,q(case['velocities'],slot)))
        out.append(dict(rows=rows,startAge=start_age,slots=idx,uploads=up))
    return out
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('states',nargs='*');a=p.parse_args()
    states=a.states or DEFAULT_STATES
    out=root/'build/set-piece-particle-draw';out.mkdir(parents=True,exist_ok=True)
    from extract_fx_microcode import export
    export(out/'vu');program=out/'vu/program4.bin'
    vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
    includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
    command=['xcrun','clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off','-DUSE_SSE2NEON']+['-I'+str(x)for x in includes]
    command+=[str(root/'tests/set_piece_particle_draw_reference.cpp'),str(vendor/'ps2xRuntime/src/lib/vu/ps2_vu1_upper.cpp'),
        str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
    for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
    binary=out/'set_piece_particle_draw';subprocess.run(command+['-o',str(binary)],check=True)
    TOP=8;jobs=[];cases=[]
    for path in states:
        s=State(root/path);rows=vu_rows(s)
        live=list(particle_cases(s))+list(trail_cases(s))
        # Synthetic blur variants of the live emitters (no live trail uses NumBlur, no live
        # burst uses 3): NumBlur 3, alphaStep 1/3, blurStep 0.02 scaled s.
        variants=[]
        for case in live:
            if case['kernel'][1]==0 and len(variants)<12:
                v=json.loads(json.dumps(case));v['kernel'][1]=3
                v['kernel'][12]=struct.unpack('<I',struct.pack('<f',1/3))[0];v['kernel'][13]=struct.unpack('<I',struct.pack('<f',0.02))[0]
                v['synthetic']='NumBlur 3';variants.append(v)
        for case in live+variants:
            case.update(state=path,vuRows=rows,jobs=[])
            if case['kind']=='particle':
                ups=[(i,struct.pack('<4I',*case['kernel'][i*4:i*4+4])) for i in range(21)]
                case['jobs'].append(len(jobs));jobs.append((0x000,TOP,image(rows,TOP,ups)))
            else:
                case['batches']=[]
                for batch in trail_batches(case):
                    case['jobs'].append(len(jobs));jobs.append((0x510,TOP,image(rows,TOP,batch.pop('uploads'))));case['batches'].append(batch)
            cases.append(case)
    blob=b'SPVU'+struct.pack('<I',len(jobs))+b''.join(struct.pack('<II',pc,top)+data for pc,top,data in jobs)
    (out/'jobs.bin').write_bytes(blob)
    result=subprocess.run([str(binary),str(program),str(out/'jobs.bin')],check=True,capture_output=True,text=True,timeout=600).stdout.split('\n')
    packets={};current=None
    for line in result:
        if line.startswith('J '):_,j,kicks,n=line.split();current=packets.setdefault(int(j),dict(kicks=int(kicks),sprites=[]))
        elif line.startswith('S '):current['sprites'].append([int(x,16) for x in line.split()[1:]])
    for case in cases:case['oracle']=[packets[j] for j in case.pop('jobs')]
    report=dict(version=1,program_sha256=hashlib.sha256(program.read_bytes()).hexdigest(),top=TOP,
        note='GIF sprites per job: [ST0 xyzw, RGBAQ ints, XYZ2 v0 (ftoi4), ST1, XYZ2 v1]; vuRows are VU1 rows 0..6 (u32 bits)',cases=cases)
    target=root/'local/browser-validation/set-piece-particle-draw-oracle.json';target.parent.mkdir(parents=True,exist_ok=True)
    target.write_text(json.dumps(report)+'\n')
    summary={}
    for c in cases:
        key=f"{c['kind']} tex{c['texture']} blend{c['blend']}";d=summary.setdefault(key,[0,0]);d[0]+=1;d[1]+=sum(len(o['sprites']) for o in c['oracle'])
    print(json.dumps(dict(output=str(target),jobs=len(jobs),cases=len(cases),sprites_by_kind=summary)))
if __name__=='__main__':main()
