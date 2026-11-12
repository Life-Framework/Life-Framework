// tools/lib/addons.mjs - the repo's addons and how Workbench is told where they are.
//
// Workbench keeps only the LAST of repeated `-addonsDir` flags (pack-move-spike.md
// section 4), so a project that depends on another repo addon (LF_Everon1989 on
// LifeFramework) fails with `dependency '79636E668EA37AC9' can't be added`. One
// flag with comma-separated folders works. Every launcher builds its flag here.

import { existsSync, mkdirSync, mkdtempSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { addonResources } from "./rdb-fresh.mjs";

// Core first: it is the one that always participates.
export const ADDONS = [
  { name: "LifeFramework", guid: "79636E668EA37AC9" },
  { name: "LF_Everon1989", guid: "B1E84D5687E11724" },
];
export const CORE = ADDONS[0].name;

// ["-addonsDir", "a,b"]: one flag, comma-separated, empty entries dropped.
export function addonsDirArgs(dirs) {
  return ["-addonsDir", dirs.filter(Boolean).join(",")];
}

export function addonDir(root, name) {
  return join(root, "addons", name);
}

export function gprojOf(root, name) {
  return join(addonDir(root, name), `${name}.gproj`);
}

// Accepts the addon name (any case) or its GUID; returns the ADDONS entry or null.
export function resolveAddon(arg) {
  const a = String(arg ?? "").trim().toLowerCase();
  if (!a) return null;
  return ADDONS.find((x) => x.name.toLowerCase() === a || x.guid.toLowerCase() === a) ?? null;
}

// Addons the resource-database tooling covers: the core always, any other addon
// that has a project file and declares at least one resource. An empty pack has
// no rdb to keep fresh, so today's single-core behaviour is unchanged.
export function rdbAddons(root) {
  return ADDONS.filter((a, i) => {
    if (i === 0) return true;
    if (!existsSync(gprojOf(root, a.name))) return false;
    return addonResources(addonDir(root, a.name)).length > 0;
  }).map((a) => a.name);
}

// Where `pack` writes by default: one folder per addon, so a second pack never
// replaces the first.
export function defaultPackOut(root, name) {
  return join(root, "tmp", "workshop-pack", name);
}

// Every addon folder under addons/ with a project file is one of ADDONS.
export function unknownAddonFolders(root) {
  const dir = join(root, "addons");
  if (!existsSync(dir)) return [];
  return readdirSync(dir, { withFileTypes: true })
    .filter((e) => e.isDirectory() && existsSync(join(dir, e.name, `${e.name}.gproj`)) && !ADDONS.some((a) => a.name === e.name))
    .map((e) => e.name);
}

// Source files that pass two `-addonsDir` flags to one launch. Returns the offending line numbers.
export function repeatedAddonsDirLines(text) {
  const lines = text.split("\n");
  const hits = [];
  lines.forEach((l, i) => {
    if (/flatMap\([^)]*\)\s*=>\s*\["-addonsDir"/.test(l)) hits.push(i + 1);
  });
  // two literal "-addonsDir" entries within 6 lines of each other in one array
  const at = lines.map((l, i) => (/["']-addonsDir["']/.test(l) ? i : -1)).filter((i) => i >= 0);
  for (let k = 1; k < at.length; k++) if (at[k] - at[k - 1] <= 6 && /^\s*["']-addonsDir["'],/.test(lines[at[k]]) && /^\s*["']-addonsDir["'],/.test(lines[at[k - 1]])) hits.push(at[k] + 1);
  return hits;
}

export function selftestAddons() {
  const f = [];
  const a = addonsDirArgs(["C:\r\addons", "", "C:\j"]);
  if (a.length !== 2 || a[0] !== "-addonsDir" || a[1] !== "C:\r\addons,C:\j") f.push(`addonsDirArgs is not one comma-separated flag: ${JSON.stringify(a)}`);
  if (resolveAddon("lf_everon1989")?.guid !== "B1E84D5687E11724") f.push("resolveAddon by name");
  if (resolveAddon("79636e668ea37ac9")?.name !== "LifeFramework") f.push("resolveAddon by GUID");
  if (resolveAddon("Nope") !== null || resolveAddon("") !== null) f.push("resolveAddon accepted an unknown addon");
  if (defaultPackOut("R", "A") === defaultPackOut("R", "B")) f.push("two addons share one default pack folder");
  if (!defaultPackOut("R", "LF_Everon1989").endsWith("LF_Everon1989")) f.push("default pack folder is not named after the addon");
  if (repeatedAddonsDirLines('const x = [\n  "-addonsDir", a,\n  "-addonsDir", b,\n];').length !== 1) f.push("two -addonsDir entries not flagged");
  if (repeatedAddonsDirLines('...dirs.flatMap((d) => ["-addonsDir", d]),').length !== 1) f.push("a flatMap of -addonsDir not flagged");
  if (repeatedAddonsDirLines('const x = ["-addonsDir", a, "-addons", "X"];').length !== 0) f.push("a single -addonsDir flagged");
  // rdbAddons: the empty pack stays out (today's single-core behaviour); a pack with a resource joins.
  const tmp = mkdtempSync(join(tmpdir(), "lf-addons-"));
  try {
    for (const a of ADDONS) {
      mkdirSync(addonDir(tmp, a.name), { recursive: true });
      writeFileSync(gprojOf(tmp, a.name), "GameProject {}\n");
    }
    const list = rdbAddons(tmp).join(",");
    if (list !== CORE) f.push(`an empty pack joined the rdb list: ${list}`);
    writeFileSync(join(addonDir(tmp, "LF_Everon1989"), "x.et.meta"), 'MetaFileClass {\n Name "{0123456789ABCDEF}x.et"\n}\n');
    const both = rdbAddons(tmp).join(",");
    if (both !== "LifeFramework,LF_Everon1989") f.push(`a pack with a resource left the rdb list: ${both}`);
  } finally {
    try { rmSync(tmp, { recursive: true, force: true }); } catch {}
  }
  return f;
}
