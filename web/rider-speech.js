// Front-end rider speech (docs/audio-logic.md 4.5, docs/characters.md): the FE preview slot's flags +0xC20 / +0xC1C
// are polled by 1A0358 on the next preview update:
//   +0xC20 (set by Select Character's Cross, 0x18185C)  -> 2A16B0 Post_Selection (event 0x20BC), only when
//          0x535C11 != 0 (not Conquer the Mountain), when Setup Character comes up with the cheer (semantic 435);
//   +0xC1C (set by the Equip Gear commit, 0x199EA4)     -> 2A1778 Customize (0x20BD), in every mode.
// Both use the CHARACTER bus of the preview slot's character (+0: the base rider; a cheat skin speaks with its base
// rider's voice); 29F2B0 maps the character to the speaker. The banks are SPEECH.BIG <Bank>_<abbr>.
// Playback is web/game-audio.js speak(); the caller passes it in (ui.cb.speech).
export const SPEECH_ABBR = { allegra: 'ari', elise: 'eli', griff: 'grf', kaori: 'kao', mac: 'mac', moby: 'mob', nate: 'nat', psymon: 'psy', viggo: 'vig', zoe: 'zoe' };

// The bank for a front-end cue: kind 'select' (Post_Selection) or 'customize' (Customize); null when the rider has no
// original voice (Sam) or the original would not speak (Post_Selection in Conquer the Mountain).
export function frontEndSpeechBank(kind, rider, { career = false } = {}) {
  const base = rider?.kind === 'cheat' ? (rider.base || 'zoe') : rider?.id;
  const abbr = SPEECH_ABBR[base];
  if (!abbr) return null;
  if (kind === 'select') return career ? null : `Post_Selection_${abbr}`;
  if (kind === 'customize') return `Customize_${abbr}`;
  return null;
}

export function speakFrontEnd(ui, kind, rider) {
  const bank = frontEndSpeechBank(kind, rider, { career: !!ui?.careerMode });
  if (!bank || !ui?.cb?.speech) return null;
  return ui.cb.speech(bank, { bus: 'CHARACTER' });
}
