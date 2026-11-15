// The one place tools\cli building touches Blender and the ArchBuilder add-on.
//
// ArchBuilder is a paid Blender add-on licensed to one buyer. This repo is public, so:
// - nothing here copies or imports its source; the job script drives its operators;
// - the buyer's id and key are read only to refuse any exported file that carries them
//   (licenseMarks + scanForMarks), and they are never printed;
// - a machine without it (CI, the test laptop) skips the stages that need it (ArchBuilderMissing).
//
// runArchBuilder() is the one function to switch over to tools/lib/blender.mjs
// (blenderExe({ needs: "archbuilder" }), runHeadless) when that lands on main.
import { closeSync, copyFileSync, existsSync, mkdirSync, openSync, readFileSync, readdirSync, renameSync, rmSync, statSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { spawn, spawnSync } from "node:child_process";

const HERE = dirname(fileURLToPath(import.meta.url));
export const JOB_SCRIPT = join(HERE, "blender", "archbuilder_job.py");
export const SKIP_EXIT = 4;

export class ArchBuilderMissing extends Error {}

const appData = () => process.env.APPDATA || join(process.env.USERPROFILE || "", "AppData", "Roaming");

// Blender 5.3 with the add-on. Where the add-on's source lives is LF_ARCHBUILDER_DIR, by default the
// maintainer's local checkout (Documents/Reforger/blender-addons/arch_builder, loaded by Blender as
// the local extension repo `lf_local`), else the Blender extension folder a plain install uses.
// Blender itself finds the add-on by its own preferences; the job script looks for the enabled
// module whose name ends in ".arch_builder", so the repo name (user_default, lf_local) never matters.
export function archBuilderSetup(env = process.env) {
  const exe = env.LF_BLENDER_ARCHBUILDER || "C:/Program Files/Blender Foundation/Blender 5.3/blender.exe";
  const version = env.LF_BLENDER_ARCHBUILDER_VERSION || "5.3";
  const extensions = join(appData(), "Blender Foundation", "Blender", version, "extensions");
  const candidates = env.LF_ARCHBUILDER_DIR ? [env.LF_ARCHBUILDER_DIR]
    : [join(env.USERPROFILE || "", "Documents", "Reforger", "blender-addons", "arch_builder"), join(extensions, "user_default", "arch_builder")];
  const ext = candidates.find((d) => existsSync(join(d, "__init__.py"))) || candidates[0];
  // The key Blender activated is kept per extension repo: extensions/.user/<repo>/arch_builder/.
  const userDir = join(extensions, ".user");
  const activated = existsSync(userDir)
    ? readdirSync(userDir).map((r) => join(userDir, r, "arch_builder", "archbuilder_LICENSE.json")).filter((f) => existsSync(f))
    : [];
  let why = "";
  if (!existsSync(exe)) why = `Blender ${version} not found at ${exe} (LF_BLENDER_ARCHBUILDER overrides)`;
  else if (!existsSync(join(ext, "__init__.py"))) why = `the ArchBuilder add-on is not installed (looked in ${candidates.join(" and ")}; LF_ARCHBUILDER_DIR overrides). Get it at https://arribas22.github.io/archbuilder/`;
  return { exe, ext, activated, ok: !why, why, addonVersion: addonVersion(ext) };
}

function addonVersion(ext) {
  try {
    return /^version\s*=\s*"([^"]+)"/m.exec(readFileSync(join(ext, "blender_manifest.toml"), "utf8"))?.[1] || "";
  } catch {
    return "";
  }
}

// Blender's environment. Its FBX exporter makes every object's id from Python's hash() of the
// object's name, which is salted per process, so the same scene exported twice gave two .fbx files
// that differ in 750 bytes, and every export looked like a changed model to the import. A fixed
// PYTHONHASHSEED (read only with --python-use-system-env) makes the export byte-for-byte the same
// but for its creation time stamp (import.mjs contentHash). The user's other PYTHON* settings are
// dropped so the system env cannot point Blender at another Python.
export function blenderEnv(env = process.env) {
  const out = Object.fromEntries(Object.entries(env).filter(([k]) => !/^PYTHON/i.test(k)));
  return { ...out, PYTHONHASHSEED: "0" };
}
const BLENDER_FLAGS = ["--background", "--python-use-system-env", "--python-exit-code", "3"];

