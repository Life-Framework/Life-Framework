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
import { SCHEMA_PATH } from "./schema.mjs";
import { checkProfitLoops, judge, parseConf, DEFAULT_SOURCES } from "../economy/profit-loops.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const BASELINE = join(here, "..", "validation", "profit-loop-baseline.json");
const SYSTEM_CATALOG = join(here, "..", "..", "addons", "LifeFramework", "Configs", "Systems", "LF_SystemCatalog.conf");

// ---- JSON with positions ---------------------------------------------------------------------
// JSON.parse reports an offset at best, and gives no position for a value. This reader keeps the
// line and column of every key and value. Strict JSON, like the engine's reader: no comments, no
// trailing commas.
export class JsonSyntaxError extends Error {
  constructor(msg, line, col) { super(msg); this.line = line; this.col = col; }
}

export function parseJson(text) {
  if (text.charCodeAt(0) === 0xfeff) text = text.slice(1);
  let i = 0, line = 1, col = 1;
  const at = () => ({ line, col });
  const adv = () => {
    if (text[i] === "\n") { line++; col = 1; } else col++;
    i++;
  };
  const fail = (msg) => { throw new JsonSyntaxError(msg, line, col); };
  const describe = () => (i >= text.length ? "the end of the file" : `'${text[i]}'`);
  const ws = () => { while (i < text.length && /[ \t\r\n]/.test(text[i])) adv(); };
  const str = () => {
    const pos = at();
    adv();
    let out = "";
    for (;;) {
      if (i >= text.length) fail("a string is not closed");
      const c = text[i];
      if (c === '"') { adv(); return { value: out, pos }; }
      if (c === "\n") fail("a string is not closed before the end of the line");
      if (c === "\\") {
        adv();
        const e = text[i];
        if (e === "u") {
          const hex = text.slice(i + 1, i + 5);
          if (!/^[0-9a-fA-F]{4}$/.test(hex)) fail("a \\u escape needs four hex digits");
          out += String.fromCharCode(parseInt(hex, 16));
          for (let k = 0; k < 4; k++) adv();
        } else if ('"\\/bfnrt'.includes(e) && e !== undefined) out += { b: "\b", f: "\f", n: "\n", r: "\r", t: "\t" }[e] ?? e;
        else fail(`'\\${e ?? ""}' is not a JSON escape`);
        adv();
        continue;
      }
      out += c;
      adv();
    }
  };
  const value = () => {
    ws();
    const pos = at();
    const c = text[i];
    if (c === "{") {
      adv();
      const props = [];
      ws();
      if (text[i] === "}") { adv(); return { t: "obj", props, pos }; }
      for (;;) {
        ws();
        if (text[i] !== '"') fail(`expected a property name in quotes, found ${describe()}`);
        const k = str();
        ws();
        if (text[i] !== ":") fail(`expected ':' after the property name, found ${describe()}`);
        adv();
        props.push({ key: k.value, pos: k.pos, val: value() });
        ws();
        if (text[i] === ",") { adv(); continue; }
        if (text[i] === "}") { adv(); return { t: "obj", props, pos }; }
        fail(`expected ',' or '}' after a value, found ${describe()}`);
      }
    }
    if (c === "[") {
      adv();
      const items = [];
      ws();
      if (text[i] === "]") { adv(); return { t: "arr", items, pos }; }
      for (;;) {
        items.push(value());
        ws();
        if (text[i] === ",") { adv(); continue; }
        if (text[i] === "]") { adv(); return { t: "arr", items, pos }; }
        fail(`expected ',' or ']' after a list item, found ${describe()}`);
      }
    }
    if (c === '"') return { t: "str", value: str().value, pos };
    for (const [word, t, v] of [["true", "bool", true], ["false", "bool", false], ["null", "null", null]]) {
      if (text.startsWith(word, i)) { for (let k = 0; k < word.length; k++) adv(); return { t, value: v, pos }; }
    }
    const m = /^-?(0|[1-9]\d*)(\.\d+)?([eE][+-]?\d+)?/.exec(text.slice(i));
    if (m) { for (let k = 0; k < m[0].length; k++) adv(); return { t: "num", value: Number(m[0]), pos }; }
    fail(i >= text.length ? "the file ends before a value" : `expected a value, found ${describe()}`);
  };
  ws();
  if (i >= text.length) fail("the file is empty");
  const root = value();
  ws();
  if (i < text.length) fail(`unexpected ${describe()} after the end of the document`);
  return root;
}

