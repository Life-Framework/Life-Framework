// Risk tiers for `wt gate` / `wt ship` / the ship queue, chosen from the diff (docs/worktrees.md "Gate tiers").
//
//   T0  nothing under addons/ changed: `validate --all` only (docs, website, tools).
//   T1  data only: existing values changed in place in a .conf, .st, .layout or a number in an .et.
//       validate (static), the config harness static stage, the placement gate, and ONE boot of the
//       LOGIC tests plus every test whose source names the touched file, config class, GUID or string key.
//   T2  everything else: the full gate (Workbench compile + build, the whole suite, core only, config
//       sessions, placement audit).
//
// Conservative by construction: a file is T1 only when its base and head texts have the same number of
// lines, every line that holds a brace is unchanged (so the block structure is identical), and every
// changed line keeps its key and changes only a scalar value of the kind its file type allows. Anything
// else, including any file this module cannot read, makes the whole change T2.
// Pure functions; proven offline by tools/validation/gate-tier-selftest.mjs.

const GUID = /\{[0-9A-Fa-f]{16}\}/;
const NUM = /^-?(\d+(\.\d*)?|\.\d+)(e[-+]?\d+)?$/i;
// A list of numbers, quoted or not (0 1.5 -2), what an .et writes for vectors and angles.
const NUM_VEC = /^("?)\s*-?[\d.eE+-]+(\s+-?[\d.eE+-]+)*\s*\1$/;
const QUOTED = /^"(?:[^"\\]|\\.)*"$/;
// Keys whose value is identity or a reference, never data: changing them is structure.
const IDENTITY_KEYS = new Set(["ID", "Id", "Name", "Parent", "ItemClassName", "Prefab", "m_Prefab", "ResourceName"]);

const T1_EXT = [".conf", ".st", ".layout", ".et"];

function ext(path) {
  const m = /\.[^./]+$/.exec(path);
  return m ? m[0].toLowerCase() : "";
}

// "  m_iMax 5" -> { key: "m_iMax", value: "5" }; '  "text"' -> { key: null, value: '"text"' }.
function splitLine(line) {
  const t = line.trim();
  if (QUOTED.test(t) || NUM.test(t)) return { key: null, value: t };
  const m = /^([A-Za-z_][\w]*)\s+(.+)$/.exec(t);
  if (!m) return null;
  return { key: m[1], value: m[2].trim() };
}

function scalarOk(kind, value) {
  if (GUID.test(value)) return false;
  if (kind === "et") return NUM.test(value) || NUM_VEC.test(value);
  return NUM.test(value) || NUM_VEC.test(value) || QUOTED.test(value) || /^[A-Za-z_]\w*$/.test(value);
}

// Why one modified addons/ text file is not data-only, or null when it is. `ids` collects the
// string-table Ids whose text changed (for the test mapping).
export function dataOnlyProblem(path, before, after, ids = []) {
  const e = ext(path);
  if (!T1_EXT.includes(e)) return `${e || "this file type"} is not a data file`;
  if (typeof before !== "string" || typeof after !== "string") return "unreadable";
  const a = before.replace(/\r\n/g, "\n").split("\n");
  const b = after.replace(/\r\n/g, "\n").split("\n");
  if (a.length !== b.length) return `line count changed (${a.length} -> ${b.length}): a key, row or block was added or removed`;
  const kind = e.slice(1);
  const stack = [];
  let lastId = null;
  for (let i = 0; i < a.length; i++) {
    const x = a[i];
    const y = b[i];
    if (x === y) {
      if (/\{\s*$/.test(x)) stack.push(x.trim());
      else if (/^\s*\}/.test(x)) stack.pop();
      const idm = /^\s*Id\s+"([^"]*)"/.exec(x);
      if (idm) lastId = idm[1];
      continue;
    }
    const n = i + 1;
    if (/[{}]/.test(x.replace(/\{[0-9A-Fa-f]{16}\}/g, "")) || /[{}]/.test(y.replace(/\{[0-9A-Fa-f]{16}\}/g, ""))) return `line ${n}: a block header or brace changed`;
    if (GUID.test(x) || GUID.test(y)) return `line ${n}: a GUID or resource reference changed`;
    const p = splitLine(x);
    const q = splitLine(y);
    if (!p || !q) return `line ${n}: not a key/value line`;
    if (p.key !== q.key) return `line ${n}: key changed (${p.key ?? "value"} -> ${q.key ?? "value"})`;
    if (p.key && IDENTITY_KEYS.has(p.key)) return `line ${n}: ${p.key} is an identity, not a value`;
    const top = stack[stack.length - 1] ?? "";
    if (kind === "st" && !(p.key && /^(Target_\w+|Comment)$/.test(p.key))) return `line ${n}: only Target_* text and Comment may change in a string table`;
    if (kind === "conf" && /^Ids\s*\{/.test(top)) return `line ${n}: a string-table Id changed`;
    if (!scalarOk(kind, p.value) || !scalarOk(kind, q.value)) return `line ${n}: ${kind === "et" ? "only numbers may change in an .et" : "not a scalar value"}`;
    if (kind === "st" && lastId) ids.push(lastId);
  }
  return null;
}

