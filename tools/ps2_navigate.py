#!/usr/bin/env python3
"""Headless front-end driver for the original game in ARMSX2 (development reference only).

`build` derives a savestate with one guarded hook in the pad DEVICE reader 0x326EB0 (the
vtable+0x14 method of the pad object, 0x48E488).  At 0x327208, right after scePadRead has
filled the 32-byte packet at sp+0 (status, id, buttons, sticks, 12 pressure bytes) and set
s3 = 1 for a valid read, the hook overwrites packet bytes sp+2..sp+0x13 of port 0 / slot 0
with a scripted 18-byte PS2 pad frame.  Everything after it (0x327210 conversion into the
24 channels, the 30-entry device ring at *(gp-0x850), the front end, the in-race button
history 0x321298 and INPUT.MAP) is the original code, so the same script drives menus,
loading screens, the race overlay and the rider.

The script is indexed by the number of valid port-0 reads seen by the hook ("samples",
one per vsync in practice); after the last segment the pad is neutral.  Probe arena
0xF0000..0x100000 (inside ps2_capture's free 0x96000..0x100000 range, clear of its code,
script and ring).

`run` launches ARMSX2 headless (unique PINE slot and data path per run), requests PINE
savestates (MsgSaveState) at sample counts: periodic screenshots (--snap-every) and named
states (script "save" marks / --save NAME@SAMPLE).  Each named state is CLEANED before it
is written: the hook bytes are restored to the original instructions and the arena is
zeroed (guarded patch), so the result is an ordinary savestate of the unmodified game at
that moment (usable as a tools/ps2_capture.py baseline).  Original discs and existing
reference savestates are never modified.

  python3 tools/ps2_navigate.py build BASE.p2s SCRIPT.json OUT.p2s
  python3 tools/ps2_navigate.py run OUT.p2s OUTDIR --frames 900 [--snap-every 60] [--save name@600]
  python3 tools/ps2_navigate.py inspect STATE.p2s [--png OUT.png]

Script: {"segments": [{"frames": 20}, {"frames": 8, "buttons": ["DPadDown"]},
          {"frames": 30, "save": "menu-a"}, {"frames": 8, "buttons": ["Cross"], "lx": 0.0}]}
A segment's "save" requests a named savestate when the sample count reaches its END.
"""
import argparse, hashlib, json, os, shutil, struct, subprocess, sys, time, zipfile, random
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from reference_replay import patch_state, pad_frame  # noqa: E402
from ps2_capture import Asm, Pine, prepare_datapath, axis_byte, PCSX2, ISO, DATAPATH, GP, BUTTONS, fpu_mode  # noqa: E402
from ps2_capture import T0, T1, T2, T3, T4, T5, ZERO, SP  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
NAV_CODE = 0xF0000
NAV_DATA = 0xF0800       # u32 enabled, samples, calls, last_packet[20]
NAV_SCRIPT = 0xF1000     # u32 count, pad x3, entries {u32 stop, 18 bytes frame, 2 pad} = 24 bytes
NAV_END = 0x100000
ENTRY = 24
D_ENABLED, D_SAMPLES, D_CALLS, D_LAST = 0, 4, 8, 12
HOOK = 0x327208
HOOK_BYTES = struct.pack('<2I', 0x1260012C, 0x93A20002)   # beqz s3,0x3276BC / lbu v0,2(sp)
S0, S3, V0 = 16, 19, 2
# ps2_capture button names -> reference_replay (P2M2) names
PAD_NAMES = {'Select': 'Select', 'Start': 'Start', 'L3': 'L3', 'R3': 'R3', 'DPadRight': 'Right', 'DPadLeft': 'Left',
             'DPadUp': 'Up', 'DPadDown': 'Down', 'Triangle': 'Triangle', 'Circle': 'Circle', 'Cross': 'Cross',
             'Square': 'Square', 'L1': 'L1', 'R1': 'R1', 'L2': 'L2', 'R2': 'R2'}


def raw_frame(segment):
    names = segment.get('buttons', [])
    for n in names:
        if n not in BUTTONS: raise ValueError(f'Unknown button {n}')
    stick = lambda x, y: (axis_byte(segment.get(x, 0)), axis_byte(-segment.get(y, 0)))
    return pad_frame([PAD_NAMES[n] for n in names], left=stick('lx', 'ly'), right=stick('rx', 'ry'))


