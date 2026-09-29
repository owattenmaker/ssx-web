// Equip Gear icons for Sam's outfit items (tools/sam_ps2/SamTextures.cs packs them as su03..su06 into sam_icons.ssh;
// tools/export_wardrobe.py copies them into WARDROBE/SAM/icons). Each icon is the item rendered on Sam's own browser
// package (bind pose, plain lighting, transparent background) with viewer.html, cropped to the item and saved 64x64
// into the git-ignored local/sam-model/icons (they show derived geometry).
//   node tools/sam_icons/render.mjs          (needs web/public/assets/RIDER_SAM_* from tools/build_sam_web.py --all)
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import os from 'node:os';
// playwright-core from the npx cache (PLAYWRIGHT_CORE=/path/to/playwright-core/index.mjs overrides)
const { chromium } = await import(process.env.PLAYWRIGHT_CORE || path.join(os.homedir(), '.npm/_npx/5e2e484947874241/node_modules/playwright-core/index.mjs'));
const root = path.resolve(path.dirname(new URL(import.meta.url).pathname), '../..');
const exe = process.env.CHROME || path.join(os.homedir(), 'Library/Caches/ms-playwright/chromium-1228/chrome-mac-arm64/Google Chrome for Testing.app/Contents/MacOS/Google Chrome for Testing');
const out = path.join(root, 'local/sam-model/icons'); fs.mkdirSync(out, { recursive: true });
const mounts = [['/three/', path.join(root, 'web/node_modules/three/build/')], ['/assets/', path.join(root, 'web/public/assets/')], ['/', path.join(root, 'tools/sam_icons/')]];
const types = { '.html': 'text/html', '.js': 'text/javascript', '.json': 'application/json', '.png': 'image/png', '.bin': 'application/octet-stream' };
const server = http.createServer((req, res) => {
  const url = decodeURIComponent(req.url.split('?')[0]);
  for (const [prefix, dir] of mounts) if (url.startsWith(prefix)) {
    const file = path.join(dir, url.slice(prefix.length) || 'viewer.html');
    if (file.startsWith(dir) && fs.existsSync(file) && fs.statSync(file).isFile()) { res.writeHead(200, { 'content-type': types[path.extname(file)] || 'application/octet-stream' }); fs.createReadStream(file).pipe(res); return; }
  }
  res.writeHead(404); res.end();
});
await new Promise((r) => server.listen(0, '127.0.0.1', r));
const port = server.address().port;
// item icon: package, view, crop box in the 256px render (tops: the jacket; kits: the back)
const ICONS = [
  ['su03', 'RIDER_SAM_PACKERS', 'view=three&cy=0.95&ty=0.92', [44, 28, 212, 196]],
  ['su04', 'RIDER_SAM_LODGE', 'view=three&cy=0.95&ty=0.92', [44, 28, 212, 196]],
  ['su05', 'RIDER_SAM_FISHING', 'view=back&yaw=-0.5&cy=1.0&ty=0.95', [44, 20, 212, 188]],
  ['su06', 'RIDER_SAM_FISHING_TUBE', 'view=back&yaw=-0.5&cy=1.0&ty=0.95', [44, 20, 212, 188]],
];
const browser = await chromium.launch({ executablePath: exe, headless: true });
const page = await browser.newPage({ viewport: { width: 256, height: 256 } });
for (const [name, pkg, view, [x0, y0, x1, y1]] of ICONS) {
  await page.goto(`http://127.0.0.1:${port}/viewer.html?bg=none&w=256&h=256&riders=s&s=assets/${pkg}/&cd=2.3&fov=30&${view}`);
  await page.waitForFunction(() => window.done, null, { timeout: 20000 });
  const png = await page.screenshot({ omitBackground: true, clip: { x: x0, y: y0, width: x1 - x0, height: y1 - y0 } });
  // 64x64 downscale in the page (canvas), keeps alpha
  const small = await page.evaluate(async (b64) => {
    const img = new Image(); img.src = 'data:image/png;base64,' + b64; await img.decode();
    const c = document.createElement('canvas'); c.width = c.height = 64; const g = c.getContext('2d'); g.imageSmoothingQuality = 'high'; g.drawImage(img, 0, 0, 64, 64);
    return c.toDataURL('image/png').split(',')[1];
  }, png.toString('base64'));
  fs.writeFileSync(path.join(out, `${name}.png`), Buffer.from(small, 'base64'));
  console.log(name, pkg);
}
await browser.close(); server.close();
