// Draws the Workshop preview candidates for the 0.1.0 upload (docs/release/workshop-upload-0.1.0.md "Pictures"):
//   public/docs-images/release-0.1.0/source/*.jpg        native `tools\cli look` frames, Workbench overlay marks removed
//   public/docs-images/release-0.1.0/<slot>.jpg          the same frames at 1280x720 (Workshop screenshot size)
//   public/docs-images/release-0.1.0/preview-*.jpg       1920x1080 preview-card candidates, brand fonts and tokens
//   public/docs-images/release-0.1.0/stills.json         camera, hour and source of every still
//
//   npm i --no-save playwright@1.49.1 && npx playwright install chromium   # or PLAYWRIGHT_CHANNEL=msedge
//   node scripts/make-workshop-previews.mjs stills      # needs the tmp/look* folders of the look runs (git-ignored)
//   node scripts/make-workshop-previews.mjs cards       # needs only the committed source/ frames
//   node scripts/make-workshop-previews.mjs             # both
//
// The cards follow BRAND.md: paper, ink, stamp red once, a sign-yellow plate once, hard offset shadow, no gradients.
// No mock-ups: the only pictures are game frames. The competitor's name is never written here.

import fs from "node:fs";
import path from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { chromium } from "playwright";

const site = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");
const repo = path.resolve(site, "..");
const pub = path.join(site, "public");
const out = path.join(pub, "docs-images", "release-0.1.0");
const srcDir = path.join(out, "source");
fs.mkdirSync(srcDir, { recursive: true });

// slot -> the look frame. `hour` is the solar time of the run; the area names are the temporary wide-camera areas
// (agent-box/areas.json entries that were never committed): the camera is in stills.json.
const STILLS = [
  { slot: "bank-1-front", from: "tmp/look-wp4/wp4-bank-b", hour: 18, note: "the bank facade and vault door from the road, the sign readable" },
  { slot: "bank-2-angle", from: "tmp/look-wp4/wp4-bank-d", hour: 18, note: "the bank from the south-west, the lane and the car lot behind" },
  { slot: "bank-3-day", from: "tmp/look-wp/wp-bank-b", hour: 17, note: "the bank facade in full daylight" },
  { slot: "police-1-wide", from: "tmp/look-wp4/wp4-pol-f", hour: 18, note: "the police villa from the road with the fuel station beside it" },
  { slot: "police-2-uaz", from: "tmp/look-wp/wp-pol-c", hour: 17, note: "the police villa, plate and the two blue UAZ jeeps" },
  { slot: "police-3-side", from: "tmp/look-wp2/wp2-pol-g", hour: 17, note: "the police villa from the north-west, red bench and the street" },
  { slot: "town-1-vista-dusk", from: "tmp/look-wp4/wp4-vista-d", hour: 18, note: "the street from above: car lot, police villa, fuel station, market stalls" },
  { slot: "town-2-vista", from: "tmp/look-wp3/wp3-vista-f", hour: 17, note: "the same view in daylight" },
  { slot: "town-3-street", from: "tmp/look-wp3/wp3-street-c", hour: 17, note: "down the main street from the bank, market stalls on the right" },
];

const mode = process.argv[2] || "both";
const b64 = (f) => `data:${f.endsWith(".png") ? "image/png" : "image/jpeg"};base64,${fs.readFileSync(f).toString("base64")}`;
const url = (p) => pathToFileURL(path.join(pub, p)).href;

const browser = await chromium.launch(process.env.PLAYWRIGHT_CHANNEL ? { channel: process.env.PLAYWRIGHT_CHANNEL } : {});
const page = await browser.newPage({ viewport: { width: 1920, height: 1080 } });

// ---- stills ----------------------------------------------------------------------------------------------------------

