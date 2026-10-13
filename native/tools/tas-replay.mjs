// A replay file of a page run that has no finish (docs/native.md "N2"): tools/tas/page-run.mjs keeps replay.ssxr only for a finished
// run, but its start.json (the page's replay snapshot, rider, attributes, lineup) and the TAS pad are the whole replay. This makes the
// portable file (web/server/replay-file.mjs) from them, with the pad stream web/replay.js records (createRecording of the
// pad-format.mjs channels), so the native verifier can be checked against the run's trace.json tick by tick.
//   node native/tools/tas-replay.mjs PAD.tas RUN_DIR OUT.ssxr [--course BRA2] [--ticks N]
import fs from 'node:fs';
import path from 'node:path';
import { parse, toChannels } from '../../tools/tas/pad-format.mjs';
import { createRecording } from '../../web/replay.js';
import { encodeReplayFile } from '../../web/server/replay-file.mjs';

const args = process.argv.slice(2);
const opt = (k, d) => {
  const i = args.indexOf('--' + k);
  return i < 0 ? d : args[i + 1];
};
const [padPath, runDir, out] = args.filter((a, i) => !a.startsWith('--') && !(i > 0 && args[i - 1].startsWith('--')));
if (!padPath || !runDir || !out) throw new Error('usage: tas-replay.mjs PAD.tas RUN_DIR OUT.ssxr [--course BRA2] [--ticks N]');
const course = opt('course', 'BRA2');
const start = JSON.parse(fs.readFileSync(path.join(runDir, 'start.json'), 'utf8'));
const trace = JSON.parse(fs.readFileSync(path.join(runDir, 'trace.json'), 'utf8'));
const frames = parse(fs.readFileSync(padPath, 'utf8')).frames;
const ticks = +opt('ticks', trace.length);
const rec = createRecording();
for (let t = 0; t < ticks; t++) rec.push(toChannels(frames[Math.min(t, frames.length - 1)]));
const meta = {
  event: `0:${course}`,
  mode: 0,
  course,
  name: 'TAS',
  claim: { ticks: Math.max(1, ticks - 180) },
  round: start.lineup?.round ?? 3,
  character: 3,
  rider: { id: start.rider, kind: 'rider', base: null, career: false, outfit: null, attributes: start.attributes ?? Array(7).fill(5), uber: [], uberKey: '' },
  ticks,
  finishTick: ticks - 1,
  highlights: [],
  events: [],
  snapshot: start.snapshot,
  lineup: start.lineup,
  core: '0000000000000000',
  build: 'native-test',
  giveUp: false
};
fs.writeFileSync(out, encodeReplayFile({ meta, pad: rec.exportBytes() }));
console.log(`${out}: ${ticks} ticks, ${rec.exportBytes().length} pad bytes`);
