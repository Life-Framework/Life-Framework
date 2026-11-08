// tools/lib/rdb-fresh.mjs - is resourceDatabase.rdb missing resources the addon has?
//
// resourceDatabase.rdb is gitignored and only Workbench writes it. A worktree
// copies the main checkout's file, so a prefab that landed on main after the
// last Workbench run there is unknown to it: the server logs `Wrong GUID/name`,
// replication refuses the entity, and its inventory operations never complete.
//
// The rdb stores each registered resource as its addon-relative path string
// (e.g. `Prefabs/Props/Hideouts/LF_HideoutStash.et`) followed by its GUID as
// 8 little-endian bytes (FD1CCD705B12BBCB -> CB BB 12 5B 70 CD 1C FD).
// Observed on Workbench 1.9.0.37. A .meta whose path or GUID is absent from the
// file is unregistered. Reading ~370 metas and a 75 KB file takes ~30 ms.

import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { join, relative } from "node:path";

export const RDB_NAME = "resourceDatabase.rdb";

// Folders Workbench never registers (generated, not resources).
const SKIP_DIRS = new Set([".git", "node_modules", "tmp"]);

function walkMetas(dir, out = []) {
  let entries;
  try {
    entries = readdirSync(dir, { withFileTypes: true });
  } catch {
    return out;
  }
  for (const e of entries) {
    if (e.isDirectory()) {
      if (!SKIP_DIRS.has(e.name)) walkMetas(join(dir, e.name), out);
    } else if (e.name.endsWith(".meta")) {
      out.push(join(dir, e.name));
    }
  }
  return out;
}

// The `{GUID}` in a .meta's `Name "{GUID}path"` line, or null.
export function metaGuid(text) {
  const m = /Name\s+"\{([0-9A-Fa-f]{16})\}/.exec(text);
  return m ? m[1].toUpperCase() : null;
}

export function guidBytes(guid) {
  return Buffer.from(guid, "hex").reverse();
}

// The resources [{ path, guid }] the addon's .meta files declare.
export function addonResources(addonDir) {
  return walkMetas(addonDir).map((file) => {
    const path = relative(addonDir, file).slice(0, -".meta".length).split("\\").join("/");
    let guid = null;
    try {
      guid = metaGuid(readFileSync(file, "utf8"));
    } catch {}
    return { path, guid };
  });
}

// ASCII lower-case copy: Windows paths are case-insensitive, and a checkout's
// folder casing can differ from the one Workbench saw (`Data/` vs `data/`).
function lowerAscii(buf) {
  const out = Buffer.from(buf);
  for (let i = 0; i < out.length; i++) if (out[i] >= 65 && out[i] <= 90) out[i] += 32;
  return out;
}

// Pure core: which resources are missing from an rdb buffer.
export function missingFrom(rdb, resources) {
  const lower = lowerAscii(rdb);
  return resources.filter(({ path, guid }) => !lower.includes(Buffer.from(path.toLowerCase(), "utf8")) || (guid && !rdb.includes(guidBytes(guid))));
}

// { exists, missing: [{path, guid}], total, rdbPath }
export function rdbFreshness(addonDir) {
  const rdbPath = join(addonDir, RDB_NAME);
  const resources = addonResources(addonDir);
  if (!existsSync(rdbPath)) return { exists: false, missing: resources, total: resources.length, rdbPath };
  const rdb = readFileSync(rdbPath);
  return { exists: true, missing: missingFrom(rdb, resources), total: resources.length, rdbPath, mtimeMs: statSync(rdbPath).mtimeMs };
}

export function describeMissing(missing, limit = 3) {
  const shown = missing.slice(0, limit).map((r) => r.path).join(", ");
  return missing.length > limit ? `${shown}, +${missing.length - limit} more` : shown;
}

// Selftest on synthetic buffers in the rdb layout; returns failure strings.
export function selftestRdbFresh() {
  const failures = [];
  const entry = (path, guid) => {
    const len = Buffer.alloc(4);
    len.writeUInt32LE(path.length);
    return Buffer.concat([len, Buffer.from(path), Buffer.from([0, 6, 0, 0, 0, 0, 0]), guidBytes(guid)]);
  };
  const stash = { path: "Prefabs/Props/Hideouts/LF_HideoutStash.et", guid: "FD1CCD705B12BBCB" };
  const conf = { path: "Configs/Police/LF_Police.conf", guid: "0123456789ABCDEF" };
  const rdb = Buffer.concat([Buffer.from("FORM\0\0\0\0RDBC"), entry(conf.path, conf.guid)]);

  if (guidBytes(stash.guid).toString("hex") !== "cbbb125b70cd1cfd") failures.push("guidBytes is not little-endian");
  if (metaGuid('MetaFileClass {\n Name "{fd1ccd705b12bbcb}Prefabs/x.et"\n') !== "FD1CCD705B12BBCB") failures.push("metaGuid did not read the Name line");
  if (metaGuid("MetaFileClass {\n}\n") !== null) failures.push("metaGuid invented a GUID");
  const miss = missingFrom(rdb, [stash, conf]);
  if (miss.length !== 1 || miss[0] !== stash) failures.push(`a registered resource counted missing or an unregistered one passed: ${JSON.stringify(miss)}`);
  if (missingFrom(rdb, [{ path: conf.path, guid: "1111111111111111" }]).length !== 1) failures.push("a re-GUIDed resource at a known path passed");
  if (missingFrom(Buffer.concat([rdb, entry(stash.path, stash.guid)]), [stash, conf]).length !== 0) failures.push("a refreshed rdb still reads stale");
  if (missingFrom(rdb, [{ path: conf.path, guid: null }]).length !== 0) failures.push("a .meta without a GUID line counted missing");
  if (missingFrom(rdb, [{ path: conf.path.toLowerCase(), guid: conf.guid }]).length !== 0) failures.push("folder casing (Data/ vs data/) counted missing");
  return failures;
}
