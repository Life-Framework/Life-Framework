// tools/validation/validate-incremental-selftest.mjs
// inputs: tools/**, agent-box/**, .githooks/**
//
// Proves the incremental `validate` rules (tools/lib/validate-incremental.mjs) on a scratch git
// repo: a check is skipped only when its declared inputs are untouched since its last green run,
// and every kind of change (edit, add, delete, rename, its own script, a baseline) re-runs it.
//
// red-proof: 2026-11-18 (#3189, CI green set), with loadShared() ignoring the file's `tool` hash the run printed `FAIL a green set written by
// another validate tool must run everything`, `FAIL a green set with no tool hash must run everything` and `FAIL a green set made by a different
// validate tool ...`; with it keeping non-list entries plan() threw `TypeError: state.shared?.[s]?.includes is not a function`; restored, it passes.
// red-proof: 2026-11-18 (#3115, import closure), with plan() building an empty closure the run printed `FAIL editing a module the check
// imports did not re-run it`, `FAIL editing a module the check imports only through another module ...` and `FAIL a branch edit to a
// transitively imported module must run an imports check`; restored, it passes.
// red-proof: 2026-11-18 (base skip), with touches() returning false the run printed `FAIL a modified input since the base
// must run the check` and three more FAIL lines; restored, it passes.
// red-proof: 2026-11-14, with fingerprint() no longer hashing each matched file path and stat signature
// (only the match count), the run printed `FAIL editing a matched file did not re-run the check`,
// `FAIL renaming a matched file ...`, `FAIL editing the check itself ...` and exited 1; restored, it passes.
import { execGit } from "../lib/git-retry.mjs";
import { mkdirSync, readdirSync, readFileSync, renameSync, rmSync, utimesSync, writeFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { baseChanges, ciGreenFile, globToRegExp, incrementalOff, loadState, parseInputs, plan, publishShared, saveState, sharedFile, snapshotFiles, toolHash, touches, ttlOf } from "../lib/validate-incremental.mjs";
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
check(JSON.stringify(parseInputs("// c\n// inputs: imports\n")) === '["imports"]', "the imports token is an input list, not 'always'");
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
check(incrementalOff({ CI: "true" }) && incrementalOff({ LF_VALIDATE_ALL: "1" }) && !incrementalOff({}), "CI without a green set and LF_VALIDATE_ALL must disable it");
// CI skips only when the workflow named a restored green set (#3189); LF_VALIDATE_ALL still runs everything.
check(!incrementalOff({ CI: "true", LF_VALIDATE_GREEN_FILE: "x.json" }), "CI with a green set file must allow skipping");
check(incrementalOff({ CI: "true", LF_VALIDATE_GREEN_FILE: "x.json", LF_VALIDATE_ALL: "1" }), "LF_VALIDATE_ALL must beat a CI green set");
check(ciGreenFile({ LF_VALIDATE_GREEN_FILE: "x.json" }) === null, "a green set path outside CI must be ignored (a developer's shell never reads the CI file)");
check(ciGreenFile({ GITHUB_ACTIONS: "true", LF_VALIDATE_GREEN_FILE: "x.json" }) === "x.json", "a green set path in CI must be used");

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
  const sh = (...a) => execGit(a, { cwd: root, stdio: "ignore" });
  sh("init", "-q");
  const put = (rel, text = "x") => {
    mkdirSync(dirname(join(root, rel)), { recursive: true });
    writeFileSync(join(root, rel), text);
  };
  put("tools/validation/check-a.mjs", "// check-a\n// inputs: world/**/*.layer\n");
  put("tools/validation/check-b.mjs", "// check-b\n// inputs: docs/*.md\n");
  put("tools/validation/check-c.mjs", "// check-c\n// inputs: always\n");
  put("tools/validation/check-t.mjs", "// check-t\n// inputs: ttl=300\n");
  // check-d reads no data, only the modules it imports (dep.mjs, which imports dep2.mjs): `inputs: imports`.
  put("tools/validation/check-d.mjs", '// check-d\n// inputs: imports\nimport "../lib/dep.mjs";\n');
  put("tools/lib/dep.mjs", 'import "./dep2.mjs";\nexport const x = 1;\n');
  put("tools/lib/dep2.mjs", "export const y = 2;\n");
  put("world/a/one.layer", "1");
  put("world/a/two.layer", "2");
  put("docs/x.md", "d");
  const dir = join(root, "tools", "validation");
  const scripts = ["check-a.mjs", "check-b.mjs", "check-c.mjs", "check-t.mjs", "check-d.mjs"];
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

  // A check's static import closure is an input whatever its header lists (#3115): `imports` checks have no data.
  const s8 = green(plans({ checks: {} }));
  check(plans(s8).get("check-d.mjs").unchanged, "an imports check with nothing changed must skip");
  put("tools/lib/unrelated.mjs", "export const z = 3;\n");
  put("world/a/one.layer", "1 changed again");
  check(plans(s8).get("check-d.mjs").unchanged, "a module the check does not import (or a data file) re-ran an imports check");
  put("tools/lib/dep.mjs", 'import "./dep2.mjs";\nexport const x = 5;\n');
  check(!plans(s8).get("check-d.mjs").unchanged, "editing a module the check imports did not re-run it");
  const s9 = green(plans({ checks: {} }));
  put("tools/lib/dep2.mjs", "export const y = 9;\n");
  check(!plans(s9).get("check-d.mjs").unchanged, "editing a module the check imports only through another module did not re-run it");

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
  check(pb.get("check-d.mjs").unchanged && pb.get("check-d.mjs").base, "an imports check whose modules the branch did not touch must skip against the base");
  put("tools/lib/dep2.mjs", "export const y = 10;\n");
  check(!basePlan().get("check-d.mjs").unchanged, "a branch edit to a transitively imported module must run an imports check");
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

  // CI green set (#3189): a file restored by actions/cache from another run is trusted only for checks whose
  // fingerprint it lists; a changed input, another tool version, or a damaged file runs the check.
  const ciPath = join(root, "ci-green", "validate-green.json");
  const ciEnv = { CI: "true", LF_VALIDATE_GREEN_FILE: ciPath };
  check(sharedFile(root, ciEnv) === ciPath, "sharedFile must be the CI green set in CI");
  check(ciGreenFile({ ...ciEnv, GITHUB_WORKSPACE: root }, root) === ciPath, "the CI green set must apply to the runner's workspace");
  check(ciGreenFile({ ...ciEnv, GITHUB_WORKSPACE: join(root, "elsewhere") }, root) === null, "a scratch repo run inside CI must not read or rewrite the real green set");
  const ciPlan = () => plan(root, dir, scripts, loadState(root, sharedFile(root, ciEnv), ciEnv), snapshotFiles(root), null);
  check(ciPlan().get("check-a.mjs").unchanged === false, "a missing CI green set must run everything");
  // run 1: nothing cached, every check passes, the run publishes the set (saveState writes tmp/ in the scratch repo).
  const run1 = loadState(root, sharedFile(root, ciEnv), ciEnv);
  for (const [name, v] of plan(root, dir, scripts, run1, snapshotFiles(root), null)) {
    if (v.fp) run1.checks[name] = { fp: v.fp, secs: 1, at: Date.now() };
  }
  saveState(root, run1);
  rmSync(join(root, "tmp"), { recursive: true, force: true }); // a new runner has the restored green set only, no tmp/validate-state.json
  // run 2 (another job): the restored file skips what its fingerprints match, never an 'always' check or a ttl check.
  let cp = ciPlan();
  check(cp.get("check-a.mjs").unchanged && cp.get("check-b.mjs").unchanged && cp.get("check-d.mjs").unchanged, "a restored green set must skip checks whose inputs match");
  check(/another worktree/.test(cp.get("check-a.mjs").why), "the CI skip is a shared green skip");
  check(!cp.get("check-c.mjs").unchanged, "an 'always' check must still run in CI");
  check(!cp.get("check-t.mjs").unchanged, "a ttl (machine state) green must not carry over to a new CI runner");
  // a changed input reruns that check and only that one
  put("world/a/uno.layer", "edited after the cache was saved");
  cp = ciPlan();
  check(!cp.get("check-a.mjs").unchanged, "a changed input must rerun the check despite a restored green set");
  check(cp.get("check-b.mjs").unchanged, "a restored green set must still skip a check whose inputs did not change");
  put("tools/lib/dep2.mjs", "export const y = 77;\n");
  check(!ciPlan().get("check-d.mjs").unchanged, "a changed import of the check must rerun it despite a restored green set");
  // the file as it is on disk, damaged in each way
  const good = readFileSync(ciPath, "utf8");
  const damage = (text) => writeFileSync(ciPath, text);
  const allRun = () => [...ciPlan().entries()].filter(([, v]) => v.unchanged).map(([k]) => k);
  put("world/a/uno.layer", "1");
  put("tools/lib/dep2.mjs", "export const y = 9;\n");
  damage(good);
  check(allRun().length > 0, "the undamaged green set must skip something (the test's own control)");
  damage("{ not json");
  check(allRun().length === 0, "a corrupt green set (not JSON) must run everything");
  damage("");
  check(allRun().length === 0, "an empty green set file must run everything");
  damage(JSON.stringify({ ...JSON.parse(good), version: 1 }));
  check(allRun().length === 0, "a green set of another state version must run everything");
  damage(JSON.stringify({ ...JSON.parse(good), tool: "0".repeat(40) }));
  check(allRun().length === 0, "a green set written by another validate tool must run everything");
  const noTool = JSON.parse(good);
  delete noTool.tool;
  damage(JSON.stringify(noTool));
  check(allRun().length === 0, "a green set with no tool hash must run everything");
  damage(JSON.stringify({ ...JSON.parse(good), checks: { "check-a.mjs": "abc", "check-b.mjs": 7, "check-d.mjs": [null, 3, "x"] } }));
  check(allRun().length === 0, "a corrupt green set (a string where a list belongs) must not skip by substring");
  damage(JSON.stringify([1, 2]));
  check(allRun().length === 0, "a green set that is a JSON array must run everything");
  damage(good);
  // toolHash moves with the tool's code (CI keys the cache on it too)
  const h0 = toolHash(root);
  put("tools/lib/validate-incremental.mjs", "// stand-in tool\n");
  const h1 = toolHash(root);
  check(h1 !== h0, "toolHash must change when the validate tool's code changes");
  check(h1 === toolHash(root) && /^[0-9a-f]{40}$/.test(h0), "toolHash must be a stable sha1");
  put("tools/lib/validate-incremental.mjs", "// stand-in tool\r\n");
  check(toolHash(root) === h1, "toolHash must ignore CRLF versus LF checkouts");
  check(allRun().length === 0, "a green set made by a different validate tool must run everything (tool hash moved)");
} finally {
  rmSync(root, { recursive: true, force: true });
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("validate-incremental-selftest: OK (header parsing, globs, skip when unchanged, re-run on edit/add/delete/rename/own script/baseline, ttl, always, CI green set restored from cache: matching inputs skip, changed/corrupt/other-tool runs)");
