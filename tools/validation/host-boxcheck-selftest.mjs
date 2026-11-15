// tools/validation/host-boxcheck-selftest.mjs
// inputs: tools/host/boxcheck.mjs, tools/host/commands/boxcheck.mjs, docs/ops/rented-box.md
//
// Proves `tools\cli host boxcheck`: injected machine facts give a pass, a short-RAM fail and a
// missing-install fail with one line per item and one failing line each; the remote reading
// parses; and the spec numbers still match the table in docs/ops/rented-box.md step 2.
// red-proof: before tools/host/boxcheck.mjs existed the import failed:
// "FAIL boxcheck.mjs missing (Cannot find module ...)" (2026-11-15).

import { readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const GB = 1024 ** 3;
let failed = 0;
function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

let box, cmd;
try {
  box = await import(pathToFileURL(join(ROOT, "tools", "host", "boxcheck.mjs")).href);
  cmd = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "boxcheck.mjs")).href);
} catch (e) {
  fail(`boxcheck.mjs missing (${e.message})`);
  console.log("SUMMARY host-boxcheck-selftest fail 1");
  process.exit(1);
}

const good = { cores: 8, clockMhz: 3500, ramBytes: 16 * GB, freeBytes: 200 * GB, installDir: "C:/srv", installFound: true, build: "1.9.0.37" };

// capture the command's output
async function run(facts, args = []) {
  const lines = [];
  const log = console.log;
  console.log = (...a) => lines.push(a.join(" "));
  let code;
  try {
    code = await cmd.main(args, { root: ROOT, hostDir: ROOT, facts });
  } finally {
    console.log = log;
  }
  return { code, lines };
}

// pass: five item lines (cores, clock, ram, disk, install), no FAIL, PASS verdict, exit 0
{
  const r = box.compare(good);
  if (!r.ok || r.failed !== 0) fail("a 8-core 16 GB box with the install did not pass");
  const names = r.items.map((i) => i.item).join(",");
  if (names !== "cores,clock,ram,disk,install") fail(`items ${names}, want cores,clock,ram,disk,install`);
  const out = await run(good);
  if (out.code !== 0) fail(`pass exit ${out.code}, want 0`);
  if (out.lines.filter((l) => / FAIL /.test(l)).length) fail("a passing box printed a FAIL line");
  if (!out.lines.some((l) => /verdict: PASS/.test(l))) fail("no PASS verdict line");
  // header + 5 items + verdict
  if (out.lines.length !== 7) fail(`pass printed ${out.lines.length} lines, want 7`);
}

// a 15.6 GB reading of a 16 GB box (firmware reserve) still passes
{
  const r = box.compare({ ...good, ramBytes: 15.6 * GB });
  if (!r.ok) fail("15.6 GB read as short of 16 GB");
}

// short RAM: exactly one FAIL line, on ram, exit 1
{
  const out = await run({ ...good, ramBytes: 8 * GB });
  if (out.code !== 1) fail(`short-RAM exit ${out.code}, want 1`);
  const bad = out.lines.filter((l) => / FAIL /.test(l));
  if (bad.length !== 1 || !/ram/.test(bad[0]) || !/8\.0 GB/.test(bad[0])) fail(`short-RAM fail lines: ${JSON.stringify(bad)}`);
  if (!out.lines.some((l) => /verdict: FAIL, 1 of 4/.test(l))) fail("short-RAM verdict is not 'FAIL, 1 of 4'");
}

// missing install: exactly one FAIL line, on install, exit 1
{
  const out = await run({ ...good, installFound: false, build: null });
  if (out.code !== 1) fail(`missing-install exit ${out.code}, want 1`);
  const bad = out.lines.filter((l) => / FAIL /.test(l));
  if (bad.length !== 1 || !/install/.test(bad[0]) || !/not found/.test(bad[0])) fail(`missing-install fail lines: ${JSON.stringify(bad)}`);
}

// too few cores and too little disk each fail their own item; unreadable facts fail, never pass
{
  const r = box.compare({ ...good, cores: 2, freeBytes: 30 * GB });
  const names = r.items.filter((i) => i.status === "fail").map((i) => i.item).join(",");
  if (names !== "cores,disk") fail(`2 cores / 30 GB failed ${names}, want cores,disk`);
  const u = box.compare({ cores: null, clockMhz: null, ramBytes: null, freeBytes: null, installDir: null, installFound: false, build: null });
  if (u.failed !== 4) fail(`unreadable machine failed ${u.failed} items, want 4 (the clock is never judged)`);
  if (u.items.find((i) => i.item === "clock").status !== "info") fail("clock is judged");
}

// remote reading: the lf-box lines parse into facts, junk lines and missing keys stay null
{
  const f = box.parseRemote("noise\nlf-box: cores=16\nlf-box: clockMhz=3200\nlf-box: ramBytes=17179869184\nlf-box: installFound=True\nlf-box: build=1.9.0.37\nlf-box: freeBytes=123456789012\n", "C:/srv");
  if (f.cores !== 16 || f.clockMhz !== 3200 || f.ramBytes !== 17179869184 || f.freeBytes !== 123456789012) fail(`parseRemote numbers: ${JSON.stringify(f)}`);
  if (!f.installFound || f.build !== "1.9.0.37" || f.installDir !== "C:/srv") fail(`parseRemote install: ${JSON.stringify(f)}`);
  const empty = box.parseRemote("");
  if (empty.cores !== null || empty.installFound !== false || empty.build !== null) fail("empty remote output not null");
  const script = box.remoteScript("C:/srv dir", (s) => `'${s.replace(/'/g, "''")}'`);
  if (!/Win32_Processor/.test(script) || !/'C:\\srv dir'/.test(script)) fail("remoteScript lacks the cpu query or the quoted install path");
}

// the command's argument errors and help
{
  if (typeof cmd.help !== "string" || !cmd.help.startsWith("boxcheck")) fail("commands/boxcheck.mjs lacks help");
  const err = console.error;
  console.error = () => {};
  const code = await cmd.main(["--ssh"], { root: ROOT, hostDir: ROOT });
  console.error = err;
  if (code !== 1) fail(`--ssh with no value exit ${code}, want 1`);
}

// the spec in the code is the spec on the page (docs/ops/rented-box.md step 2)
{
  const page = readFileSync(join(ROOT, "docs", "ops", "rented-box.md"), "utf8");
  const row = (part) => (page.split(/\r?\n/).find((l) => l.startsWith(`| ${part} |`)) ?? "");
  if (!new RegExp(`\\b${box.SPEC.cores} or more cores`).test(row("CPU"))) fail(`rented-box.md CPU row no longer says "${box.SPEC.cores} or more cores"`);
  if (!new RegExp(`\\b${box.SPEC.ramGb} GB`).test(row("RAM"))) fail(`rented-box.md RAM row no longer says ${box.SPEC.ramGb} GB`);
  if (!new RegExp(`\\b${box.SPEC.diskGb} GB`).test(row("Disk"))) fail(`rented-box.md Disk row no longer says ${box.SPEC.diskGb} GB`);
}

console.log(failed ? `SUMMARY host-boxcheck-selftest fail ${failed}` : "SUMMARY host-boxcheck-selftest ok");
process.exit(failed ? 1 : 0);
