// Server-side plausibility of an online racer's finish (web/server/mp-server.mjs). The server does not simulate a
// rider; it watches the state packets it relays (web/net/rider-packet.js header: race tick since GO, remaining route
// distance, finish ticks, reset counters, physical position) and checks that a claimed finish time is one the
// streamed run could have produced:
//   clock    the race tick does not run ahead of the server clock (ticks since GO) for long: a lead above
//            AHEAD_TICKS must come back within AHEAD_RECOVER_TICKS and stay under AHEAD_BURST_TICKS (a client
//            catching up after a hitch overshoots for a few seconds: web/net/mp-game.js pace); the finish is not
//            claimed earlier than the server clock allows (claimed ticks + the course's countdown vs the tick of the
//            finish packet's arrival, with room for latency and a throttled tab catching up); the countdown is the
//            event's: 3-2-1 (180 ticks) before the race clock, none on a rolling start (backcountry);
//   trace    a streamed packet shows the finish with the claimed time (finish ticks); the stream covers the race;
//   motion   between packets of the same placement the rider moves at most MAX_SPEED (legit runs peak at ~4600 cm/s,
//            the 3333 cm/s speed limit plus boost / falls); a reset / rescue (counters +1) moves it at most
//            MAX_RESET_JUMP and not more often than MIN_RESET_GAP; a longer jump is accepted only where a stage teleport of
//            the course explains it (a Metro-City phone booth / water tower: from its trigger to one of its destinations,
//            web/server/teleport-beams.mjs), with the placement counter bumped by one or not at all;
//   distance the travelled path is at least MIN_PATH of the route length (legit finishers: 1.18..1.32), its mean speed
//            is below MAX_MEAN_SPEED, and the time is not below the course floor (route length at MAX_AVERAGE).
// Without simulating the rider on the server a forged stream that stays inside every bound is not detectable; the
// bounds limit what one can claim (no finish under ~88 s on Snow Jam, no teleports, no clock games).
// The bounds come from real full runs (tuck, boost, weave, scripted tricks and crashes; ARA1 and BRA2) and real
// browser races, with 2x margins; web/test-mp-plausibility.mjs replays such a run (no finding) and forgeries.
export const COUNTDOWN_TICKS = 180;     // event start 3-2-1: the race clock counts from GO tick 181 (races, freestyle)
export const MAX_SPEED = 9000;          // cm/s between packets (legit max ~4600)
export const MAX_RESET_JUMP = 20000;    // cm per reset placement (legit max ~6900)
export const MIN_RESET_GAP = 60;        // ticks between reset placements
export const AHEAD_TICKS = 45;          // race tick ahead of the server clock, held (clock-sync error)
export const AHEAD_BURST_TICKS = 1200;  // ... at most, for a while (a hitch's catch-up overshoot: ~50-600 ticks, older clients)
export const AHEAD_RECOVER_TICKS = 1800; // wall ticks a lead above AHEAD_TICKS may last (the overshoot decays in ~1-10 s)
export const BEHIND_TICKS = 300;        // finish claimed earlier than the server clock allows (latency, catching up)
export const MIN_PATH = 0.8;            // travelled path / route length
export const MAX_AVERAGE = 4000;        // cm/s along the route: the course floor (legit ~1300-1500)
export const MAX_MEAN_SPEED = 3500;     // cm/s travelled path over the race (legit ~1900; the speed limit is 3333)
export const MIN_COVERAGE = 0.5;        // streamed packets / expected (20 Hz)
export const BEAM_REACH = 300;          // cm: a stage teleport's slack (the body reaches the trigger before the root, probe, rounding)

import { beamsFor, teleportJump } from './teleport-beams.mjs';

// Ticks from GO to the race clock's start on a course. Races and freestyle events count down 3-2-1 (the clock reads 1
// on tick 181); backcountry events (Rival Time / Rival Points: 234AD0 sends event kinds 4..6 to 233AA0 instead of the
// countdown, docs/backcountry.md) and the peak free-ride streams have a rolling start: the clock reads 1 on tick 0 (GO's).
// Course codes: <peak letter><event><n>, BC = backcountry (web/public/assets/courses.json).
export function countdownTicks(course) {
  const c = String(course ?? '').toUpperCase();
  return /^[A-Z]BC\d$/.test(c) || /^PEAK\d$/.test(c) || c.startsWith('MOUNTAIN') ? 0 : COUNTDOWN_TICKS;
}

// Minimal header read of a kind-2 packet (without the sender slot byte).
export function readHeader(bytes) {
  if (bytes.length < 56 || bytes[0] !== 2) return null;
  const v = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const h = { finished: !!(bytes[1] & 1), dnf: !!(bytes[1] & 2), tick: v.getUint32(4, true), remaining: v.getFloat32(8, true), finishTicks: v.getFloat32(12, true),
    placements: v.getUint16(28, true), rescues: v.getUint16(30, true), position: [v.getFloat32(32, true), v.getFloat32(36, true), v.getFloat32(40, true)] };
  return [h.tick, h.remaining, h.finishTicks, ...h.position].every(Number.isFinite) ? h : null;
}

