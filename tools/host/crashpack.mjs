// tools/host/crashpack.mjs - one zip that carries a server crash to a GitHub issue (#1457).
//
// `tools\cli host crashpack [--session <stamp>]` (tools/host/commands/crashpack.mjs) calls this. The
// watchdog (#1262) keeps a crash in <host>/backups/<stamp>-crash/ (the crashed run's log folder in
// crash-log/, next to the saves backup). The pack holds:
//   crash-dump/...    the dump files found in that folder, byte for byte (a dump is raw process
//                     memory: it cannot be redacted, the issue body says so)
//   console-tail.log  the last lines of the newest console.log, webhook URLs and known secrets redacted
//   build-stamp.txt   the game build (Steam manifest)
//   mods.txt          the mods the server loads (server.json game.mods)
//   server.json       the host's server.json, config.json the owner config: every password,
//                     secret, token and Discord webhook URL replaced by <redacted>, nothing else changed
//   issue-body.md     an issue draft: title, crash folder, build, mods, the last errors
// Nothing here files an issue or runs `gh`; the operator attaches the zip.
// red-proof: tools/validation/host-crashpack-selftest.mjs.

import { existsSync, readdirSync, readFileSync, statSync, openSync, readSync, closeSync } from "node:fs";
import { join, relative, sep } from "node:path";
import { hostPaths } from "./host.mjs";
import { makeZip } from "../release/pack-playtest.mjs";

export const REDACTED = "<redacted>";
export const DEFAULT_LINES = 500;
// A dump bigger than this is left out of the zip (GitHub attachments cap at 25 MB; the zip is deflated).
export const MAX_DUMP_BYTES = 48 * 1024 * 1024;
const TAIL_BYTES = 2 * 1024 * 1024;
const MAX_DEPTH = 8;

const WEBHOOK = /https?:\/\/(?:[a-z]+\.)?discord(?:app)?\.com\/api(?:\/v\d+)?\/webhooks\/[^\s"'<>\\]+/gi;
const SECRET_KEY = /password|passwd|secret|token|api[-_]?key|webhook/i;
const DUMP_FILE = /\.(?:dmp|mdmp|dump)$|^crash.*\.(?:bin|txt)$/i;
const ERROR_LINE = /\(E\)|Virtual Machine Exception|VME|CRASH|access violation|Unhandled exception|\[LFDebug:[A-Za-z0-9_]+\]\s*(?:ERROR|Error)/;

const isWebhookFree = (s) => !new RegExp(WEBHOOK.source, "i").test(s);

// Redacts one parsed JSON value in place of a copy. `secretKey` is true below a key that names a secret.
// Collects the redacted values into `secrets` so the log tail can be scrubbed of them too.
function redactNode(node, secretKey, secrets) {
  if (typeof node === "string") {
    if (secretKey && node !== "") {
      secrets.add(node);
      return REDACTED;
    }
    if (!isWebhookFree(node)) {
      for (const m of node.matchAll(WEBHOOK)) secrets.add(m[0]);
      return node.replace(WEBHOOK, REDACTED);
    }
    return node;
  }
  if (Array.isArray(node)) return node.map((v) => redactNode(v, secretKey, secrets));
  if (node && typeof node === "object") {
    const out = {};
    for (const [k, v] of Object.entries(node)) out[k] = redactNode(v, secretKey || SECRET_KEY.test(k), secrets);
    return out;
  }
  return node;
}

/** Scrubs text: webhook URLs, then every known secret value (4+ characters). */
export function redactText(text, secrets = []) {
  let out = String(text).replace(WEBHOOK, REDACTED);
  const known = [...new Set(secrets)].filter((s) => s.length >= 4).sort((a, b) => b.length - a.length);
  for (const s of known) out = out.split(s).join(REDACTED);
  return out;
}

/**
 * Redacts a JSON config. Valid JSON keeps its key order and is written with 2-space indentation; a
 * file that does not parse (comments, a stray comma) is redacted line by line and never copied raw.
 * Returns { text, secrets: [values redacted], parsed }.
 */
export function redactJsonText(text) {
  const secrets = new Set();
  try {
    const out = redactNode(JSON.parse(text), false, secrets);
    return { text: JSON.stringify(out, null, 2) + "\n", secrets: [...secrets], parsed: true };
  } catch {
    // Fallback: a "<key naming a secret>": "<value>" pair, then any webhook URL.
    const pair = /("[^"\n]*(?:password|passwd|secret|token|api[-_]?key|webhook)[^"\n]*"\s*:\s*)"((?:[^"\\\n]|\\.)*)"/gi;
    let out = String(text).replace(pair, (all, key, value) => {
      if (value === "") return all;
      secrets.add(value);
      return `${key}"${REDACTED}"`;
    });
    for (const m of out.matchAll(WEBHOOK)) secrets.add(m[0]);
    out = out.replace(WEBHOOK, REDACTED);
    return { text: out, secrets: [...secrets], parsed: false };
  }
}

