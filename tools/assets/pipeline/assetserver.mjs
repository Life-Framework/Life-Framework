// 3d-asset-server (github.com/arielshad/3d-asset-server, Apache-2.0) as the search and free-download rung of the
// source ladder. It gives one result shape (licence, free or paid, formats, polys, preview) for many sites; this
// file installs it, maps its results to our candidate shape, applies our licence gate, scores against the spec and
// writes the shortlist. Playbook: docs/asset-pipeline/sources.md "3d-asset-server".
//
// It is third-party code on this PC, so: pinned to one commit, built with `npm ci --ignore-scripts` into the
// gitignored tools/mcp/3d-asset-server/, and used as a library (dist/index.js), not through its cli.js, because
// cli.js starts usage telemetry to stats.shep.bot. The MCP route (docs) sets ASSET_SERVER_TELEMETRY=0.
//
// We never buy, log in or enter payment details. A paid or blocked result becomes a shortlist entry with its link;
// the user buys it and drops the download in tmp/assets/<slug>/purchased/ (source.provider "purchased").
import { existsSync, mkdirSync, readFileSync, readdirSync, statSync, writeFileSync } from "node:fs";
import { join, relative } from "node:path";
import { pathToFileURL } from "node:url";
import { spawnSync } from "node:child_process";

export const REPO = "https://github.com/arielshad/3d-asset-server.git";
export const PIN = "454fa3931748e14130041c2048a542a5090cd83f"; // main, 2026-11-14; bump it by reading the diff first
export const DIR_NAME = "3d-asset-server";

// Providers we never query through it: CGTrader's robots.txt says `Disallow: /search*` for every agent and the
// provider reads that path, so CGTrader (and any link-only market) gets a ready-made search URL for the human.
// Poly Haven stays on our own adapter (source.mjs): it checks the API's md5s and splits the packed arm map.
export const SKIP_PROVIDERS = new Set(["cgtrader", "polyhaven"]);
// "scrape" providers parse HTML pages; their robots.txt was not audited, so they are opt-in (--scrape).
export const DEFAULT_ACCESS = new Set(["api", "link"]);

export const MODEL_FORMATS = ["glb", "gltf", "fbx", "obj", "blend"]; // what the clean stage can start from
export const cgtraderSearchUrl = (q) => `https://www.cgtrader.com/3d-models?keywords=${encodeURIComponent(q)}&license=royalty_free`;

export function serverDir(root) {
  const own = join(root, "tools", "mcp", DIR_NAME);
  if (existsSync(join(own, "dist", "index.js"))) return own;
  // A worktree shares the main checkout's install until it has its own (tools/cli.mjs mcpDirFor does the same).
  const out = spawnSync("git", ["rev-parse", "--git-common-dir"], { cwd: root, encoding: "utf8", windowsHide: true });
  const common = out.status === 0 ? join(root, String(out.stdout).trim(), "..") : null;
  const shared = common ? join(common, "tools", "mcp", DIR_NAME) : null;
  return shared && existsSync(join(shared, "dist", "index.js")) ? shared : own;
}
export const installed = (root) => existsSync(join(serverDir(root), "dist", "index.js"));

function run(cmd, args, cwd) {
  const r = spawnSync(cmd, args, { cwd, encoding: "utf8", windowsHide: true, shell: process.platform === "win32" && /^(npm|npx)$/.test(cmd), timeout: 600000 });
  if (r.status !== 0) throw new Error(`${cmd} ${args.join(" ")} failed (${r.status}): ${String(r.stderr || r.stdout).slice(-600)}`);
  return String(r.stdout);
}

// Clone, check out the pinned commit, install without running any package script, compile the server only
// (the web site build is skipped: `tsc -p tsconfig.build.json`). Idempotent.
export function install(root, log = console.log) {
  const dir = join(root, "tools", "mcp", DIR_NAME);
  mkdirSync(join(root, "tools", "mcp"), { recursive: true });
  if (!existsSync(join(dir, ".git"))) run("git", ["clone", "--quiet", REPO, dir], root);
  else run("git", ["fetch", "--quiet", "origin"], dir);
  run("git", ["checkout", "--quiet", PIN], dir);
  if (!existsSync(join(dir, "node_modules"))) run("npm", ["ci", "--ignore-scripts", "--no-audit", "--no-fund"], dir);
  run("npx", ["--no-install", "tsc", "-p", "tsconfig.build.json"], dir);
  if (!existsSync(join(dir, "dist", "index.js"))) throw new Error("build wrote no dist/index.js");
  log(`3d-asset-server ${PIN.slice(0, 7)} installed in ${relative(root, dir)} (not committed; telemetry is off because we use the library, not cli.js)`);
  return dir;
}

