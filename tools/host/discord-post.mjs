// Helpers for `tools\cli host post` (#1414): read the announcements webhook URL from the host's
// secrets.json, check it is a Discord webhook, and POST one message with a bounded fetch.
//
// The URL is a secret (whoever has it can post to the channel). It lives only in
// <host dir>/secrets.json as `announceWebhookUrl`, never in the repo, and is never printed: error text
// from a failed send has it scrubbed. The staff webhook (`staffWebhookUrl`) is a different channel and
// is never used here.
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";

export const SECRET_KEY = "announceWebhookUrl";
export const MAX_CONTENT = 2000; // Discord's limit for one message
export const TIMEOUT_MS = 10000;

// { url } or { error } (the error never contains the URL).
export function readWebhookUrl(hostDir) {
  const file = join(hostDir, "secrets.json");
  if (!existsSync(file)) return { error: `no secrets.json in ${hostDir}; add "${SECRET_KEY}": "<announcements webhook URL>" to it` };
  let secrets;
  try {
    secrets = JSON.parse(readFileSync(file, "utf8"));
  } catch {
    return { error: `secrets.json in ${hostDir} is not valid JSON` };
  }
  const url = secrets && typeof secrets === "object" ? secrets[SECRET_KEY] : undefined;
  if (typeof url !== "string" || !url.trim()) return { error: `secrets.json has no "${SECRET_KEY}" (the announcements webhook URL)` };
  return { url: url.trim() };
}

// True only for https://discord.com|discordapp.com/api/webhooks/... so a typo or a pasted page cannot leak a post elsewhere.
export function isDiscordWebhook(url) {
  let u;
  try {
    u = new URL(url);
  } catch {
    return false;
  }
  return u.protocol === "https:" && /^(discord|discordapp)\.com$/.test(u.hostname) && u.pathname.startsWith("/api/webhooks/");
}

// POSTs one message. Returns { ok, status, error? }; never throws, and `error` never holds the URL.
// No mentions are parsed, so a post cannot ping @everyone.
export async function postToWebhook(url, content, { timeoutMs = TIMEOUT_MS } = {}) {
  try {
    const res = await fetch(url, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ content, allowed_mentions: { parse: [] } }),
      signal: AbortSignal.timeout(timeoutMs),
    });
    if (!res.ok) return { ok: false, status: res.status, error: `HTTP ${res.status}` };
    return { ok: true, status: res.status };
  } catch (e) {
    const timedOut = e && (e.name === "TimeoutError" || e.name === "AbortError");
    return { ok: false, status: 0, error: timedOut ? `no answer within ${timeoutMs} ms` : scrub(String(e && e.message ? e.message : e), url) };
  }
}

// Removes the URL (and its token) from text that is about to be printed.
export function scrub(text, url) {
  let out = String(text);
  if (url) out = out.split(url).join("<webhook>");
  return out.replace(/https?:\/\/\S*\/api\/webhooks\/\S*/g, "<webhook>");
}
