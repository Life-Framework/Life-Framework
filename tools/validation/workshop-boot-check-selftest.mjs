// Proves the clean-server boot check (#371) without a box: the server.json it writes, the launch flags
// (-config, no -addons, an empty -addonsDir), the log judge on the real lines lf-cloud printed on
// 2026-11-14 (an id that is not on the Workshop), and the remote script's markers.
// inputs: tools/release/boot-check.mjs, tools/commands/workshop.mjs, server/example/server.json
// red-proof: with the mod-problem branch of judgeBootLog switched off, the run printed
// "FAIL the Workshop 'not found' log stops at mods: got ready".
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { buildServerJson, clientArgs, isHexId, judgeBootLog, parseRemoteOutput, remoteScript, serverArgs } from "../release/boot-check.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(msg); };

const example = JSON.parse(readFileSync(join(root, "server", "example", "server.json"), "utf8"));
const j = buildServerJson(example, "AAAAAAAAAAAAAAAA", "BBBBBBBBBBBBBBBB");
expect(j.game.mods.map((m) => m.modId).join() === "AAAAAAAAAAAAAAAA,BBBBBBBBBBBBBBBB", "server.json lists the two ids, core first");
expect(j.game.scenarioId === example.game.scenarioId, "the scenario is the example's");
expect(j.bindPort !== 2001 && j.bindPort >= 2300, "the boot port is outside the pool lanes");
expect(example.game.mods.length === 2 && example.bindPort === 2001, "the example itself is unchanged by the build");

const sa = serverArgs("R");
expect(sa.includes("-config") && !sa.includes("-addons") && sa[sa.indexOf("-addonsDir") + 1] === "R/addons", "server flags: -config, no -addons, empty addonsDir");
const ca = clientArgs("R");
expect(!ca.includes("-addons") && !ca.includes("-noBackend") && ca.includes("-client"), "client flags: no -addons, no -noBackend");
expect(isHexId("4550A2B656E1B569") && !isHexId("PLACEHOLDER-PACK-ID-AFTER-370") && !isHexId("4550A2B656E1B56"), "isHexId");

// Real lines from lf-cloud, 2026-11-14, pack id not on the Workshop.
const notFound = [
  "11:54:38.597  DEFAULT      : Config load @\"{904C0051711396ED}Missions/RallyAirport.conf\"",
  "11:54:38.641   RESOURCES (E): Wrong GUID/name for resource @\"{75033D4FB2FE793E}worlds/Rally_Everon_NorthernForest.ent\" in property \"World\"",
  "11:54:41.882 BACKEND   (E): Addon B1E84D5687E11724 - Addon was not found on workshop.",
  "11:54:41.884 BACKEND   (E): 1 addons are not downloadable! Cannot start until they are removed from server config.",
  "11:54:41.982 ENGINE    (E): Unable to initialize the game",
].join("\n");
let r = judgeBootLog(notFound, { wantIds: ["4550A2B656E1B569", "B1E84D5687E11724"] });
expect(r.stage === "mods" && !r.ok && /not found on workshop/.test(r.line), `the Workshop 'not found' log stops at mods: got ${r.stage}`);

const good = "10:00:00.000 SCRIPT       : [LFDebug:Persistence] resolved\n10:00:09.000 NETWORK      : Entered online game state.\n";
r = judgeBootLog(good, { client: false });
expect(r.ok && r.stage === "pass", "a ready log with no client passes");
r = judgeBootLog(good, { client: true });
expect(!r.ok && r.stage === "join", `a ready log with no player stops at join: got ${r.stage}`);
r = judgeBootLog(good + "10:01:00.000 SCRIPT       : [LFDebug:CharacterCreation] flow start for player 1\n", { client: true });
expect(r.ok, "a ready log with a joined player passes");
r = judgeBootLog(good + "10:00:10.000 SCRIPT    (E): Unknown type 'LF_Foo'\n", { client: false });
expect(r.stage === "scripts", "a SCRIPT (E) line stops at scripts");
r = judgeBootLog("10:00:00.000 ENGINE       : Initializing engine\n", { client: false });
expect(r.stage === "ready", "a log that never gets ready stops at ready");

const script = remoteScript({ run: "C:/LF/bootcheck/run-x", serverJsonB64: "e30=", budgetS: 60, client: true });
expect(/BOOTCHECK-RUN/.test(script) && /=====LOG-BEGIN=====/.test(script) && /-WorkingDirectory \$srv/.test(script), "the remote script prints its markers and starts in the server folder");
expect(!/s-addonss/.test(script), "the remote script never passes -addons");
const out = parseRemoteOutput("BOOTCHECK-RUN C:/x seconds=9 exited=True\n=====LOG-BEGIN=====\nhello\n=====LOG-END=====\n=====STDERR-BEGIN=====\ne\n=====STDERR-END=====\n");
expect(out.run === "C:/x" && out.log.trim() === "hello", "parseRemoteOutput reads the log between the markers");
expect(parseRemoteOutput("BOOTCHECK-DIRTY LifeFramework").dirty === "LifeFramework", "parseRemoteOutput reads the dirty-install refusal");

for (const f of fails) console.log(`  FAIL  ${f}`);
if (fails.length) { console.log("workshop-boot-check-selftest: FAIL"); process.exit(1); }
console.log("workshop-boot-check-selftest: OK (server.json, launch flags, log judge, remote script)");
