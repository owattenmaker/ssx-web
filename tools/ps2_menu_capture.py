#!/usr/bin/env python3
"""Scripted front-end (menu) pad input + PS2 screenshots through ARMSX2 (development reference only).

tools/ps2_capture.py drives only the in-race human rider pad.  Front-end menus read the
same per-port button records, which every live pad sample updates through the consumed
pad-sample history 0x321298 (called from 0x227E98, the per-poll device-ring drain, as
`0x321298(padRecord, channelCount, float values[24])` for port 0 then port 1).  This tool
hooks 0x321298's entry and, for the port-0 call from 0x227E98 (ra == 0x227F28, s1 == 1),
replaces the 24 decoded channels with a scripted sample chosen by the number of port-0
samples consumed so far.  Everything downstream (edge/repeat history, menus) is original.

The hook also keeps a small table of every distinct (ra, a0, a1, s1) caller with counts.

Script JSON (list, {"events": [...], "snaps": [...], "frames": N}, or a NAME.session.json):
  {"frame": 30, "press": "Cross"}                 held 6 samples (default) then released
  {"frame": 30, "press": ["L1", "R1"], "hold": 20} chord
  {"frame": 90, "hold": 300, "buttons": ["Cross"], "lx": 0, "ly": 1}  sticks in [-1, 1]
Frames are consumed port-0 pad samples (~60 Hz while the game polls the pad).

  ps2_menu_capture.py session BASE.p2s NAME &        # interactive exploration (keeps ARMSX2 running)
  ps2_menu_capture.py send NAME "press Down; wait 30; press Cross; wait 90; snap NAME/menu; state NAME/menu-state"
      commands: press A[+B] [hold] | hold A+B|none N [lx ly rx ry] | wait N | sync | snap LABEL | state LABEL | stats | quit
      Presses are queued after the previous one (+12 idle samples); `wait N` advances the cursor; snap/state wait
      for the cursor.  Every input is logged with its absolute sample number in NAME.session.json, which `run`
      replays deterministically (menus replay pixel-identical).
  ps2_menu_capture.py run BASE.p2s SCRIPT.json NAME [--snap 10,40,...] [--frames N] [--keep-final]
writes local/ps2-capture/menus/NAME.fNNNNN.png (Screenshot.png of a PINE savestate), with
NAME.json listing the script, derived-state provenance and screenshots.  --keep-final
also stores NAME.final.p2s, which can be the BASE of a later run (the hook is reinstalled
with a fresh counter and script).  Original savestates are never modified.
MEMCARD=card.ps2 [MEMCARD_OUT=out.ps2]: a memory card in slot 1 (a missing file = an unformatted card); the card is
kept in MEMCARD_OUT when the run ends.
"""
import argparse, hashlib, json, os, random, shutil, struct, subprocess, sys, time, zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ps2_capture import (Asm, Pine, prepare_datapath, decode_pad, BUTTONS, PCSX2, ISO,  # noqa: E402
                         T0, T1, T2, T3, T4, T5, T6, T7, A0, A1, A2, S0, S1, SP, ZERO)

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'local/ps2-capture/menus'
CODE = 0x96000
DATA = 0x97000          # 0 enabled, 4 port-0 sample index, 8 total calls, 16 active table entry 0, 32.. caller table
STATS, STAT_SLOTS, STAT_SIZE = 32, 8, 20   # (ra, a0, a1, s1, count)
SCRIPT = 0x97100
SCRIPT_END = 0xA0000
BUFFERS = (0x97100, 0x9B880)   # session double buffer, 0x4780 bytes (~160 entries) each
PAD_HOOK = 0x321298
PAD_BYTES = struct.pack('<2I', 0x27BDFFC0, 0x7FB00030)   # addiu sp,sp,-0x40 / sq s0,0x30(sp)
DRAIN_RA, DRAIN_PORT0_S1 = 0x227F28, 1
RA = 31
ALIASES = {'Up': 'DPadUp', 'Down': 'DPadDown', 'Left': 'DPadLeft', 'Right': 'DPadRight', 'X': 'Cross'}


