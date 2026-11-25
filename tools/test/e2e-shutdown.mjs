// The shutdown-save proof of `tools\cli wt e2e` (#830).
//
// Pass 1 boots the suite's mission header (EL_DebugTest.conf) and ends the way
// `tools\cli host stop` ends a playtest server: the stop file, LF_HostStop, then
// GetGame().RequestClose(). The engine writes its SHUTDOWN save on that close.
// A bare world (`-server DebugWorld.ent`, no mission header) enters the SHUTDOWN
// state but writes no SHUTDOWN save, which is why e2e used to reload the manual
// save. Pass 2 loads the latest save, which is then the shutdown save.
//
// checkShutdownSave reads pass 1's console.log. It fails unless the stop file
// closed the server, the SHUTDOWN save completed, and the feature statics were
// reset only after it (LF_StaticReset.ResetAll in the game mode destructor; a
// reset before the save writes empty accounts, jail, ledger, storage).

import { existsSync, readdirSync } from "node:fs";
import { join } from "node:path";

// The suite's core mission header on DebugWorld (an LF_MissionHeader that watches the stop file).
export const E2E_MISSION = "{5A1E5A1E5A1E0002}Missions/EL_DebugTest.conf";
// Pass 1 defines this: the test runner leaves the close to the stop file.
export const HOST_STOP_DEFINE = "LF_E2E_HOST_STOP";
// LF_Test_E2E_ShutdownSave prints this once the manual save is done and the marker is deposited.
// The harness writes the stop file only after it, so the marker is in the shutdown save alone.
export const MARKER_READY = "e2e shutdown marker deposited after the manual save";
// LF_Test_E2E_ShutdownSave prints this in pass 2 when the marker came back.
export const MARKER_LOADED = "e2e shutdown save loaded";

const STOP = /\[LFDebug:Host\] stop requested/;
const SAVED = /\[LFDebug:Persistence\] save completed \(SHUTDOWN\)/;
const MISSING = /session ended without a shutdown save/;
const RESET = /\[LFDebug:StaticReset\] feature statics reset/;
const LOADED = /session continued from a save point|e2e shutdown save loaded/;

function firstIndex(lines, re) {
  return lines.findIndex((l) => re.test(l));
}

// Pass 1's console.log text -> { ok, lines: [evidence], reason }.
export function checkShutdownSave(text) {
  const lines = String(text ?? "").split(/\r?\n/);
  const at = { stop: firstIndex(lines, STOP), saved: firstIndex(lines, SAVED), missing: firstIndex(lines, MISSING), reset: firstIndex(lines, RESET) };
  const evidence = ["stop", "saved", "missing", "reset"].filter((k) => at[k] >= 0).sort((a, b) => at[a] - at[b]).map((k) => lines[at[k]].trim());
  const fail = (reason) => ({ ok: false, lines: evidence, reason });
  if (at.missing >= 0) return fail("the server logged `session ended without a shutdown save`");
  if (at.stop < 0) return fail("the host stop file never closed the server (no `[LFDebug:Host] stop requested`)");
  if (at.saved < 0) return fail("no `save completed (SHUTDOWN)` line: the engine wrote no shutdown save");
  if (at.saved < at.stop) return fail("the SHUTDOWN save completed before the stop request");
  if (at.reset < 0) return fail("no `feature statics reset` line: the game mode was never torn down cleanly");
  if (at.reset < at.saved) return fail("the feature statics were reset BEFORE the shutdown save, so it holds empty state");
  return { ok: true, lines: evidence, reason: null };
}

// Pass 2's console.log text -> the lines that show the shutdown save loaded.
export function loadLines(text) {
  return String(text ?? "").split(/\r?\n/).filter((l) => LOADED.test(l)).map((l) => l.trim());
}

// The log folders a worktree's test profile holds now (a Set of names). Taken before pass 2 boots, it names
// the boots that came before: pass 2 may only trust a log in a folder that is not in it (#3569).
export function testLogDirs(root) {
  const logs = join(root, "server", "profile", "test", "logs");
  if (!existsSync(logs)) return new Set();
  return new Set(readdirSync(logs).filter((d) => d.startsWith("logs_")));
}

