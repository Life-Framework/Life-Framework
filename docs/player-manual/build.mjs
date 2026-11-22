// docs/player-manual/build.mjs - rebuilds the player manual from the game's data.
//
//   node docs/player-manual/build.mjs            legends, shop tables and manual.html; maps stay as they are on disk
//   node docs/player-manual/build.mjs --maps     also re-renders every map PNG (offline; reads the cached terrain)
//
// Neither mode starts the game, Workbench or a session.
import { mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { basename, join } from "node:path";
import { parseConf, findBlocks, str, num } from "../../tools/map/lib/conf.mjs";
import { loadStrings } from "../../tools/map/lib/overlay.mjs";
import { buildMaps, IMG_DIR } from "./build/maps.mjs";
import { ROOT } from "./build/places.mjs";
import { GROUPS } from "./build/places.mjs";

const DIR = join(ROOT, "docs/player-manual");
const IMG_REL = "../../website/public/docs-images/player-manual";
const PAGES = ["README.md", "01-getting-started.md", "02-money.md", "03-jobs.md", "04-homes.md", "05-shops.md", "06-police.md", "07-crime.md", "08-medic.md", "09-controls.md"];
const render = process.argv.includes("--maps");

const data = await buildMaps({ render, log: (m) => console.log(m) });

// ---------------------------------------------------------------- legends
const km = (x, z) => Math.hypot(x - 4440, z - 10700) / 1000;
const legendTable = (rows, cols) => ["| " + cols.map((c) => c[0]).join(" | ") + " |", "|" + cols.map(() => "---").join("|") + "|", ...rows.map((r) => "| " + cols.map((c) => c[1](r)).join(" | ") + " |")].join("\n");
const byN = (rows) => [...rows].sort((a, b) => a.n - b.n);
const img = (key, alt) => `![${alt}](${IMG_REL}/${key}.png)`;

function mapBlock(key) {
  if (key === "town") {
    return `${img("saint-philippe", "Saint-Philippe: every place")}\n\nCloser in, the centre of town:\n\n${img("saint-philippe-centre", "Saint-Philippe centre")}\n\n` +
      legendTable(byN(data.town), [["#", (r) => r.n], ["Place (in-game name)", (r) => r.name], ["Kind", (r) => GROUPS[r.group].name], ["What you do here", (r) => r.what]]);
  }
  if (key === "island") {
    return `${img("island", "Everon island")}\n\n` +
      legendTable(byN(data.island), [["#", (r) => r.n], ["Place (in-game name)", (r) => r.name], ["Distance from Saint-Philippe", (r) => (r.name === "Saint-Philippe" ? "-" : `${km(r.x, r.z).toFixed(1)} km`)], ["What is there", (r) => r.what]]);
  }
  if (key === "homes") {
    const homes = data.homes.filter((h) => h.id.includes(".home."));
    const plots = data.homes.filter((h) => !h.id.includes(".home."));
    return `${img("homes", "Homes and plots for sale")}\n\nThe label on the map is the home's code and its alpha price in thousands. Prices are the alpha price (${data.propertyPercent} percent of the list).\n\n` +
      legendTable(homes, [["#", (r) => r.n], ["Home", (r) => r.code], ["Tier", (r) => r.tier], ["List price", (r) => "$" + r.listed.toLocaleString("en-US")], ["Alpha price", (r) => "**$" + r.price.toLocaleString("en-US") + "**"]]) +
      `\n\n` + legendTable(plots, [["#", (r) => r.n], ["Plot", (r) => r.name], ["List price", (r) => "$" + r.listed.toLocaleString("en-US")], ["Alpha price", (r) => "**$" + r.price.toLocaleString("en-US") + "**"]]);
  }
  if (key === "crime") {
    return `${img("crime-town", "Crime and police in Saint-Philippe")}\n\n` + legendTable(byN(data.crime), [["#", (r) => r.n], ["Place", (r) => r.name], ["Why you care", (r) => r.what]]);
  }
  if (key.startsWith("inset:")) {
    const k = key.slice(6);
    const ins = data.insets[k];
    if (!ins) throw new Error(`manual: no inset ${k}`);
    return `${img(k, ins.title)}\n\n` + legendTable(byN(ins.rows), [["#", (r) => r.n], ["Place", (r) => r.name], ["What", (r) => r.what]]);
  }
  throw new Error(`manual: unknown map block ${key}`);
}

// ---------------------------------------------------------------- shop tables
const text = loadStrings(ROOT);
const walk = (d, o = []) => { for (const f of readdirSync(d)) { const p = join(d, f); statSync(p).isDirectory() ? walk(p, o) : p.endsWith(".conf") && o.push(p); } return o; };
const NICE = {
  ETool_ALICE: "Shovel", Flashlight_Soviet_01: "Flashlight", Gauze_01: "Gauze", Gauze_02: "Surgical gauze", Tourniquet_US_01: "Tourniquet",
  Tourniquet_USSR_01: "Tourniquet (Soviet)", AspirinBottle: "Aspirin", HandsetRadio: "Handset radio", TouristMap: "Tourist map", CabbageSeeds: "Cabbage seeds",
  BaitTin: "Bait tin", FishingRod: "Fishing rod", DoorKit: "Door kit", WoodPlank: "Plank", CementBag: "Cement bag", GravelBag: "Gravel bag", Sandbag: "Sandbag",
  RepairKit: "Repair kit", CarParts: "Car parts", CoffeeTin: "Coffee tin", PickleJar: "Pickle jar", SodaBottle: "Soda", MilkBottle: "Milk", WaterBottle: "Water",
  AppleJuice: "Apple juice", BeerBottle: "Beer", DriedMeat: "Dried meat", CannedMeat: "Canned meat", CannedFish: "Canned fish", ChocolateBar: "Chocolate bar",
  Lockpick: "Lockpick", Scanner: "Police scanner",
};
const pretty = (p) => {
  const b = basename(String(p).replace(/^\{[0-9A-F]+\}/, "")).replace(/\.et$/, "").replace(/^LF_/, "");
  if (NICE[b]) return NICE[b];
  const m = b.match(/^Furniture_(.+)_Item$/);
  const base = m ? m[1] : b;
  return base.replace(/([a-z])([A-Z])/g, "$1 $2").replace(/_/g, " ");
};
const stocks = {};
for (const f of [...walk(join(ROOT, "addons/LifeFramework/Configs")), ...walk(join(ROOT, "addons/LF_Everon1989/Configs"))]) {
  const t = readFileSync(f, "utf8");
  if (!t.includes("LF_ShopStock ")) continue;
  for (const s of findBlocks(parseConf(t), "LF_ShopStock")) {
    stocks[str(s.props.m_sId)] = findBlocks(s, "LF_ShopStockItem").map((i) => ({
      name: text(str(i.props.m_sDisplayName).replace(/^#/, "")) || pretty(i.props.m_Prefab), price: num(i.props.m_iPrice), crimes: num(i.props.m_iMinCrimes),
    }));
  }
}
const SHOPS = [
  ["supermarket", "Supermarket"], ["general_store", "General store"], ["bakery", "Bakery"], ["butcher", "Butcher"], ["cafe", "Cafe"],
  ["newsagent", "Newsagent"], ["pharmacy", "Pharmacy"], ["hardware", "Hardware store"], ["seed_feed", "Seed and feed store"], ["furniture", "Furniture store"],
  ["fuel_kiosk", "Filling station"], ["mechanic", "Garage"],
];
function shopsBlock() {
  const rows = SHOPS.map(([id, place]) => {
    const t = data.town.find((p) => p.name === place);
    if (!stocks[id]) throw new Error(`manual: no stock ${id}`);
    return `| ${t ? t.n : ""} | **${place}** | ${stocks[id].map((i) => `${i.name} $${i.price}`).join(", ")} |`;
  });
  return "| # | Shop | What it sells |\n|---|---|---|\n" + rows.join("\n");
}

// ---------------------------------------------------------------- rewrite the generated blocks in the markdown
const files = [...PAGES, "maps.md"];
const sources = {};
for (const f of files) {
  let s = readFileSync(join(DIR, f), "utf8");
  s = s.replace(/<!-- map:([\w:-]+) -->[\s\S]*?<!-- \/map -->/g, (_, key) => `<!-- map:${key} -->\n${mapBlock(key)}\n<!-- /map -->`);
  s = s.replace(/<!-- shops -->[\s\S]*?<!-- \/shops -->/g, () => `<!-- shops -->\n${shopsBlock()}\n<!-- /shops -->`);
  writeFileSync(join(DIR, f), s);
  sources[f] = s;
}

// ---------------------------------------------------------------- manual.html
const esc = (s) => s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
const slug = (s) => s.toLowerCase().replace(/[^a-z0-9 -]/g, "").trim().replace(/ +/g, "-");
const pageId = (f) => "p-" + f.replace(/\.md$/, "");
const dataUri = (rel) => `data:image/png;base64,${readFileSync(join(IMG_DIR, basename(rel))).toString("base64")}`;
const mapAnchors = { "maps.md": "#map-town", "maps.md#crime-and-police-saint-philippe": "#map-crime" };

function inline(s) {
  s = esc(s);
  s = s.replace(/!\[([^\]]*)\]\(([^)]+)\)/g, (_, alt, src) => `<img alt="${alt}" src="${dataUri(src)}">`);
  s = s.replace(/\[([^\]]+)\]\(([^)]+)\)/g, (_, label, href) => {
    if (mapAnchors[href]) return `<a href="${mapAnchors[href]}">${label}</a>`;
    if (/^\.\.\//.test(href) || href === "manual.html") return label;
    const m = href.match(/^(\d\d-[\w-]+|README)\.md$/);
    if (m) return `<a href="#${pageId(m[0])}">${label}</a>`;
    // A section on another page: the manual is one HTML file, so the heading's own id is the target.
    const sec = href.match(/^(?:\d\d-[\w-]+|README|maps)\.md#([\w-]+)$/);
    if (sec) return `<a href="#${sec[1]}">${label}</a>`;
    return `<a href="${href}">${label}</a>`;
  });
  s = s.replace(/`([^`]+)`/g, "<code>$1</code>").replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>").replace(/(^|[^*\w])\*([^*\s][^*]*)\*/g, "$1<em>$2</em>");
  return s;
}

function mdToHtml(md, file, skipMaps) {
  const lines = md.replace(/<!-- shops -->|<!-- \/shops -->/g, "").split("\n");
  const out = [];
  const used = new Set();
  let i = 0;
  while (i < lines.length) {
    const line = lines[i];
    const mapOpen = line.match(/^<!-- map:([\w:-]+) -->$/);
    if (mapOpen) {
      let j = i + 1;
      while (!/^<!-- \/map -->$/.test(lines[j])) j++;
      const key = mapOpen[1];
      const inner = mdToHtml(lines.slice(i + 1, j).join("\n"), file, skipMaps);
      out.push(`<div class="map" id="map-${key.replace(":", "-")}">${inner}</div>`);
      i = j + 1;
      continue;
    }
    if (/^\s*$/.test(line)) { i++; continue; }
    if (/^---+$/.test(line)) { out.push("<hr>"); i++; continue; }
    const h = line.match(/^(#{1,3}) (.*)$/);
    if (h) {
      let id = slug(h[2]);
      if (used.has(id)) id += "-" + file.replace(/\W/g, "");
      used.add(id);
      const lvl = h[1].length;
      out.push(`<h${lvl}${lvl === 1 ? ` id="${pageId(file)}"` : ` id="${id}"`}>${inline(h[2])}</h${lvl}>`);
      i++;
      continue;
    }
    if (line.startsWith("```")) {
      let j = i + 1;
      const buf = [];
      while (!lines[j].startsWith("```")) buf.push(lines[j++]);
      out.push(`<pre>${esc(buf.join("\n"))}</pre>`);
      i = j + 1;
      continue;
    }
    if (line.startsWith("|")) {
      const rows = [];
      while (i < lines.length && lines[i].startsWith("|")) rows.push(lines[i++]);
      const cells = (r) => r.replace(/^\||\|$/g, "").split(/(?<!\\)\|/).map((c) => c.trim());
      const head = cells(rows[0]);
      const body = rows.slice(2).map(cells);
      out.push(`<div class="tw"><table><thead><tr>${head.map((c) => `<th>${inline(c)}</th>`).join("")}</tr></thead><tbody>${body.map((r) => `<tr>${r.map((c) => `<td>${inline(c)}</td>`).join("")}</tr>`).join("")}</tbody></table></div>`);
      continue;
    }
    const li = line.match(/^(\*|-|\d+\.) (.*)$/);
    if (li) {
      const ordered = /\d/.test(li[1]);
      const items = [];
      while (i < lines.length && /^(\*|-|\d+\.) /.test(lines[i])) {
        let t = lines[i].replace(/^(\*|-|\d+\.) /, "");
        i++;
        while (i < lines.length && /^ {2,}\S/.test(lines[i]) && !/^\s*(\*|-|\d+\.) /.test(lines[i])) t += " " + lines[i++].trim();
        items.push(t);
        while (i < lines.length && /^ {2,}(\*|-|\d+\.) /.test(lines[i])) i++;
      }
      out.push(`<${ordered ? "ol" : "ul"}>${items.map((t) => `<li>${inline(t)}</li>`).join("")}</${ordered ? "ol" : "ul"}>`);
      continue;
    }
    const para = [];
    while (i < lines.length && !/^\s*$/.test(lines[i]) && !/^(#{1,3} |\||```|---+$|(\*|-|\d+\.) |<!-- )/.test(lines[i])) para.push(lines[i++]);
    if (!para.length) { i++; continue; }
    out.push(`<p>${inline(para.join(" "))}</p>`);
  }
  return out.join("\n");
}

