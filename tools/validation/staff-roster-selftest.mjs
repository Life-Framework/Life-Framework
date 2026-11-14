// Proves tools/commands/staff.mjs (`staff roster check|admins`) on fixture rosters (#1320). The roster is
// the file the game reads, $profile:LifeFramework/staff.txt (#1287): `<uid> [level] [# name]`.
//   1. one administrator: `admins` prints that uid as the game.admins array; a legacy line (a bare uid)
//      is still accepted;
//   2. an unknown level, a duplicate uid and a malformed uid fail `check`;
//   3. a #pending entry (no uid yet) is a warning on `check` and an error on `admins`;
//   4. the committed server/flagship/staff.txt passes `check` (maintainer pending, warned).
// inputs: tools/**, server/**
// red-proof: 2026-11-14, with the duplicate-uid test in checkRoster removed this printed
// "FAIL staff-roster: duplicate uid fails check: ok != fail".
import { mkdtempSync, writeFileSync } from "node:fs";
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
if (!/WARN/.test(r.out)) failures.push(`committed roster: the pending maintainer should warn:\n${r.out}`);
eq(failed(await run(["roster", "admins"])), "fail", "committed roster: admins waits for the maintainer's uid");
eq(failed(await run(["roster"])), "fail", "no subcommand prints usage and fails");

if (failures.length) {
  for (const f of failures) console.log(`FAIL staff-roster: ${f}`);
  process.exit(1);
}
console.log("staff-roster selftest: ok");
