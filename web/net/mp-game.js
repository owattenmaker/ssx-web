// Online race glue for web/main.js: owns the lobby client (web/net/mp-client.js), the synchronized start and race
// clock, the local rider's packets, the remote riders (web/net/remote-riders.js, drawn by web/opponent-riders.js),
// rider-vs-rider (web/net/pair-net.js), the place display and the results.
//
// main.js calls (all no-ops outside an online race):
//   beforeStart(core)      startRun, before _start_event: grid slot + the online pair world
//   holding()              before advancing the simulation: the countdown waits for the shared GO instant
//   pace(dt)               the frame's sim seconds, nudged so the race tick follows the server clock
//   simulating(paused)     true while an online race keeps simulating behind the pause menu (no pause online)
//   beginTick()/endTick()  around each sim tick (where aiRace.beginTick/endTick go with computer riders)
//   afterFrame()           once per frame (finish report)
//   render(alpha)          where the computer riders are drawn
//   hud()                  the place display (0x21E1B0) while racing
//   aiAllowed()            no computer riders in online races
// A lobby on another course: main.js switchCourse(url) loads it in the page (the socket and the lobby seat stay; no reconnect);
// courseReady() resolves when the course is loaded (a start waits for it), pendingCourse() is the course being loaded.
// A hidden tab gets no animation frames: a worker timer keeps calling `advance` (main.js runs the sim ticks), so the
// rider keeps racing and streaming.
import { createMpClient, sessionToken } from './mp-client.js';
import { createRemoteRiders, DELAY_TICKS } from './remote-riders.js';
import { captureState, encodeState, encodeAttack, encodeWorld, decodeFrame, drainFx, ATTACK, STATE, WORLD } from './rider-packet.js';
import { createRemoteFx } from './remote-fx.js';
import { createPairNet, DEFAULT_PAIR } from './pair-net.js';
import { createPickupArbiter } from './pickup-arbiter.js';
import { paceSeconds } from './race-pace.js';
import { createOpponentRiders } from '../opponent-riders.js';
import { estimateFinishTicks, orderByTime } from '../ai-race.js';
import { loadCharacter, humanSettings } from '../character-roster.js';
import { outfitKey, remoteOutfitRider } from '../wardrobe.js';
import { humanCharacter } from '../lineup.js';
import { onlineGridState } from './grid-seed.js';

const SEND_EVERY = 3;            // ticks: 20 Hz
const BACKGROUND_MS = 50;        // hidden-tab sim cadence
const HELD_FRAME_MS = 250;       // no animation frame for this long: the worker drives the simulation

