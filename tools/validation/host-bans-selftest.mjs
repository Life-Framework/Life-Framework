// tools/validation/host-bans-selftest.mjs
// inputs: tools/host/bans.mjs, tools/host/commands/bans.mjs, tools/validation/fixtures/host-bans/**
//
// Proves `tools\cli host bans` (#1609): a fixture ban record in the V1 row format of LF_BanRecord.c
// (#1285). A ban then an unban shows as closed, `--open` hides it, a later ban of the same player is
// open again, a malformed row (wrong field count, unknown version, unknown action, bad time) is
// reported with its line number and skipped, and `--player` keeps one uid's rows.
// red-proof: before tools/host/bans.mjs existed this printed
// "FAIL cannot load tools/host/bans.mjs or commands/bans.mjs: ERR_MODULE_NOT_FOUND" (2026-11-15).

import { readFileSync, mkdtempSync, mkdirSync, copyFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "validation", "fixtures", "host-bans");
let failed = 0;

function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

function check(cond, msg) {
  if (!cond) fail(msg);
}

let bans;
let cmd;
try {
  bans = await import(pathToFileURL(join(ROOT, "tools", "host", "bans.mjs")).href);
  cmd = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "bans.mjs")).href);
} catch (e) {
  fail(`cannot load tools/host/bans.mjs or commands/bans.mjs: ${e.code ?? e.message}`);
  console.log(`SUMMARY host-bans-selftest fail ${failed}`);
  process.exit(1);
}

const text = readFileSync(join(FIX, "ban_record.txt"), "utf8");
const parsed = bans.parseBanRecord(text);

// Rows: 5 good (4 BAN + 1 UNBAN), 4 skipped: lines 3, 7, 8, 10 (line 9 is blank and not an error).
check(parsed.rows.length === 5, `parsed ${parsed.rows.length} rows, want 5`);
const skippedLines = parsed.skipped.map((s) => s.line).join(",");
check(skippedLines === "3,7,8,10", `skipped lines ${skippedLines}, want 3,7,8,10`);
check(parsed.skipped.every((s) => typeof s.reason === "string" && s.reason), "every skipped row carries a reason");

const first = parsed.rows[0];
check(first && first.action === "BAN" && first.uid === "aaaa-1111" && first.time === 1700000000 && first.staff === "staff-1" && first.duration === 0 && first.reason === "Griefing the bank", `row 1 fields: ${JSON.stringify(first)}`);
check(first && JSON.stringify(first.incidents) === "[3,7]", `row 1 incidents ${JSON.stringify(first?.incidents)}, want [3,7]`);
check(parsed.rows[1]?.incidents.length === 0 && parsed.rows[1]?.duration === 86400, "row 2: no incidents, 86400 s");

// Status: ban 1 of aaaa is closed by the unban; the second ban of aaaa is open; bbbb and cccc are open.
const all = bans.listBans(parsed.rows, {});
check(all.length === 5, `all lists ${all.length} entries, want 5`);
const statuses = all.map((e) => `${e.row.uid}:${e.row.action}:${e.status}`).join(" ");
check(statuses === "aaaa-1111:BAN:closed bbbb-2222:BAN:open aaaa-1111:UNBAN:lifted cccc-3333:BAN:open aaaa-1111:BAN:open", `statuses: ${statuses}`);
check(all[0].closedBy?.time === 1700005000 && all[0].closedBy?.staff === "staff-2", "closed ban names the unban that closed it");

const open = bans.listBans(parsed.rows, { open: true });
const openUids = open.map((e) => e.row.uid).join(",");
check(openUids === "bbbb-2222,cccc-3333,aaaa-1111", `--open: ${openUids}, want bbbb-2222,cccc-3333,aaaa-1111`);
check(open.every((e) => e.row.action === "BAN"), "--open shows bans only");

const one = bans.listBans(parsed.rows, { player: "aaaa-1111" });
check(one.length === 3, `--player aaaa-1111 lists ${one.length}, want 3`);
check(bans.listBans(parsed.rows, { player: "aaaa-1111", open: true }).length === 1, "--player with --open: only the second ban");
check(bans.listBans(parsed.rows, { player: "nobody" }).length === 0, "--player with no rows lists none");

// An unban for a player never banned is listed, never crashes.
const stray = bans.parseBanRecord("V1|UNBAN|zzzz-9|1700000000|rcon|0||\n");
check(stray.rows.length === 1 && bans.listBans(stray.rows, {})[0].status === "lifted", "a lone unban is listed");

// The command, against a temp host dir: lines, line-numbered warning, exit code, --open, --player, no file.
function run(args, hostDir) {
  const lines = [];
  const orig = console.log;
  console.log = (...a) => lines.push(a.join(" "));
  let code;
  return Promise.resolve(cmd.main(args, { root: ROOT, hostDir }))
    .then((c) => {
      code = c;
    })
    .finally(() => {
      console.log = orig;
    })
    .then(() => ({ code, out: lines.join("\n") }));
}

const host = mkdtempSync(join(tmpdir(), "lf-host-bans-"));
try {
  mkdirSync(join(host, "profile", "LifeFramework"), { recursive: true });
  copyFileSync(join(FIX, "ban_record.txt"), join(host, "profile", "LifeFramework", "ban_record.txt"));

  const r1 = await run([], host);
  check(r1.code === 0, `host bans exit ${r1.code}, want 0`);
  check(r1.out.includes("Griefing the bank") && r1.out.includes("staff-1") && r1.out.includes("3,7"), "a row prints its reason, staff uid and incident ids");
  check(r1.out.includes("Appeal accepted"), "the unban row prints");
  check(/line 3\b/.test(r1.out) && /line 10\b/.test(r1.out) && /line 7\b/.test(r1.out), "malformed rows are reported with their line numbers");
  check(!r1.out.includes("A layout this reader does not know"), "a skipped row is not printed as a ban");
  check(/SUMMARY host-bans ok rows=5 skipped=4/.test(r1.out), `summary line: ${r1.out.split("\n").filter((l) => l.startsWith("SUMMARY")).join("|")}`);

  const r2 = await run(["--open"], host);
  check(!r2.out.includes("Appeal accepted") && !r2.out.includes("Griefing the bank"), "--open hides the closed ban and its unban");
  check(r2.out.includes("Spam in chat") && r2.out.includes("Second offence") && r2.out.includes("Cheating"), "--open shows the open bans");

  const r3 = await run(["--player", "cccc-3333"], host);
  check(r3.out.includes("Cheating") && !r3.out.includes("Spam in chat"), "--player keeps one uid");

  const r4 = await run(["--player"], host);
  check(r4.code !== 0, "--player with no uid is a usage error");

  const empty = mkdtempSync(join(tmpdir(), "lf-host-bans-empty-"));
  try {
    const r5 = await run([], empty);
    check(r5.code === 1 && r5.out.includes("[LFHost:bans]"), `no ban record: exit ${r5.code}, want 1 with the [LFHost:bans] prefix`);
  } finally {
    rmSync(empty, { recursive: true, force: true });
  }
} finally {
  rmSync(host, { recursive: true, force: true });
}

console.log(failed ? `SUMMARY host-bans-selftest fail ${failed}` : "SUMMARY host-bans-selftest ok");
process.exit(failed ? 1 : 0);