def assemble():
    a = Asm(CODE)
    a.label('pad')
    a.li(T0, DATA)
    a.lw(T1, 0, T0); a.beq(T1, ZERO, 'done'); a.nop()
    a.lw(T1, 8, T0); a.addiu(T1, T1, 1); a.sw(T1, 8, T0)
    # caller statistics: find (ra, a0, a1, s1) or the first empty slot
    a.addiu(T2, T0, STATS); a.addiu(T3, ZERO, STAT_SLOTS)
    a.label('stat_scan')
    a.lw(T4, 16, T2); a.beq(T4, ZERO, 'stat_new'); a.nop()
    a.lw(T5, 0, T2); a.bne(T5, RA, 'stat_next'); a.nop()
    a.lw(T5, 4, T2); a.bne(T5, A0, 'stat_next'); a.nop()
    a.lw(T5, 8, T2); a.bne(T5, A1, 'stat_next'); a.nop()
    a.lw(T5, 12, T2); a.bne(T5, S1, 'stat_next'); a.nop()
    a.addiu(T4, T4, 1); a.sw(T4, 16, T2); a.beq(ZERO, ZERO, 'stat_done'); a.nop()
    a.label('stat_next')
    a.addiu(T2, T2, STAT_SIZE); a.addiu(T3, T3, -1); a.bne(T3, ZERO, 'stat_scan'); a.nop()
    a.beq(ZERO, ZERO, 'stat_done'); a.nop()
    a.label('stat_new')
    a.sw(RA, 0, T2); a.sw(A0, 4, T2); a.sw(A1, 8, T2); a.sw(S1, 12, T2); a.addiu(T4, ZERO, 1); a.sw(T4, 16, T2)
    a.label('stat_done')
    # scripted sample only for the live drain's port-0 call
    a.li(T1, DRAIN_RA); a.bne(RA, T1, 'done'); a.nop()
    a.addiu(T1, ZERO, DRAIN_PORT0_S1); a.bne(S1, T1, 'done'); a.nop()
    a.lw(T6, 4, T0); a.addiu(T7, T6, 1); a.sw(T7, 4, T0)
    a.lw(T1, 16, T0)                                    # active table (+16), double-buffered by session
    a.label('scan')
    a.lw(T4, 0, T1); a.sltu(T5, T6, T4); a.bne(T5, ZERO, 'found'); a.nop()
    a.addiu(T1, T1, 112); a.beq(ZERO, ZERO, 'scan'); a.nop()
    a.label('found')
    for k in range(24):
        a.lw(T4, 16 + 4 * k, T1); a.sw(T4, 4 * k, A2)
    a.label('done')
    a.addiu(SP, SP, -0x40); a.sq(S0, 0x30, SP)          # relocated 0x321298/0x32129C
    a.j(PAD_HOOK + 8); a.nop()
    return a.link(), CODE + 4 * a.labels['pad']


def button(name):
    name = ALIASES.get(name, name)
    if name not in BUTTONS: raise ValueError(f'Unknown button {name}')
    return name


def events_to_segments(events, limit=SCRIPT_END - SCRIPT):
    """Frame-indexed press events -> (stop, 24 floats) table; later events override earlier on overlap."""
    samples = {}
    end = 0
    for e in events:
        start = int(e['frame'])
        if 'press' in e:
            names = e['press'] if isinstance(e['press'], list) else [e['press']]
            length = int(e.get('hold', 6)); sticks = (0, 0, 0, 0)
        else:
            names = e.get('buttons', []); length = int(e['hold'])
            sticks = (e.get('lx', 0), e.get('ly', 0), e.get('rx', 0), e.get('ry', 0))
        names = [button(n) for n in names]
        for f in range(start, start + length):
            samples[f] = (tuple(sorted(names)), sticks)
        end = max(end, start + length)
    table = []
    neutral = ((), (0, 0, 0, 0))
    current = samples.get(0, neutral)
    for f in range(1, end + 1):
        s = samples.get(f, neutral)
        if s != current: table.append((f, current)); current = s
    table.append((0xFFFFFFFF, neutral))
    out = struct.pack('<4I', len(table), 0, 0, 0)
    for stop, (names, (lx, ly, rx, ry)) in table:
        out += struct.pack('<4I', stop, 0, 0, 0) + struct.pack('<24f', *decode_pad(names, lx, ly, rx, ry))
    if len(out) > limit: raise ValueError('Script exceeds arena')
    return out, end


