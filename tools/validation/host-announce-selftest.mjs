// tools/validation/host-announce-selftest.mjs
// inputs: tools/host/announce.mjs, tools/host/commands/announce.mjs, docs/community/announce-template.md, website/content/sessions.json
//
// `tools\cli host announce --at <time> [--wipe]` fills the announcement template, writes the next
// session to website/content/sessions.json and (only with --send) posts it. This selftest runs on a
// fixed clock: a fixture time and build give the exact text and JSON row; --wipe adds the wipe line
// and refuses an --at 23 hours out while accepting 24. Nothing is posted and no real file is touched.
// red-proof: before tools/host/announce.mjs existed the import failed:
// "Cannot find module .../tools/host/announce.mjs" (2026-11-15).

import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
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

const libPath = join(ROOT, "tools", "host", "announce.mjs");
if (!existsSync(libPath)) {
  fail(`tools/host/announce.mjs is missing`);
  console.log(`host-announce-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
const lib = await import(pathToFileURL(libPath).href);

const NOW = Date.parse("2026-11-20T12:00:00Z");
const AT = "2026-11-22T19:00:00Z";
const BUILD = "1.8.0.13";
const template = readFileSync(join(ROOT, "docs", "community", "announce-template.md"), "utf8");

// 1. the session row matches the SJ schema: { at, build, wipe, announced }
const row = lib.sessionRow({ at: AT, build: BUILD, wipe: false });
eq("row", JSON.stringify(row), JSON.stringify({ at: "2026-11-22T19:00:00.000Z", build: BUILD, wipe: false, announced: false }));

// 2. the post for a plain session
const post = lib.buildPost({ template, at: AT, build: BUILD, wipe: false, now: NOW });
const epoch = Math.floor(Date.parse(AT) / 1000);
for (const want of [`<t:${epoch}:F>`, `<t:${epoch}:R>`, BUILD, "Life Framework | Everon 1989 (Alpha)", "life-framework.org"]) {
  if (!post.includes(want)) fail(`plain post lacks ${JSON.stringify(want)}:\n${post}`);
}
if (/\{\{|\}\}/.test(post)) fail(`plain post has an unfilled placeholder:\n${post}`);
if (/wipe/i.test(post)) fail(`plain post mentions a wipe:\n${post}`);

// 3. --wipe adds the wipe line, in the words of the policy
const wipePost = lib.buildPost({ template, at: AT, build: BUILD, wipe: true, now: NOW });
if (!/WIPE/.test(wipePost)) fail(`wipe post lacks the wipe line:\n${wipePost}`);
if (!/24 hours/.test(wipePost)) fail(`wipe post lacks the 24 hour notice:\n${wipePost}`);
if (!/progress is kept from the public launch/i.test(wipePost)) fail(`wipe post lacks the public-launch line:\n${wipePost}`);

// 4. the 24 hour rule: 23 h refused, 24 h accepted, past refused, wipe only
const at = (h) => new Date(NOW + h * 3600 * 1000).toISOString();
eq("wipe 23h refused", lib.checkAt({ at: at(23), wipe: true, now: NOW }).ok, false);
eq("wipe 24h accepted", lib.checkAt({ at: at(24), wipe: true, now: NOW }).ok, true);
eq("wipe 24h+1s accepted", lib.checkAt({ at: new Date(NOW + 24 * 3600 * 1000 + 1000).toISOString(), wipe: true, now: NOW }).ok, true);
eq("plain 23h accepted", lib.checkAt({ at: at(23), wipe: false, now: NOW }).ok, true);
eq("past refused", lib.checkAt({ at: at(-1), wipe: false, now: NOW }).ok, false);
eq("garbage refused", lib.checkAt({ at: "next friday", wipe: false, now: NOW }).ok, false);

// 5. the command: prints, writes the row, never posts without --send
const dir = mkdtempSync(join(tmpdir(), "lf-host-announce-"));
try {
  const sessions = join(dir, "sessions.json");
  writeFileSync(sessions, '{ "next": null }\n');
  const run = (args, extra = {}) =>
    spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "announce", ...args], {
      cwd: ROOT,
      encoding: "utf8",
      timeout: 60000,
      windowsHide: true,
      env: { ...process.env, LF_ANNOUNCE_NOW: String(NOW), LF_ANNOUNCE_SESSIONS: sessions, ...extra },
    });
  const out = (r) => `${r.stdout ?? ""}${r.stderr ?? ""}`;

  const r1 = run(["--at", AT, "--build", BUILD]);
  if (r1.status !== 0) fail(`announce exited ${r1.status}: ${out(r1)}`);
  if (!out(r1).includes(`<t:${epoch}:F>`)) fail(`announce did not print the post: ${out(r1)}`);
  eq("sessions.json after announce", readFileSync(sessions, "utf8"), JSON.stringify({ next: row }, null, 2) + "\n");

  // a refused wipe leaves the file alone
  const before = readFileSync(sessions, "utf8");
  const r2 = run(["--at", at(23), "--build", BUILD, "--wipe"]);
  if (r2.status === 0) fail(`announce --wipe 23h out must exit non-zero: ${out(r2)}`);
  eq("refused wipe leaves sessions.json", readFileSync(sessions, "utf8"), before);

  const r3 = run(["--at", at(24), "--build", BUILD, "--wipe"]);
  if (r3.status !== 0) fail(`announce --wipe 24h out must pass (exit ${r3.status}): ${out(r3)}`);
  if (JSON.parse(readFileSync(sessions, "utf8")).next?.wipe !== true) fail("wipe row did not set wipe=true");

  // --send with no channel must fail before it posts anything
  const r4 = run(["--at", AT, "--build", BUILD, "--send"], { LF_ANNOUNCE_CHANNEL: "" });
  if (r4.status === 0) fail(`announce --send without a channel must exit non-zero: ${out(r4)}`);
} finally {
  rmSync(dir, { recursive: true, force: true });
}

const real = JSON.parse(readFileSync(join(ROOT, "website", "content", "sessions.json"), "utf8"));
if (!("next" in real)) fail("website/content/sessions.json has no `next` key");

if (failed) {
  console.log(`host-announce-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log("host-announce-selftest: OK (template fill, session row, wipe line, 24 h rule, command prints and writes, no post without --send)");
