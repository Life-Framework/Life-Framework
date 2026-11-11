// Config inventory (#277): config roots, their attributes, and every numeric
// tuning constant in Feature/, each tagged with a config section and an owner.
//
//   node tools/config/inventory.mjs            print counts
//   node tools/config/inventory.mjs --write    regenerate docs/config-inventory.md
//   node tools/config/inventory.mjs --check    exit 1 when the doc is stale
//
// The same scan feeds tools/validation/validate-config-constants.mjs.
import { readFileSync, writeFileSync, readdirSync, existsSync, statSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
export const REPO = join(here, "..", "..");
export const ADDON = join(REPO, "addons", "LifeFramework");
export const FEATURE_DIR = join(ADDON, "Scripts", "Game", "Feature");
const DOC = join(REPO, "docs", "config-inventory.md");

// The seven owner-facing config sections (decisions 2026-11-08).
export const SECTIONS = ["jobs", "shops", "crime", "licences", "vehicles", "zones", "persistence"];

// Feature folder -> section. A folder not listed is "unrouted": a tunable there
// still needs a config class, but no section claims it yet.
export const AREA_SECTION = {
  Jobs: "jobs", Gathering: "jobs", Mining: "jobs", Farming: "jobs", Processing: "jobs",
  Chains: "jobs", Medic: "jobs", Quests: "jobs", Organizations: "jobs",
  Shop: "shops", Trader: "shops", Money: "shops", Banking: "shops", ATM: "shops",
  Police: "crime", Crime: "crime", Investigation: "crime", Hideouts: "crime",
  License: "licences", LicensePlate: "licences", Whitelist: "licences",
  Vehicles: "vehicles", VehicleLock: "vehicles", SirenLights: "vehicles",
  Moderation: "zones", Signs: "zones", Houses: "zones",
  Account: "persistence", Character: "persistence", CharacterCreation: "persistence",
};

// Roots whose shipped values are flagship choices: owner "split" until the M3 move.
const SPLIT_ROOTS = new Set([
  "LF_ShopPricesConfig", "LF_ResourceChainCatalog", "LF_SignCatalog", "LF_GarageConfig",
  "LF_JobsConfig", "LF_PoliceConfig", "LF_RespawnConfig", "LF_SurvivalConfig",
]);

const FIXED_TOKENS = new Set([
  "VERSION", "MASK", "SHIFT", "ID", "INVALID", "NONE", "EMPTY", "HEIGHT", "WIDTH", "ROWS", "COLS",
  "PAD", "PADDING", "MARGIN", "FONT", "SLOT", "BIT", "FLAG", "FLAGS", "INDEX", "LENGTH", "PAIRS",
  "VARIANTS", "SIDE", "FRONT", "OFFSET", "SCALE",
]);
const TUNABLE_TOKENS = new Set([
  "MS", "SEC", "SECS", "SECONDS", "INTERVAL", "COOLDOWN", "TIMEOUT", "DURATION", "DELAY", "TTL",
  "RANGE", "RADIUS", "DISTANCE", "DIST", "REACH", "PRICE", "COST", "FINE", "FEE", "PAY", "WAGE",
  "RATE", "CHANCE", "SUCCESS", "NOISE", "DAMAGE", "LIMIT", "MAX", "MIN", "CAP", "WANTED",
  "SPEED", "TICK", "DEFAULT", "AMOUNT", "PER", "INCREASE", "DECAY", "THRESHOLD", "WINDOW",
]);

function walk(dir, ext, out = []) {
  if (!existsSync(dir)) return out;
  for (const e of readdirSync(dir).sort()) {
    const p = join(dir, e);
    if (statSync(p).isDirectory()) walk(p, ext, out);
    else if (p.endsWith(ext)) out.push(p);
  }
  return out;
}
const rel = (p, base) => relative(base, p).split("\\").join("/");

/** Classify a numeric constant name: "fixed" | "tunable" | "unclassified". */
export function classify(name) {
  const toks = name.split("_").filter(Boolean).map((t) => t.toUpperCase());
  if (toks.some((t) => FIXED_TOKENS.has(t))) return "fixed";
  if (toks.some((t) => TUNABLE_TOKENS.has(t))) return "tunable";
  return "unclassified";
}

/**
 * Numeric constants declared under featureDir:
 * { file, line, name, type, value, area, section, kind, marked, configDefault }.
 * `marked` is a `// lf-fixed: <why>` on the line or the line above: deliberately fixed.
 */
export function scanConstants(featureDir = FEATURE_DIR) {
  const out = [];
  const re = /^\s*(?:protected |private )?(?:static )?const (int|float)\s+(\w+)\s*=\s*([^;]+);/;
  for (const file of walk(featureDir, ".c")) {
    const lines = readFileSync(file, "utf8").split("\n");
    const r = rel(file, featureDir);
    const area = r.split("/")[0];
    for (let i = 0; i < lines.length; i++) {
      const m = re.exec(lines[i]);
      if (!m) continue;
      const [, type, name, value] = m;
      const marked = /lf-fixed:/.test(lines[i]) || (i > 0 && /^\s*\/\/\s*lf-fixed:/.test(lines[i - 1]));
      const configDefault = /Config\.c$/.test(file) && /^DEFAULT_/.test(name);
      out.push({
        file: r, line: i + 1, name, type, value: value.trim(), area,
        section: AREA_SECTION[area] ?? "unrouted",
        kind: classify(name), marked, configDefault,
      });
    }
  }
  return out;
}

/** A constant the check holds to: tunable, not marked fixed, not a config class default. */
export const needsEntry = (c) => c.kind === "tunable" && !c.marked && !c.configDefault;
export const constKey = (c) => `${c.file}::${c.name}`;

/** Config roots: the class named by each Configs/**.conf, with the attributes in its script file. */
export function scanRoots(addonDir = ADDON) {
  const scripts = walk(join(addonDir, "Scripts", "Game"), ".c");
  const classFile = new Map();
  for (const f of scripts) {
    const text = readFileSync(f, "utf8");
    for (const m of text.matchAll(/^\s*class\s+(\w+)/gm)) if (!classFile.has(m[1])) classFile.set(m[1], f);
  }
  const roots = [];
  for (const conf of walk(join(addonDir, "Configs"), ".conf")) {
    const head = /^\s*(\w+)\s*(?:"[^"]*"\s*)?\{/m.exec(readFileSync(conf, "utf8"));
    if (!head || !classFile.has(head[1])) continue;
    const sf = classFile.get(head[1]);
    const attrs = (readFileSync(sf, "utf8").match(/\[Attribute\(/g) ?? []).length;
    const area = /Scripts\/Game\/Feature\/(\w+)\//.exec(rel(sf, addonDir))?.[1];
    roots.push({
      root: head[1], conf: rel(conf, addonDir), script: rel(sf, addonDir), attributes: attrs,
      section: (area && AREA_SECTION[area]) ?? "unrouted",
      owner: SPLIT_ROOTS.has(head[1]) ? "split" : "core",
    });
  }
  return roots.sort((a, b) => a.root.localeCompare(b.root));
}

export function buildInventory() {
  return { roots: scanRoots(), consts: scanConstants() };
}

function tally(items, keyFn) {
  const t = {};
  for (const i of items) t[keyFn(i)] = (t[keyFn(i)] ?? 0) + 1;
  return t;
}

export function counts({ roots, consts }) {
  const tun = consts.filter((c) => c.kind === "tunable");
  return {
    roots: roots.length,
    attributes: roots.reduce((n, r) => n + r.attributes, 0),
    constants: consts.length,
    tunable: tun.length,
    fixed: consts.filter((c) => c.kind === "fixed").length,
    unclassified: consts.filter((c) => c.kind === "unclassified").length,
    exempt: tun.filter((c) => !needsEntry(c)).length,
    debt: tun.filter(needsEntry).length,
  };
}

export function render(inv) {
  const { roots, consts } = inv;
  const n = counts(inv);
  const tun = consts.filter((c) => c.kind === "tunable");
  const L = [];
  L.push("# Config inventory", "");
  L.push("Generated by `node tools/config/inventory.mjs --write`; do not edit by hand.");
  L.push("`tools\\cli validate` runs `validate-config-constants.mjs`, which uses the same scan and");
  L.push("fails on a tunable constant that is neither baselined nor marked fixed (#277).", "");
  L.push("## Counts", "");
  L.push(`- Config roots (a \`.conf\` file whose root class is script-defined): **${n.roots}**`);
  L.push(`- Config attributes (\`[Attribute(\` in the root's script file): **${n.attributes}**`);
  L.push(`- Numeric constants in \`Feature/\`: **${n.constants}**; tunable **${n.tunable}**, fixed **${n.fixed}**, unclassified **${n.unclassified}**`);
  L.push(`- Tunables that are config defaults or marked \`// lf-fixed:\`: **${n.exempt}**; baselined debt for #284: **${n.debt}**`, "");
  L.push("A constant is **tunable** when its name holds a time, range, price, cap or rate word");
  L.push("(`_MS`, `_RADIUS`, `_FINE`, `MAX_`, ...). It is **fixed** when it holds a structural word");
  L.push("(`VERSION`, `_MASK`, `_HEIGHT`, ...). To keep a tunable in script on purpose, put");
  L.push("`// lf-fixed: <why>` on or above it. A new tunable otherwise belongs on a config class.", "");
  L.push("## Config roots", "", "| Root | Section | Owner | Attributes | Config | Script |", "|---|---|---|---|---|---|");
  for (const r of roots) L.push(`| \`${r.root}\` | ${r.section} | ${r.owner} | ${r.attributes} | \`${r.conf}\` | \`${r.script}\` |`);
  L.push("", "## Tunable constants by section", "", "| Section | Tunable | Baselined debt |", "|---|---|---|");
  const by = tally(tun, (c) => c.section), debt = tally(tun.filter(needsEntry), (c) => c.section);
  for (const s of [...SECTIONS, "unrouted"]) if (by[s]) L.push(`| ${s} | ${by[s]} | ${debt[s] ?? 0} |`);
  L.push("", "## Tunable constants by feature area", "", "| Area | Section | Tunable | Baselined debt |", "|---|---|---|---|");
  const areas = tally(tun, (c) => c.area), areaDebt = tally(tun.filter(needsEntry), (c) => c.area);
  for (const a of Object.keys(areas).sort()) L.push(`| ${a} | ${AREA_SECTION[a] ?? "unrouted"} | ${areas[a]} | ${areaDebt[a] ?? 0} |`);
  L.push("", "## Tunable constants", "", "Owner is core for every row until the M3 move.", "",
    "| File | Constant | Value | Section | Status |", "|---|---|---|---|---|");
  for (const c of tun) {
    let status = "to move (#284)";
    if (c.marked) status = "fixed (marked)";
    else if (c.configDefault) status = "config default";
    L.push(`| \`${c.file}\` | \`${c.name}\` | \`${c.value.replace(/\|/g, "\\|")}\` | ${c.section} | ${status} |`);
  }
  return L.join("\n") + "\n";
}

if (process.argv[1] && process.argv[1].endsWith("inventory.mjs")) {
  const inv = buildInventory();
  const text = render(inv);
  const args = process.argv.slice(2);
  if (args.includes("--write")) writeFileSync(DOC, text);
  else if (args.includes("--check")) {
    if (!existsSync(DOC) || readFileSync(DOC, "utf8") !== text) {
      console.log("config-inventory: docs/config-inventory.md is stale; run node tools/config/inventory.mjs --write");
      process.exit(1);
    }
  }
  const n = counts(inv);
  console.log(`config-inventory: ${n.roots} roots, ${n.attributes} attributes, ${n.constants} numeric constants (${n.tunable} tunable, ${n.debt} baselined debt)`);
}
