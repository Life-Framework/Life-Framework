// tools\cli world: check and ship the maintainer's hand edits of DebugWorld (docs/worktrees.md "Maintainer world edits").
//
//   tools\cli world diff [--wt <slug>] [--json]          what the .layer edits changed against the merge base: MOVED,
//                                                        ADDED, REMOVED, PROP CHANGED, and the NOISE Workbench writes
//   tools\cli world ship [--wt <slug>] [--dry-run] [--author "Name <mail>"] [--yes]
//                                                        revert noise-only layers, commit, fast checks, allowlist the
//                                                        maintainer's flagged placements, then `wt ship` (push, PR, queue)
//
// --wt defaults to the worktree the command runs in, else town-maintainer-edits. The parsing is
// tools/lib/world-diff.mjs; the fast gate tier for such a branch (T1w) is tools/lib/gate-tier.mjs.
import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import * as wd from "../lib/world-diff.mjs";
import { selectTests } from "../lib/gate-tier.mjs";
import { layoutWarnings, MAINTAINER_BRANCH } from "../lib/world-checks.mjs";
import { judgeWorldTests, testWarningLines, TEST_WARNINGS_HEADING } from "../lib/world-test-judge.mjs";

export const help = "world diff|ship [--wt <slug>] [--json|--dry-run]   semantic diff of .layer edits / commit, check and ship a maintainer world edit";

export const DEFAULT_WT = "town-maintainer-edits";
export const ALLOW_REASON = "maintainer hand placement (world ship)";
const ALLOW_FILE = "agent-box/place-audit-allow.json";

function opt(args, name) {
  const i = args.indexOf(name);
  return i >= 0 ? args[i + 1] : undefined;
}

function git(cwd, args, input) {
  const r = spawnSync("git", ["-C", cwd, ...args], { encoding: "utf8", windowsHide: true, maxBuffer: 256 * 1024 * 1024, input });
  return { status: r.status, stdout: r.stdout ?? "", stderr: r.stderr ?? "" };
}

// The worktree to work on: --wt <slug> through the worktree state, else this checkout when it is a ws/ worktree.
async function resolveWt(args, root) {
  const slug = opt(args, "--wt");
  if (!slug) {
    const top = git(process.cwd(), ["rev-parse", "--show-toplevel"]).stdout.trim();
    const br = top ? git(top, ["rev-parse", "--abbrev-ref", "HEAD"]).stdout.trim() : "";
    if (top && br.startsWith("ws/")) return { slug: br.slice(3), path: resolve(top), branch: br };
  }
  const s = slug || DEFAULT_WT;
  const wt = await import("../wt.mjs");
  const e = wt.requireWorktree(root, s);
  return { slug: s, path: e.path, branch: git(e.path, ["rev-parse", "--abbrev-ref", "HEAD"]).stdout.trim(), base: e.base };
}

// The .layer files changed against the merge base with origin/<base>, uncommitted and untracked included.
export function collectLayers(path, base = "main") {
  const mb = git(path, ["merge-base", `origin/${base}`, "HEAD"]);
  if (mb.status !== 0) throw new Error(`no merge base with origin/${base}: ${mb.stderr.trim()}`);
  const ref = mb.stdout.trim();
  const ns = git(path, ["diff", "--name-status", "--no-renames", ref, "--", "*.layer"]);
  const un = git(path, ["ls-files", "--others", "--exclude-standard", "--", "*.layer"]);
  const files = [];
  for (const line of ns.stdout.split(/\r?\n/).filter(Boolean)) {
    const [st, p] = line.split("\t");
    const before = st === "A" ? null : git(path, ["show", `${ref}:${p}`]).stdout;
    let after = null;
    try { after = readFileSync(join(path, p), "utf8"); } catch { /* deleted */ }
    files.push({ path: p, before, after });
  }
  for (const p of un.stdout.split(/\r?\n/).filter(Boolean)) files.push({ path: p, before: null, after: readFileSync(join(path, p), "utf8") });
  // Other addons/ changes beside the layers: a world ship refuses them (they need their own PR and gate).
  const all = git(path, ["diff", "--name-only", ref, "--", "addons"]).stdout.split(/\r?\n/).filter(Boolean);
  const others = [...all, ...git(path, ["ls-files", "--others", "--exclude-standard", "--", "addons"]).stdout.split(/\r?\n/).filter(Boolean)].filter((p) => !p.endsWith(".layer"));
  return { ref, files, others };
}

