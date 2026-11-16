// tools/lib/heal-hotspots.mjs - merge main into a branch that still edits the old central hotspots.
//
// Two shared files became per-feature files (PR "kill hotspots"):
//   - Core/LF_StaticReset.c lost its list; each class registers an LF_StaticResetHook subclass in its
//     own file (tools/lib/static-reset-hooks.mjs). A branch that still adds the interim
//     `modded class LF_StaticResetHooks` layer (#1870, retired by #2034: the chain overflowed the
//     stack) gets that layer converted to a hook class in place.
//   - docs/features.md became a generated index; the rows live in docs/features/<area>.md
//     (tools/lib/features-index.mjs).
// A branch cut before that still appends to the old files, so merging main conflicts on them. healBranch
// reads what the branch added since the merge base, merges main, takes main's side of both files and
// puts the branch's additions where they live now: one hook block per declaring file, and a 3-way
// merge of each changed features section into its area file. Other conflicts are left for the merge
// drivers and `prs heal`; anything still unmerged stops the heal with the merge in progress.
//
// `tools\cli heal-hotspots` is the command; tools/validation/heal-hotspots-selftest.mjs proves it.

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { CENTRAL, addHooks, addedCalls, centralCalls, classFiles, git, gitShow, migrateText, placeCalls } from "./static-reset-hooks.mjs";
import { mergeLines } from "../merge/merge-markdown.mjs";

export const FEATURES = "docs/features.md";

async function featuresLib() {
  try {
    return await import("./features-index.mjs");
  } catch {
    return null;
  }
}

// 3-way merge of text (ours <- base -> theirs) with the lf-markdown driver's rule: pure insertions at
// one spot keep both sides. Returns { text, conflicts }.
export function merge3(ours, base, theirs) {
  const m = mergeLines(base, ours, theirs);
  return { text: m.text, conflicts: m.ok ? 0 : m.conflicts || 1 };
}

// Sections of the old docs/features.md the branch changed since base: [{ slug, base, head }].
export function changedSections(lib, baseText, headText) {
  if (!lib || baseText == null || headText == null) return [];
  const before = new Map(lib.splitSections(baseText).map((s) => [s.slug, s.body]));
  return lib
    .splitSections(headText)
    .filter((s) => before.get(s.slug) !== s.body)
    .map((s) => ({ slug: s.slug, base: before.get(s.slug) ?? "", head: s.body }));
}

const isIndex = (text) => text != null && !/^### /m.test(text);