/** The crash folder under <host>/backups: the named one (`<stamp>` or `<stamp>-crash`), else the newest `*-crash`. */
export function findCrash(dir, session) {
  const { backups } = hostPaths(dir);
  if (!existsSync(backups)) return null;
  const names = readdirSync(backups, { withFileTypes: true })
    .filter((e) => e.isDirectory() && e.name.endsWith("-crash"))
    .map((e) => e.name)
    .sort();
  if (session) {
    const want = String(session).replace(/-crash$/, "");
    return names.includes(`${want}-crash`) ? join(backups, `${want}-crash`) : null;
  }
  return names.length ? join(backups, names[names.length - 1]) : null;
}

function walk(dir, visit, depth = 0) {
  if (depth > MAX_DEPTH) return;
  let entries;
  try {
    entries = readdirSync(dir, { withFileTypes: true });
  } catch {
    return;
  }
  for (const e of entries.sort((a, b) => (a.name < b.name ? -1 : 1))) {
    if (e.isSymbolicLink()) continue;
    const p = join(dir, e.name);
    if (e.isDirectory()) {
      if (e.name === "game-addons") continue;
      walk(p, visit, depth + 1);
    } else if (e.isFile()) visit(p);
  }
}

function tailText(file, bytes = TAIL_BYTES) {
  const size = statSync(file).size;
  const fd = openSync(file, "r");
  try {
    const len = Math.min(size, bytes);
    const buf = Buffer.alloc(len);
    readSync(fd, buf, 0, len, size - len);
    return buf.toString("utf8");
  } finally {
    closeSync(fd);
  }
}

function lastLines(text, n) {
  const lines = text.split(/\r?\n/);
  while (lines.length && lines[lines.length - 1] === "") lines.pop();
  return lines.slice(-n);
}

function readJsonFile(file) {
  try {
    return JSON.parse(readFileSync(file, "utf8"));
  } catch {
    return null;
  }
}

// server.json: next to the host profile (where the operator keeps it), else inside it.
function findServerJson(dir) {
  const { profile } = hostPaths(dir);
  for (const f of [join(dir, "server.json"), join(profile, "server.json")]) if (existsSync(f)) return f;
  return null;
}

const ownerConfig = (dir) => join(hostPaths(dir).profile, "profile", "LifeFramework", "config.json");

function defaultBuildText() {
  try {
    // Loaded on demand: it needs the Reforger install, which a bare host folder (and CI) may lack.
    return import("../lib/install-api.mjs").then((m) => m.buildStamp().text).catch(() => "build unknown (no Steam manifest)");
  } catch {
    return Promise.resolve("build unknown (no Steam manifest)");
  }
}

function clip(s, n) {
  return s.length > n ? `${s.slice(0, n)}...` : s;
}

/**
 * Builds the pack. opts: { session, lines (tail length), buildText (skip the manifest lookup), mods
 * (a list of "name (id)" lines, null reads server.json) }.
 * Returns { ok:false, reason } or { ok:true, name, crashDir, title, body, zip: Buffer, files: [names], skipped: [notes] }.
 */
