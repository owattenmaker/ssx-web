#!/usr/bin/env python3
"""Exhaustive byte-domain rider texture combination against the GS software path."""
from pathlib import Path
import hashlib
import json
import subprocess
root=Path(__file__).resolve().parents[1]
vendor=root/'local/vendor/PS2Recomp/ps2xRuntime'
source=(vendor/'src/lib/gs/gs_cpu_backend.cpp').read_text()
start=source.index('    struct TextureCombineResult\n')
end=source.index('    uint32_t swizzleClutIndexCSM1(',start)
combine=source[start:end]
common=(vendor/'include/runtime/gs/ps2_gs_common.h').read_text()
start=common.index('static inline uint8_t clampU8(int v)')
end=common.index('\n}',start)+2
clamp=common[start:end]
work=root/'local/rider-lighting';work.mkdir(parents=True,exist_ok=True)
include=work/'rider_texture_original.inc'
# Only these selector fields are read by the extracted pure byte combiner.
include.write_text('struct GSTex0Reg { unsigned tfx=0,tcc=0; };\n'+clamp+'\n'+combine)
binary=root/'build/ssx3_rider_texture_reference'
subprocess.run(['xcrun','clang++','-std=c++20','-O2','-I'+str(work),str(root/'tests/rider_texture_reference.cpp'),'-o',str(binary)],check=True)
golden=root/'web/public/test-data/rider-texture-reference.bin';golden.parent.mkdir(parents=True,exist_ok=True)
subprocess.run([str(binary),str(golden)],check=True,timeout=60)
(work/'texture-combine-reference.json').write_text(json.dumps(dict(
    source='PS2Recomp GS CPU backend combineTexture, TFX3/TCC1',
    source_sha256=hashlib.sha256(source.encode()).hexdigest(),
    extracted_sha256=hashlib.sha256(include.read_bytes()).hexdigest(),
    gpu_reference_sha256=hashlib.sha256(golden.read_bytes()).hexdigest(),gpu_reference_pixels=6*256*256,
    cases=256**3,scope='Byte combination only; excludes texture sampling, framebuffer blending and console framebuffer parity'),indent=2)+'\n')
