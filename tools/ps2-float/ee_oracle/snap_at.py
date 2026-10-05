"""Savestate at the exact start of a chosen tick's rider pass, for the EE oracle (docs/ps2-float.md "EE oracle").

usage: snap_at.py STATE.p2s TICK OUT.p2s [--mode exact|mode1] [--speed turbo|normal] [--timeout S]

STATE.p2s is a capture's built savestate (its hooks drive the scripted pad). A derived copy gets one more hook: the
entry of the rider pass 0x128AF0 jumps to a stub that spins while the game tick (rider manager +8) equals TICK, writing
TICK to a marker word first. ARMSX2 runs it (one emulator, silent, PS2_CAPTURE_FPU semantics via --mode); once the
marker shows TICK the state is saved over PINE and the emulator stopped. The saved EE memory is the start of TICK's
rider pass: 0x128AF0(rider manager) in the oracle runs exactly that tick.
"""
import argparse
import shutil
import struct
import subprocess
import sys
import time
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / 'tools'))
import ps2_capture as pc  # noqa: E402
from reference_replay import patch_state  # noqa: E402

PASS_ENTRY = 0x128AF0
PASS_ENTRY_WORDS = (0x27BDFF70, 0x7FB30050)   # addiu sp,sp,-0x90 / sq s3,0x50(sp)
STUB = 0xFF800
FLAG = 0xFF900          # +0 target tick, +4 the tick the stub is spinning at, +8 the two displaced entry words


def freeze_patches(memory, tick):
    """The stub, its data (target tick, spin marker, the two entry words it displaced) and the entry jump.

    The entry may already be hooked (a --ai-state capture jumps from 0x128AF0 to its own arena): the stub then replays
    the displaced jump and its delay slot, so that hook still runs; otherwise it replays the prologue and jumps back."""
    entry_words = struct.unpack_from('<2I', memory, PASS_ENTRY)
    hooked = (entry_words[0] >> 26) == 2
    if not hooked and entry_words != PASS_ENTRY_WORDS:
        raise ValueError('0x128AF0 is neither the rider pass prologue nor a hook jump')
    a = pc.Asm(STUB)
    T0, T1, T2 = pc.T0, pc.T1, pc.T2
    a.lui(T0, FLAG >> 16)
    a.ori(T0, T0, FLAG & 0xFFFF)
    a.lw(T1, 0, T0)
    a.beq(T1, pc.ZERO, 'resume')
    a.nop()
    pc.tick_into(a, T2)
    a.bne(T1, T2, 'resume')
    a.nop()
    a.label('spin')
    a.sw(T2, 4, T0)
    a.lw(T1, 0, T0)
    a.bne(T1, pc.ZERO, 'spin')
    a.nop()
    a.label('resume')
    a.emit(entry_words[0])
    a.emit(entry_words[1])
    if not hooked:
        a.j(PASS_ENTRY + 8)
        a.nop()
    code = a.link()
    data = struct.pack('<4I', tick, 0, *entry_words)
    if any(memory[STUB:STUB + len(code)]) or any(memory[FLAG:FLAG + len(data)]):
        raise ValueError('the freeze stub arena 0xFF800..0xFF910 is not free')
    jump = struct.pack('<2I', (2 << 26) | ((STUB >> 2) & 0x3FFFFFF), 0)
    return [
        dict(address=hex(STUB), expected=memory[STUB:STUB + len(code)].hex(), replacement=code.hex()),
        dict(address=hex(FLAG), expected=memory[FLAG:FLAG + len(data)].hex(), replacement=data.hex()),
        dict(address=hex(PASS_ENTRY), expected=memory[PASS_ENTRY:PASS_ENTRY + 8].hex(), replacement=jump.hex()),
    ]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('state')
    parser.add_argument('tick', type=int)
    parser.add_argument('output')
    parser.add_argument('--mode', default='exact', choices=sorted(pc.FPU_MODES))
    parser.add_argument('--speed', default='turbo', choices=['turbo', 'normal'])
    parser.add_argument('--timeout', type=int, default=900)
    args = parser.parse_args()
    output = Path(args.output)
    output.parent.mkdir(parents=True, exist_ok=True)
    derived = output.with_name(output.stem + '.freeze.p2s')
    with zipfile.ZipFile(args.state) as archive:
        memory = archive.read('eeMemory.bin')
    patch_state(Path(args.state), derived, freeze_patches(memory, args.tick))
    import random
    slot = random.randint(28100, 28999)
    datapath = pc.DATAPATH.parent / f'pcsx2-snap-{slot}'
    pc.prepare_datapath(datapath, slot, args.mode)
    command = [str(pc.PCSX2), '-datapath', str(datapath), '-batch', '-nogui', '-statefile', str(derived.resolve())]
    if args.speed == 'turbo':
        command.append('-turbo')
    command += ['--', str(pc.ISO)]
    log = open(output.with_suffix('.pcsx2.log'), 'w')
    process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
    pine = None
    started = time.time()
    last_report = started
    try:
        while True:
            if time.time() - started > args.timeout:
                raise RuntimeError('timed out before the tick')
            if process.poll() is not None:
                raise RuntimeError(f'emulator exited early ({process.returncode})')
            if pine is None:
                try:
                    pine = pc.Pine(slot)
                except OSError:
                    time.sleep(0.5)
                    continue
            try:
                spinning, = struct.unpack('<I', pine.read(FLAG + 4, 4))
            except (OSError, RuntimeError, ConnectionError):
                time.sleep(0.2)
                continue
            if spinning == args.tick:
                break
            if time.time() - last_report > 10:
                print(f'waiting: marker {spinning}', file=sys.stderr, flush=True)
                last_report = time.time()
            time.sleep(0.05)
        pine.request(b'\x09' + bytes([1]))
        deadline = time.time() + 60
        saved = []
        while time.time() < deadline and not saved:
            time.sleep(0.5)
            saved = list(datapath.rglob('*.p2s'))
        if not saved:
            raise RuntimeError('the savestate did not appear')
        time.sleep(2)
        shutil.copyfile(saved[0], output)
    finally:
        if pine:
            pine.close()
        process.terminate()
        try:
            process.wait(10)
        except subprocess.TimeoutExpired:
            process.kill()
        log.close()
        shutil.rmtree(datapath, ignore_errors=True)
    print(output)


if __name__ == '__main__':
    main()
