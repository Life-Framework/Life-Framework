// The wipe step of `tools\cli host wipe --confirm <host dir name>` (#1274).
//
// A wipe is a deliberate maintainer decision (M4: any time during the alpha, announced 24 h
// ahead on Discord; `host announce --wipe` enforces the notice). It backs the saves up with the
// `wipe` tag, moves the save root aside (nothing is deleted), leaves an empty save root, logs
// the step to backups/wipes.log and says how to restore.
//
// The wipe notice (#1413): the wipe refuses unless `host announce --wipe --send` recorded a wipe
// notice (the `next` row of website/content/sessions.json, `wipe: true`) at least 24 hours before
// now, and says when the wipe becomes allowed. This file reads the notice and never writes it.
// The posting time is `next.announced` as an ISO date-time (sessions.schema.json); the names
// `announcedAt`, `noticeAt` and `postedAt` are read first (the opening-day script ages those).
import { appendFileSync, existsSync, mkdirSync, readFileSync, renameSync } from "node:fs";
import { basename, dirname, join } from "node:path";
import { WIPE_NOTICE_MS } from "./announce.mjs";
import { backupSaves, freshDir, hostPaths, pidAlive, readHostState, saveRoot, stamp } from "./host.mjs";

export const WIPES_LOG = "wipes.log";

function refuse(reason) {
  return { ok: false, reason };
}

const TIME_FIELDS = ["announcedAt", "noticeAt", "postedAt", "announced"];

// Reads the wipe notice from the sessions.json at `file`. { ok: true, postedAt, allowedAt } when
// the notice is at least 24 h old at `now`; otherwise { ok: false, reason, allowedAt? } where
// allowedAt (ISO) is when the notice, once old enough, allows the wipe.
export function checkWipeNotice(file, now = new Date()) {
  const hint = "post it first: tools\\cli host announce --wipe --at <iso> --send (a wipe needs 24 hours of notice on Discord)";
  if (!file || !existsSync(file)) return { ok: false, reason: `no wipe notice: ${file || "no sessions.json"} does not exist; ${hint}` };
  let next;
  try {
    next = JSON.parse(readFileSync(file, "utf8"))?.next;
  } catch (e) {
    return { ok: false, reason: `no wipe notice: ${file} is not readable JSON (${e.message})` };
  }
  if (!next || next.wipe !== true) return { ok: false, reason: `no wipe notice: the next session in ${file} is ${next ? "not a wipe" : "not scheduled"}; ${hint}` };
  let posted = null;
  for (const f of TIME_FIELDS) {
    const v = next[f];
    if (typeof v === "string" && v !== "") { posted = { f, v }; break; }
  }
  if (!posted) {
    // `host announce` leaves `announced` out of a draft; an older file may hold announced=false.
    if (next.announced === undefined || next.announced === null || next.announced === false) return { ok: false, reason: `the wipe notice is drafted, not posted (no announced time in ${file}); ${hint}` };
    return { ok: false, reason: `the wipe notice has no posting time (${file} has announced=${JSON.stringify(next.announced)}, not an ISO date-time): the 24 hours cannot be counted` };
  }
  const ms = /^\d{4}-\d{2}-\d{2}T/.test(posted.v) ? Date.parse(posted.v) : NaN;
  if (Number.isNaN(ms)) return { ok: false, reason: `the wipe notice time ${posted.f}=${JSON.stringify(posted.v)} is not an ISO date-time` };
  const allowed = new Date(ms + WIPE_NOTICE_MS);
  if (now.getTime() < allowed.getTime()) {
    const hours = ((allowed.getTime() - now.getTime()) / 3600000).toFixed(1);
    const allowedAt = allowed.toISOString();
    return { ok: false, allowedAt, reason: `the wipe notice was posted ${new Date(ms).toISOString()}; a wipe needs 24 hours of notice, so it is allowed from ${allowedAt} (in ${hours} h)` };
  }
  return { ok: true, postedAt: new Date(ms).toISOString(), allowedAt: allowed.toISOString() };
}

// Wipes the host's saves. Returns { ok: true, backup, aside, restore } (backup is null when there
// was nothing to wipe), or { ok: false, reason } and nothing touched.
// `confirm` must equal the host dir's folder name exactly. `sessions` is the sessions.json that holds the wipe notice.
// A refusal for a young notice carries `allowedAt`.
export function wipeHost(dir, { confirm, now = new Date(), sessions } = {}) {
  const name = basename(dir);
  if (confirm !== name) return refuse(`confirm does not match: type the host folder name exactly (--confirm ${name})`);
  const notice = checkWipeNotice(sessions, now);
  if (!notice.ok) return notice;
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