def derive(baseline, events, output):
    code, entry = assemble()
    with zipfile.ZipFile(baseline) as archive:
        memory = bytearray(archive.read('eeMemory.bin'))
        jump = struct.pack('<2I', (2 << 26) | (entry >> 2), 0)
        hooked = memory[PAD_HOOK:PAD_HOOK + 8]
        if hooked not in (PAD_BYTES, jump): raise ValueError('0x321298 is neither original nor this tool\'s hook')
        if memory[CODE:CODE + len(code)] not in (bytes(len(code)), code): raise ValueError('Code arena is not free')
        if hooked == PAD_BYTES and any(memory[DATA:SCRIPT_END]): raise ValueError('Data arena is not free')
        script, end = events_to_segments(events)
        memory[DATA:SCRIPT_END] = bytes(SCRIPT_END - DATA)          # fresh counter/statistics/script
        memory[CODE:CODE + len(code)] = code
        memory[SCRIPT:SCRIPT + len(script)] = script
        memory[DATA:DATA + 4] = struct.pack('<I', 1)
        memory[DATA + 16:DATA + 20] = struct.pack('<I', SCRIPT + 16)
        memory[PAD_HOOK:PAD_HOOK + 8] = jump
        output.parent.mkdir(parents=True, exist_ok=True)
        with zipfile.ZipFile(output, 'w', compression=zipfile.ZIP_DEFLATED) as result:
            for info in archive.infolist():
                result.writestr(info.filename, bytes(memory) if info.filename == 'eeMemory.bin' else archive.read(info.filename))
    return end


def _wait_state(datapath, known, timeout=60):
    deadline = time.time() + timeout
    while time.time() < deadline:
        for f in datapath.rglob('*.p2s'):
            if f in known: continue
            try:
                size = f.stat().st_size; time.sleep(0.3)
                if size != f.stat().st_size or size == 0: continue
                with zipfile.ZipFile(f) as z: z.read('Screenshot.png'); z.read('eeMemory.bin')
                return f
            except (zipfile.BadZipFile, KeyError, OSError): time.sleep(0.2)
        time.sleep(0.2)
    raise RuntimeError('Savestate was not written')


def read_stats(pine):
    raw = pine.read(DATA, STATS + STAT_SLOTS * STAT_SIZE)
    enabled, index, calls = struct.unpack_from('<3I', raw, 0)
    table = []
    for k in range(STAT_SLOTS):
        ra, a0, a1, s1, count = struct.unpack_from('<5I', raw, STATS + k * STAT_SIZE)
        if count: table.append(dict(ra=hex(ra), a0=hex(a0), a1=a1, s1=s1, count=count))
    return dict(index=index, calls=calls, callers=table)


def rel(path):
    path = Path(path).resolve()
    return str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path)


