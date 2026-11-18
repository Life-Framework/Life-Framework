// tools/validation/validate-incremental-selftest.mjs
// inputs: tools/**, agent-box/**, .githooks/**
//
// Proves the incremental `validate` rules (tools/lib/validate-incremental.mjs) on a scratch git
// repo: a check is skipped only when its declared inputs are untouched since its last green run,
// and every kind of change (edit, add, delete, rename, its own script, a baseline) re-runs it.
//
// red-proof: 2026-11-18 (base skip), with touches() returning false the run printed `FAIL a modified input since the base
// must run the check` and three more FAIL lines; restored, it passes.
// red-proof: 2026-11-14, with fingerprint() no longer hashing each matched file path and stat signature
// (only the match count), the run printed `FAIL editing a matched file did not re-run the check`,
// `FAIL renaming a matched file ...`, `FAIL editing the check itself ...` and exited 1; restored, it passes.
import { execFileSync } from "node:child_process";
import { mkdirSync, readdirSync, readFileSync, renameSync, rmSync, utimesSync, writeFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { baseChanges, globToRegExp, incrementalOff, loadState, parseInputs, plan, publishShared, snapshotFiles, touches, ttlOf } from "../lib/validate-incremental.mjs";
import { scratchDir } from "../lib/scratch.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

// Pure parsing and globbing.
check(JSON.stringify(parseInputs("// x\n// inputs: a/**/*.c, b.md\n")) === '["a/**/*.c","b.md"]', "mjs header not parsed");
check(JSON.stringify(parseInputs("# t\n# inputs: a.layer\n")) === '["a.layer"]', "ps1 header not parsed");
check(parseInputs("// inputs: always\n") === null, "'always' must mean no skipping");
check(parseInputs("// nothing declared\nconst x = 1;\n") === null, "no header must mean no skipping");
check(ttlOf(parseInputs("// inputs: ttl=300\n")) === 300, "ttl token not read");
const g = (glob, path) => globToRegExp(glob).test(path);
check(g("addons/**/*.layer", "addons/A/B/c.layer"), "** across folders");
check(g("addons/**/*.layer", "addons/c.layer"), "** matches zero folders");
check(!g("addons/*.layer", "addons/A/c.layer"), "* must stay in one folder");
check(!g("addons/**/*.layer", "addons/A/c.layers"), "glob must anchor at the end");
check(g("docs/features.md", "docs/features.md") && !g("docs/features.md", "docsXfeatures.md"), "dots are literal");
// touches(): what counts as a changed input for the base skip.
const self = "tools/validation/check-x.mjs";
check(!touches(["tools/town/claims/a.json"], ["tools/**"], self, "// a pool selftest"), "a town claim must not count for a broad tools/** check that never names it");
check(touches(["tools/town/claims/a.json"], ["tools/**"], self, "read tools/town/claims here"), "a town claim must count for a check that names town/claims");
check(touches(["tools/town/claims/a.json"], ["tools/town/**"], self, ""), "a narrower glob matching a data folder must always count");
check(!touches(["docs/features/bank.md"], ["tools/**", "docs/**"], self, ""), "a feature doc must not count for a broad docs/** check that never names docs/features");
check(touches(["tools/lib/pool.mjs"], ["tools/**"], self, ""), "a tools code change must count for a tools/** check");
check(!touches(["tools/validation/record-shapes.json"], ["addons/**"], self, "// lint"), "a baseline must count only for checks that name it");
check(touches(["tools/validation/record-shapes.json"], ["addons/**"], self, "record-shapes.json"), "a baseline must count for the check that names it");
check(touches(["tools/validation/fixtures/x/y.txt"], ["addons/**"], self, ""), "a fixture change must count for every check");
check(touches([self], ["addons/**"], self, ""), "the check's own file must count");
check(!touches(["addons/a.c"], ["docs/*.md"], self, ""), "a path outside every glob must not count");
check(incrementalOff({ CI: "true" }) && incrementalOff({ LF_VALIDATE_ALL: "1" }) && !incrementalOff({}), "CI and LF_VALIDATE_ALL must disable it");

// Every real check should state its inputs (or "always"). One that does not simply runs every time.
const realDir = join(dirname(fileURLToPath(import.meta.url)));
for (const f of readdirSync(realDir)) {
  if (!/\.(mjs|ps1)$/.test(f)) continue;
  const head = readFileSync(join(realDir, f), "utf8").split(/\r?\n/).slice(0, 30);
  if (!head.some((l) => /^\s*(?:\/\/|#)\s*inputs:\s*\S/.test(l))) {
    // A warning, not a failure: a check added on another branch must not break this one. It runs every time until it declares.
    console.log(`WARN ${f} has no "inputs:" header in its first 30 lines (list globs, or "always"); it runs on every validate`);
  }
}

// Fingerprints on a scratch repo.
const root = scratchDir("lf-validate-inc-");
try {
  const sh = (...a) => execFileSync("git", a, { cwd: root, stdio: "ignore" });
  sh("init", "-q");
  const put = (rel, text = "x") => {
    mkdirSync(dirname(join(root, rel)), { recursive: true });
    writeFileSync(join(root, rel), text);
  };
  put("tools/validation/check-a.mjs", "// check-a\n// inputs: world/**/*.layer\n");
  put("tools/validation/check-b.mjs", "// check-b\n// inputs: docs/*.md\n");
  put("tools/validation/check-c.mjs", "// check-c\n// inputs: always\n");
  put("tools/validation/check-t.mjs", "// check-t\n// inputs: ttl=300\n");
  put("world/a/one.layer", "1");
  put("world/a/two.layer", "2");
  put("docs/x.md", "d");
  const dir = join(root, "tools", "validation");
  const scripts = ["check-a.mjs", "check-b.mjs", "check-c.mjs", "check-t.mjs"];
  const plans = (state) => plan(root, dir, scripts, state, snapshotFiles(root));
  const green = (p, now = Date.now()) => ({ checks: Object.fromEntries([...p].filter(([, v]) => v.fp).map(([k, v]) => [k, { fp: v.fp, at: now }])) });

  const state = green(plans({ checks: {} }));
  let p = plans(state);
  check(p.get("check-a.mjs").unchanged && p.get("check-b.mjs").unchanged, "nothing changed: both checks must skip");
  check(!p.get("check-c.mjs").unchanged, "'always' check must never skip");
  check(p.get("check-t.mjs").unchanged, "ttl check inside its window must skip");
  check(!plan(root, dir, scripts, { checks: { "check-t.mjs": { fp: "ttl", at: Date.now() - 301000 } } }, snapshotFiles(root)).get("check-t.mjs").unchanged, "ttl check past its window must run");

  const bump = (rel) => {
    const t = new Date(Date.now() + 5000);
    utimesSync(join(root, rel), t, t);
  };
  put("docs/unrelated.txt");
  p = plans(state);
  check(p.get("check-a.mjs").unchanged && p.get("check-b.mjs").unchanged, "a file outside every glob re-ran a check");

  put("world/a/one.layer", "1 changed");
  p = plans(state);
  check(!p.get("check-a.mjs").unchanged, "editing a matched file did not re-run the check");
  check(p.get("check-b.mjs").unchanged, "editing a layer re-ran the docs check");

  const s2 = green(plans({ checks: {} }));
  put("world/b/new.layer");
  check(!plans(s2).get("check-a.mjs").unchanged, "adding a matched file did not re-run the check");

  const s3 = green(plans({ checks: {} }));
  rmSync(join(root, "world/a/two.layer"));
  check(!plans(s3).get("check-a.mjs").unchanged, "deleting a matched file did not re-run the check");

  const s4 = green(plans({ checks: {} }));
  renameSync(join(root, "world/a/one.layer"), join(root, "world/a/uno.layer"));
  check(!plans(s4).get("check-a.mjs").unchanged, "renaming a matched file did not re-run the check");

  const s5 = green(plans({ checks: {} }));
  put("tools/validation/check-a.mjs", "// check-a\n// inputs: world/**/*.layer\n// edited\n");
  check(!plans(s5).get("check-a.mjs").unchanged, "editing the check itself did not re-run it");

  const s6 = green(plans({ checks: {} }));
  put("tools/validation/check-a-baseline.json", "{}");
  check(!plans(s6).get("check-a.mjs").unchanged, "adding a baseline beside the checks did not re-run the check");

  const s7 = green(plans({ checks: {} }));
  bump("docs/x.md");
  check(plans(s7).get("check-b.mjs").unchanged, "touching a matched file's mtime (same bytes) must skip: fingerprints are content-addressed");

  // A fresh worktree has no local state; a fingerprint another worktree published as green skips the check.
  const fresh = plans({ checks: {} });
  const shared = { checks: {}, shared: { "check-a.mjs": [fresh.get("check-a.mjs").fp] } };
  const ps = plans(shared);
  check(ps.get("check-a.mjs").unchanged && /another worktree/.test(ps.get("check-a.mjs").why), "a fingerprint green in another worktree must skip the check");
  check(!ps.get("check-b.mjs").unchanged, "a check with no shared green fingerprint must run");

  // The same bytes give the same fingerprint untracked (hashed here) and committed (git's blob id): what
  // makes the shared file valid across worktrees.
  const before = plans({ checks: {} }).get("check-a.mjs").fp;
  sh("add", "-A");
  sh("-c", "user.name=t", "-c", "user.email=t@t", "commit", "-q", "-m", "x");
  check(plans({ checks: {} }).get("check-a.mjs").fp === before, "committing unchanged bytes changed the fingerprint (blob id mismatch)");

  // Base-clean skip: with no local or shared green, a check whose inputs this branch did not change
  // since its merge-base with origin/main is skipped; one whose inputs it changed runs.
  sh("update-ref", "refs/remotes/origin/main", "HEAD");
  check(baseChanges(root, "origin/nope") === null, "a missing base ref must turn the base skip off (null)");
  const basePlan = () => plan(root, dir, scripts, { checks: {} }, snapshotFiles(root), baseChanges(root));
  let pb = basePlan();
  check(pb.get("check-a.mjs").unchanged && pb.get("check-a.mjs").base && pb.get("check-b.mjs").unchanged, "a branch equal to its base must skip declared checks");
  check(!pb.get("check-c.mjs").unchanged, "the base skip must never skip an 'always' check");
  check(!plan(root, dir, scripts, { checks: {} }, snapshotFiles(root), null).get("check-a.mjs").unchanged, "without a base (null) nothing is skipped that way");
  put("world/a/uno.layer", "branch edit");
  pb = basePlan();
  check(!pb.get("check-a.mjs").unchanged, "a modified input since the base must run the check");
  check(pb.get("check-b.mjs").unchanged, "an edit outside a check's inputs must not run it");
  sh("add", "-A");
  sh("-c", "user.name=t", "-c", "user.email=t@t", "commit", "-q", "-m", "branch");
  check(!basePlan().get("check-a.mjs").unchanged, "an input changed in a branch commit (not only the working tree) must run the check");
  put("docs/new.md", "untracked");
  check(!basePlan().get("check-b.mjs").unchanged, "an untracked new input must run the check");
  rmSync(join(root, "docs/x.md"));
  check(baseChanges(root).includes("docs/x.md"), "a deleted input must be listed as changed");
  put("tools/validation/check-b.mjs", "// check-b\n// inputs: docs/*.md\n// edited\n");
  check(baseChanges(root).includes("tools/validation/check-b.mjs") && !basePlan().get("check-b.mjs").unchanged, "editing the check itself must run it");

  // A ttl check green in another worktree within its window skips here too (machine state is shared).
  const ttlState = { checks: {} };
  Object.defineProperty(ttlState, "sharedTtl", { value: { "check-t.mjs": Date.now() - 1000 }, enumerable: false });
  check(plan(root, dir, scripts, ttlState, snapshotFiles(root)).get("check-t.mjs").unchanged, "a shared ttl green inside the window must skip");
  const sharedPath = join(root, "shared-green.json");
  publishShared(sharedPath, { checks: { "check-t.mjs": { fp: "ttl", at: Date.now() } } });
  check(plan(root, dir, scripts, loadState(root, sharedPath), snapshotFiles(root)).get("check-t.mjs").unchanged, "publishShared must carry a ttl green time to other worktrees");
} finally {
  rmSync(root, { recursive: true, force: true });
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("validate-incremental-selftest: OK (header parsing, globs, skip when unchanged, re-run on edit/add/delete/rename/own script/baseline, ttl, always, CI off)");
