import { testMuted } from './audio-engine.js';
// Full-screen front-end movies (the MoviePlayer state 0x1D23E0: 'MoviePlayer' object 0x1D22F0, the sound system paused
// 0x2B3A70 while it plays, popped back 0x1D2638 when it ends or on a skip button 0x1D2518). Used by the rewards room
// Videos (web/fe-options.js) and Main menu > Previews (web/fe-previews.js). Files: tools/export_movies.py ->
// /assets/MOVIES/<KEY>.mp4 + movies.json (git-ignored; needs ffmpeg).

let index = null;
// movies.json ({movies: [{key, name, reward, src, preview?, ...}]}); an empty list when it is missing.
export function loadMovieIndex(fetchFn = globalThis.fetch) {
  return index ??= Promise.resolve().then(() => fetchFn('/assets/MOVIES/movies.json'))
    .then((r) => (r?.ok ? r.json() : { movies: [] })).catch(() => ({ movies: [] }));
}

// Play entry m (a movies.json row) in a <video> over #stage (object-fit fill: the 4:3 picture as the PS2 shows it) with
// the game audio engine suspended. onEnd(reason) runs once when it ends, fails ('error') or is clicked ('skip');
// handle.stop() (a skip key, leaving the screen) removes it without calling onEnd.
// skippable: false (the boot logos, web/fe-attract.js): a click does not end it.
// reveal (the boot intro, web/fe-attract.js): the video stays transparent (still clickable) until its first frame plays, so the
// screen under it shows while it buffers; onPlaying() runs then.
export function startMovie(ui, m, onEnd = () => {}, { skippable = true, reveal = false, onPlaying = null } = {}) {
  if (typeof document === 'undefined' || !m?.src) return null;
  const v = document.createElement('video');
  if (testMuted) v.muted = true;   // automated test browsers stay silent (web/audio-engine.js)
  v.src = '/assets/' + m.src; v.playsInline = true; v.preload = 'auto'; v.dataset.movie = m.key;
  v.style.cssText = 'position:absolute;inset:0;width:100%;height:100%;object-fit:fill;background:#000;z-index:5;cursor:pointer';
  let done = false;
  const stop = () => {
    if (done) return false; done = true;
    v.pause(); v.removeAttribute('src'); v.load(); v.remove();
    try { ui.gameAudio?.engine?.resume?.(); } catch {}
    return true;
  };
  const end = (reason) => () => { if (stop()) onEnd(reason); };
  v.onended = end('ended'); v.onerror = end('error'); if (skippable) v.onclick = (e) => { e.stopPropagation(); end('skip')(); }; else v.style.cursor = 'default';
  if (reveal) { v.style.opacity = '0'; v.addEventListener('playing', () => { if (done) return; v.style.opacity = '1'; onPlaying?.(); }, { once: true }); }
  (ui.stage || document.querySelector('#stage') || document.body).appendChild(v);
  try { ui.gameAudio?.engine?.suspend?.(); } catch {}
  v.play().catch(() => { v.muted = true; return v.play(); }).catch(end('error'));   // autoplay policy: retry muted
  return { video: v, stop, get done() { return done; } };
}
