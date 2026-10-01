// pv onlineRecords (docs/online-records.md): the page's side of the online boards' replays. installOnlineReplay(ctx) (main.js) adds
//   ui.cb.onlineRun()            what the finished run allows: {available (a replay of it exists), inWorld, giveUp, character}
//   ui.cb.onlineReplayFile(m)    the finished run as a portable replay file {meta, pad} (web/server/replay-file.mjs)
//   ui.cb.watchOnlineReplay(o)   Watch Replay: download the entry's run, load its course with its rider / outfit / lineup, play it
//                                in the full replay (web/replay-ui.js, the '64replay' overlay), Exit replay back to the board
// ctx: main.js state through functions (it owns the variables): core(), course(), courses(), rider(), setRider(r), aiRace(), replay,
//   inWorld(), careerId(), navigate(url, opts), stopLive(), audioStart(), coreUrl.
import { RIDER_CHARACTER } from './career.js';
import { BUILD_ID } from './build-id.js';
import { createRecording } from './replay.js';
import { outfitKey, remoteOutfitRider } from './wardrobe.js';
import { onlineRecords } from './online-records-ui.js';

const clone = (x) => (x == null ? x : JSON.parse(JSON.stringify(x)));

export function installOnlineReplay(ctx) {
  const { ui, replay } = ctx;
  if (!replay) return null;
  let lastAttributes = null;
  const setAttributes = ui.cb.attributes;
  if (setAttributes) ui.cb.attributes = (bytes) => { lastAttributes = Array.from(bytes); return setAttributes(bytes); };   // the run's attribute bytes (main.js startRun)

  ui.cb.onlineRun = () => {
    const rider = ctx.rider(), rec = replay.recording;
    return { available: !!replay.available?.() && !watch, inWorld: !!ctx.inWorld(), giveUp: !!rec?.events?.some((e) => e.kind === 'giveUp'),
      character: RIDER_CHARACTER[ctx.careerId()] ?? RIDER_CHARACTER[rider?.id] ?? 4,
      // the finish tick's record (game-tick.js rec.finish): the claim the verifier re-simulates (race ticks, the score latched there)
      finish: replay.finishInfo ?? null };
  };

  ui.cb.onlineReplayFile = async ({ event, mode, course, name, claim }) => {
    const rec = replay.recording, rider = ctx.rider(), ai = ctx.aiRace(), entry = rider?.entry ?? rider;
    if (!rec?.exportBytes || replay.finishTick < 0 || !replay.snapshot) return null;
    const snapshot = clone({ ...replay.snapshot, inWorld: null });
    let outfit = null; try { outfit = entry && entry.kind !== 'cheat' ? await outfitKey(ui, entry) : null; } catch {}
    const lineup = ai?.lineup
      ? clone({
          values: ai.lineup.values ?? null,
          entries: ai.lineup.entries ?? null,
          opponent: ai.lineup.opponent ?? null,
          rival: ai.lineup.rival ?? null,
          mode: ai.lineup.mode ?? null,
          tables: ai.lineup.tables ?? null,
          round: ui.careerUI?.career?.active?.ev?.round ?? 3,
          career: !!ui.careerUI?.career?.active?.ev?.career,
          enabled: ai.enabled !== false
        })
      : null;
    const meta = { event, mode, course, name, claim, round: lineup?.round ?? 3, character: RIDER_CHARACTER[ctx.careerId()] ?? 4,
      rider: { id: entry?.id, kind: entry?.kind ?? 'rider', base: entry?.base ?? null, career: !!entry?.career, outfit, attributes: lastAttributes ?? Array(7).fill(5),
        uber: clone(rider?.uber_choice ?? []), uberKey: rider?.uber ?? '' },
      ticks: replay.finishTick + 1, finishTick: replay.finishTick, highlights: replay.highlights, events: clone(rec.events), snapshot, lineup,
      core: await onlineRecords().coreHash(ctx.coreUrl), build: BUILD_ID, giveUp: rec.events.some((e) => e.kind === 'giveUp') };
    return { meta, pad: rec.exportBytes() };
  };

  // ---- Watch Replay ----
  let watch = null;
  const board = () => ui.careerUI?.online;
  ui.onlineWatch = {
    get active() { return !!watch?.playing; },
    afterSwitch() { if (watch) load(); },
    exit() { if (!watch) return false; finish(); return true; },
  };
  ui.cb.watchOnlineReplay = async ({ entry, key, ev, back }) => {
    if (watch) return;
    const b = board()?.board; if (b) b.status = 'loading';
    let file;
    try { file = await onlineRecords().replay(entry.id); }
    catch (e) { console.warn('Replay download failed', e); if (b) b.status = 'error'; ui.sync(); return; }
    if (b) b.status = 'ready';
    if (!(await begin(file, { back, ev, key })) && b) { b.status = 'error'; ui.sync(); }
  };
  // ?verify (web/server/records-verifier.mjs drives it in headless Chrome on the host): re-simulate a stored run on this build and
  // report what it reproduces. -> {ok: true | false | null (could not run), reason, value, finishTick, core}
  ui.cb.verifyOnlineRun = (id) => new Promise((resolve) => {
    if (watch) { resolve({ ok: null, reason: 'busy' }); return; }
    onlineRecords().replay(id).then((file) => begin(file, { verify: resolve }).then((started) => { if (!started) resolve({ ok: null, reason: 'rider or course unknown' }); }),
      (e) => resolve({ ok: null, reason: `download: ${e?.message ?? e}` }));
  });
  if (new URLSearchParams(globalThis.location?.search ?? '').has('verify')) {
    globalThis.ssxVerify = ui.cb.verifyOnlineRun;
    globalThis.ssxVerifyCore = () => onlineRecords().coreHash(ctx.coreUrl);   // the build's core id the queue is asked for
  }
  // the downloaded run's rider and course, then load() (now or after the course switch)
  async function begin(file, { back = null, ev = null, key = null, verify = null } = {}) {
    const m = file.meta, roster = ui.riders || [], base = roster.find((r) => r.id === m.rider?.id);
    const target = ctx.courses().find((c) => c.code === m.course && !c.freeRide);
    if (!base || !target) { console.warn('Replay: unknown rider or course', m.rider?.id, m.course); return false; }
    const pick = { ...base, ...(m.rider.kind === 'cheat' ? { kind: 'cheat', base: m.rider.base || 'zoe' } : {}), ...(m.rider.career ? { career: true } : {}) };
    const resolved = await remoteOutfitRider(pick, m.rider.outfit ?? null);
    const rider = { ...resolved, entry: pick, stamp: 'online-replay', uber_choice: m.rider.uber ?? [], uber: m.rider.uberKey ?? '', replayFixed: true };
    watch = { file, back, ev, key, verify, previous: ctx.rider(), careerMode: ui.careerMode, onlineMode: ui.onlineMode, rider, playing: false };
    ui.careerMode = false; ui.onlineMode = false;
    ctx.setRider(rider);
    if (ctx.course()?.code !== target.code || ctx.course()?.freeRide) {
      const url = new URL(location.href); url.searchParams.set('course', target.code); url.searchParams.delete('autostart'); url.searchParams.delete('peakCourse'); url.searchParams.delete('peakMode');
      ctx.navigate(url, { after: 'replay' });   // the original load screen; then afterSwitch -> load()
    } else load();
    return true;
  }
  // The event as the uploader rode it: the rider (set above), the computer riders' lineup, the warm-up under the load screen (no
  // intro: a replay starts at the countdown), then the full replay.
  function load() {
    const w = watch; if (!w) return;
    const m = w.file.meta, ai = ctx.aiRace();
    if (ai && m.lineup) ai.fixedNext = m.lineup;
    let framesDone; const frames = new Promise((r) => (framesDone = r)); ui.warmFramesDone = framesDone;
    const warm = ui.cb.warmup?.(); Promise.resolve(warm).finally(framesDone).catch(() => {});
    ui.loading.run(() => { if (watch === w) play(); }, [warm].filter(Boolean));
  }
  function play() {
    const w = watch, m = w.file.meta, rec = createRecording();
    try { rec.importBytes(w.file.pad, m.ticks, m.events || []); } catch (e) { console.warn('Replay: bad pad stream', e); finish({ ok: false, reason: 'bad pad stream' }); return; }
    ui.cb.attributes?.(m.rider.attributes);
    ctx.audioStart?.();
    replay.load({ recording: rec, snapshot: m.snapshot, finishTick: m.finishTick, highlights: m.highlights || [] });
    w.playing = true; ctx.stopLive();
    if (!replay.start('full')) { console.warn('Replay: start failed'); finish({ ok: null, reason: 'replay start failed' }); return; }
    if (w.verify) { verifyRun(w); return; }
    ui.replayUi?.open(); ui.set('replay'); ui.sync();
    // D7: played on this core either way
    onlineRecords()
      .coreHash(ctx.coreUrl)
      .then((h) => {
        if (watch !== w || !ui.replayUi) return;
        w.mismatch = !!m.core && m.core !== h;
        ui.replayUi.note = w.mismatch ? 'Recorded on an earlier version of the game.' : null;
      });
  }
  // ?verify: every recorded tick again as fast as it runs (a yield every 600 ticks), the finish record of the tick it finishes on
  // (game-tick.js rec.finish), compared with the claim: a race's finish ticks, a score event's score latched at the finish, and the
  // finish on the run's last recorded tick.
  async function verifyRun(w) {
    const m = w.file.meta, end = m.finishTick, timed = m.claim?.ticks != null, claim = timed ? m.claim.ticks : m.claim?.score;
    let fin = null, at = -1, t0 = performance.now();
    try {
      while (watch === w && replay.tick <= end) {
        for (let k = 0; k < 600 && replay.tick <= end && !fin; k++) { const t = replay.tick, r = replay.step(); if (r?.finish) { fin = r.finish; at = t; } }
        if (fin) break;   // the first finish decides: a run that finishes before its last recorded tick is not the run claimed
        await new Promise((r) => setTimeout(r, 0));
      }
    } catch (e) { finish({ ok: null, reason: `simulation: ${e?.message ?? e}` }); return; }
    const value = fin ? (timed ? Math.round(fin.ticks) : fin.score | 0) : null;
    const ok = !!fin && !fin.dnf && at === end && value === claim;
    const reason = !fin ? `no finish in ${end + 1} ticks` : fin.dnf ? 'DNF' : at !== end ? `finished at tick ${at}, the run ends at ${end}`
      : value !== claim ? `${timed ? 'time' : 'score'} ${value}, claimed ${claim}` : 'reproduced';
    finish({ ok, reason, value, finishTick: at, ms: Math.round(performance.now() - t0), core: await onlineRecords().coreHash(ctx.coreUrl) });
  }
  // Exit replay (web/replay-ui.js -> main.js exit): the viewer's own rider again, back to the board. A verify run reports instead.
  function finish(result = null) {
    const w = watch; watch = null; if (!w) return;
    if (w.verify) {
      replay.stop(); ctx.stopLive();
      replay.load({ recording: null, snapshot: null, finishTick: -1, highlights: [] });
      if (ctx.aiRace()) ctx.aiRace().fixedNext = null;
      ui.careerMode = w.careerMode; ui.onlineMode = w.onlineMode;
      w.verify(result ?? { ok: null, reason: 'stopped' });
      return;
    }
    replay.stop(); ctx.stopLive(); ui.cb.quit?.(); if (ui.replayUi) ui.replayUi.note = null;
    // the page's own run can no longer be replayed exactly (the core holds the uploader's rider and lineup now): its Replay greys
    replay.load({ recording: null, snapshot: null, finishTick: -1, highlights: [] });
    if (ctx.aiRace()) ctx.aiRace().fixedNext = null;
    ui.careerMode = w.careerMode; ui.onlineMode = w.onlineMode;
    if (w.previous) ctx.setRider(w.previous);
    if (typeof w.back === 'function') w.back(); else { ui.set('main'); ui.sync(); }
  }
  return ui.onlineWatch;
}
