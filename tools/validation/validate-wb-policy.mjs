// A Workbench never starts on a PC whose pool caps lack "workbench" without going through
// tools/lib/wb-route.mjs (the maintainer's editor lives on this PC; agents never run a headless one).
// Two rules, checked on the source:
//   1. Only wb-route.mjs takes a Workbench slot from wb-lock.mjs. Anything else calling acquireWorkbench or
//      acquireWorkbenchSync skips the cap check, so a new launcher cannot forget it.
//   2. A file that names the Workbench executable (ArmaReforgerWorkbenchSteamDiag) or starts one through look.mjs
//      (look.workbenchExe, look.runWorkbench) carries a marker that the
//      cap check is in its path: it mentions wb-route, LF_WB_GUARDED or LF_WB_LOCAL. A launcher added without
//      one fails here; add the check (or route the step) rather than a marker.
// inputs: tools/**, agent-box/**
// red-proof: 2026-11-14, a probe file under tools/ calling acquireWorkbench from wb-lock failed rule 1, and one
// naming ArmaReforgerWorkbenchSteamDiag.exe with no marker failed rule 2 (tools/validation/wb-policy-selftest.mjs
// runs both rules on those probes).

import { readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

// Rule 1: files that may use wb-lock's acquire functions directly.
export const LOCK_USERS = new Set(["tools/lib/wb-lock.mjs", "tools/lib/wb-route.mjs", "tools/validation/wb-lock-selftest.mjs", "tools/validation/wb-policy-selftest.mjs"]);
// Rule 2: files that name the executable without launching it (checks, docs of the exe path, the MCP patches).
export const EXE_NAMERS = new Set([
  "tools/validation/validate-addon-source-isolation.ps1",
  "tools/validation/ship-queue-selftest.mjs",
  "tools/validation/wb-policy-selftest.mjs",
  "tools/validation/validate-wb-policy.mjs",
  "tools/ship-queue.mjs",
  "tools/lib/ship-queue.mjs",
  "agent-box/play-claim.mjs",
]);
// An import of wb-lock.mjs, static or dynamic (a comment naming the file is not one).
export const IMPORTS_LOCK = /(?:from\s+|import\s*\(\s*)["'][^"']*wb-lock\.mjs["']/;
export const MARKER =/wb-route|LF_WB_GUARDED|LF_WB_LOCAL/;

// files: [{ path (repo-relative, forward slashes), text }]. Returns the violations as strings.
export function check(files) {
  const bad = [];
  for (const { path, text } of files) {
    if (path.startsWith("tools/mcp-patches/")) continue;
    if (/\bacquireWorkbench(Sync)?\b/.test(text) && IMPORTS_LOCK.test(text) && !LOCK_USERS.has(path)) {
      bad.push(`${path}: takes a Workbench slot from wb-lock directly; use acquireWorkbench from tools/lib/wb-route.mjs (it checks this PC's workbench cap)`);
    }
    if (/ArmaReforgerWorkbenchSteamDiag|look\.(workbenchExe|runWorkbench)/.test(text) && !EXE_NAMERS.has(path) && !MARKER.test(text)) {
      bad.push(`${path}: names the Workbench executable but never consults the workbench cap (wb-route.mjs localWorkbench / requireLocalWorkbench, or LF_WB_GUARDED / LF_WB_LOCAL in a script)`);
    }
  }
  return bad;
}

function walk(dir, out, root) {
  for (const name of readdirSync(dir)) {
    if (name === "node_modules" || name.startsWith(".")) continue;
    const full = join(dir, name);
    const st = statSync(full);
    if (st.isDirectory()) walk(full, out, root);
    else if (/\.(mjs|js|ps1)$/.test(name)) out.push({ path: relative(root, full).replace(/\\/g, "/"), text: readFileSync(full, "utf8") });
  }
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const root = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const files = [];
  for (const d of ["tools", "agent-box"]) walk(join(root, d), files, root);
  const bad = check(files);
  for (const b of bad) console.log(`ERROR ${b}`);
  if (bad.length) process.exit(1);
  console.log("validate-wb-policy: OK (every Workbench launcher consults the workbench cap)");
}
