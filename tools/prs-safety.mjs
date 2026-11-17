// tools/prs-safety.mjs - the check `prs heal` runs on a merge it made, before anything is pushed.
//
// A merge driver or a regenerate step that loses a string key or a docs row damages main silently:
// the PR goes green and the loss lands. So after the merge commit, from git objects only:
//
//   string tables  every tracked *.st: the merged key set equals
//                  (main's keys + the branch's own additions) - the branch's own removals.
//                  A count mismatch and the missing or extra keys are named.
//   markdown       docs/features.md and docs/debug-log-triage.md: every line main has
//                  (that the branch did not itself remove) is still in the merged file.
//
// "Branch" is the PR head before the merge (`pre`), "main" the merged-in ref (`base`), and the
// common ancestor `mb` tells whose change a key or line is.

import { parseStringTable, coverageError } from "./merge/union-string-table.mjs";
import * as wt from "./wt.mjs";

export const MARKDOWN_ROWS = ["docs/features.md", "docs/debug-log-triage.md"];
// MARKDOWN_ROWS plus every docs/features/<area>.md the merge touched (listed from git objects by the caller).
export const isMarkdownRows = (p) => MARKDOWN_ROWS.includes(p) || (p.startsWith("docs/features/") && p.endsWith(".md"));
// Rows `tools\cli credits` regenerates from docs/credits.md; a credits regen may rename them.
export const CREDITS_KEY_PREFIX = "LF-Credits_";

function showAt(wtPath, rev, path) {
  const res = wt.run("git", ["-C", wtPath, "-c", "core.autocrlf=false", "show", `${rev}:${path}`], { maxBuffer: 256 * 1024 * 1024 });
  return res.status === 0 ? res.stdout : null;
}

function stIds(text, label, errors) {
  if (text == null) return new Set();
  const parsed = parseStringTable(text);
  const bad = coverageError(parsed, label);
  if (bad) errors.push(bad);
  return new Set(parsed.items.map((i) => i.id));
}

function lineSet(text) {
  const set = new Set();
  if (text == null) return set;
  for (const l of text.split(/\r?\n/)) if (l.trim()) set.add(l.trim());
  return set;
}

function trackedAt(wtPath, rev, pattern) {
  const res = wt.git(wtPath, ["ls-tree", "-r", "--name-only", rev]);
  return res.status === 0 ? res.stdout.split(/\r?\n/).filter((p) => pattern.test(p)) : [];
}

// Returns { ok, problems: string[] }. HEAD is the merge result.
export function safetyCheck(wtPath, { pre, base, credits = false }) {
  const problems = [];
  const mbRes = wt.git(wtPath, ["merge-base", pre, base]);
  if (mbRes.status !== 0) return { ok: false, problems: [`no merge base for ${pre.slice(0, 7)} and ${base}`] };
  const mb = mbRes.stdout.trim();
  const skip = (id) => credits && id.startsWith(CREDITS_KEY_PREFIX);

  const stFiles = new Set([...trackedAt(wtPath, base, /\.st$/), ...trackedAt(wtPath, pre, /\.st$/)]);
  for (const path of [...stFiles].sort()) {
    const errors = [];
    const mbIds = stIds(showAt(wtPath, mb, path), `${path} @merge-base`, errors);
    const mainIds = stIds(showAt(wtPath, base, path), `${path} @${base}`, errors);
    const branchIds = stIds(showAt(wtPath, pre, path), `${path} @branch`, errors);
    const mergedIds = stIds(showAt(wtPath, "HEAD", path), `${path} @merged`, errors);
    if (errors.length) { problems.push(...errors); continue; }
    const expected = new Set(mainIds);
    for (const id of branchIds) if (!mbIds.has(id)) expected.add(id); // the branch's own additions
    for (const id of mbIds) if (!branchIds.has(id)) expected.delete(id); // its own removals
    const missing = [...expected].filter((id) => !mergedIds.has(id) && !skip(id));
    const extra = [...mergedIds].filter((id) => !expected.has(id) && !skip(id));
    if (missing.length || extra.length || (!credits && expected.size !== mergedIds.size)) {
      const show = (a) => a.slice(0, 5).join(", ") + (a.length > 5 ? ` (+${a.length - 5})` : "");
      problems.push(
        `${path}: key count ${mergedIds.size}, expected ${expected.size} (main ${mainIds.size} + branch additions - branch removals)` +
          (missing.length ? `; missing ${show(missing)}` : "") + (extra.length ? `; unexpected ${show(extra)}` : ""),
      );
    }
  }

  const areaRe = /^docs\/features\/[^/]+\.md$/;
  const areaMd = [...new Set([...trackedAt(wtPath, base, areaRe), ...trackedAt(wtPath, pre, areaRe)])].sort();
  for (const path of [...MARKDOWN_ROWS, ...areaMd]) {
    const main = showAt(wtPath, base, path);
    if (main == null) continue;
    const mbLines = lineSet(showAt(wtPath, mb, path));
    const branchLines = lineSet(showAt(wtPath, pre, path));
    const merged = lineSet(showAt(wtPath, "HEAD", path));
    const lost = [...lineSet(main)].filter((l) => !(mbLines.has(l) && !branchLines.has(l)) && !merged.has(l));
    if (lost.length) problems.push(`${path}: ${lost.length} line(s) from ${base} missing after the merge, first: ${lost[0].slice(0, 100)}`);
  }
  return { ok: problems.length === 0, problems };
}
