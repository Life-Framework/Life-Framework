// tools/validation/host-ready-config-selftest.mjs
// inputs: tools/host/ready.mjs, tools/host/announce.mjs, tools/host/config-apply.mjs
//
// Proves the `host ready` config check (#1559): the live profile's server.json must hold a non-empty
// game password (the alpha gate is a password only) and the exact listing name. An empty or missing
// password is NO-GO, a wrong name is NO-GO naming the expected name, both right is green, a missing or
// unreadable file is NO-GO, and no output ever contains the password. The check is also in the
// default list `host ready` runs.
// red-proof: before configCheck existed the import found no export:
// "FAIL ready.mjs does not export configCheck" (2026-11-15).

import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const NAME = "Life Framework | Everon 1989 (Alpha)";
const SECRET = "hunter2-S3CRET-pw";
const tmp = mkdtempSync(join(tmpdir(), "lf-host-ready-config-"));
let failed = 0;
function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

let n = 0;
// A host dir whose profile/server.json is `body` (an object, a raw string, or undefined for no file).
function hostWith(body) {
  const dir = join(tmp, `h${n++}`);
  mkdirSync(join(dir, "profile"), { recursive: true });
  if (body !== undefined) writeFileSync(join(dir, "profile", "server.json"), typeof body === "string" ? body : JSON.stringify(body));
  return dir;
}
const cfg = (name, password) => ({ game: { name, password } });
const text = (r) => `${r.detail}`;

try {
  let ready, announce;
  try {
    ready = await import(pathToFileURL(join(ROOT, "tools", "host", "ready.mjs")).href);
    announce = await import(pathToFileURL(join(ROOT, "tools", "host", "announce.mjs")).href);
  } catch (e) {
    fail(`ready.mjs missing (${e.message.split("\n")[0]})`);
    throw e;
  }
  if (typeof ready.configCheck !== "function") {
    fail("ready.mjs does not export configCheck");
    throw new Error("no configCheck");
  }
  if (announce.SERVER_NAME !== NAME) fail(`announce SERVER_NAME is "${announce.SERVER_NAME}", this selftest expects "${NAME}"`);
  const check = (body) => ready.configCheck({ hostDir: hostWith(body), root: ROOT });

  // 1. both right: green, and the password is not in the detail
  let r = await check(cfg(NAME, SECRET));
  if (!r.ok) fail(`name and password right must be ok: ${text(r)}`);
  if (text(r).includes(SECRET)) fail("green detail printed the password");

  // 2. empty, blank and missing password: NO-GO, says the password is the problem
  for (const [label, body] of [["empty", cfg(NAME, "")], ["blank", cfg(NAME, "   ")], ["missing", { game: { name: NAME } }], ["non-string", cfg(NAME, 12345)]]) {
    r = await check(body);
    if (r.ok) fail(`${label} password must be NO-GO`);
    else if (!/password/i.test(r.detail)) fail(`${label} password: detail does not name the password: ${r.detail}`);
  }

  // 3. wrong name: NO-GO naming the expected name; the password still never printed
  for (const [label, name] of [["other name", "Some Other Server"], ["case change", NAME.toLowerCase()], ["trailing space", `${NAME} `], ["absent", undefined]]) {
    r = await check(cfg(name, SECRET));
    if (r.ok) fail(`${label} must be NO-GO`);
    else {
      if (!r.detail.includes(NAME)) fail(`${label}: detail does not name the expected name: ${r.detail}`);
      if (r.detail.includes(SECRET)) fail(`${label}: detail printed the password`);
    }
  }

  // 4. both wrong: one detail that names both problems
  r = await check(cfg("Wrong", ""));
  if (r.ok || !r.detail.includes(NAME) || !/password/i.test(r.detail)) fail(`both wrong must name both: ${r.detail}`);

  // 5. no file, unreadable JSON, wrong shape: NO-GO with a reason, never a throw, no password echo
  for (const [label, body] of [["missing file", undefined], ["bad json", `{ "game": { "password": "${SECRET}", `], ["not an object", "[]"], ["null", "null"]]) {
    try {
      r = await check(body);
    } catch (e) {
      fail(`${label} threw: ${e.message}`);
      continue;
    }
    if (r.ok) fail(`${label} must be NO-GO`);
    if (!r.detail) fail(`${label}: no reason given`);
    if (r.detail.includes(SECRET)) fail(`${label}: detail printed the password`);
  }

  // 6. it runs inside `host ready`: the default list carries a "config" check, and through runChecks
  //    the printed line for a wrong config is NO-GO with the password out of every line
  const names = ready.defaultChecks().map((c) => c.name);
  if (!names.includes("config")) fail(`defaultChecks has no "config" check: ${names.join(", ")}`);
  const cmd = await import(pathToFileURL(join(ROOT, "tools", "host", "commands", "ready.mjs")).href);
  const lines = [];
  const real = console.log;
  console.log = (...a) => lines.push(a.join(" "));
  let code;
  try {
    const config = ready.defaultChecks().find((c) => c.name === "config");
    code = await cmd.main([], { root: ROOT, hostDir: hostWith(cfg("Wrong", SECRET)), checks: config ? [config] : [] });
  } finally {
    console.log = real;
  }
  const out = lines.join("\n");
  if (code !== 1 || !/NO-GO config/.test(out)) fail(`a wrong config through host ready must exit 1 with NO-GO config:\n${out}`);
  if (out.includes(SECRET)) fail("host ready output printed the password");
} catch (e) {
  if (!failed) fail(`selftest crashed: ${e.stack}`);
} finally {
  rmSync(tmp, { recursive: true, force: true });
}

if (failed) {
  console.log(`host-ready-config-selftest: ${failed} failure(s)`);
  process.exit(1);
}
console.log("host-ready-config-selftest ok");
