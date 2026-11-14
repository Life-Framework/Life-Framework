// Request audit (#693a). Every `RplRcver.Server` handler takes a request from a
// inputs: addons/**
// client (RpcAsk_* by convention; #693b widened the scope to any name, which
// brought in the three RPC_LF_* quantity handlers), so the server must check it before acting: who sent it
// (ownership, role or duty), how far away it is (range), what it costs (money
// or amount bounds), how often (cooldown), or a dev gate. AGENTS.md: "Validate
// client requests on the server; never trust the client."
//
// A handler counts as checked when its body, or the body of a method it calls
// (followed two levels deep, by name, across non-test scripts), contains one of
// the CHECKS patterns below. The patterns are a heuristic: they prove a check is
// written, not that it is right. The #693b fix list is the human read of them.
//
// request-audit-baseline.json lists the handlers that had no check when this
// landed. A handler in it passes (known debt, fixed per area in #693b); any other
// unchecked handler fails. A baselined handler that recorded a cooldown (the
// LF_RateLimit helper, #693b) fails when the cooldown goes, even if other checks
// remain. A baseline entry whose handler is gone is reported as stale;
// `--update-baseline` rewrites the file.
//
//   node tools/validation/validate-request-audit.mjs                 check
//   node tools/validation/validate-request-audit.mjs --list          every handler and its verdict
//   node tools/validation/validate-request-audit.mjs --update-baseline
//
// red-proof: the selftest runs the fixtures in tools/validation/fixtures/request-audit/
// (*.bad.c must report every handler unchecked, *.good.c none) and fails the run when
// either side disagrees; it runs on every invocation.
import { existsSync, readdirSync, readFileSync, statSync, writeFileSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "..", "..");
const scripts = join(root, "addons", "LifeFramework", "Scripts", "Game");
const baselinePath = join(here, "request-audit-baseline.json");
const fixtures = join(here, "fixtures", "request-audit");