const CSS = `
:root{color-scheme:light;--ink:#2b2418;--paper:#fbf7ea;--rule:#d9ceb0;--accent:#8a4b1f;--soft:#f1ead2}
*{box-sizing:border-box}
html{-webkit-text-size-adjust:100%}
body{margin:0;background:var(--paper);color:var(--ink);font:16px/1.55 system-ui,-apple-system,"Segoe UI",Roboto,sans-serif}
main{max-width:46rem;margin:0 auto;padding:1rem 16px 4rem}
h1{font-size:1.7rem;line-height:1.2;margin:2.4rem 0 .6rem;padding-top:.4rem;border-top:4px solid var(--accent)}
h2{font-size:1.25rem;margin:1.8rem 0 .4rem;color:var(--accent)}
h3{font-size:1.05rem;margin:1.2rem 0 .3rem}
p,ul,ol{margin:.5rem 0}
li{margin:.2rem 0}
a{color:#1b4f8a}
code,pre{font-family:ui-monospace,Consolas,monospace;font-size:.9em;background:var(--soft);border-radius:3px}
code{padding:0 .25em}
pre{padding:.6rem;overflow-x:auto}
hr{border:0;border-top:1px solid var(--rule);margin:1.4rem 0}
.tw{overflow-x:auto;margin:.7rem 0}
table{border-collapse:collapse;width:100%;font-size:.92rem}
th,td{border:1px solid var(--rule);padding:.3rem .5rem;text-align:left;vertical-align:top}
th{background:var(--soft)}
tr{break-inside:avoid}
.map{margin:1rem 0}
.map img{display:block;max-width:100%;height:auto;border:1px solid var(--rule);margin:.4rem 0}
.cover{padding:1.2rem 0 .4rem}
.cover .tag{font-size:.85rem;letter-spacing:.08em;text-transform:uppercase;color:var(--accent)}
.cover h1{border:0;margin:.2rem 0;font-size:2rem;padding:0}
nav.toc{background:var(--soft);border:1px solid var(--rule);border-radius:6px;padding:.4rem 1rem;margin:1rem 0}
nav.toc ol{padding-left:1.2rem}
@media print{
  @page{size:A4;margin:13mm}
  body{background:#fff;font-size:10.5pt;line-height:1.4}
  main{max-width:none;padding:0}
  h1{break-before:page;margin-top:0}
  .cover h1,.cover+*{break-before:auto}
  h2,h3{break-after:avoid}
  .map,.map img{break-inside:avoid}
  .map img{max-height:235mm;width:auto;max-width:100%}
  nav.toc{break-after:page}
  a{color:inherit;text-decoration:none}
}`;