export async function loadLib(root) {
  if (!installed(root)) throw new Error("3d-asset-server is not installed: run tools\\cli asset-search install");
  return import(pathToFileURL(join(serverDir(root), "dist", "index.js")).href);
}

// ---------------------------------------------------------------- the licence gate
// ok      free to ship: CC0, public domain, and CC-BY once its credit row is in docs/credits.md
// credit  CC-BY: allowed, the author and URL go in docs/credits.md before it ships
// paid    CGTrader "Royalty Free": allowed after the human buys it, with the extraction caveat below
// review  CC-BY-SA, no-redistribution or a custom licence: a human reads the terms first
// refuse  Editorial, non-commercial, no-derivatives, unknown: never a candidate
//
// CGTrader terms (cgtrader.com/pages/terms-and-conditions, read 2026-11-14) limit Royalty Free use to "Incorporated
// Product": "Product that cannot be extracted from an application or other product". Section 21A.3: "you must take all
// commercially reasonable measures to prevent the end user from gaining access to the Product". A compiled .xob in a
// published .pbo can be unpacked by players, so a CGTrader model carries that risk and the shortlist says so.
export const PAID_CAVEAT = "CGTrader Royalty Free covers 'Incorporated Product' that players cannot extract (terms 21A.2, 21A.3); a published mod's .xob can be unpacked, so the human accepts that risk (or asks the seller) before buying";

export function licenceGate(license, { price, provider } = {}) {
  const name = String(license?.name || "").trim();
  const l = name.toLowerCase().replace(/[\s_]+/g, "-");
  const paid = price && price.free === false;
  const out = (verdict, why, extra = {}) => ({ verdict, licence: name || "unknown", why, ...extra });
  if (!l) return out("refuse", "no licence stated");
  if (/editorial/.test(l)) return out("refuse", "Editorial use only: not allowed in a game");
  if (/(^|-)nc(-|$)|non-?commercial/.test(l) || license?.commercialUse === false) return out("refuse", "non-commercial");
  if (/(^|-)nd(-|$)|no-?deriv/.test(l)) return out("refuse", "no derivatives");
  if (/(^|-)sa(-|$)|share-?alike/.test(l)) return out("review", "share-alike would bind the mod's own licence");
  if (/cc0|public-domain|^pd$|^zero$|cc0-1\.0/.test(l)) return out("ok", "CC0", { credit: false });
  if (/^(cc-?)?by(-[0-9.]+)?$/.test(l) || /^attribution$/.test(l)) return out("credit", "CC-BY: credit the author", { credit: true });
  if (/royalty-?free/.test(l)) {
    if (/no-?ai/.test(l) || provider === "cgtrader" || paid) return out("paid", "Royalty Free: human buys it", { credit: false, caveat: PAID_CAVEAT });
    return out("review", "Royalty Free from a source whose terms we have not read", { credit: false });
  }
  if (/no-?redistribution|all-rights-reserved|standard|custom|proprietary|unknown|other/.test(l)) return out("review", "terms need a human read (redistribution inside a mod unclear)");
  return out("review", "unrecognised licence");
}

// ---------------------------------------------------------------- results to our candidate shape
export function mapAsset(a, linkOnly = false) {
  const gate = licenceGate(a.license, { price: a.price, provider: a.provider });
  const free = a.price ? a.price.free === true : null;
  return {
    source: "asset-server", provider: a.provider, id: a.id, name: a.title, url: a.url, thumb: a.thumbnailUrl,
    licence: a.license?.name || "unknown", licenceUrl: a.license?.url, author: a.author, credit: !!gate.credit, gate,
    free, price: a.price && !a.price.free ? `${a.price.amount ?? "?"} ${a.price.currency || ""}`.trim() : free ? "free" : "?",
    formats: a.formats || [], tags: a.tags || [], tris: a.polyCount, pbr: (a.tags || []).includes("pbr"), rigged: !!a.rigged, animated: !!a.animated,
    direct: !!a.downloadable && free === true, relevance: a.score, linkOnly,
  };
}