// ---- Shipped record keys ---------------------------------------------------------------------
// The server's record readers take a key to mean "this shipped record, or a new one". Offline, the
// shipped keys come from the shipped .conf. The key grammar a reader insists on (a record it cannot
// parse is skipped) is not in the schema, so it lives here beside the table.
const typeKey = (type, mode) => `type:${type},mode:${mode}`;
/** LF_ShopPriceResolver.RuleKey for a prefab rule: the GUID (or the bare path), upper-cased. */
const prefabKey = (res) => `prefab:${String(res).replace(/^\{([0-9A-Fa-f]+)\}.*$/s, "$1").toUpperCase()}`;

/** LF_ShopPriceResolver.ParseRuleKey, normalised the way RuleKey does ("type:64" is "type:64,mode:2"). */
export function parseShopRuleKey(key) {
  if (!key) return null;
  if (key.startsWith("prefab:")) {
    const id = key.slice(7);
    return id ? { find: "", norm: prefabKey(id) } : null;
  }
  if (!key.startsWith("type:")) return { find: key, norm: key };
  const parts = key.split(",");
  let type = 0, mode = 2;
  for (let n = 0; n < parts.length; n++) {
    const pair = parts[n].split(":");
    if (pair.length !== 2 || !/^\d+$/.test(pair[1])) return null;
    if (n === 0 && pair[0] === "type") type = Number(pair[1]);
    else if (n === 1 && pair[0] === "mode") mode = Number(pair[1]);
    else return null;
  }
  return parts.length <= 2 ? { find: "", norm: typeKey(type, mode) } : null;
}

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

// ---- The rules, driven by the schema ---------------------------------------------------------
const kindOf = (n) => (n.t === "obj" ? "an object" : n.t === "arr" ? "a list" : n.t === "str" ? "a string" : n.t === "num" ? "a number" : n.t === "bool" ? "true or false" : "null");

/**
 * Checks a parsed owner file against the schema. Returns problems
 * { level: "error"|"warn", line, col, msg, path } in file order; `path` is the key path of a
 * refused value (the profit-loop check leaves those out, as the server does).
 */
