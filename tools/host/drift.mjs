// The drift check of `tools\cli host drift` (#1462). The live config is in the repo (server/flagship/), so a hand
// edit on the host is drift. This reads the host profile's config.json and server.json, diffs each against its
// repo file field by field, and ignores the host-only secret fields (docs/ops/hotfix.md, server/flagship/README.md):
// game.password, game.passwordAdmin, game.admins and the whole `staff` section. It only reads; `host config apply`
// (#1322) writes the files. An ignored field is never printed, so a drift report is safe to paste.
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { hostPaths } from "./host.mjs";

// Where the game reads the owner config ($profile:LifeFramework/config.json, -profile <dir>\profile) and where
// the host keeps the dedicated-server config.
export function configJsonPath(dir) {
  return join(hostPaths(dir).profile, "profile", "LifeFramework", "config.json");
}
export function serverJsonPath(dir) {
  return join(hostPaths(dir).profile, "server.json");
}

// Dotted field paths the host owns. A path ignores itself and everything under it.
export const HOST_ONLY = {
  "config.json": ["staff"],
  "server.json": ["game.password", "game.passwordAdmin", "game.admins"],
};

function ignored(file, field) {
  return (HOST_ONLY[file] ?? []).some((p) => field === p || field.startsWith(`${p}.`) || field.startsWith(`${p}[`));
}

// { "a.b": 1, "a.c[0]": "x" }: every leaf, arrays by index; an empty object or array is a leaf.
export function flatten(v, path = "", out = {}) {
  if (Array.isArray(v) && v.length) v.forEach((x, i) => flatten(x, `${path}[${i}]`, out));
  else if (v && typeof v === "object" && Object.keys(v).length) {
    for (const [k, x] of Object.entries(v)) flatten(x, path ? `${path}.${k}` : k, out);
  } else out[path] = Array.isArray(v) ? [] : v && typeof v === "object" ? {} : v;
  return out;
}

const show = (v) => (v === undefined ? "(absent)" : JSON.stringify(v));

function load(path) {
  if (!existsSync(path)) return { missing: true };
  try { return { json: JSON.parse(readFileSync(path, "utf8")) }; } catch (e) { return { bad: e.message }; }
}

// Diffs one repo file against its host copy. Returns the changes: { file, field, repo, host } where `field` is a
// dotted path, or null for a whole-file problem (missing, not JSON) with `note` saying which.
export function diffFile(file, repoPath, hostPath) {
  const repo = load(repoPath);
  if (repo.missing || repo.bad) return [{ file, field: null, note: `the repo file ${repoPath} ${repo.missing ? "is missing" : `is not JSON: ${repo.bad}`}` }];
  const host = load(hostPath);
  if (host.missing) return [{ file, field: null, note: `the host file ${hostPath} is missing` }];
  if (host.bad) return [{ file, field: null, note: `the host file ${hostPath} is not JSON: ${host.bad}` }];
  const a = flatten(repo.json), b = flatten(host.json);
  const changes = [];
  for (const field of [...new Set([...Object.keys(a), ...Object.keys(b)])].sort()) {
    if (ignored(file, field)) continue;
    if (JSON.stringify(a[field]) !== JSON.stringify(b[field])) changes.push({ file, field, repo: a[field], host: b[field] });
  }
  return changes;
}

// Drift for a host dir against the repo's flagship folder. `flagship` is the folder holding config.json and
// server.json (default <root>/server/flagship). Returns { ok, changes }.
export function driftHost(dir, { flagship }) {
  const changes = [
    ...diffFile("config.json", join(flagship, "config.json"), configJsonPath(dir)),
    ...diffFile("server.json", join(flagship, "server.json"), serverJsonPath(dir)),
  ];
  return { ok: changes.length === 0, changes };
}

export function formatChange(c) {
  if (c.field === null) return `${c.file}: ${c.note}`;
  return `${c.file} ${c.field}: repo ${show(c.repo)}, host ${show(c.host)}`;
}
