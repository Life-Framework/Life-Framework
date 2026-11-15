// The wipe step of `tools\cli host wipe --confirm <host dir name>` (#1274).
//
// A wipe is a deliberate maintainer decision (M4: any time during the alpha, announced 24 h
// ahead on Discord; `host announce --wipe` enforces the notice). It backs the saves up with the
// `wipe` tag, moves the save root aside (nothing is deleted), leaves an empty save root, logs
// the step to backups/wipes.log and says how to restore.
import { appendFileSync, existsSync, mkdirSync, renameSync } from "node:fs";
import { basename, dirname, join } from "node:path";
import { backupSaves, freshDir, hostPaths, pidAlive, readHostState, saveRoot, stamp } from "./host.mjs";

export const WIPES_LOG = "wipes.log";

function refuse(reason) {
  return { ok: false, reason };
}

// Wipes the host's saves. Returns { ok: true, backup, aside, restore } (backup is null when there
// was nothing to wipe), or { ok: false, reason } and nothing touched.
// `confirm` must equal the host dir's folder name exactly.
export function wipeHost(dir, { confirm, now = new Date() } = {}) {
  const name = basename(dir);
  if (confirm !== name) return refuse(`confirm does not match: type the host folder name exactly (--confirm ${name})`);
  const state = readHostState(dir);
  if (state && pidAlive(state.pid)) return refuse(`the host is running (pid ${state.pid}): stop it first (tools\\cli host stop)`);
  const { profile, backups } = hostPaths(dir);
  const root = saveRoot(profile);
  if (!root) return { ok: true, backup: null, aside: null, restore: null, note: "no saves under the host profile: nothing to wipe" };

  const backup = backupSaves(dir, "wipe", now);
  if (!backup) return refuse("the wipe backup could not be written");
  // The backup is a copy; the original moves aside so a bad copy still leaves the saves.
  const aside = freshDir(join(backups, `${stamp(now)}-wiped-saves`));
  renameSync(root, aside);
  mkdirSync(root, { recursive: true });
  const row = `${now.toISOString()} wipe host=${name} backup=${basename(backup)} aside=${basename(aside)}\n`;
  appendFileSync(join(backups, WIPES_LOG), row);
  const restore = `tools\\cli host restore ${basename(backup)}   (the wiped saves also sit untouched in backups\\${basename(aside)})`;
  return { ok: true, backup, aside, restore };
}
