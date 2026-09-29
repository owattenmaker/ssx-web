#!/usr/bin/env python3
"""Original VU irradiance coefficient preprocessing and normal evaluation."""
from pathlib import Path
import subprocess
from original_fp_oracle import write_scalar_fp_oracle
from original_oracle_build import cached_oracle_build
root=Path(__file__).resolve().parents[1];vendor=root/'local/vendor/PS2Recomp';build=root/'build/ps2recomp'
includes=[vendor/'ps2xRuntime/include',vendor/'ps2xRuntime/src/lib/Kernel',vendor/'ps2xIOP/include',build/'_deps/sse2neon-src',root/'local/output']
command=['xcrun','clang++','-std=c++20','-O2','-arch','arm64','-DUSE_SSE2NEON','-frounding-math','-ffp-contract=off']+['-I'+str(p) for p in includes]+[str(root/'tests/irradiance_evaluate_reference.cpp')]
from extract_fx_microcode import extract_programs
from export_irradiance import decode_bank
import struct,hashlib,json
work=root/'local/rider-lighting';work.mkdir(parents=True,exist_ok=True)
code,source=extract_programs((root/'local/disc/SLUS_207.72').read_bytes())[2]
# Pin the source program and the exact shader block, not a hand-written reference.
program=work/'irradiance-program.bin';program.write_bytes(code)
ps2=decode_bank((root/'local/assets/source/ps2/irr.dat').read_bytes(),'<')
gc=decode_bank((root/'local/gamecube/disc/files/data/worlds/irrngc.dat').read_bytes(),'>')
if len(ps2)!=45 or ps2.keys()!=gc.keys() or any(v['coefficient_bits']!=gc[k]['coefficient_bits'] for k,v in ps2.items()):raise ValueError('Original light banks differ')
bank=work/'irradiance-banks.bin';bank.write_bytes(b''.join(struct.pack('<40I',*v['coefficient_bits'])for v in ps2.values()))
(work/'evaluation-source.json').write_text(json.dumps(dict(program=source,program_sha256=hashlib.sha256(code).hexdigest(),scope='Program2 coefficient preprocessing1150 and ITOF15 normal decoding/transform04B0/04E0..04F0, polynomial04F8..05B0 and FTOI0 at0668; matrix selection, material/GS output excluded'),indent=2)+'\n')
# Recompile the VU upper executor with the oracle's no-contraction flags.
# The cached runtime library may otherwise contract MADD into host FMA.
command += [str(vendor/'ps2xRuntime/src/lib/vu/ps2_vu1_upper.cpp')]
command += [str(build/'ps2xRuntime/libps2_runtime.a'),str(build/'_deps/raylib-build/raylib/libraylib.a'),str(build/'ps2xIOP/libps2_iop.a')]
for framework in ['OpenGL','Cocoa','IOKit','CoreFoundation']:command+=['-framework',framework]
binary=root/'build/ssx3_irradiance_evaluate_reference';command+=['-o',str(binary)]
cached_oracle_build(command,root/'build/original-irradiance-evaluate-objects')
golden=root/'web/public/test-data/irradiance-reference.bin';golden.parent.mkdir(parents=True,exist_ok=True)
run=subprocess.run([str(binary),str(program),str(bank),str(golden),str(root/'web/public/test-data/rider-normal-reference.bin')],check=False,timeout=60,capture_output=True,text=True)
print(run.stdout,end='');print(run.stderr,end='');run.check_returncode()
if 'missing-target' in run.stdout or 'missing-target' in run.stderr: raise RuntimeError('Incomplete original irradiance evaluation')

metadata=work/'evaluation-source.json'
record=json.loads(metadata.read_text());record.update(gpu_reference_sha256=hashlib.sha256(golden.read_bytes()).hexdigest(),gpu_record_floats=48,gpu_records=12000)
record.update(gpu_normal_reference_sha256=hashlib.sha256((root/'web/public/test-data/rider-normal-reference.bin').read_bytes()).hexdigest(),gpu_normal_record_floats=20)
metadata.write_text(json.dumps(record,indent=2)+'\n')
