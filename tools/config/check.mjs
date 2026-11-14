// Offline owner-file check (#281): `tools\cli config-check <config.json>` tells a server owner,
// before a restart, what LF_ConfigService would log at boot. The rules come from
// docs/config/config.schema.json (the generated schema, #280), so a new field or range needs no
// change here. The messages mirror LF_ConfigService / LF_ConfigReader word for word:
//   ERROR  the server logs LF_Debug.Error (the value or section is refused, the shipped one stays)
//   WARN   the server logs LF_Debug.Warn  (the key is ignored) or the file is legal but odd
// Exit code 1 when any ERROR, 0 with warnings only (`--strict` makes warnings fail too).
//
//   node tools/config/check.mjs <config.json> [--schema f] [--no-loops] [--strict] [--json]
//
// Checked: JSON syntax (line and column), unknown keys, wrong types, values outside the schema's
// minimum..maximum, a schemaVersion above or below what this build reads, record keys no shipped
// record has (shops.priceRules, vehicles.vehicles), and every profit loop the owner's prices make
// (tools/economy/profit-loops.mjs). Red-proof: tools/validation/config-check-selftest.mjs.
import { readFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { JsonSyntaxError, parseJson, parseShopRuleKey, checkOwner, typeKey, prefabKey } from "./rules.mjs";
import { SCHEMA_PATH } from "./schema.mjs";
import { checkProfitLoops, judge, parseConf, DEFAULT_SOURCES } from "../economy/profit-loops.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const BASELINE = join(here, "..", "validation", "profit-loop-baseline.json");
const SYSTEM_CATALOG = join(here, "..", "..", "addons", "LifeFramework", "Configs", "Systems", "LF_SystemCatalog.conf");

export { JsonSyntaxError, parseJson, parseShopRuleKey, checkOwner };

const pathOfRes = (r) => String(r ?? "").replace(/^\{[0-9A-Fa-f]+\}/, "");

function shippedKeys(sources) {
  const read = (p) => (p && existsSync(p) ? parseConf(readFileSync(p, "utf8")) : null);
  const keys = {};
  const shop = read(sources.shop);
  keys["shops.priceRules"] = new Set();
  for (const r of shop?.props.m_aPriceRules?.list ?? []) {
    const p = r.props;
    if (p.m_Prefab) keys["shops.priceRules"].add(prefabKey(p.m_Prefab));
    else if (p.m_sFind) keys["shops.priceRules"].add(p.m_sFind);
    else if (typeof p.m_eItemType === "number" && p.m_eItemType !== 0) keys["shops.priceRules"].add(typeKey(p.m_eItemType, typeof p.m_eItemMode === "number" ? p.m_eItemMode : 2));
  }
  const veh = read(sources.vehicles);
  keys["vehicles.vehicles"] = new Set();
  for (const v of veh?.props.m_aVehicles?.list ?? []) {
    for (const r of [v.props.m_SpawnPrefab, v.props.m_Prefab]) if (r) keys["vehicles.vehicles"].add(pathOfRes(r));
  }
  // System ids (#960): the server refuses an id the catalog does not list, so a typo is worth a warning.
  const catalog = read(sources.systems ?? SYSTEM_CATALOG);
  keys["systems.switches"] = new Set();
  for (const d of catalog?.props.m_aSystems?.list ?? []) if (d.props.m_sId) keys["systems.switches"].add(d.props.m_sId);
  return keys;
}

/** Plain value of a node, leaving out every path in `drop` (values the server refuses). */
function plain(n, path, drop) {
  if (drop.has(path.join("\u0000"))) return undefined;
  if (n.t === "obj") {
    const o = {};
    for (const p of n.props) {
      const v = plain(p.val, [...path, p.key], drop);
      if (v !== undefined) o[p.key] = v;
    }
    return o;
  }
  if (n.t === "arr") return n.items.map((it) => it.value);
  return n.value;
}

// ---- The whole check -------------------------------------------------------------------------
/**
 * Checks owner-file text. opts: { schema (object), source (name in messages), loops (bool),
 * sources (profit-loop sources), baseline ([{id, margin}]) }. Returns { problems, loops }.
 */
export function checkText(text, opts = {}) {
  const source = opts.source ?? "config.json";
  const sources = opts.sources ?? DEFAULT_SOURCES;
  const schema = opts.schema ?? JSON.parse(readFileSync(SCHEMA_PATH, "utf8"));
  let root;
  try { root = parseJson(text); } catch (e) {
    if (!(e instanceof JsonSyntaxError)) throw e;
    return { problems: [{ level: "error", line: e.line, col: e.col, msg: `${source} is not valid JSON (${e.message}); every section uses the shipped defaults`, path: null }], loops: [] };
  }
  const problems = checkOwner(root, schema, source, shippedKeys(sources));
  const loops = [];
  if (opts.loops !== false && root.t === "obj") {
    const drop = new Set(problems.filter((p) => p.path).map((p) => p.path.join("\u0000")));
    const ownerData = plain(root, [], drop);
    const { fresh, grown } = judge(checkProfitLoops({ ...sources, owner: null, ownerData }), opts.baseline ?? []);
    const shipped = new Set([...judge(checkProfitLoops({ ...sources, owner: null }), opts.baseline ?? []).fresh.map((l) => l.id)]);
    for (const l of [...fresh, ...grown]) {
      if (shipped.has(l.id)) continue; // a loop the shipped prices already make is not the owner's
      const at = root.props.find((p) => p.key === "shops") ?? root.props[0] ?? root;
      loops.push({ id: l.id, margin: l.margin });
      problems.push({ level: "warn", line: at.pos.line, col: at.pos.col, msg: `profit loop ${l.id}: ${l.path} (margin +${l.margin}). Players can make money for nothing; fix the prices`, path: null, loop: true });
    }
  }
  problems.sort((a, b) => a.line - b.line || a.col - b.col);
  return { problems, loops };
}

export function main(args) {
  const flags = new Set(args.filter((a) => a.startsWith("--")));
  const schemaAt = args.indexOf("--schema");
  const file = args.find((a, n) => !a.startsWith("--") && (schemaAt < 0 || n !== schemaAt + 1));
  if (!file) {
    console.log("usage: tools\\cli config-check <path/to/config.json> [--strict] [--no-loops] [--json] [--schema f]");
    return 2;
  }
  if (!existsSync(file)) {
    console.log(`ERROR  ${file}: cannot open it; every section would use the shipped defaults`);
    return 1;
  }
  const baseline = existsSync(BASELINE) ? JSON.parse(readFileSync(BASELINE, "utf8")) : [];
  const schema = schemaAt >= 0 ? JSON.parse(readFileSync(args[schemaAt + 1], "utf8")) : undefined;
  const { problems } = checkText(readFileSync(file, "utf8"), { source: file, schema, loops: !flags.has("--no-loops"), baseline });
  const errors = problems.filter((p) => p.level === "error").length;
  const warns = problems.length - errors;
  if (flags.has("--json")) console.log(JSON.stringify({ file, errors, warnings: warns, problems }, null, 2));
  else {
    console.log(`config-check: ${file}`);
    for (const p of problems) console.log(`${p.level === "error" ? "ERROR" : "WARN "}  ${p.line}:${p.col}  ${p.msg}`);
    console.log(`config-check: ${errors} error(s), ${warns} warning(s)`);
  }
  return errors > 0 || (flags.has("--strict") && warns > 0) ? 1 : 0;
}

if (process.argv[1] && /[\\/]check\.mjs$/.test(process.argv[1])) process.exit(main(process.argv.slice(2)));