// The Enfusion Blender Tools add-on prints the same import errors on every start (ar_mqa wants a
// module it no longer ships; object_visualization needs a GPU that --background has not got). They
// say nothing about the building: dropped from what the pipeline prints, kept in the log file.
export function withoutBlenderNoise(text) {
  // A message starts "[EnfusionBlenderTools...] [LEVEL]" or "[EBT] [LEVEL]" and runs to the colour
  // reset that ends it (a traceback spans lines), or to the exception line when there is no colour.
  const out = [];
  let skipping = false;
  const ends = (raw, line) => /\x1b\[0m\s*$/.test(raw) || /^[A-Za-z.]*(Error|Exception): /.test(line);
  for (const raw of text.split(/\r?\n/)) {
    const line = raw.replace(/\x1b\[[0-9;]*m/g, "");
    if (!skipping && /^\[(EnfusionBlenderTools[^\]]*|EBT)\] \[[A-Z]+\]/.test(line)) skipping = !ends(raw, line);
    else if (skipping) skipping = !ends(raw, line);
    else out.push(line);
  }
  return out.join("\n");
}

function jobResult(mode, { code, report, text, log, error }) {
  if (error) throw new Error(`Blender ${mode}: ${error} (log ${log})`);
  if (code === SKIP_EXIT) throw new ArchBuilderMissing(report?.skip || "ArchBuilder is not usable in this Blender");
  if (code !== 0 || !report) {
    const tail = withoutBlenderNoise(text).trim().split("\n").filter((l) => l.trim()).slice(-12).join("\n");
    throw new Error(`Blender ${mode} exited ${code} (log ${log})\n${tail}`);
  }
  report.log = log;
  return report;
}

function writeJob(mode, { spec, out, blend, extra, setup }) {
  mkdirSync(out, { recursive: true });
  const job = { mode, spec, out, blend: blend || "", version: setup.addonVersion, ...extra };
  writeFileSync(join(out, `${mode}-job.json`), JSON.stringify(job, null, 1));
  rmSync(join(out, `${mode}.json`), { force: true }); // a report left by an earlier run is not this run's
  return job;
}

const readReport = (out, mode) => {
  const f = join(out, `${mode}.json`);
  return existsSync(f) ? JSON.parse(readFileSync(f, "utf8")) : null;
};

// One headless run of the job script. Throws ArchBuilderMissing when the add-on is not there (the
// stage skips) and Error on anything else (the stage fails). Returns the job's JSON report.
// Inside openSession() ... closeSession() (one `building run`), every call goes to that session's one
// Blender instead of starting its own.
export function runArchBuilder(mode, { spec, out, blend, extra = {}, timeoutMs = 300000, setup = archBuilderSetup() }) {
  if (!setup.ok) throw new ArchBuilderMissing(setup.why);
  if (session) return session.run(mode, { spec, out, blend, extra, timeoutMs, setup });
  writeJob(mode, { spec, out, blend, extra, setup });
  const args = [...BLENDER_FLAGS.slice(0, 1), ...(blend ? [blend] : []), ...BLENDER_FLAGS.slice(1), "--python", JOB_SCRIPT, "--", join(out, `${mode}-job.json`)];
  const res = spawnSync(setup.exe, args, { encoding: "utf8", timeout: timeoutMs, windowsHide: true, maxBuffer: 64 * 1024 * 1024, env: blenderEnv() });
  const log = join(out, `${mode}-blender.log`);
  const text = (res.stdout || "") + "\n" + (res.stderr || "");
  writeFileSync(log, text);
  return jobResult(mode, { code: res.status, report: readReport(out, mode), text, log, error: res.error?.message });
}

// ------------------------------------------------------------------------------- one Blender
// A `building run` used to start Blender once per stage (generate, review, polish, export and the
// export's materials job), 5 to 30 s each before ArchBuilder did a thing. A session starts it once
// (archbuilder_job.py --serve) and hands it each job as a file; each job opens its own .blend there.
// Synchronous like spawnSync, so the stages and materials.mjs call runArchBuilder as before.
let session = null;
const sleepSync = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

export class BlenderSession {
  constructor(dir, { setup = archBuilderSetup() } = {}) {
    this.dir = dir;
    this.setup = setup;
    this.child = null;
    this.starts = 0;
  }

  alive() {
    if (!this.child?.pid) return false;
    try {
      process.kill(this.child.pid, 0);
      return true;
    } catch {
      return false;
    }
  }

  start() {
    this.stop();
    if (existsSync(this.dir)) for (const f of readdirSync(this.dir)) if (/^(job|done)-\d+\.json|^quit$/.test(f)) rmSync(join(this.dir, f), { force: true });
    mkdirSync(this.dir, { recursive: true });
    this.logFile = join(this.dir, "blender.log");
    writeFileSync(this.logFile, "");
    const fd = openSync(this.logFile, "a");
    try {
      this.child = spawn(this.setup.exe, [...BLENDER_FLAGS, "--python", JOB_SCRIPT, "--", "--serve", this.dir], { stdio: ["ignore", fd, fd], windowsHide: true, env: blenderEnv() });
      this.child.on("error", () => {});
    } finally {
      closeSync(fd);
    }
    this.n = 0;
    this.offset = 0;
    this.starts++;
  }

  run(mode, { spec, out, blend, extra = {}, timeoutMs = 300000, setup = this.setup }) {
    // A job with no .blend (generate) builds on the startup scene, which a used Blender no longer has.
    if (!this.alive() || (!blend && this.n > 0)) this.start();
    writeJob(mode, { spec, out, blend, extra, setup });
    const n = ++this.n;
    const jobFile = join(this.dir, `job-${n}.json`);
    copyFileSync(join(out, `${mode}-job.json`), jobFile + ".part");
    renameSync(jobFile + ".part", jobFile);
    const doneFile = join(this.dir, `done-${n}.json`);
    const t0 = Date.now();
    let error = null;
    while (!existsSync(doneFile)) {
      if (!this.alive()) { error = "Blender stopped"; break; }
      if (Date.now() - t0 > timeoutMs) { error = `no answer in ${Math.round(timeoutMs / 1000)} s`; this.stop(); break; }
      sleepSync(50);
    }
    const all = readFileSync(this.logFile, "utf8");
    const text = all.slice(this.offset);
    this.offset = all.length;
    const log = join(out, `${mode}-blender.log`);
    writeFileSync(log, text);
    const code = error ? null : JSON.parse(readFileSync(doneFile, "utf8")).code;
    if (code !== 0 && code !== SKIP_EXIT) this.stop(); // a failed job can leave the scene half-made
    return jobResult(mode, { code, report: error ? null : readReport(out, mode), text, log, error });
  }

  stop() {
    if (!this.child) return;
    if (this.alive()) {
      writeFileSync(join(this.dir, "quit"), "");
      for (let i = 0; i < 40 && this.alive(); i++) sleepSync(50);
      if (this.alive()) this.child.kill();
    }
    this.child = null;
  }
}

// One Blender for the stages that follow, until closeSession(). Started on the first job.
export function openSession(dir, opts) {
  closeSession();
  session = new BlenderSession(dir, opts);
  return session;
}
export function closeSession() {
  session?.stop();
  session = null;
}
process.on("exit", closeSession);

// ---------------------------------------------------------------------------------- watermark
// The strings that identify the buyer: the user id and the key from the add-on's LICENSE.json and
// from the activated license file. ArchBuilder writes "Licensed to: <user>" into EXPORT_INFO.txt
// (the import never copies it); any other file carrying one of these is refused.
export function licenseMarks(setup = archBuilderSetup()) {
  const marks = new Set();
  for (const f of [join(setup.ext, "LICENSE.json"), ...setup.activated]) {
    if (!existsSync(f)) continue;
    try {
      const j = JSON.parse(readFileSync(f, "utf8"));
      for (const k of ["user", "key"]) if (typeof j[k] === "string" && j[k].trim().length >= 4) marks.add(j[k].trim());
      // An email-style buyer id also shows up as its name part (a path, a "made by" line).
      const local = typeof j.user === "string" ? j.user.trim().split("@")[0] : "";
      if (local.length >= 6 && local !== (j.user || "").trim()) marks.add(local);
    } catch {
      // an unreadable license file is not ours to fix; the export itself would have been locked
    }
  }
  return [...marks];
}

// Files (absolute paths) that contain a mark, case-insensitive, in text or binary. Never returns the
// mark itself: the caller names the file and says "license watermark".
export function scanForMarks(files, marks) {
  if (!marks.length) return [];
  // Each file is read as each encoding it could carry the mark in (latin1, UTF-8 for a non-ASCII
  // name, UTF-16 at both byte offsets) and compared case-insensitively as text.
  const needles = marks.map((m) => m.toLowerCase());
  const hits = [];
  for (const f of files) {
    const buf = readFileSync(f);
    const views = [buf.toString("latin1"), buf.toString("utf8"), buf.toString("utf16le"), buf.subarray(1).toString("utf16le")];
    if (views.some((v) => { const low = v.toLowerCase(); return needles.some((n) => low.includes(n)); })) hits.push(f);
  }
  return hits;
}

export function listFiles(dir, base = dir, out = []) {
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const p = join(dir, e.name);
    if (e.isDirectory()) listFiles(p, base, out);
    else if (statSync(p).isFile()) out.push(p);
  }
  return out;
}
