// tools/lib/selftest-sections.mjs - run the independent sections of a selftest in parallel child processes.
//
// A git-heavy selftest is a chain of scenes that each build their own scratch repo and share nothing. Run in one
// process they take the sum of their times (spawnSync blocks the event loop), so the check is the long pole of
// `tools\cli validate`. This splits them: the file registers each scene with `add(name, fn)`; the parent runs
// the file again once per scene (env LF_SELFTEST_SECTION=<name>) with up to LF_SELFTEST_JOBS (default 4) at a time,
// replays each child's output in registration order, and adds up the failures. Every assertion still runs, in the
// same code, once; only the process it runs in changed.
//
//   const S = sections(import.meta.url);
//   S.add("string union", () => { ...assertions that call fail()... });
//   process.exit(await S.run({ failed: () => failed, summary: (n) => `SUMMARY foo ${n ? "fail" : "ok"}` }));
//
// In a child, run() runs that one section and returns its failure count as the exit code (min(count, 100); a crash
// is exit 1 with its message on stderr). Top-level code of the file runs in every process: put only declarations
// and cheap setup there, and any pure assertions in a section of their own.
import { spawn } from "node:child_process";
import { cpus } from "node:os";
import { fileURLToPath } from "node:url";

export function sectionJobs(env = process.env, cpuCount = cpus().length) {
  const n = Number(env.LF_SELFTEST_JOBS);
  if (Number.isInteger(n) && n >= 1) return n;
  return Math.max(2, Math.min(4, Math.floor(cpuCount / 2)));
}

export function sections(importMetaUrl) {
  const file = fileURLToPath(importMetaUrl);
  const list = [];
  const add = (name, fn) => {
    if (list.some((s) => s.name === name)) throw new Error(`selftest-sections: duplicate section '${name}'`);
    list.push({ name, fn });
  };

  // opts.failed(): this process's failure count so far. opts.summary(n): the parent's final line (optional).
  async function run(opts = {}) {
    const failedNow = opts.failed ?? (() => 0);
    const only = process.env.LF_SELFTEST_SECTION;
    if (only !== undefined) {
      const s = list.find((x) => x.name === only);
      if (!s) {
        console.log(`FAIL  selftest-sections: no section named '${only}'`);
        return 1;
      }
      await s.fn();
      return Math.min(failedNow(), 100);
    }
    const results = new Array(list.length);
    let next = 0;
    const worker = async () => {
      for (let i = next++; i < list.length; i = next++) {
        results[i] = await runChild(file, list[i].name);
      }
    };
    await Promise.all(Array.from({ length: Math.min(sectionJobs(), list.length) }, worker));
    let failed = 0;
    list.forEach((s, i) => {
      const r = results[i];
      if (r.stdout) process.stdout.write(r.stdout.endsWith("\n") ? r.stdout : `${r.stdout}\n`);
      if (r.stderr) process.stderr.write(r.stderr.endsWith("\n") ? r.stderr : `${r.stderr}\n`);
      if (r.code !== 0) {
        failed += Math.max(1, r.code ?? 1);
        console.log(`FAIL  section '${s.name}' exited ${r.code}${r.signal ? ` (${r.signal})` : ""}`);
      }
    });
    if (opts.summary) console.log(opts.summary(failed));
    return failed ? 1 : 0;
  }

  return { add, run };
}

function runChild(file, name) {
  return new Promise((resolve) => {
    let stdout = "";
    let stderr = "";
    const child = spawn(process.execPath, [file, ...process.argv.slice(2)], {
      windowsHide: true,
      stdio: ["ignore", "pipe", "pipe"],
      env: { ...process.env, LF_SELFTEST_SECTION: name },
    });
    child.stdout.on("data", (d) => { stdout += d; });
    child.stderr.on("data", (d) => { stderr += d; });
    child.on("error", (e) => resolve({ code: 1, stdout, stderr: `${stderr}failed to start section '${name}': ${e.message}\n` }));
    child.on("close", (code, signal) => resolve({ code: code ?? 1, signal, stdout, stderr }));
  });
}
