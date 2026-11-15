// tools/validation/host-crashloop-selftest.mjs
// inputs: tools/host/**
//
// Proves the crash-loop guard (#1263, Ops O4) of `tools\cli host watch`: a crash history of 3 in
// 10 minutes is "hold" and 2 is "restart"; the window slides (old crashes stop counting); a limit
// of 0 turns the guard off; the watch loop, driven with fake seams, restarts below the limit and,
// at it, stops restarting, leaves the host down and hands back one alert naming the crash logs;
// the alert reaches the staff webhook when the host has one (config.json `staff.webhookUrl`, or
// secrets.json `staffWebhookUrl`) and is print-only otherwise; and the URL is never in any output.
// red-proof: before tools/host/crashloop.mjs existed this file died on the import:
// "Cannot find module '.../tools/host/crashloop.mjs'" (2026-11-15).

import { mkdirSync, mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve, dirname } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const cl = await import(pathToFileURL(join(ROOT, "tools", "host", "crashloop.mjs")).href);
const wd = await import(pathToFileURL(join(ROOT, "tools", "host", "watchdog.mjs")).href);
const { hostPaths } = await import(pathToFileURL(join(ROOT, "tools", "host", "host.mjs")).href);

const failures = [];
const fail = (m) => failures.push(m);

const MIN = 60 * 1000;
const T = Date.UTC(2026, 10, 15, 12, 0, 0);
const at = (...minutes) => minutes.map((m) => T + m * MIN);

// 1. The decision. Defaults are 3 crashes in 10 minutes.
if (cl.DEFAULT_LIMIT !== 3 || cl.DEFAULT_WINDOW_MS !== 10 * MIN) fail(`defaults are ${cl.DEFAULT_LIMIT} in ${cl.DEFAULT_WINDOW_MS} ms, expected 3 in 10 minutes`);
const decide = (times, now, opts) => cl.decideCrashLoop(times, now, opts);
if (decide(at(0, 4, 8), T + 8 * MIN) !== "hold") fail("3 crashes in 10 minutes is not hold");
if (decide(at(0, 4), T + 4 * MIN) !== "restart") fail("2 crashes in 10 minutes is not restart");
if (decide([], T) !== "restart") fail("no crashes is not restart");
if (decide(at(0, 5, 11), T + 11 * MIN) !== "restart") fail("a crash 11 minutes old still counted (the window slides)");
if (decide(at(0, 4, 8), T + 8 * MIN, { limit: 4 }) !== "restart") fail("3 crashes held with --crash-limit 4");
if (decide(at(0, 4), T + 4 * MIN, { limit: 2 }) !== "hold") fail("2 crashes did not hold with --crash-limit 2");
if (decide(at(0, 1, 2), T + 2 * MIN, { windowMs: 90 * 1000 }) !== "restart") fail("a 90 s window counted crashes a minute apart");
if (decide(at(0, 0, 0, 0), T, { limit: 0 }) !== "restart") fail("limit 0 did not turn the guard off");
if (cl.crashesInWindow(at(0, 4, 8, 20), T + 20 * MIN, 10 * MIN) !== 1) fail("crashesInWindow counted old crashes");

// The settings come from flags and env, and a bad one is refused rather than guessed.
const ok = cl.crashLoopSettings({ limit: "5", windowMin: "30" });
if (ok.error || ok.limit !== 5 || ok.windowMs !== 30 * MIN) fail(`settings 5/30 gave ${JSON.stringify(ok)}`);
const envd = cl.crashLoopSettings({}, { LF_CRASHLOOP_LIMIT: "2", LF_CRASHLOOP_WINDOW_MIN: "1" });
if (envd.error || envd.limit !== 2 || envd.windowMs !== MIN) fail(`env settings gave ${JSON.stringify(envd)}`);
const dflt = cl.crashLoopSettings({}, {});
if (dflt.error || dflt.limit !== 3 || dflt.windowMs !== 10 * MIN) fail(`default settings gave ${JSON.stringify(dflt)}`);
for (const bad of [{ limit: "-1" }, { limit: "x" }, { windowMin: "0" }, { windowMin: "abc" }]) {
  if (!cl.crashLoopSettings(bad, {}).error) fail(`bad settings ${JSON.stringify(bad)} were accepted`);
}

// 2. The loop. Each run exits at once with no stop or restart signal (a crash); `clock` advances a
//    minute per crash so three land inside ten minutes.
const NONE = { stopFilePresent: false, stopSeen: false, logStop: false, restartMarker: false, logRestart: false };
function drive({ runs, guard, startMinute = 0, stepMinutes = 1, maxRestarts = 4 }) {
  let run = -1;
  let alive = false;
  let now = T + startMinute * MIN;
  const events = [];
  const r = wd.watchLoop({
    launch() { run++; alive = true; events.push(`launch ${run}`); return { ok: true, pid: 100 + run }; },
    isAlive: () => alive,
    signals() { alive = false; return { ...NONE, ...(runs[Math.min(run, runs.length - 1)] ?? {}) }; },
    onCrash(n) { events.push(`crash ${n}`); now += stepMinutes * MIN; return { logDir: `crash-${n}` }; },
    afterExit() {},
    sleep() {},
    log() {},
    now: () => now,
    crashLoop: guard,
    maxRestarts,
  });
  return { r, events, launches: run + 1 };
}
const guard = { limit: 3, windowMs: 10 * MIN };

