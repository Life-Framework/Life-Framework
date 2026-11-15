// The flagship never runs with the job-gate testing bypass on (test-job-bypass).
// inputs: server/flagship/config.json, server/example/config.json, addons/LF_Everon1989/Missions/LF_Everon1989.conf, tools/validation/fixtures/flagship-job-gates/**
//
// The bypass (LF_JobGates) opens every whitelist-gated job to every player. It is a tester's switch: the owner
// config key `jobs.testUnlockAll` and the mission header flags `m_bLF_TestUnlockAllJobs` /
// `m_bLF_TestUnlockOnDedicated`. A live server must have none of them on, so this check fails when
//  - server/flagship/config.json (or the server/example/config.json that owners copy) sets jobs.testUnlockAll to true, or
//  - the flagship mission header addons/LF_Everon1989/Missions/LF_Everon1989.conf sets either header flag to 1.
// Fixtures: fixtures/flagship-job-gates/bad-config.json and bad-header.conf must fail, every run; good-* pass.
// red-proof: `node tools/validation/validate-flagship-job-gates.mjs --config tools/validation/fixtures/flagship-job-gates/bad-config.json`
// exits 1 with "FAIL ... jobs.testUnlockAll"; --header with bad-header.conf exits 1 with "FAIL ... m_bLF_TestUnlockAllJobs".

import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIXTURES = join(ROOT, "tools", "validation", "fixtures", "flagship-job-gates");
const HEADER_FLAGS = ["m_bLF_TestUnlockAllJobs", "m_bLF_TestUnlockOnDedicated"];

// Problems in an owner config.json text: jobs.testUnlockAll anywhere truthy.
export function configProblems(text, label) {
  let json;
  try { json = JSON.parse(text); } catch (e) { return [`${label} is not JSON: ${e.message}`]; }
  const v = json?.jobs?.testUnlockAll;
  if (v === undefined || v === false) return [];
  return [`${label} sets jobs.testUnlockAll (${JSON.stringify(v)}); the job-gate bypass is for test hosts only`];
}

// Problems in a mission header .conf text: a bypass flag set to anything but 0.
export function headerProblems(text, label) {
  const out = [];
  text.split(/\r?\n/).forEach((line, i) => {
    const m = line.match(/^\s*(\w+)\s+(\S+)/);
    if (m && HEADER_FLAGS.includes(m[1]) && m[2] !== "0") out.push(`${label}:${i + 1} sets ${m[1]} ${m[2]}; the flagship header must not open the job gates`);
  });
  return out;
}

const arg = (name) => { const i = process.argv.indexOf(name); return i > -1 ? process.argv[i + 1] : null; };
const fail = (errors) => { for (const e of errors) console.error(`FAIL ${e}`); console.error(`validate-flagship-job-gates: ${errors.length} problem(s)`); process.exit(1); };

const configArg = arg("--config");
const headerArg = arg("--header");
if (configArg || headerArg) {
  const errors = [];
  if (configArg) errors.push(...configProblems(readFileSync(configArg, "utf8"), configArg));
  if (headerArg) errors.push(...headerProblems(readFileSync(headerArg, "utf8"), headerArg));
  if (errors.length) fail(errors);
  console.log(`validate-flagship-job-gates: OK ${configArg ?? headerArg}`);
  process.exit(0);
}

const errors = [];

// The fixtures keep the check honest: the bad ones must fail, the good ones must pass.
const fixture = (file, fn) => fn(readFileSync(join(FIXTURES, file), "utf8"), file);
if (fixture("bad-config.json", configProblems).length === 0) errors.push("fixture bad-config.json no longer fails the check");
if (fixture("bad-header.conf", headerProblems).length === 0) errors.push("fixture bad-header.conf no longer fails the check");
for (const f of fixture("good-config.json", configProblems)) errors.push(f);
for (const f of fixture("good-header.conf", headerProblems)) errors.push(f);

const flagshipConfig = join(ROOT, "server", "flagship", "config.json");
if (!existsSync(flagshipConfig)) errors.push("server/flagship/config.json is missing");
else errors.push(...configProblems(readFileSync(flagshipConfig, "utf8"), "server/flagship/config.json"));

const exampleConfig = join(ROOT, "server", "example", "config.json");
if (existsSync(exampleConfig)) errors.push(...configProblems(readFileSync(exampleConfig, "utf8"), "server/example/config.json"));

const header = join(ROOT, "addons", "LF_Everon1989", "Missions", "LF_Everon1989.conf");
if (!existsSync(header)) errors.push("addons/LF_Everon1989/Missions/LF_Everon1989.conf is missing");
else errors.push(...headerProblems(readFileSync(header, "utf8"), "addons/LF_Everon1989/Missions/LF_Everon1989.conf"));

if (errors.length) fail(errors);
console.log("validate-flagship-job-gates: OK (flagship config and header leave the job gates closed)");