export function printDiff(d, log = console.log) {
  for (const f of d.files) {
    if (!f.changes.length && !f.noise.length) continue;
    log(`${f.path}${f.noiseOnly ? "  (NOISE only)" : ""}`);
    for (const c of f.changes) log(`  ${wd.changeLine(c)}`);
    for (const n of f.noise) log(`  NOISE ${n}`);
  }
}

// Workbench unsaved state. Workbench keeps no lock or temp file per unsaved world edit that we know of; its
// window title carries a `*` while the world has unsaved changes. Returns { refuse, warn }.
export function workbenchState(tasklistCsv) {
  const rows = String(tasklistCsv || "").split(/\r?\n/).filter((l) => /workbench/i.test(l));
  if (!rows.length) return { refuse: null, warn: null };
  const titles = rows.map((r) => r.split('","').pop().replace(/"\s*$/, ""));
  const dirty = titles.find((t) => /\*/.test(t));
  if (dirty) return { refuse: `Workbench shows unsaved changes (window title "${dirty}"). Save the world in Workbench (Ctrl+S), then run world ship again.`, warn: null };
  return { refuse: null, warn: "Workbench is running: make sure the world is saved (Ctrl+S) before shipping; a later save rewrites the layers." };
}

function workbenchCheck() {
  if (process.platform !== "win32") return { refuse: null, warn: null };
  const r = spawnSync("tasklist", ["/v", "/fo", "csv"], { encoding: "utf8", windowsHide: true, timeout: 15000 });
  return workbenchState(r.stdout);
}

function escapeRe(s) {
  return s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

// Allowlist entries for flagged rows of the audit report that the maintainer's edit touched.
export function allowEntries(report, touched, existing = []) {
  const out = [];
  const rows = (report?.ranked ?? []).filter((r) => r.problems?.length);
  for (const t of touched) {
    const short = t.named ? t.name : (t.prefab || "").split("/").pop().replace(/\.et$/, "");
    for (const r of rows.filter((x) => x.name === short)) {
      for (const p of r.problems) {
        const match = t.named ? `^${escapeRe(t.name)} ` : `^ ${escapeRe(t.prefab)}$`;
        if ([...existing, ...out].some((a) => a.match === match && a.kind === p.kind)) continue;
        out.push({ match, kind: p.kind, why: `${ALLOW_REASON}: ${t.label} in ${t.layer} (${p.text})` });
      }
    }
  }
  return out;
}

export function prBody(d, { slug, allowed = [], checks = [], warnings = [], testWarnings = [] }) {
  const L = [];
  L.push(`Maintainer world edit from \`${slug}\`, shipped with \`tools\\cli world ship\`.`, "", `**${wd.summaryLine(d)}**`, "", "## Changes", "");
  for (const f of d.files.filter((x) => !x.noiseOnly)) {
    L.push(`- \`${f.path}\``);
    for (const c of f.changes) L.push(`  - ${wd.changeLine(c)}`);
    for (const n of f.noise) L.push(`  - noise: ${n}`);
  }
  const reverted = d.files.filter((x) => x.noiseOnly);
  if (reverted.length) L.push("", `Reverted as noise only: ${reverted.map((f) => `\`${f.path}\` (${f.noise.join("; ")})`).join(", ")}.`);
  if (allowed.length) {
    L.push("", "## Placements allowlisted", "", `The placement audit flagged these; the maintainer placed them by hand, so they went to \`${ALLOW_FILE}\` with the reason "${ALLOW_REASON}":`, "");
    for (const a of allowed) L.push(`- \`${a.match}\` ${a.kind}: ${a.why}`);
  }
  if (warnings.length) {
    L.push("", "## Layout warnings", "", "These judge where things stand, not whether the layer is sound, so they did not block this PR (tools/lib/world-checks.mjs). Look at them when you play the world:", "");
    for (const w of warnings) L.push(`- ${w}`);
  }
  if (testWarnings.length) {
    L.push("", TEST_WARNINGS_HEADING, "", "These tests failed on the new layout. A failing test never blocks a maintainer world edit, so the PR still merges; each one is either a test that pinned the old layout (fix the test) or a real gameplay break (fix the game):", "");
    for (const t of testWarnings) L.push(`- ${t}`);
  }
  if (checks.length) L.push("", "## Checks", "", ...checks.map((c) => `- ${c}`));
  L.push("", "No screenshots: the maintainer placed and looked at these entities in Workbench while editing.", "");
  L.push("## Manual testing", "", "1. Host DebugWorld from `main` after the merge.", "2. Walk to each entity listed under Changes and check it stands where the maintainer put it.", "");
  L.push("🤖 Generated with [Claude Code](https://claude.com/claude-code)", "");
  return L.join("\n");
}

