// Audio menu rules (web/audio-menu.js, docs/audio-menus.md): menu row -> radio mode (0x196540), disabled rows and
// help (0x196B08 / 0x196960), Request Line order / toggling / buying (0x197AD8, 0x1977D0, 0x197500, 0x1981F8,
// 0x198340), slider knob (0x39E130) and the 0x294F78 event numbers. The exported layouts are checked when present.
import assert from 'node:assert/strict';
import { existsSync, readFileSync } from 'node:fs';
import { MENU_MODES, musicDisabled, musicHelp, songOrder, songState, songHelp, songButton, toggleSong, knobX, scrollTop, playlistBits, uiEvent } from './audio-menu.js';

// 140audio rows: radio big, big mountain, request line, playlist -> SetRadioMode 0, 2, 1, 3.
assert.deepEqual([...MENU_MODES], [0, 2, 1, 3]);
assert.deepEqual(musicDisabled({ has: false, ctm: false }), [false, false, true, true, true]);
assert.deepEqual(musicDisabled({ has: false, ctm: true }), [false, false, true, true, false]);   // lodge: Edit Playlist to create one
assert.deepEqual(musicDisabled({ has: true, ctm: false }), [false, false, false, false, false]);
assert.equal(musicHelp(0, { has: false }), 'kT_HELPAUDIORadioBig');
assert.equal(musicHelp(1, { has: false }), 'kT_HELPAUDIOBMA');
assert.equal(musicHelp(2, { has: true }), 'kT_FEHELPCustPlayDJ');
assert.equal(musicHelp(3, { has: true }), 'kT_FEHELPCustPlayNoDJ');
assert.equal(musicHelp(2, { has: false, ingame: true }), 'kT_HELPAUDIOCreateInLodge');
assert.equal(musicHelp(2, { has: false, ctm: true }), 'kT_HELPAUDIOCreate');
assert.equal(musicHelp(3, { has: false }), 'kT_HELPAUDIOCreateInCTM');
assert.equal(musicHelp(4, { has: false, ctm: true }), 'kT_HELPAUDIOEditReq');
assert.equal(musicHelp(4, { has: false }), 'kT_HELPAUDIOCreateInCTM');

// Request Line: owned songs first; the last song cannot be removed; unowned songs only buyable in CTM.
const owned = [false, true, false, true, true], playlist = [false, true, false, false, false];
assert.deepEqual(songOrder(owned), [1, 3, 4, 0, 2]);
assert.deepEqual(playlistBits({ playlist: [true, true, true, false, true] }, owned), [false, true, false, false, true]);
let st = songState(1, { owned, playlist, ctm: false });
assert.equal(st.last, true); assert.equal(st.selectable, false);
assert.equal(songHelp(st, { ctm: false }), 'kT_HELPAudio1SongMin'); assert.equal(songButton(st), null);
assert.deepEqual(toggleSong(playlist, 1), playlist);                                   // stays in the list
st = songState(3, { owned, playlist, ctm: false });
assert.equal(st.selectable, true); assert.equal(songButton(st), 'kT_BTNAddSong'); assert.equal(songHelp(st, {}), 'kT_HELPAUDIOAddRem');
assert.deepEqual(toggleSong(playlist, 3), [false, true, false, true, false]);
assert.equal(songButton(songState(1, { owned, playlist: [false, true, false, true, false] })), 'kT_BTNRemSong');
st = songState(0, { owned, playlist, ctm: false });
assert.equal(st.selectable, false); assert.equal(songHelp(st, { ctm: false }), 'kT_FAQRADIOBA');
st = songState(0, { owned, playlist, ctm: true, cash: 0 });                             // 3 owned: 3 free credits left
assert.equal(st.credits, 3); assert.equal(st.buy, true); assert.equal(songButton(st), 'kT_16BuyMusicTracks'); assert.equal(songHelp(st, { ctm: true }), 'kT_HELPAUDIOAvailBuy');
const six = [true, true, true, true, true, true, false];
st = songState(6, { owned: six, playlist: six, ctm: true, cash: 4999 });
assert.equal(st.credits, 0); assert.equal(st.selectable, false); assert.equal(songHelp(st, { ctm: true }), 'kT_16SaveMoreCarsh');
assert.equal(songState(6, { owned: six, playlist: six, ctm: true, cash: 5000 }).buy, true);

// Sliders: 12 steps, knob from the track's left edge to (trackW - knobW).
assert.equal(knobX(0, 160, 18), 0); assert.equal(knobX(11, 160, 18), 142); assert.equal(knobX(20, 160, 18), 142);
assert.ok(Math.abs(knobX(5, 120, 18) - 5 * 102 / 11) < 1e-9);
// 8-row list window.
assert.equal(scrollTop(0, 7, 35), 0); assert.equal(scrollTop(0, 8, 35), 1); assert.equal(scrollTop(10, 3, 35), 3); assert.equal(scrollTop(0, 34, 35), 27);
// 0x294F78: FE accept/back/move/scroll/error 0..4, in game 9..13.
assert.deepEqual(['accept', 'back', 'move', 'scroll', 'error'].map((n) => uiEvent(n, false)), [0, 1, 2, 3, 4]);
assert.deepEqual(['accept', 'back', 'move', 'scroll', 'error'].map((n) => uiEvent(n, true)), [9, 10, 11, 12, 13]);

// Exported layouts (tools/export_audio_menus.py; git-ignored asset): widget names and slider children.
const file = new URL('./public/assets/UI/audio-menus.json', import.meta.url);
if (existsSync(file)) {
  const data = JSON.parse(readFileSync(file, 'utf8'));
  for (const key of ['140audio', '16radio', '141advsettings', '142audio_pda', '143radio_pda']) assert.ok(data.screens[key], key);
  const labels = (k) => new Map(data.screens[k].elements.filter((e) => e.label).map((e) => [e.label, e]));
  for (const k of ['140audio', '142audio_pda']) { const l = labels(k); for (const n of ['0box', '1box', '2box', '3box', 'radio big', 'big mountain', 'request line', 'playlist', 'edit req', 'helptext']) assert.ok(l.has(n), `${k} ${n}`); }
  const r = labels('16radio'); for (const n of ['songs0', 'songs7', 'arr_up', 'arr_down', 'song list', 'song by', 'cost', 'free songs', 'btext_select', 'btext_preview']) assert.ok(r.has(n), n);
  const sliders = data.screens['141advsettings'].elements.filter((e) => e.widget === 'slider');
  assert.equal(sliders.length, 3); for (const s of sliders) assert.equal(s.children.length, 2);
  assert.equal(data.strings.kT_OVRCMNSongsInList, 'Songs in your playlist:  %d');
  console.log('audio menus: layouts ok');
}
console.log('audio menu rules ok');
