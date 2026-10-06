#!/usr/bin/env python3
"""Closed-loop PS2 capture: an autopilot rides long runs (the All Peak Race / Jam, the Peak 2 Race) on ARMSX2.

tools/ps2_capture.py replays a pad script fixed in advance (open loop). A run of 20+ minutes across the whole mountain
needs steering that reacts to where the ORIGINAL rider really is, so this tool keeps ps2_capture's derived state, record
ring and watches, and only swaps its pad hook for a live one:

* 0x321298 (pad-sample history update, human pad): the 24 decoded channels come from a 256-entry ring in the capture
  arena (LIVE_RING, 112 bytes per entry: u32 tag, 3 pad words, 24 floats), entry (index & 255), where `index` is
  ps2_capture's distinct-game-tick counter (record +28). The entry is used only when its tag equals the index; otherwise
  the last applied channels repeat (LIVE_LAST). The applied tag and a miss counter live in LIVE_STATE, which the build
  adds as a --watch window, so every record says which entry the original consumed on that tick.
* The host (`run`) streams the records to disk as they are written, steers toward the rider's own route lookahead
  (rider+0x4A0: the race path of the path bank the original delivered for the current location, the one 112A50 keeps
  ahead of the rider), tucks, and writes each future entry exactly once (values first, the tag last), LEAD ticks ahead.
* `pads` turns a finished run into an ordinary ps2_capture segments script (the entry the record names, or the previous
  channels on a miss), so web/compare-ps2-capture.mjs replays the run like any other capture.

    python3 tools/ps2_autopilot.py build CARD.p2s OUT.p2s [--watch ADDR:LEN ...]
    python3 tools/ps2_autopilot.py run OUT.p2s OUT.bin --frames N [--jam] [--snap-events]
    python3 tools/ps2_autopilot.py pads OUT.bin
    python3 tools/ps2_autopilot.py run OUT.p2s NEW.bin --frames N --replay RUN.script.json   (open loop: RUN's consumed pads)

`--replay` writes the entry of every index from a finished run's consumed script (`pads`) instead of steering, so a run is
captured again with the same inputs, e.g. under another arithmetic profile (PS2_CAPTURE_FPU=exact, docs/ps2-float.md).

Development reference only: the ISO and the baseline savestates are never modified (derived states only).
"""
import argparse, json, math, os, random, shutil, signal, struct, subprocess, sys, time, zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ps2_capture as cap  # noqa: E402
from reference_replay import patch_state  # noqa: E402

LIVE_CODE = 0x97400      # inside ps2_capture's script arena (0x97100..0xA0000; the build's script is one entry)
LIVE_STATE = 0x97800     # u32 applied tag, misses, calls, spare; LIVE_LAST 24 floats at +0x10
LIVE_LAST = LIVE_STATE + 0x10
LIVE_RING = 0x98000      # 256 x 112 bytes = 0x7000 -> 0x9F000
ENTRY = 112
SLOTS = 256
LEAD = 10                # ticks written ahead of the last record


def assemble_live():
    """Pad hook with the same prologue bookkeeping as ps2_capture's (F_INDEX / F_LASTTICK / F_INITED / F_PADCALLS)."""
    T0, T1, T3, T4, T5, T6, T7 = cap.T0, cap.T1, cap.T3, cap.T4, cap.T5, cap.T6, cap.T7
    a = cap.Asm(LIVE_CODE)
    a.label('pad')
    a.li(T0, cap.DATA)
    a.lw(T1, cap.F_HUMAN_PAD, T0); a.bne(cap.A0, T1, 'done'); a.nop()
    a.lw(T1, cap.F_ENABLED, T0); a.beq(T1, cap.ZERO, 'done'); a.nop()
    cap.tick_into(a, T3)
    a.lw(T4, cap.F_LASTTICK, T0); a.lw(T5, cap.F_INITED, T0)
    a.beq(T5, cap.ZERO, 'first'); a.nop()
    a.beq(T3, T4, 'apply'); a.nop()
    a.lw(T6, cap.F_INDEX, T0); a.addiu(T6, T6, 1); a.sw(T6, cap.F_INDEX, T0); a.sw(T3, cap.F_LASTTICK, T0)
    a.beq(cap.ZERO, cap.ZERO, 'apply'); a.nop()
    a.label('first')
    a.addiu(T5, cap.ZERO, 1); a.sw(T5, cap.F_INITED, T0); a.sw(T3, cap.F_LASTTICK, T0); a.sw(cap.ZERO, cap.F_INDEX, T0)
    a.label('apply')
    a.lw(T7, cap.F_PADCALLS, T0); a.addiu(T7, T7, 1); a.sw(T7, cap.F_PADCALLS, T0)
    a.lw(T6, cap.F_INDEX, T0)
    a.andi(T1, T6, SLOTS - 1)
    a.sll(T4, T1, 4); a.sll(T1, T1, 7); a.r(T1, T1, T4, 0x23)   # T1 = slot * 112 (128 - 16)
    a.li(T4, LIVE_RING); a.addu(T1, T1, T4)
    a.li(T5, LIVE_STATE)
    a.lw(T4, 0, T1); a.bne(T4, T6, 'miss'); a.nop()
    a.sw(T6, 0, T5); a.addiu(T1, T1, 16)                          # applied tag = index; source = entry channels
    a.beq(cap.ZERO, cap.ZERO, 'copy'); a.nop()
    a.label('miss')
    a.lw(T4, 4, T5); a.addiu(T4, T4, 1); a.sw(T4, 4, T5)
    a.addiu(T1, T5, 0x10)                                          # source = the last applied channels
    a.label('copy')
    for k in range(24):
        a.lw(T4, 4 * k, T1); a.sw(T4, 4 * k, cap.A2); a.sw(T4, 0x10 + 4 * k, T5)
    a.label('done')
    a.addiu(cap.SP, cap.SP, -0x40); a.sq(cap.S0, 0x30, cap.SP)    # relocated 0x321298 / 0x32129C
    a.j(cap.PAD_HOOK + 8); a.nop()
    return a.link()


