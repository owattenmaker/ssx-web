#!/usr/bin/env node
// `npm run online`: the multiplayer server plus the game's dev server, reachable from other machines on the
// network (Vite --host 0.0.0.0). Share the printed http://<LAN address>:5173/ link with friends.
// PORT=5174 npm run online uses another port (e.g. when a dev server already holds 5173).
import { spawn } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import path from 'node:path';
const web = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const children = [
  spawn(process.execPath, ['server/mp-server.mjs'], { cwd: web, stdio: 'inherit' }),
  spawn(process.execPath, ['node_modules/vite/bin/vite.js', '--host', '0.0.0.0', '--port', String(process.env.PORT || 5173), '--strictPort'], { cwd: web, stdio: 'inherit' }),
];
const stop = () => { for (const c of children) c.kill(); process.exit(0); };
process.on('SIGINT', stop); process.on('SIGTERM', stop);
for (const c of children) c.on('exit', (code) => { if (code) console.error('online: a process exited with', code); });
