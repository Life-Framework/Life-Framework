import { readFileSync } from "node:fs";
import { join } from "node:path";
import { newestLog, sessionDir, sessionPaths } from "../session.mjs";
import { loadFixtures, judgeBootLog } from "../../tools/config/harness.mjs";

// #299: the config harness's boot stage, the half that runs where the server runs. `tools\cli config-harness
// --stage boot` boots a server-only session seeded with a fixture's config.json; this reads that server's
// newest console.log and judges the sidecar's `bootLog` (has / absent) against it. The server may still be
// printing its start lines, so it re-reads for up to 40 s while a `has` line is missing.
//   config-boot-proof --fixture <name> [--dir <fixtures>]   (up --clients 0 --seed config.json)
async function runConfigBootProof(args) {
  const val = (f) => (args.includes(f) ? args[args.indexOf(f) + 1] : undefined);
  const name = val("--fixture");
  const fx = loadFixtures(val("--dir")).find((f) => f.name.endsWith(`/${name}`) && f.expect?.boot === true);
  if (!fx) { console.log(`FAIL  config boot proof: no boot fixture named ${name}`); return 1; }
  const logs = join(sessionPaths(sessionDir()).server, "logs");
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  let fails = [];
  const until = Date.now() + 40000;
  for (;;) {
    const file = newestLog(logs);
    fails = file ? judgeBootLog(fx.expect.bootLog, readFileSync(file, "utf8")) : [`boot: no server console.log under ${logs}`];
    if (!fails.length || Date.now() > until) break;
    wait(2000);
  }
  for (const f of fails) console.log(`FAIL  ${fx.name}  ${f}`);
  console.log(fails.length ? "config boot proof: FAIL" : `config boot proof: PASS (${fx.name}: the booted server logged what the fixture expects)`);
  return fails.length ? 1 : 0;
}

export default {
  name: "config-boot-proof",
  noReset: true,
  usage: "config-boot-proof --fixture <name>   (up --clients 0 --seed config.json)",
  run: (ctx) => runConfigBootProof(ctx.args ?? []),
};
