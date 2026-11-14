// tools\cli staff roster check | admins [--file <staff.txt>]   the staff roster (#1320, on top of #1287)
//
// One roster, the file the game already reads: $profile:LifeFramework/staff.txt (LF_PoliceApplicationStaff.ReadRoster).
// The repo copy is server/flagship/staff.txt; it is copied to the server profile. Format, one entry per line:
//   <uid> [level] [# display name]
//   # a comment                          (blank lines and comments are skipped)
//   #pending <level> <display name>      (a person whose uid is not known yet; the game ignores it as a comment)
// level is moderator, operator or administrator and is optional (a line without one is plain staff, as before).
// uid is 1..64 of [A-Za-z0-9_-], the same test as LF_PoliceApplicationStaff.IsValidUid.
// `check` fails on an unknown level, a malformed or duplicate uid, and warns on a #pending entry.
// `admins` prints the uids with level administrator as the JSON array for `game.admins` in server.json,
// and fails while an administrator is still #pending. The engine admin flag counts as staff too, so the
// maintainer in game.admins needs no line here to use the staff tools.
// Readers in Node: import { parseRoster, adminIds } from this file. In the game: LF_PoliceApplicationStaff.
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";

export const help = "staff roster check | admins [--file f]   validate the staff roster (server/flagship/staff.txt); print the administrators' uids as the game.admins array";

export const LEVELS = ["moderator", "operator", "administrator"];
const UID = /^[A-Za-z0-9_-]{1,64}$/;
export const isValidUid = (uid) => UID.test(uid);

// text -> [{ line, uid, level, name, pending }]; uid is "" for a #pending entry
export function parseRoster(text) {
  const out = [];
  text.split(/\r?\n/).forEach((raw, i) => {
    const line = i + 1;
    const pending = /^\s*#pending\s+(\S+)\s*(.*)$/.exec(raw);
    if (pending) {
      out.push({ line, uid: "", level: pending[1], name: pending[2].trim(), pending: true });
      return;
    }
    const hash = raw.indexOf("#");
    const code = (hash >= 0 ? raw.slice(0, hash) : raw).trim();
    if (!code) return;
    const m = /^(\S+)\s*(.*)$/.exec(code);
    out.push({ line, uid: m[1], level: m[2].trim(), name: hash >= 0 ? raw.slice(hash + 1).trim() : "", pending: false });
  });
  return out;
}

// { errors, warnings } for parsed entries
export function checkRoster(entries) {
  const errors = [];
  const warnings = [];
  const seen = new Map();
  for (const e of entries) {
    const at = `line ${e.line}${e.name ? ` (${e.name})` : ""}`;
    if (e.level && !LEVELS.includes(e.level)) errors.push(`${at}: unknown level ${JSON.stringify(e.level)}; use ${LEVELS.join(", ")}`);
    if (e.pending) {
      warnings.push(`${at}: ${e.level} has no uid yet; fill it in before this person has any powers`);
      continue;
    }
    if (!isValidUid(e.uid)) {
      errors.push(`${at}: malformed uid ${JSON.stringify(e.uid)}; expected 1-64 of letters, digits, - and _`);
      continue;
    }
    const key = e.uid.toLowerCase();
    if (seen.has(key)) errors.push(`${at}: repeats the uid of line ${seen.get(key)}`);
    else seen.set(key, e.line);
  }
  return { errors, warnings };
}

// { ids, errors, warnings } for `admins`: an administrator still #pending is an error here
export function adminIds(entries) {
  const { errors, warnings } = checkRoster(entries);
  const ids = [];
  for (const e of entries) {
    if (e.level !== "administrator") continue;
    if (e.pending) errors.push(`line ${e.line}${e.name ? ` (${e.name})` : ""}: the administrator has no uid yet, so game.admins cannot be written`);
    else if (isValidUid(e.uid) && !ids.includes(e.uid)) ids.push(e.uid);
  }
  return { ids, errors, warnings };
}

export async function main(args, { root }) {
  const [group, sub] = args;
  if (group !== "roster" || !["check", "admins"].includes(sub)) {
    console.log("usage: tools\\cli staff roster check|admins [--file <staff.txt>]");
    return 1;
  }
  const fi = args.indexOf("--file");
  const file = fi >= 0 ? args[fi + 1] : join(root, "server", "flagship", "staff.txt");
  if (!file || !existsSync(file)) {
    console.log(`FAIL  roster not found: ${file}`);
    return 1;
  }
  const entries = parseRoster(readFileSync(file, "utf8"));
  if (sub === "check") {
    const { errors, warnings } = checkRoster(entries);
    for (const w of warnings) console.log(`WARN  ${w}`);
    for (const e of errors) console.log(`FAIL  ${e}`);
    if (errors.length) return 1;
    console.log(`staff roster: ok (${entries.length} entr${entries.length === 1 ? "y" : "ies"}, ${warnings.length} warning${warnings.length === 1 ? "" : "s"})`);
    return 0;
  }
  const { ids, errors } = adminIds(entries);
  if (errors.length) {
    for (const e of errors) console.log(`FAIL  ${e}`);
    return 1;
  }
  console.log(JSON.stringify(ids));
  return 0;
}