export function createMpGame({
  T,
  scene,
  load,
  loader,
  origin,
  ui,
  getCore,
  isRunning,
  isFinished,
  resultTicks,
  rider,
  course,
  startRun,
  lighting = null,
  advance = null,
  camera = () => null,
  switchCourse = null,
  courseReady = null,
  pendingCourse = () => null
}) {
  const params = new URL(location.href).searchParams;
  const client = createMpClient({
    name: rider().name,
    rider: rider().id,
    pkg: rider().package,
    base: rider().kind === 'cheat' ? rider().base || 'zoe' : null,
    token: sessionToken()
  });
  const stats = {
    frames: 0,
    sentStates: 0,
    sentBytes: 0,
    sentAttacks: 0,
    receivedAttacks: 0,
    backgroundTicks: 0,
    paceTicks: 0,
    skipped: ''
  };
  let race = null,
    racing = false,
    goLocal = 0,
    renderer = null,
    order = [],
    remote = createRemoteRiders(),
    pairNet = null;
  let reportedFinish = false,
    pendingLobby = params.get('lobby'),
    npcDoc = null,
    finished = new Map(),
    finalResults = null,
    lastFrameMs = 0,
    lightPtr = 0;
  let remoteFx = null,
    worldQueue = [],
    sentTriggers = new Set(),
    fxPending = [],
    resetCounters = null,
    pickups = null,
    lineups = null,
    gridScales = null,
    seedSet = false,
    starting = false;

  // The local rider in the lobby profile: character, package, a cheat skin's base rider, the worn outfit
  // (web/wardrobe.js outfitKey: the committed item ids, 'sam:<package>', or null for the default outfit) and the
  // pair inputs (0x11FF98 weight, collision/attack stats) -- everything another client needs to show this rider.
  const profileOf = async () => {
    const r = rider();
    let pair = null,
      outfit = null;
    try {
      pair = (await loadCharacter(r))?.identity?.pair ?? null;
    } catch {}
    try {
      outfit = await outfitKey(ui, r);
    } catch {}
    return {
      name: r.name,
      rider: r.id,
      pkg: r.package,
      base: r.kind === 'cheat' ? r.base || 'zoe' : null,
      outfit,
      pair: pair
        ? { weight_attribute: pair.weight_attribute, collision_stat: pair.collision_stat, attack_stat: pair.attack_stat }
        : DEFAULT_PAIR
    };
  };
  let profileSent = '';
  async function refreshProfile() {
    const p = await profileOf(),
      key = JSON.stringify(p);
    if (key === profileSent && client.state.lobby) return;
    profileSent = key;
    client.setProfile(p);
  }

  const session = {
    client,
    async connect() {
      if (client.state.status === 'online') return;
      await refreshProfile();
      await client.connect();
      await new Promise((r) => {
        const off = client.on('status', (s) => {
          if (s === 'online' || s === 'version') {
            off();
            r();
          }
        });
        if (client.state.status === 'online') {
          off();
          r();
        }
      });
      if (pendingLobby && !client.state.lobby) {
        client.join(pendingLobby);
        pendingLobby = null;
      }
    },
    disconnect() {
      client.leave();
      client.disconnect();
    },
    createLobby() {
      client.create({ name: `${rider().name}'s lobby`, course: course().code });
    },
    joinLobby(id) {
      client.join(id);
    },
    cycleCourse() {
      const list = (ui.courses || []).filter((c) => c.ready !== false);
      const i = list.findIndex((c) => c.code === client.state.lobby?.course);
      const next = list[(i + 1) % list.length];
      if (next) client.course(next.code);
    },
    // Pause > Quit in an online race: DNF, back to the lobby.
    quitRace() {
      if (racing && !reportedFinish) {
        reportedFinish = true;
        client.finish(0, true, 'quit');
      }
      leaveRace();
    },
    // Back from the results.
    backToLobby() {
      leaveRace();
    },
    get racing() {
      return racing;
    },
    get starting() {
      return starting;
    }, // a start waiting for the course load (main.js keeps the load screen for it)
    get race() {
      return race;
    },
    resultRows: (raceTicks) => resultRows(raceTicks),
    get finalResults() {
      return finalResults;
    },
    get stats() {
      return stats;
    }
  };

  // Lobby on another course: load it in the page while still in the lobby (main.js switchCourse; the socket and the seat stay).
  // The URL is the one a reload would use (?course&rider[&base]&online=1&lobby: the reconnect token keeps the seat then).
  client.on('lobby', (lobby) => {
    if (!lobby) return;
    if (lobby.course !== (pendingCourse() ?? course().code) && !racing && !lobby.race) {
      const url = new URL(location.href);
      url.search = '';
      url.searchParams.set('course', lobby.course);
      url.searchParams.set('rider', rider().id);
      if (rider().kind === 'cheat') url.searchParams.set('base', rider().base || 'zoe');
      url.searchParams.set('online', '1');
      url.searchParams.set('lobby', lobby.id);
      if (switchCourse) {
        leaveRace();
        switchCourse(url);
      } else location.assign(url);
      return;
    }
    if (['mp-lobbies', 'mp-connect'].includes(ui.screen)) ui.set('mp-lobby');
    const me = lobby.members.find((m) => m.id === client.state.id);
    if (me) refreshProfile(); // a rider / skin base / outfit change since (only sent when it differs)
  });
  client.on('error', (message) => ui.mpUI?.flash(message));
  client.on('status', (s) => {
    if (s === 'version') ui.mpUI?.flash(client.state.error, 20000);
  });
  // After a reconnect: a race this page is still running continues; a race this page lost (reload) is a DNF.
  client.on('welcome', (m) => {
    const r = m.race;
    if (r && !r.finished && !(racing && race?.id === r.id)) client.finish(0, true, 'reload');
  });

  // Host started: load the event (original loading screen), then report loaded and wait for the shared GO time.
  client.on('start', async (r) => {
    if (courseReady) {
      starting = true;
      try {
        await courseReady();
      } finally {
        starting = false;
      }
    } // a course switch still loading
    leaveRace();
    race = { ...r, slot: client.state.slot };
    racing = true;
    if (ui.mpUI) ui.mpUI.onlineResults = true;
    goLocal = 0;
    reportedFinish = false;
    finished = new Map();
    finalResults = null;
    const others = r.players.filter((p) => p.slot !== client.state.slot);
    const work = (async () => {
      [npcDoc, lineups, gridScales] = await Promise.all(
        ['npc-riders.json', 'lineups.json', 'grid-scales.json'].map((f) => load(course().root + f).catch(() => null))
      );
      if (!others.length) return;
      // Every other racer exactly as its client rides it: its character (a cheat skin over its base rider) in its
      // worn outfit (wardrobe.js remoteOutfitRider builds the same outfit package here).
      const entries = await Promise.all(
        others.map(async (p) => {
          const known = (ui.riders || []).find((e) => e.id === p.rider);
          const pkg = /^RIDER_[A-Z0-9_]+$/.test(p.pkg ?? '')
            ? p.pkg
            : (known?.package ?? 'RIDER_' + String(p.rider || 'zoe').toUpperCase());
          let entry = { ...(known || {}), id: p.rider, name: p.name, package: pkg };
          if (p.base) entry = { ...entry, kind: 'cheat', base: p.base };
          const resolved = await remoteOutfitRider(entry, p.outfit ?? null);
          return { slot: p.slot, name: p.name, entry: resolved, root: resolved.root || `/assets/${resolved.package}/` };
        })
      );
      // opponent-riders.js loads <root><package>/: the package is given as its full folder (default or outfit).
      renderer = await createOpponentRiders({
        T,
        scene,
        load,
        loader,
        origin,
        root: '',
        packages: entries.map((e) => e.root.replace(/\/$/, ''))
      });
      await Promise.all(
        entries.map(async (e) => {
          remote.add(e.slot, await load(`${e.root}rider.json`), { name: e.name, rider: e.entry });
        })
      );
      order = entries.map((e) => e.slot);
      // Their rider effects (web/net/remote-fx.js): a puppet core each, fed by the streamed FX records, with the
      // rider's own settings (a skin composed over its base, the outfit's values laid over).
      const initial = await load(course().initial);
      remoteFx = await createRemoteFx({
        T,
        scene,
        origin,
        load,
        environment: lighting?.meta && lighting?.bytes ? { meta: lighting.meta, bytes: lighting.bytes } : null,
        entries: entries.map((e) => ({ slot: e.slot, pkg: e.entry.package, root: e.root })),
        settingsFor: async (x) =>
          JSON.stringify(humanSettings(initial, await loadCharacter(entries.find((e) => e.slot === x.slot).entry).catch(() => null)))
      }).catch((e) => {
        console.warn('Online rider effects unavailable', e);
        return null;
      });
    })().catch((e) => console.warn('Online riders unavailable', e));
    ui.loadEvent(() => {
      if (!racing || race?.id !== r.id) return;
      ui.set('game');
      startRun();
      client.loaded();
    }, [work]);
  });
  client.on('go', (at) => {
    goLocal = client.localTimeOf(at);
    if (race) race.goAt = at;
  });
  client.on('clock', () => {
    if (race?.goAt) goLocal = client.localTimeOf(race.goAt);
  });
  client.on('state', (frame) => {
    if (!racing) return;
    if (frame[1] === ATTACK) {
      const e = decodeFrame(frame);
      if (e && e.slot === e.attacker) {
        stats.receivedAttacks++;
        pairNet?.receiveAttack(e);
      }
      return;
    }
    if (frame[1] === STATE) {
      const s = remote.receive(frame);
      if (s?.fx) remoteFx?.receive(s);
      return;
    }
    if (frame[1] === WORLD) {
      const w = decodeFrame(frame);
      if (w) worldQueue.push(w);
    }
  });
  client.on('presence', (m) => {
    if (race && m.slot !== race.slot) remote.markGone(m.slot, !m.online);
  });
  client.on('finished', (m) => {
    if (!race) return;
    finished.set(m.slot, { ticks: m.ticks, dnf: m.dnf, reason: m.reason });
    if (m.dnf && m.slot !== race.slot) {
      if (['quit', 'left', 'late', 'reload', 'disconnected'].includes(m.reason)) remote.hide(m.slot);
      else remote.markGone(m.slot, m.reason !== 'time');
    }
    // Dropped for loading too late: wait in the lobby for the next race.
    if (m.slot === race.slot && m.dnf && m.reason === 'late' && racing) {
      leaveRace();
      if (ui.mpUI) ui.mpUI.onlineResults = false;
      ui.set('mp-lobby');
      ui.mpUI?.flash('Loading took too long: you race in the next one', 6000);
    }
  });
  client.on('results', (results) => {
    finalResults = results;
    racing = false;
    if (['game', 'pause', 'results'].includes(ui.screen)) ui.mpUI?.showResults();
  });

  // Arrived from an invite link or a course reload: straight to the lobby.
  if (params.get('online') === '1' || pendingLobby)
    queueMicrotask(() => {
      ui.onlineMode = true;
      ui.mpUI?.enter();
    });

  // Hidden tab: requestAnimationFrame stops, timers are throttled to 1 Hz; a worker's timer is not.
  let ticker = null;
  function startTicker() {
    if (ticker || typeof Worker === 'undefined' || !advance) return;
    try {
      const code = `let t=setInterval(()=>postMessage(0),${BACKGROUND_MS});onmessage=(e)=>{if(e.data==='stop'){clearInterval(t);close();}}`;
      ticker = new Worker(URL.createObjectURL(new Blob([code], { type: 'text/javascript' })));
      ticker.onmessage = () => {
        if (!racing || performance.now() - lastFrameMs <= HELD_FRAME_MS) return;
        const t0 = performance.now(),
          k0 = pairNet?.tick ?? 0;
        stats.backgroundTicks++;
        try {
          advance();
        } catch (e) {
          console.warn('mp: background tick failed', e);
        }
        stats.backgroundSimTicks = (stats.backgroundSimTicks ?? 0) + (pairNet?.tick ?? 0) - k0;
        stats.backgroundMs = Math.round((stats.backgroundMs ?? 0) + performance.now() - t0);
      };
    } catch (e) {
      console.warn('mp: no background ticker', e);
      ticker = null;
    }
  }
  function stopTicker() {
    ticker?.postMessage('stop');
    ticker = null;
  }

  function leaveRace() {
    stopTicker();
    // Back to the rider's own grid seed (npc_gameplay.inc: clearing restores the settings seed init_animation installed).
    const c = getCore();
    if (seedSet && c?._human_grid_seed) {
      const p = c._malloc(1);
      c.HEAPU8[p] = 0;
      c._human_grid_seed(p);
      c._free(p);
      seedSet = false;
    }
    remoteFx?.dispose();
    remoteFx = null;
    worldQueue = [];
    sentTriggers = new Set();
    getCore()?._fx_recording?.(0);
    pairNet?.dispose();
    pairNet = null;
    renderer?.dispose();
    renderer = null;
    order = [];
    remote.clear();
    racing = false;
    goLocal = 0;
    const core = getCore();
    if (lightPtr && core) {
      core._free(lightPtr);
      lightPtr = 0;
    }
  }
  function sendState() {
    const tick = pairNet.tick - 1,
      fx = fxPending.map((r) => ({ back: tick - r.tick, values: r.values, reset: r.reset }));
    fxPending = [];
    const core = getCore(),
      s = captureState(core, {
        tick,
        finished: isFinished(),
        dnf: !!core._race_timed_out?.(),
        lightingConfig: lighting?.configuration ?? null,
        fx
      });
    if (!s) {
      stats.skipped = 'no pose';
      return;
    }
    const bytes = encodeState(s);
    if (client.sendState(bytes)) {
      stats.sentStates++;
      stats.sentBytes += bytes.length;
      stats.lastPacket = bytes.length;
    }
  }
  // 0x122D78 estimates for racers still on course; recorded times for the finished; DNF last.
  function resultRows(raceTicks) {
    if (!race) return [];
    const core = getCore(),
      progress = core ? new Float32Array(core.HEAPF32.buffer, core._race_progress_info(), 8) : null;
    const origins = [npcDoc?.riders[0]?.progress_origin, ...(npcDoc?.riders ?? []).map((x) => x.progress_origin)];
    const rows = race.players.map((p) => {
      const s = p.slot,
        mine = s === race.slot,
        f = finished.get(s),
        fin = finalResults?.find((x) => x.slot === s);
      let ticks = null,
        dnf = false,
        estimated = false;
      if (fin) {
        dnf = fin.dnf;
        ticks = fin.ticks;
      } else if (f) {
        dnf = f.dnf;
        ticks = f.ticks;
      } else if (mine && progress && progress[3] >= 0) {
        ticks = resultTicks();
        dnf = !!core._race_timed_out?.();
      } else {
        const latest = mine ? { remaining: progress?.[0] } : remote.racers.get(s)?.latest;
        const originCm = origins[s] ?? latest?.remaining ?? 0;
        if (latest && Number.isFinite(latest.remaining)) {
          ticks = estimateFinishTicks(raceTicks, originCm, latest.remaining, s);
          estimated = true;
        } else dnf = true;
      }
      return { slot: s, name: p.name, human: mine, ticks, dnf, estimated, reason: fin?.reason ?? f?.reason ?? '' };
    });
    const timed = rows.filter((r) => !r.dnf),
      dnfs = rows.filter((r) => r.dnf);
    const sorted = orderByTime(timed.map((r) => r.ticks)).map((i) => timed[i]);
    return [...sorted, ...dnfs].map((r, place) => ({ ...r, place }));
  }

  return {
    session,
    client,
    remote,
    stats,
    get racing() {
      return racing;
    },
    get pairs() {
      return pairNet?.counts ?? null;
    },
    aiAllowed() {
      return !racing;
    },
    // startRun, before core._start_event(): the grid slot and the online race world.
    beforeStart(core) {
      pairNet?.dispose();
      pairNet = null;
      if (!racing || !race) return; // off-line runs keep the rider's own grid seed (and ai-race.js's per course)
      // The racer's countdown slot, on the spot a rider of its own body scale and stance takes there (grid-seed.js).
      if (core?._human_grid_seed) {
        const r = rider(),
          scale = new Float32Array(core.HEAPF32.buffer, core._rider_skin_scale(), 1)[0];
        const grid = onlineGridState({
          lineups,
          gridScales,
          rider: r,
          humanBase: humanCharacter(r, ui.riders || []).base,
          scale,
          slot: race.slot
        });
        let text = grid ? JSON.stringify(grid.state) : '';
        if (!grid && race.slot > 0) {
          const g = npcDoc?.riders.find((x) => x.slot === race.slot);
          if (g) text = JSON.stringify(g.ground.state);
        } // a course without lineup data
        stats.grid = grid ? { slot: race.slot, exact: grid.exact, position: grid.state.position } : { slot: race.slot, fallback: !!text };
        if (text || seedSet) {
          const bytes = new TextEncoder().encode(text + '\0'),
            p = core._malloc(bytes.length);
          core.HEAPU8.set(bytes, p);
          try {
            core._human_grid_seed(p);
          } finally {
            core._free(p);
          }
        }
        seedSet = !!text;
      }
      const inputs = [];
      for (const p of race.players) inputs[p.slot] = p.pair ?? DEFAULT_PAIR;
      pairNet = createPairNet({
        core,
        remote,
        slot: race.slot,
        count: race.players.length,
        pairInputs: inputs,
        seed: race.seed >>> 0,
        sendAttack: (e) => {
          stats.sentAttacks++;
          client.sendState(encodeAttack(e), { essential: true });
        }
      });
      // This rider's FX inputs are recorded for the others (web/fx_puppet.inc); the shared world log starts empty.
      core._fx_recording?.(1);
      drainFx(core);
      core._world_events?.();
      sentTriggers = new Set();
      worldQueue = [];
      fxPending = [];
      resetCounters = null;
      pickups = createPickupArbiter({ slot: race.slot });
      if (core._world_triggers) {
        const p = core._world_triggers(),
          n = new Uint32Array(core.HEAPU8.buffer, p, 1)[0];
        for (const k of new Uint32Array(core.HEAPU8.buffer, p + 4, n)) sentTriggers.add(k);
      }
      startTicker();
    },
    // Hold the countdown until the server's GO moment (every racer starts on the same wall-clock instant).
    holding() {
      return racing && (!goLocal || Date.now() < goLocal);
    },
    // Online races do not pause: the pause menu opens over a race that keeps running (neutral pad).
    simulating() {
      return racing && !!pairNet;
    },
    // Keep the race tick on the server clock (frame hitches, a slow tab, clock drift; web/net/race-pace.js).
    // pending = the frame clock's debt (web/fixed-step-clock.js): already scheduled, not asked for again.
    pace(dt, pending = 0) {
      if (!racing || !goLocal || !pairNet) return dt;
      const seconds = paceSeconds({ expected: (Date.now() - goLocal) * 0.06, tick: pairNet.tick, dt, pending });
      if (seconds !== dt) stats.paceTicks++;
      return seconds;
    },
    beginTick() {
      if (!pairNet) return;
      // Shared world changes other racers made (web/shared_world.inc replays, as between the six AI cores).
      const core = getCore();
      for (const w of worldQueue.splice(0)) {
        for (const k of w.triggers)
          if (!sentTriggers.has(k)) {
            sentTriggers.add(k);
            core._world_trigger_apply?.(k);
          }
        for (let at = 0; at < w.events.length; ) {
          const len = 2 + w.events[at + 1];
          if (at + len > w.events.length) break;
          // Two racers took the same boost pickup inside the latency: the later take gives it back (pickup-arbiter.js).
          if (w.events[at] === 2 && pickups?.remote(w.events[at + 2], w.tick, w.slot) && core._pickup_revoke) {
            core._pickup_revoke(w.events[at + 2]);
            stats.pickupsRevoked = (stats.pickupsRevoked ?? 0) + 1;
          }
          const q = core._malloc(len * 4);
          new Uint32Array(core.HEAPU8.buffer, q, len).set(w.events.subarray(at, at + len));
          try {
            core._world_event_apply(q);
            stats.worldApplied = (stats.worldApplied ?? 0) + 1;
          } catch (e) {
            console.warn('mp: shared world event failed', e);
          } finally {
            core._free(q);
          }
          at += len;
        }
      }
      pairNet.beginTick();
    },
    endTick() {
      if (!pairNet) return;
      pairNet.endTick();
      // This tick's FX pass record, tagged with the reset this core made during the tick (before its FX pass): a
      // rescue (main.js restarts the rider animation) or a reset placement 111890 -- the puppets repeat it.
      const core0 = getCore(),
        counters = [
          new Float32Array(core0.HEAPF32.buffer, core0._reset_info(), 3)[2],
          new Float32Array(core0.HEAPF32.buffer, core0._rider_state(), 16)[13]
        ];
      const reset = !resetCounters ? 0 : counters[1] !== resetCounters[1] ? 2 : counters[0] !== resetCounters[0] ? 1 : 0;
      resetCounters = counters;
      const recs = drainFx(core0);
      recs.forEach((r, i) =>
        fxPending.push({ tick: pairNet.tick - 1 - r.back, values: r.values, reset: i === recs.length - 1 ? reset : 0 })
      );
      if (fxPending.length > 30) fxPending.splice(0, fxPending.length - 30);
      // This racer's shared world changes (pickups taken, crashbags kicked, triggers, sections) to the others.
      const core = getCore(),
        triggers = [];
      if (core._world_triggers) {
        const p = core._world_triggers(),
          n = new Uint32Array(core.HEAPU8.buffer, p, 1)[0];
        for (const k of new Uint32Array(core.HEAPU8.buffer, p + 4, n))
          if (!sentTriggers.has(k)) {
            sentTriggers.add(k);
            triggers.push(k);
          }
      }
      const ep = core._world_events?.(),
        en = ep ? new Uint32Array(core.HEAPU8.buffer, ep, 1)[0] : 0;
      if (en) {
        const ev = new Uint32Array(core.HEAPU8.buffer, ep + 4, en);
        for (let at = 0; at < en; at += 2 + ev[at + 1]) if (ev[at] === 2) pickups?.took(ev[at + 2], pairNet.tick - 1);
      }
      pickups?.prune(pairNet.tick - 1);
      if (triggers.length || en) {
        client.sendState(
          encodeWorld({
            tick: pairNet.tick - 1,
            triggers,
            events: en ? new Uint32Array(core.HEAPU8.buffer, ep + 4, en).slice() : new Uint32Array(0)
          }),
          { essential: true }
        );
        stats.sentWorld = (stats.sentWorld ?? 0) + 1;
      }
      if (pairNet.tick % SEND_EVERY === 1) sendState();
    },
    afterFrame(fromBackground = false) {
      stats.frames++;
      if (!fromBackground) lastFrameMs = performance.now();
      if (!racing || !isRunning()) {
        stats.skipped = racing ? 'not running' : 'not racing';
        return;
      }
      const core = getCore();
      if (!reportedFinish && isFinished()) {
        reportedFinish = true;
        const dnf = !!core._race_timed_out?.();
        client.finish(dnf ? 0 : resultTicks(), dnf, dnf ? 'gave up' : '');
      }
    },
    render(alpha = 1) {
      if (!renderer || !pairNet) return;
      // The local rider is drawn between the states after ticks n-2 and n-1 (pairNet.tick = n); remote packets are
      // stamped with the tick whose end state they carry.
      const opponents = remote.frame(order, pairNet.tick - 2 + alpha);
      renderer.capture(opponents);
      renderer.update(opponents, 1);
      const cam = camera();
      if (remoteFx && cam) remoteFx.frame(remote, pairNet.tick - 2 + alpha - DELAY_TICKS, cam);
      // Each remote rider's own original lighting (its streamed irradiance, this client's light world and camera).
      const core = getCore();
      if (!lighting?.configuration || !core?._shade_external_rider_lighting) return;
      const viewPtr = core._camera_render_view();
      if (!viewPtr) return;
      if (!lightPtr) lightPtr = core._malloc(4 * (6 + 2 + 3 + 40 + 4 + 5));
      const at = lightPtr >> 2;
      renderer.entries.forEach((entry, i) => {
        const l = remote.lighting(order[i]);
        if (!l || !entry.group.visible) return;
        const H = core.HEAPF32;
        H.set([l.bounds[0], l.bounds[1], l.bounds[2], 1, l.bounds[3], l.bounds[4], l.bounds[5], 1], at);
        H.set(l.point, at + 8);
        H.set(l.irradiance, at + 11);
        H.set([l.point[0], l.point[1], l.point[2], 1], at + 51);
        H.set(lighting.configuration.rim_constants, at + 55);
        const ptr = core._shade_external_rider_lighting(
          lightPtr,
          lightPtr + 32,
          lightPtr + 44,
          viewPtr,
          lightPtr + 204,
          l.rim,
          lightPtr + 220
        );
        entry.lighting.capture({ HEAPF32: core.HEAPF32, _rider_lighting_gpu_coefficients: () => ptr });
      });
    },
    hud() {
      return racing ? (pairNet?.hud() ?? null) : null;
    },
    // Background (hidden tab) step from the worker: main.js advances the simulation and calls afterFrame(true).
    get backgroundActive() {
      return !!ticker;
    },
    leaveRace,
    // QA (?perf=1): race state, remote riders received/drawn, pairs, bandwidth.
    debug() {
      return {
        screen: ui.screen,
        stats: { ...stats },
        racing,
        slot: race?.slot ?? -1,
        tick: pairNet?.tick ?? null,
        goIn: goLocal ? goLocal - Date.now() : null,
        order: [...order],
        rtt: client.state.rtt,
        offset: client.state.offset,
        sent: { frames: client.state.sentFrames, bytes: client.state.sentBytes },
        remote: { ...remote.stats },
        pairs: pairNet
          ? {
              ...pairNet.counts,
              rank: pairNet.rank(race.slot),
              mask: pairNet.disabledMask,
              rankInputs: pairNet.rankInputs,
              ranks: race.players.map((p) => pairNet.rank(p.slot))
            }
          : null,
        received: [...remote.racers.entries()].map(([slot, r]) => ({
          slot,
          snapshots: r.snapshots.length,
          tick: r.latest?.tick,
          position: r.latest?.position,
          remaining: r.latest?.remaining,
          gone: r.gone
        })),
        drawn: renderer ? renderer.entries.map((e) => ({ name: e.name, visible: e.group.visible, captured: e.captured })) : [],
        finished: [...finished.entries()],
        hud: pairNet?.hud() ?? null,
        fx: remoteFx ? { ...remoteFx.stats } : null,
        pickups: pickups ? { ...pickups.stats } : null
      };
    }
  };
}
