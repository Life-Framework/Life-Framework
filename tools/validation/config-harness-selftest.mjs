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
import { FIXTURES, STAGES, isBoot, judgeBootLog, judgeRead, loadFixtures, main, runHarness, setBootDriver, setClientDriver } from "../config/harness.mjs";

const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(msg); };
const quiet = console.log;
const silently = (fn) => { console.log = () => {}; try { return fn(); } finally { console.log = quiet; } };

const fixtures = loadFixtures();
const OFFLINE = STAGES.filter((s) => !s.needs);
const bad = runHarness(fixtures, OFFLINE);
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
expect(runHarness(loadFixtures(tmp), OFFLINE).some((b) => b.fixture === "broken/range-below-min" && b.stage === "validate"), "a wrong expectation must fail the harness");
e.problems[0].msg = e.problems[0].msg.replace("minimum 1", "minimum 0");
e.exit = 0;
writeFileSync(sc, JSON.stringify(e));
expect(runHarness(loadFixtures(tmp), OFFLINE).some((b) => b.fails.some((f) => f.includes("exit"))), "a wrong exit code must fail the harness");
const sx = join(tmp, "broken", "syntax.expect.json");
const s = JSON.parse(readFileSync(sx, "utf8"));
s.parse = "ok";
writeFileSync(sx, JSON.stringify(s));
expect(runHarness(loadFixtures(tmp), OFFLINE).some((b) => b.stage === "parse"), "a wrong parse verdict must fail the parse stage");
expect(readdirSync(tmp).length > 0, "temp copy");
writeFileSync(join(tmp, "broken", "extra.json"), "{}");
expect(runHarness(loadFixtures(tmp), OFFLINE).some((b) => b.stage === "load"), "a fixture with no sidecar must fail");

// Client stage (#300), with a fake client: it must show the value, a wrong value fails, and no client fails.
const clientFx = fixtures.filter((f) => f.expect?.client?.length);
expect(clientFx.length >= 3, "fewer than 3 client fixtures (price, term, system switch)");
const CLIENT = STAGES.find((s) => s.name === "client");
expect(runHarness(clientFx, [CLIENT]).length === clientFx.length, "with no client attached every client fixture must fail");
setClientDriver({ apply() {}, read: (fx, r) => (r.kind === "actionShown" ? r.expect : r.expect + " ") });
expect(runHarness(clientFx, [CLIENT]).length === 0, "a client that shows every expected value must pass");
setClientDriver({ apply() {}, read: (fx, r) => (r.kind === "actionShown" ? !r.expect : "$999") });
expect(runHarness(clientFx, [CLIENT]).length === clientFx.length, "a client that shows the wrong value must fail every fixture");
setClientDriver(null);
expect(judgeRead({ kind: "shopPrice", shop: "S", item: "Water", expect: "$17" }, "Water $17 Buy") === "", "a price on the list passes");
expect(judgeRead({ kind: "shopPrice", shop: "S", item: "Water", expect: "$17" }, "Water $6 Buy") !== "", "another price fails");
expect(judgeRead({ kind: "actionShown", entity: "E", class: "C", expect: false }, true) !== "", "an offered action fails a hidden expectation");
expect(judgeRead({ kind: "nope" }, "") !== "", "an unknown read kind fails");

// Boot stage (#299): a fixture that says "boot": true is judged against the server log of its own boot.
const bootFx = fixtures.filter(isBoot);
expect(bootFx.length >= 2, "fewer than 2 boot fixtures (a preset a reload cannot do, a broken one)");
expect(bootFx.some((f) => f.kind === "broken"), "no broken boot fixture (it boots, logs the field, keeps the default)");
expect(bootFx.every((f) => !f.expect.client), "a boot fixture also lists client reads: those run on the shared server");
const BOOT = STAGES.find((s) => s.name === "boot");
expect(runHarness(bootFx, [BOOT]).length === bootFx.length, "with no server driver every boot fixture must fail");
setBootDriver({ log: (fx) => [...fx.expect.bootLog.has, "unrelated line"].join(" | ") });
expect(runHarness(bootFx, [BOOT]).length === 0, "a server log with every expected line must pass");
setBootDriver({ log: () => "[LFDebug:Persistence] autosave scheduled every 600 seconds" });
expect(runHarness(bootFx, [BOOT]).length > 0, "a log missing the fixture's line must fail");
setBootDriver(null);
expect(judgeBootLog({ has: ["a"], absent: ["b"] }, "a b").length === 1, "an absent line that shows up fails");
expect(judgeBootLog({}, "a").length === 1, "a sidecar with no bootLog fails");
expect(runHarness(fixtures.filter((f) => !isBoot(f)), [BOOT]).length === 0, "a reloadable fixture is not booted");
const only = clientFx.every((f) => !isBoot(f));
expect(only, "a client fixture must not be a boot fixture");

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("config-harness-selftest: OK (every fixture matches its log line; wrong message, exit, parse verdict and missing sidecar fail)");
