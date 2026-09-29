// SSX 3 (PS2) EA speech-event interpreter, re-implemented from the EE code at 0x3D6498..0x3DB9D8.
// Static RE; spec: docs/audio-logic.md section 4 (Events.evt, .hdr, dispatch). Data: tools/export_speech_events.py
// (speech/Events.evt + speech/registry.json).
//
// Public API
//   parseEvents(bytes)                     -> EventFile           (Events.evt, 0x3D6498 register / 0x3D7110 lookup)
//   parseHdr(bytes, name?)                 -> Hdr                 (.hdr, 0x3D69F0 register)
//   hdrFromExportJson(json, name?)         -> Hdr                 (web/public/assets/AUDIO/speech/<bank>.json)
//   makeBankTable(hdrs)                    -> Map<bankId, Hdr>    (registry, 0x3D6D50 lookup; order = registry index)
//   EaRng(seed)                            -> RNG 0x3DB4D0 (seeded like 0x3DB790)
//   createSpeechState()                    -> per-session state (ring 0x450B88, hdr histories, last event 0x450660)
//   resolveEvent(events, banks, eventId, args, rng, state, opts?) -> { items:[{bankId,bank,line,silenceMs?}], record, reason }
//       = 0x3D9FB0 (one event -> one record -> a sequence of lines). opts.commit (default true) applies the
//         send-time side effects of 0x3D9830/0x3D9BD8 (hdr history push, per-channel last-event/repeat count).
//   SpeechScheduler                        -> 16-slot pending-event table + dispatcher (0x3D7418/0x3D72A0/0x3D7A50/
//                                             0x3D7760/0x3D7D58/0x3D7EC8), time in speech-timer ticks (60 Hz).
//   GameRequestTable                       -> the game's 10-slot request table (0x2B1458/0x2B1520/0x2B1720).
//
// Argument convention: `args` are the game's post arguments in order (arg0 = language mask for DJ/PA events,
// speaker mask for rider events ...). Internally the engine sees A[0] = handle, A[1..] = args, 20 words total.

const u16 = (b, o) => b[o] | (b[o + 1] << 8);
const u32 = (b, o) => (b[o] | (b[o + 1] << 8) | (b[o + 2] << 16) | (b[o + 3] << 24)) >>> 0;
const s8 = (b, o) => (b[o] << 24) >> 24;
const pad4 = (x) => (x + 3) & ~3;

// ---------------------------------------------------------------- Events.evt
export function parseEvents(bytes) {
  const b = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
  if (u16(b, 0) !== 0x0c03) throw new Error('Events.evt: bad magic ' + u16(b, 0).toString(16));
  const file = {
    fileByte8: b[8], // handle byte 3 (bank/file id) -> 1
    fileByte9: b[9], // handle byte 2 -> 0
    count: u16(b, 0x10),
    nVars: b[0x12], // file-global variables (0xFF-initialised by 0x3D6498); 0 in SSX3
    sizePct: u16(b, 0x14), // 100: size threshold used only in channel mode 1 (inert)
    prioThreshold: u16(b, 0x16), // 500: priority threshold used only in channel mode 1 (inert)
    events: new Map(),
    list: [],
  };
  const varsOff = 0x18 + pad4(file.count * 2);
  file.varsOff = varsOff;
  for (let i = 0; i < file.count; i++) {
    const e = u16(b, 0x18 + 2 * i) * 4;
    const ev = parseEvent(b, e);
    ev.index = i;
    file.events.set(ev.id, ev);
    file.list.push(ev);
  }
  return file;
}

function parseEvent(b, e) {
  const nrec = b[e + 6];
  const nExt = b[e + 7] & 15;
  const nCond = s8(b, e + 7) >> 4; // signed nibble
  const ev = {
    off: e,
    id: u16(b, e),
    W: u16(b, e + 2), // max wait, speech-timer ticks (60 Hz)
    P: u16(b, e + 4), // priority
    maxRepeat: s8(b, e + 8), // >0: may not start again while it already started this many times in a row on the channel
    chance: s8(b, e + 9), // % (post-time roll)
    flags: b[e + 10], // 0x01 mode-1 filter, 0x04 survives flush, 0x08 arg-presence masks, 0x10 follow list, 0x20 append/queue
    b11: b[e + 11],
    ext: [],
    cond: [],
    records: [],
    follow: null,
    presence: null,
  };
  const recOffs = [];
  for (let r = 0; r < nrec; r++) recOffs.push(u16(b, e + 12 + 2 * r) * 4);
  let o = e + 12 + pad4(2 * nrec);
  for (let k = 0; k < nExt; k++) ev.ext.push({ id: b[o + 3 * k], arg: b[o + 3 * k + 1], b2: b[o + 3 * k + 2] });
  o += pad4(3 * nExt);
  for (let k = 0; k < Math.max(nCond, 0); k++) ev.cond.push({ b0: b[o + 3 * k], arg: b[o + 3 * k + 1], b2: b[o + 3 * k + 2] });
  o += pad4(3 * nCond);
  if (ev.flags & 0x08) {
    const codes = u32(b, o);
    const req = [];
    for (let r = 0; r < nrec; r++) req.push(u16(b, o + 4 + 2 * r));
    ev.presence = { codes, req };
    o += (2 * nrec + 7) & ~3;
  }
  if (ev.flags & 0x10) {
    const c = b[o];
    ev.follow = [];
    for (let k = 0; k < c; k++) ev.follow.push(u16(b, o + 2 + 2 * k));
  }
  for (let r = 0; r < nrec; r++) ev.records.push(parseRecord(b, e + recOffs[r], r, Math.max(nCond, 0)));
  return ev;
}