// Workbench draws red probe fans and lines over a play frame; a player never sees them. Pure overlay red only
// (the engine's own reds, a roof or the BANK plate, are darker or less saturated), grown by 2 px, then filled from the neighbours.
async function clean(dataUrl) {
  return page.evaluate(async (u) => {
    const img = new Image();
    img.src = u;
    await img.decode();
    const c = document.createElement("canvas");
    c.width = img.width;
    c.height = img.height;
    const g = c.getContext("2d");
    g.drawImage(img, 0, 0);
    const d = g.getImageData(0, 0, c.width, c.height);
    const w = c.width, h = c.height, px = d.data;
    let mask = new Uint8Array(w * h);
    let hits = 0;
    for (let i = 0; i < w * h; i++) {
      const r = px[i * 4], gg = px[i * 4 + 1], b = px[i * 4 + 2];
      if (r >= 200 && gg <= 60 && b <= 60) { mask[i] = 1; hits++; }
    }
    for (let k = 0; k < 2; k++) {
      const next = mask.slice();
      for (let y = 1; y < h - 1; y++) for (let x = 1; x < w - 1; x++) {
        const i = y * w + x;
        if (mask[i - 1] || mask[i + 1] || mask[i - w] || mask[i + w] || mask[i - w - 1] || mask[i - w + 1] || mask[i + w - 1] || mask[i + w + 1]) next[i] = 1;
      }
      mask = next;
    }
    let left = mask.reduce((a, v) => a + v, 0);
    for (let pass = 0; pass < 60 && left > 0; pass++) {
      const fill = [];
      for (let y = 0; y < h; y++) for (let x = 0; x < w; x++) {
        const i = y * w + x;
        if (!mask[i]) continue;
        let n = 0, r = 0, gg = 0, b = 0;
        for (let dy = -1; dy <= 1; dy++) for (let dx = -1; dx <= 1; dx++) {
          const xx = x + dx, yy = y + dy;
          if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
          const j = yy * w + xx;
          if (mask[j]) continue;
          n++; r += px[j * 4]; gg += px[j * 4 + 1]; b += px[j * 4 + 2];
        }
        if (n) fill.push([i, r / n, gg / n, b / n]);
      }
      for (const [i, r, gg, b] of fill) { px[i * 4] = r; px[i * 4 + 1] = gg; px[i * 4 + 2] = b; mask[i] = 0; left--; }
    }
    g.putImageData(d, 0, 0);
    return { url: c.toDataURL("image/jpeg", 0.95), hits };
  }, dataUrl);
}

async function shoot(html, file, w, h, quality) {
  await page.setViewportSize({ width: w, height: h });
  // A file page, not setContent: an about:blank page may not load the file:// fonts and logos.
  const tmp = path.join(out, `.${path.basename(file, ".jpg")}.html`);
  fs.writeFileSync(tmp, html);
  await page.goto(pathToFileURL(tmp).href);
  await page.evaluate(() => document.fonts.ready);
  await page.evaluate(() => Promise.all([...document.images].map((i) => i.decode())));
  await page.screenshot({ path: file, type: "jpeg", quality });
  fs.rmSync(tmp);
  console.log(`${path.relative(site, file).replace(/\\/g, "/")} ${Math.round(fs.statSync(file).size / 1024)} KB`);
}

const stillMeta = [];
if (mode === "stills" || mode === "both") {
  for (const s of STILLS) {
    const dir = path.join(repo, s.from);
    const png = path.join(dir, "wide.png");
    const idx = JSON.parse(fs.readFileSync(path.join(dir, "index.json"), "utf8"));
    const wide = idx.shots.find((x) => x.kind === "wide");
    const cleaned = await clean(b64(png));
    fs.writeFileSync(path.join(srcDir, `${s.slot}.jpg`), Buffer.from(cleaned.url.split(",")[1], "base64"));
    console.log(`${s.slot}: ${cleaned.hits} overlay pixels removed`);
    await shoot(
      `<!doctype html><body style="margin:0"><img src="${cleaned.url}" style="display:block;width:1280px;height:720px;object-fit:cover">`,
      path.join(out, `${s.slot}.jpg`), 1280, 720, 90);
    stillMeta.push({ slot: s.slot, area: idx.area, hour: s.hour, cam: wide.cam, size: "838x447 native, 1280x720 here", note: s.note, overlayPixelsRemoved: cleaned.hits });
  }
  fs.writeFileSync(path.join(out, "stills.json"), JSON.stringify(stillMeta, null, 2) + "\n");
}

