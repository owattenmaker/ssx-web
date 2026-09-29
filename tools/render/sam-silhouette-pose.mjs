// Render a rider's FE preview alone (UI canvases hidden) on a magenta and a green clear colour, at several yaws: the
// two renders give the exact coverage (alpha) for tools/export_sam_roster.py. Needs the dev server on :5173.
// node tools/render/sam-silhouette-pose.mjs OUTDIR 0,-40 sam
import os from 'node:os';
import path from 'node:path';
// playwright-core from the npx cache and Chrome for Testing from the Playwright cache (PLAYWRIGHT_CORE / CHROME override)
const { chromium } = await import(process.env.PLAYWRIGHT_CORE || path.join(os.homedir(), '.npm/_npx/5e2e484947874241/node_modules/playwright-core/index.mjs'));
const exe = process.env.CHROME || path.join(os.homedir(), 'Library/Caches/ms-playwright/chromium-1228/chrome-mac-arm64/Google Chrome for Testing.app/Contents/MacOS/Google Chrome for Testing');
const out = process.argv[2], yaws = (process.argv[3] || '0').split(',').map(Number), rider = process.argv[4] || 'sam';
const browser = await chromium.launch({ executablePath: exe, headless: false, args: ['--enable-unsafe-webgpu', '--window-size=1400,1100'] });
const page = await browser.newPage({ viewport: { width: 1280, height: 960 } });
await page.goto('http://127.0.0.1:5173/index.html?qa=1&perf=1');
for (let i = 0; i < 120 && !(await page.evaluate(() => !!window.ssxQA && window.ssxQA.ui()?.ready)); i++) await page.waitForTimeout(500);
await page.evaluate((id) => { const ui = window.ssxQA.ui(); ui.set('main'); ui.set('character'); ui.riderIndex = ui.riders.findIndex((r) => r.id === id); }, rider);
await page.waitForTimeout(6000);
for (const yaw of yaws) {
  await page.evaluate((yaw) => { const cs = window.ssxQA.ui().characterSelect; cs.yaw = yaw; if (cs.anim) cs.anim.t = 0; document.querySelector('#ui').style.visibility = 'hidden'; document.querySelector('#ui-bg').style.visibility = 'hidden'; }, yaw);
  await page.waitForTimeout(80);
  await page.evaluate(() => { const cs = window.ssxQA.ui().characterSelect; if (cs.anim) { cs.anim.t = 0; cs.anim.frozen = true; } });
  for (const [key, colour] of [['m', 0xff00ff], ['g', 0x00ff00]]) {
    await page.evaluate((c) => { const r = window.__perfRenderer; r.setClearColor(c, 1); const { scene } = window.__perfScene(); scene.background = null; }, colour);
    await page.waitForTimeout(150);
    await page.locator('#game').screenshot({ path: `${out}/pose-${rider}-${yaw}-${key}.png` });
  }
}
await browser.close();
