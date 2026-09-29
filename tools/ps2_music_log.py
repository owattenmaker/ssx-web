#!/usr/bin/env python3
"""ps2_music_log.py: music director / DJ / speech call log for ARMSX2 (docs/audio-logic.md 9.11; development reference only).

Used by tools/ps2_music_drive.py. Arena 0xF2000..0x100000 (clear of tools/ps2_menu_capture.py 0x96000..0xA0000).

Every hooked entry jumps to a 7-word stub: t8 = tag | flags << 16, t9 = ra, jal COMMON, ra = t9, the two original
instructions, j entry+8.  COMMON writes one 128-byte record into the ring:
  0 tag  4 tick  8..32 a0 a1 a2 a3 t0 t1 t2  36 ra  40 seq  44 x0  48 x1  52 f12 bits  56 current song id (song+0x58)
  60 music index (audio+0x508)  64 nav samples  68 world state  72 course word 0x535C08  76 audio+0x530 (active)
  80..111 string (flags 1: from a1, 2: from a2)  112.. unused
flags 4: x0 = *(a1+0x1C), x1 = *(a1+0x20) (timer callback data).
Only temporaries that are not arguments at a function entry are used (v0 v1 at t4-t9).
"""
import struct, sys
sys.path.insert(0, str(__import__('pathlib').Path(__file__).resolve().parent))
from ps2_capture import Asm

CODE, COUNT, RING, N, REC = 0xF2000, 0xF3FF0, 0xF4000, 384, 128
ARENA_END = RING + N * REC
V0, V1, T4, T5, T6, T7, T8, T9, GP, RA, AT, ZERO = 2, 3, 12, 13, 14, 15, 24, 25, 28, 31, 1, 0
ARGS = [4, 5, 6, 7, 8, 9, 10]
AUDIO_PTR = 0x4A3500  # *(gp+0x410)
S1, S2, S4 = 1, 2, 4
HOOKS = {
    0x2B1458: (2, 'speech', 0),
    0x29EEE0: (3, 'speech_bus', S2),
    0x2B1758: (4, 'speech_flush', 0),
    0x28E8C0: (10, 'music_code', 0),
    0x28CF98: (11, 'play_music', 0),
    0x28D488: (12, 'pick_next', 0),
    0x2B3838: (13, 'play_song', S1),
    0x2B35A0: (14, 'play_song_named', S1),
    0x28F478: (15, 'listener_play', 0),
    0x2B3BC0: (16, 'send_event', 0),
    0x2B3B88: (17, 'send_forced', 0),
    0x2B3D48: (18, 'fade_out', 0),
    0x2B3AC0: (19, 'stop', 0),
    0x2B3A70: (20, 'pause', 0),
    0x2B3A98: (21, 'resume', 0),
    0x28E088: (22, 'request_cb', S4),
    0x28E548: (23, 'dj_timer', S4),
    0x28DF18: (24, 'change_song', 0),
    0x28F140: (26, 'fe_state', 0),
    0x28BF78: (27, 'radio_mode', 0),
    0x28F768: (28, 'loading_on', 0),
    0x28FA98: (29, 'loading_off', 0),
    0x28F5B8: (30, 'map_open', 0),
    0x28F678: (31, 'map_close', 0),
    0x289B70: (32, 'game_pause', 0),
    0x289BB8: (33, 'game_resume', 0),
    0x2867E8: (34, 'world_load', 0),
    0x286A80: (35, 'world_leave', 0),
    0x286200: (36, 'fe_to_game', 0),
    0x28CD48: (37, 'pktrans', 0),
    0x28E888: (38, 'cinematic_end', 0),
    0x29D370: (39, 'ambience', 0),
}
TAGS = {t: n for t, n, _ in HOOKS.values()}


def guard_ptr(a, reg, skip):
    """branch to skip unless 0x100000 <= reg < 0x2000000"""
    a.lui(AT, 0x0200); a.sltu(AT, reg, AT); a.beq(AT, ZERO, skip); a.nop()
    a.lui(AT, 0x0010); a.sltu(AT, reg, AT); a.bne(AT, ZERO, skip); a.nop()