// A search link a human opens in a browser, for a market we do not query (or that blocks us).
export function linkEntry(provider, name, url) {
  return { source: "asset-server", provider, id: `${provider}:search`, name: `${name} search page`, url, licence: "see the listing", gate: { verdict: "link", why: "opened by the human" }, linkOnly: true, free: null, price: "?", formats: [] };
}

// ---------------------------------------------------------------- scoring against the spec
// 0..100: how well a candidate fits the spec, in the open so the shortlist shows why. Candidates the gate refuses
// score 0 and never reach the shortlist.
export const STOPWORDS = new Set("a an the of for and with in on at to from by or is are as it its this that 1989 old new small large big one two".split(" "));
export const words = (t) => [...new Set(String(t || "").replace(/([a-z])([A-Z])/g, "$1 $2").toLowerCase().split(/[^a-z0-9]+/).filter((w) => w.length >= 3 && !STOPWORDS.has(w)))];

export function scoreCandidate(c, spec = {}) {
  if (c.gate.verdict === "refuse") return { score: 0, why: [`refused: ${c.gate.why}`] };
  const why = [];
  let score = 0;
  const want = words([spec.name, spec.search].filter(Boolean).join(" ") || "");
  const have = words(`${c.name} ${(c.tags || []).join(" ")}`);
  const hit = want.filter((w) => have.includes(w));
  const wordPts = want.length ? Math.round((40 * hit.length) / want.length) : 20;
  score += wordPts; why.push(`words ${hit.length}/${want.length || "?"} (+${wordPts})`);
  const fmt = (c.formats || []).filter((f) => MODEL_FORMATS.includes(f));
  if (fmt.length) { score += 20; why.push(`importable ${fmt.join("/")} (+20)`); }
  else if (c.formats?.length) why.push(`no importable format (${c.formats.join("/")})`);
  else why.push("formats unknown");
  if (c.pbr) { score += 10; why.push("PBR maps (+10)"); }
  const budget = spec.budget?.lod0_tris;
  if (c.tris && budget) {
    if (c.tris <= budget) { score += 15; why.push(`${c.tris} tris within ${budget} (+15)`); }
    else if (c.tris <= budget * 4) { score += 7; why.push(`${c.tris} tris, ${(c.tris / budget).toFixed(1)}x the budget (+7)`); }
    else why.push(`${c.tris} tris, over 4x the budget`);
  } else if (c.tris) { score += 8; why.push(`${c.tris} tris`); }
  const g = { ok: 15, credit: 12, paid: 8, review: 0, link: 0 }[c.gate.verdict] ?? 0;
  score += g; why.push(`licence ${c.gate.verdict} (+${g})`);
  if (c.gate.verdict === "review") score = Math.min(score, 50);
  if (c.animated || c.rigged) { score -= 5; why.push("rigged or animated (-5): the clean stage drops rigs"); }
  return { score: Math.max(0, Math.min(100, score)), why };
}

export function rank(candidates, spec) {
  return candidates.map((c) => ({ ...c, ...scoreCandidate(c, spec) })).filter((c) => c.score > 0).sort((a, b) => b.score - a.score || a.name.localeCompare(b.name));
}

// ---------------------------------------------------------------- search
// Returns { candidates, links, reports }. `service` is injectable for tests.
export async function search(root, { query, limit = 12, scrape = false, types = ["model"], freeOnly = false, service } = {}) {
  let svc = service;
  if (!svc) {
    const lib = await loadLib(root);
    const providers = lib.allProviders.filter((p) => !SKIP_PROVIDERS.has(p.id) && (scrape || DEFAULT_ACCESS.has(p.access)));
    svc = new lib.AssetService({ providers });
  }
  const res = await svc.search({ query, types, limit, freeOnly });
  const candidates = res.results.map((a) => mapAsset(a));
  const links = res.providers.filter((p) => p.status === "link" && p.searchUrl).map((p) => linkEntry(p.provider, p.name, p.searchUrl));
  links.unshift(linkEntry("cgtrader", "CGTrader", cgtraderSearchUrl(query)));
  return { candidates, links, reports: res.providers.map((p) => ({ provider: p.provider, status: p.status, count: p.count, error: p.error })) };
}