export function checkOwner(root, schema, source, shipped = {}) {
  const out = [];
  const add = (level, node, msg, path = null) => out.push({ level, line: node.pos.line, col: node.pos.col, msg, path });
  const dotted = (p) => p.join(".");
  const keep = "keeping the shipped value";

  if (root.t !== "obj") {
    add("error", root, `${source}: the top level is ${kindOf(root)}, not an object; every section uses the shipped defaults`);
    return out;
  }

  const range = (sch, n, path) => {
    const v = n.value;
    if (sch.minimum !== undefined && v < sch.minimum) add("error", n, `${dotted(path)} = ${v} is below the minimum ${sch.minimum}; ${keep}`, path);
    else if (sch.maximum !== undefined && v > sch.maximum) add("error", n, `${dotted(path)} = ${v} is above the maximum ${sch.maximum}; ${keep}`, path);
  };

  const scalar = (sch, n, path) => {
    const where = dotted(path);
    switch (sch.type) {
      case "integer":
        if (n.t !== "num" || !Number.isInteger(n.value)) add("error", n, `${where} is not a whole number; ${keep}`, path);
        else range(sch, n, path);
        return;
      case "number":
        if (n.t !== "num") add("error", n, `${where} is not a number; ${keep}`, path);
        else range(sch, n, path);
        return;
      case "boolean":
        if (n.t !== "bool") add("error", n, `${where} is not true or false; ${keep}`, path);
        return;
      case "string":
        if (n.t !== "str" && n.t !== "null") add("error", n, `${where} is not a string; ${keep}`, path);
        return;
      case "array": {
        const kind = sch.items?.type === "integer" || sch.items?.type === "number" ? "num" : sch.items?.type === "boolean" ? "bool" : "str";
        const what = kind === "str" ? "strings" : kind === "bool" ? "true or false" : sch.items?.type === "integer" ? "whole numbers" : "numbers";
        const wrong = n.t !== "arr"
          || n.items.some((it) => it.t !== kind)
          || (sch.items?.type === "integer" && n.items.some((it) => !Number.isInteger(it.value)));
        if (wrong) add("error", n, `${where} is not a list of ${what}; keeping the shipped list`, path);
        return;
      }
      default:
    }
  };

  const object = (sch, n, path) => {
    const known = sch.properties ?? {};
    const recordSchema = typeof sch.additionalProperties === "object" ? sch.additionalProperties : null;
    if (recordSchema) return records(recordSchema, n, path);
    for (const p of n.props) {
      const sub = known[p.key];
      if (!sub) { add("warn", p, `${dotted([...path, p.key])} is not a known key; ignored`); continue; }
      value(sub, p.val, [...path, p.key]);
    }
  };

  const records = (rsch, n, path) => {
    const grammar = dotted(path) === "shops.priceRules" ? parseShopRuleKey : (k) => (k ? { norm: k } : null);
    const have = shipped[dotted(path)];
    for (const p of n.props) {
      const where = dotted([...path, p.key]);
      if (p.val.t !== "obj") { add("error", p, `${where} is not a record (an object); record skipped`, [...path, p.key]); continue; }
      const key = grammar(p.key);
      if (!key) {
        add("error", p, `${dotted(path)}.${p.key}: the key is neither a prefab-path substring, prefab:<GUID> nor type:<flags>,mode:<mode>; record skipped`, [...path, p.key]);
        continue;
      }
      if (dotted(path) === "systems.switches" && have && have.size && !have.has(key.norm)) {
        add("error", p, `${where}: the key is not a system id (Configs/Systems/LF_SystemCatalog.conf lists them); record skipped`, [...path, p.key]);
        continue;
      }
      if (have && have.size && !have.has(key.norm)) {
        add("warn", p, `${where}: no shipped record has this key; it adds a new record. A typo would sit unused`);
      }
      for (const f of p.val.props) {
        const sub = rsch.properties?.[f.key];
        if (!sub) { add("warn", f, `${dotted([...path, p.key, f.key])} is not a known key; ignored`); continue; }
        value(sub, f.val, [...path, p.key, f.key]);
      }
    }
  };

  const value = (sch, n, path) => {
    if (sch.type === "object") {
      if (n.t !== "obj") {
        const recs = typeof sch.additionalProperties === "object";
        add("error", n, `${dotted(path)} is not an object${recs ? " of records keyed by their key" : ""}; keeping the shipped ${recs ? "records" : "value"}`, path);
        return;
      }
      object(sch, n, path);
    } else scalar(sch, n, path);
  };

  const top = schema.properties ?? {};
  const max = top.schemaVersion?.maximum ?? 1;
  for (const p of root.props) {
    if (p.key === "schemaVersion") {
      const v = p.val;
      if (v.t !== "num" || !Number.isInteger(v.value)) add("error", v, `${source}: schemaVersion is not a whole number; reading it as version 1`, [p.key]);
      else if (v.value < 1) add("error", v, `${source}: schemaVersion ${v.value} is below 1; reading it as version 1`, [p.key]);
      else if (v.value > max) add("error", v, `${source} is written for schemaVersion ${v.value} but this build reads up to ${max}; update the server. Fields this build knows still apply, anything else is ignored with a warning`, [p.key]);
      else if (v.value < max) add("warn", v, `${source} is written for schemaVersion ${v.value}, older than this build's ${max}; read as-is (migrations are #303)`, [p.key]);
      continue;
    }
    const sec = top[p.key];
    if (!sec) { add("warn", p, `${source}: unknown top-level key ${p.key}; ignored`); continue; }
    if (p.val.t !== "obj") { add("error", p.val, `${source}: section ${p.key} is not an object; the section uses the shipped defaults`, [p.key]); continue; }
    object(sec, p.val, [p.key]);
  }
  return out;
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
