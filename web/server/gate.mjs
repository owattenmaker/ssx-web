// Shared-password gate for the hosted game (web/server/mp-server.mjs, docs/hosting.md). Off unless a password is
// configured (MP_GATE_PASSWORD_FILE, a file holding the password, or MP_GATE_PASSWORD). Every request (page, assets,
// /mp/status, the /mp WebSocket upgrade) needs a signed session cookie; without one a page request gets the login form
// and anything else 401. Login: POST /gate/login (form field `password`), rate limited per client address, constant-
// time comparison; success sets an HttpOnly, Secure, SameSite=Lax cookie (HMAC-SHA256 over the expiry with a secret
// kept in MP_GATE_SECRET_FILE, generated on first start so sessions survive restarts) and redirects to the page the
// visitor asked for (a friend's invite link keeps its lobby query).
import crypto from 'node:crypto';
import fs from 'node:fs';

const COOKIE = 'ssx_gate', SESSION_S = 30 * 24 * 3600, MAX_BODY = 2048;
const ATTEMPTS = { perMinute: 6, lockoutMs: 10 * 60000, lockoutAfter: 20 };
// Login page styled like the SSX 3 front end (sky gradient, orange swoosh, drifting snowflakes, slanted white title,
// right-aligned menu rows, pulsing "Press START", PS2 ✕ legend). It is served before login, so it uses no game data at
// all: only this CSS, inline SVG shapes and system fonts. The CSP pins the one <style> by hash; nothing else may load.
const GATE_STYLE = `:root{--n:#112634;--o:#f08a1c;--f:"Arial Black","Helvetica Neue",Impact,system-ui,sans-serif}
*{box-sizing:border-box}
body{margin:0;min-height:100vh;min-height:100dvh;overflow-x:hidden;color:var(--n);font:italic 900 16px/1.2 var(--f);background:#7198b0 linear-gradient(180deg,#d4e8f4 0%,#a3c9e1 26%,#75a9cb 58%,#7198b0 100%)}
.bg{position:fixed;inset:0;overflow:hidden;pointer-events:none}
.bg svg{position:absolute}
.sw{left:0;bottom:0;width:100%;height:min(44vh,400px)}
.f{width:var(--s);height:var(--s);opacity:.9;animation:drift 9s ease-in-out infinite alternate}
.f1{--s:48vmax;right:-14vmax;top:-12vmax;opacity:.2;animation:spin 60s linear infinite}
.f2{--s:26vmax;left:-9vmax;top:34%;opacity:.16;animation:spin 80s linear infinite reverse}
.f3{--s:30vmax;right:16%;bottom:-13vmax;opacity:.14;animation:spin 70s linear infinite}
.f4{--s:30px;left:14%;top:30%}.f5{--s:18px;left:47%;top:8%;animation-duration:7s}.f6{--s:24px;right:8%;top:56%;animation-duration:11s}.f7{--s:14px;left:36%;top:58%;animation-duration:6s}.f8{--s:20px;left:70%;top:24%;animation-duration:8s}
@keyframes drift{from{transform:translate(0,-5vh) rotate(0)}to{transform:translate(2vw,6vh) rotate(90deg)}}
@keyframes spin{to{transform:rotate(60deg)}}
@keyframes pulse{50%{opacity:.35}}
main{position:relative;min-height:100vh;min-height:100dvh;display:grid;grid-template-columns:minmax(0,1fr) minmax(0,480px);grid-template-rows:auto 1fr auto;column-gap:4vw;padding:max(28px,env(safe-area-inset-top)) max(36px,env(safe-area-inset-right)) max(24px,env(safe-area-inset-bottom)) max(36px,env(safe-area-inset-left))}
header{grid-column:1/-1}
h1{margin:0;color:#fff;font:italic 900 clamp(76px,12vw,168px)/.95 var(--f);letter-spacing:-.02em;transform:skewX(-10deg);transform-origin:0 100%;text-shadow:-2px -2px 0 var(--n),2px -2px 0 var(--n),-2px 2px 0 var(--n),2px 2px 0 var(--n),7px 7px 0 #1d4d74}
h1 span{color:var(--o)}
.sub{margin:10px 0 0;padding-bottom:10px;max-width:560px;border-bottom:4px dashed #fff;color:#fff;font-size:clamp(15px,1.8vw,22px);letter-spacing:.16em;text-transform:uppercase;text-shadow:2px 2px 0 var(--n)}
form{grid-column:2;grid-row:2;align-self:center;display:grid;gap:16px;justify-items:end;text-align:right;padding:24px 0}
.prompt,.legend{margin:0;color:#fff;letter-spacing:.06em;text-transform:uppercase;text-shadow:2px 2px 0 var(--n)}
.row,.go{position:relative;isolation:isolate}
.row::before,.go::before{content:"";position:absolute;inset:0;z-index:-1;transform:skewX(-14deg);background:rgba(240,247,252,.88);border-bottom:4px solid rgba(17,38,52,.4)}
.row{display:grid;grid-template-columns:auto minmax(0,1fr);align-items:center;gap:14px;width:100%;min-height:64px;padding:10px 20px 10px 24px}
.row:focus-within::before{background:#fff;box-shadow:0 0 0 3px var(--o),0 0 0 6px #fff}
input:focus{outline:0;background:#fff}
label{font-size:22px;text-transform:uppercase;color:#101c28}
input{width:100%;min-width:0;min-height:44px;padding:8px 10px;border:0;border-bottom:3px dashed var(--n);border-radius:0;background:#dcebf5;color:#101c28;font:700 20px/1 system-ui,sans-serif}
.go{min-height:60px;min-width:240px;padding:14px 36px;border:0;background:none;color:#fff;font:italic 900 28px/1 var(--f);letter-spacing:.06em;text-transform:uppercase;text-shadow:2px 2px 0 #7a3504;cursor:pointer}
.go::before{background:linear-gradient(180deg,#f9ab4e,var(--o) 45%,#d57b0b);border-bottom-color:#8a3f05}
.go:hover::before{filter:brightness(1.1)}
.go:focus-visible{outline:0}.go:focus-visible::before{box-shadow:0 0 0 3px #fff,0 0 0 6px var(--n)}
.go span{animation:pulse 1.2s ease-in-out infinite}
.err{margin:0;padding:8px 18px;background:var(--n);color:#fff;transform:skewX(-14deg)}
.err:empty{display:none}
.legend{grid-column:2;grid-row:3;justify-self:end;display:flex;align-items:center;gap:10px}
.legend svg{width:30px;height:30px}
@media (max-width:700px){main{grid-template-columns:minmax(0,1fr)}form,.legend{grid-column:1}main{padding-left:max(20px,env(safe-area-inset-left));padding-right:max(20px,env(safe-area-inset-right))}form{justify-items:stretch}.go{min-width:0;font-size:24px}.prompt{font-size:12px;letter-spacing:.02em}}
@media (orientation:landscape) and (max-height:520px){main{grid-template-columns:minmax(0,1fr) minmax(0,400px);grid-template-rows:1fr auto;padding-top:max(16px,env(safe-area-inset-top));padding-bottom:max(12px,env(safe-area-inset-bottom))}header{grid-column:1;grid-row:1;align-self:center}h1{font-size:min(12vw,30vh)}form{grid-column:2;grid-row:1;padding:0;gap:12px}.prompt{font-size:13px;letter-spacing:.02em}.legend{grid-row:2}}
@media (prefers-reduced-motion:reduce){.f,.go span{animation:none}}`;
const GATE_CSP = `default-src 'none'; style-src 'sha256-${crypto.createHash('sha256').update(GATE_STYLE).digest('base64')}'; form-action 'self'; base-uri 'none'; frame-ancestors 'none'`;