export const CHECKS = {
  devgate: /\bLF_DevGate\s*\.\s*Check\w*\s*\(/,
  range: /\bvector\s*\.\s*Distance(?:Sq)?\s*\(|\b\w*(?:Reach|TooFar|AtDesk|AwayFrom\w*|InRange|\w*Near\w*|AtApplicationDesk|WithinDistance)\w*\s*\(/,
  money: /\b(?:GetCash|HasCash|CanAfford|TryRemoveCash|RemoveCash|GetBalance|TotalDebt)\s*\(|\b(?:amount|quantity|splitSize|score)\s*(?:<=|<|>=|>)\s*\w+/,
  cooldown: /Cooldown|RateLimit|Throttle/,
  role: /\b(?:IsOnDuty\w*|IsAdmin\w*|Authorize\w*|AllowsUser|IsWhitelisted\w*|IsTrusted\w*|CanOpen\w*|CanUse\w*)\s*\(|\bm_eCurrentJob\s*[!=]=/,
  ownership: /\bGetOwner\s*\(\)\s*[!=]=|\bIsOwner\w*\s*\(|[!=]=\s*GetControlledEntity\s*\(\)|GetControlledEntity\s*\(\)\s*[!=]=/,
};

function walk(dir, out = []) {
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    if (statSync(p).isDirectory()) walk(p, out);
    else if (name.endsWith(".c") && !name.startsWith("LF_Test")) out.push(p);
  }
  return out;
}

// Comments and string literals blanked, so a log line cannot satisfy a pattern.
export function strip(src) {
  return src
    .replace(/\r\n/g, "\n")
    .replace(/\/\*[\s\S]*?\*\//g, (m) => m.replace(/[^\n]/g, " "))
    .replace(/\/\/[^\n]*/g, (m) => " ".repeat(m.length))
    .replace(/"(?:[^"\\\n]|\\.)*"/g, (m) => '"' + " ".repeat(m.length - 2) + '"');
}

function bodyAt(code, open) {
  let depth = 1;
  let i = open;
  while (i < code.length && depth > 0) {
    if (code[i] === "{") depth++;
    else if (code[i] === "}") depth--;
    i++;
  }
  return code.slice(open, i - 1);
}

// Method bodies per class: Map<class, Map<method, body[]>>. Calls resolve as
// Class.Method, then a bare Method in the calling class, then a receiver call
// (`var.Method(`) when exactly one class defines that name.
export function methodIndex(sources) {
  const idx = new Map();
  const head = /\b(?:static\s+|protected\s+|private\s+|override\s+|proto\s+)*[\w<>\[\],]+\s+(\w+)\s*\([^;{)]*\)\s*\{/g;
  const skip = new Set(["if", "while", "for", "foreach", "switch", "return", "else"]);
  for (const src of sources) {
    const code = strip(src);
    const cre = /\bclass\s+(\w+)[^{;]*\{/g;
    let c;
    while ((c = cre.exec(code))) {
      const cbody = bodyAt(code, cre.lastIndex);
      if (!idx.has(c[1])) idx.set(c[1], new Map());
      const methods = idx.get(c[1]);
      let m;
      head.lastIndex = 0;
      while ((m = head.exec(cbody))) {
        if (skip.has(m[1])) continue;
        const body = bodyAt(cbody, head.lastIndex);
        if (!methods.has(m[1])) methods.set(m[1], []);
        methods.get(m[1]).push(body);
        head.lastIndex = head.lastIndex + body.length;
      }
    }
  }
  return idx;
}

export function handlers(src, file) {
  const code = strip(src);
  const out = [];
  const re = /RplRcver\s*\.\s*Server\s*\)\s*\]\s*(?:protected\s+|private\s+|static\s+)*void\s+(\w+)\s*\(([^)]*)\)\s*\{/g;
  let m;
  while ((m = re.exec(code))) {
    let cls = "";
    for (const c of code.slice(0, m.index).matchAll(/\bclass\s+(\w+)/g)) cls = c[1];
    out.push({ file, cls, name: m[1], params: m[2].trim(), line: code.slice(0, m.index).split("\n").length, body: bodyAt(code, re.lastIndex) });
  }
  return out;
}

function callees(idx, cls, body) {
  const out = [];
  const call = /(?:(\w+)\s*\.\s*)?\b(\w+)\s*\(/g;
  let m;
  while ((m = call.exec(body))) {
    const recv = m[1];
    const name = m[2];
    let hit = null;
    if (recv && idx.has(recv)) hit = { cls: recv, bodies: idx.get(recv).get(name) };
    else if ((!recv || recv === "this" || recv === "super") && idx.has(cls)) hit = { cls, bodies: idx.get(cls).get(name) };
    if (!hit || !hit.bodies) {
      if (!recv) continue;
      const owners = [...idx].filter(([, ms]) => ms.has(name));
      if (owners.length !== 1) continue;
      hit = { cls: owners[0][0], bodies: owners[0][1].get(name) };
    }
    out.push({ key: hit.cls + "." + name, cls: hit.cls, bodies: hit.bodies });
  }
  return out;
}

function evidence(body, cls, idx, depth, seen) {
  const found = new Set();
  for (const [kind, re] of Object.entries(CHECKS)) if (re.test(body)) found.add(kind);
  if (depth === 0) return found;
  for (const c of callees(idx, cls, body)) {
    if (seen.has(c.key)) continue;
    seen.add(c.key);
    for (const b of c.bodies) for (const k of evidence(b, c.cls, idx, depth - 1, seen)) found.add(k);
  }
  return found;
}

export function audit(files) {
  const idx = methodIndex(files.map((f) => readFileSync(f, "utf8")));
  const out = [];
  for (const f of files) {
    for (const h of handlers(readFileSync(f, "utf8"), f)) {
      const kinds = [...evidence(h.body, h.cls, idx, 3, new Set())];
      if (!h.params) kinds.push("noinput");
      out.push({ file: f, name: h.name, line: h.line, checks: kinds.sort() });
    }
  }
  return out;
}

const rel = (f) => relative(scripts, f).split("\\").join("/");
const key = (h) => `${h.file}#${h.name}`;

// Judge the current handlers against the baseline entries ({file, handler, verdict, checks, note}).
// A handler with no entry must have a check. An entry with verdict "unchecked" is known debt and
// passes. An entry with verdict "checked" fails when the heuristic no longer finds any check.
export function compare(current, entries) {
  const byKey = new Map(entries.map((e) => [`${e.file}#${e.handler}`, e]));
  const fresh = [];
  const lost = [];
  const debt = [];
  for (const h of current) {
    const e = byKey.get(key(h));
    if (!e) {
      if (!h.checks.length) fresh.push(h);
    } else if (e.verdict === "unchecked") {
      debt.push(h);
    } else if (!h.checks.length) {
      lost.push(h);
    } else if ((e.checks || []).includes("cooldown") && !h.checks.includes("cooldown")) {
      lost.push(h);
    }
  }
  const have = new Set(current.map(key));
  const stale = entries.filter((e) => !have.has(`${e.file}#${e.handler}`));
  return { fresh, lost, debt, stale };
}

function selftest() {
  const files = existsSync(fixtures) ? readdirSync(fixtures).filter((n) => n.endsWith(".c")) : [];
  if (!files.some((n) => n.includes(".bad.")) || !files.some((n) => n.includes(".good."))) {
    return "selftest: need at least one .bad.c and one .good.c fixture in tools/validation/fixtures/request-audit";
  }
  for (const n of files) {
    const hs = audit([join(fixtures, n)]);
    if (!hs.length) return `selftest: ${n} has no server RPC handler`;
    const unchecked = hs.filter((h) => h.checks.length === 0);
    if (n.includes(".bad.") && unchecked.length !== hs.length) return `selftest: ${n} should be all unchecked`;
    if (n.includes(".good.") && unchecked.length) return `selftest: ${n} flagged ${unchecked.map((h) => h.name).join(", ")}`;
  }
  const bare = { file: "f", name: "RpcAsk_A", checks: [] };
  const ok = { file: "f", name: "RpcAsk_B", checks: ["range"] };
  if (compare([bare], []).fresh.length !== 1) return "selftest: a new unchecked handler passed";
  if (compare([ok], []).fresh.length !== 0) return "selftest: a new checked handler failed";
  if (compare([bare], [{ file: "f", handler: "RpcAsk_A", verdict: "unchecked" }]).fresh.length !== 0) return "selftest: baselined debt failed";
  if (compare([bare], [{ file: "f", handler: "RpcAsk_A", verdict: "checked" }]).lost.length !== 1) return "selftest: a removed check passed";
  if (compare([], [{ file: "f", handler: "RpcAsk_A", verdict: "checked" }]).stale.length !== 1) return "selftest: a gone handler was not reported stale";
  if (compare([ok], [{ file: "f", handler: "RpcAsk_B", verdict: "checked", checks: ["cooldown", "range"] }]).lost.length !== 1) return "selftest: a removed rate limit passed";
  if (compare([ok], [{ file: "f", handler: "RpcAsk_B", verdict: "checked", checks: ["range"] }]).lost.length !== 0) return "selftest: a handler that kept its checks failed";
  const named = handlers("class C { [RplRpc(RplChannel.Reliable, RplRcver.Server)] protected void RPC_LF_X(int n) { } }", "f");
  if (named.length !== 1) return "selftest: a server RPC not named RpcAsk_ was not audited";
  return null;
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const args = new Set(process.argv.slice(2));
  const self = selftest();
  if (self) {
    console.log(`FAIL ${self}`);
    process.exit(1);
  }
  const all = audit(walk(scripts)).map((h) => ({ ...h, file: rel(h.file) }));
  all.sort((a, b) => key(a).localeCompare(key(b)));
  const old = existsSync(baselinePath) ? JSON.parse(readFileSync(baselinePath, "utf8")).entries : [];
  if (args.has("--list")) {
    for (const h of all) console.log(`${h.checks.length ? "checked  " : "UNCHECKED"} ${key(h)} [${h.checks.join(",")}]`);
  }
  if (args.has("--update-baseline")) {
    // Human verdicts and notes survive; a handler new to the file takes the heuristic's verdict.
    const prior = new Map(old.map((e) => [`${e.file}#${e.handler}`, e]));
    const entries = all.map((h) => {
      const e = { file: h.file, handler: h.name, verdict: h.checks.length ? "checked" : "unchecked", checks: h.checks };
      const p = prior.get(key(h));
      if (p) {
        e.verdict = p.verdict;
        if (p.note) e.note = p.note;
      }
      return e;
    });
    const unchecked = entries.filter((e) => e.verdict === "unchecked").length;
    writeFileSync(baselinePath, JSON.stringify({ handlers: entries.length, checked: entries.length - unchecked, unchecked, entries }, null, 1) + "\n");
    console.log(`validate-request-audit: baseline rewritten, ${entries.length - unchecked} checked, ${unchecked} unchecked`);
    process.exit(0);
  }
  const { fresh, lost, debt, stale } = compare(all, old);
  for (const h of fresh) console.log(`FAIL ${h.file}:${h.line} ${h.name} takes a client request with no server-side check (identity, range, money, cooldown or dev gate); see tools/validation/validate-request-audit.mjs`);
  for (const h of lost) console.log(`FAIL ${h.file}:${h.line} ${h.name} lost the server-side check (or the rate limit) the baseline recorded`);
  for (const e of stale) console.log(`NOTE stale baseline entry ${e.file}#${e.handler} (handler gone); run --update-baseline`);
  console.log(`validate-request-audit: ${all.length} handlers, ${all.length - debt.length} checked, ${debt.length} unchecked (known debt, #693b), ${fresh.length + lost.length} failing`);
  process.exit(fresh.length + lost.length ? 1 : 0);
}
