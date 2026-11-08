// Draws the 1200x630 share images (public/og/*.jpg) from the brand fonts and a game still.
// Run it by hand after changing a page's share line or the brand, then commit the images.
//
//   npm i --no-save playwright@1.49.1 && npx playwright install chromium
//   node scripts/make-og.mjs            # or PLAYWRIGHT_CHANNEL=msedge to use an installed Edge

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { chromium } from "playwright";

const site = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const pub = path.join(site, "public");
const out = path.join(pub, "og");
fs.mkdirSync(out, { recursive: true });

const cards = [
  { file: "home", kicker: "Open-source life/RP for Arma Reforger", title: "We are building the modern Life Framework", still: "spawn-square-1280" },
  { file: "roadmap", kicker: "Roadmap", title: "Where we are on the road", still: "village-overview-640" },
  { file: "devlog", kicker: "Devlog", title: "What changed, day by day", still: "village-post-640" },
  { file: "servers", kicker: "For server owners", title: "Host it, tune it, build on it", still: "village-police-640" },
  { file: "contribute", kicker: "Contribute", title: "Pick a scope. Prove it in the game.", still: "village-fuel-640" },
  { file: "decisions", kicker: "Decisions", title: "The calls that set the taste", still: "village-square-640" },
  { file: "docs", kicker: "Docs", title: "The paperwork", still: "sign-board-1280" },
];

const url = (p) => pathToFileURL(path.join(pub, p)).href;
const html = (c) => `<!doctype html><html><head><style>
@font-face{font-family:B;font-weight:700;src:url(${url("fonts/barlow-condensed-latin-700-normal.woff2")})}
@font-face{font-family:M;font-weight:500;src:url(${url("fonts/ibm-plex-mono-latin-500-normal.woff2")})}
*{margin:0;box-sizing:border-box}
body{width:1200px;height:630px;background:#f0ebe1;color:#1b1d19;font-family:B;display:grid;grid-template-columns:640px 1fr;overflow:hidden}
.l{padding:56px 48px 48px 64px;display:flex;flex-direction:column;border-right:3px solid #1b1d19}
.mark{display:flex;align-items:center;gap:20px;font-size:40px;text-transform:uppercase}
.k{font-family:M;font-size:20px;letter-spacing:.08em;text-transform:uppercase;color:#54564d;margin-top:auto}
h1{font-size:${c.title.length > 30 ? 84 : 96}px;line-height:.93;text-transform:uppercase;margin-top:14px}
.r{position:relative;background:#e5ddcd}
.r img{width:100%;height:100%;object-fit:cover;filter:saturate(.85)}
.s{z-index:1;position:absolute;left:28px;top:40px;font-family:M;font-size:18px;letter-spacing:.1em;text-transform:uppercase;color:#a3302a;border:3px solid #a3302a;outline:1.5px solid #a3302a;outline-offset:3px;padding:8px 10px 6px;transform:rotate(-3deg);background:rgba(240,235,225,.9)}
.bar{z-index:1;position:absolute;left:0;right:0;top:0;height:14px;background:#e3b23c;border-bottom:3px solid #1b1d19}
</style></head><body>
<div class="l"><div class="mark"><img src="${url("brand/logo-light.svg")}" width="132" height="132">Life Framework</div>
<p class="k">${c.kicker}</p><h1>${c.title}</h1></div>
<div class="r"><div class="bar"></div><img src="${url(`media/${c.still}.webp`)}"><span class="s">In development</span></div>
</body></html>`;

const browser = await chromium.launch(process.env.PLAYWRIGHT_CHANNEL ? { channel: process.env.PLAYWRIGHT_CHANNEL } : {});
const page = await browser.newPage({ viewport: { width: 1200, height: 630 } });
for (const c of cards) {
  const tmp = path.join(out, `.${c.file}.html`);
  fs.writeFileSync(tmp, html(c));
  await page.goto(pathToFileURL(tmp).href);
  await page.evaluate(() => document.fonts.ready);
  await page.screenshot({ path: path.join(out, `${c.file}.jpg`), type: "jpeg", quality: 82 });
  fs.rmSync(tmp);
  console.log(`public/og/${c.file}.jpg`);
}
await browser.close();
