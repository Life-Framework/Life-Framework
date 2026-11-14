// Proves the example server template check (#295): the shipped folder is clean, and each way a host
// folder goes wrong is named. red-proof: with the a2s.address fixture left "" the engine-refused file passed
// the check; with the check in place it prints "FAIL ... a2s.address" below.
import { cpSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { templateLaunch, templateProblems } from "../server-template/template.mjs";
import { scaffoldPack } from "../pack/new.mjs";
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
mutate("server.json", (t) => t.replace(/,\s*\{\s*"modId": "B1E84D5687E11724",\s*"name": "[^"]*"\s*\}/, ""), /none of the listed addons/);
mutate("server.json", (t) => t.replace('"passwordAdmin": "CHANGE-ME"', '"passwordAdmin": "two words"'), /passwordAdmin/);
mutate("config.json", (t) => t.replace('"defaultBuyPrice": 50', '"defaultBuyPrice": "cheap"'), /differs from the flagship|config\.json 1?\d*:\d+/);
mutate("start.sh", (t) => t.replace("-config", "-conf"), /start\.sh does not start/);
mutate("README.md", (t) => t.replace(/LF_REFORGER_SERVER/g, "X"), /README\.md does not mention/);
// #297: the swap. The shipped core-only variant is clean and differs only in mods and scenarioId.
const co = templateLaunch(undefined, "core-only");
expect(co.addons.join() === "LifeFramework" && /EL_DebugTest\.conf$/.test(co.scenario), `core-only launch ${co.addons} ${co.scenario}`);
const variantMutate = (fn, want) => {
  const root = copy();
  cpSync("tools/server-template/variants", join(root, "tools", "server-template", "variants"), { recursive: true });
  for (const a of ["LifeFramework", "LF_Everon1989"]) { mkdirSync(join(root, "addons", a), { recursive: true }); cpSync(join("addons", a, `${a}.gproj`), join(root, "addons", a, `${a}.gproj`)); }
  cpSync("addons/LifeFramework/Missions/EL_DebugTest.conf.meta", join(root, "addons", "LifeFramework", "Missions", "EL_DebugTest.conf.meta"), { recursive: true });
  cpSync("addons/LF_Everon1989/Missions", join(root, "addons", "LF_Everon1989", "Missions"), { recursive: true });
  const p = join(root, "tools", "server-template", "variants", "core-only", "server.json");
  writeFileSync(p, fn(readFileSync(p, "utf8")));
  const got = templateProblems(root).join(" | ");
  if (want) expect(want.test(got), `variant: expected ${want}, got "${got}"`);
  else expect(got === "", `variant should be clean, got "${got}"`);
  return root;
};
variantMutate((t) => t, null);
variantMutate((t) => t.replace('"maxPlayers": 16', '"maxPlayers": 32'), /more than game\.mods and game\.scenarioId/);
variantMutate((t) => t.replace('{5A1E5A1E5A1E0002}', '{0000000000000000}'), /does not match/);
variantMutate((t) => t.replace('"modId": "79636E668EA37AC9"', '"modId": "B1E84D5687E11724"'), /must start with the core/);
// A `pack new` scaffold swaps in by those two entries alone.
{
  const root = copy();
  mkdirSync(join(root, "addons", "LifeFramework", "Scripts"), { recursive: true });
  cpSync("addons/LifeFramework/LifeFramework.gproj", join(root, "addons", "LifeFramework", "LifeFramework.gproj"));
  cpSync("addons/LifeFramework/Scripts/Game/Feature/Packs/LF_PackManifest.c", join(root, "addons", "LifeFramework", "Scripts", "LF_PackManifest.c"));
  const r = scaffoldPack(join(root, "addons"), "Swapped");
  expect(!r.error, `scaffoldPack: ${r.error}`);
  if (!r.error) {
    const gproj = readFileSync(join(r.dir, `${r.id}.gproj`), "utf8");
    const packGuid = /GUID\s+"([0-9A-F]{16})"/.exec(gproj)[1];
    const mission = readFileSync(join(r.dir, "Missions", `${r.id}.conf.meta`), "utf8").match(/\{([0-9A-F]{16})\}/)[1];
    const sjPath = join(root, "server", "example", "server.json");
    const sj = JSON.parse(readFileSync(sjPath, "utf8"));
    sj.game.mods[1] = { modId: packGuid, name: r.id };
    sj.game.scenarioId = `{${mission}}Missions/${r.id}.conf`;
    writeFileSync(sjPath, JSON.stringify(sj, null, 2) + "\n");
    expect(templateProblems(root).join(" | ") === "", `a pack new scaffold swapped in: ${templateProblems(root).join(" | ")}`);
    expect(templateLaunch(root).addons.join() === `LifeFramework,${r.id}`, "the scaffold's addon list");
    sj.game.scenarioId = `{${mission}}Missions/Nope.conf`;
    writeFileSync(sjPath, JSON.stringify(sj, null, 2) + "\n");
    expect(/is in none of the listed addons/.test(templateProblems(root).join(" | ")), "a scenario in no listed addon passed");
  }
}
expect(engineVerdict("x BACKEND      : JSON is Valid").ok, "engineVerdict: valid");
expect(!engineVerdict("14 BACKEND   (E): Value of \"#/a2s/address\" does not match").ok, "engineVerdict: invalid");

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("server-template-selftest: OK");