const loop = drive({ runs: [{}], guard });
if (loop.r.reason !== "crash-loop") fail(`3 crashes in 3 minutes ended with ${JSON.stringify(loop.r)}`);
if (loop.launches !== 3) fail(`the host was launched ${loop.launches} times, expected 3 (then held down)`);
if (loop.r.crashes !== 3) fail(`crashes counted ${loop.r.crashes}`);
const alertText = loop.r.alert ?? "";
for (const n of [1, 2, 3]) if (!alertText.includes(`crash-${n}`)) fail(`the alert does not name crash log crash-${n}: ${alertText}`);
if (!/3 crashes/.test(alertText) || !/10 minutes/.test(alertText)) fail(`the alert does not say how many crashes in how long: ${alertText}`);
if (!/down/i.test(alertText)) fail(`the alert does not say the host stays down: ${alertText}`);
if ((alertText.match(/\n/g) ?? []).length > 12) fail("the alert is not one short message");

// Crashes 6 minutes apart never put 3 inside 10 minutes: the guard stays quiet and --max-restarts (4 here) still ends it.
const slow = drive({ runs: [{}], guard, stepMinutes: 6 });
if (slow.r.reason !== "max-crashes" || slow.launches !== 5) fail(`6-minute crashes were held or not capped: ${JSON.stringify(slow.r)} launches ${slow.launches}`);
// A stop still ends the watch with no alert; a scheduled close is no crash.
const stop = drive({ runs: [{ stopFilePresent: true }], guard });
if (stop.r.reason !== "stop" || stop.r.alert) fail(`a clean stop gave ${JSON.stringify(stop.r)}`);
const sched = drive({ runs: [{ restartMarker: true }, { restartMarker: true }, { restartMarker: true }, { stopFilePresent: true }], guard });
if (sched.r.reason !== "stop" || sched.launches !== 4) fail(`scheduled closes counted toward the guard: ${JSON.stringify(sched.r)} launches ${sched.launches}`);
// No guard given: the loop behaves as #1262 shipped it.
const off = drive({ runs: [{}], guard: null });
if (off.r.reason !== "max-crashes") fail(`with no guard the loop ended ${JSON.stringify(off.r)}`);

// 3. The alert reaches the webhook when one is set, and never prints it.
const base = mkdtempSync(join(tmpdir(), "lf-host-crashloop-"));
try {
  const SECRET = "https://example.invalid/api/webhooks/123/SECRET-TOKEN-xyz";
  const bare = join(base, "bare");
  const viaConfig = join(base, "config");
  const viaSecrets = join(base, "secrets");
  for (const d of [bare, viaConfig, viaSecrets]) mkdirSync(d, { recursive: true });
  const cfgDir = join(hostPaths(viaConfig).profile, "profile", "LifeFramework");
  mkdirSync(cfgDir, { recursive: true });
  writeFileSync(join(cfgDir, "config.json"), JSON.stringify({ staff: { webhookUrl: SECRET } }));
  writeFileSync(join(viaSecrets, "secrets.json"), JSON.stringify({ staffWebhookUrl: SECRET }));

  if (cl.findWebhookUrl(bare)) fail("a host with no config found a webhook URL");
  if (cl.findWebhookUrl(viaConfig) !== SECRET) fail("the webhook URL in the profile config.json was not found");
  if (cl.findWebhookUrl(viaSecrets) !== SECRET) fail("the webhook URL in secrets.json was not found");
  writeFileSync(join(viaSecrets, "secrets.json"), "{ not json");
  if (cl.findWebhookUrl(viaSecrets)) fail("a broken secrets.json produced a URL");

  const sent = [];
  const fakeFetch = async (url, init) => { sent.push({ url, init }); return { ok: true, status: 204 }; };
  const lines = [];
  const logIt = (m) => lines.push(m);

  const posted = await cl.sendAlert("host is down: 3 crashes\ncrash-1 \"quoted\"", { dir: viaConfig, env: {}, fetchFn: fakeFetch, log: logIt });
  if (!posted.posted || sent.length !== 1) fail(`the alert was not posted to the webhook: ${JSON.stringify(posted)}`);
  else {
    if (sent[0].url !== SECRET) fail("the alert went to the wrong URL");
    let body;
    try { body = JSON.parse(sent[0].init.body); } catch { fail("the webhook body is not valid JSON"); }
    if (body && !(body.content ?? "").includes("\"quoted\"")) fail("the webhook body lost the alert text");
  }
  if (lines.join("\n").includes("SECRET-TOKEN")) fail("the webhook URL appeared in a log line");

  sent.length = 0;
  const none = await cl.sendAlert("host is down", { dir: bare, env: {}, fetchFn: fakeFetch, log: logIt });
  if (none.posted || sent.length) fail("an alert was posted with no channel configured");
  if (!/no staff channel/i.test(lines.join("\n"))) fail("with no channel the alert did not say it was print-only");

  const failing = await cl.sendAlert("host is down", { dir: viaConfig, env: {}, fetchFn: async () => { throw new Error("boom"); }, log: logIt });
  if (failing.posted) fail("a failing post reported success");
  if (lines.join("\n").includes("SECRET-TOKEN")) fail("a failing post printed the webhook URL");
} finally {
  safeRemoveTree(base);
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log("host-crashloop-selftest ok");
