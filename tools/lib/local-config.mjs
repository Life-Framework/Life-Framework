// tools/lib/local-config.mjs - per-machine settings that never enter the repo.
//
// <main-checkout>/lf.local.json (gitignored) is a flat map of setting -> value,
// shared by every worktree of that checkout:
//
//   {
//     "ENFUSION_SERVER_PATH": "C:/LF/ArmaReforgerServer",
//     "LF_GAME_ADDONS": "C:/LF/ArmaReforgerServer/addons",
//     "LF_TESTBOX": "lf-testbox"
//   }
//
// Precedence everywhere: process env > lf.local.json > opencode.json > defaults.
// Keys (docs/setup.md "Machine-local settings"):
//   ENFUSION_*_PATH   tool install paths (same meaning as the env vars)
//   LF_GAME_ADDONS    folder holding the base-game core/ and data/ projects the
//                     test server loads; default: the server install's addons,
//                     else the game client's
//   LF_TESTBOX        ssh host of a second test machine (tools/remote/testbox.mjs)
//   LF_TESTBOX_ROOT   main checkout on that machine (default C:/LF/Life-Framework)
//   LF_TESTBOX_AUTO   "0" stops `test` from moving to the test box when the
//                     local server-2001 lock is busy

import { existsSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";

export const LOCAL_CONFIG_NAME = "lf.local.json";

function mainRoot(root) {
  const res = spawnSync("git", ["-C", root, "rev-parse", "--git-common-dir"], { windowsHide: true, encoding: "utf8" });
  if (res.status !== 0) return root;
  return dirname(resolve(root, res.stdout.trim()));
}

export function localConfigPath(root) {
  return join(mainRoot(root), LOCAL_CONFIG_NAME);
}

const cache = new Map();

export function localConfig(root) {
  const p = localConfigPath(root);
  if (cache.has(p)) return cache.get(p);
  let cfg = {};
  if (existsSync(p)) {
    try {
      const parsed = JSON.parse(readFileSync(p, "utf8").replace(/^\uFEFF/, ""));
      if (parsed && typeof parsed === "object" && !Array.isArray(parsed)) cfg = parsed;
    } catch (e) {
      console.log(`WARN  ${p} is not valid JSON (${e.message}); ignoring it`);
    }
  }
  cache.set(p, cfg);
  return cfg;
}

// One setting: process env first, then lf.local.json. Empty strings count as unset.
export function localSetting(root, key) {
  if (process.env[key]) return process.env[key];
  const v = localConfig(root)[key];
  return v === undefined || v === null || v === "" ? undefined : String(v);
}

// Every string setting in lf.local.json, for merging into a tool's env.
export function localEnv(root) {
  const out = {};
  for (const [k, v] of Object.entries(localConfig(root))) {
    if (typeof v === "string" && v) out[k] = v;
  }
  return out;
}