class Emulator:
    """ARMSX2 on a derived copy of BASELINE with its own PINE slot and data path."""

    def __init__(self, baseline, events, name, speed='normal'):
        self.name = name; self.baseline = Path(baseline)
        (OUT / name).parent.mkdir(parents=True, exist_ok=True)
        self.slot = random.randint(29100, 29899)
        self.datapath = ROOT / f'local/ps2-capture/pcsx2-menu-{self.slot}'
        self.derived = self.datapath.with_name(self.datapath.name + '.state.p2s')
        self.end = derive(self.baseline, events, self.derived)
        prepare_datapath(self.datapath, self.slot)
        if os.environ.get('MEMCARD'):   # a memory card in slot 1: a copy of MEMCARD, or (no such file) an unformatted one ARMSX2 makes
            import re
            card = self.datapath / 'memorycards/Mcd001.ps2'; card.parent.mkdir(parents=True, exist_ok=True)
            if Path(os.environ['MEMCARD']).exists(): shutil.copy(os.environ['MEMCARD'], card)
            for ini in (self.datapath / 'inis/PCSX2.ini', self.datapath / 'ARMSX2/inis/PCSX2.ini'):
                if not ini.exists(): continue
                t = ini.read_text(); t = re.sub(r'(?m)^Slot1_Enable = .*$', 'Slot1_Enable = true', t)
                t = re.sub(r'(?m)^MemoryCards = .*$', f'MemoryCards = {card.parent}', t); ini.write_text(t)
        args = [str(PCSX2), '-datapath', str(self.datapath), '-batch', '-nogui', '-statefile', str(self.derived.resolve())]
        if speed == 'unlimited': args.append('-unlimited')
        elif speed == 'turbo': args.append('-turbo')
        args += ['--', str(ISO)]
        self.log = open(OUT / f'{name}.pcsx2.log', 'w')
        self.proc = subprocess.Popen(args, stdout=self.log, stderr=subprocess.STDOUT)
        self.pine = None; self.buffer = 0; self.last = (None, time.time())
        deadline = time.time() + 60
        while True:
            if self.proc.poll() is not None: raise RuntimeError(f'ARMSX2 exited early ({self.proc.returncode})')
            if time.time() > deadline: raise RuntimeError('PINE never answered')
            try:
                if self.pine is None: self.pine = Pine(self.slot)
                self.stats(); break
            except (OSError, RuntimeError, ConnectionError): time.sleep(0.3)

    def stats(self):
        s = read_stats(self.pine)
        if s['index'] != self.last[0]: self.last = (s['index'], time.time())
        return s

    def index(self, stall=30):
        if self.proc.poll() is not None: raise RuntimeError(f'ARMSX2 exited early ({self.proc.returncode})')
        s = self.stats()
        if time.time() - self.last[1] > stall: raise RuntimeError(f'Pad sampling stalled at {s}')
        return s['index']

    def wait_until(self, frame, timeout=600):
        deadline = time.time() + timeout
        while True:
            try: i = self.index()
            except (OSError, ConnectionError): time.sleep(0.2); continue
            if i >= frame: return i
            if time.time() > deadline: raise RuntimeError(f'Timed out waiting for sample {frame} (at {i})')
            time.sleep(0.01)

    def set_events(self, events):
        """Double-buffered live table swap: write the idle buffer, then flip DATA+16 in one word."""
        self.buffer ^= 1
        base = BUFFERS[self.buffer]
        data, _ = events_to_segments(events, BUFFERS[1] - BUFFERS[0])
        data += bytes((-len(data)) % 8)
        cmd = b''.join(b'\x07' + struct.pack('<I', base + k) + data[k:k + 8] for k in range(0, len(data), 8))
        self.pine.request(cmd)
        self.pine.request(b'\x06' + struct.pack('<II', DATA + 16, base + 16))

    def save(self, label, keep_state=False):
        known = set(self.datapath.rglob('*.p2s'))
        for f in known: f.unlink()
        frame = self.stats()['index']
        # ARMSX2 refuses a savestate while the memory card is busy (a format / save in progress): ask again until one is written
        for attempt in range(40):
            self.pine.request(b'\x09' + bytes([1]))
            try: f = _wait_state(self.datapath, set(), timeout=60 if attempt == 39 else 6); break
            except RuntimeError:
                if attempt == 39: raise
        with zipfile.ZipFile(f) as z:
            png = OUT / f'{label}.png'; png.parent.mkdir(parents=True, exist_ok=True); png.write_bytes(z.read('Screenshot.png'))
            memory = z.read('eeMemory.bin'); saved_frame, = struct.unpack_from('<I', memory, DATA + 4)
        entry = dict(label=label, frame=saved_frame, requested_at=frame, png=rel(png))
        if keep_state:
            kept = OUT / f'{label}.p2s'; shutil.move(str(f), kept); entry['state'] = rel(kept)
        else: f.unlink()
        return entry

    def close(self):
        if self.pine: self.pine.close()
        self.proc.terminate()
        try: self.proc.wait(10)
        except subprocess.TimeoutExpired: self.proc.kill()
        self.log.close()
        card = self.datapath / 'memorycards/Mcd001.ps2'   # MEMCARD_OUT: keep the card as the run left it
        if os.environ.get('MEMCARD_OUT') and card.exists(): shutil.copy(card, os.environ['MEMCARD_OUT'])
        shutil.rmtree(self.datapath, ignore_errors=True)
        self.derived.unlink(missing_ok=True)


