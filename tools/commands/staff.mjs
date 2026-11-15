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
// `add <uid> <level> --name "<display>"`, `remove <uid>` and `promote <name-of-pending> <uid>` edit the roster in
// place (#1466): comments, blank lines and order stay, a duplicate or malformed entry is refused with the file
// untouched, and `check` runs after. `add` appends; `promote` rewrites the `#pending` line where it stands.
// Readers in Node: import { parseRoster, adminIds } from this file. In the game: LF_PoliceApplicationStaff.
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";

export const help = "staff roster check | admins | add <uid> <level> --name n | remove <uid> | promote <pending-name> <uid>  [--file f]   the staff roster (server/flagship/staff.txt): validate, print the game.admins array, or edit it in place";

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

// ---- edits (#1466): pure text -> { text } | { error }; comments, blank lines and order are kept ----
const eolOf = (text) => (text.includes("\r\n") ? "\r\n" : "\n");
const sameUid = (a, b) => a.toLowerCase() === b.toLowerCase();
const cleanName = (name) => (name || "").trim();
const badName = (name) => /[\r\n\0]/.test(name);
const entryLine = (uid, level, name) => `${uid} ${level}${name ? ` # ${name}` : ""}`;

function checkUidLevel(uid, level) {
  if (!uid || !isValidUid(uid)) return `malformed uid ${JSON.stringify(uid || "")}; expected 1-64 of letters, digits, - and _`;
  if (!LEVELS.includes(level)) return `level must be one of ${LEVELS.join(", ")}`;
  return "";
}

// a uid already on the roster (case-insensitive, like check) is refused
const uidTaken = (text, uid) => parseRoster(text).find((e) => !e.pending && sameUid(e.uid, uid));

export function addEntry(text, uid, level, name) {
  const bad = checkUidLevel(uid, level);
  if (bad) return { error: bad };
  name = cleanName(name);
  if (badName(name)) return { error: "the display name must be one line" };
  const taken = uidTaken(text, uid);
  if (taken) return { error: `${uid} is already on the roster (line ${taken.line}); remove it first to change its level` };
  const eol = eolOf(text);
  const body = text.replace(/(\r?\n)+$/, "");
  return { text: (body ? body + eol : "") + entryLine(uid, level, name) + eol };
}

export function removeEntry(text, uid) {
  const hit = uidTaken(text, uid || "");
  if (!hit) return { error: `${uid || "(no uid)"} is not on the roster` };
  const lines = text.split(/\r?\n/);
  lines.splice(hit.line - 1, 1);
  return { text: lines.join(eolOf(text)), removed: hit };
}

// turn the `#pending <level> <name>` line called <name> into a real uid line, in place
export function promoteEntry(text, pendingName, uid) {
  pendingName = cleanName(pendingName);
  const matches = parseRoster(text).filter((e) => e.pending && e.name.toLowerCase() === pendingName.toLowerCase());
  if (!pendingName || !matches.length) return { error: `no #pending entry called ${JSON.stringify(pendingName)}` };
  if (matches.length > 1) return { error: `${matches.length} #pending entries are called ${JSON.stringify(pendingName)} (lines ${matches.map((m) => m.line).join(", ")}); edit the file by hand` };
  const [hit] = matches;
  const bad = checkUidLevel(uid, hit.level);
  if (bad) return { error: bad };
  const taken = uidTaken(text, uid);
  if (taken) return { error: `${uid} is already on the roster (line ${taken.line})` };
  const lines = text.split(/\r?\n/);
  lines[hit.line - 1] = entryLine(uid, hit.level, hit.name);
  return { text: lines.join(eolOf(text)) };
}

// `--file f` and `--name n` out of the argument list; the rest are positionals
function takeFlags(args) {
  const flags = {};
  const rest = [];
  for (let i = 0; i < args.length; i++) {
    if (args[i] === "--file" || args[i] === "--name") flags[args[i].slice(2)] = args[++i];
    else rest.push(args[i]);
  }
  return { flags, rest };
}

function printCheck(entries) {
  const { errors, warnings } = checkRoster(entries);
  for (const w of warnings) console.log(`WARN  ${w}`);
  for (const e of errors) console.log(`FAIL  ${e}`);
  if (errors.length) return 1;
  console.log(`staff roster: ok (${entries.length} entr${entries.length === 1 ? "y" : "ies"}, ${warnings.length} warning${warnings.length === 1 ? "" : "s"})`);
  return 0;
}

function edit(sub, rest, flags, file) {
  const text = readFileSync(file, "utf8");
  let r;
  if (sub === "add") r = rest.length === 2 ? addEntry(text, rest[0], rest[1], flags.name) : { error: "usage: staff add <uid> <level> --name \"<display>\"" };
  else if (sub === "remove") r = rest.length === 1 ? removeEntry(text, rest[0]) : { error: "usage: staff remove <uid>" };
  else r = rest.length === 2 ? promoteEntry(text, rest[0], rest[1]) : { error: "usage: staff promote <name-of-pending> <uid>" };
  if (r.error) {
    console.log(`FAIL  ${r.error}`);
    return 1;
  }
  // never make the roster worse: a result with more errors than the file had is not written
  const before = checkRoster(parseRoster(text)).errors.length;
  const after = checkRoster(parseRoster(r.text)).errors;
  if (after.length > before) {
    for (const e of after) console.log(`FAIL  ${e}`);
    return 1;
  }
  writeFileSync(file, r.text);
  console.log(`staff ${sub}: wrote ${file}`);
  return printCheck(parseRoster(r.text));
}

export async function main(args, { root }) {
  const { flags, rest: positional } = takeFlags(args);
  const [group, sub] = positional;
  const editing = ["add", "remove", "promote"].includes(group);
  if (!editing && (group !== "roster" || !["check", "admins"].includes(sub))) {
    console.log("usage: tools\\cli staff roster check|admins [--file <staff.txt>]");
    console.log("       tools\\cli staff add <uid> <level> --name \"<display>\" | remove <uid> | promote <name-of-pending> <uid>  [--file <staff.txt>]");
    return 1;
  }
  const file = flags.file !== undefined ? flags.file : join(root, "server", "flagship", "staff.txt");
  if (!file || !existsSync(file)) {
    console.log(`FAIL  roster not found: ${file}`);
    return 1;
  }
  if (editing) return edit(group, positional.slice(1), flags, file);
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
