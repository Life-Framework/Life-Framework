// tools/validation/host-drift-selftest.mjs
// inputs: tools/host/**, tools/validation/fixtures/host-drift/**
//
// Proves `tools\cli host drift` (#1462) on a fixture flagship folder and fixture host dirs: equal files
// pass; a changed `dayCycle.dayLengthMinutes` fails naming the field; a different `passwordAdmin`, a host
// `password`, `admins` and a `staff` section are ignored and never printed; a missing host file and a
// field only the host has are drift.
// red-proof: before tools/host/drift.mjs existed this file died on the import:
// "Cannot find module '.../tools/host/drift.mjs'" (2026-11-15).

import { cpSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { safeRemoveTree } from "../link-safety.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FLAGSHIP = join(ROOT, "tools", "validation", "fixtures", "host-drift", "flagship");
const { driftHost, serverJsonPath, configJsonPath } = await import(pathToFileURL(join(ROOT, "tools", "host", "drift.mjs")).href);
const { main } = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "drift.mjs")).href);

const failures = [];
const fail = (m) => failures.push(m);
const base = mkdtempSync(join(tmpdir(), "lf-host-drift-"));

// A host dir whose live files start as a copy of the flagship fixture; `edit` changes them.
let n = 0;
function hostWith(edit = () => {}) {
  const dir = join(base, `host${n++}`);
  const cfg = JSON.parse(readFileSync(join(FLAGSHIP, "config.json"), "utf8"));
  const srv = JSON.parse(readFileSync(join(FLAGSHIP, "server.json"), "utf8"));
  edit(cfg, srv);
  mkdirSync(dirname(configJsonPath(dir)), { recursive: true });
  writeFileSync(configJsonPath(dir), JSON.stringify(cfg, null, 2));
  writeFileSync(serverJsonPath(dir), JSON.stringify(srv, null, 2));
  return dir;
}

async function run(dir) {
  const lines = [];
  const log = console.log, err = console.error;
  console.log = (...a) => lines.push(a.join(" "));
  console.error = (...a) => lines.push(a.join(" "));
  let code;
  try { code = await main(["--flagship", FLAGSHIP], { root: ROOT, hostDir: dir }); } finally { console.log = log; console.error = err; }
  return { code, out: lines.join("\n") };
}

try {
  // 1. Equal files pass.
  let r = await run(hostWith());
  if (r.code !== 0) fail(`equal files: expected exit 0, got ${r.code}\n${r.out}`);

  // 2. A changed dayLengthMinutes fails and names the field and both values.
  r = await run(hostWith((c) => { c.dayCycle.dayLengthMinutes = 60; }));
  if (r.code !== 1) fail(`changed dayLengthMinutes: expected exit 1, got ${r.code}`);
  if (!/dayCycle\.dayLengthMinutes/.test(r.out) || !/120/.test(r.out) || !/60/.test(r.out)) fail(`changed dayLengthMinutes: output does not name the field and both values:\n${r.out}`);

  // 3. Host-only secrets are ignored and never printed.
  r = await run(hostWith((c, s) => {
    s.game.passwordAdmin = "hunter2-admin";
    s.game.password = "hunter2-join";
    s.game.admins = ["uid-1"];
    c.staff = { webhookUrl: "https://discord.example/hook/abc" };
  }));
  if (r.code !== 0) fail(`secrets only: expected exit 0, got ${r.code}\n${r.out}`);
  if (/hunter2|discord\.example|uid-1/.test(r.out)) fail(`secrets only: a secret value was printed:\n${r.out}`);

  // 4. A secret differing alongside a real drift: the drift is named, the secret is not printed.
  r = await run(hostWith((c, s) => { s.game.passwordAdmin = "hunter2-admin"; s.game.name = "Other"; }));
  if (r.code !== 1 || !/game\.name/.test(r.out)) fail(`name drift: expected exit 1 naming game.name:\n${r.out}`);
  if (/hunter2/.test(r.out) || /passwordAdmin/.test(r.out)) fail(`name drift: the ignored passwordAdmin leaked into the output:\n${r.out}`);

  // 5. A field only the host has, and a changed array element.
  r = await run(hostWith((c) => { c.dayCycle.extra = true; c.restarts.times[1] = "19:00"; }));
  if (r.code !== 1 || !/dayCycle\.extra/.test(r.out) || !/restarts\.times\[1\]/.test(r.out)) fail(`extra/array drift: expected both fields named:\n${r.out}`);

  // 6. A missing host file is drift, not a crash.
  const empty = join(base, "empty");
  mkdirSync(empty, { recursive: true });
  r = await run(empty);
  if (r.code !== 1 || !/config\.json/.test(r.out) || !/server\.json/.test(r.out)) fail(`missing files: expected exit 1 naming both files:\n${r.out}`);

  // 7. The library result carries the changed field list.
  const d = driftHost(hostWith((c) => { c.dayCycle.mode = "realtime"; }), { flagship: FLAGSHIP });
  if (d.ok || !d.changes.some((c) => c.file === "config.json" && c.field === "dayCycle.mode")) fail(`driftHost: expected a config.json dayCycle.mode change: ${JSON.stringify(d)}`);
} finally {
  safeRemoveTree(base);
}

if (failures.length) {
  for (const f of failures) console.error(`FAIL host-drift-selftest: ${f}`);
  process.exit(1);
}
console.log("host-drift-selftest: ok (equal passes, field drift named, host-only secrets ignored and never printed, missing files are drift)");