// files: [{ path, status: "M"|"A"|"D"|"R"|"?"..., before, after }]. Returns
// { tier: 0|1|2, reasons: [string], data: [paths], ids: [string-table Ids] }.
export function classify(files, { forceFull = false } = {}) {
  if (forceFull) return { tier: 2, reasons: ["--full-gate"], data: [], ids: [] };
  const game = files.filter((f) => f.path.startsWith("addons/"));
  if (!game.length) return { tier: 0, reasons: ["nothing under addons/ changed"], data: [], ids: [] };
  const reasons = [];
  const data = [];
  const ids = [];
  for (const f of game) {
    if (f.status !== "M") {
      const what = { A: "added", D: "deleted", R: "renamed", C: "copied", "?": "untracked (new)" }[f.status] ?? `status ${f.status}`;
      reasons.push(`${f.path}: ${what}`);
      continue;
    }
    if (f.path.endsWith(".meta")) {
      reasons.push(`${f.path}: a .meta (GUID) changed`);
      continue;
    }
    const p = dataOnlyProblem(f.path, f.before, f.after, ids);
    if (p) reasons.push(`${f.path}: ${p}`);
    else data.push(f.path);
  }
  if (reasons.length) return { tier: 2, reasons, data, ids: [...new Set(ids)] };
  return { tier: 1, reasons: [`data only: ${data.length} file(s) change values in place`], data, ids: [...new Set(ids)] };
}

// The tokens a test source would name if it depends on a data file: its file stem, its GUID (from
// the .meta text) and, for a .conf, the config class on its first line.
export function fileTokens(path, text, metaText) {
  const toks = new Set();
  const stem = path.split("/").pop().replace(/\.[^.]+$/, "");
  if (stem.length >= 4) toks.add(stem);
  const g = metaText && /Name\s+"(\{[0-9A-Fa-f]{16}\})/.exec(metaText);
  if (g) toks.add(g[1].slice(1, -1));
  if (path.endsWith(".conf") && typeof text === "string") {
    const c = /^\s*([A-Za-z_]\w*)\s*(?::\s*\S+\s*)?\{/.exec(text);
    if (c && c[1] !== "StringTableRuntime") toks.add(c[1]);
  }
  return [...toks];
}

// tests: [{ file, text }]. Returns { logic: [classes], related: [classes] }: every LOGIC-tier class,
// and every class in a file whose text names one of the tokens.
export function selectTests(tests, tokens) {
  const logic = [];
  const related = [];
  for (const t of tests) {
    let tier = null;
    const classes = [];
    for (const line of t.text.split(/\r?\n/)) {
      const tm = /^\s*\/\/\s*tier:\s*(\w+)/.exec(line);
      if (tm) tier = tm[1].toUpperCase();
      const cm = /^\s*class\s+(LF_Test_\w+)\s*:\s*LF_Test\b/.exec(line);
      if (cm) classes.push({ name: cm[1], tier });
    }
    const hit = tokens.some((k) => t.text.includes(k));
    for (const c of classes) {
      if (c.tier === "LOGIC") logic.push(c.name);
      // A PERF test is a timing gate that only means something on `test --tier perf` (idle lane, more repeats); a gate never runs it at the normal settings.
      if (hit && c.tier !== "PERF") related.push(c.name);
    }
  }
  return { logic: [...new Set(logic)], related: [...new Set(related)] };
}

export function tierLine(c) {
  const name = ["T0 (no game files)", "T1 (data only)", "T2 (full gate)"][c.tier];
  return `gate tier ${name}: ${c.reasons.slice(0, 6).join("; ")}${c.reasons.length > 6 ? `; +${c.reasons.length - 6} more` : ""}`;
}

// Reads the change set of a worktree against the merge base with origin/<base>, uncommitted and
// untracked files included. `git(args)` returns { status, stdout }; `read(path)` the working file or null.
export function collectDiff({ git, read, base }) {
  const mb = git(["merge-base", `origin/${base}`, "HEAD"]);
  if (mb.status !== 0) return null;
  const ref = mb.stdout.trim();
  const ns = git(["diff", "--name-status", "-M", ref]);
  if (ns.status !== 0) return null;
  const files = [];
  for (const line of ns.stdout.split(/\r?\n/).filter(Boolean)) {
    const parts = line.split("\t");
    const status = parts[0][0];
    const path = parts[parts.length - 1];
    const f = { path, status };
    if (status === "M" && path.startsWith("addons/") && T1_EXT.includes(ext(path))) {
      const s = git(["show", `${ref}:${path}`]);
      f.before = s.status === 0 ? s.stdout : null;
      f.after = read(path);
    }
    files.push(f);
  }
  const un = git(["ls-files", "--others", "--exclude-standard"]);
  if (un.status !== 0) return null;
  for (const p of un.stdout.split(/\r?\n/).filter(Boolean)) files.push({ path: p, status: "?" });
  return files;
}