function cli(cwd, args, extraEnv = {}) {
  const r = spawnSync(process.execPath, [join(cwd, "tools", "cli.mjs"), ...args], { cwd, encoding: "utf8", windowsHide: true, env: { ...process.env, ...extraEnv }, maxBuffer: 256 * 1024 * 1024, timeout: 590000 });
  return { status: r.status ?? 1, out: `${r.stdout ?? ""}${r.stderr ?? ""}` };
}

function lastSummary(text) {
  return String(text).split(/\r?\n/).filter((l) => /^SUMMARY /.test(l)).pop() || "";
}

async function cmdDiff(args, root) {
  const t0 = Date.now();
  const w = await resolveWt(args, root);
  const { files, others } = collectLayers(w.path, w.base || "main");
  const d = wd.diffFiles(files);
  if (args.includes("--json")) {
    console.log(JSON.stringify({ worktree: w.slug, branch: w.branch, summary: wd.summaryLine(d), others, ...d }, null, 2));
    return 0;
  }
  printDiff(d);
  if (others.length) console.log(`other addons/ changes (not layers): ${others.slice(0, 5).join(", ")}${others.length > 5 ? ` +${others.length - 5}` : ""}`);
  console.log(`world diff ${w.slug}: ${wd.summaryLine(d)} (${files.length} layer file(s), ${((Date.now() - t0) / 1000).toFixed(1)} s)`);
  return d.totals.unparsed ? 1 : 0;
}

