#!/usr/bin/env python3
"""Development-only procedural rider-pose original instruction oracle."""
from pathlib import Path
import argparse,subprocess,zipfile
parser=argparse.ArgumentParser(description=__doc__);parser.add_argument("--live",action="store_true");args=parser.parse_args()
from original_fp_oracle import write_scalar_fp_oracle
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p)for p in includes]
command += [str(root/('tests/live_rider_pose_reference.cpp' if args.live else 'tests/rider_pose_reference.cpp'))]+[str(root/'engine'/f)for f in ['animation_motion.cpp','rider_pose_motion.cpp','orientation_motion.cpp','ground_motion.cpp','ground_pose_motion.cpp','ground_animation_control.cpp']]
for name in (['0011EB98','0011FA10','00310200','0011F3D8','0031BE50','0031C128'] if args.live else ['0011EB98','0011FA10','0031BE50','0031C128','0031C040','0013D818','00131620','0012EE30','0031C228']):
    path=next((root/'local/output').glob('sub_'+name+'*'));source=path.read_text()
    if name=='0011EB98' and not args.live:
        for boundary in ['ctx->pc = 0x11f008u;','label_11f00c:']:
            assert source.count(boundary)==1;source=source.replace(boundary,boundary+'\n ctx->pc=0x12345678;return;')
    if name=='00131620':
        if 'case 0x131980u:' not in source:
            source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x131980u:goto pose_entry_131980;').replace('    // 0x131980:','pose_entry_131980:\n    // 0x131980:')
        if 'label_131be8:' in source:source=source.replace('label_131be8:','label_131be8:\n ctx->pc=0x12345678;return;')
        else:source=source.replace('    // 0x131be8:','ctx->pc=0x12345678;return;\n    // 0x131be8:')
    if name=='0013D818':
        if 'case 0x13e148u:' not in source:
            source=source.replace('switch (ctx->pc) {','switch (ctx->pc) {\ncase 0x13e148u:goto pose_entry_13e148;')
            source=source.replace('    // 0x13e148:', 'pose_entry_13e148:\n    // 0x13e148:')
        source=source.replace('label_13e22c:','label_13e22c:\n ctx->pc=0x12345678;return;')
        assert source.count('label_13f11c:')==1;source=source.replace('label_13f11c:','label_13f11c:\n ctx->pc=0x12345678;return;')
    output=root/'build'/('pose-oracle-'+name+'.cpp');write_scalar_fp_oracle(path,output,source);command.append(str(output))
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for f in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',f]
binary=root/('build/ssx3_live_rider_pose_reference' if args.live else 'build/ssx3_rider_pose_reference');command+=['-o',str(binary)];subprocess.run(command,check=True)
fixture=root/'build/rider-pose-oracle-ram.bin';fixture.write_bytes(zipfile.ZipFile(root/'local/reference/pcsx2/snow-jam-glide.p2s').read('eeMemory.bin'))
subprocess.run([str(binary),str(fixture)],check=True,timeout=60)
