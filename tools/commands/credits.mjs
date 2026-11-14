// tools\cli credits [--check]: build the in-game credits screen's data from docs/credits.md (#481).
//
// docs/credits.md is the single source of the credits (the website /credits page and the README read it
// too). This writes two generated outputs, so the game cannot drift from it:
//   addons/LifeFramework/Language/everonlife_localization.st   the LF-Credits_* rows (then regen-localization)
//   addons/LifeFramework/Scripts/Game/Feature/Credits/LF_CreditsData.c   the ordered row list of string keys
// `--check` writes nothing and exits 1 when either differs from what the file would produce
// (validate-credits.mjs runs it). Rules: a `##` heading is a heading row; a table row is its first cell
// (label) and its Author, Licence or Tool cells (value); a bullet is a label; a paragraph counts only in a
// section with no table or bullet. Links, backticks and bold are stripped; a `(...)` after a table label is
// dropped (it is a repo path). Code blocks are skipped.
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

export const help = "credits [--check]   generate the in-game credits rows and strings from docs/credits.md";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
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

// The table with every LF-Credits_ row replaced by the block (appended before the closing braces).
export function patchSt(st, list) {
  const items = /\n    CustomStringTableItem "\{[0-9A-F]{16}\}" \{\n     Id "LF-Credits_[^"]*"\n(?:     [^\n]*\n)*?    \}/g;
  const stripped = st.replace(items, "");
  const end = stripped.lastIndexOf("\n }\n}");
  if (end === -1) throw new Error(`${ST}: closing braces not found`);
  return `${stripped.slice(0, end)}\n${stBlock(list)}${stripped.slice(end)}`;
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

function read(p) {
  return existsSync(join(ROOT, p)) ? readFileSync(join(ROOT, p), "utf8") : "";
}

export function expected() {
  const list = entries(read(SOURCE));
  return { list, st: patchSt(read(ST), list), data: dataFile(list) };
}

export async function main(args) {
  const { list, st, data } = expected();
  if (!list.length) { console.log(`credits: ${SOURCE} gave no rows`); return 1; }
  const stOk = st === read(ST);
  const dataOk = data === read(DATA);
  if (args.includes("--check")) {
    if (stOk && dataOk) { console.log(`credits: ${list.length} rows in sync with ${SOURCE}`); return 0; }
    console.log(`credits: ${[!stOk && ST, !dataOk && DATA].filter(Boolean).join(" and ")} out of date with ${SOURCE}; run tools\\cli credits and tools\\cli regen-localization`);
    return 1;
  }
  mkdirSync(dirname(join(ROOT, DATA)), { recursive: true });
  writeFileSync(join(ROOT, ST), st, "utf8");
  writeFileSync(join(ROOT, DATA), data, "utf8");
  console.log(`credits: wrote ${list.length} rows from ${SOURCE}; now run tools\\cli regen-localization`);
  return 0;
}