def assemble(memory):
    a = Asm(CODE)
    lbu = lambda rt, off, rs: a.i(36, rt, rs, off)
    sb = lambda rt, off, rs: a.i(40, rt, rs, off)
    swc1 = lambda ft, off, rs: a.i(57, ft, rs, off)
    srl = lambda rd, rt, sa: a.r(rd, 0, rt, 2, sa)
    # ---- COMMON (t8 = tag | flags << 16) ----
    a.label('common')
    a.li(V0, COUNT); a.lw(V1, 0, V0)
    a.addiu(T6, ZERO, N); a.r(0, V1, T6, 0x1B)          # divu v1, t6
    a.r(T6, 0, 0, 0x10)                                  # mfhi t6
    a.sll(T6, T6, 7); a.li(T7, RING); a.addu(T6, T6, T7)
    a.andi(T7, T8, 0xFFFF); a.sw(T7, 0, T6); a.sw(V1, 40, T6)
    for k, reg in enumerate(ARGS): a.sw(reg, 8 + 4 * k, T6)
    a.sw(T9, 36, T6)
    for off in range(44, 128, 4): a.sw(ZERO, off, T6)
    swc1(12, 52, T6)
    a.addiu(T7, ZERO, -1); a.sw(T7, 4, T6); a.sw(T7, 68, T6)
    a.lw(T7, -0x848, GP); guard_ptr(a, T7, 'nt')
    a.lw(T7, 0x84, T7); guard_ptr(a, T7, 'nt')
    a.lw(T5, 0x214, T7); a.sw(T5, 68, T6)
    a.lw(T7, 0x0C, T7); guard_ptr(a, T7, 'nt')
    a.lw(T5, 8, T7); a.sw(T5, 4, T6)
    a.label('nt')
    a.li(T7, 0x97000); a.lw(T5, 4, T7); a.sw(T5, 64, T6)
    a.li(T7, 0x535C08); a.lw(T5, 0, T7); a.sw(T5, 72, T6)
    a.li(T7, AUDIO_PTR); a.lw(T7, 0, T7); guard_ptr(a, T7, 'na')
    a.lw(T5, 0x508, T7); a.sw(T5, 60, T6)
    a.lw(T5, 0x530, T7); a.sw(T5, 76, T6)
    a.lw(T7, 0x520, T7); guard_ptr(a, T7, 'na')
    a.lw(T5, 0x58, T7); a.sw(T5, 56, T6)
    a.label('na')
    # flags
    srl(T4, T8, 16)
    a.andi(T5, T4, S4); a.beq(T5, ZERO, 'nf4'); a.nop()
    guard_ptr(a, 5, 'nf4')
    a.lw(T5, 0x1C, 5); a.sw(T5, 44, T6); a.lw(T5, 0x20, 5); a.sw(T5, 48, T6)
    a.label('nf4')
    for flag, reg, lab in ((S1, 5, 's1'), (S2, 6, 's2')):
        a.andi(T5, T4, flag); a.beq(T5, ZERO, 'n' + lab); a.nop()
        guard_ptr(a, reg, 'n' + lab)
        for k in range(32):
            lbu(T5, k, reg); sb(T5, 80 + k, T6)
        a.label('n' + lab)
    a.li(V0, COUNT); a.lw(V1, 0, V0); a.addiu(V1, V1, 1); a.sw(V1, 0, V0)
    a.r(0, RA, 0, 0x08); a.nop()                         # jr ra
    entries = {}
    for addr, (tag, _, flags) in HOOKS.items():
        entries[addr] = a.here()
        a.li(T8, tag | (flags << 16))
        a.r(T9, RA, ZERO, 0x21)                          # addu t9, ra, zero
        a.emit((3 << 26) | ((CODE >> 2) & 0x3FFFFFF)); a.nop()   # jal common
        a.r(RA, T9, ZERO, 0x21)                          # addu ra, t9, zero
        a.emit(struct.unpack_from('<I', memory, addr)[0]); a.emit(struct.unpack_from('<I', memory, addr + 4)[0])
        a.j(addr + 8); a.nop()
    return a.link(), entries


def patches(memory):
    if any(memory[CODE:ARENA_END]): raise ValueError('arena not free')
    for addr in HOOKS:
        for w in struct.unpack_from('<2I', memory, addr):
            op = w >> 26
            if op in (1, 2, 3, 4, 5, 6, 7, 20, 21, 22, 23) or (op == 0 and (w & 0x3F) in (8, 9)): raise ValueError(f'hook {addr:#x}: control transfer')
            # the stub uses v0 v1 t4-t9 at: an original instruction reading them would see garbage
            rs, rt = (w >> 21) & 31, (w >> 16) & 31
            reads = [rs] if op not in (15,) else []
            if op == 0 or op in (40, 41, 43, 44, 45, 46, 47, 31, 63): reads.append(rt)
            if any(r in (2, 3, 12, 13, 14, 15, 24, 25, 1) for r in reads): raise ValueError(f'hook {addr:#x}: reads a scratch register')
    code, entries = assemble(memory)
    assert CODE + len(code) <= COUNT, hex(CODE + len(code))
    j = lambda t: struct.pack('<2I', (2 << 26) | (t >> 2), 0)
    out = [dict(address=hex(CODE), expected='00' * len(code), replacement=code.hex())]
    for addr, e in entries.items():
        out.append(dict(address=hex(addr), expected=memory[addr:addr + 8].hex(), replacement=j(e).hex()))
    return out


def clean_patches(memory, original):
    """restore the hooked entries from `original` (an unhooked memory image) and zero the log arena"""
    out = []
    for addr in list(HOOKS) + [a for a in (0x2A43B8,) if a not in HOOKS]:
        cur, orig = memory[addr:addr + 8], original[addr:addr + 8]
        if cur != orig: out.append(dict(address=hex(addr), expected=cur.hex(), replacement=orig.hex()))
    arena = memory[CODE:ARENA_END]; end = len(arena.rstrip(b'\0'))
    if end: out.append(dict(address=hex(CODE), expected=arena[:end].hex(), replacement='00' * end))
    return out