// ---- cards -----------------------------------------------------------------------------------------------------------

const slotImg = (slot) => b64(path.join(srcDir, `${slot}.jpg`));
const css = `
@font-face{font-family:B;font-weight:700;src:url(${url("fonts/barlow-condensed-latin-700-normal.woff2")})}
@font-face{font-family:B;font-weight:600;src:url(${url("fonts/barlow-condensed-latin-600-normal.woff2")})}
@font-face{font-family:M;font-weight:500;src:url(${url("fonts/ibm-plex-mono-latin-500-normal.woff2")})}
@font-face{font-family:M;font-weight:400;src:url(${url("fonts/ibm-plex-mono-latin-400-normal.woff2")})}
@font-face{font-family:S;font-weight:600;src:url(${url("fonts/ibm-plex-sans-latin-600-normal.woff2")})}
:root{--paper:#f0ebe1;--card:#fbf8f2;--inset:#e5ddcd;--ink:#1b1d19;--ink2:#54564d;--rule:#c9bfac;--green:#2d4a33;--stamp:#a3302a;--sign:#e3b23c;--night:#121410;--nink:#ebe5d7;--nink2:#aaa696;--band:#0b0e12}
*{margin:0;box-sizing:border-box}
body{width:1920px;height:1080px;overflow:hidden;position:relative;font-family:B;color:var(--ink);background:var(--paper)}
.k{font-family:M;font-weight:500;text-transform:uppercase;letter-spacing:.08em}
.plate{display:inline-block;font-family:B;font-weight:700;text-transform:uppercase;background:var(--sign);color:var(--ink);border:5px solid var(--ink);padding:6px 26px 0;line-height:1.05}
.stamp{display:inline-block;font-family:M;font-weight:500;text-transform:uppercase;letter-spacing:.1em;color:var(--stamp);border:6px solid var(--stamp);outline:3px solid var(--stamp);outline-offset:6px;padding:14px 26px 10px;transform:rotate(-3deg)}
.bar{position:absolute;left:0;right:0;top:0;height:22px;background:var(--sign);border-bottom:5px solid var(--ink)}
.title{font-weight:700;text-transform:uppercase;line-height:.9}
`;

