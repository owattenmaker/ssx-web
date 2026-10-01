// Audio field counters (docs/audio-logic.md 9.13): what the audio layer did late or dropped this page session. Plain
// increments at the places that decide (web/pathfinder.js, web/sfx.js, web/audio-speech.js, web/audio-engine.js); web/diagnostics.js
// sends them with its heartbeat to /mp/diag when they changed, so real devices report what the lab cannot hear. No timers, no
// allocation on the hot paths. globalThis.__ssxAudioStats is the same object (QA).
//   musicLate / musicLateMaxMs   a music bar or loop slice started after its due time (a gap before it; resumes excluded)
//   musicMissed                  a committed bar that never played (not decoded / not downloaded before it ended)
//   musicPumpMaxMs               the longest interval between two music scheduler pumps while the page was visible
//   decodeSlow / decodeMaxMs     main-thread decodes (music bar, speech line, sound patch at play time) over 8 ms / the longest,
//   decodeWorst                  and what that was ('music', 'speech', 'sfx:<bank>/<sound>')
//   sfxStolen / sfxDropped       voices stolen for a new request (3BA0B0) / requests that got no voice at all
//   sfxGated                     requests dropped while the context was stopped (audioInterrupt; before: all played at once later)
//   speechLate                   a speech line started after its planned time
//   ctxInterrupted / ctxResumeFailed   the context left 'running' on its own (iOS interruption) / a resume that was refused
export const audioStats = { musicLate: 0, musicLateMaxMs: 0, musicMissed: 0, musicPumpMaxMs: 0, decodeSlow: 0, decodeMaxMs: 0, decodeWorst: '',
  sfxStolen: 0, sfxDropped: 0, sfxGated: 0, speechLate: 0, ctxInterrupted: 0, ctxResumeFailed: 0 };
try { globalThis.__ssxAudioStats = audioStats; } catch {}
export function noteMusicLate(ms) { audioStats.musicLate++; if (ms > audioStats.musicLateMaxMs) audioStats.musicLateMaxMs = Math.round(ms); }
export function noteDecode(ms, kind = '') { if (ms > 8) audioStats.decodeSlow++; if (ms > audioStats.decodeMaxMs) { audioStats.decodeMaxMs = Math.round(ms); audioStats.decodeWorst = kind; } }
// A copy of the counters that are not zero (null when all are): the heartbeat's payload.
export function audioStatsSnapshot() { let out = null; for (const k in audioStats) if (audioStats[k]) (out ??= {})[k] = audioStats[k]; return out; }
