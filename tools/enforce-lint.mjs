// Structural lint for the addon's Enforce Script: the defect classes the code
// audits kept finding by hand, encoded as mechanical rules.
//
//   L1 unguarded-spawn   GetGame().SpawnEntityPrefab(Resource.Load(...)) outside
//                        Core/EL_Utils.c. Use EL_Utils.SpawnEntityPrefab, which
//                        logs and returns null on a bad prefab.
//   L2 null-deref        a lookup that can return null dereferenced in the same
//                        expression: Find*(...).X, Cast(...Find*(...)).X,
//                        GetGame().GetWorkspace()/GetGameMode()/GetCameraManager().X,
//                        GetUI().X, and Foo.GetInstance().X where Foo's GetInstance
//                        never constructs the instance.
//   L3 timer-pairing     a repeating CallLater(X, ..., true) with no Remove(X) in
//                        the file; a class that subscribes (RegisterScriptHandler,
//                        other.GetOnX().Insert(Y)) with no matching unsubscribe and
//                        no OnDelete, destructor, OnMenuClose, or HandlerDeattached.
//                        One-shot CallLater is not counted.
//   L4 static-registry   a static ref array/map/set holding an entity or component
//                        type in a class whose OnDelete/destructor never touches it.
//   L5 bare-print        Print("[EL_..." / "[LF_..." in a file that already logs
//                        through EL_Debug (one greppable path per feature).
//   L6 resource-ref      a "{GUID}path" literal whose path or GUID belongs to this
//                        addon but does not match the addon's .meta, or whose
//                        EL_/LF_-named file does not exist. Vanilla paths are skipped
//                        so the check needs no game install.
//   L7 null-coalescing   `??` in code (the ternary half lives in
//                        check-no-ternaries).
//
// Pure Node, no game data: runs in CI. The CLI wrapper is
// tools/validation/validate-enforce-lint.mjs; the selftest is
// tools/validation/enforce-lint-selftest.mjs.
import { readdirSync, readFileSync, statSync, existsSync } from "node:fs";
import { join, relative, sep } from "node:path";

export const RULES = {
  L1: "unguarded-spawn",
  L2: "null-deref",
  L3: "timer-pairing",
  L4: "static-registry",
  L5: "bare-print",
  L6: "resource-ref",
  L7: "null-coalescing",
};

const DATA_EXT = /\.(et|layer|conf|layout|ent|emat|st)$/i;

function walk(dir, pred, out = []) {
  if (!existsSync(dir)) return out;
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    const st = statSync(p);
    if (st.isDirectory()) walk(p, pred, out);
    else if (pred(name)) out.push(p);
  }
  return out;
}

const posix = (p) => p.split(sep).join("/");

// Blank out comments (keepStrings) or comments and string bodies (code view),
// preserving every newline and column so offsets map back to source lines.
export function mask(src, keepStrings) {
  let out = "";
  let i = 0;
  const n = src.length;
  while (i < n) {
    const c = src[i];
    const d = src[i + 1];
    if (c === "/" && d === "/") {
      while (i < n && src[i] !== "\n") { out += " "; i++; }
    } else if (c === "/" && d === "*") {
      out += "  "; i += 2;
      while (i < n && !(src[i] === "*" && src[i + 1] === "/")) { out += src[i] === "\n" ? "\n" : " "; i++; }
      if (i < n) { out += "  "; i += 2; }
    } else if (c === '"') {
      out += '"'; i++;
      while (i < n && src[i] !== '"' && src[i] !== "\n") {
        if (src[i] === "\\" && i + 1 < n && src[i + 1] !== "\n") {
          out += keepStrings ? src[i] + src[i + 1] : "  ";
          i += 2;
          continue;
        }
        out += keepStrings ? src[i] : " ";
        i++;
      }
      if (i < n && src[i] === '"') { out += '"'; i++; }
    } else {
      out += c; i++;
    }
  }
  return out;
}

function lineStarts(src) {
  const starts = [0];
  for (let i = 0; i < src.length; i++) if (src[i] === "\n") starts.push(i + 1);
  return starts;
}

function lineOf(starts, off) {
  let lo = 0, hi = starts.length - 1;
  while (lo < hi) {
    const mid = (lo + hi + 1) >> 1;
    if (starts[mid] <= off) lo = mid; else hi = mid - 1;
  }
  return lo + 1;
}

function matchClose(code, open, o = "(", c = ")") {
  let depth = 0;
  for (let i = open; i < code.length; i++) {
    if (code[i] === o) depth++;
    else if (code[i] === c) { depth--; if (depth === 0) return i; }
  }
  return -1;
}

function matchOpenBackward(code, close) {
  let depth = 0;
  for (let i = close; i >= 0; i--) {
    if (code[i] === ")") depth++;
    else if (code[i] === "(") { depth--; if (depth === 0) return i; }
  }
  return -1;
}

