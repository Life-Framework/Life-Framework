// tools/validation/validate-incremental-selftest.mjs
// inputs: tools/**, agent-box/**, .githooks/**
//
// Proves the incremental `validate` rules (tools/lib/validate-incremental.mjs) on a scratch git
// repo: a check is skipped only when its declared inputs are untouched since its last green run,
// and every kind of change (edit, add, delete, rename, its own script, a baseline) re-runs it.
//
// red-proof: 2026-11-14, with fingerprint() no longer hashing each matched file path and stat signature
// (only the match count), the run printed `FAIL editing a matched file did not re-run the check`,
// `FAIL renaming a matched file ...`, `FAIL editing the check itself ...` and exited 1; restored, it passes.
import { execFileSync } from "node:child_process";
import { mkdirSync, mkdtempSync, readdirSync, readFileSync, renameSync, rmSync, utimesSync, writeFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { globToRegExp, incrementalOff, parseInputs, plan, snapshotFiles, ttlOf } from "../lib/validate-incremental.mjs";

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
check(incrementalOff({ CI: "true" }) && incrementalOff({ LF_VALIDATE_ALL: "1" }) && !incrementalOff({}), "CI and LF_VALIDATE_ALL must disable it");

// Every real check states its inputs (or "always"): a new check cannot silently skip or silently run.
const realDir = join(dirname(fileURLToPath(import.meta.url)));
for (const f of readdirSync(realDir)) {
  if (!/\.(mjs|ps1)$/.test(f)) continue;
  const head = readFileSync(join(realDir, f), "utf8").split(/\r?\n/).slice(0, 30);
  if (!head.some((l) => /^\s*(?:\/\/|#)\s*inputs:\s*\S/.test(l))) {
    failures.push(`${f} has no "inputs:" header in its first 30 lines (list globs, or "always")`);
  }
}

// Fingerprints on a scratch repo.
const root = mkdtempSync(join(tmpdir(), "lf-validate-inc-"));
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
  check(!plans(s7).get("check-b.mjs").unchanged, "touching a matched file's mtime must re-run (safe side)");
} finally {
  rmSync(root, { recursive: true, force: true });
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("validate-incremental-selftest: OK (header parsing, globs, skip when unchanged, re-run on edit/add/delete/rename/own script/baseline, ttl, always, CI off)");
