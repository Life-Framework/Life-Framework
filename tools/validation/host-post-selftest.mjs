// tools/validation/host-post-selftest.mjs
// inputs: tools/host/discord-post.mjs, tools/host/commands/post.mjs, tools/host/announce.mjs, docs/community/announce-template.md
//
// `tools\cli host post session|wipe [--send]` (#1414) prints the post `host announce` drafted from the next
// session in sessions.json and, only with --send, POSTs it to the announcements webhook URL kept in the
// host's secrets.json (`announceWebhookUrl`). The HTTP call goes through a seam: every case here injects a
// sender (ctx.send) or talks to a fake server on 127.0.0.1, and global fetch is replaced with a thrower so
// no case can reach the real Discord API. The URL is never printed.
// red-proof: before tools/host/commands/post.mjs existed the import failed with
// "Cannot find module .../tools/host/commands/post.mjs" (2026-11-15).

import { existsSync, mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { createServer } from "node:http";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
let failed = 0;
const fail = (msg) => {
  console.log(`FAIL  ${msg}`);
  failed++;
};
const eq = (name, got, want) => {
  if (got !== want) fail(`${name}\n   got:  ${JSON.stringify(got)}\n   want: ${JSON.stringify(want)}`);
};

const cmdPath = join(ROOT, "tools", "host", "commands", "post.mjs");
const libPath = join(ROOT, "tools", "host", "discord-post.mjs");
for (const p of [cmdPath, libPath]) {
  if (!existsSync(p)) {
    fail(`${p.slice(ROOT.length + 1)} is missing`);
    console.log(`host-post-selftest: ${failed} case(s) failed`);
    process.exit(1);
  }
}
const cmd = await import(pathToFileURL(cmdPath).href);
const lib = await import(pathToFileURL(libPath).href);

// Nothing here may reach the network except the loopback fake server below.
const realFetch = globalThis.fetch;
globalThis.fetch = (url, ...rest) => {
  if (!String(url).startsWith("http://127.0.0.1:")) {
    fail(`a case tried a real network call to ${String(url).replace(/\/api\/webhooks\/.*/, "/api/webhooks/<redacted>")}`);
    return Promise.reject(new Error("blocked by host-post-selftest"));
  }
  return realFetch(url, ...rest);
};

const dir = mkdtempSync(join(tmpdir(), "lf-host-post-"));
const hostDir = join(dir, "host");
const sessions = join(dir, "sessions.json");
const secrets = join(hostDir, "secrets.json");
const FAKE_URL = "https://discord.com/api/webhooks/123456789/SELFTEST-TOKEN-never-posted";
const AT = "2026-11-22T19:00:00Z";
const epoch = Math.floor(Date.parse(AT) / 1000);
const BUILD = "1.8.0.13";
process.env.LF_ANNOUNCE_SESSIONS = sessions;
const writeSessions = (row) => writeFileSync(sessions, JSON.stringify({ next: row }));
const writeSecrets = async (obj) => {
  rmSync(hostDir, { recursive: true, force: true });
  mkdirSync(hostDir, { recursive: true });
  writeFileSync(secrets, JSON.stringify(obj));
};

// Runs the command; returns { code, out (everything it printed), sent (bodies the seam saw) }.
async function run(args, { send } = {}) {
  const sent = [];
  const lines = [];
  const log = console.log, err = console.error;
  console.log = (...a) => lines.push(a.join(" "));
  console.error = (...a) => lines.push(a.join(" "));
  let code;
  try {
    code = await cmd.main(args, {
      root: ROOT,
      hostDir,
      send: send ?? (async (url, content) => {
        sent.push({ url, content });
        return { ok: true, status: 204 };
      }),
    });
  } finally {
    console.log = log;
    console.error = err;
  }
  return { code, out: lines.join("\n"), sent };
}

try {
  // 1. dry run: prints the post, sends nothing, exits 0 (the URL is not even needed).
  writeSessions({ at: AT, build: BUILD, wipe: false });
  await writeSecrets({ announceWebhookUrl: FAKE_URL });
  let r = await run(["session"]);
  eq("dry run exit", r.code, 0);
  eq("dry run sends nothing", r.sent.length, 0);
  for (const want of [`<t:${epoch}:F>`, `<t:${epoch}:R>`, BUILD, "Life Framework | Everon 1989 (Alpha)"]) {
    if (!r.out.includes(want)) fail(`dry run did not print ${JSON.stringify(want)}:\n${r.out}`);
  }
  if (/WIPE/.test(r.out)) fail(`a plain session post mentions the wipe:\n${r.out}`);

  // 2. --send: exactly one send, under 2000 characters, equal to what the dry run printed; no URL printed.
  r = await run(["session", "--send"]);
  eq("--send exit", r.code, 0);
  eq("--send sends once", r.sent.length, 1);
  eq("--send uses the secrets.json URL", r.sent[0]?.url, FAKE_URL);
  const body = r.sent[0]?.content ?? "";
  if (body.length === 0 || body.length >= 2000) fail(`--send body is ${body.length} characters, want 1..1999`);
  if (!body.includes(BUILD)) fail(`--send body lacks the build:\n${body}`);
  if (r.out.includes("SELFTEST-TOKEN") || r.out.includes("/api/webhooks/")) fail(`the output shows the webhook URL:\n${r.out}`);

  // 3. a missing URL: --send exits 1, says so, sends nothing, still no URL printed.
  await writeSecrets({ staffWebhookUrl: "https://discord.com/api/webhooks/9/staff" });
  r = await run(["session", "--send"]);
  eq("missing URL exit", r.code, 1);
  eq("missing URL sends nothing", r.sent.length, 0);
  if (!/announceWebhookUrl/.test(r.out)) fail(`missing URL does not say which key to set:\n${r.out}`);
  rmSync(hostDir, { recursive: true, force: true });
  r = await run(["session", "--send"]);
  eq("no secrets.json exit", r.code, 1);
  eq("no secrets.json sends nothing", r.sent.length, 0);

  // 4. a URL that is not a Discord webhook is refused before anything is sent (the staff URL is never reused).
  for (const bad of ["http://discord.com/api/webhooks/1/x", "https://example.com/api/webhooks/1/x", "https://discord.com/other", "not a url"]) {
    await writeSecrets({ announceWebhookUrl: bad });
    r = await run(["session", "--send"]);
    eq(`bad URL ${bad} exit`, r.code, 1);
    eq(`bad URL ${bad} sends nothing`, r.sent.length, 0);
    if (r.out.includes(bad) && bad.includes("/")) fail(`the refusal echoes the URL ${bad}`);
  }

  // 5. wipe: needs a wipe session, posts the wipe notice; a plain session is refused.
  await writeSecrets({ announceWebhookUrl: FAKE_URL });
  r = await run(["wipe", "--send"]);
  eq("wipe on a plain session exit", r.code, 1);
  eq("wipe on a plain session sends nothing", r.sent.length, 0);
  writeSessions({ at: AT, build: BUILD, wipe: true });
  r = await run(["wipe"]);
  eq("wipe dry run exit", r.code, 0);
  eq("wipe dry run sends nothing", r.sent.length, 0);
  if (!/WIPE/.test(r.out) || !r.out.includes(`<t:${epoch}:F>`)) fail(`wipe post lacks the wipe line or the time:\n${r.out}`);
  r = await run(["wipe", "--send"]);
  eq("wipe --send exit", r.code, 0);
  eq("wipe --send sends once", r.sent.length, 1);
  if (!/WIPE/.test(r.sent[0]?.content ?? "")) fail(`the wipe body lacks the wipe line:\n${r.sent[0]?.content}`);

  // 6. no next session: nothing to post, exit 1.
  writeSessions(null);
  r = await run(["session", "--send"]);
  eq("no next session exit", r.code, 1);
  eq("no next session sends nothing", r.sent.length, 0);

  // 7. a sender that fails: exit 1, one line, no URL.
  writeSessions({ at: AT, build: BUILD, wipe: false });
  r = await run(["session", "--send"], { send: async () => ({ ok: false, status: 429, error: "HTTP 429" }) });
  eq("failed send exit", r.code, 1);
  if (r.out.includes("SELFTEST-TOKEN")) fail(`a failed send printed the URL:\n${r.out}`);
  r = await run(["session", "--send"], { send: async () => { throw new Error(`boom at ${FAKE_URL}`); } });
  eq("throwing sender exit", r.code, 1);
  if (r.out.includes("SELFTEST-TOKEN")) fail(`a throwing sender leaked the URL:\n${r.out}`);

  // 8. bad arguments.
  r = await run([]);
  eq("no kind exit", r.code, 2);
  r = await run(["session", "--bogus"]);
  eq("unknown flag exit", r.code, 2);

  // 9. a post over 2000 characters is refused, never truncated or split.
  writeSessions({ at: AT, build: "x".repeat(2100), wipe: false });
  r = await run(["session", "--send"]);
  eq("oversize exit", r.code, 1);
  eq("oversize sends nothing", r.sent.length, 0);

  // 10. the library sender against a fake server on loopback: JSON body, no mentions, bounded by a timeout.
  const hits = [];
  const server = createServer((req, res) => {
    let data = "";
    req.on("data", (c) => (data += c));
    req.on("end", () => {
      hits.push({ method: req.method, type: req.headers["content-type"], body: data });
      if (req.url === "/hang") return; // never answers
      res.statusCode = req.url === "/fail" ? 500 : 204;
      res.end();
    });
  });
  await new Promise((ok) => server.listen(0, "127.0.0.1", ok));
  const base = `http://127.0.0.1:${server.address().port}`;
  try {
    const ok = await lib.postToWebhook(`${base}/ok`, "hello @everyone", { timeoutMs: 2000 });
    eq("lib send ok", ok.ok, true);
    eq("lib one request", hits.length, 1);
    eq("lib method", hits[0]?.method, "POST");
    eq("lib content type", hits[0]?.type, "application/json");
    const parsed = JSON.parse(hits[0]?.body ?? "{}");
    eq("lib body content", parsed.content, "hello @everyone");
    eq("lib body allows no mentions", JSON.stringify(parsed.allowed_mentions), JSON.stringify({ parse: [] }));
    const bad = await lib.postToWebhook(`${base}/fail`, "x", { timeoutMs: 2000 });
    eq("lib HTTP 500 is not ok", bad.ok, false);
    const t0 = Date.now();
    const hung = await lib.postToWebhook(`${base}/hang`, "x", { timeoutMs: 300 });
    eq("lib timeout is not ok", hung.ok, false);
    if (Date.now() - t0 > 3000) fail(`the timeout took ${Date.now() - t0} ms, want about 300`);
  } finally {
    server.closeAllConnections?.();
    await new Promise((ok) => server.close(ok));
  }
} finally {
  rmSync(dir, { recursive: true, force: true });
}

console.log(failed ? `host-post-selftest: ${failed} case(s) failed` : "host-post-selftest: ok");
process.exit(failed ? 1 : 0);
