// The CTM freestyle heat results (web/fs-standings.js, pv fsStandings; OV.LUI 42freestyle_standings, 0x1E5B80 / 0x1E64C8):
// the rows, columns and help of each case, and (when present) the executable's strings and the exported panel.
import assert from 'node:assert/strict';
import fs from 'node:fs';
import { standingsModel, cardModel, DASHES, DNF } from './fs-standings.js';

let checks = 0; const eq = (a, b, m) => { assert.deepEqual(a, b, m); checks++; };
const AI = [['Mac', 162880, 70000], ['Nate', 95980, 90000], ['Kaori', 43760, 120000], ['Elise', 30960, 20000], ['Griff', 20360, 10000]];
const rows = (human, rank, dnf = false) => [{ name: 'Zoe', human: true, rank: rank[0], dnf, heat1: human[0], heat2: human[1] ?? null },
  ...AI.map(([name, a, b], i) => ({ name, human: false, rank: rank[i + 1], heat1: a, heat2: b }))];

// Heat 1 given up (PS2 menus/pipegu/giveup.final: Mac 162880 .. Griff 20360, Zoe DNF last; Heat 2 '- - -'; no Total; no help)
let m = standingsModel({ round: 1, rows: rows([0], [5, 0, 1, 2, 3, 4], true) });
eq(m.rows.map((r) => [r.rank, r.name, r.heat1, r.heat2, r.total]), [[1, 'Mac', '162880', DASHES, null], [2, 'Nate', '95980', DASHES, null], [3, 'Kaori', '43760', DASHES, null],
  [4, 'Elise', '30960', DASHES, null], [5, 'Griff', '20360', DASHES, null], [6, 'Zoe', DNF, DASHES, null]]);
eq([m.totalHeader, m.help, m.pulse, m.pulseRun], [false, null, 5, 1]);
// Heat 1 ridden, 6th: 'You are currently in 6th place.'
m = standingsModel({ round: 1, rows: rows([0], [5, 0, 1, 2, 3, 4]) });
eq([m.rows[5].heat1, m.help], ['0', { key: 'current', place: 6 }]);
// Heat 1 into the final (GMM+0x70 = 3): every heat 2 and total, the human's heat 2 '- - -' and total = heat 1; help_current0N
// with kT_CMNHELPEarnedEnoughPoints; the header 'Total:' stays hidden (0x1E5DBC hides it in round 1)
m = standingsModel({ round: 1, rows: rows([300000], [0, 1, 2, 3, 4, 5]), qualified: true });
eq(m.rows.map((r) => [r.name, r.heat1, r.heat2, r.total]), [['Zoe', '300000', DASHES, '300000'], ['Mac', '162880', '70000', '232880'], ['Nate', '95980', '90000', '185980'],
  ['Kaori', '43760', '120000', '163760'], ['Elise', '30960', '20000', '50960'], ['Griff', '20360', '10000', '30360']]);
eq([m.totalHeader, m.help], [false, { key: 'earned', place: 1 }]);
// Heat 2: heat 1 + heat 2 = Total, help_advance in the top three, else help_sorry; the pulse on run_2
m = standingsModel({ round: 2, rows: rows([0, 5000], [5, 0, 1, 2, 3, 4]) });
eq([m.rows[5].heat1, m.rows[5].heat2, m.rows[5].total, m.totalHeader, m.help, m.pulseRun], ['0', '5000', '5000', true, { key: 'sorry' }, 2]);
m = standingsModel({ round: 2, rows: rows([100000, 150000], [0, 1, 2, 3, 4, 5]) });
eq([m.rows[0].total, m.help], ['250000', { key: 'advance' }]);
// Heat 2 given up: 'DNF' in Heat 2, no total, no help
m = standingsModel({ round: 2, rows: rows([90000, 0], [5, 0, 1, 2, 3, 4], true) });
eq([m.rows[5].heat1, m.rows[5].heat2, m.rows[5].total, m.help], ['90000', DNF, null, null]);