export function createGate({ password = null, passwordFile = null, secretFile = null, secure = true, clientAddress } = {}) {
  const pass = passwordFile ? fs.readFileSync(passwordFile, 'utf8').replace(/\r?\n$/, '') : password;
  if (!pass) return null;
  const passHash = crypto.createHash('sha256').update(pass).digest();
  let secret;
  if (secretFile) {
    try { secret = Buffer.from(fs.readFileSync(secretFile, 'utf8').trim(), 'hex'); } catch {}
    if (!secret || secret.length < 32) { secret = crypto.randomBytes(32); fs.writeFileSync(secretFile, secret.toString('hex') + '\n', { mode: 0o600 }); }
  } else secret = crypto.randomBytes(32);
  const sign = (expiry) => crypto.createHmac('sha256', secret).update(`v1.${expiry}`).digest('base64url');
  const token = () => { const expiry = Math.floor(Date.now() / 1000) + SESSION_S; return `v1.${expiry}.${sign(expiry)}`; };
  const cookies = (req) => Object.fromEntries(String(req.headers.cookie ?? '').split(';').map((c) => c.trim().split('=')).filter((p) => p.length === 2));
  function valid(req) {
    const m = /^v1\.(\d{1,12})\.([A-Za-z0-9_-]{43})$/.exec(cookies(req)[COOKIE] ?? ''); if (!m) return false;
    if (+m[1] < Date.now() / 1000) return false;
    const a = Buffer.from(m[2]), b = Buffer.from(sign(+m[1]));
    return a.length === b.length && crypto.timingSafeEqual(a, b);
  }
  const tries = new Map(); // address -> {count, windowAt, failures, lockedUntil}
  setInterval(() => { const t = Date.now(); for (const [k, v] of tries) if (v.lockedUntil < t && t - v.windowAt > 60000) tries.delete(k); }, 60000).unref();
  function allowAttempt(address) {
    const t = Date.now(); let e = tries.get(address);
    if (!e) tries.set(address, (e = { count: 0, windowAt: t, failures: 0, lockedUntil: 0 }));
    if (e.lockedUntil > t) return false;
    if (t - e.windowAt > 60000) { e.windowAt = t; e.count = 0; }
    return ++e.count <= ATTEMPTS.perMinute;
  }
  function failed(address) { const e = tries.get(address); if (e && ++e.failures >= ATTEMPTS.lockoutAfter) { e.lockedUntil = Date.now() + ATTEMPTS.lockoutMs; e.failures = 0; } }
  // Only same-site relative paths come back from the form (no open redirect).
  const safeNext = (n) => (typeof n === 'string' && n.startsWith('/') && !n.startsWith('//') && !n.startsWith('/\\') && n.length < 1024 ? n : '/');
  const page = (
    next,
    message = ''
  ) => `<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover"><title>SSX 3</title><style>${GATE_STYLE}</style></head>
<body><div class="bg" aria-hidden="true"><svg width="0" height="0"><defs><g id="k" fill="none" stroke="#fff" stroke-width="5" stroke-linecap="round"><path id="a" d="M0-46V46M0-30l-12-12M0-30l12-12M0 30l-12 12M0 30l12 12M0-14l-7-6M0-14l7-6M0 14l-7 6M0 14l7 6"/><use href="#a" transform="rotate(60)"/><use href="#a" transform="rotate(120)"/></g></defs></svg>
<svg class="sw" viewBox="0 0 1000 400" preserveAspectRatio="none" focusable="false"><path fill="#fff" opacity=".22" d="M0 120C330 150 680 270 1000 400H0Z"/><path fill="#fff" opacity=".85" d="M0 178C330 204 640 290 900 392C640 300 330 218 0 192Z"/><path fill="#8a3f05" d="M0 206C330 230 650 310 1000 396V400H0Z"/><path fill="#e27f0e" d="M0 220C330 244 650 322 950 400H0Z"/><path fill="#f7a445" d="M0 238C320 262 600 330 840 400H790C570 338 310 280 0 262Z"/><path fill="#c9670a" d="M0 318C240 328 440 360 560 400H0Z"/></svg>
${[1, 2, 3, 4, 5, 6, 7, 8].map((i) => `<svg class="f f${i}" viewBox="-50 -50 100 100"><use href="#k"/></svg>`).join('')}</div>
<main><header><h1>SSX <span>3</span></h1><p class="sub">Private server</p></header>
<form method="post" action="/gate/login"><p class="prompt">Enter the password and press START</p><div class="row"><label for="pw">Password</label><input id="pw" type="password" name="password" autocomplete="current-password" autofocus required></div><input type="hidden" name="next" value="${escapeHtml(next)}"><button type="submit" class="go"><span>Press START</span></button><p class="err" role="alert">${escapeHtml(message)}</p></form>
<p class="legend"><svg viewBox="0 0 30 30" aria-hidden="true" focusable="false"><circle cx="15" cy="15" r="13.5" fill="#2458b4" stroke="#fff" stroke-width="2"/><path d="M10 10l10 10M20 10L10 20" stroke="#fff" stroke-width="3.2" stroke-linecap="round"/></svg>Enter</p></main></body></html>`;
  const html = (res, status, body) => {
    res.writeHead(status, {
      'content-type': 'text/html; charset=utf-8',
      'cache-control': 'no-store',
      'x-frame-options': 'DENY',
      'referrer-policy': 'no-referrer',
      'content-security-policy': GATE_CSP
    });
    res.end(body);
  };
  return {
    valid,
    // Returns true when the request was answered here (login form / login POST / 401); false lets it through.
    handle(req, res) {
      let url; try { url = new URL(req.url, 'http://x'); } catch { res.writeHead(400); res.end(); return true; }
      if (url.pathname === '/gate/login' && req.method === 'POST') {
        const address = clientAddress(req);
        let body = '', aborted = false;
        req.on('data', (d) => { body += d; if (body.length > MAX_BODY) { aborted = true; req.destroy(); } });
        req.on('end', () => {
          if (aborted) return;
          const form = new URLSearchParams(body), next = safeNext(form.get('next'));
          if (!allowAttempt(address)) { html(res, 429, page(next, 'Too many attempts. Try again in a few minutes.')); return; }
          const given = crypto.createHash('sha256').update(form.get('password') ?? '').digest();
          if (!crypto.timingSafeEqual(given, passHash)) { failed(address); html(res, 401, page(next, 'Wrong password.')); return; }
          res.writeHead(303, { location: next, 'cache-control': 'no-store', 'set-cookie': `${COOKIE}=${token()}; Path=/; Max-Age=${SESSION_S}; HttpOnly; SameSite=Lax${secure ? '; Secure' : ''}` });
          res.end();
        });
        return true;
      }
      if (valid(req)) return false;
      const wantsPage = req.method === 'GET' && (url.pathname === '/' || url.pathname.endsWith('.html')) && !url.pathname.startsWith('/mp');
      if (wantsPage) { html(res, 200, page(url.pathname + url.search)); return true; }
      res.writeHead(401, { 'content-type': 'text/plain', 'cache-control': 'no-store' }); res.end('login required');
      return true;
    },
  };
}
function escapeHtml(s) { return String(s).replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' })[c]); }