function parseRecord(b, ro, index, nCond) {
  const wb = b[ro];
  const nItems = b[ro + 4] >> 2;
  const nMasks = b[ro + 6];
  const rec = {
    index,
    off: ro,
    weightByte: wb,
    weight: (wb & 31) * (1 << (2 * (wb >> 5))), // 0x3DA3C8: (b & 0x1F) * 4^(b >> 5)
    chance: b[ro + 1],
    extMask: b[ro + 2],
    extVal: b[ro + 3],
    mode: b[ro + 4] & 3, // 0 always, 1 only when channel mode == 2, 2 unless channel mode == 2
    nState: b[ro + 5],
    nMasks,
    b7: b[ro + 7],
    masks: [],
    items: [],
  };
  const mo = ro + 8 + pad4(nItems);
  for (let k = 0; k < nMasks; k++) rec.masks.push(u32(b, mo + 4 * k));
  // cond masks used by 0x3D9ED0 are read from the same array, one per event cond descriptor
  rec.condMasks = [];
  for (let k = 0; k < nCond; k++) rec.condMasks.push(u32(b, mo + 4 * k));
  for (let k = 0; k < nItems; k++) rec.items.push(parseItem(b, ro + 4 * b[ro + 8 + k]));
  return rec;
}

function parseItem(b, io) {
  const nf = s8(b, io + 4);
  const item = {
    off: io,
    bankId: u16(b, io),
    argIdx: b[io + 2], // for type 1/2
    type: b[io + 3], // 0 bank by id, 1 bank by (id, low16(A[argIdx]) == hdr.u16[2]), 2 bank by id + variant A[argIdx]
    nf,
    param: [b[io + 8], b[io + 9], b[io + 10], b[io + 11]], // per key field: 0 any, 1..0xFD A[] index, 0xFE produce, 0xFF consume
    state: [b[io + 16], b[io + 17], b[io + 18], b[io + 19]], // per key field: 0x80|n record state word n, else file var n
    fmask: [],
  };
  for (let j = 0; j < Math.max(nf, 0); j++) item.fmask.push(u32(b, io + 20 + 4 * j));
  return item;
}

// ---------------------------------------------------------------- .hdr
export function parseHdr(bytes, name = null) {
  const b = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
  const F = b[4] & 15;
  const N = b[5];
  const H = b[6];
  const M = b[7];
  const hasPlayed = !!(b[4] & 0x80);
  const unit = 256 * (M + 1);
  const h = {
    name,
    id: u16(b, 0),
    sub: u16(b, 2), // matched by type-1 items (0xFFFF on every SSX3 bank)
    F,
    N,
    H,
    M,
    unit,
    hasPlayed,
    totalUnits: u16(b, 8),
    variants: u16(b, 10), // type-2 variant count (0 on every SSX3 bank)
    lines: [],
    masks: [],
  };
  const es = F + 2;
  for (let i = 0; i < N; i++) {
    const p = 12 + i * es;
    const keys = [];
    for (let j = 0; j < F; j++) keys.push(b[p + 2 + j]);
    const alias = (b[p] & 0x80) ? b[p + 1] : -1; // 0x3DB838: entry is an alias of line b[p+1]
    h.lines.push({ index: i, keys, alias, rawOffset: (b[p] << 8) | b[p + 1] });
  }
  const mo = 12 + pad4(N * es);
  for (let j = 0; j < F; j++) h.masks.push(u32(b, mo + 4 * j));
  let ho = mo + 4 * F;
  if (hasPlayed) {
    h.playedDivisions = b[ho];
    h.playedOff = ho;
    ho += ((N + 7) >> 3) + 1;
  }
  h.historyCursor = b[ho];
  h.history = Array.from(b.slice(ho + 1, ho + 1 + H));
  return h;
}