// The heat card 41freestyle_pre (0x1FBD20): heat 1 = the three best heat 1 scores in one column, 'Up next' '- - -'; heat 2 = the
// three best combined with Heat 1 / Heat 2 / Total and the human's heat 1 (PS2 transport-map/heats/h2.f00300: Mac 162880 165440
// 328320, Nate 95980 90980 186960, Kaori 43760 46300 90060, Up next Zoe 2000 - - -); the final = the final's posted scores
// under 'Score'.
{
  const posted = AI.map(([name, a, b], i) => ({ name, heat1: a, heat2: b, final: 1000 * (5 - i) }));
  let k = cardModel({ round: 1, posted, human: { name: 'Zoe' } });
  eq([k.rows.map((r) => [r.name, r.heat1, r.heat2, r.total]), k.columns, k.tab, k.human, k.pulse], [[['Mac', '162880', null, null], ['Nate', '95980', null, null], ['Kaori', '43760', null, null]], false, 'heat1_tab', { name: 'Zoe', run1: DASHES }, 'p0run1']);
  const ps2 = [['Mac', 162880, 165440], ['Nate', 95980, 90980], ['Kaori', 43760, 46300], ['Elise', 30960, 29740], ['Griff', 20360, 19080]].map(([name, a, b]) => ({ name, heat1: a, heat2: b }));
  k = cardModel({ round: 2, posted: ps2, human: { name: 'Zoe', heat1: 2000 } });
  eq([k.rows.map((r) => [r.name, r.heat1, r.heat2, r.total]), k.columns, k.tab, k.human, k.pulse], [[['Mac', '162880', '165440', '328320'], ['Nate', '95980', '90980', '186960'], ['Kaori', '43760', '46300', '90060']], true, 'heat2_tab', { name: 'Zoe', run1: '2000' }, 'p0dashes']);
  k = cardModel({ round: 3, posted, human: { name: 'Zoe', heat1: 5 } });
  eq([k.rows[0].heat1, k.columns, k.tab, k.scoreHeader, k.human.run1], ['5000', false, 'finalheat_tab', true, DASHES]);
}

// The executable (local/disc/SLUS_207.72): the strings and the switches the model follows.
const elfPath = new URL('../local/disc/SLUS_207.72', import.meta.url).pathname;
if (fs.existsSync(elfPath)) {
  const e = fs.readFileSync(elfPath), phoff = e.readUInt32LE(0x1C), phn = e.readUInt16LE(0x2C), phs = e.readUInt16LE(0x2A), segs = [];
  for (let i = 0; i < phn; i++) { const o = phoff + i * phs; if (e.readUInt32LE(o) === 1) segs.push({ off: e.readUInt32LE(o + 4), va: e.readUInt32LE(o + 8), size: e.readUInt32LE(o + 16) }); }
  const at = (va) => { const s = segs.find((g) => va >= g.va && va < g.va + g.size); return s.off + va - s.va; };
  const str = (va) => { const o = at(va); return e.subarray(o, e.indexOf(0, o)).toString('latin1'); }, u32 = (va) => e.readUInt32LE(at(va));
  eq([str(0x4A2160), str(0x4A2168), str(0x4A2150), str(0x46E3D0), str(0x46E3E8), str(0x46E468), str(0x46E4E8)],
    [DNF, DASHES, 'total', 'kT_OVRCMNQFHeat1Stand', 'kT_OVRCMNQFHeat2Stand', 'kT_CMNHELPEarnedEnoughPoints', '@TextPulseWhiteOrange']);
  eq([str(0x46E418), str(0x46E428), str(0x46E438), str(0x46E448), str(0x46E458), str(0x46E500)], ['rider_%d', 'run_1.%d', 'run_2.%d', 'total_%d', 'help_current0%d', 'run_%d.%d']);
  eq(u32(0x1E5DBC) >>> 26, 3, 'round 1: jal 39E8B8 (hide) "total"');
  eq(u32(0x1E6558) >>> 26, 3, '0x1E64C8: jal 20A6F0 (the pulse)');
} else console.log('SLUS_207.72 not present: executable checks skipped');

// The export (git-ignored; python3 tools/export_fs_standings.py): the row widgets and the pulse (orange 255,128,0 -> white).
const dataPath = process.env.FS_STANDINGS_JSON || new URL('./public/assets/UI/fs-standings.json', import.meta.url).pathname;   // FS_STANDINGS_JSON: a staged export
if (fs.existsSync(dataPath)) {
  const d = JSON.parse(fs.readFileSync(dataPath, 'utf8')), s = d.screens['42freestyle_standings'], labels = new Set(s.elements.map((x) => x.label));
  for (let k = 1; k <= 6; k++) for (const l of [`rider_${k}`, `run_1.${k}`, `run_2.${k}`, `total_${k}`, `help_current0${k}`]) assert.ok(labels.has(l), l);
  checks++;
  const a = s.animations[d.pulse]; eq([a.frames, a.tracks['15'].map((x) => x[0]).sort(), a.tracks['16'].map((x) => x[0]).sort()], [31, [128, 255], [0, 255]]);
} else console.log('fs-standings.json not exported: panel checks skipped');
console.log(`fs-standings: ${checks} checks passed`);
