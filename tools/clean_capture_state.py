#!/usr/bin/env python3
"""Turn a kept tools/ps2_capture.py savestate (RUN.tickN.p2s, hooks installed) back into the unmodified game
(development reference only): every hooked instruction outside the capture arena gets its original bytes from the
capture's patches file, and the whole arena 0x96000..0x100000 (code, control block, script, ring, AI areas) is zeroed,
as it is in the baseline. The result is an ordinary savestate at that tick (usable by tools/ps2_navigate.py).

    python3 tools/clean_capture_state.py RUN.tickN.p2s RUN.patches.json OUT.p2s
"""
import json, sys, zipfile
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from reference_replay import patch_state  # noqa: E402

ARENA = (0x96000, 0x100000)


def main(state, patches_path, out):
    patches = json.loads(Path(patches_path).read_text())['patches']
    memory = zipfile.ZipFile(state).read('eeMemory.bin')
    base = json.loads(Path(patches_path).read_text())['source']
    out_patches = []
    for p in patches:
        at = int(p['address'], 0); n = len(bytes.fromhex(p['expected']))
        if ARENA[0] <= at < ARENA[1]: continue
        current = memory[at:at + n]
        if current != bytes.fromhex(p['expected']): out_patches.append(dict(address=hex(at), expected=current.hex(), replacement=p['expected']))
    arena = memory[ARENA[0]:ARENA[1]]
    out_patches.append(dict(address=hex(ARENA[0]), expected=arena.hex(), replacement='00' * len(arena)))
    patch_state(Path(state), Path(out), out_patches)
    print(json.dumps(dict(output=out, restored=len(out_patches) - 1)))


if __name__ == '__main__':
    main(*sys.argv[1:4])
