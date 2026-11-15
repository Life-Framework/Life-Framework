// tools/lib/log-triage.mjs - group a server log into known and new lines.
//
// Pure functions, no I/O beyond `loadTriageRows`: `tools\cli host triage` prints the result and
// the O12 server-doctor reuses it. A line is in scope when it is an `[LFDebug:*]` warning or error
// (the engine prints those as `SCRIPT (W):` / `SCRIPT (E):`), any `SCRIPT (E)` line, or a `VME:` /
// `Virtual Machine Exception` line. Lines are grouped by normalised text (timestamp, category
// and level dropped; numbers, GUIDs and addresses replaced) and each group is marked known when a
// pattern in a `docs/debug-log-triage.md` row matches it, new otherwise.
//
// Pattern rules (the first cell of a row, each `backticked` span is one pattern):
//   - `...`, `<anything>`, `{GUID}` and a bare `N` are wildcards; the literal pieces must appear in
//     order in the line.
//   - A later span in a row whose first span starts `[LFDebug:Tag]` inherits that tag, so
//     `[LFDebug:Jobs] job request refused` / `equipment refused` covers both.
//   - A span with nothing but the tag (`[LFDebug:Siren] ...`) is too generic to vouch for an error
//     or a warning and is ignored; so is any span with fewer than 6 literal characters beyond its tag.
//   - When several patterns match, the one with the most literal characters wins.

import { existsSync, readFileSync } from "node:fs";

const TIMESTAMP = /^\s*\d{1,2}:\d{2}:\d{2}(?:\.\d+)?\s+/;
const CATEGORY = /^([A-Za-z_]+)\s*(?:\(([EW])\))?\s*:\s?(.*)$/;
const TAG = /^\[LFDebug:[A-Za-z0-9_]+\]/;
const VME = /VME:|Virtual Machine Exception/;
const WILDCARD = /\.\.\.|…|<[^>]*>|\{GUID\}|\bN\b/;
const MIN_LITERAL = 6;

/** Replace the parts of a message that differ between runs so equal faults group together. */
export function normalizeMessage(text) {
  return text
    .replace(/\{[0-9A-Fa-f]{16}\}/g, "{GUID}")
    .replace(/0x[0-9A-Fa-f]+/g, "0xADDR")
    .replace(/\b\d+(?:\.\d+)?\b/g, "N")
    .replace(/\s+/g, " ")
    .trim();
}

/**
 * The lines of a log that triage looks at: [{ line, level, kind, body, message }].
 * `body` is the line without its timestamp (what patterns match against); `message` is the text
 * after the `CATEGORY (L):` prefix (what is normalised and grouped).
 */
export function collectLines(logText) {
  const out = [];
  const lines = logText.split(/\r?\n/);
  for (let i = 0; i < lines.length; i++) {
    const body = lines[i].replace(TIMESTAMP, "").trimEnd();
    if (!body) continue;
    const m = CATEGORY.exec(body);
    const category = m ? m[1] : "";
    const level = m ? m[2] || "" : "";
    const message = m ? m[3] : body;
    let kind = "";
    if (VME.test(body)) kind = "VME";
    else if (category === "SCRIPT" && level === "E") kind = "SCRIPT (E)";
    else if (TAG.test(message) && (level === "E" || level === "W")) kind = "LFDebug";
    if (!kind) continue;
    out.push({ line: i + 1, level: level || "E", kind, body, message });
  }
  return out;
}

function literalPieces(span) {
  const pieces = [];
  let rest = span;
  for (;;) {
    const m = WILDCARD.exec(rest);
    const head = m ? rest.slice(0, m.index) : rest;
    const piece = head.trim();
    if (piece) pieces.push(piece);
    if (!m) break;
    rest = rest.slice(m.index + m[0].length);
  }
  return pieces;
}

/**
 * Rows of a triage markdown table: [{ line, verdict, patterns: [{ text, pieces, score }] }].
 * Only rows with a backticked pattern in the first cell and a verdict in the second are read.
 */
export function parseTriageTable(md) {
  const rows = [];
  const lines = md.split(/\r?\n/);
  for (let i = 0; i < lines.length; i++) {
    const m = /^\|\s*(.*?)\s*\|\s*([^|]*?)\s*\|/.exec(lines[i]);
    if (!m || !m[1].includes("`")) continue;
    const verdictMatch = /^\**([A-Z][A-Z ]*[A-Z])/.exec(m[2]);
    if (!verdictMatch) continue;
    const patterns = [];
    let rowTag = "";
    for (const sm of m[1].matchAll(/`([^`]+)`/g)) {
      let text = sm[1].trim();
      const tag = TAG.exec(text);
      if (tag) rowTag = tag[0];
      else if (rowTag) text = `${rowTag} ${text}`;
      const pieces = literalPieces(text);
      const beyondTag = pieces.map((p) => (TAG.test(p) ? p.replace(TAG, "").trim() : p)).join("");
      if (beyondTag.length < MIN_LITERAL) continue;
      patterns.push({ text, pieces, score: pieces.join("").length });
    }
    if (patterns.length) rows.push({ line: i + 1, verdict: verdictMatch[1], patterns });
  }
  return rows;
}

function matchesPieces(body, pieces) {
  let at = 0;
  for (const p of pieces) {
    const found = body.indexOf(p, at);
    if (found < 0) return false;
    at = found + p.length;
  }
  return true;
}

/** The best matching row for a line: { row, pattern } or null when the line is new. */
export function matchKnown(body, rows) {
  let best = null;
  for (const row of rows) {
    for (const pattern of row.patterns) {
      if (matchesPieces(body, pattern.pieces) && (!best || pattern.score > best.pattern.score)) best = { row, pattern };
    }
  }
  return best;
}

/** Group a log: { total, groups }. Groups list new before known, then by count, then by key. */
export function triageLog(logText, rows) {
  const found = collectLines(logText);
  const byKey = new Map();
  for (const l of found) {
    const key = normalizeMessage(l.message);
    let g = byKey.get(key);
    if (!g) {
      const hit = matchKnown(l.body, rows);
      g = {
        key,
        kind: l.kind,
        count: 0,
        known: !!hit,
        verdict: hit ? hit.row.verdict : "",
        row: hit ? hit.row.line : 0,
        pattern: hit ? hit.pattern.text : "",
        firstLine: l.line,
        sample: l.message,
      };
      byKey.set(key, g);
    }
    g.count++;
  }
  const groups = [...byKey.values()].sort((a, b) => Number(a.known) - Number(b.known) || b.count - a.count || a.key.localeCompare(b.key));
  return { total: found.length, groups };
}

/** Read and parse a triage table file; a missing file is an empty table. */
export function loadTriageRows(file) {
  if (!existsSync(file)) return [];
  return parseTriageTable(readFileSync(file, "utf8"));
}