const names = { "README.md": "Start here", "01-getting-started.md": "1. Getting started", "02-money.md": "2. Money", "03-jobs.md": "3. Jobs", "04-homes.md": "4. Homes", "05-shops.md": "5. Shops", "06-police.md": "6. Police", "07-crime.md": "7. Crime", "08-medic.md": "8. Medic and clinic", "09-controls.md": "9. Controls" };
const forHtml = (f) => (f === "README.md" ? sources[f].slice(0, sources[f].indexOf("## Rebuilding this manual")) : sources[f]);
const sections = PAGES.map((f) => `<section>${mdToHtml(forHtml(f), f)}</section>`).join("\n");
const toc = `<nav class="toc"><strong>Contents</strong><ol>${PAGES.map((f) => `<li><a href="#${pageId(f)}">${names[f]}</a></li>`).join("")}</ol></nav>`;
const html = `<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Everon 1989 Player Manual</title>
<style>${CSS}</style></head>
<body><main>
<div class="cover"><div class="tag">Life Framework alpha 0.2.0</div><h1 style="border:0">Everon 1989: player manual</h1><p>Maps, jobs, money, police and crime for Saint-Philippe and the island. Open it on a phone or print it.</p></div>
${toc}
${sections}
</main></body></html>
`;
// Every link must work inside the one page: an in-page #anchor with a matching id, or an absolute URL.
// A leftover relative link (`03-jobs.md#fishing`) fails the website's route check after deploy, so stop here.
const ids = new Set([...html.matchAll(/\sid="([^"]+)"/g)].map((x) => x[1]));
const broken = [];
for (const [, href] of html.matchAll(/<a href="([^"]*)"/g)) {
  if (href.startsWith("#")) { if (!ids.has(href.slice(1))) broken.push(`${href} (no heading or block has this id)`); }
  else if (!/^(https?:|mailto:)/.test(href)) broken.push(`${href} (relative link; the manual is one page)`);
}
if (broken.length) {
  console.error(`manual FAIL: ${broken.length} broken link(s):\n  ${[...new Set(broken)].join("\n  ")}`);
  process.exit(1);
}
writeFileSync(join(DIR, "manual.html"), html);
// The same file is the public page life-framework.org/manual/ (website/public is served as is).
const SITE_DIR = join(ROOT, "website/public/manual");
mkdirSync(SITE_DIR, { recursive: true });
writeFileSync(join(SITE_DIR, "index.html"), html);
console.log(`manual ok: ${PAGES.length} pages, manual.html ${Math.round(html.length / 1024)} KB`);
