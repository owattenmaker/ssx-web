#!/usr/bin/env python3
"""Regenerate shop.json (SSX 3 PS2 NTSC-U, SLUS_207.72) from the local disc copies.

Inputs (read-only):
  local/disc/SLUS_207.72                     ELF (tables, jump tables, code immediates)
  local/sam-ps2/original/BOLTPS2.DAT         DATA/CHAR/BOLTPS2.DAT (gear DB)   [falls back to the ISO]
  local/career/RWRDPS2.DAT                   DATA/BE/RWRDPS2.DAT (rewards DB)  [falls back to the ISO]
  DATA/LOCALE/{FEAMER,OVAMER,CMNAMER}.LOC     locale text by kT_ id hash (read from the ISO)
  ISO via tools/inspect_disc.Disc            DATA/CONFIG/MUSIC.INF, PLAYLIST.INF
  optional: local/ps2-capture/menus/ctm/state-lodge-peak1.p2s (PCSX2 state, fresh career Zoe) for validation

Usage: export_lodge_shop.py [out.json]   (default: web/public/assets/CAREER/shop.json, git-ignored)

Everything semantic (what a function does) was recovered by reading the disassembly; the script re-reads
all data tables and verifies the code immediates it relies on (assert_imm) so a mismatch fails loudly.
Fields named *_inferred or notes marked 'INFERRED' are interpretation, not directly read."""
import glob, json, re, struct, sys, zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LOCAL = ROOT / 'local'
from disc_paths import ps2_iso;ISO = ps2_iso()
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'web/public/assets/CAREER/shop.json'

# ---------------------------------------------------------------- ELF ------------------------------
ELF = (LOCAL / 'disc/SLUS_207.72').read_bytes()
_phoff = struct.unpack_from('<I', ELF, 28)[0]
_segs = []
for i in range(struct.unpack_from('<H', ELF, 44)[0]):
    t, off, va, pa, fs, ms, fl, al = struct.unpack_from('<8I', ELF, _phoff + i * 32)
    if t == 1:
        _segs.append((off, va, fs))


def at(v, n):
    for off, base, fs in _segs:
        if base <= v and v + n <= base + fs:
            return ELF[off + v - base: off + v - base + n]
    raise ValueError(hex(v))


def u32(v): return struct.unpack('<I', at(v, 4))[0]
def u16(v): return struct.unpack('<H', at(v, 2))[0]
def s16(v): return struct.unpack('<h', at(v, 2))[0]


def cstr(v, m=96):
    return at(v, m).split(b'\0')[0].decode('latin1')


def H(x): return '0x%X' % x


# instruction helpers --------------------------------------------------------------------------------
REG = ['zero', 'at', 'v0', 'v1', 'a0', 'a1', 'a2', 'a3', 't0', 't1', 't2', 't3', 't4', 't5', 't6', 't7',
       's0', 's1', 's2', 's3', 's4', 's5', 's6', 's7', 't8', 't9', 'k0', 'k1', 'gp', 'sp', 'fp', 'ra']


def dec(a):
    w = u32(a)
    op = w >> 26
    return dict(w=w, op=op, rs=(w >> 21) & 31, rt=(w >> 16) & 31, rd=(w >> 11) & 31, sa=(w >> 6) & 31,
                fn=w & 63, imm=struct.unpack('<h', struct.pack('<H', w & 0xFFFF))[0], uimm=w & 0xFFFF,
                tgt=(w & 0x3FFFFFF) << 2)


def assert_imm(addr, reg, value, what=''):
    """addiu/ori reg,zero,value at addr (the code constant this JSON relies on)."""
    d = dec(addr)
    ok = d['op'] in (0x09, 0x0D) and d['rs'] == 0 and REG[d['rt']] == reg and (d['imm'] if d['op'] == 9 else d['uimm']) == value
    if not ok:
        raise AssertionError('code check failed @%x (%s): %08x' % (addr, what, d['w']))
    return value


def assert_jal(addr, target):
    d = dec(addr)
    if d['op'] != 3 or d['tgt'] != target:
        raise AssertionError('expected jal %x @%x, got %08x' % (target, addr, d['w']))


class Mini:
    """Tiny EE interpreter for straight-line/branchy leaf code (no loads, byte/half stores into a dict)."""

    def __init__(self, regs=None):
        self.r = [0] * 32
        for k, v in (regs or {}).items():
            self.r[REG.index(k)] = v
        self.mem = {}

    def run(self, pc, stop, limit=4000):
        delay = None
        for _ in range(limit):
            if pc in stop and delay is None:
                return pc
            d = dec(pc)
            nxt = pc + 4
            taken = None
            likely = False
            op, rs, rt, rd, fn = d['op'], d['rs'], d['rt'], d['rd'], d['fn']
            R = self.r
            if op == 0:
                if fn == 0x2D or fn == 0x21:   # daddu / addu
                    self._w(rd, R[rs] + R[rt])
                elif fn == 0x00:               # sll
                    self._w(rd, (R[rt] << d['sa']) & 0xFFFFFFFF)
                elif fn == 0x0A:               # movz
                    if R[rt] == 0: self._w(rd, R[rs])
                elif fn == 0x0B:               # movn
                    if R[rt] != 0: self._w(rd, R[rs])
                elif fn == 0x08:               # jr
                    taken = ('ret',)
                elif fn == 0x2A:
                    self._w(rd, int(self._s(R[rs]) < self._s(R[rt])))
                else:
                    raise NotImplementedError('special fn %x @%x' % (fn, pc))
            elif op == 0x09: self._w(rt, R[rs] + d['imm'])
            elif op == 0x0A: self._w(rt, int(self._s(R[rs]) < d['imm']))
            elif op == 0x0B: self._w(rt, int((R[rs] & 0xFFFFFFFF) < (d['imm'] & 0xFFFFFFFF)))
            elif op == 0x0C: self._w(rt, R[rs] & d['uimm'])
            elif op == 0x0D: self._w(rt, R[rs] | d['uimm'])
            elif op == 0x0E: self._w(rt, R[rs] ^ d['uimm'])
            elif op == 0x0F: self._w(rt, (d['uimm'] << 16))
            elif op in (0x04, 0x05, 0x14, 0x15, 0x06, 0x07, 0x01):
                a, b = self._s(R[rs]), self._s(R[rt])
                if op in (0x04, 0x14): c = a == b
                elif op in (0x05, 0x15): c = a != b
                elif op == 0x06: c = a <= 0
                elif op == 0x07: c = a > 0
                else:
                    c = (a < 0) if rt in (0, 2) else (a >= 0)
                    likely = rt in (2, 3)
                likely = likely or op in (0x14, 0x15)
                tgt = pc + 4 + (d['imm'] << 2)
                taken = ('br', tgt) if c else ('skip' if likely else None)
            elif op == 0x28: self.mem[(R[rs] + d['imm']) & 0xFFFFFFFF] = R[rt] & 0xFF
            elif op == 0x29:
                a = (R[rs] + d['imm']) & 0xFFFFFFFF
                self.mem[a] = R[rt] & 0xFF; self.mem[a + 1] = (R[rt] >> 8) & 0xFF
            else:
                raise NotImplementedError('op %x @%x' % (op, pc))
            if delay is not None:
                pc = delay
                delay = None
                if pc == 'ret':
                    return 'ret'
                continue
            if taken is None:
                pc = nxt
            elif taken[0] == 'skip':
                pc = nxt + 4
            elif taken[0] == 'ret':
                delay = 'ret'; pc = nxt
            else:
                delay = taken[1]; pc = nxt
        raise RuntimeError('mini interpreter limit')

    @staticmethod
    def _s(v):
        v &= 0xFFFFFFFF
        return v - (1 << 32) if v & 0x80000000 else v

    def _w(self, r, v):
        if r: self.r[r] = v & 0xFFFFFFFF


