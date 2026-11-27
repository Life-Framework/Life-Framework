// Proves the main guard's stall watchdog (tools/lib/main-guard.mjs stallCheck, noteHead, stallAction, stallRestart) on
// a replay of the 2026-11-26 stall, and that stage 0 no longer holds the tick loop (tools/main-guard.mjs).
// The incident: last green 8e0d513d at 00:58Z, nothing merged until f5dde772 at 20:04Z (an idle guard, not a stall),
// then a stage-0 `validate --all` started at 20:07Z wrote nothing for 37 min while it was awaited inside the tick, so
// 06aaeb40 and 527184fd queued behind it unchecked; `main-guard stop`, `start` at 20:44Z moved it at once.
// inputs: tools/lib/main-guard.mjs, tools/main-guard.mjs
// red-proof: with the stall-window test in stallCheck inverted this printed "FAIL incident at 20:43Z: stalled"; with
//   `stage0 = v0.finally(...)` replaced by `await v0` (the old inline stage 0) it printed "FAIL stage 0 runs in the background".
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import * as g from "../lib/main-guard.mjs";

const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };
const at = (iso) => Date.parse(iso);
const W = 30 * 60 * 1000;

// The idle day: main's head is the last compiled one. Twenty hours of nothing to check is not a stall.
const idle = { lastCompiled: "8e0d513d", lastCompileAt: "2026-11-26T00:47:46Z", tickAt: "2026-11-26T20:00:00Z" };
check(!g.stallCheck(idle, "8e0d513d", { now: at("2026-11-26T20:00:00Z"), stallMs: W }).stalled, "an idle guard on a checked head is not stalled");
check(g.noteHead(idle, "8e0d513d", at("2026-11-26T20:00:00Z")) === null, "noteHead: a checked head changes nothing");

// f5dde772 lands; the worker notes it and then sits in a silent validate (the tick never reaches the compile).
let st = { ...idle };
st = { ...st, ...g.noteHead(st, "f5dde772", at("2026-11-26T20:07:00Z")) };
check(st.uncheckedSince === "2026-11-26T20:07:00.000Z" && st.uncheckedHead === "f5dde772", "noteHead records the first unchecked head");
st = { ...st, ...(g.noteHead(st, "527184fd", at("2026-11-26T20:34:00Z")) ?? {}) };
check(st.uncheckedSince === "2026-11-26T20:07:00.000Z" && st.uncheckedHead === "527184fd", "a burst keeps the first unchecked time");
st.validating = { head: "f5dde772", at: "2026-11-26T20:07:28Z" };
check(!g.stallCheck(st, "527184fd", { now: at("2026-11-26T20:30:00Z"), stallMs: W }).stalled, "23 min unchecked is inside the window");
const s43 = g.stallCheck(st, "527184fd", { now: at("2026-11-26T20:43:00Z"), stallMs: W });
check(s43.stalled, "FAIL incident at 20:43Z: stalled");
check(/527184fd/.test(s43.why) && /validating f5dde772/.test(s43.why), `the stall line names the head and the running validate: ${s43.why}`);
check(g.stallAction(s43, { now: at("2026-11-26T20:43:00Z"), stallMs: W }) === "kick", "36 min: kick (re-fetch, start stage 0 and 1)");
const s80 = g.stallCheck(st, "527184fd", { now: at("2026-11-26T21:20:00Z"), stallMs: W });
check(g.stallAction(s80, { now: at("2026-11-26T21:20:00Z"), stallMs: W }) === "restart", "73 min: the loop restarts");
check(g.stallAction(s80, { now: at("2026-11-26T21:20:00Z"), stallMs: W, stage2: true }) === "kick", "never restarts under a running stage 2");
check(g.stallAction(s80, { now: at("2026-11-26T21:20:00Z"), stallMs: W, lastRestartAt: "2026-11-26T20:30:00Z" }) === "kick", "at most one loop restart per gap");

// A compile that started recently is work in progress, not a stall; a checked head clears the clock.
check(!g.stallCheck({ ...st, lastCompileAt: "2026-11-26T21:15:00Z" }, "527184fd", { now: at("2026-11-26T21:20:00Z"), stallMs: W }).stalled, "a compile started 5 min ago is not a stall");
const done = { ...st, lastCompiled: "527184fd" };
check(!g.stallCheck(done, "527184fd", { now: at("2026-11-26T21:20:00Z"), stallMs: W }).stalled, "a compiled head is not stalled");
check(g.noteHead(done, "527184fd", at("2026-11-26T21:20:00Z"))?.uncheckedSince === null, "noteHead clears the clock once the head is checked");

// A reader that never saw the head (status, the watchdog on a fresh state) falls back to the head's commit time.
check(g.stallCheck({ lastCompiled: "a" }, "b", { now: at("2026-11-26T21:00:00Z"), headAt: "2026-11-26T20:04:49Z", stallMs: W }).stalled, "headAt fallback: 55 min since the merge");

// A fetch that keeps failing leaves origin/main stale: every head looks checked, so it is flagged on its own.
const ff = g.stallCheck({ ...done, fetchErr: "Could not resolve host", fetchOkAt: "2026-11-26T20:00:00Z" }, "527184fd", { now: at("2026-11-26T21:00:00Z"), stallMs: W });
check(ff.stalled && /fetch/.test(ff.why), "a fetch failing for an hour is a stall");

// The watchdog (another process) restarts only a loop that stopped ticking, never under a stage-2 run.
const sr = (state) => g.stallRestart({ alive: 11516, state, stall: s80, now: at("2026-11-26T21:20:00Z") });
check(sr({ ...st, tickAt: "2026-11-26T21:19:00Z" }) === null, "a ticking loop kicks itself; the watchdog leaves it");
check(sr({ ...st, tickAt: "2026-11-26T20:07:00Z" })?.action === "restart-stalled", "a loop silent 73 min with main stalled is restarted");
check(sr({ ...st, tickAt: "2026-11-26T20:07:00Z", running: { step: "suite" } }) === null, "never under a running stage 2");
check(g.stallRestart({ alive: 11516, state: st, stall: { stalled: false } }) === null, "no stall, no restart");
check(g.stallMsFrom({ LF_MAIN_GUARD_STALL_MIN: "10" }) === 600000 && g.stallMsFrom({}) === g.STALL_MS, "LF_MAIN_GUARD_STALL_MIN overrides the window");

// The worker: stage 0 is a background promise in its own worktree with a budget, never awaited in front of the compile.
const src = readFileSync(join(dirname(fileURLToPath(import.meta.url)), "..", "main-guard.mjs"), "utf8");
check(/stage0 = v0\.finally/.test(src) && !/try \{ await validateHead\(root, cdir, head\)/.test(src), "FAIL stage 0 runs in the background");
check(/VALIDATE_SLUG = "main-guard-validate"/.test(src), "stage 0 has its own worktree");
check(/mg\.VALIDATE_BUDGET_MS\)/.test(src), "the stage-0 validate runs under VALIDATE_BUDGET_MS");
check(/\["fetch", "origin", "--prune"\], \{ timeout: FETCH_TIMEOUT_MS \}/.test(src), "the tick's fetch has a timeout");
check(/mg\.stallCheck\(/.test(src) && /mg\.stallRestart\(/.test(src) && /STALLED:/.test(src), "worker, watchdog and status all read the stall rules");

if (fails.length) {
  for (const f of fails) console.log(f.startsWith("FAIL") ? f : `FAIL ${f}`);
  process.exit(1);
}
console.log("main-guard-stall-selftest: ok");
