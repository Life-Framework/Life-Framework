// Finds the Enforce string operations that silently cut at 8191 chars (docs/architecture/enforce-string-limits.md,
// issue #2673): a `Substring` splice or tail of a string that has no cap, and a `string.Format` with an argument
// that can be a whole payload. Pure text scan; validate-string-splice.mjs and its selftest both use it.
//
// Kinds:
//   splice  a `.Substring(start, len)` whose `len` is not a literal or a CONSTANT, inside a statement that also
//           concatenates (`+`, `+=`): the shape that rewrote a 9388-char save into 8204 (#2520).
//   tail    a `.Substring(start, ... .Length() ...)` of a payload-named string (json, payload, blob ...): "from
//           here to the end" of a string with no known cap. A tail of a row name or a path is not flagged.
//   format  a `string.Format(...)` whose argument is named like a payload (json, payload, blob, ledger ...) or is
//           `SaveToString()`, and whose result is not just a log line.
// A hit is keyed `<file>::<method>::<kind>` (line numbers drift, methods do not) with an occurrence count.
// A site that is bounded for a stated reason is marked `// lf-bounded: <why>` on its line or the line above.

const CONSTANT = /^(?:[A-Z][A-Za-z0-9_]*\.)?[A-Z][A-Z0-9_]*$/;
const PAYLOAD_NAME = /(?:payload|json|serialized|serialised|blob|csv|ledger|dump|saveText)/i;
const NOT_METHOD = new Set(["if", "for", "foreach", "while", "switch", "return", "else", "new", "delete", "case", "ref", "class", "modded", "sealed"]);
const MODIFIER = "(?:static|override|private|protected|event|proto|native|external|sealed|ref|autoptr|const|volatile|out|inout)";
const METHOD_HEAD = new RegExp(`^\\s*(?:${MODIFIER}\\s+)*[A-Za-z_][\\w]*(?:\\s*<[^>]*>)?\\s+(~?[A-Za-z_]\\w*)\\s*\\(`);

/** Blank comments and string literals (keeping length and newlines) so a scan never sees their text. */
export function strip(src) {
  let out = "";
  for (let i = 0; i < src.length; ) {
    const c = src[i];
    const d = src[i + 1];
    if (c === "/" && d === "/") {
      while (i < src.length && src[i] !== "\n") { out += " "; i++; }
    } else if (c === "/" && d === "*") {
      while (i < src.length && !(src[i] === "*" && src[i + 1] === "/")) { out += src[i] === "\n" ? "\n" : " "; i++; }
      if (i < src.length) { out += "  "; i += 2; }
    } else if (c === '"') {
      out += '"'; i++;
      while (i < src.length && src[i] !== '"' && src[i] !== "\n") {
        if (src[i] === "\\" && i + 1 < src.length) { out += "  "; i += 2; } else { out += " "; i++; }
      }
      if (src[i] === '"') { out += '"'; i++; }
    } else { out += c; i++; }
  }
  return out;
}

/** Index of the paren closing the one at `open`, or -1. */
function closeParen(s, open) {
  let depth = 0;
  for (let i = open; i < s.length; i++) {
    if (s[i] === "(") depth++;
    else if (s[i] === ")" && --depth === 0) return i;
  }
  return -1;
}

/** Top-level comma-separated arguments of s[open..close]. */
function splitArgs(s, open, close) {
  const args = [];
  let depth = 0;
  let from = open + 1;
  for (let i = open + 1; i < close; i++) {
    const c = s[i];
    if (c === "(" || c === "[" || c === "<") depth++;
    else if (c === ")" || c === "]" || c === ">") depth--;
    else if (c === "," && depth === 0) { args.push(s.slice(from, i)); from = i + 1; }
  }
  args.push(s.slice(from, close));
  return args;
}

