// Cutting a streamed location's collision package into small append documents (web/peak-world-worker.js, tests).
// World slices carry the streamed world's location ("PEAK1", "PEAK3": the folder of the location package, /assets/PEAK<N>/<LOC>/):
// the core selects that peak's stage tables by it (stage_script_gameplay.inc, streamed_world.hpp).
const PATCHES = 3, INSTANCES = 10; // one slice stays under ~5 ms of core time (terrain: two 100-point coarse grids per patch)

export function terrainBatches(t) {
  const out = [];
  for (let i = 0; i < t.patches.length; i += PATCHES)
    out.push(JSON.stringify({ version: t.version, location: t.location, source_sha256: t.source_sha256, patches: t.patches.slice(i, i + PATCHES) }));
  return out;
}

// world_collision.json slices: each keeps only the descriptors (renumbered), model hierarchies and collision meshes
// its instances reference; init_world_collision reads nothing else per instance.
export function worldBatches(w, location = 'PEAK1') {
  const out = [];
  for (let i = 0; i < w.instances.length; i += INSTANCES) {
    const instances = [], bindings = {}, nodes = {}, meshes = {}, remap = new Map();
    for (const source of w.instances.slice(i, i + INSTANCES)) {
      const track = String(source.track), key = `${track}:${source.collision_descriptor}`;
      bindings[track] ??= { descriptors: [] };
      if (!remap.has(key)) { remap.set(key, bindings[track].descriptors.length); bindings[track].descriptors.push(w.bindings[track].descriptors[source.collision_descriptor]); }
      const descriptor = w.bindings[track].descriptors[source.collision_descriptor];
      instances.push({ ...source, collision_descriptor: remap.get(key) });
      const model = `${source.model_resource & 255}:${source.model_resource >>> 8}`;
      if (w.render_model_nodes[model]) nodes[model] = w.render_model_nodes[model];
      if (descriptor && descriptor.collision_resource !== undefined) {
        const r = descriptor.collision_resource, k = `${r & 255}:${r >>> 8}`;
        if (w.collision_meshes[k]) meshes[k] = w.collision_meshes[k];
      }
    }
    out.push(JSON.stringify({ version: w.version, location, source_sha256: w.source_sha256, event_locations: w.event_locations,
      source_axis: w.source_axis, source_units: w.source_units, bindings, instances, collision_meshes: meshes, render_model_nodes: nodes }));
  }
  return out;
}

// pv sliceLoad: a location's rail catalog in parts of about `bytes` of text (one whole catalog was 20-55 ms of core time at 1x, 80-220 ms
// on a phone, on a riding frame). Each part is the catalog's header with its own rails and counts, the location's segment count per
// track before it (segment_base: the core's +0x60/+0x64 indices go on across the parts) and `partial` on all but the last (the core's
// teeter / static-record pass runs after the last); the rails' text is the whole catalog's (JSON.stringify of the same values).
export function railBatches(rails, bytes = 24576) {
  const head = {}; for (const [k, v] of Object.entries(rails)) if (k !== 'rails') head[k] = v;
  const texts = rails.rails.map((r) => JSON.stringify(r)), out = [], seen = {};
  for (let i = 0; i < texts.length;) {
    const segment_base = { ...seen }; let size = 0, j = i, segs = 0;
    while (j < texts.length && (j === i || size + texts[j].length <= bytes)) { size += texts[j].length; const r = rails.rails[j], t = r.packed_id & 255; seen[t] = (seen[t] ?? 0) + r.segments.length; segs += r.segments.length; j++; }
    const last = j >= texts.length;
    out.push(`{${Object.entries({ ...head, rail_count: j - i, segment_count: segs, segment_base, ...(last ? {} : { partial: true }) }).map(([k, v]) => `${JSON.stringify(k)}:${JSON.stringify(v)}`).join(',')},"rails":[${texts.slice(i, j).join(',')}]}`);
    i = j;
  }
  return out;
}