def entry_bytes(index, segment):
    values = cap.decode_pad(segment.get('buttons', []), segment.get('lx', 0), segment.get('ly', 0), segment.get('rx', 0), segment.get('ry', 0))
    return struct.pack('<4I', index, 0, 0, 0) + struct.pack('<24f', *values)


def build(baseline, output, start_segments, watches, roles=None):
    """ps2_capture's derived state (record ring, watches, obstacle hook) with the live pad hook and the start entries."""
    output = Path(output)
    tmp_script = output.with_suffix('.start.json')
    tmp_script.write_text(json.dumps({'segments': [{'frames': 1}]}) + '\n')
    staged = output.with_name(output.stem + '.stage.p2s')
    watches = list(watches) + [(LIVE_STATE, 0x70)]
    manifest = cap.build(baseline, tmp_script, staged, watches=watches)
    with zipfile.ZipFile(staged) as z: memory = z.read('eeMemory.bin')
    code = assemble_live()
    if len(code) > LIVE_STATE - LIVE_CODE: raise ValueError('Live hook exceeds its arena')
    for at, size in ((LIVE_CODE, len(code)), (LIVE_STATE, 0x70), (LIVE_RING, SLOTS * ENTRY)):
        if any(memory[at:at + size]): raise ValueError(f'Live arena {at:#x} is not free')
    ring = bytearray(SLOTS * ENTRY); t = 0
    for seg in start_segments:   # the first entries (e.g. Cross on the objectives card's Continue), tagged in advance
        for _ in range(int(seg['frames'])):
            if t >= SLOTS - LEAD: raise ValueError('Start script longer than the ring')
            ring[t * ENTRY:(t + 1) * ENTRY] = entry_bytes(t, seg); t += 1
    j = lambda target: struct.pack('<2I', (2 << 26) | (target >> 2), 0)
    old_jump = memory[cap.PAD_HOOK:cap.PAD_HOOK + 8]
    patches = [dict(address=hex(LIVE_CODE), expected='00' * len(code), replacement=code.hex()),
               dict(address=hex(LIVE_RING), expected='00' * len(ring), replacement=bytes(ring).hex()),
               dict(address=hex(cap.PAD_HOOK), expected=old_jump.hex(), replacement=j(LIVE_CODE).hex())]
    patch_state(staged, output, patches)
    staged.unlink(); staged.with_suffix('.patches.json').unlink(missing_ok=True)
    manifest.update(live=dict(code=hex(LIVE_CODE), state=hex(LIVE_STATE), ring=hex(LIVE_RING), slots=SLOTS, entry=ENTRY,
                              start_segments=start_segments, start_entries=t, lead=LEAD, roles={k: hex(v) for k, v in (roles or {}).items()}),
                    baseline=str(Path(baseline).resolve()))
    staged_manifest = staged.with_suffix('.capture.json')
    if staged_manifest.exists(): staged_manifest.unlink()
    output.with_suffix('.capture.json').write_text(json.dumps(manifest, indent=2) + '\n')
    return manifest


class Pine(cap.Pine):
    def write(self, address, data):
        """MsgWrite64 / MsgWrite32 (PINE opcodes 7 / 6), batched in one request."""
        cmd = b''; at = 0
        while at + 8 <= len(data): cmd += b'\x07' + struct.pack('<I', address + at) + data[at:at + 8]; at += 8
        while at + 4 <= len(data): cmd += b'\x06' + struct.pack('<I', address + at) + data[at:at + 4]; at += 4
        self.request(cmd)


def f32s(data, offset, n): return struct.unpack_from(f'<{n}f', data, offset)


FINISH_COURSE = 1
CONNECTOR_OF = {16: ('EBC3_E',), 21: ('E_ERA5',), 4: ('ERA5_C',), 19: ('C_CRA3',), 2: ('CRA3_D',), 20: ('D_DRA4',), 3: ('DRA4_A',), 17: ('A_ARA1',),
                0: ('ARA1_B',), 18: ('B_BRA2',), 15: ('DBC2_D',), 1: ('BRA2',)}   # the exit connector of each course / station on the runs
                                                                         # (Metro-City: its finish gate, FINISH)