// course: the race's course code (the lobby's), for its stage teleports.
export function createRunCheck({ course = null } = {}) {
  const beams = beamsFor(course), countdown = countdownTicks(course);
  // ahead: the open lead above AHEAD_TICKS ({since: wall, peak}); lead: the largest lead seen (diagnostics).
  const run = { packets: 0, first: null, last: null, path: 0, route: 0, lastReset: -1e9, finishPacket: null, findings: [], teleports: [], countdown, ahead: null, lead: -Infinity };
  const find = (code, detail) => { if (run.findings.length < 20) run.findings.push({ code, ...detail }); };
  return {
    run,
    // One relayed packet; wallTicks = server ticks since GO at its arrival.
    feed(bytes, wallTicks) {
      const h = readHeader(bytes); if (!h) return;
      run.packets++;
      const a = run.last;
      // The lead over the server clock, on packets in order (a replayed old packet does not end a lead).
      if (!a || h.tick > a.tick) {
        const lead = h.tick - wallTicks; run.lead = Math.max(run.lead, lead);
        if (lead <= AHEAD_TICKS) run.ahead = null;
        else {
          const e = run.ahead ?? (run.ahead = { since: wallTicks, peak: lead, found: false }); e.peak = Math.max(e.peak, lead);
          if (!e.found && (lead > AHEAD_BURST_TICKS || wallTicks - e.since > AHEAD_RECOVER_TICKS)) { e.found = true; find('clock-ahead', { tick: h.tick, wall: Math.round(wallTicks), lead: Math.round(lead), since: Math.round(e.since) }); }
        }
      }
      if (!a) { run.first = h; run.route = h.remaining; }
      else if (h.tick > a.tick) {
        const d = Math.hypot(h.position[0] - a.position[0], h.position[1] - a.position[1], h.position[2] - a.position[2]), dt = (h.tick - a.tick) / 60;
        const resets = (h.placements - a.placements) + (h.rescues - a.rescues);
        const beam = (resets === 0 || resets === 1) && d > MAX_SPEED * dt && beams.length ? teleportJump(beams, a.position, h.position, MAX_SPEED * dt + BEAM_REACH) : null;
        if (beam) { if (run.teleports.length < 50) run.teleports.push({ tick: h.tick, cm: Math.round(d), resets, ...beam }); } // not path, not a reset
        else if (resets === 0) { if (d / dt > MAX_SPEED) find('speed', { tick: h.tick, cmps: Math.round(d / dt) }); run.path += d; }
        else if (resets < 0 || resets > 2 || d > MAX_RESET_JUMP + MAX_SPEED * dt || h.tick - run.lastReset < MIN_RESET_GAP) find('reset', { tick: h.tick, resets, cm: Math.round(d) });
        else run.lastReset = h.tick;
      } else return; // replayed / out of order: ignored
      if (h.finished && !run.finishPacket && !h.dnf) run.finishPacket = { tick: h.tick, finishTicks: h.finishTicks, wall: wallTicks };
      run.last = h;
    },
    // A finish claim: {ok, findings}. wallTicks = server ticks since GO when the claim arrived.
    verdict(ticks) {
      const out = [...run.findings], f = run.finishPacket;
      if (!f) out.push({ code: 'no-trace' });
      else {
        if (Math.abs(f.finishTicks - ticks) > 1) out.push({ code: 'time-mismatch', claimed: ticks, streamed: f.finishTicks });
        if (ticks + countdown < f.wall - BEHIND_TICKS) out.push({ code: 'clock-behind', claimed: ticks, wall: Math.round(f.wall), countdown });
        if (f.tick < ticks + countdown - 3) out.push({ code: 'finish-early', packetTick: f.tick, claimed: ticks, countdown });
      }
      if (run.route > 0) {
        if (run.path < MIN_PATH * run.route) out.push({ code: 'path', path: Math.round(run.path), route: Math.round(run.route) });
        if (ticks < run.route / MAX_AVERAGE * 60) out.push({ code: 'too-fast', claimed: ticks, floor: Math.round(run.route / MAX_AVERAGE * 60) });
        if (ticks > 0 && run.path / (ticks / 60) > MAX_MEAN_SPEED) out.push({ code: 'mean-speed', cmps: Math.round(run.path / (ticks / 60)) });
      }
      const expected = (ticks + countdown) / 3;
      if (run.packets < MIN_COVERAGE * expected) out.push({ code: 'coverage', packets: run.packets, expected: Math.round(expected) });
      return { ok: out.length === 0, findings: out, stats: { packets: run.packets, path: Math.round(run.path), route: Math.round(run.route), teleports: run.teleports.length, lead: Number.isFinite(run.lead) ? Math.round(run.lead) : null } };
    },
  };
}
