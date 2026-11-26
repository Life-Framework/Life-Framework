// Proves the four harness frictions from the 2026-11-25 and 2026-11-26 round reports are gone:
//   1. `tools\cli checkpoint <hour> [--client n]` builds `harness.mjs [--client n] send checkpoint <hour>`, refuses a
//      missing hour file and a bad client with the list of hours;
//   2. `harness.mjs <verb> ...` for a word that is no harness command prints one line pointing at `send` (and at
//      `tools\cli checkpoint` for checkpoint), not the usage whose last line is "Play mode has to be running";
//   3. `harness.mjs send time 23 [weather]` builds the `time` line from its words (it threw "time hour must be 0-24:
//      undefined"), and a missing hour still throws;
//   4. `session pull` on a local session copies each client's newest console.log and el-captures and the server's
//      newest console.log into the destination (it printed "no remote session recorded" and copied nothing).
// inputs: tools/commands/checkpoint.mjs, agent-box/harness.mjs, agent-box/session.mjs
// red-proof: 2026-11-27, on origin/main's harness.mjs `buildLine(1, { verb: "time", words: ["23"] })` threw "time hour
// must be 0-24: undefined", unknownCommandHint and pullLocalEvidence did not exist (import failed), and
// tools/commands/checkpoint.mjs was missing.
import { existsSync, mkdirSync, readFileSync, utimesSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { scratchDir } from "../lib/scratch.mjs";
import { harnessArgs } from "../commands/checkpoint.mjs";
import { buildLine, unknownCommandHint } from "../../agent-box/harness.mjs";
import { pullLocalEvidence } from "../../agent-box/session.mjs";

const failures = [];
const check = (ok, msg) => { if (!ok) failures.push(msg); };

// 1. checkpoint alias
const known = [0, 5, 10];
check(JSON.stringify(harnessArgs(["5"], known).argv) === JSON.stringify(["send", "checkpoint", "5"]), "checkpoint 5 -> send checkpoint 5");
check(JSON.stringify(harnessArgs(["5", "--client", "2"], known).argv) === JSON.stringify(["--client", "2", "send", "checkpoint", "5"]), "checkpoint 5 --client 2 keeps the client");
check(/hours: 0, 5, 10/.test(harnessArgs([], known).error ?? ""), "checkpoint with no hour prints the usage and the hours");
check(/hour-7\.json/.test(harnessArgs(["7"], known).error ?? ""), "a missing hour file is refused by name");
check(/--client takes a number/.test(harnessArgs(["5", "--client", "x"], known).error ?? ""), "a bad --client is refused");

// 2. unknown harness command
const hint = unknownCommandHint("checkpoint", ["5"]);
check(/send checkpoint 5/.test(hint) && /tools\\cli checkpoint 5/.test(hint) && !/Play mode/.test(hint), `checkpoint hint points at send and the CLI alias: ${hint}`);
check(/send repgrant UNDERWORLD 0/.test(unknownCommandHint("repgrant", ["UNDERWORLD", "0"])), "any feature verb gets the send form with its words");

// 3. send time <hour> [weather]
check(buildLine(1, { verb: "time", words: ["23"] }) === "1 time 23", "send time 23 builds '1 time 23'");
check(buildLine(1, { verb: "time", words: ["6", "rainy"] }) === "1 time 6 rainy", "send time 6 rainy keeps the weather");
check(buildLine(1, { verb: "time", hour: 12 }) === "1 time 12", "the scripted { verb: time, hour } form still works");
let threw = false;
try { buildLine(1, { verb: "time", words: [] }); } catch { threw = true; }
check(threw, "send time with no hour still throws");

// 4. local session pull
const base = scratchDir("lf-pull-");
const sdir = join(base, "session");
const mk = (rel, text, ageMin = 0) => {
  const p = join(sdir, rel);
  mkdirSync(join(p, ".."), { recursive: true });
  writeFileSync(p, text);
  if (ageMin) {
    const t = new Date(Date.now() - ageMin * 60000);
    utimesSync(join(p, ".."), t, t);
  }
};
mk("client1/logs/old/console.log", "old run", 60);
mk("client1/logs/new/console.log", "client1 new");
mk("client1/logs/new/el-captures/shot_001.png", "png");
mk("client2/logs/only/console.log", "client2");
mk("server/logs/run/console.log", "server log");
const t = new Date(Date.now() - 3600000);
utimesSync(join(sdir, "client1", "logs", "old"), t, t);
const dest = join(base, "out");
const got = pullLocalEvidence(sdir, dest);
check(readFileSync(join(dest, "client1", "console.log"), "utf8") === "client1 new", "the newest client1 log folder is the one copied");
check(existsSync(join(dest, "client1", "el-captures", "shot_001.png")), "client1's el-captures are copied");
check(readFileSync(join(dest, "client2", "console.log"), "utf8") === "client2", "client2's log is copied");
check(readFileSync(join(dest, "server", "console.log"), "utf8") === "server log", "the server console.log is copied");
check(got.length === 4, `four items reported (got ${got.length})`);
check(pullLocalEvidence(join(base, "none"), join(base, "out2")).length === 0, "a missing session dir copies nothing and does not throw");

if (failures.length) {
  console.log("harness-cli-hints-selftest: FAIL");
  for (const f of failures) console.log(`  - ${f}`);
  process.exit(1);
}
console.log("harness-cli-hints-selftest: OK (checkpoint alias, send hint, send time words, local session pull)");