# Detours (source cm): Gravitude's race path runs through its finish corral, which the end fences close in a peak run
# (mdl_ERA5_fencecollision_end_1002..1004: a wall from (-182980, 38860) south-west to (-186680, 33420) and south to
# (-184960, 30050); the race path crosses it where a race has no fence): down the west side of the wall, round its south end,
# then the race path again into ERA5_C.
VIAS = {'ERA5_C': [(-187800, 32000), (-185600, 28600)],
        # Metro-City: the race path west of the buildings east of it, where the pilot pinned itself after a crash's reset
        'BRA2:Finish': [(-185500, 22000), (-183500, 15000)],
        # Snow Jam's finish: in a peak run (kinds 4..6) its challenge reset planes (mdl_ARA1_challenge_reset_plane_finish_2000 /
        # 2003, x < -305278 at y > 158408) reset a rider that follows the race path into the finish; ARA1_B lies north-east
        'ARA1_B': [(-303600, 157600), (-299000, 162500)]}
# The runs' last waypoint: Metro-City's finish gate (10E5D8 accepts course 1), reached along BRA2's race paths.
FINISH = ('BRA2', 'mdl_BRA2_finish_gate_1000')
ROUTES = {   # connectors crossed by the peak runs (docs/peak3.md "All Peak Race"; docs/peak2.md "Peak 2 Race")
    'allpeak': ['EBC3_E', 'E_ERA5', 'ERA5_C', 'C_CRA3', 'CRA3_D', 'D_DRA4', 'DRA4_A', 'A_ARA1', 'ARA1_B', 'B_BRA2'],
    'peak2': ['DBC2_D', 'D_DRA4', 'DRA4_A', 'A_ARA1', 'ARA1_B', 'B_BRA2'],
}


def route_waypoints(name):
    """Centres / sizes (source cm) of the Unload then Load trigger volumes of each connector of a run, from the exported
    location packages (web/public/assets/PEAK<N>/<LOC>/world_collision.json: mdl_<LOC>_Unload_0 / _Load_0)."""
    assets = cap.ROOT / 'web/public/assets'; out = []
    for code in ROUTES[name]:
        doc = next(json.loads(p.read_text()) for p in sorted(assets.glob(f'PEAK*/{code}/world_collision.json')))
        byname = {i.get('name'): i for i in doc['instances']}
        for kind in ('Unload', 'Load'):
            i = byname[f'mdl_{code}_{kind}_0']; lo, hi = i['bounds_min_cm'], i['bounds_max_cm']
            w = dict(name=f'{code}:{kind}', center=[(a + b) / 2 for a, b in zip(lo, hi)], half=[(b - a) / 2 for a, b in zip(lo, hi)])
            if kind == 'Unload' and code in VIAS: w['via'] = [list(v) for v in VIAS[code]]
            out.append(w)
    code, gate = FINISH
    doc = next(json.loads(p.read_text()) for p in sorted(assets.glob(f'PEAK*/{code}/world_collision.json')))
    i = next(x for x in doc['instances'] if x.get('name') == gate); lo, hi = i['bounds_min_cm'], i['bounds_max_cm']
    out.append(dict(name=f'{code}:Finish', center=[(a + b) / 2 for a, b in zip(lo, hi)], half=[(b - a) / 2 for a, b in zip(lo, hi)],
                    via=[list(v) for v in VIAS.get(f'{code}:Finish', [])]))
    return out


def location_paths():
    """{course: {variant: [polyline of (x, y, z) source cm]}} from every exported location's AIP bank (paths.json race paths:
    origin + unit-direction x length segments). Location ids 0..21 are the course indexes."""
    assets = cap.ROOT / 'web/public/assets'; out = {}
    for f in sorted(assets.glob('PEAK*/*/paths.json')):
        m = json.loads((f.parent.parent / 'peak.json').read_text())
        loc = next((l for l in m['locations'] if l['code'] == f.parent.name), None)
        if not loc or loc['id'] >= 22 or loc['id'] in out: continue
        banks = {}
        for v, bank in json.loads(f.read_text())['variants'].items():
            lines = []
            for rp in bank['race_paths']:
                pts = [tuple(rp['origin'])]
                for sg in rp['segments']: pts.append(tuple(pts[-1][k] + sg[k] * sg[3] for k in range(3)))
                if len(pts) > 1: lines.append(pts)
            banks[int(v)] = lines
        out[loc['id']] = banks
    return out


