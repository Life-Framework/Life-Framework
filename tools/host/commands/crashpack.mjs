// `tools\cli host crashpack [--session <stamp>] [--out <dir>] [--lines N]` (#1457). The logic is tools/host/crashpack.mjs.
import { mkdirSync, writeFileSync } from "node:fs";
import { join, resolve } from "node:path";
import { buildCrashpack, defaultBuildText, DEFAULT_LINES } from "../crashpack.mjs";

export const help =
  "crashpack [--session <stamp>] [--out <dir>] [--lines N]   one zip of the newest (or named) crash: dump, log tail, build stamp, mods, server.json and config.json with passwords and webhooks <redacted>, plus an issue body draft; prints the gh command, never runs it";

function flagValue(args, name) {
  const i = args.indexOf(name);
  return i >= 0 ? args[i + 1] : undefined;
}

export async function main(args, ctx) {
  const session = flagValue(args, "--session");
  const out = resolve(flagValue(args, "--out") ?? join(ctx.root, "tmp", "crashpack"));
  const lines = flagValue(args, "--lines") === undefined ? DEFAULT_LINES : Number(flagValue(args, "--lines"));
  for (const f of ["--session", "--out", "--lines"]) {
    if (args.includes(f) && flagValue(args, f) === undefined) {
      console.log(`[LFDebug:HostCrashpack] ${f} needs a value`);
      return 1;
    }
  }
  const r = buildCrashpack(ctx.hostDir, { session, lines, buildText: await defaultBuildText() });
  if (!r.ok) {
    console.log(`[LFDebug:HostCrashpack] ${r.reason}`);
    return 1;
  }
  mkdirSync(out, { recursive: true });
  const zipFile = join(out, `${r.name}.zip`);
  const bodyFile = join(out, `${r.name}-issue.md`);
  writeFileSync(zipFile, r.zip);
  writeFileSync(bodyFile, r.body);
  console.log(`[LFDebug:HostCrashpack] ${r.name}: ${r.files.length} files, ${Math.ceil(r.zip.length / 1024)} KB -> ${zipFile}`);
  for (const s of r.skipped) console.log(`[LFDebug:HostCrashpack] note: ${s}`);
  console.log(`[LFDebug:HostCrashpack] issue draft: ${bodyFile}`);
  console.log(`[LFDebug:HostCrashpack] to file it (not run): gh issue create --title "${r.title.replace(/"/g, "'")}" --body-file "${bodyFile}"  then attach ${zipFile}`);
  console.log(`SUMMARY host-crashpack ok crash=${r.name} files=${r.files.length} zip=${zipFile}`);
  return 0;
}
