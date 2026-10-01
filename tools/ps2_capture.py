#!/usr/bin/env python3
"""Automated original-game gameplay capture through PCSX2 (development reference only).

`build` derives a disposable savestate from a verified Snow Jam checkpoint and installs
two guarded hooks in the free 0x96000..0x100000 arena:

* 0x321298 (consumed pad-sample history update): when its pad argument is the human
  rider's pad, the 24 decoded channels are replaced by a scripted sample selected by the
  number of distinct game ticks seen so far.  Everything downstream (button history,
  INPUT.MAP expressions, provider 0x127998, controllers) is the original code.
* 0x128630 (common exit of provider 0x127998): appends one 1 KiB record to a RAM ring:
  tick, the two command words the provider just produced, motion mode/control state,
  rider+0x100..0x300 and +0x360..0x3E0, and the live DEFAULT_3 camera words +0..+0x160.
  The record is written before this tick's controllers run, so it holds the state that
  resulted from the previous tick together with the command consumed by this tick.

`build` works on any event baseline: `discover` finds the loaded course (streaming table
0x442168), the human and computer riders (game roster + rider vtables), the live DEFAULT_3
camera and the outer compositor camera (vtable 0x45CA38 scan).  On ARA1 it must reproduce the
audited Snow Jam addresses below, so Snow Jam builds stay byte-identical.  The manifest records
`location`, `resident_locations`, `rider`, `camera`, `outer_camera`, `human_index`, `roster`,
`computer_riders` and `others` (unused computer-rider record slots stay zero).

`build --ai-state` (opt-in) grows records to 32 KiB (ring of 11) and adds the five computer riders' actor/owner
windows, the NPC provider 0x10A768 command words (exit hook 0x10A87C, one tick late), the human actor +0..0x100,
game info and a per-draw log of the shared RNG 0x317810 attributed to rider-manager passes (see ai_layout()).

`run` launches PCSX2 headless with an isolated data path, streams the ring over PINE and
writes the records.  Original discs and reference savestates are never modified.
"""
import argparse, hashlib, json, os, shutil, socket, struct, subprocess, sys, time, zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from reference_replay import patch_state  # noqa: E402
from reference_input import float32_zero  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
CODE = 0x96000
DATA = 0x97000          # control block (0x100 bytes)
SCRIPT = 0x97100        # script: u32 count, 12 pad, then entries of 112 bytes
SCRIPT_END = 0xA0000
RING = 0xA0000
RECORD = 16384
CAPACITY = 16           # 256 KiB ring, 0xA0000..0xE0000
PAD_HOOK = 0x321298
LOG_HOOK = 0x128630
RIDER = 0x14701A0
CAMERA = 0x1A58650
GP = 0x4A30F0
# DATA fields
F_ENABLED, F_HUMAN_PAD, F_INITED, F_LASTTICK, F_INDEX, F_PADCALLS, F_WRITES, F_RIDER, F_CAMERA = (
    0, 4, 8, 12, 16, 20, 24, 28, 32)
F_OTHERS = 36           # five computer rider addresses
F_OWNER = 56            # refreshed each record
F_BONES = 60            # *(*(rider+0x780)+0x2C), refreshed each record
F_OUTER = 64            # outer (compositor) camera
F_BODY = 96             # *(rider+0xAA0), refreshed each record (own slot; 68..80 belong to pose fields)
F_RNG = 100             # shared original RNG words (0x4FF030), set each record
RNG_STATE = 8896        # 6 words -> 8896..8920
# Collision notification hook 0x105D98 entry (a0 actor, a1 contact record, a2 kind):
# DATA 104 ra, 108 a2, 112 count, 116 copy base, 120 tick, 124 a3, 128..224 *(a1+0..0x60),
# 224 a0, 228 a1, 232 f12, 236 f13.  DATA 104..256 -> record 8920..9072 (last call before the record).
COLLIDE_HOOK = 0x105D98
COLLIDE_BYTES = struct.pack('<2I', 0x27BDFF40, 0x7FB10080)   # addiu sp,sp,-0xC0 / sq s1,0x80(sp)
F_HIT_FN, F_HIT_OBJ, F_HIT_COUNT, F_HIT_BASE, F_HIT_TICK, F_HIT_PRE, F_HIT_POST = 104, 108, 112, 116, 120, 128, 224
HIT_RECORD = 8920
CONTROL_OBJECTS = 9072  # owner+0x1C0..0x200 -> 9072..9136 (control 1 object 0x12FC80 at owner+0x1D0: phase,+4,+8,+C,+10)
F_LOCALP, F_LOCALQ, F_ANIM = 68, 72, 76   # *(geometry+0x24), *(geometry+0x28), *(rider+0x784)
# Animation-state extension (record offsets): local pose and sequence lists, written
# after the outer camera.  Each sequence slot is [channel, address, 0xD0 raw bytes].
LOCAL_POSITIONS, LOCAL_ROTATIONS, ANIMATOR, SEQUENCES, SEQUENCE_SLOT, SEQUENCE_SLOTS, SEQUENCE_COUNT = (
    5440, 5952, 6464, 6592, 216, 6, 7888)
OWNER_CONTROL = 7892    # motion owner +0x200..+0x300 (air control state at +0x230)
OUTER = 0x1597D10
OTHERS = (0x18D0C40, 0x18E1270, 0x18F1D50, 0x1902830, 0x1913310)
BUTTONS = ('Select', 'Start', 'L3', 'R3', 'DPadRight', 'DPadLeft', 'DPadUp', 'DPadDown',
           'Triangle', 'Circle', 'Cross', 'Square', 'L1', 'R1', 'L2', 'R2')
# ARMSX2 (native arm64 PCSX2 fork) runs without Rosetta; PCSX2_BINARY overrides it.
PCSX2 = Path(os.environ.get('PCSX2_BINARY', str(ROOT / 'local/vendor/armsx2/ARMSX2.app/Contents/MacOS/ARMSX2')))
from disc_paths import ps2_iso;ISO = ps2_iso()
DATAPATH_TEMPLATE = ROOT / 'local/browser-pickups/pcsx2'
DATAPATH = ROOT / 'local/ps2-capture/pcsx2'
PINE_SLOT = 28033

RECIPROCAL_255 = struct.unpack('<f', struct.pack('<I', 0x3B808081))[0]


def f32(x):
    return struct.unpack('<f', struct.pack('<f', x))[0]


def axis_byte(value):
    """Nearest byte on the symmetric 0..255 range (engine/input_adapter.cpp)."""
    value = max(-1.0, min(1.0, float(value)))
    import math
    return int(math.floor((value + 1.0) * 127.5 + 0.5))