// Build the same structure from the exported JSON (header.id / fieldCount / count / count2, lines[].fields).
export function hdrFromExportJson(j, name = null) {
  const hd = j.header;
  const F = hd.fieldCount;
  const N = hd.count;
  const H = hd.count2 ?? N;
  const h = {
    name: name ?? (j.file ? j.file.replace(/\.dat$/, '') : null),
    id: hd.id,
    sub: 0xffff,
    F,
    N,
    H,
    M: hd.alignMask ?? 0,
    unit: hd.unit ?? 256,
    hasPlayed: false,
    totalUnits: 0,
    variants: 0,
    lines: j.lines.map((l, i) => ({ index: i, keys: (l.fields || []).slice(0, F), alias: -1 })),
    masks: [],
  };
  for (let f = 0; f < F; f++) {
    let m = 0;
    for (const l of h.lines) m |= 1 << l.keys[f];
    h.masks.push(m >>> 0);
  }
  h.historyCursor = 0;
  h.history = new Array(H).fill(0xff);
  return h;
}

// Registry entry from speech/registry.json ({name, id, F, N, H, masks, keys}, tools/export_speech_events.py).
export function hdrFromRegistry(r) {
  return { name: r.name, id: r.id, sub: 0xffff, F: r.F, N: r.N, H: r.H, M: 0, unit: 256, hasPlayed: false, totalUnits: 0, variants: 0,
    lines: r.keys.map((keys, index) => ({ index, keys: keys.slice(), alias: -1 })), masks: r.masks.slice(), historyCursor: 0,
    history: new Array(r.H).fill(0xff) };
}

// Registry (gp+0x1734): the engine looks banks up by the .hdr u16 id. Registry index (registration order:
// headers.big entries, then langhead.big entries) is also the no-repeat key of 0x3DB5C0.
export function makeBankTable(hdrs) {
  const m = new Map();
  hdrs.forEach((h, i) => {
    if (!m.has(h.id)) {
      h.regIndex = i;
      m.set(h.id, h);
    }
  });
  return m;
}

// ---------------------------------------------------------------- RNG (0x3DB4D0, seed 0x3DB790)
export class EaRng {
  constructor(seed = 0) {
    this.seed(seed);
  }
  seed(seed) {
    seed >>>= 0;
    const x = (seed + ((seed << 16) >>> 0)) >>> 0;
    const s = new Array(6);
    s[0] = (x + 0xf22d0e56) >>> 0;
    s[1] = (s[0] + 0x96041893) >>> 0;
    s[2] = (s[1] + 0x3df3b646) >>> 0;
    s[3] = (s[2] + 0x40dde76d) >>> 0;
    s[4] = (s[3] + 0x97327ae1) >>> 0;
    s[5] = (s[4] + 0xd1a9fbe7) >>> 0;
    this.s = s;
  }
  next() {
    const s = this.s;
    let a = (s[5] + s[4]) >>> 0;
    let c = a < s[5] || a < s[4] ? 1 : 0;
    s[4] = a;
    for (const k of [3, 2, 1]) {
      const old = s[k];
      a = (((a + old) >>> 0) + c) >>> 0;
      c = a < old ? 1 : 0;
      s[k] = a;
    }
    a = (((a + s[0]) >>> 0) + c) >>> 0;
    s[0] = a;
    s[5] = (s[5] + 1) >>> 0;
    if (s[5] === 0) {
      s[4] = (s[4] + 1) >>> 0;
      if (s[4] === 0) {
        s[3] = (s[3] + 1) >>> 0;
        if (s[3] === 0) {
          s[2] = (s[2] + 1) >>> 0;
          if (s[2] === 0) {
            s[1] = (s[1] + 1) >>> 0;
            if (s[1] === 0) s[0] = (a + 1) >>> 0;
          }
        }
      }
    }
    return s[0];
  }
}

// ---------------------------------------------------------------- state
export function createSpeechState() {
  return {
    ring: Array.from({ length: 32 }, () => ({ value: 0xffff, key: 0xffff })), // 0x450B88
    ringIdx: 0, // gp+0x1768
    hdrHist: new Map(), // bankId -> {cursor, list}   (the .hdr's own history bytes)
    hdrPlayed: new Map(), // bankId -> Uint8Array bitmap (only banks with hdr[4] bit 7; none in SSX3)
    last: new Map(), // channel -> {id, count}   (0x450660 / 0x450664)
    fileVars: [], // Events.evt global variables (none in SSX3)
  };
}

function hist(state, h) {
  let x = state.hdrHist.get(h.id);
  if (!x) {
    x = { cursor: h.historyCursor ?? 0, list: (h.history && h.history.length === h.H ? h.history.slice() : new Array(h.H).fill(0xff)) };
    state.hdrHist.set(h.id, x);
  }
  return x;
}

// 0x3D6E30: recency of `line` in the bank history (1 = most recent, 0 = not present)
function recency(state, h, line) {
  const x = hist(state, h);
  const H = h.H;
  if (H <= 0) return 0;
  let p = x.cursor - 1;
  for (let k = 0; k < H; k++) {
    if (p < 0) p = H - 1;
    if (x.list[p] === line) return k + 1;
    p--;
  }
  return 0;
}

