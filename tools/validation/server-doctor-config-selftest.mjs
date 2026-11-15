// Proves tools/lib/server-doctor-config.mjs (#1272, O13): checkEffectiveConfig(profileDir) reads the
// profile's config.json against docs/config/config.schema.json (through tools/config/check.mjs) and the
// [LFDebug:Config] lines of the newest console.log, and lists the bad fields and the fields that fell back
// to the shipped default. Fixtures: tools/validation/fixtures/server-doctor-config/.
// inputs: tools/**, docs/config/**
//
// red-proof: before tools/lib/server-doctor-config.mjs existed the import failed with
// "Cannot find module '../lib/server-doctor-config.mjs'" and the run exited 1.
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const fx = (n) => join(here, "fixtures", "server-doctor-config", n);
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(msg); };

const { checkEffectiveConfig } = await import("../lib/server-doctor-config.mjs");

// Acceptance: one out-of-range field and one unknown field give two findings and a failing result.
const bad = checkEffectiveConfig(fx("bad"));
expect(bad.ok === false, `bad: expected ok=false, got ${bad.ok}`);
expect(bad.findings.length === 2, `bad: expected 2 findings, got ${JSON.stringify(bad.findings)}`);
const range = bad.findings.find((f) => f.field === "shops.defaultSellPrice");
const unknown = bad.findings.find((f) => f.field === "shops.bogus");
expect(range && range.kind === "bad" && range.fellBack === true && range.line === 4, `bad: range finding ${JSON.stringify(range)}`);
expect(unknown && unknown.kind === "unknown" && unknown.fellBack === false, `bad: unknown finding ${JSON.stringify(unknown)}`);
expect(JSON.stringify(bad.badFields) === JSON.stringify(["shops.defaultSellPrice"]), `bad: badFields ${JSON.stringify(bad.badFields)}`);
expect(JSON.stringify(bad.fellBack) === JSON.stringify(["shops.defaultSellPrice"]), `bad: fellBack ${JSON.stringify(bad.fellBack)}`);
expect(bad.log === null, `bad: no console.log expected, got ${JSON.stringify(bad.log)}`);

// A clean file passes with no findings.
const good = checkEffectiveConfig(fx("good"));
expect(good.ok === true && good.findings.length === 0, `good: ${JSON.stringify(good)}`);

// No config.json at all: the server runs on the shipped defaults; that is reported, not an exception.
const empty = checkEffectiveConfig(fx("empty"));
expect(empty.ok === true && empty.findings.length === 0 && empty.present === false, `empty: ${JSON.stringify(empty)}`);

// With a console.log: the two config findings are not repeated; a log-only line adds one finding.
const logged = checkEffectiveConfig(fx("logged"));
expect(logged.ok === false && logged.findings.length === 3, `logged: expected 3 findings, got ${JSON.stringify(logged.findings)}`);
const lo = logged.findings.find((f) => f.source === "log");
expect(lo && lo.field === "vehicles.fleet" && lo.kind === "bad" && lo.level === "error", `logged: log-only finding ${JSON.stringify(lo)}`);
expect(logged.log && logged.log.lines === 4 && logged.log.loaded === true, `logged: log summary ${JSON.stringify(logged.log)}`);

for (const f of fails) console.log(`FAIL  ${f}`);
console.log(fails.length ? `server-doctor-config-selftest: ${fails.length} failing` : "server-doctor-config-selftest: OK");
process.exit(fails.length ? 1 : 0);