def decode_pad(buttons, lx, ly, rx, ry, pressure=True):
    """Python mirror of originalDecodePad for a fully pressed digital/pressure button set.

    Sticks are normalized [-1,1] with +y meaning up; they are converted to the PS2 byte
    (low byte = up/left) and then through the original axial response."""
    values = [0.0] * 24
    held = set(buttons)
    for name in held:
        if name not in BUTTONS:
            raise ValueError(f'Unknown button {name}')
    for i, name in enumerate(BUTTONS):
        pressed = name in held
        if pressure and 4 <= i < 16:
            values[i] = float32_zero((255 if pressed else 0) * RECIPROCAL_255)
        else:
            values[i] = 1.0 if pressed else 0.0
    raw = [axis_byte(rx), axis_byte(-ry), axis_byte(lx), axis_byte(-ly)]
    for axis, b in enumerate(raw):
        negative = max(((79 - b) * 255) // 79 if 79 - b >= 0 else -((-(79 - b) * 255) // 79), 0)
        positive = max(((b - 176) * 255) // 79 if b - 176 >= 0 else -((-(b - 176) * 255) // 79), 0)
        values[16 + 2 * axis] = float32_zero(negative * RECIPROCAL_255)
        values[17 + 2 * axis] = float32_zero(positive * RECIPROCAL_255)
    return [f32(v) for v in values]


class Asm:
    def __init__(self, base):
        self.base = base; self.words = []; self.labels = {}; self.fix = []

    def here(self): return self.base + 4 * len(self.words)
    def label(self, name): self.labels[name] = len(self.words)
    def emit(self, w): self.words.append(w & 0xFFFFFFFF)
    def i(self, op, rt, rs, imm): self.emit((op << 26) | (rs << 21) | (rt << 16) | (imm & 0xFFFF))
    def r(self, rd, rs, rt, funct, sa=0): self.emit((rs << 21) | (rt << 16) | (rd << 11) | (sa << 6) | funct)
    def nop(self): self.emit(0)
    def lui(self, rt, v): self.i(15, rt, 0, v)
    def ori(self, rt, rs, v): self.i(13, rt, rs, v)
    def li(self, rt, v): self.lui(rt, (v >> 16) & 0xFFFF); self.ori(rt, rt, v & 0xFFFF)
    def addiu(self, rt, rs, v): self.i(9, rt, rs, v)
    def andi(self, rt, rs, v): self.i(12, rt, rs, v)
    def lw(self, rt, off, rs): self.i(35, rt, rs, off)
    def sw(self, rt, off, rs): self.i(43, rt, rs, off)
    def lq(self, rt, off, rs): self.i(30, rt, rs, off)
    def sq(self, rt, off, rs): self.i(31, rt, rs, off)
    def addu(self, rd, rs, rt): self.r(rd, rs, rt, 0x21)
    def sltu(self, rd, rs, rt): self.r(rd, rs, rt, 0x2B)
    def sltiu(self, rt, rs, v): self.i(11, rt, rs, v)
    def sll(self, rd, rt, sa): self.r(rd, 0, rt, 0, sa)
    def j(self, target): self.emit((2 << 26) | ((target >> 2) & 0x3FFFFFF))
    def jalr(self, rs): self.r(31, rs, 0, 0x09)
    def branch(self, op, rs, rt, label):
        self.fix.append((len(self.words), op, rs, rt, label)); self.emit(0)
    def beq(self, rs, rt, label): self.branch(4, rs, rt, label)
    def bne(self, rs, rt, label): self.branch(5, rs, rt, label)

    def link(self):
        for at, op, rs, rt, label in self.fix:
            offset = self.labels[label] - at - 1
            self.words[at] = (op << 26) | (rs << 21) | (rt << 16) | (offset & 0xFFFF)
        return struct.pack('<' + 'I' * len(self.words), *self.words)


T0, T1, T2, T3, T4, T5, T6, T7, T8, T9 = 8, 9, 10, 11, 12, 13, 14, 15, 24, 25
A0, A1, A2, S0, S1, SP, GPR, ZERO = 4, 5, 6, 16, 17, 29, 28, 0
V0, V1, AT = 2, 3, 1


def tick_into(a, reg):
    a.lw(reg, -0x848, GPR); a.lw(reg, 0x84, reg); a.lw(reg, 0x0C, reg); a.lw(reg, 8, reg)


# Trick/score state (docs/tricks-scoring.md): the rider's scoring object *(rider+0x790) +0..0x1D0 and the
# first 0x18 bytes (type, maximum, value, arg, +0x10, points) of each of its 44 HUD message slots *(score+0x1B0)+k*0x9C.
SCORE_RECORD, SCORE_BYTES = 9136, 0x1D0      # -> 9136..9600
HUD_SLOTS_RECORD, HUD_SLOT_COUNT, HUD_SLOT_WORDS = 9600, 44, 6   # -> 9600..10656
F_SCORE, F_HUD = 240, 244   # DATA words refreshed each record (not a switch: local/ctm-events/menu_pad.py once used DATA+0xF0 as one)
# Reserved: 0x9C000..0xA0000 inside the script region is local/ctm-events/menu_pad.py's arena (its device-pad hook, script and the
# pad-hook switch 0x9C8F0); it asserts the arena is still zero, i.e. the tick script never reaches it.
WATCH = 10752         # optional raw memory windows (--watch ADDR:LEN), appended from here (9072.. belongs to fixed fields)


def assemble(watches=(), n_others=5, ai=None):
    """`ai` (--ai-state): dict(entry=AI copy routine address, watch_offset=...) from assemble_ai()."""
    a = Asm(CODE)
    # ---- pad hook (function entry: temporaries are free, a0..a2 must survive) ----
    a.label('pad')
    a.li(T0, DATA)
    a.lw(T1, F_HUMAN_PAD, T0); a.bne(A0, T1, 'pad_done'); a.nop()
    a.lw(T1, F_ENABLED, T0); a.beq(T1, ZERO, 'pad_done'); a.nop()
    tick_into(a, T3)
    a.lw(T4, F_LASTTICK, T0); a.lw(T5, F_INITED, T0)
    a.beq(T5, ZERO, 'pad_first'); a.nop()
    a.beq(T3, T4, 'pad_apply'); a.nop()
    a.lw(T6, F_INDEX, T0); a.addiu(T6, T6, 1); a.sw(T6, F_INDEX, T0); a.sw(T3, F_LASTTICK, T0)
    a.beq(ZERO, ZERO, 'pad_apply'); a.nop()
    a.label('pad_first')
    a.addiu(T5, ZERO, 1); a.sw(T5, F_INITED, T0); a.sw(T3, F_LASTTICK, T0); a.sw(ZERO, F_INDEX, T0)
    a.label('pad_apply')
    a.lw(T7, F_PADCALLS, T0); a.addiu(T7, T7, 1); a.sw(T7, F_PADCALLS, T0)
    a.lw(T6, F_INDEX, T0)
    a.li(T1, SCRIPT + 16)
    a.label('pad_scan')
    a.lw(T4, 0, T1); a.sltu(T5, T6, T4); a.bne(T5, ZERO, 'pad_found'); a.nop()
    a.addiu(T1, T1, 112); a.beq(ZERO, ZERO, 'pad_scan'); a.nop()
    a.label('pad_found')
    for k in range(24):
        a.lw(T4, 16 + 4 * k, T1); a.sw(T4, 4 * k, A2)
    a.label('pad_done')
    a.addiu(SP, SP, -0x40); a.sq(S0, 0x30, SP)          # relocated 0x321298/0x32129C
    a.j(PAD_HOOK + 8); a.nop()
    # ---- provider exit hook (mid-function: save every temporary we touch) ----
    a.label('log')
    a.addiu(SP, SP, -0xC0)
    regs = [T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, V0, V1]
    for n, reg in enumerate(regs): a.sq(reg, n * 16, SP)
    a.li(T0, DATA)
    a.lw(T1, F_ENABLED, T0); a.beq(T1, ZERO, 'log_done'); a.nop()
    if ai:   # AI_RECORD-byte records in an AI_CAPACITY ring: slot counter kept in AI_DATA (wrapped by the AI routine)
        a.lw(T2, F_WRITES, T0); a.li(T3, AI_DATA + AI_F_SLOT); a.lw(T3, 0, T3); a.sll(T3, T3, AI_RECORD_SHIFT)
    else:
        a.lw(T2, F_WRITES, T0); a.andi(T3, T2, CAPACITY - 1); a.sll(T3, T3, 14)
    a.li(T4, RING); a.addu(T3, T3, T4)
    tick_into(a, T5); a.sw(T5, 4, T3)
    a.lw(T5, 0, S0); a.sw(T5, 8, T3); a.lw(T5, 4, S0); a.sw(T5, 12, T3)
    a.lw(T5, 0xDE0, S1); a.sw(T5, 16, T3); a.lw(T5, 0xDE4, S1); a.sw(T5, 20, T3)
    a.lw(T5, F_PADCALLS, T0); a.sw(T5, 24, T3); a.sw(ZERO, F_PADCALLS, T0)
    a.lw(T5, F_INDEX, T0); a.sw(T5, 28, T3)

    def copy(src_field, src_offset, dst_offset, words, name):
        a.lw(T6, src_field, T0); a.addiu(T6, T6, src_offset); a.addiu(T7, T3, dst_offset); a.addiu(T8, ZERO, words)
        a.label(name)
        a.lw(T5, 0, T6); a.sw(T5, 0, T7); a.addiu(T6, T6, 4); a.addiu(T7, T7, 4); a.addiu(T8, T8, -1)
        a.bne(T8, ZERO, name); a.nop()
    copy(F_RIDER, 0x100, 32, 656, 'copy_rider')          # rider+0x100..0xB40 -> 32..2656
    copy(F_CAMERA, 0, 2656, 88, 'copy_camera')           # camera+0..0x160 -> 2656..3008
    for k in range(n_others):                             # computer riders -> 3008..3168 (unused slots stay zero)
        copy(F_OTHERS + 4 * k, 0x110, 3008 + 32 * k, 4, f'copy_other_p{k}')
        copy(F_OTHERS + 4 * k, 0x1E0, 3024 + 32 * k, 4, f'copy_other_v{k}')
    a.lw(T9, F_RIDER, T0); a.lw(T9, 0x77C, T9); a.sw(T9, F_OWNER, T0)   # motion owner = *(rider+0x77C)
    copy(F_OWNER, 0, 3168, 16, 'copy_owner_a')           # owner+0..0x40 -> 3168..3232
    copy(F_OWNER, 0xDE0, 3232, 8, 'copy_owner_b')        # owner+0xDE0..0xE00 -> 3232..3264
    a.lw(T9, F_RIDER, T0); a.lw(T9, 0x780, T9); a.lw(T9, 0x2C, T9); a.sw(T9, F_BONES, T0)  # cached world bones
    copy(F_BONES, 0, 3264, 256, 'copy_bones')            # 32 bones x 32 bytes -> 3264..4288
    copy(F_OUTER, 0, 4288, 288, 'copy_outer')            # outer camera +0..0x480 -> 4288..5440
    a.lw(T9, F_RIDER, T0); a.lw(T9, 0xAA0, T9); a.sw(T9, F_BODY, T0)   # collision body *(rider+0xAA0)
    a.lw(T9, F_RIDER, T0); a.lw(T9, 0x780, T9); a.lw(T1, 0x24, T9); a.sw(T1, F_LOCALP, T0); a.lw(T1, 0x28, T9); a.sw(T1, F_LOCALQ, T0)
    copy(F_LOCALP, 0, LOCAL_POSITIONS, 128, 'copy_local_p')   # 32 local positions (float4)
    copy(F_LOCALQ, 0, LOCAL_ROTATIONS, 128, 'copy_local_q')   # 32 local quaternions
    a.lw(T9, F_RIDER, T0); a.lw(T9, 0x784, T9); a.sw(T9, F_ANIM, T0)
    copy(F_ANIM, 0, ANIMATOR, 32, 'copy_animator')           # animator +0..0x80
    # Walk the six channel lists at *(animator+0x50) (count, head; next = +0xC8).
    a.lw(T9, F_ANIM, T0); a.lw(T9, 0x50, T9)
    a.addiu(T1, T3, SEQUENCES); a.addiu(T4, ZERO, 0); a.addiu(V0, ZERO, SEQUENCE_SLOTS); a.addiu(V1, ZERO, 0)
    a.label('seq_channel')
    a.lw(T5, 4, T9)
    a.label('seq_node')
    a.beq(T5, ZERO, 'seq_next_channel'); a.nop()
    a.beq(V0, ZERO, 'seq_done'); a.nop()
    a.sw(T4, 0, T1); a.sw(T5, 4, T1)
    a.addiu(T6, T5, 0); a.addiu(T7, T1, 8); a.addiu(T8, ZERO, 52)
    a.label('seq_copy')
    a.lw(A0, 0, T6); a.sw(A0, 0, T7); a.addiu(T6, T6, 4); a.addiu(T7, T7, 4); a.addiu(T8, T8, -1)
    a.bne(T8, ZERO, 'seq_copy'); a.nop()
    a.addiu(T1, T1, SEQUENCE_SLOT); a.addiu(V0, V0, -1); a.addiu(V1, V1, 1)
    a.lw(T5, 0xC8, T5)
    a.beq(ZERO, ZERO, 'seq_node'); a.nop()
    a.label('seq_next_channel')
    a.addiu(T9, T9, 8); a.addiu(T4, T4, 1); a.sltiu(A0, T4, 6)
    a.bne(A0, ZERO, 'seq_channel'); a.nop()
    a.label('seq_done')
    a.sw(V1, SEQUENCE_COUNT, T3)
    copy(F_OWNER, 0x200, OWNER_CONTROL, 64, 'copy_owner_c')      # owner+0x200..0x300 -> 7892..8148
    copy(F_BODY, 0, 8192, 176, 'copy_body')              # collision body +0..0x2C0 (broad + 20 child spheres) -> 8192..8896
    a.li(T9, 0x4FF030); a.sw(T9, F_RNG, T0)
    copy(F_RNG, 0, RNG_STATE, 6, 'copy_rng')              # shared RNG state -> 8896..8920
    a.li(T9, DATA + F_HIT_FN); a.sw(T9, F_HIT_BASE, T0)
    copy(F_HIT_BASE, 0, HIT_RECORD, 38, 'copy_hit')       # last ground obstacle callback -> 8920..9072
    at = ai['watch_offset'] if ai else WATCH
    for n, (address, length) in enumerate(watches):     # fixed EE windows (e.g. a dynamic-entity pool)
        a.li(T6, address); a.addiu(T7, T3, at); a.addiu(T8, ZERO, length // 4)
        a.label(f'copy_watch{n}')
        a.lw(T5, 0, T6); a.sw(T5, 0, T7); a.addiu(T6, T6, 4); a.addiu(T7, T7, 4); a.addiu(T8, T8, -1)
        a.bne(T8, ZERO, f'copy_watch{n}'); a.nop()
        at += length
    copy(F_OWNER, 0x1C0, CONTROL_OBJECTS, 16, 'copy_owner_d')    # owner+0x1C0..0x200 (control-1 object at +0x1D0) -> 9072..9136
    a.lw(T9, F_RIDER, T0); a.lw(T9, 0x790, T9); a.sw(T9, F_SCORE, T0)   # scoring object *(rider+0x790)
    copy(F_SCORE, 0, SCORE_RECORD, SCORE_BYTES // 4, 'copy_score')
    a.lw(T9, F_SCORE, T0); a.lw(T9, 0x1B0, T9); a.sw(T9, F_HUD, T0)     # HUD message slot bank *(score+0x1B0)
    a.lw(T6, F_HUD, T0); a.addiu(T7, T3, HUD_SLOTS_RECORD); a.addiu(V0, ZERO, HUD_SLOT_COUNT)
    a.label('hud_slot')
    for k in range(HUD_SLOT_WORDS): a.lw(T5, 4 * k, T6); a.sw(T5, 4 * k, T7)
    a.addiu(T6, T6, 0x9C); a.addiu(T7, T7, 4 * HUD_SLOT_WORDS); a.addiu(V0, V0, -1)
    a.bne(V0, ZERO, 'hud_slot'); a.nop()
    if ai:   # computer-rider/RNG block (assemble_ai), then back here; the sequence word is still written last
        a.j(ai['entry']); a.nop()
        a.label('ai_back')
    a.addiu(T2, T2, 1); a.sw(T2, 0, T3); a.sw(T2, F_WRITES, T0)
    a.label('log_done')
    for n, reg in enumerate(regs): a.lq(reg, n * 16, SP)
    a.addiu(SP, SP, 0xC0)
    # relocated 0x128630 lw a0,0xDF8(s1) / 0x128634 beql a0,zero,0x128648 (lq s0 in likely slot)
    a.lw(A0, 0xDF8, S1)
    a.beq(A0, ZERO, 'log_skip'); a.nop()
    a.j(0x12863C); a.nop()
    a.label('log_skip')
    a.lq(S0, 0x20, SP); a.j(0x128648); a.nop()
    # ---- collision notification hook (function entry: temporaries are free) ----
    a.label('collide')
    a.li(V0, DATA)
    a.sw(31, F_HIT_FN, V0); a.sw(A2, F_HIT_OBJ, V0); a.sw(7, 124, V0)
    a.lw(AT, F_HIT_COUNT, V0); a.addiu(AT, AT, 1); a.sw(AT, F_HIT_COUNT, V0)
    tick_into(a, AT); a.sw(AT, F_HIT_TICK, V0)
    for k in range(6): a.lq(AT, 16 * k, A1); a.sq(AT, F_HIT_PRE + 16 * k, V0)
    a.sw(A0, 224, V0); a.sw(5, 228, V0)
    a.emit(0x44000000 | (AT << 16) | (12 << 11)); a.sw(AT, 232, V0)      # mfc1 at,f12
    a.emit(0x44000000 | (AT << 16) | (13 << 11)); a.sw(AT, 236, V0)      # mfc1 at,f13
    a.addiu(SP, SP, -0xC0); a.sq(S1, 0x80, SP)
    a.j(COLLIDE_HOOK + 8); a.nop()
    code = a.link()
    if ai: ai['back'] = CODE + 4 * a.labels['ai_back']
    return code, CODE + 4 * a.labels['pad'], CODE + 4 * a.labels['log'], CODE + 4 * a.labels['collide']


# ---- --ai-state: computer-rider state, NPC provider commands and shared-RNG attribution --------------------------
# Opt-in (build --ai-state).  Records grow to AI_RECORD bytes in an AI_CAPACITY ring (0xA0000..0xF8000); the extra
# hook code and its data live in 0xF8000..0x100000 (inside the arena verify_baseline requires to be zero).  Offsets
# 0..10752 keep the default layout exactly; the AI block starts at the default watch offset and --watch windows move
# behind it (manifest layout.watch_offset).  All of it is sampled by the human provider-exit hook 0x128630, i.e. in
# pass 121068 of the rider manager 128AF0 for the human, before the human's controllers (111728) and before any
# computer rider's 121068 (provider + controllers) of the same tick.
AI_RECORD, AI_RECORD_SHIFT, AI_CAPACITY = 32768, 15, 11
AI_CODE, AI_CODE_END, AI_DATA = 0xF8000, 0xFE000, 0xFE000
assert RING + AI_RECORD * AI_CAPACITY <= AI_CODE
# AI_DATA words
AI_F_SLOT, AI_F_DRAWS, AI_F_TOTAL, AI_F_MARK_A0, AI_F_MARK_RA, AI_F_UNMATCHED, AI_F_RIDER = 0, 4, 8, 12, 16, 20, 28
AI_F_OTHERS = 32            # five computer rider actors (0 = unused slot)
AI_F_NPC = 64               # per computer rider: w0, w1, game tick at the NPC provider return, returns since last record
AI_F_LOG = 256              # RNG log: AI_LOG_ENTRIES x (resolved caller $ra, leaf $ra, marker a0, marker $ra)
AI_LOG_ENTRIES = 64
# Record layout (bytes)
AI_BASE, AI_STRIDE = 10752, 3648
AI_ACTOR, AI_OWNER_A, AI_OWNER_B, AI_OWNER_C, AI_NPC = 0, 2880, 2944, 3264, 3632   # within one AI slot
AI_HUMAN_HEAD = AI_BASE + 5 * AI_STRIDE         # 28992: human actor +0x0..+0x100 (pair records, ranking)
AI_GAME = AI_HUMAN_HEAD + 0x100                 # 29248: game info *(*(*(gp-0x848)+0x84)+0x0C) +0x0..+0xA0
AI_GLOBALS = AI_GAME + 0xA0                     # 29408: *(0x4D33AC) AI path bank, *(0x4D33B4) course paths, game info ptr, human owner
AI_RNG = AI_GLOBALS + 16                        # 29424: draws since previous record, total draws, marker a0, marker $ra, unmatched NPC returns, 3 x 0
AI_RNG_LOG = AI_RNG + 32                        # 29456: AI_LOG_ENTRIES x 16 bytes
AI_WATCH = 30720                                # --watch windows with --ai-state
assert AI_RNG_LOG + 16 * AI_LOG_ENTRIES <= AI_WATCH
NPC_EXIT = 0x10A87C         # common epilogue of NPC provider 0x10A768: s0 = 8-byte command (a1), s1 = owner (a0)
NPC_EXIT_BYTES = struct.pack('<2I', 0x7BB00030, 0x7BB10020)       # lq s0,0x30(sp) / lq s1,0x20(sp)
RNG_ENTRY = 0x317810        # shared generator draw: 317A08(0x4FF030); wrappers 317830 (range) and 317890 (signed range)
RNG_ENTRY_BYTES = struct.pack('<2I', 0x27BDFFF0, 0x3C040050)      # addiu sp,sp,-0x10 / lui a0,0x50
NPC_PROVIDER_INTERFACE = 0x4585F0
# Attribution markers: entry hooks that store (a0, $ra) in AI_DATA.  They are the per-rider passes of the rider
# manager 128AF0 (a0 = actor for the 12x passes, owner+component for the 2Dx/2Ex/2Fx passes) plus the manager
# entry and the calls around the passes, so each RNG draw carries the pass it happened in and that pass's argument.
MARKERS = (0x128AF0, 0x12BB20, 0x113C20, 0x10F560, 0x120ED8, 0x120F20, 0x121068, 0x1210B0, 0x1211F8, 0x1216E0,
           0x121700, 0x121728, 0x121750, 0x1217F8, 0x121818, 0x1218D0, 0x121950, 0x2DD0B8, 0x2DABC8, 0x2E8938,
           0x2E66B8, 0x2EADD0, 0x2EF6D0, 0x2D4C08, 0x2E39D8, 0x2DF920, 0x2F1150, 0x2F6518, 0x120E88, 0x101B60, 0x1013A8)


def relocatable(word):
    """True when an instruction can run from a trampoline: no branch/jump/trap and no use of $at."""
    op, rs, rt, rd, funct = word >> 26, (word >> 21) & 31, (word >> 16) & 31, (word >> 11) & 31, word & 63
    if op in (1, 2, 3, 4, 5, 6, 7, 20, 21, 22, 23): return False
    if op == 0: return funct not in (8, 9, 12, 13) and AT not in (rs, rt, rd)
    if op in (0x10, 0x11, 0x12): return rs not in (8,) and not (op == 0x11 and rs in (0, 4) and rt == AT)
    if op in (0x31, 0x35, 0x39, 0x3D): return rs != AT          # lwc1/ldc1/swc1/sdc1: rt is an FPR
    if op == 0x1C: return AT not in (rs, rt, rd)
    return AT not in (rs, rt)


def assemble_ai(back, marker_words):
    """AI record block (entered from the provider-exit hook, returns to `back`), NPC exit hook, RNG hook, markers."""
    a = Asm(AI_CODE)
    hi = (AI_DATA + 0x8000) >> 16; lo = AI_DATA - (hi << 16)

    def copy(src, src_offset, dst_offset, words, name):
        a.addiu(T6, src, src_offset); a.addiu(T7, T3, dst_offset); a.addiu(T8, ZERO, words)
        a.label(name)
        a.lw(T5, 0, T6); a.sw(T5, 0, T7); a.addiu(T6, T6, 4); a.addiu(T7, T7, 4); a.addiu(T8, T8, -1)
        a.bne(T8, ZERO, name); a.nop()
    # ---- record block: T0 = DATA, T2 = writes, T3 = record; T1, T4..T9, V0, V1, A0 are saved/dead in the log hook
    a.label('ai_copy')
    a.li(V1, AI_DATA)
    for k in range(MAX_OTHERS):
        base = AI_BASE + k * AI_STRIDE
        a.lw(T9, AI_F_OTHERS + 4 * k, V1); a.beq(T9, ZERO, f'ai_skip{k}'); a.nop()
        copy(T9, 0, base + AI_ACTOR, 0xB40 // 4, f'ai_actor{k}')
        a.lw(T9, 0x77C, T9)                                       # motion owner
        copy(T9, 0, base + AI_OWNER_A, 0x40 // 4, f'ai_owner_a{k}')
        copy(T9, 0x1C0, base + AI_OWNER_B, 0x140 // 4, f'ai_owner_b{k}')
        copy(T9, 0xDE0, base + AI_OWNER_C, 0x170 // 4, f'ai_owner_c{k}')
        copy(V1, AI_F_NPC + 16 * k, base + AI_NPC, 4, f'ai_npc{k}')
        a.sw(ZERO, AI_F_NPC + 16 * k + 12, V1)
        a.label(f'ai_skip{k}')
    a.lw(T9, AI_F_RIDER, V1)
    copy(T9, 0, AI_HUMAN_HEAD, 0x100 // 4, 'ai_human')
    a.lw(T9, -0x848, GPR); a.lw(T9, 0x84, T9); a.lw(T9, 0x0C, T9); a.sw(T9, AI_GLOBALS + 8, T3)
    copy(T9, 0, AI_GAME, 0xA0 // 4, 'ai_game')
    a.li(T9, 0x4D33A0); a.lw(T1, 0xC, T9); a.sw(T1, AI_GLOBALS, T3); a.lw(T1, 0x14, T9); a.sw(T1, AI_GLOBALS + 4, T3)
    a.lw(T9, AI_F_RIDER, V1); a.lw(T9, 0x77C, T9); a.sw(T9, AI_GLOBALS + 12, T3)
    for n, field in enumerate((AI_F_DRAWS, AI_F_TOTAL, AI_F_MARK_A0, AI_F_MARK_RA, AI_F_UNMATCHED)):
        a.lw(T1, field, V1); a.sw(T1, AI_RNG + 4 * n, T3)
    copy(V1, AI_F_LOG, AI_RNG_LOG, AI_LOG_ENTRIES * 4, 'ai_log')
    a.sw(ZERO, AI_F_DRAWS, V1)
    a.lw(T1, AI_F_SLOT, V1); a.addiu(T1, T1, 1); a.addiu(T9, ZERO, AI_CAPACITY)
    a.bne(T1, T9, 'ai_slot'); a.nop()
    a.addiu(T1, ZERO, 0)
    a.label('ai_slot')
    a.sw(T1, AI_F_SLOT, V1)
    a.j(back); a.nop()
    # ---- NPC provider exit 0x10A87C (s0 = command, s1 = owner): keep the words per computer-rider slot
    a.label('npc')
    a.addiu(SP, SP, -0x40)
    for n, reg in enumerate((T0, T1, T2, T3)): a.sq(reg, 16 * n, SP)
    a.li(T0, AI_DATA)
    for k in range(MAX_OTHERS):
        a.lw(T1, AI_F_OTHERS + 4 * k, T0); a.beq(T1, ZERO, f'npc_next{k}'); a.nop()
        a.lw(T1, 0x77C, T1); a.bne(T1, S1, f'npc_next{k}'); a.nop()
        a.addiu(T2, T0, AI_F_NPC + 16 * k); a.beq(ZERO, ZERO, 'npc_store'); a.nop()
        a.label(f'npc_next{k}')
    a.lw(T1, AI_F_UNMATCHED, T0); a.addiu(T1, T1, 1); a.sw(T1, AI_F_UNMATCHED, T0)
    a.beq(ZERO, ZERO, 'npc_done'); a.nop()
    a.label('npc_store')
    a.lw(T1, 0, S0); a.sw(T1, 0, T2); a.lw(T1, 4, S0); a.sw(T1, 4, T2)
    tick_into(a, T1); a.sw(T1, 8, T2)
    a.lw(T1, 12, T2); a.addiu(T1, T1, 1); a.sw(T1, 12, T2)
    a.label('npc_done')
    for n, reg in enumerate((T0, T1, T2, T3)): a.lq(reg, 16 * n, SP)
    a.addiu(SP, SP, 0x40)
    a.lq(S0, 0x30, SP); a.lq(S1, 0x20, SP)                      # relocated 0x10A87C / 0x10A880
    a.j(NPC_EXIT + 8); a.nop()
    # ---- RNG draw 0x317810 entry: count, and log (resolved caller, leaf $ra, marker a0, marker $ra)
    a.label('rng')
    a.addiu(SP, SP, -0x40)
    for n, reg in enumerate((T0, T1, T2, T3)): a.sq(reg, 16 * n, SP)
    a.li(T0, AI_DATA)
    a.lw(T1, AI_F_DRAWS, T0); a.sltiu(T2, T1, AI_LOG_ENTRIES); a.beq(T2, ZERO, 'rng_count'); a.nop()
    a.sll(T2, T1, 4); a.addu(T2, T2, T0)
    a.sw(31, AI_F_LOG + 4, T2)
    a.r(T3, 31, ZERO, 0x25)                                     # or t3,ra,zero
    a.li(T1, 0x31784C); a.bne(T3, T1, 'rng_resolved'); a.nop()   # called from 317830: its caller's $ra at 0(sp)
    a.lw(T3, 0x40, SP)
    a.li(T1, 0x3178C0); a.bne(T3, T1, 'rng_resolved'); a.nop()   # 317830 called from 317890: $ra at 0x20(sp)
    a.lw(T3, 0x60, SP)
    a.label('rng_resolved')
    a.sw(T3, AI_F_LOG, T2)
    a.lw(T1, AI_F_MARK_A0, T0); a.sw(T1, AI_F_LOG + 8, T2); a.lw(T1, AI_F_MARK_RA, T0); a.sw(T1, AI_F_LOG + 12, T2)
    a.label('rng_count')
    a.lw(T1, AI_F_DRAWS, T0); a.addiu(T1, T1, 1); a.sw(T1, AI_F_DRAWS, T0)
    a.lw(T1, AI_F_TOTAL, T0); a.addiu(T1, T1, 1); a.sw(T1, AI_F_TOTAL, T0)
    for n, reg in enumerate((T0, T1, T2, T3)): a.lq(reg, 16 * n, SP)
    a.addiu(SP, SP, 0x40)
    for w in struct.unpack('<2I', RNG_ENTRY_BYTES): a.emit(w)   # relocated 0x317810 / 0x317814
    a.j(RNG_ENTRY + 8); a.nop()
    # ---- markers: $at is saved below sp (the callee's prologue owns that space next anyway)
    entries = {}
    for fn in MARKERS:
        w1, w2 = marker_words[fn]
        if not (relocatable(w1) and relocatable(w2)): raise ValueError(f'Marker {fn:#x} entry cannot be relocated')
        a.label(f'mark_{fn:x}'); entries[fn] = a.here()
        a.sq(AT, -0x10, SP); a.lui(AT, hi); a.sw(A0, lo + AI_F_MARK_A0, AT); a.sw(31, lo + AI_F_MARK_RA, AT); a.lq(AT, -0x10, SP)
        a.emit(w1); a.emit(w2); a.j(fn + 8); a.nop()
    code = a.link()
    if AI_CODE + len(code) > AI_CODE_END: raise ValueError('AI hook code exceeds its arena')
    return code, dict(entry=AI_CODE + 4 * a.labels['ai_copy'], npc=AI_CODE + 4 * a.labels['npc'],
                      rng=AI_CODE + 4 * a.labels['rng'], markers=entries)


def ai_layout():
    return dict(base=AI_BASE, stride=AI_STRIDE, slots=MAX_OTHERS,
                slot_fields=dict(actor_000_b40=AI_ACTOR, owner_00_40=AI_OWNER_A, owner_1c0_300=AI_OWNER_B,
                                 owner_de0_f50=AI_OWNER_C, npc_words_w0_w1_tick_calls=AI_NPC),
                human_actor_000_100=AI_HUMAN_HEAD, game_info_00_a0=AI_GAME,
                globals_pathbank_coursepaths_game_humanowner=AI_GLOBALS,
                rng_draws_total_marka0_markra_npcunmatched=AI_RNG, rng_log=AI_RNG_LOG, rng_log_entries=AI_LOG_ENTRIES,
                rng_log_entry='resolved_ra, leaf_ra, marker_a0, marker_ra', markers=[hex(m) for m in MARKERS],
                npc_exit=hex(NPC_EXIT), rng_entry=hex(RNG_ENTRY),
                timing=('record N (tick N): AI windows/human head/game info/RNG words are sampled at the human provider exit '
                        '(pass 121068, human first) after tick N passes 12BB20/113C20/10F560/120F20 ran for every rider and '
                        'before any computer rider ran 121068; npc words are the ones the NPC provider returned in tick N-1 '
                        '(their tick field says which tick; calls = returns since record N-1); the RNG log/draws cover every '
                        'draw after record N-1 was written up to record N.'))


def script_bytes(segments):
    entries = []; end = 0
    for s in segments:
        frames = int(s['frames'])
        if frames <= 0: raise ValueError('Segment frame count must be positive')
        end += frames
        values = decode_pad(s.get('buttons', []), s.get('lx', 0), s.get('ly', 0), s.get('rx', 0), s.get('ry', 0))
        entries.append((end, values))
    # The final entry never ends, so the hook's scan always terminates.
    entries.append((0xFFFFFFFF, entries[-1][1] if entries else [0.0] * 24))
    out = struct.pack('<4I', len(entries), 0, 0, 0)
    for stop, values in entries:
        out += struct.pack('<4I', stop, 0, 0, 0) + struct.pack('<24f', *values)
    if SCRIPT + len(out) > SCRIPT_END: raise ValueError('Script exceeds arena')
    return out, end


HUMAN_VTABLES, COMPUTER_VTABLES, DEFAULT3_VTABLE = (0x4583A8, 0x458360), (0x458660, 0x458618), 0x45CA38
MAX_OTHERS = 5          # record layout: five computer-rider position/velocity slots (3008..3168)


def _word_hits(memory, value, start=0x100000):
    """Word-aligned EE addresses holding `value` (fast bytes.find scan)."""
    needle = struct.pack('<I', value); hits = []; at = memory.find(needle, start)
    while at >= 0:
        if at % 4 == 0: hits.append(at)
        at = memory.find(needle, at + 1)
    return hits


def location_names():
    """Location id -> name from the ELF location table 0x43E250 (50 x 24 bytes)."""
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
    ph = struct.unpack_from('<I', elf, 28)[0]; stride, count = struct.unpack_from('<HH', elf, 42)
    for i in range(count):
        typ, offset, base, _, length, _, _, _ = struct.unpack_from('<8I', elf, ph + i * stride)
        if typ == 1 and base <= 0x43E250 and 0x43E250 + 50 * 24 <= base + length:
            rows = [elf[offset + 0x43E250 - base + 24 * k:][:24] for k in range(50)]
            return {struct.unpack_from('<I', r)[0]: r[4:20].split(b'\0')[0].decode() for r in rows}
    raise ValueError('Location table not found in ELF')


def discover(memory):
    """Find the event's objects by their interfaces instead of fixed Snow Jam addresses.

    * resident locations: streaming table 0x442168 (50 x {id, track, state, kind}), state 2;
      the course is the kind-0 entry without an underscore (ARA1, BRA2, ...).
    * riders: the game roster *(*(*(gp-0x848)+0x84)+0x0C) +0x28.. (count +0x78), classified by the
      rider vtables at +0x6C0/+0x6E8 (human 0x4583A8/0x458360, computer 0x458660/0x458618); every
      rider object found by a vtable scan must be in the roster.
    * cameras: exactly two DEFAULT_3 objects (vtable 0x45CA38 at +0x10).  The live updating one is
      referenced by the camera manager; the other is embedded at +0xC0 of the outer compositor camera,
      which the manager references.
    """
    u = lambda at: struct.unpack_from('<I', memory, at & 0x1FFFFFF)[0]
    names = location_names(); resident = []
    for i in range(50):
        ident, track, state, kind = struct.unpack_from('<4i', memory, 0x442168 + 16 * i)
        if state == 2: resident.append(dict(id=ident, name=names.get(ident, f'#{ident}'), track=track, kind=kind))
    courses = [r['name'] for r in resident if r['kind'] == 0 and '_' not in r['name']]
    if len(courses) != 1: raise ValueError(f'Cannot identify the loaded course from {resident}')
    game = u(u(u(GP - 0x848) + 0x84) + 0x0C)
    count = u(game + 0x78)
    if not 1 <= count <= 6: raise ValueError('Invalid rider roster count')
    roster = [u(game + 0x28 + 4 * n) for n in range(count)]
    kinds = [(u(r + 0x6C0), u(r + 0x6E8)) for r in roster]
    humans = [n for n, v in enumerate(kinds) if v == HUMAN_VTABLES]
    if len(humans) != 1: raise ValueError('Expected exactly one human rider in the roster')
    others = [r for r, v in zip(roster, kinds) if v == COMPUTER_VTABLES]
    if len(others) + 1 != count: raise ValueError('Unknown rider interface in the roster')
    if len(others) > MAX_OTHERS: raise ValueError('More computer riders than record slots')
    scanned = {a - 0x6C0 for a in _word_hits(memory, HUMAN_VTABLES[0]) + _word_hits(memory, COMPUTER_VTABLES[0])
               if (u(a), u(a + 0x28)) in (HUMAN_VTABLES, COMPUTER_VTABLES)}
    if scanned != set(roster): raise ValueError('Rider objects outside the roster')
    cams = [a - 0x10 for a in _word_hits(memory, DEFAULT3_VTABLE)]
    if len(cams) > 2:   # a dead camera left in freed heap (the career Intimidator final after WS13, docs/career-events.md): no word
        # references it or the outer camera it would be the compositor copy of
        cams = [c for c in cams if any(a < 0x1FF0000 for a in _word_hits(memory, c) + _word_hits(memory, c - 0xC0))]
    if len(cams) != 2: raise ValueError(f'Expected two DEFAULT_3 cameras, found {len(cams)}')
    # a stale word on the EE stack (top of RAM, e.g. 0x1FFF990 in the Ruthless ready states, docs/peak2.md) is no reference
    referenced = [c for c in cams if any(a < 0x1FF0000 for a in _word_hits(memory, c))]
    if len(referenced) == 2:
        # a stale heap word can reference the compositor copy too (the lodge exit's world load, docs/career-events.md
        # "Buy Attributes"): the copy is the one at +0xC0 of a referenced outer camera
        referenced = [c for c in cams if _word_hits(memory, next(o for o in cams if o != c) - 0xC0) and not _word_hits(memory, c - 0xC0)]
    if len(referenced) != 1: raise ValueError('Cannot tell the live DEFAULT_3 camera from the compositor copy')
    camera = referenced[0]; outer = next(c for c in cams if c != camera) - 0xC0
    if not _word_hits(memory, outer): raise ValueError('Outer compositor camera is not referenced')
    return dict(location=courses[0], resident=resident, rider=roster[humans[0]], human_index=humans[0],
                roster=roster, others=sorted(others), camera=camera, outer=outer)


def verify_baseline(memory, found=None):
    found = found or discover(memory)
    rider, camera, outer, others = found['rider'], found['camera'], found['outer'], found['others']
    # derived countdowns with another roster (characters/*, docs/characters.md lineups) have other heap addresses: SSX3_CAPTURE_DERIVED=1
    if found['location'] == 'ARA1' and not os.environ.get('SSX3_CAPTURE_DERIVED') and (rider, camera, outer, tuple(others)) != (RIDER, CAMERA, OUTER, OTHERS):
        raise ValueError('Snow Jam objects differ from the audited ARA1 addresses')
    u = lambda at: struct.unpack_from('<I', memory, at)[0]
    if (u(rider + 0x6C0), u(rider + 0x6E8)) != HUMAN_VTABLES: raise ValueError('Baseline human rider moved')
    owner = u(rider + 0x77C)
    if u(u(owner + 0xDE8) + 12) != 0x127998: raise ValueError('Unexpected input provider')
    pad = u(u(owner + 0xDF0))
    if u(pad) != 24: raise ValueError('Unexpected pad layout')
    if u(camera + 0x10) != DEFAULT3_VTABLE: raise ValueError('Live DEFAULT_3 camera moved')
    if u(outer + 0xC0 + 0x10) != DEFAULT3_VTABLE: raise ValueError('Outer camera compositor moved')
    if any(memory[CODE:0x100000]): raise ValueError('Probe arena is not free in baseline')
    for other in others:
        if (u(other + 0x6C0), u(other + 0x6E8)) != COMPUTER_VTABLES: raise ValueError('Baseline computer rider moved')
    if memory[PAD_HOOK:PAD_HOOK + 8] != struct.pack('<2I', 0x27BDFFC0, 0x7FB00030): raise ValueError('Pad hook bytes differ')
    if memory[LOG_HOOK:LOG_HOOK + 8] != struct.pack('<2I', 0x8E240DF8, 0x50800004): raise ValueError('Provider hook bytes differ')
    if memory[COLLIDE_HOOK:COLLIDE_HOOK + 8] != COLLIDE_BYTES: raise ValueError('Obstacle hook bytes differ')
    return pad


def ai_patches(memory, rider, others, back):
    """--ai-state hook patches (the AI arena 0xF8000..0x100000 is covered by verify_baseline's zero check)."""
    u = lambda at: struct.unpack_from('<I', memory, at)[0]
    for other in others:
        if u(u(other + 0x77C) + 0xDE8) != NPC_PROVIDER_INTERFACE: raise ValueError('Computer rider without the NPC input provider')
    if any(memory[RING:0x100000]): raise ValueError('AI ring/code arena is not free in baseline')
    for address, expected in ((NPC_EXIT, NPC_EXIT_BYTES), (RNG_ENTRY, RNG_ENTRY_BYTES)):
        if memory[address:address + 8] != expected: raise ValueError(f'Hook bytes differ at {address:#x}')
    words = {fn: struct.unpack_from('<2I', memory, fn) for fn in MARKERS}
    code, entries = assemble_ai(back, words)
    data = struct.pack('<16I', 0, 0, 0, 0, 0, 0, 0, rider, *others, *[0] * (MAX_OTHERS - len(others)), 0, 0, 0)
    j = lambda target: struct.pack('<2I', (2 << 26) | (target >> 2), 0)
    patches = [dict(address=hex(AI_CODE), expected='00' * len(code), replacement=code.hex()),
               dict(address=hex(AI_DATA), expected='00' * len(data), replacement=data.hex()),
               dict(address=hex(NPC_EXIT), expected=NPC_EXIT_BYTES.hex(), replacement=j(entries['npc']).hex()),
               dict(address=hex(RNG_ENTRY), expected=RNG_ENTRY_BYTES.hex(), replacement=j(entries['rng']).hex())]
    for fn, entry in entries['markers'].items():
        patches.append(dict(address=hex(fn), expected=struct.pack('<2I', *words[fn]).hex(), replacement=j(entry).hex()))
    return patches


def build(baseline, script_path, output, isolate=False, camera_variant=None, watches=(), ai_state=False, pokes=(), audio_log=False):
    spec = json.loads(Path(script_path).read_text())
    with zipfile.ZipFile(baseline) as archive: memory = archive.read('eeMemory.bin')
    found = discover(memory)
    pad = verify_baseline(memory, found)
    rider, camera, outer, others = found['rider'], found['camera'], found['outer'], found['others']
    record, capacity, watch_offset = (AI_RECORD, AI_CAPACITY, AI_WATCH) if ai_state else (RECORD, CAPACITY, WATCH)
    if any(length % 4 or length <= 0 for _, length in watches) or watch_offset + sum(l for _, l in watches) > record:
        raise ValueError('Watch windows must be word multiples that fit the record')
    ai = dict(entry=AI_CODE, watch_offset=AI_WATCH) if ai_state else None
    code, pad_entry, log_entry, collide_entry = assemble(watches, len(others), ai)
    if len(code) > 0x1000: raise ValueError('Probe code exceeds arena')
    script, frames = script_bytes(spec['segments'])
    data = struct.pack('<20I', 1, pad, 0, 0, 0, 0, 0, rider, camera, *others, *[0] * (MAX_OTHERS - len(others)), 0, 0, outer, 0, 0, 0)
    j = lambda target: struct.pack('<2I', (2 << 26) | (target >> 2), 0)
    patches = [
        dict(address=hex(CODE), expected='00' * len(code), replacement=code.hex()),
        dict(address=hex(DATA), expected='00' * len(data), replacement=data.hex()),
        dict(address=hex(SCRIPT), expected='00' * len(script), replacement=script.hex()),
        dict(address=hex(PAD_HOOK), expected=struct.pack('<2I', 0x27BDFFC0, 0x7FB00030).hex(), replacement=j(pad_entry).hex()),
        dict(address=hex(LOG_HOOK), expected=struct.pack('<2I', 0x8E240DF8, 0x50800004).hex(), replacement=j(log_entry).hex()),
        dict(address=hex(COLLIDE_HOOK), expected=COLLIDE_BYTES.hex(), replacement=j(collide_entry).hex()),
    ]
    if isolate:
        # The browser has no opponents yet: disable only the human<->computer rider-pair records
        # (0x107888 skips a disabled owner record). Computer riders still race and collide with each other.
        # Pair records: rider+36*j is the enabled word of that rider's record for roster index j; the two
        # records of a pair carry the same float at +8 (checked, so a different layout fails closed).
        one, zero = struct.pack('<I', 1).hex(), struct.pack('<I', 0).hex()
        roster, h = found['roster'], found['human_index']
        u = lambda at: struct.unpack_from('<I', memory, at)[0]
        pairs = [(k, other) for k, other in enumerate(roster) if k != h]
        for k, other in pairs:
            if u(rider + 36 * k + 8) != u(other + 36 * h + 8): raise ValueError(f'Pair record layout differs for roster {k}')
        for k, _ in pairs: patches.append(dict(address=hex(rider + 36 * k), expected=one, replacement=zero))
        for other in others: patches.append(dict(address=hex(other + 36 * h), expected=one, replacement=zero))
    if camera_variant:
        # Run the live chase object through another variant driver (0x45CAB0 Near / 0x45C9C0 Far) with its state intact.
        vt = {0x3C: 0x45CAB0, 0x3E: 0x45C9C0}[camera_variant]
        patches.append(dict(address=hex(camera + 0x10), expected=struct.pack('<I', 0x45CA38).hex(), replacement=struct.pack('<I', vt).hex()))
    if ai_state: patches += ai_patches(memory, rider, others, ai['back'])
    if audio_log:   # sound/speech dispatch call log (tools/ps2_audio_log.py), streamed by run() into RUN.audio.json
        import ps2_audio_log; patches += ps2_audio_log.patches(memory)
    for address, value in pokes:
        # Scenario setup (e.g. rider+0x2F8 boost meter near full for Uber captures): replaces one EE word.
        patches.append(dict(address=hex(address), expected=memory[address:address + 4].hex(), replacement=struct.pack('<I', value).hex()))
    output = Path(output)
    patch_state(Path(baseline), output, patches)
    manifest = dict(baseline=str(Path(baseline).resolve()), baseline_sha256=hashlib.sha256(Path(baseline).read_bytes()).hexdigest(),
                    script=str(Path(script_path).resolve()), script_frames=frames, segments=spec['segments'],
                    code=hex(CODE), data=hex(DATA), ring=hex(RING), record=record, capacity=capacity,
                    location=found['location'], resident_locations=[r['name'] for r in found['resident']],
                    rider=hex(rider), camera=hex(camera), outer_camera=hex(outer), human_index=found['human_index'],
                    roster=[hex(x) for x in found['roster']], computer_riders=len(others), human_pad=hex(pad), isolated_from_computer_riders=isolate, camera_variant=camera_variant,
                    layout={'seq': 0, 'tick': 4, 'word0': 8, 'word1': 12, 'motion_mode': 16, 'control': 20,
                            'pad_calls': 24, 'script_index': 28, 'rider_100_b40': 32, 'camera_000_160': 2656,
                            'others_position_velocity': 3008, 'owner_00_40': 3168, 'owner_de0_e00': 3232, 'world_bones_32x32': 3264, 'outer_camera_000_480': 4288,
                            'local_positions_32x16': LOCAL_POSITIONS, 'local_rotations_32x16': LOCAL_ROTATIONS, 'animator_00_80': ANIMATOR,
                            'sequences_6x216_channel_address_d0': SEQUENCES, 'sequence_count': SEQUENCE_COUNT, 'owner_200_300': OWNER_CONTROL, 'shared_rng_6': RNG_STATE, 'obstacle_callback': HIT_RECORD, 'score_000_1d0': SCORE_RECORD, 'hud_slots_44x6': HUD_SLOTS_RECORD,
                            'watches': [dict(address=hex(w), length=l) for w, l in watches], 'watch_offset': watch_offset,
                            **({'ai_state': ai_layout()} if ai_state else {})}, others=[hex(x) for x in others])
    if ai_state:
        u = lambda at: struct.unpack_from('<I', memory, at)[0]
        manifest.update(ai_state=True, ai_code=hex(AI_CODE), ai_data=hex(AI_DATA), human_owner=hex(u(rider + 0x77C)),
                        others_owners=[hex(u(o + 0x77C)) for o in others], ai_path_bank=hex(u(0x4D33AC)))
    if pokes: manifest['pokes'] = [dict(address=hex(a), value=hex(v)) for a, v in pokes]
    if audio_log: manifest['audio_log'] = True
    output.with_suffix('.capture.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


class Pine:
    def __init__(self, slot, timeout=5):
        path = Path(os.environ.get('TMPDIR', '/tmp')) / f'pcsx2.sock.{slot}' if slot != 28011 else Path(os.environ.get('TMPDIR', '/tmp')) / 'pcsx2.sock'
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM); self.sock.settimeout(timeout); self.sock.connect(str(path))

    def close(self): self.sock.close()

    def _recv(self, n):
        data = bytearray()
        while len(data) < n:
            part = self.sock.recv(n - len(data))
            if not part: raise ConnectionError('PINE closed')
            data.extend(part)
        return bytes(data)

    def request(self, commands):
        self.sock.sendall(struct.pack('<I', len(commands) + 4) + commands)
        size, = struct.unpack('<I', self._recv(4)); data = self._recv(size - 4)
        if data[0] != 0: raise RuntimeError('PINE request rejected')
        return data[1:]

    def status(self):
        v, = struct.unpack('<I', self.request(b'\x0f')); return {0: 'running', 1: 'paused', 2: 'shutdown'}.get(v, v)

    def read(self, address, size):
        out = bytearray()
        while size:
            count = min(size, 64 * 1024); wide = count // 8; tail = count % 8
            cmd = b''.join(b'\x03' + struct.pack('<I', address + i * 8) for i in range(wide))
            cmd += b''.join(b'\x00' + struct.pack('<I', address + wide * 8 + i) for i in range(tail))
            data = self.request(cmd)
            out.extend(data); address += count; size -= count
        return bytes(out)


def prepare_datapath(DATAPATH=DATAPATH, PINE_SLOT=PINE_SLOT):
    ini_source = DATAPATH_TEMPLATE / 'inis/PCSX2.ini'
    (DATAPATH / 'inis').mkdir(parents=True, exist_ok=True)
    for sub in ('snapshots', 'savestates', 'memorycards', 'logs', 'cheats', 'patches', 'cache', 'textures', 'userresources'):
        (DATAPATH / sub).mkdir(exist_ok=True)
    text = ini_source.read_text()
    text = text.replace(str(DATAPATH_TEMPLATE), str(DATAPATH))
    replacements = {'StartPaused': 'false', 'PINESlot': str(PINE_SLOT), 'EnablePINE': 'true', 'PauseOnFocusLoss': 'false',
                    'ConfirmShutdown': 'false'}
    # Silent by default: captures run while the user works on this Mac (turbo runs used FastForwardVolume = 100).
    # PS2_CAPTURE_AUDIO=1 keeps the template's volumes for tools that record the emulator's audio output.
    if not os.environ.get('PS2_CAPTURE_AUDIO'):
        replacements.update({'StandardVolume': '0', 'FastForwardVolume': '0'})
    if os.environ.get('PS2_CAPTURE_SCALAR'):   # opt-in slower emulation (e.g. 0.5) so PINE keeps up on a loaded host; timing only
        replacements['NominalScalar'] = os.environ['PS2_CAPTURE_SCALAR']
    if os.environ.get('PS2_CAPTURE_EECYCLE'):  # opt-in EE clock (EECycleRate -3..3, -3 = 50 %): a game frame that overruns its vsync (frame-pacing studies)
        replacements['EECycleRate'] = os.environ['PS2_CAPTURE_EECYCLE']
    lines = []
    for line in text.splitlines():
        key = line.split('=')[0].strip()
        if key in replacements and '=' in line: line = f'{key} = {replacements[key]}'
        lines.append(line)
    # PCSX2 reads <datapath>/inis; ARMSX2 reads <datapath>/ARMSX2/inis.
    for inis in (DATAPATH / 'inis', DATAPATH / 'ARMSX2/inis'):
        inis.mkdir(parents=True, exist_ok=True)
        (inis / 'PCSX2.ini').write_text('\n'.join(lines) + '\n')


def run(state, output, frames, speed='normal', timeout=600, snaps=(), keep_states=False):
    manifest = json.loads(Path(state).with_suffix('.capture.json').read_text())
    record, capacity = manifest.get('record', RECORD), manifest.get('capacity', CAPACITY)   # --ai-state: 32 KiB x 11
    # Unique PINE slot and data path per run so concurrent captures never share a socket.
    import random
    slot = random.randint(28100, 28999)
    datapath = DATAPATH.parent / f'pcsx2-{slot}'
    prepare_datapath(datapath, slot)
    args = [str(PCSX2), '-datapath', str(datapath), '-batch', '-nogui', '-statefile', str(Path(state).resolve())]
    if speed == 'unlimited': args.append('-unlimited')
    elif speed == 'turbo': args.append('-turbo')
    args += ['--', str(ISO)]
    log = open(Path(output).with_suffix('.pcsx2.log'), 'w')
    proc = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT)
    records = {}; started = time.time(); pine = None
    audio = [] if manifest.get('audio_log') else None
    pending = sorted(set(snaps)); taken = []   # PINE MsgSaveState (0x09 slot): the state zip carries the PS2 Screenshot.png
    try:
        while time.time() - started < timeout:
            if proc.poll() is not None: raise RuntimeError(f'PCSX2 exited early ({proc.returncode})')
            if pine is None:
                try: pine = Pine(slot)
                except OSError: time.sleep(0.5); continue
            try: writes, = struct.unpack('<I', pine.read(DATA + F_WRITES, 4))
            except (OSError, RuntimeError, ConnectionError): time.sleep(0.2); continue
            have = max(records) if records else 0
            if writes - have > capacity: raise RuntimeError(f'Ring overrun: {writes} written, {have} read')
            for seq in range(have + 1, writes + 1):
                slot = (seq - 1) % capacity
                data = pine.read(RING + slot * record, record)
                got, = struct.unpack_from('<I', data, 0)
                if got != seq: break  # not yet complete; reread next poll
                records[seq] = data
            if audio is not None and records:
                import ps2_audio_log; audio += ps2_audio_log.read_new(pine, len(audio))
            if pending and records and max(records) >= pending[0]:
                pine.request(b'\x09' + bytes([len(taken) + 1])); taken.append(pending.pop(0))
            if records and max(records) >= frames and not pending:
                deadline = time.time() + 30   # savestates are written asynchronously
                while taken and time.time() < deadline and len(list(datapath.rglob('*.p2s'))) < len(taken): time.sleep(0.5)
                time.sleep(1); break
            time.sleep(0.02)
        else:
            raise RuntimeError('Capture timed out')
    finally:
        if pine: pine.close()
        proc.terminate()
        try: proc.wait(10)
        except subprocess.TimeoutExpired: proc.kill()
        log.close()
        snapshots = []
        for state_file in sorted(datapath.rglob('*.p2s'), key=lambda f: f.name):
            try:
                with zipfile.ZipFile(state_file) as z:
                    memory = z.read('eeMemory.bin'); u = lambda a: struct.unpack_from('<I', memory, a & 0x1FFFFFF)[0]
                    tick = u(u(u(u(GP - 0x848) + 0x84) + 0x0C) + 8)
                    png = Path(output).with_name(f'{Path(output).stem}.tick{tick}.png'); png.write_bytes(z.read('Screenshot.png'))
                    keep = os.environ.get('PS2_CAPTURE_KEEP_STATES')  # optional: retain snapshot savestates (EE memory) for inspection
                    if keep: Path(keep).mkdir(parents=True, exist_ok=True); shutil.copy2(state_file, Path(keep) / f'{Path(output).stem}.tick{tick}.p2s')
                    snapshots.append(dict(tick=tick, png=str(png)))
                if keep_states:   # full savestate for offline memory analysis (entity/runtime state)
                    kept = Path(output).with_name(f'{Path(output).stem}.tick{tick}.p2s'); shutil.copyfile(state_file, kept); snapshots[-1]['state'] = str(kept)
            except (zipfile.BadZipFile, KeyError) as error: snapshots.append(dict(error=str(error), file=state_file.name))
        shutil.rmtree(datapath, ignore_errors=True)
    ordered = [records[k] for k in sorted(records)]
    Path(output).write_bytes(b''.join(ordered))
    summary = dict(state=str(state), manifest=manifest, records=len(ordered), bytes_per_record=record,
                   first_tick=struct.unpack_from('<I', ordered[0], 4)[0] if ordered else None,
                   last_tick=struct.unpack_from('<I', ordered[-1], 4)[0] if ordered else None,
                   sha256=hashlib.sha256(b''.join(ordered)).hexdigest(), snapshots=snapshots)
    if audio is not None:
        Path(output).with_suffix('.audio.json').write_text(json.dumps(dict(hooks='tools/ps2_audio_log.py', entries=audio)) + '\n'); summary['audio_entries'] = len(audio)
    Path(output).with_suffix('.json').write_text(json.dumps(summary, indent=2) + '\n')
    return summary


def main():
    p = argparse.ArgumentParser(description=__doc__); sub = p.add_subparsers(dest='cmd', required=True)
    b = sub.add_parser('build'); b.add_argument('baseline'); b.add_argument('script'); b.add_argument('output'); b.add_argument('--isolate', action='store_true'); b.add_argument('--camera-variant', type=lambda x: int(x, 0))
    b.add_argument('--watch', action='append', default=[], help='ADDR:LEN raw EE window copied into every record from layout.watch_offset')
    b.add_argument('--poke', action='append', default=[], help='ADDR:VALUE 32-bit word written into the derived savestate (a VALUE with a decimal point is a float, e.g. 0x1470498:0.97)')
    b.add_argument('--audio-log', action='store_true', help='log sound plays / speech requests / Uber and Tricky dispatchers (tools/ps2_audio_log.py) into RUN.audio.json')
    b.add_argument('--ai-state', action='store_true', help='32 KiB records: five computer riders (actor/owner windows, NPC provider commands), human +0..0x100, game info, per-draw RNG attribution (manifest layout.ai_state)')
    r = sub.add_parser('run'); r.add_argument('state'); r.add_argument('output'); r.add_argument('--frames', type=int, required=True)
    r.add_argument('--speed', default='normal', choices=['normal', 'turbo', 'unlimited']); r.add_argument('--timeout', type=int, default=600)
    r.add_argument('--snap', default='', help='comma-separated record counts at which to save a PS2 screenshot (savestate Screenshot.png)')
    r.add_argument('--keep-states', action='store_true', help='also keep each --snap savestate as RUN.tickN.p2s')
    args = p.parse_args()
    def poke(text):
        address, value = text.split(':')
        word = struct.unpack('<I', struct.pack('<f', float(value)))[0] if '.' in value and not value.lower().startswith('0x') else int(value, 0) & 0xFFFFFFFF
        return int(address, 0), word
    if args.cmd == 'build': print(json.dumps(build(args.baseline, args.script, args.output, args.isolate, args.camera_variant, [tuple(int(v, 0) for v in w.split(':')) for w in args.watch], args.ai_state, [poke(x) for x in args.poke], args.audio_log), indent=2))
    else: print(json.dumps(run(args.state, args.output, args.frames, args.speed, args.timeout, [int(x) for x in args.snap.split(',') if x], args.keep_states), indent=2))


if __name__ == '__main__':
    main()
