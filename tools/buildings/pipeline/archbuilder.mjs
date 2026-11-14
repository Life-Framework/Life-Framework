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
import { existsSync, readFileSync, writeFileSync, mkdirSync, readdirSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";

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

// One headless run of the job script. Throws ArchBuilderMissing when the add-on is not there (the
// stage skips) and Error on anything else (the stage fails). Returns the job's JSON report.
export function runArchBuilder(mode, { spec, out, blend, extra = {}, timeoutMs = 300000, setup = archBuilderSetup() }) {
  if (!setup.ok) throw new ArchBuilderMissing(setup.why);
  mkdirSync(out, { recursive: true });
  const jobFile = join(out, `${mode}-job.json`);
  writeFileSync(jobFile, JSON.stringify({ mode, spec, out, version: setup.addonVersion, ...extra }, null, 1));
  const args = ["--background", ...(blend ? [blend] : []), "--python-exit-code", "3", "--python", JOB_SCRIPT, "--", jobFile];
  const res = spawnSync(setup.exe, args, { encoding: "utf8", timeout: timeoutMs, windowsHide: true, maxBuffer: 64 * 1024 * 1024 });
  const log = join(out, `${mode}-blender.log`);
  writeFileSync(log, (res.stdout || "") + "\n" + (res.stderr || ""));
  const reportFile = join(out, `${mode}.json`);
  const report = existsSync(reportFile) ? JSON.parse(readFileSync(reportFile, "utf8")) : null;
  if (res.error) throw new Error(`Blender ${mode}: ${res.error.message} (log ${log})`);
  if (res.status === SKIP_EXIT) throw new ArchBuilderMissing(report?.skip || "ArchBuilder is not usable in this Blender");
  if (res.status !== 0 || !report) {
    const tail = ((res.stdout || "") + (res.stderr || "")).trim().split("\n").filter((l) => !/EnfusionBlenderTools/.test(l)).slice(-12).join("\n");
    throw new Error(`Blender ${mode} exited ${res.status} (log ${log})\n${tail}`);
  }
  report.log = log;
  return report;
}

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
