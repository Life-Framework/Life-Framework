// Git merge driver for docs/features.md.
// Keeps feature rows added on either side. The same heading with different
// body text keeps both copies so a rebase does not stop for a hand merge.
//
//   node tools/merge/union-features.mjs %O %A %B %P

import { readFileSync, writeFileSync } from "node:fs";

function parseFeatures(text) {
  const lines = text.split("\n");
  const sections = [];
  const preamble = [];
  let current = null;
  let row = null;
  const flushRow = () => {
    if (!current || !row) return;
    current.rows.push(row);
    row = null;
  };
  const startSection = (heading) => {
    flushRow();
    current = { heading, intro: [], rows: [] };
    sections.push(current);
  };
  for (const line of lines) {
    if (/^##($| [^#])/.test(line)) {
      startSection(line);
      continue;
    }
    if (line.startsWith("### ")) {
      if (!current) startSection("");
      flushRow();
      row = { heading: line, body: [] };
      continue;
    }
    if (!current) preamble.push(line);
    else if (!row) current.intro.push(line);
    else row.body.push(line);
  }
  flushRow();
  return { preamble, sections };
}

function rowText(row) {
  if (!row) return null;
  return `${row.heading}\n${row.body.join("\n")}`;
}

function unionText(base, ours, theirs) {
  if (ours === theirs) return ours ?? "";
  if (ours === base) return theirs ?? "";
  if (theirs === base) return ours ?? "";
  const oursLines = (ours ?? "").split("\n");
  const have = new Set(oursLines);
  const extra = (theirs ?? "").split("\n").filter((line) => !have.has(line));
  return oursLines.concat(extra).join("\n");
}

function mergeRows(baseRows, oursRows, theirsRows) {
  const order = [];
  const seen = new Set();
  for (const list of [baseRows, oursRows, theirsRows]) {
    for (const row of list) {
      if (seen.has(row.heading)) continue;
      seen.add(row.heading);
      order.push(row.heading);
    }
  }
  const baseMap = new Map(baseRows.map((row) => [row.heading, row]));
  const oursMap = new Map(oursRows.map((row) => [row.heading, row]));
  const theirsMap = new Map(theirsRows.map((row) => [row.heading, row]));
  const out = [];
  for (const heading of order) {
    const baseRow = baseMap.get(heading) || null;
    const oursRow = oursMap.get(heading) || null;
    const theirsRow = theirsMap.get(heading) || null;
    const baseBody = rowText(baseRow);
    const oursBody = rowText(oursRow);
    const theirsBody = rowText(theirsRow);
    if (oursBody === theirsBody) {
      if (oursRow || theirsRow) out.push(oursRow || theirsRow);
      continue;
    }
    if (oursBody === baseBody) {
      if (theirsRow) out.push(theirsRow);
      continue;
    }
    if (theirsBody === baseBody) {
      if (oursRow) out.push(oursRow);
      continue;
    }
    if (oursRow) out.push(oursRow);
    if (theirsRow) out.push(theirsRow);
  }
  return out;
}

function mergeSections(baseSections, oursSections, theirsSections) {
  const order = [];
  const seen = new Set();
  for (const list of [baseSections, oursSections, theirsSections]) {
    for (const section of list) {
      if (seen.has(section.heading)) continue;
      seen.add(section.heading);
      order.push(section.heading);
    }
  }
  const baseMap = new Map(baseSections.map((section) => [section.heading, section]));
  const oursMap = new Map(oursSections.map((section) => [section.heading, section]));
  const theirsMap = new Map(theirsSections.map((section) => [section.heading, section]));
  const out = [];
  for (const heading of order) {
    const baseSection = baseMap.get(heading) || null;
    const oursSection = oursMap.get(heading) || null;
    const theirsSection = theirsMap.get(heading) || null;
    if (!oursSection && !theirsSection) continue;
    const intro = unionText(
      textOf(baseSection ? baseSection.intro : null),
      textOf(oursSection ? oursSection.intro : null),
      textOf(theirsSection ? theirsSection.intro : null),
    );
    out.push({
      heading,
      intro: linesOf(intro),
      rows: mergeRows(
        baseSection ? baseSection.rows : [],
        oursSection ? oursSection.rows : [],
        theirsSection ? theirsSection.rows : [],
      ),
    });
  }
  return out;
}

function linesOf(text) {
  if (text == null) return [];
  return text.split("\n");
}

function textOf(lines) {
  if (!lines || lines.length === 0) return null;
  return lines.join("\n");
}

function emit(doc, crlf) {
  const lines = [...doc.preamble];
  for (const section of doc.sections) {
    if (section.heading) lines.push(section.heading);
    lines.push(...section.intro);
    for (const row of section.rows) {
      lines.push(row.heading);
      lines.push(...row.body);
    }
  }
  let text = lines.join("\n");
  if (crlf) text = text.replace(/\n/g, "\r\n");
  return text;
}

export function mergeFeatures(baseText, oursText, theirsText) {
  const crlf = (oursText ?? "").includes("\r\n") || (theirsText ?? "").includes("\r\n") || (baseText ?? "").includes("\r\n");
  const norm = (text) => (text ?? "").replace(/\r\n/g, "\n");
  const base = parseFeatures(norm(baseText));
  const ours = parseFeatures(norm(oursText));
  const theirs = parseFeatures(norm(theirsText));
  const preamble = unionText(textOf(base.preamble), textOf(ours.preamble), textOf(theirs.preamble));
  const text = emit(
    {
      preamble: linesOf(preamble),
      sections: mergeSections(base.sections, ours.sections, theirs.sections),
    },
    crlf,
  );
  return { ok: true, text };
}

function runDriver() {
  const basePath = process.argv[2];
  const oursPath = process.argv[3];
  const theirsPath = process.argv[4];
  if (!basePath || !oursPath || !theirsPath) {
    console.error("usage: union-features.mjs <base> <ours> <theirs> [path]");
    process.exit(2);
  }
  const merged = mergeFeatures(
    readFileSync(basePath, "utf8"),
    readFileSync(oursPath, "utf8"),
    readFileSync(theirsPath, "utf8"),
  );
  writeFileSync(oursPath, merged.text);
}

const invoked = process.argv[1] && process.argv[1].replace(/\\/g, "/").includes("union-features.mjs");
if (invoked) runDriver();
