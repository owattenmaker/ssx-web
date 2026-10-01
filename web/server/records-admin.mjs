#!/usr/bin/env node
// Online records admin, on the host (docs/online-records.md "Anti-cheat floors"): the flagged runs (under their event's floor, or
// failed by the verifier), approve one onto its board, delete one, list a board. It edits <dir>/board.json in place (written to a
// temporary file, then renamed); the running server reads the changed file before its next request.
//   node server/records-admin.mjs [--dir DIR] flagged            the flagged runs, oldest first
//   node server/records-admin.mjs [--dir DIR] list <event>        an event's listed runs ("0:ARA1"), with their verifier state
//   node server/records-admin.mjs [--dir DIR] show <id>           one run as stored
//   node server/records-admin.mjs [--dir DIR] approve <id>        a flagged run onto its board (one entry per name, the best kept)
//   node server/records-admin.mjs [--dir DIR] delete <id>         a run and its replay, listed or flagged
//   node server/records-admin.mjs [--dir DIR] requeue <id>        verify a run again on the verifier's next cycle (a live check)
// DIR: --dir, else MP_RECORDS_DIR (the host: ~/ssx-host/state/records). Exit 0 on success, 1 on a bad id, 2 on bad usage.
import path from 'node:path';
import { approveEntry, deleteEntry, findEntry, loadBoard, requeueEntry, saveBoard, timedEvent } from './records.mjs';

const args = process.argv.slice(2), at = args.indexOf('--dir');
const dir = at >= 0 ? args.splice(at, 2)[1] : process.env.MP_RECORDS_DIR;
const [command, arg] = args;
const usage = () => {
  console.error('usage: records-admin.mjs [--dir DIR] flagged | list <event> | show <id> | approve <id> | delete <id> | requeue <id>');
  process.exit(2);
};
if (!dir || !command) usage();
const file = path.join(dir, 'board.json'), replays = path.join(dir, 'replays');
const board = loadBoard(file);
const time = (t) => `${String(Math.floor(t / 3600)).padStart(2, '0')}:${String(Math.floor(t / 60) % 60).padStart(2, '0')}.${String(Math.floor(((t % 60) * 100) / 60)).padStart(2, '0')}`;
const value = (key, v) => (timedEvent(key) ? `${time(v)} (${v} ticks)` : `${v} pts`);
const line = (key, e) => [e.id, key, e.name, value(key, e.value), e.at ?? '', `verified ${e.verified ?? '-'}`, e.stale ? 'stale' : '',
  e.flagged ? `flagged: ${e.flagged.reason} (floor ${e.flagged.floor ?? '-'} from ${e.flagged.source ?? '-'})` : '', e.verifyNote ? `note: ${e.verifyNote}` : '']
  .filter(Boolean).join('  ');
switch (command) {
  case 'flagged': {
    if (!board.flagged.length) console.log('no flagged runs');
    for (const e of board.flagged) console.log(line(e.event, e));
    break;
  }
  case 'list': {
    if (!arg) usage();
    const list = board.events[arg] ?? [];
    if (!list.length) console.log(`no listed runs on ${arg}`);
    list.forEach((e, i) => console.log(`${i + 1}. ${line(arg, e)}${e.verified === false ? '  (pulled)' : ''}`));
    break;
  }
  case 'show': {
    const f = arg && findEntry(board, arg); if (!f) { console.error('no such run'); process.exit(1); }
    console.log(JSON.stringify({ event: f.key, flagged: f.flagged, ...f.entry }, null, 1));
    break;
  }
  case 'approve': {
    const r = arg ? approveEntry(board, arg, replays) : { ok: false };
    if (!r.ok) { console.error('no such flagged run'); process.exit(1); }
    saveBoard(file, board);
    console.log(r.kept ? 'approved: listed' : 'approved, but the name already has a better run listed: dropped');
    break;
  }
  case 'delete': {
    if (!arg || !deleteEntry(board, arg, replays)) { console.error('no such run'); process.exit(1); }
    saveBoard(file, board);
    console.log('deleted');
    break;
  }
  case 'requeue': {
    if (!arg || !requeueEntry(board, arg)) { console.error('no such run'); process.exit(1); }
    saveBoard(file, board);
    console.log('requeued: the verifier takes it on its next cycle (see logs/verifier.log)');
    break;
  }
  default: usage();
}
