// tools/validation/rpc-host-path-selftest.mjs - validate-rpc-host-path (#2511).
// inputs: tools/validation/validate-rpc-host-path.mjs, tools/validation/rpc-host-path-selftest.mjs
//
// Fixture methods go through rpcAskSites, and a throwaway tree goes through the validator itself:
//   - a method with a bare `Rpc(RpcAsk_X)` is flagged; the host-branch form is not;
//   - an Rpc inside a comment or a string is not a call; a non-RpcAsk Rpc (server to client) is not its business;
//   - the host check in a neighbouring method does not cover this one;
//   - a baselined site passes, a stale baseline entry fails.
//
// red-proof: 2026-11-17 with the hostBranch test in rpcAskSites forced to true, "a bare Rpc(RpcAsk_) is flagged"
// printed FAIL.
import { spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { rpcAskSites } from "./validate-rpc-host-path.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const wrap = (body) => `modded class SCR_ChimeraCharacter\n{\n${body}\n}\n`;
const bare = wrap(`\tvoid LF_AskX(int a)\n\t{\n\t\tRpc(RpcAsk_LF_X, a);\n\t}\n\n\t[RplRpc(RplChannel.Reliable, RplRcver.Server)]\n\tprotected void RpcAsk_LF_X(int a)\n\t{\n\t}`);
const branched = wrap(`\tvoid LF_AskX(int a)\n\t{\n\t\tif (Replication.IsServer())\n\t\t{\n\t\t\tRpcAsk_LF_X(a);\n\t\t\treturn;\n\t\t}\n\n\t\tRpc(RpcAsk_LF_X, a);\n\t}`);

const sites = (t) => rpcAskSites(t);
check(sites(bare).length === 1 && !sites(bare)[0].hostBranch, "a bare Rpc(RpcAsk_) is flagged");
check(sites(bare)[0].method === "LF_AskX" && sites(bare)[0].line === 5, "the site names its method and 1-based line");
check(sites(branched).length === 1 && sites(branched)[0].hostBranch, "the host-branch form passes");
check(sites(wrap(`\tvoid A()\n\t{\n\t\t// Rpc(RpcAsk_LF_X);\n\t\tstring s = "Rpc(RpcAsk_LF_X)";\n\t}`)).length === 0, "comments and strings are not calls");
check(sites(wrap(`\tvoid A()\n\t{\n\t\tRpc(RpcDo_LF_X);\n\t}`)).length === 0, "a server-to-client Rpc is not checked");
const neighbour = wrap(`\tvoid A()\n\t{\n\t\tif (Replication.IsServer())\n\t\t\treturn;\n\t}\n\n\tvoid B()\n\t{\n\t\tRpc(RpcAsk_LF_X);\n\t}`);
check(sites(neighbour).length === 1 && !sites(neighbour)[0].hostBranch, "a host check in another method does not cover this one");
const nested = wrap(`\tvoid A(bool b)\n\t{\n\t\tif (b)\n\t\t{\n\t\t\tif (Replication.IsServer())\n\t\t\t\treturn;\n\t\t\tRpc(RpcAsk_LF_X);\n\t\t}\n\t}`);
check(sites(nested).length === 1 && sites(nested)[0].hostBranch, "a branch nested in the method body counts");

const validator = join(dirname(fileURLToPath(import.meta.url)), "validate-rpc-host-path.mjs");
const dir = mkdtempSync(join(tmpdir(), "lf-rpc-host-"));
const feature = join(dir, "addons", "LifeFramework", "Scripts", "Game", "Feature", "Demo");
mkdirSync(feature, { recursive: true });
const baseline = join(dir, "baseline.json");
const run = () => spawnSync(process.execPath, [validator], { env: { ...process.env, LF_RPCHOST_ROOT: dir, LF_RPCHOST_BASELINE: baseline }, encoding: "utf8" });
const site = "addons/LifeFramework/Scripts/Game/Feature/Demo/LF_Demo.c::LF_AskX";

writeFileSync(baseline, JSON.stringify({ sites: [] }));
writeFileSync(join(feature, "LF_Demo.c"), bare);
let r = run();
check(r.status === 1 && /LF_Demo\.c:5 LF_AskX\(\)/.test(r.stdout), "the validator fails a tree with a bare site and names file:line");
writeFileSync(baseline, JSON.stringify({ sites: [{ site, reason: "fixture" }] }));
r = run();
check(r.status === 0 && /1 baselined/.test(r.stdout), "a baselined site passes");
writeFileSync(join(feature, "LF_Demo.c"), branched);
r = run();
check(r.status === 1 && /no longer flagged/.test(r.stdout), "a stale baseline entry fails");
writeFileSync(baseline, JSON.stringify({ sites: [] }));
r = run();
check(r.status === 0, "the host-branch tree with an empty baseline passes");

if (failed) {
  console.log(`rpc-host-path-selftest: ${failed} failed`);
  process.exit(1);
}
console.log("rpc-host-path-selftest: OK");
