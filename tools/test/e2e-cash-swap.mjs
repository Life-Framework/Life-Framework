// The cash-slot restart proof of `tools\cli wt e2e <slug> --cash-swap` (#1130).
//
// Two boots of one test, LF_Test_E2E_CashSlotSwap, on the suite's mission header:
// 1. save boot: a carried inventory with 237 cash under the core MoneyStack; ends by the host
//    stop file so the engine writes the SHUTDOWN save;
// 2. reload with the cash slot swapped by a pack: the saved cash is still counted, spent and
//    earned against.

import { readFileSync } from "node:fs";
import { checkShutdownSave, newestTestConsole, E2E_MISSION, HOST_STOP_DEFINE } from "./e2e-shutdown.mjs";

export const TEST = "LF_Test_E2E_CashSlotSwap";
const BASE = "LF_E2E_CASH_SWAP";

export const PASSES = [
  { name: "save boot (core cash)", defines: [BASE, HOST_STOP_DEFINE], keepProfile: false, hostStop: true, marker: "e2e cash swap seeded" },
  { name: "reload with the cash slot swapped", defines: [BASE, "LF_E2E_LOAD_BOOT"], keepProfile: true, hostStop: false, marker: "e2e cash swap counted" },
];

const EVIDENCE = /PrefabSlots|e2e cash swap/;

export function evidenceLines(text) {
  return String(text ?? "").split(/\r?\n/).filter((l) => EVIDENCE.test(l)).map((l) => l.trim());
}

// runPass(opts) -> exit code; opts as runE2ePass takes them. root is the worktree.
export function runCashSwap(root, runPass, log = console.log) {
  for (let i = 0; i < PASSES.length; i++) {
    const p = PASSES[i];
    const label = `e2e cash-swap: pass ${i + 1}/${PASSES.length}`;
    log(`${label} - ${p.name} (${p.defines.join(",")})`);
    let code = runPass({
      mission: E2E_MISSION,
      only: TEST,
      keepProfile: p.keepProfile,
      loadLatestSave: p.keepProfile,
      hostStop: p.hostStop,
      hostStopAfter: p.hostStop ? p.marker : undefined,
      extraDefine: p.defines.join(","),
    });
    const file = newestTestConsole(root);
    const text = file ? readFileSync(file, "utf8") : "";
    for (const line of evidenceLines(text)) log(`${label} ${line}`);
    if (code === 0 && !text.includes(p.marker)) {
      log(`${label} FAILED: no "${p.marker}" line`);
      code = 1;
    }
    if (code === 0 && p.hostStop) {
      const check = checkShutdownSave(text);
      if (!check.ok) {
        log(`${label} FAILED (shutdown save): ${check.reason}`);
        code = 1;
      }
    }
    if (code !== 0) {
      log(`${label} FAILED (exit ${code}); log ${file ?? "(none)"}`);
      return code;
    }
  }
  log("e2e cash-swap: OK (cash saved under the core prefab is still counted after the slot swap and a restart)");
  return 0;
}

// Fixture check for tools/validation/e2e-cash-swap-selftest.mjs; returns failure strings.
export function selftestCashSwap() {
  const failures = [];
  if (PASSES[0].defines.includes("LF_E2E_LOAD_BOOT")) failures.push("the save boot must not be a load boot");
  if (!PASSES[0].hostStop) failures.push("the save boot must end by the host stop so its save is written");
  if (!PASSES[1].defines.includes("LF_E2E_LOAD_BOOT") || !PASSES[1].keepProfile) failures.push("boot 2 loads the save of boot 1");
  const lines = evidenceLines([
    "10:00:00.000 SCRIPT       : [LFDebug:PrefabSlots] slot 'cash' -> {6853E2E2AE878611}Prefabs/Resources/Mining/IronNugget.et (pack)",
    "10:00:00.001 SCRIPT       : [LFDebug:Persistence] applied 3 carried snapshot(s)",
  ].join("\n"));
  if (lines.length !== 1) failures.push(`evidenceLines kept ${lines.length} line(s), expected 1`);
  return failures;
}