def assemble():
    a = Asm(NAV_CODE)
    lbu = lambda rt, off, rs: a.i(36, rt, rs, off)
    sb = lambda rt, off, rs: a.i(40, rt, rs, off)
    a.label('hook')
    a.beq(S3, ZERO, 'pass'); a.nop()                 # scePadRead failed: nothing to replace
    a.lw(T0, 4, S0); a.bne(T0, ZERO, 'pass'); a.nop()   # port 0
    a.lw(T0, 8, S0); a.bne(T0, ZERO, 'pass'); a.nop()   # slot 0
    a.li(T0, NAV_DATA)
    a.lw(T1, D_CALLS, T0); a.addiu(T1, T1, 1); a.sw(T1, D_CALLS, T0)
    a.lw(T1, D_ENABLED, T0); a.beq(T1, ZERO, 'pass'); a.nop()
    a.lw(T2, D_SAMPLES, T0); a.addiu(T3, T2, 1); a.sw(T3, D_SAMPLES, T0)
    a.li(T1, NAV_SCRIPT + 16)
    a.label('scan')
    a.lw(T4, 0, T1); a.sltu(T5, T2, T4); a.bne(T5, ZERO, 'found'); a.nop()
    a.addiu(T1, T1, ENTRY); a.beq(ZERO, ZERO, 'scan'); a.nop()
    a.label('found')
    for k in range(18):
        lbu(T4, 4 + k, T1); sb(T4, 2 + k, SP); sb(T4, D_LAST + k, T0)
    a.label('pass')
    lbu(V0, 2, SP)                                      # relocated delay-slot load (both paths)
    a.beq(S3, ZERO, 'skip'); a.nop()
    a.j(0x327210); a.nop()
    a.label('skip')
    a.j(0x3276BC); a.nop()
    return a.link(), NAV_CODE + 4 * a.labels['hook']


def script_bytes(segments):
    entries = []; end = 0; saves = []
    for s in segments:
        frames = int(s['frames'])
        if frames <= 0: raise ValueError('Segment frame count must be positive')
        end += frames
        entries.append((end, raw_frame(s)))
        if s.get('save'): saves.append((end, s['save']))
    entries.append((0xFFFFFFFF, raw_frame({})))
    out = struct.pack('<4I', len(entries), 0, 0, 0)
    for stop, frame in entries:
        out += struct.pack('<I', stop) + frame + b'\0\0'
    if NAV_SCRIPT + len(out) > NAV_END: raise ValueError('Script exceeds arena')
    return out, end, saves


def verify(memory):
    if memory[HOOK:HOOK + 8] != HOOK_BYTES: raise ValueError('Pad reader hook bytes differ')
    if any(memory[NAV_CODE:NAV_END]): raise ValueError('Navigation arena is not free')
    if struct.unpack_from('<I', memory, 0x48E488 + 0x14)[0] != 0x326EB0:
        raise ValueError('Unexpected pad device vtable')


