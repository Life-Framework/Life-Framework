// tools/validation/rpc-owner-host-path-selftest.mjs - validate-rpc-owner-host-path (#2620).
// inputs: tools/validation/validate-rpc-owner-host-path.mjs, tools/validation/rpc-owner-host-path-selftest.mjs
//
// Fixture methods go through broadcastHandlers / broadcastSites, and a throwaway tree goes through the validator:
//   - a bare `Rpc(<Broadcast handler>)` is flagged; a direct call of the handler, or a host guard, passes;
//   - an Owner handler is not checked (the engine runs it on the host already, rpc-routing.md);
//   - comments and strings are not calls, and a local call in a neighbouring method does not cover this one;
//   - a baselined site passes, a stale baseline entry fails, and a Tests/ file is skipped.
//
// red-proof: 2026-11-18 with the localPath test in broadcastSites forced to true, "a bare Broadcast Rpc is flagged"
// printed FAIL.
import { spawnSync } from "node:child_process";
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { scratchDir } from "../lib/scratch.mjs";
import { broadcastHandlers, broadcastSites } from "./validate-rpc-owner-host-path.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const wrap = (body) => `modded class SCR_ChimeraCharacter\n{\n${body}\n}\n`;
const handler = (kind = "Broadcast") => `\n\t[RplRpc(RplChannel.Reliable, RplRcver.${kind})]\n\tprotected void RpcDo_LF_X(int a)\n\t{\n\t}`;
const bare = wrap(`\tvoid LF_SendX(int a)\n\t{\n\t\tRpc(RpcDo_LF_X, a);\n\t}\n${handler()}`);
const direct = wrap(`\tvoid LF_SendX(int a)\n\t{\n\t\tRpc(RpcDo_LF_X, a);\n\t\tRpcDo_LF_X(a);\n\t}\n${handler()}`);
const guarded = wrap(`\tvoid LF_SendX(int a)\n\t{\n\t\tRpc(RpcDo_LF_X, a);\n\t\tif (RplSession.Mode() != RplMode.Dedicated)\n\t\t\tLF_ShowX(a);\n\t}\n${handler()}`);
const owner = wrap(`\tvoid LF_SendX(int a)\n\t{\n\t\tRpc(RpcDo_LF_X, a);\n\t}\n${handler("Owner")}`);

const sitesOf = (t) => broadcastSites(t, broadcastHandlers(t));
check(JSON.stringify(broadcastHandlers(bare)) === '["RpcDo_LF_X"]', "a Broadcast handler is found by its attribute");
check(broadcastHandlers(owner).length === 0, "an Owner handler is not a Broadcast handler");
check(sitesOf(bare).length === 1 && !sitesOf(bare)[0].localPath, "a bare Broadcast Rpc is flagged");
check(sitesOf(bare)[0].method === "LF_SendX" && sitesOf(bare)[0].line === 5, "the site names its method and 1-based line");
check(sitesOf(direct).length === 1 && sitesOf(direct)[0].localPath, "a direct call of the handler passes");
check(sitesOf(guarded).length === 1 && sitesOf(guarded)[0].localPath, "a host guard in the method passes");
check(sitesOf(owner).length === 0, "an owner reply is not checked (the engine runs it on the host)");
check(sitesOf(wrap(`\tvoid A()\n\t{\n\t\t// Rpc(RpcDo_LF_X);\n\t\tstring s = "Rpc(RpcDo_LF_X)";\n\t}\n${handler()}`)).length === 0, "comments and strings are not calls");
const neighbour = wrap(`\tvoid A()\n\t{\n\t\tRpcDo_LF_X(1);\n\t}\n\n\tvoid B()\n\t{\n\t\tRpc(RpcDo_LF_X, 1);\n\t}\n${handler()}`);
check(sitesOf(neighbour).length === 1 && !sitesOf(neighbour)[0].localPath, "a local call in another method does not cover this one");
const nested = wrap(`\tvoid A(bool b)\n\t{\n\t\tif (b)\n\t\t{\n\t\t\tRpc(RpcDo_LF_X, 1);\n\t\t\tRpcDo_LF_X(1);\n\t\t}\n\t}\n${handler()}`);
check(sitesOf(nested).length === 1 && sitesOf(nested)[0].localPath, "a call nested in the method body counts");
check(broadcastSites(wrap(`\tvoid A()\n\t{\n\t\tRpc(RpcDo_LF_Y);\n\t}`), ["RpcDo_LF_X"]).length === 0, "an Rpc to another handler is ignored");

const validator = join(dirname(fileURLToPath(import.meta.url)), "validate-rpc-owner-host-path.mjs");
const dir = scratchDir("lf-rpc-owner-");
const game = join(dir, "addons", "LifeFramework", "Scripts", "Game");
const feature = join(game, "Feature", "Demo");
mkdirSync(feature, { recursive: true });
mkdirSync(join(game, "Tests"), { recursive: true });
const baseline = join(dir, "baseline.json");
const run = () => spawnSync(process.execPath, [validator], { env: { ...process.env, LF_RPCOWNER_ROOT: dir, LF_RPCOWNER_BASELINE: baseline }, encoding: "utf8" });
const site = "addons/LifeFramework/Scripts/Game/Feature/Demo/LF_Demo.c::LF_SendX";

writeFileSync(baseline, JSON.stringify({ sites: [] }));
writeFileSync(join(feature, "LF_Demo.c"), bare);
let r = run();
check(r.status === 1 && /LF_Demo\.c:5 LF_SendX\(\)/.test(r.stdout), "the validator fails a tree with a bare site and names file:line");
writeFileSync(baseline, JSON.stringify({ sites: [{ site, reason: "fixture" }] }));
r = run();
check(r.status === 0 && /1 baselined/.test(r.stdout), "a baselined site passes");
writeFileSync(join(feature, "LF_Demo.c"), direct);
r = run();
check(r.status === 1 && /no longer flagged/.test(r.stdout), "a stale baseline entry fails");
writeFileSync(baseline, JSON.stringify({ sites: [] }));
r = run();
check(r.status === 0, "the direct-call tree with an empty baseline passes");
writeFileSync(join(feature, "LF_Demo.c"), wrap(handler()));
writeFileSync(join(game, "Tests", "LF_Probe.c"), wrap(`\tvoid Send()\n\t{\n\t\tRpc(RpcDo_LF_X, 1);\n\t}`));
r = run();
check(r.status === 0, "a bare Rpc in a Tests/ file is skipped");

if (failed) {
  console.log(`rpc-owner-host-path-selftest: ${failed} failed`);
  process.exit(1);
}
console.log("rpc-owner-host-path-selftest: OK");
