// tools/validation/host-crashpack-selftest.mjs
// inputs: tools/host/**, tools/release/pack-playtest.mjs, tools/validation/fixtures/host-crashpack/**
//
// `tools\cli host crashpack` (#1457) bundles a crash for a GitHub issue. This check builds a fixture
// host folder in a temp dir (a crash backup with a dump and a console log, a server.json and a
// config.json full of fake secrets), makes the zip, reads it back and proves:
//   - server.json has <redacted> for password, passwordAdmin and every discord webhook URL, and is
//     otherwise the same JSON (no other change);
//   - config.json gets the same treatment;
//   - no fake secret appears anywhere in any file of the zip (names and bytes), log tail and issue
//     body included;
//   - the zip holds the dump, the console log tail, the build stamp, the mod list and the issue body;
//   - --session picks the named crash, the default is the newest, a missing crash is a clear error;
//   - the CLI path (`host crashpack`) writes the same zip.
// red-proof: before tools/host/crashpack.mjs existed this check failed to import it
// (ERR_MODULE_NOT_FOUND, 2026-11-15); with the redaction of `password` switched off it failed with
// "leaked secret FAKE-JOIN-PW-7731" (2026-11-15).

import { copyFileSync, existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "validation", "fixtures", "host-crashpack");
const cp = await import(pathToFileURL(join(ROOT, "tools", "host", "crashpack.mjs")).href);
const { readZip } = await import(pathToFileURL(join(ROOT, "tools", "release", "pack-playtest.mjs")).href);
const { safeRemoveTree } = await import(pathToFileURL(join(ROOT, "tools", "link-safety.mjs")).href);

const failures = [];
const fail = (m) => failures.push(m);

const SECRETS = [
  "FAKE-JOIN-PW-7731",
  "FAKE-ADMIN-PW-4410",
  "111111111111111111/FAKE-WEBHOOK-TOKEN-aaaa",
  "222222222222222222/FAKE-WEBHOOK-TOKEN-bbbb",
  "333333333333333333/FAKE-WEBHOOK-TOKEN-cccc",
  "444444444444444444/FAKE-WEBHOOK-TOKEN-dddd",
  "FAKE-WEBHOOK-TOKEN",
];