const cards = {
  // The core, night band: emblem, name, one plate. Setting-neutral, no town.
  "preview-core-a": `<style>${css}
body{background:var(--band);color:var(--nink)}
.emblem{position:absolute;left:96px;top:140px;width:800px;height:800px}
.t{position:absolute;left:1000px;top:190px;right:50px}
.t .k{font-size:34px;color:var(--nink2)}
.t .title{font-size:190px;margin-top:26px}
.t .rule{height:5px;background:var(--nink);margin:40px 0 34px;width:640px}
.t .plate{font-size:96px}
.t .s{font-family:M;font-weight:500;font-size:34px;letter-spacing:.08em;text-transform:uppercase;color:var(--nink2);margin-top:34px;line-height:1.5}
</style><body>
<img class="emblem" src="${url("brand/logo.svg")}">
<div class="t"><div class="k">Open-source framework</div><div class="title">Life<br>Framework</div><div class="rule"></div>
<span class="plate">Alpha 0.1.0</span><div class="s">Run your own roleplay server<br>for Arma Reforger</div></div></body>`,

  // The core, paper sheet with the host's server config: the run-your-own angle, real IDs.
  "preview-core-b": `<style>${css}
.l{position:absolute;left:84px;top:96px;width:900px}
.l .k{font-size:32px;color:var(--ink2)}
.l .title{font-size:200px;margin-top:20px}
.l .lede{font-family:S;font-weight:600;font-size:50px;line-height:1.25;color:var(--ink2);margin-top:44px;max-width:780px}
.l .plate{font-size:84px;margin-top:56px}
.term{position:absolute;left:1040px;top:226px;width:790px;background:#1b1d19;color:var(--nink);border:3px solid var(--ink);box-shadow:14px 14px 0 var(--ink);font-family:M;font-weight:400;font-size:29px;line-height:1.55}
.term .h{background:#2a2d26;border-bottom:3px solid var(--ink);padding:14px 26px 10px;font-weight:500;font-size:26px;letter-spacing:.08em;text-transform:uppercase;color:var(--nink2)}
.term pre{font:inherit;padding:30px 30px 36px;white-space:pre}
.q{color:#a3c596}.n{color:var(--sign)}
.st{position:absolute;left:1380px;top:830px}
.st .stamp{font-size:64px}
</style><body><div class="bar"></div>
<div class="l"><div class="k">Core · needs a pack to play</div><div class="title">Life<br>Framework</div>
<div class="lede">Run your own roleplay server.</div><span class="plate">Version 0.1.0</span></div>
<div class="term"><div class="h">server config.json</div><pre><span class="q">"mods"</span>: [
  {
    <span class="q">"modId"</span>: <span class="n">"79636E668EA37AC9"</span>,
    <span class="q">"name"</span>: "Life Framework (Core)"
  },
  {
    <span class="q">"modId"</span>: <span class="n">"B1E84D5687E11724"</span>,
    <span class="q">"name"</span>: "Life Framework: Everon 1989"
  }
]</pre></div><div class="st"><span class="stamp">Alpha</span></div></body>`,

  // The pack, a photo print on paper beside the title.
  "preview-pack-a": `<style>${css}
.print{position:absolute;left:70px;top:112px;width:1120px;height:630px;border:6px solid var(--ink);box-shadow:14px 14px 0 var(--ink);background:var(--inset)}
.print img{display:block;width:100%;height:100%;object-fit:cover}
.cap{position:absolute;left:76px;top:796px;font-size:28px;color:var(--ink2)}
.r{position:absolute;left:1260px;top:112px;right:50px}
.r .k{font-size:30px;color:var(--ink2)}
.r .title{font-size:204px;margin-top:22px}
.r .plate{font-size:72px;margin-top:40px}
.r .stamp{font-size:46px;margin-top:60px}
.foot{position:absolute;left:76px;right:70px;bottom:62px;border-top:5px solid var(--ink);padding-top:20px;font-size:30px;color:var(--ink2);display:flex;justify-content:space-between}
</style><body><div class="bar"></div>
<div class="print"><img src="${slotImg("town-1-vista-dusk")}"></div>
<div class="cap k">Everon, 1989. Captured in the game</div>
<div class="r"><div class="k">Life Framework</div><div class="title">Everon<br>1989</div><span class="plate">Version 0.1.0</span><br><span class="stamp">Alpha</span></div>
<div class="foot k"><span>Flagship pack</span><span>Needs Life Framework (Core)</span></div></body>`,

  // The pack, the frame full bleed with one paper sheet on it.
  "preview-pack-b": `<style>${css}
body{background:#222}
.bg{position:absolute;inset:0;width:1920px;height:1080px;object-fit:cover}
.sheet{position:absolute;right:70px;bottom:70px;width:900px;background:var(--paper);border:5px solid var(--ink);box-shadow:14px 14px 0 var(--ink);padding:38px 50px 34px}
.sheet .k{font-size:30px;color:var(--ink2)}
.sheet .title{font-size:168px;margin-top:10px;line-height:.86;white-space:nowrap}
.sheet .row{display:flex;align-items:center;justify-content:space-between;margin-top:26px;border-top:5px solid var(--ink);padding-top:26px}
.sheet .plate{font-size:64px}
.sheet .stamp{font-size:34px;padding:10px 20px 6px;border-width:5px}
</style><body>
<img class="bg" src="${slotImg("bank-2-angle")}">
<div class="sheet"><div class="k">Life Framework</div><div class="title">Everon 1989</div>
<div class="row"><span class="plate">Version 0.1.0</span><span class="stamp">Alpha</span></div></div></body>`,
};

if (mode === "cards" || mode === "both") {
  for (const [name, html] of Object.entries(cards)) {
    await shoot(`<!doctype html><html><head><meta charset="utf-8"></head>${html}</html>`, path.join(out, `${name}.jpg`), 1920, 1080, 88);
  }
}
await browser.close();
