// tools/lib/int-time-math.mjs - find Math.Max/Min/Clamp calls whose arguments are int time values (#2787).
//
// Enforce `Math.Max`, `Math.Min` and `Math.Clamp` work in floats, so an int above 2^24 loses its low bits: unix
// 1800000480 comes back as 1800000512. A time value (seconds, milliseconds, a unix stamp, a deadline) goes through
// LF_Utils.MaxInt / MinInt / ClampInt. The scan is a name heuristic: a call is flagged when an argument names a time
// (a word of its identifier is in TIME_WORDS) and that name is not a float (an `m_f`/`s_f` prefix, or a `float <name>`
// declaration in the 60 lines above). Genuine float time math is marked `// lf-float: <why>` or sits in the baseline.
import { readFileSync } from "node:fs";

/** Words of an identifier (split on camelCase and `_`) that name a time. */
export const TIME_WORDS = new Set([
  "unix", "epoch", "stamp", "second", "seconds", "secs", "sec", "ms", "millis", "milliseconds", "minute", "minutes",
  "hour", "hours", "deadline", "until", "expire", "expires", "expiry", "lease", "lapse", "rent", "cooldown",
  "elapsed", "now", "duration", "age", "ago", "delay", "interval", "grace", "window",
]);

const CALL = /\bMath\.(Max|Min|Clamp)\s*\(/g;
const CONTROL = /^(if|while|for|foreach|switch|return|else|do)\b/;

/** Blank comments and string literals, keeping line structure and length. */
export function strip(src) {
  return src
    .replace(/\/\*[\s\S]*?\*\//g, (m) => m.replace(/[^\n]/g, " "))
    .replace(/\/\/[^\n]*/g, (m) => " ".repeat(m.length))
    .replace(/"(?:[^"\\\n]|\\.)*"/g, (m) => '"' + " ".repeat(m.length - 2) + '"');
}

/** The words of one identifier: `m_iOnDutySeconds` -> on, duty, seconds. */
export function words(ident) {
  return ident
    .replace(/^(?:m_|s_|g_)/, "")
    .replace(/^[a-z](?=[A-Z])/, "")
    .split(/_|(?<=[a-z0-9])(?=[A-Z])|(?<=[A-Z])(?=[A-Z][a-z])/)
    .filter(Boolean)
    .map((w) => w.toLowerCase());
}

/** Whether an identifier names a time. */
export function isTimeName(ident) {
  return words(ident).some((w) => TIME_WORDS.has(w));
}

/** The text between the call's parentheses starting at `open` (the index of "("), or null when unbalanced. */
function argText(text, open) {
  let depth = 0;
  for (let i = open; i < text.length; i++) {
    if (text[i] === "(") depth++;
    else if (text[i] === ")" && --depth === 0) return text.slice(open + 1, i);
  }
  return null;
}

/** The method a line belongs to: the nearest earlier line that opens a method declaration. */
function methodAt(lines, index) {
  for (let i = index; i >= 0; i--) {
    const line = lines[i];
    if (/;\s*$/.test(line) || /^\s*[{}]?\s*$/.test(line)) continue;
    const m = /^\s*(?:(?:static|override|protected|private|proto|event|sealed|native|external|volatile|owned|autoptr|ref|const)\s+)*[\w<>,\[\]]+(?:\s+|\s*\*\s*)(\w+)\s*\(/.exec(line);
    if (m && !CONTROL.test(line.trim())) return m[1];
  }
  return "(class)";
}

/** True when the identifier is a float: an `m_f`/`s_f` name, or `float <name>` declared above the call. */
function isFloat(ident, lines, index) {
  if (/^[ms]_f[A-Z]/.test(ident) || /^f[A-Z]/.test(ident)) return true;
  const decl = new RegExp(`\\bfloat\\s+${ident}\\b`);
  for (let i = index; i >= Math.max(0, index - 60); i--) if (decl.test(lines[i])) return true;
  return false;
}

/** Identifiers in an argument list that are not callee names (`Foo.Bar(` keeps Foo and drops Bar) or members after a dot's call. */
function identifiers(args) {
  const out = [];
  const re = /[A-Za-z_]\w*/g;
  let m;
  while ((m = re.exec(args))) {
    const after = args[m.index + m[0].length];
    const before = args[m.index - 1];
    if (/^\d/.test(m[0])) continue;
    // `LF_Utils.MaxInt(` and `x.Count()`: a called name is the callee, but a time-named getter still counts.
    out.push({ name: m[0], call: after === "(", member: before === "." });
  }
  return out;
}

/** [{ line, method, call, ident }] for each flagged call in one source text. */
export function scanSource(src) {
  const clean = strip(src);
  const lines = clean.split("\n");
  const raw = src.split("\n");
  const hits = [];
  CALL.lastIndex = 0;
  let m;
  while ((m = CALL.exec(clean))) {
    const open = m.index + m[0].length - 1;
    const args = argText(clean, open);
    if (args === null) continue;
    const line = clean.slice(0, m.index).split("\n").length;
    const own = [raw[line - 1] || "", raw[line - 2] || ""].join("\n");
    if (/\/\/\s*lf-float:/.test(own)) continue;
    const named = identifiers(args).filter((i) => isTimeName(i.name)).filter((i) => i.call || !isFloat(i.name, lines, line - 1));
    if (named.length === 0) continue;
    hits.push({ line, method: methodAt(lines, line - 1), call: m[1], ident: named[0].name, text: (raw[line - 1] || "").trim() });
  }
  return hits;
}

/** Count flagged sites per `<file>::<method>` key. */
export function keyHits(file, hits, found) {
  for (const h of hits) {
    const key = `${file}::${h.method}`;
    found.set(key, (found.get(key) || 0) + 1);
  }
  return found;
}

/** The ratchet shared with string-splice: new sites fail, fixed sites are stale, an entry needs a reason. */
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

export function readText(path) {
  return readFileSync(path, "utf8");
}
