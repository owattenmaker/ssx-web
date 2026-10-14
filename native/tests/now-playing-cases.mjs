// Cases for native/tests/now_playing_test.cpp: web/now-playing.js NowPlayingHud over every MUSIC.INF playlist song (the game's fonts),
// ticked through its whole popup (8 s), each tick's state, box (nowPlayingDrawList) and lines compared exactly.
//   node now-playing-cases.mjs --run TEST_EXE [ASSETS]   (ctest frontend_now_playing; exits 77 without the game data)
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { NowPlayingHud, nowPlayingDrawList } from '../../web/now-playing.js';

const args = process.argv.slice(2);
const exe = args[args.indexOf('--run') + 1];
const assets = args.find((a, i) => i > 0 && args[i - 1] !== '--run' && a !== '--run') ?? path.resolve(import.meta.dirname, '../../web/public/assets');
const ui = path.join(assets, 'UI');
const catalogPath = path.join(assets, 'AUDIO', 'catalog.json');
if (!fs.existsSync(path.join(ui, 'FEFONT-glyphs.json')) || !fs.existsSync(catalogPath)) {
  console.log('no game data: skipped');
  process.exit(77);
}
const fe = JSON.parse(fs.readFileSync(path.join(ui, 'FEFONT-glyphs.json'), 'utf8'));
const hud = JSON.parse(fs.readFileSync(path.join(ui, 'HUDFONT-glyphs.json'), 'utf8'));
const catalog = JSON.parse(fs.readFileSync(catalogPath, 'utf8'));
const songs = catalog.songs.filter((s) => s.ADDTOFE).map((s) => ({ title: s.TITLE ?? '', artist: s.ARTIST ?? '', album: s.ALBUM ?? '', ticks: 490 }));
// a wrapped title and empty lines too
songs.push({ title: 'A Title Long Enough To Wrap Over The Three Hundred Pixel Line Of The Popup Box', artist: '', album: '', ticks: 30 });
const expected = songs.map((c) => {
  const np = new NowPlayingHud({ fonts: { FEFONT: fe, HUDFONT: hud }, images: {} });
  np.show(c);
  const steps = [];
  for (let k = 0; k < c.ticks; k++) {
    np.tick();
    const box = np.layout ? nowPlayingDrawList(np.layout, np.slide, np.alpha).box : { x: 0, y: 0, w: 0, h: 0 };
    const lines = np.layout ? np.layout.lines.map((l) => [l.text, l.w, l.h]) : [];
    steps.push([np.active, np.time, np.slide, np.alpha, box.x, box.y, box.w, box.h, lines]);
  }
  return steps;
});
const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'now-playing-'));
const inPath = path.join(dir, 'cases.json');
const outPath = path.join(dir, 'out.json');
fs.writeFileSync(inPath, JSON.stringify({ fe, hud, songs }));
const r = spawnSync(exe, [inPath, outPath], { stdio: 'inherit' });
if (r.status !== 0) process.exit(1);
const got = JSON.parse(fs.readFileSync(outPath, 'utf8'));
fs.rmSync(dir, { recursive: true, force: true });
let bad = 0;
let steps = 0;
for (let c = 0; c < songs.length; c++) {
  for (let k = 0; k < songs[c].ticks; k++) {
    steps++;
    // numbers compared as values (a native double prints 20.0, JS 20)
    const a = JSON.stringify(expected[c][k]);
    const b = JSON.stringify(JSON.parse(JSON.stringify(got[c][k])));
    if (a !== b) {
      if (bad < 5) console.log(`${songs[c].title} tick ${k}\n  page   ${a}\n  native ${b}`);
      bad++;
    }
  }
}
console.log(`${songs.length} songs, ${steps} ticks: ${bad} differ`);
process.exit(bad ? 1 : 0);