// 0x3D6EE8: push a started line
function pushHistory(state, h, line) {
  const x = hist(state, h);
  if (x.cursor < h.H) x.list[x.cursor] = line;
  let c = (x.cursor + 1) & 0xff;
  if (!(c < h.H)) c = 0;
  x.cursor = c;
}

// 0x3DB5C0: uniform 0..n-1; key != -1 adds the 32-entry global no-repeat ring
export function randN(rng, state, n, key = -1) {
  let depth = n >= 0 ? n >> 1 : -((-n) >> 1);
  if (!(depth < 11)) depth = 10;
  const draw = () => (((rng.next() >>> 16) * n) >>> 16);
  let best = draw();
  if (key === -1) return best;
  key &= 0xffff;
  let fp = -1;
  let attempts = 0;
  let t2 = best;
  for (;;) {
    t2 = draw();
    const t0 = Math.min(depth, 32);
    let idx = state.ringIdx;
    let matches = 0;
    let found = -1;
    let scanned = 0;
    for (;;) {
      let stop = false;
      if (!(matches < t0)) stop = true;
      else {
        const e = state.ring[idx];
        if (e.key === key) {
          matches++;
          if (e.value === t2) {
            stop = true;
            found = matches - 1;
          }
        }
      }
      idx--;
      scanned++;
      if (idx < 0) idx += 32;
      if (scanned >= 33) stop = true;
      if (stop) break;
    }
    let done = false;
    if (found === -1) {
      best = t2;
      done = true;
    } else if (found > fp) {
      fp = found;
      best = t2;
    }
    attempts++;
    if (attempts >= 32) done = true;
    if (done) break;
  }
  state.ringIdx = state.ringIdx + 1 < 32 ? state.ringIdx + 1 : 0;
  state.ring[state.ringIdx] = { value: best & 0xffff, key };
  return t2; // PS2 quirk: returns the last draw (== best unless all 32 draws hit the ring)
}

// 0x3D8CB0: weighted random order without replacement; zero-weight records appended in index order
function recordOrder(ev, rng, state) {
  const n = ev.records.length;
  const w = ev.records.map((r) => r.weight);
  let total = w.reduce((a, x) => a + x, 0);
  const order = [];
  while (total > 0) {
    let r = randN(rng, state, total, -1);
    let i = 0;
    r -= w[0];
    while (r >= 0) {
      i++;
      if (!(i < n)) break;
      r -= w[i];
    }
    order.push(i);
    total -= w[i];
    w[i] = 0;
  }
  ev.records.forEach((rec, i) => {
    if (rec.weight === 0) order.push(i);
  });
  return order;
}

// ---------------------------------------------------------------- resolution (0x3D9FB0)
const SILENCE_RE = /_Silence_(\d+)$/i;

export function resolveEvent(events, banks, eventId, args, rng, state, opts = {}) {
  const ch = opts.channel ?? 0;
  const chMode = opts.channelMode ?? 0; // 0x4506A0[ch]; never set in SSX3 (0)
  const commit = opts.commit ?? true;
  const ev = events.events.get(eventId);
  if (!ev) return { items: [], record: -1, reason: 'no such event' };
  const A = new Array(20).fill(opts.garbage ?? 0);
  A[0] = ((1 << 24) | (0 << 16) | eventId) >>> 0;
  args.forEach((v, i) => {
    if (i + 1 < 20) A[i + 1] = v >>> 0;
  });

  // 0x450660 repeat rule
  const last = state.last.get(ch) ?? { id: 0xffff, count: 0 };
  if (last.id === ev.id && ev.maxRepeat > 0 && !(last.count < ev.maxRepeat)) {
    return { items: [], record: -1, reason: `maxRepeat ${ev.maxRepeat} reached` };
  }
  let s6 = chMode;
  if (!(ev.flags & 1) && s6 === 1) s6 = 0;

  // external conditions: no hook installed (0x44FF94 == 0) -> every ext bit is "don't care"
  let extDontCare = 0;
  ev.ext.forEach((_, j) => {
    extDontCare |= (1 << (7 - j)) & 0xff;
  });
  const extVal = 0;

  // presence mask (flags & 8)
  let present = 0;
  if (ev.presence) {
    for (let i = 0; i < 20; i++) {
      const code = (ev.presence.codes >>> (2 * i)) & 3;
      const a = A[1 + i] ?? 0;
      const has = code === 1 ? a !== 0 : code === 2 ? a !== 0xffffffff : false;
      if (has && i < 16) present |= 1 << i;
    }
  }

  const order = recordOrder(ev, rng, state);
  for (const ri of order) {
    if (!(ri < ev.records.length)) break;
    const rec = ev.records[ri];
    const roll = randN(rng, state, 100, -1);
    if (!(roll < rec.chance)) continue;
    // 0x3D9ED0 cond
    let ok = true;
    for (let k = 0; k < ev.cond.length; k++) {
      const m = rec.condMasks[k];
      if (m !== 0 && ((m & A[ev.cond[k].arg]) >>> 0) === 0) {
        ok = false;
        break;
      }
    }
    if (!ok) continue;
    if (ev.presence && ev.presence.req[ri] !== present) continue;
    const res = buildRecord(ev, rec, A, banks, state, s6, extVal, extDontCare);
    if (!res) continue;
    const chosen = chooseLines(rec, res, rng, state, s6);
    if (!chosen) continue;
    const items = chosen.map(({ h, line, sub }) => {
      const it = { bankId: h.id, bank: h.name, line };
      if (sub !== -1) it.variant = sub;
      const m = h.name && SILENCE_RE.exec(h.name);
      if (m) it.silenceMs = +m[1];
      return it;
    });
    if (commit) commitEvent(state, ev, chosen, ch);
    return { items, record: ri, order, reason: 'ok' };
  }
  return { items: [], record: -1, order, reason: 'no record resolved' };
}

