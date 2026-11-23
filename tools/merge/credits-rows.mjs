// The credits rows builder: docs/credits.md -> the LF-Credits_* string rows and LF_CreditsData.c (#481).
// Lives in tools/merge so the merge drivers can regenerate both from a merged docs/credits.md (#4668): the rows
// are numbered in source order, so two branches that each add a credits entry write the same Id with different
// text, and neither a line merge nor the string-table union can resolve that. tools/commands/credits.mjs is the
// CLI around it. Imports stay inside tools/merge (run-driver.mjs extracts only that folder).
import { createHash } from "node:crypto";
import { mergeLines } from "./merge-markdown.mjs";
import { resolveMergeSides, showAt } from "./merge-sides.mjs";
import { canonicalStringTable } from "./sort-string-table.mjs";

export const SOURCE = "docs/credits.md";
export const ST = "addons/LifeFramework/Language/everonlife_localization.st";
export const DATA = "addons/LifeFramework/Scripts/Game/Feature/Credits/LF_CreditsData.c";
const PREFIX = "LF-Credits_";
const MAX_LABEL = 200;

function plain(text) {
  return text
    .replace(/\[([^\]]+)\]\([^)]*\)/g, "$1")
    .replace(/`([^`]*)`/g, "$1")
    .replace(/\*\*([^*]+)\*\*/g, "$1")
    .replace(/\s+/g, " ")
    .trim();
}

function clip(text) {
  if (text.length <= MAX_LABEL) return text;
  const cut = text.slice(0, MAX_LABEL).replace(/\s+\S*$/, "");
  return `${cut}...`;
}

function cells(line) {
  return line.trim().replace(/^\||\|$/g, "").split("|").map((c) => c.trim());
}

// -> [{ heading, rows: [{ label, value }] }]
export function parse(md) {
  const sections = [];
  let cur = null;
  let inCode = false;
  let header = null;
  let paragraph = [];
  let lastBullet = false;
  const flush = () => {
    if (cur && paragraph.length) cur.paragraphs.push(plain(paragraph.join(" ")));
    paragraph = [];
  };
  for (const raw of md.split("\n")) {
    if (raw.trim().startsWith("```")) { inCode = !inCode; continue; }
    if (inCode) continue;
    const h = /^## (.+)$/.exec(raw);
    if (h) {
      flush();
      cur = { heading: plain(h[1]), rows: [], paragraphs: [] };
      sections.push(cur);
      header = null;
      continue;
    }
    if (!cur) continue;
    if (raw.trim().startsWith("|")) {
      flush();
      const c = cells(raw);
      if (c.every((x) => /^:?-+:?$/.test(x))) continue;
      if (!header) { header = c.map((x) => x.toLowerCase()); continue; }
      const label = clip(plain(c[0]).replace(/\s*\(.*$/, ""));
      const value = [];
      header.forEach((name, i) => {
        if (i === 0 || !c[i]) return;
        if (name === "author" || name === "licence" || name === "tool") value.push(plain(c[i]));
      });
      const joined = clip(value.join(", "));
      if (label) cur.rows.push({ label, value: joined === label ? "" : joined });
      continue;
    }
    header = null;
    const b = /^- (.+)$/.exec(raw);
    if (b) { flush(); cur.rows.push({ label: plain(b[1]), value: "" }); lastBullet = true; continue; }
    if (lastBullet && /^\s+\S/.test(raw)) { const r = cur.rows[cur.rows.length - 1]; r.label = plain(`${r.label} ${raw}`); continue; }
    lastBullet = false;
    if (raw.trim() === "") { flush(); continue; }
    paragraph.push(raw.trim());
  }
  flush();
  for (const s of sections) {
    for (const r of s.rows) r.label = clip(r.label);
    if (!s.rows.length && s.paragraphs.length) {
      const first = s.paragraphs[0];
      s.rows.push({ label: clip(first.split(/(?<=\.)\s/)[0]), value: "" });
    }
  }
  return sections.filter((s) => s.rows.length);
}

// -> [{ kind: "H"|"R", label: {key,text}, value?: {key,text} }] in screen order, with keys numbered.
export function entries(md) {
  const out = [];
  let n = 0;
  for (const s of parse(md)) {
    out.push({ kind: "H", label: { key: `${PREFIX}${++n}`, text: s.heading.toUpperCase() } });
    for (const r of s.rows) {
      const e = { kind: "R", label: { key: `${PREFIX}${++n}`, text: r.label } };
      if (r.value) e.value = { key: `${PREFIX}${++n}`, text: r.value };
      out.push(e);
    }
  }
  return out;
}