function statementBounds(s, at) {
  let a = at;
  while (a > 0 && !";{}".includes(s[a - 1])) a--;
  // A control header (`if (...)`, `foreach (...)`) is not part of the statement it guards.
  const head = /^\s*(?:else\s+)?(?:if|for|foreach|while)\s*\(/.exec(s.slice(a, at));
  if (head) {
    const close = closeParen(s, a + head[0].length - 1);
    if (close >= 0 && close < at) a = close + 1;
  }
  let depth = 0;
  let b = at;
  for (; b < s.length; b++) {
    if (s[b] === "(") depth++;
    else if (s[b] === ")" && --depth < 0) break;
    else if (s[b] === ";" && depth <= 0) break;
  }
  return [a, b];
}

/** The expression a call sits in: back and forward to the statement end or the enclosing call's `(`, `)` or `,`. */
function segmentBounds(s, at, close) {
  let a = at;
  for (let depth = 0; a > 0; a--) {
    const c = s[a - 1];
    if (";{}".includes(c)) break;
    if (c === ")") depth++;
    else if (c === "(") { if (depth === 0) break; depth--; }
    else if (c === "," && depth === 0) break;
  }
  let b = close + 1;
  for (let depth = 0; b < s.length; b++) {
    const c = s[b];
    if (c === ";") break;
    if (c === "(") depth++;
    else if (c === ")") { if (depth === 0) break; depth--; }
    else if (c === "," && depth === 0) break;
  }
  return [a, b];
}

/** Replace the inside of every balanced paren pair with spaces, so a `+` in a nested call does not count. */
function blankParens(t) {
  let out = "";
  let depth = 0;
  for (const c of t) {
    if (c === "(") { depth++; out += depth === 1 ? "(" : " "; }
    else if (c === ")") { if (depth > 0) depth--; out += depth === 0 ? ")" : " "; }
    else out += depth > 0 ? " " : c;
  }
  return out;
}

/** A length argument that cannot grow with the input: a number, a CONSTANT, or Math.Min(..., one of those). */
export function boundedLength(arg) {
  const t = arg.trim();
  if (/^\d+$/.test(t) || CONSTANT.test(t)) return true;
  const m = /^Math\.Min\s*\((.*)\)$/s.exec(t);
  if (m) return m[1].split(",").some((p) => /^\d+$/.test(p.trim()) || CONSTANT.test(p.trim()));
  return false;
}

const lineOf = (src, idx) => src.slice(0, idx).split("\n").length;

/** The method whose header is the closest one at or above `line` (1-based), or "<file>". */
function methodAt(lines, line) {
  for (let i = line - 1; i >= 0; i--) {
    const m = METHOD_HEAD.exec(lines[i]);
    if (m && !NOT_METHOD.has(m[1]) && !/;\s*$/.test(lines[i]) && !/=/.test(lines[i].slice(0, m.index + m[0].length))) return m[1];
  }
  return "<file>";
}

/** Every flagged site in one script: [{ line, method, kind, text }]. */
export function scanSource(src) {
  const code = strip(src);
  const lines = code.split("\n");
  const raw = src.split("\n");
  const hits = [];
  const seen = new Set();
  const add = (idx, kind) => {
    const line = lineOf(code, idx);
    if (/lf-bounded:/.test(raw[line - 1] || "") || /lf-bounded:/.test(raw[line - 2] || "")) return;
    const key = `${line}:${kind}`;
    if (seen.has(key)) return;
    seen.add(key);
    hits.push({ line, method: methodAt(lines, line), kind, text: (raw[line - 1] || "").trim() });
  };

  for (const m of code.matchAll(/\.Substring\s*\(/g)) {
    const open = m.index + m[0].length - 1;
    const close = closeParen(code, open);
    if (close < 0) continue;
    const args = splitArgs(code, open, close);
    if (args.length < 2 || boundedLength(args[1])) continue;
    // `.Length() + 1`, `.ToInt() + 1`: the Substring is consumed as a number or flag, nothing is rebuilt.
    const consumed = /^\s*\.\s*(?:Length|ToInt|ToFloat|Hash|IndexOf|IndexOfFrom|Contains|StartsWith|EndsWith|Compare|ToType)\s*\(/.test(code.slice(close + 1, close + 40));
    const [a, b] = segmentBounds(code, m.index, close);
    const around = blankParens(code.slice(a, m.index) + "\u0000" + code.slice(close + 1, b));
    if (!consumed && /\+=|(?<!\+)\+(?!\+)/.test(around)) add(m.index, "splice");
    else if (/\.Length\s*\(\s*\)/.test(args[1]) && PAYLOAD_NAME.test(/[\w.\[\]()]*$/.exec(code.slice(a, m.index))[0])) add(m.index, "tail");
  }

  for (const m of code.matchAll(/\bstring\s*\.\s*Format\s*\(/g)) {
    const open = m.index + m[0].length - 1;
    const close = closeParen(code, open);
    if (close < 0) continue;
    const args = splitArgs(code, open, close).slice(1);
    const long = args.some((x) => PAYLOAD_NAME.test(x) || /SaveToString\s*\(/.test(x));
    if (!long) continue;
    const [a] = statementBounds(code, m.index);
    if (/\bLF_Debug\s*\.\s*\w+\s*\(|\bPrint\s*\(|\bPrintFormat\s*\(/.test(code.slice(a, m.index))) continue;
    add(m.index, "format");
  }
  return hits.sort((x, y) => x.line - y.line);
}

/** `file::method::kind` -> count, for one file's hits. */
export function keyHits(file, hits, into = new Map()) {
  for (const h of hits) {
    const k = `${file}::${h.method}::${h.kind}`;
    into.set(k, (into.get(k) || 0) + 1);
  }
  return into;
}

/**
 * Compare found keys with the baseline ({ key: { count, reason } }).
 * fresh: more hits than allowed (or an unlisted key); stale: fewer than the baseline holds (shrink it);
 * noReason: an entry with no real reason.
 */
export function compare(found, baseline) {
  const fresh = [];
  const stale = [];
  const noReason = [];
  for (const [k, n] of found) {
    const b = baseline[k];
    if (!b || n > b.count) fresh.push({ key: k, found: n, allowed: b ? b.count : 0 });
  }
  for (const [k, b] of Object.entries(baseline)) {
    const n = found.get(k) || 0;
    if (n < b.count) stale.push({ key: k, found: n, allowed: b.count });
    if (typeof b.reason !== "string" || b.reason.trim().length < 12) noReason.push(k);
  }
  return { fresh, stale, noReason };
}
