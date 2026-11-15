// Does main's movement overlap a PR that already passed its gate? (docs/worktrees.md "Async ship", "Re-gate only
// on overlap".) The ship queue gates a PR once; when main moves before the merge (the post-gate fetch, or GitHub's
// "Base branch was modified" on auto-merge) it gates again only if the new main commits touch the PR's files or
// something that depends on them. Everything else is merged as gated; the main guard (tools/main-guard.mjs) and
// the PR's own `validate` check catch what this misses. Conservative on purpose: a doubt is an overlap.
//
// Overlap, each with a reason:
//   same file        a file both sides changed (driver-merged files and docs/**/*.md left out, as ship-queue overlap())
//   generated        both sides changed the same generated file (test registry, language conf, string table)
//   feature area     an addons/ file in the same Feature/<Area>, Prefabs/<Area> or Configs/<Area> folder
//   Core             main changed Scripts/Game/Core and the PR changes a script, or the other way round
//   GUID             a {GUID} one side references is the .meta GUID of a file the other side changed
//   name             a class, or a file stem, one side declares appears as a whole word in the other side's text
//   unreadable / too many   a text file that cannot be read, or more files than a gate-time comparison can read
//
// Pure: file texts come from readPr(path) and readMain(path, "after"|"before"). Proven by
// tools/validation/regate-overlap-selftest.mjs.

import { conflictKind } from "../prs.mjs";
import { docsText, driverMerged } from "./ship-queue.mjs";
import { fileTokens } from "./gate-tier.mjs";

const norm = (p) => String(p).replace(/\\/g, "/");
const TEXT = /\.(c|et|conf|layer|layout|st|ent|json|mjs|js|ts|tsx|ps1|cmd|gproj|yml|yaml)$/i;
const GUID_G = /\{([0-9A-Fa-f]{16})\}/g;
const AREA = /^addons\/[^/]+\/(?:Scripts\/Game\/Feature|Prefabs|Configs)\/([^/]+)\//;
const CORE = /^addons\/[^/]+\/Scripts\/Game\/Core\//;
export const MAX_READS = 400; // files read to compare, both sides together; past this a gate-time comparison costs more than it saves

const skip = (p) => driverMerged(p) || docsText(p);
const script = (p) => p.endsWith(".c");
const isMeta = (p) => p.endsWith(".meta");

function guidsIn(text) {
  return [...String(text ?? "").matchAll(GUID_G)].map((m) => m[1].toUpperCase());
}

// The class, enum and typedef names a script declares.
function declared(text) {
  return [...String(text ?? "").matchAll(/^\s*(?:modded\s+|sealed\s+)*(?:class|enum|typedef)\s+(\w+)/gm)].map((m) => m[1]);
}

function wordRe(tokens) {
  const list = [...tokens].filter(Boolean).map((t) => t.replace(/[.*+?^${}()|[\]\\]/g, "\\$&"));
  if (!list.length) return null;
  return new RegExp(`(?<![\\w-])(?:${list.join("|")})(?![\\w-])`, "g");
}

// The first of `tokens` (a Map token -> path) found as a whole word in `text`, or null.
function firstHit(re, text) {
  if (!re || typeof text !== "string") return null;
  re.lastIndex = 0;
  const m = re.exec(text);
  return m ? m[0] : null;
}