# ---------------------------------------------------------------- locale ---------------------------
def kt_hash(t):
    x = 0
    for c in t.encode('latin1'):
        c = c - 256 if c > 127 else c
        x = ((x << 4) + c) & 0xFFFFFFFF
        g = x & 0xF0000000
        if g:
            x = (x ^ (g >> 23)) ^ g
    return x


LOC = {}   # hash -> text from the disc's own locale files (FEAMER/OVAMER/CMNAMER), filled after disc_file exists


def loc(kid):
    return LOC.get(kt_hash(kid)) if kid else None


# ---------------------------------------------------------------- disc files -----------------------
_disc = None


def disc_file(name, local=None):
    global _disc
    if local and Path(local).exists():
        return Path(local).read_bytes()
    if _disc is None:
        sys.path.insert(0, str(ROOT / 'tools'))
        from inspect_disc import Disc
        _disc = Disc(ISO)
    return _disc.file(name)


sys.path.insert(0, str(ROOT / 'tools/sam_ps2'))
import loc_file  # noqa: E402
for _name in ('OVAMER', 'FEAMER', 'CMNAMER'):
    for _k, _v in loc_file.entries(disc_file(f'DATA/LOCALE/{_name}.LOC')).items(): LOC.setdefault(_k, _v)

RIDERS = ['Moby', 'Kaori', 'Allegra', 'Mac', 'Zoe', 'Griff', 'Elise', 'Nate', 'Psymon', 'Viggo']
PROFILE_BASE, PROFILE_STRIDE, CHAR_STRIDE = 0x4A6CA8, 0x9B50, 0xF88

# ================================================================= RWRDPS2 (for names) ==============
RW = disc_file('DATA/BE/RWRDPS2.DAT', LOCAL / 'career/RWRDPS2.DAT')
RW_POOL = 0x15DC


def rws(o):
    if not o: return None
    a = RW_POOL + o - 1
    return RW[a:RW.index(b'\0', a)].decode('latin1')


