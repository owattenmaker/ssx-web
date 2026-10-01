// One audio decode job (web/sfx.js bank patches, web/pathfinder.js MicroTalk music bars), run in
// web/audio-decode-worker.js, or on the main thread when the worker is unavailable (web/worker-guard.js).
// {bnk, patches: [patch]} or {music (the segment's SCxl bytes), sample} -> [{sampleRate, channels, length, data: [Float32Array], loopStart, loopEnd}]
import { decodeBankPatch, decodeMusicSample } from './audio-decode.js';
import { withTransfer } from './worker-guard-child.js';
// fold: a 6-channel music bar is returned as its stereo sum, as web/audio-decode.js toAudioBuffer folds it.
export function decodeAudioJob({ bnk, patches, music, sample, fold = false }) {
  const decoded = music ? [decodeMusicSample(music, null, sample)] : patches.map((p) => decodeBankPatch(bnk, p));
  if (fold && decoded[0]?.channels === 6) decoded[0] = foldStereo(decoded[0]);
  return withTransfer(decoded, decoded.flatMap((d) => d.data.map((x) => x.buffer)));
}

function foldStereo(d) {
  const data = [
    [0, 2, 4],
    [1, 3, 5]
  ].map((srcs) => {
    const mix = new Float32Array(d.length);
    for (const c of srcs) {
      const x = d.data[c];
      for (let i = 0; i < mix.length; i++) {
        const v = x[i];
        mix[i] += Number.isFinite(v) ? v : 0;
      }
    }
    return mix;
  });
  return { ...d, channels: 2, data };
}
