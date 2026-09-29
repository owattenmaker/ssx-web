// Avalanche draw / rumble (web/avalanche-state.js, docs/avalanche.md "Audio"):
//  1. 0x29E438 / 0x2DA1C0: the volume from the nearest tumbler and the average scale (127 near, 0 at 100 m and beyond), the centroid;
//  2. the snapshot reader against a fake core (avalanche_info / avalanche_pieces / avalanche_sounds layouts of
//     web/avalanche_gameplay.inc): one read per tick, released pieces kept until reset;
//  3. the draw's visibility rule: drawn-at-start pieces until released, hidden-at-start pieces while their tumbler drives them.
import fs from 'node:fs';
import * as T from 'three';
import { avalancheRumble, avalancheState, createAvalancheDraw, takeLoopEvents, ENTITY_DRAWN } from './avalanche-state.js';

let failures = 0, checks = 0;
const fail = (m) => { if (failures++ < 12) console.error(m); };
const expect = (what, got, want) => { checks++; if (got !== want) fail(`${what}: ${JSON.stringify(got)}, want ${JSON.stringify(want)}`); };

// 1. volume
expect('no tumbler', avalancheRumble([], [0, 0, 0]).volume, 0);
expect('near, scale 1', avalancheRumble([[0, 0, 0, 1]], [100, 0, 0]).volume, 127);
expect('100 m', avalancheRumble([[0, 0, 0, 1]], [10000, 0, 0]).volume, 0);
expect('beyond', avalancheRumble([[0, 0, 0, 1]], [20000, 0, 0]).volume, 0);
expect('95 m', avalancheRumble([[0, 0, 0, 1]], [9500, 0, 0]).volume, Math.trunc(Math.fround(Math.fround(Math.fround(Math.fround(100 - 95) * Math.fround(1.27)) * 1) * Math.fround(8.466667175292969))));
expect('scale 0.05 near', avalancheRumble([[0, 0, 0, 0.05]], [0, 0, 0]).volume, 53);
expect('nearest decides', avalancheRumble([[0, 0, 0, 1], [9800, 0, 0, 1]], [9900, 0, 0]).volume, 127);
expect('centroid', JSON.stringify(avalancheRumble([[0, 0, 0, 1], [200, 400, -600, 1]], [0, 0, 0]).centroid), JSON.stringify([100, 200, -300]));

// 2. fake core
const heap = new ArrayBuffer(1 << 16), F = new Float32Array(heap), U = new Uint32Array(heap);
const core = { HEAPU8: new Uint8Array(heap), HEAPF32: F, info: [0, 0, 0, 0], pieces: [], released: [], loop: 0, changes: [],
  _avalanche_info() { U.set(this.info, 0); return 0; },
  _avalanche_pieces() { let at = 64; F[at++] = this.pieces.length; for (const p of this.pieces) { F.set(p, at); at += 22; } F[at++] = this.released.length; for (const r of this.released) F[at++] = r; this.released = [];
    F[at++] = this.loop; F[at++] = this.changes.length; for (const v of this.changes) F[at++] = v; this.changes = []; return 256; } };
const piece = (resource, x) => { const p = new Float32Array(22); p[0] = resource; p[1] = 1; p[2] = 1; p[4] = p[9] = p[14] = p[19] = 1; p[16] = x; p[21] = 1; return p; };

// 3. draw
const group = new T.Group(), mk = (r, hidden) => { const m = new T.Mesh(); m.userData.movingResource = r; if (hidden) m.userData.batch = { moving_resource: r }; else group.add(m); return m; };
const shown = mk(11, false), hid = mk(22, true), other = mk(33, false); group.userData.hiddenMeshes = [hid];
const node = mk(44, true); group.userData.hiddenMeshes.push(node);
const draw = createAvalancheDraw({ core, group, followers: new Set([11, 22, 44]), entityDrawn: new Set([22]) });
expect('pieces', draw.pieces, 3);
draw.update(); expect('before: static drawn', shown.visible, true); expect('before: hidden not in scene', hid.parent, null);
core.info = [0, 1, 1, 0]; core.pieces = [piece(11, 5), piece(22, 7), piece(44, 9)]; core.loop = 1; core.changes = [1]; draw.update();
expect('driven: node (no key 2) never drawn', node.parent, null);
expect('driven: hidden shown', hid.parent, group); expect('driven: static drawn', shown.visible, true);
const s = avalancheState(core); expect('tumblers', s.tumblers.length, 3); expect('loop', s.loop, true); expect('tumbler x', s.tumblers[1][0], 7);
expect('loop events', JSON.stringify(takeLoopEvents(core)), '[1]'); expect('loop events taken', takeLoopEvents(core).length, 0);
core.info = [0, 1, 2, 0]; core.pieces = [piece(22, 7)]; core.released = [11]; draw.update();
expect('released: static gone', shown.visible, false); expect('still driven', hid.parent, group);
core.info = [0, 1, 3, 0]; core.pieces = []; core.released = [22]; core.loop = 0; core.changes = [0]; draw.update();
expect('loop off', avalancheState(core).loop, false); expect('stop event', JSON.stringify(takeLoopEvents(core)), '[0]');
expect('released: hidden gone', hid.parent, null); expect('non-follower untouched', other.visible, true);
draw.reset(); expect('reset: static back', shown.visible, true); expect('reset: hidden stays hidden', hid.parent, null);

// 4. ENTITY_DRAWN against the stage programs (tools/export_avalanches.py --entity-drawn)
const fixture = new URL('../local/reference/avalanche/entity-drawn.json', import.meta.url);
if (fs.existsSync(fixture)) expect('ENTITY_DRAWN = the programs', JSON.stringify(ENTITY_DRAWN), JSON.stringify(JSON.parse(fs.readFileSync(fixture, 'utf8'))));
else console.log('entity-drawn fixture missing: skipped');
if (failures) { console.error(`test-avalanche-state: ${failures} failures`); process.exit(1); }
console.log(`test-avalanche-state: ok (${checks} checks)`);
