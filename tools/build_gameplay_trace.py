#!/usr/bin/env python3
from pathlib import Path
import shlex,subprocess
root=Path(__file__).resolve().parents[1];out=root/'local/browser-validation';out.mkdir(exist_ok=True)
shim=out/'include/emscripten';shim.mkdir(parents=True,exist_ok=True);(shim/'emscripten.h').write_text('#pragma once\n#define EMSCRIPTEN_KEEPALIVE\n')
line=next(x for x in (root/'web/build-core.sh').read_text().splitlines() if x.startswith('local/vendor/emsdk/'))
files=[x for x in shlex.split(line) if x.endswith('.cpp')];sources=[]
for name in files:
 if name.startswith('web/generated/'):
  if name.endswith('/animation_graph.cpp'):
   # Restore the native root/sequence rounding scopes stripped by the browser generator.
   text=(root/name).read_text().replace('/* WebAssembly nearest rounding; original operation order retained. */','ssx::OriginalRounding round;')
   path=root/'web/generated/native_animation_graph.cpp';path.write_text(text);sources.append(str(path))
  else:sources.append(str(root/'engine'/Path(name).name))
 else:sources.append(str(root/name))
command=['clang++','-std=c++20','-O2','-frounding-math','-ffp-contract=off','-I'+str(out/'include'),'-I'+str(root/'engine'),'-I'+str(root/'local/vendor/ModernGekko/vendor/dolphin/Externals/tinygltf/tinygltf')]+sources+[str(root/'web/native-gameplay-trace.cpp'),'-o',str(out/'native-gameplay-trace')]
subprocess.run(command,check=True)