def run(baseline, script_path, name, snaps=(), frames=None, keep_final=False, keep_snap_states=False, timeout=600, speed='normal'):
    spec = json.loads(Path(script_path).read_text())
    if isinstance(spec, list): spec = dict(events=spec)
    events = spec.get('events', spec.get('script', []))            # NAME.session.json replays as-is
    if not snaps and not spec.get('snaps') and spec.get('shots'):   # default: re-take the session's snapshots
        snaps = [s['frame'] for s in spec['shots']]
    snaps = sorted(set(list(snaps) + list(spec.get('snaps', []))))
    started = time.time(); shots = []
    emu = Emulator(baseline, events, name, speed)
    frames = frames or spec.get('frames') or max([emu.end + 30] + [s + 1 for s in snaps])
    try:
        for s in snaps:
            emu.wait_until(s, timeout - (time.time() - started))
            shots.append(emu.save(f'{name}.f{s:05d}', keep_snap_states))
        emu.wait_until(frames, timeout - (time.time() - started))
        if keep_final: shots.append(emu.save(f'{name}.final', True))
        stats = emu.stats()
    finally:
        emu.close()
    summary = dict(baseline=rel(baseline), baseline_sha256=hashlib.sha256(Path(baseline).read_bytes()).hexdigest(),
                   script=events, frames=frames, snaps=snaps, shots=shots,
                   hook=dict(address=hex(PAD_HOOK), drain_ra=hex(DRAIN_RA), stats=stats), seconds=round(time.time() - started, 1))
    (OUT / f'{name}.json').write_text(json.dumps(summary, indent=2) + '\n')
    return summary


# ---------------- interactive session (exploration; every input is logged as a replayable run script) ----------------
def socket_path(name):
    return Path(os.environ.get('TMPDIR', '/tmp')) / f'ssx3-menu-{name}.sock'


def session(baseline, name, speed='normal', release_gap=12):
    import socket as _socket
    emu = Emulator(baseline, [], name, speed)
    events, shots, cursor = [], [], 0
    log_path = OUT / f'{name}.session.json'

    def write_log():
        log_path.write_text(json.dumps(dict(baseline=rel(baseline), baseline_sha256=hashlib.sha256(Path(baseline).read_bytes()).hexdigest(),
                                            script=events, shots=shots), indent=2) + '\n')

    def command(words):
        nonlocal cursor
        op = words[0]; now = emu.index()
        if op in ('press', 'hold'):                      # press A+B [hold] | hold A+B|none N [lx ly rx ry]
            names = [] if words[1] == 'none' else words[1].split('+')
            hold = int(words[2]) if len(words) > 2 else 6
            start = max(cursor, now + 3)
            e = dict(frame=start, press=names, hold=hold) if op == 'press' else dict(frame=start, hold=hold, buttons=names)
            if op == 'hold' and len(words) > 3: e.update(zip(('lx', 'ly', 'rx', 'ry'), map(float, words[3:7])))
            events.append(e); cursor = start + hold + (release_gap if op == 'press' else 0)
            live = [x for x in events if x['frame'] + x['hold'] > now]
            emu.set_events(live); write_log()
            return dict(scheduled=e, now=now)
        if op == 'wait':
            cursor = max(cursor, now) + int(words[1]); return dict(cursor=cursor, now=now)
        if op == 'sync':
            return dict(now=emu.wait_until(cursor))
        if op in ('snap', 'state'):
            emu.wait_until(cursor)
            shot = emu.save(f'{words[1]}', op == 'state'); shot['script_events'] = len(events)
            shots.append(shot); write_log(); return shot
        if op == 'stats': return emu.stats()
        raise ValueError(f'unknown command {op}')

    path = socket_path(name); path.unlink(missing_ok=True)
    server = _socket.socket(_socket.AF_UNIX, _socket.SOCK_STREAM); server.bind(str(path)); server.listen(1)
    print(json.dumps(dict(session=name, socket=str(path), slot=emu.slot)), flush=True)
    try:
        while True:
            conn, _ = server.accept()
            with conn:
                text = b''
                while not text.endswith(b'\n'):
                    part = conn.recv(65536)
                    if not part: break
                    text += part
                replies = []; stop = False
                for line in text.decode().replace(';', '\n').splitlines():
                    words = line.split()
                    if not words: continue
                    if words[0] == 'quit': stop = True; break
                    try: replies.append(dict(cmd=line.strip(), **command(words)))
                    except Exception as error: replies.append(dict(cmd=line.strip(), error=str(error))); break
                conn.sendall(json.dumps(replies).encode() + b'\n')
                if stop: break
    finally:
        server.close(); path.unlink(missing_ok=True); write_log(); emu.close()


