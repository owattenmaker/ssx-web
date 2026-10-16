// The time-attack podium's script and its length (server setting podiumFit, docs/big-multiplayer.md "The podium's world and its
// hand-off to the vote"): the server picks the winner's podium scene the way the page's ScriptChoice does (web/cutscenes.js
// chooseScript, 0x27B0C0: the entries of the location's choice list whose masks fit the cast, the least played in this session,
// then random), sends its number with the results so every page plays the same scene, and ends the podium phase with it.
// Lists (CUTSCENES/index.json filters[location], from scfilter<LOC>.dat): 9 the winner's win_ps_<char>{a,b,c} (mask word 0 = the
// winner's base character bit), 10 win_ps_cheat for a winner in cheat skins 10..20 or 28 (0x27AC60). Only mask word 0 (the winner) is
// used by lists 9 and 10 on the disc; an entry with another word set makes the table refuse the location (null: the fixed phase).
// Imports only node: (web/server runs on its own on the host).
import fs from 'node:fs';
import path from 'node:path';

const WIN = 9;
const WIN_CHEAT = 10;
// 0x27AC60: a winner in cheat skins 10..20 or 28 gets win_ps_cheat (web/cutscenes.js playCutscene `cheat`)
const cheatScene = (c) => (c >= 10 && c <= 20) || c === 28;
const riderBit = (c) => (c >= 0 && c < 10 ? 1 << c : 0);

// assets: the public assets root (CUTSCENES/index.json, riders.json). Null when either is missing or unreadable.
export function podiumScripts(assets) {
  let index = null;
  let riders = null;
  try {
    index = JSON.parse(fs.readFileSync(path.join(assets, 'CUTSCENES', 'index.json'), 'utf8'));
    riders = JSON.parse(fs.readFileSync(path.join(assets, 'riders.json'), 'utf8'));
  } catch {
    return null;
  }
  const ticks = new Map((index.scripts ?? []).map((s) => [s.number, s.duration]));
  const tickHz = index.tick_hz ?? 60;
  const roster = new Map((Array.isArray(riders) ? riders : []).map((r) => [r.id, r]));
  // the character whose bit the masks test (web/main.js cutsceneRider: a cheat skin counts as its base rider, Zoe without one)
  const baseCharacter = (rider, base) => {
    const e = roster.get(rider);
    if (!e) return -1;
    if (e.kind !== 'cheat') return e.character ?? -1;
    return roster.get(base || 'zoe')?.character ?? -1;
  };
  return {
    tickHz,
    // podium: the session's top three [{rider, base}]; plays: Map script -> times shown in this session; random: () => [0, 1)
    // -> {script, ticks} or null (no list for the location, an unknown rider, a mask the server does not model)
    choose(location, podium, plays = new Map(), random = Math.random) {
      const lists = index.filters?.[location];
      const winner = podium?.[0];
      if (!lists || !winner) return null;
      const own = roster.get(winner.rider)?.character ?? -1;
      const list = lists[roster.get(winner.rider)?.kind === 'cheat' && cheatScene(own) ? WIN_CHEAT : WIN] ?? [];
      if (list.some((e) => e.masks.some((m, k) => m && k > 2))) return null;
      const words = [0, 1, 2].map((k) => (podium[k] ? riderBit(baseCharacter(podium[k].rider, podium[k].base)) : 0));
      const ok = list.filter((e) => e.masks.every((m, k) => !m || (m & (words[k] || 0))));
      if (!ok.length) return null;
      const least = Math.min(...ok.map((e) => plays.get(e.script) || 0));
      const pool = ok.filter((e) => (plays.get(e.script) || 0) === least);
      const script = pool[Math.floor(random() * pool.length) % pool.length].script;
      const length = ticks.get(script);
      return Number.isFinite(length) ? { script, ticks: length } : null;
    }
  };
}
