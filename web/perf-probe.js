// Frame-cost probe for web/bench-free-roam.mjs (docs/sim-performance.md "Free-roam steady state"). QA only: the bench injects
// it into the page (Chrome: before the page's scripts; WebKit: right after load); the game never imports it.
// Per drawn frame: rAF callback ms, simulation ms (FixedStepClock.advance) and ticks, core export ms by name (exclusive),
// renderer.render ms, free-ride update / set-piece update ms, GPU counters (passes, pipelines set / switched, bind groups,
// draws, triangles, buffer bytes written), GPU time (timestamp queries if the device has them, else submit->done),
// visible locations, draws per location, GC probe hits, long tasks.
(() => {
  if (window.__cp) return;
  const now = () => performance.now();
  const P = window.__cp = { frames: [], gc: [], longtasks: [], on: false, info: {}, errors: [], label: '' };
  // ---------------- GPU counters ----------------
  let F = null; // current frame record
  const G = { passes: 0, pipes: 0, pipeSwitch: 0, binds: 0, draws: 0, tris: 0, wbytes: 0, submits: 0 };
  const g = globalThis;
  let tsDevice = null, tsSet = null, tsIndex = 0, tsFree = [], tsPending = 0; const TS_MAX = 1024;
  if (g.GPUCommandEncoder) {
    const D = GPUDevice.prototype, E = GPUCommandEncoder.prototype, R = GPURenderPassEncoder.prototype, Q = GPUQueue.prototype;
    const cce = D.createCommandEncoder;
    D.createCommandEncoder = function (d) { const e = cce.call(this, d); e.__dev = this; return e; };
    const brp = E.beginRenderPass;
    E.beginRenderPass = function (desc) {
      G.passes++;
      const dev = this.__dev;
      if (P.timestamps && dev && !desc.timestampWrites && dev.features?.has?.('timestamp-query')) {
        try {
          if (tsDevice !== dev) { tsDevice = dev; tsSet = dev.createQuerySet({ type: 'timestamp', count: TS_MAX }); tsIndex = 0; }
          if (tsIndex + 2 <= TS_MAX) { desc = { ...desc, timestampWrites: { querySet: tsSet, beginningOfPassWriteIndex: tsIndex, endOfPassWriteIndex: tsIndex + 1 } }; tsIndex += 2; }
        } catch (e) { P.errors.push('ts ' + e); P.timestamps = false; }
      }
      const pass = brp.call(this, desc); pass.__lastPipe = null; return pass;
    };
    const sp = R.setPipeline; R.setPipeline = function (p) { G.pipes++; if (p !== this.__lastPipe) { G.pipeSwitch++; this.__lastPipe = p; } return sp.call(this, p); };
    const sb = R.setBindGroup; R.setBindGroup = function (...a) { G.binds++; return sb.apply(this, a); };
    const dr = R.draw; R.draw = function (v, i = 1, ...a) { G.draws++; G.tris += (v / 3) * i; return dr.call(this, v, i, ...a); };
    const di = R.drawIndexed; R.drawIndexed = function (c, i = 1, ...a) { G.draws++; G.tris += (c / 3) * i; return di.call(this, c, i, ...a); };
    const dind = R.drawIndirect; R.drawIndirect = function (...a) { G.draws++; return dind.apply(this, a); };
    const didx = R.drawIndexedIndirect; R.drawIndexedIndirect = function (...a) { G.draws++; return didx.apply(this, a); };
    const wb = Q.writeBuffer; Q.writeBuffer = function (b, o, d, off, size) { G.wbytes += size ?? d?.byteLength ?? 0; return wb.call(this, b, o, d, off, size); };
    const sub = Q.submit; Q.submit = function (cbs) { G.submits++; const r = sub.call(this, cbs);
      if (F && !P.timestamps) { const f = F, t0 = now(); f.__sub ??= t0; this.onSubmittedWorkDone().then(() => { f.gpuDone = now(); }); }
      return r; };
  }
  // Resolve this frame's pass timestamps: sum of (end - begin) per pass, and first begin .. last end.
  function flushTimestamps(rec) {
    if (!P.timestamps || !tsDevice || !tsIndex) return;
    const n = tsIndex; tsIndex = 0;
    try {
      const dev = tsDevice, size = n * 8;
      const resolve = dev.createBuffer({ size: TS_MAX * 8, usage: GPUBufferUsage.QUERY_RESOLVE | GPUBufferUsage.COPY_SRC });
      let read = tsFree.pop(); if (!read) read = dev.createBuffer({ size: TS_MAX * 8, usage: GPUBufferUsage.MAP_READ | GPUBufferUsage.COPY_DST });
      const enc = dev.createCommandEncoder(); enc.resolveQuerySet(tsSet, 0, n, resolve, 0); enc.copyBufferToBuffer(resolve, 0, read, 0, size);
      const s0 = G.submits; dev.queue.submit([enc.finish()]); G.submits = s0;
      tsPending++;
      read.mapAsync(GPUMapMode.READ, 0, size).then(() => {
        const v = new BigUint64Array(read.getMappedRange(0, size)); let sum = 0, first = null, last = null;
        for (let k = 0; k + 1 < n; k += 2) { const b = v[k], e = v[k + 1]; if (b === 0n || e === 0n || e < b) continue; sum += Number(e - b); if (first === null || b < first) first = b; if (last === null || e > last) last = e; }
        rec.gpu = sum / 1e6; rec.gpuSpan = first !== null ? Number(last - first) / 1e6 : 0; read.unmap(); tsFree.push(read); resolve.destroy(); tsPending--;
      }).catch((e) => { P.errors.push('map ' + e); resolve.destroy(); tsPending--; });
    } catch (e) { P.errors.push('flush ' + e); P.timestamps = false; }
  }
  // ---------------- core export timing (exclusive) ----------------
  const stack = []; let wrappedCore = null;
  const SKIP = new Set(['_malloc', '_free', '_emscripten_stack_get_current', '_emscripten_stack_restore', '_emscripten_stack_alloc']);
  let inSim = false;
  function wrapFn(obj, key, bucket, label) {
    const f = obj[key]; if (typeof f !== 'function' || f.__cp) return;
    const w = function (...a) {
      if (!F) return f.apply(this, a);
      const t0 = now(); stack.push(0);
      try { return f.apply(this, a); } finally {
        const el = now() - t0, child = stack.pop(), ex = el - child;
        if (stack.length) stack[stack.length - 1] += el;
        const b = inSim ? F.exp : F.expF; b[label] = (b[label] || 0) + ex; F.expCalls++;
      }
    };
    w.__cp = f; obj[key] = w;
  }
  function wrapCore(core) {
    if (!core || core === wrappedCore) return; wrappedCore = core;
    for (const k of Object.keys(core)) if (k[0] === '_' && k[1] !== '_' && !SKIP.has(k) && typeof core[k] === 'function' && !/^_emscripten|^_stack/.test(k)) wrapFn(core, k, 'exp', k);
  }
  // ---------------- JS spans (inclusive, by name) ----------------
  function span(obj, key, label) {
    if (!obj) return; const f = obj[key]; if (typeof f !== 'function' || f.__cps) return;
    let depth = 0;
    const w = function (...a) { if (!F || depth) return f.apply(this, a); const t0 = now(); depth++; try { return f.apply(this, a); } finally { depth--; if (F) F.js[label] = (F.js[label] || 0) + (now() - t0); } };
    w.__cps = f; obj[key] = w;
  }
  let wrappedRenderer = null, wrappedFr = null, wrappedSp = null, wrappedEsp = null;
  async function wrapModules() {
    try {
      const m = await import('/fixed-step-clock.js'); const C = m.FixedStepClock.prototype, adv = C.advance;
      if (!adv.__cps) { C.advance = function (sec, cb) { if (!F) return adv.call(this, sec, cb); const t0 = now(); let n = 0; inSim = true; try { return adv.call(this, sec, (...a) => { n++; return cb(...a); }); } finally { inSim = false; F.sim += now() - t0; F.ticks += n; } }; C.advance.__cps = adv; }
    } catch (e) { P.errors.push('clock ' + e); }
    for (const [url, name, methods] of [['/livecomp-animation.js', 'LiveCompAnimation', ['tick', 'nodeDeltas']], ['/flag-animation.js', 'FlagAnimation', ['tick', 'writeWorld']], ['/uv-scroll.js', 'UvScroll', ['tick']]]) {
      try { const m = await import(url); const C = m[name]?.prototype; if (C) for (const k of methods) span(C, k, name + '.' + k); } catch (e) { P.errors.push(url + ' ' + e); }
    }
  }
  wrapModules();
  // per-location draw attribution (three backend.draw)
  const locOf = (o) => { if (o.__loc !== undefined) return o.__loc; let loc = null; for (let q = o; q; q = q.parent) { if (q.userData?.peakLocation) { loc = q.userData.peakLocation; break; } if (q.name && q.name !== 'static-cell') { loc = q.name; break; } if (q.userData?.shadowRider) { loc = 'rider'; break; } if (q.parent?.isScene) { loc = 'top:' + q.type + (q.userData?.courseHash ? ':world' : ''); break; } } o.__loc = loc ?? (o.type || '?'); return o.__loc; };
  function hookRenderer(r) {
    if (!r || r === wrappedRenderer) return; wrappedRenderer = r;
    span(r, 'render', 'render'); span(r, 'compute', 'compute');
    // node-material builds (three NodeManager.nodeBuilderCache insertions) per frame
    const nbc = r._nodes?.nodeBuilderCache; if (nbc && !nbc.__cp) { const set = nbc.set.bind(nbc); nbc.set = (k, v) => { if (F) F.builds = (F.builds || 0) + 1; else P.buildsOutside = (P.buildsOutside || 0) + 1; return set(k, v); }; nbc.__cp = true; }
    // which materials build during recorded frames (P.buildLog: time, material, object, location, ms)
    const N = r._nodes, gfr = N?.getForRender; if (gfr && !gfr.__cp) { N.getForRender = function (ro, ...a) { const had = !!this.get(ro)?.nodeBuilderState, t0 = now(); const s = gfr.call(this, ro, ...a); if (!had && F && P.on) { const ms = now() - t0; if (ms > 1) (P.buildLog ??= []).push([Math.round(t0), ro.material?.name || ro.material?.type, ro.material?.userData?.originalWorldCombine || '', ro.object?.name || ro.object?.type, locOf(ro.object), +ms.toFixed(1)]); } return s; }; N.getForRender.__cp = gfr; }
    const b = r.backend, d = b?.draw;
    if (d && !d.__cps) { b.draw = function (ro, info) { if (F && P.byLoc) { const k = locOf(ro.object); F.loc[k] = (F.loc[k] || 0) + 1; } return d.call(this, ro, info); }; b.draw.__cps = d; }
  }
  // ---------------- GC probe / long tasks ----------------
  const fin = g.FinalizationRegistry ? new FinalizationRegistry((t) => P.gc.push([now(), t])) : null;
  try { new PerformanceObserver((l) => { for (const e of l.getEntries()) P.longtasks.push([e.startTime, e.duration]); }).observe({ type: 'longtask', buffered: false }); } catch {}
  // ---------------- virtual pad + autopilot (along AIP race paths, source cm) ----------------
  const buttons = Array.from({ length: 17 }, () => ({ pressed: false, touched: false, value: 0 }));
  const pad = { id: 'cp-virtual (STANDARD GAMEPAD Vendor: 054c Product: 09cc)', index: 0, connected: true, mapping: 'standard', axes: [0, 0, 0, 0], buttons, timestamp: 0, vibrationActuator: null };
  const realPads = navigator.getGamepads?.bind(navigator);
  navigator.getGamepads = () => (P.usePad ? [pad, null, null, null] : (realPads ? realPads() : []));
  P.press = (i, on) => { buttons[i].pressed = on; buttons[i].value = on ? 1 : 0; pad.timestamp = now(); };
  P.pad = pad;
  const dist = (a, b) => Math.hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]);
  function nearest(pos) { let best = null; (P.paths || []).forEach((p, pi) => { const pts = p.pts; for (let k = 0; k < pts.length - 1; k++) { const [ax, ay, az] = pts[k], [bx, by, bz] = pts[k + 1], vx = bx - ax, vy = by - ay, vz = bz - az, L2 = vx * vx + vy * vy + vz * vz || 1; const u = Math.max(0, Math.min(1, ((pos[0] - ax) * vx + (pos[1] - ay) * vy + (pos[2] - az) * vz) / L2)); const d = dist(pos, [ax + u * vx, ay + u * vy, az + u * vz]); if (!best || d < best[0]) best = [d, pi, k, u]; } }); return best; }
  function ahead(pi, k, u, d) { const p = P.paths[pi].pts; const [x0, y0, z0] = p[k], [x1, y1, z1] = p[k + 1]; let cur = [x0 + u * (x1 - x0), y0 + u * (y1 - y0), z0 + u * (z1 - z0)]; const seen = new Set();
    for (;;) { const nxt = P.paths[pi].pts[k + 1], seg = dist(cur, nxt); if (seg >= d) { const f = seg ? d / seg : 0; return cur.map((c, i) => c + f * (nxt[i] - c)); } d -= seg; cur = nxt; k++;
      if (k + 1 >= P.paths[pi].pts.length) { seen.add(pi); let cand = null; P.paths.forEach((q, j) => { if (j === pi || seen.has(j) || q.pts[0][2] > cur[2] + 500) return; const dd = dist(cur, q.pts[0]); if (!cand || dd < cand[0]) cand = [dd, j]; }); if (!cand || cand[0] > 20000) return cur; pi = cand[1]; k = 0; cur = P.paths[pi].pts[0]; } } }
  function autopilot(core) {
    const m = new Float32Array(core.HEAPF32.buffer, core._reference_motion.__cp ? core._reference_motion.__cp() : core._reference_motion(), 20);
    const pos = [m[0], m[1], m[2]], vel = [m[3], m[4], m[5]]; P.pos = pos;
    const b = nearest(pos); if (!b) return;
    const tgt = ahead(b[1], b[2], b[3], P.look ?? 2500), h = Math.atan2(vel[1], vel[0]), tt = Math.atan2(tgt[1] - pos[1], tgt[0] - pos[0]);
    const err = ((tt - h + Math.PI) % (2 * Math.PI) + 2 * Math.PI) % (2 * Math.PI) - Math.PI;
    let lx = Math.max(-1, Math.min(1, -(P.gain ?? 2.5) * err)); if (Math.hypot(vel[0], vel[1]) < 200) lx = Math.max(-1, Math.min(1, -2 * err));
    pad.axes[0] = lx; pad.axes[1] = -1; pad.timestamp = now();
  }
  // ---------------- rAF wrapper: one record per rAF timestamp ----------------
  const raf = window.requestAnimationFrame.bind(window);
  let frameTs = -1, lastDrawn = 0;
  function begin(ts) {
    for (const k in G) G[k] = 0;
    F = { t: 0, ts, cb: 0, sim: 0, ticks: 0, exp: {}, expF: {}, expCalls: 0, js: {}, loc: {}, gpu: null };
  }
  function end() {
    const f = F; F = null; if (!f) return;
    const drawn = G.passes > 0;
    Object.assign(f, G);
    f.t = now();
    if (drawn) { f.iv = lastDrawn ? f.t - lastDrawn : 0; lastDrawn = f.t; }
    if (drawn) flushTimestamps(f);
    if (fin) { const o = {}; fin.register(o, f.t); }
    if (P.on && drawn) {
      f.label = P.label;
      try { const fr = window.__freeRide; if (fr) { f.region = fr.region?.(); f.course = fr.course?.(); const vis = []; for (const [c, s] of fr.render) if (s.group?.visible) vis.push(c); f.vis = vis.join(','); f.built = fr.render.size; f.stalled = fr.stalled?.() ? 1 : 0; } } catch {}
      try { const pc = window.__perfCore?.(); if (pc?.core) { const s = pc.state; f.speed = s?.[7]; f.pos = P.pos ? P.pos.map((x) => Math.round(x)) : null; } const ui = window.__perfUI?.(); f.screen = ui?.screen; f.cs = window.__cutscenes?.active ? 1 : 0; } catch {}
      delete f.ts; P.frames.push(f);
    }
  }
  window.requestAnimationFrame = function (cb) {
    return raf((ts) => {
      if (ts !== frameTs) { if (F) end(); frameTs = ts; begin(ts); }
      // hooks that need the live objects
      try { const pc = window.__perfCore?.(); if (pc?.core) { wrapCore(pc.core); if (P.auto && P.usePad && window.__perfUI?.()?.screen === 'game') autopilot(pc.core); } } catch (e) { P.errors.push('pc ' + e); }
      try { hookRenderer(window.__perfRenderer); } catch (e) { P.errors.push('r ' + e); }
      try { const fr = window.__freeRide; if (fr && fr !== wrappedFr) { wrappedFr = fr; span(fr, 'update', 'fr.update'); span(fr, 'tick', 'fr.tick'); span(fr.peak, 'pump', 'peak.pump'); }
        const sp = fr?.setPieces?.(); if (sp && sp !== wrappedSp) { wrappedSp = sp; span(sp, 'update', 'sp.update'); span(sp.particles, 'update', 'sp.particles'); span(sp.halos, 'update', 'sp.halos'); } } catch (e) { P.errors.push('fr ' + e); }
      try { const esp = window.ssxQA?.setPieces?.(); if (esp && esp !== wrappedEsp) { wrappedEsp = esp; span(esp, 'update', 'esp.update'); } } catch {}
      const t0 = now();
      try { cb(ts); } finally { if (F) F.cb += now() - t0; }
    });
  };
  // close the record at the next task (after every rAF callback of the frame ran)
  const mc = new MessageChannel(); mc.port1.onmessage = () => { if (F && frameTs >= 0) end(); };
  const origRaf = window.requestAnimationFrame;
  window.requestAnimationFrame = function (cb) { const id = origRaf((ts) => { cb(ts); mc.port2.postMessage(0); }); return id; };
  P.summary = () => ({ n: P.frames.length, errors: P.errors.slice(0, 5) });
  P.take = () => { const f = P.frames; P.frames = []; const gc = P.gc; P.gc = []; const lt = P.longtasks; P.longtasks = []; const bl = P.buildLog || []; P.buildLog = []; return { frames: f, gc, longtasks: lt, buildLog: bl }; };
})();