class Pilot:
    """Tuck and pure pursuit along the location's own race path that ends nearest the run's next connector (the AIP banks the
    original delivers: a course's race path runs to its finish, a station's time-challenge bank to the exit connector), then
    straight to that connector's trigger volume; a stuck rider turns hard one way, then the other. Jam: jumps with a grab."""
    def __init__(self, gain=2.5, jam=False, waypoints=(), mode=8):
        self.gain = gain; self.jam = jam; self.trick = []; self.last_trick = 0
        self.waypoints = list(waypoints); self.next = 0; self.log = []
        self.paths = location_paths() if waypoints else {}; self.mode = mode
        self.guide = None; self.guide_key = None; self.history = []; self.escape = []; self.escapes = 0
        self.plans = {}; self.vias = set()
        if waypoints:   # the route's courses and their next connector (the waypoint after the previous connector's Load)
            for c in self.paths:
                for g in range(0, len(self.waypoints), 2): self.plans[(c, g)] = None
            for (c, g) in list(self.plans):
                self.plans[(c, g)] = self.plan(c, g) if self.relevant(c, g) else None

    def relevant(self, course, goal_index):
        # only the pairs a run meets: the goal's connector leaves this course's row (tools/ps2_autopilot ROUTES order)
        return goal_index < len(self.waypoints) and any(self.waypoints[goal_index]['name'].startswith(x) for x in CONNECTOR_OF.get(course, ()))

    def plan(self, course, goal_index):
        """Guide graph of one location's bank toward the next connector: every race-path vertex, forward edges along each path
        and 'hop' edges to any vertex of another path within 25 m; reverse Dijkstra from the vertex nearest the goal volume gives
        every vertex its next hop (precomputed before the run: the record loop must not stall)."""
        import heapq
        banks = self.paths.get(course)
        if not banks or goal_index >= len(self.waypoints): return None
        variant = (1 if self.mode in (6, 7, 8, 11) else 2) if 17 <= course <= 21 else 0
        lines = banks.get(variant) or banks.get(0) or []
        verts = [(li, k, pt) for li, line in enumerate(lines) for k, pt in enumerate(line)]
        if not verts: return None
        goal = self.waypoints[goal_index]['center']
        rev = [[] for _ in verts]; index = {(li, k): i for i, (li, k, _) in enumerate(verts)}
        for i, (li, k, pt) in enumerate(verts):
            if k + 1 < len(lines[li]): j = index[(li, k + 1)]; rev[j].append((i, math.dist(pt[:2], verts[j][2][:2])))
            for j, (lj, kj, q) in enumerate(verts):
                if lj != li:
                    d = math.hypot(pt[0] - q[0], pt[1] - q[1])
                    if d < 2500 and q[2] <= pt[2] + 300: rev[j].append((i, d * 1.5 + 200))
        target = min(range(len(verts)), key=lambda i: math.hypot(verts[i][2][0] - goal[0], verts[i][2][1] - goal[1]))
        dist = {target: 0.0}; nxt = {target: None}; heap = [(0.0, target)]
        while heap:
            d, j = heapq.heappop(heap)
            if d > dist.get(j, 1e30): continue
            for i, w in rev[j]:
                if d + w < dist.get(i, 1e30): dist[i] = d + w; nxt[i] = j; heapq.heappush(heap, (d + w, i))
        return dict(verts=verts, nxt=nxt, dist=dist, target=target)

    def pick(self, course, pos):
        plan = self.plans.get((course, self.next + (self.next & 1)))   # between a connector's Unload and Load: the next station's exit guide
        if plan is None: return None
        verts, nxt = plan['verts'], plan['nxt']
        cands = [i for i in nxt]
        if not cands: return None
        start = min(cands, key=lambda i: math.hypot(verts[i][2][0] - pos[0], verts[i][2][1] - pos[1]) + 0.05 * plan['dist'][i])
        line = []; i = start
        while i is not None and len(line) < 4000: line.append(verts[i][2]); i = nxt[i]
        return line

    def target(self, pos, course):
        w = self.waypoints[self.next] if self.next < len(self.waypoints) else None
        if w and w.get('via'):   # a detour before this connector: once within 120 m, straight to each, then the guide again
            for k, v in enumerate(w['via']):
                if (self.next, k) in self.vias: continue
                d = math.hypot(v[0] - pos[0], v[1] - pos[1])
                if d < 1500: self.vias.add((self.next, k)); self.log.append((0, f'via {k}')); continue
                if d < 12000 or any(key[0] == self.next for key in self.vias): return (v[0], v[1], 0)
                break
        key = (course, self.next)
        if key != self.guide_key: self.guide_key = key; self.guide = self.pick(course, pos)
        g = self.next + (self.next & 1)
        goal = self.waypoints[g]['center'] if g < len(self.waypoints) else self.waypoints[self.next]['center'] if self.next < len(self.waypoints) else None
        if not self.guide: return goal
        line = self.guide; best, at = None, 0
        for k in range(len(line) - 1):
            a, b = line[k], line[k + 1]; dx, dy = b[0] - a[0], b[1] - a[1]; L2 = dx * dx + dy * dy or 1
            t = max(0.0, min(1.0, ((pos[0] - a[0]) * dx + (pos[1] - a[1]) * dy) / L2))
            d = math.hypot(a[0] + dx * t - pos[0], a[1] + dy * t - pos[1])
            if best is None or d < best: best, at = d, k + t
        # 10 m ahead along the line (or the goal past its end)
        k, t = int(at), at - int(at); ahead = 1000.0
        while k < len(line) - 1:
            a, b = line[k], line[k + 1]; L = math.hypot(b[0] - a[0], b[1] - a[1]); rest = L * (1 - t)
            if rest >= ahead: f = t + ahead / (L or 1); return (a[0] + (b[0] - a[0]) * f, a[1] + (b[1] - a[1]) * f, 0)
            ahead -= rest; k += 1; t = 0.0
        return goal or line[-1]

    def segment(self, rec, index, course=None):
        pos = f32s(rec, 32 + 0x10, 3); vel = f32s(rec, 32 + 0xE0, 3); look = f32s(rec, 32 + 0x3A0, 3)
        control, = struct.unpack_from('<I', rec, 20)
        speed = math.hypot(vel[0], vel[1])
        for ahead in range(self.next, min(self.next + 3, len(self.waypoints))):   # passed: inside a volume's box (xy, 4 m margin)
            w = self.waypoints[ahead]
            if all(abs(pos[k] - w['center'][k]) <= w['half'][k] + 400 for k in (0, 1)) and abs(pos[2] - w['center'][2]) <= w['half'][2] + 3000:
                self.log.append((index, w['name'])); self.next = ahead + 1
        self.history.append(pos); self.history = self.history[-300:]
        if self.escape: return self.escape.pop(0)
        if len(self.history) >= 300 and math.hypot(pos[0] - self.history[0][0], pos[1] - self.history[0][1]) < 300:
            side = 0.9 if self.escapes % 2 == 0 else -0.9; self.escapes += 1; self.history = []
            self.log.append((index, f'escape {side}'))
            self.escape = [{'frames': 1, 'ly': 1, 'lx': side} for _ in range(90 + 60 * (self.escapes % 3))]
            return self.escape.pop(0)
        if self.trick: return self.trick.pop(0)
        target = self.target(pos, course) if self.waypoints else look
        if target is None: target = look
        dx, dy = target[0] - pos[0], target[1] - pos[1]
        cross = vel[0] * dy - vel[1] * dx; dot = vel[0] * dx + vel[1] * dy
        angle = math.atan2(cross, dot) if (dx or dy) and speed > 1 else 0.0
        lx = max(-0.9, min(0.9, -self.gain * angle))
        if abs(lx) < 0.15: lx = 0.0
        elif abs(lx) < 0.45: lx = math.copysign(0.45, lx)
        lx = round(round(lx / 0.1) * 0.1, 2)
        seg = {'frames': 1, 'ly': 1}
        if lx: seg['lx'] = lx
        if self.jam and control == 0 and speed > 1500 and index - self.last_trick > 240 and abs(lx) < 0.5:
            # a charged jump with a grab: Cross held 20 ticks, released, R1 held 30 (web/bc-autopilot.mjs --tricks)
            self.last_trick = index
            self.trick = [{'frames': 1, 'ly': 1, 'buttons': ['Cross']} for _ in range(19)] + [{'frames': 1, 'ly': 1, 'buttons': ['R1']} for _ in range(30)]
            return {'frames': 1, 'ly': 1, 'buttons': ['Cross']}
        return seg