// 0x3D9088 + 0x3D88C8 (+0x3D85C8, 0x3D8780): per item, the candidate line list
function buildRecord(ev, rec, A, banks, state, s6, extVal, extDontCare) {
  const nItems = rec.items.length;
  if (!(nItems < 13)) return null;
  const mode = rec.mode;
  let modeOk;
  if (mode === 1) modeOk = s6 === 2;
  else if (mode === 2) modeOk = s6 !== 2;
  else modeOk = mode === 0;
  if (!modeOk) return null;
  if ((((rec.extVal ^ extVal) & rec.extMask & ~extDontCare) & 0xff) !== 0) return null;

  const recState = new Array(rec.nState).fill(0);
  const out = rec.items.map(() => ({ done: false, h: null, sub: -1, cands: [] }));
  let tries = 200;
  let allOk;
  do {
    allOk = true;
    tries--;
    for (let i = 0; i < nItems; i++) {
      const item = rec.items[i];
      // consumer fields whose record state is not produced yet -> retry next pass
      let ready = true;
      for (let j = 0; j < item.nf; j++) {
        if (item.param[j] === 0xff && item.state[j] !== 0xff && item.state[j] & 0x80) {
          if (recState[item.state[j] & 0x7f] === 0) ready = false;
        }
      }
      if (!ready) {
        allOk = false;
        continue;
      }
      if (out[i].done) continue;
      let h = null;
      let sub = -1;
      if (item.type === 0) h = banks.get(item.bankId) ?? null;
      else if (item.type === 1) {
        if (item.argIdx < 20) {
          const want = A[item.argIdx] & 0xffff;
          for (const x of banks.values()) if (x.id === item.bankId && x.sub === want) h = x;
        }
      } else if (item.type === 2) {
        h = banks.get(item.bankId) ?? null;
        const v = A[item.argIdx] >>> 0;
        if (h && !(h.variants !== 0xffff && v < h.variants)) h = null;
        sub = A[item.argIdx] & 0xffff;
      }
      if (!h) return null; // bank not registered (e.g. non-English bank on the USA disc) -> record fails
      const cands = candidates(rec, item, h, sub, A, state, recState);
      if (cands.length === 0) return null;
      out[i] = { done: true, h, sub, cands };
    }
  } while (!allOk && tries > 0);
  if (tries === 0 && !allOk) return null;
  return out;
}

function candidates(rec, item, h, sub, A, state, recState) {
  const F = h.F;
  // pre-check: every hdr field j driven by a parameter must intersect the bank's value mask
  for (let j = 0; j < F; j++) {
    const p = j < 4 ? item.param[j] : 0;
    if (p === 0 || p === 0xff || p === 0xfe) continue;
    if (((h.masks[j] & A[p]) >>> 0) === 0) return [];
  }
  const hasFE = item.param.slice(0, Math.max(item.nf, 0)).some((p) => p === 0xfe);
  const fresh = [];
  let bestR = 0;
  let bestLine = -1;
  for (let line = 0; line < h.N; line++) {
    if (!lineMatches(item, h.lines[line].keys, A, recState)) continue;
    if (!lineAvailable(h, line, sub, state)) continue;
    const r = recency(state, h, line);
    if (r > 0) {
      if (bestR < r) {
        bestR = r;
        bestLine = line;
      }
    } else {
      fresh.push(line);
      if (hasFE) produce(item, h.lines[line].keys, recState);
    }
  }
  if (fresh.length === 0 && bestLine !== -1) {
    fresh.push(bestLine);
    if (hasFE) produce(item, h.lines[bestLine].keys, recState);
  }
  return fresh;
}