def decode(raw):
    tag, tick = struct.unpack_from('<Ii', raw, 0)
    regs = struct.unpack_from('<7i', raw, 8)
    ra, seq, x0, x1 = struct.unpack_from('<IIii', raw, 36)
    f12, = struct.unpack_from('<f', raw, 52)
    song, index, samples, ws, course, active = struct.unpack_from('<iiIiIi', raw, 56)
    s = raw[80:112].split(b'\0')[0]
    d = dict(seq=seq, tag=tag, name=TAGS.get(tag, tag), tick=tick, sample=samples, ws=ws, ra=hex(ra),
             a0=hex(regs[0] & 0xFFFFFFFF), a1=regs[1], a2=regs[2], a3=regs[3], t0=regs[4], t1=regs[5], t2=regs[6],
             f12=round(f12, 4), song=song, index=index, course=course & 0xFF, kind=(course >> 16) & 0xFF, active=active)
    if tag in (22, 23): d.update(x0=x0, x1=x1)
    if s and all(32 <= c < 127 for c in s): d['str'] = s.decode()
    return d


def read_new(pine, have):
    count, = struct.unpack('<I', pine.read(COUNT, 4))
    if count - have > N: raise RuntimeError(f'overrun: {count} written, {have} read')
    rows = []
    for s in range(have, count):
        r = decode(pine.read(RING + REC * (s % N), REC))
        if r['seq'] != s: break
        rows.append(r)
    return rows


# ---- polled director state (read over PINE between hook reads) ----
POLL = [('dj', 0x5738, 0x88), ('dir', 0x6230, 0x98), ('radio', 0x608C, 4), ('vol', 0x62C8, 16 * 11), ('duck', 0x6394, 44)]


def poll(pine):
    u = lambda a: struct.unpack('<I', pine.read(a, 4))[0]
    ok = lambda p: 0x100000 <= p < 0x2000000
    A = u(AUDIO_PTR)
    if not ok(A): return None
    out = {}
    for name, off, n in POLL: out[name] = pine.read(A + off, n)
    song = u(A + 0x520)
    out['song'] = struct.unpack('<i', pine.read(song + 0x58, 4))[0] if ok(song) else None
    out['index'] = struct.unpack('<i', pine.read(A + 0x508, 4))[0]
    # Pathfinder slot 0, track 0 voice: +0x36 last committed node, +0x30 part head, +1 intensity
    v = u(0x517610 + 0x58)
    out['node'] = struct.unpack('<hhB', pine.read(v + 0x36, 2) + pine.read(v + 0x30, 2) + pine.read(v + 1, 1)) if ok(v) else None
    out['course'] = pine.read(0x535C08, 12)
    return out


def summarize(p):
    """compact dict of the polled bytes (the fields named in docs/audio-logic.md)"""
    if p is None: return None
    dj = p['dj']; d = p['dir']
    w = lambda b, off: struct.unpack_from('<i', b, off)[0]
    f = lambda b, off: struct.unpack_from('<f', b, off)[0]
    pend = {n: w(dj, o - 0x5738) for n, o in (('bcChallenge', 0x5788), ('radioBigOutro', 0x5740), ('finishLine', 0x5744),
            ('hub1', 0x574C), ('hub2', 0x5750), ('hub3', 0x5754), ('eventIntro', 0x5758), ('radioBigIntro', 0x5768),
            ('firstSpoke', 0x576C), ('freeRideIntro', 0x5770), ('artist', 0x5774), ('textMessage', 0x577C),
            ('bcIntro', 0x5784)) if w(dj, o - 0x5738)}
    s = dict(song=p['song'], index=p['index'], node=p['node'], radio=w(p['radio'], 0),
             course=p['course'][0], kind=p['course'][8], path=p['course'][9], mode=p['course'][10],
             pend=pend, tmVar=w(dj, 0x5780 - 0x5738), hubFirst=w(dj, 0x5790 - 0x5738), bcIntroFlag=w(dj, 0x578C - 0x5738),
             hubUsed=hex(w(dj, 0x573C - 0x5738)),
             zone=w(d, 0x623C - 0x6230), zoneExit=w(d, 0x6240 - 0x6230), cin=w(d, 0x6254 - 0x6230), arrived=w(d, 0x6258 - 0x6230),
             trigA=w(d, 0x6264 - 0x6230), trigB=w(d, 0x6268 - 0x6230), rearm=w(d, 0x6270 - 0x6230),
             req=(w(d, 0x6274 - 0x6230), w(d, 0x6278 - 0x6230)), x627C=w(d, 0x627C - 0x6230), dest=w(d, 0x6284 - 0x6230),
             fe=w(d, 0x6290 - 0x6230), latch=w(d, 0x629C - 0x6230), prev=w(d, 0x62A4 - 0x6230),
             music=round(f(p['vol'], 16), 3), djv=round(f(p['vol'], 32), 3), musicDuck=round(f(p['duck'], 4), 3))
    return s