// Top-level class bodies: { name, start (offset of `class`), open, close }.
function classes(code) {
  const out = [];
  const re = /\b(?:modded\s+)?class\s+(\w+)[^;{]*\{/g;
  let m;
  while ((m = re.exec(code))) {
    const open = m.index + m[0].length - 1;
    const close = matchClose(code, open, "{", "}");
    if (close < 0) continue;
    out.push({ name: m[1], start: m.index, open, close, body: code.slice(open, close + 1) });
    re.lastIndex = close + 1;
  }
  return out;
}

function methodBody(body, nameRe) {
  const re = new RegExp(`\\b(?:void|bool|int)?\\s*${nameRe}\\s*\\([^)]*\\)\\s*\\{`, "g");
  const m = re.exec(body);
  if (!m) return null;
  const open = m.index + m[0].length - 1;
  const close = matchClose(body, open, "{", "}");
  return close < 0 ? null : body.slice(open, close + 1);
}

const norm = (s) => s.trim().replace(/\s+/g, " ");

// Classes whose static GetInstance() can return null: it never `new`s itself.
function nullableSingletons(sources) {
  const nullable = new Set();
  for (const { code } of sources) {
    for (const cls of classes(code)) {
      const body = methodBody(cls.body, "GetInstance");
      if (body && !/static\s+\w+\s+GetInstance/.test(cls.body)) continue;
      if (body && !/=\s*new\s+\w+/.test(body)) nullable.add(cls.name);
    }
  }
  return nullable;
}

const ENTITY_TYPE = /^(IEntity|GenericEntity|\w*Entity|\w*Component)$/;

export function lint({ addonDir, scriptsDir, includeData = true }) {
  const findings = [];
  const files = walk(scriptsDir, (n) => n.endsWith(".c") && !n.endsWith(".generated.c"));
  const sources = files.map((abs) => {
    const raw = readFileSync(abs, "utf8").replace(/\r\n/g, "\n");
    return {
      abs,
      rel: posix(relative(addonDir, abs)),
      raw,
      lines: raw.split("\n"),
      starts: lineStarts(raw),
      code: mask(raw, false),
      withStrings: mask(raw, true),
    };
  });
  const add = (rule, src, line, message) =>
    findings.push({ rule, file: src.rel, line, text: norm(src.lines[line - 1] ?? ""), message });

  const nullable = nullableSingletons(sources);
  const metas = indexMetas(addonDir);

  for (const src of sources) {
    const { code, starts } = src;
    const at = (off) => lineOf(starts, off);

    // L1
    if (!/(^|\/)Core\/EL_Utils\.c$/.test(src.rel)) {
      const re = /\bGetGame\(\)\s*\.\s*SpawnEntityPrefab(?:Ex|Local)?\s*\(\s*Resource\.Load\s*\(/g;
      let m;
      while ((m = re.exec(code))) add("L1", src, at(m.index), "spawn without the prefab-resolve guard; use EL_Utils.SpawnEntityPrefab");
    }

    // L2a: Find*(...).X and Cast(...Find*(...)).X
    {
      const re = /\b(Find|FindComponent|FindComponentByType|FindEntityByID|FindEntityByName|FindItem|FindAnyWidget|FindWidget|FindHandler)\s*\(/g;
      let m;
      while ((m = re.exec(code))) {
        const open = m.index + m[0].length - 1;
        const close = matchClose(code, open);
        if (close < 0) continue;
        let j = close + 1;
        while (code[j] === " ") j++;
        if (code[j] === ")") {
          const castOpen = matchOpenBackward(code, j);
          if (castOpen < 0 || !/\.\s*Cast\s*$/.test(code.slice(Math.max(0, castOpen - 20), castOpen))) continue;
          j++;
          while (code[j] === " ") j++;
        }
        if (code[j] === "." && /[A-Za-z_]/.test(code[j + 1] ?? "")) {
          add("L2", src, at(m.index), `${m[1]}(...) result dereferenced without a null check`);
        }
      }
    }

    // L2b: engine getters that are null on a server, before game mode, or out of play.
    {
      const re = /(?:\bGetGame\(\)\s*\.\s*(GetWorkspace|GetGameMode|GetCameraManager)|\b(GetUI))\(\)\s*\.\s*[A-Za-z_]/g;
      let m;
      while ((m = re.exec(code))) {
        const name = m[1] || m[2];
        const lineNo = at(m.index);
        const lineCode = code.slice(starts[lineNo - 1], m.index);
        if (new RegExp(`!\\s*(GetGame\\(\\)\\s*\\.\\s*)?${name}\\(\\)`).test(lineCode)) continue;
        add("L2", src, lineNo, `${name}() dereferenced without a null check`);
      }
    }

    // L2c: Foo.GetInstance().X where Foo's GetInstance can return null.
    {
      const re = /\b(\w+)\s*\.\s*GetInstance\(\)\s*\.\s*[A-Za-z_]/g;
      let m;
      while ((m = re.exec(code))) {
        if (!nullable.has(m[1])) continue;
        const lineNo = at(m.index);
        const lineCode = code.slice(starts[lineNo - 1], m.index);
        if (new RegExp(`!\\s*${m[1]}\\s*\\.\\s*GetInstance\\(\\)`).test(lineCode)) continue;
        add("L2", src, lineNo, `${m[1]}.GetInstance() can be null and is dereferenced`);
      }
    }

    // L3a: repeating CallLater needs a Remove of the same callback in the file.
    {
      const re = /\bCallLater\s*\(/g;
      let m;
      while ((m = re.exec(code))) {
        const open = m.index + m[0].length - 1;
        const close = matchClose(code, open);
        if (close < 0) continue;
        const args = splitArgs(code.slice(open + 1, close));
        if (args.length < 3 || args[2].trim() !== "true") continue;
        const fn = args[0].trim();
        const removed = new RegExp(`\\bRemove\\s*\\(\\s*${escapeRe(fn)}\\s*\\)`).test(code);
        if (!removed) add("L3", src, at(m.index), `repeating CallLater(${fn}) is never Remove(${fn})'d`);
      }
    }

    // L3b + L4: per class.
    for (const cls of classes(code)) {
      const subs = unpairedSubscriptions(cls.body, src.withStrings.slice(cls.open, cls.close + 1), src.withStrings);
      const cleanup = cleanupBodies(cls);
      if (subs.length && cleanup.length === 0) {
        add("L3", src, at(cls.start), `class ${cls.name} subscribes (${subs.join(", ")}) with no unsubscribe and no OnDelete, destructor, OnMenuClose, or HandlerDeattached`);
      }

      const sre = /\bstatic\s+(?:(?:protected|private)\s+)?ref\s+(array|map|set)\s*<([^;]*?)>\s+(\w+)/g;
      let m;
      while ((m = sre.exec(cls.body))) {
        const types = m[2].split(/[<>,\s]+/).filter((t) => t && t !== "ref");
        if (!types.some((t) => ENTITY_TYPE.test(t))) continue;
        const field = m[3];
        if (cleanup.some((b) => new RegExp(`\\b${field}\\b`).test(b))) continue;
        add("L4", src, at(cls.open + m.index), `static ${m[1]} ${field} holds ${types.join(",")} and no OnDelete/destructor removes from it`);
      }
    }

    // L5
    if (/\bEL_Debug\s*\./.test(code)) {
      const re = /\bPrint(?:Format)?\s*\(\s*"\[(?:EL|LF)_/g;
      let m;
      // Strings are masked in `code`; match against withStrings at the same offsets.
      while ((m = re.exec(src.withStrings))) {
        if (code.slice(m.index, m.index + 5) !== "Print") continue; // inside a comment
        add("L5", src, at(m.index), "bare Print in a file that logs through EL_Debug");
      }
    }

    // L6 (script string literals)
    checkRefs(src.withStrings, src, metas, add, at);

    // L7
    {
      const re = /\?\?/g;
      let m;
      while ((m = re.exec(code))) add("L7", src, at(m.index), "`??` does not exist in Enforce Script");
    }
  }

  if (includeData) {
    for (const abs of walk(addonDir, (n) => DATA_EXT.test(n))) {
      const raw = readFileSync(abs, "utf8").replace(/\r\n/g, "\n");
      const src = { rel: posix(relative(addonDir, abs)), lines: raw.split("\n"), starts: lineStarts(raw) };
      checkRefs(raw, src, metas, add, (off) => lineOf(src.starts, off));
    }
  }

  return findings;
}

function escapeRe(s) {
  return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function splitArgs(s) {
  const out = [];
  let depth = 0, cur = "";
  for (const ch of s) {
    if (ch === "(" || ch === "<" || ch === "[" || ch === "{") depth++;
    else if (ch === ")" || ch === ">" || ch === "]" || ch === "}") depth--;
    if (ch === "," && depth === 0) { out.push(cur); cur = ""; } else cur += ch;
  }
  out.push(cur);
  return out;
}

function cleanupBodies(cls) {
  const out = [];
  for (const name of ["OnDelete", `~${cls.name}`, "OnMenuClose", "HandlerDeattached", "EOnDeactivate"]) {
    const b = methodBody(cls.body, escapeRe(name));
    if (b) out.push(b);
  }
  return out;
}

// Direct members of a class body that have a `{...}` block: { header, start, end }.
function members(body) {
  const out = [];
  let boundary = 1;
  for (let i = 1; i < body.length - 1; i++) {
    const ch = body[i];
    if (ch === ";" || ch === "}") boundary = i + 1;
    else if (ch === "{") {
      const end = matchClose(body, i, "{", "}");
      if (end < 0) break;
      out.push({ header: body.slice(boundary, i), start: i, end });
      i = end;
      boundary = end + 1;
    }
  }
  return out;
}

// Unpaired subscriptions made from instance methods: a RegisterScriptHandler
// with no RemoveScriptHandler for that event, or another object's
// GetOnX().Insert(Y) with no GetOnX().Remove(Y). Repeating timers are L3a's.
// A static method's process-wide hook has no instance lifetime to pair with,
// and Insert on the class's own invoker (no receiver) dies with the class.
// `body` has strings masked (for brace matching); `strBody` and `fileCode` keep
// them at the same offsets, since event names are string literals.
function unpairedSubscriptions(body, strBody, fileCode) {
  const out = new Set();
  for (const mem of members(body)) {
    if (/\bstatic\b/.test(mem.header)) continue;
    const text = strBody.slice(mem.start, mem.end + 1);
    let m;
    const reg = /\bRegisterScriptHandler\s*\(\s*"(\w+)"/g;
    while ((m = reg.exec(text))) {
      if (!new RegExp(`\\bRemoveScriptHandler\\s*\\(\\s*"${m[1]}"`).test(fileCode)) out.add(`RegisterScriptHandler(${m[1]})`);
    }
    const ins = /\.\s*(GetOn\w*)\(\)\s*\.\s*Insert\s*\(\s*(\w+)\s*\)/g;
    while ((m = ins.exec(text))) {
      if (!new RegExp(`\\b${m[1]}\\(\\)\\s*\\.\\s*Remove\\s*\\(\\s*${m[2]}\\s*\\)`).test(fileCode)) out.add(`${m[1]}().Insert(${m[2]})`);
    }
  }
  return [...out];
}

// ------------------------------------------------------------------- L6 refs

export function indexMetas(addonDir) {
  const byPath = new Map(); // lowercased rel path -> { guid, path }
  const byGuid = new Map(); // guid -> rel path
  for (const abs of walk(addonDir, (n) => n.endsWith(".meta"))) {
    const rel = posix(relative(addonDir, abs)).replace(/\.meta$/, "");
    const text = readFileSync(abs, "utf8");
    const m = /Name\s+"\{([0-9A-Fa-f]{16})\}/.exec(text);
    if (!m) continue;
    const guid = m[1].toUpperCase();
    byPath.set(rel.toLowerCase(), { guid, path: rel, exists: existsSync(join(addonDir, rel)) });
    byGuid.set(guid, rel);
  }
  return { byPath, byGuid, addonDir };
}

const REF_RE = /"\{([0-9A-Fa-f]{16})\}([^"\n]+?\.[A-Za-z0-9]+)"/g;

function checkRefs(text, src, metas, add, at) {
  let m;
  REF_RE.lastIndex = 0;
  while ((m = REF_RE.exec(text))) {
    const guid = m[1].toUpperCase();
    const path = m[2];
    const key = path.toLowerCase();
    const meta = metas.byPath.get(key);
    const line = at(m.index);
    if (meta) {
      if (meta.guid !== guid) add("L6", src, line, `{${guid}}${path}: the addon's .meta says {${meta.guid}}`);
      else if (!meta.exists) add("L6", src, line, `{${guid}}${path}: .meta exists but the file does not`);
      continue;
    }
    const owner = metas.byGuid.get(guid);
    if (owner) {
      add("L6", src, line, `{${guid}}${path}: that GUID belongs to ${owner}`);
      continue;
    }
    const base = path.split("/").pop();
    if (/^(EL|LF)_/.test(base) && !existsSync(join(metas.addonDir, path))) {
      add("L6", src, line, `{${guid}}${path}: addon-named resource does not exist and has no .meta`);
    }
  }
}

// ----------------------------------------------------------------- baseline

export const keyOf = (f) => `${f.rule}|${f.file}|${f.text}`;

export function compare(findings, baseline) {
  const budget = new Map();
  for (const e of baseline) budget.set(keyOf(e), (budget.get(keyOf(e)) ?? 0) + 1);
  const fresh = [];
  const known = [];
  for (const f of findings) {
    const k = keyOf(f);
    const left = budget.get(k) ?? 0;
    if (left > 0) { budget.set(k, left - 1); known.push(f); } else fresh.push(f);
  }
  let stale = 0;
  for (const v of budget.values()) stale += v;
  return { fresh, known, stale };
}

export function toBaseline(findings) {
  return findings
    .map((f) => ({ rule: f.rule, file: f.file, text: f.text }))
    .sort((a, b) => keyOf(a).localeCompare(keyOf(b)));
}