def send(name, text):
    import socket as _socket
    s = _socket.socket(_socket.AF_UNIX, _socket.SOCK_STREAM); s.connect(str(socket_path(name)))
    s.sendall(text.encode() + b'\n'); data = b''
    while not data.endswith(b'\n'):
        part = s.recv(65536)
        if not part: break
        data += part
    return data.decode()


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest='cmd', required=True)
    r = sub.add_parser('run'); r.add_argument('baseline'); r.add_argument('script'); r.add_argument('name')
    r.add_argument('--snap', default='', help='comma-separated port-0 sample counts at which to save a screenshot')
    r.add_argument('--frames', type=int); r.add_argument('--timeout', type=int, default=600)
    r.add_argument('--keep-final', action='store_true', help='save NAME.final.p2s (chainable BASE) when the run ends')
    r.add_argument('--keep-snap-states', action='store_true', help='keep every snapshot savestate as NAME.fNNNNN.p2s')
    r.add_argument('--speed', default='normal', choices=['normal', 'turbo', 'unlimited'])
    se = sub.add_parser('session', help='keep ARMSX2 running and accept commands from `send`')
    se.add_argument('baseline'); se.add_argument('name'); se.add_argument('--speed', default='normal', choices=['normal', 'turbo', 'unlimited'])
    sd = sub.add_parser('send', help="commands: press A[+B] [hold]; hold A+B|none N [lx ly rx ry]; wait N; sync; snap NAME; state NAME; stats; quit")
    sd.add_argument('name'); sd.add_argument('commands')
    s = sub.add_parser('script', help='print the sample table built from a script (no emulator)'); s.add_argument('script')
    args = p.parse_args()
    if args.cmd == 'run':
        snaps = [int(x) for x in args.snap.split(',') if x]
        print(json.dumps(run(args.baseline, args.script, args.name, snaps, args.frames, args.keep_final, args.keep_snap_states, args.timeout, args.speed), indent=2))
    elif args.cmd == 'session': session(args.baseline, args.name, args.speed)
    elif args.cmd == 'send': print(send(args.name, args.commands))
    else:
        spec = json.loads(Path(args.script).read_text()); spec = spec if isinstance(spec, list) else spec.get('events', [])
        data, end = events_to_segments(spec); count, = struct.unpack_from('<I', data, 0)
        for k in range(count):
            stop, = struct.unpack_from('<I', data, 16 + 112 * k); vals = struct.unpack_from('<24f', data, 32 + 112 * k)
            print(stop, [BUTTONS[i] for i in range(16) if vals[i] > 0.5])


if __name__ == '__main__':
    main()
