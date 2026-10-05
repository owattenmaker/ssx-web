"""Extract a PCSX2 / ARMSX2 savestate's EE memory for the EE oracle (tools/ps2-float/ee_oracle/oracle.cpp).

usage: state.py STATE.p2s OUT_DIR
Writes eeMemory.bin, Scratchpad.bin, vu0Memory.bin and vu0MicroMem.bin, and prints the game tick the state holds
(the same read as tools/ps2_capture.py's snapshots) and the rider manager, the a0 of the rider pass 0x128AF0.
"""
import struct
import sys
import zipfile
from pathlib import Path

GP = 0x4A30F0


def main():
    out = Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(sys.argv[1]) as archive:
        for name in ('eeMemory.bin', 'Scratchpad.bin', 'vu0Memory.bin', 'vu0MicroMem.bin'):
            (out / name).write_bytes(archive.read(name))
    memory = bytearray((out / 'eeMemory.bin').read_bytes())
    # A snap_at.py state still carries its freeze hook: put the rider pass entry back and clear the stub arena.
    jump = struct.pack('<I', (2 << 26) | ((0xFF800 >> 2) & 0x3FFFFFF))
    if memory[0x128AF0:0x128AF4] == jump:
        # snap_at.py keeps the two entry words it displaced (the prologue, or a capture hook's jump) at 0xFF908.
        memory[0x128AF0:0x128AF8] = memory[0xFF908:0xFF910]
        memory[0xFF800:0xFF910] = bytes(0x110)
        (out / 'eeMemory.bin').write_bytes(bytes(memory))
        print('freeze hook removed')

    def word(address):
        return struct.unpack_from('<I', memory, address & 0x1FFFFFF)[0]

    manager = word(word(word(GP - 0x848) + 0x84) + 0x0C)
    # The rider manager (cAI): its +8 is the game tick, and 0x128AF0(manager) is one whole rider pass.
    print('tick', word(manager + 8), 'rider_manager', hex(manager))


if __name__ == '__main__':
    main()