// The newest console.log of a worktree's test profile, or null. `exclude`: folder names (testLogDirs) to skip,
// so a reload boot that never created its own log yields null instead of the previous boot's.
export function newestTestConsole(root, exclude = null) {
  const logs = join(root, "server", "profile", "test", "logs");
  if (!existsSync(logs)) return null;
  const dirs = readdirSync(logs).filter((d) => d.startsWith("logs_") && !(exclude && exclude.has(d))).sort();
  for (let i = dirs.length - 1; i >= 0; i--) {
    const f = join(logs, dirs[i], "console.log");
    if (existsSync(f)) return f;
  }
  return null;
}

// Pass 2's verdict on its own boot (#3569). `text` is the console.log of a folder that did not exist before the
// boot (null when there was none: the harness read the previous boot's SUMMARY, or the server never started).
// A real reload boot prints "session continued from a save point" (about 10 s); a stale green prints nothing.
export function checkReloadBoot(text) {
  if (text === null || text === undefined) {
    return { ok: false, reason: "pass 2 wrote no log folder of its own: it never booted, so any green it reported came from an earlier boot" };
  }
  if (!loadLines(text).length) {
    return { ok: false, reason: "pass 2 booted but its log has no `session continued from a save point` line: it did not load the shutdown save" };
  }
  return { ok: true, reason: null };
}

// Tests whose restart proof is a seed (pass 1) plus a verify (pass 2) behind a silent skip (`LF_E2E.RequiresSeed`,
// `RequiresSection`): a pass 2 that skipped is green. Each prints one line only when its restart check ran to the end, so
// the harness fails a leg that holds the test but whose pass 2 log lacks the line (#3630: the cargo truck seed was
// gated to the save boot and nothing noticed when the reload boot stopped finding it).
export const RESTART_PROOFS = [
  { cls: "LF_Test_CargoBedPersist", what: "cargo bed truck keeps its logs (#3266)", text: "[LFDebug:Cargo] e2e restart: e2e-cargo-bed-truck collected with N unit(s) on its bed", line: /\[LFDebug:Cargo\] e2e restart: e2e-cargo-bed-truck collected with [1-9]\d* unit\(s\) on its bed/ },
  { cls: "LF_Test_WorkplaceJobRestart", what: "a desk job taken at a workplace survives a restart (#4959)", text: "[LFDebug:Jobs] e2e restart: test-workplacejob-e2e is still a MINER", line: /\[LFDebug:Jobs\] e2e restart: test-workplacejob-e2e is still a MINER/ },
  { cls: "LF_Test_ParkedCarRestart", what: "one locked parked car per marker after a restart (#4122)", text: "[LFDebug:ParkedCar] e2e restart: 1 car(s) at spot A, 1 at spot B", line: /\[LFDebug:ParkedCar\] e2e restart: 1 car\(s\) at spot A, 1 at spot B/ },
];

// Pass 2's console.log text and the leg's class list (null: the whole tier) -> the proofs that never printed.
// A pass 2 on an older save (--legacy-save, --pre-move) holds no such data: the caller does not ask.
export function missingRestartProofs(text, classes) {
  const t = String(text ?? "");
  return RESTART_PROOFS.filter((p) => (!classes || classes.includes(p.cls)) && !p.line.test(t));
}

