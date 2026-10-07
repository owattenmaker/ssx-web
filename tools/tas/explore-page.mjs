// Scratch exploration: open the page on BRA2 with Mac and run JS snippets given on the command line, printing each result.
//   node tools/tas/explore-page.mjs "<js>" ...
import { startBrowser, startServer, sleep } from '../../web/headless-chrome.mjs';

const snippets = process.argv.slice(2);
const browser = await startBrowser({ width: 640, height: 480 });
const server = await startServer();
try {
  await browser.goto(`${server.origin}/manifest.webmanifest`);
  await browser.evaluate('(()=>{localStorage.clear();sessionStorage.clear();return 1})()');
  await browser.goto(`${server.origin}/?qa=1&course=BRA2&rider=mac&cutscenes=0&quality=low&simtrace=1&mute=1`);
  await browser.waitFor('!!window.ssxQA || !!document.body.dataset.loadError', 300000);
  for (const s of snippets) {
    if (s.startsWith('wait:')) {
      await browser.waitFor(s.slice(5), 300000);
      console.log('waited', s.slice(5));
      continue;
    }
    if (s.startsWith('sleep:')) {
      await sleep(+s.slice(6));
      continue;
    }
    const v = await browser.evaluate(s);
    console.log('>', s.slice(0, 120), '\n', typeof v === 'string' ? v : JSON.stringify(v));
  }
} catch (e) {
  console.error(e.message);
} finally {
  for (const l of browser.logs.filter((l) => /error|exception/i.test(l)).slice(0, 20)) console.log('LOG', l);
  await browser.close();
  await server.close();
}
