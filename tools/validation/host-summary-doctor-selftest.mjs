// tools/validation/host-summary-doctor-selftest.mjs
// inputs: tools/host/summary.mjs, tools/host/commands/summary.mjs, tools/lib/server-doctor.mjs, tools/validation/fixtures/host-summary-doctor/**
//
// Proves the session report opens with server-doctor's verdict on the same log (#1605): the good-boot
// fixture puts "booted" and the mod list in the report, the missing-mod fixture puts the doctor's fix
// line in it, a known SCRIPT (E) line is listed with its triage advice, the command reads the server
// name from the host's server.json, and a log with no boot header (an old fixture, a stub) gets no
// doctor section at all.
// red-proof: before summarize() ran the doctor this printed
// "FAIL good boot: the report must open with the doctor verdict" (2026-11-15).

import { cpSync, mkdirSync, mkdtempSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "validation", "fixtures", "host-summary-doctor");
let failed = 0;
const check = (cond, msg) => {
  if (!cond) {
    console.log(`FAIL  ${msg}`);
    failed++;
  }
};
const fx = (n) => readFileSync(join(FIX, n), "utf8");

const lib = await import(pathToFileURL(join(ROOT, "tools", "host", "summary.mjs")).href);
const cmd = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "summary.mjs")).href);
const triageText = fx("triage.md");
const page = (logText, extra = {}) => lib.renderMarkdown(lib.summarize({ logText, triageText, ...extra }), "STAMP");

// Good boot: the page opens with the verdict, says booted, lists the mods and the server name.
const good = page(fx("good-boot.log"), { serverName: "Everon 1989 Test" });
const goodLines = good.split("\n");
check(/^## Server doctor/.test(goodLines[2] ?? ""), "good boot: the report must open with the doctor verdict");
check(/PASS/.test(good) && /booted/.test(good), "good boot: the report must say PASS and booted");
check(/Mods loaded \(4\)/.test(good) && /LifeFramework/.test(good) && /LF_Everon1989/.test(good) && /ArmaReforger/.test(good), "good boot: the report must list the four mods");
check(/Server name: Everon 1989 Test/.test(good), "good boot: the report must carry the server name");
check(goodLines.findIndex((l) => /^## Server doctor/.test(l)) < goodLines.findIndex((l) => /^- Log:/.test(l)), "good boot: the doctor section comes before the log facts");

// Missing mod: the doctor's fix line is in the report.
const miss = page(fx("missing-mod.log"));
check(/FAIL/.test(miss) && /stage "mods"/.test(miss), `missing mod: the report must say FAIL at stage mods:\n${miss}`);
check(/Fix: A mod could not load/.test(miss), `missing mod: the report must carry the doctor's fix line:\n${miss}`);
check(/Server name: unknown/.test(miss), "no server name given: the report says unknown");

// A known line the doctor marked is listed with the triage advice.
const known = page(fx("known-error.log"));
check(/Known lines marked: 1/.test(known) && /m_iSlot/.test(known) && /OPEN: Fixture row/.test(known), `known error: the marked line and its advice must be listed:\n${known}`);

// No boot header: no doctor section (the older host-summary fixture stays as it was).
const bare = page("18:00:00.000  DEFAULT      : Engine start\n18:00:30.000  NETWORK      : Entered online game state.\n");
check(!/Server doctor/.test(bare), "a log with no boot header must get no doctor section");

// The command: server name from <root>/server.json, the page written under <host>/sessions.
const tmp = mkdtempSync(join(tmpdir(), "lf-host-summary-doctor-"));
try {
  const host = join(tmp, "host");
  const logDir = join(host, "profile", "logs", "logs_2026-11-15_10-02-00");
  mkdirSync(logDir, { recursive: true });
  cpSync(join(FIX, "good-boot.log"), join(logDir, "console.log"));
  mkdirSync(join(tmp, "docs"), { recursive: true });
  cpSync(join(FIX, "triage.md"), join(tmp, "docs", "debug-log-triage.md"));
  writeFileSync(join(tmp, "server.json"), JSON.stringify({ game: { name: "Everon 1989 Test" } }));
  const real = console.log;
  console.log = () => {};
  let code;
  try {
    code = await cmd.main([], { root: tmp, hostDir: host });
  } finally {
    console.log = real;
  }
  const dir = join(host, "sessions");
  const files = readdirSync(dir);
  const md = files.length ? readFileSync(join(dir, files[0]), "utf8") : "";
  check(code === 0 && /Server name: Everon 1989 Test/.test(md) && /PASS/.test(md) && /booted/.test(md), `command page lacks the doctor section:\n${md}`);
} finally {
  safeRemoveTree(tmp);
}

if (failed) {
  console.log(`SUMMARY host-summary-doctor-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY host-summary-doctor-selftest ok");