// Fixtures cut from real pass 1 logs (2026-11-12); returns failure strings.
export function selftestE2eShutdown() {
  const failures = [];
  const before = [
    "00:18:33.111 SCRIPT       : [LFDebug:Persistence] save completed (MANUAL)",
    "00:18:37.645 SCRIPT       : [LF_Tests] closing game",
    "00:18:37.680 SCRIPT       : [LFDebug:Persistence] session ending, engine shutdown save expected",
    "00:18:37.711 SCRIPT       : [LFDebug:Persistence] system state ACTIVE -> SHUTDOWN",
    "00:18:38.250 SCRIPT    (E): [LFDebug:Persistence] session ended without a shutdown save - the engine wrote none on this exit path",
    "00:18:38.251 SCRIPT       : [LFDebug:StaticReset] feature statics reset (run 1)",
  ].join("\n");
  const good = [
    "SCRIPT       : [LFDebug:Persistence] save completed (MANUAL)",
    "SCRIPT       : [LF_Tests] e2e shutdown marker deposited after the manual save",
    "SCRIPT       : [LFDebug:Host] stop requested, closing the game (engine shutdown save follows)",
    "SCRIPT       : [LFDebug:Persistence] session ending, engine shutdown save expected",
    "SCRIPT       : [LFDebug:Persistence] system state ACTIVE -> SHUTDOWN",
    "SCRIPT       : [LFDebug:Persistence] save completed (SHUTDOWN)",
    "SCRIPT       : [LFDebug:StaticReset] feature statics reset (run 1)",
  ];
  const resetFirst = [good[0], good[1], good[2], good[3], good[6], good[4], good[5]].join("\n");
  const r1 = checkShutdownSave(before);
  if (r1.ok || !/without a shutdown save/.test(r1.reason)) failures.push("the bare-world log (no shutdown save) passes");
  const r2 = checkShutdownSave(good.join("\n"));
  if (!r2.ok) failures.push(`a host-stop shutdown save fails: ${r2.reason}`);
  if (r2.lines.length !== 3) failures.push(`expected 3 evidence lines, got ${r2.lines.length}`);
  const r3 = checkShutdownSave(resetFirst);
  if (r3.ok || !/BEFORE the shutdown save/.test(r3.reason)) failures.push("statics reset before the shutdown save passes");
  const r4 = checkShutdownSave(good.filter((l) => !/Host\]/.test(l)).join("\n"));
  if (r4.ok) failures.push("a shutdown save without the host stop request passes");
  const r5 = checkShutdownSave(good.filter((l) => !/StaticReset/.test(l)).join("\n"));
  if (r5.ok) failures.push("a log without the statics reset passes");
  if (loadLines("x [LFDebug:Persistence] session continued from a save point (-loadSessionSave)\ny\n[LF_Tests] e2e shutdown save loaded: x").length !== 2) failures.push("loadLines misses a load line");
  // #3569: a stale pass 2 (no log folder of its own, or a log with no load line) fails; a real reload passes.
  if (checkReloadBoot(null).ok) failures.push("a pass 2 with no log folder of its own passes");
  if (checkReloadBoot(good.join("\n")).ok) failures.push("a pass 2 log that never loaded a save passes (it is pass 1's log)");
  if (!checkReloadBoot("x [LFDebug:Persistence] session continued from a save point (-loadSessionSave)\n[LFTEST] SUMMARY tier=persistence passed=1 failed=0 total=1").ok) failures.push("a real reload boot fails");
  // #3630: a leg that holds the cargo test fails when its pass 2 never printed the restart line; a leg without it does not.
  const cargoLine = "SCRIPT       : [LFDebug:Cargo] e2e restart: e2e-cargo-bed-truck collected with 4 unit(s) on its bed";
  if (missingRestartProofs(`${cargoLine}\n`, ["LF_Test_CargoBedPersist"]).length) failures.push("a pass 2 that ran the cargo restart check is reported missing");
  if (missingRestartProofs("[LFTEST] SUMMARY tier=only passed=1 failed=0 total=1\n", ["LF_Test_CargoBedPersist", "LF_Test_X"]).length !== 1) failures.push("a pass 2 that skipped the cargo restart check passes");
  const cargoMissing = (text) => missingRestartProofs(text, null).filter((p) => p.cls === "LF_Test_CargoBedPersist").length;
  if (cargoMissing("x\n") !== 1) failures.push("a whole-tier pass 2 without the cargo restart line passes");
  if (missingRestartProofs("x\n", ["LF_Test_X"]).length) failures.push("a leg without the cargo test is asked for its line");
  if (cargoMissing("[LFDebug:Cargo] e2e restart: e2e-cargo-bed-truck collected with 0 unit(s) on its bed\n") !== 1) failures.push("an empty bed counts as a restored load");
  // #4122: one locked car per marker after the restart; two cars at a spot is the bug, not a proof.
  const parked = ["LF_Test_ParkedCarRestart"];
  if (missingRestartProofs("SCRIPT       : [LFDebug:ParkedCar] e2e restart: 1 car(s) at spot A, 1 at spot B\n", parked).length) failures.push("a pass 2 that ran the parked-car restart check is reported missing");
  if (missingRestartProofs("[LFDebug:ParkedCar] e2e restart: 2 car(s) at spot A, 1 at spot B\n", parked).length !== 1) failures.push("two parked cars on one marker count as a restart proof");
  return failures;
}
