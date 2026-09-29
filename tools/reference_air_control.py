#!/usr/bin/env python3
"""Read original state5 angular and presentation inputs from a PCSX2 snapshot."""
import argparse,hashlib,json,math,struct,zipfile
from pathlib import Path
from reference_input import float32_zero

def extract_air_control(memory,base=0x14701a0):
    def read(fmt,at):
        if not 0<=at<=len(memory)-struct.calcsize(fmt):raise ValueError('Air-control pointer outside EE memory')
        return struct.unpack_from(fmt,memory,at)
    def u(at):return read('<I',at)[0]
    def i(at):return read('<i',at)[0]
    def f(at):
        v=read('<f',at)[0]
        if not math.isfinite(v):raise ValueError('Nonfinite air-control field')
        return v
    def vec(at,n=3):return [f(at+4*k) for k in range(n)]
    owner=u(base+0x77c);control=owner+0x230
    if i(owner+0xde4)!=5:raise ValueError('Reference is not in air control state5')
    if u(control+0x58)!=base:raise ValueError('Air-control actor link does not match')
    float_fields=((0x10,'target_flip'),(0x14,'target_spin'),(0x18,'progress_flip'),(0x1c,'progress_spin'),
      (0x20,'total_spin'),(0x24,'total_flip'),(0x28,'adjust_spin'),(0x2c,'adjust_flip'),
      (0x30,'scored_spin'),(0x34,'scored_flip'),(0x38,'max_spin'),(0x3c,'max_flip'),
      (0x40,'axis_blend'),(0x48,'hold_spin'),(0x4c,'hold_flip'),(0x50,'input_angle'),(0x54,'idle_time'))
    state={name:f(control+offset) for offset,name in float_fields}
    state.update(mode=i(control),phase=i(control+0xc),extended=i(control+0x44),spin_rate=f(base+0x2dc),flip_rate=f(base+0x2e0))
    slot=i(base+0x86c)
    if not 0<=slot<16:raise ValueError('Invalid original rider slot')
    entry=i(0x5305b0+slot*4);record=0x535b20+entry*28
    char=read('<b',record+17)[0];bank=2 if i(record+12)==-1 else u(record+16)&1
    override=i(base+0xb34)
    if u(0x534fe0+entry*28+16)&4:stat=.5
    else:
        numerator=override if override>0 else int(read('<b',0x535538+bank*70+char*7+4)[0]/5)
        denominator=read('<b',0x5308d8+char*15+12)[0]
        if denominator<=0:raise ValueError('Invalid original trick-stat denominator')
        stat=struct.unpack('<f',struct.pack('<f',numerator/denominator))[0]
    skeleton=u(base+0x780);pivot_index=i(base+0x89c)
    if not 0<=pivot_index<256:raise ValueError('Invalid original pivot index')
    bone=u(skeleton+0x24)+pivot_index*16
    pivot=[float32_zero(f(bone+4*k)*f(skeleton+0x140+4*k)) for k in range(3)]
    animation_id=i(u(base+0x784)+8)
    trajectory=u(base+0x788);status=i(trajectory+0xac);surface=i(trajectory+0x90)
    surfaces=u(u(u(0x4a30f0-0x848)+0x84)+0x44)
    # 0x22F868 allocates 0xD10 bytes as SurfPropCache; stride0xB0 gives19.
    surface_properties44=[i(surfaces+n*0xb0+0x44) for n in range(19)]
    surface_property=0
    if status in (1,3):
        if not 0<=surface<256:raise ValueError('Invalid predicted air surface')
        surface_property=i(surfaces+surface*0xb0+0x44)
    if u(owner+0x24)!=base:raise ValueError('Air physics actor link does not match')
    alignment=dict(normal=vec(trajectory+0x20),heading=vec(trajectory+0x10),physical_forward=vec(base+0x1b0),
        predicted_time=f(trajectory+0x98),elapsed_time=f(trajectory+0xa0),time_scale=f(base+0x300),
        adjust_spin=f(owner+0x258),trajectory_status=status,surface_index=surface,
        surface_flags=read('<h',trajectory+0x94)[0],surface_property44=surface_property,
        control_state=i(owner+0xde4),air_mode_flag=i(owner+0x20))

    trajectory_state=dict(hit_position=vec(trajectory),heading=vec(trajectory+0x10),normal=vec(trajectory+0x20),
        patch_id=i(trajectory+0x30),patch_u=f(trajectory+0x34),patch_v=f(trajectory+0x38),apex_position=vec(trajectory+0x40),
        prediction=dict(position=vec(trajectory+0x50),velocity=vec(trajectory+0x60)),
        integrated=dict(position=vec(trajectory+0x70),velocity=vec(trajectory+0x80)),
        surface=i(trajectory+0x90),patch_flags=read('<h',trajectory+0x94)[0],predicted_time=f(trajectory+0x98),
        apex_time=f(trajectory+0x9c),elapsed=f(trajectory+0xa0),integrated_time=f(trajectory+0xa4),
        speed_limit=f(trajectory+0xa8),status=i(trajectory+0xac))
    return dict(profile=dict(trick_stat=stat,boost_modifier=f(base+0x2ec)>0,landing_animation=animation_id==0x120),
        state=state,alignment_context=alignment,trajectory=trajectory_state,surface_properties44=surface_properties44,physical=dict(position=vec(base+0x110),quaternion=vec(base+0x120,4)),pivot=pivot,
        context=dict(control_mode=i(control),grab_phase=i(control+4),grab_index=i(control+8),animation_id=animation_id,
            no_grab_context=i(control+4)==0 and i(control+8)==-1),
        provenance=dict(control_address=f'0x{control:08x}',source_axis='Z-up',units='centimeters/radians',
            stat_getter='0x1495A8; progression4/maximum12',character=char,progress_bank=bank,stat_override=override,
            pivot_index=pivot_index,pivot_address=f'0x{bone:08x}',pose_function='0x134DD0'))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('state',type=Path);p.add_argument('--output',type=Path);a=p.parse_args()
    with zipfile.ZipFile(a.state) as z:memory=z.read('eeMemory.bin')
    result=dict(state=str(a.state.resolve()),ee_sha256=hashlib.sha256(memory).hexdigest(),original_air_control=extract_air_control(memory))
    text=json.dumps(result,indent=2)+'\n'
    if a.output:a.output.write_text(text)
    print(text)
if __name__=='__main__':main()
