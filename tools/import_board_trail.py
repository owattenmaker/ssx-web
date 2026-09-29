#!/usr/bin/env python3
"""Export original PS2 snow-track texture and authored board-trail parameters."""
import argparse
import hashlib
import json
import struct
import zlib
from pathlib import Path
from inspect_disc import Disc, inspect_elf, EXPECTED_SHA1

GP=0x4a30f0
PARAMETERS=[
 ('enabled',0x148c,'I'),('inner_width_scale',0x1490,'f'),('outer_width_scale',0x1494,'f'),
 ('height_scale',0x1498,'f'),('depth_to_height_scale',0x149c,'f'),('packed_depth_scale',0x14a0,'f'),
 ('loose_depth_scale',0x14a4,'f'),('powder_depth_scale',0x14a8,'f'),('deep_powder_depth_scale',0x14ac,'f'),
 ('top_bias_front',0x14b0,'f'),('top_bias_behind',0x14b4,'f'),('top_bias_clear',0x14b8,'f'),
 ('inner_position_jitter',0x14bc,'f'),('middle_texture_u',0x14c0,'f'),('inner_texture_u',0x14c4,'f'),('outer_texture_u',0x14c8,'f'),
 ('outer_normal_offset',0x14cc,'f'),('inner_normal_offset',0x14d0,'f'),('base_normal_offset',0x14d4,'f'),
 ('fade_segments',0x14d8,'I'),('draw_fixup_layer',0x14dc,'I'),('use_slice_caps',0x14e0,'I'),('plane_offset',0xa38,'f'),('backwards_offset',0xa3c,'f'),('turn_cosine',-0x39d0,'f'),('minimum_speed',-0x39cc,'f'),('angle_peak',-0x39b8,'f'),('angle_scale',-0x39b4,'f'),('speed_scale',-0x39ac,'f')]

def elf_memory(data):
    info=inspect_elf(data);memory=bytearray(32*1024*1024)
    for s in info['load_segments']:
        memory[s['address']:s['address']+s['file_size']]=data[s['offset']:s['offset']+s['file_size']]
    return memory

def png(width,height,rgba):
    def chunk(kind,payload):return struct.pack('>I',len(payload))+kind+payload+struct.pack('>I',zlib.crc32(kind+payload))
    rows=b''.join(b'\0'+rgba[y*width*4:(y+1)*width*4] for y in range(height))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',width,height,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b'')

def export(iso,output):
    disc=Disc(iso)
    try:
        elf=disc.file('SLUS_207.72')
        if hashlib.sha1(elf).hexdigest()!=EXPECTED_SHA1:raise ValueError('Expected verified USA PS2 SSX3 executable')
        source=disc.file('DATA/TEXTURES/EFFECTS.SSH')
    finally:disc.close()
    if source[:4]!=b'SHPS':raise ValueError('Expected original PS2 SHPS texture container')
    records=[struct.unpack_from('<4sI',source,16+i*8) for i in range(struct.unpack_from('<I',source,8)[0])]
    matches=[offset for name,offset in records if name==b'btrl']
    if len(matches)!=1:raise ValueError('Original board-trail texture is not unique')
    offset=matches[0];width,height=struct.unpack_from('<HH',source,offset+4);extent=int.from_bytes(source[offset+1:offset+4],'little')
    if source[offset]!=5 or extent!=16+width*height*4:raise ValueError('Unsupported original board-trail texture encoding')
    raw=source[offset+16:offset+extent]
    if len(raw)!=width*height*4 or max(raw[3::4])>128:raise ValueError('Unexpected PS2 board-trail texture alpha range')
    memory=elf_memory(elf);profile={};provenance={}
    for name,address,kind in PARAMETERS:
        profile[name]=struct.unpack_from('<'+kind,memory,GP+address)[0]
        provenance[name]=dict(address=hex(GP+address),bits=hex(struct.unpack_from('<I',memory,GP+address)[0]))
    targets=struct.unpack_from('<14I',memory,0x487b40)
    depth={0x2e8900:'packed_depth_scale',0x2e88f8:'loose_depth_scale',0x2e8908:'powder_depth_scale',0x2e8910:'deep_powder_depth_scale',0x2e8920:None}
    surface_gates=[dict(surface=i,enabled=depth[target] is not None,depth_parameter=depth[target]) for i,target in enumerate(targets)]
    output.mkdir(parents=True,exist_ok=True)
    (output/'btrl.rgba').write_bytes(raw)
    preview=bytearray(raw)
    for i in range(3,len(preview),4):preview[i]=min(255,round(preview[i]*255/128))
    (output/'btrl-preview.png').write_bytes(png(width,height,bytes(preview)))
    result=dict(version=1,profile=profile,surface_gates=surface_gates,
        texture=dict(id=55,name='btrl',binding_proof='0x2EFFAC loads55;0x2F0008 writesGP+14E4;0x386FD0 selects this ID;0x4891B0+55*12 names btrl',file='btrl.rgba',width=width,height=height,format='RGBA8',source_alpha_max=128,
                     normalized_sample_alpha_multiplier=255/128,raw_sha256=hashlib.sha256(raw).hexdigest(),
                     preview='btrl-preview.png',note='Raw original texels retained. Preview alone expands PS2 alpha to PNG range; renderer must apply original alpha convention.'),
        topology=dict(capacity_slices=54,bands=6,vertex_source_stride=48,band_order=[2,0,3,4,1,5],
                      triangle_strip_pairs=[[2,0],[0,3],[3,4],[4,1],[1,5]],
                      band_u=[profile['inner_texture_u'],1-profile['inner_texture_u'],profile['outer_texture_u'],profile['middle_texture_u'],1-profile['middle_texture_u'],1-profile['outer_texture_u']],
                      sampling_distance_cm=110,special_sampling_distance_cm=30,
                      retention='54-slice ring; draw window and segment fade are source-derived, not a time-based generic lifetime'),
        source=dict(elf_sha256=hashlib.sha256(elf).hexdigest(),container='DATA/TEXTURES/EFFECTS.SSH',container_sha256=hashlib.sha256(source).hexdigest(),texture_offset=offset,
                    initialize='0x2E81E8',surface_gate='0x2E87E8',sample_gate='0x2E86F0',update='0x2E8938',draw_window='0x2EA538',render='0x386E78',
                    debug_parameter_menu='0x24B0E8',parameter_provenance=provenance),
        limitations=['Native geometry/ring state matches30000 complete original calls. GS/VU render-pass parity is a separate validation; this export does not claim complete gameplay equivalence.'])
    (output/'board_trail.json').write_text(json.dumps(result,indent=2)+'\n');return result

def main():
    from disc_paths import ps2_iso;parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--iso',type=Path,default=ps2_iso());parser.add_argument('--output',type=Path,default=Path('local/assets/native/BOARD_TRAIL'));args=parser.parse_args()
    result=export(args.iso,args.output);print(f"Exported original {result['texture']['width']}x{result['texture']['height']} btrl texels and {len(PARAMETERS)} authored parameters to {args.output}")
if __name__=='__main__':main()
