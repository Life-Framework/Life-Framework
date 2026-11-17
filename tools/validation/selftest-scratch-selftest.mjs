// tools/validation/selftest-scratch-selftest.mjs
// inputs: tools/lib/scratch.mjs, tools/lib/selftest-git.mjs, tools/commands/tmp-sweep.mjs, tools/validation/selftest-scratch-selftest.mjs, tools/validation/*selftest*.mjs
//
// The guard for #2589 (selftests left `lf-*` folders in the OS temp dir: 400,000 in a week on lf-testbox, 18 GB):
//   * static: no selftest makes a temp folder with a bare mkdtempSync(join(tmpdir(), ...)); it uses scratchDir() from
//     tools/lib/scratch.mjs, which removes the folder when the process ends, even on a failed check or process.exit;
//   * dynamic: a sample of real selftests runs with TMP/TEMP/TMPDIR pointed at an empty folder, and nothing may remain in it;
//   * scratchDir itself leaves nothing after a throw or a process.exit(3) and keeps the exit code;
//   * tmp-sweep only plans `lf-*` folders older than the cutoff.
// red-proof: 2026-11-17, on the code before the fix (bare mkdtempSync in 145 selftests, no scratch.mjs): the static check
// named 145 bare callers and promote-selftest left lf-gitcfg-* in the private temp dir (2 FAIL, exit 1); green after.

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, relative } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { planSweep } from "../commands/tmp-sweep.mjs";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const TOOLS = join(ROOT, "tools");
let failed = 0;
const check = (name, ok, detail = "") => {
  if (!ok) {
    failed++;
    console.log(`FAIL  ${name}${detail ? `: ${detail}` : ""}`);
  }
};

// A private temp root for the child runs. It is made with a bare mkdtempSync on purpose (the guard cannot depend on
// the helper it checks) and removed in the finally.
const priv = mkdtempSync(join(tmpdir(), "lf-scratch-guard-"));
const runIn = (tmp, args) => spawnSync(process.execPath, args, { cwd: ROOT, encoding: "utf8", windowsHide: true, timeout: 280000, env: { ...process.env, TMP: tmp, TEMP: tmp, TMPDIR: tmp } });

try {
  // ---- static: no selftest uses a bare mkdtempSync under the OS temp dir
  const selftests = [];
  (function walk(d) {
    for (const n of readdirSync(d, { withFileTypes: true })) {
      if (n.name === "node_modules" || n.name === ".git") continue;
      const p = join(d, n.name);
      if (n.isDirectory()) walk(p);
      else if (/\.mjs$/.test(n.name) && /selftest|\.test\./.test(n.name)) selftests.push(p);
    }
  })(TOOLS);
  const bare = selftests.filter((f) => !f.endsWith("selftest-scratch-selftest.mjs") && /mkdtemp(Sync)?\(\s*(?:path\.)?join\(\s*(?:os\.)?tmpdir\(\)/.test(readFileSync(f, "utf8")));
  check("no selftest makes a temp folder with a bare mkdtempSync(join(tmpdir(), ...)): use scratchDir() from tools/lib/scratch.mjs", bare.length === 0, `${bare.length}: ${bare.slice(0, 8).map((f) => relative(ROOT, f).replace(/\\/g, "/")).join(", ")}${bare.length > 8 ? ", ..." : ""}`);

  // ---- dynamic: a sample of real selftests leaves nothing in its temp dir
  const SAMPLE = ["disk-guard-selftest", "features-index-selftest", "gpu-session-selftest", "guid-paths-selftest", "promote-selftest"];
  for (const name of SAMPLE) {
    const file = `tools/validation/${name}.mjs`;
    if (!existsSync(join(ROOT, file))) {
      check(`sample selftest exists: ${name}`, false);
      continue;
    }
    const tmp = join(priv, name);
    mkdirSync(tmp);
    const r = runIn(tmp, [file]);
    const left = readdirSync(tmp);
    check(`${name} still passes`, r.status === 0, `exit ${r.status}: ${String(r.stdout ?? "").trim().split("\n").slice(-2).join(" | ")}`);
    check(`${name} leaves no folder in the OS temp dir`, left.length === 0, `left: ${left.slice(0, 5).join(", ")}${left.length > 5 ? ` (+${left.length - 5})` : ""}`);
  }

  // ---- scratchDir cleans up however the process ends
  if (!existsSync(join(TOOLS, "lib", "scratch.mjs"))) {
    check("tools/lib/scratch.mjs exists", false);
  } else {
    const helper = JSON.stringify(pathToFileURL(join(TOOLS, "lib", "scratch.mjs")).href);
    const cases = [
      ["a throw", `import { scratchDir } from ${helper}; import { writeFileSync } from "node:fs"; const d = scratchDir("lf-guard-a-"); writeFileSync(d + "/x", "1"); throw new Error("boom");`, 1],
      ["process.exit(3)", `import { scratchDir } from ${helper}; import { mkdirSync, writeFileSync } from "node:fs"; const d = scratchDir("lf-guard-b-"); mkdirSync(d + "/deep/er", { recursive: true }); writeFileSync(d + "/deep/er/x", "1"); process.exit(3);`, 3],
      ["a chdir into the folder", `import { scratchDir } from ${helper}; process.chdir(scratchDir("lf-guard-c-"));`, 0],
    ];
    cases.forEach(([label, code, exit], i) => {
      const tmp = join(priv, `case-${i}`);
      const sub = join(tmp, "t");
      mkdirSync(sub, { recursive: true });
      const file = join(tmp, "case.mjs");
      writeFileSync(file, code);
      const r = runIn(sub, [file]);
      check(`scratchDir: after ${label} the exit code is kept`, r.status === exit, `exit ${r.status}, wanted ${exit}`);
      check(`scratchDir: after ${label} nothing is left behind`, readdirSync(sub).length === 0, readdirSync(sub).join(", "));
    });
  }

  // ---- tmp-sweep plans only old lf-* folders
  {
    const now = 1_000_000_000_000;
    const h = 3600 * 1000;
    const plan = planSweep(
      [
        { name: "lf-old-1", mtimeMs: now - 30 * h },
        { name: "lf-fresh", mtimeMs: now - 2 * h },
        { name: "other-old", mtimeMs: now - 90 * h },
        { name: "lf", mtimeMs: now - 90 * h },
      ],
      { now, hours: 24 },
    );
    check("tmp-sweep plans only lf-* folders older than the cutoff", plan.length === 1 && plan[0] === "lf-old-1", JSON.stringify(plan));
  }
} finally {
  try {
    safeRemoveTree(priv);
  } catch (e) {
    console.error(`WARN  could not remove ${priv}: ${e.message}`);
  }
}

if (failed) {
  console.log(`selftest-scratch-selftest: ${failed} failed`);
  process.exit(1);
}
console.log("selftest-scratch-selftest: ok");
