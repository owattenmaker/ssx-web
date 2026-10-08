#!/usr/bin/env python3
"""A TAS pad on the PS2 (docs/tas.md "Proof"): tools/ps2_capture.py's capture, with the pad from a TAS file (tools/tas/pad-format.mjs).

tools/ps2_capture.py's script table holds at most ~320 pad entries of 112 bytes (0x97100..0xA0000). A TAS changes the stick on
almost every tick, so this keeps ps2_capture's hooks and records and changes only where the pad hook reads its sample:

* the TAS as 8 bytes a tick (u16 button bits in PAD_BUTTONS order, then the rx, ry, lx, ly stick bytes, 2 spare) at 0xE0000, and
  a 256-entry stick table (the two channels of a stick byte: the original axial response, ps2_capture.decode_pad's arithmetic)
  at 0xF4000. Both are in the arena verify_baseline requires to be zero and a capture without --ai-state does not use
  (its ring is 0xA0000..0xE0000);
* the pad hook's script scan (`li t1, SCRIPT + 16` .. its 24 copies) jumps to a decoder at 0xF6000 instead, which writes the
  24 channels of the tick's entry (the script index; the last entry holds after the end) and jumps back to the hook's exit.

Buttons are written as ps2_capture does for a full press (Select..R3 1.0, the others float32_zero(255 x 1/255)); the TAS
only uses the pressures 00 / ff. Everything else (the derived state, the manifest, `run`, the records) is ps2_capture's.

    python3 tools/tas/ps2_tas_capture.py BASELINE.p2s PAD.tas OUT.p2s [--skip 18] [--watch ADDR:LEN ...]
    PS2_CAPTURE_FPU=mode1 nice -n 10 python3 tools/ps2_capture.py run OUT.p2s OUT.bin --frames N --timeout 1500

--skip: TAS ticks before the baseline's first scripted tick (the page's race starts at tick 0, a countdown state at game tick
18 starts the script at the page's tick 18).
"""
import argparse, json, struct, sys, tempfile, zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import ps2_capture as pc  # noqa: E402
from reference_replay import patch_state  # noqa: E402
from reference_input import float32_zero  # noqa: E402

TABLE = 0xE0000
TABLE_END = 0xF4000
LUT = 0xF4000
DECODER = 0xF6000
DECODER_END = 0xF8000
FULL = float32_zero(255 * pc.RECIPROCAL_255)


def parse_tas(text):
    """The pad frames of a TAS file: [(buttons 16 bytes, [lx, ly, rx, ry])] one per tick (tools/tas/pad-format.mjs parse)."""
    changes, end = [], -1
    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            continue
        if line.startswith('#'):
            if line.startswith('# end '):
                end = int(line.split()[2])
            continue
        tick, hexs, *sticks = line.split()
        changes.append((int(tick), bytes.fromhex(hexs), [int(s) for s in sticks]))
    if end < 0:
        end = changes[-1][0] + 1
    frames, k, cur = [], 0, (bytes(16), [128, 128, 128, 128])
    for t in range(end):
        while k < len(changes) and changes[k][0] <= t:
            cur = (changes[k][1], changes[k][2])
            k += 1
        frames.append(cur)
    return frames


def table_bytes(frames):
    out = bytearray()
    for buttons, (lx, ly, rx, ry) in frames:
        if any(b not in (0, 255) for b in buttons):
            raise ValueError('only full presses (00 / ff) reach the PS2 the way ps2_capture presses them')
        bits = sum(1 << i for i, b in enumerate(buttons) if b)
        out += struct.pack('<H6B', bits, rx, ry, lx, ly, 0, 0)
    return bytes(out)