// ---------------------------------------------------------------- the shortlist file
export function shortlistMarkdown(slug, spec, ranked, links, { query } = {}) {
  const rows = ranked.map((c, i) => {
    const specs = [c.formats?.length ? c.formats.join("/") : "formats ?", c.tris ? `${c.tris} tris` : "tris ?", c.pbr ? "PBR" : ""].filter(Boolean).join(", ");
    return `| ${i + 1} | [${c.name.replace(/\|/g, "/")}](${c.url}) | ${c.provider} | ${c.price} | ${c.licence} (${c.gate.verdict}) | ${specs} | ${c.score} | ${c.thumb ? `![](${c.thumb})` : ""} |`;
  });
  const lines = [
    `# Shortlist for ${slug} (3d-asset-server)`,
    "",
    `Query: \`${query}\`. Spec: ${spec.name || slug}${spec.size_m ? `, ${spec.size_m.length} m long` : ""}${spec.budget?.lod0_tris ? `, ${spec.budget.lod0_tris} triangle budget` : ""}.`,
    "The pipeline stops here. It never buys, logs in or enters payment details.",
    "",
    "| # | Model | Source | Price | Licence (gate) | Formats | Fit | Preview |",
    "|---|---|---|---|---|---|---|---|",
    ...(rows.length ? rows : ["| | none passed the licence gate | | | | | | |"]),
    "",
  ];
  for (const c of ranked) lines.push(`- **${c.name}** fit ${c.score}: ${c.why.join("; ")}${c.gate.caveat ? `\n  - ${c.gate.caveat}` : ""}${c.gate.verdict === "review" ? `\n  - needs a human look: ${c.gate.why}` : ""}`);
  lines.push("", "## Markets we do not search for you", "");
  for (const l of links) lines.push(`- ${l.provider}: ${l.url}`);
  lines.push(
    "",
    "## Next",
    "",
    "- Free and direct (CC0, CC-BY): `tools\\cli asset source pick <slug> <n>` writes `spec.source`; `asset run <slug>` downloads it into `tmp/assets/<slug>/download/` and carries on (clean, import, states).",
    "- Paid (CGTrader, Fab, TurboSquid): open the link, check the licence on the page, buy it yourself, unzip it into `tmp/assets/<slug>/purchased/`, set `\"source\": { \"provider\": \"purchased\", \"url\": \"<store page>\", \"licence\": \"Royalty Free\", \"author\": \"<seller>\" }` and `asset run <slug>`.",
    "- Never commit the raw purchased files (they stay under gitignored `tmp/`); commit only the compiled assets and the `docs/credits.md` row.",
    "",
  );
  return lines.join("\n");
}

// ---------------------------------------------------------------- an incoming model folder
// Finds the model to import in a download/ or purchased/ folder: glb/gltf first, then fbx, obj, blend.
export function findModel(dir) {
  const all = [];
  const walk = (d) => { for (const n of readdirSync(d)) { const p = join(d, n); statSync(p).isDirectory() ? walk(p) : all.push(p); } };
  if (!existsSync(dir)) return null;
  walk(dir);
  for (const ext of MODEL_FORMATS) {
    const hits = all.filter((p) => p.toLowerCase().endsWith("." + ext) && !/_lod[1-9]|collision/i.test(p)).sort((a, b) => statSync(b).size - statSync(a).size);
    if (hits.length) return hits[0];
  }
  return null;
}

// The credit text for docs/credits.md: one row per model, whatever the licence (we record CC0 and paid ones too).
export function creditLine(rec) {
  return `| ${rec.name} | ${rec.author || "unknown"} | ${rec.url} | ${rec.licence} | \`tools/assets/${rec.slug}/source.json\` |`;
}

export function writeShortlist(root, slug, text) {
  const file = join(root, "tmp", "assets", slug, "shortlist.md");
  mkdirSync(join(root, "tmp", "assets", slug), { recursive: true });
  writeFileSync(file, text);
  return file;
}
export const readJson = (f) => JSON.parse(readFileSync(f, "utf8"));