def watch_offsets(manifest):
    at = manifest['layout']['watch_offset']; out = {}
    for w in manifest['layout']['watches']: out[int(w['address'], 16)] = at; at += w['length']
    return out


def run(state, output, frames, speed='normal', timeout=7200, jam=False, snap_every=0, snap_events=True, idle_end=240, route=None, mode=8, resume=None,
        replay=None):
    """Stream the records to OUTPUT (never held in memory), steer LEAD ticks ahead, save states (PS2 screenshots) at every
    course / split / world-state change. Ends at `frames` records or `idle_end` seconds without a record (the results)."""
    # resume=(OLD.bin, capture state): continue a run from one of its kept states (RUN.raw/*.p2s, the hooks and ring live in it):
    # the records up to the state's write counter and the pads written before its ring's last tag are carried over.
    resume_state = state
    manifest = json.loads(Path(resume[1] if resume else state).with_suffix('.capture.json').read_text())
    record, capacity = manifest.get('record', cap.RECORD), manifest.get('capacity', cap.CAPACITY)
    offsets = watch_offsets(manifest); roles = {k: int(v, 16) for k, v in manifest['live'].get('roles', {}).items()}
    at = lambda role: offsets.get(roles.get(role, -1))
    live_at, course_at, rows_at, handler_at, world_at = offsets[LIVE_STATE], at('course'), at('rows'), at('handler'), at('world')
    slot = random.randint(28100, 28999)
    datapath = cap.DATAPATH.parent / f'pcsx2-auto-{slot}'
    # The arithmetic profile (PS2_CAPTURE_FPU, else the manifest's, else mode 1) is recorded in the summary.
    fpu = cap.fpu_mode(manifest)
    cap.prepare_datapath(datapath, slot, fpu)
    # replay: the consumed segment of every script index, expanded from a `pads` script (the last one repeats past its end).
    replay_entries = []
    if replay:
        for segment in json.loads(Path(replay).read_text())['segments']:
            replay_entries += [segment] * int(segment['frames'])
    args = [str(cap.PCSX2), '-datapath', str(datapath), '-batch', '-nogui', '-statefile', str(Path(state).resolve())]
    if speed == 'unlimited': args.append('-unlimited')
    elif speed == 'turbo': args.append('-turbo')
    args += ['--', str(cap.ISO)]
    output = Path(output); rawdir = output.with_name(output.stem + '.raw'); rawdir.mkdir(exist_ok=True)
    log = open(output.with_suffix('.pcsx2.log'), 'w')
    proc = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT)
    out = open(output, 'wb'); padlog = open(output.with_suffix('.pads.jsonl'), 'w')
    pilot = Pilot(jam=jam, waypoints=route_waypoints(route) if route else (), mode=mode); next_write = manifest['live']['start_entries']
    have0 = 0
    if resume:
        with zipfile.ZipFile(resume_state) as z: mem = z.read('eeMemory.bin')
        have0 = struct.unpack_from('<I', mem, cap.DATA + cap.F_WRITES)[0]
        tags = [struct.unpack_from('<I', mem, LIVE_RING + k * ENTRY)[0] for k in range(SLOTS)]
        next_write = max(tags) + 1
        if resume[0] != '-':   # '-': a fresh stream from the kept state (a derived continuation, e.g. a poked race clock)
            old_bin, old_pads = Path(resume[0]), Path(resume[0]).with_suffix('.pads.jsonl')
            with open(old_bin, 'rb') as f:
                for _ in range(have0): out.write(f.read(record))
            for line in old_pads.read_text().splitlines():
                if json.loads(line)[0] < next_write: padlog.write(line + '\n')
        print(json.dumps(dict(resume=str(resume_state), records=have0, next_write=next_write)), flush=True)
    events = []; last = dict(course=None, split=None, ws=None, rows=None); inflight = []; wanted = []; collected = 0
    started = time.time(); pine = None; have = have0; last_rec = None; last_time = time.time(); periodic = have0 // snap_every if snap_every else 0; ended = False
    try:
        while time.time() - started < timeout:
            if proc.poll() is not None: raise RuntimeError(f'ARMSX2 exited early ({proc.returncode})')
            if pine is None:
                try: pine = Pine(slot)
                except OSError: time.sleep(0.5); continue
            try: writes, = struct.unpack('<I', pine.read(cap.DATA + cap.F_WRITES, 4))
            except (OSError, RuntimeError, ConnectionError): time.sleep(0.2); continue
            if writes - have > capacity: raise RuntimeError(f'Ring overrun: {writes} written, {have} read')
            got = False
            for seq in range(have + 1, writes + 1):
                data = pine.read(cap.RING + ((seq - 1) % capacity) * record, record)
                if struct.unpack_from('<I', data, 0)[0] != seq: break
                out.write(data); have = seq; last_rec = data; got = True
                tick, index = struct.unpack_from('<I', data, 4)[0], struct.unpack_from('<I', data, 28)[0]
                cur = dict(course=data[course_at] if course_at is not None else None,
                           split=struct.unpack_from('<I', data, handler_at + 8)[0] if handler_at is not None else None,
                           ws=struct.unpack_from('<I', data, world_at + 0x14)[0] if world_at is not None else None,
                           rows=bytes(data[rows_at:rows_at + 800]) if rows_at is not None else None)
                for key in ('course', 'split', 'ws'):
                    if cur[key] != last[key]:
                        events.append(dict(seq=seq, tick=tick, index=index, kind=key, value=cur[key], was=last[key]))
                        if snap_events and last[key] is not None: wanted.append((seq, f'{key}{cur[key]}'))
                if cur['rows'] != last['rows'] and cur['rows'] is not None:
                    changes = [(i, *struct.unpack_from('<4i', cur['rows'], 16 * i)) for i in range(50)
                               if last['rows'] is None or cur['rows'][16 * i:16 * i + 16] != last['rows'][16 * i:16 * i + 16]]
                    events.append(dict(seq=seq, tick=tick, index=index, kind='rows', value=changes))
                last.update(cur)
                if snap_every and seq // snap_every > periodic: periodic = seq // snap_every; wanted.append((seq, 'periodic'))
            if got:
                last_time = time.time()
                index = struct.unpack_from('<I', last_rec, 28)[0]
                if resume and pilot.waypoints and not pilot.log and course_at is not None:   # resumed mid-run: the current course's exit connector
                    exit_ = CONNECTOR_OF.get(last_rec[course_at], ('',))[0] + (':Finish' if last_rec[course_at] == FINISH_COURSE else ':Unload')
                    pilot.next = next((k for k, w in enumerate(pilot.waypoints) if w['name'] == exit_), 0); pilot.log.append((index, f'resume at {exit_}'))
                while next_write <= index + LEAD:
                    if replay_entries:
                        seg = replay_entries[min(next_write, len(replay_entries) - 1)]
                    else:
                        seg = pilot.segment(last_rec, next_write, last_rec[course_at] if course_at is not None else None)
                    base = LIVE_RING + (next_write % SLOTS) * ENTRY
                    pine.write(base + 16, entry_bytes(next_write, seg)[16:]); pine.write(base, struct.pack('<I', next_write))
                    padlog.write(json.dumps([next_write, seg]) + '\n'); next_write += 1
            # PINE MsgSaveState: up to 8 in flight, each in its own slot; finished files are moved aside at once (cheap)
            while wanted and len(inflight) < 8:
                seq, name = wanted.pop(0); used = {k for k, *_ in inflight}; k = next(n for n in range(1, 10) if n not in used)
                for old in datapath.rglob(f'*.{k:02d}.p2s*'): old.unlink()
                pine.request(b'\x09' + bytes([k])); inflight.append((k, seq, name, time.time()))
            for item in list(inflight):
                k, seq, name, t0 = item
                files = [f for f in datapath.rglob(f'*.{k:02d}.p2s') if f.is_file()]
                if not files:
                    if time.time() - t0 > 60: inflight.remove(item)
                    continue
                try:
                    with zipfile.ZipFile(files[0]) as z:
                        if not {'eeMemory.bin', 'Screenshot.png'} <= set(z.namelist()): continue
                except (zipfile.BadZipFile, OSError): continue
                files[0].rename(rawdir / f'{collected:04d}-{seq}-{name}.p2s'); collected += 1; inflight.remove(item)
            if have >= frames and not inflight and not wanted: break
            if time.time() - last_time > idle_end and not inflight and not wanted:
                if ended: break
                ended = True; events.append(dict(seq=have, kind='end', idle=idle_end)); wanted.append((have, 'end')); last_time = time.time()
            if not got: time.sleep(0.004)
        else:
            raise RuntimeError('Capture timed out')
    finally:
        if pine: pine.close()
        proc.terminate()
        try: proc.wait(10)
        except subprocess.TimeoutExpired: proc.kill()
        log.close(); out.close(); padlog.close()
        shutil.rmtree(datapath, ignore_errors=True)
        output.with_suffix('.events.json').write_text(json.dumps(events + [dict(kind='waypoint', index=i, name=n) for i, n in pilot.log]) + '\n')
    snaps = []
    for f in sorted(rawdir.glob('*.p2s')):   # post-process after the emulator stopped: PNG + kept state named by game tick
        try:
            with zipfile.ZipFile(f) as z:
                memory = z.read('eeMemory.bin'); u = lambda a: struct.unpack_from('<I', memory, a & 0x1FFFFFF)[0]
                try: tick = u(u(u(u(cap.GP - 0x848) + 0x84) + 0x0C) + 8)
                except struct.error: tick = -1
                name = f.stem.split('-', 2)[2]
                png = output.with_name(f'{output.stem}.tick{tick}.{name}.png'); png.write_bytes(z.read('Screenshot.png'))
            kept = output.with_name(f'{output.stem}.tick{tick}.{name}.p2s'); f.rename(kept)
            snaps.append(dict(tick=tick, name=name, png=str(png), state=str(kept)))
        except (zipfile.BadZipFile, OSError, KeyError) as error: snaps.append(dict(error=str(error), file=f.name))
    shutil.rmtree(rawdir, ignore_errors=True)
    summary = dict(state=str(state), records=have, bytes_per_record=record, events=len(events), snapshots=snaps, fpu_mode=fpu)
    output.with_suffix('.json').write_text(json.dumps(dict(summary, manifest=manifest), indent=2) + '\n')
    return summary