async function cmdShip(args, root) {
  const dry = args.includes("--dry-run");
  const w = await resolveWt(args, root);
  const base = w.base || "main";
  const logDir = join(w.path, "tmp", "logs");
  mkdirSync(logDir, { recursive: true });
  const log = join(logDir, `world-ship-${Date.now()}.log`);
  const lines = [];
  const note = (s) => lines.push(s);
  const done = (ok, msg) => {
    writeFileSync(log, `${lines.join("\n")}\n`);
    console.log(`SUMMARY world ship ${ok ? "ok" : "fail"} ${w.slug}: ${msg} log=${log}`);
    return ok ? 0 : 1;
  };

  // 1. Workbench saved?
  const wb = workbenchCheck();
  if (wb.refuse) {
    console.log(`FAIL  ${wb.refuse}`);
    return done(false, "Workbench has unsaved changes");
  }
  if (wb.warn) console.log(`WARN  ${wb.warn}`);

  const { ref, files, others } = collectLayers(w.path, base);
  const d = wd.diffFiles(files);
  printDiff(d, note);
  if (others.length) {
    console.log(`FAIL  addons/ changes beside the layers: ${others.slice(0, 5).join(", ")}. A world ship carries .layer edits only; ship those with wt ship.`);
    return done(false, "non-layer addons/ changes");
  }
  if (d.totals.unparsed) {
    for (const f of d.files) for (const c of f.changes) if (c.kind === "UNPARSED") console.log(`FAIL  ${c.label}: ${c.text}`);
    return done(false, "a layer did not parse");
  }
  const real = d.files.filter((f) => !f.noiseOnly);
  const noise = d.files.filter((f) => f.noiseOnly);
  console.log(`world ship ${w.slug}: ${wd.summaryLine(d)}`);
  for (const f of real) for (const c of f.changes) console.log(`  ${wd.changeLine(c)}`);

  // 2. Revert noise-only files.
  for (const f of noise) {
    note(`revert noise-only ${f.path}: ${f.noise.join("; ")}`);
    if (!dry) git(w.path, ["checkout", ref, "--", f.path]); // the merge-base text: also undoes a committed noise rewrite
  }
  if (noise.length) console.log(`${dry ? "would revert" : "reverted"} ${noise.length} noise-only file(s): ${noise.map((f) => f.path.split("/").pop()).join(", ")}`);
  if (!real.length) return done(true, `nothing to ship (noise only${noise.length ? `, ${noise.length} file(s) ${dry ? "would be " : ""}reverted` : ""})`);

  const touched = wd.touchedNames(d);
  const layers = [...new Set(real.map((f) => f.path.split("/").pop()))];
  const testsDir = join(w.path, "addons", "LifeFramework", "Scripts", "Game", "Tests");
  const tests = existsSync(testsDir) ? readdirSync(testsDir).filter((f) => /^LF_Test_\w+\.c$/.test(f)).map((f) => ({ file: f, text: readFileSync(join(testsDir, f), "utf8") })) : [];
  const related = selectTests(tests, wd.testTokens(layers, touched)).related;
  if (dry) {
    console.log(`dry run: would commit ${real.length} layer file(s), run validate --world (layout checks warn), audit placement --layer ${layers.join(",")}${related.length ? `, test --only ${related.join(",")}` : ", no layer-named tests"}, then wt ship ${w.slug}`);
    return done(true, `dry run: ${wd.summaryLine(d)}`);
  }

  // 3. Commit as the maintainer (the worktree's git identity, or --author).
  const author = opt(args, "--author");
  const msg = `World edit: ${wd.summaryLine(d)}\n\n${real.flatMap((f) => f.changes.map((c) => `- ${wd.changeLine(c)}`)).join("\n")}\n`;
  const paths = [...real, ...noise].map((f) => f.path);
  git(w.path, ["add", "--", ...paths]);
  const cm = git(w.path, ["commit", "-F", "-", ...(author ? ["--author", author] : []), "--", ...paths], msg);
  note(cm.stdout + cm.stderr);
  if (cm.status !== 0 && !/nothing to commit/.test(cm.stdout + cm.stderr)) {
    console.log(`FAIL  git commit: ${(cm.stderr || cm.stdout).trim().split("\n").pop()}`);
    return done(false, "commit failed");
  }

  // 4. Fast checks.
  const checks = [];
  // Only the checks that read .layer files run (validate --world); a layout check that fails is a warning.
  const warnings = [];
  const testWarnings = [];
  const v = cli(w.path, ["validate", "--world"]);
  note(v.out);
  for (const lw of layoutWarnings(v.out)) warnings.push(`validate/${lw.check}: ${lw.text || "failed"}`);
  checks.push(`validate --world: ${v.status === 0 ? `pass${warnings.length ? ` (${warnings.length} layout warning(s))` : ""}` : "FAIL"}`);
  if (v.status !== 0) {
    console.log(`FAIL  validate: ${lastSummary(v.out) || "see log"}`);
    return done(false, "validate failed");
  }
  const a = cli(w.path, ["audit", "placement", "--layer", layers.join(","), "--top", "200"]);
  note(a.out);
  let report = null;
  try { report = JSON.parse(readFileSync(join(w.path, "tmp", "place-audit", "report.json"), "utf8")); } catch { /* no report */ }
  if (!report) {
    console.log(`WARN  audit placement wrote no report: ${lastSummary(a.out) || "see log"}`);
    warnings.push(`placement audit wrote no report (${lastSummary(a.out) || "see the log"}); the touched placements were not audited`);
  }
  const allowPath = join(w.path, ALLOW_FILE);
  const allowJson = JSON.parse(readFileSync(allowPath, "utf8"));
  const added = report ? allowEntries(report, touched, allowJson.allow) : [];
  for (const e of added) warnings.push(`placement audit ${e.kind}: ${e.why}`);
  if (added.length) {
    allowJson.allow.push(...added);
    writeFileSync(allowPath, `${JSON.stringify(allowJson, null, 2)}\n`);
    git(w.path, ["add", "--", ALLOW_FILE]);
    git(w.path, ["commit", "-m", `Allowlist ${added.length} maintainer hand placement(s) (world ship)`, ...(author ? ["--author", author] : []), "--", ALLOW_FILE]);
    console.log(`allowlisted ${added.length} flagged maintainer placement(s) in ${ALLOW_FILE}`);
  }
  checks.push(`audit placement --layer ${layers.join(",")}: ${added.length ? `${added.length} flagged maintainer placement(s) allowlisted` : "nothing flagged on the touched entities"}`);
  if (related.length) {
    const t = cli(w.path, ["test", "--no-build", "--only", related.join(",")]);
    note(t.out);
    // A failing test is a warning on a town-maintainer-* branch; only a run that judged nothing blocks.
    const maintainer = MAINTAINER_BRANCH.test(w.branch || `ws/${w.slug}`);
    const verdict = maintainer ? judgeWorldTests({ status: t.status, text: t.out, fallbackTests: related, tests: tests.map((x) => ({ file: x.file, text: x.text })) }) : { blocks: t.status === 0 ? null : "layer-named tests failed", warnings: [] };
    if (verdict.blocks) {
      checks.push(`test --only ${related.join(",")}: FAIL`);
      console.log(`FAIL  layer-named tests: ${verdict.blocks}: ${lastSummary(t.out) || "see log"}`);
      return done(false, "layer-named tests blocked");
    }
    for (const x of verdict.warnings) testWarnings.push(...testWarningLines([x]));
    for (const x of testWarningLines(verdict.warnings)) console.log(`WARN  test failed on the new layout (not blocking): ${x}`);
    checks.push(`test --only ${related.join(",")}: ${t.status === 0 ? "pass" : `${verdict.failed} failing, listed as warnings`}`);
  } else checks.push("no test names a touched layer or entity");

  // 5. Push, PR, queue (wt ship classifies the branch T1w: static validate, audit, layer-named tests).
  const bodyFile = join(w.path, "tmp", `pr-body-world-${w.slug}.md`);
  writeFileSync(bodyFile, prBody(d, { slug: w.slug, allowed: added, checks, warnings, testWarnings }));
  const mainRoot = root;
  const s = cli(mainRoot, ["wt", "ship", w.slug, "--body-file", bodyFile]);
  note(s.out);
  const sl = lastSummary(s.out);
  if (s.status !== 0) {
    console.log(`FAIL  wt ship: ${sl || s.out.trim().split("\n").slice(-3).join(" | ")}`);
    return done(false, "wt ship failed");
  }
  return done(true, `${wd.summaryLine(d)}; ${sl.replace(/^SUMMARY /, "") || "queued"}`);
}

export async function main(args, { root }) {
  const [sub, ...rest] = args;
  try {
    if (sub === "diff") return await cmdDiff(rest, root);
    if (sub === "ship") return await cmdShip(rest, root);
    // sync: world-sync.mjs (the sync subcommand is added here by tools/lib/world-sync.mjs's author)
  } catch (err) {
    console.log(`FAIL  world ${sub}: ${err.message}`);
    return 1;
  }
  console.log("usage: tools\\cli world diff [--wt <slug>] [--json]\n       tools\\cli world ship [--wt <slug>] [--dry-run] [--author \"Name <mail>\"]");
  return 1;
}