def lut_bytes():
    out = bytearray()
    for b in range(256):
        negative = max(((79 - b) * 255) // 79 if 79 - b >= 0 else -((-(79 - b) * 255) // 79), 0)
        positive = max(((b - 176) * 255) // 79 if b - 176 >= 0 else -((-(b - 176) * 255) // 79), 0)
        out += struct.pack('<2f', float32_zero(negative * pc.RECIPROCAL_255), float32_zero(positive * pc.RECIPROCAL_255))
    return bytes(out)


def decoder(count, back):
    """Entry: t6 = the script index, a2 = the 24-float sample. Clobbers t1..t7 (the hook's own temporaries)."""
    a = pc.Asm(DECODER)
    T1, T2, T3, T4, T5, T6, T7, A2, ZERO = pc.T1, pc.T2, pc.T3, pc.T4, pc.T5, pc.T6, pc.T7, pc.A2, pc.ZERO
    lbu = lambda rt, off, rs: a.i(36, rt, rs, off)
    lhu = lambda rt, off, rs: a.i(37, rt, rs, off)
    srl = lambda rd, rt, sa: a.r(rd, 0, rt, 2, sa)
    # the last entry holds after the end
    a.li(T1, count - 1)
    a.sltu(T5, T1, T6)
    a.beq(T5, ZERO, 'inside')
    a.nop()
    a.addu(T6, T1, ZERO)
    a.label('inside')
    a.sll(T5, T6, 3)
    a.li(T1, TABLE)
    a.addu(T1, T1, T5)
    lhu(T4, 0, T1)
    one = struct.unpack('<I', struct.pack('<f', 1.0))[0]
    full = struct.unpack('<I', struct.pack('<f', FULL))[0]
    for i in range(16):
        srl(T5, T4, i)
        a.andi(T5, T5, 1)
        a.addu(T7, ZERO, ZERO)
        a.beq(T5, ZERO, f'b{i}')
        a.nop()
        a.li(T7, one if i < 4 else full)
        a.label(f'b{i}')
        a.sw(T7, 4 * i, A2)
    # the table keeps rx, ry, lx, ly at bytes 2..5: channels 16/17, 18/19, 20/21, 22/23
    for k in range(4):
        lbu(T5, 2 + k, T1)
        a.sll(T5, T5, 3)
        a.li(T7, LUT)
        a.addu(T7, T7, T5)
        a.lw(T2, 0, T7)
        a.lw(T3, 4, T7)
        a.sw(T2, 4 * (16 + 2 * k), A2)
        a.sw(T3, 4 * (17 + 2 * k), A2)
    a.j(back)
    a.nop()
    return a.link()


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('baseline')
    p.add_argument('pad')
    p.add_argument('output')
    p.add_argument('--skip', type=int, default=18)
    p.add_argument('--watch', action='append', default=[])
    args = p.parse_args()
    frames = parse_tas(Path(args.pad).read_text())[args.skip:]
    table = table_bytes(frames)
    if TABLE + len(table) > TABLE_END:
        raise ValueError(f'the TAS ({len(frames)} ticks) does not fit the table area')
    out = Path(args.output)
    stage = out.with_name(out.stem + '.stage.p2s')
    watches = [(int(w.split(':')[0], 0), int(w.split(':')[1], 0)) for w in args.watch]
    # ps2_capture's own build, with a one-segment script (its scan is replaced below)
    with tempfile.NamedTemporaryFile('w', suffix='.json', delete=False) as f:
        json.dump({'segments': [{'frames': len(frames)}]}, f)
        script_path = f.name
    manifest = pc.build(args.baseline, script_path, stage, watches=watches)
    memory = zipfile.ZipFile(stage).read('eeMemory.bin')
    lui = (15 << 26) | (pc.T1 << 16) | ((pc.SCRIPT + 16) >> 16)
    ori = (13 << 26) | (pc.T1 << 21) | (pc.T1 << 16) | ((pc.SCRIPT + 16) & 0xFFFF)
    code = memory[pc.CODE:pc.CODE + 0x1000]
    words = list(struct.unpack('<1024I', code))
    at = next(i for i in range(len(words) - 1) if words[i] == lui and words[i + 1] == ori)
    done = next(i for i in range(at, len(words)) if words[i] == 0x27BDFFC0)   # pad_done: the relocated addiu sp, sp, -0x40
    back = pc.CODE + 4 * done
    dec = decoder(len(frames), back)
    if DECODER + len(dec) > DECODER_END:
        raise ValueError('decoder exceeds its area')
    jump = struct.pack('<2I', (2 << 26) | (DECODER >> 2), 0)
    lut = lut_bytes()
    for start, data in ((TABLE, table), (LUT, lut), (DECODER, dec)):
        if any(memory[start:start + len(data)]):
            raise ValueError(f'area {start:#x} is not free')
    patches = [
        dict(address=hex(pc.CODE + 4 * at), expected=code[4 * at:4 * at + 8].hex(), replacement=jump.hex()),
        dict(address=hex(TABLE), expected='00' * len(table), replacement=table.hex()),
        dict(address=hex(LUT), expected='00' * len(lut), replacement=lut.hex()),
        dict(address=hex(DECODER), expected='00' * len(dec), replacement=dec.hex()),
    ]
    patch_state(stage, out, patches)
    manifest.update(tas=dict(pad=str(Path(args.pad).resolve()), skip=args.skip, ticks=len(frames), table=hex(TABLE), lut=hex(LUT),
                             decoder=hex(DECODER), hook_scan=hex(pc.CODE + 4 * at), hook_exit=hex(back)))
    out.with_suffix('.capture.json').write_text(json.dumps(manifest, indent=2) + '\n')
    # ps2_capture's own patches (hooks, control block) stay beside the output: OUT.base-patches.json
    stage.with_suffix('.patches.json').rename(out.with_name(out.stem + '.base-patches.json'))
    stage.with_suffix('.capture.json').unlink()
    stage.unlink()
    print(json.dumps(dict(output=str(out), ticks=len(frames), rider=manifest['rider'], others=manifest['others'], location=manifest['location'])))


if __name__ == '__main__':
    main()