// What a heal would do, read from git only. { onto, base, calls, sections, needed }
export async function planHeal({ root, onto = "origin/main" }) {
  const base = git(root, ["merge-base", "HEAD", onto]).trim();
  const ontoCentral = gitShow(root, onto, CENTRAL);
  const migrated = ontoCentral != null && centralCalls(ontoCentral).length === 0;
  const calls = migrated ? addedCalls(gitShow(root, base, CENTRAL), gitShow(root, "HEAD", CENTRAL)) : [];
  const registry = ontoCentral != null && /static\s+bool\s+Register\(/.test(ontoCentral);
  const layers = registry ? moddedLayerFiles(root, base) : [];
  const lib = await featuresLib();
  const split = isIndex(gitShow(root, onto, FEATURES)) && !isIndex(gitShow(root, "HEAD", FEATURES));
  const sections = split ? changedSections(lib, gitShow(root, base, FEATURES), gitShow(root, "HEAD", FEATURES)) : [];
  return { onto, base, calls, layers, sections, migrated, split, lib, needed: calls.length > 0 || layers.length > 0 || sections.length > 0 };
}

// .c files the branch changed since base that still hold a `modded class LF_StaticResetHooks` layer.
function moddedLayerFiles(root, base) {
  const changed = git(root, ["diff", "--name-only", base, "HEAD", "--", "addons"]).split("\n").filter((f) => f.endsWith(".c"));
  return changed.filter((f) => /^modded\s+class\s+LF_StaticResetHooks\b/m.test(gitShow(root, "HEAD", f) ?? ""));
}

function unmerged(root) {
  return git(root, ["diff", "--name-only", "--diff-filter=U"]).split("\n").filter(Boolean);
}

// Merge `onto` into HEAD and move the branch's old-hotspot additions to their new homes.
// Returns { ok, plan, log: [lines], left: [paths still unmerged] }.
export async function healBranch({ root, onto = "origin/main", dryRun = false, commit = true }) {
  const log = [];
  const plan = await planHeal({ root, onto });
  for (const c of plan.calls) log.push(`static reset: ${c.cls}.${c.method}() moves to a hook in its own file`);
  for (const f of plan.layers) log.push(`static reset: ${f} has a modded LF_StaticResetHooks layer; it becomes a registry hook class`);
  for (const s of plan.sections) log.push(`features: section "${s.slug}" changes go to ${plan.lib.areaPath(s.slug)}`);
  if (!plan.needed) log.push(`nothing to heal: the branch adds no entry to ${CENTRAL}, no modded reset layer, and no rows to ${FEATURES}`);
  if (dryRun || !plan.needed) return { ok: true, plan, log, left: [] };
  if (git(root, ["status", "--porcelain", "--untracked-files=no"]).trim()) return { ok: false, plan, log: [...log, "the worktree has uncommitted changes; commit or stash them first"], left: [] };

  const r = spawnSync("git", ["merge", "--no-commit", "--no-ff", onto], { cwd: root, encoding: "utf8", windowsHide: true });
  log.push(`git merge ${onto}: ${r.status === 0 ? "clean" : "conflicts"}`);

  if (plan.calls.length || plan.migrated) {
    git(root, ["checkout", onto, "--", CENTRAL]);
    const { byFile, missing } = placeCalls(plan.calls, classFiles(root));
    for (const [f, cs] of byFile) {
      const p = join(root, f);
      writeFileSync(p, addHooks(readFileSync(p, "utf8"), cs));
      git(root, ["add", "--", f]);
      log.push(`hooked ${cs.map((c) => c.cls).join(", ")} in ${f}`);
    }
    for (const c of missing) log.push(`WARN no file declares class ${c.cls}; add its hook by hand`);
    git(root, ["add", "--", CENTRAL]);
  }

  for (const f of plan.layers) {
    const p = join(root, f);
    if (!existsSync(p)) continue;
    const text = readFileSync(p, "utf8");
    if (/^(<<<<<<<|>>>>>>>)/m.test(text)) {
      log.push(`WARN ${f} has conflict markers; convert its reset layer after resolving (node tools/codemods/static-reset-registry.mjs --no-central)`);
      continue;
    }
    const r = migrateText(text);
    writeFileSync(p, r.text);
    git(root, ["add", "--", f]);
    log.push(`converted the reset layer in ${f} (${r.calls.map((c) => c.cls).join(", ")})`);
  }

  if (plan.split) {
    git(root, ["checkout", onto, "--", FEATURES]);
    for (const s of plan.sections) {
      const rel = plan.lib.areaPath(s.slug);
      const p = join(root, rel);
      const ours = existsSync(p) ? readFileSync(p, "utf8") : "";
      const m = ours ? merge3(ours, s.base, s.head) : { text: s.head, conflicts: 0 };
      mkdirSync(dirname(p), { recursive: true });
      writeFileSync(p, m.text);
      if (m.conflicts) log.push(`CONFLICT ${rel}: ${m.conflicts} hunk(s) both sides changed; resolve by hand`);
      else git(root, ["add", "--", rel]);
    }
    if (plan.lib.renderIndex) {
      writeFileSync(join(root, FEATURES), plan.lib.renderIndex(root));
    }
    git(root, ["add", "--", FEATURES]);
  }

  const left = unmerged(root);
  if (left.length) {
    log.push(`still unmerged (merge left in progress): ${left.join(", ")}`);
    return { ok: false, plan, log, left };
  }
  if (commit) {
    git(root, ["commit", "-q", "-m", `Merge ${onto}; heal-hotspots moved StaticReset entries and features rows to per-feature files`]);
    log.push(`committed the merge (${git(root, ["rev-parse", "--short", "HEAD"]).trim()})`);
  }
  return { ok: true, plan, log, left: [] };
}