export function locationBatches(terrain, world, rails, location = 'PEAK1', { railParts = false } = {}) {
  const whole = JSON.stringify(rails), parts = railParts && whole.length > 32768 ? railBatches(rails) : null;
  return [
    ...worldBatches(world, location).map((text) => ({ kind: 'world', text })),
    ...terrainBatches(terrain).map((text) => ({ kind: 'terrain', text })),
    // the parts carry the whole catalog on the first (web/peak-world.js: the world's first location is a plain init)
    ...(parts ? parts.map((text, k) => (k ? { kind: 'rails', text } : { kind: 'rails', text, whole })) : [{ kind: 'rails', text: whole }]),
  ];
}

// A location's environment slice (tools/export_mountain_world.py MOUNTAIN/ENV/<LOC>: patches and lattice textures with global
// ids) cut into small environment_add calls: at most 8 textures (their bytes rebased into the call's own buffer) or 400 patches.
export function environmentBatches(doc, bytes) {
  const out = [];
  for (let i = 0; i < doc.textures.length; i += 8) {
    const textures = [], parts = []; let at = 0;
    for (const t of doc.textures.slice(i, i + 8)) {
      const cells = (t.width + 1) * (t.height + 1), rgba = bytes.subarray(t.rgba_offset, t.rgba_offset + cells * 4), valid = bytes.subarray(t.valid_offset, t.valid_offset + cells);
      textures.push({ ...t, rgba_offset: at, valid_offset: at + rgba.length }); parts.push(rgba, valid); at += rgba.length + valid.length;
    }
    const b = new Uint8Array(at); let o = 0; for (const x of parts) { b.set(x, o); o += x.length; }
    out.push({ text: JSON.stringify({ textures, patches: [] }), bytes: b });
  }
  for (let i = 0; i < doc.patches.length; i += 400) out.push({ text: JSON.stringify({ textures: [], patches: doc.patches.slice(i, i + 400) }), bytes: new Uint8Array(0) });
  return out;
}