function produce(item, keys, recState) {
  for (let j = 0; j < item.nf; j++) {
    if (item.param[j] === 0xfe && item.state[j] & 0x80) recState[item.state[j] & 0x7f] |= 1 << keys[j];
  }
}

// 0x3D85C8
function lineMatches(item, keys, A, recState, fileVars = []) {
  const nf = item.nf;
  if (!(nf < 5)) return false;
  for (let j = 0; j < nf; j++) {
    const key = keys[j] ?? 0;
    if (!(key < 32)) return false;
    const bit = (1 << key) >>> 0;
    if (((item.fmask[j] & bit) >>> 0) === 0) return false;
    const p = item.param[j];
    if (p === 0 || p === 0xfe) continue;
    if (p === 0xff) {
      const s = item.state[j];
      if (s & 0x80) {
        if (((recState[s & 0x7f] & bit) >>> 0) === 0) return false;
      } else if (fileVars[s] !== key) return false;
      continue;
    }
    if (((A[p] & bit) >>> 0) === 0) return false;
  }
  return true;
}

// 0x3D8780 (played bitmap / variants; always true for SSX3 banks)
function lineAvailable(h, line, sub, state) {
  if (h.N < line) return false;
  if (sub !== -1) return sub < h.variants; // variant bitmap not modelled (unused in SSX3)
  if (!h.hasPlayed) return true;
  const bm = state.hdrPlayed.get(h.id);
  return bm ? !!(bm[line >> 3] & (1 << (line & 7))) : true;
}

// 0x3D95B8 (+0x3D93C0/0x3D8E58 when the record has state words)
function chooseLines(rec, res, rng, state, s6) {
  const key = (h) => (h.regIndex ?? h.id) & 0xffff;
  if (s6 !== 1 && rec.nState === 0) {
    const pick = [];
    for (const o of res) {
      const n = o.cands.length;
      if (n === 0) return null;
      const k = randN(rng, state, n, key(o.h));
      pick.push({ h: o.h, sub: o.sub, line: o.cands[k] });
    }
    return pick;
  }
  // shuffle every candidate list, then first consistent combination in odometer order
  for (const o of res) {
    for (let s0 = o.cands.length; s0 >= 2; s0--) {
      const j = randN(rng, state, s0, -1);
      const t = o.cands[s0 - 1];
      o.cands[s0 - 1] = o.cands[j];
      o.cands[j] = t;
    }
  }
  const cur = res.map(() => 0);
  for (;;) {
    if (consistent(rec, res, cur)) return res.map((o, i) => ({ h: o.h, sub: o.sub, line: o.cands[cur[i]] }));
    let k = res.length - 1;
    for (;;) {
      cur[k]++;
      if (cur[k] < res[k].cands.length) break;
      cur[k] = 0;
      k--;
      if (k < 0) return null;
    }
  }
}

function consistent(rec, res, cur) {
  const S = new Array(rec.nState).fill(0);
  const done = res.map(() => false);
  for (let pass = 0; pass < 64; pass++) {
    let again = false;
    for (let i = 0; i < res.length; i++) {
      if (done[i]) continue;
      done[i] = true;
      const item = rec.items[i];
      const keys = res[i].h.lines[res[i].cands[cur[i]]].keys;
      for (let j = 0; j < item.nf; j++) {
        const s = item.state[j];
        if (s === 0xff || !(s & 0x80)) continue;
        if (item.param[j] === 0xff) {
          const v = S[s & 0x7f];
          if (v === 0) {
            done[i] = false;
            again = true;
          } else if (!((v >> keys[j]) & 1)) return false;
        } else if (item.param[j] === 0xfe) S[s & 0x7f] = 1 << keys[j];
      }
    }
    if (!again) return true;
  }
  return false;
}

// send-time effects (0x3D9830 per line, 0x3D9BD8 per event)
export function commitEvent(state, ev, chosen, ch = 0) {
  for (const { h, line } of chosen) {
    if (h.hasPlayed) {
      const bm = state.hdrPlayed.get(h.id);
      if (bm) bm[line >> 3] &= ~(1 << (line & 7));
    }
    pushHistory(state, h, line);
  }
  const last = state.last.get(ch) ?? { id: 0xffff, count: 0 };
  if (last.id === ev.id) last.count++;
  else {
    last.id = ev.id;
    last.count = 1;
  }
  state.last.set(ch, last);
}

