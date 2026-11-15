// Proves tools/commands/staff.mjs (`staff roster check|admins`) on fixture rosters (#1320). The roster is
// the file the game reads, $profile:LifeFramework/staff.txt (#1287): `<uid> [level] [# name]`.
//   1. one administrator: `admins` prints that uid as the game.admins array; a legacy line (a bare uid)
//      is still accepted;
//   2. an unknown level, a duplicate uid and a malformed uid fail `check`;
//   3. a #pending entry (no uid yet) is a warning on `check` and an error on `admins`;
//   4. the committed server/flagship/staff.txt passes `check` and `admins` prints the maintainer's uid;
//   5. `add`, `remove` and `promote` (#1466) edit a fixture roster in place, keep comments and order,
//      refuse a duplicate, bad or missing entry without touching the file, and run the check after.
// inputs: tools/**, server/**
// red-proof: 2026-11-14, with the duplicate-uid test in checkRoster removed this printed
// "FAIL staff-roster: duplicate uid fails check: ok != fail". Section 5 (#1466): 2026-11-15, before add,
// remove and promote existed it printed "FAIL staff-roster: add: exit: 1 != 0".
import { mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { main } from "../commands/staff.mjs";

const failures = [];
const eq = (got, want, msg) => {
  if (got !== want) failures.push(`${msg}: ${got} != ${want}`);
};
const failed = (r) => (r.code === 0 ? "ok" : "fail");

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const dir = mkdtempSync(join(tmpdir(), "lf-staff-"));
let n = 0;
const fixture = (...lines) => {
  const file = join(dir, `staff-${n++}.txt`);
  writeFileSync(file, lines.join("\n") + "\n");
  return file;
};

// run a subcommand, capturing what it prints
async function run(args) {
  const real = console.log;
  let out = "";
  console.log = (...a) => (out += a.join(" ") + "\n");
  let code;
  try {
    code = await main(args, { root });
  } finally {
    console.log = real;
  }
  return { code, out };
}

// 1: one administrator
let r = await run(["roster", "admins", "--file", fixture("# staff", "", "boss-1 administrator # Boss", "mod_2\tmoderator", "old-style-uid")]);
eq(r.code, 0, "one administrator: admins exit");
eq(r.out.trim(), JSON.stringify(["boss-1"]), "one administrator: game.admins array");
r = await run(["roster", "admins", "--file", fixture("a1 administrator", "76561198000000001 administrator  # Steam id")]);
eq(r.out.trim(), JSON.stringify(["a1", "76561198000000001"]), "two administrators");
r = await run(["roster", "check", "--file", fixture("boss-1 administrator # Boss", "mod_2 moderator", "op-3 operator", "legacy-4")]);
eq(r.code, 0, "good roster passes check (legacy bare uid included)");

// 2: bad rosters fail check
eq(failed(await run(["roster", "check", "--file", fixture("a1 overlord")])), "fail", "unknown level fails check");
eq(failed(await run(["roster", "check", "--file", fixture("a1 moderator", "b2 operator", "a1 operator")])), "fail", "duplicate uid fails check");
eq(failed(await run(["roster", "check", "--file", fixture("a|b moderator")])), "fail", "malformed uid fails check");
eq(failed(await run(["roster", "check", "--file", fixture("x".repeat(65))])), "fail", "over-long uid fails check");
eq(failed(await run(["roster", "check", "--file", fixture("#pending overlord Zed")])), "fail", "pending entry with an unknown level fails check");

// 3: pending warns on check, errors on admins
const pending = fixture("#pending administrator Boss", "mod-1 moderator");
r = await run(["roster", "check", "--file", pending]);
eq(r.code, 0, "pending: check exit");
if (!/WARN/.test(r.out)) failures.push(`pending: check should warn:\n${r.out}`);
eq(failed(await run(["roster", "admins", "--file", pending])), "fail", "pending administrator fails admins");
eq((await run(["roster", "admins", "--file", fixture("#pending moderator Mo")])).out.trim(), "[]", "a pending moderator does not block admins");

// 4: the committed roster
r = await run(["roster", "check"]);
eq(r.code, 0, "committed roster passes check");
if (/WARN/.test(r.out)) failures.push(`committed roster: the maintainer has a uid, check should not warn:\n${r.out}`);
r = await run(["roster", "admins"]);
eq(r.code, 0, "committed roster: admins exit");
if (!/"[0-9a-f-]{36}"/.test(r.out)) failures.push(`committed roster: admins should print the maintainer's uid:\n${r.out}`);
eq(failed(await run(["roster"])), "fail", "no subcommand prints usage and fails");

// 5: add / remove / promote edit the file in place (#1466): comments and order stay, check runs after
const fx = join(root, "tools", "validation", "fixtures", "staff-roster");
const base = readFileSync(join(fx, "base.txt"), "utf8");
const expected = (name) => readFileSync(join(fx, name), "utf8");
const copy = () => {
  const file = join(dir, `edit-${n++}.txt`);
  writeFileSync(file, base);
  return file;
};
const text = (file) => readFileSync(file, "utf8");

let f = copy();
r = await run(["add", "new-3", "operator", "--name", "Three", "--file", f]);
eq(r.code, 0, "add: exit");
eq(text(f), expected("after-add.txt"), "add: file keeps comments and order, new line last");
if (!/staff roster: ok/.test(r.out)) failures.push(`add: should run the roster check after:\n${r.out}`);
for (const dup of ["new-3", "NEW-3", "boss-1"]) {
  const before = text(f);
  r = await run(["add", dup, "moderator", "--name", "Again", "--file", f]);
  eq(r.code, 1, `duplicate add of ${dup}: refused`);
  eq(text(f), before, `duplicate add of ${dup}: file untouched`);
}
for (const bad of [["a|b", "moderator"], ["ok-9", "overlord"], ["ok-9"]]) {
  const before = text(f);
  eq(failed(await run(["add", ...bad, "--file", f])), "fail", `add ${bad.join(" ")}: refused`);
  eq(text(f), before, `add ${bad.join(" ")}: file untouched`);
}
f = copy();
eq(failed(await run(["add", "x-1", "moderator", "--name", "Two\nlines", "--file", f])), "fail", "add: a name with a line break is refused");
eq(text(f), base, "add: bad name leaves the file untouched");

f = copy();
r = await run(["promote", "mo", "new-5", "--file", f]);
eq(r.code, 0, "promote: exit");
eq(text(f), expected("after-promote.txt"), "promote: the #pending line becomes a uid line in place");
if (/WARN/.test(r.out)) failures.push(`promote: no pending left, check should not warn:\n${r.out}`);
f = copy();
eq(failed(await run(["promote", "Nobody", "new-5", "--file", f])), "fail", "promote: unknown pending name refused");
eq(failed(await run(["promote", "Mo", "boss-1", "--file", f])), "fail", "promote: a uid already on the roster refused");
eq(failed(await run(["promote", "Mo", "a|b", "--file", f])), "fail", "promote: malformed uid refused");
eq(failed(await run(["promote", "Two", "new-5", "--file", f])), "fail", "promote: a name that is not pending refused");
eq(text(f), base, "promote: refusals leave the file untouched");

f = copy();
r = await run(["remove", "mod_2", "--file", f]);
eq(r.code, 0, "remove: exit");
eq(text(f), expected("after-remove.txt"), "remove: only that line goes");
eq(failed(await run(["remove", "mod_2", "--file", f])), "fail", "remove: a uid not on the roster refused");
eq(text(f), expected("after-remove.txt"), "remove: refusal leaves the file untouched");

// CRLF rosters stay CRLF
f = join(dir, `crlf-${n++}.txt`);
writeFileSync(f, "# c\r\nboss-1 administrator\r\n");
await run(["add", "m-1", "moderator", "--file", f]);
eq(text(f), "# c\r\nboss-1 administrator\r\nm-1 moderator\r\n", "add: CRLF roster stays CRLF");

// the committed roster is never edited by the selftest
eq(failed(await run(["add", "x", "moderator", "--file", join(dir, "missing.txt")])), "fail", "add: a missing roster file fails");

if (failures.length) {
  for (const f of failures) console.log(`FAIL staff-roster: ${f}`);
  process.exit(1);
}
console.log("staff-roster selftest: ok");