_rw_sz = [12, 12, 8, 16, 16, 16, 16, 16, 16]
RWSEC = {}
_p = 0
for _ in range(9):
    t, c = struct.unpack_from('<II', RW, _p); _p += 8
    RWSEC[t] = [struct.unpack_from('<%dI' % (_rw_sz[t] // 4), RW, _p + k * _rw_sz[t]) for k in range(c)]
    _p += c * _rw_sz[t]
RW_CATNAME = {0: 'trophy', 1: 'medal', 2: 'peak_pass', 3: 'poster', 4: 'trading_card', 5: 'art', 6: 'video', 7: 'toy', 8: 'cheat_character'}
CHEAT = {}
for k, w in enumerate(RWSEC[8]):
    pr = struct.unpack('<h', struct.pack('<H', w[3] & 0xFFFF))[0]
    pk = struct.unpack('<b', bytes([(w[3] >> 16) & 0xFF]))[0]
    CHEAT[w[3] >> 24] = dict(character_id=w[3] >> 24, name=rws(w[0]), rwrd_index=k,
                             shop_price=pr * 10 if pr > 0 else None, shop_peak=pk if pk > 0 else None,
                             save_bit_at_0xF57=(w[3] >> 24) - 10)


def cheat(cid):
    c = CHEAT.get(cid)
    return dict(character_id=H(cid), name=c['name'] if c else None)


# ================================================================= 1. AWARDS =======================
JT_AWARD = 0x45AA40
AWARD_FN = 0x159CD0
jt = [u32(JT_AWARD + 4 * i) for i in range(32)]

# code-constant checks for the pieces the table relies on
assert_jal(0x159E78, 0x157080); assert_jal(0x159E44, 0x1580F8)
assert_imm(0x159E6C, 'a3', 4, 'card category'); assert_imm(0x159EA4, 'a3', 3, 'poster category')
assert_jal(0x159EC0, 0x157FD0); assert_imm(0x159F20, 'a3', 7, 'toy category'); assert_jal(0x159F3C, 0x158348)
assert_imm(0x159F9C, 'a3', 5, 'art category'); assert_jal(0x159F74, 0x158220)
CARD_LOOP_MAX = dec(0x159E64)['imm']       # slti v0,s2,4
ART_LOOP_MAX = dec(0x159F94)['imm']        # slti v0,s2,2
assert (CARD_LOOP_MAX, ART_LOOP_MAX) == (4, 2)
assert_imm(0x159F04, 't0', 1, '158F60 pass arg (peak-1 goals)'); assert_imm(0x159FF0, 't0', 2, '158F60 pass arg (peak-2 goals)')
assert_imm(0x159EDC, 'v0', 0xB, '1E38B8 on award 11'); assert_imm(0x159FC8, 'v0', 0xC, '1E38B8 on award 12')
assert u32(0x15A03C) == 0x0000382D  # daddu a3,zero,zero (peak0)
assert_imm(0x15A058, 'a3', 1, '156C70 peak1'); assert_imm(0x15A074, 'a3', 2, '156C70 peak2')
assert_jal(0x15A08C, 0x156EE0); assert_jal(0x15A124, 0x14B560); assert_jal(0x15A0D0, 0x158618)


def case_char(target):
    """cases that only set s7 = cheat character id (addiu s7,zero,imm within the first 3 instrs)."""
    for a in range(target, target + 24, 4):
        d = dec(a)
        if d['op'] == 9 and d['rs'] == 0 and REG[d['rt']] == 's7':
            return d['imm']
    return None


def case_jals(target):
    out = []
    for a in range(target, target + 12, 4):
        d = dec(a)
        if d['op'] == 3:
            out.append(d['tgt'])
    return out


# 159080: award id for (peak a3, goal t0) -- run it
GOALS = ['race', 'freestyle', 'explore', 'earnings']
peak_goal_award = {}
for pk in range(3):
    for g in range(4):
        m = Mini({'a3': pk, 't0': g, 'ra': 0})
        m.run(0x159080, {'ret'})
        peak_goal_award[(pk, g)] = Mini._s(m.r[2])

# 1591E8 event-result trigger: jump table 0x45A9F0 (mode 0..13) -> 'addiu fp, s0, imm'
JT_EVT = 0x45A9F0
event_award = {}
for mode in range(14):
    t = u32(JT_EVT + 4 * mode)
    # case bodies are "b join / addiu fp,s0,imm" or a lone "addiu fp,s0,imm"
    base = None
    for a in (t, t + 4):
        dd = dec(a)
        if dd['op'] == 9 and REG[dd['rt']] == 'fp' and REG[dd['rs']] == 's0':
            base = dd['imm']
    event_award[mode] = base
MODE_NAMES = {0: 'race', 1: 'slopestyle', 2: 'superpipe', 3: 'big_air', 4: 'rival (backcountry) race', 5: 'rival (backcountry) jam',
              6: 'Peak 1 Race', 7: 'Peak 2 Race', 8: 'All Peak Race (peak 3)', 9: 'Peak 1 Jam', 10: 'Peak 2 Jam', 11: 'All Peak Jam (peak 3)',
              12: 'mode 12 (no award)', 13: 'mode 13 (no award)'}

# messages sent by the side-effect hooks (verified immediates)
MSG_HOOKS = {
    0x1E2578: dict(flag=assert_imm(0x1E25A8, 'a0', 0x38), message=assert_imm(0x1E25C0, 'a0', 0xFB), text_id='kT_MSGCONQUER'),
    0x1E2518: dict(flag=assert_imm(0x1E2548, 'a0', 0x3C), message=assert_imm(0x1E2560, 'a0', 0xFF), text_id='kT_MSGCOMPLMTN'),
}
E2428 = {0: dict(flag=0x39, message=assert_imm(0x1E24AC, 'a0', 0xFC), text_id='kT_MSGBEATPK1'),
         1: dict(flag=0x3A, message=assert_imm(0x1E24CC, 'a0', 0xFD), text_id='kT_MSGBEATPK2'),
         2: dict(flag=0x3B, message=assert_imm(0x1E24FC, 'a0', 0xFE), text_id='kT_MSGBEATPK3')}
for v in list(MSG_HOOKS.values()) + list(E2428.values()):
    v['text'] = loc(v['text_id'])
    v['flag'] = H(v['flag']); v['message'] = H(v['message'])

awards = []
for aid in range(32):
    t = jt[aid]
    e = dict(award_id=aid, case_address=H(t), save_bit=dict(byte=H(0xF28 + (aid >> 3)), mask=H(1 << (aid & 7)),
                                                             note='R+0xF28 bitfield, set by 0x15A358 after the case runs'))
    grants = []
    trig = None
    if aid <= 4:
        ch = case_char(t)
        hook = case_jals(t)
        e['grants'] = [dict(kind='cheat_character', via='0x158618(obj,prof,char,charId,pay=0)', **cheat(ch))]
        msg = None
        if hook and hook[0] in MSG_HOOKS:
            msg = MSG_HOOKS[hook[0]]
        elif hook and hook[0] == 0x1E2428:
            msg = E2428[{0x159DF0: 0, 0x159E04: 1, 0x159E18: 2}[t]]
        e['inbox_message'] = dict(hook=H(hook[0]) if hook else None, **(msg or {}),
                                  note='persistent flag (0x1E3A78 id) + lodge inbox message (0x1E2FE0 id) -- sent only if flag not yet set')
        trig = {0: 'Conquer the mountain: 0x1577E0 -> for each peak 0x157920 (every entry of 0x45AAD8 race/freestyle lists: modes 0-3 need GOLD or better (medal<2), modes 4-11 any medal; explore goal: collectible medal (0x1589B0) AND big-challenge medal (0x158A50) both PLATINUM (==0)) AND sum of the 8 stat-medal tiers R+0xBB8..0xBBF >= 24 (i.e. all at tier 3; tiers set by 0x155390 from thresholds 0x440ED0)',
                1: 'All goals of all 3 peaks complete: 0x1578A0 -> 0x157A78(peak) for peak 0..2',
                2: 'All 4 goals of Peak 1 complete: 0x157A78(peak 0) -> 0x157BF0(peak0, goal 0..3)',
                3: 'All 4 goals of Peak 2 complete: 0x157A78(peak 1)',
                4: 'All 4 goals of Peak 3 complete: 0x157A78(peak 2)'}[aid]
    elif aid <= 16:
        pk = (aid - 5) % 3; g = (aid - 5) // 3
        assert peak_goal_award[(pk, g)] == aid
        trig = 'Peak %d %s goal complete (0x157BF0(peak=%d, goal=%d); award id = 0x159080(peak,goal) = 5 + goal*3 + peak). Checked by 0x159170 after every event result (0x1591E8), collectible/BC update (0x1599A0, 0x159B08), earnings (0x159818) and the cascade loop inside 0x159CD0 itself (@0x15A12C..0x15A2AC)' % (pk + 1, GOALS[g], pk, g)
        e['peak'] = pk + 1; e['goal'] = GOALS[g]
        e['trophy_inferred'] = dict(rwrd_trophy_index=aid - 5, name=rws(RWSEC[0][aid - 5][0]),
                                    note='INFERRED: trophy room shows award bits 5..16 (trophy table order race/fs/explore/earnings x peak matches award-5)')
        if t == jt[5]:        # peak 1 goals: 4 cards + 1 poster + peak 2 pass
            grants = [dict(kind='trading_card', count=CARD_LOOP_MAX, pick='0x157080(category 4)', give='0x1580F8(...,pay=0)', stops_early='when no unowned card left (pick returns -1)'),
                      dict(kind='poster', count=1, pick='0x157080(category 3)', give='0x157FD0(...,pay=0)'),
                      dict(kind='peak_pass', via='0x158F60(...,award,1)', effect='clears lock bit 12 (R+0x278 & ~0x1000) = Peak 2 pass if still locked; message cat 2 item char*3+1')]
        elif t == jt[6]:      # peak 2 goals: toy + 2 art + peak 3 pass
            grants = [dict(kind='toy', count=1, pick='0x157080(category 7)', give='0x158348(...,pay=0)'),
                      dict(kind='art', count=ART_LOOP_MAX, pick='0x157080(category 5)', give='0x158220(...,pay=0)'),
                      dict(kind='peak_pass', via='0x158F60(...,award,2)', effect='clears lock bit 13 (Peak 3 pass, msg item char*3+2) and, if still set, bit 12 (Peak 2 pass, msg item char*3+1)')]
        else:                 # peak 3 goals: cheat character
            grants = [dict(kind='cheat_character', via='0x158618(obj,prof,char,charId,pay=0)', **cheat(case_char(t)))]
        e['grants'] = grants
        if aid == 11:
            e['inbox_message'] = dict(hook='0x1E38B8(profile)', note='0x1E38B8: if Peak 1 explore goal done (0x157BF0(0,2)) and flag 0x36 unset: set it + inbox msg 0xF9; if Peak 2 explore goal done and flag 0x37 unset: flag + msg 0xFA. Both messages use body kT_OVRHELPEarnTropPassExp (message table built at 0x21A5DC..0x21A608)', text=loc('kT_OVRHELPEarnTropPassExp'))
        if aid == 12:
            e['inbox_message'] = dict(hook='0x1E38B8(profile)', note='same hook as award 11')
    elif aid <= 19:
        pk = aid - 17
        e['grants'] = [dict(kind='gear', count=1, pick='0x156C70(obj,prof,char,peakIdx=%d)' % pk,
                            pool='this rider\'s BOLTPS2 entries with flag 0x%X (tier %d) that are NOT owned (inventory flag 0x2 clear); uniform random (rand()%%n, 0x3177F0)' % (0x100 << pk, 11 + pk),
                            give='0x14B560(...,itemId,pay=0) (+ bundled items with price == -itemId)', message='0x15A628 cat 9 (gear), item id', none_left='returns -1 -> nothing granted')]
        trig = 'First GOLD or PLATINUM (medal 0/1) in a standard Peak %d event (modes 0-3: race/slopestyle/superpipe/big air) -- 0x1591E8: jump table 0x45A9F0 fp = 0x11 + peak; condition medal<2 AND event slot (R+0xAD0+slot*8)+2 (u16 count of gold-or-better finishes, incremented by 0x152528 AFTER this check) == 0' % (pk + 1)
        e['repeatable'] = 'YES: 0x159CD0 skips the already-awarded test for ids 17..19 (@0x159D64 sltiu (id-0x11)<3), so every standard event of that peak grants one item on its first gold'
        e['peak'] = pk + 1
    else:
        e['grants'] = [dict(kind='gear', count=1, pick='0x156EE0(obj,prof,char)',
                            pool='this rider\'s BOLTPS2 entries with flag 0x800 (tier 14 "special" pool - the rider\'s signature boards) that are NOT owned; uniform random',
                            give='0x14B560(...,itemId,pay=0)', message='0x15A628 cat 9 (gear)', none_left='returns -1 -> nothing')]
        k = aid - 20
        grp, pk = [('rival (backcountry) race', 0x14), ('rival (backcountry) jam', 0x17), ('peak race event (modes 6-8)', 0x1A), ('peak jam event (modes 9-11)', 0x1D)][k // 3][0], k % 3
        trig = 'First GOLD or PLATINUM in the Peak %d %s (0x1591E8 fp = base+peak; modes 6-11 force peak index 0/1/2 from the mode)' % (pk + 1, grp)
        e['peak'] = pk + 1
    e['trigger'] = trig
    awards.append(e)

award_section = dict(
    function=H(AWARD_FN), signature='0x159CD0(obj, profile, character, awardId)', jump_table=H(JT_AWARD),
    guard='if awardId not in 17..19 and bit already set in R+0xF28 (0x15A2E0/0x15A320) -> return; ids >= 32 skip the switch',
    after_switch='0x15A358 sets award bit; if s7 (cheat char id) != 0: message cat 8 + 0x158618(...,s7,pay=0); if gear id != -1: message cat 9 + 0x14B560(...,gear,pay=0); then cascade: re-check award 0 (0x1577E0), award 1 (0x1578A0), awards 2..4 (0x157A78 peak 0..2) and every peak goal (0x159080/0x157BF0 -> 0x159170), recursively calling 0x159CD0',
    event_trigger=dict(function='0x1591E8 (called from 0x1550B0 after an event result)', jump_table=H(JT_EVT),
                       mode_to_award_base={MODE_NAMES[m]: (H(v) if v is not None else None) for m, v in event_award.items()},
                       peak_override='modes 6/9 -> peak 0, 7/10 -> 1, 8/11 -> 2 (@0x1592A0..0x1592EC); else peak = 0x15A398(course)',
                       condition='medal < 2 (platinum 0 / gold 1) AND event slot record +2 (u16 gold-or-better count) == 0'),
    peak_goal_award_map={'peak%d' % (pk + 1): {GOALS[g]: peak_goal_award[(pk, g)] for g in range(4)} for pk in range(3)},
    random_pick_0x157080=dict(
        signature='0x157080(obj, profile, character, category) -> index or -1',
        rule='total = 0x157210(category) (RewardDB count), owned = 0x157518 (popcount of the category bitfield); if owned >= total -> -1; r = rand() % (total-owned) (0x3177F0); return the index of the r-th CLEAR bit in the category bitfield (walk from bit 0). Eligible = every unowned item of the category regardless of peak or price (peak field / not-for-sale items are NOT filtered).',
        bitfields='jump table 0x45A950 (category-3): 3 poster R+0xF30, 4 card R+0xF36, 5 art R+0xF45, 6 video R+0xF52, 7 toy R+0xF53'),
    give_functions={
        '0x157FD0': 'poster: set bit R+0xF30; if pay: cash -= price*10 (0x150B48); if owned == total (0x1575C0 vs 0x1572B0) -> 0x158618(...,0x14,pay=0)',
        '0x1580F8': 'trading card: bit R+0xF36; all owned (0x157620 vs 0x1572D0) -> 0x158618(...,0x11,pay=0)',
        '0x158220': 'art: bit R+0xF45; all owned (0x157680 vs 0x1572F0) -> 0x158618(...,0x16,pay=0)',
        '0x158348': 'toy: bit R+0xF53; all owned (0x1576E0 vs 0x157310) -> 0x158618(...,0x13,pay=0)',
        '0x158470': 'video: bit R+0xF52 (no collection bonus)',
        '0x158618': 'cheat character: bit (charId-10) at R+0xF57; if pay: cash -= price*10',
    },
    collection_bonuses=[dict(collection='all trading cards', check='0x1581E0', **cheat(0x11)),
                        dict(collection='all toys', check='0x158430', **cheat(0x13)),
                        dict(collection='all posters', check='0x1580B8', **cheat(0x14)),
                        dict(collection='all art', check='0x158308', **cheat(0x16))],
    collection_counts={RW_CATNAME[t]: len(RWSEC[t]) for t in range(9)},
    duplicates_rule='No cash-for-duplicates anywhere in 0x159CD0/0x157080/0x156C70/0x156EE0: draws only consider unowned items and simply grant nothing (-1) when the pool is exhausted.',
    message_queue=dict(function='0x15A628(0x4C3EF0 queue, awardId, category, item)', categories={'2': 'peak pass (item = char*3+peak0)', '3': 'poster', '4': 'trading card', '5': 'art', '7': 'toy', '8': 'cheat character (charId)', '9': 'gear (BOLTPS2 item id)'}),
    cheat_characters={H(k): dict(v, unlock=None) for k, v in sorted(CHEAT.items())},
    awards=awards)
for k, v in award_section['cheat_characters'].items():
    cid = int(k, 16)
    src = [a['award_id'] for a in awards for gr in a.get('grants', []) if gr.get('character_id') == k]
    col = [b['collection'] for b in award_section['collection_bonuses'] if b['character_id'] == k]
    v['unlock'] = ('award %s' % src if src else '') + ('collection: %s' % col if col else '') + \
                  ('lodge purchase peak %s $%s' % (v['shop_peak'], v['shop_price']) if v['shop_price'] else '')

# ================================================================= 2. GEAR ========================
BOLT = disc_file('DATA/CHAR/BOLTPS2.DAT', LOCAL / 'sam-ps2/original/BOLTPS2.DAT')
b_ver, b_n = struct.unpack_from('<2I', BOLT, 0)
p = 8
ENT = [BOLT[p + i * 56: p + (i + 1) * 56] for i in range(b_n)]
p += b_n * 56
TABS = []
for stride in (12, 8, 8):
    n = struct.unpack_from('<I', BOLT, p)[0]; p += 4
    TABS.append([BOLT[p + k * stride: p + (k + 1) * stride] for k in range(n)]); p += n * stride
POOL = p + 4


def bs(o):
    if not o: return None
    a = POOL + o - 1
    return BOLT[a:BOLT.index(b'\0', a)].decode('latin1')


def bparse(i, r):
    item, cls, parent, order, weight, cost = struct.unpack_from('<6h', r, 4)
    offs = struct.unpack_from('<9I', r, 0x14)
    flags = offs[8]
    return dict(index=i, char=r[0], b1=r[1], tier=struct.unpack('b', r[2:3])[0], b3=r[3], item=item, cls=cls, parent=parent,
                order=order, weight=weight, cost=cost, name=bs(offs[0]), models=[bs(x) for x in offs[1:5]], path=bs(offs[5]),
                texture=bs(offs[6]), icon=bs(offs[7]), flags=flags)


E = [bparse(i, r) for i, r in enumerate(ENT)]
DEFAULTS = [struct.unpack('<BBhhh', t) for t in TABS[2]]


def header(n):
    if n and n.startswith('kT_'):
        return loc(n) or n
    return n


TIER_DESC = {0: 'default / not sold (owned at start if price field is 0 or -1)', 1: 'sold in Peak 1 lodge', 2: 'sold in Peak 2 lodge', 3: 'sold in Peak 3 lodge',
             11: 'not sold: Peak 1 reward pool (flag 0x100, award 17)', 12: 'not sold: Peak 2 reward pool (flag 0x200, award 18)',
             13: 'not sold: Peak 3 reward pool (flag 0x400, award 19)', 14: 'not sold: special reward pool (flag 0x800, awards 20-31)'}


def flag_names(f):
    out = []
    for bit, nm in ((0x1, 'model/slot-exclusive'), (0x4, 'selectable item (leaf)'), (0x8, 'multi-select/stackable (INFERRED)'), (0x10, '0x10'),
                    (0x20, 'menu folder'), (0x40, '0x40'), (0x100, 'peak1 reward pool'), (0x200, 'peak2 reward pool'), (0x400, 'peak3 reward pool'),
                    (0x800, 'special reward pool'), (0x1000, '0x1000')):
        if f & bit: out.append(nm)
    return out


RULES = [dict(char=t[0], on=struct.unpack('b', t[1:2])[0], item=struct.unpack_from('<h', t, 2)[0], b4=t[4],
              cond_state=struct.unpack('b', t[5:6])[0], cond=struct.unpack_from('<h', t, 6)[0], b8=t[8],
              desired=struct.unpack('b', t[9:10])[0], target=struct.unpack_from('<h', t, 10)[0]) for t in TABS[0]]


class Inventory:
    """Python port of the gear inventory: 0x151C90 equip(R,item,on) + 0x151EF0 rule application (12-byte rules,
    selected by 0x14DB40 on (char, item, on)) + 0x1513B8 initialisation. Verified bit-exact (flags 0x2/0x4/0x10/0x20)
    against all 10 riders in the fresh-career lodge savestate."""

    def __init__(self, ch):
        self.ch = ch
        self.ents = [e for e in E if e['char'] == ch]
        self.by = {}
        for e in self.ents:
            self.by.setdefault(e['item'], e)
        self.f = {e['item']: 0 for e in self.ents}
        self.rules = [r for r in RULES if r['char'] == ch]

    def equip(self, item, on):
        e = self.by[item]
        was = self.f[item] & 0x10
        self.f[item] = self.f[item] | 0x10 if on else self.f[item] & ~0x10
        if on:
            if was:
                return 1
            if e['flags'] & 1:      # model/slot entry: unequip every other entry of the same +6 class
                for S in self.ents:
                    if S['cls'] == e['cls'] and S['item'] != item and ((self.f[S['item']] & 0x10) or (S['flags'] & 8)):
                        if not self.equip(S['item'], 0):
                            return 0
            return self.apply(item, 1)
        if was or (e['flags'] & 8):  # unequip: also unequip entries whose +6 class is this item
            for K in self.ents:
                if K['cls'] == item and not self.equip(K['item'], 0):
                    return 0
        return self.apply(item, 0) if was else 1

    def apply(self, item, on):
        for r in self.rules:
            if r['item'] != item or (r['on'] != 0) != bool(on) or r['target'] == -1:
                continue
            if r['cond'] != -1 and bool(self.f.get(r['cond'], 0) & 0x10) != (r['cond_state'] != 0):
                continue
            tgt, des = r['target'], r['desired'] != 0
            cur = bool(self.f.get(tgt, 0) & 0x10)
            if cur == des:
                if cur:
                    continue
                te = self.by.get(tgt)
                if not te or not (te['flags'] & 8):
                    continue
            if tgt != item:
                if not self.equip(tgt, des):
                    return 0
            else:
                self.f[item] = self.f[item] | 0x10 if des else self.f[item] & ~0x10
        return 1

    def init(self):
        for d in DEFAULTS:
            if d[0] == self.ch:
                self.equip(d[4], 1)
        for k in self.f:
            if self.f[k] & 0x10:
                self.f[k] |= 0x26
        found = 0
        for e in self.ents:
            if e['cost'] == 0:
                self.f[e['item']] |= 2
            elif e['cost'] == -1:
                self.f[e['item']] |= 2
                self.equip(e['item'], 1)
                found = 1
        if found:
            for k in self.f:
                if self.f[k] & 0x10:
                    self.f[k] |= 6
                else:
                    self.f[k] &= ~4
        return self.f


def initial_inventory(ch, ents=None):
    return Inventory(ch).init()


def children(ents, parent, depth=-1, into_folders=False):
    """0x14D7E8(db,char,parentId,out,depth,recurseFolders)."""
    out = []
    for e in ents:
        if e['parent'] == parent:
            out.append(e)
            if depth != 0 and (not (e['flags'] & 0x20) or into_folders):
                out += children(ents, e['item'], depth - 1, into_folders)
    return out


def buy_list(ents, inv, folder, peak):
    """0x19B180 (buy mode): leaves unowned with tier == lodge peak; folders that contain such a leaf (0x19B098); sorted by +0xA."""
    res = []
    for e in children(ents, folder):
        if not e['name']: continue
        if e['flags'] & 0x20:
            if any((c['flags'] & 4) and not (inv.get(c['item'], 0) & 2) and c['tier'] == peak and not (c['flags'] & 0x20)
                   for c in children(ents, e['item'], -1, True)):
                res.append(e)
        elif e['flags'] & 4:
            if not (inv.get(e['item'], 0) & 2) and (peak == -1 or e['tier'] == peak):
                res.append(e)
    return sorted(res, key=lambda e: e['order'])


def menu_path(ents_by_item, e):
    path = []
    cur = e['parent']
    seen = set()
    while cur != -1 and cur in ents_by_item and cur not in seen:
        seen.add(cur)
        f = ents_by_item[cur]
        path.append(header(f['name']))
        cur = f['parent']
    return list(reversed(path))


riders = []
for ch in range(10):
    ents = [e for e in E if e['char'] == ch]
    by_item = {}
    for e in ents:
        by_item.setdefault(e['item'], e)
    inv = initial_inventory(ch, ents)
    items = []
    for e in ents:
        if not (e['flags'] & 4):
            continue
        pth = menu_path(by_item, e)
        price = e['cost'] * 10 if e['cost'] > 0 else 0
        tier = e['tier']
        it = dict(item_id=e['item'], name=e['name'], menu_path=pth, top=pth[0] if pth else None,
                  category=pth[-1] if pth else None, price_field=e['cost'], price=price,
                  tier=tier, source=TIER_DESC.get(tier, str(tier)),
                  sold_in_lodge_peak=tier if tier in (1, 2, 3) else None,
                  reward_pool={11: 'peak1', 12: 'peak2', 13: 'peak3', 14: 'special'}.get(tier),
                  owned_at_start=bool(inv.get(e['item'], 0) & 2), equipped_at_start=bool(inv.get(e['item'], 0) & 0x10),
                  granted_with_item=(-e['cost'] if e['cost'] < -1 else None),
                  flags=H(e['flags']), flag_names=flag_names(e['flags']), model_class=e['cls'], menu_parent=e['parent'],
                  menu_order=e['order'], b1=e['b1'], b3=e['b3'], weight=e['weight'], bolt_index=e['index'],
                  model=e['path'] or next((m for m in e['models'] if m), None), texture=e['texture'], icon=e['icon'])
        items.append(it)
    folders = [dict(item_id=e['item'], text_id=e['name'], text=header(e['name']), parent=e['parent'], order=e['order'])
               for e in ents if e['flags'] & 0x20]
    outfit = []
    for d in DEFAULTS:
        if d[0] == ch:
            e = by_item.get(d[4])
            outfit.append(dict(model_class=d[3], item_id=d[4], name=e['name'] if e else None))
    pools = {nm: [dict(item_id=e['item'], name=e['name'], category=(menu_path(by_item, e) or [None])[-1]) for e in ents
                  if (e['flags'] & bit) and (e['flags'] & 4)]
             for nm, bit in (('award17_peak1_0x100', 0x100), ('award18_peak2_0x200', 0x200), ('award19_peak3_0x400', 0x400), ('awards20_31_special_0x800', 0x800))}
    lodge = {}
    for pk in (1, 2, 3):
        def tree(folder, depth=0):
            out = []
            for e in buy_list(ents, inv, folder, pk):
                if e['flags'] & 0x20:
                    out.append({'folder': header(e['name']), 'items': tree(e['item'], depth + 1)})
                else:
                    out.append({'item': e['name'], 'item_id': e['item'], 'price': e['cost'] * 10 if e['cost'] > 0 else 0})
            return out
        lodge['peak%d' % pk] = tree(-1)
    sold = [i for i in items if i['sold_in_lodge_peak']]
    riders.append(dict(character=ch, name=RIDERS[ch], bolt_entries=len(ents), selectable_items=len(items),
                       sold_in_lodges=len(sold), total_lodge_price=sum(i['price'] for i in sold),
                       owned_at_start=sum(1 for i in items if i['owned_at_start']),
                       default_outfit=outfit, lodge_buy_menu_fresh_career=lodge, reward_pools=pools,
                       equipped_entries_at_start=sorted(k for k, v in inv.items() if v & 0x10),
                       folders=folders, items=items,
                       equip_rules=[[r['on'], r['item'], r['cond'], r['cond_state'], r['target'], r['desired'], r['b4'], r['b8']]
                                    for r in RULES if r['char'] == ch]))

# validation against PS2 frames local/ps2-capture/menus/lodge/13..17 (Zoe, fresh career, Peak 1 lodge, $507)
zoe = riders[4]['lodge_buy_menu_fresh_career']['peak1']


def fnames(node): return [x.get('folder') or x.get('item') for x in node]


def sub(node, name): return next(x['items'] for x in node if x.get('folder') == name)


checks = {
    'root (12-buy-gear)': (fnames(zoe), ['Head', 'Upper Body', 'Lower Body', 'Boards']),
    'Head (13)': (fnames(sub(zoe, 'Head')), ['Hats', 'Eyewear', 'Accessories', 'Special']),
    'Hats first 6 (14)': (fnames(sub(sub(zoe, 'Head'), 'Hats'))[:6], ['Peacekeeper', 'True Hero', 'Low Beanies', 'Beanies w Roller', 'Tall Beanie', 'Nordic Beanie']),
    'Peacekeeper cost (14)': ([x['price'] for x in sub(sub(zoe, 'Head'), 'Hats') if x.get('item') == 'Peacekeeper'], [50000]),
    'Upper Body (15)': (fnames(sub(zoe, 'Upper Body')), ['Tops', 'Hands', 'Accessories']),
    'Lower Body (16)': (fnames(sub(zoe, 'Lower Body')), ['Bottoms', 'Boots']),
    'Boards (17)': ([(x.get('item'), x.get('price')) for x in sub(zoe, 'Boards')], [('Stuff', 1000), ('Element', 1000), ('Stuff II', 1000), ('dnL', 1000)]),
}
validation = {k: dict(computed=a, ps2_frame=b, ok=(list(map(tuple, a)) if a and isinstance(a[0], list) else a) == b) for k, (a, b) in checks.items()}

gear_section = dict(
    source='DATA/CHAR/BOLTPS2.DAT (%d bytes; loaded into DB object 0x4A6750)' % len(BOLT),
    format=dict(header='u32 %d, u32 entry count %d; entries 56 B; then 3 tables (u32 n + n*stride): 12B x%d (compat rules), 8B x%d, 8B x%d (default outfit {u8 char,u8,s16 0,s16 model_class,s16 item}); u32 pool size; string pool (1-based offsets)' % (b_ver, b_n, len(TABS[0]), len(TABS[1]), len(TABS[2])),
                entry={'+0': 'u8 character (0-9 riders; 10-29 cheat characters)', '+1': 'u8 b1 (variant index?)', '+2': 's8 tier (see tiers)', '+3': 'u8 b3 (body-slot group?)',
                       '+4': 's16 item id (unique per character)', '+6': 's16 model/class id (slot group used by equip)', '+8': 's16 menu parent (folder item id, -1 root)',
                       '+0xA': 's16 menu order (buy/equip lists sorted by it)', '+0xC': 's16 weight?', '+0xE': 's16 price/10 (0 = owned at start, -1 = owned+equipped at start, -N (N>1) = granted free when item N is bought, >0 = price)',
                       '+0x14': 'name (plain English; folders are kT_BoltHeader_* locale ids)', '+0x18..+0x24': 'model names', '+0x28': 'model path', '+0x2C': 'texture', '+0x30': 'icon', '+0x34': 'u32 flags'},
                flags={'0x1': 'model entry / slot-exclusive', '0x4': 'selectable item (buy/equip leaf)', '0x8': 'INFERRED multi-select (equip does not unequip siblings)', '0x20': 'menu folder',
                       '0x100/0x200/0x400': 'peak 1/2/3 reward pool (tier 11/12/13, award 17/18/19)', '0x800': 'special reward pool (tier 14, awards 20-31)'},
                tiers={str(k): v for k, v in TIER_DESC.items()}),
    db_object='0x4A6750: +4 entries, +0x2C count[30] (0x14D988), +0xA4 first index[30] (0x14D998), +0x3EC default count (0x14DC40), +0x464 default first (0x14DC50)',
    inventory=dict(record='R = 0x4A6CA8 + profile*0x9B50 + char*0xF88', ptr='R+0x288 runtime s16[itemId] -> slot (-1 none)', count='R+0x28C', slots='R+0x290: 0x20D x {s16 itemId, u16 flags}, one per BOLT entry of that character in file order',
                   flags={'0x2': 'owned', '0x10': 'equipped', '0x4': 'committed (INFERRED)', '0x20': 'default outfit'},
                   init='0x151A88 (slots) + 0x1513B8 (see equip_algorithm.init)',
                   character_byte='R+0xBC0 = character id'),
    functions={'buy': '0x14B560(obj,profile,char,itemId,pay): 0x151C48 own item; own every entry of the char whose price field == -itemId; if pay: cash R+0xAC4 -= price*10 via 0x150B48 (no funds check here)',
               'buy_ui': '0x19BE80 (Buy Gear confirm, pay=1) ; help text 0x19BA60: folder -> "Choose a category", cash < price -> kT_129HELPSaveCashItem "Save more cash to buy this item.", else buy text',
               'buy_list': '0x19B180: lodge peak (+0xCC, from 0x1990B0: station course 17/18 -> 1, 19/20 -> 2, 21 -> 3); lists named leaves that are unowned with tier == lodge peak and folders containing one (0x19B098); children via 0x14D7E8; sorted by +0xA',
               'equip': '0x151C90(R,itemId,on) + 0x151EF0 rules: see equip_algorithm',
               'rewards_screen_help': '0x1D0238 uses kT_129HELPBuyInPeak1/2/3 / kT_129HELPSaveCashItem for RWRD items (posters etc.), not gear'},
    equip_algorithm=dict(
        equip='0x151C90(R,item,on): set/clear flag 0x10. ON: if it was already equipped return; if the entry has flag 0x1, unequip (recursively) every other entry of the rider with the same +6 class that is equipped or has flag 0x8; then apply rules(item,on=1). OFF: if it was equipped or has flag 0x8, unequip every entry whose +6 class == item; if it was equipped apply rules(item,on=0).',
        rules='0x151EF0 + 0x14DB40: rules = BOLTPS2 table 1 (12 B: u8 char, s8 on, s16 item, u8 b4, s8 cond_state, s16 cond_item, u8 b8, s8 desired, s16 target), contiguous per (char,item,on). For each: skip target -1; if cond_item != -1 and equipped(cond_item) != (cond_state!=0) skip; if equipped(target) != desired (or both off and target has flag 0x8): target==item ? set/clear directly : equip(target, desired) recursively.',
        per_rider_rule_columns=['on', 'item', 'cond_item', 'cond_state', 'target', 'desired', 'b4', 'b8'],
        init='0x1513B8: equip(default outfit items, table 3); OR 0x26 into equipped; then for every entry in file order: price 0 -> owned; price -1 -> owned + equip(); if any -1 item: equipped |= 6, others &= ~4. (Python port verified bit-exact vs savestate for all 10 riders.)',
        buy_does_not_equip='0x14B560 only sets owned (0x2) on the item and its bundle; equipping happens in Equip Gear'),
    locale_headers={e['name']: header(e['name']) for e in E if e['name'] and e['name'].startswith('kT_BoltHeader')},
    validation_vs_ps2_frames=validation,
    riders=riders,
    # Compact runtime tables for the browser port of 0x1513B8 / 0x151C90 / 0x151EF0 / 0x14B560 / 0x19B180 (web/lodge.js):
    # entries in file order [item, class(+6), parent(+8), order(+0xA), flags, price field(+0xE), tier(+2), name];
    # rules [on, item, cond_item, cond_state, target, desired]; defaults = default-outfit items (table 3).
    runtime={str(ch): dict(
        entries=[[e['item'], e['cls'], e['parent'], e['order'], e['flags'], e['cost'], e['tier'], header(e['name'])] for e in E if e['char'] == ch],
        rules=[[r['on'], r['item'], r['cond'], r['cond_state'], r['target'], r['desired']] for r in RULES if r['char'] == ch],
        defaults=[d[4] for d in DEFAULTS if d[0] == ch],
        initial_flags={str(k): v for k, v in initial_inventory(ch).items() if v}) for ch in range(10)})

# ================================================================= 3. UBER TRICKS ==================
UB_TAB, UB_NAMES = 0x45AEB8, 0x43D160
assert_jal(0x185000, 0x14FE08)


def ubname(i):
    try:
        return cstr(u32(UB_NAMES + 4 * i)).strip() or None
    except Exception:
        return None


ROW = {'Mute': 1, 'Indy': 3, 'Stalefish': 2, 'Method': 0, 'Nose Grab': 4, 'Tail Grab': 9}   # 0x185100.. string compare -> +0x18
# verify the row->category immediates in the menu handler
for addr, v in ((0x1851A0, 1), (0x1851B8, 3), (0x1851D0, 2), (0x1851FC, 4), (0x185218, 9)):
    assert_imm(addr, 'v0', v, 'uber row category')
cats = []
for c in range(15):
    r = at(UB_TAB + c * 0x14, 0x14)
    f = struct.unpack('<6h', r[:12]); n = struct.unpack_from('<H', r, 12)[0]; lp = struct.unpack_from('<I', r, 16)[0]
    ent = []
    for k in range(n):
        a, b, idx, price = struct.unpack('<4H', at(lp + 8 * k, 8))
        ent.append(dict(entry=k, name=ubname(idx), name_index=idx, trick_ids=[a, b], price=price,
                        hidden=k < 2, note='entries 0/1 are the hidden base ubers (visible and lock bits 0,1 always cleared); per-rider +0 byte picks which one' if k < 2 else None))
    cats.append(dict(category=c, base_grab=ubname(f[2]), name_fields={str(i): dict(index=x, text=ubname(x)) for i, x in enumerate(f)},
                     menu_row=next((k for k, v in ROW.items() if v == c), None), count=n, list_address=H(lp) if lp else None, entries=ent))

# per-rider defaults: run 0x150558(char, buf) case code (per-char jump table 0x45A6F0; char >= 9 -> 0x1507A8)
JT_UB = 0x45A6F0
BUF = 0x10000000
per_char = []
for ch in range(10):
    m = Mini({'s0': BUF, 'a0': ch})
    start = u32(JT_UB + 4 * ch) if ch < 9 else 0x1507A8
    m.run(start, {0x1507E8})
    buf = bytearray(90)
    for a, v in m.mem.items():
        buf[a - BUF] = v
    # generic tail 0x1507E8: masks = (1<<n)-1; lock &= ~(1<<buf[c*6+1]); both &= 0xFFFC
    rows = {}
    for c in range(15):
        n = cats[c]['count']
        lock = vis = (1 << n) - 1 if n else 0
        lock &= ~(1 << buf[c * 6 + 1]) & 0xFFFC
        vis &= 0xFFFC
        buf[c * 6 + 2:c * 6 + 4] = struct.pack('<H', lock); buf[c * 6 + 4:c * 6 + 6] = struct.pack('<H', vis)
        if n:
            rows[cats[c]['menu_row']] = dict(category=c, hidden_base=dict(entry=buf[c * 6], name=cats[c]['entries'][buf[c * 6]]['name']),
                                             owned_default=dict(entry=buf[c * 6 + 1], name=cats[c]['entries'][buf[c * 6 + 1]]['name']),
                                             for_sale=[dict(entry=k, name=cats[c]['entries'][k]['name'], price=cats[c]['entries'][k]['price'])
                                                       for k in range(n) if lock >> k & 1],
                                             lock_mask=H(lock), visible_mask=H(vis))
    per_char.append(dict(character=ch, name=RIDERS[ch], rows=rows, save_bytes=bytes(buf).hex()))

uber_section = dict(
    table=dict(address=H(UB_TAB), accessor='0x14FF90(obj,_,category,&count) -> list ptr', stride=0x14,
               layout='6 x s16 name indices (+4 = base grab name) , +0xC u16 count, +0x10 u32 list pointer',
               list_entry='8 B: u16 trick id A, u16 trick id B, u16 name index (0x116938 -> ptr table 0x43D160), u16 price in $ (no x10)'),
    ui_rows='Ubertrick Setup rows (menu handler 0x184F40 area, string compare @0x1850D4..): Mute=cat1, Indy=cat3, Stalefish=cat2, Method=cat0, Nose Grab=cat4, Tail Grab=cat9 (cats 5-8,10-14 have no list)',
    state=dict(runtime='0x530EC0 + profile*0x13EC + char*0x1FE + cat*6 (0x14FD80 / 0x14FE08)', save='R+0xBE6 + cat*6 (R+0xBE6..0xDE4 block copied to runtime; initialised by 0x150558 at 0x1518D8)',
               bytes={'+0': 'hidden base uber (entry 0/1)', '+1': 'selected/equipped entry (0x14FD80 writes it)', '+2': 'u16 LOCK mask (bit set = not owned)', '+4': 'u16 visible mask'}),
    init='0x150558(char, buf): memset 0x5A; per-character switch (jump table 0x45A6F0 chars 0-8, Viggo -> 0x1507A8) writes +0/+1 for cats 0-4,9; then for every cat with a list: lock = visible = (1<<n)-1, lock &= ~(1<<+1), both &= 0xFFFC',
    buy='select handler 0x1854A0: if cash >= list price (+6) open confirm dialog 0x1CAF58 (kT_OVRCMNBuyThisUber, name via +4) else kT_OVRCMNSaveCashUber; help 0x185938: kT_FEHELPTrickAvailBuy vs SaveCash; kT_FEHELPTrickBuyCTM outside career. Confirm 0x184F40: cash -= price (0x150B48) then 0x14FE08(obj,profile,char,cat,entry) clears the lock bit. Buying does not auto-select; selection (+1) is written by 0x14FD80.',
    categories=[c for c in cats if c['count']], categories_without_list=[dict(category=c['category'], base_grab=c['base_grab']) for c in cats if not c['count']],
    per_rider_defaults=per_char)

# ================================================================= 4. SONGS =======================
MUS = disc_file('DATA/CONFIG/MUSIC.INF').decode('latin1').replace('\r', '')
PL = disc_file('DATA/CONFIG/PLAYLIST.INF').decode('latin1').replace('\r', '')
sections = []
cur = None
for line in MUS.split('\n'):
    s = line.strip()
    m = re.match(r'^\[(.+)\]$', s)
    if m:
        cur = dict(key=m.group(1), fields={}); sections.append(cur); continue
    m = re.match(r'^(\w+)\s*=\s*"?([^"]*)"?', s)
    if cur and m and not s.startswith('#'):
        cur['fields'].setdefault(m.group(1).upper(), []).append(m.group(2))
songsecs = [x for x in sections if x['key'] != 'GLOBAL']
fe = [x for x in songsecs if x['fields'].get('ADDTOFE', ['0'])[0] == '1']
pl_order = re.findall(r'SONG\s*=\s*"([^"]+)"', PL)
CATN = {'0': 'race', '1': 'slopestyle', '2': 'big_air', '3': 'halfpipe', '4': 'backcountry'}
assert_imm(0x1585E0, 'a3', 5000, 'song price')
songs = []
for i, x in enumerate(fe):
    f = x['fields']
    songs.append(dict(index=i, key=x['key'], title=(f.get('TITLE') or [None])[0], artist=(f.get('ARTIST') or [None])[0],
                      album=(f.get('ALBUM') or [None])[0], preferred_events=[CATN.get(c, c) for c in f.get('CATEGORY', [])],
                      big=(f.get('SONGBIG') or [None])[0], owned_bit=dict(qword='R+0xF70', bit=i), playlist_bit=dict(qword='R+0xF78', bit=i)))
song_section = dict(
    source='DATA/CONFIG/MUSIC.INF (sections with ADDTOFE = 1, file order) + DATA/CONFIG/PLAYLIST.INF ([SSX Mix] order)',
    count=len(songs), playlist_inf_order_matches=[s['key'] for s in songs] == pl_order,
    runtime_count_check='music manager (gp+0x410) +0x504 = 35 in the lodge savestate; its name list is the MUSIC.INF order',
    buy_function='0x158558(obj,profile,char,song,pay): R+0xF70 |= 1<<song; if pay: cash -= 5000 (0x1388 immediate @0x1585E0)',
    buy_ui='0x1988D8: pay = (popcount(owned mask) >= 6); also sets the playlist bit (screen +0x158 -> R+0xF78). The first 6 songs are free "song credits" (kT_OVRCMNBuySongCredit "Buy song using free song credit?"), then $5,000 each (kT_16BuyMusicTrack "Buy song?")',
    price=5000, free_credits=6,
    owned_by_default='none: R+0xF70 = R+0xF78 = 0 for every rider in the fresh-career lodge savestate (radio "Radio BIG" / "BIG Mountain Ambience" modes need no songs; custom playlist entries are greyed until songs are owned)',
    per_peak_lodge='none found: 0x197AD8 lists all 35 songs (owned first, then unowned) with no peak test; songs are per character (bits in the character record)',
    other_fields=dict(F80='R+0xF80 u32 music mode (0x158700; INFERRED: Radio BIG / Ambience / Custom DJ / Custom no-DJ)', cheat='0x187D38 code hash 0x061F9F02: owns all songs for every character'),
    songs=songs)

# ================================================================= validation vs savestate ==========
state_check = None
P2S = LOCAL / 'ps2-capture/menus/ctm/state-lodge-peak1.p2s'
if P2S.exists():
    ee = zipfile.ZipFile(P2S).read('eeMemory.bin')
    res = {}
    for ch in range(10):
        R = PROFILE_BASE + ch * CHAR_STRIDE
        ub_ok = ee[R + 0xBE6:R + 0xBE6 + 90].hex() == per_char[ch]['save_bytes']
        # inventory
        cnt = struct.unpack_from('<i', ee, R + 0x28C)[0]
        slots = [struct.unpack_from('<hH', ee, R + 0x290 + 4 * k) for k in range(cnt)]
        ents = [e for e in E if e['char'] == ch]
        inv = initial_inventory(ch, ents)
        own_ok = all(bool(fl & 2) == bool(inv.get(it, 0) & 2) for it, fl in slots)
        flags_ok = all((fl & 0x36) == (inv.get(it, 0) & 0x36) for it, fl in slots)
        eq_ok = all(bool(fl & 0x10) == bool(inv.get(it, 0) & 0x10) for it, fl in slots)
        songs_ok = struct.unpack_from('<QQ', ee, R + 0xF70) == (0, 0)
        res[RIDERS[ch]] = dict(uber_bytes_match=ub_ok, inventory_slots=cnt, owned_flags_match=own_ok, equipped_flags_match=eq_ok, all_flags_0x36_match=flags_ok, songs_zero=songs_ok,
                               cash=struct.unpack_from('<i', ee, R + 0xAC4)[0])
    state_check = dict(state=str(P2S.relative_to(ROOT)), profile=0, results=res)

out = dict(
    meta=dict(game='SSX 3 PS2 NTSC-U (SLUS_207.72)', generator='extract_shop.py', conventions='addresses are EE virtual addresses; R = save record 0x4A6CA8 + profile*0x9B50 + char*0xF88; prices in $ as shown in game',
              riders={i: n for i, n in enumerate(RIDERS)}),
    save_record_fields={'+0x278': 'u64 lock bits (bit12 peak2 pass, bit13 peak3 pass, 6+peak/9+peak rival, 14+peak/17+peak peak events)', '+0x288..+0x290..': 'gear inventory (see gear.inventory)',
                        '+0xAC4': 's32 cash', '+0xAC8': 'lifetime earnings', '+0xAD0+slot*8': 'event records {u8 played, s8 best medal, u16 gold-or-better count, ...}',
                        '+0xBB8..0xBBF': '8 stat-medal tiers (0..3)', '+0xBC0': 'character id', '+0xBE6..0xDE4': 'uber trick state (+cat*6)',
                        '+0xF28': 'award bits 0..31', '+0xF30': 'posters', '+0xF36': 'cards', '+0xF45': 'art', '+0xF52': 'videos', '+0xF53': 'toys', '+0xF57': 'cheat characters (bit charId-10)',
                        '+0xF70': 'u64 songs owned', '+0xF78': 'u64 songs in playlist', '+0xF80': 'music mode'},
    awards=award_section, gear=gear_section, uber_tricks=uber_section, songs=song_section, savestate_validation=state_check)
OUT.write_text(json.dumps(out, indent=1, ensure_ascii=False))
print('wrote', OUT)
for k, v in validation.items():
    print('frame check', k, v['ok'])
if state_check:
    for k, v in state_check['results'].items():
        print('state', k, v)
