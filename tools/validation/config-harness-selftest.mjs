// Proves `tools\cli config-harness` (#298): every shipped fixture matches its expected log line, a
// preset yields nothing, each reject class has its own broken fixture, and the harness goes red when
// an expectation is wrong or a fixture has none. Fixtures: tools/test/config-fixtures/.
//
// red-proof: with the expected message in broken/range-below-min.expect.json changed to "minimum 1" the
// run printed "FAIL shipped fixtures: validate: unexpected  error 1:54 ... below the minimum 0" and
// exited 1; with the `missing` loop of the validate stage removed the "wrong expectation" check printed
// "FAIL a wrong expectation must fail the harness"; restored, it prints "config-harness-selftest: OK".
import { cpSync, mkdtempSync, readFileSync, writeFileSync, readdirSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { FIXTURES, STAGES, loadFixtures, main, runHarness } from "../config/harness.mjs";

const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(msg); };
const quiet = console.log;
const silently = (fn) => { console.log = () => {}; try { return fn(); } finally { console.log = quiet; } };

const fixtures = loadFixtures();
const bad = runHarness(fixtures, STAGES);
expect(bad.length === 0, `shipped fixtures: ${bad.map((b) => `${b.fixture} ${b.fails.join("; ")}`).join(" | ")}`);
expect(silently(() => main(["--stage", "validate"])) === 0, "main --stage validate should exit 0");
expect(silently(() => main(["--stage", "nope"])) === 2, "an unknown stage exits 2");

// Shape: presets are clean, broken fixtures each have a problem, and every one has a sidecar.
for (const fx of fixtures) {
  expect(fx.expect, `${fx.name}: no .expect.json`);
  if (!fx.expect) continue;
  if (fx.kind === "preset") expect(fx.expect.problems.length === 0 && fx.expect.exit === 0, `${fx.name}: a preset must expect nothing`);
  else expect(fx.expect.problems.length > 0, `${fx.name}: a broken fixture must expect a log line`);
}
expect(fixtures.filter((f) => f.kind === "broken").length >= 12, "fewer than 12 reject classes covered");

// Red path, on a copy: a wrong message, a wrong exit code and a missing sidecar each fail.
const tmp = mkdtempSync(join(tmpdir(), "lf-config-harness-"));
cpSync(FIXTURES, tmp, { recursive: true });
const sc = join(tmp, "broken", "range-below-min.expect.json");
const e = JSON.parse(readFileSync(sc, "utf8"));
e.problems[0].msg = e.problems[0].msg.replace("minimum 0", "minimum 1");
writeFileSync(sc, JSON.stringify(e));
expect(runHarness(loadFixtures(tmp), STAGES).some((b) => b.fixture === "broken/range-below-min" && b.stage === "validate"), "a wrong expectation must fail the harness");
e.problems[0].msg = e.problems[0].msg.replace("minimum 1", "minimum 0");
e.exit = 0;
writeFileSync(sc, JSON.stringify(e));
expect(runHarness(loadFixtures(tmp), STAGES).some((b) => b.fails.some((f) => f.includes("exit"))), "a wrong exit code must fail the harness");
const sx = join(tmp, "broken", "syntax.expect.json");
const s = JSON.parse(readFileSync(sx, "utf8"));
s.parse = "ok";
writeFileSync(sx, JSON.stringify(s));
expect(runHarness(loadFixtures(tmp), STAGES).some((b) => b.stage === "parse"), "a wrong parse verdict must fail the parse stage");
expect(readdirSync(tmp).length > 0, "temp copy");
writeFileSync(join(tmp, "broken", "extra.json"), "{}");
expect(runHarness(loadFixtures(tmp), STAGES).some((b) => b.stage === "load"), "a fixture with no sidecar must fail");

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("config-harness-selftest: OK (every fixture matches its log line; wrong message, exit, parse verdict and missing sidecar fail)");