def pads(run_bin):
    """The consumed pad of every record -> a ps2_capture segments script (RUN.script.json) for the comparer."""
    run_bin = Path(run_bin)
    summary = json.loads(run_bin.with_suffix('.json').read_text()); manifest = summary['manifest']
    record = manifest['record']; watch_at = manifest['layout']['watch_offset']; at = watch_at; live_at = None
    for w in manifest['layout']['watches']:
        if int(w['address'], 16) == LIVE_STATE: live_at = at
        at += w['length']
    written = {}
    t = 0
    for seg in manifest['live']['start_segments']:
        for _ in range(int(seg['frames'])): written[t] = seg; t += 1
    for line in run_bin.with_suffix('.pads.jsonl').read_text().splitlines():
        k, seg = json.loads(line); written[k] = seg
    segments = []; last = {'frames': 1}; misses = 0; prev_index = -1
    size = run_bin.stat().st_size
    with open(run_bin, 'rb') as f:
        for n in range(size // record):
            data = f.read(record)
            index, = struct.unpack_from('<I', data, 28); applied, = struct.unpack_from('<I', data, live_at)
            if index == prev_index: continue
            if index != prev_index + 1: raise ValueError(f'script index jumps {prev_index} -> {index} at record {n}')
            prev_index = index
            seg = written.get(index) if applied == index else None
            if seg is None: misses += 1; seg = last
            cur = {k: v for k, v in seg.items() if k != 'frames'}
            if segments and {k: v for k, v in segments[-1].items() if k != 'frames'} == cur: segments[-1]['frames'] += 1
            else: segments.append(dict(cur, frames=1))
            last = seg
    out = run_bin.with_suffix('.script.json')
    out.write_text(json.dumps({'segments': segments}) + '\n')
    # The comparer reads the manifest's segments: a copy of the capture manifest with the consumed script.
    cm = run_bin.with_suffix('.capture.json')
    manifest = dict(manifest, segments=segments, script=str(out), script_frames=sum(s['frames'] for s in segments))
    cm.write_text(json.dumps(manifest, indent=2) + '\n')
    return dict(segments=len(segments), ticks=prev_index + 1, misses=misses)


def analyze(run_bin):
    """The PS2 ground truth of a peak-run capture (RUN.analysis.json): the start rows, then every change of the current
    course 0x535C08, the world state, the split counter (handler +8) and the location / sky rows, record by record; the measured
    disc read of every location (ticks from the pass after its row reads to the pass showing it resident, less the pass the
    core's model counts: web/peak_world.inc read_ticks); the splits (ticks since Continue). web/test-mountain-world.mjs replays
    the triggers through the core's streaming and compares every row."""
    run_bin = Path(run_bin); cm = json.loads(run_bin.with_suffix('.capture.json').read_text()) if run_bin.with_suffix('.capture.json').exists() else json.loads(run_bin.with_suffix('.json').read_text())['manifest']
    R = cm['record']; off = watch_offsets(cm); roles = {k: int(v, 16) for k, v in cm['live']['roles'].items()}
    o_rows, o_course, o_world, o_h = off[roles['rows']], off[roles['course']], off[roles['world']], off[roles['handler']]
    names = cap.location_names(); n = run_bin.stat().st_size // R; ev = []; prev = None; reads = {}; reading_since = {}; last_done = 0
    with open(run_bin, 'rb') as f:
        for i in range(n):
            f.seek(i * R); r = f.read(R); idx, = struct.unpack_from('<I', r, 28)
            rows = [struct.unpack_from('<4i', r, o_rows + 16 * k) for k in range(50)]
            cur = dict(course=r[o_course], ws=struct.unpack_from('<I', r, o_world + 0x14)[0], split=struct.unpack_from('<H', r, o_h + 8)[0], pos=[round(x) for x in struct.unpack_from('<3f', r, 48)])
            if prev is None: ev.append(dict(record=i, index=idx, kind='start', rows={names.get(x[0], x[0]): x[2] for x in rows if x[2]}, **cur))
            else:
                for key in ('course', 'ws', 'split'):
                    if cur[key] != prev[0][key]: ev.append(dict(record=i, index=idx, kind=key, value=cur[key], was=prev[0][key], pos=cur['pos']))
                ch = [(names.get(rows[k][0], rows[k][0]), prev[1][k][2], rows[k][2]) for k in range(50) if rows[k][2] != prev[1][k][2]]
                if ch: ev.append(dict(record=i, index=idx, kind='rows', changes=ch))
                for name, a, b in ch:
                    if b in (6, 8) and a in (3, 4): reading_since[name] = i
                    if a in (6, 8) and b in (1, 2):
                        reads.setdefault(name, []).append(i - max(last_done, reading_since.get(name, i) + 1) - 1); last_done = i
            prev = (cur, rows)
    splits = [dict(record=e['record'], index=e['index'], split=e['value'], seconds=int(e['index'] * 0.016666668)) for e in ev if e['kind'] == 'split']
    out = dict(records=n, events=ev, reads=reads, splits=splits)
    run_bin.with_suffix('.analysis.json').write_text(json.dumps(out, indent=1) + '\n')
    return dict(records=n, splits=splits, reads={k: v[0] for k, v in reads.items()})


def main():
    def stop(signum, frame): raise KeyboardInterrupt(f'signal {signum}')
    signal.signal(signal.SIGTERM, stop)
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest='cmd', required=True)
    b = sub.add_parser('build'); b.add_argument('baseline'); b.add_argument('output')
    b.add_argument('--start', default='[{"frames": 1, "buttons": ["Cross"]}, {"frames": 30, "ly": 1}]', help='JSON segments of the first entries')
    b.add_argument('--watch', action='append', default=[])
    b.add_argument('--role', action='append', default=[], help='ROLE=ADDR (rows, course, gmm, handler, world, streamer): a watched window the run reads')
    r = sub.add_parser('run'); r.add_argument('state'); r.add_argument('output'); r.add_argument('--frames', type=int, required=True)
    r.add_argument('--speed', default='normal', choices=['normal', 'turbo', 'unlimited']); r.add_argument('--timeout', type=int, default=7200)
    r.add_argument('--replay', help="a finished run's consumed script (RUN.script.json from `pads`): open-loop pads instead of the pilot")
    r.add_argument('--resume', nargs=2, metavar=('OLD_BIN', 'CAPTURE_STATE'), help='continue OLD_BIN from STATE (a kept RUN.raw state; OLD_BIN "-": only the new records); STATE here is the kept state');
    r.add_argument('--route', choices=sorted(ROUTES)); r.add_argument('--mode', type=int, default=8, help='game mode (8 All Peak Race, 11 All Peak Jam, 7 Peak 2 Race): the stations\' bank variant'); r.add_argument('--jam', action='store_true'); r.add_argument('--snap-every', type=int, default=0); r.add_argument('--no-snap-events', action='store_true')
    q = sub.add_parser('pads'); q.add_argument('run')
    z = sub.add_parser('analyze'); z.add_argument('run')
    a = p.parse_args()
    if a.cmd == 'build':
        roles = {r.split('=')[0]: int(r.split('=')[1], 0) for r in a.role}
        print(json.dumps(build(a.baseline, a.output, json.loads(a.start), [tuple(int(v, 0) for v in w.split(':')) for w in a.watch], roles), indent=2)[:3000])
    elif a.cmd == 'run':
        print(json.dumps(run(a.state, a.output, a.frames, a.speed, a.timeout, a.jam, a.snap_every, not a.no_snap_events, route=a.route, mode=a.mode, resume=a.resume, replay=a.replay), indent=2))
    elif a.cmd == 'analyze':
        print(json.dumps(analyze(a.run)))
    else:
        print(json.dumps(pads(a.run), indent=2))


if __name__ == '__main__':
    main()