const base = mkdtempSync(join(tmpdir(), "lf-crashpack-selftest-"));
try {
  // A host folder: <host>/server.json, <host>/profile/profile/LifeFramework/config.json, two crash backups.
  const host = join(base, "host");
  const profileOwner = join(host, "profile", "profile", "LifeFramework");
  mkdirSync(profileOwner, { recursive: true });
  copyFileSync(join(FIX, "server.json"), join(host, "server.json"));
  copyFileSync(join(FIX, "config.json"), join(profileOwner, "config.json"));
  const crash = (name, logsDir) => {
    const logs = join(host, "backups", name, "crash-log", logsDir);
    mkdirSync(logs, { recursive: true });
    copyFileSync(join(FIX, "console.log.txt"), join(logs, "console.log"));
    return join(host, "backups", name);
  };
  const oldCrash = crash("2026-11-14_09-00-00-crash", "logs_2026-11-14_08-00-00");
  writeFileSync(join(oldCrash, "crash-log", "old-marker.txt"), "old");
  const newCrash = crash("2026-11-15_10-00-00-crash", "logs_2026-11-15_09-00-00");
  const dump = Buffer.from([0x4d, 0x44, 0x4d, 0x50, 0x93, 0xa7, 0x00, 0xff, 0x10, 0x20]);
  writeFileSync(join(newCrash, "crash-log", "logs_2026-11-15_09-00-00", "crash.dmp"), dump);
  // A plain (non-crash) backup is never picked.
  mkdirSync(join(host, "backups", "2026-11-15_11-00-00-stop"), { recursive: true });

  const opts = { buildText: "Arma Reforger build 99999 (installed 2026-11-01)", mods: null };
  const r = cp.buildCrashpack(host, opts);
  if (!r.ok) fail(`buildCrashpack failed: ${r.reason}`);
  else {
    if (r.name !== "2026-11-15_10-00-00-crash") fail(`the default is not the newest crash: ${r.name}`);
    const files = new Map(readZip(r.zip).map((e) => [e.name, e.data]));
    for (const want of ["server.json", "config.json", "console-tail.log", "build-stamp.txt", "mods.txt", "issue-body.md", "crash-dump/logs_2026-11-15_09-00-00/crash.dmp"]) {
      if (!files.has(want)) fail(`the zip has no ${want} (has: ${[...files.keys()].join(", ")})`);
    }

    // server.json: exactly the redactions, nothing else.
    if (files.has("server.json")) {
      const got = JSON.parse(files.get("server.json").toString("utf8"));
      const want = JSON.parse(readFileSync(join(FIX, "server.json"), "utf8"));
      want.game.password = "<redacted>";
      want.game.passwordAdmin = "<redacted>";
      want.operating.alertHook = "<redacted>";
      want.alerts[0] = "<redacted>";
      if (JSON.stringify(got) !== JSON.stringify(want)) fail(`server.json differs from the source beyond the redactions:\n${files.get("server.json").toString("utf8")}`);
      if (got.game?.password !== "<redacted>") fail("server.json game.password is not <redacted>");
      if (got.game?.passwordAdmin !== "<redacted>") fail("server.json game.passwordAdmin is not <redacted>");
    }
    if (files.has("config.json")) {
      const got = JSON.parse(files.get("config.json").toString("utf8"));
      const want = JSON.parse(readFileSync(join(FIX, "config.json"), "utf8"));
      want.staff.webhookUrl = "<redacted>";
      if (JSON.stringify(got) !== JSON.stringify(want)) fail(`config.json differs from the source beyond the redactions:\n${files.get("config.json").toString("utf8")}`);
    }

    // The dump is carried byte for byte.
    const dumpGot = files.get("crash-dump/logs_2026-11-15_09-00-00/crash.dmp");
    if (dumpGot && !dumpGot.equals(dump)) fail("the crash dump was changed");

    // The log tail keeps its lines and loses its secrets.
    const tail = files.get("console-tail.log")?.toString("utf8") ?? "";
    if (!tail.includes("Virtual Machine Exception") || !tail.includes("CRASH: access violation")) fail("console-tail.log lost the crash lines");
    if (!tail.includes("<redacted>")) fail("console-tail.log has no <redacted> where the webhook was");

    // Build stamp and mods.
    if (!files.get("build-stamp.txt")?.toString("utf8").includes("build 99999")) fail("build-stamp.txt lacks the build stamp");
    const mods = files.get("mods.txt")?.toString("utf8") ?? "";
    if (!mods.includes("Life Framework (Core)") || !mods.includes("B1E84D5687E11724")) fail(`mods.txt does not list the server.json mods: ${mods}`);

    // The issue body names the crash, the build and the last error.
    const body = files.get("issue-body.md")?.toString("utf8") ?? "";
    for (const need of ["2026-11-15_10-00-00-crash", "build 99999", "Virtual Machine Exception"]) {
      if (!body.includes(need)) fail(`issue-body.md lacks "${need}"`);
    }
    if (!/^#\s+/m.test(body) || typeof r.title !== "string" || !r.title) fail("no issue title");

    // No secret anywhere: every file, every name, the issue title, the zip bytes' text view.
    const haystacks = [...[...files.entries()].map(([n, d]) => [n, Buffer.concat([Buffer.from(n), d]).toString("latin1")]), ["title", r.title], ["raw zip", r.zip.toString("latin1")]];
    for (const [where, text] of haystacks) {
      for (const s of SECRETS) if (text.includes(s)) fail(`leaked secret ${s} in ${where}`);
    }
  }

  // --session names one crash: by full name or by the stamp without "-crash".
  for (const session of ["2026-11-14_09-00-00-crash", "2026-11-14_09-00-00"]) {
    const o = cp.buildCrashpack(host, { ...opts, session });
    if (!o.ok || o.name !== "2026-11-14_09-00-00-crash") fail(`--session ${session} picked ${o.ok ? o.name : o.reason}`);
  }
  const none = cp.buildCrashpack(host, { ...opts, session: "1999-01-01_00-00-00" });
  if (none.ok || !/no crash/i.test(none.reason ?? "")) fail(`a missing session did not fail clearly: ${JSON.stringify({ ok: none.ok, reason: none.reason })}`);
  const empty = cp.buildCrashpack(join(base, "no-host"), opts);
  if (empty.ok || !/no crash/i.test(empty.reason ?? "")) fail("a host with no crash did not fail clearly");

  // --lines limits the tail.
  const short = cp.buildCrashpack(host, { ...opts, lines: 2 });
  const shortTail = short.ok ? (readZip(short.zip).find((e) => e.name === "console-tail.log")?.data.toString("utf8") ?? "") : "";
  if (shortTail.trim().split("\n").length !== 2) fail(`--lines 2 gave ${JSON.stringify(shortTail)}`);

  // A server.json that is not JSON is still redacted, never copied raw.
  const bad = cp.redactJsonText('{ "game": { "password": "FAKE-JOIN-PW-7731", // x\n "passwordAdmin": "FAKE-ADMIN-PW-4410" } ');
  if (bad.text.includes("FAKE-JOIN-PW-7731") || bad.text.includes("FAKE-ADMIN-PW-4410")) fail(`an unparsable server.json leaked: ${bad.text}`);
  // An empty password is not a secret and stays as it is.
  if (JSON.parse(cp.redactJsonText('{"game":{"password":"","passwordAdmin":"x1y2z3"}}').text).game.password !== "") fail("an empty password was rewritten");

  // The CLI path.
  const outDir = join(base, "out");
  const cli = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "crashpack", "--session", "2026-11-15_10-00-00", "--out", outDir], {
    cwd: ROOT,
    encoding: "utf8",
    timeout: 60000,
    windowsHide: true,
    env: { ...process.env, LF_HOST_DIR: host },
  });
  const cliOut = `${cli.stdout ?? ""}${cli.stderr ?? ""}`;
  const zipFile = join(outDir, "2026-11-15_10-00-00-crash.zip");
  if (cli.status !== 0) fail(`host crashpack exited ${cli.status}: ${cliOut.trim()}`);
  else if (!existsSync(zipFile)) fail(`host crashpack wrote no ${zipFile}: ${cliOut.trim()}`);
  else {
    const raw = readFileSync(zipFile).toString("latin1");
    for (const s of SECRETS) if (raw.includes(s) || cliOut.includes(s)) fail(`the CLI leaked secret ${s}`);
    const names = readZip(readFileSync(zipFile)).map((e) => e.name);
    if (!names.includes("server.json") || !names.includes("issue-body.md")) fail(`CLI zip lacks files: ${names.join(", ")}`);
    if (!/\[LFDebug:HostCrashpack\]/.test(cliOut)) fail(`the CLI printed no [LFDebug:HostCrashpack] line: ${cliOut.trim()}`);
  }
  const cliBad = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "host", "crashpack", "--session", "1999-01-01_00-00-00", "--out", outDir], {
    cwd: ROOT,
    encoding: "utf8",
    timeout: 60000,
    windowsHide: true,
    env: { ...process.env, LF_HOST_DIR: host },
  });
  if (cliBad.status === 0) fail("host crashpack for a missing crash exited 0");
} finally {
  safeRemoveTree(base);
}

if (failures.length) {
  for (const f of failures) console.log(`FAIL  ${f}`);
  console.log(`host-crashpack-selftest: ${failures.length} case(s) failed`);
  process.exit(1);
}
console.log("host-crashpack-selftest: OK (server.json and config.json redact passwords and webhooks and change nothing else; no secret in any zip file)");
