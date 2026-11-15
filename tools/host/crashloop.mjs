// Crash-loop guard (#1263, Ops O4) for `tools\cli host watch` (tools/host/watchdog.mjs).
//
// A host that crashes again and again would be restarted forever, hiding the fault and churning the
// saves. The guard counts crashes in a sliding window; at the limit (default 3 in 10 minutes) the
// watch stops restarting, the host stays down, and one staff alert names the crash logs.
//
// The alert goes, in order, to:
//   1. the staff webhook, the one the mod's `staff` config section names (LF_StaffWebhookConfig, #1343):
//      `staff.webhookUrl` in the host's config.json (<host>/profile/profile/LifeFramework/config.json),
//      else `staffWebhookUrl` in the host's secrets.json (<host>/secrets.json). The mod cannot send
//      this one itself: the game server is the thing that is down.
//   2. a Discord channel when LF_STAFF_CHANNEL is set, through tools/discord-mcp/say.mjs (the bot
//      token stays in ~/.life-framework, never in the repo).
//   3. nothing: the alert is only printed.
// The webhook URL is a secret: it is never printed or logged, not even when the post fails.
import { spawnSync } from "node:child_process";
import { mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { hostPaths } from "./host.mjs";
import { safeRemoveTree } from "../link-safety.mjs";

const MIN_MS = 60 * 1000;
export const DEFAULT_LIMIT = 3;
export const DEFAULT_WINDOW_MS = 10 * MIN_MS;
const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");

// How many of `times` (epoch ms) fall inside the window that ends at `now`. Pure.
export function crashesInWindow(times, now, windowMs) {
  return times.filter((t) => now - t < windowMs && t <= now).length;
}

// "hold" (stop restarting, keep the host down) or "restart". A limit of 0 turns the guard off. Pure.
export function decideCrashLoop(times, now, { limit = DEFAULT_LIMIT, windowMs = DEFAULT_WINDOW_MS } = {}) {
  if (!limit || limit < 1) return "restart";
  return crashesInWindow(times, now, windowMs) >= limit ? "hold" : "restart";
}

// The guard settings from the flags (--crash-limit N, --crash-window MIN) over the environment
// (LF_CRASHLOOP_LIMIT, LF_CRASHLOOP_WINDOW_MIN) over the defaults. A bad value is an error, not a guess.
// Returns { limit, windowMs } or { error }.
export function crashLoopSettings({ limit, windowMin } = {}, env = process.env) {
  const rawLimit = limit ?? env.LF_CRASHLOOP_LIMIT;
  const rawWindow = windowMin ?? env.LF_CRASHLOOP_WINDOW_MIN;
  let l = DEFAULT_LIMIT;
  let w = DEFAULT_WINDOW_MS;
  if (rawLimit !== undefined && rawLimit !== "") {
    l = Number(rawLimit);
    if (!Number.isInteger(l) || l < 0) return { error: `--crash-limit must be a whole number >= 0, 0 turning the guard off (got ${rawLimit})` };
  }
  if (rawWindow !== undefined && rawWindow !== "") {
    const m = Number(rawWindow);
    if (!Number.isFinite(m) || m <= 0) return { error: `--crash-window must be a number of minutes > 0 (got ${rawWindow})` };
    w = m * MIN_MS;
  }
  return { limit: l, windowMs: w };
}

function minutesText(ms) {
  const m = ms / MIN_MS;
  return `${Number.isInteger(m) ? m : m.toFixed(1)} minute${m === 1 ? "" : "s"}`;
}

// The one alert. `logDirs` are the crash folders (backups/<time>-crash) of the crashes in the window.
export function crashAlertText({ crashes, windowMs, logDirs = [] }) {
  const lines = [
    `Life Framework host is DOWN: ${crashes} crashes in ${minutesText(windowMs)}. The watchdog stopped restarting it and the host stays down until staff look.`,
  ];
  if (logDirs.length) {
    lines.push("Crash logs (console log inside each, in crash-log/):");
    for (const d of logDirs) lines.push(`- ${d}`);
  } else {
    lines.push("No crash log was kept (no console log found).");
  }
  lines.push("Fix the cause, then start the host again: tools\\cli host watch.");
  return lines.join("\n");
}

function readJson(file) {
  try {
    return JSON.parse(readFileSync(file, "utf8"));
  } catch {
    return null;
  }
}

// The staff webhook URL for this host, or "". Never printed by anything here.
export function findWebhookUrl(dir) {
  const owner = readJson(join(hostPaths(dir).profile, "profile", "LifeFramework", "config.json"));
  const fromConfig = owner?.staff?.webhookUrl;
  if (typeof fromConfig === "string" && fromConfig.trim()) return fromConfig.trim();
  const secrets = readJson(join(dir, "secrets.json"));
  const fromSecrets = secrets?.staffWebhookUrl;
  if (typeof fromSecrets === "string" && fromSecrets.trim()) return fromSecrets.trim();
  return "";
}

// Posts through say.mjs; returns true on success. The text goes through a file, as say.mjs takes it.
function sayToChannel(channel, text) {
  const tmp = mkdtempSync(join(tmpdir(), "lf-crashloop-"));
  try {
    const file = join(tmp, "alert.txt");
    writeFileSync(file, text);
    const r = spawnSync(process.execPath, [join(ROOT, "tools", "discord-mcp", "say.mjs"), channel, "-", file], { encoding: "utf8", timeout: 30000, windowsHide: true });
    return r.status === 0;
  } finally {
    safeRemoveTree(tmp);
  }
}

// Sends the alert; returns { posted, via }. Prints one line saying where it went (or that it only
// printed), never the URL. A failing post is logged and does not throw: the host stays down either way.
export async function sendAlert(text, { dir, env = process.env, fetchFn = globalThis.fetch, log = console.log, sayFn = sayToChannel } = {}) {
  const url = dir ? findWebhookUrl(dir) : "";
  if (url) {
    try {
      const res = await fetchFn(url, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ content: text, text }),
      });
      if (res?.ok) {
        log("the staff alert was posted to the staff webhook");
        return { posted: true, via: "webhook" };
      }
      log(`the staff webhook refused the alert (HTTP ${res?.status ?? "?"}); the alert above is the only copy`);
    } catch (e) {
      log(`the staff webhook post failed (${e?.name ?? "error"}); the alert above is the only copy`);
    }
    return { posted: false, via: "webhook" };
  }
  const channel = env.LF_STAFF_CHANNEL;
  if (channel) {
    let ok = false;
    try {
      ok = sayFn(channel, text);
    } catch {
      ok = false;
    }
    log(ok ? `the staff alert was posted to Discord channel ${channel}` : `posting to Discord channel ${channel} failed; the alert above is the only copy`);
    return { posted: ok, via: "discord" };
  }
  log("no staff channel is configured (staff.webhookUrl in the host config.json or staffWebhookUrl in secrets.json); the alert above is only printed");
  return { posted: false, via: "none" };
}