export function buildCrashpack(dir, opts = {}) {
  const lines = Number.isFinite(opts.lines) && opts.lines > 0 ? Math.floor(opts.lines) : DEFAULT_LINES;
  const crashDir = findCrash(dir, opts.session);
  if (!crashDir) {
    return { ok: false, reason: opts.session ? `no crash folder "${opts.session}" under ${hostPaths(dir).backups}` : `no crash folder (*-crash) under ${hostPaths(dir).backups}; the watchdog has not recorded a crash` };
  }
  const name = crashDir.slice(crashDir.lastIndexOf(sep) + 1);
  const entries = [];
  const skipped = [];
  const secrets = new Set();

  // The configs, redacted.
  const serverFile = findServerJson(dir);
  let serverJson = null;
  if (serverFile) {
    const r = redactJsonText(readFileSync(serverFile, "utf8"));
    r.secrets.forEach((s) => secrets.add(s));
    entries.push({ name: "server.json", data: Buffer.from(r.text) });
    serverJson = readJsonFile(serverFile);
    if (!r.parsed) skipped.push("server.json is not valid JSON; it was redacted line by line");
  } else skipped.push("no server.json next to the host profile");
  const configFile = ownerConfig(dir);
  if (existsSync(configFile)) {
    const r = redactJsonText(readFileSync(configFile, "utf8"));
    r.secrets.forEach((s) => secrets.add(s));
    entries.push({ name: "config.json", data: Buffer.from(r.text) });
    if (!r.parsed) skipped.push("config.json is not valid JSON; it was redacted line by line");
  } else skipped.push("no owner config.json in the host profile");

  // The dump files and the newest console log of the crashed run.
  let logFile = null;
  walk(crashDir, (p) => {
    const rel = relative(crashDir, p).split(sep);
    const base = rel[rel.length - 1];
    if (base === "console.log") {
      if (!logFile || p > logFile) logFile = p;
    } else if (DUMP_FILE.test(base)) {
      const sub = rel[0] === "crash-log" ? rel.slice(1) : rel;
      const size = statSync(p).size;
      if (size > MAX_DUMP_BYTES) skipped.push(`${sub.join("/")} is ${Math.round(size / 1048576)} MB; left out (over ${MAX_DUMP_BYTES / 1048576} MB)`);
      else entries.push({ name: `crash-dump/${sub.join("/")}`, data: readFileSync(p) });
    }
  });
  if (!entries.some((e) => e.name.startsWith("crash-dump/"))) skipped.push("no crash dump file in the crash folder");
  let tail = [];
  if (logFile) {
    tail = lastLines(redactText(tailText(logFile), [...secrets]), lines);
    entries.push({ name: "console-tail.log", data: Buffer.from(tail.join("\n") + "\n") });
  } else skipped.push("no console.log in the crash folder");

  // Build stamp and mods.
  const buildText = opts.buildText ?? "build unknown (no stamp given)";
  entries.push({ name: "build-stamp.txt", data: Buffer.from(`${buildText}\n`) });
  let mods = opts.mods;
  if (!mods) {
    mods = Array.isArray(serverJson?.game?.mods) ? serverJson.game.mods.map((m) => `${m.name ?? "(unnamed)"} (${m.modId ?? "no id"})`) : [];
    if (!mods.length) skipped.push("no game.mods in server.json; mods.txt is empty");
  }
  entries.push({ name: "mods.txt", data: Buffer.from(mods.length ? `${mods.join("\n")}\n` : "(none listed)\n") });

  // The issue draft.
  const errors = tail.filter((l) => ERROR_LINE.test(l));
  const lastError = errors.length ? errors[errors.length - 1].replace(/^\s*\d{1,2}:\d{2}:\d{2}(?:\.\d+)?\s+/, "").trim() : "no error line in the log tail";
  const title = redactText(`Server crash ${name.replace(/-crash$/, "")}: ${clip(lastError, 100)}`, [...secrets]);
  const body = redactText(
    [
      `# ${title}`,
      "",
      "## What happened",
      "",
      "(operator: what the players saw, and what was running)",
      "",
      "## Crash",
      "",
      `- Crash folder: \`${name}\``,
      `- ${buildText}`,
      "- Mods:",
      ...(mods.length ? mods.map((m) => `  - ${m}`) : ["  - (none listed)"]),
      "",
      "## Last errors in the log tail",
      "",
      "```",
      ...(errors.length ? errors.slice(-8).map((l) => clip(l.trimEnd(), 300)) : ["(none)"]),
      "```",
      "",
      "## Attached",
      "",
      "`crashpack` zip: crash dump, console log tail, build stamp, mod list, `server.json` and `config.json`.",
      "Passwords, tokens and Discord webhook URLs are `<redacted>`. A dump is raw process memory and is not redacted: attach this only to a private issue or one you trust.",
      ...(skipped.length ? ["", "## Left out", "", ...skipped.map((s) => `- ${s}`)] : []),
      "",
    ].join("\n"),
    [...secrets],
  );
  entries.push({ name: "issue-body.md", data: Buffer.from(body) });

  return { ok: true, name, crashDir, title, body, zip: makeZip(entries), files: entries.map((e) => e.name), skipped };
}

export { defaultBuildText };