// pv sliceLoad (web/load-slices.js, core stage_world_part): the three stage-world documents (particles.json, livecomp.json,
// stage-world.json) cut into parts the core loads one per call, in the documents' order: [kind, text] with kind 0 begin ('1' when a
// stage-world document is given), 1 stage-world parts, 2 particles parts (the head: location, initial, crowd, boosts, halos, magnets,
// multi; then the instances and carriers maps in chunks), 3 livecomp chunks (after every particles part), 4 end.
// The parts are cut from the documents' own text (no parse / stringify: a number re-written by JavaScript can parse to another float
// in the core, e.g. its shortest form; the loaded state must be the one the whole documents give).
// spans(text, start, end): the members of the JSON object or array text[start..end): [[memberStart, memberEnd, valueStart]] (an
// object member is `"key": value`; an array member is its value), and the key of each object member.
function spans(text, start, end) {
  const out = []; let i = start; while (i < end && text.charCodeAt(i) !== 123 && text.charCodeAt(i) !== 91) i++;
  const isObject = text.charCodeAt(i) === 123; i++;
  let depth = 0, memberStart = -1, keyStart = -1, keyEnd = -1, afterColon = !isObject;
  const push = (at) => { let e = at; while (e > memberStart && text.charCodeAt(e - 1) <= 32) e--; out.push({ s: memberStart, e, key: keyStart >= 0 ? JSON.parse(text.slice(keyStart, keyEnd)) : null }); memberStart = -1; keyStart = -1; afterColon = !isObject; };
  for (; i < end; i++) {
    const c = text.charCodeAt(i);
    if (c === 34) { // a string: skipped whole (escapes included)
      const s0 = i; i++; while (i < end) { const d = text.charCodeAt(i); if (d === 92) i += 2; else if (d === 34) break; else i++; }
      if (depth === 0) { if (memberStart < 0) memberStart = s0; if (isObject && !afterColon && keyStart < 0) { keyStart = s0; keyEnd = i + 1; } }
      continue;
    }
    if (c <= 32) continue;
    if (depth === 0) {
      if (c === 58) { afterColon = true; continue; }
      if (c === 44) { push(i); continue; }
      if (c === 125 || c === 93) { if (memberStart >= 0) push(i); return out; }
      if (memberStart < 0) memberStart = i;
    }
    if (c === 123 || c === 91) depth++;
    else if (c === 125 || c === 93) depth--;
  }
  return out;
}
// the value text of an object member
const valueText = (text, m) => { let v = text.indexOf(':', m.s + JSON.stringify(m.key).length - 1) + 1; return text.slice(v, m.e).trim(); };
// pv sliceLoad (web/load-slices.js): an environment.json cut from its own text: the document without its textures and patches (marked
// streamed: environment_add keeps the textures by id), then the textures (2 a part) and the patches (400 a part); the texture offsets stay
// those of the whole environment.bin, which the caller puts in the core once.
export function environmentParts(text, { textures = 2, patches = 400 } = {}) { // 2 textures a part: 8 were up to ~90 ms at 4x CPU (PEAK1)
  const top = new Map(); for (const x of spans(text, 0, text.length)) top.set(x.key, x);
  const members = (m) => spans(text, text.indexOf(':', m.s + JSON.stringify(m.key).length - 1) + 1, m.e);
  const chunks = (list, n) => { const r = []; for (let i = 0; i < list.length; i += n) r.push(list.slice(i, i + n)); return r; };
  const join = (list) => list.map((x) => text.slice(x.s, x.e)).join(',');
  const head = [...top.values()].filter((m) => m.key !== 'textures' && m.key !== 'patches' && m.key !== 'streamed').map((m) => text.slice(m.s, m.e));
  const out = { head: `{${[...head, '"streamed":true', '"textures":[]', '"patches":[]'].join(',')}}`, parts: [] };
  for (const c of chunks(members(top.get('textures')), textures)) out.parts.push(`{"textures":[${join(c)}],"patches":[]}`);
  for (const c of chunks(members(top.get('patches')), patches)) out.parts.push(`{"textures":[],"patches":[${join(c)}]}`);
  return out;
}
export function stageWorldParts(particlesText, liveText, stageText, { instances = 100, live = 6, meshanim = 20, script = 400 } = {}) {
  const out = [[0, stageText ? '1' : '']];
  const top = (text) => { const m = new Map(); for (const x of spans(text, 0, text.length)) m.set(x.key, x); return m; };
  const members = (text, m) => spans(text, text.indexOf(':', m.s + JSON.stringify(m.key).length - 1) + 1, m.e);
  const chunks = (list, n) => { const r = []; for (let i = 0; i < list.length; i += n) r.push(list.slice(i, i + n)); return r; };
  const join = (text, list) => list.map((x) => text.slice(x.s, x.e)).join(',');
  if (stageText) {
    const t = top(stageText);
    for (const c of chunks(members(stageText, t.get('meshanim')), meshanim)) out.push([1, `{"meshanim":[${join(stageText, c)}]}`]);
    out.push([1, `{"teleports":${valueText(stageText, t.get('teleports'))}}`]);
    if (t.has('script')) for (const c of chunks(members(stageText, t.get('script')), script)) out.push([1, `{"script":[${join(stageText, c)}]}`]);
  }
  const t = top(particlesText);
  out.push([2, `{${['location', 'initial', 'crowd', 'boosts', 'halos', 'magnets', 'multi'].filter((k) => t.has(k)).map((k) => `${JSON.stringify(k)}:${valueText(particlesText, t.get(k))}`).join(',')}}`]);
  for (const c of chunks(members(particlesText, t.get('instances')), instances)) out.push([2, `{"instances":{${join(particlesText, c)}}}`]);
  if (t.has('carriers')) for (const c of chunks(members(particlesText, t.get('carriers')), instances)) out.push([2, `{"carriers":{${join(particlesText, c)}}}`]);
  if (liveText) { const l = top(liveText); for (const c of chunks(members(liveText, l.get('instances')), live)) out.push([3, `{"instances":[${join(liveText, c)}]}`]); }
  out.push([4, '']);
  return out;
}

