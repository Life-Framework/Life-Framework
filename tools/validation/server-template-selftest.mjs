// Proves the example server template check (#295): the shipped folder is clean, and each way a host
// folder goes wrong is named. red-proof: with the a2s.address fixture left "" the engine-refused file passed
// the check; with the check in place it prints "FAIL ... a2s.address" below.
import { cpSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { templateLaunch, templateProblems } from "../server-template/template.mjs";
import { engineVerdict } from "../commands/server-template.mjs";

const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(msg); };
const real = templateProblems();
expect(real.length === 0, `the shipped template has problems: ${real.join(" | ")}`);
const l = templateLaunch();
expect(l.addons.join() === "LifeFramework,LF_Everon1989", `launch addons ${l.addons}`);
expect(/LF_Everon1989\.conf$/.test(l.scenario), `launch scenario ${l.scenario}`);

const copy = () => {
  const root = mkdtempSync(join(tmpdir(), "lf-tpl-"));
  cpSync("server/example", join(root, "server", "example"), { recursive: true });
  cpSync("tools/test/config-fixtures/presets", join(root, "tools", "test", "config-fixtures", "presets"), { recursive: true });
  return root;
};
const mutate = (file, fn, want) => {
  const root = copy();
  const p = join(root, "server", "example", file);
  writeFileSync(p, fn(readFileSync(p, "utf8")));
  const got = templateProblems(root).join(" | ");
  expect(want.test(got), `${file}: expected ${want}, got "${got}"`);
};
mutate("server.json", (t) => t.replace('"address": "0.0.0.0"', '"address": ""'), /a2s\.address/);
mutate("server.json", (t) => t.replace(/,\s*\{\s*"modId": "B1E84D5687E11724",\s*"name": "[^"]*"\s*\}/, ""), /game\.mods/);
mutate("server.json", (t) => t.replace('"passwordAdmin": "CHANGE-ME"', '"passwordAdmin": "two words"'), /passwordAdmin/);
mutate("config.json", (t) => t.replace('"defaultBuyPrice": 50', '"defaultBuyPrice": "cheap"'), /differs from the flagship|config\.json 1?\d*:\d+/);
mutate("start.sh", (t) => t.replace("-config", "-conf"), /start\.sh does not start/);
mutate("README.md", (t) => t.replace(/LF_REFORGER_SERVER/g, "X"), /README\.md does not mention/);
expect(engineVerdict("x BACKEND      : JSON is Valid").ok, "engineVerdict: valid");
expect(!engineVerdict("14 BACKEND   (E): Value of \"#/a2s/address\" does not match").ok, "engineVerdict: invalid");

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("server-template-selftest: OK");
