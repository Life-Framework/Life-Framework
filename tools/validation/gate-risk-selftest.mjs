// Proves the slim-gate risk rules (tools/lib/gate-risk.mjs, docs/worktrees.md "Gate tiers"): a T2 diff that
// touches persistence, a money path, an RPC handler, a GUID or .meta, the addon boundary or .gproj, or has an
// interrogate file keeps the full gate; any other T2 diff gets the slim gate (validate --full + LOGIC).
// inputs: tools/lib/gate-risk.mjs, tools/lib/gate-tier.mjs
// red-proof: run before tools/lib/gate-risk.mjs existed it failed to import ("Cannot find module"); with
//   riskClasses returning [] it printed one FAIL per class ("persistence: expected full, got slim", ...).
import { riskClasses, rpcRegions, parsePatch } from "../lib/gate-risk.mjs";
import { classify, tierLine } from "../lib/gate-tier.mjs";

const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };

// A -U0 patch for one file: hunks of [oldStart, oldLines, newStart, newLines, lines].
const patch = (path, hunks) => [
  `diff --git a/${path} b/${path}`,
  `--- a/${path}`,
  `+++ b/${path}`,
  ...hunks.flatMap(([a, b, c, d, lines]) => [`@@ -${a},${b} +${c},${d} @@`, ...lines]),
].join("\n");
const C = (path, before, after, hunks, status = "M") => ({ path, status, before, after, patch: patch(path, hunks) });

// Real shapes (trimmed) from addons/LifeFramework at 26b2d59d.
const SHOP_C = [
  "class LF_ShopComponent : ScriptComponent",       // 1
  "{",                                              // 2
  "\tvoid Log(string s)",                           // 3
  "\t{",                                            // 4
  "\t\tPrint(s);",                                  // 5
  "\t}",                                            // 6
  "",                                               // 7
  "\t[RplRpc(RplChannel.Reliable, RplRcver.Server)]", // 8
  "\tprotected void RpcAsk_Buy(int idx)",           // 9
  "\t{",                                            // 10
  "\t\tif (idx < 0)",                               // 11
  "\t\t{",                                          // 12
  "\t\t\treturn;",                                  // 13
  "\t\t}",                                          // 14
  "\t\tDoBuy(idx);",                                // 15
  "\t}",                                            // 16
  "}",                                              // 17
  "",
].join("\n");
const UI_C = "class LF_HudHint : ScriptedWidgetComponent\n{\n\tvoid Show()\n\t{\n\t\tm_wRoot.SetOpacity(1);\n\t}\n}\n";
const UI = "addons/LifeFramework/Scripts/Game/UI/LF_HudHint.c";

const NORMAL = C(UI, UI_C, UI_C.replace("SetOpacity(1)", "SetOpacity(0.8)"), [[5, 1, 5, 1, ["-\t\tm_wRoot.SetOpacity(1);", "+\t\tm_wRoot.SetOpacity(0.8);"]]]);