// ---------------------------------------------------------------- scheduler (pending events)
// Time unit: speech timer ticks (0x450DCC, timer-1 interrupt programmed at 60 Hz by 3E4AF0(60)).
export class SpeechScheduler {
  constructor(events, banks, rng, state) {
    Object.assign(this, { events, banks, rng, state });
    this.slots = Array.from({ length: 16 }, () => ({ active: false }));
    this.count = new Array(8).fill(0);
    this.lastPosted = new Array(8).fill(-1); // B+0x20
    this.lastPostedPlain = new Array(8).fill(-1); // B+0x40
    this.seqTime = -1;
    this.seq = 0;
  }
  // 0x3D7418: returns true when the event was accepted into a slot
  post(eventId, args, now, ch = 0) {
    const ev = this.events.events.get(eventId);
    if (!ev) return false;
    const queued = !!(ev.flags & 0x20);
    let P = ev.P;
    if (queued) {
      const a = this.lastPostedPlain[ch];
      const b = this.lastPosted[ch];
      if (!(a >= 0 && a < 16 && b >= 0 && b < 16)) return false;
      P = this.slots[a].ev.P;
    } else {
      const roll = randN(this.rng, this.state, 100, -1);
      if (ev.chance < roll) return false;
    }
    const s = this.allocSlot(P, ch, queued, now);
    if (s < 0) return false;
    if (now !== this.seqTime) this.seq = 0;
    this.seq = (this.seq + 1) & 0xffff;
    this.seqTime = now;
    this.slots[s] = { active: true, ev, ch, seq: this.seq, time: now, queued, link: -1, args: args.slice() };
    if (queued) this.slots[this.lastPosted[ch]].link = s;
    else this.count[ch]++;
    this.lastPosted[ch] = s;
    if (!queued) this.lastPostedPlain[ch] = s;
    return true;
  }
  // 0x3D72A0
  allocSlot(P, ch, queued, now) {
    for (let i = 0; i < 16; i++) if (!this.slots[i].active) return i;
    const keep = queued ? this.lastPostedPlain[ch] : -1;
    for (let i = 0; i < 16; i++) {
      const sl = this.slots[i];
      if (sl.queued) continue;
      if (sl.ev.W === 0) continue;
      if (!(sl.ev.W < ((now - sl.time) >>> 0))) continue;
      if (i === keep) continue;
      this.kill(i);
      return i;
    }
    for (let i = 0; i < 16; i++) {
      const sl = this.slots[i];
      if (sl.queued) continue;
      if (P < sl.ev.P) continue;
      if (sl.ch !== ch) continue;
      if (i === keep) continue;
      sl.active = false;
      this.count[ch]--;
      return i;
    }
    return -1;
  }
  // 0x3D7C38
  kill(i) {
    const sl = this.slots[i];
    if (sl.queued) return;
    sl.active = false;
    if (this.lastPostedPlain[sl.ch] === i) this.lastPostedPlain[sl.ch] = -1;
    if (this.lastPosted[sl.ch] === i) this.lastPosted[sl.ch] = -1;
    let l = sl.link;
    while (l >= 0 && l < 16) {
      const q = this.slots[l];
      if (!q.queued) break;
      if (!q.active) break;
      if (this.lastPosted[sl.ch] === l) this.lastPosted[sl.ch] = -1;
      q.active = false;
      l = q.link;
    }
    this.count[sl.ch] = Math.max(0, this.count[sl.ch] - 1);
  }
  // 0x3D7760: best pending slot on the channel (highest P, newest on ties); expired ones are dropped
  pick(ch, now, follow) {
    let best = -1;
    let bestP = 0;
    let bestAge = 0xffffffff;
    let bestSeq = 0;
    for (let i = 0; i < 16; i++) {
      const sl = this.slots[i];
      if (!sl.active || sl.ch !== ch || sl.queued) continue;
      const age = (now - sl.time) >>> 0;
      if (sl.ev.W !== 0 && sl.ev.W < age) {
        this.kill(i);
        continue;
      }
      if (follow && !follow.includes(sl.ev.id)) continue;
      const P = sl.ev.P;
      if (bestP < P || (P === bestP && (age < bestAge || (age === bestAge && bestSeq < sl.seq)))) {
        best = i;
        bestP = P;
        bestAge = age;
        bestSeq = sl.seq;
      }
    }
    return best;
  }
  // 0x3D7EC8 (+0x3D9BD8 send): called when the voice is free (2AFC88). Returns resolved lines or null.
  dispatch(ch, now) {
    for (;;) {
      if (this.count[ch] === 0) return null;
      const last = this.state.last.get(ch);
      const lastEv = last ? this.events.events.get(last.id) : null;
      let s = -1;
      if (lastEv && lastEv.follow) {
        s = this.pick(ch, now, lastEv.follow);
        if (s < 0) s = this.pick(ch, now, null);
      } else s = this.pick(ch, now, null);
      if (s < 0) return null;
      // canStart hook 0x44FF9C is not installed -> accepted
      const chosen = this.slots[s];
      // 0x3D7D58: flush older pending events on the channel unless their event has flag 0x04
      for (let i = 0; i < 16; i++) {
        if (i === s) continue;
        const o = this.slots[i];
        if (!o.active || o.ch !== chosen.ch || o.queued) continue;
        const older = o.time < chosen.time || (o.time === chosen.time && o.seq < chosen.seq);
        if (older && !(o.ev.flags & 0x04)) this.kill(i);
      }
      const out = [];
      let cur = s;
      let r;
      do {
        const sl = this.slots[cur];
        r = resolveEvent(this.events, this.banks, sl.ev.id, sl.args, this.rng, this.state, { channel: ch });
        out.push({ eventId: sl.ev.id, ...r });
        cur = sl.link;
      } while (cur !== -1 && r.items.length > 0);
      this.kill(s);
      return out;
    }
  }
}

