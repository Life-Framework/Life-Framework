// Proves the ship queue's re-gate rule (tools/lib/regate-overlap.mjs, tools/lib/ship-queue.mjs regateDecision):
// a PR whose gate passed is not gated again when main moved only in files that neither are the PR's files
// nor depend on them (same feature area, shared Core, a prefab or config referenced by GUID, a class named
// in the other side's script). No git, ssh or game: the file texts are injected.
// inputs: tools/lib/regate-overlap.mjs, tools/lib/ship-queue.mjs, tools/prs.mjs, tools/lib/gate-tier.mjs
//
// red-proof: 2026-11-16 (queue-regate), run before the module existed ("Cannot find module regate-overlap.mjs"),
// then each rule broken alone in the module: no area rule "same feature folder re-gates"; no Core rule "main
// changed Core, the PR changes a script"; no GUID rule "main changed a prefab the PR references by GUID";
// no token rule "main script names a class the PR changes"; generated files counted unconditionally "main
// changed the test registry, the PR changes no test: pushed as is"; unreadable ignored "an unreadable main
// script re-gates".

import * as ro from "../lib/regate-overlap.mjs";
import * as sq from "../lib/ship-queue.mjs";
import { regateOn } from "../ship-queue.mjs";
import { spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";

let failed = 0;
const check = (ok, msg) => {
  if (ok) console.log(`PASS  ${msg}`);
  else {
    failed++;
    console.log(`FAIL  ${msg}`);
  }
};

const A = "addons/LifeFramework/";
const F = `${A}Scripts/Game/Feature/`;
const GUID = (n) => `{${String(n).padStart(16, "0")}}`;
const meta = (g, p) => `MetaFileClass {\n Name "${g}${p}"\n}\n`;

// A tiny repo: { path: text }, the PR's tree and main's new tree.
function run({ pr, main, prTree = {}, mainTree = {}, mainBefore = {} }) {
  const readPr = (p) => (p in prTree ? prTree[p] : null);
  const readMain = (p, side = "after") => (side === "before" ? (p in mainBefore ? mainBefore[p] : null) : p in mainTree ? mainTree[p] : null);
  return ro.dependentOverlap({ prFiles: pr, mainFiles: main, readPr, readMain });
}

const prBank = { [`${F}Banking/LF_BankAccount.c`]: "class LF_BankAccount { void Deposit(int n) {} }\n" };
const pr = Object.keys(prBank);

// 1. Nothing shared, nothing referenced: no overlap.
let r = run({ pr, main: [`${F}Chat/LF_ChatLog.c`, "website/src/App.tsx"], prTree: prBank, mainTree: { [`${F}Chat/LF_ChatLog.c`]: "class LF_ChatLog {}\n", "website/src/App.tsx": "export {}\n" } });
check(!r.overlap, "main moved in a different area and the site: no overlap");

// 2. The same file.
r = run({ pr, main: pr, prTree: prBank, mainTree: prBank });
check(r.overlap && /same file/.test(r.reasons.join(" ")), "main changed the PR's file: overlap");

// 3. Docs and driver-merged files never count (both sides append to them).
r = run({ pr: [...pr, "docs/features.md"], main: ["docs/features.md", "docs/worktrees.md"], prTree: prBank, mainTree: {} });
check(!r.overlap, "docs/features.md and docs/*.md on both sides: no overlap");

// 4. Same feature area, a different file.
r = run({ pr, main: [`${F}Banking/LF_BankVault.c`], prTree: prBank, mainTree: { [`${F}Banking/LF_BankVault.c`]: "class LF_BankVault {}\n" } });
check(r.overlap && /area Banking/.test(r.reasons.join(" ")), "main changed another file in the PR's feature folder: overlap");
r = run({ pr, main: [`${A}Prefabs/Banking/Vault.et`], prTree: prBank, mainTree: { [`${A}Prefabs/Banking/Vault.et`]: "GenericEntity {}\n" } });
check(r.overlap && /area Banking/.test(r.reasons.join(" ")), "main changed a prefab in the same-named area folder: overlap");

// 5. Shared Core.
r = run({ pr, main: [`${A}Scripts/Game/Core/LF_Debug.c`], prTree: prBank, mainTree: { [`${A}Scripts/Game/Core/LF_Debug.c`]: "class LF_Debug {}\n" } });
check(r.overlap && /Core/.test(r.reasons.join(" ")), "main changed a Core script and the PR changes a script: overlap");
r = run({ pr: [`${A}Scripts/Game/Core/LF_Component.c`], main: [`${F}Chat/LF_ChatLog.c`], prTree: { [`${A}Scripts/Game/Core/LF_Component.c`]: "class LF_Component {}\n" }, mainTree: { [`${F}Chat/LF_ChatLog.c`]: "class LF_ChatLog {}\n" } });
check(r.overlap && /Core/.test(r.reasons.join(" ")), "the PR changes Core and main changed a script: overlap");
r = run({ pr: ["website/src/App.tsx"], main: [`${A}Scripts/Game/Core/LF_Debug.c`], prTree: { "website/src/App.tsx": "export {}\n" }, mainTree: { [`${A}Scripts/Game/Core/LF_Debug.c`]: "class LF_Debug {}\n" } });
check(!r.overlap, "a website PR does not depend on Core: no overlap");

// 6. A prefab the PR's script references by GUID.
const vaultGuid = GUID(111);
const prRef = { [`${F}Banking/LF_BankAccount.c`]: `class LF_BankAccount { ResourceName s = "${vaultGuid}Prefabs/Props/Vault.et"; }\n` };
r = run({ pr: Object.keys(prRef), main: [`${A}Prefabs/Props/Vault.et`], prTree: prRef, mainTree: { [`${A}Prefabs/Props/Vault.et`]: "GenericEntity {}\n", [`${A}Prefabs/Props/Vault.et.meta`]: meta(vaultGuid, "Prefabs/Props/Vault.et") } });
check(r.overlap && /GUID/.test(r.reasons.join(" ")), "main changed a prefab the PR references by GUID: overlap");
r = run({ pr: Object.keys(prRef), main: [`${A}Prefabs/Props/Crate.et`], prTree: prRef, mainTree: { [`${A}Prefabs/Props/Crate.et`]: "GenericEntity {}\n", [`${A}Prefabs/Props/Crate.et.meta`]: meta(GUID(222), "Prefabs/Props/Crate.et") } });
check(!r.overlap, "main changed another prefab: no overlap");
// ... and the other way: main adds a config that references what the PR changes.
const prPrefab = { [`${A}Prefabs/Props/Safe.et`]: "GenericEntity {}\n", [`${A}Prefabs/Props/Safe.et.meta`]: meta(GUID(333), "Prefabs/Props/Safe.et") };
r = run({ pr: Object.keys(prPrefab), main: [`${A}Configs/Shop/Stock.conf`], prTree: prPrefab, mainTree: { [`${A}Configs/Shop/Stock.conf`]: `Stock {\n Item "${GUID(333)}Prefabs/Props/Safe.et"\n}\n` } });
check(r.overlap && /GUID/.test(r.reasons.join(" ")), "main added a config that references a prefab the PR changes: overlap");
r = run({ pr: Object.keys(prPrefab), main: [`${A}Configs/Shop/Stock.conf`], prTree: prPrefab, mainTree: { [`${A}Configs/Shop/Stock.conf`]: `Stock {\n Item "${GUID(444)}Prefabs/Props/Other.et"\n}\n` } });
check(!r.overlap, "a config referencing something else: no overlap");
// A deleted prefab: its text is gone, its .meta GUID is read from before.
r = run({ pr: Object.keys(prRef), main: [`${A}Prefabs/Props/Vault.et`, `${A}Prefabs/Props/Vault.et.meta`], prTree: prRef, mainTree: {}, mainBefore: { [`${A}Prefabs/Props/Vault.et.meta`]: meta(vaultGuid, "Prefabs/Props/Vault.et") } });
check(r.overlap && /GUID/.test(r.reasons.join(" ")), "main deleted a prefab the PR references: overlap");

// 7. A class named across the two sides.
const prCls = { [`${F}Banking/LF_BankAccount.c`]: "class LF_BankAccount { }\nclass LF_BankStatement { }\n" };
r = run({ pr: Object.keys(prCls), main: [`${F}Chat/LF_ChatLog.c`], prTree: prCls, mainTree: { [`${F}Chat/LF_ChatLog.c`]: "class LF_ChatLog { void Say() { LF_BankStatement s; } }\n" } });
check(r.overlap && /LF_BankStatement/.test(r.reasons.join(" ")), "a main script names a class the PR declares: overlap");
r = run({ pr: [`${F}Chat/LF_ChatPanel.c`], main: [`${F}Banking/LF_BankAccount.c`], prTree: { [`${F}Chat/LF_ChatPanel.c`]: "class LF_ChatPanel { LF_BankAccount a; }\n" }, mainTree: prCls });
check(r.overlap && /LF_BankAccount/.test(r.reasons.join(" ")), "the PR names a class main changed: overlap");
r = run({ pr: [`${F}Chat/LF_ChatPanel.c`], main: [`${F}Banking/LF_BankAccount.c`], prTree: { [`${F}Chat/LF_ChatPanel.c`]: "class LF_ChatPanel { LF_BankAccountX a; }\n" }, mainTree: prCls });
check(!r.overlap, "a longer identifier does not count as the class (word match): no overlap");
r = run({ pr: ["tools/lib/gate-risk.mjs"], main: ["tools/lib/gate-tier.mjs", "tools/other.mjs"], prTree: { "tools/lib/gate-risk.mjs": "export {};\n" }, mainTree: { "tools/lib/gate-tier.mjs": 'import { x } from "./gate-risk.mjs";\n', "tools/other.mjs": "export {};\n" } });
check(r.overlap && /gate-risk/.test(r.reasons.join(" ")), "a tools module imports a module the PR changes: overlap");

// 8. Generated files count only when the PR changes the same generated file.
const REG = `${A}Scripts/Game/Tests/LF_TestRegistrations.generated.c`;
const reg = sq.generated(REG);
r = run({ pr, main: [REG], prTree: prBank, mainTree: { [REG]: "// registry\n" } });
check(reg && !r.overlap, "main changed the test registry, the PR changes no test: no overlap (pushed as is)");
r = run({ pr: [...pr, REG], main: [REG], prTree: { ...prBank, [REG]: "// registry\n" }, mainTree: { [REG]: "// registry 2\n" } });
check(r.overlap, "both changed the test registry: overlap");

// 9. Unreadable text and a huge main push are never guessed at.
r = ro.dependentOverlap({ prFiles: pr, mainFiles: [`${F}Chat/LF_ChatLog.c`], readPr: (p) => prBank[p] ?? null, readMain: () => null });
check(r.overlap && /unreadable/.test(r.reasons.join(" ")), "a main script that cannot be read: overlap (conservative)");
const many = Array.from({ length: 800 }, (_, i) => `website/src/f${i}.tsx`);
r = ro.dependentOverlap({ prFiles: pr, mainFiles: many, readPr: (p) => prBank[p] ?? null, readMain: () => "export {}\n" });
check(r.overlap && /too many/.test(r.reasons.join(" ")), "800 changed main files: too many to compare, overlap");
r = run({ pr, main: [], prTree: prBank });
check(!r.overlap, "main did not move: no overlap");

// 10. regateDecision: the decision the worker acts on.
let d = sq.regateDecision({ prFiles: pr, mainFiles: ["website/src/App.tsx"], deps: { overlap: false, reasons: [] } });
check(!d.regate && /pushing|merg/.test(d.why), "no overlap: the gated head goes up");
d = sq.regateDecision({ prFiles: pr, mainFiles: [`${F}Banking/LF_BankVault.c`], deps: { overlap: true, reasons: ["same feature area Banking"] } });
check(d.regate && /area Banking/.test(d.why), "overlap: re-gate, with the reason");
d = sq.regateDecision({ prFiles: pr, mainFiles: [`${F}Banking/LF_BankVault.c`], deps: { overlap: true, reasons: ["x"] }, attempts: 3 });
check(!d.regate && d.capped, "overlap after 3 gates: capped, the head goes up");
d = sq.regateDecision({ prFiles: pr, mainFiles: ["x.mjs"], deps: null });
check(!d.regate, "no deps and no shared file: pushed as before");
d = sq.regateDecision({ prFiles: pr, mainFiles: [REG], deps: null });
check(d.regate, "no deps (could not compare) and a generated file on main: re-gate as before");

if (failed) {
  console.log(`\nregate-overlap-selftest: ${failed} failed`);
  process.exit(1);
}
console.log("\nregate-overlap-selftest: all passed");