const FIXED = [
  ["Title", "CREDITS", "Heading of the in-game credits screen (#481)."],
  ["Intro", "Who Life Framework is built on.", "Line under the credits title (#481)."],
  ["Page", "Page %1 of %2", "Page counter of the credits screen (#481)."],
  ["Next", "NEXT", "Next-page button of the credits screen (#481)."],
  ["Prev", "PREVIOUS", "Previous-page button of the credits screen (#481)."],
  ["Close", "CLOSE", "Close button of the credits screen (#481)."],
];

function guid(key) {
  return createHash("sha256").update(key).digest("hex").slice(0, 16).toUpperCase();
}

function stItem(key, text, comment) {
  const t = text.replace(/\\/g, "\\\\").replace(/"/g, '\\"');
  return [`    CustomStringTableItem "{${guid(key)}}" {`, `     Id "${key}"`, `     Target_en_us "${t}"`, `     Comment "${comment}"`, "    }"].join("\n");
}

export function stBlock(list) {
  const rows = FIXED.map(([k, t, c]) => stItem(`${PREFIX}${k}`, t, c));
  for (const e of list) {
    rows.push(stItem(e.label.key, e.label.text, `Generated from ${SOURCE} by tools\\cli credits (#481)`));
    if (e.value) rows.push(stItem(e.value.key, e.value.text, `Generated from ${SOURCE} by tools\\cli credits (#481)`));
  }
  return rows.join("\n");
}

const CREDITS_ROW = /\n    CustomStringTableItem "\{[0-9A-F]{16}\}" \{\n     Id "LF-Credits_[^"]*"\n(?:     [^\n]*\n)*?    \}/g;

// Every generated credits row of a string table, joined: a branch changed them when this differs from the base's.
export function creditsRowsText(st) {
  return (st.match(CREDITS_ROW) || []).join("");
}

// The table without any LF-Credits_ row (the merge driver unions the rest, then regenerates these).
export function stripCreditsRows(st) {
  return st.replace(CREDITS_ROW, "");
}

// docs/credits.md merged three ways like any markdown list: both sides' new rows stay. { ok, text }; not ok on a real conflict.
export function mergeCreditsMd(base, ours, theirs) {
  if (ours == null || theirs == null) return { ok: false, text: ours ?? theirs ?? "" };
  if (ours === theirs) return { ok: true, text: ours };
  if (base != null && ours === base) return { ok: true, text: theirs };
  if (base != null && theirs === base) return { ok: true, text: ours };
  const merged = mergeLines(base ?? "", ours, theirs);
  return { ok: merged.ok, text: merged.text };
}

// The merged docs/credits.md of the merge a driver runs in: { ok, text }, or null when the sides cannot be found.
export function mergedCreditsFromGit() {
  const sides = resolveMergeSides();
  if (!sides) return null;
  return mergeCreditsMd(showAt(sides.base, SOURCE), showAt(sides.ours, SOURCE), showAt(sides.theirs, SOURCE));
}

// The table with every LF-Credits_ row replaced by the block, sorted by Id.
export function patchSt(st, list) {
  const stripped = stripCreditsRows(st);
  // Rows go inside the last group, before its closing brace, so the table round-trips through the merge driver.
  let end = stripped.lastIndexOf("\n  }\n }\n}");
  if (end === -1) end = stripped.lastIndexOf("\n }\n}");
  if (end === -1) throw new Error(`${ST}: closing braces not found`);
  const patched = `${stripped.slice(0, end)}\n${stBlock(list)}${stripped.slice(end)}`;
  // The table is kept sorted by Id (tools/merge/sort-string-table.mjs); one that does not parse
  // whole stays as patched and regen-localization reports it.
  const sorted = canonicalStringTable(patched);
  return sorted.ok ? sorted.text : patched;
}

export function dataFile(list) {
  const lines = [];
  for (const e of list) lines.push(`\t\trows.Insert("${e.kind}|#${e.label.key}|${e.value ? `#${e.value.key}` : ""}");`);
  return `// GENERATED by tools\\cli credits from ${SOURCE} (#481). Do not edit; edit the source and run it again.
// Each row is "H|<label key>|" for a section heading or "R|<label key>|<value key>" for an entry.

class LF_CreditsData
{
	//------------------------------------------------------------------------------------------------
	static array<string> GetRows()
	{
		array<string> rows = {};
${lines.join("\n")}
		return rows;
	}
}
`;
}