def build(baseline, script_path, output):
    spec = json.loads(Path(script_path).read_text())
    with zipfile.ZipFile(baseline) as z: memory = z.read('eeMemory.bin')
    verify(memory)
    code, entry = assemble()
    if len(code) > NAV_DATA - NAV_CODE: raise ValueError('Hook code exceeds arena')
    script, frames, saves = script_bytes(spec['segments'])
    data = struct.pack('<4I', 1, 0, 0, 0)
    j = struct.pack('<2I', (2 << 26) | (entry >> 2), 0)
    patches = [dict(address=hex(NAV_CODE), expected='00' * len(code), replacement=code.hex()),
               dict(address=hex(NAV_DATA), expected='00' * len(data), replacement=data.hex()),
               dict(address=hex(NAV_SCRIPT), expected='00' * len(script), replacement=script.hex()),
               dict(address=hex(HOOK), expected=HOOK_BYTES.hex(), replacement=j.hex())]
    output = Path(output)
    patch_state(Path(baseline), output, patches)
    manifest = dict(kind='ps2_navigate', baseline=str(Path(baseline).resolve()),
                    baseline_sha256=hashlib.sha256(Path(baseline).read_bytes()).hexdigest(),
                    script=str(Path(script_path).resolve()), script_samples=frames, segments=spec['segments'],
                    saves=[dict(sample=s, name=n) for s, n in saves], hook=hex(HOOK), code=hex(NAV_CODE), data=hex(NAV_DATA))
    output.with_suffix('.nav.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


def clean_memory_patches(memory):
    """Guarded patches that turn a hooked state back into the unmodified game."""
    j = memory[HOOK:HOOK + 8]
    if j == HOOK_BYTES: patches = []
    elif struct.unpack_from('<I', j)[0] >> 26 == 2 and ((struct.unpack_from('<I', j)[0] & 0x3FFFFFF) << 2) == NAV_CODE:
        patches = [dict(address=hex(HOOK), expected=j.hex(), replacement=HOOK_BYTES.hex())]
    else: raise ValueError('Unexpected bytes at the pad hook')
    arena = memory[NAV_CODE:NAV_END]
    end = len(arena.rstrip(b'\0'))
    if end: patches.append(dict(address=hex(NAV_CODE), expected=arena[:end].hex(), replacement='00' * end))
    return patches


def game_tick(memory):
    u = lambda a: struct.unpack_from('<I', memory, a & 0x1FFFFFF)[0]
    try:
        p = u(GP - 0x848)
        for off in (0x84, 0x0C):
            p = u(p + off)
            if not p: return None
        return u(p + 8)
    except struct.error: return None


def describe(memory):
    """Front-end / event facts read from EE memory (location, race clock, riders)."""
    u = lambda a: struct.unpack_from('<I', memory, a & 0x1FFFFFF)[0]
    elf = (ROOT / 'local/disc/SLUS_207.72').read_bytes()
    ph = struct.unpack_from('<I', elf, 28)[0]; stride, count = struct.unpack_from('<HH', elf, 42)
    def eread(address, size):
        for i in range(count):
            typ, offset, base, _, length, _, _, _ = struct.unpack_from('<8I', elf, ph + i * stride)
            if typ == 1 and base <= address and address + size <= base + length:
                return elf[offset + address - base:offset + address - base + size]
    names = {}
    for i in range(50):
        e = eread(0x43E250 + 24 * i, 24); names[struct.unpack_from('<I', e)[0]] = e[4:20].split(b'\0')[0].decode()
    streaming = []
    for i in range(50):
        ident, track, state, kind = struct.unpack_from('<4i', memory, 0x442168 + 16 * i)
        if state == 2: streaming.append(dict(id=ident, name=names.get(ident, '?'), track=track, kind=kind))
    info = dict(tick=game_tick(memory), resident_locations=streaming,
                nav_samples=struct.unpack_from('<I', memory, NAV_DATA + D_SAMPLES)[0])
    try:
        from reference_race_event import extract_race_event
        event = extract_race_event(memory)
        info['race'] = dict(clock=event['clock'], participants=[
            dict(index=p['index'], human=p['human'], motion=p['motion_mode'], control=p['control_state'],
                 position=[round(v, 1) for v in p['position']]) for p in event['participants']])
    except Exception as error:  # not in an event (front end / loading)
        info['race'] = f'unavailable: {error}'
    riders = []
    for at in range(0x1000000, 0x2000000 - 0x800, 0x10):
        v = struct.unpack_from('<2I', memory, at + 0x6C0)[0], struct.unpack_from('<I', memory, at + 0x6E8)[0]
        if v in ((0x4583A8, 0x458360), (0x458660, 0x458618)):
            riders.append(dict(address=hex(at), human=v[0] == 0x4583A8))
    info['rider_objects'] = riders
    return info


def run(state, outdir, frames, speed='normal', timeout=900, snap_every=0, saves=(), keep_snap_states=False, tick_saves=()):
    state = Path(state).resolve(); outdir = Path(outdir); outdir.mkdir(parents=True, exist_ok=True)
    manifest_path = state.with_suffix('.nav.json')
    manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else {}
    requests = [(s['sample'], s['name']) for s in manifest.get('saves', [])] + list(saves)
    if snap_every: requests += [(k, None) for k in range(snap_every, frames + 1, snap_every)]
    requests.append((frames, 'final'))
    requests = sorted(set(requests), key=lambda r: (r[0], r[1] or ''))
    slot = random.randint(28100, 28999)
    datapath = DATAPATH.parent / f'pcsx2-nav-{slot}'
    # The arithmetic profile: PS2_CAPTURE_FPU (mode1 | exact, default mode1), recorded in navigate.json (docs/ps2-float.md).
    fpu = fpu_mode(manifest)
    prepare_datapath(datapath, slot, fpu)
    args = [str(PCSX2), '-datapath', str(datapath), '-batch', '-nogui', '-statefile', str(state)]
    if speed == 'unlimited': args.append('-unlimited')
    elif speed == 'turbo': args.append('-turbo')
    args += ['--', str(ISO)]
    log = open(outdir / 'navigate.pcsx2.log', 'w')
    proc = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT)
    started = time.time(); pine = None; results = []; pending = list(requests); inflight = []; samples = 0
    tick_saves = list(tick_saves); collected = []; rawdir = outdir / '.raw'; rawdir.mkdir(exist_ok=True)
    try:
        while time.time() - started < timeout:
            if proc.poll() is not None: raise RuntimeError(f'ARMSX2 exited early ({proc.returncode})')
            if pine is None:
                try: pine = Pine(slot)
                except OSError: time.sleep(0.5); continue
            try:
                samples, = struct.unpack('<I', pine.read(NAV_DATA + D_SAMPLES, 4))
                tick = None
                if tick_saves:
                    p = struct.unpack('<I', pine.read(GP - 0x848, 4))[0]
                    for off in (0x84, 0x0C, 8):
                        if not 0 < p < 0x2000000: p = None; break
                        p = struct.unpack('<I', pine.read(p + off, 4))[0]
                    tick = p
            except (OSError, RuntimeError, ConnectionError): time.sleep(0.2); continue
            for item in list(tick_saves):
                target, name = item
                if tick is not None and samples > 0 and target <= tick <= target + 5:
                    pending.insert(0, (samples, name)); tick_saves.remove(item)
            while pending and samples >= pending[0][0] and len(inflight) < 8:
                want, name = pending.pop(0)
                used = {k for k, *_ in inflight}
                k = next(n for n in range(1, 10) if n not in used)
                for old in datapath.rglob(f'*.{k:02d}.p2s*'): old.unlink()
                pine.request(b'\x09' + bytes([k])); inflight.append((k, want, name, samples, time.time()))
            for item in list(inflight):
                k, want, name, asked, t0 = item
                files = [f for f in datapath.rglob(f'*.{k:02d}.p2s') if f.is_file()]
                if not files:
                    if time.time() - t0 > 60: raise RuntimeError(f'Savestate slot {k} never appeared')
                    continue
                try:   # only the central directory: cheap, so polling keeps up with the emulator
                    with zipfile.ZipFile(files[0]) as z:
                        if not {'eeMemory.bin', 'Screenshot.png'} <= set(z.namelist()): continue
                except (zipfile.BadZipFile, OSError): continue   # still being written
                raw = rawdir / f'{len(collected):04d}.p2s'; files[0].rename(raw)
                collected.append((want, name, raw)); inflight.remove(item)
            if not pending and not inflight and (not tick_saves or samples >= frames): break
            time.sleep(0.02)
        else:
            raise RuntimeError(f'Navigation timed out at sample {samples}')
    finally:
        if pine: pine.close()
        proc.terminate()
        try: proc.wait(10)
        except subprocess.TimeoutExpired: proc.kill()
        log.close()
        shutil.rmtree(datapath, ignore_errors=True)
    for want, name, raw in collected:   # post-process after the emulator has stopped
        with zipfile.ZipFile(raw) as z: memory = z.read('eeMemory.bin'); png = z.read('Screenshot.png')
        got = struct.unpack_from('<I', memory, NAV_DATA + D_SAMPLES)[0]
        tick = game_tick(memory)
        label = name or f'sample{got:05d}'
        (outdir / f'{label}.png').write_bytes(png)
        entry = dict(requested=want, sample=got, tick=tick, png=str(outdir / f'{label}.png'))
        if name or keep_snap_states:
            target = outdir / f'{label}.p2s'
            patch_state(raw, target, clean_memory_patches(memory))
            entry['state'] = str(target)
        results.append(entry); raw.unlink()
        print(json.dumps(entry), flush=True)
    shutil.rmtree(rawdir, ignore_errors=True)
    summary = dict(state=str(state), manifest=manifest, frames=frames, results=results, fpu_mode=fpu)
    (outdir / 'navigate.json').write_text(json.dumps(summary, indent=2) + '\n')
    return summary


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest='cmd', required=True)
    b = sub.add_parser('build'); b.add_argument('baseline'); b.add_argument('script'); b.add_argument('output')
    r = sub.add_parser('run'); r.add_argument('state'); r.add_argument('outdir'); r.add_argument('--frames', type=int, required=True)
    r.add_argument('--speed', default='normal', choices=['normal', 'turbo', 'unlimited']); r.add_argument('--timeout', type=int, default=900)
    r.add_argument('--snap-every', type=int, default=0, help='screenshot every N samples')
    r.add_argument('--save', action='append', default=[], help='NAME@SAMPLE (or NAME@tTICK: when the game tick first reads TICK..TICK+5): save a cleaned savestate')
    r.add_argument('--keep-snap-states', action='store_true', help='also keep cleaned savestates for periodic snaps')
    i = sub.add_parser('inspect'); i.add_argument('state'); i.add_argument('--png')
    args = p.parse_args()
    if args.cmd == 'build': print(json.dumps(build(args.baseline, args.script, args.output), indent=2))
    elif args.cmd == 'run':
        saves = [(int(s.split('@')[1]), s.split('@')[0]) for s in args.save if not s.split('@')[1].startswith('t')]
        tick_saves = [(int(s.split('@')[1][1:]), s.split('@')[0]) for s in args.save if s.split('@')[1].startswith('t')]
        summary = run(args.state, args.outdir, args.frames, args.speed, args.timeout, args.snap_every, saves, args.keep_snap_states, tick_saves)
        print(json.dumps(dict(results=len(summary['results'])), indent=2))
    else:
        with zipfile.ZipFile(args.state) as z:
            memory = z.read('eeMemory.bin')
            if args.png: Path(args.png).write_bytes(z.read('Screenshot.png'))
        print(json.dumps(describe(memory), indent=2))


if __name__ == '__main__':
    main()