export function dependentOverlap({ prFiles = [], mainFiles = [], readPr, readMain, maxReads = MAX_READS }) {
  const reasons = [];
  const add = (r) => {
    if (!reasons.includes(r)) reasons.push(r);
  };
  const pr = [...new Set(prFiles.map(norm))];
  const main = [...new Set(mainFiles.map(norm))];
  if (!main.length) return { overlap: false, reasons };

  // same file / generated: no text needed
  const prSet = new Set(pr.filter((p) => !skip(p)));
  for (const p of main) if (prSet.has(p)) add(`same file ${p}`);
  const prKinds = new Set(pr.map(conflictKind).filter((k) => k && /^(regen-|string-table)/.test(k)));
  for (const p of main) {
    const k = conflictKind(p);
    if (k && prKinds.has(k)) add(`both change the generated ${k} file (${p.split("/").pop()})`);
  }

  // folders: feature area and Core (an addons/ file; driver-merged and docs left out)
  const prGame = pr.filter((p) => p.startsWith("addons/") && !skip(p));
  const mainGame = main.filter((p) => p.startsWith("addons/") && !skip(p));
  const prAreas = new Map(prGame.map((p) => [AREA.exec(p)?.[1], p]).filter(([a]) => a));
  for (const p of mainGame) {
    const a = AREA.exec(p)?.[1];
    if (a && prAreas.has(a) && !prSet.has(p)) add(`same feature area ${a} (${p.split("/").pop()} on main, ${prAreas.get(a).split("/").pop()} in the PR)`);
  }
  const prScript = prGame.some(script);
  const mainScript = mainGame.some(script);
  const mainCore = mainGame.find((p) => CORE.test(p) && script(p));
  const prCore = prGame.find((p) => CORE.test(p) && script(p));
  if (mainCore && prScript) add(`main changed shared Core (${mainCore.split("/").pop()}) and the PR changes a script`);
  if (prCore && mainScript) add(`the PR changes shared Core (${prCore.split("/").pop()}) and main changed a script`);

  // text: GUID references and names. Read each side once.
  const reads = pr.length + main.length;
  if (reads > maxReads) {
    add(`too many files to compare (${pr.length} in the PR, ${main.length} on main)`);
    return { overlap: true, reasons };
  }
  const scan = (files, read, side) => {
    const out = { refs: new Map(), owned: new Map(), names: new Map(), text: [] };
    for (const p of files) {
      if (skip(p) || p.endsWith(".generated.c") || conflictKind(p)) continue;
      // The text of a file; a deleted one is read from before (main side only).
      const get = (path) => (side === "main" ? read(path, "after") ?? read(path, "before") : read(path));
      if (isMeta(p)) {
        const t = get(p);
        for (const g of guidsIn(t).slice(0, 1)) out.owned.set(g, p);
        continue;
      }
      // the sibling .meta of any changed file names the GUID of the resource
      const metaText = get(`${p}.meta`);
      for (const g of guidsIn(metaText).slice(0, 1)) out.owned.set(g, p);
      if (!TEXT.test(p)) continue;
      const t = get(p);
      if (typeof t !== "string") {
        // a text file that is not there on either side of a deletion, or could not be read
        if (p.startsWith("addons/") || script(p)) add(`unreadable ${p} (${side}); compared as an overlap`);
        continue;
      }
      out.text.push([p, t]);
      for (const g of guidsIn(t)) if (!out.refs.has(g)) out.refs.set(g, p);
      const names = new Set(fileTokens(p, t, metaText));
      if (script(p)) for (const n of declared(t)) names.add(n);
      for (const n of names) if (n.length >= 4 && !out.names.has(n)) out.names.set(n, p);
    }
    return out;
  };
  const a = scan(pr, readPr, "pr");
  const b = scan(main, readMain, "main");

  for (const [g, f] of a.refs) if (b.owned.has(g)) add(`the PR (${f.split("/").pop()}) references {${g}}, the GUID of ${b.owned.get(g).split("/").pop()} that main changed`);
  for (const [g, f] of b.refs) if (a.owned.has(g)) add(`main (${f.split("/").pop()}) references {${g}}, the GUID of ${a.owned.get(g).split("/").pop()} that the PR changes`);

  // names: a declared class or a file stem of one side as a whole word in the other side's text
  const reA = wordRe(a.names.keys());
  const reB = wordRe(b.names.keys());
  for (const [p, t] of b.text) {
    const hit = firstHit(reA, t);
    if (hit) add(`main (${p.split("/").pop()}) names ${hit}, which the PR changes (${a.names.get(hit)?.split("/").pop()})`);
  }
  for (const [p, t] of a.text) {
    const hit = firstHit(reB, t);
    if (hit) add(`the PR (${p.split("/").pop()}) names ${hit}, which main changed (${b.names.get(hit)?.split("/").pop()})`);
  }

  return { overlap: reasons.length > 0, reasons };
}