// ---------------------------------------------------------------- the game's request table (0x2B1458..)
export class GameRequestTable {
  constructor() {
    this.e = Array.from({ length: 10 }, () => ({ active: false }));
  }
  // 0x2B1458: false -> caller must not post the event
  admit(ch, eventId, refreshDup = false, extra = {}) {
    let free = -1;
    for (let i = 0; i < 10; i++) {
      const x = this.e[i];
      if (x.active) {
        if (x.ch === ch && x.eventId === eventId) {
          if (!refreshDup) return false;
          x.ttl = 180;
          return true;
        }
      } else if (free === -1) free = i;
    }
    if (free === -1) return false;
    this.e[free] = { active: true, ttl: 180, ch, eventId, ...extra };
    return true;
  }
  // 0x2B1720, once per frame
  tick() {
    for (const x of this.e) if (x.active && --x.ttl <= 0) x.active = false;
  }
  // 0x2B1520: a started line is only streamed if its request is still here
  lookup(ch, eventId) {
    return this.e.find((x) => x.active && x.ch === ch && x.eventId === eventId) ?? null;
  }
}

// ---------------------------------------------------------------- pretty printer (events.txt)
export function describeEvents(events, names = new Map()) {
  const nm = (id) => names.get(id) ?? `bank_${id.toString(16)}(absent)`;
  const argName = (i) => (i === 0 ? 'handle' : `arg${i - 1}`);
  const hex = (x) => '0x' + (x >>> 0).toString(16);
  const lines = [
    '# SSX 3 Events.evt (59 events) as pseudo-code (describeEvents in speech-events.js).',
    '# argN = N-th argument the game passes to the post (arg0 = language mask for DJ/PA, speaker mask for rider/select',
    '#        events; arcade events have no language arg). `bank[k0∈M&argX]` = choose a line of `bank` whose key field 0',
    '#        value v satisfies (M & 1<<v) and (argX & 1<<v); `any` = every line. Lines of one record play in order.',
    '# W is in speech-timer ticks (60 Hz). KEEP = flag 0x04. bank_XXXX(absent) = non-English bank not on the USA disc',
    '# (record fails -> next record in the weighted order). Choice rules: notes.md section 4.',
    '',
  ];
  for (const ev of events.list) {
    const fl = [];
    if (ev.flags & 4) fl.push('KEEP(0x04: not flushed when a newer event starts)');
    if (ev.flags & 0x10) fl.push(`FOLLOW(${ev.follow.map(hex).join(',')})`);
    if (ev.flags & ~0x14) fl.push(hex(ev.flags));
    lines.push(
      `event ${hex(ev.id)}  W=${ev.W} ticks (${(ev.W / 60).toFixed(2)} s)  P=${ev.P}  chance=${ev.chance}%` +
        (ev.maxRepeat > 0 ? `  maxRepeat=${ev.maxRepeat}` : '') +
        (fl.length ? '  ' + fl.join(' ') : '')
    );
    const tot = ev.records.reduce((a, r) => a + r.weight, 0);
    lines.push(`  pick ONE record: weighted random order (total ${tot}), first record whose conditions pass and whose items all resolve`);
    for (const rec of ev.records) {
      const conds = [];
      ev.cond.forEach((c, k) => {
        const m = rec.condMasks[k];
        if (m) conds.push(`(${argName(c.arg)} & ${hex(m)})`);
      });
      const head = `  R${rec.index} w=${rec.weight}${rec.chance !== 100 ? ` chance=${rec.chance}%` : ''}${rec.mode ? ` mode=${rec.mode}` : ''}`;
      const seq = rec.items.map((it) => {
        const f = [];
        for (let j = 0; j < it.nf; j++) {
          const p = it.param[j];
          const src = p === 0 ? 'any' : p === 0xfe ? `produce[${it.state[j]}]` : p === 0xff ? `consume[${it.state[j]}]` : argName(p);
          f.push(`k${j}∈${hex(it.fmask[j])}${p === 0 ? '' : '&' + src}`);
        }
        return `${nm(it.bankId)}${it.type ? `{type${it.type} arg${it.argIdx - 1}}` : ''}[${f.join(',') || 'any'}]`;
      });
      lines.push(`${head.padEnd(14)} if ${conds.join(' && ') || 'true'}: play ${seq.join(' -> ')}`);
    }
    lines.push('');
  }
  return lines.join('\n');
}
