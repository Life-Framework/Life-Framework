// tools/validation/host-attendance-selftest.mjs
// inputs: tools/host/attendance.mjs, tools/host/commands/attendance.mjs, tools/validation/fixtures/host-attendance/**
//
// Proves `tools\cli host attendance`: a fixture server log (real engine line shapes) with 12 joins,
// 3 rejoins (one under a new display name) and 5 leaves, two of them glued mid-line onto another
// line, gives 9 unique players, a peak of 7 first reached at 02:08:00, and the minutes each player
// stayed; plus the --json shape the history and gate commands read, midnight rollover, a missing
// disconnect, and a log with nobody in it.
// red-proof: before tools/host/attendance.mjs existed this printed
// "FAIL cannot load tools/host/attendance.mjs: ERR_MODULE_NOT_FOUND" (2026-11-15).

import { readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "validation", "fixtures", "host-attendance");
let failed = 0;

function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

function check(cond, msg) {
  if (!cond) fail(msg);
}

let att;
let cmd;
try {
  att = await import(pathToFileURL(join(ROOT, "tools", "host", "attendance.mjs")).href);
  cmd = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "attendance.mjs")).href);
} catch (e) {
  fail(`cannot load tools/host/attendance.mjs or commands/attendance.mjs: ${e.code ?? e.message}`);
  console.log(`SUMMARY host-attendance-selftest fail ${failed}`);
  process.exit(1);
}

const text = readFileSync(join(FIX, "console.log"), "utf8");
const r = att.parseAttendance(text);

check(r.unique === 9, `unique players is ${r.unique}, want 9`);
check(r.joins === 12, `joins is ${r.joins}, want 12`);
check(r.rejoins === 3, `rejoins is ${r.rejoins}, want 3`);
check(r.leaves === 5, `leaves is ${r.leaves}, want 5`);
check(r.stillConnected === 7, `stillConnected is ${r.stillConnected}, want 7`);
check(r.peak?.count === 7, `peak count is ${r.peak?.count}, want 7`);
check(r.peak?.at === "02:08:00", `peak at ${r.peak?.at}, want 02:08:00 (the first time 7 were on)`);
check(r.unmatchedLeaves === 0, `unmatchedLeaves is ${r.unmatchedLeaves}, want 0`);

// Minutes stayed, summed across rejoins; the log ends at 02:30:30 for whoever was still on.
const want = { Anna: 29.5, Ben: 27.5, Cleo: 22.5, Dan: 10, Eve: 10, Finn: 25.5, Gus: 22.5, Hana: 18.5, Ivo: 15.5 };
for (const [name, minutes] of Object.entries(want)) {
  const p = r.players.find((x) => x.name === name || x.names.includes(name));
  check(!!p, `player ${name} not found`);
  if (p) check(p.minutes === minutes, `${name} stayed ${p.minutes} min, want ${minutes}`);
}
const cleo = r.players.find((x) => x.names.includes("Cleo_2"));
check(cleo && cleo.sessions === 2 && cleo.names.length === 2, "Cleo rejoined as Cleo_2: one player, two sessions, two names");
check(r.players.length === 9, `players list has ${r.players.length} rows, want 9`);

// Midnight rollover: the clock wraps, the minutes keep counting.
const night = [
  "23:58:00.000 NETWORK      : Player connected: connectionID=0",
  "23:58:00.001 NETWORK      : ### Connecting player: connectionID=0, Name=",
  "23:58:00.010   NETWORK      : ### Creating player: PlayerId=1, Name=Owl",
  "00:03:00.000 NETWORK      : Player disconnected: connectionID=0",
].join("\n");
const n = att.parseAttendance(night);
check(n.unique === 1 && n.players[0].minutes === 5, `rollover: ${n.players[0]?.minutes} min, want 5`);

// A join whose leave line is missing: the next join of the same PlayerId closes it, no double count.
const lost = [
  "01:00:00.000 NETWORK      : ### Creating player: PlayerId=3, Name=Moth",
  "01:10:00.000 NETWORK      : ### Creating player: PlayerId=3, Name=Moth",
].join("\n");
const l = att.parseAttendance(lost);
check(l.unique === 1 && l.joins === 2 && l.rejoins === 1 && l.peak.count === 1, `lost leave: ${JSON.stringify({ u: l.unique, j: l.joins, p: l.peak })}`);

// Nobody ever joined.
const empty = att.parseAttendance("02:00:00.000 SCRIPT       : LFPROBE beat=1 top=none dialog=0 world=1\n");
check(empty.unique === 0 && empty.peak === null && empty.players.length === 0, "empty log: no players, no peak");

// The command: text and --json over the fixture, a missing file is an error.
const lines = [];
const realLog = console.log;
console.log = (...a) => lines.push(a.join(" "));
let code;
let jsonCode;
let missingCode;
try {
  code = await cmd.main([join(FIX, "console.log")], { root: ROOT, hostDir: join(ROOT, "server", "host-does-not-exist") });
  const textOut = lines.splice(0);
  jsonCode = await cmd.main([join(FIX, "console.log"), "--json"], { root: ROOT, hostDir: join(ROOT, "server", "host-does-not-exist") });
  const jsonOut = lines.splice(0).join("\n");
  missingCode = await cmd.main([join(FIX, "no-such.log")], { root: ROOT, hostDir: join(ROOT, "server", "host-does-not-exist") });
  console.log = realLog;
  check(code === 0, `text run exit ${code}`);
  check(textOut.some((l) => /unique players:\s+9/.test(l)), "text output has 'unique players: 9'");
  check(textOut.some((l) => /peak concurrent:\s+7 at 02:08:00/.test(l)), "text output has 'peak concurrent: 7 at 02:08:00'");
  check(textOut.some((l) => l.startsWith("SUMMARY host-attendance ok unique=9 peak=7")), "text output ends with a SUMMARY host-attendance ok line");
  check(jsonCode === 0, `json run exit ${jsonCode}`);
  let j = null;
  try {
    j = JSON.parse(jsonOut);
  } catch (e) {
    fail(`--json output is not one JSON document: ${e.message}`);
  }
  if (j) {
    check(j.unique === 9 && j.peak?.count === 7 && j.peak?.at === "02:08:00", "json: unique 9, peak 7 at 02:08:00");
    check(Array.isArray(j.players) && j.players.length === 9 && typeof j.players[0].minutes === "number", "json: players list with numeric minutes");
    check(typeof j.log === "string" && j.log.endsWith("console.log"), "json: log path");
  }
  check(missingCode === 1, `missing log exit ${missingCode}, want 1`);
} finally {
  console.log = realLog;
}

console.log(failed ? `SUMMARY host-attendance-selftest fail ${failed}` : "SUMMARY host-attendance-selftest ok");
process.exit(failed ? 1 : 0);