const cases = [
  // [name, files, opts, expected class or null for slim]
  ["normal script change", [NORMAL], {}, null],
  ["new UI script, no risk", [C("addons/LifeFramework/Scripts/Game/UI/LF_New.c", null, UI_C, [[0, 0, 1, 7, UI_C.trim().split("\n").map((l) => `+${l}`)]], "A")], {}, null],
  ["persistence: a Persistence/ folder", [C("addons/LifeFramework/Scripts/Game/Feature/ATM/Persistence/LF_BankAccountRecord.c", "x\n", "y\n", [[1, 1, 1, 1, ["-x", "+y"]]])], {}, "persistence"],
  ["persistence: a payload registration in any file", [C(UI, UI_C, UI_C, [[5, 0, 6, 1, ["+\t\tRegisterPersistencePayloads(m);"]]])], {}, "persistence"],
  ["persistence: a *Save*.c file", [C("addons/LifeFramework/Scripts/Game/Feature/Atmosphere/LF_DayCycleSave.c", "x\n", "y\n", [[1, 1, 1, 1, ["-x", "+y"]]])], {}, "persistence"],
  ["money: a shop script", [C("addons/LifeFramework/Scripts/Game/Feature/Shop/LF_ShopRules.c", "x\n", "y\n", [[1, 1, 1, 1, ["-x", "+y"]]])], {}, "money"],
  ["money: robbery payout", [C("addons/LifeFramework/Scripts/Game/Feature/Crime/LF_BankLoot.c", "x\n", "y\n", [[1, 1, 1, 1, ["-x", "+y"]]])], {}, "money"],
  ["money: fence", [C("addons/LifeFramework/Scripts/Game/Feature/Crime/LF_FenceRules.c", "x\n", "y\n", [[1, 1, 1, 1, ["-x", "+y"]]])], {}, "money"],
  ["money: vehicle buy", [C("addons/LifeFramework/Scripts/Game/Feature/Vehicles/LF_VehicleLotActions.c", "x\n", "y\n", [[1, 1, 1, 1, ["-x", "+y"]]])], {}, "money"],
  ["money: a cash call in any script", [C(UI, UI_C, UI_C, [[5, 0, 6, 1, ["+\t\tLF_MoneyUtils.AddCash(e, 5);"]]])], {}, "money"],
  ["rpc: a line inside an RpcAsk_ body", [C("addons/LifeFramework/Scripts/Game/UI/LF_Thing.c", SHOP_C, SHOP_C.replace("idx < 0", "idx < 1"), [[11, 1, 11, 1, ["-\t\tif (idx < 0)", "+\t\tif (idx < 1)"]]])], {}, "rpc"],
  ["rpc: an added [RplRpc] attribute", [C(UI, UI_C, UI_C, [[3, 0, 3, 1, ["+\t[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]"]]])], {}, "rpc"],
  ["guid: a .meta changed", [NORMAL, { path: "addons/LifeFramework/Prefabs/X.et.meta", status: "M" }], {}, "guid"],
  ["guid: a new resource with its .meta", [NORMAL, { path: "addons/LifeFramework/Prefabs/New.et", status: "?" }, { path: "addons/LifeFramework/Prefabs/New.et.meta", status: "?" }], {}, "guid"],
  ["guid: a reference swapped in a prefab", [C("addons/LifeFramework/Prefabs/Y.et", "a\n", "b\n", [[4, 1, 4, 1, ['-  m_Prefab "{AAAAAAAAAAAAAAAA}Prefabs/A.et"', '+  m_Prefab "{BBBBBBBBBBBBBBBB}Prefabs/B.et"']]])], {}, "guid"],
  ["boundary: a .gproj", [NORMAL, { path: "addons/LF_Everon1989/LF_Everon1989.gproj", status: "M" }], {}, "boundary"],
  ["boundary: a mission header", [NORMAL, { path: "addons/LF_Everon1989/Missions/LF_Everon1989.conf", status: "M" }], {}, "boundary"],
  ["boundary: tools/lib/addons.mjs beside a script", [NORMAL, { path: "tools/lib/addons.mjs", status: "M" }], {}, "boundary"],
  ["review: an interrogate file", [NORMAL], { review: ["tmp/review-main-guard.md"] }, "review"],
  ["unreadable script diff stays full", [{ path: UI, status: "M" }], {}, "unreadable"],
];

for (const [name, files, opts, want] of cases) {
  const got = riskClasses(files, opts).map((r) => r.cls);
  if (want === null) check(got.length === 0, `${name}: expected slim, got full (${got.join(", ")})`);
  else check(got.includes(want), `${name}: expected full (${want}), got ${got.length ? got.join(", ") : "slim"}`);
}

// The RPC regions: the body of RpcAsk_Buy (lines 9-16), not Log.
const reg = rpcRegions(SHOP_C);
check(reg.length === 1 && reg[0][0] <= 9 && reg[0][1] === 16, `rpcRegions: ${JSON.stringify(reg)}`);
check(parsePatch(patch("a.c", [[3, 2, 3, 0, ["-x", "-y"]]]))[0].oldStart === 3, "parsePatch oldStart");

// classify: a normal T2 diff is slim; a risky one is full; T0 and T1 are untouched; --full-gate is full.
const slim = classify([NORMAL]);
check(slim.tier === 2 && slim.slim === true, `classify normal: ${tierLine(slim)}`);
check(/T2s \(slim/.test(tierLine(slim)), `tierLine slim: ${tierLine(slim)}`);
const full = classify([NORMAL, { path: "addons/LifeFramework/Prefabs/X.et.meta", status: "M" }]);
check(full.tier === 2 && full.slim === false && /T2 \(full gate\)/.test(tierLine(full)), `classify risky: ${tierLine(full)}`);
check(classify([NORMAL], { review: ["tmp/review-x.md"] }).slim === false, "classify with an interrogate file must be full");
check(classify([NORMAL], { forceFull: true }).slim !== true, "--full-gate must never be slim");
check(classify([{ path: "docs/x.md", status: "M" }]).tier === 0, "docs-only stays T0");

if (fails.length) {
  for (const f of fails) console.log(`FAIL  ${f}`);
  process.exit(1);
}
console.log(`PASS  gate-risk: ${cases.length} cases (slim, persistence, money, rpc, guid, boundary, review, unreadable), rpc regions, classify`);