// ---- pv eventSlices (web/load-slices.js initEventWorldSliced): an event course's terrain.json / world_collision.json / rails.json cut
// into the core's parts from their own text (the numbers as written: the loaded state is the whole documents', core body_load_hash /
// world_load_hash / rail_load_hash, web/test-event-slices.mjs) ----
// parse_key of web/world_bridge.cpp over a text's UTF-8 bytes: "salt:bytes:FNV-1a" (the key of the core's course parse caches).
export function parseKey(bytes, salt) {
  let h = 0x811c9dc5; for (let i = 0; i < bytes.length; i++) h = Math.imul(h ^ bytes[i], 0x01000193);
  return `${salt}:${bytes.length}:${h >>> 0}`;
}
const topMembers = (text) => { const m = new Map(); for (const x of spans(text, 0, text.length)) m.set(x.key, x); return m; };
const memberList = (text, m) => spans(text, text.indexOf(':', m.s + JSON.stringify(m.key).length - 1) + 1, m.e);
const memberText = (text, m) => text.slice(m.s, m.e);
// terrain.json: the head (no patches: init_terrain / init_body_terrain start both terrains) and parts of `patches` patches (terrain_part:
// one parse for both; a patch is ~0.2 ms of core time at 1x CPU).
export function eventTerrainParts(text, { patches = 12 } = {}) {
  const top = topMembers(text), keep = [...top.values()].filter((m) => m.key !== 'patches').map((m) => memberText(text, m));
  const head = `{${[...keep, '"patches":[]'].join(',')}}`, list = memberList(text, top.get('patches')), parts = [];
  const hash = JSON.parse(valueText(text, top.get('source_sha256'))), version = valueText(text, top.get('version')), location = valueText(text, top.get('location'));
  for (let i = 0; i < list.length; i += patches) parts.push(`{"version":${version},"location":${location},"source_sha256":${JSON.stringify(hash)},"patches":[${list.slice(i, i + patches).map((m) => memberText(text, m)).join(',')}]}`);
  return { head, parts, hash };
}
// world_collision.json: the head (no instances: the course selection and track locations) and parts of whole instances up to about
// `bytes` of text, each with the descriptors it uses (renumbered in the part: the instance's collision_descriptor is rewritten), the
// collision meshes and the model hierarchies it references (init_world_collision reads nothing else per instance).
export function eventWorldParts(text, { bytes = 49152 } = {}) {
  const top = topMembers(text), heavy = new Set(['instances', 'bindings', 'collision_meshes', 'render_model_nodes']);
  const keep = [...top.values()].filter((m) => !heavy.has(m.key)).map((m) => memberText(text, m));
  const head = `{${[...keep, '"bindings":{}', '"instances":[]', '"collision_meshes":{}', '"render_model_nodes":{}'].join(',')}}`;
  const small = ['version', 'location', 'event_locations', 'source_sha256', 'source_axis', 'source_units'].filter((k) => top.has(k)).map((k) => memberText(text, top.get(k)));
  const keyed = (m) => { const out = new Map(); for (const x of memberList(text, m)) out.set(x.key, x); return out; };
  const meshes = keyed(top.get('collision_meshes')), models = keyed(top.get('render_model_nodes'));
  const descriptors = new Map(); // track -> [descriptor spans]
  for (const b of memberList(text, top.get('bindings'))) { const d = [...memberList(text, b)].find((x) => x.key === 'descriptors'); descriptors.set(b.key, d ? memberList(text, d) : []); }
  const descriptorInfo = new Map(); // "track:index" -> collision resource
  const rkey = (r) => `${r & 255}:${r >>> 8}`;
  const parts = []; let cur = null;
  const flush = () => { if (!cur) return;
    const bindings = [...cur.bindings].map(([t, list]) => `${JSON.stringify(t)}:{"descriptors":[${list.map((m) => memberText(text, m)).join(',')}]}`).join(',');
    parts.push(`{${[...small, `"bindings":{${bindings}}`, `"instances":[${cur.instances.join(',')}]`, `"collision_meshes":{${[...cur.meshes].map((k) => memberText(text, meshes.get(k))).join(',')}}`,
      `"render_model_nodes":{${[...cur.models].map((k) => memberText(text, models.get(k))).join(',')}}`].join(',')}}`);
    cur = null; };
  for (const inst of memberList(text, top.get('instances'))) {
    const it = memberText(text, inst), o = JSON.parse(it), track = String(o.track), index = o.collision_descriptor, dkey = `${track}:${index}`;
    const dspan = descriptors.get(track)?.[index];
    if (dspan && !descriptorInfo.has(dkey)) { const d = JSON.parse(memberText(text, dspan)); descriptorInfo.set(dkey, d.collision_resource); }
    const mesh = descriptorInfo.get(dkey) !== undefined ? rkey(descriptorInfo.get(dkey)) : null, model = rkey(o.model_resource);
    const size = it.length + (dspan ? dspan.e - dspan.s : 0) + (mesh && meshes.has(mesh) ? meshes.get(mesh).e - meshes.get(mesh).s : 0) + (models.has(model) ? models.get(model).e - models.get(model).s : 0);
    if (cur && cur.size + size > bytes) flush();
    cur ??= { size: 0, instances: [], bindings: new Map(), remap: new Map(), meshes: new Set(), models: new Set() };
    let at = cur.remap.get(dkey);
    if (at === undefined && dspan) { const list = cur.bindings.get(track) ?? []; cur.bindings.set(track, list); at = list.length; list.push(dspan); cur.remap.set(dkey, at); }
    // the instance's own text with collision_descriptor renumbered for the part (every other member as written)
    const members = spans(it, 0, it.length).map((m) => (m.key === 'collision_descriptor' ? `"collision_descriptor":${at ?? index}` : it.slice(m.s, m.e)));
    cur.instances.push(`{${members.join(',')}}`);
    if (mesh && meshes.has(mesh)) cur.meshes.add(mesh); if (models.has(model)) cur.models.add(model);
    cur.size += size;
  }
  flush();
  return { head, parts };
}
// rails.json: the head (no rails) and parts of about `bytes` of rails, each with the location's segment count per track before it
// (segment_base) and "partial" (the core's tail runs once, at rails_seal).
export function eventRailParts(text, { bytes = 24576 } = {}) {
  const top = topMembers(text), provenance = ['version', 'location', 'source_record_kind', 'source_sha256'].filter((k) => top.has(k)).map((k) => memberText(text, top.get(k)));
  const head = `{${[...provenance, '"rail_count":0', '"segment_count":0', '"rails":[]'].join(',')}}`;
  const list = memberList(text, top.get('rails')), parts = [], seen = {};
  for (let i = 0; i < list.length;) {
    const base = { ...seen }; let size = 0, j = i, segs = 0; const texts = [];
    while (j < list.length && (j === i || size + (list[j].e - list[j].s) <= bytes)) {
      const t = memberText(text, list[j]), r = JSON.parse(t), track = r.packed_id & 255; texts.push(t); size += t.length;
      seen[track] = (seen[track] ?? 0) + r.segments.length; segs += r.segments.length; j++;
    }
    parts.push(`{${[...provenance, `"rail_count":${j - i}`, `"segment_count":${segs}`, `"segment_base":${JSON.stringify(base)}`, '"partial":true', `"rails":[${texts.join(',')}]`].join(',')}}`);
    i = j;
  }
  return { head, parts, count: list.length };
}
